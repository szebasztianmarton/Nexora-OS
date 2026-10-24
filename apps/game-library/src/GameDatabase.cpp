#include "nexora/library/GameDatabase.h"

#include "nexora/shared/Logging.h"
#include "nexora/shared/SystemCatalog.h"

#include <QAtomicInteger>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QSet>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

#include <algorithm>

namespace nexora::library {

namespace {

QAtomicInteger<int> connectionCounter;

// SQLite stores a null QString as NULL, which the NOT NULL columns reject.
QString nonNull(const QString &text)
{
    return text.isNull() ? QStringLiteral("") : text;
}

QString joinList(const QStringList &list)
{
    return nonNull(list.join(u','));
}

QStringList splitList(const QString &text)
{
    return text.isEmpty() ? QStringList{} : text.split(u',', Qt::SkipEmptyParts);
}

constexpr int MaxTitleLength = 200;
constexpr int MaxNotesLength = 2000;

bool hasControlCharacters(const QString &text, bool allowNewline)
{
    return std::any_of(text.cbegin(), text.cend(), [allowNewline](QChar c) {
        return c.category() == QChar::Other_Control && !(allowNewline && c == u'\n');
    });
}

QString collectionsTable()
{
    return QStringLiteral("CREATE TABLE IF NOT EXISTS collections ("
                          " id INTEGER PRIMARY KEY,"
                          " name TEXT NOT NULL UNIQUE COLLATE NOCASE,"
                          " created_at INTEGER NOT NULL)");
}

QString collectionGamesTable()
{
    return QStringLiteral("CREATE TABLE IF NOT EXISTS collection_games ("
                          " collection_id INTEGER NOT NULL,"
                          " game_id INTEGER NOT NULL,"
                          " position INTEGER NOT NULL,"
                          " added_at INTEGER NOT NULL,"
                          " PRIMARY KEY (collection_id, game_id))");
}

bool isUnder(const QString &path, const QString &root)
{
    return path == root || path.startsWith(root + u'/');
}

} // namespace

GameDatabase::GameDatabase()
    : m_connection(QStringLiteral("nexora-library-%1").arg(connectionCounter.fetchAndAddRelaxed(1)))
{ }

GameDatabase::~GameDatabase()
{
    close();
}

bool GameDatabase::isOpen() const
{
    return QSqlDatabase::contains(m_connection) && QSqlDatabase::database(m_connection, false).isOpen();
}

void GameDatabase::close()
{
    if (!QSqlDatabase::contains(m_connection))
        return;
    {
        QSqlDatabase db = QSqlDatabase::database(m_connection, false);
        db.close();
    }
    QSqlDatabase::removeDatabase(m_connection);
}

bool GameDatabase::exec(const QString &sql)
{
    QSqlQuery q(QSqlDatabase::database(m_connection, false));
    if (!q.exec(sql)) {
        m_lastError = q.lastError().text();
        // SQLITE_BUSY (5) / SQLITE_LOCKED (6) are transient, not corruption.
        const QString code = q.lastError().nativeErrorCode();
        if (code == QLatin1String("5") || code == QLatin1String("6"))
            m_recoverable = false;
        qCWarning(lcLibrary).noquote() << "SQL failed:" << sql.left(80) << "-" << m_lastError;
        return false;
    }
    return true;
}

bool GameDatabase::open(const QString &path, bool allowRecovery)
{
    close();
    m_issues.clear();
    m_recoverable = true;
    m_backupPath.clear();
    m_path = path;
    if (!QSqlDatabase::isDriverAvailable(QStringLiteral("QSQLITE"))) {
        m_lastError = QStringLiteral("Qt SQLite driver (QSQLITE) is not available");
        qCCritical(lcLibrary).noquote() << m_lastError;
        return false;
    }
    const bool inMemory = path == QStringLiteral(":memory:");
    if (!inMemory)
        QDir().mkpath(QFileInfo(path).absolutePath());

    for (int attempt = 0; attempt < 2; ++attempt) {
        {
            QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connection);
            db.setDatabaseName(path);
            if (db.open() && exec(QStringLiteral("PRAGMA foreign_keys = ON")) && migrate()) {
                exec(QStringLiteral("PRAGMA journal_mode = WAL"));
                return true;
            }
            if (m_lastError.isEmpty())
                m_lastError = db.lastError().text();
        }
        close();
        if (inMemory || attempt == 1 || !QFile::exists(path) || !m_recoverable || !allowRecovery)
            break;
        const QString aside = path + QStringLiteral(".corrupt-")
            + QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss"));
        if (!QFile::rename(path, aside))
            break;
        // Keep the write-ahead log with the copy: it may hold committed transactions.
        QFile::rename(path + QStringLiteral("-wal"), aside + QStringLiteral("-wal"));
        QFile::remove(path + QStringLiteral("-shm"));
        m_issues.append(QStringLiteral("library database was unusable (%1) and was moved to %2; "
                                       "favorites and play history start empty")
                            .arg(m_lastError, aside));
        qCWarning(lcLibrary).noquote() << m_issues.last();
        m_lastError.clear();
    }
    qCCritical(lcLibrary).noquote() << "Cannot open library database" << path << "-" << m_lastError;
    return false;
}

