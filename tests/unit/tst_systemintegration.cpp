#include "nexora/system/SingleInstanceGuard.h"
#include "nexora/system/SystemIntegration.h"
#include "nexora/system/UnixSignalWatcher.h"

#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <csignal>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

using namespace nexora::system;

class TestSystemIntegration : public QObject
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

    static SystemIntegration::Environment fakeEnv(const QTemporaryDir &dir)
    {
        SystemIntegration::Environment env;
        env.procRoot = dir.filePath(QStringLiteral("proc"));
        env.sysRoot = dir.filePath(QStringLiteral("sys"));
        env.osReleaseFile = dir.filePath(QStringLiteral("etc/os-release"));
        env.nexoraReleaseFile = dir.filePath(QStringLiteral("etc/nexora-release"));
        env.powerRequestFifo = dir.filePath(QStringLiteral("run/power-request"));
        env.mixerProgram = dir.filePath(QStringLiteral("bin/amixer"));
        return env;
    }

private slots:
    void detectsPlatform()
    {
        QTemporaryDir dir;
        const auto env = fakeEnv(dir);
        QCOMPARE(SystemIntegration::detectPlatform(env, {}), Platform::Desktop);
        write(env.nexoraReleaseFile, "VERSION=0.2.0\nNEXORA_PLATFORM=\"embedded\"\n");
        QCOMPARE(SystemIntegration::detectPlatform(env, {}), Platform::Embedded);
        QCOMPARE(SystemIntegration::detectPlatform(env, QStringLiteral("desktop")), Platform::Desktop);
        QCOMPARE(SystemIntegration::detectPlatform(env, QStringLiteral("bogus")), Platform::Embedded);
        QCOMPARE(SystemIntegration::create(env, QStringLiteral("desktop"))->platformName(), QStringLiteral("desktop"));
        QCOMPARE(SystemIntegration::create(env)->platformName(), QStringLiteral("embedded"));
    }

    void readsStatusFromProc()
    {
        QTemporaryDir dir;
        const auto env = fakeEnv(dir);
        write(env.procRoot + QStringLiteral("/uptime"), "12345.67 99.0\n");
        write(env.procRoot + QStringLiteral("/loadavg"), "0.42 0.30 0.10 1/100 999\n");
        write(env.procRoot + QStringLiteral("/meminfo"),
              "MemTotal:       16000000 kB\nMemFree: 1 kB\nMemAvailable:    8000000 kB\n");
        write(env.osReleaseFile, "NAME=Test\nPRETTY_NAME=\"Test OS 1.0\"\n");
        DesktopSystemIntegration sys(env);
        const SystemStatus s = sys.status();
        QCOMPARE(s.platform, QStringLiteral("desktop"));
        QCOMPARE(s.osName, QStringLiteral("Test OS 1.0"));
        QCOMPARE(s.uptimeSeconds, 12345);
        QCOMPARE(s.load1, 0.42);
        QCOMPARE(s.memoryTotalKiB, 16000000);
        QCOMPARE(s.memoryAvailableKiB, 8000000);
        QVERIFY(!s.hostname.isEmpty());
    }

    void missingProcIsTolerated()
    {
        QTemporaryDir dir;
        DesktopSystemIntegration sys(fakeEnv(dir));
        const SystemStatus s = sys.status();
        QCOMPARE(s.uptimeSeconds, -1);
        QCOMPARE(s.memoryTotalKiB, -1);
        QVERIFY(!sys.cpuTimes().valid);
    }

    void readsRealProc()
    {
        // Real proc files report a size of 0; reading must not depend on it.
        if (!QFile::exists(QStringLiteral("/proc/meminfo")) || !QFile::exists(QStringLiteral("/proc/stat")))
            QSKIP("No /proc on this system");
        DesktopSystemIntegration sys(SystemIntegration::Environment{});
        const SystemStatus s = sys.status();
        QVERIFY(s.memoryTotalKiB > 0);
        QVERIFY(s.memoryAvailableKiB >= 0);
        QVERIFY(sys.cpuTimes().valid);
    }

    void readsDeviceStatus()
    {
        QTemporaryDir dir;
        const auto env = fakeEnv(dir);
        DesktopSystemIntegration sys(env);
        // Nothing there: no battery, no network.
        DeviceStatus d = sys.deviceStatus();
        QCOMPARE(d.batteryPercent, -1);
        QCOMPARE(d.network, QStringLiteral("none"));

        const QString supply = env.sysRoot + QStringLiteral("/class/power_supply/");
        write(supply + QStringLiteral("AC/type"), "Mains\n");
        write(supply + QStringLiteral("BAT0/type"), "Battery\n");
        write(supply + QStringLiteral("BAT0/capacity"), "87\n");
        write(supply + QStringLiteral("BAT0/status"), "Charging\n");
        const QString net = env.sysRoot + QStringLiteral("/class/net/");
        write(net + QStringLiteral("lo/operstate"), "unknown\n");
        write(net + QStringLiteral("docker0/operstate"), "up\n"); // virtual: no device link
        write(net + QStringLiteral("wlan0/operstate"), "up\n");
        write(net + QStringLiteral("wlan0/device"), "");
        QDir().mkpath(net + QStringLiteral("wlan0/wireless"));
        write(net + QStringLiteral("eth0/operstate"), "down\n");
        write(net + QStringLiteral("eth0/device"), "");
        d = sys.deviceStatus();
        QCOMPARE(d.batteryPercent, 87);
        QVERIFY(d.batteryCharging);
        QCOMPARE(d.network, QStringLiteral("wifi"));

        // A wired link wins over Wi-Fi.
        write(net + QStringLiteral("eth0/operstate"), "up\n");
        write(supply + QStringLiteral("BAT0/status"), "Discharging\n");
        d = sys.deviceStatus();
        QCOMPARE(d.network, QStringLiteral("ethernet"));
        QVERIFY(!d.batteryCharging);
    }

    void readsCpuTimes()
    {
        QTemporaryDir dir;
        const auto env = fakeEnv(dir);
        DesktopSystemIntegration sys(env);
        // user nice system idle iowait irq softirq steal guest guest_nice
        write(env.procRoot + QStringLiteral("/stat"), "cpu  100 5 50 800 20 3 2 0 40 0\ncpu0 1 2 3 4\n");
        const CpuTimes t = sys.cpuTimes();
        QVERIFY(t.valid);
        QCOMPARE(t.total, quint64(980)); // guest time is already part of user time
        QCOMPARE(t.idle, quint64(820));

        write(env.procRoot + QStringLiteral("/stat"), "intr 1 2 3\n");
        QVERIFY(!sys.cpuTimes().valid);
        write(env.procRoot + QStringLiteral("/stat"), "cpu  1 x 3 4\n");
        QVERIFY(!sys.cpuTimes().valid);
    }

    void desktopNeverControlsThePower()
    {
        QTemporaryDir dir;
        DesktopSystemIntegration sys(fakeEnv(dir));
        QString error;
        QVERIFY(!sys.supports(Capability::Power));
        QVERIFY(!sys.requestPower(PowerAction::Poweroff, &error));
        QVERIFY(!error.isEmpty());
        QVERIFY(!sys.applyVolume(50, &error));
    }

    void embeddedPowerRequestUsesFifo()
    {
        QTemporaryDir dir;
        const auto env = fakeEnv(dir);
        EmbeddedSystemIntegration sys(env);
        QString error;
        QVERIFY(!sys.supports(Capability::Power));
        QVERIFY(!sys.requestPower(PowerAction::Reboot, &error)); // helper not running
        QVERIFY(error.contains(QStringLiteral("not running")));

        QDir().mkpath(QFileInfo(env.powerRequestFifo).absolutePath());
        QCOMPARE(::mkfifo(QFile::encodeName(env.powerRequestFifo).constData(), 0620), 0);
        QVERIFY(sys.supports(Capability::Power));
        QVERIFY(!sys.requestPower(PowerAction::Reboot, &error)); // nobody reading
        QVERIFY(error.contains(QStringLiteral("not listening")));

        const int reader = ::open(QFile::encodeName(env.powerRequestFifo).constData(), O_RDONLY | O_NONBLOCK);
        QVERIFY(reader >= 0);
        QVERIFY2(sys.requestPower(PowerAction::Poweroff, &error), qPrintable(error));
        char buffer[32] = {};
        const ssize_t n = ::read(reader, buffer, sizeof(buffer) - 1);
        ::close(reader);
        QCOMPARE(QByteArray(buffer, int(n)), QByteArray("poweroff\n"));
    }

    void embeddedVolumeRunsMixer()
    {
        QTemporaryDir dir;
        const auto env = fakeEnv(dir);
        const QString out = dir.filePath(QStringLiteral("mixer-args"));
        write(env.mixerProgram, "#!/bin/sh\necho \"$@\" > '" + out.toUtf8() + "'\n");
        QFile::setPermissions(env.mixerProgram, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
        EmbeddedSystemIntegration sys(env);
        QVERIFY(sys.supports(Capability::Volume));
        QString error;
        QVERIFY(sys.applyVolume(150, &error));
        auto contents = [&out] {
            QFile f(out);
            return f.open(QIODevice::ReadOnly) ? f.readAll().trimmed() : QByteArray();
        };
        QTRY_COMPARE_WITH_TIMEOUT(contents(), QByteArray("-q sset Master 100%"), 5000);
    }

    void embeddedVolumeFallsBackToPcm()
    {
        QTemporaryDir dir;
        const auto env = fakeEnv(dir);
        const QString out = dir.filePath(QStringLiteral("mixer-args"));
        write(env.mixerProgram, "#!/bin/sh\n[ \"$3\" = Master ] && exit 1\necho \"$@\" > '" + out.toUtf8() + "'\n");
        QFile::setPermissions(env.mixerProgram, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
        EmbeddedSystemIntegration sys(env);
        QSignalSpy applied(&sys, &EmbeddedSystemIntegration::volumeApplied);
        QVERIFY(sys.applyVolume(40, nullptr));
        QTRY_COMPARE_WITH_TIMEOUT(applied.size(), 1, 5000);
        QCOMPARE(applied.first().at(0).toBool(), true);
        QCOMPARE(applied.first().at(1).toString(), QStringLiteral("PCM"));

        write(env.mixerProgram, "#!/bin/sh\nexit 1\n");
        QVERIFY(sys.applyVolume(40, nullptr));
        QTRY_COMPARE_WITH_TIMEOUT(applied.size(), 2, 5000);
        QCOMPARE(applied.last().at(0).toBool(), false);
    }

    void singleInstance()
    {
        QTemporaryDir dir;
        const QString lock = dir.filePath(QStringLiteral("ui.lock"));
        auto first = std::make_unique<SingleInstanceGuard>(lock);
        QVERIFY(first->tryAcquire());
        SingleInstanceGuard second(lock);
        QVERIFY(!second.tryAcquire());
        QCOMPARE(second.ownerPid(), QCoreApplication::applicationPid());
        first.reset();
        QVERIFY(second.tryAcquire());
        QVERIFY(!SingleInstanceGuard::defaultLockPath().isEmpty());
    }

    void unixSignalsBecomeQtSignals()
    {
        UnixSignalWatcher watcher;
        QVERIFY(watcher.isActive());
        QSignalSpy spy(&watcher, &UnixSignalWatcher::signalReceived);
        ::raise(SIGHUP);
        ::raise(SIGTERM);
        QTRY_COMPARE_WITH_TIMEOUT(spy.size(), 2, 5000);
        QCOMPARE(spy.at(0).at(0).toInt(), SIGHUP);
        QCOMPARE(spy.at(1).at(0).toInt(), SIGTERM);
    }
};

QTEST_GUILESS_MAIN(TestSystemIntegration)
#include "tst_systemintegration.moc"
