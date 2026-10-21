## 2024-10-11 - QML Component Accessibility
**Learning:** In Qt/QML, web ARIA tags don't work. Instead, screen readers rely on the `Accessible` attached properties. Context properties in repeaters (like `modelData` and `index`) do not need to be prefixed with the delegate ID (like `tab.index`) inside the Accessible block, and `pressed()` can just be called directly without relying on an ID prefix.
**Action:** Always use `Accessible.role`, `Accessible.name`, `Accessible.description`, and `Accessible.onPressAction` for custom interactive QML components, being careful with implicit context properties.
## 2024-10-18 - QML Dialog Accessibility
**Learning:** QML dialog panels should define `Accessible.role: Accessible.Dialog`, as well as `Accessible.name` and `Accessible.description` properties. This provides necessary context to screen readers when the dialog is opened and receives focus, replacing standard web ARIA roles.
**Action:** Always verify that components used as floating dialogs (e.g., generic panels, confirmation boxes) declare `Accessible.Dialog` properties.
