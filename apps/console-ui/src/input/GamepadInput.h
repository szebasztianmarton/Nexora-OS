#pragma once

namespace nexora::input {

// Backend-neutral gamepad buttons, named by position (Xbox A == South, B == East).
enum class GamepadButton {
    South,
    East,
    West,
    North,
    Start,
    Select,
    Guide,
    DpadUp,
    DpadDown,
    DpadLeft,
    DpadRight,
};

// Axes relevant for navigation. Values are normalized to -1..1; negative Y is "up".
enum class GamepadAxis { LeftX, LeftY, HatX, HatY };

} // namespace nexora::input
