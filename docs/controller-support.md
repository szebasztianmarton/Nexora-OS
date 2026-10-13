# Controller support

Nexora is meant to be used with a game controller only. This page describes how gamepad input reaches the UI, which controllers are expected to work, how buttons are mapped and remapped, and how to test with real hardware.

**Test status:** no physical controller has been tested with either backend. Everything below is implemented and covered by unit tests on the mapping and navigation logic (`tst_gamepadnavigator`, `tst_buttonmapping`, `tst_inputcontroller`, plus `tst_appcontroller`/`tst_uicontext` for the chord and remapping), but the device handling of the evdev and SDL3 backends has only been exercised without devices. The SDL3 backend was compile/link-verified against SDL 3.2.30 in an earlier MVP. Use the [manual test checklist](#manual-test-checklist) when testing real pads.

Code: `apps/console-ui/src/input/`.

## Contents

- [Architecture](#architecture)
- [Backends](#backends)
- [Button names and controller layouts](#button-names-and-controller-layouts)
- [Default mapping](#default-mapping)
- [Configurable mapping](#configurable-mapping)
- [Fixed behaviour](#fixed-behaviour)
- [Keyboard](#keyboard)
- [While an emulator runs](#while-an-emulator-runs)
- [Multiple controllers and disconnects](#multiple-controllers-and-disconnects)
- [Known limitations](#known-limitations)
- [Manual test checklist](#manual-test-checklist)
- [Planned](#planned)

## Architecture

```
evdev (/dev/input/event*)  ─┐
                            ├─> GamepadNavigator ──> InputController ──> QKeyEvent ──> focused QML window
SDL3 gamepad API (optional) ┘    buttons/axes →        actions →          (posted)
                                 actions, repeat,      Qt keys, suspension,
                                 chord, per device     capture
```

1. A **backend** (`EvdevGamepadBackend` or `Sdl3GamepadBackend`) turns device events into backend-neutral `GamepadButton` presses/releases and `GamepadAxis` values (−1…1), tagged with a device id.
2. **`GamepadNavigator`** maps buttons to navigation actions through the `ButtonMapping`, turns D-pad/hat/left stick into directions with auto-repeat, and detects the Select + Start chord.
3. **`InputController`** owns the navigator and at most one backend, drops actions while suspended, handles remap capture, and posts a synthesized key press + release for each action to the focused window.
4. The **QML UI** only handles ordinary key events, so gamepad and keyboard behave identically.

All of this runs on the GUI thread; backends never block it (non-blocking reads / a 16 ms poll timer).

## Backends

Selected by `--input-backend` or, if not given, the `input.backend` setting (default `auto`); changes to the setting apply at the next start.

| Value | Behaviour |
| --- | --- |
| `auto` | SDL3 if it was built in and initialises; otherwise evdev if built in and `/dev/input` exists; otherwise no backend ("No gamepad backend available; keyboard only"). Never fails. |
| `evdev` | evdev only. Fails (backend "none", keyboard still works) if not built or `/dev/input` is missing. |
| `sdl3` | SDL3 only. Fails if not built in or SDL cannot initialise the gamepad subsystem. |
| `none` | Controller input disabled. |

Only one backend runs at a time. The Diagnostics tab shows the backend name, its status text and the connected devices.

### evdev (default on Linux)

Built whenever `linux/input.h` is available.

- Reads `/dev/input/event*` directly, non-blocking.
- **Device filter:** only devices advertising `BTN_GAMEPAD` are kept open; keyboards, mice, power buttons and similar are closed again immediately. Joysticks that advertise only `BTN_JOYSTICK`/`BTN_TRIGGER` codes are therefore not used (expected; not tested).
- **Not grabbed:** other programs (including a running emulator) receive the same events. This is why navigation is [suspended while an emulator runs](#while-an-emulator-runs).
- **Codes used** (kernel gamepad codes, see [Button names](#button-names-and-controller-layouts)):

| evdev code | Nexora button / axis |
| --- | --- |
| `BTN_SOUTH` (= `BTN_A`) | `south` |
| `BTN_EAST` (= `BTN_B`) | `east` |
| `BTN_NORTH` (= `BTN_X`) | `north` |
| `BTN_WEST` (= `BTN_Y`) | `west` |
| `BTN_START` / `BTN_SELECT` / `BTN_MODE` | `start` / `select` / `guide` |
| `BTN_TL` / `BTN_TR` | `l1` / `r1` |
| `BTN_DPAD_UP/DOWN/LEFT/RIGHT` | D-pad directions |
| `ABS_X` / `ABS_Y` | left stick (range from `EVIOCGABS`) |
| `ABS_HAT0X` / `ABS_HAT0Y` | hat (D-pad reported as an axis, −1…1) |

Kernel auto-repeat events (value 2) are ignored; the navigator does its own repeat. After `SYN_DROPPED` the device's state is reset and events are ignored until the next `SYN_REPORT`.

- **Permissions:** the user needs read access to `/dev/input/event*`, normally via the `input` group (`sudo usermod -aG input $USER`, then log in again). If no gamepad could be opened and some nodes were refused, the status reads "No gamepad readable: N of M input devices in /dev/input are not accessible (permission denied). Add your user to the 'input' group …". On the Buildroot image the `nexora` user is in `input` and a udev rule sets `/dev/input/event*` to `root:input 0660` ([buildroot.md](buildroot.md#users-and-permissions)). Nexora refuses to run as root by default.
- **Hotplug:** `/dev/input` is watched (`QFileSystemWatcher`); changes are debounced by 500 ms and new event nodes are probed. A read error (e.g. `ENODEV` on unplug) closes the device.

### SDL3 (optional)

Built only with `-DNEXORA_WITH_SDL3=ON` or `AUTO` when SDL3 is found ([development.md](development.md#optional-sdl3-backend)). Ubuntu 24.04 does not package SDL3 and **Buildroot 2026.02.3 has no SDL3 package, so the image uses evdev only** (`-DNEXORA_WITH_SDL3=OFF` in `nexora-os.mk`).

- Uses SDL's gamepad API, which normalises button positions across many controllers through SDL's built-in controller database. Nexora does not ship or load an additional mapping database; SDL's own mechanisms (e.g. the `SDL_GAMECONTROLLERCONFIG` environment variable) are SDL behaviour and were not tested with Nexora.
- Only the gamepad subsystem is initialised; events are polled every 16 ms on the GUI thread.
- Hints set: background joystick events allowed (input keeps arriving while an emulator window has focus), SDL signal handlers disabled.
- Buttons used: South/East/West/North, Start, Back (→ `select`), Guide, D-pad, left/right shoulder (→ `l1`/`r1`); axes: left stick X/Y. Triggers, right stick and other buttons are ignored.
- On Linux SDL also reads `/dev/input`, so the same permission requirements apply (expected; not tested).

## Button names and controller layouts

Buttons are named by **position**, not by the label printed on them, because labels differ between manufacturers. The table shows the conventional labels at each position. Whether a specific pad reports these positions correctly depends on its kernel driver (evdev) or SDL's database (SDL3). **None of these controllers has been tested**; the rows are what the Linux gamepad conventions and SDL lead us to expect.

| Nexora name | Position | Xbox | PlayStation | Nintendo (Switch Pro) | 8BitDo (X-input mode) |
| --- | --- | --- | --- | --- | --- |
| `south` | bottom face button | A | Cross (✕) | B | B (acts as A) |
| `east` | right face button | B | Circle (○) | A | A (acts as B) |
| `west` | left face button | X | Square (□) | Y | Y (acts as X) |
| `north` | top face button | Y | Triangle (△) | X | X (acts as Y) |
| `start` | right centre button | Menu (☰) / Start | Options | + | Start |
| `select` | left centre button | View / Back | Share / Create | − | Select |
| `guide` | central logo button | Xbox button | PS button | Home | Home / logo |
| `l1` | left shoulder | LB | L1 | L | L |
| `r1` | right shoulder | RB | R1 | R | R |

So with the default mapping, "confirm" is Cross on PlayStation and **B** on Nintendo-layout pads (the bottom button). 8BitDo pads expose different layouts depending on their mode (X-input, D-input, Switch, macOS); the column above assumes X-input mode, where they appear as an Xbox pad with Nintendo-style labels.

Known risk (unverified): the kernel defines `BTN_NORTH` and `BTN_WEST` as aliases of `BTN_X` and `BTN_Y`. Drivers that report the *label* codes `BTN_X`/`BTN_Y` for the *Xbox* X (left) and Y (top) buttons would make `west` and `north` appear swapped under evdev. SDL3 corrects such differences through its database. If you see this on real hardware, remap `favorite`/`search` or report it.

## Default mapping

Synthesized keys are fixed; the button for each remappable action comes from `input.mapping`.

| Action | Default button | Xbox / PlayStation / Nintendo label | Synthesized key | In the dashboard |
| --- | --- | --- | --- | --- |
| `accept` | `south` | A / ✕ / B | `Return` | Select / launch / confirm (destructive actions such as power off need a second press) |
| `back` | `east` | B / ○ / A | `Escape` | Leave a text field, close the profile picker or game options, cancel a pending confirmation or remap capture; on the tab bar it re-enters the page. Never quits the UI and never stops a running emulator. |
| `menu` | `start` | Menu / Options / + | `Menu` | Open the Diagnostics tab |
| `favorite` | `north` | Y / △ / X | `Favorites` | Toggle favorite of the focused game (Home rows, Games grid); in the Emulators tab's profile picker, make the selected profile the default |
| `search` | `west` | X / □ / Y | `Search` | Open the Games tab with the search field focused |
| `prevTab` | `l1` | LB / L1 / L | `PageUp` | Previous tab (Home, Games, Emulators, Settings, Diagnostics) |
| `nextTab` | `r1` | RB / R1 / R | `PageDown` | Next tab |
| `gameBar` | `guide` | Xbox / PS / Home | (none; `InputController::gameBarRequested`) | Open or close the [game bar](game-bar.md). Works while an emulator runs (navigation suspended). |
| `options` | `select` | View / Share (Create) / − | `Context1` | Open the [game options](game-library.md#game-options) of the focused game (Home rows, Games grid); closes them again |
| Up/Down/Left/Right | D-pad, hat, left stick | (fixed) | arrow keys | Move focus; Up at the top of a page moves to the tab bar; Left/Right change values on Settings rows |

The dashboard behaviour is defined in `apps/console-ui/qml/` and covered by `tst_qmlsmoke` with key events. The hint bar at the bottom shows the keys that apply to the focused element.

## Configurable mapping

The `input.mapping` setting maps action → button ([settings.md](settings.md#keys)):

```json
"input.mapping": {"accept": "south", "back": "east", "menu": "start", "favorite": "north",
                  "search": "west", "prevTab": "l1", "nextTab": "r1", "gameBar": "guide",
                  "options": "select"}
```

- Remappable actions: `accept`, `back`, `menu`, `favorite`, `search`, `prevTab`, `nextTab`, `gameBar`, `options`.
- Buttons: `south`, `east`, `west`, `north`, `start`, `select`, `guide`, `l1`, `r1`.

Validation (`ButtonMapping::fromVariantMap()`), applied when the setting is loaded or changed:

| Rule | On violation |
| --- | --- |
| Action and button names must be known | Whole mapping ignored |
| A button may be bound to only one action | Whole mapping ignored |
| `accept` and `back` must be bound | Whole mapping ignored |
| Other actions may be left out | They are simply unbound, except `options` (added in a later version): when a stored mapping does not mention it and `select` is free, it is bound to `select`, so mappings saved before it existed gain it |

An ignored mapping falls back to the **default** mapping; the settings screen lists "Controller mapping ignored (<reasons>); using defaults" and the log has a warning in `nexora.input`.

### Remapping from the UI

- On the Settings tab, each action has a row under "Controller". **Left/Right** cycles the button directly (works with keyboard only). **Accept** starts capture (`SettingsController.beginRemap(action)`): the next **non-directional** button press (any of the nine buttons above) becomes the binding. Directions keep navigating during capture; the captured press is not delivered as an action. Capture ends after the press or after 5 s (`cancelRemap()`), and is ignored while input is suspended.
- Note: during capture *every* face button, including the one bound to `back`, is captured as the new binding; the keyboard's Esc, pressing Confirm on the row again, or the 5 s timeout cancels (the row hint says so). Capture is only offered while a gamepad backend is running (`settings.controllerAvailable`); with backend `none` the row tells keyboard users to pick a button with ← →.
- `SettingsController.remap(action, button)` binds directly (used by Left/Right and by tests).
- Binding a button that another action already uses **swaps** them: the other action gets the old button of the remapped action, so every action stays reachable (example: binding `accept` to `east` moves `back` to `south`).
- Each change is validated and saved immediately. "Reset to defaults" restores the default mapping together with all other settings.

## Fixed behaviour

Not configurable in MVP 2:

| Behaviour | Details |
| --- | --- |
| Directions | D-pad buttons, hat axes and the left stick always navigate. Sticks/hats trigger at \|value\| ≥ 0.5 and release below 0.3 (hysteresis); flicking straight to the opposite side registers. |
| Auto-repeat | A held direction repeats after 400 ms, then every 120 ms. Only the most recently pressed direction repeats; releasing it stops repeat even if an older direction is still held. Buttons never repeat. |
| Return to launcher | Hold **Select + Start** on the same controller for **1 second**: the running emulator is asked to stop (SIGTERM, SIGKILL after its stop timeout, 5 s by default). Fires once per hold; releasing either button earlier cancels it. Works while navigation is suspended. Does nothing when no emulator is starting/running. |
| Guide / Select fallback | If `guide` is not bound to any action, pressing it triggers `gameBar` (so mappings saved before the game bar existed still open it); an unbound `select` triggers `menu`. Older stored mappings gain `options` on Select when Select is free, so with the default mapping neither fallback applies. |

Note: pressing Start or Select also triggers their normal action (by default `menu` and `options`) when navigation is not suspended, including at the start of the chord. While an emulator runs, navigation is suspended, so only the chord counts.

## Keyboard

The keyboard always works, with or without a controller backend. Because gamepad actions are delivered as key events, the same keys drive the UI:

| Key | Same as |
| --- | --- |
| Arrow keys | Directions |
| Enter / Return | `accept` |
| Esc | `back` |
| Menu (context-menu key) | `menu` |
| Page Up / Page Down | `prevTab` / `nextTab` |

`Favorites` and `Search` are media keys most keyboards lack, so the dashboard also accepts these keyboard-only keys:

| Key | Effect |
| --- | --- |
| `F1` | Diagnostics tab (like Menu) |
| `Ctrl+G` | Open / close the game bar (like Guide) |
| `F` (no modifier) on a focused game | Toggle favorite |
| `O` (no modifier) on a focused game | Game options (like `options`) |
| `/` or `Ctrl+F` | Search on the Games tab |
| `Ctrl+Tab` / `Ctrl+Shift+Tab` | Next / previous tab |
| `Backspace` | Same as Esc |
| `Space` | Activates buttons and rows like Enter |
| `Ctrl+Q` | Quit (development convenience) |

Text entry (search, adding a library folder) needs a keyboard: there is no on-screen keyboard.

## While an emulator runs

- Gamepads are not grabbed, so the emulator receives the same input. While an emulator is starting, running or stopping, `InputController` is **suspended**: all navigation actions are dropped, so pressing A in a game cannot click "Stop" in the launcher. Held directions and repeat are reset on suspend and resume.
- The Select + Start chord is still detected while suspended; it is the controller-only way back to the launcher.
- When the emulator exits, navigation resumes.
- Many emulators have their own Select + Start bindings (e.g. exit or menu); test the interaction per emulator.

## Multiple controllers and disconnects

- State is kept per device (evdev: file descriptor; SDL3: joystick instance id). One pad's stick noise does not release another pad's held direction.
- The most recent direction press, from any pad, is the one that repeats.
- The chord counts only when Select and Start are held on the **same** pad.
- All pads drive the same UI; there are no player slots.
- On disconnect (evdev read error, SDL `GAMEPAD_REMOVED`) that device's held buttons, directions and chord are dropped immediately, so nothing stays "stuck". Reconnecting is picked up by hotplug.

## Known limitations

- No physical controller tested (evdev or SDL3).
- No rumble, battery level, LEDs, gyro or touchpad.
- Triggers, right stick, L3/R3 and paddles are not used.
- No per-controller mapping; one mapping for all pads.
- evdev uses the kernel's codes as-is (no correction database): pads with non-standard drivers may have swapped or missing buttons; generic HID pads without `BTN_GAMEPAD` are ignored.
- SDL3 is not available on the Buildroot image.
- The input backend cannot be switched without a restart.
- The chord duration (1 s) and repeat timings are fixed.

## Manual test checklist

Run on a desktop with the debug build (`build/debug/bin/nexora-console-ui --windowed --input-backend evdev`, later `sdl3` if built) and on the image in QEMU with USB pass-through ([buildroot.md](buildroot.md#running-in-qemu)). Watch the log (`nexora.input` category) and the Diagnostics tab. Record controller model, connection (USB/Bluetooth), driver (`cat /proc/bus/input/devices`), backend and result for each item.

1. **Detection:** with the pad connected before start, the Diagnostics tab shows the backend, "1 gamepad connected" and the pad's name.
2. **Permissions:** as a user without `input` group membership, the status explains the permission problem; after adding the group and logging in again, the pad is detected.
3. **Hotplug:** unplug → "Gamepad disconnected" in the log and device list updates; plug in again → detected within about a second.
4. **Face buttons by position:** bottom = confirm, right = back, top = favorite, left = search. Note any swapped pair (especially west/north on Xbox pads under evdev).
5. **Start / Select / Guide:** Start opens the Diagnostics tab (menu action); Select on a game opens its options; Guide opens and closes the game bar.
6. **Shoulders:** L1/R1 switch to the previous/next tab.
7. **Directions:** D-pad, left stick and (if the pad reports one) the hat all move focus; a held direction repeats; releasing stops immediately; resting stick does not drift.
8. **Remap:** remap `favorite` to L1 from the settings screen; check that `prevTab` moved to the old favorite button, the new binding works, it survives a restart, and an unpressed capture times out after 5 s.
9. **Invalid mapping:** put `"accept": "south", "back": "south"` in the settings file; the settings screen reports the problem and the default mapping works.
10. **Emulator suspension:** launch a game; pressing buttons in the emulator does not navigate the launcher behind it.
11. **Game bar over a game:** in a running emulator press Guide → the Nexora window comes to the front with the game bar; the pad navigates it; Guide or "Back to game" returns to the emulator window.
12. **Return chord:** in a running emulator hold Select + Start for 1 s → emulator stops and the launcher has focus again; releasing earlier does nothing; an emulator that ignores SIGTERM is killed after its stop timeout.
13. **Two pads:** hold a direction on pad 1 while moving the stick on pad 2; holding Select on one pad and Start on the other does not trigger the chord.
14. **Disconnect while holding:** hold a direction and unplug; repeat stops.
15. **Keyboard alongside:** arrows/Enter/Esc keep working with a pad connected.
15. **Backend `none`:** `--input-backend none` → pad ignored, keyboard works.

## Planned

- Testing with real Xbox, PlayStation, Nintendo and 8BitDo pads and recording the results in this document.
- Optional correction database for evdev, or SDL3 on the image once Buildroot packages it.
- Per-controller mappings, configurable chord and repeat timings.
