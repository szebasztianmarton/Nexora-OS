// Launch profile parsing (ConfigLoader) and launch resolution (EmulatorManager::resolve).
#include "nexora/emulator/EmulatorManager.h"
#include "nexora/shared/ConfigLoader.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

using namespace nexora::emulator;
using nexora::shared::ConfigLoader;
using nexora::shared::EmulatorConfig;

class TestLaunchProfiles : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_dir;
    QString m_exe;  // an executable file
    QString m_game; // a readable game file

    static QByteArray quoted(QByteArray json) { return json.replace('\'', '"'); }

    std::optional<EmulatorConfig> parse(const QByteArray &json, QStringList *errors,
                                        QStringList *warnings = nullptr)
    {
        return ConfigLoader::parse(quoted(json), m_dir.filePath(QStringLiteral("cfg/emu.json")), errors, warnings);
    }

    EmulatorConfig config()
    {
        QStringList errors;
        auto c = parse("{'id':'emu','name':'Emu','executable':'" + m_exe.toUtf8()
                           + "','arguments':['--base','{file}'],"
                             "'profiles':[{'id':'fast','name':'Fast','arguments':['--fast','--rom={file}']},"
                             "{'id':'plain','name':'Plain'},"
                             "{'id':'here','name':'Here','workingDirectory':'" + m_dir.path().toUtf8() + "'},"
                             "{'id':'off','name':'Off','enabled':false},"
                             "{'id':'nowhere','name':'Nowhere','workingDirectory':'/nonexistent/dir'}]}",
                       &errors);
        if (!c)
            qWarning() << errors;
        return c.value_or(EmulatorConfig{});
    }

