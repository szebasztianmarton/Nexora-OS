# Game bar

The game bar is a quick overlay in the style of the Xbox Game Bar. It opens on top of the dashboard or a running emulator and closes again without leaving what you were doing.

| | Controller | Keyboard |
| --- | --- | --- |
| Open / close | Guide (Xbox / PS / Home button) | Ctrl+G |
| Close | Guide, East (B) | Esc, Backspace, Ctrl+G |
| Move between widgets | Left / Right | ← → |
| Move inside a widget | Up / Down | ↑ ↓ |
| Activate | South (A) | Enter, Space |
| Volume | Left / Right on the volume slider | ← → on the slider |

A click outside the widgets also closes it. While the bar is open, nothing reaches the dashboard or the running-emulator overlay underneath.

The Guide button is the `gameBar` action in `input.mapping` and can be remapped like the other actions ([controller-support.md](controller-support.md#configurable-mapping)). If Guide is not bound to anything, it still opens the game bar, so mappings saved before the bar existed keep working.

## Widgets

![Game bar over a running emulator](images/game-bar-running.png)

| Widget | Content |
| --- | --- |
| Menu bar (top) | Nexora logo, clock, first connected controller (or the input backend when none is connected). |
| Now playing | While an emulator runs: its title, state, session time, **Back to game** (closes the bar) and **Stop emulator** (needs a second press). With nothing running: **Home**, **Games** and **Settings** jump to that dashboard tab. |
| Performance | CPU usage with a graph of the last 30 seconds, memory used / total, 1-minute load average. |
| Audio & power | Volume slider (the `audio.volume` setting; applied only on the embedded platform, see [settings.md](settings.md#keys)), **Reboot** and **Power off** (each needs a second press; only enabled where power control is available). |

Performance figures come from `/proc/stat` and `/proc/meminfo` and are sampled once per second, **only while the bar is open**; a closed bar costs nothing. The first CPU figure appears one second after opening (it needs two samples).

## Over a running emulator

Controller navigation of the launcher is normally suspended while an emulator runs, because the pad is not grabbed and the emulator should get it. The Guide press is the exception: it is reported even while suspended (`InputController::gameBarRequested`). Opening the bar then:

1. lifts the suspension, so the pad drives the bar;
2. if the Nexora window is not the active window, raises and activates it (`QWindow::raise()` / `requestActivate()`).

Closing the bar (Guide, Back or **Back to game**) suspends navigation again and, if the bar raised the window, lowers it so the emulator window is in front again.

## Limitations

- **The game keeps running and still sees the pad.** Devices are not grabbed, so buttons pressed while the bar is open also reach the emulator. There is no pause.
- Raising and lowering the window depends on the window manager. On a bare KMS/EGLFS setup without one, whether the emulator or Nexora is visible is decided by the display stack, not by the bar.
- No capture (screenshot / recording) widget, no GPU, FPS or temperature figures, no per-game audio mix.
- The CPU figure is the whole system, not just the emulator.

## Implementation

| Part | File |
| --- | --- |
| QML overlay | `apps/console-ui/qml/GameBar.qml` (layer `z: 30` in `Main.qml`, above the running overlay) |
| State, sampling, session time | `apps/console-ui/src/GameBarController.{h,cpp}` (QML context property `gameBar`) |
| CPU counters | `SystemIntegration::cpuTimes()` in `apps/system/` |
| Guide → `gameBarRequested` | `InputController::injectAction()`, `ButtonMapping` (`gameBar` action) |
| Input suspension | `AppController::setGameBarOpen()` |

Tests: `tst_qmlsmoke` (`gameBarOverlay`), `tst_appcontroller` (`gameBarOverRunningEmulator`), `tst_inputcontroller`, `tst_buttonmapping`, `tst_systemintegration`.
