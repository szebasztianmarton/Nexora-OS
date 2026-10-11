#include "nexora/emulator/EmulatorManager.h"

#include "nexora/shared/Logging.h"

#include <QDir>
#include <QFileInfo>

#include <csignal>
#include <sys/prctl.h>
#include <sys/types.h>
#include <utility>

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

std::optional<ResolvedLaunch> EmulatorManager::resolve(const LaunchRequest &request, EmulatorError *error,
                                                       QString *detail)
{
    const shared::EmulatorConfig &config = request.emulator;
    auto reject = [&](EmulatorError e, const QString &d) -> std::optional<ResolvedLaunch> {
        if (error)
            *error = e;
        if (detail)
            *detail = d;
        return std::nullopt;
    };
    const QString emulatorName = config.name.isEmpty() ? config.id : config.name;

    if (config.id.isEmpty())
        return reject(EmulatorError::InvalidConfig, QStringLiteral("missing id"));
    if (!config.enabled)
        return reject(EmulatorError::Disabled, emulatorName);
    QString exeDetail;
    if (const EmulatorError e = validateExecutable(config.executable, &exeDetail); e != EmulatorError::None)
        return reject(e, exeDetail);

    ResolvedLaunch resolved;
    resolved.program = config.executable;
    resolved.arguments = config.arguments;
    resolved.workingDirectory
        = config.workingDirectory.isEmpty() ? QFileInfo(config.executable).absolutePath() : config.workingDirectory;
    if (config.stopTimeoutMs)
        resolved.stopTimeout = std::chrono::milliseconds(*config.stopTimeoutMs);
    resolved.displayName = request.displayName.isEmpty() ? emulatorName : request.displayName;

    if (!request.profileId.isEmpty()) {
        const shared::LaunchProfile *profile = config.profile(request.profileId);
        if (!profile)
            return reject(EmulatorError::ProfileNotFound,
                          QStringLiteral("%1 / %2").arg(emulatorName, request.profileId));
        if (!profile->enabled)
            return reject(EmulatorError::ProfileDisabled, QStringLiteral("%1 / %2").arg(emulatorName, profile->name));
        if (profile->hasArguments)
            resolved.arguments = profile->arguments;
        if (!profile->workingDirectory.isEmpty())
            resolved.workingDirectory = profile->workingDirectory;
    }

    const QFileInfo workDir(resolved.workingDirectory);
    if (!workDir.isAbsolute() || !workDir.isDir())
        return reject(EmulatorError::InvalidWorkingDirectory, resolved.workingDirectory);

    static const QString placeholder = QStringLiteral("{file}");
    if (request.gamePath.isEmpty()) {
        resolved.arguments.removeIf([](const QString &a) { return a.contains(placeholder); });
    } else {
        const QFileInfo game(request.gamePath);
        if (!game.isAbsolute() || !game.isFile() || !game.isReadable())
            return reject(EmulatorError::GameFileMissing, request.gamePath);
        const QString path = game.absoluteFilePath();
        bool substituted = false;
        for (QString &a : resolved.arguments) {
            if (a.contains(placeholder)) {
                a.replace(placeholder, path);
                substituted = true;
            }
        }
        if (!substituted)
            resolved.arguments.append(path);
    }
    if (error)
        *error = EmulatorError::None;
    return resolved;
}

LaunchResult EmulatorManager::launch(const LaunchRequest &request)
{
    const shared::EmulatorConfig &config = request.emulator;
    if (isActive()) {
        const QString message = describe(EmulatorError::AlreadyActive, m_activeName);
        qCWarning(lcProcess) << "Launch of" << config.id << "rejected:" << message;
        return {false, EmulatorError::AlreadyActive, message};
    }

    m_activeId = config.id;
    m_activeName = !request.displayName.isEmpty() ? request.displayName
        : config.name.isEmpty()                   ? config.id
                                                  : config.name;
    m_stopRequested = false;
    m_killed = false;
    m_activePausable = config.pausable;
    m_paused = false;
    m_pid = 0;
    m_startedAt = QDateTime();
    m_runClock.invalidate();
    m_stdout.clear();
    m_stderr.clear();
    m_stdoutDecoder.resetState();
    m_stderrDecoder.resetState();

    EmulatorError error = EmulatorError::None;
    QString detail;
    const auto resolved = resolve(request, &error, &detail);
    if (!resolved) {
        ProcessOutcome outcome;
        outcome.finalState = ProcessState::Failed;
        outcome.error = error;
        outcome.message = describe(error, detail);
        qCWarning(lcProcess).noquote() << "Cannot launch" << config.id << "-" << outcome.message;
        finish(outcome);
        return LaunchResult{false, error, outcome.message};
    }
    m_activeStopTimeout = resolved->stopTimeout.value_or(m_stopTimeout);

    auto *process = new QProcess(this);
    process->setProgram(resolved->program);
    process->setArguments(resolved->arguments);
    process->setProcessChannelMode(QProcess::SeparateChannels);
    process->setWorkingDirectory(resolved->workingDirectory);
    // Make sure the emulator does not outlive a crashed launcher (Linux only).
    process->setChildProcessModifier([] { ::prctl(PR_SET_PDEATHSIG, SIGTERM); });

    connect(process, &QProcess::started, this, &EmulatorManager::onStarted);
    connect(process, &QProcess::errorOccurred, this, &EmulatorManager::onErrorOccurred);
    connect(process, &QProcess::finished, this, &EmulatorManager::onFinished);
    connect(process, &QProcess::readyReadStandardOutput, this, [this] { readChannel(OutputChannel::Stdout); });
    connect(process, &QProcess::readyReadStandardError, this, [this] { readChannel(OutputChannel::Stderr); });

    m_process = process;
    setState(ProcessState::Starting);
    qCInfo(lcProcess).noquote() << "Starting" << config.id
                                << (request.profileId.isEmpty() ? QString()
                                                                : QStringLiteral("profile ") + request.profileId)
                                << "->" << resolved->program << "with" << resolved->arguments.size()
                                << "argument(s), cwd" << resolved->workingDirectory;
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
    if (m_paused) {
        // A stopped process cannot handle SIGTERM until it is continued.
        if (m_pid > 0)
            ::kill(static_cast<pid_t>(m_pid), SIGCONT);
        setPaused(false);
    }
    setState(ProcessState::Stopping);
    qCInfo(lcProcess) << "Stopping" << m_activeId << "(timeout" << m_activeStopTimeout.count() << "ms)";
    m_process->terminate();
    m_killTimer.start(m_activeStopTimeout);
    return true;
}

