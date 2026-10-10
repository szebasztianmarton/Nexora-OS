// GameLibrary end to end: asynchronous scans, SQLite persistence, user data across rescans
// and restarts, missing files, corrupted database recovery.
#include "nexora/library/GameLibrary.h"

#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

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
        const QStringList aside = QDir(QFileInfo(db()).absolutePath()).entryList({QStringLiteral("library.sqlite.corrupt-*")});
        QCOMPARE(aside.size(), 1);
        library.setScanOptions(options());
        scan(library);
        QCOMPARE(library.entries().size(), 1);
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
