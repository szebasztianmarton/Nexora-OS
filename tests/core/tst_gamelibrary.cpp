#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include "appsettings.h"
#include "emulatorregistry.h"
#include "gamelibrary.h"
#include "testutils.h"

using nexora::AppSettings;
using nexora::EmulatorRegistry;
using nexora::GameLibrary;
using nexora::GameModel;

namespace {
constexpr int Timeout = 10000;

bool scan(GameLibrary &library)
{
    QSignalSpy finished(&library, &GameLibrary::scanFinished);
    library.refresh();
    return finished.wait(Timeout);
}

QStringList fileNames(const QList<nexora::GameEntry> &entries)
{
    QStringList names;
    for (const auto &e : entries)
        names.append(e.fileName);
    names.sort();
    return names;
}
} // namespace

// One configured environment: config dir, library dir, settings, registry.
struct Fixture
{
    QTemporaryDir config;
    QTemporaryDir lib;
    AppSettings *settings = nullptr;
    EmulatorRegistry *registry = nullptr;

    Fixture()
    {
        testutils::writeEmulators(config.path(), {testutils::mockEntry("mock", {"mock"}),
                                                  testutils::mockEntry("alt", {"alt", "mock"})});
        settings = new AppSettings(config.path());
        settings->addLibraryDirectory(lib.path());
        registry = new EmulatorRegistry(config.path());
    }
    ~Fixture()
    {
        delete registry;
        delete settings;
    }
    QString game(const QString &relative)
    {
        const QString path = lib.filePath(relative);
        testutils::writeFile(path);
        return path;
    }
};

class TestGameLibrary : public QObject
{
    Q_OBJECT

private slots:
    void scanFindsSupportedExtensions();
    void depthLimit();
    void missingDirectorySkipped();
    void symlinkedDirectoriesNotFollowed();
    void titleDerivation();
    void artwork();
    void modelRoles();
    void emulatorInfoAndCounts();
    void favoritesPersist();
    void recordLaunchAndRecentCap();
    void removeFromLibraryKeepsFile();
    void emulatorOverride();
    void firstSeenPersists();
    void corruptStateFile();
    void refreshWhileScanningQueues();
};

void TestGameLibrary::scanFindsSupportedExtensions()
{
    Fixture f;
    f.game("Alpha.mock");
    f.game("Beta.MOCK");
    f.game("Gamma.alt");
    f.game("notes.txt");
    f.game("noext");
    f.game(".hidden.mock");
    f.game(".hiddendir/Delta.mock");
    f.game("sub/Epsilon.mock");
    GameLibrary library(f.config.path(), f.settings, f.registry);
    QVERIFY(!library.hasScanned());
    QVERIFY(scan(library));
    QVERIFY(library.hasScanned());
    QVERIFY(library.lastScanTime().isValid());
    QCOMPARE(fileNames(library.model()->entries()),
             QStringList({"Alpha.mock", "Beta.MOCK", "Epsilon.mock", "Gamma.alt"}));
    const nexora::GameEntry *beta = library.model()->entry(f.lib.filePath("Beta.MOCK"));
    QVERIFY(beta);
    QCOMPARE(beta->extension, QStringLiteral("mock"));
    QCOMPARE(beta->directory, QDir::cleanPath(f.lib.path()));
    QCOMPARE(beta->fileSize, qint64(1));
    QVERIFY(library.skippedEntries().isEmpty());
}

void TestGameLibrary::depthLimit()
{
    QTemporaryDir lib;
    testutils::writeFile(lib.filePath("d1/d2/d3/d4/four.mock"));
    testutils::writeFile(lib.filePath("d1/d2/d3/d4/d5/five.mock"));
    const auto result = GameLibrary::scanDirectories({lib.path()}, {"mock"}, {});
    QCOMPARE(fileNames(result.entries), QStringList{"four.mock"});
    const auto shallow = GameLibrary::scanDirectories({lib.path()}, {"mock"}, {}, 1);
    QVERIFY(shallow.entries.isEmpty());
}

void TestGameLibrary::missingDirectorySkipped()
{
    QTemporaryDir lib;
    testutils::writeFile(lib.filePath("ok.mock"));
    const QString missing = lib.filePath("unplugged");
    const auto result = GameLibrary::scanDirectories({missing, lib.path()}, {"mock"}, {});
    QCOMPARE(result.entries.size(), 1);
    QCOMPARE(result.skipped.size(), 1);
    QVERIFY(result.skipped.first().startsWith(missing));
    QVERIFY(result.skipped.first().contains("not found"));
}

