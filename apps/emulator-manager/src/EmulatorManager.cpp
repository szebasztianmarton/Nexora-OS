#include "nexora/emulator/EmulatorManager.h"

#include "nexora/shared/Logging.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QSignalBlocker>

#include <cerrno>
#include <csignal>
#include <sys/prctl.h>
#include <sys/types.h>
#include <unistd.h>
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
    // The owner may be half destroyed already (members declared after the manager are
    // gone), so the run ends without notifying anyone. Call shutdown() first to be notified.
    const QSignalBlocker blocker(this);
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
    resolved.arguments.append(request.extraArguments);
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
    m_terminatePending = false;
    m_activePausable = config.pausable;
    m_paused = false;
    m_pausedTotalMs = 0;
    m_pauseClock.invalidate();
    m_pid = 0;
    m_startedAt = QDateTime();
    m_runClock.invalidate();
    m_stdout.clear();
    m_stderr.clear();
    m_stdoutDecoder.resetState();
    m_stderrDecoder.resetState();
    m_outputLogPath.clear();

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
    openOutputLog(*resolved);

    auto *process = new QProcess(this);
    process->setProgram(resolved->program);
    process->setArguments(resolved->arguments);
    process->setProcessChannelMode(QProcess::SeparateChannels);
    process->setWorkingDirectory(resolved->workingDirectory);
    // Runs in the child between fork and exec (async-signal-safe calls only):
    //  - its own process group, so stop/kill/pause/resume reach everything the emulator
    //    spawns (wrapper scripts, helper processes), not just the direct child;
    //  - make sure the emulator does not outlive a crashed launcher (Linux only; covers the
    //    direct child, the group is signalled on every regular stop/shutdown).
    process->setChildProcessModifier([] {
        ::setpgid(0, 0);
        ::prctl(PR_SET_PDEATHSIG, SIGTERM);
    });

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
        // A stopped process group cannot handle SIGTERM until it is continued.
        signalGroup(SIGCONT);
        setPaused(false);
    }
    setState(ProcessState::Stopping);
    qCInfo(lcProcess) << "Stopping" << m_activeId << "(timeout" << m_activeStopTimeout.count() << "ms)";
    // During Starting the pid may not be known yet: onStarted() sends SIGTERM as soon as it
    // is, instead of waiting for the kill timeout.
    m_terminatePending = !signalGroup(SIGTERM);
    m_killTimer.start(m_activeStopTimeout);
    return true;
}

bool EmulatorManager::signalGroup(int signal)
{
    const qint64 pid = m_pid > 0 ? m_pid : (m_process ? m_process->processId() : 0);
    if (pid <= 0)
        return false;
    if (::kill(-static_cast<pid_t>(pid), signal) == 0)
        return true;
    // No such group (setpgid failed, or the leader is gone): the direct child only.
    return ::kill(static_cast<pid_t>(pid), signal) == 0 || errno == ESRCH;
}

