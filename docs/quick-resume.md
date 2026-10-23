# Quick Resume

Suspend a game and continue it later, also after a restart. Nexora asks the emulator to write a **save state**, checks it, and lets the emulator quit; resuming starts the game again from that state. Owner: `GameSessionManager` (`apps/console-ui/src/GameSessionManager.{h,cpp}`); design: [ADR-001](../.10x/adrs/001-quick-resume-via-save-states.md), [ADR-002](../.10x/adrs/002-emulator-capability-adapters.md).

**Status (MVP 4):** implemented and tested end to end with `nexora-fake-emulator` (control-socket adapter). **No real emulator has been tested**: no emulator is packaged yet, and the RetroArch adapter is unverified ([emulator-integration.md](emulator-integration.md#retroarch-adapter-unverified)). A game can be suspended only if its emulator declares `capabilities.saveState`.

## Contents

- [Using it](#using-it)
- [How it works](#how-it-works)
- [Session states](#session-states)
- [Files](#files)
- [Resume checks](#resume-checks)
- [Limits and removal](#limits-and-removal)
- [Shutdown and power](#shutdown-and-power)
- [Errors](#errors)
- [Limitations](#limitations)
- [Tests](#tests)

## Using it

| Where | What |
| --- | --- |
| Quick Menu (Guide while playing) › **Suspend and go Home** | saves the game and closes the emulator. When it is not possible the row says why (no save-state support, Quick Resume off for the game, Quick Resume full, emulator not connected yet). |
| Home › **Quick Resume** row (hidden while empty, newest first) | A / Enter resumes; Select / O asks before removing the saved state. Each card shows when it was saved, its size, the emulator, and a warning when it cannot be resumed or is older than `sessions.maxAgeDays`. |
| Playing a game from the library (Play, Home rows, Games grid) | resumes it when it has a saved state; **Play with "<profile>"** in Game options starts fresh (the state is kept). |
| Game options › **Quick Resume: on/off** | per game; when off, the game always starts fresh and cannot be suspended (`sessions.excludedGames`). |
| "Can't resume" dialog | appears when a state cannot be resumed or did not load; **Start fresh** starts the game without it, **Not now** does nothing. The state is kept either way. |

There is one session per game and emulator (the same game on another emulator gets its own session). Only one game runs at a time; suspending is how you switch.

## How it works

1. **Launch.** For an emulator with a save-state capability (and the game not excluded), the session directory `<data>/states/<session id>/` is created and the adapter adds its arguments (control-socket: `launchArguments` with the socket path; when resuming also `resumeArguments` with the state file).
2. **Suspend.** If the emulator is paused (SIGSTOP from the Quick Menu) it is resumed first, because it has to answer. The adapter asks for a state (`SAVE <dir>`); the answer must arrive within `timeoutMs` and name a new, non-empty regular file directly inside the session directory (not in a subdirectory, not `meta.json`, not a file the session already recorded). Nexora computes its SHA-256 and writes `meta.json` atomically. Only then is the oldest state of the session (beyond the newest two) deleted.
3. **Close.** The adapter asks the emulator to quit (`QUIT`); if it is still running after `timeoutMs`, Nexora stops it (SIGTERM, then SIGKILL after the emulator's `stopTimeoutMs`).
4. **Resume.** The [checks](#resume-checks) run, then the game starts with the resume arguments. If the emulator fails within its startup grace period, the "Can't resume" dialog explains it.

A failed save never stops the game: the session goes back to Running and a notification says why ("Could not save … The game is still running.").

## Session states

| State | Meaning |
| --- | --- |
| Starting | launch accepted, process starting |
| Resuming | starting from a state |
| Running | the game runs |
| SuspendedInMemory | paused with SIGSTOP from the Quick Menu (not saved) |
| SavingState | waiting for the emulator to write a state |
| Closing | state saved, waiting for the emulator to quit |
| Failed | the process ended while a save was pending |

A saved session (Home row) is a `meta.json` record, not a running state. When a game that had a state is played (fresh or resumed) and then ends without being suspended, its record stays resumable with the note "Played after this state was saved; resuming returns to <time>": nothing is removed silently.

## Files

```
<data>/states/<session id>/          owner-only (0700)
    meta.json                        {"version":1, id, gameId, gameTitle, gamePath, gameSize,
                                      gameModified, emulatorId, emulatorVersion, profileId,
                                      adapter, createdAt, note, states:[{file, bytes, sha256, savedAt}]}
    <state files>                    named by the emulator (fake: fake-<pid>-<n>.state)
    retroarch/, nexora-retroarch.cfg retroarch adapter only
```

`<data>` is `$XDG_DATA_HOME/nexora` (`~/.local/share/nexora`). Each session's `meta.json` is the only record of it; there is no global index, so a damaged file affects one session only. At start every `meta.json` is read strictly (version, ids, file names, sizes, 64-character SHA-256, 1–2 states); an unreadable one is renamed `meta.json.corrupt-<time>`, its state files are left in place, and a notification names the directory. Quick Resume states are not part of [backups](user-data.md#backup-and-restore).

## Resume checks

Before a resume (the Home card shows the cheap ones without hashing):

| Check | Message when it fails |
| --- | --- |
| newest state file exists with the recorded size | The saved state is missing or damaged |
| game file exists | The game file is missing |
| emulator configured and still declares save states | Emulator "…" is not configured / … no longer declares save-state support |
| SHA-256 of the state matches | The saved state does not match its checksum |
| game file size and modification time unchanged | The game file changed after the state was saved |
| emulator `version` unchanged (states are often version-specific) | The emulator version changed (… → …); the state may not load |
| same adapter | … now uses a different save-state adapter |
| emulator available (executable valid) | the availability reason |

## Limits and removal

| Setting | Default | Effect |
| --- | --- | --- |
| `sessions.maxCount` | 5 (1–20) | with this many other games saved, suspending is refused: "Quick Resume is full … remove one from Home first" |
| `sessions.maxMegabytes` | 512 (64–16384) | with this much state data from other games, suspending is refused; if a new state takes the total over the limit, a warning is shown (nothing is deleted) |
| `sessions.maxAgeDays` | 30 (0 = off) | older saved sessions are marked on Home and offered for removal; never removed automatically |
| `sessions.excludedGames` | `[]` | game ids that never use Quick Resume (Game options) |

States are deleted only (a) by **Remove** on Home after a confirmation, and (b) the third-newest state of a session after a newer one has been verified and recorded (each session keeps its newest two).

## Shutdown and power

- **Power off / Restart from Nexora** (Settings › System, Game Bar): BusyBox init sends SIGTERM to every process at shutdown, the emulator included, so a running game is saved first when possible. The power request is sent once the emulator has quit. If the save fails, the request is **dropped**, the game keeps running and a critical notification says so. A game without Quick Resume is closed first (the confirmation dialogs say unsaved progress may be lost).
- **Application exit** (SIGTERM, Ctrl+Q): the running game is saved when possible (waiting at most twice its `timeoutMs` plus 5 s), then `EmulatorManager::shutdown()` sends SIGTERM to the emulator's process group and SIGKILL only if it is still running after min(stop timeout, 3 s). If the save fails, a warning is recorded and kept in the notification history across the restart.

## Errors

| Situation | Result |
| --- | --- |
| emulator answers `ERR`, writes an empty file, a file outside the session directory (or in a subdirectory of it, or `meta.json`, or one already recorded), or does not answer in time | save failed, game keeps running, notification |
| emulator exits while a save is pending | notification "stopped while it was being saved"; no new state; earlier state kept |
| `meta.json` cannot be written | save failed; the new file stays unreferenced; the previous state stays valid |
| resume refused or the emulator fails to start from the state | "Can't resume" dialog; the state is kept |
| control socket cannot be created | the game runs without Quick Resume; the Quick Menu shows the reason |

## Limitations

- No real emulator tested; the control-socket protocol must be implemented by the emulator or a wrapper; RetroArch support is unverified.
- One running game at a time; no in-memory multi-game suspend (the emulator owns the display on the image; see ADR-001).
- Hashing a state runs on the UI thread (states are usually a few MB; a very large state delays the UI briefly).
- No toast over a running game on the image; notifications are seen when the shell is visible.
- States are not included in backups; no per-game slot management or import/export yet ([console-experience.md](console-experience.md#dependency-aware-plan), step 7).

## Tests

`tst_gamesessionmanager` (fake emulator: suspend writes a verified state and `meta.json`, restart and resume load it, newest two states kept, save failure keeps the game running, limits refuse without deleting, excluded game and emulator without support, resume refused for a changed emulator version / changed game file / bad checksum with the state kept, start fresh, damaged `meta.json` moved aside, removal of one session only, save at shutdown, power request after a save or dropped on failure, power request closes a game without Quick Resume, power request closes an emulator started without a session, the same game on another emulator gets its own session, a save naming an already recorded file is rejected), `tst_savestateadapter`, `tst_emulatormanager::shutdownTerminatesBeforeKilling`, and `tst_qmlsmoke::quickResumeFromQuickMenuAndHome` (Quick Menu › Suspend, Home row, resume, "Can't resume" dialog, remove with confirmation; both resolutions).
