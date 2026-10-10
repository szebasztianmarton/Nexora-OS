#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include "emulatorregistry.h"
#include "testutils.h"

using nexora::EmulatorDefinition;
using nexora::EmulatorRegistry;
using nexora::EmulatorStatus;

class TestEmulatorRegistry : public QObject
{
    Q_OBJECT

private slots:
    void missingFile();
    void corruptFile();
    void validEntry();
    void roleNames();
    void missingExecutable();
    void notExecutable();
    void relativePathInvalid();
    void missingWorkingDirectory();
    void disabled();
    void structuralProblems();
    void entryWithoutIdSkipped();
    void duplicateIdSkipped();
    void buildArguments();
    void supportedExtensions();
    void emulatorForExtension();
    void validateDetectsChanges();
    void gameCounts();
};

void TestEmulatorRegistry::missingFile()
{
    QTemporaryDir dir;
    EmulatorRegistry r(dir.path());
    QCOMPARE(r.configExists(), false);
    QVERIFY(r.loadError().isEmpty());
    QCOMPARE(r.count(), 0);
    QVERIFY(r.supportedExtensions().isEmpty());
    QCOMPARE(r.configPath(), dir.filePath("emulators.json"));
}

void TestEmulatorRegistry::corruptFile()
{
    QTemporaryDir dir;
    QVERIFY(testutils::writeFile(dir.filePath("emulators.json"), "{\"emulators\": [ oops"));
    EmulatorRegistry r(dir.path());
    QCOMPARE(r.configExists(), true);
    QVERIFY(!r.loadError().isEmpty());
    QCOMPARE(r.count(), 0);
    // A hand-edited file is never moved away.
    QVERIFY(QFile::exists(dir.filePath("emulators.json")));
}

void TestEmulatorRegistry::validEntry()
{
    QTemporaryDir dir;
    QJsonObject entry = testutils::mockEntry("mock", {".MOCK", "m2"});
    entry["workingDirectory"] = dir.path();
    entry["profiles"] = QJsonArray{QJsonObject{{"id", "windowed"}, {"name", "Windowed"},
                                               {"arguments", QJsonArray{"--windowed", "{game}"}}}};
    QVERIFY(testutils::writeEmulators(dir.path(), {entry}));
    EmulatorRegistry r(dir.path());
    QVERIFY(r.loadError().isEmpty());
    QCOMPARE(r.count(), 1);
    QCOMPARE(r.availableCount(), 1);
    const EmulatorDefinition *def = r.find("mock");
    QVERIFY(def);
    QCOMPARE(def->status, EmulatorStatus::Available);
    QVERIFY(def->validationMessages.isEmpty());
    QCOMPARE(def->extensions, QStringList({"mock", "m2"}));
    QCOMPARE(def->platforms, QStringList{"Test Platform"});
    QCOMPARE(def->profiles.size(), 1);

    const QVariantMap map = r.get(0);
    QCOMPARE(map.value("id").toString(), QStringLiteral("mock"));
    QCOMPARE(map.value("status").toString(), QStringLiteral("available"));
    QCOMPARE(map.value("statusText").toString(), QStringLiteral("Available"));
    QCOMPARE(map.value("canLaunch").toBool(), true);
    QCOMPARE(map.value("profiles").toList().first().toMap().value("id").toString(), QStringLiteral("windowed"));
    QVERIFY(r.get(5).isEmpty());
    QCOMPARE(r.byId("mock").value("name").toString(), QStringLiteral("Emulator mock"));
    QCOMPARE(r.indexOf("mock"), 0);
    QCOMPARE(r.indexOf("nope"), -1);
}

void TestEmulatorRegistry::roleNames()
{
    QTemporaryDir dir;
    QVERIFY(testutils::writeEmulators(dir.path(), {testutils::mockEntry("mock")}));
    EmulatorRegistry r(dir.path());
    const QStringList expected{"id", "name", "executable", "arguments", "workingDirectory", "enabled",
                               "platforms", "extensions", "profiles", "status", "statusText",
                               "validationMessages", "canLaunch", "gameCount"};
    QStringList names;
    for (const QByteArray &n : r.roleNames())
        names.append(QString::fromUtf8(n));
    names.sort();
    QStringList sortedExpected = expected;
    sortedExpected.sort();
    QCOMPARE(names, sortedExpected);
    QCOMPARE(r.roleNames().value(EmulatorRegistry::GameCountRole), QByteArray("gameCount"));
    QCOMPARE(r.roleNames().value(EmulatorRegistry::StatusTextRole), QByteArray("statusText"));

    QStringList keys = r.get(0).keys();
    keys.sort();
    QCOMPARE(keys, sortedExpected);
    // Each role returns data for a valid index.
    const QHash<int, QByteArray> roles = r.roleNames();
    for (auto it = roles.cbegin(); it != roles.cend(); ++it)
        QVERIFY2(r.index(0).data(it.key()).isValid(), it.value().constData());
}

