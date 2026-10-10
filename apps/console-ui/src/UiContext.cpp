#include "UiContext.h"

#include "input/InputController.h"

#include "nexora/shared/Logging.h"

namespace nexora::ui {

UiContext::UiContext(const Options &options, input::InputController *input, QObject *parent)
    : QObject(parent)
    , m_settings(options.settings)
    , m_system(system::SystemIntegration::create(options.environment, options.platformOverride))
    , m_input(input)
    , m_databasePath(options.databasePath)
{
    m_settings.load(); // before anything reads preferences
    m_app = std::make_unique<AppController>(options.app);
    m_app->setDefaultProfiles(m_settings.mapValue(shared::Settings::EmulatorDefaultProfiles));
    if (m_input)
        m_app->setInputController(m_input);
    m_libraryController = std::make_unique<LibraryController>(&m_library, &m_settings, m_app.get());
    m_settingsController = std::make_unique<SettingsController>(&m_settings, m_input, m_system.get());
    m_systemController = std::make_unique<SystemController>(m_system.get());

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
    m_library.waitForScan();
}

} // namespace nexora::ui
