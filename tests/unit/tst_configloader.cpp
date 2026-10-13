#include "nexora/shared/ConfigLoader.h"
#include "nexora/shared/SystemCatalog.h"

#include <QFile>
#include <QTemporaryDir>
#include <QTest>

#include <unistd.h>

using nexora::shared::ConfigIssue;
using nexora::shared::ConfigLoader;

class TestConfigLoader : public QObject
{
    Q_OBJECT

private:
    static void writeFile(const QTemporaryDir &dir, const QString &name, const QByteArray &content)
    {
        QFile file(dir.filePath(name));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(content);
    }

    static std::optional<nexora::shared::EmulatorConfig> parse(const QByteArray &json, QStringList *errors,
                                                               QStringList *warnings = nullptr)
    {
        return ConfigLoader::parse(json, QStringLiteral("/etc/nexora/test.json"), errors, warnings);
    }

    // A minimal valid definition plus extra members; single quotes stand for double quotes
    // (moc in Qt 6.4 trips over raw string literals in test sources).
    static QByteArray emu(const char *extraMembers = "")
    {
        QByteArray json = "{'id':'a','name':'A','executable':'/bin/true'";
        if (*extraMembers)
            json += QByteArray(",") + extraMembers;
        json += "}";
        return json.replace('\'', '"');
    }

private slots:
    void parsesCompleteDefinition()
    {
        QStringList errors;
        const auto config = parse("{\"id\":\"snes-x\",\"name\":\" SNES \",\"executable\":\"/usr/bin/snes\", "
                                  "\"arguments\":[\"--fullscreen\",\"-v\"],\"enabled\":false}",
                                  &errors);
        QVERIFY2(config.has_value(), qPrintable(errors.join(u'\n')));
        QCOMPARE(config->id, QStringLiteral("snes-x"));
        QCOMPARE(config->name, QStringLiteral("SNES"));
        QCOMPARE(config->executable, QStringLiteral("/usr/bin/snes"));
        QCOMPARE(config->arguments, QStringList({QStringLiteral("--fullscreen"), QStringLiteral("-v")}));
        QCOMPARE(config->enabled, false);
        QCOMPARE(config->sourceFile, QStringLiteral("/etc/nexora/test.json"));
    }

    void appliesDefaults()
    {
        QStringList errors;
        const auto config = parse("{\"id\":\"a\",\"name\":\"A\",\"executable\":\"/bin/true\"}", &errors);
        QVERIFY(config.has_value());
        QVERIFY(config->arguments.isEmpty());
        QVERIFY(config->enabled);
    }

