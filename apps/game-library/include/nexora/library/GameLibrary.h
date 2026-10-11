#pragma once

#include "nexora/library/GameDatabase.h"
#include "nexora/library/LibraryScanner.h"

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
    bool isOpen() const { return m_db.isOpen(); }
    QString lastError() const { return m_db.lastError(); }

    void setScanOptions(const ScanOptions &options);
    const ScanOptions &scanOptions() const { return m_options; }
    // True when the configured directories/extensions differ from the last completed scan
    // (or nothing was scanned yet). Used to avoid rescanning on every start.
    bool needsRescan() const;

    // Starts an asynchronous scan. Returns false (and queues one rerun) if one is running.
    bool refresh();
    bool isScanning() const { return m_thread != nullptr; }

    QList<GameEntry> entries() const { return m_entries; }
    std::optional<GameEntry> entry(qint64 id) const;
    bool setFavorite(qint64 id, bool favorite);
    bool recordLaunch(qint64 id);
    bool setEmulatorOverride(qint64 id, const QString &emulatorId);
    int purgeMissing();
    // Entries whose file is currently missing (kept for their favorites and history).
    int missingCount() const { return m_missingCount; }

    const ScanResult &lastScan() const { return m_lastScan; }
    ApplyStats lastStats() const { return m_lastStats; }
    QStringList databaseIssues() const { return m_db.issues(); }

    // Blocks until a running scan has finished (tests and shutdown only).
    void waitForScan();
    // Asks a running scan to stop and drops a queued rerun (shutdown).
    void cancelScan();

signals:
    void scanningChanged(bool scanning);
    void entriesChanged();
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

    GameDatabase m_db;
    ScanOptions m_options;
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
