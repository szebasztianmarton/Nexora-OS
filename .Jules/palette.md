## 2024-10-11 - QML Component Accessibility
**Learning:** In Qt/QML, web ARIA tags don't work. Instead, screen readers rely on the `Accessible` attached properties. Context properties in repeaters (like `modelData` and `index`) do not need to be prefixed with the delegate ID (like `tab.index`) inside the Accessible block, and `pressed()` can just be called directly without relying on an ID prefix.
**Action:** Always use `Accessible.role`, `Accessible.name`, `Accessible.description`, and `Accessible.onPressAction` for custom interactive QML components, being careful with implicit context properties.

## 2024-10-18 - QML PageTab Accessibility
**Learning:** In QML Accessibility, elements with the role `Accessible.PageTab` must use `Accessible.selected` instead of `Accessible.checkable` and `Accessible.checked` to properly map to `QAccessible::State::Selected` and represent their state.
**Action:** Always use `Accessible.selected` when setting the active state of `Accessible.PageTab` components.
