#pragma once

#include <QObject>

namespace nexora::input {
Q_NAMESPACE

// Backend-neutral UI navigation actions produced from controller input.
enum class NavAction { Up, Down, Left, Right, Accept, Back, Menu, Favorite, Search, PrevTab, NextTab, GameBar };
Q_ENUM_NS(NavAction)

} // namespace nexora::input
