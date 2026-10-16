## 2024-10-11 - QML Component Accessibility
**Learning:** In Qt/QML, web ARIA tags don't work. Instead, screen readers rely on the `Accessible` attached properties. Context properties in repeaters (like `modelData` and `index`) do not need to be prefixed with the delegate ID (like `tab.index`) inside the Accessible block, and `pressed()` can just be called directly without relying on an ID prefix.
**Action:** Always use `Accessible.role`, `Accessible.name`, `Accessible.description`, and `Accessible.onPressAction` for custom interactive QML components, being careful with implicit context properties.

## 2024-10-16 - QML Component Selection Accessibility
**Learning:** In QML Accessibility, use `Accessible.selected` for tabs and selectable list items as it maps to the correct accessibility state (e.g., `QAccessible::State::Selected`). Replacing it with `Accessible.checkable` and `Accessible.checked` causes an accessibility regression.
**Action:** Always use `Accessible.selected` to represent selection state instead of `Accessible.checked` and `Accessible.checkable` for elements such as tabs or selectable items.
