# Design system

The console UI's look is defined by one QML singleton, `Theme` (`apps/console-ui/qml/Theme.qml`). Every colour, spacing step, radius, font size, icon size, focus-ring width, animation duration and card dimension is a token there; screens and components only reference tokens. Four accessibility inputs — interface scale, high contrast, reduced motion and reduced transparency — are applied once in `Main.qml` from the settings and every token derives from them.

Related: [settings.md](settings.md) (`ui.theme`, `ui.scale`, `ui.reducedMotion`, `ui.reduceTransparency`), [controller-support.md](controller-support.md) (navigation), [testing.md](testing.md) (UI tests).

## Contents

- [Principles](#principles)
- [Accessibility inputs](#accessibility-inputs)
- [Tokens](#tokens)
- [Liquid Glass](#liquid-glass)
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
| `Theme.glass` | `settings.theme === "glass"` (`ui.theme`: `glass` (default), `dark` or `highContrast`) | the glass look: translucent surfaces over the `Backdrop`'s light glows, dense `surfaceFloating` panels for anything that floats over other content, a specular highlight and a light rim drawn by `GlassSurface` ([Liquid Glass](#liquid-glass)); `Theme.glassActive` is `glass && !highContrast` |
| `Theme.highContrast` | `settings.theme === "highContrast"` | swaps the whole palette: pure black background, white text, yellow `#ffd400` focus ring and accent, wider focus ring; every colour used for text keeps a contrast ratio of at least 7:1 against the background it is drawn on (stated in `Theme.qml`; the ratios were chosen by design, not measured by a test) |
| `Theme.reducedMotion` | `settings.reducedMotion` (`ui.reducedMotion`) | `animFast`, `animNormal`, `animSlow` and `animMove` become 0; continuous animations (library scan spinner, pulsing status dot) stop; the glass focus pill jumps instead of gliding and an active glass surface no longer scales up |
| `Theme.reduceTransparency` | `settings.reduceTransparency` (`ui.reduceTransparency`) | glass fills become the dense `surfaceFloating` values, the specular highlight weakens (6 % instead of 18 % white) and the backdrop blur is off ([Liquid Glass](#liquid-glass)) |
| `Theme.gpuEffects` | `UiCapabilities.gpuEffects` | true only on a hardware renderer with a Qt that has `QtQuick.Effects`; the precondition for the backdrop blur |
| `Theme.sessionActive` | `app.state` is `starting`, `running` or `stopping` | an emulator session is active; the blur is off so the GPU is left to the emulator |

The user-facing ones are in Settings › General (Theme, Interface scale, Reduce motion, Reduce transparency); the interface scale can also be changed from the quick menu's display page.

## Tokens

### Colour

| Token | Glass (default) | Dark | High contrast | Use |
| --- | --- | --- | --- | --- |
| `background` | `#070a14` + `Backdrop` glows | `#0b0d13` | `#000000` | window background |
| `surface` | white 6 % | `#141821` | `#000000` | cards, panels, bars |
| `surfaceRaised` | white 9 % | `#1d2230` | `#141414` | raised panels, tiles |
| `surfaceHover` | white 15 % | `#252b3c` | `#262626` | focused/hovered rows |
| `surfaceFloating` | `rgba(26, 31, 48, 0.90)` | = `surface` | `#000000` | panels over other content: dialogs, on-screen keyboard, update panel, tutorial, profile picker, toasts |
| `border` | white 16 % | `#2c3244` | `#ffffff` | separators, outlines |
| `accent` | `#6f9bff` | `#5b8cff` | `#ffd400` | primary actions, selection |
| `accentSoft` | accent 30 % | `#263a6b` | `#4a3d00` | selected backgrounds |
| `text` | `#eef1f7` | `#eef1f7` | `#ffffff` | primary text |
| `textMuted` | `#b6bdce` | `#a2a9bc` | `#cfcfcf` | secondary text |
| `textFaint` | `#8c94aa` | `#6f778c` | `#b5b5b5` | tertiary text, hints (brighter in glass, where surfaces let light through) |
| `success` / `warning` / `danger` | as dark | `#3ecf8e` / `#f2b84b` / `#ff6372` | `#00ff6a` / `#ffa500` / `#ff3b3b` | status |
| `favorite` | as dark | `#ffc94d` | `#ffd400` | favorite star |
| `focusRing` | = `accent` | = `accent` | `#ffd400` | focus outline |

The game bar and quick menu use a separate `overlay*` family (Xbox Game Bar–style greys in the dark theme, the same greys as tinted glass in the glass theme, black/white/yellow in high contrast) defined next to the main palette.

The glass look and its extra tokens are described in [Liquid Glass](#liquid-glass).

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

## Liquid Glass

The default `glass` theme is an Apple "Liquid Glass" look: translucent surfaces with a specular highlight and a bright rim, a real backdrop blur where the hardware allows it, floating bars that content scrolls under, and a glass capsule that glides between focused items. It is one shared primitive (`GlassSurface`) built on Qt Quick's own effect stack, not a per-screen blur. `dark` and `highContrast` use none of it: `Theme.glassActive` (`glass && !highContrast`) is false there and every glass component is invisible.

Design background: [the Liquid Glass spec](superpowers/specs/2026-10-17-liquid-glass-design.md).

### When the blur is on

`Theme.glassBlur` is the single switch every glass component reads. It is true only when all four conditions hold:

1. `Theme.glassActive`: the glass theme is chosen and high contrast is off;
2. `Theme.gpuEffects`: `UiCapabilities.gpuEffects` (below);
3. not `Theme.reduceTransparency` (Settings › General › Reduce transparency);
4. not `Theme.sessionActive`: no emulator is starting, running or stopping, so nothing repaints behind a running game.

When it is false, glass surfaces use the **flat path** (`GlassEffectFlat`): fill, highlight and rim over the static `Backdrop`, which renders identically with Qt's software renderer (tests) and llvmpipe. When it is true, a surface that may blur (`allowBlur`) and has a usable backdrop uses the **GPU path** (`GlassEffectGpu`). Switching paths, for example when a game starts, swaps the surface's loader source and does not rebuild the owner.

`UiCapabilities.gpuEffects` (`apps/console-ui/src/UiCapabilities.{h,cpp}`, a QML singleton) is false until the main window's scene graph is initialised, and then true only if all of these hold (`UiCapabilities::decide()`):

- the Qt that is running is at least 6.5. This is checked at run time with `QLibraryInfo::version()`, not against the version compiled with: `QtQuick.Effects` (`MultiEffect`) is a QML module loaded when needed, so the minimum build stays Qt 6.4 and the image's Qt 6.9.1 gets the blur;
- `NEXORA_NO_GLASS_BLUR` is unset or empty (any non-empty value switches the blur off; see [troubleshooting.md](troubleshooting.md#the-glass-theme));
- the scene graph is neither the software backend nor an unknown API;
- for OpenGL, the `GL_RENDERER` string (lower-cased) contains neither `llvmpipe` nor `softpipe`. Other graphics APIs (for example Vulkan) pass without a renderer-string check.

If `GlassEffectGpu.qml` still cannot load (`Loader.Error`, for example `QtQuick.Effects` missing), the surface silently stays flat and logs one `nexora.ui` debug line. A GPU shader failure at run time cannot be detected from QML; `NEXORA_NO_GLASS_BLUR=1` and Reduce transparency are the escape hatches.

### Glass tokens

Defined in `Theme.qml` next to the palette. Values in the glass theme at scale 1.0; "Reduce transparency" is the value with `Theme.reduceTransparency` on.

| Token | Value | Reduce transparency | Use |
| --- | --- | --- | --- |
| `glassBlurRadius` | 32 px × `scale` | same | `MultiEffect.blurMax` of the GPU path |
| `glassSaturation` | 0.35 | same | saturation of the blurred backdrop |
| `glassFillRegular` | white 10 % | `surfaceFloating` | fill of the `regular` variant |
| `glassFillRegularActive` | white 15 % | `surfaceHover` | the same fill while an interactive surface is active |
| `glassFillClear` | white 5 % | `surfaceFloating` | fill of the `clear` variant (cards, rows, info plates) |
| `glassFillDense` | `rgb(0.10, 0.12, 0.19)` 65 % (the `surfaceFloating` tint) | `surfaceFloating` | fill of `dense` surfaces on the GPU path (floating panels with body text over the blur); `Theme.text` stays at 4.5:1 or better over it composited on a mid-grey `#808080` backdrop (tested) |
| `glassFillProminent` | accent 30 % | opaque 50 % mix of the accent into `background` | fill of the `prominent` variant; `prominentFill(tint)` computes it for any tint; the light label stays at 4.5:1 or better (tested, with and without reduce transparency) |
| `glassRim` | white 20 % | same | 1 px border of every glass surface |
| `glassRimStrong` | white 30 % | same | the rim while an interactive surface is active |
| `glassHighlight` | white 18 % | white 6 % | specular highlight, a vertical gradient fading to transparent by 45 % of the height |
| `radiusXL` | 24 px × `scale` | same | corners of floating bars and sheets |

### `GlassSurface`

A background item: put it inside a panel, filling it, below the content (`z: -1`). It is invisible outside `Theme.glassActive`.

| Property | Meaning |
| --- | --- |
| `variant` | `regular` (default), `clear` (lighter, for cards and rows) or `prominent` (tinted, for primary actions) |
| `tint` | tint of `prominent`; defaults to `Theme.accent` |
| `radius` | corner radius, default `Theme.radiusLarge` |
| `interactive`, `active` | set by the owner on focus or press: with both true the fill steps up (`glassFillRegularActive`), the rim becomes `glassRimStrong` and the surface scales to 1.02 over `animFast` (no scale under reduced motion) |
| `dense` | for panels that float over other content: on the flat path the fill is the dense `Theme.surfaceFloating` instead of the translucent one, so whatever lies behind never competes with the text; on the GPU path the fill is the darker `glassFillDense` over the blurred backdrop |
| `backdrop` | the item to blur; if unset, the `backdrop` of the enclosing `GlassGroup` |
| `allowBlur` | `false` keeps the surface on the flat path whatever `glassBlur` says (cards, rows, buttons, the focus pill) |

Backdrop rules: a backdrop that is the surface itself or one of its ancestors is rejected (it would render itself) and the surface stays flat. The GPU path is used only when `glassBlur`, `allowBlur` and a usable backdrop all hold and the GPU file loaded; `state` is then `gpu` (`blurred` is true), otherwise `flat`.

- **GPU path** (`GlassEffectGpu`): a `ShaderEffectSource` crop of the backdrop under the surface (inside a `GlassGroup`, a crop of the group's already blurred output), blurred and saturated by a `MultiEffect`, masked to the rounded shape; the flat layers go on top. There is no drop shadow (the mask would clip it, and `MultiEffect` builds its blur levels whenever a shadow is on, which would blur grouped surfaces again), and `autoPaddingEnabled` is off so the blur keeps its colour up to the surface's edges instead of fading to transparent. The crop rectangle is the surface mapped into the backdrop's coordinates. Ancestors move, scroll and scale without any signal on the surface, so the rectangle is refreshed once per frame (after animations ran) while the surface exists and reassigned only when it changed.
- **Flat path** (`GlassEffectFlat`): fill (by variant and state), specular highlight, rim.

### `GlassGroup` (shared blur)

Surfaces inside a `GlassGroup` crop one shared blurred texture instead of blurring the backdrop each. The group owns a `GlassGroupBlur` pass (the whole `backdrop` captured at half resolution, blurred once without padding so the window edges do not fade, kept off-screen) only while `Theme.glassBlur` holds and its backdrop is usable (not null, not the group itself or one of its ancestors); otherwise it is a plain `Item`. `Main.qml` uses one group, `glassChrome`, for the three bars.

### `GlassFocusPill`

One glass capsule per container that slides and resizes onto the focused item. It sits below the container's items (`z: -1`), `target` points at the item to cover, and it follows the target when the target or a container up to the pill's parent moves. It wraps a `GlassSurface` (`interactive`, `active`, `allowBlur: false`) that is never scaled up (the morph is the motion, and a lift would overhang full-width rows). `radius` sets its corners: a capsule (`height / 2`) by default, `Theme.radiusSmall` (the `SettingRow` corners) in the quick menu and Settings. Used by:

- `NavTabBar`: the current tab (the tab itself stays transparent);
- `QuickMenu`: the focused entry of the visible page;
- `SettingsScreen`: the focused setting row.

The first placement on a target snaps (the pill never sweeps in from the corner); later moves animate `x`, `y`, `width` and `height` with `Easing.OutBack` (overshoot 1.1) over `animNormal`. Under reduced motion it jumps. The focus ring stays drawn by the items on top of it.

### `NxButton` styles

`NxButton.style` is `auto` (default), `glass`, `glassProminent` or `solid`. `effectiveStyle` resolves it: `solid` outside the glass theme and for `kind: "danger"`; otherwise an explicit `style`; with `auto`, `glassProminent` for `kind: "primary"` and `glass` for everything else. A glass button is a transparent, fully rounded rectangle with a `GlassSurface` behind it (`regular` or `prominent`, `interactive`, `active` while hovered or focused, `allowBlur: false`). An armed "press again" button leaves the glass look and shows its danger fill. `solid` is the plain filled button of the dark and high contrast themes.

### Floating chrome and layout

In the glass theme the header, tab bar and hint bar are floating glass bars inset by `space4` from the window edges (`radiusXL` corners, `space2` between header and tab bar), all inside the `glassChrome` group. `Main.qml` builds two siblings:

- `sceneLayer`: the `Backdrop` plus the `pageArea` with all pages. It fills the whole window, so content scrolls under the bars, and it is the group's `backdrop`;
- `glassChrome`: the bars.

In the glass theme each bar has a `MouseArea` below its own controls that takes every press on the bar, so a card scrolled underneath cannot be clicked through it (wheel events still pass to the page). The tab bar's tabs sit above it and keep their own clicks.

Pages receive `chromeTop` and `chromeBottom` (the height the bars cover plus `space2`; `chromeBottom` is `space4` without the hint bar) and pad their first and last rows by them, so nothing is hidden at rest. The Games and Systems grids and the Settings list scroll underneath the bars and still bring their last row fully above the hint bar. In `dark` and `highContrast` the bars stay docked: `chromeTop` and `chromeBottom` are 0 and the `pageArea` has top and bottom margins of `dockedTop` and `dockedBottom`.

Window-level dialogs and overlays (game options, toast, tutorial, update panel and similar) live in `overlayArea`, which spans from the bottom of the header to the top of the hint bar, so the bars that list their controls stay visible. In glass the tutorial, game options and update panel share one modal scrim (`glassModalScrim`, black at 0.40 alpha, shown while any of them is open): it is the last child of `sceneLayer`, so it dims the whole window including the gaps around the floating bars, yet sits below `glassChrome`, which keeps the bars undimmed and readable. The overlays' own scrims are transparent in glass so nothing is dimmed twice (docked themes keep `Theme.background`), and the GPU blur samples the dimmed scene. They pass `sceneLayer` as `glassBackdrop` and blur it under themselves. Floating panels use `dense` glass over a dark scrim; cards, rows and info plates (`EmulatorCard`, `SettingRow`, `GameTile`) use the `clear` variant on the flat path only. The game bar and quick menu keep their `overlay*` palette (they draw over another process's frame, which cannot be sampled) and add a flat `clear` `GlassSurface` for the highlight and rim.

### Performance budget

- The three bars share one blur pass (the whole `sceneLayer`, captured at half resolution and blurred once) and each crops its part at full size.
- Things that appear many times per screen (cards, rows, tiles, buttons, the focus pill) never blur; they read `glassActive`, not `glassBlur`.
- A window-level modal adds one blurred crop of its own while it is open.
- No blur while an emulator runs (`sessionActive`); nothing animates continuously behind it. The backdrop itself is static: an animated full-screen background would repaint every frame.

The `Backdrop` is three large pre-rendered light glows (`qml/images/glow-*.png`, regenerated by `scripts/make-glass-glows.py`) under a vignette.

### SwiftUI to QML mapping

| SwiftUI | Nexora | Purpose |
| --- | --- | --- |
| `.glassEffect(.regular / .clear)` | `GlassSurface { variant: "regular" / "clear" }` | a glass background for any panel, bar or card |
| `.glassEffect(.regular.tint(c))`, `.glassProminent` | `GlassSurface { variant: "prominent"; tint: c }` | accent-tinted glass for primary actions |
| `.interactive()` | `GlassSurface { interactive: true }` | brightens and lifts on focus/press |
| `GlassEffectContainer` | `GlassGroup` | children share one backdrop capture and one blur pass |
| `glassEffectID` + namespace | `GlassFocusPill` | one glass capsule that animates between focused items |
| `.buttonStyle(.glass)` / `.glassProminent` | `NxButton { style: "glass" / "glassProminent" }` | glass buttons |

## Components

Reusable items in `apps/console-ui/qml/` (QML module `Nexora.Console`):

| Component | Purpose |
| --- | --- |
| `NxButton` | focusable button with label, optional hint, disabled and danger styles; `style` (`auto`, `glass`, `glassProminent`, `solid`) picks the glass look in the glass theme ([Liquid Glass](#liquid-glass)); in high contrast a focused primary button (yellow fill) draws its label in the background colour |
| `SettingRow` | one settings line: label, value, Left/Right to change, Accept to activate; the value takes at most half the row and elides; an optional `descriptionColor` (e.g. `danger` for a failed action) |
| `ConfirmDialog` | modal confirmation; focus trapped (Left/Right move between the two buttons, every other key, including L1/R1, Menu and Search, stops there); **default focus on the safe button (Cancel)**; Back cancels without action; if its page or layer is hidden while it is open, it cancels itself |
| `EmptyState` | icon, title, explanation and an action for empty/error states |
| `QuickResumeRow` | Home row of saved Quick Resume sessions (cards with title, saved time, size, emulator, problem/age warning in `warning`); hidden when empty |
| `Toast` | view of the [notification service](notifications.md)'s current entry at the bottom; warnings in `warning`, criticals in `danger`, "×N" for repeats |
| `GameTile`, `GameGrid`, `GameRow` | game card (artwork or a placeholder drawn from the title, missing-file badge, favorite star), grid/list container, horizontal row for Home |
| `NavTabBar`, `NavColumn` | top tab bar; vertical list that skips hidden and info rows |
| `Header`, `HintBar` | title/status header; context-sensitive button hints (`ui.showHints`) |
| `OnScreenKeyboard` | controller-operable keyboard (used by `MetadataEditor`); never taller than the space it gets: with little height the keys get shorter and the help line hides |
| `BarIcon` | line icons drawn on a 24×24 grid (no icon font needed on the image) |
| `EmulatorCard` | emulator with health status |
| `Backdrop` | window background (glass: dark base, light glows, vignette; otherwise the background colour), used by `Main.qml`, `GameDetailsScreen` and `InputTestPanel` |
| `GlassSurface`, `GlassGroup`, `GlassFocusPill` | the Liquid Glass primitives: a glass background for any panel, bar, card or button; one shared blur pass for several surfaces; one glass capsule that glides between focused items ([Liquid Glass](#liquid-glass)). Implementation files: `GlassEffectFlat`, `GlassEffectGpu`, `GlassGroupBlur` |
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
| `tst_console_qml` → `tst_theme.qml` | user scale 1.5 multiplies `fontBody`, `space4`, `cardWidth` and control sizes; the resolution factor multiplies the user scale; high contrast gives `#000000` background, white text, `#ffd400` focus ring and switches the overlay palette; glass makes surfaces translucent, floating panels dense and yields completely to high contrast; the backdrop shows its glows (loaded) only in glass, and a focused primary button stays readable; `Theme.glassBlur` is false without glass, without `gpuEffects`, with a session active, with reduce transparency or with high contrast; reduce transparency makes the glass fills the dense values; `glassBlurRadius` and `radiusXL` scale; reduced motion zeroes the animation tokens; old names still resolve |
| `tst_console_qml` → `tst_glass.qml` | `GlassSurface` is flat under the software backend; with `glassBlur` forced on it loads the GPU file when `QtQuick.Effects` exists and otherwise stays flat without errors (only the engine's own "module not installed" warning is tolerated); `interactive` + `active` raise the fill; prominent uses the accent fill; a `GlassGroup` provides the backdrop; a backdrop that is the surface itself or an ancestor is rejected, and so is a group's own ancestor; panels, cards and rows draw `GlassSurface` and drop their plain border for the rim; `dense` uses `surfaceFloating` when flat and `glassFillDense` over the blur, which keeps `Theme.text` at 4.5:1 over mid grey; surfaces are hidden outside glass; `GlassFocusPill` follows its target, snaps on first placement, keeps its surface at scale 1, takes a configurable `radius`, jumps under reduced motion and is invisible outside glass; button styles resolve, a glass button is transparent and a solid one is not; the prominent label keeps 4.5:1 on the flat fill, also with reduce transparency |
| `tst_uicapabilities` (unit) | `decide()`: a hardware renderer allows GPU effects only when the running Qt is 6.5 or newer (skipped below 6.5, where `qtOlderThan65DisablesGpuEffects` checks the gate); the software backend, llvmpipe and softpipe disable them; `NEXORA_NO_GLASS_BLUR` disables them; `gpuEffects` is false until probed and notifies only on change |
| `tst_settings` (unit) | `ui.reduceTransparency` defaults to `false` and rejects a non-boolean |
| `tst_console_qml` → `tst_settings_ui.qml` | the seven settings sections exist; keyboard navigation stays inside the screen; `ConfirmDialog` defaults to Cancel and Back cancels; a long value elides at half the row and is plain text |
| `tst_console_qml` → `tst_navigation.qml` | all eight tabs with keys only; tabs fit at TV resolutions and large scale |
| `tst_console_qml` → `tst_gamedetails.qml` | markup in a title or note is shown literally; a failed save keeps the editor open |
| `tst_qmlsmoke` | every journey at 1280×720 and 1920×1080; `accessibilityModes` runs with high contrast and reduced motion; `layoutAtLargeScale` (150 %: grid/list fallback, tabs, keyboard, editor), Back from a page to the tab bar, modal key trapping, confirmation before stopping a game and for game bar power actions, Settings action feedback, default-emulator rows keeping focus, "Other" and an empty system in the Games filter, lazy hardware probe, the rolled-back update card and its dismissal; any QML warning fails the test; `glassChromeFloatsWithoutCoveringContent` (both sizes): the scene fills the window, the bars are inset and floating, `chromeTop` / `chromeBottom` cover them, and walking down the Games grid, the Systems grid and the Settings list scrolls them under the hint bar while the last row ends fully above it; `dockedChromeLayout`: dark and high contrast keep the docked header, tab bar and hint bar, with and without hints; `glassBarsSwallowClicks`: a click and double-click on the hint bar over a card neither selects nor opens it, and a tab still takes its click; `emulatorsLaunchAndProfiles` also checks that `Theme.sessionActive` is true only while the emulator runs and that Reduce transparency reaches `Theme.reduceTransparency` |
| `scripts/qmllint.sh` | 0 errors (155 warnings, mostly unqualified access, after the verification fixes) |

All UI tests run on the offscreen platform with the software renderer, so they exercise only the flat glass path; nothing has been checked on a real TV or with a screen reader.

## Limitations

- Only English; `ui.language` accepts `en` only.
- No screen-reader support was tested (components set `Accessible` names and roles, but no assistive technology was used to check them).
- Contrast ratios are documented design intent, not measured by an automated test.
- No light theme; no per-user font choice.
- Text contrast in glass depends on where the glows fall (and, with the blur, on what is behind a bar) and is design intent, not measured, apart from the prominent button label.
- The GPU blur path (`GlassEffectGpu`, `GlassGroupBlur`) has not been verified on real Intel/AMD hardware or on any Qt 6.5 or newer build; CI and the local tests run only the flat path (Qt 6.4.2, software backend). Frame cost while scrolling and the visual result of the blur are unknown until someone checks a 1080p run.
- Emulators (a horizontal list) and System/Diagnostics stop between the floating bars instead of scrolling under them.
- In-page dialogs (confirm, metadata editor, on-screen keyboard, launch-profile picker) always use the dense flat glass, because their backdrop would be an ancestor of the dialog.
- The game bar and quick menu get a faint `clear` glass fill on top of the `overlay*` palette.
- Edge refraction (lensing) is not implemented; it would need a custom shader (`.qsb`, `qt6-shadertools`).
- Artwork is a generated placeholder; there is no cover-art download or import.
