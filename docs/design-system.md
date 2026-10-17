# Design system

The console UI's look is defined by one QML singleton, `Theme` (`apps/console-ui/qml/Theme.qml`). Every colour, spacing step, radius, font size, icon size, focus-ring width, animation duration and card dimension is a token there; screens and components only reference tokens. Three accessibility inputs — interface scale, high contrast and reduced motion — are applied once in `Main.qml` from the settings and every token derives from them.

Related: [settings.md](settings.md) (`ui.theme`, `ui.scale`, `ui.reducedMotion`), [controller-support.md](controller-support.md) (navigation), [testing.md](testing.md) (UI tests).

## Contents

- [Principles](#principles)
- [Accessibility inputs](#accessibility-inputs)
- [Tokens](#tokens)
- [Components](#components)
- [Screens and navigation](#screens-and-navigation)
- [States](#states)
- [Icons and typography](#icons-and-typography)
- [How it is tested](#how-it-is-tested)
- [Limitations](#limitations)

## Principles

- **Console, not desktop:** a dark foundation, full-screen, no windows or mouse-first widgets; everything reachable with directions, Accept and Back.
- **Focus is always visible:** a `focusRing` outline of `focusWidth` (3 px, 4 px in high contrast) on the focused item, never only a colour change of the content.
- **Readable at TV distance:** body text 17 px at scale 1.0 (scaled up with the window size above 1280×720), nothing below 12 px; layouts are checked at 1280×720 and 1920×1080, also at 150 % interface scale.
- **Plain text only:** every `Text` sets `textFormat: Text.PlainText`; titles, notes, file names, paths and update bundle content are never interpreted as markup.
- **Honest states:** loading, empty and error states say what is happening and what to do; features that do not exist do not appear (there is no Downloads tab).
- **Restrained motion:** short transitions (90–240 ms), none with reduced motion.

## Accessibility inputs

Set in `Main.qml` with bindings, so they change live when the setting changes:

| Input | Bound to | Effect |
| --- | --- | --- |
| `Theme.userScale` | `settings.uiScale / 100` (`ui.scale`, 75–150 %, default 100) | the user's part of `Theme.scale` |
| `Theme.resolutionScale` | `max(1, min(window width / 1280, window height / 720))` | the layout is designed for 1280×720; larger windows scale up (1920×1080 → 1.5), never down |
| `Theme.scale` (read-only) | `userScale × resolutionScale` | multiplies spacing, typography, radii, icon sizes, control heights and the card grid |
| `Theme.highContrast` | `settings.theme === "highContrast"` (`ui.theme`: `dark` or `highContrast`) | swaps the whole palette: pure black background, white text, yellow `#ffd400` focus ring and accent, wider focus ring; every colour used for text keeps a contrast ratio of at least 7:1 against the background it is drawn on (stated in `Theme.qml`; the ratios were chosen by design, not measured by a test) |
| `Theme.reducedMotion` | `settings.reducedMotion` (`ui.reducedMotion`) | `animFast`, `animNormal`, `animSlow` and `animMove` become 0; continuous animations (library scan spinner, pulsing status dot) stop |

All three are in Settings › General (Theme, Interface scale, Reduce motion); the interface scale can also be changed from the quick menu's display page.

## Tokens

### Colour

| Token | Dark | High contrast | Use |
| --- | --- | --- | --- |
| `background` | `#0b0d13` | `#000000` | window background |
| `surface` | `#141821` | `#000000` | cards, panels |
| `surfaceRaised` | `#1d2230` | `#141414` | raised panels, dialogs |
| `surfaceHover` | `#252b3c` | `#262626` | focused/hovered rows |
| `border` | `#2c3244` | `#ffffff` | separators, outlines |
| `accent` | `#5b8cff` | `#ffd400` | primary actions, selection |
| `accentSoft` | `#263a6b` | `#4a3d00` | selected backgrounds |
| `text` | `#eef1f7` | `#ffffff` | primary text |
| `textMuted` | `#a2a9bc` | `#cfcfcf` | secondary text |
| `textFaint` | `#6f778c` | `#b5b5b5` | tertiary text, hints |
| `success` / `warning` / `danger` | `#3ecf8e` / `#f2b84b` / `#ff6372` | `#00ff6a` / `#ffa500` / `#ff3b3b` | status |
| `favorite` | `#ffc94d` | `#ffd400` | favorite star |
| `focusRing` | = `accent` | `#ffd400` | focus outline |

The game bar and quick menu use a separate `overlay*` family (Xbox Game Bar–style greys in the dark theme, black/white/yellow in high contrast) defined next to the main palette.

### Spacing, radii, sizes (× `scale`, i.e. user scale × resolution factor)

| Group | Tokens (px at scale 1.0) |
| --- | --- |
| Spacing | `space1` 4, `space2` 8, `space3` 12, `space4` 16 (= `spacing`), `space5` 24, `space6` 32 |
| Radii | `radiusSmall` 10, `radius` 12, `radiusLarge` 18 |
| Typography | `fontTiny` 12, `fontSmall` 14, `fontBody` 17, `fontLarge` 20, `fontTitle` 24, `fontHero` 30 |
| Icons | `iconSmall` 16, `icon` 24, `iconLarge` 40 |
| Controls | `controlHeight` 48, `controlHeightCompact` 40, `chipHeight` 44, `tabHeight` 42, `rowHeight` 60, `barHeight` 48, `headerHeight` 64 |
| Game grid | `cardWidth` 230, `cardHeight` 200, `cardSpacing` = `space4` |
| Focus | `focusWidth` 3 (4 in high contrast), not scaled |

### Motion (ms)

`animFast` 90, `animNormal` 160, `animSlow` 240, `animMove` 120 (list moves); all 0 with reduced motion.

Older property names from MVP 2 are kept as aliases so existing screens keep working (`tst_theme::test_existingPropertyNamesStillWork`).

## Components

Reusable items in `apps/console-ui/qml/` (QML module `Nexora.Console`):

| Component | Purpose |
| --- | --- |
| `NxButton` | focusable button with label, optional hint, disabled and danger styles; in high contrast a focused primary button (yellow fill) draws its label in the background colour |
| `SettingRow` | one settings line: label, value, Left/Right to change, Accept to activate; the value takes at most half the row and elides; an optional `descriptionColor` (e.g. `danger` for a failed action) |
| `ConfirmDialog` | modal confirmation; focus trapped (Left/Right move between the two buttons, every other key, including L1/R1, Menu and Search, stops there); **default focus on the safe button (Cancel)**; Back cancels without action; if its page or layer is hidden while it is open, it cancels itself |
| `EmptyState` | icon, title, explanation and an action for empty/error states |
| `Toast` | transient notice at the bottom; errors in `danger` |
| `GameTile`, `GameGrid`, `GameRow` | game card (artwork or a placeholder drawn from the title, missing-file badge, favorite star), grid/list container, horizontal row for Home |
| `NavTabBar`, `NavColumn` | top tab bar; vertical list that skips hidden and info rows |
| `Header`, `HintBar` | title/status header; context-sensitive button hints (`ui.showHints`) |
| `OnScreenKeyboard` | controller-operable keyboard (used by `MetadataEditor`); never taller than the space it gets: with little height the keys get shorter and the help line hides |
| `BarIcon` | line icons drawn on a 24×24 grid (no icon font needed on the image) |
| `EmulatorCard` | emulator with health status |
| Overlays | `RunningOverlay`, `GameBar`, `QuickMenu`, `GameOptionsDialog`, `TutorialOverlay`, `InputTestPanel`, `UpdatePanel`, `MetadataEditor` |

## Screens and navigation

Top-level tabs, in order (`Main.qml`): **Home, Games, Systems, Favorites, Recent, Emulators, Settings, System**. L1/R1 (Page Up/Down) move between tabs; Up from a page reaches the tab bar; **Back inside a page** (when nothing there consumes it: no dialog, details view or text field) **moves focus to the tab bar**, and Back on the tab bar re-enters the page. When the widest tab label does not fit (large scale at 1280 px), all tabs switch to a smaller font and tighter padding instead of eliding. There is no Downloads/Imports tab because importing beyond library scanning is not implemented.

| Screen | Content |
| --- | --- |
| Home | Continue playing, Favorites, Recently added and Systems rows (horizontal navigation), status cards: controllers connected, health summary (unavailable emulators, configuration and library issues), updates (the result of the last update comes first: "Update rolled back" in `danger` or "Update installed" in `success` until it is dismissed in the update panel, otherwise availability) |
| Games | grid/list toggle, search, sort (title, recently played, date added), system filter chips (All systems, every system with games, **Other** for games without a system, and the active filter even when it has no games), scan progress with Cancel, missing-file badges, onboarding when empty. Card size follows the available height; when even small cards do not fit (large scale at 720p) the grid shows list rows and the view chip says "List" |
| Systems | one card per system with games or an emulator, plus "Other" for games without a system; Accept opens Games filtered to it ("Other" filters to games without a system, `@other`) |
| Favorites, Recent | game collections (`GameCollectionScreen`) |
| Game details | opened with Accept on a tile; artwork/placeholder, system, emulator selector, last played, play count, notes; Play, Favorite, Edit details (on-screen keyboard), Change system, Remove from library (confirmation states that the file is not deleted); Back returns focus to the same tile |
| Emulators | emulator cards with health, launch and profiles |
| Settings | sections General, Controllers, Display, Audio, Library, Emulators, System. Emulators lists a default-emulator row only for catalog systems with games or a compatible emulator, cycling through "Automatic" and the enabled emulators that declare the system. System actions report in place: Export diagnostics and Create backup show "Saved to …" or "Export failed: …"/"Backup failed: …" in `danger` under their row; Restore backup and Clear artwork cache report through a toast |
| System | version and build, update status and panel, hardware (CPU, memory, GPUs, storage, audio, display outputs), launch history, configuration/input/library/log diagnostics, Export diagnostics. The hardware probe is synchronous, so it runs lazily: about 150 ms after Settings or System becomes current, and at most once per 30 s (Reload probes at once) |
| Running overlay | shown while an emulator runs: **Stop** asks first ("Stop the emulator?", default Cancel; it closes by itself if the emulator exits meanwhile); the scrim is opaque in high contrast |
| Quick menu | over a running game (Guide, or hold Start 1 s): Return to game (resumes a paused emulator first), Pause/Resume (when the emulator allows it), controller status, volume and mute, display settings, system info, Exit game (with confirmation) |
| Game bar | Restart and Power off ask through `ConfirmDialog` (default Cancel); Stop emulator still needs a second press ([game-bar.md](game-bar.md)) |

## States

Every screen that loads data has a loading state (scan spinner, "Scanning… N files"), an empty state with the next step (add a folder, rescan, clear search, configure an emulator) and an error state (database unavailable, unknown game, emulator unavailable with reason). Disabled controls are drawn with `textFaint` and are skipped or explained in the hint bar. Failed edits (game details, system change, remove from library, default emulator) show an error notice with the reason; the metadata editor stays open with the edits after a failed save. Modal layers (dialogs, the input test panel) keep every key and close or cancel themselves when hidden from outside.

## Icons and typography

- Icons are vector paths in `BarIcon.qml` (game bar, status) plus text glyphs; no external icon font or image set is shipped.
- The application uses Qt's default application font; on the image fontconfig provides DejaVu (the only family in the image), on a desktop whatever the system default is. Monospace text (log excerpts on the System tab) uses `monospace`.

## How it is tested

| Test | Checks |
| --- | --- |
| `tst_console_qml` → `tst_theme.qml` | user scale 1.5 multiplies `fontBody`, `space4`, `cardWidth` and control sizes; the resolution factor multiplies the user scale; high contrast gives `#000000` background, white text, `#ffd400` focus ring and switches the overlay palette, and a focused primary button stays readable; reduced motion zeroes the animation tokens; old names still resolve |
| `tst_console_qml` → `tst_settings_ui.qml` | the seven settings sections exist; keyboard navigation stays inside the screen; `ConfirmDialog` defaults to Cancel and Back cancels; a long value elides at half the row and is plain text |
| `tst_console_qml` → `tst_navigation.qml` | all eight tabs with keys only; tabs fit at TV resolutions and large scale |
| `tst_console_qml` → `tst_gamedetails.qml` | markup in a title or note is shown literally; a failed save keeps the editor open |
| `tst_qmlsmoke` | every journey at 1280×720 and 1920×1080; `accessibilityModes` runs with high contrast and reduced motion; `layoutAtLargeScale` (150 %: grid/list fallback, tabs, keyboard, editor), Back from a page to the tab bar, modal key trapping, confirmation before stopping a game and for game bar power actions, Settings action feedback, default-emulator rows keeping focus, "Other" and an empty system in the Games filter, lazy hardware probe, the rolled-back update card and its dismissal; any QML warning fails the test |
| `scripts/qmllint.sh` | 0 errors (155 warnings, mostly unqualified access, after the verification fixes) |

All UI tests run on the offscreen platform with the software renderer; nothing has been checked on a real TV or with a screen reader.

## Limitations

- Only English; `ui.language` accepts `en` only.
- No screen-reader support was tested (components set `Accessible` names and roles, but no assistive technology was used to check them).
- Contrast ratios are documented design intent, not measured by an automated test.
- No light theme; no per-user font choice.
- Artwork is a generated placeholder; there is no cover-art download or import.