bool GameDatabase::migrate()
{
    QSqlQuery q(QSqlDatabase::database(m_connection, false));
    if (!q.exec(QStringLiteral("PRAGMA user_version")) || !q.next()) {
        m_lastError = q.lastError().text();
        const QString code = q.lastError().nativeErrorCode();
        if (code == QLatin1String("5") || code == QLatin1String("6"))
            m_recoverable = false;
        return false;
    }
    // Reading the version is also our "is this a database at all" probe.
    if (!exec(QStringLiteral("SELECT count(*) FROM sqlite_master")))
        return false;
    const int version = q.value(0).toInt();
    q.finish(); // release the read lock before anything is written or checkpointed
    if (version > SchemaVersion) {
        // Written by a newer Nexora: leave it untouched for that version.
        m_recoverable = false;
        m_lastError = QStringLiteral("database schema %1 is newer than supported (%2); not modified")
                          .arg(version)
                          .arg(SchemaVersion);
        return false;
    }
    if (version == SchemaVersion)
        return true;
    if (version == 0)
        return createSchema();

    // From here on the file is a real Nexora database: whatever goes wrong must not be
    // mistaken for corruption and must not move it aside.
    m_recoverable = false;
    if (version != 1 && version != 2) {
        m_lastError = QStringLiteral("unsupported library database schema %1").arg(version);
        return false;
    }
    if (!backupBeforeMigration(version) || !migrateFrom(version))
        return false;
    if (!m_backupPath.isEmpty()) {
        m_issues.append(QStringLiteral("library database upgraded to schema %1; the previous version was saved as %2")
                            .arg(SchemaVersion)
                            .arg(m_backupPath));
    }
    return true;
}

bool GameDatabase::createSchema()
{
    const QStringList statements{
        QStringLiteral("CREATE TABLE IF NOT EXISTS games ("
                       " id INTEGER PRIMARY KEY,"
                       " path TEXT NOT NULL UNIQUE,"
                       " title TEXT NOT NULL,"
                       " extension TEXT NOT NULL,"
                       " root TEXT NOT NULL,"
                       " size INTEGER NOT NULL DEFAULT 0,"
                       " modified INTEGER,"
                       " emulator_candidates TEXT NOT NULL DEFAULT '',"
                       " emulator_override TEXT NOT NULL DEFAULT '',"
                       " present INTEGER NOT NULL DEFAULT 1,"
                       " favorite INTEGER NOT NULL DEFAULT 0,"
                       " play_count INTEGER NOT NULL DEFAULT 0,"
                       " last_played INTEGER,"
                       " added_at INTEGER NOT NULL,"
                       " system_id TEXT NOT NULL DEFAULT '',"
                       " system_override TEXT NOT NULL DEFAULT '',"
                       " system_candidates TEXT NOT NULL DEFAULT '',"
                       " title_override TEXT NOT NULL DEFAULT '',"
                       " notes TEXT NOT NULL DEFAULT '',"
                       " hidden INTEGER NOT NULL DEFAULT 0,"
                       " play_seconds INTEGER NOT NULL DEFAULT 0,"
                       " pinned INTEGER NOT NULL DEFAULT 0)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS games_present ON games(present)"),
        collectionsTable(),
        collectionGamesTable(),
        QStringLiteral("CREATE TABLE IF NOT EXISTS meta (key TEXT PRIMARY KEY, value TEXT NOT NULL)"),
        QStringLiteral("PRAGMA user_version = %1").arg(SchemaVersion),
    };
    QSqlDatabase db = QSqlDatabase::database(m_connection, false);
    db.transaction();
    for (const QString &sql : statements) {
        if (!exec(sql)) {
            db.rollback();
            return false;
        }
    }
    return db.commit();
}