void TestGameLibrary::symlinkedDirectoriesNotFollowed()
{
    QTemporaryDir lib;
    QTemporaryDir outside;
    testutils::writeFile(outside.filePath("secret.mock"));
    testutils::writeFile(lib.filePath("real.mock"));
    QVERIFY(QFile::link(outside.path(), lib.filePath("linked")));
    const auto result = GameLibrary::scanDirectories({lib.path()}, {"mock"}, {});
    QCOMPARE(fileNames(result.entries), QStringList{"real.mock"});
}

void TestGameLibrary::titleDerivation()
{
    QCOMPARE(GameLibrary::titleFromFileName("Super_Game__Deluxe.mock"), QStringLiteral("Super Game Deluxe"));
    QCOMPARE(GameLibrary::titleFromFileName("  Spaced   Out  .mock"), QStringLiteral("Spaced Out"));
    QCOMPARE(GameLibrary::titleFromFileName("Game.v1.2.zip"), QStringLiteral("Game.v1.2"));
    QCOMPARE(GameLibrary::titleFromFileName("noext"), QStringLiteral("noext"));
    QCOMPARE(GameLibrary::titleFromFileName("___.mock"), QStringLiteral("___.mock"));
}

void TestGameLibrary::artwork()
{
    QTemporaryDir lib;
    testutils::writeFile(lib.filePath("Side.mock"));
    testutils::writeFile(lib.filePath("Side.png"));
    testutils::writeFile(lib.filePath("Cover.mock"));
    testutils::writeFile(lib.filePath("covers/Cover.jpg"));
    testutils::writeFile(lib.filePath("Art.v2.mock"));
    testutils::writeFile(lib.filePath("artwork/Art.v2.webp"));
    testutils::writeFile(lib.filePath("None.mock"));
    QCOMPARE(GameLibrary::findArtwork(lib.filePath("Side.mock")), lib.filePath("Side.png"));
    QCOMPARE(GameLibrary::findArtwork(lib.filePath("Cover.mock")), lib.filePath("covers/Cover.jpg"));
    QCOMPARE(GameLibrary::findArtwork(lib.filePath("Art.v2.mock")), lib.filePath("artwork/Art.v2.webp"));
    QCOMPARE(GameLibrary::findArtwork(lib.filePath("None.mock")), QString());

    const auto result = GameLibrary::scanDirectories({lib.path()}, {"mock"}, {});
    QCOMPARE(result.entries.size(), 4); // covers/ and artwork/ contain no games
}

void TestGameLibrary::modelRoles()
{
    Fixture f;
    const QString path = f.game("Role_Game.mock");
    testutils::writeFile(f.lib.filePath("Role_Game.png"));
    GameLibrary library(f.config.path(), f.settings, f.registry);
    QVERIFY(scan(library));

    const QStringList expected{"path", "title", "fileName", "extension", "directory", "fileSize", "artworkUrl",
                               "hasArtwork", "emulatorId", "emulatorName", "emulatorOverridden", "platform",
                               "favorite", "lastPlayed", "firstSeen"};
    GameModel *model = library.model();
    QStringList names;
    for (const QByteArray &n : model->roleNames())
        names.append(QString::fromUtf8(n));
    names.sort();
    QStringList sortedExpected = expected;
    sortedExpected.sort();
    QCOMPARE(names, sortedExpected);
    QCOMPARE(model->roleNames().value(GameModel::ArtworkUrlRole), QByteArray("artworkUrl"));

    const QVariantMap map = library.get(path);
    QStringList keys = map.keys();
    keys.sort();
    QCOMPARE(keys, sortedExpected);
    QCOMPARE(map.value("title").toString(), QStringLiteral("Role Game"));
    QCOMPARE(map.value("artworkUrl").toString(), QUrl::fromLocalFile(f.lib.filePath("Role_Game.png")).toString());
    QCOMPARE(map.value("hasArtwork").toBool(), true);
    QCOMPARE(model->index(0).data(GameModel::TitleRole).toString(), QStringLiteral("Role Game"));
    QVERIFY(library.get("/nonexistent").isEmpty());
}