bool EmulatorManager::pause()
{
    if (!m_process || m_state != ProcessState::Running || !m_activePausable || m_pid <= 0)
        return false;
    if (m_paused)
        return true;
    if (::kill(static_cast<pid_t>(m_pid), SIGSTOP) != 0) {
        qCWarning(lcProcess) << "Could not pause" << m_activeId << "pid" << m_pid;
        return false;
    }
    qCInfo(lcProcess) << "Paused" << m_activeId << "pid" << m_pid;
    setPaused(true);
    return true;
}

bool EmulatorManager::resume()
{
    if (!m_process || !m_paused || m_pid <= 0)
        return false;
    if (::kill(static_cast<pid_t>(m_pid), SIGCONT) != 0) {
        qCWarning(lcProcess) << "Could not resume" << m_activeId << "pid" << m_pid;
        return false;
    }
    qCInfo(lcProcess) << "Resumed" << m_activeId << "pid" << m_pid;
    setPaused(false);
    return true;
}

void EmulatorManager::setPaused(bool paused)
{
    if (m_paused == paused)
        return;
    m_paused = paused;
    emit pausedChanged(m_paused);
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
        qCCritical(lcProcess) << "Invalid state transition" << nameOf(m_state) << "->" << nameOf(state);
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
    const QByteArray bytes
        = channel == OutputChannel::Stdout ? m_process->readAllStandardOutput() : m_process->readAllStandardError();
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
    m_pid = m_process->processId();
    m_startedAt = QDateTime::currentDateTimeUtc();
    m_runClock.start();
    qCInfo(lcProcess) << m_activeId << "started, pid" << m_pid;
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
        outcome.startupFailure = true;
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
        outcome.message
            = describe(EmulatorError::NonZeroExit, QStringLiteral("%1 (exit code %2)").arg(m_activeName).arg(exitCode));
    } else {
        outcome.finalState = ProcessState::Exited;
        outcome.message = QStringLiteral("%1 exited normally").arg(m_activeName);
    }
    // A failure right after the process started is reported as a startup failure.
    if (outcome.finalState == ProcessState::Failed && m_runClock.isValid() && m_startupGrace.count() > 0
        && m_runClock.elapsed() < m_startupGrace.count()) {
        outcome.startupFailure = true;
        outcome.message = outcome.exitCode
            ? QStringLiteral("%1 failed during startup (exit %2)").arg(m_activeName).arg(*outcome.exitCode)
            : QStringLiteral("%1 failed during startup (crashed)").arg(m_activeName);
    }
    qCInfo(lcProcess).noquote() << m_activeId << "finished:" << outcome.message;
    releaseProcess();
    finish(outcome);
}

void EmulatorManager::onStopTimeout()
{
    if (!m_process || m_state != ProcessState::Stopping)
        return;
    qCWarning(lcProcess) << m_activeId << "did not exit within" << m_activeStopTimeout.count() << "ms, sending SIGKILL";
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
    outcome.pid = m_pid;
    outcome.startedAt = m_startedAt;
    if (!outcome.endedAt.isValid())
        outcome.endedAt = QDateTime::currentDateTimeUtc();
    m_pid = 0;
    const bool wasPaused = std::exchange(m_paused, false);
    m_lastOutcome = outcome;
    // State is updated (and the process released) before notifying, so listeners may
    // immediately launch again from their slots.
    setState(outcome.finalState);
    if (wasPaused)
        emit pausedChanged(false);
    emit processFinished(m_activeId, m_lastOutcome);
}

} // namespace nexora::emulator
