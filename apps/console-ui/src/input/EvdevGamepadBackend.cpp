#include "input/EvdevGamepadBackend.h"

#include "input/EvdevMapping.h"
#include "input/GamepadNavigator.h"
#include "nexora/shared/Logging.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSocketNotifier>

#include <linux/input.h>

#include <algorithm>
#include <cerrno>
#include <climits>
#include <cstring>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

namespace nexora::input {

namespace {

constexpr int kRescanDebounceMs = 500;
constexpr std::size_t kBitsPerLong = sizeof(unsigned long) * CHAR_BIT;
constexpr std::size_t kKeyBitsLongs = KEY_MAX / kBitsPerLong + 1;

bool testBit(const unsigned long *bits, std::size_t bit)
{
    return (bits[bit / kBitsPerLong] >> (bit % kBitsPerLong)) & 1UL;
}

QString errnoString(int err)
{
    return QString::fromLocal8Bit(std::strerror(err));
}

} // namespace

EvdevGamepadBackend::EvdevGamepadBackend(GamepadNavigator *navigator, QObject *parent)
    : InputBackend(navigator, parent)
{
    m_rescanTimer.setSingleShot(true);
    m_rescanTimer.setInterval(kRescanDebounceMs);
    connect(&m_rescanTimer, &QTimer::timeout, this, &EvdevGamepadBackend::rescan);
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this,
            &EvdevGamepadBackend::scheduleRescan);
}

EvdevGamepadBackend::~EvdevGamepadBackend()
{
    for (auto &device : m_devices)
        closeDevice(*device);
}

bool EvdevGamepadBackend::start()
{
    if (m_started)
        return true;
    if (!QFileInfo(m_inputDir).isDir()) {
        m_status = QStringLiteral("%1 does not exist; evdev gamepad input unavailable").arg(m_inputDir);
        qCWarning(lcInput).noquote() << m_status;
        emit statusChanged();
        return false;
    }
    m_started = true;
    if (!m_watcher.addPath(m_inputDir))
        qCWarning(lcInput) << "Cannot watch" << m_inputDir << "- gamepad hotplug disabled";
    rescan();
    return true;
}

QString EvdevGamepadBackend::name() const
{
    return QStringLiteral("evdev");
}

QString EvdevGamepadBackend::statusText() const
{
    return m_status;
}

QStringList EvdevGamepadBackend::deviceNames() const
{
    QStringList names;
    for (const auto &device : m_devices) {
        if (device->fd >= 0)
            names.append(device->name);
    }
    return names;
}

void EvdevGamepadBackend::scheduleRescan()
{
    m_rescanTimer.start(); // restarts the debounce window
}

void EvdevGamepadBackend::rescan()
{
    const QDir dir(m_inputDir);
    const QStringList entries =
        dir.entryList({QStringLiteral("event*")}, QDir::System | QDir::Files | QDir::NoDotAndDotDot,
                      QDir::Name);

    m_eventNodeCount = static_cast<int>(entries.size());
    m_permissionDeniedCount = 0;
    for (const QString &entry : entries) {
        const QString path = dir.absoluteFilePath(entry);
        const bool known = std::any_of(m_devices.cbegin(), m_devices.cend(),
                                       [&](const auto &d) { return d->fd >= 0 && d->path == path; });
        if (known)
            continue;
        int err = 0;
        if (auto device = openDevice(path, &err)) {
            qCInfo(lcInput).noquote() << "Gamepad connected:" << device->name << '(' + path + ')';
            m_devices.push_back(std::move(device));
        } else if (err == EACCES || err == EPERM) {
            ++m_permissionDeniedCount;
        }
    }
    // The watcher may drop the path if /dev/input was recreated.
    if (m_started && !m_watcher.directories().contains(m_inputDir) && QFileInfo(m_inputDir).isDir())
        m_watcher.addPath(m_inputDir);
    updateStatus();
}

