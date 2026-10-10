#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

class QQmlEngine;
class QWindow;

namespace nexora {

class AppSettings;
class EmulatorRegistry;
class EmulatorManager;
class GameLibrary;
class ControllerManager;
class Diagnostics;

// Owns every backend service and exposes them to QML under `import Nexora.Core`:
//   Settings     -> AppSettings
//   Emulators    -> EmulatorRegistry
//   Launcher     -> EmulatorManager
//   Library      -> GameLibrary
//   Controllers  -> ControllerManager
//   Diagnostics  -> Diagnostics
//   GameFilterModel, DirectoryBrowser -> instantiable types
// Used by the application and by the QML test runner (with a temporary config
// directory), so both exercise the same wiring.
class Backend : public QObject
{
    Q_OBJECT
public:
    explicit Backend(const QString &configDirectory, QObject *parent = nullptr);
    ~Backend() override;

    void registerQmlTypes();
    void attachWindow(QWindow *window);

    AppSettings *settings() const { return m_settings; }
    EmulatorRegistry *emulators() const { return m_emulators; }
    EmulatorManager *launcher() const { return m_launcher; }
    GameLibrary *library() const { return m_library; }
    ControllerManager *controllers() const { return m_controllers; }
    Diagnostics *diagnostics() const { return m_diagnostics; }

    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    AppSettings *m_settings;
    EmulatorRegistry *m_emulators;
    EmulatorManager *m_launcher;
    GameLibrary *m_library;
    ControllerManager *m_controllers;
    Diagnostics *m_diagnostics;
    QWindow *m_window = nullptr;
    QStringList m_indexedExtensions;
};

} // namespace nexora
