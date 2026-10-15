## 2024-10-11 - QML Component Accessibility
**Learning:** In Qt/QML, web ARIA tags don't work. Instead, screen readers rely on the `Accessible` attached properties. Context properties in repeaters (like `modelData` and `index`) do not need to be prefixed with the delegate ID (like `tab.index`) inside the Accessible block, and `pressed()` can just be called directly without relying on an ID prefix.
**Action:** Always use `Accessible.role`, `Accessible.name`, `Accessible.description`, and `Accessible.onPressAction` for custom interactive QML components, being careful with implicit context properties.
## 2024-10-15 - Tab Accessibility State
**Learning:** In QML, components with the `Accessible.PageTab` role should use `Accessible.selected` instead of `Accessible.checkable` and `Accessible.checked`. Using the checked state instead of the selected state causes screen readers to misreport the state of the tab.
**Action:** When creating or fixing custom tab components, ensure the accessibility state uses `Accessible.selected` and not the `checked` property.
