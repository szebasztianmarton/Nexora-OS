#include <QElapsedTimer>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include "emulatormanager.h"
#include "emulatorregistry.h"
#include "testutils.h"

using nexora::EmulatorManager;
using nexora::EmulatorRegistry;

namespace {
constexpr int Timeout = 10000;
}

class TestEmulatorManager : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void normalExit();
    void errorExit();
    void crash();
    void missingExecutable();
    void failedToStart();
    void unavailableRefused();
    void missingGameRefused();
    void secondLaunchRefused();
    void terminateStops();
    void terminateKillsStubbornProcess();
    void retryRelaunches();
    void argumentsAreSeparateArgv();
    void destructorKillsChild();

private:
    QString writeGame(const QString &name);

    QTemporaryDir *m_dir = nullptr;
    EmulatorRegistry *m_registry = nullptr;
    EmulatorManager *m_manager = nullptr;
    QString m_argsFile;
    QString m_readyFile;
};

QString TestEmulatorManager::writeGame(const QString &name)
{
    const QString path = m_dir->filePath("games/" + name);
    testutils::writeFile(path);
    return path;
}

void TestEmulatorManager::init()
{
    m_dir = new QTemporaryDir;
    QVERIFY(m_dir->isValid());
    m_argsFile = m_dir->filePath("args.json");
    m_readyFile = m_dir->filePath("ready.json");

    const QString garbage = m_dir->filePath("garbage");
    QVERIFY(testutils::writeFile(garbage, "this is not a program\n"));
    QFile::setPermissions(garbage, QFile::permissions(garbage) | QFileDevice::ExeOwner | QFileDevice::ExeUser);

    using testutils::mockEntry;
    QJsonObject missing = mockEntry("missing");
    missing["executable"] = m_dir->filePath("no-such-emulator");
    QJsonObject off = mockEntry("off");
    off["enabled"] = false;
    QJsonObject broken = mockEntry("garbage");
    broken["executable"] = garbage;
    QJsonObject withWd = mockEntry("args", {"mock"},
                                   {"--write-args", m_argsFile, "--", "--fullscreen", "{game}", "two words"});
    withWd["workingDirectory"] = m_dir->path();

    QVERIFY(testutils::writeEmulators(
        m_dir->path(),
        {mockEntry("ok", {"mock"}, {"--sleep-ms", "100", "{game}"}),
         mockEntry("fail", {"mock"}, {"--exit-code", "3"}),
         mockEntry("crash", {"mock"}, {"--crash"}),
         mockEntry("long", {"mock"}, {"--sleep-ms", "60000", "--write-args", m_readyFile}),
         mockEntry("stubborn", {"mock"}, {"--ignore-sigterm", "--sleep-ms", "60000", "--write-args", m_readyFile}),
         withWd, missing, off, broken}));
    m_registry = new EmulatorRegistry(m_dir->path());
    QCOMPARE(m_registry->count(), 9);
    m_manager = new EmulatorManager(m_registry);
}

void TestEmulatorManager::cleanup()
{
    delete m_manager;
    delete m_registry;
    delete m_dir;
    m_manager = nullptr;
    m_registry = nullptr;
    m_dir = nullptr;
}

void TestEmulatorManager::normalExit()
{
    const QString game = writeGame("Game One.mock");
    QList<EmulatorManager::State> states;
    connect(m_manager, &EmulatorManager::stateChanged, this, [&] { states.append(m_manager->state()); });
    QSignalSpy started(m_manager, &EmulatorManager::processStarted);
    QSignalSpy finished(m_manager, &EmulatorManager::processFinished);

    QVERIFY(m_manager->launch("ok", game));
    QCOMPARE(m_manager->state(), EmulatorManager::Launching);
    QVERIFY(m_manager->busy());
    QCOMPARE(m_manager->activeEmulatorId(), QStringLiteral("ok"));
    QCOMPARE(m_manager->activeEmulatorName(), QStringLiteral("Emulator ok"));
    QCOMPARE(m_manager->activeGamePath(), game);

    QVERIFY(finished.wait(Timeout));
    QCOMPARE(started.count(), 1);
    QCOMPARE(started.first().at(0).toString(), QStringLiteral("ok"));
    QCOMPARE(started.first().at(1).toString(), game);
    QCOMPARE(states, QList<EmulatorManager::State>({EmulatorManager::Launching, EmulatorManager::Running,
                                                    EmulatorManager::Idle}));
    const QList<QVariant> args = finished.first();
    QCOMPARE(args.at(0).toString(), QStringLiteral("ok"));
    QCOMPARE(args.at(2).toInt(), 0);
    QCOMPARE(args.at(3).value<EmulatorManager::Result>(), EmulatorManager::NormalExit);
    QVERIFY(args.at(4).toLongLong() >= 50);
    QCOMPARE(m_manager->lastResult(), EmulatorManager::NormalExit);
    QVERIFY(m_manager->lastErrorMessage().isEmpty());
    QCOMPARE(m_manager->lastEmulatorId(), QStringLiteral("ok"));
    QCOMPARE(m_manager->lastGamePath(), game);
    QVERIFY(!m_manager->canRetry());
    QVERIFY(m_manager->activeEmulatorId().isEmpty());
}

