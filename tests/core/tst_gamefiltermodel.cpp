#include <QSignalSpy>
#include <QTest>

#include "gamefiltermodel.h"
#include "gamemodel.h"

using nexora::GameEntry;
using nexora::GameFilterModel;
using nexora::GameModel;

namespace {

GameEntry makeEntry(const QString &title, const QString &emulator = QStringLiteral("mock"))
{
    GameEntry e;
    e.title = title;
    e.fileName = title + QStringLiteral(".mock");
    e.path = QStringLiteral("/games/") + e.fileName;
    e.extension = QStringLiteral("mock");
    e.emulatorId = emulator;
    return e;
}

QStringList titles(const GameFilterModel &model)
{
    QStringList result;
    for (int row = 0; row < model.rowCount(); ++row)
        result.append(model.get(row).value(QStringLiteral("title")).toString());
    return result;
}

} // namespace

class TestGameFilterModel : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void defaultSortTitle();
    void search();
    void sortModes();
    void favoritesOnly();
    void artworkOnly();
    void playedOnly();
    void emulatorFilter();
    void excludePath();
    void clearFilters();
    void countUpdates();
    void getAndIndexOfPath();

private:
    GameModel *m_source = nullptr;
    GameFilterModel *m_proxy = nullptr;
};

void TestGameFilterModel::init()
{
    const QDateTime base = QDateTime::currentDateTime();
    GameEntry zelda = makeEntry(QStringLiteral("zelda Quest"));
    zelda.favorite = true;
    zelda.lastPlayed = base.addSecs(-10);
    zelda.firstSeen = base.addDays(-3);
    GameEntry alpha = makeEntry(QStringLiteral("Alpha"), QStringLiteral("alt"));
    alpha.artworkPath = QStringLiteral("/games/Alpha.png");
    alpha.firstSeen = base.addDays(-1);
    GameEntry beta = makeEntry(QStringLiteral("beta Run"));
    beta.lastPlayed = base;
    beta.firstSeen = base.addDays(-2);
    beta.fileName = QStringLiteral("beta_run_v2.mock");
    beta.path = QStringLiteral("/games/beta_run_v2.mock");
    GameEntry game10 = makeEntry(QStringLiteral("Game 10"));
    game10.firstSeen = base.addDays(-5);
    GameEntry game2 = makeEntry(QStringLiteral("Game 2"));
    game2.firstSeen = base.addDays(-4);

    m_source = new GameModel;
    m_source->resetEntries({zelda, alpha, beta, game10, game2});
    m_proxy = new GameFilterModel;
    m_proxy->setSourceModel(m_source);
}

void TestGameFilterModel::cleanup()
{
    delete m_proxy;
    delete m_source;
}

void TestGameFilterModel::defaultSortTitle()
{
    QCOMPARE(m_proxy->sortMode(), QStringLiteral("title-asc"));
    QCOMPARE(m_proxy->count(), 5);
    QCOMPARE(titles(*m_proxy), QStringList({"Alpha", "beta Run", "Game 2", "Game 10", "zelda Quest"}));
}

void TestGameFilterModel::search()
{
    QSignalSpy filterChanged(m_proxy, &GameFilterModel::filterChanged);
    m_proxy->setSearchText(QStringLiteral("QUEST"));
    QCOMPARE(titles(*m_proxy), QStringList{"zelda Quest"});
    QVERIFY(m_proxy->filtersActive());
    QCOMPARE(filterChanged.count(), 1);
    m_proxy->setSearchText(QStringLiteral("v2")); // file name match
    QCOMPARE(titles(*m_proxy), QStringList{"beta Run"});
    m_proxy->setSearchText(QStringLiteral("game"));
    QCOMPARE(m_proxy->count(), 2);
    m_proxy->setSearchText(QStringLiteral("nothing matches"));
    QCOMPARE(m_proxy->count(), 0);
    m_proxy->setSearchText(QStringLiteral("   "));
    QCOMPARE(m_proxy->count(), 5);
    QVERIFY(!m_proxy->filtersActive());
}

void TestGameFilterModel::sortModes()
{
    m_proxy->setSortMode(QStringLiteral("title-desc"));
    QCOMPARE(titles(*m_proxy), QStringList({"zelda Quest", "Game 10", "Game 2", "beta Run", "Alpha"}));
    m_proxy->setSortMode(QStringLiteral("recent"));
    const QStringList recent = titles(*m_proxy);
    QCOMPARE(recent.mid(0, 2), QStringList({"beta Run", "zelda Quest"}));
    QCOMPARE(recent.mid(2), QStringList({"Alpha", "Game 2", "Game 10"})); // never played: by title
    m_proxy->setSortMode(QStringLiteral("added"));
    QCOMPARE(titles(*m_proxy), QStringList({"Alpha", "beta Run", "zelda Quest", "Game 2", "Game 10"}));
    m_proxy->setSortMode(QStringLiteral("bogus"));
    QCOMPARE(m_proxy->sortMode(), QStringLiteral("added"));

    // Dynamic re-sorting when the source changes.
    m_proxy->setSortMode(QStringLiteral("recent"));
    GameEntry game2 = *m_source->entry(QStringLiteral("/games/Game 2.mock"));
    game2.lastPlayed = QDateTime::currentDateTime().addDays(1);
    m_source->updateEntry(game2);
    QCOMPARE(titles(*m_proxy).first(), QStringLiteral("Game 2"));
}

