#include "input/EvdevMapping.h"

#include <linux/input.h>

#include <algorithm>

namespace nexora::input {

namespace {

constexpr quint16 kVendorMicrosoft = 0x045e;
constexpr quint16 kVendorValve = 0x28de;
constexpr quint16 kVendorDragonRise = 0x0079;
constexpr quint16 kProductDragonRiseGamepad = 0x0006;

// Joystick-class pads, after SDL's gamecontrollerdb (Linux entries): buttons 1-4 are the
// face buttons, 5/6 the shoulders, 7/8 the triggers (unused), 9 Select and 10 Start.
std::optional<GamepadButton> joystickButton(quint16 code, bool playStationOrder)
{
    switch (code) {
    case BTN_TRIGGER: return playStationOrder ? GamepadButton::North : GamepadButton::West;
    case BTN_THUMB: return playStationOrder ? GamepadButton::East : GamepadButton::South;
    case BTN_THUMB2: return playStationOrder ? GamepadButton::South : GamepadButton::East;
    case BTN_TOP: return playStationOrder ? GamepadButton::West : GamepadButton::North;
    case BTN_TOP2: return GamepadButton::L1;
    case BTN_PINKIE: return GamepadButton::R1;
    case BTN_BASE3: return GamepadButton::Select;
    case BTN_BASE4: return GamepadButton::Start;
    default: return std::nullopt;
    }
}

} // namespace

EvdevLayout evdevLayout(bool gamepad, quint16 vendor, quint16 product, const QString &driver)
{
    if (!gamepad) {
        return vendor == kVendorDragonRise && product == kProductDragonRiseGamepad ? EvdevLayout::JoystickPlayStation
                                                                                   : EvdevLayout::Joystick;
    }
    if (driver == QLatin1String("xpad") || driver == QLatin1String("hid-steam") || vendor == kVendorMicrosoft
        || vendor == kVendorValve)
        return EvdevLayout::XboxLabels;
    return EvdevLayout::Positional;
}

QString evdevLayoutName(EvdevLayout layout)
{
    switch (layout) {
    case EvdevLayout::Positional: return QStringLiteral("positional");
    case EvdevLayout::XboxLabels: return QStringLiteral("xbox-labels");
    case EvdevLayout::Joystick: return QStringLiteral("joystick");
    case EvdevLayout::JoystickPlayStation: return QStringLiteral("joystick-playstation");
    }
    return {};
}

std::optional<GamepadButton> evdevKeyToButton(quint16 code, EvdevLayout layout)
{
    if (layout == EvdevLayout::Joystick || layout == EvdevLayout::JoystickPlayStation) {
        if (const auto button = joystickButton(code, layout == EvdevLayout::JoystickPlayStation))
            return button;
    } else if (layout == EvdevLayout::XboxLabels) {
        if (code == BTN_X)
            return GamepadButton::West;
        if (code == BTN_Y)
            return GamepadButton::North;
    }

    switch (code) {
    case BTN_SOUTH: return GamepadButton::South; // == BTN_A
    case BTN_EAST: return GamepadButton::East; // == BTN_B
    case BTN_NORTH: return GamepadButton::North; // == BTN_X
    case BTN_WEST: return GamepadButton::West; // == BTN_Y
    case BTN_START: return GamepadButton::Start;
    case BTN_SELECT: return GamepadButton::Select;
    case BTN_MODE: return GamepadButton::Guide;
    case BTN_DPAD_UP: return GamepadButton::DpadUp;
    case BTN_DPAD_DOWN: return GamepadButton::DpadDown;
    case BTN_DPAD_LEFT: return GamepadButton::DpadLeft;
    case BTN_DPAD_RIGHT: return GamepadButton::DpadRight;
    case BTN_TL: return GamepadButton::L1;
    case BTN_TR: return GamepadButton::R1;
    default: return std::nullopt;
    }
}

std::optional<GamepadAxis> evdevAbsToAxis(quint16 code)
{
    switch (code) {
    case ABS_X: return GamepadAxis::LeftX;
    case ABS_Y: return GamepadAxis::LeftY;
    case ABS_HAT0X: return GamepadAxis::HatX;
    case ABS_HAT0Y: return GamepadAxis::HatY;
    default: return std::nullopt;
    }
}

double normalizeAxis(qint32 value, qint32 min, qint32 max)
{
    if (max <= min)
        return 0.0;
    const double range = static_cast<double>(max) - static_cast<double>(min);
    const double normalized = (static_cast<double>(value) - static_cast<double>(min)) * 2.0 / range - 1.0;
    return std::clamp(normalized, -1.0, 1.0);
}

} // namespace nexora::input
