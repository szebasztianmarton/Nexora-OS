#include "nexora/emulator/EmulatorManager.h"

#include "nexora/shared/Logging.h"

#include <QDir>
#include <QFileInfo>

#include <csignal>
#include <sys/prctl.h>

namespace nexora::emulator {

EmulatorManager::EmulatorManager(QObject *parent)
    : QObject(parent)
{
    qRegisterMetaType<ProcessOutcome>();
    m_killTimer.setSingleShot(true);
    connect(&m_killTimer, &QTimer::timeout, this, &EmulatorManager::onStopTimeout);
}

EmulatorManager::~EmulatorManager()
{
    shutdown();
}

EmulatorError EmulatorManager::validateExecutable(const QString &path, QString *detail)
{
    if (detail)
        *detail = path;
    if (path.isEmpty()) {
        if (detail)
            *detail = QStringLiteral("no executable configured");
        return EmulatorError::InvalidConfig;
    }
    if (QDir::isRelativePath(path)) {
        if (detail)
            *detail = QStringLiteral("path must be absolute: %1").arg(path);
        return EmulatorError::InvalidConfig;
    }
    const QFileInfo info(path);
    if (!info.exists())
        return EmulatorError::ExecutableMissing;
    if (!info.isFile())
        return EmulatorError::NotAFile;
    if (!info.isExecutable())
        return EmulatorError::NotExecutable;
    return EmulatorError::None;
}

LaunchResult EmulatorManager::launch(const shared::EmulatorConfig &config)
{
    if (isActive()) {
        const QString message = describe(EmulatorError::AlreadyActive, m_activeName);
        qCWarning(lcProcess) << "Launch of" << config.id << "rejected:" << message;
        return {false, EmulatorError::AlreadyActive, message};
    }

    m_activeId = config.id;
    m_activeName = config.name.isEmpty() ? config.id : config.name;
    m_stopRequested = false;
    m_killed = false;
    m_stdout.clear();
    m_stderr.clear();
    m_stdoutDecoder.resetState();
    m_stderrDecoder.resetState();

    auto rejectValidation = [&](EmulatorError error, const QString &detail) {
        ProcessOutcome outcome;
        outcome.finalState = ProcessState::Failed;
        outcome.error = error;
        outcome.message = describe(error, detail);
        qCWarning(lcProcess).noquote() << "Cannot launch" << config.id << "-" << outcome.message;
        finish(outcome);
        return LaunchResult{false, error, outcome.message};
    };

    if (config.id.isEmpty())
        return rejectValidation(EmulatorError::InvalidConfig, QStringLiteral("missing id"));
    if (!config.enabled)
        return rejectValidation(EmulatorError::Disabled, m_activeName);
    QString detail;
    if (const EmulatorError error = validateExecutable(config.executable, &detail);
        error != EmulatorError::None) {
        return rejectValidation(error, detail);
    }

    auto *process = new QProcess(this);
    process->setProgram(config.executable);
    process->setArguments(config.arguments);
    process->setProcessChannelMode(QProcess::SeparateChannels);
    process->setWorkingDirectory(QFileInfo(config.executable).absolutePath());
    // Make sure the emulator does not outlive a crashed launcher (Linux only).
    process->setChildProcessModifier([] { ::prctl(PR_SET_PDEATHSIG, SIGTERM); });

    connect(process, &QProcess::started, this, &EmulatorManager::onStarted);
    connect(process, &QProcess::errorOccurred, this, &EmulatorManager::onErrorOccurred);
    connect(process, &QProcess::finished, this, &EmulatorManager::onFinished);
    connect(process, &QProcess::readyReadStandardOutput, this,
            [this] { readChannel(OutputChannel::Stdout); });
    connect(process, &QProcess::readyReadStandardError, this,
            [this] { readChannel(OutputChannel::Stderr); });

    m_process = process;
    setState(ProcessState::Starting);
    qCInfo(lcProcess).noquote() << "Starting" << config.id << "->" << config.executable
                                << "with" << config.arguments.size() << "argument(s)";
    process->start(QIODevice::ReadOnly);
    return {true, EmulatorError::None, QStringLiteral("Starting %1").arg(m_activeName)};
}

bool EmulatorManager::stop()
{
    if (!m_process || !isActive())
        return false;
    if (m_state == ProcessState::Stopping)
        return true; // already in progress; timer is running

    m_stopRequested = true;
    setState(ProcessState::Stopping);
    qCInfo(lcProcess) << "Stopping" << m_activeId << "(timeout" << m_stopTimeout.count() << "ms)";
    m_process->terminate();
    m_killTimer.start(m_stopTimeout);
    return true;
}

void EmulatorManager::shutdown()
{
    m_killTimer.stop();
    if (!m_process)
        return;
    QProcess *process = m_process;
    process->disconnect(this);
    if (process->state() != QProcess::NotRunning) {
        qCInfo(lcProcess) << "Shutdown: killing" << m_activeId;
        process->kill();
        process->waitForFinished(2000);
    }
    m_process = nullptr;
    delete process;
}

void EmulatorManager::setState(ProcessState state)
{
    if (state == m_state)
        return;
    if (!isValidTransition(m_state, state)) {
        qCCritical(lcProcess) << "Invalid state transition" << nameOf(m_state) << "->"
                              << nameOf(state);
        Q_ASSERT_X(false, "EmulatorManager::setState", "invalid state transition");
    }
    qCDebug(lcProcess) << "State" << nameOf(m_state) << "->" << nameOf(state);
    m_state = state;
    emit stateChanged(m_state, m_activeId);
}

void EmulatorManager::readChannel(OutputChannel channel)
{
    if (!m_process || sender() != m_process)
        return;
    const QByteArray bytes = channel == OutputChannel::Stdout ? m_process->readAllStandardOutput()
                                                              : m_process->readAllStandardError();
    if (bytes.isEmpty())
        return;
    QStringDecoder &decoder = channel == OutputChannel::Stdout ? m_stdoutDecoder : m_stderrDecoder;
    appendOutput(channel, decoder.decode(bytes));
}

void EmulatorManager::appendOutput(OutputChannel channel, const QString &text)
{
    if (text.isEmpty())
        return;
    QString &buffer = channel == OutputChannel::Stdout ? m_stdout : m_stderr;
    buffer.append(text);
    if (buffer.size() > m_outputLimit)
        buffer.remove(0, buffer.size() - m_outputLimit);

    const char *tag = channel == OutputChannel::Stdout ? "stdout" : "stderr";
    for (const QString &line : text.split(QLatin1Char('\n'), Qt::SkipEmptyParts))
        qCDebug(lcProcessOutput).noquote() << m_activeId << tag << line;
    emit outputReceived(m_activeId, channel, text);
}

void EmulatorManager::onStarted()
{
    if (sender() != m_process)
        return;
    qCInfo(lcProcess) << m_activeId << "started, pid" << m_process->processId();
    // A stop() during Starting already moved us to Stopping; stay there.
    if (m_state == ProcessState::Starting)
        setState(ProcessState::Running);
}

void EmulatorManager::onErrorOccurred(QProcess::ProcessError error)
{
    if (sender() != m_process)
        return;
    if (m_stopRequested && error == QProcess::Crashed) // expected after SIGTERM/SIGKILL
        qCDebug(lcProcess) << m_activeId << "terminated by signal after stop request";
    else
        qCWarning(lcProcess) << m_activeId << "process error" << error << m_process->errorString();

    // Only FailedToStart is terminal without a following finished() signal. Crashes are
    // resolved in onFinished(); read/write/timeouts are informational.
    if (error != QProcess::FailedToStart)
        return;

    ProcessOutcome outcome;
    if (m_state == ProcessState::Stopping) {
        outcome.finalState = ProcessState::Exited;
        outcome.stoppedByRequest = true;
        outcome.message = QStringLiteral("%1 was stopped before it started").arg(m_activeName);
    } else {
        outcome.finalState = ProcessState::Failed;
        outcome.error = EmulatorError::FailedToStart;
        outcome.message = describe(EmulatorError::FailedToStart, m_process->errorString());
    }
    releaseProcess();
    finish(outcome);
}

void EmulatorManager::onFinished(int exitCode, QProcess::ExitStatus status)
{
    if (sender() != m_process)
        return;
    // Drain anything still buffered (a trailing incomplete UTF-8 sequence is dropped).
    readChannel(OutputChannel::Stdout);
    readChannel(OutputChannel::Stderr);

    ProcessOutcome outcome;
    outcome.stoppedByRequest = m_stopRequested;
    outcome.killed = m_killed;
    if (status == QProcess::NormalExit)
        outcome.exitCode = exitCode;

    if (m_stopRequested) {
        outcome.finalState = ProcessState::Exited;
        outcome.message = m_killed
            ? QStringLiteral("%1 did not stop in time and was terminated forcefully").arg(m_activeName)
            : QStringLiteral("%1 was stopped").arg(m_activeName);
    } else if (status == QProcess::CrashExit) {
        outcome.finalState = ProcessState::Failed;
        outcome.error = EmulatorError::Crashed;
        outcome.message = describe(EmulatorError::Crashed, m_activeName);
    } else if (exitCode != 0) {
        outcome.finalState = ProcessState::Failed;
        outcome.error = EmulatorError::NonZeroExit;
        outcome.message = describe(EmulatorError::NonZeroExit,
                                   QStringLiteral("%1 (exit code %2)").arg(m_activeName).arg(exitCode));
    } else {
        outcome.finalState = ProcessState::Exited;
        outcome.message = QStringLiteral("%1 exited normally").arg(m_activeName);
    }
    qCInfo(lcProcess).noquote() << m_activeId << "finished:" << outcome.message;
    releaseProcess();
    finish(outcome);
}

void EmulatorManager::onStopTimeout()
{
    if (!m_process || m_state != ProcessState::Stopping)
        return;
    qCWarning(lcProcess) << m_activeId << "did not exit within" << m_stopTimeout.count()
                         << "ms, sending SIGKILL";
    m_killed = true;
    m_process->kill();
}

void EmulatorManager::releaseProcess()
{
    m_killTimer.stop();
    if (!m_process)
        return;
    QProcess *process = m_process;
    m_process = nullptr;
    process->disconnect(this);
    process->deleteLater(); // never delete inside its own signal emission
}

void EmulatorManager::finish(ProcessOutcome outcome)
{
    m_lastOutcome = outcome;
    // State is updated (and the process released) before notifying, so listeners may
    // immediately launch again from their slots.
    setState(outcome.finalState);
    emit processFinished(m_activeId, m_lastOutcome);
}

} // namespace nexora::emulator
