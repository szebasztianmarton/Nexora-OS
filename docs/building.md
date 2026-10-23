# Building

How to build, test and run Nexora OS from source on a development host, how the development build differs from the release image, and the conventions for working on the code. The bootable image is built with Buildroot; see [buildroot.md](buildroot.md).

**Verified build environment (2026-10-16):** Ubuntu 24.04 container, Qt 6.4.2 (distribution packages), CMake 3.28.3, Ninja, GCC 13.3 and Clang 18.1.3, OpenSSL 3. With `-DNEXORA_WARNINGS_AS_ERRORS=ON` both compilers built cleanly and **41/41** CTest tests passed on that date; the suite has 49 tests now ([testing.md](testing.md)). The same steps are what CI runs on `ubuntu-24.04`. Building on Windows 11 through WSL 2 is described (in Hungarian) in [testing-windows11.md](testing-windows11.md).

## Contents

- [Prerequisites](#prerequisites)
- [Configure and build](#configure-and-build)
- [CMake options](#cmake-options)
- [Scripts](#scripts)
- [Running](#running)
- [Command-line reference](#command-line-reference)
- [Development build vs. release image](#development-build-vs-release-image)
- [Optional SDL3 backend](#optional-sdl3-backend)
- [Code layout](#code-layout)
- [Conventions](#conventions)
- [Licensing](#licensing)

## Prerequisites

```sh
sudo apt install cmake ninja-build g++ \
    qt6-base-dev qt6-declarative-dev qt6-declarative-dev-tools \
    qml6-module-qtqml qml6-module-qtqml-workerscript qml6-module-qtqml-models \
    qml6-module-qtquick qml6-module-qtquick-controls qml6-module-qtquick-templates \
    qml6-module-qtquick-layouts qml6-module-qtquick-window qml6-module-qtquick-shapes \
    qml6-module-qttest libqt6sql6-sqlite libgl-dev libssl-dev openssl
# optional, for the system tests, linting and formatting:
sudo apt install grub-common shellcheck clang clang-format-18
```

- Requirements from `CMakeLists.txt`: CMake ≥ 3.22, a C++20 compiler, Qt ≥ 6.4 with Core, Gui, Qml, Quick, QuickControls2, Sql (and Test + QuickTest when `BUILD_TESTING=ON`), OpenSSL 3 libcrypto (Ed25519, SHA-256 for updates).
- `libqt6sql6-sqlite` is the SQLite driver; without it the library is disabled at run time.
- `libgl-dev` is needed because Qt6Gui's CMake package looks for OpenGL.
- `qml6-module-*` packages are run-time QML imports; a missing one shows up as `module "..." is not installed` (also in `tst_qmlsmoke`).
- `qt6-declarative-dev-tools` provides `qmllint` for `scripts/qmllint.sh`.
- `grub-common` (`grub-editenv`, `grub-script-check`), `bash` and `shellcheck` enable the image helper tests (label `system`); without them those tests are not registered.
- For a self-built Qt, pass `-DCMAKE_PREFIX_PATH=/path/to/Qt/6.x/gcc_64`.

## Configure and build

With the presets in `CMakePresets.json` (Ninja, `BUILD_TESTING=ON`, compile commands exported, binary dir `build/<preset>`):

| Preset | Build type | Notes |
| --- | --- | --- |
| `debug` | Debug | default for the scripts |
| `release` | Release | |
| `ci` | Debug | `NEXORA_WARNINGS_AS_ERRORS=ON` (`-Werror`) |

```sh
cmake --preset ci
cmake --build --preset ci
ctest --preset ci                 # all tests, offscreen
CC=clang CXX=clang++ cmake --preset ci   # (in a fresh build dir) the same with Clang
```

Or without presets, into any directory:

```sh
cmake -S . -B ~/nxbuild/dev -G Ninja -DCMAKE_BUILD_TYPE=Debug -DNEXORA_WARNINGS_AS_ERRORS=ON
cmake --build ~/nxbuild/dev
cd ~/nxbuild/dev && QT_QPA_PLATFORM=offscreen ctest --output-on-failure
```

Output: `build/<preset>/bin/nexora-console-ui`, `build/<preset>/bin/nexora-update-tool`, `build/<preset>/bin/nexora-fake-emulator`, test executables in `build/<preset>/tests/`. In-source builds are rejected. Personal presets go in `CMakeUserPresets.json` (git-ignored).

## CMake options

| Option | Default | Meaning |
| --- | --- | --- |
| `BUILD_TESTING` | `ON` | build the test suite (needs Qt Test and Qt QuickTest) |
| `NEXORA_WARNINGS_AS_ERRORS` | `OFF` | add `-Werror` (GCC/Clang) |
| `NEXORA_BUILD_FAKE_EMULATOR` | `ON` | build `nexora-fake-emulator` and the development emulator configurations (also built whenever tests are) |
| `NEXORA_WITH_SDL3` | `AUTO` | `AUTO`: use SDL3 if found; `ON`: require it; `OFF`: never |
| `NEXORA_BUILD_ID` | empty | build identifier override; empty = `git describe --always --dirty`, recomputed at every configure, `dev` without git |
| `NEXORA_BOARD` | `nexora-x86_64` | board name reported by `--version`, written to `build-info` and compared with update manifests |

The project version is `project(NexoraOS VERSION 0.4.0)`; `nexora-console-ui --version` prints `<version> <build id> <board>`.

## Scripts

| Script | What it does |
| --- | --- |
| `scripts/build.sh [debug\|release\|ci] [--test]` | checks for `cmake`, `ninja` and a compiler, configures and builds the preset (default `debug`); `--test` runs `ctest --preset <preset>` |
| `scripts/run.sh [preset] [-- app args…]` | builds if needed (otherwise incremental), runs `build/<preset>/bin/nexora-console-ui` |
| `scripts/qmllint.sh BUILD_DIR` | lints all QML; fails on errors only |
| `scripts/shellcheck.sh` | shellcheck on every shell script in the repository |
| `scripts/image/build-image.sh [make targets]` | clones Buildroot 2026.02.3 into `~/nexora-br/buildroot` and builds the image into `~/nexora-br/out` ([buildroot.md](buildroot.md#building)) |
| `scripts/image/qemu-boot-test.sh [disk.img]` | boots an image in QEMU/OVMF and waits for the UI session to start ([testing.md](testing.md#manual-and-system-tests)) |
| `scripts/release/*.sh` | release artifacts, signing, verification, documentation check ([release-process.md](release-process.md)) |

## Running

```sh
scripts/run.sh                                   # debug build, windowed or fullscreen per display.mode
scripts/run.sh -- --windowed --settings-file /tmp/nx-settings.json --library-db /tmp/nx-library.sqlite
QT_QPA_PLATFORM=offscreen scripts/run.sh          # no display
```

**Game folder for development:** put game files (or folders) into `.games/` at the repository root; it is git-ignored and never committed. When that folder exists at configure time, the build writes `build/<preset>/share/nexora/settings-defaults.json` with `library.directories` set to it (the system-defaults layer of [settings.md](settings.md#file-locations)), so a dev run scans it without any Settings work. Another folder: `-DNEXORA_DEV_LIBRARY_DIR=<dir>`. Folders added in Settings still work on top of it. Re-run the configure step after creating or removing `.games/`; nothing of this is installed into the image.

**Fake emulators for development:** `-DNEXORA_DEV_FAKE_EMULATOR_CONFIGS=ON` generates emulator configurations for the fake emulator (`fake-ok`, `fake-fail`, `fake-crash`, `fake-long`) next to `example.json` in `build/<preset>/share/nexora/emulators/`, so launching, failure handling and the running overlay can be tried without a real emulator. Off by default (the Emulators tab shows only real definitions); configuring with it off removes the generated files again. These are build artefacts; do not commit them.

By default the application uses your real XDG directories (`~/.config/nexora`, `~/.local/share/nexora`, `~/.local/state/nexora`, `~/.cache/nexora`; [user-data.md](user-data.md)). Use `--settings-file`/`--library-db` or point `XDG_CONFIG_HOME`, `XDG_DATA_HOME`, `XDG_STATE_HOME` and `XDG_CACHE_HOME` at a scratch directory to keep experiments separate.

**Root policy:** the application exits with code 2 when run as root unless `--allow-root` or `NEXORA_ALLOW_ROOT=1` (emulators would inherit root).

**Controller access:** the evdev backend reads `/dev/input/event*`; add yourself to the `input` group (`sudo usermod -aG input "$USER"`, then log in again). See [controller-support.md](controller-support.md).

## Command-line reference

| Option | Meaning |
| --- | --- |
| `--config-dir <dir>` | emulator definitions (overrides `NEXORA_CONFIG_DIR`; default `<appDir>/../share/nexora/emulators`) |
| `--settings-file <file>` | user settings file (default `$XDG_CONFIG_HOME/nexora/settings.json`) |
| `--library-db <file>` | library database (default `$XDG_DATA_HOME/nexora/library.sqlite`) |
| `--log-file <file>` | also log to `<file>` (rotated at 2 MiB, 3 old files); on the embedded platform the default is `<state>/logs/console-ui.log` |
| `--input-backend auto\|evdev\|sdl3\|none` | controller backend (default: `input.backend` setting) |
| `--fullscreen` / `--windowed` | override `display.mode` for this run |
| `--check-config` | validate emulator definitions, settings and the controller mapping, print a report, exit 0/1; read-only |
| `--backup [--include-saves]` | create a backup headlessly, print its path, exit 0/1 |
| `--restore <dir>` | restore a backup headlessly, exit 0/1 |
| `--export-diagnostics` | write a redacted diagnostics report, print its path, exit 0/1 |
| `--verify-update <dir>` | check and verify an update bundle (signature, compatibility, image hash), print the final state, exit 0 when ready to install |
| `--version` | print version, build id and board |
| `--allow-root` | allow running as root (also `NEXORA_ALLOW_ROOT=1`) |
| `-h`, `--help` | help |

Environment: `NEXORA_CONFIG_DIR`, `NEXORA_PLATFORM` (`desktop`/`embedded`; overrides detection through `/etc/nexora-release`), `NEXORA_ALLOW_ROOT`, `NEXORA_UPDATE_TEST=1` (enables the update panel on the desktop, for testing), `QT_QPA_PLATFORM=offscreen`, `QT_LOGGING_RULES` (e.g. `nexora.*.debug=true`). `--help`, `--version`, `--check-config` and the headless actions switch to the offscreen platform when no display is set.

Exit codes: 0 normal, 1 QML load failure / failed check or headless action, 2 refused to run as root, 3 another instance is running.

## Development build vs. release image

| | Development build | Release image |
| --- | --- | --- |
| Built by | CMake on the host (`debug`/`ci` presets) | Buildroot `nexora-os` package (`BUILD_TESTING=OFF`, `NEXORA_BUILD_FAKE_EMULATOR=OFF`, SDL3 off, no `-Werror`) |
| Qt | host Qt (6.4.2 on Ubuntu 24.04) | Qt 6.9.1 cross-compiled, EGLFS/KMS |
| Platform | desktop (`NEXORA_PLATFORM` unset) | embedded (`/etc/nexora-release`) |
| Emulators | `example.json`, `xenia.json` (+ fake emulator configs with `NEXORA_DEV_FAKE_EMULATOR_CONFIGS=ON`) | `example.json`, `xenia.json` |
| Power, volume, updates | power refused, volume stored only, updates not supported | power and update FIFOs to root helpers, `amixer` volume |
| User data | your XDG directories | `/home/nexora` on the data partition |
| Update keys | `<build>/share/nexora/update-keys` (empty) | `/usr/share/nexora/update-keys` (from `buildroot-external/board/nexora-x86_64/update-keys/`) |

No development-host path is written into the image: the package is built from an rsynced copy of the source tree, and `/etc/nexora-release` is filled from the `build-info` file the build installs.

## Optional SDL3 backend

SDL3 is not packaged for Ubuntu 24.04 and not available in Buildroot 2026.02.x. Build a release into a prefix (3.2.30 is the version CI uses):

```sh
git clone --depth 1 --branch release-3.2.30 https://github.com/libsdl-org/SDL.git ~/src/SDL
cmake -S ~/src/SDL -B ~/src/SDL/build -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$HOME/.local/sdl3" -DSDL_TESTS=OFF -DSDL_EXAMPLES=OFF -DSDL_UNIX_CONSOLE_BUILD=ON
cmake --build ~/src/SDL/build && cmake --install ~/src/SDL/build
cmake --preset debug -DNEXORA_WITH_SDL3=ON -DCMAKE_PREFIX_PATH="$HOME/.local/sdl3"
```

The configure step prints `Nexora: gamepad backends: evdev=ON sdl3=ON`. `--input-backend auto` then prefers SDL3. The SDL3 backend has not been exercised with a real controller.

## Code layout

```
CMakeLists.txt, CMakePresets.json, cmake/   build configuration, warning flags
shared/                 nexora_shared: settings, emulator config loader, logging, paths,
                        DataLayout, AtomicFile, Redaction, SystemCatalog, BuildInfo
apps/emulator-manager/  nexora_emulator_manager: EmulatorManager, process states, health, launch history
apps/game-library/      nexora_library: scanner, SQLite database (schema v2), queries, GameLibrary
apps/system/            nexora_system: SystemIntegration, HardwareInfo, BackupService, DiagnosticReport,
                        single instance, signals
apps/update/            nexora_update: Version, UpdateManifest, SignatureVerifier, UpdateService;
                        tools/nexora-update-tool
apps/console-ui/        nexora_ui + QML module Nexora.Console (qml/), controllers (src/),
                        src/input/ = nexora_input; nexora-console-ui
config/                 emulators/example.json and xenia.json, examples/, systems/systems.json
tests/                  unit/, integration/, qml/, system/, support/, fixtures/
buildroot-external/     the Buildroot BR2_EXTERNAL tree (image)
scripts/                build/run, image/, release/, qmllint.sh, shellcheck.sh
prototypes/             separate UI prototype, not built
```

Architecture: [architecture.md](architecture.md).

## Conventions

- C++20, no extensions; warnings from `nexora_warnings` (`-Wall -Wextra -Wpedantic -Wshadow -Wnon-virtual-dtor -Woverloaded-virtual -Wcast-align -Wformat=2`); code must build with GCC and Clang with `-Werror`.
- Format with the repository `.clang-format` (`clang-format-18 -i`); CI runs `clang-format-18 --dry-run --Werror` on every tracked `*.cpp`/`*.h` outside `prototypes/`.
- Namespaces per module (`nexora::shared`, `nexora::emulator`, `nexora::library`, `nexora::system`, `nexora::update`, `nexora::input`, `nexora::ui`); public headers as `nexora/<module>/Header.h`.
- `#pragma once`; one class per `PascalCase.h/.cpp`; members `m_`; `QStringLiteral` for literals; 4-space indentation.
- Never block the GUI thread (scans and hashing run on worker threads); never start processes through a shell; never build a command string from metadata.
- QML is presentation only; system operations go through C++ controllers; no generic "run command" or "write file" entry point for QML.
- Write every owned file atomically (`shared::writeFileAtomically`); move corrupt files aside; version every persistent format.
- No telemetry and no network access.
- Log through the `nexora.*` categories; user-fixable errors also appear in the UI.
- Every behavioural change comes with a test ([testing.md](testing.md)); commit messages are imperative, sentence case.

## Licensing

Nexora OS is licensed under the GNU GPL, version 3 or later (`GPL-3.0-or-later`, [LICENSE](../LICENSE)); `cmake --install` installs the license text to `share/doc/nexora/LICENSE`. Dependencies keep their own licenses (Qt: LGPLv3, linked dynamically; OpenSSL 3: Apache-2.0; SQLite: public domain; SDL3: zlib). Contributions are accepted under the same license. Never commit games, BIOS files, firmware or private keys.
