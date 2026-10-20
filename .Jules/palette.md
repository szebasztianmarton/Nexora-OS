## 2024-10-11 - QML Component Accessibility
**Learning:** In Qt/QML, web ARIA tags don't work. Instead, screen readers rely on the `Accessible` attached properties. Context properties in repeaters (like `modelData` and `index`) do not need to be prefixed with the delegate ID (like `tab.index`) inside the Accessible block, and `pressed()` can just be called directly without relying on an ID prefix.
**Action:** Always use `Accessible.role`, `Accessible.name`, `Accessible.description`, and `Accessible.onPressAction` for custom interactive QML components, being careful with implicit context properties.

## 2024-10-18 - QML Tab Accessibility States
**Learning:** Using `Accessible.checkable` and `Accessible.checked` for elements with the `Accessible.PageTab` role causes accessibility regressions in Qt/QML. The correct state property for tabs (and selectable list items) is `Accessible.selected`, which maps to `QAccessible::State::Selected`. Radio buttons (`Accessible.RadioButton`) should continue using `Accessible.checkable` and `Accessible.checked`.
**Action:** When creating tab navigation or selectable lists, bind the active state to `Accessible.selected`. Only use `Accessible.checked` for radio buttons and checkboxes.
