// LibraryScanner against real files in a temporary directory.
#include "nexora/library/LibraryScanner.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

#include <unistd.h>

using namespace nexora::library;

class TestLibraryScanner : public QObject
{
    Q_OBJECT

private:
    static void touch(const QString &path, const QByteArray &content = "x")
    {
        QDir().mkpath(QFileInfo(path).absolutePath());
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(content);
    }

    static ScanOptions options(const QStringList &roots)
    {
        ScanOptions o;
        o.roots = roots;
        o.extensionMap = {{QStringLiteral(".sfc"), {QStringLiteral("snes")}},
                          {QStringLiteral(".nes"), {QStringLiteral("nes"), QStringLiteral("multi")}}};
        return o;
    }

    static QStringList names(const ScanResult &r)
    {
        QStringList n;
        for (const auto &f : r.files)
            n.append(QFileInfo(f.path).fileName());
        n.sort();
        return n;
    }

private slots:
    void findsConfiguredExtensionsOnly()
    {
        QTemporaryDir dir;
        touch(dir.filePath(QStringLiteral("a.sfc")));
        touch(dir.filePath(QStringLiteral("sub/deeper/B.SFC")));
        touch(dir.filePath(QStringLiteral("c.nes")));
        touch(dir.filePath(QStringLiteral("notes.txt")));
        touch(dir.filePath(QStringLiteral("noext")));
        touch(dir.filePath(QStringLiteral(".hidden.sfc")));
        touch(dir.filePath(QStringLiteral(".cache/x.sfc")));
        const auto r = LibraryScanner::scan(options({dir.path()}));
        QCOMPARE(names(r), QStringList({QStringLiteral("B.SFC"), QStringLiteral("a.sfc"), QStringLiteral("c.nes")}));
        QVERIFY(r.issues.isEmpty());
        for (const auto &f : r.files) {
            if (f.extension == QStringLiteral(".nes"))
                QCOMPARE(f.emulatorIds, QStringList({QStringLiteral("nes"), QStringLiteral("multi")}));
            QCOMPARE(f.root, QFileInfo(dir.path()).canonicalFilePath());
            QVERIFY(f.size > 0);
        }
    }

    void reportsBadRoots()
    {
        QTemporaryDir dir;
        touch(dir.filePath(QStringLiteral("file.sfc")));
        const auto r
            = LibraryScanner::scan(options({dir.filePath(QStringLiteral("missing")), QStringLiteral("relative/dir"),
                                            QStringLiteral("/"), dir.filePath(QStringLiteral("file.sfc"))}));
        QVERIFY(r.files.isEmpty());
        QCOMPARE(r.issues.size(), 4);
        QVERIFY(r.scannedRoots.isEmpty());
    }

    void skipsSymlinksByDefault()
    {
        QTemporaryDir dir;
        QTemporaryDir outside;
        touch(dir.filePath(QStringLiteral("lib/real.sfc")));
        touch(outside.filePath(QStringLiteral("secret.sfc")));
        QVERIFY(
            QFile::link(dir.filePath(QStringLiteral("lib/real.sfc")), dir.filePath(QStringLiteral("lib/alias.sfc"))));
        QVERIFY(QFile::link(outside.path(), dir.filePath(QStringLiteral("lib/escape"))));
        const auto r = LibraryScanner::scan(options({dir.filePath(QStringLiteral("lib"))}));
        QCOMPARE(names(r), QStringList{QStringLiteral("real.sfc")});
        QCOMPARE(r.skippedSymlinks, 2);
    }

