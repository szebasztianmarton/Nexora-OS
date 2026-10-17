# Roadmap

Where Nexora OS stands after MVP 4 and what comes next. Everything under "Next" is a plan, not a commitment, and nothing listed there exists yet.

## Milestones so far

| Milestone | Goal | State |
| --- | --- | --- |
| MVP 1 | Qt 6/QML desktop prototype: controller input, emulator process management, logging, fake emulator, CMake/Ninja, CTest | Done (desktop) |
| MVP 2 | Gaming dashboard: library (SQLite), settings, remapping, evdev/SDL3 input, desktop/embedded system integration, game bar | Done (desktop) |
| MVP 3 | Bootable Buildroot image that starts the UI | Partial: the external tree exists and was validated; a single-root image was reported built and booted in QEMU once (commit `73a4df0`), never on hardware |
| MVP 4 (0.4.0) | Reliable platform: versioned user data with backup/restore, library with systems and user metadata, emulator health and history, hardware info, signed updates with A/B slots, diagnostics, completed controller-first UI, release engineering, documentation | Implemented and tested on a Linux host (41/41 tests, GCC and Clang); **image side unverified** (A/B image not built or booted); details in [mvp-4.md](mvp-4.md) |

## Next: close the MVP 4 verification gaps

These items turn "implemented" into "verified" and are the precondition for a first public release:

1. Build the A/B image (`scripts/image/build-image.sh`) and pass `scripts/image/qemu-boot-test.sh`; record the result in [testing.md](testing.md) and [buildroot.md](buildroot.md).
2. In QEMU: first boot (data partition creation), recovery menu, power off/restart from the UI, an update installed from a test-signed bundle, a forced rollback (break the new slot), and confirmation by `nexora-bootok`.
3. On at least one Intel and one AMD mini PC: boot from USB, GPU driver, audio, an Xbox, a PlayStation and a Nintendo controller (checklist in [controller-support.md](controller-support.md#manual-test-checklist)); update [hardware-compatibility.md](hardware-compatibility.md).
4. One real emulator end to end (definition, launch, quick menu, exit, pause if it tolerates SIGSTOP).
5. First signed release through [release-process.md](release-process.md), with `docs/release-notes/0.4.x.md`.

## Next: product gaps found during MVP 4

| Area | Gap | Notes |
| --- | --- | --- |
| Controller-first | search and library-folder entry need a keyboard; GRUB and the recovery menu need a keyboard | extend the on-screen keyboard; add a folder browser; a controller-driven recovery screen |
| Getting content onto the image | no automounter, no file manager, no network | automount removable media read-only under `/media`; an import screen |
| Console experience (MVP 4 brief) | steps 1–3 done: notifications, emulator capabilities and save-state adapters, Quick Resume with graceful shutdown ([console-experience.md](console-experience.md), [quick-resume.md](quick-resume.md)); collections, dashboard layout, per-game profiles, save-data manager and the rest are planned there | Quick Resume is verified only with the fake emulator |
| Emulators | none packaged | the separate (Hungarian) plan for RetroArch with libretro cores and PPSSPP: [emulator-integracio.md](emulator-integracio.md); per-emulator save directories under `<data>/saves` |
| Emulator definitions on the image | live on the root slot, so hand-added ones are lost after an update | read additional definitions from the data partition (root-owned or validated) |
| Data partition | fixed 1 GiB | grow to fill the disk on first boot |
| Logs on the image | tmpfs | keep a bounded log on the data partition |
| Security | no Secure Boot, no disk encryption, development root password, emulators share the UI user | signed boot chain; set a root password in release images; emulator sandboxing |
| Updates | local bundles only; no delta updates; ESP/GRUB never updated | network delivery (would need a network stack and a policy), ESP update path |
| Audio/display | output selection and display/mode selection are read-only | real device selection where the stack supports it |
| Localization | English only | translations |
| Artwork | generated placeholders only | local artwork import (no scraping service by default) |
| Licensing | decided: GPL-3.0-or-later ([LICENSE](../LICENSE)) | add SPDX headers to the source files; ship license texts of bundled emulators with them |

## Out of scope

- Bundling or downloading commercial games, BIOS or firmware.
- Mandatory online accounts or telemetry.
- NVIDIA proprietary drivers and non-x86-64 targets for now.
