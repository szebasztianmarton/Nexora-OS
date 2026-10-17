// GameLibrary end to end: asynchronous scans, SQLite persistence, user data across rescans
// and restarts, missing files, corrupted database recovery.
#include "nexora/library/GameLibrary.h"
#include "nexora/shared/SystemCatalog.h"

#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QSignalSpy>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>

using namespace nexora::library;

class TestGameLibrary : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_dir;

    static void touch(const QString &path)
    {
        QDir().mkpath(QFileInfo(path).absolutePath());
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("rom");
    }

    QString games() const { return m_dir.filePath(QStringLiteral("games")); }
    QString db() const { return m_dir.filePath(QStringLiteral("data/library.sqlite")); }

    ScanOptions options() const
    {
        ScanOptions o;
        o.roots = {games()};
        o.extensionMap = {{QStringLiteral(".sfc"), {QStringLiteral("snes")}}};
        return o;
    }

    static void scan(GameLibrary &library)
    {
        QSignalSpy done(&library, &GameLibrary::scanFinished);
        QVERIFY(library.refresh());
        QVERIFY(library.isScanning());
        QVERIFY(done.wait(10000));
        QVERIFY(!library.isScanning());
    }

    static std::optional<GameEntry> byTitle(const GameLibrary &library, const QString &title)
    {
        for (const auto &e : library.entries()) {
            if (e.title == title)
                return e;
        }
        return std::nullopt;
    }

