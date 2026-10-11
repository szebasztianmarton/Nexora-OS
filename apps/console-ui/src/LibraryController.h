#pragma once

#include "GameListModel.h"

#include "nexora/library/GameLibrary.h"
#include "nexora/library/LibraryQuery.h"

#include <QObject>
#include <QTimer>
#include <QtQml/qqmlregistration.h>

namespace nexora::shared {
class Settings;
}

namespace nexora::ui {

class AppController;

// QML-facing game library: search/filter/sort state, the visible lists, refresh and
// favorites. Scanning and storage live in nexora::library::GameLibrary.
class LibraryController : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Created by the application")

    Q_PROPERTY(nexora::ui::GameListModel *games READ games CONSTANT)
    Q_PROPERTY(nexora::ui::GameListModel *recentGames READ recentGames CONSTANT)
    Q_PROPERTY(nexora::ui::GameListModel *favoriteGames READ favoriteGames CONSTANT)
    Q_PROPERTY(QString searchText READ searchText WRITE setSearchText NOTIFY queryChanged)
    Q_PROPERTY(QString filter READ filter WRITE setFilter NOTIFY queryChanged)
    Q_PROPERTY(QString sortOrder READ sortOrder WRITE setSortOrder NOTIFY queryChanged)
    Q_PROPERTY(bool scanning READ isScanning NOTIFY scanningChanged)
    Q_PROPERTY(bool available READ isAvailable CONSTANT)
    Q_PROPERTY(int totalCount READ totalCount NOTIFY contentChanged)
    Q_PROPERTY(QStringList directories READ directories NOTIFY directoriesChanged)
    Q_PROPERTY(QStringList issues READ issues NOTIFY contentChanged)
    Q_PROPERTY(QString lastScanSummary READ lastScanSummary NOTIFY contentChanged)
    Q_PROPERTY(int missingCount READ missingCount NOTIFY contentChanged)

public:
    LibraryController(library::GameLibrary *library, shared::Settings *settings, AppController *app,
                      QObject *parent = nullptr);

    GameListModel *games() { return &m_games; }
    GameListModel *recentGames() { return &m_recent; }
    GameListModel *favoriteGames() { return &m_favorites; }

    QString searchText() const { return m_query.search; }
    void setSearchText(const QString &text);
    QString filter() const;
    void setFilter(const QString &filter);
    QString sortOrder() const;
    void setSortOrder(const QString &order);

    bool isScanning() const { return m_library->isScanning(); }
    bool isAvailable() const { return m_library->isOpen(); }
    int totalCount() const { return int(m_library->entries().size()); }
    QStringList directories() const;
    QStringList issues() const;
    QString lastScanSummary() const;
    int missingCount() const { return m_library->missingCount(); }

    Q_INVOKABLE bool refresh();
    Q_INVOKABLE bool launch(qint64 gameId);
    // Launches with an explicit profile; "" = the emulator's own arguments (no profile).
    Q_INVOKABLE bool launchWithProfile(qint64 gameId, const QString &profileId);
    // For the game options dialog: title, path, size, play history, the effective emulator,
    // "emulators" (every emulator declaring the file's extension: id, name, available,
    // reason, selected), "automatic" (no override set), and "profiles" (enabled launch
    // profiles of the effective emulator: id, name, isDefault). Empty map if unknown.
    Q_INVOKABLE QVariantMap gameDetails(qint64 gameId) const;
    // Chooses which emulator launches the game; "" returns to automatic (first candidate).
    // Only emulators that declare the file's extension are accepted.
    Q_INVOKABLE bool setGameEmulator(qint64 gameId, const QString &emulatorId);
    Q_INVOKABLE bool toggleFavorite(qint64 gameId);
    // Adds/removes a library directory (persisted in settings) and rescans.
    Q_INVOKABLE bool addDirectory(const QString &path);
    Q_INVOKABLE bool removeDirectory(const QString &path);
    Q_INVOKABLE int purgeMissing();

    // Recomputes scan options from emulator definitions + settings; rescans only when
    // they changed since the last completed scan (or |force|).
    // |autoScan| = false only updates the options (the caller scans itself).
    void updateScanOptions(bool force = false, bool autoScan = true);

signals:
    void queryChanged();
    void scanningChanged();
    void contentChanged();
    void directoriesChanged();
    // For the UI: a message to show after an action (e.g. "Added /games").
    void notice(const QString &message, bool isError);

private:
    void rebuildViews();
    void rebuildEmulatorInfo();
    bool launchEntry(qint64 gameId, const QString &profileId, bool applyDefaultProfile);

    library::GameLibrary *m_library;
    shared::Settings *m_settings;
    AppController *m_app;
    library::QueryOptions m_query;
    GameListModel m_games;
    GameListModel m_recent;
    GameListModel m_favorites;
    QTimer m_searchDebounce;
    QHash<QString, std::pair<QString, QString>> m_emulatorInfo; // id -> (name, unavailable reason)
    qint64 m_pendingLaunch = 0; // recorded as played once its process is running
};

} // namespace nexora::ui
