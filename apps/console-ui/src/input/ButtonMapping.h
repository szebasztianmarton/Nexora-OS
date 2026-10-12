#pragma once

#include "input/GamepadInput.h"
#include "input/NavAction.h"

#include <QHash>
#include <QString>
#include <QStringList>
#include <QVariantMap>

#include <optional>

namespace nexora::input {

// Which gamepad button triggers which non-directional action. Directions (D-pad, hat,
// left stick) are fixed. Stored in settings as {"accept": "south", ...} (action -> button).
//
// Buttons are named by position (south/east/west/north), not by label, because labels
// differ between layouts: Xbox A / PlayStation Cross / Nintendo B are all "south".
class ButtonMapping
{
public:
    static ButtonMapping defaults();

    // Validates |actionToButton|. Unknown names, a button bound twice or a missing
    // accept/back binding are errors; the result is then std::nullopt. Actions added in later
    // versions (currently "options") that the map does not mention get their default button
    // when it is free.
    static std::optional<ButtonMapping> fromVariantMap(const QVariantMap &actionToButton, QStringList *errors);
    QVariantMap toVariantMap() const;

    std::optional<NavAction> actionFor(GamepadButton button) const;
    std::optional<GamepadButton> buttonFor(NavAction action) const;

    // Rebinds |action| to |button|; an action previously bound to |button| swaps onto the
    // action's old button so that every action stays reachable.
    void bind(NavAction action, GamepadButton button);

    static QStringList actionNames(); // remappable actions, in display order
    static QStringList buttonNames();
    static std::optional<NavAction> actionFromName(const QString &name);
    static QString actionName(NavAction action);
    static std::optional<GamepadButton> buttonFromName(const QString &name);
    static QString buttonName(GamepadButton button);

    bool operator==(const ButtonMapping &) const = default;

private:
    QHash<NavAction, GamepadButton> m_actionToButton;
};

} // namespace nexora::input
