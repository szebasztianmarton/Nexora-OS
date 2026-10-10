#include "nexora/library/GameDatabase.h"

#include "nexora/shared/Logging.h"

#include <QAtomicInteger>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace nexora::library {

namespace {

QAtomicInteger<int> connectionCounter;

QString joinList(const QStringList &list)
{
    return list.join(u',');
}

QStringList splitList(const QString &text)
{
    return text.isEmpty() ? QStringList{} : text.split(u',', Qt::SkipEmptyParts);
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
        qCWarning(lcLibrary).noquote() << "SQL failed:" << sql.left(80) << "-" << m_lastError;
        return false;
    }
    return true;
}

bool GameDatabase::open(const QString &path)
{
    close();
    m_issues.clear();
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
        if (inMemory || attempt == 1 || !QFile::exists(path))
            break;
        const QString aside = path + QStringLiteral(".corrupt-")
            + QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss"));
        if (!QFile::rename(path, aside))
            break;
        QFile::remove(path + QStringLiteral("-wal"));
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
        return false;
    }
    // Reading the version is also our "is this a database at all" probe.
    if (!exec(QStringLiteral("SELECT count(*) FROM sqlite_master")))
        return false;
    const int version = q.value(0).toInt();
    if (version > SchemaVersion) {
        m_lastError = QStringLiteral("database schema %1 is newer than supported (%2)").arg(version).arg(SchemaVersion);
        return false;
    }
    if (version == SchemaVersion)
        return true;

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
                       " added_at INTEGER NOT NULL)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS games_present ON games(present)"),
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

std::optional<ApplyStats> GameDatabase::applyScan(const ScanResult &result, const QStringList &configuredRoots)
{
    if (!isOpen())
        return std::nullopt;
    QSqlDatabase db = QSqlDatabase::database(m_connection, false);
    ApplyStats stats;
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    db.transaction();

    QSqlQuery find(db);
    find.prepare(QStringLiteral("SELECT id, present, title, size, emulator_candidates FROM games WHERE path = ?"));
    QSqlQuery insert(db);
    insert.prepare(QStringLiteral("INSERT INTO games (path, title, extension, root, size, modified, "
                                  "emulator_candidates, present, added_at) VALUES (?, ?, ?, ?, ?, ?, ?, 1, ?)"));
    QSqlQuery update(db);
    update.prepare(QStringLiteral("UPDATE games SET title = ?, extension = ?, root = ?, size = ?, modified = ?, "
                                  "emulator_candidates = ?, present = 1 WHERE id = ?"));

    QSet<QString> found;
    for (const ScannedFile &f : result.files) {
        found.insert(f.path);
        const QVariant modified = f.modified.isValid() ? QVariant(f.modified.toSecsSinceEpoch()) : QVariant();
        find.addBindValue(f.path);
        if (!find.exec()) {
            m_lastError = find.lastError().text();
            db.rollback();
            return std::nullopt;
        }
        if (find.next()) {
            const qint64 id = find.value(0).toLongLong();
            const bool changed = !find.value(1).toBool() || find.value(2).toString() != f.title
                || find.value(3).toLongLong() != f.size || find.value(4).toString() != joinList(f.emulatorIds);
            find.finish();
            for (const QVariant &v : {QVariant(f.title), QVariant(f.extension), QVariant(f.root), QVariant(f.size),
                                      modified, QVariant(joinList(f.emulatorIds)), QVariant(id)})
                update.addBindValue(v);
            if (!update.exec()) {
                m_lastError = update.lastError().text();
                db.rollback();
                return std::nullopt;
            }
            if (changed)
                ++stats.updated;
        } else {
            find.finish();
            for (const QVariant &v : {QVariant(f.path), QVariant(f.title), QVariant(f.extension), QVariant(f.root),
                                      QVariant(f.size), modified, QVariant(joinList(f.emulatorIds)), QVariant(now)})
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
    QSqlQuery present(db);
    present.exec(QStringLiteral("SELECT id, path, root FROM games WHERE present = 1"));
    QList<qint64> vanished;
    while (present.next()) {
        const QString path = present.value(1).toString();
        const QString root = present.value(2).toString();
        if (found.contains(path))
            continue;
        const bool rootScanned = std::any_of(result.scannedRoots.cbegin(), result.scannedRoots.cend(),
                                             [&](const QString &r) { return isUnder(path, r); });
        const bool rootConfigured
            = std::any_of(configuredRoots.cbegin(), configuredRoots.cend(), [&](const QString &r) {
                  const QString c = QFileInfo(r).canonicalFilePath();
                  return isUnder(root, c.isEmpty() ? QDir::cleanPath(r) : c);
              });
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
        mark.exec();
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
    return e;
}

const QString selectColumns
    = QStringLiteral("SELECT id, path, title, extension, root, size, emulator_candidates, emulator_override, present, "
                     "favorite, play_count, last_played, added_at FROM games");

} // namespace

QList<GameEntry> GameDatabase::entries(bool includeMissing) const
{
    QList<GameEntry> list;
    if (!isOpen())
        return list;
    QSqlQuery q(QSqlDatabase::database(m_connection, false));
    q.exec(selectColumns + (includeMissing ? QString() : QStringLiteral(" WHERE present = 1"))
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
    q.addBindValue(emulatorId);
    q.addBindValue(id);
    return q.exec() && q.numRowsAffected() == 1;
}

int GameDatabase::purgeMissing()
{
    QSqlQuery q(QSqlDatabase::database(m_connection, false));
    if (!q.exec(QStringLiteral("DELETE FROM games WHERE present = 0")))
        return 0;
    return q.numRowsAffected();
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

} // namespace nexora::library