void TestGameLibrary::emulatorInfoAndCounts()
{
    Fixture f;
    const QString mockGame = f.game("A.mock");
    f.game("B.mock");
    const QString altGame = f.game("C.alt");
    GameLibrary library(f.config.path(), f.settings, f.registry);
    QVERIFY(scan(library));
    const QVariantMap a = library.get(mockGame);
    QCOMPARE(a.value("emulatorId").toString(), QStringLiteral("mock"));
    QCOMPARE(a.value("emulatorName").toString(), QStringLiteral("Emulator mock"));
    QCOMPARE(a.value("platform").toString(), QStringLiteral("Test Platform"));
    QCOMPARE(a.value("emulatorOverridden").toBool(), false);
    QCOMPARE(library.get(altGame).value("emulatorId").toString(), QStringLiteral("alt"));
    QCOMPARE(f.registry->byId("mock").value("gameCount").toInt(), 2);
    QCOMPARE(f.registry->byId("alt").value("gameCount").toInt(), 1);
}

void TestGameLibrary::favoritesPersist()
{
    Fixture f;
    const QString path = f.game("Fav.mock");
    {
        GameLibrary library(f.config.path(), f.settings, f.registry);
        QVERIFY(scan(library));
        QSignalSpy favSpy(&library, &GameLibrary::favoriteChanged);
        QVERIFY(library.toggleFavorite(path));
        QCOMPARE(favSpy.count(), 1);
        QCOMPARE(library.favoriteCount(), 1);
        QCOMPARE(library.get(path).value("favorite").toBool(), true);
    }
    GameLibrary restarted(f.config.path(), f.settings, f.registry);
    QCOMPARE(restarted.favoriteCount(), 1);
    QVERIFY(scan(restarted));
    QCOMPARE(restarted.get(path).value("favorite").toBool(), true);
    QVERIFY(!restarted.toggleFavorite(path));
    QCOMPARE(restarted.favoriteCount(), 0);
}

void TestGameLibrary::recordLaunchAndRecentCap()
{
    Fixture f;
    const QString path = f.game("Played.mock");
    f.game("Other.mock");
    GameLibrary library(f.config.path(), f.settings, f.registry);
    QVERIFY(scan(library));
    QCOMPARE(library.mostRecentPath(), QString());
    library.recordLaunch(path);
    QVERIFY(library.get(path).value("lastPlayed").toDateTime().isValid());
    QCOMPARE(library.mostRecentPath(), path);

    for (int i = 0; i < 24; ++i)
        library.recordLaunch(QStringLiteral("/elsewhere/game%1.mock").arg(i));
    QCOMPARE(library.recentCount(), 20);
    // Pushed out of the capped list: no longer "played".
    QVERIFY(!library.get(path).value("lastPlayed").toDateTime().isValid());
    QCOMPARE(library.mostRecentPath(), QString()); // none of the recent paths exist in the model

    library.recordLaunch(path);
    {
        GameLibrary restarted(f.config.path(), f.settings, f.registry);
        QCOMPARE(restarted.recentCount(), 20);
        QVERIFY(scan(restarted));
        QCOMPARE(restarted.mostRecentPath(), path);
        QVERIFY(restarted.get(path).value("lastPlayed").toDateTime().isValid());
    }
}

void TestGameLibrary::removeFromLibraryKeepsFile()
{
    Fixture f;
    const QString path = f.game("Remove.mock");
    f.game("Keep.mock");
    {
        GameLibrary library(f.config.path(), f.settings, f.registry);
        QVERIFY(scan(library));
        QCOMPARE(library.model()->count(), 2);
        library.removeFromLibrary(path);
        QCOMPARE(library.model()->count(), 1);
        QCOMPARE(library.hiddenCount(), 1);
        QVERIFY(QFile::exists(path));
        QVERIFY(scan(library));
        QCOMPARE(library.model()->count(), 1);
    }
    GameLibrary restarted(f.config.path(), f.settings, f.registry);
    QVERIFY(scan(restarted));
    QCOMPARE(restarted.model()->count(), 1);
    QCOMPARE(restarted.hiddenCount(), 1);

    QSignalSpy finished(&restarted, &GameLibrary::scanFinished);
    restarted.restoreHiddenEntries();
    QCOMPARE(restarted.hiddenCount(), 0);
    QVERIFY(finished.wait(Timeout));
    QCOMPARE(restarted.model()->count(), 2);
}

