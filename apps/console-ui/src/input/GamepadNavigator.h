#pragma once

#include "input/GamepadInput.h"
#include "input/NavAction.h"

#include <QObject>
#include <QTimer>

#include <array>
#include <chrono>
#include <optional>

namespace nexora::input {

// Turns raw, backend-neutral gamepad state changes into navigation actions.
//
// South -> Accept, East -> Back, Start/Select/Guide -> Menu; D-pad, hat and left stick map
// to directions. Axes are edge-triggered with hysteresis (press at |v| >= 0.5, release at
// |v| < 0.3). A held direction auto-repeats (initial delay, then interval); only the most
// recently pressed direction repeats. Accept/Back/Menu never repeat.
class GamepadNavigator : public QObject
{
    Q_OBJECT

public:
    static constexpr double kAxisPressThreshold = 0.5;
    static constexpr double kAxisReleaseThreshold = 0.3;

    explicit GamepadNavigator(QObject *parent = nullptr);

    void buttonEvent(GamepadButton button, bool pressed);
    void axisEvent(GamepadAxis axis, double normalized);

    // Forgets all held state and stops auto-repeat (e.g. after dropped events, a device
    // disconnect or when input is suspended).
    void reset();

    void setRepeatDelays(std::chrono::milliseconds initial, std::chrono::milliseconds interval);
    std::chrono::milliseconds initialRepeatDelay() const { return m_initialDelay; }
    std::chrono::milliseconds repeatInterval() const { return m_interval; }

    // The direction currently auto-repeating, if any.
    std::optional<NavAction> repeatingAction() const;

signals:
    void actionTriggered(nexora::input::NavAction action);

private:
    // Independent sources that can hold a direction.
    enum Source { SrcDpadUp, SrcDpadDown, SrcDpadLeft, SrcDpadRight, SrcLeftX, SrcLeftY,
                  SrcHatX, SrcHatY, SourceCount };

    void pressDirection(int source, NavAction action);
    void releaseSource(int source);
    void onRepeatTimeout();

    std::array<std::optional<NavAction>, SourceCount> m_held{};
    std::array<int, 4> m_axisState{}; // -1, 0, +1 per GamepadAxis
    int m_repeatSource = -1;
    QTimer m_repeatTimer;
    std::chrono::milliseconds m_initialDelay{400};
    std::chrono::milliseconds m_interval{120};
};

} // namespace nexora::input