    void followsOnlyLinksInsideTheRoot()
    {
        QTemporaryDir dir;
        QTemporaryDir outside;
        touch(dir.filePath(QStringLiteral("lib/sub/inner.sfc")));
        touch(outside.filePath(QStringLiteral("secret.sfc")));
        QVERIFY(QFile::link(dir.filePath(QStringLiteral("lib/sub")), dir.filePath(QStringLiteral("lib/sublink"))));
        QVERIFY(QFile::link(outside.path(), dir.filePath(QStringLiteral("lib/escape"))));
        QVERIFY(QFile::link(outside.filePath(QStringLiteral("secret.sfc")),
                            dir.filePath(QStringLiteral("lib/secret.sfc"))));
        QVERIFY(QFile::link(dir.filePath(QStringLiteral("lib")), dir.filePath(QStringLiteral("lib/sub/loop"))));
        QVERIFY(
            QFile::link(dir.filePath(QStringLiteral("nowhere.sfc")), dir.filePath(QStringLiteral("lib/broken.sfc"))));
        auto o = options({dir.filePath(QStringLiteral("lib"))});
        o.followSymlinks = true;
        const auto r = LibraryScanner::scan(o);
        // inner.sfc found once (via sub/ and sublink/ it is the same file), escapes refused,
        // the loop back to lib/ terminates and is counted.
        QCOMPARE(names(r), QStringList{QStringLiteral("inner.sfc")});
        QCOMPARE(r.skippedSymlinks, 4);
        bool sawOutsideIssue = false;
        bool sawBrokenIssue = false;
        for (const auto &i : r.issues) {
            sawOutsideIssue |= i.message.contains(QStringLiteral("outside the library"));
            sawBrokenIssue |= i.message.contains(QStringLiteral("broken symbolic link"));
        }
        QVERIFY(sawOutsideIssue);
        QVERIFY(sawBrokenIssue);
    }

    void detectsDuplicates()
    {
        QTemporaryDir dir;
        touch(dir.filePath(QStringLiteral("all/snes/game.sfc")));
        QVERIFY(::link(QFile::encodeName(dir.filePath(QStringLiteral("all/snes/game.sfc"))).constData(),
                       QFile::encodeName(dir.filePath(QStringLiteral("all/hardlink.sfc"))).constData())
                == 0);
        // Overlapping roots: all/ contains all/snes/.
        const auto r = LibraryScanner::scan(
            options({dir.filePath(QStringLiteral("all")), dir.filePath(QStringLiteral("all/snes"))}));
        QCOMPARE(r.files.size(), 1);
        QCOMPARE(r.duplicates, 2);
    }

