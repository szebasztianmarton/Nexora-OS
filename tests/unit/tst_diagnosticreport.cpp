#include "nexora/shared/Redaction.h"
#include "nexora/system/DiagnosticReport.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTest>

using namespace nexora::system;

class TestDiagnosticReport : public QObject
{
    Q_OBJECT

private:
    static nexora::shared::RedactionRules rules()
    {
        nexora::shared::RedactionRules r;
        r.homeDir = QStringLiteral("/home/alice");
        return r;
    }

    static DiagnosticInputs sampleInputs()
    {
        DiagnosticInputs in;
        in.appVersion = QStringLiteral("0.4.0");
        in.buildId = QStringLiteral("abc1234");
        in.platform = QStringLiteral("embedded");
        in.hardware.cpuModel = QStringLiteral("Test CPU");
        in.hardware.cpuThreads = 4;
        GpuInfo gpu;
        gpu.card = QStringLiteral("card0");
        gpu.vendorId = QStringLiteral("0x8086");
        gpu.deviceId = QStringLiteral("0x5917");
        gpu.driver = QStringLiteral("i915");
        gpu.vendorName = QStringLiteral("Intel");
        in.hardware.gpus.append(gpu);
        return in;
    }

private slots:
    void containsVersionBuildAndGpu()
    {
        const QString text = buildDiagnosticReport(sampleInputs(), rules());
        QVERIFY(text.contains(QStringLiteral("0.4.0")));
        QVERIFY(text.contains(QStringLiteral("abc1234")));
        QVERIFY(text.contains(QStringLiteral("i915")));
        QVERIFY(text.contains(QStringLiteral("Intel")));
        QVERIFY(text.contains(QStringLiteral("Test CPU")));
    }

    void redactsEveryLine()
    {
        DiagnosticInputs in = sampleInputs();
        in.recentLog = {QStringLiteral("opened /home/alice/roms/game.iso"), QStringLiteral("api_key=hunter2")};
        in.launchSummaries = {QStringLiteral("launch /home/alice/bin/emu failed")};
        in.configIssues = {QStringLiteral("bad file /home/alice/.config/nexora/x.json")};
        in.settingsIssues = {QStringLiteral("token: abcdef")};
        const QString text = buildDiagnosticReport(in, rules());
        QVERIFY2(!text.contains(QStringLiteral("/home/alice")), qPrintable(text));
        QVERIFY(text.contains(QStringLiteral("~/roms/game.iso")));
        QVERIFY(!text.contains(QStringLiteral("hunter2")));
        QVERIFY(!text.contains(QStringLiteral("abcdef")));
    }

    void boundsTheSizeAndKeepsTheNewestLogLines()
    {
        DiagnosticInputs in = sampleInputs();
        for (int i = 0; i < 100000; ++i)
            in.recentLog.append(QStringLiteral("log line number %1 with some padding text to take space").arg(i));
        const QString text = buildDiagnosticReport(in, rules());
        QVERIFY(text.toUtf8().size() <= 512 * 1024);
        QVERIFY(text.contains(QStringLiteral("log line number 99999 ")));
        QVERIFY(!text.contains(QStringLiteral("log line number 0 ")));
        // Non-log content is kept.
        QVERIFY(text.contains(QStringLiteral("abc1234")));
    }

    void exportKeepsTheNewestTenWith0600()
    {
        QTemporaryDir dir;
        const QString diag = dir.filePath(QStringLiteral("diagnostics"));
        QStringList created;
        for (int i = 0; i < 13; ++i) {
            QString error;
            const auto path = exportDiagnosticReport(QStringLiteral("report %1\n").arg(i), diag, &error);
            QVERIFY2(path.has_value(), qPrintable(error));
            created.append(*path);
            QCOMPARE(QFileInfo(*path).permissions()
                         & (QFileDevice::ReadGroup | QFileDevice::WriteGroup | QFileDevice::ReadOther
                            | QFileDevice::WriteOther),
                     QFileDevice::Permissions());
            QVERIFY(QFileInfo(*path).permissions().testFlag(QFileDevice::ReadOwner));
        }
        const QStringList left = QDir(diag).entryList({QStringLiteral("diagnostics-*.txt")}, QDir::Files);
        QCOMPARE(left.size(), 10);
        // The newest survive, the three oldest are gone.
        for (int i = 0; i < 3; ++i)
            QVERIFY(!QFileInfo::exists(created[i]));
        for (int i = 3; i < 13; ++i)
            QVERIFY(QFileInfo::exists(created[i]));
        QFile f(created.last());
        QVERIFY(f.open(QIODevice::ReadOnly));
        QCOMPARE(f.readAll(), QByteArray("report 12\n"));
    }
};

QTEST_GUILESS_MAIN(TestDiagnosticReport)
#include "tst_diagnosticreport.moc"
