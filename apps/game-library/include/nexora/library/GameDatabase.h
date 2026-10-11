#pragma once

#include "nexora/library/GameEntry.h"
#include "nexora/library/LibraryScanner.h"

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
// Schema v2 adds systems, user metadata (title, notes, system) and hidden entries; a v1
// database is copied to "<path>.v1-backup-<timestamp>" before it is migrated.
// Not thread-safe: use from the thread that called open().
class GameDatabase
{
public:
    static constexpr int SchemaVersion = 2;

    GameDatabase();
    ~GameDatabase();
    GameDatabase(const GameDatabase &) = delete;
    GameDatabase &operator=(const GameDatabase &) = delete;

    // Opens (creating if needed) |path|. A file that is not a usable database is moved to
    // <path>.corrupt-<timestamp> (with its -wal file) and a new one is created. A database
    // from a newer schema or a busy/locked one is left untouched and open() fails. So is a
    // v1 database whose migration fails: it is rolled back, never treated as corrupt, and the
    // backup stays.
    // ":memory:" is accepted.
    bool open(const QString &path);
    void close();
    bool isOpen() const;
    QString lastError() const { return m_lastError; }
    QStringList issues() const { return m_issues; }
    // The backup written by the last open() that migrated a v1 database; empty otherwise.
    QString lastBackupPath() const { return m_backupPath; }

    // Merges a scan: upserts found files and marks entries under |result.scannedRoots| (and
    // entries whose root is no longer in |configuredRoots|) that were not found as missing.
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
    // Removes entries that are not present (explicit user action).
    int purgeMissing();

    QString meta(const QString &key) const;
    bool setMeta(const QString &key, const QString &value);

private:
    bool exec(const QString &sql);
    bool migrate();
    bool createSchema();
    bool backupBeforeMigration(int fromVersion);
    bool migrateV1ToV2();

    QString m_connection;
    QString m_path;
    QString m_backupPath;
    bool m_recoverable = true; // false: failure must not be treated as corruption
    QString m_lastError;
    QStringList m_issues;
};

} // namespace nexora::library
