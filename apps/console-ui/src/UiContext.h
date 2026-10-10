#pragma once

#include "AppController.h"
#include "LibraryController.h"
#include "SettingsController.h"
#include "SystemController.h"

#include "nexora/library/GameLibrary.h"
#include "nexora/shared/Settings.h"
#include "nexora/system/SystemIntegration.h"

#include <QObject>

#include <memory>

namespace nexora::input {
class InputController;
}

namespace nexora::ui {

// Owns every backend object the QML UI talks to and wires them together. Used by main()
// and by the integration/QML tests so both exercise the same object graph.
class UiContext : public QObject
{
    Q_OBJECT

public:
    struct Options
    {
        AppController::Options app;
        shared::Settings::Paths settings;
        QString databasePath; // ":memory:" allowed
        QString platformOverride; // "", "desktop" or "embedded"
        system::SystemEnvironment environment;
    };

    explicit UiContext(const Options &options, input::InputController *input = nullptr, QObject *parent = nullptr);
    ~UiContext() override;

    shared::Settings &settingsStore() { return m_settings; }
    library::GameLibrary &gameLibrary() { return m_library; }
    system::SystemIntegration &systemIntegration() { return *m_system; }
    AppController *app() { return m_app.get(); }
    LibraryController *library() { return m_libraryController.get(); }
    SettingsController *settings() { return m_settingsController.get(); }
    SystemController *system() { return m_systemController.get(); }

    // Startup: load settings, open the library, apply preferences, scan if needed.
    void start();
    // Shutdown: stop the emulator and wait for a running scan. Settings need no flush: every
    // change is saved when it is made.
    void shutdown();

private:
    shared::Settings m_settings;
    library::GameLibrary m_library;
    std::unique_ptr<system::SystemIntegration> m_system;
    std::unique_ptr<AppController> m_app;
    std::unique_ptr<LibraryController> m_libraryController;
    std::unique_ptr<SettingsController> m_settingsController;
    std::unique_ptr<SystemController> m_systemController;
    input::InputController *m_input;
    QString m_databasePath;
};

} // namespace nexora::ui
