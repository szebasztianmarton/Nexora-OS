# SDE — collections-home (sub-project 2) implementation notes

Commits on `claude/vibrant-mayer-f5agvc` after PR #21: schema v3 + play time (aa3337a), focus race fix (ef9d1ea), collections/pinning/Home layout UI (4c6f314), docs.

Deviations from the spec:
- Home layout lives in LibraryController (it knows the collections) rather than SettingsController.
- `homeSectionRows` is a notifying property: `void x;` dependencies inside QML bindings were not re-evaluated by the compiled bindings (Qt 6.4), so lists that depend on several properties take them as function arguments or properties.
- Reordering Home reassigns the column's children; section items bind their width to `column.width` (a re-parented item briefly has no parent).
- Collections cannot be reordered by hand; games keep the order they were added.
