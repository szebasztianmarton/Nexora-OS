#include "nexora/shared/Settings.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using nexora::shared::Settings;

class TestSettings : public QObject
{
    Q_OBJECT

private:
    static void write(const QString &path, const QByteArray &content)
    {
        QDir().mkpath(QFileInfo(path).absolutePath());
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(content);
    }

    static QString wrap(const QByteArray &settingsObject)
    {
        return QStringLiteral("{\"version\":1,\"settings\":%1}").arg(QString::fromUtf8(settingsObject));
    }

private slots:
    void defaultsWithoutFiles()
    {
        QTemporaryDir dir;
        Settings s({{dir.filePath(QStringLiteral("none.json"))}, dir.filePath(QStringLiteral("user/settings.json"))});
        QVERIFY(s.load());
        QVERIFY(s.issues().isEmpty());
        QCOMPARE(s.stringValue(Settings::UiLanguage), QStringLiteral("en"));
        QCOMPARE(s.stringValue(Settings::LibrarySortOrder), QStringLiteral("title"));
        QCOMPARE(s.intValue(Settings::AudioVolume), 80);
        QCOMPARE(s.stringValue(Settings::DisplayMode), QStringLiteral("fullscreen"));
        QVERIFY(s.stringListValue(Settings::LibraryDirectories).isEmpty());
        QCOMPARE(s.mapValue(Settings::InputMapping).value(QStringLiteral("accept")).toString(),
                 QStringLiteral("south"));
        QVERIFY(!s.boolValue(Settings::LibraryFollowSymlinks));
    }

    void persistsAcrossInstances()
    {
        QTemporaryDir dir;
        const Settings::Paths paths{{}, dir.filePath(QStringLiteral("cfg/settings.json"))};
        {
            Settings s(paths);
            QVERIFY(s.load());
            QSignalSpy changed(&s, &Settings::valueChanged);
            QVERIFY(s.setValue(Settings::AudioVolume, 35));
            QVERIFY(s.setValue(Settings::LibrarySortOrder, QStringLiteral("recent")));
            QVERIFY(s.setValue(Settings::LibraryDirectories, QStringList{QStringLiteral("/games/snes/")}));
            QCOMPARE(changed.size(), 3);
            QVERIFY(s.save());
        }
        Settings again(paths);
        QVERIFY(again.load());
        QVERIFY(again.issues().isEmpty());
        QCOMPARE(again.intValue(Settings::AudioVolume), 35);
        QCOMPARE(again.stringValue(Settings::LibrarySortOrder), QStringLiteral("recent"));
        QCOMPARE(again.stringListValue(Settings::LibraryDirectories), QStringList{QStringLiteral("/games/snes")});
    }

    void rejectsInvalidValues_data()
    {
        QTest::addColumn<QString>("key");
        QTest::addColumn<QVariant>("value");
        QTest::newRow("language") << Settings::UiLanguage << QVariant(QStringLiteral("xx"));
        QTest::newRow("volume high") << Settings::AudioVolume << QVariant(101);
        QTest::newRow("volume fraction") << Settings::AudioVolume << QVariant(3.5);
        QTest::newRow("volume string") << Settings::AudioVolume << QVariant(QStringLiteral("50"));
        QTest::newRow("sort") << Settings::LibrarySortOrder << QVariant(QStringLiteral("random"));
        QTest::newRow("relative dir") << Settings::LibraryDirectories << QVariant(QStringList{QStringLiteral("games")});
        QTest::newRow("root dir") << Settings::LibraryDirectories << QVariant(QStringList{QStringLiteral("/")});
        QTest::newRow("bad ext") << Settings::LibraryExtensions << QVariant(QStringList{QStringLiteral(".a b")});
        QTest::newRow("resolution") << Settings::DisplayResolution << QVariant(QStringLiteral("big"));
        QTest::newRow("bool") << Settings::UiShowHints << QVariant(QStringLiteral("yes"));
        QTest::newRow("mapping action") << Settings::InputMapping
                                        << QVariant(QVariantMap{{QStringLiteral("jump"), QStringLiteral("south")}});
        QTest::newRow("mapping button") << Settings::InputMapping
                                        << QVariant(QVariantMap{{QStringLiteral("accept"), QStringLiteral("x9")}});
        QTest::newRow("unknown key") << QStringLiteral("ui.nonexistent") << QVariant(QStringLiteral("dark"));
    }

    void rejectsInvalidValues()
    {
        QFETCH(QString, key);
        QFETCH(QVariant, value);
        QTemporaryDir dir;
        Settings s({{}, dir.filePath(QStringLiteral("s.json"))});
        s.load();
        const QVariant before = s.value(key);
        QString error;
        QVERIFY(!s.setValue(key, value, &error));
        QVERIFY(!error.isEmpty());
        QCOMPARE(s.value(key), before);
    }

