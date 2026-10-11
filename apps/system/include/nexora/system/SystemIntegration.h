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
enum class Capability { Power, Volume, Update };

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

// What the system can say about updates. MVP 2 has no update mechanism: this is the seam a
// future one (e.g. A/B image updates via a privileged helper) plugs into, so the UI can
// already show the installed version and how to update.
struct UpdateInfo
{
    bool supported = false; // an in-place update mechanism exists
    QString installedVersion; // NEXORA_VERSION from /etc/nexora-release; empty on desktop
    QString channel; // NEXORA_UPDATE_CHANNEL from /etc/nexora-release, if any
    QString message; // how updates work on this platform
};

// Filesystem locations the integration reads or writes (overridable for tests).
struct SystemEnvironment
{
    QString procRoot = QStringLiteral("/proc");
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
    // Never blocks and never contacts the network.
    virtual UpdateInfo updateInfo() const;

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
