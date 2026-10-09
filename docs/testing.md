# Testing

The MVP 1 test suite uses Qt Test, registered with CTest. It needs no display, no controller and no real emulator: GUI tests run on the offscreen Qt platform, and process tests drive a small fake emulator.

Locally verified on Ubuntu 24.04 (GCC 13.3, Qt 6.4.2): `ci` preset builds cleanly with `-Werror` and 10/10 tests pass.

## Layout

```
tests/
  CMakeLists.txt                 nexora_add_test() helper
  unit/                          fast, isolated tests              (label: unit)
  integration/                   real processes / real QML         (label: integration)
  fixtures/fake-emulator/        nexora-fake-emulator + dev configs
```

Test executables are written to `build/<preset>/tests/`.

## What each test covers

### Unit (`-L unit`)

| Test | Links | Covers |
| --- | --- | --- |
| `tst_configloader` | `nexora_shared` | Parsing a complete definition, defaults (`arguments`, `enabled`), rejection of invalid definitions (data-driven: JSON errors, wrong types, bad ids, empty fields), unknown-key warnings, relative executable resolution against the config file, id validation, missing/empty directories, sorted loading that ignores non-`.json` files, duplicate ids, keeping valid entries next to invalid ones, oversized files. |
| `tst_paths` | `nexora_shared` | Config-dir precedence (command line > environment > `<appDir>/../share/nexora/emulators`), relative overrides made absolute, source descriptions. |
| `tst_processstate` | `nexora_emulator_manager` | Full transition table (data-driven), active states, state names, error messages. |
| `tst_launchvalidation` | `nexora_emulator_manager` | `validateExecutable()` (empty, relative, missing, directory, not executable, valid), synchronous failure of invalid launches, disabled configs, empty ids, `stop()` without a process. |
| `tst_emulatorlistmodel` | `nexora_ui` | Model roles, status updates and reset, lookup by id. |
| `tst_gamepadnavigator` | `nexora_input` | Button → action mapping, release behaviour, ignored buttons, axis thresholds and hysteresis, directions, flipping to the opposite side, hat axes, auto-repeat (start, stop on release, latest direction wins, no repeat for Accept/Back/Menu), `reset()`, default repeat delays. When the evdev backend is built (`NEXORA_HAVE_EVDEV`): evdev key/abs mapping and axis normalization. |
| `tst_inputcontroller` (GUI) | `nexora_input` | Action → key mapping, synthesized press/release delivery to a target window, navigator → window path, suspension, dropped events without a target window, `start("none")`, unknown backend failure, `auto` never failing, backend restart. |

### Integration (`-L integration`)

| Test | Links | Covers |
| --- | --- | --- |
| `tst_emulatormanager` | `nexora_emulator_manager` + fake emulator | Normal exit, non-zero exit, crash (`segv` and `abort`), missing executable, failure to start, stdout/stderr capture, output bound, delayed start-up, controlled stop (state sequence `Starting → Running → Stopping → Exited`), SIGKILL escalation with a short stop timeout, rejection of a second launch, relaunch after exit and from the `processFinished` slot, recovery after failures, destroying the manager while a process runs. |
| `tst_appcontroller` | `nexora_ui` + fake emulator | Config loading and issue reporting, missing directory, launch-and-return, failure reporting and recovery, stop and busy guards (e.g. no reload while running). |
| `tst_qmlsmoke` (GUI) | `nexora_uiplugin`, `nexora_ui` + fake emulator | Loads the real `Main.qml` via `loadMainWindow()`: initial focus, ←/→ navigation, F1 → diagnostics, Esc → back, Esc on the root is a no-op, Enter launches and shows the running overlay with focus on Stop, Esc ignored while running, Enter stops and returns focus to the library, empty-configuration state. Fails on any QML warning. |

## Fake emulator

`nexora-fake-emulator` (`tests/fixtures/fake-emulator/main.cpp`) is a Qt-free C++ program. Options are applied in this fixed order regardless of their position on the command line:

| Option | Effect |
| --- | --- |
| `--startup-delay-ms N` | Sleep N ms before doing anything (slow start-up) |
| `--ignore-sigterm` | Ignore SIGTERM (forces the manager to escalate to SIGKILL) |
| *(always)* | Print `ready` on stdout once signal handling is configured |
| `--stdout TEXT` | Write TEXT + newline to stdout (repeatable) |
| `--stderr TEXT` | Write TEXT + newline to stderr (repeatable) |
| `--sleep-ms N` | Stay alive for N ms (capped at 10 minutes) |
| `--crash segv\|abort` | Terminate via SIGSEGV or SIGABRT (core dumps disabled) |
| `--exit-code N` | Exit with status N (default 0) |

