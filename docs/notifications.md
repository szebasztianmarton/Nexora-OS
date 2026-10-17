# Notifications

The console UI reports events (a controller connected, a game failed to start, a library scan finished) through one service, `NotificationService` (`apps/console-ui/src/NotificationService.{h,cpp}`). The toast at the bottom of the screen (`Toast.qml`) only displays the service's current entry; the System tab lists its history. Design: [ADR-003](../.10x/adrs/003-notification-service.md).

## Contents

- [What a notification is](#what-a-notification-is)
- [Display rules](#display-rules)
- [History](#history)
- [Settings](#settings)
- [Producers](#producers)
- [Limitations](#limitations)
- [Tests](#tests)

## What a notification is

| Field | Meaning |
| --- | --- |
| category | `controller`, `game`, `saveState`, `library`, `storage`, `update`, `system`, `recovery` |
| severity | `info`, `warning`, `critical` |
| text | plain text (rendered with `Text.PlainText`), trimmed; empty text is ignored |
| key | deduplication key; default `category:text` |
| count | how many times the event happened within the dedup window |
| createdAt / updatedAt | UTC times of the first and the last occurrence |

## Display rules

- **One at a time.** Further notifications wait in a queue.
- **Duration.** Info is shown for `notifications.durationMs` (default 3.5 s), warnings for twice and criticals for three times as long. Criticals are not kept on screen indefinitely; they stay in the history.
- **Criticals go first.** A critical notification jumps the queue; if an info or warning is on screen, it is put back at the front of the queue and shown again afterwards.
- **Deduplication.** The same key within 10 seconds of the last occurrence does not show a new toast: the existing entry's count increases (shown as "×3"), its text is replaced by the latest wording and its severity is raised if needed. A shown entry restarts its timer.
- **Quiet mode** suppresses the toast for info notifications. **Hidden categories** suppress the toast for warnings and infos of those categories. Criticals are always shown. Suppressed notifications still go to the history.
- **During a game** the emulator owns the display on the image (EGLFS/KMS), so toasts posted while a game runs are seen when the shell is visible again (Guide), if they have not timed out; the history keeps them either way.

## History

Newest first, at most 100 entries in memory; the System tab ("Notifications" section) shows the newest 15 with count and severity. Warnings and criticals (newest 50) are written atomically to `<state>/notifications.json` and loaded at the next start, marked "before the last restart", so a failure reported while shutting down is still visible after the next boot. Loaded entries are history only: they do not pop up again and are not merged with new events. An unreadable or corrupt file is moved aside (`notifications.json.corrupt-…`) and the UI starts with an empty history. Info notifications are not persisted.

## Settings

| Key | Default | Meaning |
| --- | --- | --- |
| `notifications.quiet` | `false` | Quiet mode (Settings › General › "Quiet notifications"). |
| `notifications.durationMs` | `3500` | Base display time, 2000–15000 ms (Settings › General › "Notification time", 2–15 s). |
| `notifications.hiddenCategories` | `[]` | Category names whose non-critical toasts are suppressed; no UI row yet, set in the settings file. |

See [settings.md](settings.md#keys).

## Producers

| Event | Category, severity | Source |
| --- | --- | --- |
| Controller connected / disconnected (any screen, game running or not) | controller, info / warning | `UiContext::wireNotifications()` from `InputController` |
| Game launch or emulator error (`AppController` status with error flag) | game, warning | `UiContext::wireNotifications()` |
| Library notices (scan results, errors) | library, info / warning | `Main.qml` → `postMessage()` |
| Settings notices | system, info / warning | `Main.qml` → `postMessage()` |

Save-state and recovery notifications are added with Quick Resume ([console-experience.md](console-experience.md#dependency-aware-plan), step 3).

## Limitations

- No toast over a running game on the image (see above); no sound or vibration.
- Hidden categories have no UI row yet.
- The history cannot be filtered in the UI; it is cleared only by `clearHistory()` (not exposed in the UI yet) or by deleting the file.

## Tests

`tst_notificationservice` (dedup, window expiry, explicit keys, queue order and criticals, durations, quiet mode and hidden categories, bounded history, persistence, corrupt file, QML mapping) and `tst_uicontext::controllerEventsBecomeNotifications`. The toast and the System-tab section are rendered by `tst_qmlsmoke`.