void TestGameFilterModel::favoritesOnly()
{
    m_proxy->setFavoritesOnly(true);
    QCOMPARE(titles(*m_proxy), QStringList{"zelda Quest"});
    QVERIFY(m_proxy->filtersActive());
}

void TestGameFilterModel::artworkOnly()
{
    m_proxy->setArtworkOnly(true);
    QCOMPARE(titles(*m_proxy), QStringList{"Alpha"});
}

void TestGameFilterModel::playedOnly()
{
    m_proxy->setPlayedOnly(true);
    QCOMPARE(titles(*m_proxy), QStringList({"beta Run", "zelda Quest"}));
}

void TestGameFilterModel::emulatorFilter()
{
    m_proxy->setEmulatorFilter(QStringLiteral("alt"));
    QCOMPARE(titles(*m_proxy), QStringList{"Alpha"});
    m_proxy->setEmulatorFilter(QString());
    QCOMPARE(m_proxy->count(), 5);
}

void TestGameFilterModel::excludePath()
{
    m_proxy->setExcludePath(QStringLiteral("/games/Alpha.mock"));
    QCOMPARE(m_proxy->count(), 4);
    QVERIFY(!titles(*m_proxy).contains(QStringLiteral("Alpha")));
    QVERIFY(!m_proxy->filtersActive()); // not a user filter
}

void TestGameFilterModel::clearFilters()
{
    m_proxy->setSortMode(QStringLiteral("title-desc"));
    m_proxy->setSearchText(QStringLiteral("a"));
    m_proxy->setFavoritesOnly(true);
    m_proxy->setPlayedOnly(true);
    m_proxy->setArtworkOnly(true);
    m_proxy->setEmulatorFilter(QStringLiteral("mock"));
    QCOMPARE(m_proxy->count(), 0);
    m_proxy->clearFilters();
    QCOMPARE(m_proxy->count(), 5);
    QVERIFY(!m_proxy->filtersActive());
    QVERIFY(m_proxy->searchText().isEmpty());
    QCOMPARE(m_proxy->sortMode(), QStringLiteral("title-desc"));
}

void TestGameFilterModel::countUpdates()
{
    QSignalSpy countChanged(m_proxy, &GameFilterModel::countChanged);
    m_proxy->setFavoritesOnly(true);
    QVERIFY(countChanged.count() > 0);
    QCOMPARE(m_proxy->count(), 1);

    countChanged.clear();
    m_proxy->setFavoritesOnly(false);
    QVERIFY(countChanged.count() > 0);

    countChanged.clear();
    m_source->removeEntry(QStringLiteral("/games/Alpha.mock"));
    QVERIFY(countChanged.count() > 0);
    QCOMPARE(m_proxy->count(), 4);

    countChanged.clear();
    m_source->resetEntries({makeEntry(QStringLiteral("Solo"))});
    QVERIFY(countChanged.count() > 0);
    QCOMPARE(m_proxy->count(), 1);
}

void TestGameFilterModel::getAndIndexOfPath()
{
    const QVariantMap first = m_proxy->get(0);
    QCOMPARE(first.value(QStringLiteral("title")).toString(), QStringLiteral("Alpha"));
    QCOMPARE(first.value(QStringLiteral("hasArtwork")).toBool(), true);
    QCOMPARE(first.value(QStringLiteral("artworkUrl")).toString(), QStringLiteral("file:///games/Alpha.png"));
    QVERIFY(m_proxy->get(-1).isEmpty());
    QVERIFY(m_proxy->get(99).isEmpty());
    QCOMPARE(m_proxy->indexOfPath(QStringLiteral("/games/Alpha.mock")), 0);
    QCOMPARE(m_proxy->indexOfPath(QStringLiteral("/games/zelda Quest.mock")), 4);
    QCOMPARE(m_proxy->indexOfPath(QStringLiteral("/nope")), -1);
    m_proxy->setFavoritesOnly(true);
    QCOMPARE(m_proxy->indexOfPath(QStringLiteral("/games/Alpha.mock")), -1);
}

QTEST_GUILESS_MAIN(TestGameFilterModel)
#include "tst_gamefiltermodel.moc"