Unknown options, missing values or invalid numbers exit with status 64. Tests get its path through the compile definition `NEXORA_FAKE_EMULATOR`; the build also generates the `fake-ok`, `fake-fail`, `fake-crash` and `fake-long` development configs from it.

Example:

```sh
build/debug/bin/nexora-fake-emulator --stdout hi --sleep-ms 500 --exit-code 3; echo $?
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

A single CTest test (regular expression on the name):

```sh
ctest --preset debug -R tst_emulatormanager
ctest --preset debug -R tst_qmlsmoke --output-on-failure -V
```

A test binary directly, optionally a single Qt Test function:

```sh
export QT_QPA_PLATFORM=offscreen
build/debug/tests/tst_emulatormanager                    # all functions
build/debug/tests/tst_emulatormanager stopEscalatesToKill
build/debug/tests/tst_emulatormanager -functions          # list functions
build/debug/tests/tst_configloader "rejectsInvalidDefinitions:uppercase id"   # one data row
```

### Test environment

`nexora_add_test()` sets these properties on every test, so a plain `ctest` (without a preset) behaves the same everywhere:

- `QT_QPA_PLATFORM=offscreen`, `QT_QUICK_BACKEND=software`
- `QT_LOGGING_RULES=nexora.*.debug=true` (full Nexora logging in test output)
- `TIMEOUT 120` seconds and the test's labels

The test presets additionally set `QT_QPA_PLATFORM=offscreen`, print output on failure and treat "no tests found" as an error. When running a binary by hand, set `QT_QPA_PLATFORM=offscreen` yourself if there is no display.

## Adding a test

1. Create `tests/unit/tst_<name>.cpp` (or `tests/integration/`) as a Qt Test class ending with `QTEST_MAIN(...)` (or `QTEST_GUILESS_MAIN`) and `#include "tst_<name>.moc"`.
2. Register it in that directory's `CMakeLists.txt`:

   ```cmake
   nexora_add_test(tst_<name> SOURCES tst_<name>.cpp
       LIBS nexora_emulator_manager LABELS unit [GUI] [FAKE_EMULATOR])
   ```

   - `LIBS`: the Nexora libraries under test (`Qt6::Test` and `nexora_warnings` are added automatically).
   - `LABELS`: `unit` or `integration`.
   - `GUI`: marks a test that needs a `QGuiApplication` (it runs on the offscreen platform like all tests).
   - `FAKE_EMULATOR`: adds a build dependency on `nexora-fake-emulator` and defines `NEXORA_FAKE_EMULATOR="<path>"`.
3. Use temporary directories (`QTemporaryDir`) for config files, `QTRY_*` macros with timeouts for asynchronous process behaviour, and never depend on real emulators, real input devices or a display.
4. The test must build with the `ci` preset (`-Werror`).

## Continuous integration

`.github/workflows/ci.yml` runs on push, pull request and manual dispatch, on `ubuntu-24.04` with the apt Qt 6.4.2 packages:

| Job | Configuration |
| --- | --- |
| gcc / debug (-Werror) | `ci` preset with GCC: configure, build, `ctest --preset ci` |
| clang / debug (-Werror) | `ci` preset with Clang |
| gcc / release | `release` preset with GCC |
| gcc / debug + SDL3 backend | Builds SDL3 `release-3.2.30` from source (cached), then `ci` preset with `-DNEXORA_WITH_SDL3=ON -DCMAKE_PREFIX_PATH=<sdl3 prefix>`, build and tests |

On failure, `LastTest.log` is uploaded as an artifact.

**Status:** all four jobs passed on `feature/mvp1` ([run \#2](https://github.com/szebasztianmarton/Nexora-OS/actions/runs/37954884460), 2026-10-09). The first run failed only in the SDL3 job (SDL refused to configure without X11/Wayland headers), fixed by building SDL as a console-only library.

## Not covered by automated tests

- Real gamepad hardware (evdev and SDL3 backends' device handling).
- Real emulators.
- On-screen rendering beyond the offscreen smoke test, and full-screen/kiosk operation.
- Any bootable image (does not exist yet).
