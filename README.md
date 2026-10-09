# Nexora OS

A Linux-based gaming operating system built for controller-first gaming.

Nexora OS is intended to be a gaming operating system, built from open-source components, that provides a console-like experience on x86-64 PCs and mini PCs. Its goal is to bring compatible native Linux emulators together in a unified, controller-driven interface. (The license of Nexora OS itself has not been decided yet; see [Licensing](#licensing).)

> **Status:** MVP 1 desktop prototype. There is no bootable image yet. See [Project Status](#project-status).

## Contents

- [Vision](#vision)
- [Project Status](#project-status)
- [MVP 1: what is implemented](#mvp-1-what-is-implemented)
- [Current limitations](#current-limitations)
- [Tech stack](#tech-stack)
- [Quick start](#quick-start)
- [Running](#running)
- [Testing](#testing)
- [Emulator configuration](#emulator-configuration)
- [Controls](#controls)
- [Troubleshooting](#troubleshooting)
- [MVP goals and roadmap](#mvp--minimum-viable-product)
- [Design principles](#design-principles)
- [Scope and limitations](#scope-and-limitations)
- [Licensing](#licensing)

Further documentation: [docs/architecture.md](docs/architecture.md), [docs/development.md](docs/development.md), [docs/testing.md](docs/testing.md).

## Vision

Create a dedicated gaming environment that boots directly into a simple, polished interface, allowing users to browse, launch, and manage their emulators without needing a keyboard or mouse for everyday use.

## Project Status

Current stage: **MVP 1 desktop prototype** (version 0.1.0, roadmap phases 1–3 in progress on a desktop Linux host).

| Area | Status |
| --- | --- |
| Build (CMake presets, Ninja, Qt 6.4.2 on Ubuntu 24.04, GCC 13) | Verified locally; `ci` preset builds cleanly with `-Werror` |
| Test suite (7 unit + 3 integration test executables) | 10/10 passing locally (offscreen; GCC + Clang, Debug + Release; also as a non-root user) |
| GitHub Actions CI | Configured; status not yet verified |
| Optional SDL3 gamepad backend | Compiles and links against SDL 3.2.30 built from source; never tested with a real controller |
| evdev gamepad backend | Mapping and navigation logic unit-tested; never tested with real controller hardware |
| Real emulators | None tested; no compatibility with any real emulator is claimed |
| Bootable image (Buildroot) | Not started; future work |

Nexora OS is experimental software. Hardware compatibility, supported emulators, and release readiness will be documented as development progresses.

## MVP 1: what is implemented

The MVP 1 prototype runs as an ordinary Qt application on a desktop Linux system.

Implemented:

- **Console UI** (`nexora-console-ui`, Qt 6 / QML): a horizontal emulator library, a diagnostics screen (version, configuration directory and its source, configuration issues, disabled entries, emulator state and last captured output, input backend status and devices, root warning, log file, recent log lines, configuration reload) and a modal "running" overlay with a Stop action.
- **Emulator Manager**: launches one external process at a time via `QProcess` with an explicit argument list (never through a shell), supervises it, captures bounded stdout/stderr, and handles normal exit, non-zero exit, crash, missing/non-executable files, failure to start, and stop requests (SIGTERM, then SIGKILL after a timeout). The UI returns to the library when the process ends.
- **Configuration**: one JSON file per emulator in a configuration directory, strictly validated; problems are reported in the UI and the log instead of aborting.
- **Controller input**: a backend-neutral navigator (D-pad, hat, left stick with hysteresis and auto-repeat) fed by a Linux evdev backend (built on Linux) or an optional SDL3 backend. Controller actions are turned into ordinary key events, so the same QML handles keyboard and controller.
- **Logging and diagnostics**: categorized Qt logging to stderr, an optional log file and an in-memory buffer shown on the diagnostics screen.
- **Safety defaults**: refuses to run as root unless explicitly allowed; the launched emulator receives SIGTERM if the launcher dies.
- **Testing**: Qt Test / CTest unit and integration tests, including a QML smoke test, driven by a fake emulator executable.

Planned (not in MVP 1): the Buildroot-based bootable image, kiosk/EGLFS start-up, a "return to launcher" hotkey while an emulator is in the foreground, persistent settings (SQLite where needed), game library scanning and metadata, configurable controls. See [docs/architecture.md](docs/architecture.md#future-work).

## Current limitations

- Desktop prototype only: no bootable image, no system integration, no installer.
- Only one emulator process can run at a time.
- No real emulator has been tested; the shipped `example.json` just runs `/bin/true`, and the development configurations run the bundled fake emulator.
- No real gamepad hardware has been tested (neither evdev nor SDL3).
- Controller navigation is suspended while an emulator runs (devices are never grabbed, so the emulator keeps receiving the pad). The running overlay can therefore only be stopped with the keyboard (Enter) or by quitting the emulator itself; a controller "return to launcher" chord is future work.
- Several connected gamepads share one navigation state, so simultaneous input from two pads can interfere.
- Configuration is read at start-up and on manual reload from the diagnostics screen; there is no file watching and no in-app editing.
- Controls are fixed (not remappable). Settings are not persisted.
- `Ctrl+Q` quits the prototype (a development convenience the console build will not expose).

## Tech stack

| Concern | Used in MVP 1 |
| --- | --- |
| Language | C++20 |
| UI | Qt 6 (minimum 6.4: Core, Gui, Qml, Quick, QuickControls2) and QML, static QML module `Nexora.Console` |
| Build | CMake ≥ 3.22, Ninja, `CMakePresets.json` (`debug`, `release`, `ci`) |
| Process management | `QProcess` (no shell), `prctl(PR_SET_PDEATHSIG)` |
| Input | Linux evdev (built when `linux/input.h` is available); SDL3 gamepad API (optional) |
| Configuration | JSON files (`QJsonDocument`) |
| Logging | Qt logging categories (`nexora.*`) |
| Testing | Qt Test and CTest, plus a fake emulator executable |
| CI | GitHub Actions (configured; status not yet verified) |

Planned: Buildroot-based Linux image; SQLite where structured persistence is needed.

## Quick start

Tested on Ubuntu 24.04 with the distribution's Qt 6.4.2.

### Install dependencies

```sh
sudo apt install cmake ninja-build g++ \
    qt6-base-dev qt6-declarative-dev \
    qml6-module-qtqml qml6-module-qtqml-workerscript qml6-module-qtqml-models \
    qml6-module-qtquick qml6-module-qtquick-controls qml6-module-qtquick-templates \
    qml6-module-qtquick-layouts qml6-module-qtquick-window \
    libgl-dev
```

`clang` can be used instead of `g++`. SDL3 is optional and not packaged on Ubuntu 24.04; see [docs/development.md](docs/development.md#optional-sdl3-backend) to build it from source.

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
```

A development build generates configurations for the fake emulator, so the app shows a populated library without any setup (see [Emulator configuration](#emulator-configuration)).

Command-line options:

| Option | Meaning |
| --- | --- |
| `--config-dir <dir>` | Directory containing emulator `*.json` definitions (overrides `NEXORA_CONFIG_DIR`) |
| `--log-file <file>` | Also append log output to `<file>` |
| `--input-backend <backend>` | `auto` (default), `evdev`, `sdl3` or `none`. `auto` tries SDL3 (if built), then evdev, then runs keyboard-only |
| `--fullscreen` | Show the interface full screen |
| `--allow-root` | Allow running as root (not recommended) |
| `-h`, `--help` / `-v`, `--version` | Standard Qt options |

**Root policy.** The application refuses to run as root (exit code 2), because launched emulators would inherit root privileges. Override with `--allow-root` or `NEXORA_ALLOW_ROOT=1`; a warning is logged.

**Headless.** Without a display, run with the offscreen platform:

```sh
QT_QPA_PLATFORM=offscreen scripts/run.sh
```

## Testing

```sh
scripts/build.sh ci --test
# or
ctest --preset debug            # all tests
ctest --preset debug -L unit    # unit tests only
ctest --preset debug -L integration
```

Tests run on the offscreen Qt platform and need no display or controller. See [docs/testing.md](docs/testing.md).

## Emulator configuration

Each emulator is one JSON object in its own `*.json` file:

```json
{
  "id": "example-true",
  "name": "Example (exits immediately)",
  "executable": "/bin/true",
  "arguments": [],
  "enabled": true
}
```

| Key | Type | Required | Rules |
| --- | --- | --- | --- |
| `id` | string | yes | `^[a-z0-9][a-z0-9_-]{0,63}$`; unique across all files |
| `name` | string | yes | non-empty (trimmed) |
| `executable` | string | yes | absolute path, or relative to the directory of the JSON file; never looked up in `PATH` |
| `arguments` | array of strings | no | default `[]`; passed verbatim, no shell expansion |
| `enabled` | boolean | no | default `true`; disabled entries are listed on the diagnostics screen only |

Rules:

- Only regular, readable `*.json` files directly in the directory are read, in file-name order. Files larger than 256 KiB are rejected.
- An invalid file (bad JSON, wrong types, invalid id) is skipped and reported as an error; valid files are still loaded.
- A duplicate `id` is an error; the later file (by name) is ignored.
- Unknown keys produce a warning and are otherwise ignored.
- Executables are checked at load time (absolute, exists, regular file, executable). Entries that fail are still shown, marked unavailable with the reason, and are checked again on launch.
- The emulator's working directory is the directory containing its executable.

Configuration directory lookup (first match wins):

1. `--config-dir <dir>`
2. `NEXORA_CONFIG_DIR`
3. `<appDir>/../share/nexora/emulators` (relative to the directory of the `nexora-console-ui` binary)

The diagnostics screen shows which directory and source were used. In a build tree, the default resolves to `build/<preset>/share/nexora/emulators/`, which the build fills with `example.json` and four generated fake-emulator configurations:

| File | Behaviour |
| --- | --- |
| `fake-ok.json` | prints a line, runs 3 s, exits 0 |
| `fake-fail.json` | runs 1 s, writes to stderr, exits with code 3 |
| `fake-crash.json` | runs 1 s, crashes with SIGSEGV |
| `fake-long.json` | ignores SIGTERM and runs up to 10 min (exercises SIGKILL escalation on Stop) |

The generated files contain absolute paths into the build tree; they are for development only.

## Controls

| Action | Keyboard | Controller |
| --- | --- | --- |
| Move selection (library) | ← / → | D-pad, left stick, hat |
| Launch selected emulator | Enter / Return / Space | A (south button) |
| Open diagnostics | F1 / Menu key | Start, Select/Back or Guide |
| Back | Esc / Backspace | B (east button) |
| Scroll (diagnostics) | ↑ / ↓ | D-pad, left stick |
| Reload configuration (diagnostics) | Enter | A |
| Stop running emulator (overlay) | Enter / Space | — (controller input is paused while an emulator runs) |
| Quit (development only) | Ctrl+Q | — |

Back on the library screen does nothing (the UI never quits on Back). While an emulator is running, every key except the Stop action is ignored. Held directions auto-repeat (400 ms initial delay, then every 120 ms). The left stick triggers at 50 % deflection and releases below 30 %. Buttons are named by position: "A" is the south button (Cross on PlayStation layouts), "B" the east button. The mouse also works: click selects a card, double-click launches, and the Stop and Reload buttons are clickable.

## Troubleshooting

| Symptom | Fix |
| --- | --- |
| CMake: `Qt 6.4+ with components [...] was not found` | Install `qt6-base-dev qt6-declarative-dev` (see [dependencies](#install-dependencies)) or pass `-DCMAKE_PREFIX_PATH=/path/to/Qt/6.x/gcc_64` for a custom Qt. |
| CMake: OpenGL not found while configuring Qt6Gui | `sudo apt install libgl-dev` |
| Runtime: `module "QtQuick.Controls" is not installed` (or `QtQuick`, `QtQuick.Layouts`, `QtQml.WorkerScript`, ...) | Install the matching `qml6-module-*` package from the dependency list. |
| `could not connect to display` / `Could not load the Qt platform plugin "xcb"` | Run inside a desktop session, or headless with `QT_QPA_PLATFORM=offscreen`. This applies even to `--help`, because the Qt GUI application is created before options are parsed. |
| `Refusing to run as root` (exit code 2) | Run as a regular user. If you must, pass `--allow-root` or set `NEXORA_ALLOW_ROOT=1`. |
| Controller not detected | Open diagnostics (F1) and read the input status. "permission denied" means your user cannot read `/dev/input/event*`: `sudo usermod -aG input $USER`, then log out and in again. Try `--input-backend evdev` explicitly. Only devices reporting gamepad buttons (`BTN_GAMEPAD`) are used. |
| `--input-backend sdl3` fails with "not built in" | Build with SDL3, see [docs/development.md](docs/development.md#optional-sdl3-backend). |
| Library is empty / "Configuration directory not found" | Check the directory shown on the diagnostics screen; pass `--config-dir` or set `NEXORA_CONFIG_DIR`. |
| Entry shown as unavailable | The executable is missing, not a regular file, not executable or not absolute; the card and the diagnostics screen show the reason. |
| Need more detail | Run with `--log-file nexora.log` and `QT_LOGGING_RULES="nexora.*.debug=true"`. |

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

MVP 1 (this prototype) covers steps 2–6 on a desktop Linux host; step 1 depends on Phase 4.

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
