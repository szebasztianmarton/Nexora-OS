#include "nexora/shared/AtomicFile.h"
#include "nexora/shared/DataLayout.h"
#include "nexora/system/BackupService.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTest>

using namespace nexora::system;
using nexora::shared::DataLayout;

class TestBackupService : public QObject
{
    Q_OBJECT

private:
    static void write(const QString &path, const QByteArray &content)
    {
        QDir().mkpath(QFileInfo(path).absolutePath());
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
        f.write(content);
    }

    static QByteArray read(const QString &path)
    {
        QFile f(path);
        return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray("<missing>");
    }

    static BackupService::DbSnapshot copySnapshot(const QString &dbPath)
    {
        return [dbPath](const QString &dest, QString *error) {
            if (QFile::copy(dbPath, dest))
                return true;
            if (error)
                *error = QStringLiteral("snapshot copy failed");
            return false;
        };
    }

    struct Fixture
    {
        QTemporaryDir dir;
        DataLayout layout = DataLayout::underRoot(dir.path());
        QString settings = layout.configDir + QStringLiteral("/settings.json");
        QString library = layout.dataDir + QStringLiteral("/library.sqlite");
        QString history = layout.stateDir + QStringLiteral("/launch-history.json");
        QString save = layout.savesDir + QStringLiteral("/dolphin/slot1.sav");

        Fixture()
        {
            QString error;
            layout.ensureDirectories(&error);
            write(settings, "{\"version\":1,\"volume\":40}");
            write(library, "SQLITE-FAKE-CONTENT");
            write(history, "{\"version\":1,\"entries\":[]}");
            write(save, "SAVE-DATA");
        }
    };

    static QJsonObject readManifest(const QString &backup)
    {
        return QJsonDocument::fromJson(read(backup + QStringLiteral("/manifest.json"))).object();
    }

    static void writeManifest(const QString &backup, const QJsonObject &obj)
    {
        write(backup + QStringLiteral("/manifest.json"), QJsonDocument(obj).toJson());
    }

private slots:
    void createWritesFilesAndMatchingHashes()
    {
        Fixture fx;
        BackupService service(fx.layout, copySnapshot(fx.library));
        QString error;
        const auto path = service.createBackup(false, &error);
        QVERIFY2(path.has_value(), qPrintable(error));
        QVERIFY(QFileInfo(*path).fileName().startsWith(QStringLiteral("nexora-backup-")));
        QVERIFY(path->startsWith(fx.layout.backupDir));

        const QJsonObject manifest = readManifest(*path);
        QCOMPARE(manifest.value(QStringLiteral("version")).toInt(), 1);
        QVERIFY(!manifest.value(QStringLiteral("createdAt")).toString().isEmpty());
        QVERIFY(!manifest.value(QStringLiteral("appVersion")).toString().isEmpty());
        QStringList paths;
        for (const QJsonValue &v : manifest.value(QStringLiteral("files")).toArray()) {
            const QJsonObject e = v.toObject();
            const QString rel = e.value(QStringLiteral("path")).toString();
            paths << rel;
            const QByteArray hash
                = QCryptographicHash::hash(read(*path + u'/' + rel), QCryptographicHash::Sha256).toHex();
            QCOMPARE(e.value(QStringLiteral("sha256")).toString().toLatin1(), hash);
        }
        paths.sort();
        QCOMPARE(paths,
                 (QStringList{QStringLiteral("config/settings.json"), QStringLiteral("data/library.sqlite"),
                              QStringLiteral("state/launch-history.json")}));
        QCOMPARE(read(*path + QStringLiteral("/data/library.sqlite")), QByteArray("SQLITE-FAKE-CONTENT"));
        // Saves are excluded by default.
        QVERIFY(!QFileInfo::exists(*path + QStringLiteral("/saves")));
    }

