#pragma once

#include "nexora/shared/DataLayout.h"

#include <QObject>
#include <QStringList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

namespace nexora::system {
class SystemIntegration;
class BackupService;
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

class AppController;
class LibraryController;

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

    // Lightweight status only (uptime, load, memory, displays); never probes /proc or /sys
    // beyond what SystemIntegration::status() already reads cheaply. Also refreshes the
    // (heavier) hardware snapshot: safe to call after the first frame, never called from
    // the constructor.
    Q_INVOKABLE void refresh();
    Q_INVOKABLE bool powerOff();
    Q_INVOKABLE bool reboot();
    // Writes a diagnostics text bundle and returns its path, or "" on failure (lastError()
    // explains why).
    Q_INVOKABLE QString exportDiagnostics();
    // Returns the new backup's directory, or "" on failure (lastError()).
    Q_INVOKABLE QString createBackup(bool includeSaves);
    Q_INVOKABLE QStringList backups() const;
    // Closes the library database, restores the backup, reopens the database and reloads
    // settings. Returns false on failure (lastError()); the library is reopened either way.
    Q_INVOKABLE bool restoreBackup(const QString &dir);
    // Deletes only the contents of layout.cacheDir (artwork cache); never touches saves,
    // settings or the library database.
    Q_INVOKABLE bool clearArtworkCache();

signals:
    void statusChanged();
    void hardwareChanged();

private:
    void refreshStatus();

    Dependencies m_deps;
    QStringList m_lines;
    QString m_lastError;
    QVariantMap m_hardware;
    QString m_osVersion;
    QString m_buildId;
    QString m_board;
};

} // namespace nexora::ui
