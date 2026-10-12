# Testing

The test suite uses Qt Test, registered with CTest. It needs no display, no controller, no real emulator and no network: GUI tests run on the offscreen Qt platform, process tests drive a small fake emulator, and library/settings tests use temporary directories and databases.

**Status (MVP 2 completion, `claude/mvp2-completion`, 2026-10-11):** locally verified on Ubuntu 24.04 (Qt 6.4.2): **19/19** tests pass with GCC 13 Debug `-Werror` (`ci` preset), Clang 18 Debug `-Werror` and GCC Release; all 19 test executables (including `tst_qmlsmoke`) also pass when run as a non-root user, so the [root-skipped functions](#tests-skipped-as-root) were executed. On GitHub, all five `ci.yml` jobs passed for this branch ([run \#27](https://github.com/szebasztianmarton/Nexora-OS/actions/runs/38121236221), commit 0701c8a, 2026-10-11), including the GCC + SDL3 job; the `dashboard-screenshots` artifact was uploaded. Earlier: all five `ci.yml` jobs passed for the MVP 2 dashboard commit ([run \#8](https://github.com/szebasztianmarton/Nexora-OS/actions/runs/38070075614), 2026-10-10).

Testing on Windows 11 (WSL 2): see [testing-windows11.md](testing-windows11.md).

## Contents

- [Layout](#layout)
- [What each test covers](#what-each-test-covers)
- [Fake emulator](#fake-emulator)
- [Running tests](#running-tests)
- [Test environment](#test-environment)
- [Tests skipped as root](#tests-skipped-as-root)
- [Adding a test](#adding-a-test)
- [Continuous integration](#continuous-integration)
- [Manual tests](#manual-tests)
- [Not covered by automated tests](#not-covered-by-automated-tests)

## Layout

```
tests/
  CMakeLists.txt                 nexora_add_test() helper
  unit/                          fast, isolated tests              (label: unit)
  integration/                   real processes / SQLite / QML     (label: integration)
  fixtures/fake-emulator/        nexora-fake-emulator + dev configs
```

Test executables are written to `build/<preset>/tests/`.

## What each test covers

### Unit (`-L unit`, 14 tests)

| Test | Links | Covers |
| --- | --- | --- |
| `tst_configloader` | `nexora_shared` | Emulator definitions: complete definition, defaults, rejection of invalid definitions (data-driven: JSON errors, wrong types, bad ids, empty fields), unknown-key warnings, relative executable resolved against the config file, trimming, unreadable files, id validation, missing/empty directory, sorted loading ignoring non-`.json` files, duplicate ids, valid entries kept next to invalid ones, oversized files. |
| `tst_settings` | `nexora_shared` | Built-in defaults without files, persistence across instances, rejection of invalid values (data-driven), normalisation (paths, extensions, JSON doubles as ints), setting the layered default removes the override, system defaults layered under user values, invalid entries dropped with issues, corrupted user file moved to `.corrupt-<timestamp>` (data-driven), reset keeps `settings.json.bak`, inspect mode (used by `--check-config`) never renames a broken file, saved file is versioned JSON, every built-in default passes its own validator. |
| `tst_paths` | `nexora_shared` | Config-dir precedence (command line > `NEXORA_CONFIG_DIR` > `<appDir>/../share/nexora/emulators`), relative overrides made absolute, source descriptions, state directory from `XDG_STATE_HOME` (relative values ignored). |
| `tst_logging` | `nexora_shared` | Log file rotation and size bound; emulator output (`nexora.process.output`) kept out of the in-memory ring buffer. |
| `tst_processstate` | `nexora_emulator_manager` | Full state transition table (data-driven), active states, state names, error messages. |
| `tst_launchprofiles` | `nexora_emulator_manager` | Profiles and MVP 2 options (`workingDirectory`, `extensions`, `stopTimeoutMs`) parsed; MVP 1 format still accepted; invalid profiles/options rejected (data-driven); `resolve()` without a game (drops `{file}` arguments), `{file}` substitution, appending the game path without a placeholder, profile working directory, stop timeout from the config, invalid requests rejected (data-driven), invalid request fails a launch without starting a process. |
| `tst_launchvalidation` | `nexora_emulator_manager` | `validateExecutable()` (empty, relative, missing, directory, not executable, valid), synchronous failure of invalid launches, disabled configs, empty ids, `stop()` without a process. |
| `tst_emulatorlistmodel` | `nexora_ui` | Model roles, status updates and reset, lookup by id. |
| `tst_libraryquery` | `nexora_library` | Title sort (case-insensitive), missing entries hidden, recent sort (played first), emulator and added sort, favorites filter, recent filter limit and order, search with all terms (whitespace incl. tabs separates terms), emulator filter, effective emulator (override vs candidates), name round trips, titles from file names (data-driven). |
| `tst_libraryscanner` | `nexora_library` | Only configured extensions found; bad roots (missing, relative, `/`) reported; symlinks skipped by default; with `followSymlinks` only links inside the root followed, escapes refused, broken links reported as "broken symbolic link", loops terminate; duplicates across overlapping roots; unreadable entries skipped; depth and file-count limits; cancellation. |
| `tst_systemintegration` | `nexora_system` | Platform detection (`NEXORA_PLATFORM`, `/etc/nexora-release`), status from a fake `/proc`, missing `/proc` tolerated, status and CPU times from the real `/proc` (size-0 proc files), CPU time parsing from `/proc/stat`, battery and network status from a fake `/sys` (virtual interfaces ignored, wired wins over Wi-Fi), desktop never controls power, update info without an update mechanism (installed version and channel from `/etc/nexora-release`), embedded power requests via the FIFO, embedded volume runs the mixer and falls back from `Master` to `PCM` (fake `amixer`), single-instance lock, Unix signals turned into Qt signals. |
| `tst_buttonmapping` | `nexora_input` | Default mapping, settings default equals the built-in default, variant-map round trip, older stored mappings gain `options` on a free Select (and keep Select's binding otherwise), `bind()` swaps conflicting buttons, invalid mappings rejected (data-driven: unknown names, duplicate buttons, missing accept/back), unbound Guide falls back to the game bar, name round trips. |
| `tst_gamepadnavigator` | `nexora_input` | Button → action mapping (data-driven), release behaviour, unmapped buttons, axis thresholds and hysteresis, directions, flip to the opposite side, hat axes, auto-repeat (start, no repeat for buttons, stop on release, latest direction wins, `reset()`), default repeat delays, Select + Start chord (fires after the hold, default 1000 ms, cancelled by early release, needs both buttons, per device), per-device state (`devicesDoNotInterfere`, `resetDevice()` drops only that device). When the evdev backend is built (`NEXORA_HAVE_EVDEV`): evdev key mapping (incl. `BTN_TL`/`BTN_TR` → L1/R1), abs mapping, axis normalisation. |
| `tst_inputcontroller` (GUI) | `nexora_input` | Action → key mapping (data-driven) and `keyForAction()`, Accept sends Return, synthesized press/release delivered to a target window, navigator → window path, suspension drops actions and resume delivers again, no target window / empty provider, `start("none")`, unknown backend fails, `auto` never fails (whatever `/dev/input` looks like), backend restart, chord reported while suspended (Menu still dropped), new actions (favorite/search/tabs) and remap capture (directions still navigate, captured button not delivered as an action), Guide reported as `gameBarRequested` (never a key, also while suspended, not during capture). |

### Integration (`-L integration`, 5 tests)

| Test | Links | Covers |
| --- | --- | --- |
| `tst_emulatormanager` | `nexora_emulator_manager` + fake emulator | Normal exit, non-zero exit, crash (`segv`, `abort`), missing executable, failure to start, stdout/stderr capture and bound, delayed start-up, controlled stop, SIGKILL escalation, second launch rejected, relaunch after exit and from the `processFinished` slot, recovery after failures, launch with profile and game (arguments and working directory checked via `--echo-args`/`--print-cwd`), `stopTimeoutMs` from the config used, destroying the manager while a process runs, the on-disk output log (header, `out|`/`err|` lines, outcome footer, previous run kept as `.log.1` after a crash and relaunch, size cap, disabled by default). |
| `tst_gamelibrary` | `nexora_library` | Asynchronous scan and persistence (a new instance sees the data without scanning), rescans keep favorites/history and track missing files (hidden while missing, restored when the file returns), an unavailable root keeps its entries while removing a root from the configuration drops them, refresh queued while scanning, destruction during a scan, corrupted database moved aside and recreated, a newer-schema database left untouched, a symlinked root whose drive is unmounted keeps its entries, emulator override, favorites and launches written while the worker merges a scan are not lost, the in-memory database fallback; `benchmarkEventLoopStall` (opt-in, see [below](#library-benchmark)). |
| `tst_appcontroller` | `nexora_ui` + fake emulator | Config loading and issue reporting, missing directory, launch and return, failure reporting and recovery, controller input suspended while an emulator runs, Select + Start chord stops the running emulator, game bar over a running emulator (Guide opens/closes it and lifts input suspension, CPU/memory figures and history from a fake `/proc`, session time), stop and busy guards. |
| `tst_uicontext` (GUI) | `nexora_ui` + fake emulator | The full backend object graph used by the UI (`UiContext`) with temporary settings, database and game directory: settings and library persist across restarts (no rescan needed); corrupted settings fall back to (system) defaults; invalid controller mapping reported and ignored, repaired by remapping; remap capture only with a controller backend, capture of the next button with swap, and the completing press not also executed as an action; launching a game from the library (favorites, recent list, search/filters); a launch that fails to start is not counted as played; game file deleted after the scan (launch refused cleanly, refresh recovers); profiles and default profiles (disabled/unknown profile messages, stale default falls back); recovery after configuration errors; desktop system integration never powers off and reports no volume support; game options (details, emulator choice limited to candidates and kept across restarts, back to automatic), default profile used by a plain launch and skipped by "emulator defaults", missing games counted and purged. |
| `tst_qmlsmoke` (GUI) | `nexora_uiplugin`, `nexora_ui` + fake emulator | Loads the real `Main.qml` against a real `UiContext` (temporary config, settings, database and game folder) and drives the dashboard with key events, exactly as controller input arrives; any QML warning fails the test. `startsOnHomeAndSwitchesTabs`: starts on Home with the empty-library hint, PageUp/PageDown and Ctrl+Tab switch tabs, each tab remembers its focus, Menu/F1 open Diagnostics, Back on a root tab is a no-op, Up reaches the tab bar. `gamesSearchLaunchAndFavorite`: only game extensions listed, `/` and Ctrl+F focus search, typing filters, empty state, tab switching from the text field, launch shows the overlay (Back and tab switching blocked while running, Enter stops) and focus returns to the same game, favorite toggle (`Favorites` key and `F`), Home shows "Continue playing" and launches from there. `emulatorsEmptyConfiguration`: empty state. `emulatorsLaunchAndProfiles`: direct launch without profiles, profile picker with enabled profiles, Back closes it, Y/F sets the default profile and the picker reopens on it. `gameOptionsDialog`: `O` and the controller's `Context1` key open the game options, tab keys do not leak through, choosing the second emulator adds its profiles, Back returns focus to the game, Play launches with the chosen emulator and focus returns afterwards, "Remove missing games" appears on Settings after a file disappears. `settingsVolumeAndRemap`: volume row Left/Right, hint bar toggle, keyboard remap with swap and focus kept, Up/Down stay on the screen, adding a library folder from the text field. `gameBarOverlay`: Ctrl+G and `GameBarController::toggle()` open the game bar on the Home button, widget buttons switch panels, Down/Up enter and leave a panel, Left/Right walk the tiles, volume Left/Right, Back closes it and restores focus, "Game library" switches tabs, a recently played game launches from its tile, over a running emulator "Back to game" comes first and keys never reach the running overlay, stop needs a confirmation (shown in the tile label) and the panel switches to its idle tiles. |

## Fake emulator

`nexora-fake-emulator` (`tests/fixtures/fake-emulator/main.cpp`) is a Qt-free C++ program. Options are applied in this fixed order regardless of their position on the command line:

| Option | Effect |
| --- | --- |
| `--startup-delay-ms N` | Sleep N ms before doing anything (slow start-up) |
| `--ignore-sigterm` | Ignore SIGTERM (forces the manager to escalate to SIGKILL) |
| *(always)* | Print `ready` on stdout once signal handling is configured |
| `--print-cwd` | Print `cwd: <dir>` after `ready` |
| `--echo-args` | Print `arg: <value>` for every command-line argument after `ready` |
| `--stdout TEXT` | Write TEXT + newline to stdout (repeatable) |
| `--stderr TEXT` | Write TEXT + newline to stderr (repeatable) |
| `--sleep-ms N` | Stay alive for N ms (capped at 10 minutes) |
| `--crash segv\|abort` | Terminate via SIGSEGV or SIGABRT (core dumps disabled) |
| `--exit-code N` | Exit with status N (default 0) |

Arguments that do not start with `--` (e.g. a game file path substituted for `{file}`) are accepted and ignored. Unknown `--` options, missing values or invalid numbers exit with status 64. Tests get its path through the compile definition `NEXORA_FAKE_EMULATOR`; the build also generates the `fake-ok`, `fake-fail`, `fake-crash` and `fake-long` development configs from it.

```sh
build/debug/bin/nexora-fake-emulator --echo-args --print-cwd /tmp/game.rom --exit-code 3; echo $?
```

## Running tests

All tests:

```sh
scripts/build.sh ci --test      # configure + build + ctest with the ci preset
ctest --preset debug            # after building the debug preset
```

By label:

```sh
ctest --preset debug -L unit
ctest --preset debug -L integration
```

A single CTest test or a group (regular expression on the name):

```sh
ctest --preset debug -R tst_gamelibrary
ctest --preset debug -R 'tst_library|tst_gamelibrary'     # all library tests
ctest --preset debug -R tst_qmlsmoke --output-on-failure -V
```

A test binary directly, optionally a single Qt Test function or data row:

```sh
export QT_QPA_PLATFORM=offscreen
build/debug/tests/tst_emulatormanager                         # all functions
build/debug/tests/tst_emulatormanager stopEscalatesToKill
build/debug/tests/tst_libraryscanner -functions               # list functions
build/debug/tests/tst_configloader "rejectsInvalidDefinitions:uppercase id"   # one data row
```

## Test environment

`nexora_add_test()` sets these properties on every test, so a plain `ctest` (without a preset) behaves the same everywhere:

- `QT_QPA_PLATFORM=offscreen`, `QT_QUICK_BACKEND=software`
- `QT_LOGGING_RULES=nexora.*.debug=true` (full Nexora logging in test output)
- `TIMEOUT 120` seconds and the test's labels

The test presets additionally set `QT_QPA_PLATFORM=offscreen`, print output on failure and treat "no tests found" as an error. When running a binary by hand, set `QT_QPA_PLATFORM=offscreen` yourself if there is no display.

### Library benchmark

`tst_gamelibrary benchmarkEventLoopStall` creates 10 000 files and measures the longest stall of the event loop (a 5 ms timer) during a first scan and an unchanged rescan. It is skipped unless `NEXORA_LIBRARY_BENCH=1`; use a Release build:

```sh
NEXORA_LIBRARY_BENCH=1 QT_QPA_PLATFORM=offscreen build/release/tests/tst_gamelibrary benchmarkEventLoopStall
```

Results on the development container (4 cores, Release, 3 runs): total 0.16–0.25 s, longest stall 5–6 ms. The same benchmark against the previous implementation (merge on the UI thread) gave 118–139 ms (first scan) and 72–95 ms (rescan).

### Dashboard screenshots

With `NEXORA_SCREENSHOT_DIR=<dir>`, `tst_qmlsmoke` saves PNGs of the game options, the games grid, the profile picker and the Settings purge row into `<dir>`. CI uploads them as the `dashboard-screenshots` artifact (GCC Debug job).

Tests create their configuration files, settings files, SQLite databases and game directories in `QTemporaryDir`s and pass the paths explicitly (e.g. `UiContext::Options`), so they never read or write your real `~/.config/nexora` or `~/.local/share/nexora`. `tst_inputcontroller` starts the real `auto` backend, which only reads `/dev/input` (if present); it must succeed whatever it finds.

## Tests skipped as root

Two test functions check permission handling and cannot work as root, because root can read any file. They call `QSKIP` when `geteuid() == 0`:

| Test function | Skipped because |
| --- | --- |
| `tst_configloader::reportsUnreadableFiles` | root can read a `chmod 000` definition |
| `tst_libraryscanner::skipsUnreadableEntries` | root can read unreadable files and directories |

The test still passes overall when skipped. They were executed in the local non-root run (all 19 test executables, 2026-10-11). Containers and CI jobs that run as root silently lose this coverage; GitHub-hosted `ubuntu-24.04` runners run as a non-root user.

## Adding a test

1. Create `tests/unit/tst_<name>.cpp` (or `tests/integration/`) as a Qt Test class ending with `QTEST_MAIN(...)` (or `QTEST_GUILESS_MAIN`) and `#include "tst_<name>.moc"`.
2. Register it in that directory's `CMakeLists.txt`:

   ```cmake
   nexora_add_test(tst_<name> SOURCES tst_<name>.cpp
       LIBS nexora_library LABELS unit [GUI] [FAKE_EMULATOR])
   ```

   - `LIBS`: the Nexora libraries under test (`Qt6::Test` and `nexora_warnings` are added automatically).
   - `LABELS`: `unit` or `integration`.
   - `GUI`: documents that the test needs a `QGuiApplication`. The flag is currently only a marker; every test runs on the offscreen platform anyway.
   - `FAKE_EMULATOR`: adds a build dependency on `nexora-fake-emulator` and defines `NEXORA_FAKE_EMULATOR="<path>"`.
3. Use `QTemporaryDir` for all files, `":memory:"` or a temporary path for databases, `QTRY_*` macros with timeouts for asynchronous behaviour (processes, scans), and never depend on real emulators, real input devices, a display or the user's home directory. Guard permission tests with a root check.
4. The test must build with the `ci` preset (`-Werror`) and pass `clang-format-18`.

## Continuous integration

**Status:** all five `ci.yml` jobs below passed on `feature/mvp2` ([run \#8](https://github.com/szebasztianmarton/Nexora-OS/actions/runs/38070075614), commit 727f34a); the `buildroot.yml` validate job passed as well (its opt-in full image job has never been run). Run \#6 failed only in `lint` because one test file was not yet formatted. (For reference: all `ci.yml` jobs as they existed for MVP 1 passed on `feature/mvp1`, [run \#2](https://github.com/szebasztianmarton/Nexora-OS/actions/runs/37954884460), 2026-10-09; the `lint` job and the `--check-config` step are new since then.)

### `.github/workflows/ci.yml`

Runs on push, pull request and manual dispatch, on `ubuntu-24.04` with the apt Qt 6.4.2 packages (including `libqt6sql6-sqlite`).

| Job | Steps |
| --- | --- |
| `lint` — formatting and file checks | `clang-format-18 --dry-run --Werror` on all tracked `*.cpp`/`*.h`; every tracked `*.json` parsed with `python3 -m json.tool`; `shellcheck scripts/*.sh`. |
| `build-test` — gcc / debug (-Werror) | `ci` preset with GCC: configure, build, `ctest -L unit`, `ctest -L integration`, then `--check-config` (below). |
| `build-test` — clang / debug (-Werror) | Same with Clang. |
| `build-test` — gcc / release | Same with the `release` preset. |
| `build-sdl3` — gcc / debug + SDL3 backend | Builds SDL3 `release-3.2.30` from source (console-only, cached), configures the `ci` preset with `-DNEXORA_WITH_SDL3=ON -DCMAKE_PREFIX_PATH=<sdl3 prefix>`, builds and runs all tests. |

The configuration validation step of each `build-test` job runs:

```sh
nexora-console-ui --check-config --config-dir config/emulators          --settings-file config/examples/settings.json
nexora-console-ui --check-config --config-dir config/examples/emulators --settings-file config/examples/settings.json
```

and fails the job on any emulator definition error, settings issue or invalid `input.mapping`. The check is read-only and never renames a settings file ([settings.md](settings.md#checking-a-configuration---check-config)). On failure of a `build-test` job, `LastTest.log` is uploaded as an artifact.

### `.github/workflows/buildroot.yml`

Runs on pushes and pull requests that touch `buildroot-external/**` or the workflow itself, and on manual dispatch.

| Job | Steps |
| --- | --- |
| `validate` — check-package + defconfig symbols | Clones Buildroot `2026.02.3`, runs `utils/check-package --br2-external` on every file of `buildroot-external/`, and `buildroot-external/scripts/check-defconfig.sh` to make sure the defconfig symbols survive Kconfig. |
| `image` — full image build | Only on manual dispatch with `full_build: true`; builds `disk.img` and uploads it compressed. May exceed the 6 h limit of hosted runners; see [buildroot.md](buildroot.md#ci). |

## Manual tests

Not automated; run before calling a release ready and record the results:

| Area | How |
| --- | --- |
| Controller hardware | The [controller manual test checklist](controller-support.md#manual-test-checklist): detection, permissions, hotplug, button positions, remapping, suspension, Select + Start chord, multiple pads. No physical controller has been tested so far. |
| Buildroot image | Build the image and boot it in QEMU (UEFI/OVMF) as described in [buildroot.md](buildroot.md#running-in-qemu); check auto-start, recovery access, power requests and a USB controller passed through to the VM. |
| Real emulators | Configure at least one real emulator with `extensions` and `{file}`, add a directory with legally obtained test files, launch from the library, stop via the UI and via the chord. |
| Display and audio | Full-screen/windowed switching, `display.resolution` window size, volume on the image (`amixer`). |

## Not covered by automated tests

- Real gamepad hardware; the evdev and SDL3 backends' device handling (only the mapping/navigation logic is unit-tested).
- Real emulators.
- On-screen rendering beyond the offscreen QML smoke test; full-screen/kiosk operation on KMS.
- Booting the Buildroot image (manual, see above).
- Large real game collections (scanner limits are tested with small fixtures).