    void normalisesValues()
    {
        QTemporaryDir dir;
        Settings s({{}, dir.filePath(QStringLiteral("s.json"))});
        s.load();
        QVERIFY(s.setValue(Settings::LibraryExtensions,
                           QStringList{QStringLiteral("SFC"), QStringLiteral(".sfc"), QStringLiteral(" .Nes ")}));
        QCOMPARE(s.stringListValue(Settings::LibraryExtensions),
                 QStringList({QStringLiteral(".sfc"), QStringLiteral(".nes")}));
        QVERIFY(s.setValue(Settings::AudioVolume, 40.0)); // JSON numbers arrive as double
        QCOMPARE(s.value(Settings::AudioVolume).typeId(), int(QMetaType::Int));
    }

    void settingDefaultRemovesOverride()
    {
        QTemporaryDir dir;
        Settings s({{}, dir.filePath(QStringLiteral("s.json"))});
        s.load();
        QVERIFY(s.setValue(Settings::AudioVolume, 10));
        QVERIFY(s.isOverridden(Settings::AudioVolume));
        QVERIFY(s.setValue(Settings::AudioVolume, 80));
        QVERIFY(!s.isOverridden(Settings::AudioVolume));
    }

    void layersSystemDefaultsUnderUserValues()
    {
        QTemporaryDir dir;
        const QString system = dir.filePath(QStringLiteral("system.json"));
        const QString user = dir.filePath(QStringLiteral("user.json"));
        write(system, wrap("{\"audio.volume\": 50, \"display.mode\": \"windowed\"}").toUtf8());
        write(user, wrap("{\"audio.volume\": 20}").toUtf8());
        Settings s({{system}, user});
        QVERIFY(s.load());
        QCOMPARE(s.intValue(Settings::AudioVolume), 20);
        QCOMPARE(s.stringValue(Settings::DisplayMode), QStringLiteral("windowed"));
        // Back to the system default (not the built-in one) drops the user override.
        QVERIFY(s.setValue(Settings::AudioVolume, 50));
        QVERIFY(!s.isOverridden(Settings::AudioVolume));
    }

    void dropsInvalidEntriesWithIssues()
    {
        QTemporaryDir dir;
        const QString user = dir.filePath(QStringLiteral("user.json"));
        write(user, wrap("{\"audio.volume\": 400, \"ui.theme\": \"x\", \"library.sortOrder\": \"recent\"}").toUtf8());
        Settings s({{}, user});
        QVERIFY(s.load()); // file is usable; individual entries are not
        QCOMPARE(s.issues().size(), 2);
        QCOMPARE(s.intValue(Settings::AudioVolume), 80);
        QCOMPARE(s.stringValue(Settings::LibrarySortOrder), QStringLiteral("recent"));
    }

    void recoversFromCorruptedFile_data()
    {
        QTest::addColumn<QByteArray>("content");
        QTest::newRow("garbage") << QByteArray("{not json");
        QTest::newRow("array") << QByteArray("[]");
        QTest::newRow("no version") << QByteArray("{\"settings\":{}}");
        QTest::newRow("future version") << QByteArray("{\"version\":99,\"settings\":{}}");
        QTest::newRow("settings not object") << QByteArray("{\"version\":1,\"settings\":[]}");
    }

    void recoversFromCorruptedFile()
    {
        QFETCH(QByteArray, content);
        QTemporaryDir dir;
        const QString user = dir.filePath(QStringLiteral("settings.json"));
        write(user, content);
        Settings s({{}, user});
        QVERIFY(!s.load());
        QVERIFY(!s.issues().isEmpty());
        QCOMPARE(s.intValue(Settings::AudioVolume), 80);
        QVERIFY(!QFile::exists(user)); // moved aside, not destroyed
        const QStringList aside = QDir(dir.path()).entryList({QStringLiteral("settings.json.corrupt-*")});
        QCOMPARE(aside.size(), 1);
        // A fresh save works afterwards.
        QVERIFY(s.setValue(Settings::AudioVolume, 30));
        QVERIFY(s.save());
        Settings again({{}, user});
        QVERIFY(again.load());
        QCOMPARE(again.intValue(Settings::AudioVolume), 30);
    }

    void inspectModeNeverRenames()
    {
        QTemporaryDir dir;
        const QString user = dir.filePath(QStringLiteral("settings.json"));
        write(user, "{broken");
        Settings s({{}, user});
        QVERIFY(!s.load(Settings::LoadMode::Inspect));
        QVERIFY(!s.issues().isEmpty());
        QVERIFY(QFile::exists(user));
        QCOMPARE(QDir(dir.path()).entryList({QStringLiteral("settings.json.corrupt-*")}).size(), 0);
    }

    void resetKeepsBackup()
    {
        QTemporaryDir dir;
        const QString user = dir.filePath(QStringLiteral("settings.json"));
        Settings s({{}, user});
        s.load();
        QVERIFY(s.setValue(Settings::AudioVolume, 5));
        QVERIFY(s.save());
        QSignalSpy changed(&s, &Settings::valueChanged);
        QVERIFY(s.resetToDefaults());
        QCOMPARE(changed.size(), 1);
        QCOMPARE(s.intValue(Settings::AudioVolume), 80);
        QVERIFY(!QFile::exists(user));
        QVERIFY(QFile::exists(user + QStringLiteral(".bak")));
        Settings again({{}, user});
        QVERIFY(again.load());
        QCOMPARE(again.intValue(Settings::AudioVolume), 80);
    }