bool GameDatabase::backupBeforeMigration(int fromVersion)
{
    if (m_path == QStringLiteral(":memory:"))
        return true;
    // Fold the write-ahead log into the main file so the copy is complete.
    if (!exec(QStringLiteral("PRAGMA wal_checkpoint(TRUNCATE)")))
        return false;
    const QString stamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-hhmmss"));
    QString backup = QStringLiteral("%1.v%2-backup-%3").arg(m_path).arg(fromVersion).arg(stamp);
    for (int n = 2; QFile::exists(backup); ++n)
        backup = QStringLiteral("%1.v%2-backup-%3-%4").arg(m_path).arg(fromVersion).arg(stamp).arg(n);
    if (!QFile::copy(m_path, backup)) {
        m_lastError = QStringLiteral("cannot back up the library database to %1 before migrating it").arg(backup);
        qCCritical(lcLibrary).noquote() << m_lastError;
        return false;
    }
    m_backupPath = backup;
    return true;
}

bool GameDatabase::migrateFrom(int version)
{
    // Every step of the chain in one transaction: a failure leaves the old schema intact.
    QStringList statements;
    if (version <= 1) {
        statements << QStringLiteral("ALTER TABLE games ADD COLUMN system_id TEXT NOT NULL DEFAULT ''")
                   << QStringLiteral("ALTER TABLE games ADD COLUMN system_override TEXT NOT NULL DEFAULT ''")
                   << QStringLiteral("ALTER TABLE games ADD COLUMN system_candidates TEXT NOT NULL DEFAULT ''")
                   << QStringLiteral("ALTER TABLE games ADD COLUMN title_override TEXT NOT NULL DEFAULT ''")
                   << QStringLiteral("ALTER TABLE games ADD COLUMN notes TEXT NOT NULL DEFAULT ''")
                   << QStringLiteral("ALTER TABLE games ADD COLUMN hidden INTEGER NOT NULL DEFAULT 0");
    }
    if (version <= 2) {
        statements << QStringLiteral("ALTER TABLE games ADD COLUMN play_seconds INTEGER NOT NULL DEFAULT 0")
                   << QStringLiteral("ALTER TABLE games ADD COLUMN pinned INTEGER NOT NULL DEFAULT 0")
                   << collectionsTable() << collectionGamesTable();
    }
    statements << QStringLiteral("PRAGMA user_version = %1").arg(SchemaVersion);
    QSqlDatabase db = QSqlDatabase::database(m_connection, false);
    if (!db.transaction()) {
        m_lastError = db.lastError().text();
        return false;
    }
    for (const QString &sql : std::as_const(statements)) {
        if (!exec(sql)) {
            db.rollback();
            return false;
        }
    }
    if (!db.commit()) {
        m_lastError = db.lastError().text();
        db.rollback();
        return false;
    }
    return true;
}

