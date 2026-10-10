#include <QGuiApplication>
#include <QTemporaryDir>
#include <QTest>

#include "appsettings.h"
#include "controllermanager.h"
#include "diagnostics.h"
#include "emulatormanager.h"
#include "emulatorregistry.h"
#include "gamelibrary.h"
#include "logger.h"
#include "testutils.h"

using namespace nexora;

class TestDiagnostics : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void sanitize();
    void staticInfo();
    void reportContents();
    void logTailAndRecentErrors();
    void clipboard();

private:
    QTemporaryDir m_home;
    QTemporaryDir m_logs;
    QString m_configDir;
};

void TestDiagnostics::initTestCase()
{
    QVERIFY(m_home.isValid());
    // Deterministic home directory so sanitising can be verified.
    qputenv("HOME", QFile::encodeName(m_home.path()));
    QCOMPARE(QDir::homePath(), m_home.path());
    m_configDir = m_home.filePath(".config/nexora");
    QDir().mkpath(m_configDir);
    Logger::init(m_logs.path());
}

void TestDiagnostics::cleanupTestCase()
{
    Logger::shutdown();
}

void TestDiagnostics::sanitize()
{
    const QString home = m_home.path();
    QCOMPARE(Diagnostics::sanitize(home + "/games/a.mock"), QStringLiteral("~/games/a.mock"));
    QCOMPARE(Diagnostics::sanitize("in " + home), QStringLiteral("in ~"));
    QCOMPARE(Diagnostics::sanitize(home + "suffix/x"), home + "suffix/x"); // different directory
    QCOMPARE(Diagnostics::sanitize("a: " + home + " b: " + home + "/c"), QStringLiteral("a: ~ b: ~/c"));
    QCOMPARE(Diagnostics::sanitize("/opt/other"), QStringLiteral("/opt/other"));
}

void TestDiagnostics::staticInfo()
{
    Diagnostics d(m_configDir, nullptr, nullptr, nullptr, nullptr);
    QCOMPARE(d.appVersion(), QStringLiteral(NEXORA_VERSION));
    QCOMPARE(d.buildId(), QStringLiteral(NEXORA_BUILD_ID));
    QVERIFY(!d.osName().isEmpty());
    QVERIFY(!d.kernel().isEmpty());
    QVERIFY(!d.architecture().isEmpty());
    QCOMPARE(d.qtVersion(), QString::fromLatin1(qVersion()));
    QCOMPARE(d.logFilePath(), m_logs.filePath("nexora.log"));
    QCOMPARE(d.configDirectory(), m_configDir);
    // No /etc/nexora-release on a development machine; must not crash either way.
    Q_UNUSED(d.imageVersion())
    QVERIFY(!d.report().isEmpty()); // all services missing is handled
}

void TestDiagnostics::reportContents()
{
    const QByteArray sentinel = "SENTINEL_VALUE_4f2a9c";
    qputenv("NEXORA_TEST_SECRET", sentinel);

    QJsonObject withArgs = testutils::mockEntry("mock", {"mock"}, {"--secret-flag", "{game}"});
    QVERIFY(testutils::writeEmulators(m_configDir, {withArgs}));
    AppSettings settings(m_configDir);
    EmulatorRegistry registry(m_configDir);
    EmulatorManager manager(&registry);
    ControllerManager controllers;
    GameLibrary library(m_configDir, &settings, &registry);
    Diagnostics d(m_configDir, &registry, &manager, &controllers, &library);

    const QString report = d.report();
    QVERIFY(report.contains(QStringLiteral(NEXORA_VERSION)));
    QVERIFY(report.contains(QStringLiteral(NEXORA_BUILD_ID)));
    QVERIFY(report.contains(QStringLiteral("Emulator mock")));
    QVERIFY(report.contains(QStringLiteral("Available")));
    QVERIFY(report.contains(QStringLiteral("~/.config/nexora")));
    QVERIFY(!report.contains(m_home.path()));
    QVERIFY(!report.contains(QString::fromLatin1(sentinel)));
    QVERIFY(!report.contains(QStringLiteral("--secret-flag"))); // no emulator arguments
    QVERIFY(report.contains(QStringLiteral("[Controllers]")));
    QVERIFY(report.contains(QStringLiteral("[Library]")));
}

void TestDiagnostics::logTailAndRecentErrors()
{
    Diagnostics d(m_configDir, nullptr, nullptr, nullptr, nullptr);
    d.clearRecentErrors();
    QVERIFY(d.recentErrors().isEmpty());
    qInfo("diagnostics-info-line in %s", qPrintable(m_home.filePath("x")));
    qWarning("diagnostics-warning-line");

    const QStringList tail = d.logTail(5);
    QVERIFY(tail.size() <= 5);
    QVERIFY(tail.last().contains("diagnostics-warning-line"));
    QVERIFY(tail.last().contains("WARN"));
    QVERIFY(tail.at(tail.size() - 2).contains("diagnostics-info-line in ~/x"));

    QCOMPARE(d.recentErrors().size(), 1);
    QVERIFY(d.recentErrors().first().contains("diagnostics-warning-line"));
    QVERIFY(d.report().contains("diagnostics-warning-line"));

    for (int i = 0; i < 60; ++i)
        qWarning("flood %d", i);
    QCOMPARE(Logger::recentErrors().size(), 50);
    QVERIFY(Logger::recentErrors().last().contains("flood 59"));
    QCOMPARE(Logger::tail(0), QStringList());
}

void TestDiagnostics::clipboard()
{
    Diagnostics d(m_configDir, nullptr, nullptr, nullptr, nullptr);
    QVERIFY(d.copyToClipboard(QStringLiteral("report text")));
}

QTEST_MAIN(TestDiagnostics)
#include "tst_diagnostics.moc"
