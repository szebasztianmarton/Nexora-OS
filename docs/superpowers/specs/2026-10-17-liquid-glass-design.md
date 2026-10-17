# Liquid Glass for the console UI — design

Date: 2026-10-17 · Branch: `feature/liquid-glass` · Status: approved in chat, awaiting spec review

## Goal

Give the whole console UI (`apps/console-ui/qml/`) an Apple "Liquid Glass" look: real backdrop
blur with vibrancy, a specular highlight and a bright rim on glass surfaces, floating chrome that
content scrolls under, and a glass focus pill that morphs between controls. It upgrades the
existing `glass` theme (the default) instead of adding a fourth theme.

Reference material: the SwiftUI Liquid Glass APIs (`glassEffect`, `GlassEffectContainer`,
`glassEffectID`, `.buttonStyle(.glass)`, `.buttonStyle(.glassProminent)`) and
<https://0xminds.com/blog/guides/liquid-glass-ui-prompts-ai-tailwind-guide> (blur, fill, border
and glass-count values). The SwiftUI rule "prefer system controls and native APIs over custom
blurs" translates here to: **one shared primitive built on Qt Quick's own effect stack, never a
per-screen hand-rolled blur.**

## Constraints

- **Qt versions.** The minimum stays Qt 6.4 (Ubuntu 24.04 / WSL development ships 6.4.2). The
  image ships Qt 6.9.1, where `QtQuick.Effects` (`MultiEffect`, Qt 6.5+) is available. The GPU path
  is therefore loaded at runtime and is optional; nothing in the build requires `QtQuick.Effects`.
- **Renderers.** Tests run with `QT_QUICK_BACKEND=software`; the image may fall back to llvmpipe.
  Both must get the current blur-free glass, unchanged in appearance.
- **Running emulator.** Nothing may repaint continuously behind a running emulator, so blur is off
  while an emulator session is active. The game bar and quick menu draw over another process's
  frame, which cannot be sampled: they always use the blur-free glass.
- **Accessibility.** High contrast keeps replacing glass entirely. Reduced motion collapses the
  morph and highlight animations. A new **Reduce transparency** setting forces the blur-free glass
  with denser fills.
- **Performance budget.** At most three blurred regions per screen (floating header, tab bar, hint
  bar, or one modal); glass inside a `GlassGroup` shares one blur source.

## SwiftUI → QML mapping

| SwiftUI | Nexora | Purpose |
|---|---|---|
| `.glassEffect(.regular / .clear)` | `GlassSurface { variant: "regular" / "clear" }` | a glass background for any panel, bar or card |
| `.glassEffect(.regular.tint(c))`, `.glassProminent` | `GlassSurface { variant: "prominent"; tint: c }` | accent-tinted glass for primary actions |
| `.interactive()` | `GlassSurface { interactive: true }` | brightens and lifts on focus/press |
| `GlassEffectContainer` | `GlassGroup` | children share one backdrop capture and one blur pass |
| `glassEffectID` + namespace | `GlassFocusPill` | one glass capsule that animates between focused items |
| `.buttonStyle(.glass)` / `.glassProminent` | `NxButton { style: "glass" / "glassProminent" }` | glass buttons |

## Components

All new files live in `apps/console-ui/qml/` and are registered in its `CMakeLists.txt` like
the existing ones.

### `Theme` additions

- `property bool reduceTransparency` — bound in `Main.qml` to `settings.reduceTransparency`.
- `property bool gpuEffects` — bound in `Main.qml` to `UiCapabilities.gpuEffects` (below).
- `property bool sessionActive` — bound in `Main.qml` to "an emulator session is starting,
  running or paused" (derived from `AppController.state`).
- `readonly property bool glassBlur: glassActive && gpuEffects && !reduceTransparency &&
  !sessionActive` — the single switch every glass component reads.
- Tokens (from the reference values, dark mode): `glassBlurRadius` (32 base px × scale, about
  `backdrop-blur-xl`), `glassSaturation` (0.35), `glassFillRegular` (white 10 %),
  `glassFillClear` (white 5 %), `glassFillProminent` (accent 30 %), `glassRim` (white 20 %),
  `glassRimStrong` (white 30 %), `glassHighlight` (white 18 % → 0 %, top to 45 % height),
  `glassShadow` (black 35 %, 24 px blur, 8 px y offset, GPU only). Under `reduceTransparency`
  the fills become the existing dense `surfaceFloating` values.
