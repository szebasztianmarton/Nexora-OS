# Nexora OS — Console UI design and implementation

This document describes the controller-first console interface in
`apps/console-ui/` and the backend services in `src/core/` that it is built on.

## Architecture

```
src/core/               C++ backend (static library nexora_core, Qt 6 Core/Gui/Concurrent, optional SDL2)
  AppSettings           user preferences            -> <config>/settings.json
  EmulatorRegistry      emulator list model (read-only) <- <config>/emulators.json
  EmulatorManager       one external process at a time (QProcess, never a shell)
  GameLibrary/GameModel indexed games + per-game state -> <config>/library-state.json
  GameFilterModel       search / sort / filter proxy (instantiable from QML)
  ControllerManager     SDL2 game controllers -> logical actions -> key events
  DirectoryBrowser      controller-friendly folder picker backend
  Diagnostics           real system information, log access, sanitized report
  Logger                <log dir>/nexora.log (rotated at 2 MiB) + recent errors
apps/console-ui/src/    Backend (wiring + QML registration), main.cpp
apps/console-ui/qml/    QML module "Nexora"
  styles/               Theme, Dimensions, Typography, Motion (design tokens)
  singletons/           Router (navigation/feedback API), Input (logical keys), GameActions
  components/           reusable components (see inventory below)
  pages/                screens
tools/mock-emulator/    deterministic stand-in emulator for tests and the dev sandbox
tests/core/             Qt Test unit tests for the backend
tests/qml/              QuickTest UI tests (real backend + real QML module)
```

All state shown in the UI comes from these services. There is no mock data in
production. Sample data only exists in the developer sandbox
(`scripts/run-dev.sh`), which creates its own configuration directory.

## Visual design tokens

Every component uses the tokens. There are no literal colours or font sizes in pages.

| Token group | Values |
|---|---|
| Backgrounds | primary `#0A0D14`, secondary `#101621`, elevated `#171F2D`, hover `#1D2738`, overlay `#05070BCC` |
| Borders | subtle `#273244`, strong `#3A4961` |
| Accents | primary `#6CE5E8`, secondary `#8B7CFF`, hover `#94F0F1` |
| Text | primary `#F2F5FA`, secondary `#A6B0C2`, muted `#768297`, disabled `#596376` |
| Status | success `#65D6A0`, warning `#F4C96B`, error `#FF747F`, information `#79B9FF` |
| Type (at 1080p) | page title 42, section 27, card title 21, body 20, secondary 17, button 19, caption 15 (Inter, with fallbacks) |
| Spacing | 6 / 10 / 16 / 24 / 36 / 52; page padding 52 × 38; sidebar 248 (compact 92) |
| Radii | 8 / 12 / 18 |
| Focus | 3 px accent ring with 4 px offset plus soft glow, 1.03–1.035 scale |
| Motion | focus 130 ms, card 150 ms, dialog 180 ms, page 200 ms, OutCubic; all 0 with *Reduce motion* |

`Dimensions.scale` is `min(width/1920, height/1080) × interfaceScale` (clamped
at 0.55), so the same layout serves 1920×1080, 1280×720 and larger 16:9
displays. Below 1400 px of width the sidebar switches to its compact (icon) form.

Status is never shown by colour alone. Status pills carry an icon and text. The
active navigation entry has an indicator bar and a bold label. Selected filter
chips show a check mark.

## Navigation model

### Logical actions

Controllers are translated in C++ into the same key events the keyboard
produces, so there is a single navigation path through the UI.

| Action | Keyboard | Controller (positional) |
|---|---|---|
| Navigate | Arrow keys | D-pad / left stick (auto-repeat 400 ms, then every 110 ms) |
| Confirm | Enter / Space | South (A / Cross) |
| Back | Esc / Backspace | East (B / Circle) |
| Secondary (favorite) | F | West (X / Square) |
| Search | `/` (or Ctrl+F) | North (Y / Triangle) |
| Options / more | M / Menu key | Start |
| Previous / next tab | Page Up / Page Down | LB / RB |

