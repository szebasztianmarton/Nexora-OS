#pragma once

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
    explicit InputBackend(GamepadNavigator *navigator, QObject *parent = nullptr)
        : QObject(parent)
        , m_navigator(navigator)
    {
    }

    // Starts listening. Returns false if the backend cannot work at all on this system.
    virtual bool start() = 0;
    virtual QString name() const = 0;
    // Short user-facing description of the backend state (shown on the diagnostics screen).
    virtual QString statusText() const = 0;
    // Names of the currently connected gamepads.
    virtual QStringList deviceNames() const = 0;

signals:
    void statusChanged();

protected:
    GamepadNavigator *navigator() const { return m_navigator; }

private:
    GamepadNavigator *m_navigator;
};

} // namespace nexora::input
