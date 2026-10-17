# Console experience: feature-gap report and plan

Status of the "MVP 4 Enhanced Console Experience" brief against the code on `main` + the glass UI branch (2026-10-17). Every row was checked in code, not inferred from documentation or from a QML control existing. "Partial" means some of the behaviour works end to end; "missing" means there is no backend for it. Nothing below is implemented yet unless the status column says so; this document is the audit (brief §1) and the dependency-aware plan (brief §23, phase 1).

## Contents

- [Summary](#summary)
- [Constraints that shape every feature](#constraints-that-shape-every-feature)
- [P0 gap report](#p0-gap-report)
- [P1 gap report](#p1-gap-report)
- [P2](#p2)
- [Dependency-aware plan](#dependency-aware-plan)
- [Open questions](#open-questions)

## Summary

Nexora already has a solid single-session console core: one emulator at a time with a validated launch path, SIGSTOP/SIGCONT pause for emulators declared `pausable`, a Quick Menu and Game Bar reachable from the controller while a game runs, a SQLite library with favorites, recents, systems, search, overrides and soft removal, verified backup/restore of settings, library, history and saves, signed A/B updates with rollback, a supervised session with a recovery menu, and controller remapping, dead zone, high contrast, UI scale and reduced motion.

The brief's biggest gaps are the things a console adds on top of that core: a session model that can hold more than one game (Quick Resume), any emulator save-state integration, user-defined collections and a customisable Home, per-game profiles beyond the emulator choice, a save-data manager, a notification service, screenshots, and graceful handling of a running game at shutdown. Steps 1–3 below (notifications, emulator capabilities, Quick Resume with graceful shutdown) are implemented; see the status column.

## Constraints that shape every feature

| Constraint | Consequence |
| --- | --- |
| No emulator is packaged or tested yet (`config/emulators/example.json` is `/bin/true`) | Every emulator-specific capability (save states, screenshots, FPS) can only be built against an adapter interface and proven with the fake emulator. A real adapter (RetroArch network commands) can be written but is unverified until RetroArch is integrated ([emulator-integracio.md](emulator-integracio.md)). |
| On the image the emulator owns the display (EGLFS/KMS, DRM master) | The shell cannot draw over a running game (no in-game overlay, no toast during play) and cannot grab the game's frame. Overlays appear when the shell regains the display (Guide). Screenshots of games must come from the emulator. |
| No network stack, no Bluetooth, no automounter on the image | Achievements, streaming, cloud sync and Bluetooth pads are out of reach without new image work; USB backup destinations need an automounter first. |
| Qt 6.4 on desktop/CI (6.9 on the image), software renderer in tests | Only APIs present in 6.4; no shader effects in tests. |
| One user (`nexora`) on the image, no per-user isolation | Local profiles can separate preferences, not secure data. |
| Untested on real hardware; the A/B image has not been built | System-level claims (suspend, power, capture) stay "unverified" until run on the image. |

## P0 gap report

| # | Feature (brief §) | Status | What exists (owner) | What is missing |
| --- | --- | --- | --- | --- |
| 1 | Quick Menu (§5) | partial | `QuickMenu.qml`, `GameBar.qml`, `GameBarController`: return/resume, pause/resume (pausable only), close with confirmation, volume/mute, display page, system info; opened by Guide or holding Start 1 s; tested (`tst_quickmenu.qml`, smoke) | switch game, controller battery/settings, accessibility page, captures, performance, save/load state, per-game config, sleep; customisation (entries, order, shortcut, dim); entries are hard-coded |
| 2 | Quick Resume / sessions (§4) | **implemented (step 3)**, fake emulator only | `GameSessionManager` with save-state adapters: suspend/resume through verified persistent states, session records (`meta.json`), limits that refuse instead of deleting, per-game exclusion, Home row, Quick Menu entry ([quick-resume.md](quick-resume.md)) | real emulator support (no emulator packaged; RetroArch adapter unverified), in-memory suspend of a backgrounded game while another runs (ADR-001: one process) |
| 3 | Collections and dashboard (§6, §7) | partial | fixed Home sections (continue playing, favorites, recently added, systems, status cards); Favorites/Recent/Systems tabs; search, system filter, sorts, missing filter, soft removal | user collections (none in the DB), pinning, add/remove/reorder/resize widgets, persisted layout, suspended-session/save-state/capture/storage widgets, play time, content duplicates; game id is tied to the file path |
| 4 | Per-game profiles (§8) | partial | per-game emulator and system override, title/notes; launch profiles per emulator, default profile per emulator (setting) | per-game launch profile, aspect/scaling/shader/audio/mapping/dead zone/hotkeys/Quick Resume policy, precedence chain, import/export, validation against an emulator capability schema |
| 5 | Save states and backups (§9) | partial | `BackupService`: verified backup/restore (SHA-256 manifest, rollback) of settings, library, history, `saves/`; local destination only | save-state files, slots, states directory, per-game save tracking, a save-path placeholder for emulators, retention, removable destinations, a backups page with verification status |
| 6 | Controller center (§10) | partial | device list (name, id), input test, remapping of 9 actions to 9 buttons, dead zone, backend choice, hotplug and layout correction (`tst_evdevhotplug`) | battery/charging/connection type, axis inversion, vibration, per-controller profiles and ordering, screenshot/save-state actions, a mapping-only reset |
| 7 | Library history, favorites, search (§7) | exists, small gaps | favorites, recents, play count, last played, search, filters | play time, game id in launch history |
| 8 | Storage manager (§17) | partial | free/total for data dir and `/`, library directories setting, artwork cache clear | per-type breakdown, external discovery (no automounter), missing-drive warnings, folder browser |
| 9 | Notifications (§15) | **implemented (step 1)** | `NotificationService`: categories, severities, queue, dedup, history (warnings/criticals persisted), quiet mode, hidden categories, controller/game/library/Quick Resume events ([notifications.md](notifications.md)) | UI row for hidden categories, history filtering |
| 10 | Startup, shutdown, suspend, recovery (§14) | partial | supervised session with backoff/give-up, recovery menu, boot counting, power requests via `nexora-powerd` | suspend-to-RAM, display sleep, idle timeout, persistent logs (graceful stop and Quick Resume save at shutdown/power: done in step 3) |

## P1 gap report

| Feature | Status | Notes |
| --- | --- | --- |
| Screenshots and gallery (§12) | missing | desktop: `grabWindow` captures the shell only; image: needs the emulator's own screenshot (adapter) or a root KMS reader |
| Recording (§12) | missing | report as unsupported until a capture path exists |
| Performance monitor/overlay (§13) | partial | one-shot hardware probe; live CPU % in the Game Bar; no GPU, temperatures, fans or FPS; no overlay over EGLFS games |
| Themes, wallpapers, accent (§19) | partial | glass/dark/high contrast; no accent colour, wallpaper or dim setting |
| Local user profiles (§16) | missing | single-user settings, library and history |
| Achievements (§18) | missing | needs network; isolated provider interface only |
| Advanced controller profiles (§10) | missing | see P0 #6 |

## P2

Remote streaming, network save sync, online metadata/artwork and automation: missing; all need a network stack on the image first. Not planned for MVP 4.

## Dependency-aware plan

Each step is a separately reviewable PR with its own tests and docs; later steps build on earlier ones. Effort is a rough size, not a commitment.

| Step | Scope | Depends on | Size |
| --- | --- | --- | --- |
| 1 ✅ | **Notification service** (C++ `NotificationService`, categories, dedup, queue, history, quiet mode; Toast becomes its view; controller connect/disconnect, launch, failure events) | — | M |
| 2 ✅ | **Emulator capability model** (definition fields for capabilities, adapter interface, fake-emulator adapter with save/load state through a test protocol; RetroArch network-command adapter, marked unverified) | — | M |
| 3 ✅ | **Game session manager**: session ids and the brief's state machine on top of `EmulatorManager`; graceful stop at shutdown (save state when supported, then SIGTERM, never silent SIGKILL); in-memory suspend of the current game; persistent state through adapters; limits, per-game Quick Resume policy; session metadata file (atomic, checksummed) | 1, 2 | L |
| 4 | **Library schema v3**: collections (many-to-many, stable ids), pinned games, play time, game id in launch history; migration with backup | — | M |
| 5 | **Dashboard layout service** + Home widgets (suspended sessions, collections, storage), add/remove/reorder, persisted layout with validation and reset | 3, 4 | M |
| 6 | **Per-game profiles** with precedence (game → emulator → global → defaults), validated against capabilities, import/export | 2, 4 | M |
| 7 | **Save data manager**: states directory, slots, metadata, preserve-before-overwrite, retention, backups page | 2, 3 | M |
| 8 | **Quick Menu completion and customisation** (switch game, save/load state, game settings, accessibility, sleep where supported; reorder/hide with protected power/recovery entries) | 3, 6, 7 | M |
| 9 | **Controller center and accessibility** (battery via sysfs `power_supply`, mapping-only reset, axis inversion, hold/repeat timings, text size, accent) | 1 | M |
| 10 | **Storage manager** (per-type breakdown, safe cleanup, missing-drive warnings); automounter on the image | 1 | M |
| 11 | **Power**: idle timeout, display sleep and suspend where the image supports them; persistent bounded logs | 3 | M |
| 12 | P1: screenshots (emulator adapter + shell capture), gallery; live monitor with temperatures (hwmon) | 2, 1 | M |

Documentation listed in brief §24 is written with the step that implements each feature, not ahead of it.

## Open questions

- Whether a real emulator (RetroArch) should be integrated next, so that the retroarch save-state adapter and Quick Resume are verified against real software.
