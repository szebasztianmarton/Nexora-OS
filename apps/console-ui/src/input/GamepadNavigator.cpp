#include "input/GamepadNavigator.h"

#include <algorithm>
#include <cmath>

namespace nexora::input {

GamepadNavigator::GamepadNavigator(QObject *parent)
    : QObject(parent)
{
    m_repeatTimer.setTimerType(Qt::PreciseTimer);
    connect(&m_repeatTimer, &QTimer::timeout, this, &GamepadNavigator::onRepeatTimeout);
    m_chordTimer.setSingleShot(true);
    connect(&m_chordTimer, &QTimer::timeout, this, &GamepadNavigator::returnToLauncherRequested);
    m_menuHoldTimer.setSingleShot(true);
    connect(&m_menuHoldTimer, &QTimer::timeout, this, &GamepadNavigator::quickMenuRequested);
}

void GamepadNavigator::buttonEvent(GamepadButton button, bool pressed, quint32 device)
{
    emit rawButtonEvent(button, pressed, device);

    auto directional = [&](int source, NavAction action) {
        if (pressed)
            pressDirection(device, source, action);
        else
            releaseSource(device, source);
    };

    switch (button) {
    case GamepadButton::DpadUp: directional(SrcDpadUp, NavAction::Up); return;
    case GamepadButton::DpadDown: directional(SrcDpadDown, NavAction::Down); return;
    case GamepadButton::DpadLeft: directional(SrcDpadLeft, NavAction::Left); return;
    case GamepadButton::DpadRight: directional(SrcDpadRight, NavAction::Right); return;
    case GamepadButton::South:
    case GamepadButton::East:
    case GamepadButton::West:
    case GamepadButton::North:
    case GamepadButton::Start:
    case GamepadButton::Select:
    case GamepadButton::Guide:
    case GamepadButton::L1:
    case GamepadButton::R1: break;
    }

    DeviceState &state = m_devices[device];
    bool &held = state.buttonHeld[static_cast<std::size_t>(button)];
    const bool wasHeld = held;
    held = pressed;
    if (pressed && wasHeld)
        return; // autorepeat of a button that is already down: act once per physical press

    if (button == GamepadButton::Start || button == GamepadButton::Select) {
        (button == GamepadButton::Start ? state.startHeld : state.selectHeld) = pressed;
        updateChord();
        updateMenuHold();
    }
    if (!pressed)
        return;
    emit buttonPressed(button, device);
    if (const auto action = m_mapping.actionFor(button))
        emit actionTriggered(*action);
}

void GamepadNavigator::axisEvent(GamepadAxis axis, double normalized, quint32 device)
{
    if (!std::isfinite(normalized))
        return;
    emit rawAxisEvent(axis, normalized, device);

    const auto index = static_cast<std::size_t>(axis);
    int source = SrcLeftX;
    bool horizontal = true;
    switch (axis) {
    case GamepadAxis::LeftX:
        source = SrcLeftX;
        horizontal = true;
        break;
    case GamepadAxis::LeftY:
        source = SrcLeftY;
        horizontal = false;
        break;
    case GamepadAxis::HatX:
        source = SrcHatX;
        horizontal = true;
        break;
    case GamepadAxis::HatY:
        source = SrcHatY;
        horizontal = false;
        break;
    }

    DeviceState &state = m_devices[device];
    const int previous = state.axisState[index];
    const double travel = std::abs(normalized);
    // Inside the dead zone the stick counts as centred, whatever it actually reports.
    const double magnitude = travel < m_deadzone ? 0.0 : travel;
    const double press = axisPressThreshold();
    const double release = axisReleaseThreshold();
    int next = previous;
    if (previous == 0) {
        if (magnitude >= press)
            next = normalized < 0 ? -1 : 1;
    } else if (magnitude < release) {
        next = 0;
    } else if ((normalized < 0 ? -1 : 1) != previous && magnitude >= press) {
        next = -previous; // flicked straight to the opposite side
    }

    if (next == previous)
        return;

    state.axisState[index] = next;
    releaseSource(device, source);
    if (next != 0) {
        const NavAction action = horizontal ? (next < 0 ? NavAction::Left : NavAction::Right)
                                            : (next < 0 ? NavAction::Up : NavAction::Down);
        pressDirection(device, source, action);
    }
}

void GamepadNavigator::updateChord()
{
    // Keep a running chord as long as its device still holds both buttons.
    if (m_chordDevice) {
        const auto it = m_devices.find(*m_chordDevice);
        if (it != m_devices.end() && it->second.selectHeld && it->second.startHeld)
            return;
        m_chordTimer.stop();
        m_chordDevice.reset();
    }
    for (const auto &[device, state] : m_devices) {
        if (state.selectHeld && state.startHeld) {
            m_chordDevice = device;
            m_chordTimer.start(m_chordHold);
            return;
        }
    }
}

void GamepadNavigator::updateMenuHold()
{
    const auto holdsStartAlone = [](const DeviceState &state) { return state.startHeld && !state.selectHeld; };
    if (m_menuHoldDevice) {
        const auto it = m_devices.find(*m_menuHoldDevice);
        if (it != m_devices.end() && holdsStartAlone(it->second))
            return;
        m_menuHoldTimer.stop();
        m_menuHoldDevice.reset();
    }
    for (const auto &[device, state] : m_devices) {
        if (holdsStartAlone(state)) {
            m_menuHoldDevice = device;
            m_menuHoldTimer.start(m_menuHold);
            return;
        }
    }
}

void GamepadNavigator::setMenuHoldDuration(std::chrono::milliseconds duration)
{
    m_menuHold = std::max(duration, std::chrono::milliseconds{1});
}

void GamepadNavigator::setChordHoldDuration(std::chrono::milliseconds duration)
{
    m_chordHold = std::max(duration, std::chrono::milliseconds{1});
}

void GamepadNavigator::setDeadzone(double fraction)
{
    if (!std::isfinite(fraction))
        return;
    m_deadzone = std::clamp(fraction, kMinDeadzone, kMaxDeadzone);
}

void GamepadNavigator::reset()
{
    m_devices.clear();
    m_chordTimer.stop();
    m_chordDevice.reset();
    m_menuHoldTimer.stop();
    m_menuHoldDevice.reset();
    m_repeatTimer.stop();
    m_repeat.reset();
}

void GamepadNavigator::resetDevice(quint32 device)
{
    m_devices.erase(device);
    if (m_repeat && m_repeat->device == device) {
        m_repeat.reset();
        m_repeatTimer.stop();
    }
    updateChord();
    updateMenuHold();
}

void GamepadNavigator::setRepeatDelays(std::chrono::milliseconds initial, std::chrono::milliseconds interval)
{
    m_initialDelay = std::max(initial, std::chrono::milliseconds{1});
    m_interval = std::max(interval, std::chrono::milliseconds{1});
}

std::optional<NavAction> GamepadNavigator::repeatingAction() const
{
    if (!m_repeat)
        return std::nullopt;
    const auto it = m_devices.find(m_repeat->device);
    if (it == m_devices.end())
        return std::nullopt;
    return it->second.held[static_cast<std::size_t>(m_repeat->source)];
}

void GamepadNavigator::pressDirection(quint32 device, int source, NavAction action)
{
    m_devices[device].held[static_cast<std::size_t>(source)] = action;
    m_repeat = SourceRef{device, source}; // latest press wins, across all devices
    emit actionTriggered(action);
    // A slot may have called reset() in response; only arm the timer if still held.
    if (repeatingAction())
        m_repeatTimer.start(m_initialDelay);
}

void GamepadNavigator::releaseSource(quint32 device, int source)
{
    const auto it = m_devices.find(device);
    if (it != m_devices.end())
        it->second.held[static_cast<std::size_t>(source)] = std::nullopt;
    if (m_repeat && m_repeat->device == device && m_repeat->source == source) {
        // Like keyboard auto-repeat: releasing the repeating key stops repetition even if
        // an older direction is still held.
        m_repeat.reset();
        m_repeatTimer.stop();
    }
}

void GamepadNavigator::onRepeatTimeout()
{
    const auto action = repeatingAction();
    if (!action) {
        m_repeatTimer.stop();
        return;
    }
    if (m_repeatTimer.intervalAsDuration() != m_interval)
        m_repeatTimer.setInterval(m_interval);
    emit actionTriggered(*action);
}

} // namespace nexora::input
