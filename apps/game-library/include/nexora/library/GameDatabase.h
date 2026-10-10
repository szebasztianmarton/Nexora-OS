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
// Not thread-safe: use from the thread that called open().
class GameDatabase
{
public:
    static constexpr int SchemaVersion = 1;

    GameDatabase();
    ~GameDatabase();
    GameDatabase(const GameDatabase &) = delete;
    GameDatabase &operator=(const GameDatabase &) = delete;

    // Opens (creating if needed) |path|. A file that is not a usable database is moved to
    // <path>.corrupt-<timestamp> and a new one is created. ":memory:" is accepted.
    bool open(const QString &path);
    void close();
    bool isOpen() const;
    QString lastError() const { return m_lastError; }
    QStringList issues() const { return m_issues; }

    // Merges a scan: upserts found files and marks entries under |result.scannedRoots| (and
    // entries whose root is no longer in |configuredRoots|) that were not found as missing.
    std::optional<ApplyStats> applyScan(const ScanResult &result, const QStringList &configuredRoots);

    QList<GameEntry> entries(bool includeMissing = false) const;
    std::optional<GameEntry> entry(qint64 id) const;
    bool setFavorite(qint64 id, bool favorite);
    bool recordLaunch(qint64 id, const QDateTime &when = QDateTime::currentDateTimeUtc());
    bool setEmulatorOverride(qint64 id, const QString &emulatorId);
    // Removes entries that are not present (explicit user action).
    int purgeMissing();

    QString meta(const QString &key) const;
    bool setMeta(const QString &key, const QString &value);

private:
    bool exec(const QString &sql);
    bool migrate();

    QString m_connection;
    QString m_lastError;
    QStringList m_issues;
};

} // namespace nexora::library