The hint bar labels follow the last input device: keyboard keys, Xbox
(A/B/X/Y), PlayStation (Cross/Circle/…), Nintendo (positional B/A/Y/X) or
generic (South/East/…). Text labels keep the hints understandable without glyph
assets. Hints only list actions that work in the current focus context.

Controller input is suppressed while an emulator is running. Confirm and action
keys ignore auto-repeat, and buttons debounce repeated activation, so a held
button never launches twice.

### Focus

* Every page is a `PageContainer`. It focuses `defaultFocusItem` on first
  entry and remembers the last focused descendant (`restoreFocus()`).
* Each section (Home, Library, Favorites, Emulators, Settings) has its own
  `StackView`. It is created on first visit and kept alive, so scroll position
  and focus survive section switches.
* Directional navigation is explicit (KeyNavigation / Keys handlers / item
  views). A key that cannot move further inside a page propagates. Left at the
  left edge moves focus to the sidebar.
* Modals capture focus. Unhandled keys are swallowed, Back cancels, and focus
  returns to the previously focused item when the modal closes, including when
  a menu opens a follow-up dialog.
* The launch overlay captures focus while an emulator runs and restores it
  afterwards.

### Back

1. An open modal closes.
2. A page may unwind locally first (leave the search field, leave a settings panel).
3. A detail page is popped and focus is restored to the item that opened it.
4. On a top-level page: go to the previous section in history, otherwise focus the sidebar.
5. On the sidebar: return to Home. On Home's sidebar nothing happens.

**Back never exits the application.**

## Screen inventory

| Screen | Purpose and states |
|---|---|
| First run (`FirstRunPage`) | Welcome → controller check (optional button test) → emulator status → game folders (folder browser) → honest summary. Never blocks on missing hardware or configuration. Completion is persisted and can be re-run from Settings › General. |
| Home (`HomePage`) | Status headline from real state, Continue Playing hero (most recently played game), onboarding card when there is no history, Recently played and Favorites rows (only when they have games), Quick access tiles. |
| Game Library (`GameLibraryPage`) | Search (on-screen keyboard for controllers), filters (all / favorites / with artwork / emulator), sort (A–Z, Z–A, recently played, recently added), responsive grid (3–8 columns). States: loading, empty (reason-specific), no results, partial scan warning. |
| Game Details (`GameDetailsPage`) | Artwork, full title, emulator status, platform (if configured), file information, last played, favorite; Launch, Favorite, Change emulator, More (file info, remove from library — files are never deleted). |
| Favorites (`FavoritesPage`) | Same grid as the library, filtered to favorites; explanatory empty state. |
| Emulators (`EmulatorsPage`) | Configured emulators with status (available, executable missing, not executable, invalid configuration, disabled, unknown), platforms, extensions, game count; reload and validate; corrupt/missing configuration and skipped-entry states. |
| Emulator Details (`EmulatorDetailsPage`) | Executable, arguments (`{game}` explained), working directory, profiles, associated games, validation messages; Launch (without a game), Validate, Reload. Read-only. |
| Settings (`SettingsPage` + category pages) | General, Display, Audio (marked unavailable), Controller (devices, live input test, mapping table), Game Library (folders, refresh, removed games), Emulator Configuration, System & Diagnostics (versions, status, recent errors, log viewer, copy report, reset), About. |
| Overlays | Launch overlay (launching / running / stopping, Stop with confirmation), confirmation dialogs, action menus, error dialogs with Retry / Back to Library / View logs, toasts, on-screen keyboard, folder browser, log viewer. |

## Component inventory

Base and focus: `PageContainer`, `PageHeader`, `FocusFrame`, `FocusButton`,
`OptionChip`, `NavItem`, `NexoraSidebar`, `ControllerHints`.

