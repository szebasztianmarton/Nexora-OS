# Sub-project 1 — Notifications, emulator capabilities, game sessions (Quick Resume)

Approved by the user on 2026-10-17 (plan steps 1-3 of docs/console-experience.md). One PR.

## Goals

1. A central notification service replacing the single overwriting Toast.
2. An emulator capability model with save-state adapters, proven end to end with the fake emulator.
3. A game session manager that gives Nexora a real Quick Resume based on persistent save states, and that never silently kills a running game at shutdown.

Non-goals: packaging RetroArch, several live emulator processes at once, collections, a customisable Home, per-game profiles beyond the Quick Resume exclusion.

## 1. NotificationService

- C++ `nexora::ui::NotificationService` (console-ui), exposed to QML as `notifications`.
- A notification: id, category (`controller`, `game`, `saveState`, `library`, `storage`, `update`, `system`, `recovery`), severity (`info`, `warning`, `critical`), title/text, dedup key, count, timestamps.
- Queue: one visible notification at a time; `critical` jumps the queue (a shown info/warning goes back to the front of the queue); each auto-hides after `notifications.durationMs` (info), twice that (warning) or three times that (critical) and stays in the history. (Changed during implementation: a critical that never hides would block the toast for every later event; the history keeps it.)
- Dedup: an identical key within 10 s increments `count` of the visible/queued entry instead of adding one.
- History: newest 100 in memory; warnings and criticals are also persisted (newest 50) in `<state>/notifications.json` (atomic, versioned) and reloaded at start.
- Settings: `notifications.quiet` (bool; info notifications go to history only), `notifications.hiddenCategories` (array; hidden categories go to history only, criticals are never hidden), `notifications.durationMs` (2000-15000, default 3500).
- Sources in this PR: controller connected/disconnected, game started/exited/failed, save-state saved/failed, session restore failed, library notices, settings notices. Toast.qml becomes the view of the service.

## 2. Emulator capabilities and adapters

Emulator definition gains an optional `capabilities` object; `pausable` stays valid (equivalent to `capabilities.pause = true`).

```json
"capabilities": {
  "pause": true,
  "saveState": {
    "adapter": "control-socket" | "retroarch",
    "timeoutMs": 10000,
    "resumeArguments": ["--load-state", "{state-file}"],
    "port": 55355
  }
}
```

- `control-socket` (Nexora's own line protocol): Nexora listens on a per-session Unix socket and passes its path through the `{control-socket}` placeholder; the emulator connects and answers `SAVE <dir>` with `OK <file>` / `ERR <message>` and `QUIT` with `OK`. Resume passes `resumeArguments` with `{state-file}`. Implemented by the fake emulator; usable by wrapper scripts.
- `retroarch` (unverified until RetroArch is integrated): `--appendconfig <session.cfg>` with `savestate_directory` set to the session directory and network commands enabled on `port`; save = UDP `SAVE_STATE` then wait for the state file to change; quit = UDP `QUIT`; resume = `--entryslot 0` with the same config.
- Interface `SaveStateAdapter`: `prepareLaunch(session dir, resume file) -> extra arguments`, `save(dir, timeout)`, `requestQuit()`, all asynchronous with a single completion signal carrying success/error.
- Unknown adapters or malformed blocks are config errors (the emulator stays usable without the capability). Arguments never pass through a shell.

## 3. GameSessionManager

- Owns the lifecycle on top of `EmulatorManager` (still one process at a time).
- Session record: id (UUID), gameId, gameTitle, gamePath, gameSize, gameModified, emulatorId, emulatorVersion, profileId, pid, startedAt, lastActiveAt, strategy (`none` | `memory` | `state`), state, error, stateFile, stateBytes, stateSha256, savedAt.
- States: Starting, Running, Suspending (SIGSTOP pending), SuspendedInMemory (paused in the Quick Menu), SavingState, SuspendedPersistently, Resuming, Closing, Completed, Failed, RecoveryRequired.
- Suspend to Home (only with a saveState capability and when the game is not excluded): SavingState → adapter save → verify file exists, non-empty, compute SHA-256 → write meta → Closing (adapter quit, then SIGTERM after timeout) → SuspendedPersistently. Any failure: the game keeps running (or the session becomes Failed if the process died) and a notification explains it; the previous valid state of the game is never removed by a failed save.
- Resume: Resuming → validate (game file exists with same size/mtime or user confirms, emulator configured and available, emulator version equal to the recorded one, state file present with matching SHA-256) → launch with resume arguments → Running. Validation failure → RecoveryRequired with the reason; UI offers "Start fresh" (keeps the state) or "Remove saved state".
- Persistence: `<state>/sessions.json` (version 1, atomic) for records; `<data>/states/<session id>/` for state files and `meta.json`.
- Limits (settings): `sessions.maxCount` (1-20, default 5), `sessions.maxBytes` (64 MiB-16 GiB, default 512 MiB), `sessions.maxAgeDays` (0 = off, default 30). Exceeding any limit when suspending: the suspend is refused, nothing is deleted, the UI lists sessions that could be removed (oldest, failed and RecoveryRequired first).
- Per-game exclusion: `sessions.excludedGames` (array of game ids); excluded games show "Quick Resume off" and only Close.
- Shutdown: UI power-off/restart first suspends the running game when possible (else asks Close vs Cancel), then requests power. App exit by signal: graceful stop (adapter quit, SIGTERM, stop timeout) instead of immediate SIGKILL; a failed save is recorded and shown as a notification at next start.
- Unexpected process exit while Running: session Completed (exit 0) or Failed (crash), no state written.

## UI in this PR

- Home: "Suspended" row (data-driven, hidden when empty) with Resume / Close; RecoveryRequired entries show the reason.
- Quick Menu: "Suspend and go Home" (when supported), existing Pause renamed to show the in-memory state.
- Resume failure dialog (ConfirmDialog based).

## Tests

Unit: notification dedup/queue/history/quiet; capability parsing; session state machine transitions; meta validation (missing, corrupt, wrong version, wrong sha). Integration with the fake emulator: suspend → state file written → process exited → resume with load argument → fake reports the loaded state; save failure keeps the game running; limits refuse without deleting; excluded game cannot suspend; crash → Failed; shutdown saves. QML: Home suspended row, Quick Menu entry, resume failure dialog.
