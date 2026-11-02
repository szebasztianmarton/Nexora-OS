#include "nexora/shared/SystemCatalog.h"

#include <QFile>
#include <QTemporaryDir>
#include <QTest>

using namespace nexora::shared;

class TestSystemCatalog : public QObject
{
    Q_OBJECT

private:
    static QString write(const QTemporaryDir &dir, const QByteArray &content)
    {
        const QString path = dir.filePath(QStringLiteral("systems.json"));
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly))
            return {};
        f.write(content);
        return path;
    }

private slots:
    void builtInLoads()
    {
        const SystemCatalog catalog = SystemCatalog::builtIn();
        QVERIFY2(catalog.systems().size() >= 20, qPrintable(QString::number(catalog.systems().size())));
        for (const char *id : {"nes", "snes", "n64", "gb", "gbc", "gba", "nds", "genesis", "sms", "gamegear", "psx",
                               "ps2", "psp", "gamecube", "wii", "xbox360", "pce", "atari2600", "arcade", "dos"})
            QVERIFY2(catalog.find(QLatin1String(id)), id);
        QVERIFY(catalog.find(QStringLiteral("nope")) == nullptr);
        QCOMPARE(catalog.find(QStringLiteral("snes"))->shortName, QStringLiteral("SNES"));
    }

    void builtInResourceHasNoIssues()
    {
        QStringList issues;
        const auto catalog = SystemCatalog::load(QStringLiteral(":/nexora/systems.json"), &issues);
        QVERIFY(catalog.has_value());
        QVERIFY2(issues.isEmpty(), qPrintable(issues.join(u'\n')));
    }

    void extensionLookup()
    {
        const SystemCatalog catalog = SystemCatalog::builtIn();
        QCOMPARE(catalog.systemsForExtension(QStringLiteral(".sfc")), QStringList{QStringLiteral("snes")});
        QCOMPARE(catalog.systemsForExtension(QStringLiteral(".SFC")), QStringList{QStringLiteral("snes")});
        QVERIFY(catalog.systemsForExtension(QStringLiteral(".iso")).size() >= 2);
        QCOMPARE(catalog.systemsForExtension(QStringLiteral(".xex")), QStringList{QStringLiteral("xbox360")});
        QVERIFY(catalog.systemsForExtension(QStringLiteral(".iso")).contains(QStringLiteral("xbox360")));
        QCOMPARE(catalog.find(QStringLiteral("xbox360"))->shortName, QStringLiteral("Xbox 360"));
        QVERIFY(catalog.systemsForExtension(QStringLiteral(".zzz")).isEmpty());
    }

    void invalidEntriesAreDroppedWithIssues()
    {
        QTemporaryDir dir;
        const QString path = write(dir, R"({"version":1,"systems":[
            {"id":"ok","name":"Ok","shortName":"OK","extensions":[".ok"]},
            {"id":"Bad Id","name":"B","shortName":"B","extensions":[".b"]},
            {"id":"badext","name":"B","shortName":"B","extensions":["nodot"]},
            {"id":"ok","name":"Dup","shortName":"D","extensions":[".d"]}
        ]})");
        QStringList issues;
        const auto catalog = SystemCatalog::load(path, &issues);
        QVERIFY(catalog.has_value());
        QCOMPARE(catalog->systems().size(), 1);
        QCOMPARE(catalog->systems().first().id, QStringLiteral("ok"));
        QCOMPARE(issues.size(), 3);
    }

    void oversizeFileIsRejected()
    {
        QTemporaryDir dir;
        const QString path = write(dir, QByteArray(256 * 1024 + 1, ' '));
        QStringList issues;
        QVERIFY(!SystemCatalog::load(path, &issues).has_value());
        QVERIFY(!issues.isEmpty());
    }

    void malformedAndWrongVersionAreRejected()
    {
        QTemporaryDir dir;
        QStringList issues;
        QVERIFY(!SystemCatalog::load(write(dir, "not json"), &issues).has_value());
        issues.clear();
        QVERIFY(!SystemCatalog::load(write(dir, R"({"version":2,"systems":[]})"), &issues).has_value());
        QVERIFY(!issues.isEmpty());
        issues.clear();
        QVERIFY(!SystemCatalog::load(dir.filePath(QStringLiteral("missing.json")), &issues).has_value());
        QVERIFY(!issues.isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestSystemCatalog)
#include "tst_systemcatalog.moc"
