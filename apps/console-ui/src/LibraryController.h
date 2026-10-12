#pragma once

#include "GameListModel.h"
#include "SystemListModel.h"

#include "nexora/library/GameLibrary.h"
#include "nexora/library/LibraryQuery.h"
#include "nexora/shared/SystemCatalog.h"

#include <QObject>
#include <QTimer>
#include <QVariantMap>
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
    Q_PROPERTY(nexora::ui::GameListModel *recentlyAdded READ recentlyAdded CONSTANT)
    Q_PROPERTY(nexora::ui::SystemListModel *systems READ systems CONSTANT)
    Q_PROPERTY(QString searchText READ searchText WRITE setSearchText NOTIFY queryChanged)
    Q_PROPERTY(QString filter READ filter WRITE setFilter NOTIFY queryChanged)
    Q_PROPERTY(QString sortOrder READ sortOrder WRITE setSortOrder NOTIFY queryChanged)
    Q_PROPERTY(QString systemFilter READ systemFilter WRITE setSystemFilter NOTIFY queryChanged)
    Q_PROPERTY(bool scanning READ isScanning NOTIFY scanningChanged)
    Q_PROPERTY(bool available READ isAvailable CONSTANT)
    Q_PROPERTY(int totalCount READ totalCount NOTIFY contentChanged)
    Q_PROPERTY(QStringList directories READ directories NOTIFY directoriesChanged)
    Q_PROPERTY(QStringList issues READ issues NOTIFY contentChanged)
    Q_PROPERTY(QString lastScanSummary READ lastScanSummary NOTIFY contentChanged)
    Q_PROPERTY(int scanFilesSeen READ scanFilesSeen NOTIFY scanProgressChanged)
    Q_PROPERTY(QString scanCurrentRoot READ scanCurrentRoot NOTIFY scanProgressChanged)

public:
    LibraryController(library::GameLibrary *library, shared::Settings *settings, AppController *app,
                      const shared::SystemCatalog *catalog, QObject *parent = nullptr);

    GameListModel *games() { return &m_games; }
    GameListModel *recentGames() { return &m_recent; }
    GameListModel *favoriteGames() { return &m_favorites; }
    GameListModel *recentlyAdded() { return &m_recentlyAdded; }
    SystemListModel *systems() { return &m_systems; }

    QString searchText() const { return m_query.search; }
    void setSearchText(const QString &text);
    QString filter() const;
    void setFilter(const QString &filter);
    QString sortOrder() const;
    void setSortOrder(const QString &order);
    QString systemFilter() const { return m_query.systemId; }
    void setSystemFilter(const QString &systemId);

    bool isScanning() const { return m_library->isScanning(); }
    bool isAvailable() const { return m_library->isOpen(); }
    int totalCount() const { return int(m_library->entries().size()); }
    QStringList directories() const;
    QStringList issues() const;
    QString lastScanSummary() const;
    int scanFilesSeen() const { return m_scanFilesSeen; }
    QString scanCurrentRoot() const { return m_scanCurrentRoot; }

    Q_INVOKABLE bool refresh();
    Q_INVOKABLE bool cancelScan();
    Q_INVOKABLE bool launch(qint64 gameId);
    Q_INVOKABLE bool toggleFavorite(qint64 gameId);
    // Adds/removes a library directory (persisted in settings) and rescans.
    Q_INVOKABLE bool addDirectory(const QString &path);
    Q_INVOKABLE bool removeDirectory(const QString &path);
    Q_INVOKABLE int purgeMissing();

    // {id, title, originalTitle, path (redacted), systemId, systemName, systemCandidates,
    //  emulatorId, emulatorName, emulatorCandidates, favorite, playCount, lastPlayed (ISO),
    //  addedAt (ISO), present, notes}. Empty map for an unknown id.
    Q_INVOKABLE QVariantMap gameDetails(qint64 id) const;
    // |title| empty = use the derived (file name) title.
    Q_INVOKABLE bool setGameMetadata(qint64 id, const QString &title, const QString &notes);
    // |systemId| empty clears the override.
    Q_INVOKABLE bool setGameSystem(qint64 id, const QString &systemId);
    // |emulatorId| empty clears the override.
    Q_INVOKABLE bool setGameEmulator(qint64 id, const QString &emulatorId);
    // Hides the entry from the library; the game file itself is never touched.
    Q_INVOKABLE bool removeGame(qint64 id);
    // Unhides every removed game. Returns how many were restored.
    Q_INVOKABLE int restoreRemovedGames();

    // Recomputes scan options from emulator definitions + settings; rescans only when
    // they changed since the last completed scan (or |force|).
    // |autoScan| = false only updates the options (the caller scans itself).
    void updateScanOptions(bool force = false, bool autoScan = true);

signals:
    void queryChanged();
    void scanningChanged();
    void contentChanged();
    void directoriesChanged();
    void scanProgressChanged();
    // For the UI: a message to show after an action (e.g. "Added /games").
    void notice(const QString &message, bool isError);

private:
    void rebuildViews();
    void rebuildEmulatorInfo();
    void rebuildSystems();
    // The emulator the entry would actually launch with: its override if still a
    // candidate; otherwise the settings default for its system if that is a candidate;
    // otherwise the first candidate.
    QString effectiveEmulatorId(const library::GameEntry &entry) const;

    library::GameLibrary *m_library;
    shared::Settings *m_settings;
    AppController *m_app;
    const shared::SystemCatalog *m_catalog;
    library::QueryOptions m_query;
    GameListModel m_games;
    GameListModel m_recent;
    GameListModel m_favorites;
    GameListModel m_recentlyAdded;
    SystemListModel m_systems;
    QTimer m_searchDebounce;
    QHash<QString, std::pair<QString, QString>> m_emulatorInfo; // id -> (name, unavailable reason)
    qint64 m_pendingLaunch = 0; // recorded as played once its process is running
    int m_scanFilesSeen = 0;
    QString m_scanCurrentRoot;
};

} // namespace nexora::ui
