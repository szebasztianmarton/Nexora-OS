#pragma once

#include "nexora/shared/DataLayout.h"

#include <QElapsedTimer>
#include <QObject>
#include <QStringList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

namespace nexora::system {
class SystemIntegration;
class BackupService;
enum class PowerAction;
} // namespace nexora::system
namespace nexora::library {
class GameLibrary;
}
namespace nexora::shared {
class Settings;
}
namespace nexora::emulator {
class LaunchHistory;
}

namespace nexora::ui {

class GameSessionManager;

class AppController;
class LibraryController;
class SettingsController;

// QML-facing system status, hardware snapshot, power actions, backups and diagnostics.
class SystemController : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Created by the application")

    Q_PROPERTY(QString platform READ platform CONSTANT)
    Q_PROPERTY(bool powerSupported READ powerSupported NOTIFY statusChanged)
    Q_PROPERTY(QStringList statusLines READ statusLines NOTIFY statusChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY statusChanged)
    Q_PROPERTY(QVariantMap hardware READ hardware NOTIFY hardwareChanged)
    Q_PROPERTY(QString osVersion READ osVersion CONSTANT)
    Q_PROPERTY(QString buildId READ buildId CONSTANT)
    Q_PROPERTY(QString board READ board CONSTANT)
    // How many times refresh() has probed the hardware (diagnostics and tests).
    Q_PROPERTY(int refreshCount READ refreshCount NOTIFY hardwareChanged)

public:
    // Everything SystemController needs from the rest of the object graph. All pointers
    // must outlive this controller (UiContext owns everything for the controller's
    // lifetime).
    struct Dependencies
    {
        system::SystemIntegration *system = nullptr;
        system::BackupService *backup = nullptr;
        library::GameLibrary *library = nullptr;
        shared::Settings *settings = nullptr;
        SettingsController *settingsController = nullptr; // re-applied to live subsystems after restoreBackup()
        emulator::LaunchHistory *history = nullptr;
        AppController *app = nullptr;
        LibraryController *libraryController = nullptr;
        shared::DataLayout layout;
        QString databasePath; // used to reopen the library after restoreBackup()
    };

    explicit SystemController(Dependencies deps, QObject *parent = nullptr);

    QString platform() const;
    bool powerSupported() const;
    QStringList statusLines() const { return m_lines; }
    QString lastError() const { return m_lastError; }
    QVariantMap hardware() const { return m_hardware; }
    QString osVersion() const { return m_osVersion; }
    QString buildId() const { return m_buildId; }
    QString board() const { return m_board; }
    int refreshCount() const { return m_refreshCount; }

    // Lightweight status only (uptime, load, memory, displays); never probes /proc or /sys
    // beyond what SystemIntegration::status() already reads cheaply. Also refreshes the
    // (heavier) hardware snapshot: safe to call after the first frame, never called from
    // the constructor.
    Q_INVOKABLE void refresh();
    // refresh() unless the last one is younger than |maxAgeMs|; returns whether it probed.
    // Screens call this when they become current, so the synchronous probe neither runs on
    // the start-up path nor on every tab switch.
    Q_INVOKABLE bool refreshIfStale(int maxAgeMs);
    // While a game runs, the session manager first saves it for Quick Resume (or closes it,
    // if it cannot be saved); the request is sent once no emulator runs, or dropped if the
    // save failed (the game then keeps running). Returns false only on an immediate failure.
    Q_INVOKABLE bool powerOff();
    Q_INVOKABLE bool reboot();
    void setSessions(GameSessionManager *sessions) { m_sessions = sessions; }
    // Writes a diagnostics text bundle and returns its path, or "" on failure (lastError()
    // explains why).
    Q_INVOKABLE QString exportDiagnostics();
    // Returns the new backup's directory, or "" on failure (lastError()).
    Q_INVOKABLE QString createBackup(bool includeSaves);
    Q_INVOKABLE QStringList backups() const;
    // Closes the library database, restores the backup, reopens the database, reloads
    // settings and re-applies them to the live subsystems (volume/mute, controller mapping,
    // deadzone, log level) so a restore takes effect immediately, not just in the Settings
    // store. Returns false on failure (lastError()); the library is reopened either way.
    Q_INVOKABLE bool restoreBackup(const QString &dir);
    // Deletes only the contents of layout.cacheDir (artwork cache); never touches saves,
    // settings or the library database.
    Q_INVOKABLE bool clearArtworkCache();

signals:
    void statusChanged();
    void hardwareChanged();

private:
    void refreshStatus();
    bool requestPower(system::PowerAction action);

    Dependencies m_deps;
    GameSessionManager *m_sessions = nullptr;
    QStringList m_lines;
    QString m_lastError;
    QVariantMap m_hardware;
    QString m_osVersion;
    QString m_buildId;
    QString m_board;
    int m_refreshCount = 0;
    QElapsedTimer m_lastRefresh; // invalid until the first refresh()
};

} // namespace nexora::ui
