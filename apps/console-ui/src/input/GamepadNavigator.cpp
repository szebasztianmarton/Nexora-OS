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
}

void GamepadNavigator::buttonEvent(GamepadButton button, bool pressed)
{
    auto directional = [&](int source, NavAction action) {
        if (pressed)
            pressDirection(source, action);
        else
            releaseSource(source);
    };

    switch (button) {
    case GamepadButton::DpadUp:
        directional(SrcDpadUp, NavAction::Up);
        return;
    case GamepadButton::DpadDown:
        directional(SrcDpadDown, NavAction::Down);
        return;
    case GamepadButton::DpadLeft:
        directional(SrcDpadLeft, NavAction::Left);
        return;
    case GamepadButton::DpadRight:
        directional(SrcDpadRight, NavAction::Right);
        return;
    case GamepadButton::South:
        if (pressed)
            emit actionTriggered(NavAction::Accept);
        return;
    case GamepadButton::East:
        if (pressed)
            emit actionTriggered(NavAction::Back);
        return;
    case GamepadButton::Start:
    case GamepadButton::Select:
        (button == GamepadButton::Start ? m_startHeld : m_selectHeld) = pressed;
        updateChord();
        if (pressed)
            emit actionTriggered(NavAction::Menu);
        return;
    case GamepadButton::Guide:
        if (pressed)
            emit actionTriggered(NavAction::Menu);
        return;
    case GamepadButton::West:
    case GamepadButton::North:
        return; // unused in MVP 1
    }
}

void GamepadNavigator::axisEvent(GamepadAxis axis, double normalized)
{
    if (!std::isfinite(normalized))
        return;

    const auto index = static_cast<std::size_t>(axis);
    int source = SrcLeftX;
    bool horizontal = true;
    switch (axis) {
    case GamepadAxis::LeftX: source = SrcLeftX; horizontal = true; break;
    case GamepadAxis::LeftY: source = SrcLeftY; horizontal = false; break;
    case GamepadAxis::HatX: source = SrcHatX; horizontal = true; break;
    case GamepadAxis::HatY: source = SrcHatY; horizontal = false; break;
    }

    const int previous = m_axisState[index];
    const double magnitude = std::abs(normalized);
    int next = previous;
    if (previous == 0) {
        if (magnitude >= kAxisPressThreshold)
            next = normalized < 0 ? -1 : 1;
    } else if (magnitude < kAxisReleaseThreshold) {
        next = 0;
    } else if ((normalized < 0 ? -1 : 1) != previous && magnitude >= kAxisPressThreshold) {
        next = -previous; // flicked straight to the opposite side
    }

    if (next == previous)
        return;

    m_axisState[index] = next;
    releaseSource(source);
    if (next != 0) {
        const NavAction action = horizontal ? (next < 0 ? NavAction::Left : NavAction::Right)
                                            : (next < 0 ? NavAction::Up : NavAction::Down);
        pressDirection(source, action);
    }
}

void GamepadNavigator::updateChord()
{
    if (m_selectHeld && m_startHeld) {
        if (!m_chordTimer.isActive())
            m_chordTimer.start(m_chordHold);
    } else {
        m_chordTimer.stop();
    }
}

void GamepadNavigator::setChordHoldDuration(std::chrono::milliseconds duration)
{
    m_chordHold = std::max(duration, std::chrono::milliseconds{1});
}

void GamepadNavigator::reset()
{
    m_selectHeld = false;
    m_startHeld = false;
    m_chordTimer.stop();
    m_repeatTimer.stop();
    m_repeatSource = -1;
    m_held.fill(std::nullopt);
    m_axisState.fill(0);
}

void GamepadNavigator::setRepeatDelays(std::chrono::milliseconds initial,
                                       std::chrono::milliseconds interval)
{
    m_initialDelay = std::max(initial, std::chrono::milliseconds{1});
    m_interval = std::max(interval, std::chrono::milliseconds{1});
}

std::optional<NavAction> GamepadNavigator::repeatingAction() const
{
    if (m_repeatSource < 0)
        return std::nullopt;
    return m_held[static_cast<std::size_t>(m_repeatSource)];
}

void GamepadNavigator::pressDirection(int source, NavAction action)
{
    m_held[static_cast<std::size_t>(source)] = action;
    m_repeatSource = source; // latest press wins
    emit actionTriggered(action);
    // A slot may have called reset() in response; only arm the timer if still held.
    if (m_repeatSource == source && m_held[static_cast<std::size_t>(source)])
        m_repeatTimer.start(m_initialDelay);
}

void GamepadNavigator::releaseSource(int source)
{
    m_held[static_cast<std::size_t>(source)] = std::nullopt;
    if (m_repeatSource == source) {
        // Like keyboard auto-repeat: releasing the repeating key stops repetition even if
        // an older direction is still held.
        m_repeatSource = -1;
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
