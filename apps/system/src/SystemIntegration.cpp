#include "nexora/system/SystemIntegration.h"

#include "nexora/shared/Logging.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QSysInfo>

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace nexora::system {

namespace {

QString readFirstLine(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    return QString::fromUtf8(f.readLine()).trimmed();
}

// KEY=VALUE files (os-release, nexora-release); values may be quoted.
QString keyValue(const QString &path, const QString &key)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    while (!f.atEnd()) {
        const QString line = QString::fromUtf8(f.readLine()).trimmed();
        if (!line.startsWith(key + u'='))
            continue;
        QString value = line.mid(key.size() + 1);
        if (value.size() >= 2 && (value.startsWith(u'"') || value.startsWith(u'\'')) && value.back() == value.front())
            value = value.mid(1, value.size() - 2);
        return value;
    }
    return {};
}

} // namespace

Platform SystemIntegration::detectPlatform(const Environment &env, const QString &override)
{
    if (override == QLatin1String("embedded"))
        return Platform::Embedded;
    if (override == QLatin1String("desktop"))
        return Platform::Desktop;
    if (!override.isEmpty())
        qCWarning(lcSystem) << "Unknown NEXORA_PLATFORM" << override << "- detecting instead";
    return keyValue(env.nexoraReleaseFile, QStringLiteral("NEXORA_PLATFORM")) == QLatin1String("embedded")
        ? Platform::Embedded
        : Platform::Desktop;
}

std::unique_ptr<SystemIntegration> SystemIntegration::create(const Environment &env, const QString &platformOverride)
{
    const Platform platform = detectPlatform(env, platformOverride);
    std::unique_ptr<SystemIntegration> integration;
    if (platform == Platform::Embedded)
        integration = std::make_unique<EmbeddedSystemIntegration>(env);
    else
        integration = std::make_unique<DesktopSystemIntegration>(env);
    qCInfo(lcSystem).noquote() << "System integration:" << integration->platformName();
    return integration;
}

QString SystemIntegration::platformName() const
{
    return platform() == Platform::Embedded ? QStringLiteral("embedded") : QStringLiteral("desktop");
}

SystemStatus SystemIntegration::status() const
{
    SystemStatus s;
    s.platform = platformName();
    s.osName = keyValue(m_env.osReleaseFile, QStringLiteral("PRETTY_NAME"));
    s.hostname = QSysInfo::machineHostName();
    s.kernel = QSysInfo::kernelVersion();

    const QString uptime = readFirstLine(m_env.procRoot + QStringLiteral("/uptime"));
    if (!uptime.isEmpty())
        s.uptimeSeconds = qint64(uptime.section(u' ', 0, 0).toDouble());
    const QString load = readFirstLine(m_env.procRoot + QStringLiteral("/loadavg"));
    if (!load.isEmpty())
        s.load1 = load.section(u' ', 0, 0).toDouble();

    QFile mem(m_env.procRoot + QStringLiteral("/meminfo"));
    if (mem.open(QIODevice::ReadOnly | QIODevice::Text)) {
        // Read to the end instead of looping on atEnd(): proc files report a size of 0, so
        // atEnd() is true before anything has been read.
        const QStringList lines = QString::fromLatin1(mem.readAll()).split(u'\n');
        for (const QString &raw : lines) {
            const QString line = raw.simplified();
            const qint64 kib = line.section(u' ', 1, 1).toLongLong();
            if (line.startsWith(QLatin1String("MemTotal:")))
                s.memoryTotalKiB = kib;
            else if (line.startsWith(QLatin1String("MemAvailable:")))
                s.memoryAvailableKiB = kib;
        }
    }
    return s;
}

CpuTimes SystemIntegration::cpuTimes() const
{
    // First line: "cpu  user nice system idle iowait irq softirq steal guest guest_nice".
    // guest/guest_nice are already included in user/nice, so they are not added again.
    CpuTimes t;
    const QString line = readFirstLine(m_env.procRoot + QStringLiteral("/stat"));
    const QStringList fields = line.simplified().split(u' ');
    if (fields.size() < 5 || fields.first() != QLatin1String("cpu"))
        return t;
    for (qsizetype i = 1; i < fields.size() && i <= 8; ++i) {
        bool ok = false;
        const quint64 value = fields.at(i).toULongLong(&ok);
        if (!ok)
            return CpuTimes{};
        t.total += value;
        if (i == 4 || i == 5) // idle, iowait
            t.idle += value;
    }
    t.valid = t.total > 0;
    return t;
}

DeviceStatus SystemIntegration::deviceStatus() const
{
    DeviceStatus d;
    const QDir supplies(m_env.sysRoot + QStringLiteral("/class/power_supply"));
    for (const QString &name : supplies.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name)) {
        const QString base = supplies.filePath(name) + u'/';
        if (readFirstLine(base + QStringLiteral("type")) != QLatin1String("Battery"))
            continue;
        bool ok = false;
        const int capacity = readFirstLine(base + QStringLiteral("capacity")).toInt(&ok);
        if (!ok)
            continue;
        d.batteryPercent = qBound(0, capacity, 100);
        d.batteryCharging = readFirstLine(base + QStringLiteral("status")) == QLatin1String("Charging");
        break;
    }

    // A wired link wins over Wi-Fi, like the system's own default route usually does.
    const QDir interfaces(m_env.sysRoot + QStringLiteral("/class/net"));
    for (const QString &name : interfaces.entryList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::System, QDir::Name)) {
        const QString base = interfaces.filePath(name) + u'/';
        // Only real hardware: loopback, bridges, veth and tunnels have no "device" link.
        if (!QFileInfo::exists(base + QStringLiteral("device")))
            continue;
        if (readFirstLine(base + QStringLiteral("operstate")) != QLatin1String("up"))
            continue;
        const bool wireless = QFileInfo::exists(base + QStringLiteral("wireless"))
            || QFileInfo::exists(base + QStringLiteral("phy80211"));
        if (!wireless) {
            d.network = QStringLiteral("ethernet");
            break;
        }
        d.network = QStringLiteral("wifi");
    }
    return d;
}

