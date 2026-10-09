#pragma once

#include "nexora/emulator/EmulatorProcessState.h"
#include "nexora/shared/EmulatorConfig.h"

#include <QObject>
#include <QPointer>
#include <QProcess>
#include <QStringDecoder>
#include <QTimer>

#include <chrono>
#include <optional>

namespace nexora::emulator {

enum class OutputChannel { Stdout, Stderr };

// Final result of one launch attempt.
struct ProcessOutcome
{
    ProcessState finalState = ProcessState::Idle; // Exited or Failed
    EmulatorError error = EmulatorError::None;
    std::optional<int> exitCode;  // set for normal (non-crash) exits
    bool stoppedByRequest = false; // stop() was called
    bool killed = false;           // graceful stop timed out and SIGKILL was sent
    QString message;               // user-facing summary
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
    LaunchResult launch(const shared::EmulatorConfig &config);

    // Requests a graceful stop (SIGTERM), escalating to SIGKILL after stopTimeout().
    // Returns false if no process is active.
    bool stop();

    // Kills any active process and waits (bounded) for it. Use only at application exit.
    void shutdown();

    ProcessState state() const { return m_state; }
    bool isActive() const { return emulator::isActive(m_state); }
    QString activeEmulatorId() const { return m_activeId; }
    const ProcessOutcome &lastOutcome() const { return m_lastOutcome; }

    // Bounded tail of the captured output of the current / most recent process.
    QString capturedStdout() const { return m_stdout; }
    QString capturedStderr() const { return m_stderr; }

    std::chrono::milliseconds stopTimeout() const { return m_stopTimeout; }
    void setStopTimeout(std::chrono::milliseconds timeout) { m_stopTimeout = timeout; }
    qsizetype outputLimit() const { return m_outputLimit; }
    void setOutputLimit(qsizetype characters) { m_outputLimit = qMax<qsizetype>(1, characters); }

    // Checks that |path| is absolute, exists, is a regular file and is executable.
    static EmulatorError validateExecutable(const QString &path, QString *detail = nullptr);

signals:
    void stateChanged(nexora::emulator::ProcessState state, const QString &emulatorId);
    void outputReceived(const QString &emulatorId, nexora::emulator::OutputChannel channel,
                        const QString &text);
    void processFinished(const QString &emulatorId, const nexora::emulator::ProcessOutcome &outcome);

private:
    void setState(ProcessState state);
    void readChannel(OutputChannel channel);
    void appendOutput(OutputChannel channel, const QString &text);
    void onStarted();
    void onErrorOccurred(QProcess::ProcessError error);
    void onFinished(int exitCode, QProcess::ExitStatus status);
    void onStopTimeout();
    void finish(ProcessOutcome outcome);
    void releaseProcess();

    ProcessState m_state = ProcessState::Idle;
    QPointer<QProcess> m_process;
    QString m_activeId;
    QString m_activeName;
    QTimer m_killTimer;
    std::chrono::milliseconds m_stopTimeout{5000};
    bool m_stopRequested = false;
    bool m_killed = false;
    ProcessOutcome m_lastOutcome;
    QString m_stdout;
    QString m_stderr;
    qsizetype m_outputLimit = 64 * 1024;
    QStringDecoder m_stdoutDecoder{QStringDecoder::Utf8};
    QStringDecoder m_stderrDecoder{QStringDecoder::Utf8};
};

} // namespace nexora::emulator

Q_DECLARE_METATYPE(nexora::emulator::ProcessOutcome)