void TestEmulatorRegistry::missingExecutable()
{
    QTemporaryDir dir;
    QJsonObject entry = testutils::mockEntry("gone");
    entry["executable"] = dir.filePath("no-such-binary");
    QVERIFY(testutils::writeEmulators(dir.path(), {entry}));
    EmulatorRegistry r(dir.path());
    QCOMPARE(r.find("gone")->status, EmulatorStatus::ExecutableMissing);
    QVERIFY(!r.find("gone")->validationMessages.isEmpty());
    QCOMPARE(r.get(0).value("status").toString(), QStringLiteral("missing"));
    QCOMPARE(r.availableCount(), 0);
}

void TestEmulatorRegistry::notExecutable()
{
    QTemporaryDir dir;
    QVERIFY(testutils::writeFile(dir.filePath("plain.bin")));
    QJsonObject file = testutils::mockEntry("file");
    file["executable"] = dir.filePath("plain.bin");
    QJsonObject directory = testutils::mockEntry("dir");
    directory["executable"] = dir.path();
    QVERIFY(testutils::writeEmulators(dir.path(), {file, directory}));
    EmulatorRegistry r(dir.path());
    QCOMPARE(r.find("file")->status, EmulatorStatus::NotExecutable);
    QCOMPARE(r.find("dir")->status, EmulatorStatus::NotExecutable);
    QCOMPARE(r.get(0).value("status").toString(), QStringLiteral("not-executable"));
}

void TestEmulatorRegistry::relativePathInvalid()
{
    QTemporaryDir dir;
    QJsonObject entry = testutils::mockEntry("rel");
    entry["executable"] = "bin/emulator";
    QVERIFY(testutils::writeEmulators(dir.path(), {entry}));
    EmulatorRegistry r(dir.path());
    QCOMPARE(r.find("rel")->status, EmulatorStatus::InvalidConfiguration);
    QVERIFY(r.find("rel")->validationMessages.first().contains("absolute"));
}

void TestEmulatorRegistry::missingWorkingDirectory()
{
    QTemporaryDir dir;
    QJsonObject entry = testutils::mockEntry("wd");
    entry["workingDirectory"] = dir.filePath("nowhere");
    QVERIFY(testutils::writeEmulators(dir.path(), {entry}));
    EmulatorRegistry r(dir.path());
    QCOMPARE(r.find("wd")->status, EmulatorStatus::InvalidConfiguration);
}

void TestEmulatorRegistry::disabled()
{
    QTemporaryDir dir;
    QJsonObject ok = testutils::mockEntry("off");
    ok["enabled"] = false;
    QJsonObject broken = testutils::mockEntry("off-broken");
    broken["enabled"] = false;
    broken["executable"] = dir.filePath("missing");
    QVERIFY(testutils::writeEmulators(dir.path(), {ok, broken}));
    EmulatorRegistry r(dir.path());
    QCOMPARE(r.find("off")->status, EmulatorStatus::Disabled);
    QCOMPARE(r.find("off-broken")->status, EmulatorStatus::Disabled);
    QVERIFY(!r.find("off-broken")->validationMessages.isEmpty()); // problems still reported
    QVERIFY(!r.find("off")->canLaunch());
    QCOMPARE(r.availableCount(), 0);
}

void TestEmulatorRegistry::structuralProblems()
{
    QTemporaryDir dir;
    const QJsonObject entry{{"id", "bad"}, {"executable", testutils::mockEmulator()}, {"arguments", "not-a-list"},
                            {"enabled", "yes"}};
    QVERIFY(testutils::writeEmulators(dir.path(), {entry}));
    EmulatorRegistry r(dir.path());
    QCOMPARE(r.count(), 1);
    const EmulatorDefinition *def = r.find("bad");
    QCOMPARE(def->status, EmulatorStatus::InvalidConfiguration);
    QCOMPARE(def->name, QStringLiteral("bad")); // missing name falls back to id
    QVERIFY(def->validationMessages.size() >= 3);
    r.validateAll(); // structural problems survive re-validation
    QCOMPARE(r.find("bad")->status, EmulatorStatus::InvalidConfiguration);
}

void TestEmulatorRegistry::entryWithoutIdSkipped()
{
    QTemporaryDir dir;
    QJsonObject noId = testutils::mockEntry("x");
    noId.remove("id");
    QVERIFY(testutils::writeEmulators(dir.path(), {noId, "string entry", testutils::mockEntry("ok")}));
    EmulatorRegistry r(dir.path());
    QCOMPARE(r.count(), 1);
    QCOMPARE(r.skippedEntries().size(), 2);
    QVERIFY(r.skippedEntries().first().contains("#1"));
}

void TestEmulatorRegistry::duplicateIdSkipped()
{
    QTemporaryDir dir;
    QJsonObject second = testutils::mockEntry("dup");
    second["name"] = "Second";
    QVERIFY(testutils::writeEmulators(dir.path(), {testutils::mockEntry("dup"), second}));
    EmulatorRegistry r(dir.path());
    QCOMPARE(r.count(), 1);
    QCOMPARE(r.find("dup")->name, QStringLiteral("Emulator dup"));
    QCOMPARE(r.skippedEntries().size(), 1);
    QVERIFY(r.skippedEntries().first().contains("duplicate"));
}

