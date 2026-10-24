// Library database schema migration v1 -> v2 against real SQLite files.
#include "nexora/library/GameDatabase.h"

#include <QDir>
#include <QFile>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTest>

using namespace nexora::library;

namespace {

// The schema as shipped with schema version 1, copied verbatim.
const QStringList v1Schema{
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
    QStringLiteral("PRAGMA user_version = 1"),
    QStringLiteral(
        "INSERT INTO games (path, title, extension, root, size, emulator_candidates, favorite, play_count, "
        "last_played, added_at) VALUES ('/g/Alpha.sfc', 'Alpha', '.sfc', '/g', 10, 'snes', 1, 3, 1700000000, "
        "1690000000)"),
    QStringLiteral("INSERT INTO games (path, title, extension, root, size, emulator_candidates, added_at) "
                   "VALUES ('/g/Beta.nes', 'Beta', '.nes', '/g', 20, 'nes,multi', 1690000001)"),
    QStringLiteral("INSERT INTO meta (key, value) VALUES ('scan-signature', 'abc')"),
};

QByteArray readAll(const QString &path)
{
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}

} // namespace

class TestLibraryMigration : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_dir;

    QString db() const { return m_dir.filePath(QStringLiteral("library.sqlite")); }

    // Runs |statements| against |path| and returns the first column of the last SELECT, if any.
    static QVariant sql(const QString &path, const QStringList &statements)
    {
        const QString name = QStringLiteral("raw-%1").arg(path);
        QVariant result;
        {
            QSqlDatabase raw = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), name);
            raw.setDatabaseName(path);
            if (!raw.open())
                return result;
            for (const QString &s : statements) {
                QSqlQuery q(raw);
                if (!q.exec(s))
                    qWarning() << s << q.lastError().text();
                else if (q.isSelect() && q.next())
                    result = q.value(0);
            }
            raw.close();
        }
        QSqlDatabase::removeDatabase(name);
        return result;
    }

    void createV1(const QStringList &extra = {}) { sql(db(), v1Schema + extra); }

    QStringList backups() const
    {
        return QDir(m_dir.path()).entryList({QStringLiteral("library.sqlite.v1-backup-*")}, QDir::Files);
    }