- `radiusPill` (height / 2 helper) and `radiusXL` (24 base px) for capsules and large sheets.

### `UiCapabilities` (C++, `apps/console-ui/src/`)

A small QML singleton (registered in `nexora_ui`) with one constant property, `gpuEffects`.
It is true when the scene graph is not the software backend, the GL renderer string does not
contain `llvmpipe` or `softpipe`, and the environment variable `NEXORA_NO_GLASS_BLUR` is unset.
It is computed once after the first frame (`QQuickWindow::sceneGraphInitialized`); until then it
is false. Unit test: forced via the environment variable and the software backend.

### `GlassSurface.qml` (≈ `glassEffect`)

An `Item` placed as the background of a panel. Properties: `variant` (`regular`, `clear`,
`prominent`), `tint` (colour, defaults to `Theme.accent` for prominent), `radius`,
`interactive`, `active` (focused/pressed state supplied by the owner), `backdrop` (the `Item` to
blur; defaults to the window's content root, see `GlassGroup`).

Layers, bottom to top:
1. **Shadow** (GPU path only, via `MultiEffect` shadow on the shape).
2. **Backdrop**: GPU path — a `ShaderEffectSource` of the backdrop region under the surface
   (`live: true`, `sourceRect` mapped to the surface), fed into `MultiEffect` with `blurEnabled`,
   `blurMax = Theme.glassBlurRadius`, `saturation = Theme.glassSaturation`, masked to the rounded
   shape. Flat path — nothing (the existing backdrop shows through).
3. **Fill**: the variant's fill colour (prominent: tint mixed at 30 %).
4. **Specular highlight**: a vertical gradient from `glassHighlight` at the top fading to
   transparent by 45 % of the height, clipped to the shape.
5. **Rim**: a 1 px border, `glassRim` (`glassRimStrong` when `active`). It replaces `GlassSheen`,
   which is removed once every user has migrated.

The two paths are separate files, `GlassEffectGpu.qml` (imports `QtQuick.Effects`) and
`GlassEffectFlat.qml`. `GlassSurface` picks one with a `Loader`: `GlassEffectGpu.qml` only
when `Theme.glassBlur`, falling back to `GlassEffectFlat.qml` if that load reports
`Loader.Error` (Qt < 6.5). The fallback is silent apart from one `nexora.ui` debug log line.
Switching paths (for example when a game starts) swaps the loader source and does not rebuild
the owner.

When `interactive` and `active`: the fill steps up one level (regular 10 % → 15 %) and the
surface scales to 1.02 over `Theme.animFast`. Under reduced motion the scale step is skipped.

### `GlassGroup.qml` (≈ `GlassEffectContainer`)

An `Item` that owns one `ShaderEffectSource` of its `backdrop` and exposes it to the
`GlassSurface` children through an attached-style property (`GlassGroup.source`, found by
walking parents). Surfaces inside a group sample that shared texture instead of creating
their own. Use it for the three floating bars (one group) and for multi-panel dialogs.

### `GlassFocusPill.qml` (≈ `glassEffectID`)

A single `GlassSurface { variant: "regular"; interactive: true; active: true }` per container
(tab bar, hint bar, quick menu, settings section list). It binds its geometry to the container's
current focused item, mapped into the container, and animates `x`, `y`, `width` and `height`
with a `SpringAnimation`-like easing (`Easing.OutBack`, amplitude 1.1, `Theme.animNormal`). This
gives the "liquid" morph between items. Under reduced motion it jumps. It replaces the current
accent highlight rectangles in those containers; the focus ring (`Theme.focusRing`) stays drawn
on top for accessibility.

### `NxButton` styles

`NxButton` gets a `style` property: `"glass"` (regular glass capsule, primary text) and
`"glassProminent"` (prominent glass with `Theme.accent` tint, white text). These are the
defaults in the glass theme for, respectively, secondary and primary buttons. The existing
styles stay for `dark` and `highContrast`. Text on prominent glass must stay ≥ 4.5:1 against the
flat-path fill; tested.

## Layout changes

- **Floating chrome.** In the glass theme, `Header`, `NavTabBar` and `HintBar` become floating
  glass bars inset by `Theme.space4` from the window edges with `radiusXL` corners, inside one
  `GlassGroup`. `pageArea` fills the whole window under them; each page gets top and bottom
  content margins equal to the bar heights so the first and last rows are never hidden, and
  scrolling content (game grid, settings list) passes underneath the bars. The `dark` and
  `highContrast` themes keep today's docked layout (the anchors switch on `Theme.glassActive`).
- **Floating panels.** `ConfirmDialog`, `GameOptionsDialog`, `MetadataEditor`,
  `OnScreenKeyboard`, `UpdatePanel`, `TutorialOverlay`, the launch-profile picker in `EmulatorsScreen` and `Toast` use
  `GlassSurface` (regular variant, `radiusXL`) over a `black 40 %` scrim. Their content is
  unchanged.
- **Cards and rows.** `EmulatorCard`, `SettingRow` and `GameTile` info plates use
  `GlassSurface { variant: "clear" }` and never the GPU path (too many per screen): they read
  `Theme.glassActive`, not `Theme.glassBlur`.
- **Game bar / quick menu.** These keep their `overlay*` palette but take `GlassSurface`'s
  highlight and rim layers on the flat path only.

## Settings

New key `ui.reduceTransparency` (boolean, default `false`, "Reduce transparency") in
`shared/src/Settings.cpp`, exposed as `SettingsController.reduceTransparency`, shown in
Settings › Accessibility next to Reduced motion. Documented in `docs/settings.md`.

## Error handling

- `QtQuick.Effects` missing → `Loader.Error` → flat path, one debug log line, no user message.
- Capability probe not yet run → `gpuEffects` false, so the first frame is always flat.
- A GPU shader failure at runtime cannot be detected from QML; `NEXORA_NO_GLASS_BLUR=1` (also
  documented in `docs/troubleshooting.md` and `docs/design-system.md`) is the
  escape hatch.

## Testing

- `tests/qml/tst_theme.qml`: `glassBlur` truth table (high contrast, reduce transparency,
  session active, no GPU each disable it); reduce-transparency fills equal the dense values;
  new tokens scale with `Theme.scale`.
- New `tests/qml/tst_glass.qml`: `GlassSurface` loads the flat path under the software
  backend; with `glassBlur` forced true on Qt < 6.5 it falls back to flat without errors; on
  Qt ≥ 6.5 with the effects module available it loads the GPU file (skipped otherwise);
  `interactive`/`active` raise the fill; `GlassFocusPill` follows the focused child and jumps
  under reduced motion; prominent button text contrast ≥ 4.5:1.
- `tests/unit`: `UiCapabilities` is false with the software backend and with
  `NEXORA_NO_GLASS_BLUR`; `ui.reduceTransparency` default and validation in `tst_settings`.
- `tst_qmlsmoke`: all screens in glass at 1280×720 and 1920×1080 with the floating layout: the
  first grid row and the last settings row are not covered by a bar (geometry check).
- Manual (cannot run in CI): the GPU path on a real Intel/AMD machine at 1080p — blur visible
  under the bars over the game grid, no frame drops when scrolling, blur gone while a game runs.
  Recorded as "not yet verified" in `docs/design-system.md` until someone does it.

## Rollout (one task each, subagent-driven)

1. `Theme` tokens and flags, `UiCapabilities`, `ui.reduceTransparency` setting and its tests.
2. `GlassSurface` (both paths), `GlassGroup`, `tst_glass.qml`.
3. Floating chrome: `Header`, `NavTabBar`, `HintBar`, page margins, smoke geometry checks.
4. Floating panels, cards and rows migrated to `GlassSurface`; `GlassSheen` removed.
5. `GlassFocusPill` in the tab bar, hint bar, quick menu and settings; `NxButton` glass styles.
6. Docs: `docs/design-system.md` (Liquid Glass section replacing "Glass without blur"),
   `docs/settings.md`, `docs/troubleshooting.md` (`NEXORA_NO_GLASS_BLUR`), Settings screen text.

## Out of scope

- Edge refraction / lensing (needs a custom `.qsb` shader and `qt6-shadertools`); possible
  follow-up.
- Light-mode glass.
- Blurring the emulator's frame under the game bar.
- Animated backdrop.
