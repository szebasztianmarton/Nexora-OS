#pragma once

#include <QDateTime>
#include <QFutureWatcher>
#include <QObject>
#include <QPointer>
#include <QSet>
#include <QStringList>

#include "gamemodel.h"
#include "jsonstore.h"

namespace nexora {

class AppSettings;
class EmulatorRegistry;

struct ScanResult
{
    QList<GameEntry> entries;  // without persisted state applied
    QStringList skipped;       // human readable: "/path: permission denied", "/path: directory not found"
};

// Indexes user-configured library directories in a worker thread and owns the
// per-game persisted state (<configDir>/library-state.json):
//   { "version": 1,
//     "favorites": [path...],
//     "recent": [{"path": p, "lastPlayed": iso8601}...]   (capped at 20, newest first)
//     "overrides": {path: emulatorId},
//     "hidden": [path...],                                  (removed from library, files untouched)
//     "firstSeen": {path: iso8601} }
// Only files whose extension is handled by a configured, enabled emulator are
// indexed. Directories are only those in AppSettings::libraryDirectories;
// nothing else on the filesystem is ever scanned. Recursion depth is limited to 4
// and symlinked directories are not followed.
// Artwork: a sidecar image with the same base name (.png/.jpg/.jpeg/.webp) next
// to the game file, or inside a "covers" or "artwork" sub-folder of the file's
// folder.
class GameLibrary : public QObject
{
    Q_OBJECT
    Q_PROPERTY(nexora::GameModel *model READ model CONSTANT)
    Q_PROPERTY(bool scanning READ scanning NOTIFY scanningChanged)
    Q_PROPERTY(bool hasScanned READ hasScanned NOTIFY scanFinished)     // at least one scan completed this session
    Q_PROPERTY(QDateTime lastScanTime READ lastScanTime NOTIFY scanFinished)
    Q_PROPERTY(QStringList skippedEntries READ skippedEntries NOTIFY scanFinished)
    Q_PROPERTY(int hiddenCount READ hiddenCount NOTIFY stateChanged)
    Q_PROPERTY(int favoriteCount READ favoriteCount NOTIFY stateChanged)
    Q_PROPERTY(int recentCount READ recentCount NOTIFY stateChanged)
    Q_PROPERTY(QString stateError READ stateError NOTIFY stateChanged) // problem loading/saving library-state.json

public:
    GameLibrary(const QString &configDirectory, AppSettings *settings, EmulatorRegistry *registry,
                QObject *parent = nullptr);
    ~GameLibrary() override; // waits for a running scan

    GameModel *model() const { return m_model; }
    bool scanning() const;
    bool hasScanned() const;
    QDateTime lastScanTime() const;
    QStringList skippedEntries() const;
    int hiddenCount() const;
    int favoriteCount() const;
    int recentCount() const;
    QString stateError() const;

    // Starts an asynchronous scan. If a scan is running, a rescan is queued
    // to start when it finishes (never two concurrent scans).
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void setFavorite(const QString &path, bool favorite);
    Q_INVOKABLE bool toggleFavorite(const QString &path);           // returns new state
    Q_INVOKABLE void setEmulatorOverride(const QString &path, const QString &emulatorId); // "" clears
    Q_INVOKABLE void removeFromLibrary(const QString &path);        // hides entry; file untouched
    Q_INVOKABLE void restoreHiddenEntries();                         // then rescans
    Q_INVOKABLE QVariantMap get(const QString &path) const;          // {} if unknown
    Q_INVOKABLE void recordLaunch(const QString &path);              // updates lastPlayed + recent list
    Q_INVOKABLE QString mostRecentPath() const;                      // most recently played existing entry or ""

    // Synchronous scan used by the worker thread and by tests.
    static ScanResult scanDirectories(const QStringList &directories, const QStringList &extensions,
                                      const QStringList &hiddenPaths, int maxDepth = 4);
    static QString titleFromFileName(const QString &fileName);
    static QString findArtwork(const QString &gamePath);

signals:
    void scanningChanged();
    void scanFinished();
    void stateChanged();
    void favoriteChanged(const QString &path, bool favorite);

private:
    void loadState();
    void saveState();
    void applyState(GameEntry &entry) const;
    void onScanDone();
    void refreshEmulatorInfo();
    void updateGameCounts();
    void reresolveEmulators();
    QString resolveEmulator(const GameEntry &entry, bool *overridden) const;

    GameModel *m_model;
    QPointer<AppSettings> m_settings;
    QPointer<EmulatorRegistry> m_registry;
    JsonStore m_store;
    QFutureWatcher<ScanResult> m_watcher;
    bool m_scanning = false;   // true until onScanDone() consumed the result
    bool m_rescanQueued = false;
    bool m_hasScanned = false;
    QDateTime m_lastScan;
    QStringList m_skipped;
    QString m_stateError;
    QSet<QString> m_favorites;
    QList<QPair<QString, QDateTime>> m_recent;
    QHash<QString, QString> m_overrides;
    QSet<QString> m_hidden;
    QHash<QString, QDateTime> m_firstSeen;
};

} // namespace nexora