private slots:
    void initTestCase()
    {
        QVERIFY(m_dir.isValid());
        m_exe = m_dir.filePath(QStringLiteral("bin/emu"));
        QDir().mkpath(m_dir.filePath(QStringLiteral("bin")));
        QFile exe(m_exe);
        QVERIFY(exe.open(QIODevice::WriteOnly));
        exe.write("#!/bin/sh\n");
        exe.close();
        QVERIFY(exe.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        m_game = m_dir.filePath(QStringLiteral("Super Game.sfc"));
        QFile game(m_game);
        QVERIFY(game.open(QIODevice::WriteOnly));
        game.write("rom");
    }

    void parsesProfilesAndOptions()
    {
        QStringList errors;
        const auto c = parse("{'id':'emu','name':'Emu','executable':'../bin/emu','workingDirectory':'work',"
                             "'extensions':['.SFC','.smc','.sfc'],'stopTimeoutMs':1500,"
                             "'profiles':[{'id':'p1','name':' Fast ','emulator':'emu','arguments':[]}]}",
                             &errors);
        QVERIFY2(c.has_value(), qPrintable(errors.join(u'\n')));
        QCOMPARE(c->executable, m_dir.filePath(QStringLiteral("bin/emu")));
        QCOMPARE(c->workingDirectory, m_dir.filePath(QStringLiteral("cfg/work")));
        QCOMPARE(c->extensions, QStringList({QStringLiteral(".sfc"), QStringLiteral(".smc")}));
        QCOMPARE(c->stopTimeoutMs, std::optional<int>(1500));
        QCOMPARE(c->profiles.size(), 1);
        const auto &p = c->profiles.first();
        QCOMPARE(p.id, QStringLiteral("p1"));
        QCOMPARE(p.name, QStringLiteral("Fast"));
        QCOMPARE(p.emulatorId, QStringLiteral("emu"));
        QVERIFY(p.hasArguments);
        QVERIFY(p.arguments.isEmpty());
        QVERIFY(p.enabled);
    }

    void mvp1FormatStillParses()
    {
        QStringList errors;
        const auto c = parse("{'id':'old','name':'Old','executable':'/bin/true','arguments':[],'enabled':true}", &errors);
        QVERIFY(c.has_value());
        QVERIFY(c->profiles.isEmpty());
        QVERIFY(c->extensions.isEmpty());
        QVERIFY(c->workingDirectory.isEmpty());
        QVERIFY(!c->stopTimeoutMs.has_value());
    }

    void rejectsInvalidProfiles_data()
    {
        QTest::addColumn<QByteArray>("profiles");
        QTest::addColumn<QString>("expected");
        QTest::newRow("not array") << QByteArray("{}") << QStringLiteral("\"profiles\" must be an array");
        QTest::newRow("not object") << QByteArray("[5]") << QStringLiteral("must be an object");
        QTest::newRow("bad id") << QByteArray("[{'id':'Bad Id','name':'x'}]") << QStringLiteral(".id must be a valid id");
        QTest::newRow("duplicate id") << QByteArray("[{'id':'a','name':'x'},{'id':'a','name':'y'}]")
                                      << QStringLiteral("is used by another profile");
        QTest::newRow("no name") << QByteArray("[{'id':'a'}]") << QStringLiteral(".name must be a non-empty string");
        QTest::newRow("wrong emulator") << QByteArray("[{'id':'a','name':'x','emulator':'other'}]")
                                        << QStringLiteral(".emulator must match");
        QTest::newRow("bad args") << QByteArray("[{'id':'a','name':'x','arguments':[1]}]")
                                  << QStringLiteral(".arguments[0] must be a string");
        QTest::newRow("bad enabled") << QByteArray("[{'id':'a','name':'x','enabled':'no'}]")
                                     << QStringLiteral(".enabled must be a boolean");
        QTest::newRow("bad workdir") << QByteArray("[{'id':'a','name':'x','workingDirectory':''}]")
                                     << QStringLiteral(".workingDirectory must be a non-empty string");
    }

    void rejectsInvalidProfiles()
    {
        QFETCH(QByteArray, profiles);
        QFETCH(QString, expected);
        QStringList errors;
        QVERIFY(!parse("{'id':'emu','name':'Emu','executable':'/bin/true','profiles':" + profiles + "}", &errors));
        QVERIFY2(errors.join(u'\n').contains(expected), qPrintable(errors.join(u'\n')));
    }

    void rejectsInvalidOptions_data()
    {
        QTest::addColumn<QByteArray>("extra");
        QTest::newRow("ext") << QByteArray("'extensions':['sfc']");
        QTest::newRow("ext type") << QByteArray("'extensions':'.sfc'");
        QTest::newRow("timeout low") << QByteArray("'stopTimeoutMs':10");
        QTest::newRow("timeout high") << QByteArray("'stopTimeoutMs':999999");
        QTest::newRow("timeout type") << QByteArray("'stopTimeoutMs':'5'");
        QTest::newRow("workdir type") << QByteArray("'workingDirectory':5");
    }

    void rejectsInvalidOptions()
    {
        QFETCH(QByteArray, extra);
        QStringList errors;
        QVERIFY(!parse("{'id':'emu','name':'Emu','executable':'/bin/true'," + extra + "}", &errors));
        QVERIFY(!errors.isEmpty());
    }

    void resolvesDefaultsWithoutGame()
    {
        EmulatorError error = EmulatorError::Crashed;
        const auto r = EmulatorManager::resolve({config(), {}, {}, {}}, &error, nullptr);
        QVERIFY(r);
        QCOMPARE(error, EmulatorError::None);
        QCOMPARE(r->program, m_exe);
        QCOMPARE(r->arguments, QStringList{QStringLiteral("--base")}); // {file} dropped
        QCOMPARE(r->workingDirectory, QFileInfo(m_exe).absolutePath());
        QCOMPARE(r->displayName, QStringLiteral("Emu"));
    }

    void substitutesGamePath()
    {
        const auto base = EmulatorManager::resolve({config(), {}, m_game, QStringLiteral("Super Game")}, nullptr, nullptr);
        QVERIFY(base);
        QCOMPARE(base->arguments, QStringList({QStringLiteral("--base"), m_game}));
        QCOMPARE(base->displayName, QStringLiteral("Super Game"));

        const auto fast = EmulatorManager::resolve({config(), QStringLiteral("fast"), m_game, {}}, nullptr, nullptr);
        QVERIFY(fast);
        QCOMPARE(fast->arguments, QStringList({QStringLiteral("--fast"), QStringLiteral("--rom=") + m_game}));

        // A profile without its own arguments inherits the emulator's.
        const auto plain = EmulatorManager::resolve({config(), QStringLiteral("plain"), m_game, {}}, nullptr, nullptr);
        QVERIFY(plain);
        QCOMPARE(plain->arguments, QStringList({QStringLiteral("--base"), m_game}));
    }

    void appendsGamePathWithoutPlaceholder()
    {
        EmulatorConfig c = config();
        c.arguments = {QStringLiteral("-v")};
        const auto r = EmulatorManager::resolve({c, {}, m_game, {}}, nullptr, nullptr);
        QVERIFY(r);
        QCOMPARE(r->arguments, QStringList({QStringLiteral("-v"), m_game}));
    }

    void profileWorkingDirectory()
    {
        const auto r = EmulatorManager::resolve({config(), QStringLiteral("here"), {}, {}}, nullptr, nullptr);
        QVERIFY(r);
        QCOMPARE(r->workingDirectory, m_dir.path());
    }

    void stopTimeoutComesFromConfig()
    {
        EmulatorConfig c = config();
        c.stopTimeoutMs = 750;
        const auto r = EmulatorManager::resolve({c, {}, {}, {}}, nullptr, nullptr);
        QVERIFY(r);
        QCOMPARE(r->stopTimeout, std::optional<std::chrono::milliseconds>(std::chrono::milliseconds(750)));
    }

    void rejectsInvalidRequests_data()
    {
        QTest::addColumn<QString>("profile");
        QTest::addColumn<QString>("game");
        QTest::addColumn<EmulatorError>("expected");
        QTest::newRow("unknown profile") << QStringLiteral("nope") << QString() << EmulatorError::ProfileNotFound;
        QTest::newRow("disabled profile") << QStringLiteral("off") << QString() << EmulatorError::ProfileDisabled;
        QTest::newRow("missing workdir") << QStringLiteral("nowhere") << QString()
                                         << EmulatorError::InvalidWorkingDirectory;
        QTest::newRow("missing game") << QString() << QStringLiteral("/nonexistent/game.sfc")
                                      << EmulatorError::GameFileMissing;
        QTest::newRow("relative game") << QString() << QStringLiteral("game.sfc") << EmulatorError::GameFileMissing;
        QTest::newRow("game is dir") << QString() << QStringLiteral("/tmp") << EmulatorError::GameFileMissing;
    }

    void rejectsInvalidRequests()
    {
        QFETCH(QString, profile);
        QFETCH(QString, game);
        QFETCH(EmulatorError, expected);
        EmulatorError error = EmulatorError::None;
        QString detail;
        QVERIFY(!EmulatorManager::resolve({config(), profile, game, {}}, &error, &detail));
        QCOMPARE(error, expected);
        QVERIFY(!describe(error, detail).isEmpty());
    }

    void invalidRequestFailsLaunchWithoutProcess()
    {
        EmulatorManager manager;
        const auto result = manager.launch(LaunchRequest{config(), QStringLiteral("off"), {}, {}});
        QVERIFY(!result.accepted);
        QCOMPARE(result.error, EmulatorError::ProfileDisabled);
        QCOMPARE(manager.state(), ProcessState::Failed);
        QVERIFY(!manager.isActive());
    }
};

QTEST_GUILESS_MAIN(TestLaunchProfiles)
#include "tst_launchprofiles.moc"
