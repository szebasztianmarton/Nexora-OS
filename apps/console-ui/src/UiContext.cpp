#include "UiContext.h"

#include "input/InputController.h"

#include "nexora/shared/Logging.h"

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

    connect(&m_settings, &shared::Settings::valueChanged, this, [this](const QString &key, const QVariant &value) {
        if (key == shared::Settings::EmulatorDefaultProfiles)
            m_app->setDefaultProfiles(value.toMap());
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
    m_settingsController->applyAll();
    if (!m_library.open(m_databasePath))
        qCCritical(lcLibrary).noquote() << "Game library disabled:" << m_library.lastError();
    // Only rescans when directories/extensions changed since the last completed scan.
    m_libraryController->updateScanOptions();
}

void UiContext::shutdown()
{
    if (m_app)
        m_app->manager()->shutdown();
    m_library.cancelScan(); // do not walk a large/slow library while quitting
    m_library.waitForScan();
}

} // namespace nexora::ui