UpdateInfo SystemIntegration::updateInfo() const
{
    UpdateInfo info;
    info.installedVersion = keyValue(m_env.nexoraReleaseFile, QStringLiteral("NEXORA_VERSION"));
    info.channel = keyValue(m_env.nexoraReleaseFile, QStringLiteral("NEXORA_UPDATE_CHANNEL"));
    info.message = platform() == Platform::Embedded
        ? QStringLiteral("In-place system updates are not implemented yet; write a newer image to update")
        : QStringLiteral("Updates are managed by the host system (rebuild or reinstall Nexora)");
    return info;
}

DesktopSystemIntegration::DesktopSystemIntegration(Environment env, QObject *parent)
    : SystemIntegration(parent)
{
    m_env = std::move(env);
}

bool DesktopSystemIntegration::requestPower(PowerAction, QString *error)
{
    // A desktop prototype must never shut down the developer's machine.
    if (error)
        *error = QStringLiteral("Power control is not available on the desktop platform");
    return false;
}

bool DesktopSystemIntegration::applyVolume(int, QString *error)
{
    // The volume preference is stored but the desktop's mixer is left alone.
    if (error)
        *error = QStringLiteral("Volume control is not available on the desktop platform");
    return false;
}

EmbeddedSystemIntegration::EmbeddedSystemIntegration(Environment env, QObject *parent)
    : SystemIntegration(parent)
{
    m_env = std::move(env);
}

bool EmbeddedSystemIntegration::supports(Capability capability) const
{
    switch (capability) {
    case Capability::Power: {
        const QFileInfo fifo(m_env.powerRequestFifo);
        return fifo.exists() && fifo.isWritable();
    }
    case Capability::Volume: return QFileInfo(m_env.mixerProgram).isExecutable();
    case Capability::Update: return false; // no update mechanism yet (see updateInfo())
    }
    return false;
}

bool EmbeddedSystemIntegration::requestPower(PowerAction action, QString *error)
{
    // Protocol: one line, exactly "poweroff" or "reboot", written to a FIFO owned by the
    // root-run nexora-powerd helper (mode 0620, group nexora). The UI never runs as root.
    const QByteArray path = QFile::encodeName(m_env.powerRequestFifo);
    struct ::stat st
    { };
    if (::stat(path.constData(), &st) != 0 || !S_ISFIFO(st.st_mode)) {
        if (error)
            *error = QStringLiteral("Power helper is not running (%1 missing)").arg(m_env.powerRequestFifo);
        return false;
    }
    const int fd = ::open(path.constData(), O_WRONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) {
        if (error)
            *error = errno == ENXIO
                ? QStringLiteral("Power helper is not listening")
                : QStringLiteral("Cannot contact power helper: %1").arg(QString::fromLocal8Bit(std::strerror(errno)));
        return false;
    }
    const QByteArray line
        = action == PowerAction::Poweroff ? QByteArrayLiteral("poweroff\n") : QByteArrayLiteral("reboot\n");
    const ssize_t written = ::write(fd, line.constData(), size_t(line.size()));
    const int writeErrno = errno;
    ::close(fd);
    if (written != line.size()) {
        if (error)
            *error = QStringLiteral("Cannot send power request: %1")
                         .arg(QString::fromLocal8Bit(std::strerror(writeErrno)));
        return false;
    }
    qCInfo(lcSystem) << "Power request sent:" << line.trimmed();
    return true;
}

bool EmbeddedSystemIntegration::applyVolume(int percent, QString *error)
{
    if (!supports(Capability::Volume)) {
        if (error)
            *error = QStringLiteral("No mixer program (%1)").arg(m_env.mixerProgram);
        return false;
    }
    runMixer(qBound(0, percent, 100), 0);
    return true;
}

void EmbeddedSystemIntegration::runMixer(int percent, int controlIndex)
{
    static const QStringList controls{QStringLiteral("Master"), QStringLiteral("PCM")};
    if (m_mixer) {
        m_mixer->disconnect(this);
        m_mixer->kill();
        m_mixer->deleteLater();
    }
    const QString control = controls.at(controlIndex);
    auto *process = new QProcess(this);
    m_mixer = process;
    connect(process, &QProcess::finished, this,
            [this, process, percent, controlIndex, control](int code, QProcess::ExitStatus status) {
                process->deleteLater();
                const bool ok = status == QProcess::NormalExit && code == 0;
                if (!ok && controlIndex + 1 < controls.size()) {
                    runMixer(percent, controlIndex + 1);
                    return;
                }
                if (!ok)
                    qCWarning(lcSystem) << "Could not set the volume: no usable mixer control (tried" << controls
                                        << ")";
                emit volumeApplied(ok, ok ? control : QString());
            });
    connect(process, &QProcess::errorOccurred, this, [this, process](QProcess::ProcessError e) {
        if (e != QProcess::FailedToStart)
            return;
        process->deleteLater();
        qCWarning(lcSystem) << "Cannot run" << m_env.mixerProgram;
        emit volumeApplied(false, {});
    });
    // Explicit argument list; membership in the audio group is sufficient (no root).
    process->start(m_env.mixerProgram,
                   {QStringLiteral("-q"), QStringLiteral("sset"), control, QStringLiteral("%1%").arg(percent)});
}

} // namespace nexora::system
