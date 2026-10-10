#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include "appsettings.h"
#include "testutils.h"

using nexora::AppSettings;

class TestAppSettings : public QObject
{
    Q_OBJECT

private slots:
    void defaults();
    void persistsAcrossInstances();
    void invalidFieldFallsBack();
    void corruptFile();
    void interfaceScaleClamped();
    void rejectsInvalidChoices();
    void addRemoveLibraryDirectory();
    void restoreDefaults();
};

void TestAppSettings::defaults()
{
    QTemporaryDir dir;
    AppSettings s(dir.path());
    QCOMPARE(s.firstRunCompleted(), false);
    QCOMPARE(s.language(), QStringLiteral("en"));
    QCOMPARE(s.theme(), QStringLiteral("dark"));
    QCOMPARE(s.interfaceScale(), 1.0);
    QCOMPARE(s.defaultSort(), QStringLiteral("title-asc"));
    QCOMPARE(s.reduceMotion(), false);
    QCOMPARE(s.fullscreen(), true);
    QCOMPARE(s.sidebarCompact(), false);
    QVERIFY(s.libraryDirectories().isEmpty());
    QVERIFY(s.loadError().isEmpty());
    QCOMPARE(s.filePath(), dir.filePath(QStringLiteral("settings.json")));
    QCOMPARE(AppSettings::validSortKeys(),
             QStringList({"title-asc", "title-desc", "recent", "added"}));
}

void TestAppSettings::persistsAcrossInstances()
{
    QTemporaryDir dir;
    QTemporaryDir lib;
    {
        AppSettings s(dir.path());
        QSignalSpy changed(&s, &AppSettings::changed);
        s.setFirstRunCompleted(true);
        s.setInterfaceScale(1.2);
        s.setDefaultSort(QStringLiteral("recent"));
        s.setReduceMotion(true);
        s.setFullscreen(false);
        s.setSidebarCompact(true);
        QVERIFY(s.addLibraryDirectory(lib.path()).isEmpty());
        QCOMPARE(changed.count(), 7);
        s.setReduceMotion(true); // unchanged: no signal
        QCOMPARE(changed.count(), 7);
    }
    AppSettings s(dir.path());
    QCOMPARE(s.firstRunCompleted(), true);
    QCOMPARE(s.interfaceScale(), 1.2);
    QCOMPARE(s.defaultSort(), QStringLiteral("recent"));
    QCOMPARE(s.reduceMotion(), true);
    QCOMPARE(s.fullscreen(), false);
    QCOMPARE(s.sidebarCompact(), true);
    QCOMPARE(s.libraryDirectories(), QStringList{QDir(lib.path()).absolutePath()});
}

void TestAppSettings::invalidFieldFallsBack()
{
    QTemporaryDir dir;
    const QJsonObject obj{{"firstRunCompleted", true},
                          {"defaultSort", "bogus"},
                          {"interfaceScale", "huge"},
                          {"reduceMotion", "yes"},
                          {"fullscreen", false},
                          {"language", 42},
                          {"libraryDirectories", QJsonArray{"/srv/games", 7, "relative/path"}}};
    QVERIFY(testutils::writeFile(dir.filePath("settings.json"), QJsonDocument(obj).toJson()));
    AppSettings s(dir.path());
    QVERIFY(s.loadError().isEmpty());
    QCOMPARE(s.firstRunCompleted(), true);
    QCOMPARE(s.fullscreen(), false);
    QCOMPARE(s.defaultSort(), QStringLiteral("title-asc"));
    QCOMPARE(s.interfaceScale(), 1.0);
    QCOMPARE(s.reduceMotion(), false);
    QCOMPARE(s.language(), QStringLiteral("en"));
    QCOMPARE(s.libraryDirectories(), QStringList{"/srv/games"});
}