std::optional<ApplyStats> GameDatabase::applyScan(const ScanResult &result, const QStringList &configuredRoots)
{
    if (!isOpen())
        return std::nullopt;
    QSqlDatabase db = QSqlDatabase::database(m_connection, false);
    ApplyStats stats;
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    // IMMEDIATE takes the write lock before reading: a deferred transaction could not be
    // upgraded (SQLITE_BUSY_SNAPSHOT) if another connection wrote after our read began.
    if (!exec(QStringLiteral("BEGIN IMMEDIATE")))
        return std::nullopt;

    // One pass over the existing rows instead of a SELECT per scanned file.
    struct Existing
    {
        qint64 id;
        bool present;
        QString title;
        qint64 size;
        QString candidates;
        QString root;
        QVariant modified;
        QString systemId;
        QString systemCandidates;
    };
    QHash<QString, Existing> existing;
    {
        QSqlQuery all(db);
        all.setForwardOnly(true);
        if (!all.exec(
                QStringLiteral("SELECT id, path, present, title, size, emulator_candidates, root, modified, system_id, "
                               "system_candidates FROM games"))) {
            m_lastError = all.lastError().text();
            db.rollback();
            return std::nullopt;
        }
        while (all.next()) {
            existing.insert(all.value(1).toString(),
                            {all.value(0).toLongLong(), all.value(2).toBool(), all.value(3).toString(),
                             all.value(4).toLongLong(), all.value(5).toString(), all.value(6).toString(), all.value(7),
                             all.value(8).toString(), all.value(9).toString()});
        }
    }

    QSqlQuery insert(db);
    insert.prepare(QStringLiteral("INSERT INTO games (path, title, extension, root, size, modified, "
                                  "emulator_candidates, system_id, system_candidates, present, added_at) "
                                  "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, 1, ?)"));
    QSqlQuery update(db);
    update.prepare(QStringLiteral("UPDATE games SET title = ?, extension = ?, root = ?, size = ?, modified = ?, "
                                  "emulator_candidates = ?, system_id = ?, system_candidates = ?, present = 1 "
                                  "WHERE id = ?"));

    QSet<QString> found;
    for (const ScannedFile &f : result.files) {
        found.insert(f.path);
        const QVariant modified = f.modified.isValid() ? QVariant(f.modified.toSecsSinceEpoch()) : QVariant();
        const QString candidates = joinList(f.emulatorIds);
        // Only an unambiguous extension identifies a system; a user override wins over it.
        const QString systemId = f.systemIds.size() == 1 ? f.systemIds.first() : QStringLiteral("");
        const QString systemCandidates = joinList(f.systemIds);
        const auto it = existing.constFind(f.path);
        if (it != existing.cend()) {
            const bool changed = !it->present || it->title != f.title || it->size != f.size
                || it->candidates != candidates || it->root != f.root || it->systemId != systemId
                || it->systemCandidates != systemCandidates || it->modified.toLongLong() != modified.toLongLong();
            if (!changed)
                continue; // nothing to write; keeps rescans of large libraries cheap
            for (const QVariant &v :
                 {QVariant(f.title), QVariant(f.extension), QVariant(f.root), QVariant(f.size), modified,
                  QVariant(candidates), QVariant(systemId), QVariant(systemCandidates), QVariant(it->id)})
                update.addBindValue(v);
            if (!update.exec()) {
                m_lastError = update.lastError().text();
                db.rollback();
                return std::nullopt;
            }
            ++stats.updated;
        } else {
            for (const QVariant &v :
                 {QVariant(f.path), QVariant(f.title), QVariant(f.extension), QVariant(f.root), QVariant(f.size),
                  modified, QVariant(candidates), QVariant(systemId), QVariant(systemCandidates), QVariant(now)})
                insert.addBindValue(v);
            if (!insert.exec()) {
                m_lastError = insert.lastError().text();
                db.rollback();
                return std::nullopt;
            }
            ++stats.added;
        }
    }

    // Anything present that was not found: missing if its root was scanned (file deleted)
    // or its root is no longer configured. Roots that failed to scan (e.g. an unmounted
    // drive) keep their entries as they are.
    // Canonical forms of the configured roots, computed once. For a symlinked root whose
    // target is currently unavailable (e.g. an unmounted drive) the link target is used, so
    // its entries are still recognised as configured and kept.
    QStringList configured;
    for (const QString &r : configuredRoots) {
        const QFileInfo info(r);
        configured.append(QDir::cleanPath(r));
        const QString canonical = info.canonicalFilePath();
        if (!canonical.isEmpty())
            configured.append(canonical);
        else if (info.isSymLink())
            configured.append(QDir::cleanPath(info.symLinkTarget()));
    }
    QSqlQuery present(db);
    present.setForwardOnly(true);
    if (!present.exec(QStringLiteral("SELECT id, path, root FROM games WHERE present = 1"))) {
        m_lastError = present.lastError().text();
        db.rollback();
        return std::nullopt;
    }
    QList<qint64> vanished;
    while (present.next()) {
        const QString path = present.value(1).toString();
        const QString root = present.value(2).toString();
        if (found.contains(path))
            continue;
        const bool rootScanned = std::any_of(result.scannedRoots.cbegin(), result.scannedRoots.cend(),
                                             [&](const QString &r) { return isUnder(path, r); });
        const bool rootConfigured
            = std::any_of(configured.cbegin(), configured.cend(), [&](const QString &r) { return isUnder(root, r); });
        // A truncated or cancelled scan did not see every file: only drop entries whose
        // directory was removed from the configuration in that case.
        const bool complete = !result.truncated && !result.cancelled;
        if (!rootConfigured || (rootScanned && complete))
            vanished.append(present.value(0).toLongLong());
    }
    present.finish();
    QSqlQuery mark(db);
    mark.prepare(QStringLiteral("UPDATE games SET present = 0 WHERE id = ?"));
    for (qint64 id : std::as_const(vanished)) {
        mark.addBindValue(id);
        if (!mark.exec()) {
            m_lastError = mark.lastError().text();
            db.rollback();
            return std::nullopt;
        }
    }
    stats.missing = int(vanished.size());

    if (!db.commit()) {
        m_lastError = db.lastError().text();
        return std::nullopt;
    }
    qCInfo(lcLibrary) << "Library update:" << stats.added << "added," << stats.updated << "updated," << stats.missing
                      << "missing";
    return stats;
}

