#include "nexora/library/LibraryQuery.h"
#include "nexora/library/LibraryScanner.h"

#include <QTest>

using namespace nexora::library;

namespace {

GameEntry game(qint64 id, const QString &title, const QString &emulator, bool favorite = false,
               int playedMinutesAgo = -1, bool present = true, int addedDaysAgo = 0)
{
    GameEntry e;
    e.id = id;
    e.title = title;
    e.path = QStringLiteral("/games/%1.rom").arg(title);
    e.emulatorCandidates = {emulator};
    e.favorite = favorite;
    e.present = present;
    const QDateTime now = QDateTime(QDate(2026, 1, 1), QTime(12, 0), Qt::UTC);
    if (playedMinutesAgo >= 0)
        e.lastPlayed = now.addSecs(-60 * playedMinutesAgo);
    e.addedAt = now.addDays(-addedDaysAgo);
    return e;
}

QStringList titles(const QList<GameEntry> &list)
{
    QStringList t;
    for (const auto &e : list)
        t.append(e.title);
    return t;
}

QList<GameEntry> sample()
{
    return {game(1, QStringLiteral("zelda"), QStringLiteral("snes"), true, 30, true, 5),
            game(2, QStringLiteral("Asteroids"), QStringLiteral("atari"), false, -1, true, 1),
            game(3, QStringLiteral("Metroid"), QStringLiteral("nes"), true, 5, true, 9),
            game(4, QStringLiteral("Gone Game"), QStringLiteral("nes"), true, 1, false),
            game(5, QStringLiteral("mario kart"), QStringLiteral("snes"), false, 120, true, 2)};
}

} // namespace

class TestLibraryQuery : public QObject
{
    Q_OBJECT

private slots:
    void sortsByTitleCaseInsensitively()
    {
        const auto r = LibraryQuery::apply(sample(), {});
        QCOMPARE(titles(r),
                 QStringList({QStringLiteral("Asteroids"), QStringLiteral("mario kart"), QStringLiteral("Metroid"),
                              QStringLiteral("zelda")}));
    }

    void hidesMissingEntries()
    {
        const auto r = LibraryQuery::apply(sample(), {});
        QVERIFY(!titles(r).contains(QStringLiteral("Gone Game")));
    }

    void sortsByRecentThenNeverPlayed()
    {
        QueryOptions o;
        o.sort = SortOrder::Recent;
        QCOMPARE(titles(LibraryQuery::apply(sample(), o)),
                 QStringList({QStringLiteral("Metroid"), QStringLiteral("zelda"), QStringLiteral("mario kart"),
                              QStringLiteral("Asteroids")}));
    }

    void sortsByEmulatorAndAdded()
    {
        QueryOptions o;
        o.sort = SortOrder::Emulator;
        QCOMPARE(titles(LibraryQuery::apply(sample(), o)),
                 QStringList({QStringLiteral("Asteroids"), QStringLiteral("Metroid"), QStringLiteral("mario kart"),
                              QStringLiteral("zelda")}));
        o.sort = SortOrder::Added;
        QCOMPARE(titles(LibraryQuery::apply(sample(), o)),
                 QStringList({QStringLiteral("Asteroids"), QStringLiteral("mario kart"), QStringLiteral("zelda"),
                              QStringLiteral("Metroid")}));
    }

    void filtersFavorites()
    {
        QueryOptions o;
        o.filter = LibraryFilter::Favorites;
        QCOMPARE(titles(LibraryQuery::apply(sample(), o)),
                 QStringList({QStringLiteral("Metroid"), QStringLiteral("zelda")}));
    }

    void recentFilterIsLimitedAndOrdered()
    {
        QueryOptions o;
        o.filter = LibraryFilter::Recent;
        o.sort = SortOrder::Title; // ignored for the recent view
        o.recentLimit = 2;
        QCOMPARE(titles(LibraryQuery::apply(sample(), o)),
                 QStringList({QStringLiteral("Metroid"), QStringLiteral("zelda")}));
    }

    void searchesAllTerms()
    {
        QueryOptions o;
        o.search = QStringLiteral("  MARIO  kart ");
        QCOMPARE(titles(LibraryQuery::apply(sample(), o)), QStringList{QStringLiteral("mario kart")});
        o.search = QStringLiteral("mario zelda");
        QVERIFY(LibraryQuery::apply(sample(), o).isEmpty());
        o.search = QStringLiteral("roid");
        QCOMPARE(titles(LibraryQuery::apply(sample(), o)),
                 QStringList({QStringLiteral("Asteroids"), QStringLiteral("Metroid")}));
    }

    void filtersByEmulator()
    {
        QueryOptions o;
        o.emulatorId = QStringLiteral("snes");
        QCOMPARE(titles(LibraryQuery::apply(sample(), o)),
                 QStringList({QStringLiteral("mario kart"), QStringLiteral("zelda")}));
    }

    void effectiveEmulator()
    {
        GameEntry e;
        e.emulatorCandidates = {QStringLiteral("a"), QStringLiteral("b")};
        QCOMPARE(e.emulatorId(), QStringLiteral("a"));
        e.emulatorOverride = QStringLiteral("b");
        QCOMPARE(e.emulatorId(), QStringLiteral("b"));
        e.emulatorOverride = QStringLiteral("gone"); // no longer handles the extension
        QCOMPARE(e.emulatorId(), QStringLiteral("a"));
    }

    void namesRoundTrip()
    {
        for (auto s : {SortOrder::Title, SortOrder::Recent, SortOrder::Emulator, SortOrder::Added})
            QCOMPARE(LibraryQuery::sortOrderFromString(LibraryQuery::toString(s)), s);
        for (auto f : {LibraryFilter::All, LibraryFilter::Favorites, LibraryFilter::Recent})
            QCOMPARE(LibraryQuery::filterFromString(LibraryQuery::toString(f)), f);
        QCOMPARE(LibraryQuery::sortOrderFromString(QStringLiteral("bogus"), SortOrder::Added), SortOrder::Added);
    }

    void titlesFromFileNames_data()
    {
        QTest::addColumn<QString>("file");
        QTest::addColumn<QString>("title");
        QTest::newRow("plain") << QStringLiteral("Metroid.nes") << QStringLiteral("Metroid");
        QTest::newRow("underscores") << QStringLiteral("Super_Mario__World (USA).sfc")
                                     << QStringLiteral("Super Mario World (USA)");
        QTest::newRow("dots") << QStringLiteral("F.Zero.v1.1.sfc") << QStringLiteral("F.Zero.v1.1");
        QTest::newRow("no ext") << QStringLiteral("README") << QStringLiteral("README");
        QTest::newRow("only ext") << QStringLiteral(".sfc") << QStringLiteral(".sfc");
    }

    void titlesFromFileNames()
    {
        QFETCH(QString, file);
        QFETCH(QString, title);
        QCOMPARE(LibraryScanner::titleFromFileName(file), title);
    }
};

QTEST_GUILESS_MAIN(TestLibraryQuery)
#include "tst_libraryquery.moc"