    void savedFileIsVersionedJson()
    {
        QTemporaryDir dir;
        const QString user = dir.filePath(QStringLiteral("settings.json"));
        Settings s({{}, user});
        s.load();
        QVERIFY(s.setValue(Settings::UiShowHints, false));
        QVERIFY(s.save());
        QFile f(user);
        QVERIFY(f.open(QIODevice::ReadOnly));
        const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
        QCOMPARE(doc.object().value(QStringLiteral("version")).toInt(), 1);
        QCOMPARE(doc.object().value(QStringLiteral("settings")).toObject().size(), 1);
    }

    void everySpecDefaultIsValid()
    {
        for (const auto &spec : Settings::specs()) {
            QVariant v = spec.defaultValue;
            QVERIFY2(spec.validate(v).isEmpty(), qPrintable(spec.key));
            QVERIFY2(!spec.description.isEmpty(), qPrintable(spec.key));
        }
    }

    void newUiInputAudioLogKeysHaveDefaultsAndValidate()
    {
        QTemporaryDir dir;
        Settings s({{}, dir.filePath(QStringLiteral("user/settings.json"))});
        QVERIFY(s.load());
        QCOMPARE(s.stringValue(Settings::UiTheme), QStringLiteral("glass"));
        QCOMPARE(s.intValue(Settings::UiScale), 100);
        QCOMPARE(s.boolValue(Settings::UiReducedMotion), false);
        QCOMPARE(s.boolValue(Settings::UiReduceTransparency), false);
        QCOMPARE(s.stringValue(Settings::UiStartScreen), QStringLiteral("home"));
        QCOMPARE(s.intValue(Settings::InputDeadzone), 25);
        QCOMPARE(s.boolValue(Settings::AudioMuted), false);
        QCOMPARE(s.stringValue(Settings::LogLevel), QStringLiteral("info"));
        QVERIFY(s.mapValue(Settings::EmulatorDefaultForSystem).isEmpty());

        QVERIFY(s.setValue(Settings::UiTheme, QStringLiteral("highContrast")));
        QVERIFY(s.setValue(Settings::UiScale, 150));
        QVERIFY(s.setValue(Settings::UiScale, 75));
        QVERIFY(s.setValue(Settings::InputDeadzone, 5));
        QVERIFY(s.setValue(Settings::InputDeadzone, 60));
        QVERIFY(s.setValue(Settings::LogLevel, QStringLiteral("debug")));

        QString error;
        QVERIFY(!s.setValue(Settings::UiTheme, QStringLiteral("neon"), &error));
        QVERIFY(!s.setValue(Settings::UiScale, 74, &error));
        QVERIFY(!s.setValue(Settings::UiScale, 151, &error));
        QVERIFY(!s.setValue(Settings::UiReducedMotion, 1, &error));
        QVERIFY(!s.setValue(Settings::UiReduceTransparency, QStringLiteral("on"), &error));
        QVERIFY(s.setValue(Settings::UiReduceTransparency, true));
        QVERIFY(!s.setValue(Settings::UiStartScreen, QStringLiteral("settings"), &error));
        QVERIFY(!s.setValue(Settings::InputDeadzone, 4, &error));
        QVERIFY(!s.setValue(Settings::InputDeadzone, 61, &error));
        QVERIFY(!s.setValue(Settings::AudioMuted, QStringLiteral("yes"), &error));
        QVERIFY(!s.setValue(Settings::LogLevel, QStringLiteral("trace"), &error));
    }

    void defaultForSystemValidatesKeysAndValues()
    {
        QTemporaryDir dir;
        Settings s({{}, dir.filePath(QStringLiteral("user/settings.json"))});
        QVERIFY(s.load());
        QString error;
        QVERIFY(s.setValue(Settings::EmulatorDefaultForSystem,
                           QVariantMap{{QStringLiteral("snes"), QStringLiteral("snes9x")}}, &error));
        QCOMPARE(s.mapValue(Settings::EmulatorDefaultForSystem).value(QStringLiteral("snes")).toString(),
                 QStringLiteral("snes9x"));
        QVERIFY(!s.setValue(Settings::EmulatorDefaultForSystem,
                            QVariantMap{{QStringLiteral("Bad Sys"), QStringLiteral("snes9x")}}, &error));
        QVERIFY(!s.setValue(Settings::EmulatorDefaultForSystem,
                            QVariantMap{{QStringLiteral("snes"), QStringLiteral("Bad Emu")}}, &error));
        QVERIFY(!s.setValue(Settings::EmulatorDefaultForSystem, QVariantMap{{QStringLiteral("snes"), 5}}, &error));
        // 32 characters is the longest system id.
        QVERIFY(s.setValue(Settings::EmulatorDefaultForSystem, QVariantMap{{QString(32, u'a'), QStringLiteral("x")}},
                           &error));
        QVERIFY(!s.setValue(Settings::EmulatorDefaultForSystem, QVariantMap{{QString(33, u'a'), QStringLiteral("x")}},
                            &error));
    }
};

QTEST_GUILESS_MAIN(TestSettings)
#include "tst_settings.moc"