namespace {

GameEntry entryFromQuery(const QSqlQuery &q)
{
    GameEntry e;
    e.id = q.value(0).toLongLong();
    e.path = q.value(1).toString();
    e.title = q.value(2).toString();
    e.extension = q.value(3).toString();
    e.root = q.value(4).toString();
    e.size = q.value(5).toLongLong();
    e.emulatorCandidates = splitList(q.value(6).toString());
    e.emulatorOverride = q.value(7).toString();
    e.present = q.value(8).toBool();
    e.favorite = q.value(9).toBool();
    e.playCount = q.value(10).toInt();
    if (!q.value(11).isNull())
        e.lastPlayed = QDateTime::fromSecsSinceEpoch(q.value(11).toLongLong(), Qt::UTC);
    e.addedAt = QDateTime::fromSecsSinceEpoch(q.value(12).toLongLong(), Qt::UTC);
    e.systemId = q.value(13).toString();
    e.systemCandidates = splitList(q.value(14).toString());
    e.titleOverride = q.value(15).toString();
    e.notes = q.value(16).toString();
    e.hidden = q.value(17).toBool();
    e.playSeconds = q.value(18).toLongLong();
    e.pinned = q.value(19).toInt();
    return e;
}

const QString selectColumns
    = QStringLiteral("SELECT id, path, title, extension, root, size, emulator_candidates, emulator_override, present, "
                     "favorite, play_count, last_played, added_at, "
                     "CASE WHEN system_override <> '' THEN system_override ELSE system_id END, "
                     "system_candidates, title_override, notes, hidden, play_seconds, pinned FROM games");

} // namespace

QList<GameEntry> GameDatabase::entries(bool includeMissing, bool includeHidden) const
{
    QList<GameEntry> list;
    if (!isOpen())
        return list;
    QStringList conditions;
    if (!includeMissing)
        conditions.append(QStringLiteral("present = 1"));
    if (!includeHidden)
        conditions.append(QStringLiteral("hidden = 0"));
    QSqlQuery q(QSqlDatabase::database(m_connection, false));
    q.exec(selectColumns
           + (conditions.isEmpty() ? QString() : QStringLiteral(" WHERE ") + conditions.join(QStringLiteral(" AND ")))
           + QStringLiteral(" ORDER BY id"));
    while (q.next())
        list.append(entryFromQuery(q));
    return list;
}

std::optional<GameEntry> GameDatabase::entry(qint64 id) const
{
    if (!isOpen())
        return std::nullopt;
    QSqlQuery q(QSqlDatabase::database(m_connection, false));
    q.prepare(selectColumns + QStringLiteral(" WHERE id = ?"));
    q.addBindValue(id);
    if (!q.exec() || !q.next())
        return std::nullopt;
    return entryFromQuery(q);
}

bool GameDatabase::setFavorite(qint64 id, bool favorite)
{
    QSqlQuery q(QSqlDatabase::database(m_connection, false));
    q.prepare(QStringLiteral("UPDATE games SET favorite = ? WHERE id = ?"));
    q.addBindValue(favorite ? 1 : 0);
    q.addBindValue(id);
    return q.exec() && q.numRowsAffected() == 1;
}

bool GameDatabase::recordLaunch(qint64 id, const QDateTime &when)
{
    QSqlQuery q(QSqlDatabase::database(m_connection, false));
    q.prepare(QStringLiteral("UPDATE games SET play_count = play_count + 1, last_played = ? WHERE id = ?"));
    q.addBindValue(when.toSecsSinceEpoch());
    q.addBindValue(id);
    return q.exec() && q.numRowsAffected() == 1;
}

bool GameDatabase::setEmulatorOverride(qint64 id, const QString &emulatorId)
{
    QSqlQuery q(QSqlDatabase::database(m_connection, false));
    q.prepare(QStringLiteral("UPDATE games SET emulator_override = ? WHERE id = ?"));
    // A null QString binds as SQL NULL, which the NOT NULL column rejects; "" = automatic.
    q.addBindValue(emulatorId.isEmpty() ? QStringLiteral("") : emulatorId);
    q.addBindValue(id);
    return q.exec() && q.numRowsAffected() == 1;
}

