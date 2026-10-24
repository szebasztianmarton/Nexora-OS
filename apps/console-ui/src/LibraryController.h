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

#include <map>
#include <memory>

namespace nexora::shared {
class Settings;
}

namespace nexora::ui {

class AppController;
class GameSessionManager;

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
    Q_PROPERTY(int missingCount READ missingCount NOTIFY contentChanged)
    Q_PROPERTY(int scanFilesSeen READ scanFilesSeen NOTIFY scanProgressChanged)
    Q_PROPERTY(QString scanCurrentRoot READ scanCurrentRoot NOTIFY scanProgressChanged)
    // systemFilter value for the games without a recognised system (the "Other" bucket,
    // whose SystemListModel row has an empty systemId).
    Q_PROPERTY(QString otherSystemFilter READ otherSystemFilter CONSTANT)
    // Pinned games in pin order (Home "Pinned" row).
    Q_PROPERTY(nexora::ui::GameListModel *pinnedGames READ pinnedGames CONSTANT)
    // [{id, name, count}] sorted by name (docs/game-library.md#collections).
    Q_PROPERTY(QVariantList collections READ collections NOTIFY collectionsChanged)
    // Games tab: 0 = no collection filter, otherwise only that collection's games.
    Q_PROPERTY(qint64 collectionFilter READ collectionFilter WRITE setCollectionFilter NOTIFY queryChanged)
    // Home: the visible sections in order (home.sections without collections that are gone).
    Q_PROPERTY(QStringList homeSections READ homeSections NOTIFY homeLayoutChanged)
    // Settings › Home: every known section {id, label, visible}, visible ones first in their
    // order (see homeSectionRows()).
    Q_PROPERTY(QVariantList homeSectionRows READ homeSectionRows NOTIFY homeLayoutChanged)

public:
    LibraryController(library::GameLibrary *library, shared::Settings *settings, AppController *app,
                      const shared::SystemCatalog *catalog, QObject *parent = nullptr);

    GameListModel *games() { return &m_games; }
    GameListModel *recentGames() { return &m_recent; }
    GameListModel *favoriteGames() { return &m_favorites; }
    GameListModel *recentlyAdded() { return &m_recentlyAdded; }
    SystemListModel *systems() { return &m_systems; }
    GameListModel *pinnedGames() { return &m_pinned; }
    QVariantList collections() const;
    qint64 collectionFilter() const { return m_collectionFilter; }
    void setCollectionFilter(qint64 collectionId);
    QStringList homeSections() const;

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
    int missingCount() const { return m_library->missingCount(); }
    int scanFilesSeen() const { return m_scanFilesSeen; }
    QString scanCurrentRoot() const { return m_scanCurrentRoot; }
    QString otherSystemFilter() const { return library::UnknownSystemFilter; }

    // Settings › Emulators › Default emulator. [{id: "", name: "Automatic"}, then every
    // enabled emulator that declares |systemId| ({id, name})]; empty for "" ("Other") or a
    // system the catalog does not know.
    Q_INVOKABLE QVariantList defaultEmulatorChoices(const QString &systemId) const;
    // Stores the default emulator of a catalog system ("" = automatic). Only an emulator
    // offered by defaultEmulatorChoices() is accepted; anything else returns false and
    // leaves the setting alone.
    Q_INVOKABLE bool setDefaultEmulator(const QString &systemId, const QString &emulatorId);

    Q_INVOKABLE bool refresh();
    Q_INVOKABLE bool cancelScan();
    Q_INVOKABLE bool launch(qint64 gameId);
    // Launches with an explicit profile; "" = the emulator's own arguments (no profile).
    Q_INVOKABLE bool launchWithProfile(qint64 gameId, const QString &profileId);
    Q_INVOKABLE bool toggleFavorite(qint64 gameId);
    // Adds/removes a library directory (persisted in settings) and rescans.
    Q_INVOKABLE bool addDirectory(const QString &path);
    Q_INVOKABLE bool removeDirectory(const QString &path);
    Q_INVOKABLE int purgeMissing();

