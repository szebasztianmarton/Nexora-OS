#include "nexora/shared/Paths.h"

#include <QDir>
#include <QTest>

using namespace nexora::shared;

class TestPaths : public QObject
{
    Q_OBJECT

private slots:
    void commandLineWins()
    {
        const auto r
            = resolveConfigDirectory(QStringLiteral("/cli"), QStringLiteral("/env"), QStringLiteral("/app/bin"));
        QCOMPARE(r.path, QStringLiteral("/cli"));
        QVERIFY(r.source == ConfigDirSource::CommandLine);
    }

    void environmentBeatsDefault()
    {
        const auto r = resolveConfigDirectory({}, QStringLiteral("/env"), QStringLiteral("/app/bin"));
        QCOMPARE(r.path, QStringLiteral("/env"));
        QVERIFY(r.source == ConfigDirSource::Environment);
    }

    void defaultIsRelativeToApplication()
    {
        const auto r = resolveConfigDirectory({}, {}, QStringLiteral("/opt/nexora/bin"));
        QCOMPARE(r.path, QStringLiteral("/opt/nexora/share/nexora/emulators"));
        QVERIFY(r.source == ConfigDirSource::Default);
    }

    void relativeOverridesBecomeAbsolute()
    {
        const auto r = resolveConfigDirectory(QStringLiteral("cfg"), {}, QStringLiteral("/app/bin"));
        QVERIFY(QDir::isAbsolutePath(r.path));
        QVERIFY(r.path.endsWith(QStringLiteral("/cfg")));
    }

    void describesSources()
    {
        QVERIFY(toString(ConfigDirSource::CommandLine).contains(QStringLiteral("--config-dir")));
        QVERIFY(toString(ConfigDirSource::Environment).contains(QStringLiteral("NEXORA_CONFIG_DIR")));
        QVERIFY(!toString(ConfigDirSource::Default).isEmpty());
    }

    void stateDirectoryFollowsXdg()
    {
        const QByteArray saved = qgetenv("XDG_STATE_HOME");
        qputenv("XDG_STATE_HOME", "/srv/state");
        QCOMPARE(nexora::shared::defaultStateDirectory(), QStringLiteral("/srv/state/nexora"));
        qputenv("XDG_STATE_HOME", "relative/ignored"); // the XDG spec ignores relative paths
        QCOMPARE(nexora::shared::defaultStateDirectory(), QDir::homePath() + QStringLiteral("/.local/state/nexora"));
        qunsetenv("XDG_STATE_HOME");
        QCOMPARE(nexora::shared::defaultStateDirectory(), QDir::homePath() + QStringLiteral("/.local/state/nexora"));
        if (!saved.isNull())
            qputenv("XDG_STATE_HOME", saved);
    }
};

QTEST_GUILESS_MAIN(TestPaths)
#include "tst_paths.moc"