bool GameDatabase::setMetadata(qint64 id, const QString &titleOverride, const QString &notes)
{
    const QString title = titleOverride.trimmed();
    if (title.size() > MaxTitleLength || notes.size() > MaxNotesLength || hasControlCharacters(title, false)
        || hasControlCharacters(notes, true)) {
        m_lastError = QStringLiteral("title (at most %1 characters) or notes (at most %2) are too long or contain "
                                     "control characters")
                          .arg(MaxTitleLength)
                          .arg(MaxNotesLength);
        return false;
    }
    QSqlQuery q(QSqlDatabase::database(m_connection, false));
    q.prepare(QStringLiteral("UPDATE games SET title_override = ?, notes = ? WHERE id = ?"));
    q.addBindValue(nonNull(title));
    q.addBindValue(nonNull(notes));
    q.addBindValue(id);
    return q.exec() && q.numRowsAffected() == 1;
}

bool GameDatabase::setSystemOverride(qint64 id, const QString &systemId)
{
    if (!systemId.isEmpty() && !shared::SystemCatalog::isValidId(systemId)) {
        m_lastError = QStringLiteral("invalid system id");
        return false;
    }
    QSqlQuery q(QSqlDatabase::database(m_connection, false));
    q.prepare(QStringLiteral("UPDATE games SET system_override = ? WHERE id = ?"));
    q.addBindValue(nonNull(systemId));
    q.addBindValue(id);
    return q.exec() && q.numRowsAffected() == 1;
}

bool GameDatabase::setHidden(qint64 id, bool hidden)
{
    QSqlQuery q(QSqlDatabase::database(m_connection, false));
    q.prepare(QStringLiteral("UPDATE games SET hidden = ? WHERE id = ?"));
    q.addBindValue(hidden ? 1 : 0);
    q.addBindValue(id);
    return q.exec() && q.numRowsAffected() == 1;
}

int GameDatabase::restoreHidden()
{
    QSqlQuery q(QSqlDatabase::database(m_connection, false));
    if (!q.exec(QStringLiteral("UPDATE games SET hidden = 0 WHERE hidden = 1")))
        return 0;
    return q.numRowsAffected();
}

int GameDatabase::purgeMissing()
{
    QSqlDatabase db = QSqlDatabase::database(m_connection, false);
    if (!db.transaction())
        return 0;
    QSqlQuery q(db);
    if (!q.exec(QStringLiteral("DELETE FROM collection_games WHERE game_id IN "
                               "(SELECT id FROM games WHERE present = 0 AND hidden = 0)"))
        || !q.exec(QStringLiteral("DELETE FROM games WHERE present = 0 AND hidden = 0"))) {
        db.rollback();
        return 0;
    }
    const int removed = q.numRowsAffected();
    return db.commit() ? removed : 0;
}

bool GameDatabase::addPlayTime(qint64 id, qint64 seconds)
{
    if (seconds <= 0)
        return false;
    QSqlQuery q(QSqlDatabase::database(m_connection, false));
    q.prepare(QStringLiteral("UPDATE games SET play_seconds = play_seconds + ? WHERE id = ?"));
    q.addBindValue(seconds);
    q.addBindValue(id);
    return q.exec() && q.numRowsAffected() == 1;
}

bool GameDatabase::setPinned(qint64 id, bool pinned)
{
    QSqlQuery q(QSqlDatabase::database(m_connection, false));
    if (!pinned) {
        q.prepare(QStringLiteral("UPDATE games SET pinned = 0 WHERE id = ?"));
        q.addBindValue(id);
        return q.exec() && q.numRowsAffected() == 1;
    }
    const auto current = entry(id);
    if (!current) {
        m_lastError = QStringLiteral("no such game");
        return false;
    }
    if (current->pinned > 0)
        return true;
    if (!q.exec(QStringLiteral("SELECT count(*), coalesce(max(pinned), 0) FROM games WHERE pinned > 0")) || !q.next())
        return false;
    if (q.value(0).toInt() >= MaxPinned) {
        m_lastError = QStringLiteral("at most %1 games can be pinned").arg(MaxPinned);
        return false;
    }
    const int next = q.value(1).toInt() + 1;
    q.finish();
    q.prepare(QStringLiteral("UPDATE games SET pinned = ? WHERE id = ?"));
    q.addBindValue(next);
    q.addBindValue(id);
    return q.exec() && q.numRowsAffected() == 1;
}