    // {id (also as gameId), title, originalTitle, path (redacted), size, systemId, systemName,
    //  systemCandidates, emulatorId (the effective one), emulatorName, emulatorCandidates,
    //  "automatic" (no usable override set), "emulators" (every emulator declaring the file's
    //  extension: id, name, available, reason, selected), "profiles" (enabled launch profiles
    //  of the effective emulator: id, name, isDefault), favorite, playCount, lastPlayed (ISO),
    //  lastPlayedText (local, for display), addedAt (ISO), present, notes, playSeconds,
    //  playTimeText, pinned}. Empty map for an unknown id.
    Q_INVOKABLE QVariantMap gameDetails(qint64 id) const;
    // Display name of a catalog system id ("" for an unknown or empty id). Used by the
    // game details screen to label the system candidates it offers.
    Q_INVOKABLE QString systemName(const QString &systemId) const;
    // |title| empty = use the derived (file name) title. setGameMetadata, setGameSystem and
    // removeGame emit notice(..., true) when they fail.
    Q_INVOKABLE bool setGameMetadata(qint64 id, const QString &title, const QString &notes);
    // |systemId| empty clears the override.
    Q_INVOKABLE bool setGameSystem(qint64 id, const QString &systemId);
    // Chooses which emulator launches the game; "" returns to automatic. Only emulators that
    // declare the file's extension are accepted.
    Q_INVOKABLE bool setGameEmulator(qint64 id, const QString &emulatorId);
    // Hides the entry from the library; the game file itself is never touched.
    Q_INVOKABLE bool removeGame(qint64 id);
    // Unhides every removed game. Returns how many were restored.
    Q_INVOKABLE int restoreRemovedGames();

    // Collections. Failures emit notice(..., true). createCollection returns the new id or 0;
    // with |gameId| > 0 that game is added to it. Deleting a collection never removes games.
    Q_INVOKABLE qint64 createCollection(const QString &name, qint64 gameId = 0);
    Q_INVOKABLE bool renameCollection(qint64 id, const QString &name);
    Q_INVOKABLE bool deleteCollection(qint64 id);
    Q_INVOKABLE bool setInCollection(qint64 gameId, qint64 collectionId, bool member);
    Q_INVOKABLE QVariantList collectionsOf(qint64 gameId) const; // collection ids
    // A view of one collection's games, in the order they were added; owned and kept up to
    // date by the controller. Null for an unknown id.
    Q_INVOKABLE nexora::ui::GameListModel *collectionGames(qint64 collectionId);
    Q_INVOKABLE QString collectionName(qint64 collectionId) const;
    Q_INVOKABLE bool setPinned(qint64 gameId, bool pinned);

    // Settings › Home: every known section {id, label, visible}, visible ones first in their
    // order. Moving only reorders visible sections; showing appends to the end.
    QVariantList homeSectionRows() const;
    Q_INVOKABLE bool setHomeSectionVisible(const QString &id, bool visible);
    Q_INVOKABLE bool moveHomeSection(const QString &id, int delta);
    Q_INVOKABLE void resetHomeLayout();

    // "3 h 12 min", "45 min", "Less than a minute", "Not played yet".
    static QString formatPlayTime(qint64 seconds);

    // Recomputes scan options from emulator definitions + settings; rescans only when
    // they changed since the last completed scan (or |force|).
    // |autoScan| = false only updates the options (the caller scans itself).
    void updateScanOptions(bool force = false, bool autoScan = true);
    // Game launches go through the session manager (Quick Resume) when set.
    void setSessions(GameSessionManager *sessions);

signals:
    void queryChanged();
    void scanningChanged();
    void contentChanged();
    void directoriesChanged();
    void scanProgressChanged();
    // For the UI: a message to show after an action (e.g. "Added /games").
    void notice(const QString &message, bool isError);
    void collectionsChanged();
    void homeLayoutChanged();

private:
    void rebuildViews();
    void rebuildCollectionModel(qint64 id, GameListModel *model, const QList<library::GameEntry> &all);
    void setupModel(GameListModel *model);
    bool writeHomeSections(const QStringList &sections);
    void rebuildEmulatorInfo();
    bool launchEntry(qint64 gameId, const QString &profileId, bool applyDefaultProfile);
    void rebuildSystems();
    // Why an edit of game |id| failed, for the notice shown to the user.
    QString failureReason(qint64 id) const;
    // The emulator the entry would actually launch with: its override if still a
    // candidate; otherwise the settings default for its system if that is a candidate;
    // otherwise the first candidate.
    QString effectiveEmulatorId(const library::GameEntry &entry) const;

    library::GameLibrary *m_library;
    shared::Settings *m_settings;
    AppController *m_app;
    GameSessionManager *m_sessions = nullptr;
    const shared::SystemCatalog *m_catalog;
    library::QueryOptions m_query;
    GameListModel m_games;
    GameListModel m_recent;
    GameListModel m_favorites;
    GameListModel m_recentlyAdded;
    GameListModel m_pinned;
    std::map<qint64, std::unique_ptr<GameListModel>> m_collectionModels;
    qint64 m_collectionFilter = 0;
    SystemListModel m_systems;
    QTimer m_searchDebounce;
    QHash<QString, std::pair<QString, QString>> m_emulatorInfo; // id -> (name, unavailable reason)
    qint64 m_pendingLaunch = 0; // recorded as played once its process is running
    qint64 m_runningGame = 0; // gets the play time when its process ends
    int m_scanFilesSeen = 0;
    QString m_scanCurrentRoot;
};

} // namespace nexora::ui
