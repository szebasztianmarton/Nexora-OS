#include "nexora/emulator/LaunchHistory.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTest>

using namespace nexora::emulator;

namespace {

nexora::shared::RedactionRules rules()
{
    return {QStringLiteral("/home/tester"), {}};
}

LaunchRecord record(int n)
{
    LaunchRecord r;
    r.emulatorId = QStringLiteral("emu");
    r.profileId = QStringLiteral("p");
    r.gameTitle = QStringLiteral("Game %1").arg(n);
    r.startedAt = QDateTime(QDate(2026, 1, 1), QTime(0, 0), Qt::UTC).addSecs(n);
    r.endedAt = r.startedAt.addSecs(5);
    r.pid = 1000 + n;
    r.finalState = QStringLiteral("Exited");
    r.exitCode = n;
    return r;
}

} // namespace

class TestLaunchHistory : public QObject
{
    Q_OBJECT

private slots:
    void keepsNewestFifty()
    {
        QTemporaryDir dir;
        LaunchHistory history(dir.filePath(QStringLiteral("history.json")), 50, rules());
        for (int i = 0; i < 60; ++i) {
            QString error;
            QVERIFY2(history.append(record(i), &error), qPrintable(error));
        }
        const auto records = history.records();
        QCOMPARE(records.size(), 50);
        QCOMPARE(records.first().gameTitle, QStringLiteral("Game 59"));
        QCOMPARE(records.last().gameTitle, QStringLiteral("Game 10"));
        QCOMPARE(records.first().pid, qint64(1059));
        QCOMPARE(records.first().exitCode, std::optional<int>(59));
        QCOMPARE(records.first().startedAt, record(59).startedAt);
    }

    void fileIsVersioned()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("history.json"));
        LaunchHistory history(path, 50, rules());
        QVERIFY(history.append(record(1), nullptr));
        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly));
        const QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
        QCOMPARE(root.value(QStringLiteral("version")).toInt(), 1);
        QCOMPARE(root.value(QStringLiteral("records")).toArray().size(), 1);
    }

    void tailIsBoundedAndRedacted()
    {
        QTemporaryDir dir;
        LaunchHistory history(dir.filePath(QStringLiteral("history.json")), 50, rules());
        LaunchRecord r = record(1);
        r.stderrTail
            = QStringLiteral("opening /home/tester/roms/secret.sfc failed\n") + QString(10000, QLatin1Char('x'));
        r.error = QStringLiteral("cannot read /home/tester/.config/emu");
        QVERIFY(history.append(r, nullptr));
        const LaunchRecord stored = history.records().first();
        QCOMPARE(stored.stderrTail.size(), 4096);
        QVERIFY(!stored.stderrTail.contains(QStringLiteral("/home/tester")));
        QVERIFY(!stored.error.contains(QStringLiteral("/home/tester")));
        QVERIFY(stored.error.contains(QStringLiteral("~/.config/emu")));

        LaunchRecord shortTail = record(2);
        shortTail.stderrTail = QStringLiteral("opening /home/tester/roms/a.sfc failed");
        QVERIFY(history.append(shortTail, nullptr));
        QCOMPARE(history.records().first().stderrTail, QStringLiteral("opening ~/roms/a.sfc failed"));
    }

    void storesOnlyTheTitle()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("history.json"));
        LaunchHistory history(path, 50, rules());
        LaunchRecord r = record(1);
        r.gameTitle = QStringLiteral("Super Game");
        QVERIFY(history.append(r, nullptr));
        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly));
        const QByteArray text = file.readAll();
        QVERIFY(text.contains("Super Game"));
        QVERIFY(!text.contains("gamePath"));
        QVERIFY(!text.contains("\"path\""));
    }

    void corruptFileIsMovedAside()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("history.json"));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("{ this is not json");
        file.close();

        LaunchHistory history(path, 50, rules());
        QVERIFY(history.records().isEmpty());
        QVERIFY(!QFile::exists(path));
        QCOMPARE(QDir(dir.path()).entryList({QStringLiteral("history.json.corrupt-*")}, QDir::Files).size(), 1);

        QVERIFY(history.append(record(1), nullptr));
        QCOMPARE(history.records().size(), 1);
    }

    void missingFileIsEmpty()
    {
        QTemporaryDir dir;
        LaunchHistory history(dir.filePath(QStringLiteral("none.json")), 50, rules());
        QVERIFY(history.records().isEmpty());
    }
};

QTEST_APPLESS_MAIN(TestLaunchHistory)
#include "tst_launchhistory.moc"
