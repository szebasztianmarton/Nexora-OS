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
- **Readable at TV distance:** body text 17 px at scale 1.0, nothing below 12 px; layouts are checked at 1280×720 and 1920×1080.
- **Honest states:** loading, empty and error states say what is happening and what to do; features that do not exist do not appear (there is no Downloads tab).
- **Restrained motion:** short transitions (90–240 ms), none with reduced motion.

## Accessibility inputs

Set in `Main.qml` with bindings, so they change live when the setting changes:

| Input | Bound to | Effect |
| --- | --- | --- |
| `Theme.scale` | `settings.uiScale / 100` (`ui.scale`, 75–150 %, default 100) | multiplies spacing, typography, radii, icon sizes, control heights and the card grid |
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

### Spacing, radii, sizes (× `scale`)

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
| `NxButton` | focusable button with label, optional hint, disabled and danger styles |
| `SettingRow` | one settings line: label, value, Left/Right to change, Accept to activate |
| `ConfirmDialog` | modal confirmation; focus trapped; **default focus on the safe button (Cancel)**; Back cancels without action |
| `EmptyState` | icon, title, explanation and an action for empty/error states |
| `Toast` | transient notice at the bottom; errors in `danger` |
| `GameTile`, `GameGrid`, `GameRow` | game card (artwork or a placeholder drawn from the title, missing-file badge, favorite star), grid/list container, horizontal row for Home |
| `NavTabBar`, `NavColumn` | top tab bar; vertical list that skips hidden and info rows |
| `Header`, `HintBar` | title/status header; context-sensitive button hints (`ui.showHints`) |
| `OnScreenKeyboard` | controller-operable keyboard (used by `MetadataEditor`) |
| `BarIcon` | line icons drawn on a 24×24 grid (no icon font needed on the image) |
| `EmulatorCard` | emulator with health status |
| Overlays | `RunningOverlay`, `GameBar`, `QuickMenu`, `GameOptionsDialog`, `TutorialOverlay`, `InputTestPanel`, `UpdatePanel`, `MetadataEditor` |

## Screens and navigation

Top-level tabs, in order (`Main.qml`): **Home, Games, Systems, Favorites, Recent, Emulators, Settings, System**. L1/R1 (Page Up/Down) move between tabs; Up from a page reaches the tab bar. There is no Downloads/Imports tab because importing beyond library scanning is not implemented.

| Screen | Content |
| --- | --- |
| Home | Continue playing, Favorites, Recently added and Systems rows (horizontal navigation), status cards: controllers connected, health summary (unavailable emulators, configuration and library issues), update availability |
| Games | grid/list toggle, search, sort (title, recently played, date added), system filter chips, scan progress with Cancel, missing-file badges, onboarding when empty |
| Systems | one card per system with games or an emulator; Accept opens Games filtered to it |
| Favorites, Recent | game collections (`GameCollectionScreen`) |
| Game details | opened with Accept on a tile; artwork/placeholder, system, emulator selector, last played, play count, notes; Play, Favorite, Edit details (on-screen keyboard), Change system, Remove from library (confirmation states that the file is not deleted); Back returns focus to the same tile |
| Emulators | emulator cards with health, launch and profiles |
| Settings | sections General, Controllers, Display, Audio, Library, Emulators, System |
| System | version and build, update status and panel, hardware (CPU, memory, GPUs, storage, audio, display outputs), launch history, configuration/input/library/log diagnostics, Export diagnostics |
| Quick menu | over a running game (Guide, or hold Start 1 s): Return to game, Pause/Resume (when the emulator allows it), controller status, volume and mute, display settings, system info, Exit game (with confirmation) |

## States

Every screen that loads data has a loading state (scan spinner, "Scanning… N files"), an empty state with the next step (add a folder, rescan, clear search, configure an emulator) and an error state (database unavailable, unknown game, emulator unavailable with reason). Disabled controls are drawn with `textFaint` and are skipped or explained in the hint bar.

## Icons and typography

- Icons are vector paths in `BarIcon.qml` (game bar, status) plus text glyphs; no external icon font or image set is shipped.
- The application uses Qt's default application font; on the image fontconfig provides DejaVu (the only family in the image), on a desktop whatever the system default is. Monospace text (log excerpts on the System tab) uses `monospace`.

## How it is tested

| Test | Checks |
| --- | --- |
| `tst_console_qml` → `tst_theme.qml` | scale 1.5 multiplies `fontBody`, `space4`, `cardWidth` and control sizes; high contrast gives `#000000` background, white text, `#ffd400` focus ring and switches the overlay palette; reduced motion zeroes the animation tokens; old names still resolve |
| `tst_console_qml` → `tst_settings_ui.qml` | the seven settings sections exist; keyboard navigation stays inside the screen; `ConfirmDialog` defaults to Cancel and Back cancels |
| `tst_console_qml` → `tst_navigation.qml` | all eight tabs with keys only; tabs fit at TV resolutions and large scale |
| `tst_qmlsmoke` | every journey at 1280×720 and 1920×1080; `accessibilityModes` runs with high contrast and reduced motion; any QML warning fails the test |
| `scripts/qmllint.sh` | 0 errors (149 warnings, mostly unqualified access, at the time of writing) |

All UI tests run on the offscreen platform with the software renderer; nothing has been checked on a real TV or with a screen reader.

## Limitations

- Only English; `ui.language` accepts `en` only.
- No screen-reader support was tested (components set `Accessible` names and roles, but no assistive technology was used to check them).
- Contrast ratios are documented design intent, not measured by an automated test.
- No light theme; no per-user font choice.
- Artwork is a generated placeholder; there is no cover-art download or import.
