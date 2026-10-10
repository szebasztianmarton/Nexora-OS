#pragma once

#include "nexora/library/GameDatabase.h"
#include "nexora/library/LibraryScanner.h"

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

    const ScanResult &lastScan() const { return m_lastScan; }
    ApplyStats lastStats() const { return m_lastStats; }
    QStringList databaseIssues() const { return m_db.issues(); }

    // Blocks until a running scan has finished (tests and shutdown only).
    void waitForScan();

signals:
    void scanningChanged(bool scanning);
    void entriesChanged();
    void scanFinished(const nexora::library::ApplyStats &stats, int issueCount);

private:
    void onScanDone(const ScanResult &result);
    void reloadEntries();
    QString optionsSignature() const;

    GameDatabase m_db;
    ScanOptions m_options;
    QList<GameEntry> m_entries;
    ScanResult m_lastScan;
    ApplyStats m_lastStats;
    QPointer<QThread> m_thread;
    std::shared_ptr<std::atomic_bool> m_cancel;
    bool m_rerunQueued = false;
};

} // namespace nexora::library