QList<GameDatabase::Collection> GameDatabase::collections() const
{
    QList<Collection> list;
    if (!isOpen())
        return list;
    QSqlQuery q(QSqlDatabase::database(m_connection, false));
    q.exec(QStringLiteral("SELECT c.id, c.name, c.created_at, count(g.id) FROM collections c "
                          "LEFT JOIN collection_games cg ON cg.collection_id = c.id "
                          "LEFT JOIN games g ON g.id = cg.game_id AND g.present = 1 AND g.hidden = 0 "
                          "GROUP BY c.id ORDER BY c.name COLLATE NOCASE"));
    while (q.next()) {
        list.append({q.value(0).toLongLong(), q.value(1).toString(),
                     QDateTime::fromSecsSinceEpoch(q.value(2).toLongLong(), Qt::UTC), q.value(3).toInt()});
    }
    return list;
}

bool GameDatabase::checkCollectionName(const QString &name, qint64 exceptId)
{
    if (name.isEmpty() || name.size() > MaxCollectionNameLength || hasControlCharacters(name, false)) {
        m_lastError = QStringLiteral("collection names have 1-%1 characters and no control characters")
                          .arg(MaxCollectionNameLength);
        return false;
    }
    // SQLite's NOCASE folds ASCII only, so names are compared here with Unicode case folding
    // ("Á" and "á" are the same name); there are at most MaxCollections rows.
    QSqlQuery q(QSqlDatabase::database(m_connection, false));
    q.prepare(QStringLiteral("SELECT name FROM collections WHERE id <> ?"));
    q.addBindValue(exceptId);
    if (!q.exec()) {
        m_lastError = q.lastError().text();
        return false;
    }
    while (q.next()) {
        const QString existing = q.value(0).toString().normalized(QString::NormalizationForm_C);
        if (existing.compare(name, Qt::CaseInsensitive) == 0) {
            m_lastError = QStringLiteral("a collection named \"%1\" already exists").arg(name);
            return false;
        }
    }
    return true;
}

std::optional<qint64> GameDatabase::createCollection(const QString &rawName)
{
    const QString name = rawName.trimmed().normalized(QString::NormalizationForm_C);
    if (!checkCollectionName(name, 0))
        return std::nullopt;
    QSqlQuery q(QSqlDatabase::database(m_connection, false));
    if (q.exec(QStringLiteral("SELECT count(*) FROM collections")) && q.next()
        && q.value(0).toInt() >= MaxCollections) {
        m_lastError = QStringLiteral("at most %1 collections").arg(MaxCollections);
        return std::nullopt;
    }
    q.finish();
    q.prepare(QStringLiteral("INSERT INTO collections (name, created_at) VALUES (?, ?)"));
    q.addBindValue(name);
    q.addBindValue(QDateTime::currentSecsSinceEpoch());
    if (!q.exec()) {
        m_lastError = q.lastError().text();
        return std::nullopt;
    }
    return q.lastInsertId().toLongLong();
}

bool GameDatabase::renameCollection(qint64 id, const QString &rawName)
{
    const QString name = rawName.trimmed().normalized(QString::NormalizationForm_C);
    if (!checkCollectionName(name, id))
        return false;
    QSqlQuery q(QSqlDatabase::database(m_connection, false));
    q.prepare(QStringLiteral("UPDATE collections SET name = ? WHERE id = ?"));
    q.addBindValue(name);
    q.addBindValue(id);
    if (!q.exec() || q.numRowsAffected() != 1) {
        m_lastError = QStringLiteral("no such collection");
        return false;
    }
    return true;
}

bool GameDatabase::deleteCollection(qint64 id)
{
    QSqlDatabase db = QSqlDatabase::database(m_connection, false);
    if (!db.transaction())
        return false;
    QSqlQuery q(db);
    q.prepare(QStringLiteral("DELETE FROM collection_games WHERE collection_id = ?"));
    q.addBindValue(id);
    bool ok = q.exec();
    q.prepare(QStringLiteral("DELETE FROM collections WHERE id = ?"));
    q.addBindValue(id);
    ok = ok && q.exec() && q.numRowsAffected() == 1;
    if (!ok) {
        db.rollback();
        m_lastError = QStringLiteral("no such collection");
        return false;
    }
    return db.commit();
}

