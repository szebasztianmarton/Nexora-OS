#include "emulatormanager.h"

#include <QFileInfo>
#include <QTimer>

#include "emulatorregistry.h"

namespace nexora {

namespace {
constexpr qint64 MaxLoggedOutputBytes = 1024 * 1024; // per run
}

EmulatorManager::EmulatorManager(EmulatorRegistry *registry, QObject *parent)
    : QObject(parent)
    , m_registry(registry)
{
}

EmulatorManager::~EmulatorManager()
{
    if (!m_process)
        return;
    // No signal may reach this half-destroyed object.
    m_process->disconnect(this);
    if (m_process->state() != QProcess::NotRunning) {
        qWarning("Launcher: shutting down, killing %s", qPrintable(m_activeName));
        m_process->kill();
        m_process->waitForFinished(2000);
    }
}

EmulatorManager::State EmulatorManager::state() const { return m_state; }
bool EmulatorManager::busy() const { return m_state != Idle; }
QString EmulatorManager::activeEmulatorId() const { return m_activeId; }
QString EmulatorManager::activeEmulatorName() const { return m_activeName; }
QString EmulatorManager::activeGamePath() const { return m_activeGame; }
EmulatorManager::Result EmulatorManager::lastResult() const { return m_lastResult; }
int EmulatorManager::lastExitCode() const { return m_lastExitCode; }
QString EmulatorManager::lastErrorMessage() const { return m_lastError; }
QString EmulatorManager::lastEmulatorId() const { return m_lastId; }
QString EmulatorManager::lastGamePath() const { return m_lastGame; }

bool EmulatorManager::canRetry() const
{
    return !m_lastId.isEmpty() && m_state == Idle
           && (m_lastResult == ErrorExit || m_lastResult == Crashed || m_lastResult == FailedToStart);
}

bool EmulatorManager::launch(const QString &emulatorId, const QString &gamePath, const QString &profileId)
{
    auto refuse = [&](const QString &message) {
        qWarning("Launcher: refused to launch %s: %s", qPrintable(emulatorId), qPrintable(message));
        emit launchFailed(emulatorId, gamePath, message);
        return false;
    };

    if (busy())
        return refuse(QStringLiteral("Another application is already running"));
    if (!m_registry)
        return refuse(QStringLiteral("No emulators are configured"));

    m_registry->validate(emulatorId); // the filesystem may have changed since the last check
    const EmulatorDefinition *def = m_registry->find(emulatorId);
    if (!def)
        return refuse(QStringLiteral("Unknown emulator \"%1\"").arg(emulatorId));
    if (!def->enabled)
        return refuse(QStringLiteral("%1 is disabled").arg(def->name));
    if (!def->canLaunch()) {
        return refuse(QStringLiteral("%1 is not available: %2")
                          .arg(def->name, def->validationMessages.value(0, EmulatorDefinition::statusText(def->status))));
    }
    if (!gamePath.isEmpty()) {
        const QFileInfo game(gamePath);
        if (!game.isFile())
            return refuse(QStringLiteral("Game file not found: %1").arg(game.fileName()));
    }

    const QString program = def->executable;
    const QStringList arguments = def->buildArguments(gamePath, profileId);

    m_activeId = def->id;
    m_activeName = def->name;
    m_activeGame = gamePath;
    m_activeProfile = profileId;
    m_outputBuffer.clear();
    m_outputBytes = 0;

    auto *process = new QProcess(this);
    m_process = process;
    process->setProgram(program);
    process->setArguments(arguments);
    if (!def->workingDirectory.isEmpty())
        process->setWorkingDirectory(def->workingDirectory);
    process->setProcessChannelMode(QProcess::MergedChannels);
    process->setStandardInputFile(QProcess::nullDevice());

    connect(process, &QProcess::readyReadStandardOutput, this, &EmulatorManager::readOutput);
    connect(process, &QProcess::started, this, [this] {
        m_runTimer.start();
        qInfo("Launcher: %s started (pid %lld)", qPrintable(m_activeName), qint64(m_process->processId()));
        setState(Running);
        emit processStarted(m_activeId, m_activeGame);
    });
    connect(process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            finish(FailedToStart, -1,
                   QStringLiteral("Could not start %1: %2").arg(m_activeName, m_process->errorString()));
        } else if (error != QProcess::Crashed) {
            qWarning("Launcher: %s: %s", qPrintable(m_activeName), qPrintable(m_process->errorString()));
        }
    });
    connect(process, &QProcess::finished, this, [this](int exitCode, QProcess::ExitStatus status) {
        if (m_state == Stopping)
            finish(NormalExit, exitCode, QString()); // stopped on request: not an error
        else if (status == QProcess::CrashExit)
            finish(Crashed, exitCode, QStringLiteral("%1 crashed").arg(m_activeName));
        else if (exitCode != 0)
            finish(ErrorExit, exitCode, QStringLiteral("%1 exited with error code %2").arg(m_activeName).arg(exitCode));
        else
            finish(NormalExit, 0, QString());
    });

    qInfo("Launcher: starting %s: %s %s", qPrintable(m_activeName), qPrintable(program),
          qPrintable(arguments.join(QLatin1Char(' '))));
    setState(Launching);
    process->start(QIODevice::ReadOnly);
    // A synchronous start failure has already run finish() and reset m_process.
    return m_process == process;
}