Content: `GameCard`, `GameGrid`, `GameRow`, `Artwork`, `ArtworkPlaceholder`,
`HomeHero`, `HomeQuickTile`, `EmulatorCard`, `StatusIndicator`,
`LoadingIndicator`, `EmptyState`, `Icon` (built-in vector icons, no external
assets), `NexoraLogo` (temporary brand mark).

Overlays: `ModalBase`, `ConfirmationDialog`, `ActionMenu`, `MessageDialog`,
`OnScreenKeyboard`, `FolderBrowserDialog`, `SettingsLogViewer`, `ToastHost`,
`LaunchOverlay`.

Settings rows: `SettingsRow`, `SettingsToggleRow`, `SettingsCyclerRow`,
`SettingsValueRow`, `SettingsActionRow`, `SettingsSection`, `SettingsNote`,
`SettingsCategoryPage`, `SettingsInputTester`.

Application structure: `Shell` (sidebar, section stacks, overlay layers, launch
flow), `Main` (window, scale, first-run switch).

## Emulator configuration (`emulators.json`)

```json
{
  "version": 1,
  "emulators": [
    {
      "id": "example",
      "name": "Example Emulator",
      "executable": "/usr/bin/example-emulator",
      "arguments": ["--fullscreen", "{game}"],
      "workingDirectory": "",
      "enabled": true,
      "platforms": ["Example Platform"],
      "extensions": ["ext"],
      "profiles": [{ "id": "windowed", "name": "Windowed", "arguments": ["{game}"] }]
    }
  ]
}
```

`{game}` is replaced by the game path, either as a whole argument or embedded
(`--rom={game}`). Arguments are passed as separate argv elements, never through
a shell. Only files whose extension is handled by an enabled emulator are
indexed, and only inside folders the user configured. Recursion depth is 4 and
symlinked directories are not followed. Artwork is read from a sidecar image
next to the game, or from a `covers/` or `artwork/` sub-folder.

## Build, run and test

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure

# Developer sandbox (mock emulator, sample files, generated cover art):
scripts/run-dev.sh "$PWD/build"                 # add --size 1280x720 to test 720p
NEXORA_SANDBOX_RESET=1 scripts/run-dev.sh "$PWD/build"   # start over (shows first run)

# Production-style start:
build/apps/console-ui/nexora-console-ui [--config-dir DIR] [--log-dir DIR] [--windowed]
```

Dependencies (Ubuntu 24.04): `qt6-base-dev qt6-declarative-dev qml6-module-qtquick-*
qml6-module-qtquick-shapes qml6-module-qttest libsdl2-dev ninja-build`. SDL2 is
optional (`-DNEXORA_WITH_SDL=OFF`). Without it, controller support is reported
as unavailable.

## Known limitations

* **Emulator configuration is read-only in the UI.** Users edit `emulators.json`
  and choose *Reload*.
* **No system audio control.** The Audio category says so instead of showing a
  fake volume slider.
* **Display resolution cannot be changed from Nexora.** The Display category
  shows real values and the full-screen toggle only.
* **Controller buttons cannot be remapped yet.** The mapping table is read-only.
* **Real controllers are untested in CI.** Button handling, stick hysteresis and
  repeat need hardware validation; tests cover the logic through injected
  actions.
* **English only.** Strings use `qsTr()` and are ready for translations.
* **Corners are square on artwork images.** Qt 6.4 without Qt5Compat has no
  cheap rounded clipping, so only the card frames are rounded.
* **Partial history reset.** Each finished rescan replaces the game list, so
  views keep the current game by path but lose other transient state. The
  played history is capped at 20 games, so older entries lose their
  last-played date.
* **Artwork lookups cost several stat calls per game during a scan.** This is
  fine for local libraries; for very large network libraries a directory-listing
  cache would be faster.
* **Launch buttons stay enabled while an emulator runs.** A disabled focused
  button would lose focus when the overlay closes. The overlay blocks input
  during a run and `Router.launchGame` refuses a second launch.
* **The logo is a temporary mark.** It is drawn with QML shapes and can be
  replaced by an official asset.