void TestAppSettings::corruptFile()
{
    QTemporaryDir dir;
    const QString path = dir.filePath("settings.json");
    QVERIFY(testutils::writeFile(path, "{ not json"));
    AppSettings s(dir.path());
    QVERIFY(!s.loadError().isEmpty());
    QCOMPARE(s.defaultSort(), QStringLiteral("title-asc"));
    QVERIFY(QFile::exists(path + ".corrupt"));
    QCOMPARE(testutils::readFile(path + ".corrupt"), QByteArray("{ not json"));

    s.setReduceMotion(true); // saving writes a fresh, valid file
    AppSettings reloaded(dir.path());
    QVERIFY(reloaded.loadError().isEmpty());
    QCOMPARE(reloaded.reduceMotion(), true);
}

void TestAppSettings::interfaceScaleClamped()
{
    QTemporaryDir dir;
    QVERIFY(testutils::writeFile(dir.filePath("settings.json"), R"({"interfaceScale": 5})"));
    AppSettings s(dir.path());
    QCOMPARE(s.interfaceScale(), 1.3);
    s.setInterfaceScale(0.1);
    QCOMPARE(s.interfaceScale(), 0.8);
}

void TestAppSettings::rejectsInvalidChoices()
{
    QTemporaryDir dir;
    AppSettings s(dir.path());
    QSignalSpy changed(&s, &AppSettings::changed);
    s.setDefaultSort(QStringLiteral("random"));
    s.setLanguage(QStringLiteral("xx"));
    s.setTheme(QStringLiteral("neon"));
    QCOMPARE(changed.count(), 0);
    QCOMPARE(s.defaultSort(), QStringLiteral("title-asc"));
}

void TestAppSettings::addRemoveLibraryDirectory()
{
    QTemporaryDir dir;
    QTemporaryDir lib;
    QVERIFY(testutils::writeFile(lib.filePath("keep.mock")));
    AppSettings s(dir.path());
    QSignalSpy dirsChanged(&s, &AppSettings::libraryDirectoriesChanged);

    QVERIFY(!s.addLibraryDirectory(lib.filePath("does-not-exist")).isEmpty());
    QVERIFY(!s.addLibraryDirectory(lib.filePath("keep.mock")).isEmpty()); // a file
    QVERIFY(!s.addLibraryDirectory(QString()).isEmpty());
    QCOMPARE(dirsChanged.count(), 0);

    QCOMPARE(s.addLibraryDirectory(lib.path() + "/./"), QString());
    QCOMPARE(s.libraryDirectories(), QStringList{QDir::cleanPath(lib.path())});
    QCOMPARE(s.addLibraryDirectory(lib.path()), QStringLiteral("Already configured"));
    QCOMPARE(dirsChanged.count(), 1);

    QVERIFY(!s.removeLibraryDirectory(lib.filePath("other")));
    QVERIFY(s.removeLibraryDirectory(lib.path()));
    QVERIFY(s.libraryDirectories().isEmpty());
    QCOMPARE(dirsChanged.count(), 2);
    QVERIFY(QFile::exists(lib.filePath("keep.mock"))); // nothing on disk touched
}

void TestAppSettings::restoreDefaults()
{
    QTemporaryDir dir;
    QTemporaryDir lib;
    AppSettings s(dir.path());
    s.setFirstRunCompleted(true);
    s.setSidebarCompact(true);
    s.setInterfaceScale(0.9);
    s.addLibraryDirectory(lib.path());

    QSignalSpy dirsChanged(&s, &AppSettings::libraryDirectoriesChanged);
    s.restoreDefaults();
    QCOMPARE(s.sidebarCompact(), false);
    QCOMPARE(s.interfaceScale(), 1.0);
    QCOMPARE(s.firstRunCompleted(), true);
    QCOMPARE(s.libraryDirectories().size(), 1);
    QCOMPARE(dirsChanged.count(), 0);

    s.restoreDefaults(true);
    QCOMPARE(s.firstRunCompleted(), false);
    QVERIFY(s.libraryDirectories().isEmpty());
    QCOMPARE(dirsChanged.count(), 1);

    AppSettings reloaded(dir.path());
    QCOMPARE(reloaded.firstRunCompleted(), false);
    QVERIFY(reloaded.libraryDirectories().isEmpty());
}

QTEST_GUILESS_MAIN(TestAppSettings)
#include "tst_appsettings.moc"
