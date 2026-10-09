#pragma once

#include "input/GamepadInput.h"

#include <QtGlobal>

#include <optional>

namespace nexora::input {

// Maps a Linux evdev EV_KEY code to a gamepad button (nullopt for unrelated keys).
std::optional<GamepadButton> evdevKeyToButton(quint16 code);

// Maps a Linux evdev EV_ABS code to a navigation axis (nullopt for unused axes).
std::optional<GamepadAxis> evdevAbsToAxis(quint16 code);

// Maps |value| from [min, max] to [-1, 1] (clamped). Returns 0 for a degenerate range.
double normalizeAxis(qint32 value, qint32 min, qint32 max);

} // namespace nexora::input
