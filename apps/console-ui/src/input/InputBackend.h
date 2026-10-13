#pragma once

#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>

namespace nexora::input {

class GamepadNavigator;

// A source of gamepad input (evdev, SDL3, ...) that feeds a GamepadNavigator.
// Backends run on the thread that owns them (the GUI thread) and never block it.
class InputBackend : public QObject
{
    Q_OBJECT

public:
    // One connected gamepad. |id| is stable for as long as the device stays connected and
    // unique within the backend (evdev: the device node, SDL3: the joystick id); it is what
    // the InputController tracks connects and disconnects by.
    struct DeviceInfo
    {
        QString id;
        QString name;
    };

    explicit InputBackend(GamepadNavigator *navigator, QObject *parent = nullptr)
        : QObject(parent)
        , m_navigator(navigator)
    { }

    // Starts listening. Returns false if the backend cannot work at all on this system.
    virtual bool start() = 0;
    virtual QString name() const = 0;
    // Short user-facing description of the backend state (shown on the diagnostics screen).
    virtual QString statusText() const = 0;
    // The currently connected gamepads. statusChanged() is emitted whenever this changes.
    virtual QList<DeviceInfo> devices() const = 0;

    // Names of the currently connected gamepads, in devices() order.
    QStringList deviceNames() const
    {
        QStringList names;
        const QList<DeviceInfo> list = devices();
        names.reserve(list.size());
        for (const DeviceInfo &device : list)
            names.append(device.name);
        return names;
    }

signals:
    void statusChanged();

protected:
    GamepadNavigator *navigator() const { return m_navigator; }

private:
    GamepadNavigator *m_navigator;
};

} // namespace nexora::input
