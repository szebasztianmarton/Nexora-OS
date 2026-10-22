## 2024-10-11 - QML Component Accessibility
**Learning:** In Qt/QML, web ARIA tags don't work. Instead, screen readers rely on the `Accessible` attached properties. Context properties in repeaters (like `modelData` and `index`) do not need to be prefixed with the delegate ID (like `tab.index`) inside the Accessible block, and `pressed()` can just be called directly without relying on an ID prefix.
**Action:** Always use `Accessible.role`, `Accessible.name`, `Accessible.description`, and `Accessible.onPressAction` for custom interactive QML components, being careful with implicit context properties.

## 2024-10-22 - QML Accessibility for Tabs
**Learning:** In QML Accessibility, use `Accessible.selected` for tabs (e.g., `Accessible.PageTab`) and selectable list items as it maps to the correct accessibility state (e.g., `QAccessible::State::Selected`). Replacing it with `Accessible.checkable` and `Accessible.checked` causes an accessibility regression. Elements with the role `Accessible.RadioButton` must use `Accessible.checkable` and `Accessible.checked` to correctly represent their state, distinguishing them from tabs or generic list items that use `Accessible.selected`.
**Action:** Use `Accessible.selected` instead of `Accessible.checked`/`Accessible.checkable` for `Accessible.PageTab` to ensure proper screen reader support.