bool EmulatorManager::pause()
{
    if (!m_process || m_state != ProcessState::Running || !m_activePausable || m_pid <= 0)
        return false;
    if (m_paused)
        return true;
    if (!signalGroup(SIGSTOP)) {
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
    if (!signalGroup(SIGCONT)) {
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
    if (paused)
        m_pauseClock.start();
    else if (m_pauseClock.isValid())
        m_pausedTotalMs += std::exchange(m_pauseClock, QElapsedTimer()).elapsed();
    emit pausedChanged(m_paused);
}

void EmulatorManager::shutdown()
{
    m_killTimer.stop();
    if (!m_process)
        return;
    // The same path as stop(), with the waiting done here (no event loop runs at exit):
    // SIGTERM first so the emulator can write its own save data; SIGKILL only if it is
    // still running after its stop timeout (capped: the system may be powering off).
    // onFinished() then ends the run (Exited, processFinished) and kills what is left in
    // the group, so launch history records the final run as well.
    const QProcess *process = m_process;
    const auto grace = std::min(m_activeStopTimeout, ShutdownGraceLimit);
    qCInfo(lcProcess) << "Shutdown: stopping" << m_activeId << "(SIGTERM, up to" << grace.count() << "ms)";
    stop();
    m_killTimer.stop(); // escalation is timed here, not by the event loop
    if (!m_process->waitForFinished(int(grace.count())) && m_process == process) {
        qCWarning(lcProcess) << "Shutdown:" << m_activeId << "ignored SIGTERM; killing it";
        onStopTimeout();
        m_process->waitForFinished(2000);
    }
    if (m_process != process)
        return;
    // It never reported its exit (stuck in the kernel, or it never started): end the run anyway.
    qCWarning(lcProcess) << "Shutdown:" << m_activeId << "did not report its exit";
    if (m_pid > 0)
        ::kill(-static_cast<pid_t>(m_pid), SIGKILL); // helpers left in the group; no pid fallback
    ProcessOutcome outcome;
    outcome.finalState = ProcessState::Exited;
    outcome.stoppedByRequest = true;
    outcome.killed = m_killed;
    outcome.message = QStringLiteral("%1 did not report its exit when the launcher shut down").arg(m_activeName);
    releaseProcess();
    finish(outcome);
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

    writeOutputLog(channel, text);

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
    // A stop() during Starting already moved us to Stopping; stay there, and deliver the
    // SIGTERM it could not send without a pid.
    if (m_state == ProcessState::Starting)
        setState(ProcessState::Running);
    else if (m_state == ProcessState::Stopping && m_terminatePending)
        m_terminatePending = !signalGroup(SIGTERM);
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

    // A requested stop ends the whole group: anything the emulator spawned and left behind
    // (e.g. a helper that ignores SIGTERM) goes with it.
    if (m_stopRequested && m_pid > 0)
        ::kill(-static_cast<pid_t>(m_pid), SIGKILL);

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
    signalGroup(SIGKILL);
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

void EmulatorManager::openOutputLog(const ResolvedLaunch &launch)
{
    if (m_outputLogDirectory.isEmpty())
        return;
    const QDir dir(m_outputLogDirectory);
    if (!dir.mkpath(QStringLiteral("."))) {
        qCWarning(lcProcess) << "Cannot create emulator log directory" << m_outputLogDirectory;
        return;
    }
    // m_activeId passed ConfigLoader's id pattern, so it is a safe file name.
    const QString path = dir.filePath(m_activeId + QStringLiteral(".log"));
    const QString previous = path + QStringLiteral(".1");
    QFile::remove(previous);
    QFile::rename(path, previous);

    auto file = std::make_unique<QFile>(path);
    if (!file->open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        qCWarning(lcProcess) << "Cannot write emulator log" << path << "-" << file->errorString();
        return;
    }
    m_outputLog = std::move(file);
    m_outputLogPath = path;
    m_outputLogWritten = 0;
    m_outputLogTruncated = false;
    m_pendingLine[0].clear();
    m_pendingLine[1].clear();

    QStringList quoted;
    for (const QString &arg : launch.arguments)
        quoted.append(arg.contains(QLatin1Char(' ')) || arg.isEmpty() ? QLatin1Char('"') + arg + QLatin1Char('"')
                                                                      : arg);
    const QString header = QStringLiteral("# %1 %2\n# program: %3\n# arguments: %4\n# working directory: %5\n")
                               .arg(QDateTime::currentDateTime().toString(Qt::ISODate), m_activeName, launch.program,
                                    quoted.join(QLatin1Char(' ')), launch.workingDirectory);
    writeLogBytes(header.toUtf8());
}

void EmulatorManager::writeOutputLog(OutputChannel channel, const QString &text)
{
    if (!m_outputLog)
        return;
    QString &pending = m_pendingLine[channel == OutputChannel::Stdout ? 0 : 1];
    pending += text;
    const qsizetype lastNewline = pending.lastIndexOf(QLatin1Char('\n'));
    // Keep a partial line for the next chunk, unless it is unreasonably long.
    if (lastNewline < 0 && pending.size() < 4096)
        return;
    const qsizetype end = lastNewline < 0 ? pending.size() : lastNewline;
    const QString prefix = channel == OutputChannel::Stdout ? QStringLiteral("out| ") : QStringLiteral("err| ");
    QString block;
    for (const QString &line : pending.left(end).split(QLatin1Char('\n')))
        block += prefix + line + QLatin1Char('\n');
    pending.remove(0, lastNewline < 0 ? end : end + 1);
    writeLogBytes(block.toUtf8());
}

void EmulatorManager::writeLogBytes(const QByteArray &bytes)
{
    if (!m_outputLog || m_outputLogTruncated)
        return;
    if (m_outputLogWritten + bytes.size() > m_outputLogLimit) {
        m_outputLogTruncated = true;
        m_outputLog->write("# output log limit reached; further output is not recorded\n");
        m_outputLog->flush();
        return;
    }
    m_outputLogWritten += m_outputLog->write(bytes);
    m_outputLog->flush(); // the launcher may be killed; keep what was received
}

void EmulatorManager::closeOutputLog(const QString &footer)
{
    if (!m_outputLog)
        return;
    for (OutputChannel channel : {OutputChannel::Stdout, OutputChannel::Stderr}) {
        if (!m_pendingLine[channel == OutputChannel::Stdout ? 0 : 1].isEmpty())
            writeOutputLog(channel, QStringLiteral("\n"));
    }
    m_outputLogTruncated = false; // the footer is always written
    m_outputLog->write(
        QStringLiteral("# %1 finished: %2\n").arg(QDateTime::currentDateTime().toString(Qt::ISODate), footer).toUtf8());
    m_outputLog->close();
    m_outputLog.reset();
}

void EmulatorManager::finish(ProcessOutcome outcome)
{
    closeOutputLog(outcome.message);
    outcome.pid = m_pid;
    outcome.startedAt = m_startedAt;
    if (!outcome.endedAt.isValid())
        outcome.endedAt = QDateTime::currentDateTimeUtc();
    m_pid = 0;
    outcome.runMs = m_runClock.isValid() ? m_runClock.elapsed() : -1;
    outcome.pausedMs = m_pausedTotalMs + (m_pauseClock.isValid() ? m_pauseClock.elapsed() : 0);
    m_pauseClock.invalidate();
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