private slots:
    void init()
    {
        QDir(m_dir.path()).removeRecursively();
        QDir().mkpath(games());
    }

    void scansAsynchronouslyAndPersists()
    {
        touch(games() + QStringLiteral("/Alpha.sfc"));
        touch(games() + QStringLiteral("/sub/Beta.sfc"));
        {
            GameLibrary library;
            QVERIFY(library.open(db()));
            library.setScanOptions(options());
            QVERIFY(library.needsRescan());
            QSignalSpy scanning(&library, &GameLibrary::scanningChanged);
            scan(library);
            QCOMPARE(scanning.size(), 2);
            QCOMPARE(library.entries().size(), 2);
            QCOMPARE(library.lastStats().added, 2);
            QVERIFY(!library.needsRescan());

            const auto alpha = byTitle(library, QStringLiteral("Alpha"));
            QVERIFY(alpha);
            QCOMPARE(alpha->emulatorId(), QStringLiteral("snes"));
            QVERIFY(library.setFavorite(alpha->id, true));
            QVERIFY(library.recordLaunch(alpha->id));
            QVERIFY(library.recordLaunch(alpha->id));
        }
        // "Restart": a new instance sees the same data without scanning.
        GameLibrary again;
        QVERIFY(again.open(db()));
        again.setScanOptions(options());
        QVERIFY(!again.needsRescan());
        QCOMPARE(again.entries().size(), 2);
        const auto alpha = byTitle(again, QStringLiteral("Alpha"));
        QVERIFY(alpha);
        QVERIFY(alpha->favorite);
        QCOMPARE(alpha->playCount, 2);
        QVERIFY(alpha->lastPlayed.isValid());
    }

    void writesDuringAScanAreNotLost()
    {
        // The worker merges through its own connection and hands back a snapshot; user
        // writes made on this thread while it runs must survive the swap.
        for (int i = 0; i < 50; ++i)
            touch(games() + QStringLiteral("/Game %1.sfc").arg(i));
        GameLibrary library;
        QVERIFY(library.open(db()));
        library.setScanOptions(options());
        scan(library);
        const qint64 id = byTitle(library, QStringLiteral("Game 7"))->id;

        touch(games() + QStringLiteral("/New.sfc"));
        QSignalSpy done(&library, &GameLibrary::scanFinished);
        QVERIFY(library.refresh());
        QVERIFY(library.setFavorite(id, true)); // must not fail with "database is locked"
        QVERIFY(library.recordLaunch(id));
        QVERIFY(done.wait(10000));
        QCOMPARE(library.lastStats().added, 1);
        const auto game = byTitle(library, QStringLiteral("Game 7"));
        QVERIFY(game);
        QVERIFY(game->favorite);
        QCOMPARE(game->playCount, 1);
        QVERIFY(byTitle(library, QStringLiteral("New")));
        // And the database agrees with the cache.
        QVERIFY(library.entry(id)->favorite);
    }

    void inMemoryDatabaseMergesOnOwnerThread()
    {
        touch(games() + QStringLiteral("/Alpha.sfc"));
        GameLibrary library;
        QVERIFY(library.open(QStringLiteral(":memory:")));
        library.setScanOptions(options());
        scan(library);
        QCOMPARE(library.entries().size(), 1);
        QVERIFY(!library.needsRescan());
    }

    // Opt-in measurement (NEXORA_LIBRARY_BENCH=1): longest stall of the owner's event loop
    // while 10 000 files are scanned and merged, first scan and unchanged rescan.
    void benchmarkEventLoopStall()
    {
        if (qEnvironmentVariableIntValue("NEXORA_LIBRARY_BENCH") != 1)
            QSKIP("set NEXORA_LIBRARY_BENCH=1 to run");
        for (int d = 0; d < 100; ++d) {
            for (int i = 0; i < 100; ++i)
                touch(games() + QStringLiteral("/dir%1/Game %2.sfc").arg(d).arg(i));
        }
        GameLibrary library;
        QVERIFY(library.open(db()));
        library.setScanOptions(options());
        for (const char *label : {"first scan", "unchanged rescan"}) {
            QElapsedTimer total;
            QElapsedTimer sinceTick;
            qint64 maxGap = 0;
            QTimer ticker;
            ticker.setInterval(5);
            connect(&ticker, &QTimer::timeout, this, [&] { maxGap = qMax(maxGap, sinceTick.restart()); });
            QSignalSpy done(&library, &GameLibrary::scanFinished);
            total.start();
            sinceTick.start();
            ticker.start();
            QVERIFY(library.refresh());
            QVERIFY(done.wait(60000));
            ticker.stop();
            maxGap = qMax(maxGap, sinceTick.elapsed());
            QCOMPARE(library.entries().size(), 10000);
            qInfo("%s: total %lld ms, longest event-loop stall %lld ms", label, total.elapsed(), maxGap);
        }
    }

    void rescanKeepsUserDataAndTracksMissingFiles()
    {
        touch(games() + QStringLiteral("/Alpha.sfc"));
        touch(games() + QStringLiteral("/Beta.sfc"));
        GameLibrary library;
        QVERIFY(library.open(db()));
        library.setScanOptions(options());
        scan(library);
        const qint64 alphaId = byTitle(library, QStringLiteral("Alpha"))->id;
        QVERIFY(library.setFavorite(alphaId, true));

        QVERIFY(QFile::remove(games() + QStringLiteral("/Alpha.sfc")));
        scan(library);
        QCOMPARE(library.lastStats().missing, 1);
        QVERIFY(!byTitle(library, QStringLiteral("Alpha"))); // hidden while missing
        QCOMPARE(library.entries().size(), 1);

        touch(games() + QStringLiteral("/Alpha.sfc")); // file comes back
        scan(library);
        const auto alpha = byTitle(library, QStringLiteral("Alpha"));
        QVERIFY(alpha);
        QCOMPARE(alpha->id, alphaId);
        QVERIFY(alpha->favorite); // favorites survive a temporary disappearance

        QVERIFY(QFile::remove(games() + QStringLiteral("/Beta.sfc")));
        scan(library);
        QCOMPARE(library.purgeMissing(), 1);
    }

    void unavailableRootKeepsEntries()
    {
        touch(games() + QStringLiteral("/Alpha.sfc"));
        GameLibrary library;
        QVERIFY(library.open(db()));
        library.setScanOptions(options());
        scan(library);
        QCOMPARE(library.entries().size(), 1);

        // e.g. an unmounted USB drive: the root is still configured but cannot be scanned.
        QVERIFY(QDir(games()).removeRecursively());
        scan(library);
        QCOMPARE(library.entries().size(), 1);
        QCOMPARE(library.lastScan().issues.size(), 1);

        // Removing the directory from the configuration drops its entries.
        auto o = options();
        o.roots = {m_dir.filePath(QStringLiteral("other"))};
        library.setScanOptions(o);
        QVERIFY(library.needsRescan());
        scan(library);
        QVERIFY(library.entries().isEmpty());
    }

    void symlinkedRootToUnmountedDriveKeepsEntries()
    {
        const QString drive = m_dir.filePath(QStringLiteral("usb/roms"));
        touch(drive + QStringLiteral("/Alpha.sfc"));
        const QString link = m_dir.filePath(QStringLiteral("roms-link"));
        QVERIFY(QFile::link(drive, link));
        GameLibrary library;
        QVERIFY(library.open(db()));
        auto o = options();
        o.roots = {link};
        library.setScanOptions(o);
        scan(library);
        QCOMPARE(library.entries().size(), 1);

        QVERIFY(QDir(m_dir.filePath(QStringLiteral("usb"))).removeRecursively()); // drive gone, link dangles
        scan(library);
        QCOMPARE(library.entries().size(), 1);
    }

    void queuesRefreshWhileScanning()
    {
        for (int i = 0; i < 50; ++i)
            touch(games() + QStringLiteral("/g%1.sfc").arg(i));
        GameLibrary library;
        QVERIFY(library.open(db()));
        library.setScanOptions(options());
        QSignalSpy done(&library, &GameLibrary::scanFinished);
        QVERIFY(library.refresh());
        QVERIFY(!library.refresh()); // queued, not started twice
        QTRY_COMPARE_WITH_TIMEOUT(done.size(), 2, 10000);
        QCOMPARE(library.entries().size(), 50);
    }

    void destroyDuringScanIsSafe()
    {
        for (int i = 0; i < 200; ++i)
            touch(games() + QStringLiteral("/d%1/g.sfc").arg(i));
        auto library = std::make_unique<GameLibrary>();
        QVERIFY(library->open(db()));
        library->setScanOptions(options());
        QVERIFY(library->refresh());
        library.reset(); // cancels and joins the worker
    }

    void recoversFromCorruptedDatabase()
    {
        QDir().mkpath(QFileInfo(db()).absolutePath());
        QFile f(db());
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(QByteArray(4096, 'Z'));
        f.close();

        touch(games() + QStringLiteral("/Alpha.sfc"));
        GameLibrary library;
        QVERIFY2(library.open(db()), qPrintable(library.lastError()));
        QCOMPARE(library.databaseIssues().size(), 1);
        const QStringList aside
            = QDir(QFileInfo(db()).absolutePath()).entryList({QStringLiteral("library.sqlite.corrupt-*")});
        QCOMPARE(aside.size(), 1);
        library.setScanOptions(options());
        scan(library);
        QCOMPARE(library.entries().size(), 1);
    }

    void newerSchemaIsLeftUntouched()
    {
        {
            GameLibrary library;
            QVERIFY(library.open(db()));
        }
        {
            QSqlDatabase raw = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("raw"));
            raw.setDatabaseName(db());
            QVERIFY(raw.open());
            QSqlQuery(raw).exec(QStringLiteral("PRAGMA user_version = 99"));
            raw.close();
        }
        QSqlDatabase::removeDatabase(QStringLiteral("raw"));
        GameLibrary library;
        QVERIFY(!library.open(db()));
        QVERIFY(library.lastError().contains(QStringLiteral("newer")));
        QVERIFY(QFile::exists(db()));
        QCOMPARE(QDir(QFileInfo(db()).absolutePath()).entryList({QStringLiteral("library.sqlite.corrupt-*")}).size(),
                 0);
    }

    void userMetadataSurvivesRescans()
    {
        touch(games() + QStringLiteral("/Alpha.sfc"));
        touch(games() + QStringLiteral("/Disc.iso"));
        GameLibrary library;
        QVERIFY(library.open(db()));
        library.setCatalog(nexora::shared::SystemCatalog::builtIn());
        auto o = options();
        o.extensionMap.insert(QStringLiteral(".iso"), {QStringLiteral("multi")});
        library.setScanOptions(o);
        scan(library);
        const auto alpha = byTitle(library, QStringLiteral("Alpha"));
        const auto disc = byTitle(library, QStringLiteral("Disc"));
        QVERIFY(alpha && disc);
        QCOMPARE(alpha->systemId, QStringLiteral("snes")); // unique extension
        QVERIFY(disc->systemId.isEmpty()); // ambiguous extension
        QVERIFY(disc->systemCandidates.size() >= 2);

        QVERIFY(library.setMetadata(alpha->id, QStringLiteral("Alpha Deluxe"), QStringLiteral("good\nstuff")));
        QVERIFY(library.setFavorite(alpha->id, true));
        QVERIFY(library.recordLaunch(alpha->id));
        QVERIFY(library.setSystemOverride(disc->id, disc->systemCandidates.first()));
        QVERIFY(!library.setSystemOverride(disc->id, QStringLiteral("no-such-system")));
        QVERIFY(!library.setMetadata(alpha->id, QString(201, u'x'), QString()));
        QCOMPARE(byTitle(library, QStringLiteral("Disc"))->systemId, disc->systemCandidates.first());

        // The file changes on disk (so the row is updated) and the library is rescanned.
        QFile f(games() + QStringLiteral("/Alpha.sfc"));
        QVERIFY(f.open(QIODevice::Append));
        f.write(QByteArray(100, 'x'));
        f.close();
        scan(library);
        QVERIFY(library.lastStats().updated >= 1);

        const auto again = library.entry(alpha->id);
        QVERIFY(again);
        QCOMPARE(again->titleOverride, QStringLiteral("Alpha Deluxe"));
        QCOMPARE(again->displayTitle(), QStringLiteral("Alpha Deluxe"));
        QCOMPARE(again->title, QStringLiteral("Alpha")); // derived title still follows the file
        QCOMPARE(again->notes, QStringLiteral("good\nstuff"));
        QVERIFY(again->favorite);
        QCOMPARE(again->playCount, 1);
        QCOMPARE(library.entry(disc->id)->systemId, disc->systemCandidates.first());

        QVERIFY(library.setSystemOverride(disc->id, QString())); // clearing falls back to detection
        QVERIFY(library.entry(disc->id)->systemId.isEmpty());
        QVERIFY(!library.setSystemOverride(999999, QStringLiteral("snes")));
    }

    void removeFromLibraryHidesButNeverDeletes()
    {
        touch(games() + QStringLiteral("/Alpha.sfc"));
        touch(games() + QStringLiteral("/Beta.sfc"));
        GameLibrary library;
        QVERIFY(library.open(db()));
        library.setScanOptions(options());
        scan(library);
        const auto alpha = byTitle(library, QStringLiteral("Alpha"));
        QVERIFY(alpha);
        QVERIFY(library.setFavorite(alpha->id, true));

        QVERIFY(library.removeFromLibrary(alpha->id));
        QVERIFY(QFile::exists(games() + QStringLiteral("/Alpha.sfc")));
        QVERIFY(!byTitle(library, QStringLiteral("Alpha")));
        QCOMPARE(library.entries().size(), 1);
        QCOMPARE(library.hiddenEntries().size(), 1);
        QVERIFY(!library.removeFromLibrary(999999));

        scan(library); // a rescan does not bring it back
        QVERIFY(!byTitle(library, QStringLiteral("Alpha")));
        QCOMPARE(library.lastStats().added, 0);
        QVERIFY(QFile::exists(games() + QStringLiteral("/Alpha.sfc")));

        // ... also not after a restart.
        GameLibrary again;
        QVERIFY(again.open(db()));
        QCOMPARE(again.entries().size(), 1);
        QCOMPARE(again.hiddenEntries().size(), 1);

        QVERIFY(again.restoreHidden());
        QVERIFY(!again.restoreHidden()); // nothing left to restore
        const auto restored = byTitle(again, QStringLiteral("Alpha"));
        QVERIFY(restored);
        QCOMPARE(restored->id, alpha->id);
        QVERIFY(restored->favorite); // user data was kept while hidden
        QVERIFY(again.hiddenEntries().isEmpty());
    }

    // Schema v3: collections never own games; names are unique ignoring case; limits hold.
    void collectionsGroupGamesWithoutOwningThem()
    {
        touch(games() + QStringLiteral("/Alpha.sfc"));
        touch(games() + QStringLiteral("/Beta.sfc"));
        GameLibrary library;
        QVERIFY(library.open(db()));
        library.setScanOptions(options());
        scan(library);
        const qint64 alpha = byTitle(library, QStringLiteral("Alpha"))->id;
        const qint64 beta = byTitle(library, QStringLiteral("Beta"))->id;
        QSignalSpy changed(&library, &GameLibrary::collectionsChanged);

        const auto rpg = library.createCollection(QStringLiteral("  RPG  "));
        QVERIFY2(rpg, qPrintable(library.lastError()));
        QVERIFY(!library.createCollection(QStringLiteral("rpg"))); // unique ignoring case
        QVERIFY(library.lastError().contains(QStringLiteral("already exists")));
        QVERIFY(!library.createCollection(QString()));
        QVERIFY(!library.createCollection(QString(61, u'x')));
        QVERIFY(!library.createCollection(QStringLiteral("tab\there")));
        QVERIFY(library.setInCollection(*rpg, beta, true));
        QVERIFY(library.setInCollection(*rpg, alpha, true));
        QVERIFY(library.setInCollection(*rpg, alpha, true)); // already a member: no-op
        QCOMPARE(library.collectionGameIds(*rpg), (QList<qint64>{beta, alpha})); // order added
        QCOMPARE(library.collectionsOf(alpha), QList<qint64>{*rpg});
        QCOMPARE(library.collections().first().name, QStringLiteral("RPG"));
        QCOMPARE(library.collections().first().count, 2);
        QVERIFY(!library.setInCollection(*rpg, 999999, true));

        QVERIFY(library.renameCollection(*rpg, QStringLiteral("Role-playing")));
        QVERIFY(library.setInCollection(*rpg, beta, false));
        QCOMPARE(library.collectionGameIds(*rpg), QList<qint64>{alpha});
        QVERIFY(library.deleteCollection(*rpg));
        QVERIFY(library.collections().isEmpty());
        QCOMPARE(library.entries().size(), 2); // games untouched
        QVERIFY(changed.size() >= 6);

        // Purging a missing game drops its memberships too.
        const auto keep = library.createCollection(QStringLiteral("Keep"));
        QVERIFY(library.setInCollection(*keep, alpha, true));
        QVERIFY(library.setInCollection(*keep, beta, true));
        QVERIFY(QFile::remove(games() + QStringLiteral("/Beta.sfc")));
        scan(library);
        QCOMPARE(library.purgeMissing(), 1);
        QCOMPARE(library.collectionGameIds(*keep), QList<qint64>{alpha});
    }

    void pinningAndPlayTime()
    {
        touch(games() + QStringLiteral("/Alpha.sfc"));
        touch(games() + QStringLiteral("/Beta.sfc"));
        GameLibrary library;
        QVERIFY(library.open(db()));
        library.setScanOptions(options());
        scan(library);
        const qint64 alpha = byTitle(library, QStringLiteral("Alpha"))->id;
        const qint64 beta = byTitle(library, QStringLiteral("Beta"))->id;
        QVERIFY(library.setPinned(beta, true));
        QVERIFY(library.setPinned(alpha, true));
        QVERIFY(library.setPinned(alpha, true)); // keeps its place
        QCOMPARE(library.entry(beta)->pinned, 1);
        QCOMPARE(library.entry(alpha)->pinned, 2);
        QVERIFY(library.setPinned(beta, false));
        QCOMPARE(library.entry(beta)->pinned, 0);

        QVERIFY(library.addPlayTime(alpha, 125));
        QVERIFY(library.addPlayTime(alpha, 5));
        QVERIFY(!library.addPlayTime(alpha, 0));
        QCOMPARE(byTitle(library, QStringLiteral("Alpha"))->playSeconds, qint64(130)); // cache
        library.close();
        QVERIFY(library.open(db()));
        QCOMPARE(library.entry(alpha)->playSeconds, qint64(130)); // persisted
        QCOMPARE(library.entry(alpha)->pinned, 2);
    }

    void purgeMissingKeepsHiddenEntries()
    {
        touch(games() + QStringLiteral("/Alpha.sfc"));
        GameLibrary library;
        QVERIFY(library.open(db()));
        library.setScanOptions(options());
        scan(library);
        QVERIFY(library.removeFromLibrary(library.entries().first().id));
        QVERIFY(QFile::remove(games() + QStringLiteral("/Alpha.sfc")));
        scan(library);
        QCOMPARE(library.purgeMissing(), 0);
        QCOMPARE(library.hiddenEntries().size(), 1);
    }

    void reportsScanProgress()
    {
        for (int i = 0; i < 250; ++i)
            touch(games() + QStringLiteral("/g%1.sfc").arg(i));
        GameLibrary library;
        QVERIFY(library.open(db()));
        library.setScanOptions(options());
        QSignalSpy progress(&library, &GameLibrary::scanProgress);
        scan(library);
        QVERIFY(progress.size() >= 1);
        QCOMPARE(progress.first().at(1).toString(), QFileInfo(games()).canonicalFilePath());
        QCOMPARE(library.entries().size(), 250);
    }

    void cancelledScanDoesNotMarkEntriesMissing()
    {
        for (int i = 0; i < 5000; ++i)
            touch(games() + QStringLiteral("/d%1/g%2.sfc").arg(i % 50).arg(i));
        GameLibrary library;
        QVERIFY(library.open(db()));
        library.setScanOptions(options());
        scan(library);
        QCOMPARE(library.entries().size(), 5000);

        QSignalSpy finished(&library, &GameLibrary::scanFinished);
        QSignalSpy scanning(&library, &GameLibrary::scanningChanged);
        QVERIFY(library.refresh());
        library.cancelScan();
        library.waitForScan();
        // The worker may complete before the cancel lands, so only the invariants that hold in
        // both outcomes are asserted unconditionally; mid-walk cancellation itself is covered
        // deterministically in tst_libraryscanner.
        QVERIFY(!library.isScanning());
        QCOMPARE(scanning.size(), 2); // true, then false
        QCOMPARE(scanning.last().at(0).toBool(), false);
        QCOMPARE(library.entries().size(), 5000);
        for (const GameEntry &e : library.entries())
            QVERIFY(e.present);
        if (library.lastScan().cancelled) {
            QCOMPARE(finished.size(), 0); // a cancelled scan is not applied
            QCOMPARE(library.lastStats().missing, 0);
        } else {
            QCOMPARE(finished.size(), 1);
        }
    }

    void overlappingRootsYieldOneEntry()
    {
        touch(games() + QStringLiteral("/snes/Alpha.sfc"));
        GameLibrary library;
        QVERIFY(library.open(db()));
        auto o = options();
        o.roots = {games(), games() + QStringLiteral("/snes")};
        library.setScanOptions(o);
        scan(library);
        QCOMPARE(library.entries().size(), 1);
        QCOMPARE(library.lastScan().duplicates, 1);
    }

    void migratesV1LibraryAndRescansForSystems()
    {
        touch(games() + QStringLiteral("/Alpha.sfc"));
        {
            GameLibrary library; // only to create the file
            QVERIFY(library.open(db()));
        }
        {
            QSqlDatabase raw = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("v1"));
            raw.setDatabaseName(db());
            QVERIFY(raw.open());
            const QStringList downgrade{QStringLiteral("ALTER TABLE games DROP COLUMN system_id"),
                                        QStringLiteral("ALTER TABLE games DROP COLUMN system_override"),
                                        QStringLiteral("ALTER TABLE games DROP COLUMN system_candidates"),
                                        QStringLiteral("ALTER TABLE games DROP COLUMN title_override"),
                                        QStringLiteral("ALTER TABLE games DROP COLUMN notes"),
                                        QStringLiteral("ALTER TABLE games DROP COLUMN hidden"),
                                        QStringLiteral("ALTER TABLE games DROP COLUMN play_seconds"),
                                        QStringLiteral("ALTER TABLE games DROP COLUMN pinned"),
                                        QStringLiteral("DROP TABLE collection_games"),
                                        QStringLiteral("DROP TABLE collections"),
                                        QStringLiteral("PRAGMA user_version = 1")};
            for (const QString &s : downgrade)
                QVERIFY2(QSqlQuery(raw).exec(s), qPrintable(s));
            raw.close();
        }
        QSqlDatabase::removeDatabase(QStringLiteral("v1"));

        GameLibrary library;
        QVERIFY2(library.open(db()), qPrintable(library.lastError()));
        QCOMPARE(library.databaseIssues().size(), 1);
        QCOMPARE(QDir(QFileInfo(db()).absolutePath()).entryList({QStringLiteral("library.sqlite.v1-backup-*")}).size(),
                 1);
        library.setCatalog(nexora::shared::SystemCatalog::builtIn());
        library.setScanOptions(options());
        QVERIFY(library.needsRescan());
        scan(library);
        QCOMPARE(library.entries().first().systemId, QStringLiteral("snes"));
    }

    void emulatorOverride()
    {
        touch(games() + QStringLiteral("/Alpha.sfc"));
        GameLibrary library;
        QVERIFY(library.open(db()));
        auto o = options();
        o.extensionMap[QStringLiteral(".sfc")] = {QStringLiteral("snes"), QStringLiteral("multi")};
        library.setScanOptions(o);
        scan(library);
        const qint64 id = library.entries().first().id;
        QCOMPARE(library.entries().first().emulatorId(), QStringLiteral("snes"));
        QVERIFY(library.setEmulatorOverride(id, QStringLiteral("multi")));
        QCOMPARE(library.entries().first().emulatorId(), QStringLiteral("multi"));
        QVERIFY(!library.setFavorite(999999, true)); // unknown id
    }
};

QTEST_GUILESS_MAIN(TestGameLibrary)
#include "tst_gamelibrary.moc"