void TestGameLibrary::emulatorOverride()
{
    Fixture f;
    const QString path = f.game("Override.mock");
    {
        GameLibrary library(f.config.path(), f.settings, f.registry);
        QVERIFY(scan(library));
        QCOMPARE(library.get(path).value("emulatorId").toString(), QStringLiteral("mock"));
        library.setEmulatorOverride(path, "does-not-exist");
        QCOMPARE(library.get(path).value("emulatorId").toString(), QStringLiteral("mock"));
        library.setEmulatorOverride(path, "alt");
        QCOMPARE(library.get(path).value("emulatorId").toString(), QStringLiteral("alt"));
        QCOMPARE(library.get(path).value("emulatorOverridden").toBool(), true);
        QCOMPARE(f.registry->byId("alt").value("gameCount").toInt(), 1);
        QCOMPARE(f.registry->byId("mock").value("gameCount").toInt(), 0);
    }
    GameLibrary restarted(f.config.path(), f.settings, f.registry);
    QVERIFY(scan(restarted));
    QCOMPARE(restarted.get(path).value("emulatorId").toString(), QStringLiteral("alt"));

    // The overriding emulator disappears from the configuration: fall back.
    testutils::writeEmulators(f.config.path(), {testutils::mockEntry("mock", {"mock"})});
    f.registry->reload();
    QCOMPARE(restarted.get(path).value("emulatorId").toString(), QStringLiteral("mock"));
    QCOMPARE(restarted.get(path).value("emulatorOverridden").toBool(), false);

    restarted.setEmulatorOverride(path, QString());
    QCOMPARE(restarted.get(path).value("emulatorId").toString(), QStringLiteral("mock"));
}

void TestGameLibrary::firstSeenPersists()
{
    Fixture f;
    const QString path = f.game("New.mock");
    QDateTime seen;
    {
        GameLibrary library(f.config.path(), f.settings, f.registry);
        QVERIFY(scan(library));
        seen = library.get(path).value("firstSeen").toDateTime();
        QVERIFY(seen.isValid());
    }
    const QJsonObject state = QJsonDocument::fromJson(testutils::readFile(f.config.filePath("library-state.json"))).object();
    QVERIFY(state.value("firstSeen").toObject().contains(path));
    GameLibrary restarted(f.config.path(), f.settings, f.registry);
    QVERIFY(scan(restarted));
    QCOMPARE(restarted.get(path).value("firstSeen").toDateTime(), seen);
}

void TestGameLibrary::corruptStateFile()
{
    Fixture f;
    f.game("Survivor.mock");
    QVERIFY(testutils::writeFile(f.config.filePath("library-state.json"), "\x01garbage{"));
    GameLibrary library(f.config.path(), f.settings, f.registry);
    QVERIFY(!library.stateError().isEmpty());
    QVERIFY(QFile::exists(f.config.filePath("library-state.json.corrupt")));
    QVERIFY(scan(library));
    QCOMPARE(library.model()->count(), 1);

    // Wrong types inside an otherwise valid file are ignored.
    QVERIFY(testutils::writeFile(f.config.filePath("library-state.json"),
                                 R"({"favorites": 5, "recent": [1, {"path": 2}], "overrides": [], "hidden": {}})"));
    GameLibrary tolerant(f.config.path(), f.settings, f.registry);
    QVERIFY(tolerant.stateError().isEmpty());
    QVERIFY(scan(tolerant));
    QCOMPARE(tolerant.model()->count(), 1);
}

void TestGameLibrary::refreshWhileScanningQueues()
{
    Fixture f;
    for (int i = 0; i < 50; ++i)
        f.game(QStringLiteral("dir%1/Game%1.mock").arg(i));
    GameLibrary library(f.config.path(), f.settings, f.registry);
    QSignalSpy finished(&library, &GameLibrary::scanFinished);
    QSignalSpy scanningChanged(&library, &GameLibrary::scanningChanged);
    library.refresh();
    QVERIFY(library.scanning());
    library.refresh(); // queued
    library.refresh(); // still just one queued rescan
    QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 2, Timeout);
    QVERIFY(!library.scanning());
    QTest::qWait(100);
    QCOMPARE(finished.count(), 2);
    QCOMPARE(library.model()->count(), 50);
}

QTEST_GUILESS_MAIN(TestGameLibrary)
#include "tst_gamelibrary.moc"
