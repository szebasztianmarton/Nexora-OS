#pragma once

#include "nexora/library/GameEntry.h"
#include "nexora/library/LibraryScanner.h"

#include <QDateTime>
#include <QList>
#include <QString>
#include <QStringList>

#include <optional>

namespace nexora::library {

struct ApplyStats
{
    int added = 0;
    int updated = 0;
    int missing = 0; // entries now marked not present
};

// SQLite store for game metadata (QtSql "QSQLITE" driver). Keeps user data (favorites,
// play history, emulator choice) across rescans; files that disappear are marked
// not-present rather than deleted, so a temporarily unmounted drive loses nothing.
// Schema v2 adds systems, user metadata (title, notes, system) and hidden entries; v3 adds
// play time, pinned games and collections. An older database is copied to
// "<path>.v<N>-backup-<timestamp>" before it is migrated (v1 -> v3 in one transaction).
// Not thread-safe: use from the thread that called open().
class GameDatabase
{
public:
    static constexpr int SchemaVersion = 3;
    static constexpr int MaxPinned = 24;
    static constexpr int MaxCollections = 100;
    static constexpr int MaxGamesPerCollection = 5000;
    static constexpr int MaxCollectionNameLength = 60;

    struct Collection
    {
        qint64 id = 0;
        QString name;
        QDateTime createdAt; // UTC
        int count = 0; // present, not hidden games
    };

    GameDatabase();
    ~GameDatabase();
    GameDatabase(const GameDatabase &) = delete;
    GameDatabase &operator=(const GameDatabase &) = delete;

    // Opens (creating if needed) |path|. A file that is not a usable database is moved to
    // <path>.corrupt-<timestamp> (with its -wal file) and a new one is created. A database
    // from a newer schema or a busy/locked one is left untouched and open() fails. So is a
    // v1 database whose migration fails: it is rolled back, never treated as corrupt, and the
    // backup stays.
    // ":memory:" is accepted. With |allowRecovery| false an unusable file is never moved
    // aside (used for the scan worker's second connection, while the owner holds the file).
    bool open(const QString &path, bool allowRecovery = true);
    void close();
    bool isOpen() const;
    QString lastError() const { return m_lastError; }
    QStringList issues() const { return m_issues; }
    // The backup written by the last open() that migrated a v1 database; empty otherwise.
    QString lastBackupPath() const { return m_backupPath; }

    // Merges a scan: upserts found files and marks entries under |result.scannedRoots| (and
    // entries whose root is no longer in |configuredRoots|) that were not found as missing.
    // Runs in one BEGIN IMMEDIATE transaction, so concurrent writers on other connections
    // wait (busy timeout) instead of failing the merge. Never touches favorites, play
    // history or emulator overrides.
    std::optional<ApplyStats> applyScan(const ScanResult &result, const QStringList &configuredRoots);

    // Hidden entries are those removed from the library by the user; they stay in the table so
    // rescans do not bring them back.
    QList<GameEntry> entries(bool includeMissing = false, bool includeHidden = false) const;
    std::optional<GameEntry> entry(qint64 id) const;
    bool setFavorite(qint64 id, bool favorite);
    bool recordLaunch(qint64 id, const QDateTime &when = QDateTime::currentDateTimeUtc());
    bool setEmulatorOverride(qint64 id, const QString &emulatorId);
    // Title at most 200 characters, notes at most 2000; no control characters (notes may
    // contain newlines). Empty strings clear the override. Returns false with lastError().
    bool setMetadata(qint64 id, const QString &titleOverride, const QString &notes);
    // Empty clears. The caller checks that the system exists.
    bool setSystemOverride(qint64 id, const QString &systemId);
    bool setHidden(qint64 id, bool hidden);
    // Unhides every entry; returns how many were hidden.
    int restoreHidden();
    // Removes entries that are not present (explicit user action), with their collection
    // memberships.
    int purgeMissing();
    int missingCount() const;

    // Adds |seconds| (> 0) of play time.
    bool addPlayTime(qint64 id, qint64 seconds);
    // Pinned games get the next pin order; at most MaxPinned. Unpinning clears it.
    bool setPinned(qint64 id, bool pinned);

    // Collections: names 1-60 characters, unique ignoring case, no control characters; at most
    // MaxCollections, each with at most MaxGamesPerCollection games. Deleting a collection
    // or removing a game from it never touches the game itself.
    QList<Collection> collections() const;
    std::optional<qint64> createCollection(const QString &name);
    bool renameCollection(qint64 id, const QString &name);
    bool deleteCollection(qint64 id);
    bool setInCollection(qint64 collectionId, qint64 gameId, bool member);
    QList<qint64> collectionGameIds(qint64 collectionId) const; // in the order they were added
    QList<qint64> collectionsOf(qint64 gameId) const;

    QString meta(const QString &key) const;
    bool setMeta(const QString &key, const QString &value);

    // Writes a consistent snapshot of the open database to |destFile| via SQLite's
    // "VACUUM INTO" (safe to run while other connections are reading/writing). |destFile|'s
    // parent directory must already exist. Returns false with lastError() on failure.
    bool backupTo(const QString &destFile, QString *error) const;

private:
    bool exec(const QString &sql);
    bool migrate();
    bool createSchema();
    bool backupBeforeMigration(int fromVersion);
    bool migrateFrom(int version);
    bool checkCollectionName(const QString &name, qint64 exceptId);

    QString m_connection;
    QString m_path;
    QString m_backupPath;
    bool m_recoverable = true; // false: failure must not be treated as corruption
    QString m_lastError;
    QStringList m_issues;
};

} // namespace nexora::library
