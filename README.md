# Nexora OS

A Linux-based gaming operating system built for controller-first gaming.

Nexora OS turns an x86-64 PC or mini PC into a console-like environment: it boots into a polished, controller-driven interface for finding, organizing and launching your locally stored games with open-source native Linux emulators. It works entirely offline, has no accounts and no telemetry. Nexora OS is free software under the GNU GPL, version 3 or later ([Licensing](#licensing)).

> **Status: MVP 4, version 0.4.0.** The application (Qt 6 / QML UI and C++ services) is implemented and tested on a Linux host — **41/41** automated tests pass with GCC and Clang under `-Werror`. The Buildroot image now has A/B root slots, a persistent data partition, signed updates with automatic rollback and a recovery menu, but **this image has not been built or booted yet**, and **no physical hardware, controller or real emulator has been tested**. Full acceptance report: [docs/mvp-4.md](docs/mvp-4.md). <!-- TASK12-RESULT: update once the A/B image has been built and booted in QEMU -->

## Contents

- [Features](#features)
- [What is not there yet](#what-is-not-there-yet)
- [Quick start](#quick-start)
- [Controls](#controls)
- [Documentation](#documentation)
- [Tech stack](#tech-stack)
- [Design principles](#design-principles)
- [Scope](#scope)
- [Licensing](#licensing)

## Features

Implemented in 0.4.0 (tested on a desktop Linux host unless stated otherwise):

- **Controller-first UI** (`nexora-console-ui`, Qt 6 / QML) with eight tabs — Home, Games, Systems, Favorites, Recent, Emulators, Settings, System — game details with metadata editing through an on-screen keyboard, an in-game **quick menu** (Guide or hold Start), the game bar, notices, loading/empty/error states, and confirmation dialogs whose default is Cancel. → [docs/design-system.md](docs/design-system.md)
- **Accessibility:** high-contrast theme, interface scale 75–150 % on top of automatic scaling for screens larger than 1280×720, reduced motion, visible focus everywhere, plain-text rendering of all titles and metadata, layouts checked at 1280×720 and 1920×1080 (also at 150 %).
- **Game library:** scans approved folders on a background thread with progress and cancel, never enters system directories or follows escaping symlinks, assigns games to 19 gaming systems, keeps your edited titles, notes, system and emulator choices across rescans, handles duplicates and missing files, and "removes" games by hiding them — game files are never deleted. SQLite schema v2 with an automatic backup before migration. → [docs/game-library.md](docs/game-library.md)
- **Emulator integration:** JSON emulator definitions with versions, systems and capabilities; health checks; one supervised process group with startup-failure detection, pause/resume where allowed, graceful stop with bounded kill (helper processes included); launch history with redacted error output. Tested with a mock emulator only. → [docs/emulator-integration.md](docs/emulator-integration.md)
- **Controllers:** evdev (optional SDL3) backends, position-based remappable buttons, configurable dead zone, key-repeat filtering, device list, disconnect notices, an input test screen. → [docs/controller-support.md](docs/controller-support.md)
- **User data:** one documented layout, atomic writes with owner-only permissions, versioned formats, backup and restore from the UI or the command line with full verification before anything is replaced. → [docs/user-data.md](docs/user-data.md)
- **Diagnostics:** hardware information (CPU, memory, GPUs, displays, audio, storage), rotating logs with a configurable level, redacted diagnostics reports (paths, URL credentials, secret-named keys and flags, authorization headers).
- **Signed updates:** Ed25519-signed bundles with a strict manifest, verified by the UI and again by a root helper that writes the inactive root slot (refused while a trial boot is pending); GRUB boots the new slot on trial and rolls back after three boots in which the UI did not stay up; the result is shown in the UI until you dismiss it. Image side implemented, **not verified on a booted image**. → [docs/update-system.md](docs/update-system.md)
- **Recovery:** GRUB entries for the previous slot, a recovery menu (including boot-environment repair) and a maintenance shell; a crash-looping UI falls back to a login. → [docs/recovery.md](docs/recovery.md)
- **Security:** unprivileged UI, two narrow root helpers, no shell commands built from metadata, validated inputs, no network code. → [docs/security.md](docs/security.md)
- **Release engineering:** CI with formatting, lint, unit/integration/security/system tests, QML lint and documentation checks; reproducible release archives, signing and verification scripts. → [docs/release-process.md](docs/release-process.md)

## What is not there yet

- The A/B image has not been built or booted; installation and recovery procedures are designed but unverified.
- No physical controller, GPU, audio device or real emulator has been tested; no emulator ships with the image.
- Search and adding library folders need a keyboard; so do the GRUB and recovery menus.
- No automounter, network, artwork download, translations or save-state integration.
- No Secure Boot or disk encryption; the development image has an empty root password.

Details: [docs/mvp-4.md](docs/mvp-4.md#limitations), [docs/roadmap.md](docs/roadmap.md).

## Quick start

On Ubuntu 24.04 ([full instructions](docs/building.md)):

```sh
sudo apt install cmake ninja-build g++ \
    qt6-base-dev qt6-declarative-dev qt6-declarative-dev-tools \
    qml6-module-qtqml qml6-module-qtqml-workerscript qml6-module-qtqml-models \
    qml6-module-qtquick qml6-module-qtquick-controls qml6-module-qtquick-templates \
    qml6-module-qtquick-layouts qml6-module-qtquick-window qml6-module-qtquick-shapes \
    qml6-module-qttest libqt6sql6-sqlite libgl-dev libssl-dev openssl

cmake --preset ci && cmake --build --preset ci   # -Werror build
ctest --preset ci                                # 41 tests, offscreen
scripts/run.sh                                   # run the UI (debug preset)
```

A development build is populated with fake-emulator definitions, so the Emulators tab works without setup. Useful commands: `nexora-console-ui --version`, `--check-config`, `--backup`, `--restore DIR`, `--export-diagnostics`, `--verify-update DIR` ([command-line reference](docs/building.md#command-line-reference)).

To build the disk image: `scripts/image/build-image.sh` (Buildroot 2026.02.3, several hours; [docs/buildroot.md](docs/buildroot.md)), then `scripts/image/qemu-boot-test.sh`.

## Controls

| Action | Controller (position; Xbox label) | Keyboard |
| --- | --- | --- |
| Move | D-pad, left stick | Arrow keys |
| Select / open details | South (A) | Enter |
| Back (inside a page: to the tab bar) | East (B) | Esc, Backspace |
| Previous / next tab | L1 / R1 | Page Up / Page Down |
| Favorite | North (Y) | F |
| Search | West (X) | `/`, Ctrl+F |
| Launch options | Select (View) | O |
| System tab | Start | Menu, F1 |
| Game bar | Guide (no game running) | Ctrl+G |
| Quick menu over a running game | Guide, or hold Start 1 s | — |
| Stop the running game | hold Select + Start 1 s (or Stop on the running screen, with confirmation) | Enter on Stop, then confirm |

Buttons are named by position; all non-directional actions can be remapped. → [docs/controller-support.md](docs/controller-support.md)

## Documentation

| Document | Content |
| --- | --- |
| [docs/mvp-4.md](docs/mvp-4.md) | MVP 4 status, test results and acceptance report |
| [docs/architecture.md](docs/architecture.md) | components, diagrams, QML-facing API, threads, safety rules |
| [docs/roadmap.md](docs/roadmap.md) | milestones and next steps |
| [docs/building.md](docs/building.md) | prerequisites, build, options, scripts, CLI, conventions |
| [docs/installation.md](docs/installation.md) | desktop use and disk image installation |
| [docs/hardware-compatibility.md](docs/hardware-compatibility.md) | tested vs. expected vs. unsupported hardware |
| [docs/controller-support.md](docs/controller-support.md) | input flow, layouts, mapping, dead zone, quick menu, test checklist |
| [docs/game-library.md](docs/game-library.md) | folders, scanning, database, systems, details, removal |
| [docs/emulator-integration.md](docs/emulator-integration.md) | emulator definitions, health, launch lifecycle, history |
| [docs/user-data.md](docs/user-data.md) | data layout, atomic writes, migrations, backup and restore |
| [docs/update-system.md](docs/update-system.md) | signed bundles, state machines, A/B slots, rollback |
| [docs/recovery.md](docs/recovery.md) | boot and services, recovery menu, procedures |
| [docs/security.md](docs/security.md) | threat model, mitigations, known limitations |
| [docs/testing.md](docs/testing.md) | test suite, CI, manual tests |
| [docs/release-process.md](docs/release-process.md) | versions, keys, release scripts and checklist |
| [docs/troubleshooting.md](docs/troubleshooting.md) | symptoms and fixes |
| [docs/design-system.md](docs/design-system.md) | design tokens, accessibility modes, components |
| [docs/buildroot.md](docs/buildroot.md) | the Buildroot external tree and image |
| [docs/settings.md](docs/settings.md) | every setting, validation and recovery |
| [docs/game-bar.md](docs/game-bar.md) | the game bar overlay |
| [docs/testing-windows11.md](docs/testing-windows11.md) | building and testing on Windows 11 with WSL 2 (Hungarian) |
| [docs/emulator-integracio.md](docs/emulator-integracio.md) | future plan for RetroArch and PPSSPP (Hungarian, not implemented) |
| [prototypes/console-ui/](prototypes/console-ui/README.md) | separately developed UI prototype, not part of the build |

## Tech stack

| Concern | Used |
| --- | --- |
| Language | C++20 |
| UI | Qt 6 (≥ 6.4: Core, Gui, Qml, Quick, QuickControls2, Sql), QML module `Nexora.Console` |
| Build | CMake ≥ 3.22, Ninja, `CMakePresets.json` (`debug`, `release`, `ci`) |
| Persistence | JSON (settings, emulator definitions, history, manifests), SQLite via Qt Sql (library) |
| Cryptography | OpenSSL 3 libcrypto (Ed25519, SHA-256) |
| Processes | `QProcess` with argument lists, one process group per emulator, `PR_SET_PDEATHSIG` |
| Input | Linux evdev; SDL3 (optional, desktop) |
| Testing | Qt Test, Qt Quick Test, CTest, shell tests, fake emulator |
| CI | GitHub Actions on ubuntu-24.04 (`ci.yml`, `buildroot.yml`, `release.yml`) |
| Image | Buildroot 2026.02.3, Linux 6.12, BusyBox init, GRUB 2 (EFI), Qt 6.9 EGLFS/KMS, Mesa |

## Design principles

Controller-first, local-first, open source, reliable (failures are recoverable and never silently corrupt user data), modular, accessible, hardware-aware, reproducible, safe by default, and honest about what has and has not been tested.

## Scope

Nexora OS integrates existing emulator projects rather than reimplementing them. Game files, BIOS files, firmware and other copyrighted material are not bundled or downloaded; users must supply files they are legally entitled to use.

## Licensing

Copyright © 2026 the Nexora OS contributors.

Nexora OS is free software: you can redistribute it and/or modify it under the terms of the **GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version** (SPDX: `GPL-3.0-or-later`). It is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See [LICENSE](LICENSE) for the full text.

Dependencies keep their own licenses, all compatible with GPL-3.0-or-later: Qt 6 (LGPLv3, used through dynamic linking, so Qt can be replaced), OpenSSL 3 (Apache-2.0), SQLite (public domain), SDL3 (zlib). A disk image built with Buildroot also contains the Linux kernel, BusyBox, GRUB, Mesa, firmware and other packages under their own licenses; whoever distributes an image must also offer their sources and license texts (`make legal-info` in the Buildroot output collects them). Emulators that may be packaged later (RetroArch, PPSSPP) are GPL as well. Games, BIOS files and firmware are never part of Nexora OS.
