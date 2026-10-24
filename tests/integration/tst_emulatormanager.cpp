// Drives the real EmulatorManager against the nexora-fake-emulator process.
#include "nexora/emulator/EmulatorManager.h"

#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <csignal>
#include <sys/types.h>

using namespace nexora::emulator;
using nexora::shared::EmulatorConfig;
using namespace std::chrono_literals;

namespace {

constexpr int Timeout = 10000;

EmulatorConfig fake(const QStringList &arguments, const QString &id = QStringLiteral("fake"))
{
    return EmulatorConfig::make(id, QStringLiteral("Fake Emulator"), QStringLiteral(NEXORA_FAKE_EMULATOR), arguments);
}

// Field 3 of /proc/<pid>/stat (R, S, T, Z, ...); a NUL character if it cannot be read.
QChar procState(qint64 pid)
{
    QFile file(QStringLiteral("/proc/%1/stat").arg(pid));
    if (!file.open(QIODevice::ReadOnly))
        return QChar();
    const QByteArray text = file.readAll();
    const qsizetype close = text.lastIndexOf(')'); // the command name may contain spaces
    return close < 0 || close + 2 >= text.size() ? QChar() : QChar(text.at(close + 2));
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
        manager.setStartupGrace(0ms); // an immediate exit is a plain non-zero exit here
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

    void outputLogRecordsRunAndKeepsPreviousOnRelaunch()
    {
        QTemporaryDir logs;
        EmulatorManager manager;
        manager.setOutputLogDirectory(logs.filePath(QStringLiteral("sub")));
        QSignalSpy finished(&manager, &EmulatorManager::processFinished);

        // First run crashes after printing; its output must survive the relaunch.
        QVERIFY(manager
                    .launch(fake({QStringLiteral("--stdout"), QStringLiteral("hello out"), QStringLiteral("--stderr"),
                                  QStringLiteral("bad thing"), QStringLiteral("--crash"), QStringLiteral("segv")}))
                    .accepted);
        QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, Timeout);
        const QString path = logs.filePath(QStringLiteral("sub/fake.log"));
        QCOMPARE(manager.outputLogPath(), path);
        QFile first(path);
        QVERIFY(first.open(QIODevice::ReadOnly));
        const QString crashLog = QString::fromUtf8(first.readAll());
        first.close();
        QVERIFY2(crashLog.contains(QStringLiteral("# program: ") + QStringLiteral(NEXORA_FAKE_EMULATOR)),
                 qPrintable(crashLog));
        QVERIFY2(crashLog.contains(QStringLiteral("out| hello out\n")), qPrintable(crashLog));
        QVERIFY2(crashLog.contains(QStringLiteral("err| bad thing\n")), qPrintable(crashLog));
        QVERIFY2(crashLog.contains(QStringLiteral("finished: ")), qPrintable(crashLog));
        QVERIFY2(crashLog.contains(QStringLiteral("crashed"), Qt::CaseInsensitive), qPrintable(crashLog));

        QVERIFY(manager.launch(fake({QStringLiteral("--stdout"), QStringLiteral("second run")})).accepted);
        QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 2, Timeout);
        QFile previous(path + QStringLiteral(".1"));
        QVERIFY(previous.open(QIODevice::ReadOnly));
        QCOMPARE(QString::fromUtf8(previous.readAll()), crashLog);
        QFile current(path);
        QVERIFY(current.open(QIODevice::ReadOnly));
        const QString second = QString::fromUtf8(current.readAll());
        QVERIFY(second.contains(QStringLiteral("out| second run\n")));
        QVERIFY(!second.contains(QStringLiteral("hello out")));
    }

    void outputLogIsBounded()
    {
        QTemporaryDir logs;
        EmulatorManager manager;
        manager.setOutputLogDirectory(logs.path());
        manager.setOutputLogLimit(2048);
        QSignalSpy finished(&manager, &EmulatorManager::processFinished);
        QStringList args;
        for (int i = 0; i < 100; ++i)
            args << QStringLiteral("--stdout") << QStringLiteral("line %1 %2").arg(i).arg(QString(60, u'x'));
        QVERIFY(manager.launch(fake(args)).accepted);
        QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, Timeout);
        const QFileInfo info(manager.outputLogPath());
        QVERIFY(info.exists());
        QVERIFY2(info.size() < 2048 + 512, qPrintable(QString::number(info.size())));
        QFile file(info.filePath());
        QVERIFY(file.open(QIODevice::ReadOnly));
        const QString text = QString::fromUtf8(file.readAll());
        QVERIFY(text.contains(QStringLiteral("output log limit reached")));
        QVERIFY(text.contains(QStringLiteral("finished: ")));
        QVERIFY(!text.contains(QStringLiteral("line 99 ")));
    }

    void outputLogDisabledByDefault()
    {
        EmulatorManager manager;
        QSignalSpy finished(&manager, &EmulatorManager::processFinished);
        QVERIFY(manager.launch(fake({QStringLiteral("--stdout"), QStringLiteral("x")})).accepted);
        QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, Timeout);
        QVERIFY(manager.outputLogPath().isEmpty());
        QCOMPARE(manager.capturedStdout(), QStringLiteral("ready\nx\n"));
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

    // Application exit: SIGTERM first (the emulator may write its own saves), SIGKILL only
    // for one that ignores it.
    void shutdownTerminatesBeforeKilling_data()
    {
        QTest::addColumn<bool>("ignoreSigterm");
        QTest::addColumn<QString>("footer");
        QTest::newRow("exits on SIGTERM") << false << QStringLiteral("was stopped");
        QTest::newRow("ignores SIGTERM") << true << QStringLiteral("did not stop in time and was terminated forcefully");
    }

    void shutdownTerminatesBeforeKilling()
    {
        QFETCH(bool, ignoreSigterm);
        QFETCH(QString, footer);
        QTemporaryDir logs;
        EmulatorManager manager;
        manager.setOutputLogDirectory(logs.path());
        manager.setStopTimeout(300ms);
        QStringList args{QStringLiteral("--sleep-ms"), QStringLiteral("30000")};
        if (ignoreSigterm)
            args.prepend(QStringLiteral("--ignore-sigterm"));
        QVERIFY(manager.launch(fake(args)).accepted);
        QTRY_VERIFY_WITH_TIMEOUT(manager.capturedStdout().contains(QStringLiteral("ready")), Timeout);
        const qint64 pid = manager.activePid();
        QElapsedTimer clock;
        clock.start();
        manager.shutdown();
        QVERIFY(clock.elapsed() < 3000);
        QVERIFY(!QFileInfo::exists(QStringLiteral("/proc/%1/stat").arg(pid)));
        QFile log(manager.outputLogPath());
        QVERIFY(log.open(QIODevice::ReadOnly));
        QVERIFY2(QString::fromUtf8(log.readAll()).contains(footer), qPrintable(footer));
    }

    // shutdown() ends the run like stop() does, so the final run reaches launch history.
    void shutdownEndsTheRunLikeStop_data()
    {
        QTest::addColumn<bool>("ignoreSigterm");
        QTest::newRow("exits on SIGTERM") << false;
        QTest::newRow("ignores SIGTERM") << true;
    }

    void shutdownEndsTheRunLikeStop()
    {
        QFETCH(bool, ignoreSigterm);
        EmulatorManager manager;
        manager.setStopTimeout(300ms);
        QStringList args{QStringLiteral("--sleep-ms"), QStringLiteral("30000")};
        if (ignoreSigterm)
            args.prepend(QStringLiteral("--ignore-sigterm"));
        QVERIFY(manager.launch(fake(args)).accepted);
        QTRY_VERIFY_WITH_TIMEOUT(manager.capturedStdout().contains(QStringLiteral("ready")), Timeout);
        QSignalSpy finished(&manager, &EmulatorManager::processFinished);
        QSignalSpy states(&manager, &EmulatorManager::stateChanged);
        manager.shutdown();
        QCOMPARE(manager.state(), ProcessState::Exited);
        QVERIFY(!manager.isActive());
        QCOMPARE(finished.count(), 1);
        const auto outcome = finished.first().at(1).value<ProcessOutcome>();
        QCOMPARE(outcome.finalState, ProcessState::Exited);
        QVERIFY(outcome.stoppedByRequest);
        QCOMPARE(outcome.killed, ignoreSigterm);
        QCOMPARE(states.last().at(0).value<ProcessState>(), ProcessState::Exited);
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
        QVERIFY(manager.launch(LaunchRequest{config, {}, game, QStringLiteral("My Game"), {}}).accepted);
        QCOMPARE(manager.activeEmulatorId(), QStringLiteral("fake"));
        QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, Timeout);
        // The path is passed as one argument, spaces and parentheses intact (no shell).
        QVERIFY2(manager.capturedStdout().contains(QStringLiteral("arg: ") + game + u'\n'),
                 qPrintable(manager.capturedStdout()));

        QVERIFY(manager.launch(LaunchRequest{config, QStringLiteral("cwd"), game, {}, {}}).accepted);
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

    void immediateNonZeroExitIsStartupFailure()
    {
        EmulatorManager manager;
        QSignalSpy finished(&manager, &EmulatorManager::processFinished);
        QVERIFY(manager.launch(fake({QStringLiteral("--exit-code"), QStringLiteral("2")})).accepted);
        QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, Timeout);
        const ProcessOutcome outcome = manager.lastOutcome();
        QVERIFY(outcome.startupFailure);
        QCOMPARE(outcome.finalState, ProcessState::Failed);
        QCOMPARE(outcome.error, EmulatorError::NonZeroExit);
        QCOMPARE(outcome.message, QStringLiteral("Fake Emulator failed during startup (exit 2)"));
        QVERIFY(outcome.pid > 0);
        QVERIFY(outcome.startedAt.isValid());
        QVERIFY(outcome.endedAt.isValid());
        QVERIFY(outcome.endedAt >= outcome.startedAt);
    }

    void immediateCrashIsStartupFailure()
    {
        EmulatorManager manager;
        QSignalSpy finished(&manager, &EmulatorManager::processFinished);
        QVERIFY(manager.launch(fake({QStringLiteral("--crash"), QStringLiteral("abort")})).accepted);
        QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, Timeout);
        QVERIFY(manager.lastOutcome().startupFailure);
        QCOMPARE(manager.lastOutcome().error, EmulatorError::Crashed);
    }

    void failureAfterGraceIsNotStartupFailure()
    {
        EmulatorManager manager;
        manager.setStartupGrace(100ms);
        QSignalSpy finished(&manager, &EmulatorManager::processFinished);
        QVERIFY(manager
                    .launch(fake({QStringLiteral("--sleep-ms"), QStringLiteral("700"), QStringLiteral("--exit-code"),
                                  QStringLiteral("2")}))
                    .accepted);
        QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, Timeout);
        QVERIFY(!manager.lastOutcome().startupFailure);
        QCOMPARE(manager.lastOutcome().error, EmulatorError::NonZeroExit);
        QVERIFY(manager.lastOutcome().message.contains(QStringLiteral("exit code 2")));
    }

    void cleanExitAfterGraceIsNotStartupFailure()
    {
        EmulatorManager manager;
        manager.setStartupGrace(100ms);
        QSignalSpy finished(&manager, &EmulatorManager::processFinished);
        QVERIFY(manager.launch(fake({QStringLiteral("--sleep-ms"), QStringLiteral("700")})).accepted);
        QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, Timeout);
        QVERIFY(!manager.lastOutcome().startupFailure);
        QCOMPARE(manager.lastOutcome().finalState, ProcessState::Exited);
    }

    void zeroExitWithinGraceIsNotStartupFailure()
    {
        EmulatorManager manager;
        QSignalSpy finished(&manager, &EmulatorManager::processFinished);
        QVERIFY(manager.launch(fake({})).accepted);
        QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, Timeout);
        QVERIFY(!manager.lastOutcome().startupFailure);
    }

    void stopIsNeverStartupFailure()
    {
        EmulatorManager manager;
        QSignalSpy finished(&manager, &EmulatorManager::processFinished);
        QVERIFY(manager.launch(fake({QStringLiteral("--sleep-ms"), QStringLiteral("30000")})).accepted);
        QTRY_VERIFY_WITH_TIMEOUT(manager.capturedStdout().contains(QStringLiteral("ready")), Timeout);
        QVERIFY(manager.stop());
        QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, Timeout);
        QVERIFY(!manager.lastOutcome().startupFailure);
    }

    void pidAndStartTimeArePopulated()
    {
        EmulatorManager manager;
        QSignalSpy finished(&manager, &EmulatorManager::processFinished);
        QCOMPARE(manager.activePid(), qint64(0));
        const QDateTime before = QDateTime::currentDateTimeUtc().addSecs(-1);
        QVERIFY(manager.launch(fake({QStringLiteral("--sleep-ms"), QStringLiteral("30000")})).accepted);
        QTRY_COMPARE_WITH_TIMEOUT(manager.state(), ProcessState::Running, Timeout);
        const qint64 pid = manager.activePid();
        QVERIFY(pid > 0);
        QVERIFY(QFile::exists(QStringLiteral("/proc/%1").arg(pid)));
        QVERIFY(manager.startedAt().isValid());
        QVERIFY(manager.startedAt() >= before);

        QVERIFY(manager.stop());
        QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, Timeout);
        QCOMPARE(manager.lastOutcome().pid, pid);
        QCOMPARE(manager.lastOutcome().startedAt, manager.startedAt());
        QVERIFY(manager.lastOutcome().endedAt >= manager.lastOutcome().startedAt);
        QCOMPARE(manager.activePid(), qint64(0));
    }

    void pauseAndResume()
    {
        EmulatorConfig config = fake({QStringLiteral("--sleep-ms"), QStringLiteral("30000")});
        config.pausable = true;
        EmulatorManager manager;
        QSignalSpy paused(&manager, &EmulatorManager::pausedChanged);
        QSignalSpy finished(&manager, &EmulatorManager::processFinished);
        QVERIFY(!manager.pause()); // nothing running yet
        QVERIFY(manager.launch(config).accepted);
        QTRY_COMPARE_WITH_TIMEOUT(manager.state(), ProcessState::Running, Timeout);
        QTRY_VERIFY_WITH_TIMEOUT(manager.capturedStdout().contains(QStringLiteral("ready")), Timeout);
        const qint64 pid = manager.activePid();

        QVERIFY(manager.pause());
        QVERIFY(manager.isPaused());
        QCOMPARE(manager.state(), ProcessState::Running);
        QTRY_COMPARE_WITH_TIMEOUT(procState(pid), u'T', Timeout);
        QVERIFY(manager.pause()); // idempotent
        QCOMPARE(paused.size(), 1);
        QCOMPARE(paused.last().at(0).toBool(), true);

        QVERIFY(manager.resume());
        QVERIFY(!manager.isPaused());
        QTRY_VERIFY_WITH_TIMEOUT(procState(pid) != u'T', Timeout);
        QCOMPARE(paused.size(), 2);
        QCOMPARE(paused.last().at(0).toBool(), false);
        QVERIFY(!manager.resume()); // not paused any more

        QVERIFY(manager.stop());
        QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, Timeout);
    }

    void pauseRejectedWhenNotPausable()
    {
        EmulatorManager manager;
        QSignalSpy paused(&manager, &EmulatorManager::pausedChanged);
        QSignalSpy finished(&manager, &EmulatorManager::processFinished);
        QVERIFY(manager.launch(fake({QStringLiteral("--sleep-ms"), QStringLiteral("30000")})).accepted);
        QTRY_VERIFY_WITH_TIMEOUT(manager.capturedStdout().contains(QStringLiteral("ready")), Timeout);
        const qint64 pid = manager.activePid();
        QVERIFY(!manager.pause());
        QVERIFY(!manager.isPaused());
        QCOMPARE(paused.size(), 0);
        QVERIFY(procState(pid) != u'T');
        QVERIFY(manager.stop());
        QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, Timeout);
    }

    void stopWhilePausedTerminates()
    {
        EmulatorConfig config = fake({QStringLiteral("--sleep-ms"), QStringLiteral("30000")});
        config.pausable = true;
        EmulatorManager manager;
        QSignalSpy paused(&manager, &EmulatorManager::pausedChanged);
        QSignalSpy finished(&manager, &EmulatorManager::processFinished);
        QVERIFY(manager.launch(config).accepted);
        QTRY_VERIFY_WITH_TIMEOUT(manager.capturedStdout().contains(QStringLiteral("ready")), Timeout);
        const qint64 pid = manager.activePid();
        QVERIFY(manager.pause());
        QTRY_COMPARE_WITH_TIMEOUT(procState(pid), u'T', Timeout);

        QVERIFY(manager.stop());
        QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, 4000); // well below the 5 s kill timeout
        QVERIFY(!manager.lastOutcome().killed);
        QVERIFY(manager.lastOutcome().stoppedByRequest);
        QVERIFY(!manager.isPaused());
        QCOMPARE(paused.last().at(0).toBool(), false);
    }

    void stopDuringStartingIsFast()
    {
        EmulatorManager manager;
        manager.setStopTimeout(5000ms);
        QSignalSpy finished(&manager, &EmulatorManager::processFinished);
        QVERIFY(manager.launch(fake({QStringLiteral("--sleep-ms"), QStringLiteral("30000")})).accepted);
        QCOMPARE(manager.state(), ProcessState::Starting);
        QElapsedTimer clock;
        clock.start();
        QVERIFY(manager.stop());
        QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, Timeout);
        QVERIFY2(clock.elapsed() < 2500, qPrintable(QString::number(clock.elapsed())));
        QVERIFY(manager.lastOutcome().stoppedByRequest);
        QVERIFY(!manager.lastOutcome().killed);
    }

    void stopTerminatesGrandchildren_data()
    {
        QTest::addColumn<bool>("ignoreSigterm");
        QTest::addColumn<bool>("pauseFirst");
        QTest::newRow("SIGTERM") << false << false;
        QTest::newRow("SIGKILL escalation") << true << false;
        QTest::newRow("while paused") << false << true;
    }

    void stopTerminatesGrandchildren()
    {
        QFETCH(bool, ignoreSigterm);
        QFETCH(bool, pauseFirst);
        QStringList args{QStringLiteral("--spawn-child"), QStringLiteral("--sleep-ms"), QStringLiteral("30000")};
        if (ignoreSigterm)
            args.prepend(QStringLiteral("--ignore-sigterm"));
        EmulatorConfig config = fake(args);
        config.pausable = true;
        EmulatorManager manager;
        manager.setStopTimeout(500ms);
        QSignalSpy finished(&manager, &EmulatorManager::processFinished);
        QVERIFY(manager.launch(config).accepted);
        QTRY_VERIFY_WITH_TIMEOUT(manager.capturedStdout().contains(QStringLiteral("child: ")), Timeout);
        const QString out = manager.capturedStdout();
        const qsizetype at = out.indexOf(QStringLiteral("child: ")) + 7;
        const qint64 child = out.mid(at, out.indexOf(u'\n', at) - at).toLongLong();
        QVERIFY(child > 0);
        auto alive = [child] {
            const QChar s = procState(child);
            return !s.isNull() && s != u'Z' && s != u'X';
        };
        // Never leak a sleeper into the CI host, even when a check below fails.
        const auto reap = qScopeGuard([&] {
            if (alive())
                ::kill(pid_t(child), SIGKILL);
        });
        QVERIFY(alive());
        if (pauseFirst) {
            QVERIFY(manager.pause());
            QTRY_COMPARE_WITH_TIMEOUT(procState(child), u'T', Timeout); // the whole group stops
        }

        QVERIFY(manager.stop());
        QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, Timeout);
        QCOMPARE(manager.lastOutcome().killed, ignoreSigterm);
        QTRY_VERIFY_WITH_TIMEOUT(!alive(), 3000);
    }

    void duplicateLaunchKeepsPid()
    {
        EmulatorManager manager;
        QSignalSpy finished(&manager, &EmulatorManager::processFinished);
        QVERIFY(manager.launch(fake({QStringLiteral("--sleep-ms"), QStringLiteral("30000")})).accepted);
        QTRY_COMPARE_WITH_TIMEOUT(manager.state(), ProcessState::Running, Timeout);
        const qint64 pid = manager.activePid();
        const auto second = manager.launch(fake({}, QStringLiteral("other")));
        QVERIFY(!second.accepted);
        QCOMPARE(second.error, EmulatorError::AlreadyActive);
        QCOMPARE(manager.activePid(), pid);
        QVERIFY(manager.stop());
        QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, Timeout);
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

    // Destruction ends the run silently: an owner's slots may touch members that are already
    // destroyed (members declared after the manager), so no signal may reach them.
    void destroyWhileRunningNotifiesNothing()
    {
        auto manager = std::make_unique<EmulatorManager>();
        manager->setStopTimeout(300ms);
        QVERIFY(manager->launch(fake({QStringLiteral("--sleep-ms"), QStringLiteral("30000")})).accepted);
        QTRY_VERIFY_WITH_TIMEOUT(manager->capturedStdout().contains(QStringLiteral("ready")), Timeout);
        int notifications = 0;
        connect(manager.get(), &EmulatorManager::stateChanged, [&notifications] { ++notifications; });
        connect(manager.get(), &EmulatorManager::pausedChanged, [&notifications] { ++notifications; });
        connect(manager.get(), &EmulatorManager::processFinished, [&notifications] { ++notifications; });
        manager.reset();
        QCOMPARE(notifications, 0);
    }
};

QTEST_GUILESS_MAIN(TestEmulatorManager)
#include "tst_emulatormanager.moc"
