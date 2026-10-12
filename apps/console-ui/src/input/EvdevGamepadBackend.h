#pragma once

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
// Only devices advertising BTN_GAMEPAD are kept (keyboards and mice are never held open).
// Devices are opened non-blocking and never grabbed, so other programs (e.g. a running
// emulator) still receive the same input. Hotplug is handled by watching /dev/input.
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
        bool dropping = false; // after SYN_DROPPED until the next SYN_REPORT
    };

    void scheduleRescan();
    void rescan();
    std::unique_ptr<Device> openDevice(const QString &path, int *openErrno);
    void readDevice(int fd);
    void closeDevice(Device &device);
    void removeClosedDevices();
    void updateStatus();

    QString m_inputDir = QStringLiteral("/dev/input");
    std::vector<std::unique_ptr<Device>> m_devices;
    QFileSystemWatcher m_watcher;
    QTimer m_rescanTimer;
    QString m_status;
    QStringList m_lastNames;
    int m_eventNodeCount = 0;
    int m_permissionDeniedCount = 0;
    bool m_started = false;
};

} // namespace nexora::input
