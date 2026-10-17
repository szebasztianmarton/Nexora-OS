## 2024-10-11 - QML Component Accessibility
**Learning:** In Qt/QML, web ARIA tags don't work. Instead, screen readers rely on the `Accessible` attached properties. Context properties in repeaters (like `modelData` and `index`) do not need to be prefixed with the delegate ID (like `tab.index`) inside the Accessible block, and `pressed()` can just be called directly without relying on an ID prefix.
**Action:** Always use `Accessible.role`, `Accessible.name`, `Accessible.description`, and `Accessible.onPressAction` for custom interactive QML components, being careful with implicit context properties.
## 2024-10-18 - QML Tab Accessibility State Mapping
**Learning:** In QML Accessibility, elements with the role `Accessible.PageTab` must use `Accessible.selected` instead of `Accessible.checkable` and `Accessible.checked`. Using the checkable properties for tabs creates an accessibility regression because screen readers look for `QAccessible::State::Selected` for tab controls, whereas checkboxes and radio buttons expect `QAccessible::State::Checked`.
**Action:** Always use `Accessible.selected` for custom QML components acting as tabs (`Accessible.PageTab`).
