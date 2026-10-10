#pragma once

#include "input/ButtonMapping.h"
#include "input/GamepadInput.h"
#include "input/NavAction.h"

#include <QObject>
#include <QTimer>

#include <array>
#include <chrono>
#include <map>
#include <optional>

namespace nexora::input {

// Turns raw, backend-neutral gamepad state changes into navigation actions.
//
// Buttons map to actions through a configurable ButtonMapping (default: South -> Accept,
// East -> Back, Start -> Menu, North -> Favorite, West -> Search, L1/R1 -> previous/next
// tab); D-pad, hat and left stick always map to directions.
//
// Axes are edge-triggered with hysteresis (press at |v| >= 0.5, release at |v| < 0.3). A
// held direction auto-repeats (initial delay, then interval); only the most recently
// pressed direction repeats. Non-directional actions never repeat.
//
// Holding Select + Start together on one device for chordHoldDuration() emits
// returnToLauncherRequested() once per hold. The chord is tracked independently of
// navigation, so it keeps working while the InputController drops navigation actions
// (emulator in the foreground).
//
// State is kept per |device| (any backend-chosen id, e.g. an fd or SDL instance id), so
// several connected pads do not release or override each other's held directions.
class GamepadNavigator : public QObject
{
    Q_OBJECT

public:
    static constexpr double kAxisPressThreshold = 0.5;
    static constexpr double kAxisReleaseThreshold = 0.3;

    explicit GamepadNavigator(QObject *parent = nullptr);

    void buttonEvent(GamepadButton button, bool pressed, quint32 device = 0);
    void axisEvent(GamepadAxis axis, double normalized, quint32 device = 0);

    // Forgets all held state and stops auto-repeat (e.g. when input is suspended).
    void reset();
    // Forgets the state of one device (disconnect, dropped events).
    void resetDevice(quint32 device);

    void setRepeatDelays(std::chrono::milliseconds initial, std::chrono::milliseconds interval);
    std::chrono::milliseconds initialRepeatDelay() const { return m_initialDelay; }
    std::chrono::milliseconds repeatInterval() const { return m_interval; }
    void setChordHoldDuration(std::chrono::milliseconds duration);
    void setMapping(const ButtonMapping &mapping) { m_mapping = mapping; }
    const ButtonMapping &mapping() const { return m_mapping; }
    std::chrono::milliseconds chordHoldDuration() const { return m_chordHold; }

    // The direction currently auto-repeating, if any.
    std::optional<NavAction> repeatingAction() const;

signals:
    void actionTriggered(nexora::input::NavAction action);
    void returnToLauncherRequested();
    // Every non-directional button press, before mapping (used to capture remaps).
    void buttonPressed(nexora::input::GamepadButton button, quint32 device);

private:
    // Independent sources that can hold a direction.
    enum Source {
        SrcDpadUp,
        SrcDpadDown,
        SrcDpadLeft,
        SrcDpadRight,
        SrcLeftX,
        SrcLeftY,
        SrcHatX,
        SrcHatY,
        SourceCount
    };

    struct DeviceState
    {
        std::array<std::optional<NavAction>, SourceCount> held{};
        std::array<int, 4> axisState{}; // -1, 0, +1 per GamepadAxis
        bool selectHeld = false;
        bool startHeld = false;
    };

    struct SourceRef
    {
        quint32 device = 0;
        int source = -1;
    };

    void pressDirection(quint32 device, int source, NavAction action);
    void releaseSource(quint32 device, int source);
    void onRepeatTimeout();
    void updateChord();

    ButtonMapping m_mapping = ButtonMapping::defaults();
    std::map<quint32, DeviceState> m_devices;
    std::optional<SourceRef> m_repeat;
    QTimer m_repeatTimer;
    std::chrono::milliseconds m_initialDelay{400};
    std::chrono::milliseconds m_interval{120};
    QTimer m_chordTimer;
    std::optional<quint32> m_chordDevice;
    std::chrono::milliseconds m_chordHold{1000};
};

} // namespace nexora::input