std::unique_ptr<EvdevGamepadBackend::Device> EvdevGamepadBackend::openDevice(const QString &path,
                                                                             int *openErrno)
{
    const QByteArray nativePath = QFile::encodeName(path);
    const int fd = ::open(nativePath.constData(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) {
        *openErrno = errno;
        if (errno != EACCES && errno != EPERM && errno != ENOENT && errno != ENODEV)
            qCDebug(lcInput) << "Cannot open" << path << ':' << errnoString(errno);
        return nullptr;
    }

    unsigned long keyBits[kKeyBitsLongs] = {};
    if (::ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(keyBits)), keyBits) < 0
        || !testBit(keyBits, BTN_GAMEPAD)) {
        ::close(fd); // not a gamepad (keyboard, mouse, power button, ...)
        return nullptr;
    }

    auto device = std::make_unique<Device>();
    device->fd = fd;
    device->path = path;

    char name[256] = {};
    if (::ioctl(fd, EVIOCGNAME(sizeof(name) - 1), name) >= 0 && name[0] != '\0')
        device->name = QString::fromUtf8(name).trimmed();
    if (device->name.isEmpty())
        device->name = QFileInfo(path).fileName();

    static constexpr std::array<std::pair<int, GamepadAxis>, 4> kAxes{{
        {ABS_X, GamepadAxis::LeftX},
        {ABS_Y, GamepadAxis::LeftY},
        {ABS_HAT0X, GamepadAxis::HatX},
        {ABS_HAT0Y, GamepadAxis::HatY},
    }};
    for (const auto &[code, axis] : kAxes) {
        auto &range = device->ranges[static_cast<std::size_t>(axis)];
        if (axis == GamepadAxis::HatX || axis == GamepadAxis::HatY)
            range = {-1, 1};
        input_absinfo info{};
        if (::ioctl(fd, EVIOCGABS(code), &info) >= 0 && info.maximum > info.minimum)
            range = {info.minimum, info.maximum};
    }

    device->notifier = new QSocketNotifier(fd, QSocketNotifier::Read, this);
    connect(device->notifier, &QSocketNotifier::activated, this, [this, fd] { readDevice(fd); });
    return device;
}

void EvdevGamepadBackend::readDevice(int fd)
{
    auto it = std::find_if(m_devices.begin(), m_devices.end(),
                           [fd](const auto &d) { return d->fd == fd; });
    if (it == m_devices.end())
        return;
    Device &device = **it;

    input_event events[64];
    for (;;) {
        const ssize_t bytes = ::read(fd, events, sizeof(events));
        if (bytes < 0) {
            if (errno == EINTR)
                continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                return;
            qCInfo(lcInput).noquote() << "Gamepad disconnected:" << device.name
                                      << '(' + errnoString(errno) + ')';
            closeDevice(device);
            navigator()->reset(); // drop anything held on the vanished device
            removeClosedDevices();
            updateStatus();
            return;
        }
        if (bytes == 0)
            return;

        const auto count = static_cast<std::size_t>(bytes) / sizeof(input_event);
        for (std::size_t i = 0; i < count; ++i) {
            const input_event &ev = events[i];
            if (ev.type == EV_SYN) {
                if (ev.code == SYN_DROPPED) {
                    // Kernel buffer overflowed: state is unknown until the next report.
                    device.dropping = true;
                    navigator()->reset();
                } else if (ev.code == SYN_REPORT) {
                    device.dropping = false;
                }
                continue;
            }
            if (device.dropping)
                continue;
            if (ev.type == EV_KEY) {
                if (ev.value == 2)
                    continue; // kernel autorepeat; the navigator does its own
                if (const auto button = evdevKeyToButton(ev.code))
                    navigator()->buttonEvent(*button, ev.value != 0);
            } else if (ev.type == EV_ABS) {
                if (const auto axis = evdevAbsToAxis(ev.code)) {
                    const auto &range = device.ranges[static_cast<std::size_t>(*axis)];
                    navigator()->axisEvent(*axis, normalizeAxis(ev.value, range.min, range.max));
                }
            }
        }
    }
}

void EvdevGamepadBackend::closeDevice(Device &device)
{
    if (device.notifier) {
        device.notifier->setEnabled(false);
        device.notifier->deleteLater(); // may be inside its own activated() emission
        device.notifier = nullptr;
    }
    if (device.fd >= 0) {
        ::close(device.fd);
        device.fd = -1;
    }
}

void EvdevGamepadBackend::removeClosedDevices()
{
    m_devices.erase(std::remove_if(m_devices.begin(), m_devices.end(),
                                   [](const auto &d) { return d->fd < 0; }),
                    m_devices.end());
}

void EvdevGamepadBackend::updateStatus()
{
    QString status;
    const QStringList names = deviceNames();
    if (!names.isEmpty()) {
        status = names.size() == 1 ? QStringLiteral("1 gamepad connected")
                                   : QStringLiteral("%1 gamepads connected").arg(names.size());
    } else if (m_permissionDeniedCount > 0) {
        status = QStringLiteral("No gamepad readable: %1 of %2 input devices in %3 are not "
                                "accessible (permission denied). Add your user to the "
                                "'input' group (sudo usermod -aG input $USER) and log in again.")
                     .arg(m_permissionDeniedCount)
                     .arg(m_eventNodeCount)
                     .arg(m_inputDir);
    } else {
        status = QStringLiteral("No gamepad detected; waiting for one to be connected");
    }
    // Unreadable nodes are only worth mentioning while no gamepad works: once one is open,
    // they are almost always keyboards/mice the launcher never needs.

    if (status == m_status && names == m_lastNames)
        return;
    if (status != m_status)
        qCInfo(lcInput).noquote() << "evdev:" << status;
    m_status = status;
    m_lastNames = names;
    emit statusChanged();
}

} // namespace nexora::input
