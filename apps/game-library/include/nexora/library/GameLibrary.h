#pragma once

#include "nexora/library/GameDatabase.h"
#include "nexora/library/LibraryScanner.h"
#include "nexora/shared/SystemCatalog.h"

#include <QObject>
#include <QPointer>
#include <QThread>

#include <atomic>
#include <memory>

namespace nexora::library {

// Owns the game database and runs directory scans on a worker thread. The worker also
// merges the scan into the database through its own SQLite connection and reads the new
// entry list, so the owner's (UI) thread only swaps the cached list. (An in-memory
// database cannot be shared between connections; it is merged on the owner's thread.)
// At most one scan runs at a time; a refresh requested meanwhile is queued and runs once
// afterwards.
class GameLibrary : public QObject
{
    Q_OBJECT

public:
    explicit GameLibrary(QObject *parent = nullptr);
    ~GameLibrary() override;

    // $XDG_DATA_HOME/nexora/library.sqlite (QStandardPaths::GenericDataLocation).
    static QString defaultDatabasePath();

    bool open(const QString &databasePath);
    // Closes the database connection (settings/library restore only: the caller must not be
    // mid-scan). entries() returns the last cached content until open() is called again.
    void close();
    bool isOpen() const { return m_db.isOpen(); }
    QString lastError() const { return m_db.lastError(); }
    // Consistent snapshot of the open database (see GameDatabase::backupTo()).
    bool backupTo(const QString &destFile, QString *error) const { return m_db.backupTo(destFile, error); }

    // The systems used to identify games by extension (default: none, so no game gets a
    // system). Takes effect with the next scan.
    void setCatalog(const shared::SystemCatalog &catalog);
    const shared::SystemCatalog &catalog() const { return m_catalog; }

    void setScanOptions(const ScanOptions &options);
    const ScanOptions &scanOptions() const { return m_options; }
    // True when the configured directories/extensions differ from the last completed scan
    // (or nothing was scanned yet). Used to avoid rescanning on every start.
    bool needsRescan() const;

    // Starts an asynchronous scan. Returns false (and queues one rerun) if one is running.
    bool refresh();
    bool isScanning() const { return m_thread != nullptr; }

    QList<GameEntry> entries() const { return m_entries; }
    // Entries whose file is missing (present == false); not part of the normal cache (see
    // GameLibrary.h's class comment) because rescans must not resurrect a hidden entry.
    QList<GameEntry> missingEntries() const;
    std::optional<GameEntry> entry(qint64 id) const;
    bool setFavorite(qint64 id, bool favorite);
    bool recordLaunch(qint64 id);
    bool setEmulatorOverride(qint64 id, const QString &emulatorId);
    int purgeMissing();
    // Entries whose file is currently missing (kept for their favorites and history).
    int missingCount() const { return m_missingCount; }

    // User metadata. |title| empty = use the derived title. Fails (false, lastError()) for
    // overlong text, control characters or an unknown id.
    bool setMetadata(qint64 id, const QString &title, const QString &notes);
    // |systemId| empty clears; otherwise one of the entry's candidates or a catalog system.
    bool setSystemOverride(qint64 id, const QString &systemId);
    // Hides the entry from the library. The game file is never touched and rescans do not
    // bring the entry back.
    bool removeFromLibrary(qint64 id);
    // Unhides every removed entry; true if there was any.
    bool restoreHidden();
    QList<GameEntry> hiddenEntries() const;

    // Play time, pinning and collections (schema v3; see GameDatabase). Collection changes
    // emit collectionsChanged(); pinning and play time update the cached entry.
    bool addPlayTime(qint64 id, qint64 seconds);
    bool setPinned(qint64 id, bool pinned);
    QList<GameDatabase::Collection> collections() const { return m_db.collections(); }
    std::optional<qint64> createCollection(const QString &name);
    bool renameCollection(qint64 id, const QString &name);
    bool deleteCollection(qint64 id);
    bool setInCollection(qint64 collectionId, qint64 gameId, bool member);
    QList<qint64> collectionGameIds(qint64 collectionId) const { return m_db.collectionGameIds(collectionId); }
    QList<qint64> collectionsOf(qint64 gameId) const { return m_db.collectionsOf(gameId); }

    const ScanResult &lastScan() const { return m_lastScan; }
    ApplyStats lastStats() const { return m_lastStats; }
    QStringList databaseIssues() const { return m_db.issues(); }

    // Blocks until a running scan has finished (tests and shutdown only).
    void waitForScan();
    // Asks a running scan to stop and drops a queued rerun (shutdown).
    void cancelScan();

signals:
    void scanningChanged(bool scanning);
    // Queued from the scan thread: game files found so far and the directory being scanned.
    void scanProgress(int filesSeen, const QString &currentRoot);
    void entriesChanged();
    void collectionsChanged();
    void scanFinished(const nexora::library::ApplyStats &stats, int issueCount);
    // The scan ran but its results could not be stored.
    void scanFailed(const QString &error);

private:
    struct ScanJob;
    void onScanDone(ScanJob &job, quint64 writeGeneration);
    void reloadEntries();
    // Applies |update| to the cached entry |id| (avoids reloading the whole table).
    template<typename F> void updateCached(qint64 id, F update);
    QString optionsSignature() const;
    // Options as scanned: the configured ones plus the extension -> system map.
    ScanOptions effectiveOptions() const;
    // Replaces the cached copy of |id| with the stored one (drops it if hidden or missing).
    void refreshCached(qint64 id);

    GameDatabase m_db;
    ScanOptions m_options;
    shared::SystemCatalog m_catalog;
    QList<GameEntry> m_entries;
    ScanResult m_lastScan;
    ApplyStats m_lastStats;
    QString m_databasePath;
    // Bumped by every write through m_db; a worker snapshot taken while it changed is stale.
    quint64 m_writeGeneration = 0;
    int m_missingCount = 0;
    QPointer<QThread> m_thread;
    std::shared_ptr<std::atomic_bool> m_cancel;
    bool m_rerunQueued = false;
};

} // namespace nexora::library
