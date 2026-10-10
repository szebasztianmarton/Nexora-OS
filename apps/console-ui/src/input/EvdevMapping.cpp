#include "input/EvdevMapping.h"

#include <linux/input.h>

#include <algorithm>

namespace nexora::input {

std::optional<GamepadButton> evdevKeyToButton(quint16 code)
{
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