bool GameDatabase::setInCollection(qint64 collectionId, qint64 gameId, bool member)
{
    QSqlQuery q(QSqlDatabase::database(m_connection, false));
    if (!member) {
        q.prepare(QStringLiteral("DELETE FROM collection_games WHERE collection_id = ? AND game_id = ?"));
        q.addBindValue(collectionId);
        q.addBindValue(gameId);
        return q.exec();
    }
    q.prepare(QStringLiteral("SELECT (SELECT count(*) FROM collections WHERE id = ?), "
                             "(SELECT count(*) FROM games WHERE id = ?), "
                             "(SELECT count(*) FROM collection_games WHERE collection_id = ?), "
                             "(SELECT coalesce(max(position), 0) FROM collection_games WHERE collection_id = ?)"));
    q.addBindValue(collectionId);
    q.addBindValue(gameId);
    q.addBindValue(collectionId);
    q.addBindValue(collectionId);
    if (!q.exec() || !q.next() || q.value(0).toInt() != 1 || q.value(1).toInt() != 1) {
        m_lastError = QStringLiteral("no such collection or game");
        return false;
    }
    if (q.value(2).toInt() >= MaxGamesPerCollection) {
        m_lastError = QStringLiteral("a collection holds at most %1 games").arg(MaxGamesPerCollection);
        return false;
    }
    const int position = q.value(3).toInt() + 1;
    q.finish();
    q.prepare(QStringLiteral("INSERT OR IGNORE INTO collection_games (collection_id, game_id, position, added_at) "
                             "VALUES (?, ?, ?, ?)"));
    q.addBindValue(collectionId);
    q.addBindValue(gameId);
    q.addBindValue(position);
    q.addBindValue(QDateTime::currentSecsSinceEpoch());
    return q.exec();
}

QList<qint64> GameDatabase::collectionGameIds(qint64 collectionId) const
{
    QList<qint64> ids;
    QSqlQuery q(QSqlDatabase::database(m_connection, false));
    q.prepare(QStringLiteral("SELECT game_id FROM collection_games WHERE collection_id = ? ORDER BY position"));
    q.addBindValue(collectionId);
    if (q.exec()) {
        while (q.next())
            ids.append(q.value(0).toLongLong());
    }
    return ids;
}

QList<qint64> GameDatabase::collectionsOf(qint64 gameId) const
{
    QList<qint64> ids;
    QSqlQuery q(QSqlDatabase::database(m_connection, false));
    q.prepare(QStringLiteral("SELECT collection_id FROM collection_games WHERE game_id = ? ORDER BY collection_id"));
    q.addBindValue(gameId);
    if (q.exec()) {
        while (q.next())
            ids.append(q.value(0).toLongLong());
    }
    return ids;
}

int GameDatabase::missingCount() const
{
    if (!isOpen())
        return 0;
    QSqlQuery q(QSqlDatabase::database(m_connection, false));
    return q.exec(QStringLiteral("SELECT count(*) FROM games WHERE present = 0")) && q.next() ? q.value(0).toInt() : 0;
}

QString GameDatabase::meta(const QString &key) const
{
    QSqlQuery q(QSqlDatabase::database(m_connection, false));
    q.prepare(QStringLiteral("SELECT value FROM meta WHERE key = ?"));
    q.addBindValue(key);
    return q.exec() && q.next() ? q.value(0).toString() : QString();
}

bool GameDatabase::setMeta(const QString &key, const QString &value)
{
    QSqlQuery q(QSqlDatabase::database(m_connection, false));
    q.prepare(QStringLiteral("INSERT INTO meta (key, value) VALUES (?, ?) "
                             "ON CONFLICT(key) DO UPDATE SET value = excluded.value"));
    q.addBindValue(key);
    q.addBindValue(value);
    return q.exec();
}

bool GameDatabase::backupTo(const QString &destFile, QString *error) const
{
    if (!isOpen()) {
        if (error)
            *error = QStringLiteral("database is not open");
        return false;
    }
    QFile::remove(destFile); // VACUUM INTO refuses to overwrite an existing file
    QSqlQuery q(QSqlDatabase::database(m_connection, false));
    // VACUUM INTO takes a string literal, not a bind parameter; the destination is a path
    // this process computed itself (never user/metadata-controlled), escaped defensively.
    QString escaped = destFile;
    escaped.replace(QLatin1Char('\''), QStringLiteral("''"));
    if (!q.exec(QStringLiteral("VACUUM INTO '%1'").arg(escaped))) {
        if (error)
            *error = q.lastError().text();
        return false;
    }
    return true;
}

} // namespace nexora::library
