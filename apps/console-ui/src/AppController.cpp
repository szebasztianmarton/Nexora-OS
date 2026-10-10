#include "AppController.h"

#include "input/InputController.h"

#include "nexora/shared/Logging.h"

namespace nexora::ui {

using emulator::ProcessState;

AppController::AppController(const Options &options, QObject *parent)
    : QObject(parent)
    , m_options(options)
{
    connect(&m_manager, &emulator::EmulatorManager::stateChanged, this, &AppController::onManagerStateChanged);
    connect(&m_manager, &emulator::EmulatorManager::processFinished, this, &AppController::onProcessFinished);
    // Coalesce output notifications: a chatty emulator must not re-render the UI per chunk.
    m_outputNotifyTimer.setSingleShot(true);
    m_outputNotifyTimer.setInterval(250);
    connect(&m_outputNotifyTimer, &QTimer::timeout, this, &AppController::lastOutputChanged);
    connect(&m_manager, &emulator::EmulatorManager::outputReceived, this, [this] {
        if (!m_outputNotifyTimer.isActive())
            m_outputNotifyTimer.start();
    });
    reloadConfig();
}

void AppController::setInputController(input::InputController *input)
{
    if (m_input)
        m_input->disconnect(this);
    m_input = input;
    if (m_input) {
        connect(m_input, &input::InputController::statusChanged, this, &AppController::inputChanged);
        connect(m_input, &input::InputController::returnToLauncherRequested, this, &AppController::returnToLauncher);
        m_input->setSuspended(m_manager.isActive());
    }
    emit inputChanged();
}

QString AppController::state() const
{
    return emulator::nameOf(m_manager.state()).toLower();
}

QString AppController::lastOutput() const
{
    QString text;
    if (!m_manager.capturedStdout().isEmpty())
        text += QStringLiteral("[stdout]\n") + m_manager.capturedStdout();
    if (!m_manager.capturedStderr().isEmpty()) {
        if (!text.isEmpty() && !text.endsWith(QLatin1Char('\n')))
            text += QLatin1Char('\n');
        text += QStringLiteral("[stderr]\n") + m_manager.capturedStderr();
    }
    // Keep the UI light: only the tail is shown on screen.
    constexpr qsizetype maxChars = 4000;
    return text.size() > maxChars ? text.right(maxChars) : text;
}

QString AppController::inputBackend() const
{
    return m_input ? m_input->backendName() : QStringLiteral("keyboard only");
}

QString AppController::inputStatus() const
{
    return m_input ? m_input->statusText() : QStringLiteral("No controller input layer active");
}

QStringList AppController::inputDevices() const
{
    return m_input ? m_input->devices() : QStringList{};
}

QString AppController::version() const
{
    return QStringLiteral(NEXORA_VERSION);
}

bool AppController::launch(const QString &emulatorId)
{
    QString profile = m_defaultProfiles.value(emulatorId).toString();
    if (const auto config = m_model.find(emulatorId)) {
        const auto *p = config->profile(profile);
        if (!p || !p->enabled)
            profile.clear(); // stale default: fall back to the emulator's own arguments
    }
    return launchProfile(emulatorId, profile);
}

bool AppController::launchProfile(const QString &emulatorId, const QString &profileId)
{
    const auto config = m_model.find(emulatorId);
    if (!config) {
        setStatusMessage(QStringLiteral("Unknown emulator \"%1\"").arg(emulatorId), true);
        return false;
    }
    return startLaunch({*config, profileId, {}, {}});
}

bool AppController::launchGame(const QString &emulatorId, const QString &gamePath, const QString &title,
                               const QString &profileId)
{
    const auto config = m_model.find(emulatorId);
    if (!config) {
        setStatusMessage(emulatorId.isEmpty()
                             ? QStringLiteral("No emulator is configured for \"%1\"").arg(title)
                             : QStringLiteral("Emulator \"%1\" for \"%2\" is not configured").arg(emulatorId, title),
                         true);
        return false;
    }
    QString profile = profileId;
    if (profile.isEmpty()) {
        profile = m_defaultProfiles.value(emulatorId).toString();
        const auto *p = config->profile(profile);
        if (!p || !p->enabled)
            profile.clear();
    }
    const QString display = QStringLiteral("%1 (%2)").arg(title, config->name);
    return startLaunch({*config, profile, gamePath, display});
}

QString AppController::unavailableReason(const QString &emulatorId) const
{
    if (!m_model.find(emulatorId))
        return emulatorId.isEmpty() ? QStringLiteral("No emulator handles this file type")
                                    : QStringLiteral("Emulator \"%1\" is not configured").arg(emulatorId);
    return m_unavailable.value(emulatorId);
}

bool AppController::startLaunch(const emulator::LaunchRequest &request)
{
    const auto &config = request.emulator;
    if (!m_manager.isActive()) {
        m_activeName = !request.displayName.isEmpty() ? request.displayName
            : config.name.isEmpty()                   ? config.id
                                                      : config.name;
    }
    const auto result = m_manager.launch(request);
    if (!result.accepted) {
        // AlreadyActive leaves the running emulator untouched; validation errors were
        // already reported through processFinished().
        if (result.error == emulator::EmulatorError::AlreadyActive)
            setStatusMessage(result.message, true);
        return false;
    }
    setStatusMessage(result.message, false);
    emit launchStarted(config.id, request.gamePath);
    return true;
}

bool AppController::stop()
{
    return m_manager.stop();
}

bool AppController::returnToLauncher()
{
    if (m_manager.state() != emulator::ProcessState::Starting && m_manager.state() != emulator::ProcessState::Running) {
        return false;
    }
    qCInfo(lcUi).noquote() << "Returning to the launcher: stopping" << m_activeName;
    return m_manager.stop();
}

bool AppController::reloadConfig()
{
    if (m_manager.isActive()) {
        setStatusMessage(QStringLiteral("Cannot reload the configuration while an emulator is running"), true);
        return false;
    }

    const shared::ConfigLoadResult result = shared::ConfigLoader::loadDirectory(m_options.configDirectory);
    m_configDirectoryFound = result.directoryFound;
    m_configIssues.clear();
    for (const auto &issue : result.issues) {
        const QString prefix = issue.severity == shared::ConfigIssue::Severity::Error ? QStringLiteral("Error: ")
                                                                                      : QStringLiteral("Warning: ");
        m_configIssues.append(prefix + issue.toDisplayString());
    }

    QList<shared::EmulatorConfig> enabled;
    QHash<QString, QString> unavailable;
    m_disabled.clear();
    for (const auto &config : result.emulators) {
        if (!config.enabled) {
            m_disabled.append(config.name);
            continue;
        }
        QString detail;
        const auto error = emulator::EmulatorManager::validateExecutable(config.executable, &detail);
        if (error != emulator::EmulatorError::None)
            unavailable.insert(config.id, emulator::describe(error, detail));
        enabled.append(config);
    }
    m_model.setEmulators(enabled, unavailable);
    m_enabled = enabled;
    m_unavailable = unavailable;

    qCInfo(lcUi) << "Configuration:" << enabled.size() << "enabled," << m_disabled.size() << "disabled,"
                 << unavailable.size() << "unavailable," << m_configIssues.size() << "issue(s)";
    if (!result.directoryFound)
        setStatusMessage(QStringLiteral("Configuration directory not found"), true);
    else if (enabled.isEmpty())
        setStatusMessage(QStringLiteral("No emulators configured"), true);
    else
        setStatusMessage(QStringLiteral("%1 emulator(s) available").arg(enabled.size()), false);
    emit configChanged();
    return true;
}

QStringList AppController::recentLog() const
{
    return shared::recentLogLines();
}

void AppController::onManagerStateChanged(ProcessState state, const QString &emulatorId)
{
    switch (state) {
    case ProcessState::Starting:
        m_model.resetStatuses();
        m_model.setStatus(emulatorId, QStringLiteral("starting"), QStringLiteral("Starting…"));
        break;
    case ProcessState::Running:
        m_model.setStatus(emulatorId, QStringLiteral("running"), QStringLiteral("Running"));
        setStatusMessage(QStringLiteral("%1 is running").arg(m_activeName), false);
        break;
    case ProcessState::Stopping:
        m_model.setStatus(emulatorId, QStringLiteral("stopping"), QStringLiteral("Stopping…"));
        setStatusMessage(QStringLiteral("Stopping %1…").arg(m_activeName), false);
        break;
    case ProcessState::Idle:
    case ProcessState::Exited:
    case ProcessState::Failed: break; // handled in onProcessFinished with the full outcome
    }
    updateInputSuspension();
    emit stateChanged();
}

void AppController::onProcessFinished(const QString &emulatorId, const emulator::ProcessOutcome &outcome)
{
    const bool failed = outcome.finalState == ProcessState::Failed;
    m_model.resetStatuses();
    if (failed)
        m_model.setStatus(emulatorId, QStringLiteral("failed"), QStringLiteral("Failed"), outcome.message);
    else
        m_model.setStatus(emulatorId, QStringLiteral("exited"), QStringLiteral("Exited"), outcome.message);
    setStatusMessage(outcome.message, failed);
    m_outputNotifyTimer.stop();
    updateInputSuspension();
    emit lastOutputChanged();
    emit stateChanged();
}

void AppController::updateInputSuspension()
{
    // Gamepad input is not grabbed, so while an emulator runs the pad must not drive the
    // launcher (pressing A in a game would otherwise hit "Stop emulator").
    if (m_input)
        m_input->setSuspended(m_manager.isActive());
}

void AppController::setStatusMessage(const QString &message, bool isError)
{
    if (message == m_statusMessage && isError == m_statusIsError)
        return;
    m_statusMessage = message;
    m_statusIsError = isError;
    if (isError)
        qCWarning(lcUi).noquote() << message;
    else
        qCInfo(lcUi).noquote() << message;
    emit statusMessageChanged();
}

} // namespace nexora::ui
