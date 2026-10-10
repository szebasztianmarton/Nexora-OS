#include "input/ButtonMapping.h"

namespace nexora::input {

namespace {

struct ActionName
{
    NavAction action;
    const char *name;
};
constexpr ActionName kActions[] = {
    {NavAction::Accept, "accept"},     {NavAction::Back, "back"},     {NavAction::Menu, "menu"},
    {NavAction::Favorite, "favorite"}, {NavAction::Search, "search"}, {NavAction::PrevTab, "prevTab"},
    {NavAction::NextTab, "nextTab"},
};

struct ButtonName
{
    GamepadButton button;
    const char *name;
};
constexpr ButtonName kButtons[] = {
    {GamepadButton::South, "south"}, {GamepadButton::East, "east"},   {GamepadButton::West, "west"},
    {GamepadButton::North, "north"}, {GamepadButton::Start, "start"}, {GamepadButton::Select, "select"},
    {GamepadButton::Guide, "guide"}, {GamepadButton::L1, "l1"},       {GamepadButton::R1, "r1"},
};

} // namespace

ButtonMapping ButtonMapping::defaults()
{
    ButtonMapping m;
    m.m_actionToButton = {
        {NavAction::Accept, GamepadButton::South}, {NavAction::Back, GamepadButton::East},
        {NavAction::Menu, GamepadButton::Start},   {NavAction::Favorite, GamepadButton::North},
        {NavAction::Search, GamepadButton::West},  {NavAction::PrevTab, GamepadButton::L1},
        {NavAction::NextTab, GamepadButton::R1},
    };
    return m;
}

std::optional<ButtonMapping> ButtonMapping::fromVariantMap(const QVariantMap &actionToButton, QStringList *errors)
{
    QStringList problems;
    ButtonMapping m;
    QHash<GamepadButton, QString> used;
    for (auto it = actionToButton.cbegin(); it != actionToButton.cend(); ++it) {
        const auto action = actionFromName(it.key());
        const auto button = buttonFromName(it.value().toString());
        if (!action) {
            problems.append(QStringLiteral("unknown action \"%1\"").arg(it.key()));
            continue;
        }
        if (!button) {
            problems.append(QStringLiteral("unknown button \"%1\" for %2").arg(it.value().toString(), it.key()));
            continue;
        }
        if (used.contains(*button)) {
            problems.append(QStringLiteral("button \"%1\" is assigned to both %2 and %3")
                                .arg(it.value().toString(), used.value(*button), it.key()));
            continue;
        }
        used.insert(*button, it.key());
        m.m_actionToButton.insert(*action, *button);
    }
    for (NavAction required : {NavAction::Accept, NavAction::Back}) {
        if (!m.m_actionToButton.contains(required))
            problems.append(QStringLiteral("\"%1\" must be assigned to a button").arg(actionName(required)));
    }
    if (!problems.isEmpty()) {
        if (errors)
            errors->append(problems);
        return std::nullopt;
    }
    return m;
}

QVariantMap ButtonMapping::toVariantMap() const
{
    QVariantMap map;
    for (auto it = m_actionToButton.cbegin(); it != m_actionToButton.cend(); ++it)
        map.insert(actionName(it.key()), buttonName(it.value()));
    return map;
}

std::optional<NavAction> ButtonMapping::actionFor(GamepadButton button) const
{
    for (auto it = m_actionToButton.cbegin(); it != m_actionToButton.cend(); ++it) {
        if (it.value() == button)
            return it.key();
    }
    // Unassigned Guide and Select still open the menu, as in MVP 1.
    if (button == GamepadButton::Guide || button == GamepadButton::Select)
        return NavAction::Menu;
    return std::nullopt;
}

std::optional<GamepadButton> ButtonMapping::buttonFor(NavAction action) const
{
    const auto it = m_actionToButton.constFind(action);
    if (it == m_actionToButton.cend())
        return std::nullopt;
    return *it;
}

void ButtonMapping::bind(NavAction action, GamepadButton button)
{
    const auto previous = buttonFor(action);
    for (auto it = m_actionToButton.begin(); it != m_actionToButton.end(); ++it) {
        if (it.value() == button && it.key() != action) {
            if (previous)
                it.value() = *previous; // swap
            else
                m_actionToButton.erase(it);
            break;
        }
    }
    m_actionToButton.insert(action, button);
}

QStringList ButtonMapping::actionNames()
{
    QStringList names;
    for (const auto &a : kActions)
        names.append(QString::fromLatin1(a.name));
    return names;
}

QStringList ButtonMapping::buttonNames()
{
    QStringList names;
    for (const auto &b : kButtons)
        names.append(QString::fromLatin1(b.name));
    return names;
}

std::optional<NavAction> ButtonMapping::actionFromName(const QString &name)
{
    for (const auto &a : kActions) {
        if (name == QLatin1String(a.name))
            return a.action;
    }
    return std::nullopt;
}

QString ButtonMapping::actionName(NavAction action)
{
    for (const auto &a : kActions) {
        if (a.action == action)
            return QString::fromLatin1(a.name);
    }
    return {};
}

std::optional<GamepadButton> ButtonMapping::buttonFromName(const QString &name)
{
    for (const auto &b : kButtons) {
        if (name == QLatin1String(b.name))
            return b.button;
    }
    return std::nullopt;
}

QString ButtonMapping::buttonName(GamepadButton button)
{
    for (const auto &b : kButtons) {
        if (b.button == button)
            return QString::fromLatin1(b.name);
    }
    return {};
}

} // namespace nexora::input
