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

    // A failed rename must leave the store as it was: no cleared overrides, no signals,
    // and the user file still in place. A directory at <file>.bak cannot be removed or
    // renamed over, which makes the rename fail deterministically.
    void resetKeepsValuesWhenRenameFails()
    {
        QTemporaryDir dir;
        const QString user = dir.filePath(QStringLiteral("settings.json"));
        Settings s({{}, user});
        s.load();
        QVERIFY(s.setValue(Settings::AudioVolume, 5));
        QVERIFY(s.save());
        const QString backup = user + QStringLiteral(".bak");
        write(backup + QStringLiteral("/blocker"), "x");
        QVERIFY(QFileInfo(backup).isDir());
        QSignalSpy changed(&s, &Settings::valueChanged);
        QVERIFY(!s.resetToDefaults());
        QCOMPARE(changed.size(), 0);
        QCOMPARE(s.intValue(Settings::AudioVolume), 5);
        QVERIFY(s.isOverridden(Settings::AudioVolume));
        QVERIFY(QFile::exists(user));
        QVERIFY(QFileInfo(backup).isDir());
    }

    // Re-reading the file (e.g. after a backup restore) has to tell listeners which keys
    // actually changed, exactly like setValue() does, or they keep stale copies.
    void reloadEmitsValueChangedForChangedKeysOnly()
    {
        QTemporaryDir dir;
        const QString user = dir.filePath(QStringLiteral("settings.json"));
        write(user,
              wrap(R"({"audio.volume": 30, "ui.showHints": false, "library.directories": ["/games/a"]})").toUtf8());
        Settings s({{}, user});
        QVERIFY(s.load());
        QCOMPARE(s.intValue(Settings::AudioVolume), 30);

        // Changed: volume (30 -> 55), directories (override removed -> default), sort order
        // (default -> "recent"). Unchanged: showHints stays false.
        write(user, wrap(R"({"audio.volume": 55, "ui.showHints": false, "library.sortOrder": "recent"})").toUtf8());
        QSignalSpy changed(&s, &Settings::valueChanged);
        QSignalSpy reloaded(&s, &Settings::reloaded);
        QVERIFY(s.load());
        QCOMPARE(reloaded.size(), 1);

        QVariantMap emitted;
        for (const QList<QVariant> &args : changed)
            emitted.insert(args.at(0).toString(), args.at(1));
        QCOMPARE(changed.size(), 3); // one signal per changed key, none for unchanged ones
        QCOMPARE(emitted.value(Settings::AudioVolume).toInt(), 55);
        QCOMPARE(emitted.value(Settings::LibrarySortOrder).toString(), QStringLiteral("recent"));
        QVERIFY(emitted.contains(Settings::LibraryDirectories));
        QVERIFY(emitted.value(Settings::LibraryDirectories).toStringList().isEmpty());
        QVERIFY(!emitted.contains(Settings::UiShowHints));

        // Loading the same content again is silent.
        changed.clear();
        QVERIFY(s.load());
        QCOMPARE(changed.size(), 0);
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

    void notificationAndSessionKeysValidate()
    {
        QTemporaryDir dir;
        Settings s({{}, dir.filePath(QStringLiteral("user/settings.json"))});
        QVERIFY(s.load());
        QCOMPARE(s.boolValue(Settings::NotificationsQuiet), false);
        QCOMPARE(s.intValue(Settings::NotificationsDurationMs), 3500);
        QVERIFY(s.stringListValue(Settings::NotificationsHiddenCategories).isEmpty());
        QCOMPARE(s.intValue(Settings::SessionsMaxCount), 5);
        QCOMPARE(s.intValue(Settings::SessionsMaxMegabytes), 512);
        QCOMPARE(s.intValue(Settings::SessionsMaxAgeDays), 30);
        QVERIFY(s.stringListValue(Settings::SessionsExcludedGames).isEmpty());

        QString error;
        QVERIFY(s.setValue(Settings::NotificationsHiddenCategories,
                           QStringList{QStringLiteral("controller"), QStringLiteral("saveState")}, &error));
        QVERIFY(!s.setValue(Settings::NotificationsHiddenCategories, QStringList{QStringLiteral("ads")}, &error));
        QVERIFY(s.setValue(Settings::NotificationsDurationMs, 2000));
        QVERIFY(!s.setValue(Settings::NotificationsDurationMs, 1999, &error));
        QVERIFY(!s.setValue(Settings::NotificationsDurationMs, 15001, &error));
        QVERIFY(s.setValue(Settings::SessionsMaxCount, 20));
        QVERIFY(!s.setValue(Settings::SessionsMaxCount, 0, &error));
        QVERIFY(!s.setValue(Settings::SessionsMaxMegabytes, 63, &error));
        QVERIFY(s.setValue(Settings::SessionsMaxAgeDays, 0));
        QVERIFY(s.setValue(Settings::SessionsExcludedGames, QStringList{QStringLiteral("12")}, &error));
        QVERIFY(!s.setValue(Settings::SessionsExcludedGames, QStringList{QStringLiteral("012")}, &error));
        QVERIFY(!s.setValue(Settings::SessionsExcludedGames, QStringList{QStringLiteral("abc")}, &error));
    }

    void homeSectionsValidate()
    {
        QTemporaryDir dir;
        Settings s({{}, dir.filePath(QStringLiteral("user/settings.json"))});
        QVERIFY(s.load());
        QCOMPARE(s.stringListValue(Settings::HomeSections).size(), 7);
        QString error;
        QVERIFY(s.setValue(
            Settings::HomeSections,
            QStringList{QStringLiteral("recent"), QStringLiteral("collection:12"), QStringLiteral("recent")}, &error));
        QCOMPARE(s.stringListValue(Settings::HomeSections),
                 (QStringList{QStringLiteral("recent"), QStringLiteral("collection:12")})); // de-duplicated
        QVERIFY(s.setValue(Settings::HomeSections, QStringList{}, &error)); // an empty Home is allowed
        QVERIFY(!s.setValue(Settings::HomeSections, QStringList{QStringLiteral("ads")}, &error));
        QVERIFY(!s.setValue(Settings::HomeSections, QStringList{QStringLiteral("collection:0")}, &error));
        QVERIFY(!s.setValue(Settings::HomeSections, QStringList{QStringLiteral("collection:x")}, &error));
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