    void rejectsInvalidDefinitions_data()
    {
        QTest::addColumn<QByteArray>("json");
        QTest::addColumn<QString>("expectedError");

        QTest::newRow("malformed") << QByteArray("{\"id\": ") << QStringLiteral("invalid JSON at offset");
        QTest::newRow("array root") << QByteArray("[]") << QStringLiteral("must be an object");
        QTest::newRow("missing id") << QByteArray("{\"name\":\"A\",\"executable\":\"/bin/true\"}")
                                    << QStringLiteral("\"id\" must be a string (got missing)");
        QTest::newRow("numeric id") << QByteArray("{\"id\":5,\"name\":\"A\",\"executable\":\"/bin/true\"}")
                                    << QStringLiteral("\"id\" must be a string (got number)");
        QTest::newRow("uppercase id") << QByteArray("{\"id\":\"Bad\",\"name\":\"A\",\"executable\":\"/bin/true\"}")
                                      << QStringLiteral("\"id\" \"Bad\" is invalid");
        QTest::newRow("id with slash") << QByteArray("{\"id\":\"a/b\",\"name\":\"A\",\"executable\":\"/bin/true\"}")
                                       << QStringLiteral("is invalid");
        QTest::newRow("missing name") << QByteArray("{\"id\":\"a\",\"executable\":\"/bin/true\"}")
                                      << QStringLiteral("\"name\" must be a string");
        QTest::newRow("blank name") << QByteArray("{\"id\":\"a\",\"name\":\"  \",\"executable\":\"/bin/true\"}")
                                    << QStringLiteral("\"name\" must not be empty");
        QTest::newRow("missing executable")
            << QByteArray("{\"id\":\"a\",\"name\":\"A\"}") << QStringLiteral("\"executable\" must be a string");
        QTest::newRow("empty executable") << QByteArray("{\"id\":\"a\",\"name\":\"A\",\"executable\":\"\"}")
                                          << QStringLiteral("\"executable\" must not be empty");
        QTest::newRow("arguments string")
            << QByteArray("{\"id\":\"a\",\"name\":\"A\",\"executable\":\"/bin/true\",\"arguments\":\"-v\"}")
            << QStringLiteral("\"arguments\" must be an array of strings (got string)");
        QTest::newRow("argument number") << QByteArray(
            "{\"id\":\"a\",\"name\":\"A\",\"executable\":\"/bin/true\",\"arguments\":[\"-v\",3]}")
                                         << QStringLiteral("\"arguments\"[1] must be a string (got number)");
        QTest::newRow("enabled string") << QByteArray(
            "{\"id\":\"a\",\"name\":\"A\",\"executable\":\"/bin/true\",\"enabled\":\"yes\"}")
                                        << QStringLiteral("\"enabled\" must be a boolean (got string)");
    }

    void rejectsInvalidDefinitions()
    {
        QFETCH(QByteArray, json);
        QFETCH(QString, expectedError);
        QStringList errors;
        QVERIFY(!parse(json, &errors).has_value());
        QVERIFY(!errors.isEmpty());
        const bool found
            = std::any_of(errors.cbegin(), errors.cend(), [&](const QString &e) { return e.contains(expectedError); });
        QVERIFY2(found, qPrintable(errors.join(u'\n')));
    }

    void warnsAboutUnknownKeys()
    {
        QStringList errors;
        QStringList warnings;
        const auto config = parse("{\"id\":\"a\",\"name\":\"A\",\"executable\":\"/bin/true\",\"shell\":\"rm -rf\"}",
                                  &errors, &warnings);
        QVERIFY(config.has_value());
        QCOMPARE(warnings.size(), 1);
        QVERIFY(warnings.first().contains(QStringLiteral("\"shell\"")));
    }

    void resolvesRelativeExecutableAgainstConfigFile()
    {
        QStringList errors;
        const auto config = ConfigLoader::parse("{\"id\":\"a\",\"name\":\"A\",\"executable\":\"../bin/emu\"}",
                                                QStringLiteral("/opt/nexora/config/a.json"), &errors);
        QVERIFY(config.has_value());
        QCOMPARE(config->executable, QStringLiteral("/opt/nexora/bin/emu"));
    }

    void trimsExecutable()
    {
        QStringList errors;
        const auto config = parse("{\"id\":\"a\",\"name\":\"A\",\"executable\":\"  /usr/bin/emu \"}", &errors);
        QVERIFY(config.has_value());
        QCOMPARE(config->executable, QStringLiteral("/usr/bin/emu"));
    }

    void reportsUnreadableFiles()
    {
        if (::geteuid() == 0)
            QSKIP("root can read any file");
        QTemporaryDir dir;
        writeFile(dir, QStringLiteral("locked.json"), "{\"id\":\"a\",\"name\":\"A\",\"executable\":\"/bin/true\"}");
        QVERIFY(QFile::setPermissions(dir.filePath(QStringLiteral("locked.json")), QFile::WriteOwner));
        const auto result = ConfigLoader::loadDirectory(dir.path());
        QVERIFY(result.emulators.isEmpty());
        QVERIFY(result.hasErrors());
        QVERIFY(result.issues.first().message.contains(QStringLiteral("cannot read file")));
    }

