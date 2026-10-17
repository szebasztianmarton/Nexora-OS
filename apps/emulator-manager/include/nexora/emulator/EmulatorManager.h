#pragma once

#include "nexora/emulator/EmulatorProcessState.h"
#include "nexora/shared/EmulatorConfig.h"

#include <QDateTime>
#include <QElapsedTimer>
#include <QFile>
#include <QObject>
#include <QPointer>
#include <QProcess>
#include <QStringDecoder>
#include <QTimer>

#include <chrono>
#include <memory>
#include <optional>

namespace nexora::emulator {

enum class OutputChannel { Stdout, Stderr };

// Final result of one launch attempt.
struct ProcessOutcome
{
    ProcessState finalState = ProcessState::Idle; // Exited or Failed
    EmulatorError error = EmulatorError::None;
    std::optional<int> exitCode; // set for normal (non-crash) exits
    bool stoppedByRequest = false; // stop() was called
    bool killed = false; // graceful stop timed out and SIGKILL was sent
    QString message; // user-facing summary
    bool startupFailure = false; // failed (non-zero exit / crash) within the startup grace period
    qint64 pid = 0; // 0 if the process never started
    QDateTime startedAt; // UTC; invalid if the process never started
    QDateTime endedAt; // UTC
};

// What to start: an emulator, optionally one of its launch profiles and a game file.
//
// Arguments: the profile's arguments replace the emulator's when the profile defines
// them. With a game file, every argument containing "{file}" has it replaced by the
// absolute game path; if no argument contains it, the path is appended as the last
// argument. Without a game file, arguments containing "{file}" are dropped. The game file
// is only ever passed as an argument; it is never executed. |extraArguments| (from a
// save-state adapter, already substituted) are appended after everything else.
struct LaunchRequest
{
    shared::EmulatorConfig emulator;
    QString profileId; // empty = emulator defaults
    QString gamePath; // empty = start the emulator without a game
    QString displayName; // empty = emulator name
    QStringList extraArguments;
};

struct ResolvedLaunch
{
    QString program;
    QStringList arguments;
    QString workingDirectory;
    std::optional<std::chrono::milliseconds> stopTimeout;
    QString displayName;
};

struct LaunchResult
{
    bool accepted = false;
    EmulatorError error = EmulatorError::None;
    QString message;
};

// Starts and supervises at most one external emulator process.
//
// Processes are started directly via QProcess with an explicit argument list; no shell is
// ever involved. All operations are asynchronous: nothing here blocks the calling (UI)
// thread except shutdown(), which is meant for application exit.
class EmulatorManager : public QObject
{
    Q_OBJECT

public:
    explicit EmulatorManager(QObject *parent = nullptr);
    ~EmulatorManager() override;

    // Validates |config| and starts its process. Rejected with AlreadyActive (state
    // unchanged) while another process is active. Validation failures move the state to
    // Failed synchronously and emit processFinished.
    LaunchResult launch(const LaunchRequest &request);
    LaunchResult launch(const shared::EmulatorConfig &config) { return launch(LaunchRequest{config, {}, {}, {}, {}}); }

    // Validates |request| and computes the exact program, arguments and working directory.
    // Performs filesystem checks but starts nothing.
    static std::optional<ResolvedLaunch> resolve(const LaunchRequest &request, EmulatorError *error, QString *detail);

    // Requests a graceful stop (SIGTERM), escalating to SIGKILL after stopTimeout().
    // Returns false if no process is active.
    bool stop();

    // Suspends / resumes the active process with SIGSTOP / SIGCONT. Only allowed while Running
    // and only when the active emulator config has pausable=true; otherwise returns false.
    // Both are idempotent (pause while paused / resume while not paused: see below).
    bool pause();
    bool resume();
    bool isPaused() const { return m_paused; }
    qint64 activePid() const { return m_pid; }
    QDateTime startedAt() const { return m_startedAt; }

    // Application exit: SIGTERM to the process group, SIGKILL if it is still running after
    // min(stop timeout, ShutdownGraceLimit). Blocks (bounded); emits nothing.
    void shutdown();
    static constexpr std::chrono::milliseconds ShutdownGraceLimit{3000};

    ProcessState state() const { return m_state; }
    bool isActive() const { return emulator::isActive(m_state); }
    QString activeEmulatorId() const { return m_activeId; }
    const ProcessOutcome &lastOutcome() const { return m_lastOutcome; }

    // Bounded tail of the captured output of the current / most recent process.
    QString capturedStdout() const { return m_stdout; }
    QString capturedStderr() const { return m_stderr; }

