#pragma once

#include "input/GamepadInput.h"

#include <QString>
#include <QtGlobal>

#include <optional>

namespace nexora::input {

// How a device reports its buttons, which decides how evdev key codes map to positions.
enum class EvdevLayout {
    // Kernel gamepad convention: BTN_NORTH/BTN_WEST are the top/left face buttons
    // (hid-playstation, hid-sony, hid-nintendo, hid-wiimote, most others).
    Positional,
    // Xbox labels: BTN_X/BTN_Y are the Xbox X (left) and Y (top) buttons, although the
    // kernel defines them as BTN_NORTH/BTN_WEST (xpad, hid-steam, Microsoft pads over
    // Bluetooth through hid-generic). West and North are swapped back.
    XboxLabels,
    // No BTN_GAMEPAD: numbered joystick buttons BTN_TRIGGER, BTN_THUMB, ... in the order of
    // the Logitech Dual Action / RumblePad 2, the most common PC pad layout (generic HID
    // pads, wheels, flight sticks).
    Joystick,
    // Numbered joystick buttons in PlayStation order (DragonRise "USB Gamepad" 0079:0006).
    JoystickPlayStation,
};

// Picks the layout of a device. |gamepad| is whether it advertises BTN_GAMEPAD, |driver|
// the kernel driver bound to it (sysfs; empty when unknown, e.g. for uinput devices).
EvdevLayout evdevLayout(bool gamepad, quint16 vendor, quint16 product, const QString &driver);

// Short name of |layout| for logs ("positional", "xbox-labels", ...).
QString evdevLayoutName(EvdevLayout layout);

// Maps a Linux evdev EV_KEY code to a gamepad button (nullopt for unrelated keys).
std::optional<GamepadButton> evdevKeyToButton(quint16 code, EvdevLayout layout = EvdevLayout::Positional);

// Maps a Linux evdev EV_ABS code to a navigation axis (nullopt for unused axes).
std::optional<GamepadAxis> evdevAbsToAxis(quint16 code);

// Maps |value| from [min, max] to [-1, 1] (clamped). Returns 0 for a degenerate range.
double normalizeAxis(qint32 value, qint32 min, qint32 max);

} // namespace nexora::input