private slots:
    void init()
    {
        QDir(m_dir.path()).removeRecursively();
        QDir().mkpath(m_dir.path());
    }

    void migratesV1KeepingUserData()
    {
        createV1();
        GameDatabase database;
        QVERIFY2(database.open(db()), qPrintable(database.lastError()));
        QCOMPARE(sql(db(), {QStringLiteral("PRAGMA user_version")}).toInt(), GameDatabase::SchemaVersion);
        QCOMPARE(GameDatabase::SchemaVersion, 2);

        const auto list = database.entries();
        QCOMPARE(list.size(), 2);
        const auto alpha = list.at(0).title == QStringLiteral("Alpha") ? list.at(0) : list.at(1);
        QVERIFY(alpha.favorite);
        QCOMPARE(alpha.playCount, 3);
        QCOMPARE(alpha.lastPlayed.toSecsSinceEpoch(), qint64(1700000000));
        QCOMPARE(alpha.size, qint64(10));
        QCOMPARE(alpha.emulatorCandidates, QStringList{QStringLiteral("snes")});
        QVERIFY(!alpha.hidden);
        QVERIFY(alpha.systemId.isEmpty());
        QVERIFY(alpha.titleOverride.isEmpty());
        QVERIFY(alpha.notes.isEmpty());
        QCOMPARE(alpha.displayTitle(), QStringLiteral("Alpha"));
        QCOMPARE(database.meta(QStringLiteral("scan-signature")), QStringLiteral("abc"));

        // The new columns are usable.
        QVERIFY(database.setMetadata(alpha.id, QStringLiteral("Alpha Edition"), QStringLiteral("note")));
        QCOMPARE(database.entry(alpha.id)->displayTitle(), QStringLiteral("Alpha Edition"));
    }

    void writesAReadableV1Backup()
    {
        createV1();
        GameDatabase database;
        QVERIFY(database.open(db()));
        QCOMPARE(backups().size(), 1);
        QCOMPARE(database.lastBackupPath(), m_dir.filePath(backups().first()));
        QVERIFY(database.issues().size() == 1);

        QCOMPARE(sql(database.lastBackupPath(), {QStringLiteral("PRAGMA user_version")}).toInt(), 1);
        QCOMPARE(sql(database.lastBackupPath(), {QStringLiteral("SELECT count(*) FROM games")}).toInt(), 2);
        // The backup really is v1: it has no hidden column.
        QSqlDatabase raw = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("backup-check"));
        raw.setDatabaseName(database.lastBackupPath());
        QVERIFY(raw.open());
        QVERIFY(!QSqlQuery(raw).exec(QStringLiteral("SELECT hidden FROM games")));
        raw.close();
        raw = QSqlDatabase();
        QSqlDatabase::removeDatabase(QStringLiteral("backup-check"));
    }

    void backupIncludesPendingWalContent()
    {
        // A v1 database in WAL mode whose latest row is still in the -wal file.
        QSqlDatabase raw = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("wal"));
        raw.setDatabaseName(db());
        QVERIFY(raw.open());
        QSqlQuery(raw).exec(QStringLiteral("PRAGMA journal_mode = WAL"));
        QSqlQuery(raw).exec(QStringLiteral("PRAGMA wal_autocheckpoint = 0"));
        for (const QString &s : v1Schema)
            QVERIFY(QSqlQuery(raw).exec(s));
        QVERIFY(QSqlQuery(raw).exec(QStringLiteral("INSERT INTO games (path, title, extension, root, added_at) "
                                                   "VALUES ('/g/Wal.sfc', 'Wal', '.sfc', '/g', 5)")));
        {
            GameDatabase database; // opened while the raw connection (and its WAL) is alive
            QVERIFY2(database.open(db()), qPrintable(database.lastError()));
            QCOMPARE(database.entries().size(), 3);
            QCOMPARE(sql(database.lastBackupPath(), {QStringLiteral("SELECT count(*) FROM games")}).toInt(), 3);
        }
        raw.close();
        raw = QSqlDatabase();
        QSqlDatabase::removeDatabase(QStringLiteral("wal"));
    }

    void migratingTwiceIsANoOp()
    {
        createV1();
        {
            GameDatabase first;
            QVERIFY(first.open(db()));
            QCOMPARE(backups().size(), 1);
        }
        GameDatabase second;
        QVERIFY(second.open(db()));
        QVERIFY(second.lastBackupPath().isEmpty());
        QVERIFY(second.issues().isEmpty());
        QCOMPARE(backups().size(), 1);
        QCOMPARE(second.entries().size(), 2);
    }

    void freshDatabaseNeedsNoBackup()
    {
        GameDatabase database;
        QVERIFY(database.open(db()));
        QVERIFY(database.lastBackupPath().isEmpty());
        QCOMPARE(sql(db(), {QStringLiteral("PRAGMA user_version")}).toInt(), 2);
        QVERIFY(backups().isEmpty());
    }

    void refusesNewerSchemaAndLeavesItByteIdentical()
    {
        createV1({QStringLiteral("PRAGMA user_version = 99")});
        const QByteArray before = readAll(db());
        QVERIFY(!before.isEmpty());
        {
            GameDatabase database;
            QVERIFY(!database.open(db()));
            QVERIFY(database.lastError().contains(QStringLiteral("newer")));
        }
        QCOMPARE(readAll(db()), before);
        QVERIFY(backups().isEmpty());
        QCOMPARE(QDir(m_dir.path()).entryList({QStringLiteral("*.corrupt-*")}).size(), 0);
    }

    void failedMigrationKeepsTheOriginalAndTheBackup()
    {
        // A "v1" database that already has one of the v2 columns: the ALTER fails halfway.
        createV1({QStringLiteral("ALTER TABLE games ADD COLUMN notes TEXT NOT NULL DEFAULT ''")});
        GameDatabase database;
        QVERIFY(!database.open(db()));
        QVERIFY(!database.lastError().isEmpty());

        QVERIFY(QFile::exists(db()));
        QCOMPARE(QDir(m_dir.path()).entryList({QStringLiteral("*.corrupt-*")}).size(), 0); // not "corruption"
        QCOMPARE(backups().size(), 1);
        QCOMPARE(sql(db(), {QStringLiteral("PRAGMA user_version")}).toInt(), 1); // rolled back
        QCOMPARE(sql(db(), {QStringLiteral("SELECT count(*) FROM games")}).toInt(), 2);
        // None of the columns the failed transaction added is left behind.
        QVERIFY(
            sql(db(), {QStringLiteral("SELECT count(*) FROM pragma_table_info('games') WHERE name = 'hidden'")}).toInt()
            == 0);
    }

    void failedOpenDoesNotReportAStaleError()
    {
        createV1({QStringLiteral("PRAGMA user_version = 99")});
        GameDatabase database;
        QVERIFY(!database.open(db()));
        QVERIFY(database.lastError().contains(QStringLiteral("newer")));
        // A directory cannot be opened as a database: that is the error to report now.
        const QString directory = m_dir.filePath(QStringLiteral("not-a-file"));
        QVERIFY(QDir().mkpath(directory));
        QVERIFY(!database.open(directory, false));
        QVERIFY(!database.lastError().isEmpty());
        QVERIFY2(!database.lastError().contains(QStringLiteral("newer")), qPrintable(database.lastError()));
    }

    void failedCommitLeavesTheConnectionUsable()
    {
        {
            GameDatabase database;
            QVERIFY(database.open(db()));
        }
        // A deferred foreign key is only checked at COMMIT: inserting "Bravo" makes the
        // commit itself fail.
        sql(db(),
            {QStringLiteral("CREATE TABLE trap (ref INTEGER REFERENCES games(id) DEFERRABLE INITIALLY DEFERRED)"),
             QStringLiteral("CREATE TRIGGER trap_bravo AFTER INSERT ON games WHEN NEW.title = 'Bravo' "
                            "BEGIN INSERT INTO trap (ref) VALUES (-1); END")});
        GameDatabase database;
        QVERIFY(database.open(db()));
        const auto file = [](const QString &title) {
            ScannedFile f;
            f.path = QStringLiteral("/g/%1.sfc").arg(title);
            f.title = title;
            f.extension = QStringLiteral(".sfc");
            f.root = QStringLiteral("/g");
            return f;
        };
        ScanResult failing;
        failing.files = {file(QStringLiteral("Alpha")), file(QStringLiteral("Bravo"))};
        QVERIFY(!database.applyScan(failing, {QStringLiteral("/g")}));
        QVERIFY(database.entries().isEmpty()); // nothing of the failed merge was kept

        ScanResult ok;
        ok.files = {file(QStringLiteral("Alpha"))};
        QVERIFY2(database.applyScan(ok, {QStringLiteral("/g")}), qPrintable(database.lastError()));
        QCOMPARE(database.entries().size(), 1);
        QVERIFY2(database.setFavorite(database.entries().first().id, true), qPrintable(database.lastError()));
    }

    void metadataValidation()
    {
        GameDatabase database;
        QVERIFY(database.open(db()));
        ScanResult scan;
        ScannedFile f;
        f.path = QStringLiteral("/g/A.sfc");
        f.title = QStringLiteral("A");
        f.extension = QStringLiteral(".sfc");
        f.root = QStringLiteral("/g");
        scan.files.append(f);
        QVERIFY2(database.applyScan(scan, {QStringLiteral("/g")}), qPrintable(database.lastError()));
        const qint64 id = database.entries().first().id;

        QVERIFY(database.setMetadata(id, QStringLiteral("Title"), QStringLiteral("line1\nline2")));
        QVERIFY(!database.setMetadata(id, QString(201, u'x'), QString()));
        QVERIFY(database.setMetadata(id, QString(200, u'x'), QString(2000, u'n')));
        QVERIFY(!database.setMetadata(id, QStringLiteral("ok"), QString(2001, u'n')));
        QVERIFY(!database.setMetadata(id, QStringLiteral("bad\ntitle"), QString()));
        QVERIFY(!database.setMetadata(id, QStringLiteral("ok"), QStringLiteral("bad\ttab")));
        QVERIFY(!database.setMetadata(id, QStringLiteral("ok"), QStringLiteral("bad\x01")));
        QVERIFY(!database.setMetadata(9999, QStringLiteral("ok"), QString())); // unknown id
        QCOMPARE(database.entry(id)->titleOverride.size(), 200); // rejected calls changed nothing
        QVERIFY(database.setMetadata(id, QString(), QString())); // clears
        QCOMPARE(database.entry(id)->displayTitle(), QStringLiteral("A"));
    }
};

QTEST_GUILESS_MAIN(TestLibraryMigration)
#include "tst_librarymigration.moc"