    std::chrono::milliseconds stopTimeout() const { return m_stopTimeout; }
    void setStopTimeout(std::chrono::milliseconds timeout) { m_stopTimeout = timeout; }
    // A non-zero exit or a crash this soon after the process started counts as a startup failure.
    // Zero disables the detection.
    std::chrono::milliseconds startupGrace() const { return m_startupGrace; }
    void setStartupGrace(std::chrono::milliseconds grace) { m_startupGrace = grace; }
    qsizetype outputLimit() const { return m_outputLimit; }
    void setOutputLimit(qsizetype characters) { m_outputLimit = qMax<qsizetype>(1, characters); }

    // Optional on-disk output log. When a directory is set, every accepted launch writes
    // <dir>/<emulator id>.log (header, stdout/stderr lines prefixed "out| " / "err| ",
    // outcome footer). The previous run's file is kept as <id>.log.1, so relaunching after
    // a crash does not overwrite the crash output. Each file is capped at outputLogLimit()
    // bytes. Empty (the default) disables the log. Failure to write only logs a warning.
    void setOutputLogDirectory(const QString &directory) { m_outputLogDirectory = directory; }
    QString outputLogDirectory() const { return m_outputLogDirectory; }
    void setOutputLogLimit(qint64 bytes) { m_outputLogLimit = qMax<qint64>(1024, bytes); }
    qint64 outputLogLimit() const { return m_outputLogLimit; }
    // Log file of the current / most recent launch; empty when none was written.
    QString outputLogPath() const { return m_outputLogPath; }

    // Checks that |path| is absolute, exists, is a regular file and is executable.
    static EmulatorError validateExecutable(const QString &path, QString *detail = nullptr);

signals:
    void stateChanged(nexora::emulator::ProcessState state, const QString &emulatorId);
    void outputReceived(const QString &emulatorId, nexora::emulator::OutputChannel channel, const QString &text);
    void processFinished(const QString &emulatorId, const nexora::emulator::ProcessOutcome &outcome);
    void pausedChanged(bool paused);

private:
    void setState(ProcessState state);
    void readChannel(OutputChannel channel);
    void appendOutput(OutputChannel channel, const QString &text);
    void onStarted();
    void onErrorOccurred(QProcess::ProcessError error);
    void onFinished(int exitCode, QProcess::ExitStatus status);
    void onStopTimeout();
    // Sends |signal| to the emulator's whole process group (the child is made a group
    // leader at fork time, so helpers and grandchildren it spawns are included). Falls back
    // to the direct child if the group is gone. Returns false while no pid is known yet.
    bool signalGroup(int signal);
    void finish(ProcessOutcome outcome);
    void releaseProcess();
    void openOutputLog(const ResolvedLaunch &launch);
    void writeOutputLog(OutputChannel channel, const QString &text);
    void closeOutputLog(const QString &footer);
    void writeLogBytes(const QByteArray &bytes);
    void setPaused(bool paused);

    ProcessState m_state = ProcessState::Idle;
    QPointer<QProcess> m_process;
    QString m_activeId;
    QString m_activeName;
    QTimer m_killTimer;
    std::chrono::milliseconds m_stopTimeout{5000};
    std::chrono::milliseconds m_activeStopTimeout{5000};
    bool m_stopRequested = false;
    bool m_killed = false;
    bool m_terminatePending = false; // stop() came before the pid was known; see onStarted()
    bool m_activePausable = false;
    bool m_paused = false;
    qint64 m_pid = 0;
    QDateTime m_startedAt;
    QElapsedTimer m_runClock;
    std::chrono::milliseconds m_startupGrace{3000};
    ProcessOutcome m_lastOutcome;
    QString m_stdout;
    QString m_stderr;
    qsizetype m_outputLimit = 64 * 1024;
    QStringDecoder m_stdoutDecoder{QStringDecoder::Utf8};
    QStringDecoder m_stderrDecoder{QStringDecoder::Utf8};
    QString m_outputLogDirectory;
    qint64 m_outputLogLimit = 1024 * 1024;
    QString m_outputLogPath;
    std::unique_ptr<QFile> m_outputLog;
    qint64 m_outputLogWritten = 0;
    bool m_outputLogTruncated = false;
    QString m_pendingLine[2]; // incomplete last line per channel (stdout, stderr)
};

} // namespace nexora::emulator

Q_DECLARE_METATYPE(nexora::emulator::ProcessOutcome)
