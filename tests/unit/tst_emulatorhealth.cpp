#include "nexora/emulator/EmulatorHealth.h"

#include <QFile>
#include <QTemporaryDir>
#include <QTest>

using namespace nexora::emulator;
using nexora::shared::EmulatorConfig;

class TestEmulatorHealth : public QObject
{
    Q_OBJECT

private slots:
    void availableForFakeEmulator()
    {
        const auto config = EmulatorConfig::make(QStringLiteral("fake"), QStringLiteral("Fake"),
                                                 QStringLiteral(NEXORA_FAKE_EMULATOR));
        const HealthReport report = checkHealth(config);
        QCOMPARE(report.health, Health::Available);
        QVERIFY(report.checkedAt.isValid());
    }

    void missingExecutable()
    {
        const auto config
            = EmulatorConfig::make(QStringLiteral("x"), QStringLiteral("X"), QStringLiteral("/nonexistent/x"));
        const HealthReport report = checkHealth(config);
        QCOMPARE(report.health, Health::ExecutableMissing);
        QVERIFY(report.detail.contains(QStringLiteral("/nonexistent/x")));
    }

    void notExecutable()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("plain"));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("data");
        file.close();
        QVERIFY(file.setPermissions(QFile::ReadOwner | QFile::WriteOwner));
        const auto config = EmulatorConfig::make(QStringLiteral("x"), QStringLiteral("X"), path);
        QCOMPARE(checkHealth(config).health, Health::NotExecutable);
        // A directory is not an executable either.
        const auto asDir = EmulatorConfig::make(QStringLiteral("x"), QStringLiteral("X"), dir.path());
        QCOMPARE(checkHealth(asDir).health, Health::NotExecutable);
    }

    void disabled()
    {
        const auto config = EmulatorConfig::make(QStringLiteral("fake"), QStringLiteral("Fake"),
                                                 QStringLiteral(NEXORA_FAKE_EMULATOR), {}, false);
        QCOMPARE(checkHealth(config).health, Health::Disabled);
    }

    void invalidConfig()
    {
        const auto relative = EmulatorConfig::make(QStringLiteral("x"), QStringLiteral("X"), QStringLiteral("bin/emu"));
        QCOMPARE(checkHealth(relative).health, Health::InvalidConfig);
        const auto empty = EmulatorConfig::make(QStringLiteral("x"), QStringLiteral("X"), QString());
        QCOMPARE(checkHealth(empty).health, Health::InvalidConfig);
        const auto noId = EmulatorConfig::make(QString(), QStringLiteral("X"), QStringLiteral(NEXORA_FAKE_EMULATOR));
        QCOMPARE(checkHealth(noId).health, Health::InvalidConfig);
    }

    void names()
    {
        QCOMPARE(nameOf(Health::Available), QStringLiteral("Available"));
        QCOMPARE(nameOf(Health::NotExecutable), QStringLiteral("NotExecutable"));
    }
};

QTEST_APPLESS_MAIN(TestEmulatorHealth)
#include "tst_emulatorhealth.moc"
