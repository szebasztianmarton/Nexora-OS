#include "nexora/shared/DataLayout.h"

#include <QDir>
#include <QFile>
#include <QSet>
#include <QTemporaryDir>
#include <QTest>

using nexora::shared::DataLayout;

// Qt reports the owner bits twice (Owner and User flags); ignore the duplicate.
static constexpr QFileDevice::Permissions userBits
    = QFileDevice::ReadUser | QFileDevice::WriteUser | QFileDevice::ExeUser;

class TestDataLayout : public QObject
{
    Q_OBJECT

private:
    static QStringList all(const DataLayout &l)
    {
        return {l.configDir, l.dataDir, l.savesDir,       l.emulatorConfigDir, l.cacheDir,
                l.stateDir,  l.logDir,  l.diagnosticsDir, l.backupDir};
    }

private slots:
    void underRootProducesNineAbsoluteDistinctDirs()
    {
        QTemporaryDir root;
        const DataLayout l = DataLayout::underRoot(root.path());
        const QStringList dirs = all(l);
        QCOMPARE(dirs.size(), 9);
        QCOMPARE(QSet<QString>(dirs.cbegin(), dirs.cend()).size(), 9);
        for (const QString &d : dirs) {
            QVERIFY2(QDir::isAbsolutePath(d), qPrintable(d));
            QVERIFY2(d.startsWith(root.path()), qPrintable(d));
        }
    }

    void fromEnvironmentHonoursXdg()
    {
        qputenv("XDG_CONFIG_HOME", "/tmp/nx-xdg/config");
        qputenv("XDG_DATA_HOME", "/tmp/nx-xdg/data");
        qputenv("XDG_CACHE_HOME", "/tmp/nx-xdg/cache");
        qputenv("XDG_STATE_HOME", "/tmp/nx-xdg/state");
        const DataLayout l = DataLayout::fromEnvironment();
        QCOMPARE(l.configDir, QStringLiteral("/tmp/nx-xdg/config/nexora"));
        QCOMPARE(l.dataDir, QStringLiteral("/tmp/nx-xdg/data/nexora"));
        QCOMPARE(l.savesDir, QStringLiteral("/tmp/nx-xdg/data/nexora/saves"));
        QCOMPARE(l.emulatorConfigDir, QStringLiteral("/tmp/nx-xdg/data/nexora/emulators"));
        QCOMPARE(l.cacheDir, QStringLiteral("/tmp/nx-xdg/cache/nexora"));
        QCOMPARE(l.stateDir, QStringLiteral("/tmp/nx-xdg/state/nexora"));
        QCOMPARE(l.logDir, QStringLiteral("/tmp/nx-xdg/state/nexora/logs"));
        QCOMPARE(l.diagnosticsDir, QStringLiteral("/tmp/nx-xdg/state/nexora/diagnostics"));
        QCOMPARE(l.backupDir, QStringLiteral("/tmp/nx-xdg/data/nexora/backups"));
    }

    void stateFallsBackToHomeLocalState()
    {
        qunsetenv("XDG_STATE_HOME");
        const DataLayout l = DataLayout::fromEnvironment();
        QCOMPARE(l.stateDir, QDir::homePath() + QStringLiteral("/.local/state/nexora"));
        for (const QString &d : all(l))
            QVERIFY2(QDir::isAbsolutePath(d), qPrintable(d));
    }

    void relativeXdgValuesAreIgnored()
    {
        qputenv("XDG_STATE_HOME", "relative/state");
        const DataLayout l = DataLayout::fromEnvironment();
        QVERIFY(QDir::isAbsolutePath(l.stateDir));
        QCOMPARE(l.stateDir, QDir::homePath() + QStringLiteral("/.local/state/nexora"));
        qunsetenv("XDG_STATE_HOME");
    }

    void ensureDirectoriesCreatesOwnerOnlyDirs()
    {
        QTemporaryDir root;
        const DataLayout l = DataLayout::underRoot(root.path());
        QString error;
        QVERIFY2(l.ensureDirectories(&error), qPrintable(error));
        const QFileDevice::Permissions ownerOnly
            = QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner;
        for (const QString &d : all(l)) {
            QVERIFY2(QDir(d).exists(), qPrintable(d));
            QCOMPARE(QFile::permissions(d) & ~userBits, ownerOnly);
        }
        // Idempotent.
        QVERIFY(l.ensureDirectories(&error));
    }

    void ensureDirectoriesReportsFailure()
    {
        QTemporaryDir root;
        QFile blocker(root.filePath(QStringLiteral("blocked")));
        QVERIFY(blocker.open(QIODevice::WriteOnly));
        blocker.close();
        const DataLayout l = DataLayout::underRoot(root.filePath(QStringLiteral("blocked")));
        QString error;
        QVERIFY(!l.ensureDirectories(&error));
        QVERIFY(!error.isEmpty());
    }

    void cacheOnlyDirsExcludesUserData()
    {
        QTemporaryDir root;
        const DataLayout l = DataLayout::underRoot(root.path());
        const QStringList cache = l.cacheOnlyDirs();
        QVERIFY(cache.contains(l.cacheDir));
        QVERIFY(!cache.contains(l.savesDir));
        QVERIFY(!cache.contains(l.dataDir));
        QVERIFY(!cache.contains(l.configDir));
    }
};

QTEST_GUILESS_MAIN(TestDataLayout)
#include "tst_datalayout.moc"