void TestEmulatorRegistry::buildArguments()
{
    EmulatorDefinition def;
    def.arguments = {"--fullscreen", "{game}", "--fast"};
    def.profiles = {{"windowed", "Windowed", {"--windowed", "{game}"}}, {"bare", "Bare", {"--bare"}}};
    const QString game = "/games/My Game (USA).mock";

    QCOMPARE(def.buildArguments(game), QStringList({"--fullscreen", game, "--fast"}));
    QCOMPARE(def.buildArguments(QString()), QStringList({"--fullscreen", "--fast"}));
    QCOMPARE(def.buildArguments(game, "windowed"), QStringList({"--windowed", game}));
    QCOMPARE(def.buildArguments(game, "bare"), QStringList({"--bare", game})); // appended
    QCOMPARE(def.buildArguments(game, "unknown"), QStringList({"--fullscreen", game, "--fast"}));

    EmulatorDefinition noPlaceholder;
    noPlaceholder.arguments = {"-v"};
    QCOMPARE(noPlaceholder.buildArguments(game), QStringList({"-v", game}));
    QCOMPARE(noPlaceholder.buildArguments(QString()), QStringList({"-v"}));
}

void TestEmulatorRegistry::supportedExtensions()
{
    QTemporaryDir dir;
    QJsonObject off = testutils::mockEntry("off", {"zzz"});
    off["enabled"] = false;
    QVERIFY(testutils::writeEmulators(dir.path(), {testutils::mockEntry("a", {"bbb", "aaa"}),
                                                   testutils::mockEntry("b", {"aaa", "ccc"}), off}));
    EmulatorRegistry r(dir.path());
    QCOMPARE(r.supportedExtensions(), QStringList({"aaa", "bbb", "ccc"}));
}

void TestEmulatorRegistry::emulatorForExtension()
{
    QTemporaryDir dir;
    QJsonObject missing = testutils::mockEntry("missing", {"gb"});
    missing["executable"] = dir.filePath("nope");
    QJsonObject off = testutils::mockEntry("off", {"gb", "nes"});
    off["enabled"] = false;
    QVERIFY(testutils::writeEmulators(dir.path(), {missing, off, testutils::mockEntry("good", {"gb"})}));
    EmulatorRegistry r(dir.path());
    QCOMPARE(r.emulatorForExtension("gb"), QStringLiteral("good"));   // available preferred
    QCOMPARE(r.emulatorForExtension(".GB"), QStringLiteral("good"));
    QCOMPARE(r.emulatorForExtension("nes"), QString());               // disabled only
    QCOMPARE(r.emulatorForExtension("snes"), QString());

    QVERIFY(testutils::writeEmulators(dir.path(), {missing}));
    r.reload();
    QCOMPARE(r.emulatorForExtension("gb"), QStringLiteral("missing")); // enabled fallback
}

void TestEmulatorRegistry::validateDetectsChanges()
{
    QTemporaryDir dir;
    const QString exe = dir.filePath("emu");
    QVERIFY(QFile::copy(testutils::mockEmulator(), exe));
    QJsonObject entry = testutils::mockEntry("e");
    entry["executable"] = exe;
    QVERIFY(testutils::writeEmulators(dir.path(), {entry}));
    EmulatorRegistry r(dir.path());
    QCOMPARE(r.find("e")->status, EmulatorStatus::Available);

    QSignalSpy status(&r, &EmulatorRegistry::statusChanged);
    QSignalSpy data(&r, &EmulatorRegistry::dataChanged);
    QVERIFY(QFile::remove(exe));
    r.validate("e");
    QCOMPARE(r.find("e")->status, EmulatorStatus::ExecutableMissing);
    QCOMPARE(status.count(), 1);
    QCOMPARE(data.count(), 1);
    r.validateAll(); // nothing changed
    QCOMPARE(status.count(), 1);
}

void TestEmulatorRegistry::gameCounts()
{
    QTemporaryDir dir;
    QVERIFY(testutils::writeEmulators(dir.path(), {testutils::mockEntry("a"), testutils::mockEntry("b")}));
    EmulatorRegistry r(dir.path());
    QSignalSpy data(&r, &EmulatorRegistry::dataChanged);
    r.setGameCounts({{"a", 3}});
    QCOMPARE(data.count(), 1);
    QCOMPARE(r.index(0).data(EmulatorRegistry::GameCountRole).toInt(), 3);
    QCOMPARE(r.index(1).data(EmulatorRegistry::GameCountRole).toInt(), 0);
    QCOMPARE(r.get(0).value("gameCount").toInt(), 3);
}

QTEST_GUILESS_MAIN(TestEmulatorRegistry)
#include "tst_emulatorregistry.moc"
