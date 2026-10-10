#pragma once

#include <QDateTime>
#include <QElapsedTimer>
#include <QObject>
#include <QPointer>
#include <QProcess>

namespace nexora {

class EmulatorRegistry;

// Launches and supervises ONE external emulator process at a time (QProcess,
// never through a shell). State machine:
//   Idle --launch()--> Launching --started--> Running --finished--> Idle
//   Launching --error--> Idle (lastResult = FailedToStart)
//   Running --terminate()--> Stopping --finished--> Idle
// A second launch while not Idle is refused (returns false, no side effects).
class EmulatorManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(State state READ state NOTIFY stateChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY stateChanged)                // state != Idle
    Q_PROPERTY(QString activeEmulatorId READ activeEmulatorId NOTIFY stateChanged)
    Q_PROPERTY(QString activeEmulatorName READ activeEmulatorName NOTIFY stateChanged)
    Q_PROPERTY(QString activeGamePath READ activeGamePath NOTIFY stateChanged)
    Q_PROPERTY(Result lastResult READ lastResult NOTIFY lastResultChanged)
    Q_PROPERTY(int lastExitCode READ lastExitCode NOTIFY lastResultChanged)
    Q_PROPERTY(QString lastErrorMessage READ lastErrorMessage NOTIFY lastResultChanged) // human readable, "" on success
    Q_PROPERTY(QString lastEmulatorId READ lastEmulatorId NOTIFY lastResultChanged)
    Q_PROPERTY(QString lastGamePath READ lastGamePath NOTIFY lastResultChanged)
    Q_PROPERTY(bool canRetry READ canRetry NOTIFY lastResultChanged)

public:
    enum State { Idle, Launching, Running, Stopping };
    Q_ENUM(State)
    enum Result { NoResult, NormalExit, ErrorExit, Crashed, FailedToStart };
    Q_ENUM(Result)

    explicit EmulatorManager(EmulatorRegistry *registry, QObject *parent = nullptr);
    ~EmulatorManager() override; // kills a still-running child

    State state() const;
    bool busy() const;
    QString activeEmulatorId() const;
    QString activeEmulatorName() const;
    QString activeGamePath() const;
    Result lastResult() const;
    int lastExitCode() const;
    QString lastErrorMessage() const;
    QString lastEmulatorId() const;
    QString lastGamePath() const;
    bool canRetry() const;

    // Returns false (and emits launchFailed with a reason) if the emulator is
    // unknown/unavailable/disabled, the game file does not exist, or a process
    // is already active. gamePath may be empty to start the emulator alone.
    Q_INVOKABLE bool launch(const QString &emulatorId, const QString &gamePath = {}, const QString &profileId = {});
    // Re-launches lastEmulatorId/lastGamePath/profile if canRetry.
    Q_INVOKABLE bool retry();
    // SIGTERM, then SIGKILL after `killTimeoutMs`.
    Q_INVOKABLE void terminate();
    Q_INVOKABLE void clearLastResult();

    void setKillTimeout(int ms) { m_killTimeoutMs = ms; }

signals:
    void stateChanged();
    void lastResultChanged();
    void processStarted(const QString &emulatorId, const QString &gamePath);
    void processFinished(const QString &emulatorId, const QString &gamePath, int exitCode,
                         nexora::EmulatorManager::Result result, qint64 durationMs);
    void launchFailed(const QString &emulatorId, const QString &gamePath, const QString &message);

private:
    void setState(State state);
    void finish(Result result, int exitCode, const QString &message);
    void readOutput();
    void flushOutput();

    QPointer<EmulatorRegistry> m_registry;
    QProcess *m_process = nullptr;
    State m_state = Idle;
    QString m_activeId, m_activeName, m_activeGame, m_activeProfile;
    Result m_lastResult = NoResult;
    int m_lastExitCode = 0;
    QString m_lastError, m_lastId, m_lastGame, m_lastProfile;
    QElapsedTimer m_runTimer;
    int m_killTimeoutMs = 5000;
    QByteArray m_outputBuffer;   // incomplete trailing output line
    qint64 m_outputBytes = 0;    // bytes logged during the current run
};

} // namespace nexora
