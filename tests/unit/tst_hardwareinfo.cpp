#include "nexora/system/HardwareInfo.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

using namespace nexora::system;

class TestHardwareInfo : public QObject
{
    Q_OBJECT

private:
    static HardwareRoots fixtureRoots()
    {
        const QString base = QStringLiteral(NEXORA_FIXTURE_DIR "/sysroot");
        return {base + QStringLiteral("/proc"), base + QStringLiteral("/sys")};
    }

    static bool copyTree(const QString &from, const QString &to)
    {
        QDir().mkpath(to);
        QDirIterator it(from, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            it.next();
            const QString dest = to + u'/' + QDir(from).relativeFilePath(it.filePath());
            QDir().mkpath(QFileInfo(dest).absolutePath());
            if (!QFile::copy(it.filePath(), dest))
                return false;
        }
        return true;
    }

private slots:
    void readsTheFixtureSysroot()
    {
        const HardwareSnapshot s = probeHardware(fixtureRoots(), {});
        QCOMPARE(s.cpuModel, QStringLiteral("Intel(R) Core(TM) i5-8250U CPU @ 1.60GHz"));
        QCOMPARE(s.cpuThreads, 2);
        QCOMPARE(s.memTotalKiB, qint64(8000000));
        QCOMPARE(s.memAvailableKiB, qint64(4000000));

        QCOMPARE(s.gpus.size(), 1);
        QCOMPARE(s.gpus[0].card, QStringLiteral("card0"));
        QCOMPARE(s.gpus[0].vendorId, QStringLiteral("0x8086"));
        QCOMPARE(s.gpus[0].deviceId, QStringLiteral("0x5917"));
        QCOMPARE(s.gpus[0].driver, QStringLiteral("i915"));
        QCOMPARE(s.gpus[0].vendorName, QStringLiteral("Intel"));

        QCOMPARE(s.outputs.size(), 1);
        QCOMPARE(s.outputs[0].name, QStringLiteral("HDMI-A-1"));
        QVERIFY(s.outputs[0].connected);
        QCOMPARE(s.outputs[0].modes, (QStringList{QStringLiteral("1920x1080"), QStringLiteral("1280x720")}));

        QCOMPARE(s.audio.size(), 2);
        QCOMPARE(s.audio[0].index, 0);
        QCOMPARE(s.audio[0].id, QStringLiteral("PCH"));
        QCOMPARE(s.audio[0].name, QStringLiteral("HDA Intel PCH"));
        QCOMPARE(s.audio[1].index, 1);
        QCOMPARE(s.audio[1].id, QStringLiteral("HDMI"));
    }

    void reportsStorage()
    {
        QTemporaryDir dir;
        const HardwareSnapshot s = probeHardware({}, {dir.path(), QStringLiteral("/nonexistent/nexora-path")});
        QCOMPARE(s.storage.size(), 2);
        QCOMPARE(s.storage[0].path, dir.path());
        QVERIFY(s.storage[0].totalBytes > 0);
        QVERIFY(s.storage[0].availableBytes >= 0);
        QCOMPARE(s.storage[1].path, QStringLiteral("/nonexistent/nexora-path"));
        QCOMPARE(s.storage[1].totalBytes, qint64(-1));
    }

    void nonexistentRootsGiveAnEmptySnapshot()
    {
        const HardwareSnapshot s
            = probeHardware({QStringLiteral("/nonexistent/proc"), QStringLiteral("/nonexistent/sys")}, {});
        QVERIFY(s.cpuModel.isEmpty());
        QCOMPARE(s.cpuThreads, 0);
        QCOMPARE(s.memTotalKiB, qint64(-1));
        QCOMPARE(s.memAvailableKiB, qint64(-1));
        QVERIFY(s.gpus.isEmpty());
        QVERIFY(s.outputs.isEmpty());
        QVERIFY(s.audio.isEmpty());
        QVERIFY(s.storage.isEmpty());
    }

    void oversizedFilesAreNotRead()
    {
        QTemporaryDir dir;
        const HardwareRoots roots = fixtureRoots();
        QVERIFY(copyTree(roots.proc, dir.filePath(QStringLiteral("proc"))));
        QFile big(dir.filePath(QStringLiteral("proc/cpuinfo")));
        QVERIFY(big.open(QIODevice::WriteOnly | QIODevice::Truncate));
        QVERIFY(big.write(QByteArray(2 * 1024 * 1024, 'x')) > 0);
        big.close();
        const HardwareSnapshot s = probeHardware({dir.filePath(QStringLiteral("proc")), roots.sys}, {});
        QVERIFY(s.cpuModel.isEmpty());
        QCOMPARE(s.cpuThreads, 0);
        // Other files are unaffected.
        QCOMPARE(s.memTotalKiB, qint64(8000000));
    }

    void namesKnownVendors()
    {
        QCOMPARE(gpuVendorName(QStringLiteral("0x1002")), QStringLiteral("AMD"));
        QCOMPARE(gpuVendorName(QStringLiteral("0x10DE")), QStringLiteral("NVIDIA"));
        QCOMPARE(gpuVendorName(QStringLiteral("0x1af4")), QStringLiteral("virtio"));
        QCOMPARE(gpuVendorName(QStringLiteral("0x1234")), QStringLiteral("QEMU std VGA"));
        QVERIFY(gpuVendorName(QStringLiteral("0xdead")).isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestHardwareInfo)
#include "tst_hardwareinfo.moc"