    void savesAreIncludedOnRequest()
    {
        Fixture fx;
        BackupService service(fx.layout, copySnapshot(fx.library));
        QString error;
        const auto path = service.createBackup(true, &error);
        QVERIFY2(path.has_value(), qPrintable(error));
        QCOMPARE(read(*path + QStringLiteral("/saves/dolphin/slot1.sav")), QByteArray("SAVE-DATA"));
        bool listed = false;
        for (const QJsonValue &v : readManifest(*path).value(QStringLiteral("files")).toArray())
            listed
                |= v.toObject().value(QStringLiteral("path")).toString() == QStringLiteral("saves/dolphin/slot1.sav");
        QVERIFY(listed);
    }

    void snapshotFailureFailsTheBackupCleanly()
    {
        Fixture fx;
        BackupService service(fx.layout, [](const QString &, QString *error) {
            *error = QStringLiteral("db busy");
            return false;
        });
        QString error;
        QVERIFY(!service.createBackup(false, &error).has_value());
        QVERIFY(error.contains(QStringLiteral("db busy")));
        QVERIFY(service.listBackups().isEmpty());
    }

    void restoreBringsOldContentBackAndKeepsAsideCopies()
    {
        Fixture fx;
        BackupService service(fx.layout, copySnapshot(fx.library));
        QString error;
        const auto path = service.createBackup(true, &error);
        QVERIFY2(path.has_value(), qPrintable(error));

        write(fx.settings, "{\"version\":1,\"volume\":99}");
        write(fx.library, "CHANGED-LIBRARY");
        QFile::remove(fx.history);
        write(fx.save, "NEWER-SAVE");

        QVERIFY2(service.restoreBackup(*path, &error), qPrintable(error));
        QCOMPARE(read(fx.settings), QByteArray("{\"version\":1,\"volume\":40}"));
        QCOMPARE(read(fx.library), QByteArray("SQLITE-FAKE-CONTENT"));
        QCOMPARE(read(fx.history), QByteArray("{\"version\":1,\"entries\":[]}"));
        QCOMPARE(read(fx.save), QByteArray("SAVE-DATA"));

        const QStringList aside
            = QDir(fx.layout.configDir).entryList({QStringLiteral("settings.json.pre-restore-*")}, QDir::Files);
        QCOMPARE(aside.size(), 1);
        QCOMPARE(read(fx.layout.configDir + u'/' + aside[0]), QByteArray("{\"version\":1,\"volume\":99}"));
        // A newer save is moved aside, never deleted.
        const QStringList saveAside = QDir(QFileInfo(fx.save).absolutePath())
                                          .entryList({QStringLiteral("slot1.sav.pre-restore-*")}, QDir::Files);
        QCOMPARE(saveAside.size(), 1);
        QCOMPARE(read(QFileInfo(fx.save).absolutePath() + u'/' + saveAside[0]), QByteArray("NEWER-SAVE"));
    }

    void tamperedFileRefusesRestoreAndChangesNothing()
    {
        Fixture fx;
        BackupService service(fx.layout, copySnapshot(fx.library));
        QString error;
        const auto path = service.createBackup(false, &error);
        QVERIFY(path.has_value());
        write(*path + QStringLiteral("/data/library.sqlite"), "TAMPERED");
        write(fx.settings, "{\"version\":1,\"volume\":7}");

        QVERIFY(!service.restoreBackup(*path, &error));
        QVERIFY(!error.isEmpty());
        QCOMPARE(read(fx.settings), QByteArray("{\"version\":1,\"volume\":7}"));
        QCOMPARE(read(fx.library), QByteArray("SQLITE-FAKE-CONTENT"));
        QVERIFY(QDir(fx.layout.configDir).entryList({QStringLiteral("*.pre-restore-*")}, QDir::Files).isEmpty());
    }

