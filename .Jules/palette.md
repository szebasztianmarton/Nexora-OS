## 2024-10-11 - QML Component Accessibility
**Learning:** In Qt/QML, web ARIA tags don't work. Instead, screen readers rely on the `Accessible` attached properties. Context properties in repeaters (like `modelData` and `index`) do not need to be prefixed with the delegate ID (like `tab.index`) inside the Accessible block, and `pressed()` can just be called directly without relying on an ID prefix.
**Action:** Always use `Accessible.role`, `Accessible.name`, `Accessible.description`, and `Accessible.onPressAction` for custom interactive QML components, being careful with implicit context properties.

## 2026-10-18 - QML RadioButton and Tab Accessibility
**Learning:** In QML Accessibility, elements with the role `Accessible.RadioButton` must use `Accessible.checkable` and `Accessible.checked` to correctly represent their state, distinguishing them from tabs or generic list items that use `Accessible.selected`.
**Action:** Ensure `Accessible.RadioButton` roles have both `Accessible.checkable` and `Accessible.checked` properties, and `Accessible.PageTab` roles use `Accessible.selected`.
