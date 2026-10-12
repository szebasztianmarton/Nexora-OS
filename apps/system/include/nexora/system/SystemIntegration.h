#pragma once

#include <QObject>
#include <QPointer>
#include <QProcess>
#include <QString>
#include <QStringList>

#include <memory>

namespace nexora::system {

enum class Platform { Desktop, Embedded };
enum class PowerAction { Poweroff, Reboot };
enum class Capability { Power, Volume };

struct SystemStatus
{
    QString platform; // "desktop" / "embedded"
    QString osName; // PRETTY_NAME from os-release, if any
    QString hostname;
    QString kernel; // uname release
    qint64 uptimeSeconds = -1;
    double load1 = -1; // 1-minute load average
    qint64 memoryTotalKiB = -1;
    qint64 memoryAvailableKiB = -1;
};

// Aggregate CPU time counters from /proc/stat (in clock ticks). Usage over an interval is
// 1 - (idle delta / total delta) between two samples.
struct CpuTimes
{
    quint64 total = 0;
    quint64 idle = 0; // idle + iowait
    bool valid = false;
};

// Battery and network state from sysfs, for status icons. Absent hardware is reported as
// such (batteryPercent -1, network "none"), never guessed.
struct DeviceStatus
{
    int batteryPercent = -1; // first battery; -1 when there is none
    bool batteryCharging = false;
    QString network = QStringLiteral("none"); // "ethernet", "wifi" or "none" (no interface up)
};

// Filesystem locations the integration reads or writes (overridable for tests).
struct SystemEnvironment
{
    QString procRoot = QStringLiteral("/proc");
    QString sysRoot = QStringLiteral("/sys");
    QString osReleaseFile = QStringLiteral("/etc/os-release");
    QString nexoraReleaseFile = QStringLiteral("/etc/nexora-release");
    QString powerRequestFifo = QStringLiteral("/run/nexora/power-request");
    QString mixerProgram = QStringLiteral("/usr/bin/amixer");
};

// Boundary between the UI and the operating system. The UI only talks to this interface;
// the desktop implementation deliberately does nothing privileged, the embedded one uses
// narrowly scoped, documented mechanisms (see docs/architecture.md, docs/buildroot.md).
class SystemIntegration : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;

    using Environment = SystemEnvironment;

    // NEXORA_PLATFORM=desktop|embedded wins; otherwise "embedded" when
    // /etc/nexora-release says NEXORA_PLATFORM=embedded; otherwise desktop.
    static Platform detectPlatform(const Environment &env, const QString &override);
    static std::unique_ptr<SystemIntegration> create(const Environment &env = Environment{},
                                                     const QString &platformOverride = {});

    virtual Platform platform() const = 0;
    QString platformName() const;
    virtual bool supports(Capability capability) const = 0;
    virtual SystemStatus status() const;
    // Cheap enough to sample about once per second (one small proc file).
    virtual CpuTimes cpuTimes() const;
    // A handful of small sysfs reads; fine to call about once per second.
    virtual DeviceStatus deviceStatus() const;

    // Asks the system to power off / reboot. Never blocks.
    virtual bool requestPower(PowerAction action, QString *error) = 0;
    // Applies a 0-100 master volume. Never blocks.
    virtual bool applyVolume(int percent, QString *error) = 0;

    const Environment &environment() const { return m_env; }

protected:
    Environment m_env;
};

class DesktopSystemIntegration : public SystemIntegration
{
    Q_OBJECT
public:
    explicit DesktopSystemIntegration(Environment env, QObject *parent = nullptr);
    Platform platform() const override { return Platform::Desktop; }
    bool supports(Capability) const override { return false; }
    bool requestPower(PowerAction action, QString *error) override;
    bool applyVolume(int percent, QString *error) override;
};

class EmbeddedSystemIntegration : public SystemIntegration
{
    Q_OBJECT
public:
    explicit EmbeddedSystemIntegration(Environment env, QObject *parent = nullptr);
    Platform platform() const override { return Platform::Embedded; }
    bool supports(Capability capability) const override;
    bool requestPower(PowerAction action, QString *error) override;
    // Runs amixer asynchronously, trying the "Master" control first and then "PCM" (cards
    // such as HDMI or USB audio often lack "Master"). A newer request supersedes a pending one.
    bool applyVolume(int percent, QString *error) override;

signals:
    void volumeApplied(bool ok, const QString &control);

private:
    void runMixer(int percent, int controlIndex);

    QPointer<QProcess> m_mixer;
};

} // namespace nexora::system
