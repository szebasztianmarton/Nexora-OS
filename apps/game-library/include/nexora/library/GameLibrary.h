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

// Owns the game database and runs directory scans on a worker thread. Database writes
// happen on the owner's thread when a scan completes. At most one scan runs at a time;
// a refresh requested meanwhile is queued and runs once afterwards.
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
    std::optional<GameEntry> entry(qint64 id) const;
    bool setFavorite(qint64 id, bool favorite);
    bool recordLaunch(qint64 id);
    bool setEmulatorOverride(qint64 id, const QString &emulatorId);
    int purgeMissing();

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
    void scanFinished(const nexora::library::ApplyStats &stats, int issueCount);
    // The scan ran but its results could not be stored.
    void scanFailed(const QString &error);

private:
    void onScanDone(const ScanResult &result);
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
    QPointer<QThread> m_thread;
    std::shared_ptr<std::atomic_bool> m_cancel;
    bool m_rerunQueued = false;
};

} // namespace nexora::library