    void pathEscapingManifestEntriesAreRefused()
    {
        Fixture fx;
        BackupService service(fx.layout, copySnapshot(fx.library));
        QString error;
        const auto path = service.createBackup(false, &error);
        QVERIFY(path.has_value());
        write(fx.settings, "{\"version\":1,\"volume\":7}");

        for (const QString &evil : {QStringLiteral("../../etc/passwd"), QStringLiteral("/etc/passwd"),
                                    QStringLiteral("config/../../x"), QStringLiteral("other/file")}) {
            QJsonObject manifest = readManifest(*path);
            QJsonArray files = manifest.value(QStringLiteral("files")).toArray();
            files.append(QJsonObject{{QStringLiteral("path"), evil}, {QStringLiteral("sha256"), QString(64, u'0')}});
            manifest.insert(QStringLiteral("files"), files);
            writeManifest(*path, manifest);
            QVERIFY2(!service.restoreBackup(*path, &error), qPrintable(evil));
            QCOMPARE(read(fx.settings), QByteArray("{\"version\":1,\"volume\":7}"));
        }
    }

    void unlistedFilesAndSymlinksAreRefused()
    {
        Fixture fx;
        BackupService service(fx.layout, copySnapshot(fx.library));
        QString error;
        const auto path = service.createBackup(false, &error);
        QVERIFY(path.has_value());

        write(*path + QStringLiteral("/config/extra.json"), "{}");
        QVERIFY(!service.restoreBackup(*path, &error));
        QFile::remove(*path + QStringLiteral("/config/extra.json"));
        QVERIFY2(service.restoreBackup(*path, &error), qPrintable(error));

        // Replace a listed file by a symlink with the same content.
        const QString target = fx.dir.filePath(QStringLiteral("elsewhere.json"));
        write(target, read(*path + QStringLiteral("/config/settings.json")));
        QVERIFY(QFile::remove(*path + QStringLiteral("/config/settings.json")));
        QVERIFY(QFile::link(target, *path + QStringLiteral("/config/settings.json")));
        QVERIFY(!service.restoreBackup(*path, &error));
    }

    void missingBackupOrManifestIsRefused()
    {
        Fixture fx;
        BackupService service(fx.layout, copySnapshot(fx.library));
        QString error;
        QVERIFY(!service.restoreBackup(fx.dir.filePath(QStringLiteral("nope")), &error));
        QDir().mkpath(fx.dir.filePath(QStringLiteral("empty")));
        QVERIFY(!service.restoreBackup(fx.dir.filePath(QStringLiteral("empty")), &error));
        QVERIFY(!error.isEmpty());
    }

    void listsNewestFirst()
    {
        Fixture fx;
        BackupService service(fx.layout, copySnapshot(fx.library));
        QVERIFY(service.listBackups().isEmpty());
        for (const char *name :
             {"nexora-backup-20260101-100000", "nexora-backup-20260301-100000", "nexora-backup-20260201-100000"})
            QVERIFY(QDir().mkpath(fx.layout.backupDir + u'/' + QLatin1String(name)));
        QVERIFY(QDir().mkpath(fx.layout.backupDir + QStringLiteral("/unrelated")));
        const QStringList names = service.listBackups();
        QCOMPARE(names.size(), 3);
        QVERIFY(names[0].endsWith(QStringLiteral("20260301-100000")));
        QVERIFY(names[1].endsWith(QStringLiteral("20260201-100000")));
        QVERIFY(names[2].endsWith(QStringLiteral("20260101-100000")));
    }

    void twoBackupsInTheSameSecondDoNotCollide()
    {
        Fixture fx;
        BackupService service(fx.layout, copySnapshot(fx.library));
        QString error;
        const auto a = service.createBackup(false, &error);
        const auto b = service.createBackup(false, &error);
        QVERIFY(a && b);
        QVERIFY(*a != *b);
        QCOMPARE(service.listBackups().first(), *b);
    }
};

QTEST_GUILESS_MAIN(TestBackupService)
#include "tst_backupservice.moc"