    void skipsUnreadableEntries()
    {
        if (::geteuid() == 0)
            QSKIP("root can read everything");
        QTemporaryDir dir;
        touch(dir.filePath(QStringLiteral("ok.sfc")));
        touch(dir.filePath(QStringLiteral("locked.sfc")));
        touch(dir.filePath(QStringLiteral("closed/inside.sfc")));
        QVERIFY(QFile::setPermissions(dir.filePath(QStringLiteral("locked.sfc")), QFile::WriteOwner));
        QVERIFY(QFile::setPermissions(dir.filePath(QStringLiteral("closed")), QFile::WriteOwner));
        const auto r = LibraryScanner::scan(options({dir.path()}));
        QFile::setPermissions(dir.filePath(QStringLiteral("closed")),
                              QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
        QCOMPARE(names(r), QStringList{QStringLiteral("ok.sfc")});
        QCOMPARE(r.issues.size(), 2);
    }

    void honoursLimits()
    {
        QTemporaryDir dir;
        for (int i = 0; i < 10; ++i)
            touch(dir.filePath(QStringLiteral("g%1.sfc").arg(i)));
        touch(dir.filePath(QStringLiteral("a/b/c/deep.sfc")));
        auto o = options({dir.path()});
        o.maxFiles = 4;
        auto r = LibraryScanner::scan(o);
        QVERIFY(r.truncated);
        QCOMPARE(r.files.size(), 4);

        o = options({dir.path()});
        o.maxDepth = 1;
        r = LibraryScanner::scan(o);
        QVERIFY(!names(r).contains(QStringLiteral("deep.sfc")));
        QCOMPARE(r.files.size(), 10);
    }

    void symlinkLoopTerminatesAndIsCounted()
    {
        QTemporaryDir dir;
        touch(dir.filePath(QStringLiteral("a/game.sfc")));
        QVERIFY(QFile::link(dir.path(), dir.filePath(QStringLiteral("a/link"))));
        auto o = options({dir.path()});
        o.followSymlinks = true;
        const auto r = LibraryScanner::scan(o);
        QCOMPARE(names(r), QStringList{QStringLiteral("game.sfc")});
        QCOMPARE(r.skippedSymlinks, 1);
    }

    void ambiguousExtensionKeepsAllSystems()
    {
        QTemporaryDir dir;
        touch(dir.filePath(QStringLiteral("disc.iso")));
        touch(dir.filePath(QStringLiteral("zelda.sfc")));
        touch(dir.filePath(QStringLiteral("other.nes")));
        auto o = options({dir.path()});
        o.extensionMap.insert(QStringLiteral(".iso"), {QStringLiteral("multi")});
        o.extensionSystems = {{QStringLiteral(".iso"), {QStringLiteral("ps1"), QStringLiteral("saturn")}},
                              {QStringLiteral(".sfc"), {QStringLiteral("snes")}}};
        const auto r = LibraryScanner::scan(o);
        QCOMPARE(r.files.size(), 3);
        for (const auto &f : r.files) {
            if (f.extension == QStringLiteral(".iso"))
                QVERIFY(f.systemIds.size() >= 2);
            else if (f.extension == QStringLiteral(".sfc"))
                QCOMPARE(f.systemIds, QStringList{QStringLiteral("snes")});
            else
                QVERIFY(f.systemIds.isEmpty()); // not in the system map
        }
    }

    void doesNotEnterSystemDirectories()
    {
        QTemporaryDir dir;
        touch(dir.filePath(QStringLiteral("ok.sfc")));
        touch(dir.filePath(QStringLiteral("proc/a.sfc")));
        touch(dir.filePath(QStringLiteral("sub/sys/b.sfc")));
        touch(dir.filePath(QStringLiteral("lost+found/c.sfc")));
        touch(dir.filePath(QStringLiteral(".Trash-1000/d.sfc")));
        touch(dir.filePath(QStringLiteral("run/e.sfc")));
        touch(dir.filePath(QStringLiteral("dev/f.sfc")));
        touch(dir.filePath(QStringLiteral("procs/g.sfc"))); // only exact names
        const auto r = LibraryScanner::scan(options({dir.path()}));
        QCOMPARE(names(r), QStringList({QStringLiteral("g.sfc"), QStringLiteral("ok.sfc")}));
    }

    void refusesSystemLocationsAsRoots()
    {
        const auto r = LibraryScanner::scan(options({QStringLiteral("/usr"), QStringLiteral("/etc")}));
        QVERIFY(r.files.isEmpty());
        QVERIFY(r.scannedRoots.isEmpty());
        QCOMPARE(r.issues.size(), 2);
    }

    void reportsProgress()
    {
        QTemporaryDir dir;
        for (int i = 0; i < 250; ++i)
            touch(dir.filePath(QStringLiteral("g%1.sfc").arg(i)));
        QList<int> seen;
        QStringList roots;
        const auto r = LibraryScanner::scan(options({dir.path()}), nullptr, [&](int filesSeen, const QString &root) {
            seen.append(filesSeen);
            roots.append(root);
        });
        QCOMPARE(r.files.size(), 250);
        QCOMPARE(seen, QList<int>({0, 100, 200})); // at the root, then every 100 files
        QCOMPARE(roots.first(), QFileInfo(dir.path()).canonicalFilePath());
    }

    void canBeCancelled()
    {
        QTemporaryDir dir;
        touch(dir.filePath(QStringLiteral("a.sfc")));
        std::atomic_bool cancel{true};
        const auto r = LibraryScanner::scan(options({dir.path()}), &cancel);
        QVERIFY(r.cancelled);
        QVERIFY(r.files.isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestLibraryScanner)
#include "tst_libraryscanner.moc"
