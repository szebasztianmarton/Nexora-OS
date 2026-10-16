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
// Axes are edge-triggered with hysteresis around a configurable dead zone (setDeadzone()):
// readings with |v| < deadzone count as centred, a direction is pressed at
// |v| >= deadzone + (1 - deadzone) * 0.4 and released below
// deadzone + (1 - deadzone) * 0.15. A held direction auto-repeats (initial delay, then
// interval); only the most recently pressed direction repeats.
//
// Non-directional actions never repeat, and neither does a non-directional button that is
// re-reported as pressed without an intervening release (OS/kernel key autorepeat): Confirm,
// Back and the rest fire once per physical press. Only navigation repeats.
//
// Holding Select + Start together on one device for chordHoldDuration() emits
// returnToLauncherRequested() once per hold. The chord is tracked independently of
// navigation, so it keeps working while the InputController drops navigation actions
// (emulator in the foreground).
//
// Holding Start alone (Select not held on that device) for menuHoldDuration() emits
// quickMenuRequested() once per hold; like the chord it is tracked independently of
// navigation. The normal Menu action still fires on the press edge.
//
// State is kept per |device| (any backend-chosen id, e.g. an fd or SDL instance id), so
// several connected pads do not release or override each other's held directions.
class GamepadNavigator : public QObject
{
    Q_OBJECT

public:
    // setDeadzone() clamps to this range; it matches the 5-60 percent range of the
    // "input.deadzone" setting.
    static constexpr double kMinDeadzone = 0.05;
    static constexpr double kMaxDeadzone = 0.6;
    // Same as the "input.deadzone" default of 25 percent.
    static constexpr double kDefaultDeadzone = 0.25;

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
    void setMenuHoldDuration(std::chrono::milliseconds duration);
    std::chrono::milliseconds menuHoldDuration() const { return m_menuHold; }

    // Stick/hat dead zone as a fraction of full travel, clamped to
    // [kMinDeadzone, kMaxDeadzone]. Non-finite values are ignored. See the class comment
    // for the press/release formulas.
    void setDeadzone(double fraction);
    double deadzone() const { return m_deadzone; }
    double axisPressThreshold() const { return m_deadzone + (1.0 - m_deadzone) * 0.4; }
    double axisReleaseThreshold() const { return m_deadzone + (1.0 - m_deadzone) * 0.15; }

    // The direction currently auto-repeating, if any.
    std::optional<NavAction> repeatingAction() const;

signals:
    void actionTriggered(nexora::input::NavAction action);
    void returnToLauncherRequested();
    // Start held on its own for menuHoldDuration() (see the class comment).
    void quickMenuRequested();
    // Every non-directional button press, before mapping (used to capture remaps).
    void buttonPressed(nexora::input::GamepadButton button, quint32 device);
    // Every button edge as it arrived from the backend, before mapping, autorepeat
    // filtering and the directional split (used by the controller test screen).
    void rawButtonEvent(nexora::input::GamepadButton button, bool pressed, quint32 device);
    // Every finite axis reading as it arrived from the backend, before the dead zone and
    // the press/release thresholds (used by the controller test screen).
    void rawAxisEvent(nexora::input::GamepadAxis axis, double value, quint32 device);

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

    // One slot per GamepadButton; used to drop autorepeated presses of non-directional
    // buttons (directions are deduplicated through |held| instead).
    static constexpr std::size_t kButtonCount = static_cast<std::size_t>(GamepadButton::R1) + 1;

    struct DeviceState
    {
        std::array<std::optional<NavAction>, SourceCount> held{};
        std::array<int, 4> axisState{}; // -1, 0, +1 per GamepadAxis
        std::array<bool, kButtonCount> buttonHeld{};
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
    void updateMenuHold();

    ButtonMapping m_mapping = ButtonMapping::defaults();
    std::map<quint32, DeviceState> m_devices;
    std::optional<SourceRef> m_repeat;
    QTimer m_repeatTimer;
    std::chrono::milliseconds m_initialDelay{400};
    std::chrono::milliseconds m_interval{120};
    QTimer m_chordTimer;
    std::optional<quint32> m_chordDevice;
    std::chrono::milliseconds m_chordHold{1000};
    QTimer m_menuHoldTimer;
    std::optional<quint32> m_menuHoldDevice;
    std::chrono::milliseconds m_menuHold{1000};
    double m_deadzone = kDefaultDeadzone;
};

} // namespace nexora::input