    void validatesIds()
    {
        QVERIFY(ConfigLoader::isValidId(QStringLiteral("a")));
        QVERIFY(ConfigLoader::isValidId(QStringLiteral("mgba_0-1")));
        QVERIFY(!ConfigLoader::isValidId(QString()));
        QVERIFY(!ConfigLoader::isValidId(QStringLiteral("-a")));
        QVERIFY(!ConfigLoader::isValidId(QStringLiteral("a b")));
        QVERIFY(!ConfigLoader::isValidId(QString(65, u'a')));
        QVERIFY(ConfigLoader::isValidId(QString(64, u'a')));
    }

    void reportsMissingDirectory()
    {
        QTemporaryDir dir;
        const auto result = ConfigLoader::loadDirectory(dir.filePath(QStringLiteral("nope")));
        QVERIFY(!result.directoryFound);
        QVERIFY(result.emulators.isEmpty());
        QVERIFY(result.hasErrors());
    }

    void loadsEmptyDirectory()
    {
        QTemporaryDir dir;
        const auto result = ConfigLoader::loadDirectory(dir.path());
        QVERIFY(result.directoryFound);
        QVERIFY(result.emulators.isEmpty());
        QVERIFY(result.issues.isEmpty());
    }

    void loadsDirectorySortedAndIgnoresOtherFiles()
    {
        QTemporaryDir dir;
        writeFile(dir, QStringLiteral("b.json"), "{\"id\":\"b\",\"name\":\"B\",\"executable\":\"/bin/true\"}");
        writeFile(dir, QStringLiteral("a.json"), "{\"id\":\"a\",\"name\":\"A\",\"executable\":\"/bin/true\"}");
        writeFile(dir, QStringLiteral("notes.txt"), "not json");
        writeFile(dir, QStringLiteral(".hidden.json"), "{\"id\":\"h\",\"name\":\"H\",\"executable\":\"/bin/true\"}");
        writeFile(dir, QStringLiteral("c.json"),
                  "{\"id\":\"c\",\"name\":\"C\",\"executable\":\"/bin/true\",\"enabled\":false}");

        const auto result = ConfigLoader::loadDirectory(dir.path());
        QVERIFY(result.issues.isEmpty());
        QCOMPARE(result.emulators.size(), 3);
        QCOMPARE(result.emulators.at(0).id, QStringLiteral("a"));
        QCOMPARE(result.emulators.at(1).id, QStringLiteral("b"));
        QCOMPARE(result.emulators.at(2).id, QStringLiteral("c"));
        QVERIFY(!result.emulators.at(2).enabled);
    }

    void rejectsDuplicateIds()
    {
        QTemporaryDir dir;
        writeFile(dir, QStringLiteral("1-first.json"),
                  "{\"id\":\"dup\",\"name\":\"First\",\"executable\":\"/bin/true\"}");
        writeFile(dir, QStringLiteral("2-second.json"),
                  "{\"id\":\"dup\",\"name\":\"Second\",\"executable\":\"/bin/true\"}");

        const auto result = ConfigLoader::loadDirectory(dir.path());
        QCOMPARE(result.emulators.size(), 1);
        QCOMPARE(result.emulators.first().name, QStringLiteral("First"));
        QCOMPARE(result.issues.size(), 1);
        const ConfigIssue &issue = result.issues.first();
        QCOMPARE(issue.severity, ConfigIssue::Severity::Error);
        QVERIFY(issue.file.endsWith(QStringLiteral("2-second.json")));
        QVERIFY(issue.message.contains(QStringLiteral("duplicate id \"dup\"")));
        QVERIFY(issue.message.contains(QStringLiteral("1-first.json")));
    }

