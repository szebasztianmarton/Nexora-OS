#include "UiContext.h"

#include "input/InputController.h"

#include "nexora/shared/Logging.h"

#include <QDir>
#include <QFile>

namespace nexora::ui {

namespace {

shared::SystemCatalog loadCatalog(const QString &file)
{
    if (file.isEmpty())
        return shared::SystemCatalog::builtIn();
    QStringList issues;
    if (auto loaded = shared::SystemCatalog::load(file, &issues))
        return *loaded;
    qCWarning(lcLibrary).noquote() << "System catalog" << file << "could not be loaded, using the built-in catalog:"
                                   << issues.join(QStringLiteral("; "));
    return shared::SystemCatalog::builtIn();
}

} // namespace

UiContext::UiContext(const Options &options, input::InputController *input, QObject *parent)
    : QObject(parent)
    , m_layout(options.layout)
    , m_settings(options.settings)
    , m_catalog(loadCatalog(options.systemCatalogFile))
    , m_system(system::SystemIntegration::create(options.environment, options.platformOverride))
    , m_launchHistory(options.layout.stateDir + QStringLiteral("/launch-history.json"))
    , m_input(input)
    , m_databasePath(options.databasePath.isEmpty() ? options.layout.dataDir + QStringLiteral("/library.sqlite")
                                                    : options.databasePath)
    , m_updateStateDir(options.update.stagingDir)
{
    m_settings.load(); // before anything reads preferences
    m_library.setCatalog(m_catalog);

    m_backupService = std::make_unique<system::BackupService>(
        options.layout, [this](const QString &dest, QString *error) { return m_library.backupTo(dest, error); });
    m_updateService = std::make_unique<update::UpdateService>(options.update);

    m_app = std::make_unique<AppController>(options.app);
    m_app->setDefaultProfiles(m_settings.mapValue(shared::Settings::EmulatorDefaultProfiles));
    m_app->setLaunchHistory(&m_launchHistory);
    if (m_input)
        m_app->setInputController(m_input);
    m_libraryController = std::make_unique<LibraryController>(&m_library, &m_settings, m_app.get(), &m_catalog);
    m_settingsController = std::make_unique<SettingsController>(&m_settings, m_input, m_system.get());

    SystemController::Dependencies systemDeps;
    systemDeps.system = m_system.get();
    systemDeps.backup = m_backupService.get();
    systemDeps.library = &m_library;
    systemDeps.settings = &m_settings;
    systemDeps.settingsController = m_settingsController.get();
    systemDeps.history = &m_launchHistory;
    systemDeps.app = m_app.get();
    systemDeps.libraryController = m_libraryController.get();
    systemDeps.layout = m_layout;
    systemDeps.databasePath = m_databasePath;
    m_systemController = std::make_unique<SystemController>(systemDeps);

    const bool desktopPlatform = m_system->platform() == system::Platform::Desktop;
    m_updateController
        = std::make_unique<UpdateController>(m_updateService.get(), options.update.currentVersion.toString(),
                                             options.update.currentBuildId, desktopPlatform);
    m_gameBarController = std::make_unique<GameBarController>(m_app.get(), m_system.get(), m_input);
    // Result of the last update attempt (written by nexora-bootok after the trial boot):
    // Completed or RecoveryRequired until the user acknowledges it.
    m_updateService->loadPersistedState();

    m_notifications = std::make_unique<NotificationService>(m_layout.stateDir + QStringLiteral("/notifications.json"));
    wireNotifications();

    GameSessionManager::Dependencies sessionDeps;
    sessionDeps.app = m_app.get();
    sessionDeps.settings = &m_settings;
    sessionDeps.notifications = m_notifications.get();
    sessionDeps.statesDir = m_layout.statesDir;
    m_sessions = std::make_unique<GameSessionManager>(sessionDeps);
    m_libraryController->setSessions(m_sessions.get());
    m_systemController->setSessions(m_sessions.get());

    connect(&m_settings, &shared::Settings::valueChanged, this, [this](const QString &key, const QVariant &value) {
        if (key == shared::Settings::EmulatorDefaultProfiles)
            m_app->setDefaultProfiles(value.toMap());
        if (key.startsWith(QLatin1String("notifications.")))
            applyNotificationSettings();
    });
}

void UiContext::applyNotificationSettings()
{
    m_notifications->setQuiet(m_settings.boolValue(shared::Settings::NotificationsQuiet));
    m_notifications->setHiddenCategories(m_settings.stringListValue(shared::Settings::NotificationsHiddenCategories));
    m_notifications->setDurationMs(m_settings.intValue(shared::Settings::NotificationsDurationMs));
}

void UiContext::wireNotifications()
{
    using Category = NotificationService::Category;
    using Severity = NotificationService::Severity;
    applyNotificationSettings();
    if (m_input) {
        connect(m_input, &input::InputController::deviceConnected, this, [this](const QString &name) {
            m_notifications->post(Category::Controller, Severity::Info, tr("Controller connected: %1").arg(name));
        });
        connect(m_input, &input::InputController::deviceDisconnected, this, [this](const QString &name) {
            m_notifications->post(Category::Controller, Severity::Warning, tr("Controller disconnected: %1").arg(name));
        });
    }
    connect(m_app.get(), &AppController::statusMessageChanged, this, [this] {
        if (m_app->statusIsError())
            m_notifications->post(Category::Game, Severity::Warning, m_app->statusMessage());
    });
}

UiContext::~UiContext()
{
    shutdown();
}

void UiContext::start()
{
    QString layoutError;
    if (!m_layout.ensureDirectories(&layoutError))
        qCWarning(lcApp).noquote() << "Could not prepare data directories:" << layoutError;
    // Update state (last-update.json) lives in its own owner-only directory under stateDir;
    // nexora-bootok writes into it as the same user.
    if (!m_updateStateDir.isEmpty()) {
        if (QDir().mkpath(m_updateStateDir))
            QFile::setPermissions(m_updateStateDir,
                                  QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
        else
            qCWarning(lcApp).noquote() << "Could not create the update state directory" << m_updateStateDir;
    }
    m_settingsController->applyAll();
    m_notifications->loadHistory();
    m_sessions->load();
    if (!m_library.open(m_databasePath))
        qCCritical(lcLibrary).noquote() << "Game library disabled:" << m_library.lastError();
    // Only rescans when directories/extensions changed since the last completed scan.
    m_libraryController->updateScanOptions();
}

void UiContext::shutdown()
{
    if (m_sessions)
        m_sessions->shutdown();
    if (m_app)
        m_app->manager()->shutdown();
    m_library.cancelScan(); // do not walk a large/slow library while quitting
    m_library.waitForScan();
}

} // namespace nexora::ui