bool EmulatorManager::retry()
{
    if (!canRetry())
        return false;
    return launch(m_lastId, m_lastGame, m_lastProfile);
}

void EmulatorManager::terminate()
{
    if (!m_process || (m_state != Running && m_state != Launching))
        return;
    qInfo("Launcher: stopping %s", qPrintable(m_activeName));
    setState(Stopping);
    m_process->terminate();
    QPointer<QProcess> process = m_process;
    QTimer::singleShot(m_killTimeoutMs, process, [process] {
        if (process && process->state() != QProcess::NotRunning) {
            qWarning("Launcher: process did not exit after SIGTERM, killing it");
            process->kill();
        }
    });
}

void EmulatorManager::clearLastResult()
{
    if (m_lastResult == NoResult && m_lastError.isEmpty())
        return;
    m_lastResult = NoResult;
    m_lastExitCode = 0;
    m_lastError.clear();
    emit lastResultChanged();
}

void EmulatorManager::setState(State state)
{
    if (m_state == state)
        return;
    const bool couldRetry = canRetry();
    m_state = state;
    emit stateChanged();
    if (couldRetry != canRetry())
        emit lastResultChanged();
}

void EmulatorManager::finish(Result result, int exitCode, const QString &message)
{
    if (m_process) {
        readOutput();
        flushOutput();
        m_process->disconnect(this);
        m_process->deleteLater();
        m_process = nullptr;
    }
    const qint64 duration = m_runTimer.isValid() ? m_runTimer.elapsed() : 0;
    m_runTimer.invalidate();

    const QString id = m_activeId;
    const QString game = m_activeGame;
    m_lastResult = result;
    m_lastExitCode = exitCode;
    m_lastError = message;
    m_lastId = id;
    m_lastGame = game;
    m_lastProfile = m_activeProfile;
    m_activeId.clear();
    m_activeName.clear();
    m_activeGame.clear();
    m_activeProfile.clear();

    if (message.isEmpty())
        qInfo("Launcher: %s finished (exit code %d, %lld ms)", qPrintable(id), exitCode, duration);
    else
        qWarning("Launcher: %s", qPrintable(message));

    setState(Idle);
    emit lastResultChanged();
    if (result == FailedToStart)
        emit launchFailed(id, game, message);
    else
        emit processFinished(id, game, exitCode, result, duration);
}

void EmulatorManager::readOutput()
{
    if (!m_process)
        return;
    const QByteArray data = m_process->readAllStandardOutput();
    if (data.isEmpty() || m_outputBytes >= MaxLoggedOutputBytes)
        return;
    m_outputBytes += data.size();
    m_outputBuffer += data;
    qsizetype newline;
    while ((newline = m_outputBuffer.indexOf('\n')) >= 0) {
        const QByteArray line = m_outputBuffer.left(newline);
        m_outputBuffer.remove(0, newline + 1);
        qInfo("[emulator:%s] %s", qPrintable(m_activeId), QString::fromUtf8(line).trimmed().toUtf8().constData());
    }
    if (m_outputBytes >= MaxLoggedOutputBytes) {
        flushOutput();
        qWarning("[emulator:%s] output exceeds 1 MiB, further output is not logged", qPrintable(m_activeId));
    }
}

void EmulatorManager::flushOutput()
{
    if (m_outputBuffer.isEmpty())
        return;
    qInfo("[emulator:%s] %s", qPrintable(m_activeId), QString::fromUtf8(m_outputBuffer).trimmed().toUtf8().constData());
    m_outputBuffer.clear();
}

} // namespace nexora