void TestEmulatorManager::errorExit()
{
    QSignalSpy finished(m_manager, &EmulatorManager::processFinished);
    QVERIFY(m_manager->launch("fail"));
    QVERIFY(finished.wait(Timeout));
    QCOMPARE(m_manager->lastResult(), EmulatorManager::ErrorExit);
    QCOMPARE(m_manager->lastExitCode(), 3);
    QVERIFY(m_manager->lastErrorMessage().contains("3"));
    QVERIFY(m_manager->canRetry());

    QSignalSpy resultChanged(m_manager, &EmulatorManager::lastResultChanged);
    m_manager->clearLastResult();
    QCOMPARE(resultChanged.count(), 1);
    QCOMPARE(m_manager->lastResult(), EmulatorManager::NoResult);
    QVERIFY(!m_manager->canRetry());
}

void TestEmulatorManager::crash()
{
    QSignalSpy finished(m_manager, &EmulatorManager::processFinished);
    QVERIFY(m_manager->launch("crash"));
    QVERIFY(finished.wait(Timeout));
    QCOMPARE(finished.first().at(3).value<EmulatorManager::Result>(), EmulatorManager::Crashed);
    QCOMPARE(m_manager->lastResult(), EmulatorManager::Crashed);
    QVERIFY(m_manager->lastErrorMessage().contains("crashed"));
    QVERIFY(m_manager->canRetry());
}

void TestEmulatorManager::missingExecutable()
{
    QSignalSpy failed(m_manager, &EmulatorManager::launchFailed);
    QSignalSpy stateChanged(m_manager, &EmulatorManager::stateChanged);
    QVERIFY(!m_manager->launch("missing"));
    QCOMPARE(failed.count(), 1);
    QVERIFY(!failed.first().at(2).toString().isEmpty());
    QCOMPARE(stateChanged.count(), 0);
    QCOMPARE(m_manager->state(), EmulatorManager::Idle);
}

void TestEmulatorManager::failedToStart()
{
    // The file is "executable" on disk, but exec() rejects it.
    QSignalSpy failed(m_manager, &EmulatorManager::launchFailed);
    QSignalSpy started(m_manager, &EmulatorManager::processStarted);
    m_manager->launch("garbage");
    QVERIFY(failed.count() == 1 || failed.wait(Timeout));
    QCOMPARE(started.count(), 0);
    QCOMPARE(m_manager->state(), EmulatorManager::Idle);
    QCOMPARE(m_manager->lastResult(), EmulatorManager::FailedToStart);
    QVERIFY(m_manager->lastErrorMessage().startsWith("Could not start Emulator garbage"));
    QVERIFY(m_manager->canRetry());
}

void TestEmulatorManager::unavailableRefused()
{
    QSignalSpy failed(m_manager, &EmulatorManager::launchFailed);
    QVERIFY(!m_manager->launch("off"));
    QVERIFY(!m_manager->launch("unknown-id"));
    QCOMPARE(failed.count(), 2);
    QCOMPARE(m_manager->state(), EmulatorManager::Idle);
    QCOMPARE(m_manager->lastResult(), EmulatorManager::NoResult);
}

void TestEmulatorManager::missingGameRefused()
{
    QSignalSpy failed(m_manager, &EmulatorManager::launchFailed);
    QVERIFY(!m_manager->launch("ok", m_dir->filePath("games/absent.mock")));
    QCOMPARE(failed.count(), 1);
    QVERIFY(failed.first().at(2).toString().contains("not found"));
    QCOMPARE(m_manager->state(), EmulatorManager::Idle);
}