    void keepsValidEntriesWhenOthersAreInvalid()
    {
        QTemporaryDir dir;
        writeFile(dir, QStringLiteral("good.json"), "{\"id\":\"good\",\"name\":\"Good\",\"executable\":\"/bin/true\"}");
        writeFile(dir, QStringLiteral("bad.json"), "{\"id\":\"bad\",");

        const auto result = ConfigLoader::loadDirectory(dir.path());
        QCOMPARE(result.emulators.size(), 1);
        QCOMPARE(result.emulators.first().id, QStringLiteral("good"));
        QVERIFY(result.hasErrors());
        QVERIFY(result.issues.first().toDisplayString().startsWith(QStringLiteral("bad.json: ")));
    }

    void rejectsOversizedFiles()
    {
        QTemporaryDir dir;
        QByteArray big = "{\"id\":\"big\",\"name\":\"Big\",\"executable\":\"/bin/true\",\"pad\":\"";
        big += QByteArray(ConfigLoader::MaxFileSize, 'x');
        big += "\"}";
        writeFile(dir, QStringLiteral("big.json"), big);

        const auto result = ConfigLoader::loadDirectory(dir.path());
        QVERIFY(result.emulators.isEmpty());
        QVERIFY(result.hasErrors());
        QVERIFY(result.issues.first().message.contains(QStringLiteral("larger than")));
    }

    void parsesVersionSystemsAndPausable()
    {
        const auto catalog = nexora::shared::SystemCatalog::builtIn();
        QStringList errors;
        QStringList warnings;
        const auto config = ConfigLoader::parse(emu("'version':'1.2.3','systems':['snes','nes'],'pausable':true"),
                                                QStringLiteral("/etc/nexora/test.json"), &errors, &warnings, &catalog);
        QVERIFY2(config.has_value(), qPrintable(errors.join(u'\n')));
        QCOMPARE(config->version, QStringLiteral("1.2.3"));
        QCOMPARE(config->systems, QStringList({QStringLiteral("snes"), QStringLiteral("nes")}));
        QVERIFY(config->pausable);
        QVERIFY(warnings.isEmpty());
    }

    void unknownSystemIdIsDroppedWithIssue()
    {
        const auto catalog = nexora::shared::SystemCatalog::builtIn();
        QStringList errors;
        QStringList warnings;
        const auto config = ConfigLoader::parse(emu("'systems':['snes','bogus']"),
                                                QStringLiteral("/etc/nexora/test.json"), &errors, &warnings, &catalog);
        QVERIFY(config.has_value());
        QCOMPARE(config->systems, QStringList{QStringLiteral("snes")});
        QCOMPARE(warnings.size(), 1);
        QVERIFY(warnings.first().contains(QStringLiteral("bogus")));
    }

    void systemIdsAreOnlySyntaxCheckedWithoutCatalog()
    {
        QStringList errors;
        QStringList warnings;
        const auto config = parse(emu("'systems':['anything','Bad Id']"), &errors, &warnings);
        QVERIFY(config.has_value());
        QCOMPARE(config->systems, QStringList{QStringLiteral("anything")});
        QCOMPARE(warnings.size(), 1);
    }

    void rejectsBadVersionSystemsAndPausableTypes()
    {
        const QByteArray longVersion = "'version':'" + QByteArray(65, 'v') + "'";
        const QList<QByteArray> bad{emu("'version':3"), emu("'systems':'snes'"), emu("'systems':[1]"),
                                    emu("'pausable':'yes'"), emu(longVersion.constData())};
        for (const QByteArray &json : bad) {
            QStringList errors;
            QVERIFY2(!parse(json, &errors).has_value(), json.constData());
            QVERIFY2(!errors.isEmpty(), json.constData());
        }
    }

    void legacyFileWithoutNewKeysStillLoads()
    {
        QStringList errors;
        QStringList warnings;
        const auto config = parse(emu(), &errors, &warnings);
        QVERIFY(config.has_value());
        QVERIFY(config->version.isEmpty());
        QVERIFY(config->systems.isEmpty());
        QVERIFY(!config->pausable);
        QVERIFY(warnings.isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestConfigLoader)
#include "tst_configloader.moc"
