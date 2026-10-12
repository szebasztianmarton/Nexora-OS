# Nexora OS — MVP 4 status

MVP 4 turns the MVP 2 console UI and the MVP 3 Buildroot tree into a reliable, controller-first, local-only gaming OS. The brief is in [mvp-4-brief.md](mvp-4-brief.md); the task-level plan is in [superpowers/plans/2026-10-11-mvp4.md](superpowers/plans/2026-10-11-mvp4.md).

> This document is updated as tasks land. The acceptance checklist is ticked only with evidence.

## Audit (before MVP 4, verified 2026-10-11)

Baseline: `feature/mvp4` branched from `main` at `c0aa6c9`. Built in WSL2 Ubuntu 24.04 (g++ 13, Qt 6.4.2, CMake, Ninja) with `-DNEXORA_WARNINGS_AS_ERRORS=ON`: build succeeded, **19/19 tests passed** (14 unit, 5 integration).

| Area | State |
| --- | --- |
| MVP 1 | Done: Qt 6/QML app, `EmulatorManager` (QProcess, SIGTERM then bounded SIGKILL), logging with rotation, fake emulator, CMake/Ninja, CTest. |
| MVP 2 | Done: dashboard (Home, Games, Emulators, Settings, Diagnostics), SQLite library (schema v1), layered validated settings, evdev and optional SDL3 gamepad backends, remapping, desktop/embedded `SystemIntegration`. |
| MVP 3 | Partial: Buildroot external tree (defconfig, kernel fragment, GRUB EFI, single-root GPT image, BusyBox init scripts, session supervisor with crash backoff and a tty2 recovery login, power-request FIFO helper, `nexora.noui` boot entry). The configuration was validated, but **no image had been built or booted**. |
| Prototype UI | `prototypes/console-ui` is a separate, richer UI (JSON stores, SDL2) that the main build does not compile. Kept as reference only. |

Missing for MVP 4 at the start: gaming systems, user metadata overrides, remove-from-library, migration with backup, scan progress/cancel in the UI, emulator health/version/history, hardware information, build identifier, documented data layout, backup/restore, redacted diagnostic export, verbosity setting, dead-zone setting, device list, input test screen, quick menu, game details screen, systems/recent screens, high-contrast/scale/reduced-motion, an update system, A/B slots with a persistent data partition, a release workflow, and 13 of the 18 required documents.

Risks recorded at the start: the UI is a single `Main.qml` with tabs (no stack navigation); the image keeps user data on the root filesystem, so an image update would wipe it; the kernel sits on the shared EFI partition, so slots cannot carry their own kernel; the host has Qt 6.4 and the image Qt 6.9.

## Acceptance

Not yet evaluated — filled in at the end of the milestone.

## Limitations

Recorded at the end of the milestone.