void TestEmulatorManager::secondLaunchRefused()
{
    QSignalSpy started(m_manager, &EmulatorManager::processStarted);
    QSignalSpy failed(m_manager, &EmulatorManager::launchFailed);
    QSignalSpy finished(m_manager, &EmulatorManager::processFinished);
    QVERIFY(m_manager->launch("long"));
    QVERIFY(started.wait(Timeout));
    QVERIFY(!m_manager->launch("ok"));
    QCOMPARE(failed.count(), 1);
    QCOMPARE(failed.first().at(2).toString(), QStringLiteral("Another application is already running"));
    QCOMPARE(m_manager->activeEmulatorId(), QStringLiteral("long"));
    m_manager->terminate();
    QVERIFY(finished.wait(Timeout));
}

void TestEmulatorManager::terminateStops()
{
    QSignalSpy started(m_manager, &EmulatorManager::processStarted);
    QSignalSpy finished(m_manager, &EmulatorManager::processFinished);
    QVERIFY(m_manager->launch("long"));
    QVERIFY(started.wait(Timeout));
    QCOMPARE(m_manager->state(), EmulatorManager::Running);
    m_manager->terminate();
    QCOMPARE(m_manager->state(), EmulatorManager::Stopping);
    QVERIFY(finished.wait(Timeout));
    QCOMPARE(m_manager->state(), EmulatorManager::Idle);
    QCOMPARE(m_manager->lastResult(), EmulatorManager::NormalExit); // user stop is not an error
    QVERIFY(!m_manager->canRetry());
}

void TestEmulatorManager::terminateKillsStubbornProcess()
{
    m_manager->setKillTimeout(300);
    QSignalSpy finished(m_manager, &EmulatorManager::processFinished);
    QVERIFY(m_manager->launch("stubborn"));
    // The mock writes this file only after it has started ignoring SIGTERM.
    QTRY_VERIFY_WITH_TIMEOUT(QFile::exists(m_readyFile), Timeout);
    QElapsedTimer timer;
    timer.start();
    m_manager->terminate();
    QVERIFY(finished.wait(Timeout));
    QVERIFY(timer.elapsed() >= 250);
    QCOMPARE(m_manager->state(), EmulatorManager::Idle);
}

void TestEmulatorManager::retryRelaunches()
{
    QVERIFY(!m_manager->retry()); // nothing to retry yet
    QSignalSpy finished(m_manager, &EmulatorManager::processFinished);
    QVERIFY(m_manager->launch("fail"));
    QVERIFY(finished.wait(Timeout));
    QVERIFY(m_manager->canRetry());
    QSignalSpy started(m_manager, &EmulatorManager::processStarted);
    QVERIFY(m_manager->retry());
    QVERIFY(!m_manager->canRetry()); // busy
    QVERIFY(finished.wait(Timeout));
    QCOMPARE(started.count(), 1);
    QCOMPARE(finished.count(), 2);
    QCOMPARE(m_manager->lastEmulatorId(), QStringLiteral("fail"));
}

void TestEmulatorManager::argumentsAreSeparateArgv()
{
    const QString game = writeGame("Game With Spaces; $(rm -rf x) 'q'.mock");
    QSignalSpy finished(m_manager, &EmulatorManager::processFinished);
    QVERIFY(m_manager->launch("args", game));
    QVERIFY(finished.wait(Timeout));
    QCOMPARE(m_manager->lastResult(), EmulatorManager::NormalExit);
    const QJsonArray args = QJsonDocument::fromJson(testutils::readFile(m_argsFile)).array();
    QCOMPARE(args.toVariantList(), QVariantList({"--fullscreen", game, "two words"}));
}

void TestEmulatorManager::destructorKillsChild()
{
    QSignalSpy started(m_manager, &EmulatorManager::processStarted);
    QVERIFY(m_manager->launch("stubborn"));
    QVERIFY(started.wait(Timeout));
    QElapsedTimer timer;
    timer.start();
    delete m_manager;
    m_manager = nullptr;
    QVERIFY(timer.elapsed() < 5000);
}

QTEST_GUILESS_MAIN(TestEmulatorManager)
#include "tst_emulatormanager.moc"
