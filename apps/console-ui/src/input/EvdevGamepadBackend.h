#pragma once

#include "input/EvdevMapping.h"
#include "input/GamepadInput.h"
#include "input/InputBackend.h"

#include <QFileSystemWatcher>
#include <QString>
#include <QTimer>

#include <array>
#include <memory>
#include <vector>

class QSocketNotifier;

namespace nexora::input {

// Reads gamepads directly from /dev/input/event* (Linux evdev).
//
// Kept devices: gamepads (BTN_GAMEPAD) and joystick-class pads, wheels and sticks (numbered
// BTN_TRIGGER... buttons plus ABS_X/ABS_Y); keyboards, mice and motion sensors are never held
// open. Each device gets an EvdevLayout from its vendor and kernel driver, which corrects
// drivers that report Xbox labels or numbered buttons. Devices are opened non-blocking and
// never grabbed, so other programs (e.g. a running emulator) still receive the same input.
// Hotplug is handled by watching /dev/input.
class EvdevGamepadBackend : public InputBackend
{
    Q_OBJECT

public:
    explicit EvdevGamepadBackend(GamepadNavigator *navigator, QObject *parent = nullptr);
    ~EvdevGamepadBackend() override;

    bool start() override;
    QString name() const override;
    QString statusText() const override;
    QList<DeviceInfo> devices() const override;

    void setInputDirectory(const QString &path) { m_inputDir = path; } // for tests
    void setSysfsDirectory(const QString &path) { m_sysfsDir = path; } // for tests

private:
    struct AxisRange
    {
        qint32 min = -32768;
        qint32 max = 32767;
    };

    struct Device
    {
        int fd = -1;
        QString path;
        QString name;
        QSocketNotifier *notifier = nullptr;
        std::array<AxisRange, 4> ranges{}; // indexed by GamepadAxis
        EvdevLayout layout = EvdevLayout::Positional;
        bool dropping = false; // after SYN_DROPPED until the next SYN_REPORT
    };

    void scheduleRescan();
    void rescan();
    std::unique_ptr<Device> openDevice(const QString &path, int *openErrno);
    void readDevice(int fd);
    void closeDevice(Device &device);
    void removeClosedDevices();
    void updateStatus();
    QString driverOf(const QString &eventNode) const;

    QString m_inputDir = QStringLiteral("/dev/input");
    QString m_sysfsDir = QStringLiteral("/sys");
    std::vector<std::unique_ptr<Device>> m_devices;
    QFileSystemWatcher m_watcher;
    QTimer m_rescanTimer;
    QTimer m_permissionRetryTimer;
    int m_permissionRetries = 0;
    QString m_status;
    QStringList m_lastNames;
    int m_eventNodeCount = 0;
    int m_permissionDeniedCount = 0;
    bool m_started = false;
};

} // namespace nexora::input
