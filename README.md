# Nexora OS

A Linux-based gaming operating system built for controller-first gaming.

Nexora OS is intended to be a gaming operating system, built from open-source components, that provides a console-like experience on x86-64 PCs and mini PCs. Its goal is to bring compatible native Linux emulators together in a unified, controller-driven interface. (The license of Nexora OS itself has not been decided yet; see [Licensing](#licensing).)

> **Status:** MVP 2 (version 0.2.0) — a controller-driven gaming dashboard with game library, settings and system integration, running as a desktop prototype. A Buildroot image definition exists, but no image has been built or booted yet. See [Project Status](#project-status).

## Contents

- [Vision](#vision)
- [Project Status](#project-status)
- [Features (MVP 2)](#features-mvp-2)
- [Known limitations](#known-limitations)
- [Tech stack](#tech-stack)
- [Quick start](#quick-start)
- [Running](#running)
- [Controls and controllers](#controls-and-controllers)
- [Game library](#game-library)
- [Emulator configuration](#emulator-configuration)
- [Settings](#settings)
- [Testing](#testing)
- [Troubleshooting](#troubleshooting)
- [MVP 3 outlook](#mvp-3-outlook)
- [MVP goals and roadmap](#mvp--minimum-viable-product)
- [Design principles](#design-principles)
- [Scope and limitations](#scope-and-limitations)
- [Licensing](#licensing)

Documentation:

| Document | Content |
| --- | --- |
| [docs/architecture.md](docs/architecture.md) | Components, data flows, process lifecycle, system integration, start-up/shutdown |
| [docs/development.md](docs/development.md) | Prerequisites, presets, scripts, SDL3, code layout |
| [docs/settings.md](docs/settings.md) | Settings files, all keys, validation and recovery, backups |
| [docs/game-library.md](docs/game-library.md) | Library folders, scanning, database, search, launching games |
| [docs/controller-support.md](docs/controller-support.md) | Backends, button layouts, mapping, remapping, hardware test checklist |
| [docs/testing.md](docs/testing.md) | Test suite, fake emulator, CI, manual tests |
| [docs/buildroot.md](docs/buildroot.md) | Buildroot external tree, image, start-up, power helper, QEMU |
| [docs/testing-windows11.md](docs/testing-windows11.md) | Building and testing on Windows 11 (WSL 2, WSLg, usbipd, QEMU) — in Hungarian |
| [prototypes/console-ui/](prototypes/console-ui/README.md) | Separately developed console UI prototype (from the `claude/nexora-console-ui` branch); standalone CMake project |

## Vision

Create a dedicated gaming environment that boots directly into a simple, polished interface, allowing users to browse, launch, and manage their emulators without needing a keyboard or mouse for everyday use.

## Project Status

Current stage: **MVP 2** on `feature/mvp2` (roadmap phases 1–3 largely done on a desktop host, phase 4 started).

| Area | MVP 1 (`feature/mvp1`) | MVP 2 (`feature/mvp2`) |
| --- | --- | --- |
| Local build (Ubuntu 24.04, Qt 6.4.2, GCC 13) | `ci` preset clean with `-Werror` | `ci` preset (GCC, Debug) clean with `-Werror` |
| Local tests | 10/10 passing | **19/19** `ctest` passing (GCC Debug, `-Werror`); a separate non-root run of all tests except the QML smoke test also passed |
| GitHub Actions CI | Passing ([run \#2](https://github.com/szebasztianmarton/Nexora-OS/actions/runs/37954884460)): GCC/Clang Debug `-Werror`, GCC Release, GCC + SDL3 | Passing on `feature/mvp2` ([run \#13](https://github.com/szebasztianmarton/Nexora-OS/actions/runs/38071163228), commit 1716027, 2026-10-10): lint (clang-format, JSON, shellcheck), GCC/Clang Debug `-Werror`, GCC Release (unit, integration, `--check-config`), GCC + SDL3; Buildroot tree validation workflow passing |
| evdev gamepad backend | Mapping/navigation unit-tested; no real controller tested | Same, plus configurable mapping and remap capture; no real controller tested |
| SDL3 gamepad backend (optional) | Compiles/links against SDL 3.2.30 | Unchanged; not available in the Buildroot image |
| Real emulators | None tested | None tested; no compatibility with any real emulator is claimed |
| Buildroot image | Not started | External tree validated with `check-package`, defconfig-symbol and kernel-fragment checks; full image build in progress and **unverified** — no image built or booted |

Nexora OS is experimental software. Hardware compatibility, supported emulators, and release readiness will be documented as development progresses.

## Features (MVP 2)

Implemented (runs as an ordinary Qt application on desktop Linux; the same binary is meant for the image):

- **Gaming dashboard** (`nexora-console-ui`, Qt 6 / QML) with five tabs:
  - **Home:** quick actions, "Continue playing" and "Favorites" rows, getting-started help while the library is empty.
  - **Games:** search, filters (All / Favorites / Recent), sort order, game grid, launch and favorite.
  - **Emulators:** all configured emulators, launch directly or pick a launch profile.
  - **Settings:** language, hints, default sort, display mode, window size, volume, controller backend, button remapping, library folders, rescan, reset, restart/power off (image only, with confirmation).
  - **Diagnostics:** version, configuration and its issues, emulator state and last output, input backend and devices, system status, library and settings issues, recent log.
  - A running-emulator overlay, a context-sensitive hint bar and transient notices.
- **Game library:** scans configured folders on a worker thread for the file extensions your emulators declare, stores games, favorites and play history in SQLite, keeps them across rescans and temporarily missing drives. Never executes or opens game files. → [docs/game-library.md](docs/game-library.md)
- **Emulator manager:** one supervised process at a time via `QProcess` (never a shell), launch profiles, game path substitution with `{file}`, per-emulator working directory and stop timeout, crash/exit/failure handling, SIGTERM then SIGKILL on stop.
- **Settings:** layered (built-in ← system defaults ← user file), validated, saved atomically on every change, corrupted files moved aside. → [docs/settings.md](docs/settings.md)
- **Controller input:** evdev (or optional SDL3) backend, buttons named by position, remappable actions, auto-repeat, multiple pads, hotplug; navigation suspended while an emulator runs; hold **Select + Start** for 1 s to return to Nexora. → [docs/controller-support.md](docs/controller-support.md)
- **System integration:** desktop vs. embedded behaviour; on the image, power off/restart through a minimal root helper and volume through `amixer`; single-instance lock; clean shutdown on SIGTERM/SIGINT/SIGHUP.
- **Logging and diagnostics:** categorized logging to stderr, an optional rotating log file (2 MiB × 3) and an in-memory buffer.
- **Safety defaults:** refuses to run as root unless explicitly allowed; emulators receive SIGTERM if the launcher dies.
- **Buildroot external tree** for an x86-64 UEFI image that boots into the UI (unverified; see [docs/buildroot.md](docs/buildroot.md)).
- **Testing:** 19 Qt Test / CTest executables, including a QML test that drives the dashboard with key events and a fake emulator. → [docs/testing.md](docs/testing.md)

## Known limitations

- No physical controller and no real emulator has been tested; the SDL3 backend has only been compiled.
- The Buildroot image is unverified: it has not been built completely or booted. SDL3 is not available in the image (evdev only).
- No on-screen keyboard: search and adding a library folder need a keyboard.
- The per-game emulator override and purging of missing games exist in the backend but have no UI.
- Volume and power control work only on the embedded platform; on the desktop the volume is a stored preference and power actions are unavailable.
- `display.resolution` only sets the initial window size; it does not change the display mode. The controller backend changes apply after a restart.
- Fixed font sizes (no UI scaling setting); only English.
- One emulator process at a time; one button mapping for all pads; fixed chord and repeat timings.
- No metadata scraping, artwork, online services or save-state integration; no ROMs/BIOS provided.
- Library database merges run on the UI thread (≈0.1 s for 10 000 files in a synthetic test).
- Licensing is undecided (see [Licensing](#licensing)).
- `Ctrl+Q` quits the prototype (development convenience).

## Tech stack

| Concern | Used |
| --- | --- |
| Language | C++20 |
| UI | Qt 6 (minimum 6.4: Core, Gui, Qml, Quick, QuickControls2, Sql) and QML, static QML module `Nexora.Console` |
| Build | CMake ≥ 3.22, Ninja, `CMakePresets.json` (`debug`, `release`, `ci`) |
| Process management | `QProcess` (no shell), `prctl(PR_SET_PDEATHSIG)` |
| Input | Linux evdev; SDL3 gamepad API (optional) |
| Configuration | JSON files (`QJsonDocument`) for emulators and settings |
| Persistence | SQLite via Qt Sql (`QSQLITE`) for the game library |
| Logging | Qt logging categories (`nexora.*`), rotating log file |
| Testing | Qt Test and CTest, plus a fake emulator executable |
| CI | GitHub Actions on ubuntu-24.04 (`ci.yml`, `buildroot.yml`) |
| Image | Buildroot 2026.02.3, Linux 6.12 LTS, Qt 6.9 EGLFS/KMS, Mesa |

## Quick start

Tested on Ubuntu 24.04 with the distribution's Qt 6.4.2.

### Install dependencies

```sh
sudo apt install cmake ninja-build g++ \
    qt6-base-dev qt6-declarative-dev libqt6sql6-sqlite \
    qml6-module-qtqml qml6-module-qtqml-workerscript qml6-module-qtqml-models \
    qml6-module-qtquick qml6-module-qtquick-controls qml6-module-qtquick-templates \
    qml6-module-qtquick-layouts qml6-module-qtquick-window \
    libgl-dev
```

`libqt6sql6-sqlite` provides the SQLite driver for the game library; without it the library is disabled at run time. `clang` can be used instead of `g++`. SDL3 is optional and not packaged on Ubuntu 24.04; see [docs/development.md](docs/development.md#optional-sdl3-backend).

### Build

```sh
scripts/build.sh            # debug preset
scripts/build.sh release    # release preset
scripts/build.sh ci --test  # warnings as errors, then run the tests
```

or directly with CMake presets:

```sh
cmake --preset debug
cmake --build --preset debug
```

Build output goes to `build/<preset>/`; the application is `build/<preset>/bin/nexora-console-ui`.

## Running

```sh
scripts/run.sh                          # builds if needed, runs build/debug/bin/nexora-console-ui
scripts/run.sh release -- --fullscreen  # arguments after -- go to the application
scripts/run.sh -- --windowed --settings-file /tmp/nexora-settings.json --library-db /tmp/nexora-library.sqlite
```

A development build generates configurations for the fake emulator, so the Emulators tab is populated without any setup. Settings and the library database default to your user directories (`~/.config/nexora/settings.json`, `~/.local/share/nexora/library.sqlite`); use `--settings-file` / `--library-db` to keep experiments separate.

Command-line options:

| Option | Meaning |
| --- | --- |
| `--config-dir <dir>` | Directory with emulator `*.json` definitions (overrides `NEXORA_CONFIG_DIR`) |
| `--settings-file <file>` | User settings file (default `$XDG_CONFIG_HOME/nexora/settings.json`) |
| `--library-db <file>` | Game library database (default `$XDG_DATA_HOME/nexora/library.sqlite`) |
| `--log-file <file>` | Also append log output to `<file>` (rotated at 2 MiB, 3 old files kept) |
| `--input-backend <backend>` | `auto`, `evdev`, `sdl3` or `none` (default: the `input.backend` setting) |
| `--fullscreen` / `--windowed` | Start full screen / in a window (overrides `display.mode` for this run) |
| `--check-config` | Validate emulator definitions, settings and the controller mapping, print a report and exit (0 = OK); read-only |
| `--allow-root` | Allow running as root (not recommended; also `NEXORA_ALLOW_ROOT=1`) |
| `-h`, `--help` / `-v`, `--version` | Standard Qt options |

Environment variables:

| Variable | Meaning |
| --- | --- |
| `NEXORA_CONFIG_DIR` | Emulator definition directory (if `--config-dir` is not given) |
| `NEXORA_PLATFORM` | `desktop` or `embedded`; overrides detection via `/etc/nexora-release`. `embedded` enables power control and `amixer` volume when the helper/mixer are present. |
| `NEXORA_ALLOW_ROOT=1` | Same as `--allow-root` |
| `QT_QPA_PLATFORM=offscreen` | Run without a display |
| `QT_LOGGING_RULES` | Logging filter, e.g. `nexora.*.debug=true` |

Exit codes: 0 normal, 1 QML load failure or `--check-config` errors, 2 refused to run as root, 3 another instance is running.

Validate a configuration without starting the UI:

```sh
build/debug/bin/nexora-console-ui --check-config --config-dir config/examples/emulators \
    --settings-file config/examples/settings.json
```

## Controls and controllers

The dashboard works entirely with a controller; a keyboard works too.

| Action | Controller (position; Xbox label) | Keyboard |
| --- | --- | --- |
| Move | D-pad, left stick, hat | Arrow keys |
| Select / launch | South (A) | Enter |
| Back / leave field / cancel | East (B) | Esc, Backspace |
| Previous / next tab | L1 / R1 (LB / RB) | Page Up / Page Down, Ctrl+Shift+Tab / Ctrl+Tab |
| Toggle favorite | North (Y) | F |
| Search | West (X) | `/`, Ctrl+F |
| Diagnostics | Start (Guide/Select if unbound) | Menu key, F1 |
| Return to Nexora from a running emulator | Hold Select + Start for 1 s | Enter on the overlay's Stop button |

Buttons are named by position, so the "A" role is Cross on PlayStation and B on Nintendo-layout pads. All non-directional actions can be remapped on the Settings tab. While an emulator runs, controller navigation of the launcher is paused (the emulator receives the pad). Details, supported layouts, permissions (`input` group) and a hardware test checklist: [docs/controller-support.md](docs/controller-support.md).

## Game library

1. Give your emulators file extensions (see [Emulator configuration](#emulator-configuration)).
2. Add folders on the Settings tab ("Library folders"), or set `library.directories` in the settings file.
3. The library scans automatically when folders or emulator extensions change; use "Rescan library" after copying new games.

Only configured folders are scanned (never `/`), hidden entries and symlinks are skipped by default, and files that disappear keep their favorite and history until they return. Details: [docs/game-library.md](docs/game-library.md).

## Emulator configuration

Each emulator is one JSON object in its own `*.json` file in the configuration directory. Example with extensions, `{file}` and launch profiles ([`config/examples/emulators/emulator-with-profiles.json`](config/examples/emulators/emulator-with-profiles.json)):

```json
{
  "id": "example-profiles",
  "name": "Example with launch profiles",
  "executable": "/bin/true",
  "arguments": ["{file}"],
  "workingDirectory": "/tmp",
  "extensions": [".example"],
  "stopTimeoutMs": 3000,
  "enabled": true,
  "profiles": [
    { "id": "fullscreen", "name": "Fullscreen", "emulator": "example-profiles",
      "arguments": ["--fullscreen", "{file}"] },
    { "id": "debug", "name": "Debug (disabled)", "arguments": ["--verbose", "--rom={file}"],
      "enabled": false }
  ]
}
```

| Key | Type | Required | Rules |
| --- | --- | --- | --- |
| `id` | string | yes | `^[a-z0-9][a-z0-9_-]{0,63}$`; unique across all files |
| `name` | string | yes | non-empty (trimmed) |
| `executable` | string | yes | absolute path, or relative to the JSON file's directory; never looked up in `PATH` |
| `arguments` | array of strings | no | default `[]`; passed verbatim, no shell expansion |
| `enabled` | boolean | no | default `true`; disabled entries are listed on the Diagnostics tab only |
| `workingDirectory` | string | no | relative to the JSON file's directory; default: the executable's directory |
| `extensions` | array of strings | no | `".ext"` (1–16 lower-case letters/digits); game files this emulator handles |
| `stopTimeoutMs` | integer | no | 500–60000; time between SIGTERM and SIGKILL (default 5000) |
| `profiles` | array of objects | no | max 32; each `{id, name, emulator?, arguments?, workingDirectory?, enabled?}`; `emulator` must equal the emulator id; profile `arguments` replace the emulator's |

`{file}` rules: when a game is launched, every argument containing `{file}` gets the absolute game path substituted (also inside an argument such as `--rom={file}`); if no argument contains it, the path is appended. Without a game, arguments containing `{file}` are dropped. The default profile per emulator is set with the `emulators.defaultProfiles` setting.

Rules for the directory:

- Only `*.json` files directly in the directory are read, in file-name order; files over 256 KiB are rejected.
- An invalid file is skipped and reported; valid files are still loaded. A duplicate `id` is an error (the later file is ignored). Unknown keys produce a warning.
- Executables are checked at load time; failing entries are shown as unavailable with the reason and checked again on launch.

Configuration directory lookup (first match wins): `--config-dir`, `NEXORA_CONFIG_DIR`, `<appDir>/../share/nexora/emulators`. In a build tree the default is `build/<preset>/share/nexora/emulators/`, filled with `example.json` and four fake-emulator configurations (`fake-ok`, `fake-fail`, `fake-crash`, `fake-long`) for development.

## Settings

Preferences are stored in `~/.config/nexora/settings.json` (`{"version": 1, "settings": {...}}`), layered over optional system defaults, validated on load and on every change, and saved immediately. Example with every key: [`config/examples/settings.json`](config/examples/settings.json). Full key reference, recovery behaviour and backup advice: [docs/settings.md](docs/settings.md).

## Testing

```sh
scripts/build.sh ci --test
# or
ctest --preset debug                    # all tests
ctest --preset debug -L unit            # unit tests only
ctest --preset debug -L integration
ctest --preset debug -R tst_qmlsmoke    # one test
```

Tests run on the offscreen Qt platform and need no display, controller or real emulator. See [docs/testing.md](docs/testing.md) for coverage, the fake emulator, CI jobs and manual tests.

## Troubleshooting

| Symptom | Fix |
| --- | --- |
| CMake: `Qt 6.4+ with components [...] was not found` | Install the packages from [dependencies](#install-dependencies) or pass `-DCMAKE_PREFIX_PATH=/path/to/Qt/6.x/gcc_64`. |
| CMake: OpenGL not found while configuring Qt6Gui | `sudo apt install libgl-dev` |
| Runtime: `module "QtQuick.Controls" is not installed` (or similar) | Install the matching `qml6-module-*` package. |
| Diagnostics tab shows "Library database unavailable" / "Qt SQLite driver (QSQLITE) is not available" in the log | `sudo apt install libqt6sql6-sqlite` |
| `could not connect to display` | Run inside a desktop session, or headless with `QT_QPA_PLATFORM=offscreen`. `--help`, `--version` and `--check-config` switch to offscreen automatically when no display is set. |
| `Refusing to run as root` (exit code 2) | Run as a regular user, or pass `--allow-root` / `NEXORA_ALLOW_ROOT=1`. |
| `Nexora is already running` (exit code 3) | Another instance holds `$XDG_RUNTIME_DIR/nexora-console-ui.lock`; close it first. |
| Controller not detected | Diagnostics tab → input status. "permission denied": `sudo usermod -aG input $USER`, then log in again. Only devices reporting `BTN_GAMEPAD` are used. See [docs/controller-support.md](docs/controller-support.md). |
| No games found | Check that a folder is configured, that an enabled emulator declares the files' extension, and use "Rescan library"; scan issues are listed on the Diagnostics tab. |
| Settings reset unexpectedly | The file was unusable and moved to `settings.json.corrupt-<timestamp>`; see [docs/settings.md](docs/settings.md#validation-and-recovery). |
| Entry shown as unavailable | The executable is missing, not a regular file, not executable or not absolute; the Emulators and Diagnostics tabs show the reason. |
| Need more detail | Run with `--log-file nexora.log` and `QT_LOGGING_RULES="nexora.*.debug=true"`. |

## MVP 3 outlook

Planned next (not implemented):

- Build and boot the Buildroot image in QEMU and on real PCs; fix what that reveals.
- Test real controllers (Xbox, PlayStation, Nintendo, 8BitDo) and at least one real emulator end to end; record results.
- On-screen keyboard and folder browser so no keyboard is ever needed; UI for per-game emulator choice and purging missing games.
- UI scaling for TVs; real display-mode selection.
- Move library database work off the UI thread for very large collections.
- Metadata and artwork (local only), save-state integration where emulators support it.
- A license decision.

---

## MVP — Minimum Viable Product

The first version of Nexora OS will focus on a small, reliable, testable foundation rather than supporting every emulator or gaming platform.

### MVP goals

- **Bootable Linux environment:** Create a minimal Linux-based operating system using Buildroot.
- **Controller-first UI:** Build a Qt 6 / QML interface navigable with a game controller.
- **Emulator Manager:** Launch and monitor compatible native Linux emulator processes.
- **Process lifecycle management:** Handle successful exits, crashes, missing executables, invalid configurations, and launch failures.
- **Emulator configuration:** Define emulator executables, launch arguments, and settings through configuration files.
- **Logging and diagnostics:** Record application events and errors to simplify troubleshooting.
- **Automated testing:** Test core functionality with a mock emulator and automated unit and integration tests.
- **Reproducible builds:** Use CMake, Ninja, and a documented build process.

### MVP scope

The initial release should demonstrate the complete basic workflow:

1. Boot into Nexora OS.
2. Navigate the interface using a controller.
3. Select an available emulator.
4. Launch a test application or compatible emulator.
5. Return to the Nexora OS interface after the process exits.
6. Recover gracefully from failures and provide useful logs.

MVP acceptance criteria: the system can boot on the selected test hardware, the UI is usable with a controller, and the emulator launch-and-return workflow passes automated tests.

MVP 1 covered steps 2–6 on a desktop Linux host. MVP 2 adds the game library, settings, remapping and system integration, and the Buildroot tree for step 1, which is not yet verified.

### Planned technology stack

- Operating system: Buildroot-based Linux
- Languages: C++20
- UI: Qt 6 and QML
- Build system: CMake and Ninja
- Process management: Qt QProcess
- Input: SDL3 and/or Linux input interfaces
- Configuration and persistence: JSON initially, SQLite where structured persistence is needed
- Testing: Qt Test and CTest
- CI: GitHub Actions

### Development roadmap

**Phase 1 — Foundation**

- Set up the repository and project structure.
- Establish coding standards and the build system.
- Implement shared types, configuration handling, and logging.
- Configure automated builds and tests.

**Phase 2 — Desktop Prototype**

- Create the main QML interface.
- Implement controller navigation.
- Build the emulator library and selection screen.
- Add a mock emulator for repeatable testing.

**Phase 3 — Emulator Management**

- Implement process launching and monitoring.
- Handle crashes, timeouts, and invalid configurations.
- Add emulator profiles and error reporting.
- Test the complete launch-and-return workflow.

**Phase 4 — Bootable Nexora OS**

- Create the Buildroot external tree.
- Package the application and its dependencies.
- Configure startup, graphics, input, and system services.
- Produce a bootable image for selected x86-64 hardware.

**Phase 5 — MVP Release**

- Validate the image on target hardware.
- Run integration and regression tests.
- Document installation, configuration, troubleshooting, and known limitations.
- Publish an initial experimental release.

### Future versions

**v0.2 — Improved Experience**

- Better controller support and hot-plug handling.
- Persistent settings and configurable controls.
- Improved error messages and diagnostics.
- A more polished library interface.

**v0.5 — Expanded Emulator Support**

- Integration of additional compatible native Linux emulators.
- Per-emulator settings and launch profiles.
- Game library scanning and metadata.
- Save-state and artwork integration where supported by the selected emulator.

**v1.0 — Stable Release**

- Reliable installation and system updates.
- Hardware compatibility testing.
- Recovery and configuration-reset options.
- Documented release and regression-testing process.

## Design principles

- Controller-first interaction
- Minimal distractions
- Modular architecture
- Open-source components
- Reliable process management
- Reproducible builds
- Clear licensing and attribution

## Scope and limitations

Nexora OS will integrate existing emulator projects rather than reimplement every console emulator from scratch. Platform support will depend on emulator compatibility, hardware requirements, and licensing.

Game files, BIOS files, firmware, and other copyrighted materials will not be bundled unless redistribution is explicitly permitted. Users must supply files they are legally entitled to use.

## Licensing

The repository does not contain a LICENSE file yet, and the licensing decision for Nexora OS is still open. Until a license is chosen, no license is granted for the code in this repository.

Dependencies keep their own licenses, for example Qt 6 (LGPLv3 / GPL, or commercial) and the optional SDL3 (zlib). No games, BIOS files or firmware are included or distributed.
