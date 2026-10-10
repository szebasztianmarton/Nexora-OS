// Drives the real EmulatorManager against the nexora-fake-emulator process.
#include "nexora/emulator/EmulatorManager.h"

#include <QElapsedTimer>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace nexora::emulator;
using nexora::shared::EmulatorConfig;
using namespace std::chrono_literals;

namespace {

constexpr int Timeout = 10000;

EmulatorConfig fake(const QStringList &arguments, const QString &id = QStringLiteral("fake"))
{
    return EmulatorConfig::make(id, QStringLiteral("Fake Emulator"), QStringLiteral(NEXORA_FAKE_EMULATOR), arguments);
}

QList<ProcessState> statesOf(const QSignalSpy &spy)
{
    QList<ProcessState> states;
    for (const auto &args : spy)
        states.append(args.at(0).value<ProcessState>());
    return states;
}

} // namespace

class TestEmulatorManager : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase() { QVERIFY2(QFile::exists(QStringLiteral(NEXORA_FAKE_EMULATOR)), NEXORA_FAKE_EMULATOR); }

    void normalExit()
    {
        EmulatorManager manager;
        QSignalSpy states(&manager, &EmulatorManager::stateChanged);
        QSignalSpy finished(&manager, &EmulatorManager::processFinished);

        const auto result = manager.launch(fake({}));
        QVERIFY(result.accepted);
        QCOMPARE(manager.state(), ProcessState::Starting);
        QCOMPARE(manager.activeEmulatorId(), QStringLiteral("fake"));

        QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, Timeout);
        QCOMPARE(statesOf(states),
                 QList<ProcessState>({ProcessState::Starting, ProcessState::Running, ProcessState::Exited}));
        const ProcessOutcome outcome = manager.lastOutcome();
        QCOMPARE(outcome.finalState, ProcessState::Exited);
        QCOMPARE(outcome.error, EmulatorError::None);
        QCOMPARE(outcome.exitCode, std::optional<int>(0));
        QVERIFY(!outcome.stoppedByRequest);
        QVERIFY(outcome.message.contains(QStringLiteral("exited normally")));
        QCOMPARE(finished.first().at(0).toString(), QStringLiteral("fake"));
    }

    void nonZeroExit()
    {
        EmulatorManager manager;
        QSignalSpy finished(&manager, &EmulatorManager::processFinished);
        QVERIFY(manager.launch(fake({QStringLiteral("--exit-code"), QStringLiteral("3")})).accepted);
        QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, Timeout);
        QCOMPARE(manager.state(), ProcessState::Failed);
        QCOMPARE(manager.lastOutcome().error, EmulatorError::NonZeroExit);
        QCOMPARE(manager.lastOutcome().exitCode, std::optional<int>(3));
        QVERIFY(manager.lastOutcome().message.contains(QStringLiteral("exit code 3")));
    }

    void crash_data()
    {
        QTest::addColumn<QString>("mode");
        QTest::newRow("segv") << QStringLiteral("segv");
        QTest::newRow("abort") << QStringLiteral("abort");
    }

    void crash()
    {
        QFETCH(QString, mode);
        EmulatorManager manager;
        QSignalSpy finished(&manager, &EmulatorManager::processFinished);
        QVERIFY(manager.launch(fake({QStringLiteral("--crash"), mode})).accepted);
        QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, Timeout);
        QCOMPARE(manager.state(), ProcessState::Failed);
        QCOMPARE(manager.lastOutcome().error, EmulatorError::Crashed);
        QVERIFY(!manager.lastOutcome().exitCode.has_value());
    }

    void missingExecutable()
    {
        QTemporaryDir dir;
        EmulatorManager manager;
        QSignalSpy states(&manager, &EmulatorManager::stateChanged);
        EmulatorConfig config = fake({});
        config.executable = dir.filePath(QStringLiteral("does-not-exist"));
        const auto result = manager.launch(config);
        QVERIFY(!result.accepted);
        QCOMPARE(result.error, EmulatorError::ExecutableMissing);
        QCOMPARE(statesOf(states), QList<ProcessState>({ProcessState::Failed}));
    }

    void failedToStart()
    {
        // Executable bit set but not a loadable program: passes validation, exec fails.
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("garbage"));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(QByteArray(64, '\x01'));
        file.close();
        QVERIFY(file.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));

        EmulatorManager manager;
        QSignalSpy finished(&manager, &EmulatorManager::processFinished);
        EmulatorConfig config = fake({});
        config.executable = path;
        QVERIFY(manager.launch(config).accepted);
        QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, Timeout);
        QCOMPARE(manager.state(), ProcessState::Failed);
        QCOMPARE(manager.lastOutcome().error, EmulatorError::FailedToStart);
    }

    void capturesOutput()
    {
        EmulatorManager manager;
        QSignalSpy output(&manager, &EmulatorManager::outputReceived);
        QSignalSpy finished(&manager, &EmulatorManager::processFinished);
        QVERIFY(manager
                    .launch(fake({QStringLiteral("--stdout"), QStringLiteral("hello out"), QStringLiteral("--stderr"),
                                  QStringLiteral("hello err"), QStringLiteral("--stdout"), QStringLiteral("ünïcödé")}))
                    .accepted);
        QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, Timeout);
        QVERIFY(manager.capturedStdout().contains(QStringLiteral("ready\n")));
        QVERIFY(manager.capturedStdout().contains(QStringLiteral("hello out\n")));
        QVERIFY(manager.capturedStdout().contains(QStringLiteral("ünïcödé")));
        QVERIFY(!manager.capturedStdout().contains(QStringLiteral("hello err")));
        QCOMPARE(manager.capturedStderr(), QStringLiteral("hello err\n"));
        QVERIFY(!output.isEmpty());
        bool sawStderr = false;
        for (const auto &args : output)
            sawStderr |= args.at(1).value<OutputChannel>() == OutputChannel::Stderr;
        QVERIFY(sawStderr);
    }

    void outputIsBounded()
    {
        EmulatorManager manager;
        manager.setOutputLimit(8);
        QSignalSpy finished(&manager, &EmulatorManager::processFinished);
        QVERIFY(manager.launch(fake({QStringLiteral("--stdout"), QStringLiteral("0123456789ABCDEF")})).accepted);
        QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, Timeout);
        QCOMPARE(manager.capturedStdout(), QStringLiteral("9ABCDEF\n"));
    }

    void delayedStartup()
    {
        EmulatorManager manager;
        QSignalSpy output(&manager, &EmulatorManager::outputReceived);
        QSignalSpy finished(&manager, &EmulatorManager::processFinished);
        QVERIFY(manager.launch(fake({QStringLiteral("--startup-delay-ms"), QStringLiteral("400")})).accepted);
        // The process counts as running as soon as exec succeeded, before it prints anything.
        QTRY_COMPARE_WITH_TIMEOUT(manager.state(), ProcessState::Running, Timeout);
        QCOMPARE(output.size(), 0);
        QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, Timeout);
        QCOMPARE(manager.state(), ProcessState::Exited);
        QVERIFY(manager.capturedStdout().contains(QStringLiteral("ready")));
    }

    void controlledStop()
    {
        EmulatorManager manager;
        QSignalSpy states(&manager, &EmulatorManager::stateChanged);
        QSignalSpy finished(&manager, &EmulatorManager::processFinished);
        QVERIFY(manager.launch(fake({QStringLiteral("--sleep-ms"), QStringLiteral("30000")})).accepted);
        QTRY_VERIFY_WITH_TIMEOUT(manager.capturedStdout().contains(QStringLiteral("ready")), Timeout);

        QVERIFY(manager.stop());
        QCOMPARE(manager.state(), ProcessState::Stopping);
        QVERIFY(manager.stop()); // idempotent while stopping
        QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, 5000);
        QCOMPARE(statesOf(states),
                 QList<ProcessState>(
                     {ProcessState::Starting, ProcessState::Running, ProcessState::Stopping, ProcessState::Exited}));
        QVERIFY(manager.lastOutcome().stoppedByRequest);
        QVERIFY(!manager.lastOutcome().killed);
        QCOMPARE(manager.lastOutcome().error, EmulatorError::None);
    }

    void stopEscalatesToKill()
    {
        EmulatorManager manager;
        manager.setStopTimeout(300ms);
        QSignalSpy finished(&manager, &EmulatorManager::processFinished);
        QVERIFY(manager
                    .launch(fake(
                        {QStringLiteral("--ignore-sigterm"), QStringLiteral("--sleep-ms"), QStringLiteral("30000")}))
                    .accepted);
        QTRY_VERIFY_WITH_TIMEOUT(manager.capturedStdout().contains(QStringLiteral("ready")), Timeout);

        QVERIFY(manager.stop());
        QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, 5000);
        QCOMPARE(manager.state(), ProcessState::Exited);
        QVERIFY(manager.lastOutcome().stoppedByRequest);
        QVERIFY(manager.lastOutcome().killed);
    }

    void rejectsSecondLaunchWhileActive()
    {
        EmulatorManager manager;
        QSignalSpy finished(&manager, &EmulatorManager::processFinished);
        QVERIFY(manager.launch(fake({QStringLiteral("--sleep-ms"), QStringLiteral("30000")})).accepted);
        QTRY_COMPARE_WITH_TIMEOUT(manager.state(), ProcessState::Running, Timeout);

        QSignalSpy states(&manager, &EmulatorManager::stateChanged);
        const auto second = manager.launch(fake({}, QStringLiteral("other")));
        QVERIFY(!second.accepted);
        QCOMPARE(second.error, EmulatorError::AlreadyActive);
        QCOMPARE(manager.state(), ProcessState::Running);
        QCOMPARE(manager.activeEmulatorId(), QStringLiteral("fake"));
        QCOMPARE(states.size(), 0);

        QVERIFY(manager.stop());
        QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, Timeout);
    }

    void relaunchAfterExit()
    {
        EmulatorManager manager;
        QSignalSpy finished(&manager, &EmulatorManager::processFinished);
        for (int i = 0; i < 3; ++i) {
            QVERIFY(manager.launch(fake({}, QStringLiteral("run-%1").arg(i))).accepted);
            QTRY_COMPARE_WITH_TIMEOUT(finished.size(), i + 1, Timeout);
            QCOMPARE(manager.state(), ProcessState::Exited);
            QCOMPARE(manager.activeEmulatorId(), QStringLiteral("run-%1").arg(i));
        }
    }

    void relaunchFromFinishedSlot()
    {
        // Listeners may start the next emulator directly from processFinished.
        EmulatorManager manager;
        int finishedCount = 0;
        connect(&manager, &EmulatorManager::processFinished, this, [&] {
            if (++finishedCount == 1)
                QVERIFY(manager.launch(fake({}, QStringLiteral("second"))).accepted);
        });
        QVERIFY(manager.launch(fake({}, QStringLiteral("first"))).accepted);
        QTRY_COMPARE_WITH_TIMEOUT(finishedCount, 2, Timeout);
        QCOMPARE(manager.activeEmulatorId(), QStringLiteral("second"));
        QCOMPARE(manager.state(), ProcessState::Exited);
    }

    void recoversAfterFailures()
    {
        QTemporaryDir dir;
        EmulatorManager manager;
        QSignalSpy finished(&manager, &EmulatorManager::processFinished);

        EmulatorConfig missing = fake({});
        missing.executable = dir.filePath(QStringLiteral("missing"));
        QVERIFY(!manager.launch(missing).accepted);
        QCOMPARE(manager.state(), ProcessState::Failed);

        QVERIFY(manager.launch(fake({QStringLiteral("--crash"), QStringLiteral("segv")})).accepted);
        QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 2, Timeout);
        QCOMPARE(manager.state(), ProcessState::Failed);

        QVERIFY(manager.launch(fake({})).accepted);
        QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 3, Timeout);
        QCOMPARE(manager.state(), ProcessState::Exited);
        QCOMPARE(manager.lastOutcome().error, EmulatorError::None);
    }

    void launchWithProfileAndGame()
    {
        QTemporaryDir dir;
        const QString game = dir.filePath(QStringLiteral("My Game (EU).sfc"));
        QFile f(game);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.close();

        EmulatorConfig config = fake({QStringLiteral("--echo-args"), QStringLiteral("{file}")});
        nexora::shared::LaunchProfile profile;
        profile.id = QStringLiteral("cwd");
        profile.name = QStringLiteral("Print cwd");
        profile.emulatorId = config.id;
        profile.hasArguments = true;
        profile.arguments
            = {QStringLiteral("--print-cwd"), QStringLiteral("--echo-args"), QStringLiteral("game={file}")};
        profile.workingDirectory = dir.path();
        config.profiles.append(profile);

        EmulatorManager manager;
        QSignalSpy finished(&manager, &EmulatorManager::processFinished);
        QVERIFY(manager.launch(LaunchRequest{config, {}, game, QStringLiteral("My Game")}).accepted);
        QCOMPARE(manager.activeEmulatorId(), QStringLiteral("fake"));
        QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, Timeout);
        // The path is passed as one argument, spaces and parentheses intact (no shell).
        QVERIFY2(manager.capturedStdout().contains(QStringLiteral("arg: ") + game + u'\n'),
                 qPrintable(manager.capturedStdout()));

        QVERIFY(manager.launch(LaunchRequest{config, QStringLiteral("cwd"), game, {}}).accepted);
        QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 2, Timeout);
        QCOMPARE(manager.state(), ProcessState::Exited);
        const QString out = manager.capturedStdout();
        QVERIFY2(out.contains(QStringLiteral("cwd: ") + QFileInfo(dir.path()).canonicalFilePath()), qPrintable(out));
        QVERIFY2(out.contains(QStringLiteral("arg: game=") + game), qPrintable(out));
        QVERIFY(!out.contains(QStringLiteral("{file}")));
    }

    void configStopTimeoutIsUsed()
    {
        EmulatorConfig config
            = fake({QStringLiteral("--ignore-sigterm"), QStringLiteral("--sleep-ms"), QStringLiteral("30000")});
        config.stopTimeoutMs = 500;
        EmulatorManager manager; // default timeout stays at 5 s
        QSignalSpy finished(&manager, &EmulatorManager::processFinished);
        QVERIFY(manager.launch(config).accepted);
        QTRY_VERIFY_WITH_TIMEOUT(manager.capturedStdout().contains(QStringLiteral("ready")), Timeout);
        QElapsedTimer timer;
        timer.start();
        QVERIFY(manager.stop());
        QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, Timeout);
        QVERIFY(manager.lastOutcome().killed);
        QVERIFY2(timer.elapsed() < 4000, qPrintable(QString::number(timer.elapsed())));
    }

    void destroyWhileRunning()
    {
        auto manager = std::make_unique<EmulatorManager>();
        QVERIFY(manager
                    ->launch(fake(
                        {QStringLiteral("--ignore-sigterm"), QStringLiteral("--sleep-ms"), QStringLiteral("30000")}))
                    .accepted);
        QTRY_VERIFY_WITH_TIMEOUT(manager->capturedStdout().contains(QStringLiteral("ready")), Timeout);
        manager.reset(); // must kill the child without hanging or crashing
    }
};

QTEST_GUILESS_MAIN(TestEmulatorManager)
#include "tst_emulatormanager.moc"
