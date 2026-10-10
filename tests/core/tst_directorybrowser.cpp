#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <unistd.h>

#include "directorybrowser.h"
#include "testutils.h"

using nexora::DirectoryBrowser;

class TestDirectoryBrowser : public QObject
{
    Q_OBJECT

private slots:
    void startsAtHome();
    void listsOnlyVisibleDirectories();
    void enterAndUp();
    void missingPathKeepsLocation();
    void unreadablePath();
    void locations();
};

void TestDirectoryBrowser::startsAtHome()
{
    DirectoryBrowser browser;
    QCOMPARE(browser.currentPath(), QDir::cleanPath(QDir::homePath()));
    QVERIFY(browser.error().isEmpty());
}

void TestDirectoryBrowser::listsOnlyVisibleDirectories()
{
    QTemporaryDir dir;
    QDir(dir.path()).mkpath("beta");
    QDir(dir.path()).mkpath("Alpha");
    QDir(dir.path()).mkpath("gamma/nested");
    QDir(dir.path()).mkpath(".hidden");
    testutils::writeFile(dir.filePath("file.mock"));

    DirectoryBrowser browser;
    QSignalSpy changed(&browser, &DirectoryBrowser::currentPathChanged);
    QVERIFY(browser.open(dir.path()));
    QCOMPARE(changed.count(), 1);
    QCOMPARE(browser.entries(), QStringList({"Alpha", "beta", "gamma"}));
}

void TestDirectoryBrowser::enterAndUp()
{
    QTemporaryDir dir;
    QDir(dir.path()).mkpath("gamma/nested");
    DirectoryBrowser browser;
    QVERIFY(browser.open(dir.path()));
    QVERIFY(browser.enter("gamma"));
    QCOMPARE(browser.currentPath(), dir.filePath("gamma"));
    QCOMPARE(browser.entries(), QStringList{"nested"});
    QVERIFY(!browser.enter("does-not-exist"));
    QVERIFY(!browser.error().isEmpty());
    QCOMPARE(browser.currentPath(), dir.filePath("gamma"));
    QVERIFY(!browser.enter("../.."));
    QVERIFY(browser.up());
    QCOMPARE(browser.currentPath(), QDir::cleanPath(dir.path()));
    QVERIFY(browser.error().isEmpty());

    QVERIFY(browser.open("/"));
    QVERIFY(!browser.canGoUp());
    QVERIFY(!browser.up());
    browser.home();
    QCOMPARE(browser.currentPath(), QDir::cleanPath(QDir::homePath()));
}

void TestDirectoryBrowser::missingPathKeepsLocation()
{
    QTemporaryDir dir;
    testutils::writeFile(dir.filePath("file.mock"));
    DirectoryBrowser browser;
    QVERIFY(browser.open(dir.path()));
    QVERIFY(!browser.open(dir.filePath("missing")));
    QCOMPARE(browser.currentPath(), QDir::cleanPath(dir.path()));
    QVERIFY(!browser.error().isEmpty());
    QVERIFY(!browser.open(dir.filePath("file.mock")));
    QCOMPARE(browser.error(), QStringLiteral("Not a folder"));
}

void TestDirectoryBrowser::unreadablePath()
{
    if (geteuid() == 0)
        QSKIP("Permission checks do not apply to root");
    QTemporaryDir dir;
    const QString locked = dir.filePath("locked");
    QDir().mkpath(locked);
    QFile::setPermissions(locked, QFileDevice::Permissions());
    DirectoryBrowser browser;
    QVERIFY(browser.open(dir.path()));
    QVERIFY(!browser.enter("locked"));
    QCOMPARE(browser.error(), QStringLiteral("Permission denied"));
    QCOMPARE(browser.currentPath(), QDir::cleanPath(dir.path()));
    QFile::setPermissions(locked, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
}

void TestDirectoryBrowser::locations()
{
    DirectoryBrowser browser;
    const QVariantList locations = browser.locations();
    QVERIFY(!locations.isEmpty());
    QCOMPARE(locations.first().toMap().value("name").toString(), QStringLiteral("Home"));
    QCOMPARE(locations.last().toMap().value("path").toString(), QStringLiteral("/"));
    for (const QVariant &v : locations)
        QVERIFY(QFileInfo(v.toMap().value("path").toString()).isDir());
}

QTEST_GUILESS_MAIN(TestDirectoryBrowser)
#include "tst_directorybrowser.moc"
