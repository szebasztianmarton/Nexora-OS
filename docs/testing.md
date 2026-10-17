# Testing

The test suite uses Qt Test, Qt Quick Test and a few shell tests, all registered with CTest. It needs no display, no controller, no real emulator, no network and no root: GUI tests run on the offscreen platform with the software renderer, process tests drive `nexora-fake-emulator`, update tests generate throwaway Ed25519 keys at run time, and the image helper tests run against scratch files.

## Status

| Configuration (Ubuntu 24.04 container, Qt 6.4.2, CMake 3.28.3, Ninja) | Result | Date |
| --- | --- | --- |
| GCC 13.3, Debug, `-DNEXORA_WARNINGS_AS_ERRORS=ON` | build clean, **41/41** tests pass (27 unit, 11 integration, 3 system; 9 also labelled `security`) | 2026-10-16, commit `388781b` |
| Clang 18.1.3, Debug, `-DNEXORA_WARNINGS_AS_ERRORS=ON` | build clean, **41/41** pass | 2026-10-16, commit `388781b` |
| GCC 13.3, Debug (default warnings, no `-Werror` option), after the verification fixes | build OK, **41/41** pass (27 unit, 11 integration, 3 system; 9 `security`) | 2026-10-17, commit `64b014f` |
| `scripts/qmllint.sh` | 0 errors, 155 warnings | 2026-10-17, commit `64b014f` |
| `scripts/shellcheck.sh` | clean (16 POSIX sh, 12 bash scripts) | 2026-10-17, commit `64b014f` |
| `clang-format-18 --dry-run --Werror` on all tracked C++ | clean | 2026-10-16 (not re-run for `64b014f`) |
| `--check-config` on `config/emulators` and `config/examples/emulators` | OK | 2026-10-16 |
| GitHub Actions (`ci.yml`) for the MVP 4 commits | not recorded here; check the Actions tab | — |
| Buildroot A/B image build and QEMU boot | **not yet built/booted** <!-- TASK12-RESULT: image build result (duration, size) and qemu-boot-test.sh PASS/FAIL with time and serial log excerpt --> | — |

The `-Werror` builds with GCC and Clang were not repeated after the verification fixes (commit `64b014f`); only the GCC Debug build above was. The container runs as root, so the two [root-skipped functions](#tests-skipped-as-root) were skipped in these runs. Previous milestones: 19/19 at the end of MVP 2 (2026-10-11), also verified as non-root then.

## Contents

- [Layout and labels](#layout-and-labels)
- [Running tests](#running-tests)
- [What each test covers](#what-each-test-covers)
- [Fake emulator](#fake-emulator)
- [Test environment](#test-environment)
- [Tests skipped as root](#tests-skipped-as-root)
- [Adding a test](#adding-a-test)
- [Continuous integration](#continuous-integration)
- [Manual and system tests](#manual-and-system-tests)
- [Performance measurements](#performance-measurements)
- [Not covered](#not-covered)

## Layout and labels

```
tests/
  CMakeLists.txt         nexora_add_test() helper, update fixture directory
  unit/                  fast, isolated tests                               (label unit)
  integration/           real processes, SQLite, QML, shell tests            (label integration)
  qml/                   Qt Quick Test runner (tst_console_qml) + tst_*.qml  (labels integration;GUI)
  system/                image helper checks, registered only when host tools exist (label system)
  support/               UpdateBundleFixture: signed test bundles and keys
  fixtures/              fake-emulator/, virtual-pad/ (nexora-virtual-pad, uinput profiles), sysroot/ (fake /proc and /sys for hardware info)
buildroot-external/tests/test-update-helper.sh   run by the system test test_update_helper
```

| Label | Tests | Meaning |
| --- | --- | --- |
| `unit` | 27 | isolated logic |
| `integration` | 12 | real processes, databases, QML, shell scripts, virtual controllers (uinput) |
| `system` | 3 | image helpers, GRUB script syntax, image shell scripts (need `bash`, `grub-common`, `shellcheck`). `ctest -L system` runs 4, because CTest adds the fixture `tst_updateservice` that writes the bundles for `test_update_helper` |
| `security` | 9 (also in unit/integration) | path traversal and escapes, redaction, signature/manifest/checksum rejection: `tst_libraryscanner`, `tst_redaction`, `tst_diagnosticreport`, `tst_updatemanifest`, `tst_signatureverifier`, `tst_backupservice`, `tst_updateservice`, `test_update_tool`, `test_release_scripts` |

## Running tests

```sh
ctest --preset ci                       # all (after cmake --build --preset ci)
ctest --preset ci -L unit               # by label: unit, integration, system, security
ctest --preset ci -R tst_gamelibrary    # by name (regex)
ctest --preset ci -R tst_qmlsmoke --output-on-failure -V
QT_QPA_PLATFORM=offscreen build/ci/tests/tst_emulatormanager stopEscalatesToKill   # one function
build/ci/tests/tst_console_qml -input tests/qml/tst_quickmenu.qml                    # one QML test file
```

## What each test covers

### Unit (27)

| Test | Covers |
| --- | --- |
| `tst_configloader` | emulator definitions: valid/invalid (data-driven), defaults, unknown-key warnings, relative executables, duplicates, size limit; `version`, `systems` (unknown ids dropped), `pausable`; MVP 2 files still load |
| `tst_settings` | defaults, persistence, invalid values rejected (incl. the MVP 4 keys `ui.theme`, `ui.scale`, `ui.reducedMotion`, `ui.startScreen`, `input.deadzone`, `audio.muted`, `log.level`, `emulators.defaultForSystem`), layering, corrupt file moved aside, reset, inspect mode |
| `tst_paths` | config directory precedence, state directory from `XDG_STATE_HOME` |
| `tst_logging` | rotation and size bound, emulator output kept out of the ring, `setLogLevel()` filtering |
| `tst_processstate` | process state transition table, names, error messages |
| `tst_launchprofiles` | profiles, `{file}` substitution, working directory, stop timeout |
| `tst_launchvalidation` | executable validation, synchronous failure of invalid launches |
| `tst_emulatorhealth` | `Available`, `ExecutableMissing`, `NotExecutable`, `Disabled`, `InvalidConfig` |
| `tst_launchhistory` | 50-record bound newest-first, 4 KiB stderr tail, home path redacted, corrupt file moved aside, only the title stored |
| `tst_emulatorlistmodel` | model roles, status updates, lookup by id |
| `tst_systemlistmodel` | per-system game/emulator counts; "Other" bucket only when non-empty; unchanged systems update in place (no model reset) |
| `tst_gamepadnavigator` | button → action mapping, dead-zone press/release thresholds and clamping, hysteresis, auto-repeat, OS key-repeat ignored for Confirm/Back, Select+Start chord, Start hold (quick menu), per-device state, evdev code mapping and layouts (Xbox labels, joystick-class pads) |
| `tst_inputcontroller` | action → key delivery, suspension, capture, chord and Guide reported while suspended, device list and connect/disconnect signals, test mode (raw events, no navigation, hold Back 2 s to exit) |
| `tst_buttonmapping` | default mapping, validation, swaps, fallbacks |
| `tst_libraryquery` | sorting, filters incl. `systemId`, the `@other` filter for games without a system, and `Missing`, search incl. edited titles and system names |
| `tst_libraryscanner` (security) | extensions, bad roots, symlinks (skipped, escapes refused, loops terminate), system directories such as `proc/` not entered, multi-system extensions, duplicates, limits, cancellation |
| `tst_librarymigration` | real v1 schema → v2 with rows, favorites and play counts preserved, backup file opens as v1, version 99 refused untouched, no second backup |
| `tst_systemintegration` | platform detection, status from fake `/proc`, power FIFO, `amixer` fallback, update info (supported on the image, "not running" without the update FIFO, `Capability::Update` with it), single instance, Unix signals |
| `tst_datalayout` | 9 absolute distinct directories, XDG overrides and fallbacks, mode 0700, cache-only list excludes saves/data |
| `tst_atomicfile` | round trip, mode 0600, size-limited read, missing file, `moveAside()` |
| `tst_redaction` (security) | home path → `~`, URL credentials stripped, `token=`, `"password":` masked, more key names (`passphrase`, `apikey`, `auth`, `credential`, `session`, `cookie`, …) masked while `monkey`/`author`/`bypass` are not, `--flag value` and `--flag=value` secrets, `Authorization`/`Cookie` headers and `Bearer` tokens, unrelated text unchanged |
| `tst_systemcatalog` | built-in catalog (19 systems), `.sfc` → snes, `.iso` → several, invalid/duplicate entries dropped, oversize file rejected |
| `tst_hardwareinfo` | CPU, memory, GPU (Intel i915), HDMI output with modes, audio cards from `tests/fixtures/sysroot`; missing roots; 2 MiB file not read |
| `tst_diagnosticreport` (security) | version/build/GPU present, home path redacted, 100 000 log lines → ≤ 512 KiB, newest 10 reports kept, mode 0600 |
| `tst_version` | strict version parsing and ordering |
| `tst_updatemanifest` (security) | every malformed manifest case (unknown key, path in file name, uppercase/short hash, size 0, format 2, long notes, > 64 KiB, …) |
| `tst_signatureverifier` (security) | Ed25519 verify, tampered message/signature, wrong size, zero keys fail closed, RSA key rejected, second of two keys |

### Integration (12)

| Test | Covers |
| --- | --- |
| `tst_emulatormanager` | fake emulator: exits, crashes, missing executable, start failure, output capture and log files, stop and SIGKILL escalation, duplicate launch rejected, startup failure within the grace period vs. normal exit after it, pause/resume (SIGSTOP visible as state `T`), pause refused when not pausable, stop while paused, pid and start time, stop during `Starting` is fast, stop (incl. while paused and with SIGTERM ignored) also ends a grandchild in the process group |
| `tst_gamelibrary` | asynchronous scan and persistence, rescans keep favorites/history/metadata/hidden/system override, remove-from-library leaves the file, restore, scan progress for 250 files, cancel during a 5 000-file scan keeps entries present, overlapping roots, corrupt and newer-schema databases, writes during a merge not lost; opt-in benchmark |
| `tst_appcontroller` | config loading, launch and return, failure reporting, input suspension, chord stop, launch-history record after a run, startup-failure message, pause/resume, controller-disconnect notice while running, game bar and quick menu over a running emulator |
| `tst_uicontext` | the real backend graph: settings and library persist across two instances on the same data layout, removed game stays removed after restart, `gameDetails()` keys and redacted path, backup/restore round trip through the controller with settings re-applied, remapping, profiles, game options, missing games, an update record from the boot shown at startup, `start()` creates the update state directory, default emulator validated, failed edits emit notices |
| `tst_updatecontroller` | `supported` false on desktop / true with `NEXORA_UPDATE_TEST=1`; check → verify → ready; dismissing a rollback with `acknowledgeLastResult()` |
| `tst_qmlsmoke` | the real `Main.qml` against a real `UiContext`, driven by key events at **1280×720 and 1920×1080**: tutorial, tabs, search/launch/favorite, emulators and profiles, game options, settings volume and remap, game bar, all settings sections with keys only, high contrast + reduced motion, rolled-back update card and its dismissal, default-emulator rows keep focus, modal keys do not leak, Settings action feedback, stop from the running overlay asks first, layout at 150 % scale, "Other" and an empty system in the Games filter, game bar Restart/Power off ask first, lazy hardware probe, Back from a page to the tab bar, a file name with HTML tags shown literally on tiles and in the details; any QML warning fails |
| `tst_evdevhotplug` | virtual controllers through `/dev/uinput` (profiles shared with `nexora-virtual-pad`): each family of [linux_gamepad_drivers.md](linux_gamepad_drivers.md) hot-plugged into a running evdev backend, every button arrives at its position, D-pad, two pads, replug, keyboard and motion sensor ignored; skipped without a writable `/dev/uinput` (CI enables it) |
| `tst_backupservice` (security) | backup manifest hashes, restore round trip with `.pre-restore-*`, tampered file refused with nothing changed, `../../etc/passwd` entry refused, saves only on request, listing order, multi-file rollback |
| `tst_updateservice` (security) | update state machine with a freshly signed 3 MiB bundle: happy path, wrong board, not newer, too old, tampered image, cancel during hashing, no FIFO reader, real FIFO and status file → `RestartRequired`, state directory created by `verify()` and no room needed for the image, stale status ignored, fresh status with the same text accepted, `installing` then `ok`, `ok` for another version fails, install timeout, acknowledging a rollback or a completed update, transition table; writes the fixture bundles for the shell tests |
| `test_update_tool` (security) | `nexora-update-tool verify` accepts the good bundle, rejects the tampered one |
| `test_release_scripts` (security) | `sign-update.sh` / `verify-release.sh` with a throwaway key: bundle and `SHA256SUMS` signing, verification, tamper detection, key inside a work tree refused; `check-docs.sh` on a scratch tree (missing doc, broken inline and reference links); `check-image-version.sh` on scratch ext4 images (skipped without `mkfs.ext4`/`debugfs`) and that `make-release.sh` calls it |
| `tst_console_qml` | Qt Quick Test files in `tests/qml/`: `tst_theme.qml` (scale, high contrast, reduced motion tokens), `tst_settings_ui.qml` (seven sections, focus stays inside, ConfirmDialog defaults to Cancel), `tst_navigation.qml` (eight tabs with keys only, fit at TV resolutions and large scale, grid/list), `tst_gamedetails.qml` (open/close restores focus, favorite, remove dialog defaults to Cancel and says the file stays, title editing with the on-screen keyboard, unknown game error state, markup in titles and notes shown literally, a failed save keeps the editor open), `tst_quickmenu.qml` (hidden without a game, opens over a running game, pause/resume, "Return to game" resumes a paused emulator, volume and display page, Exit game asks for confirmation); `tst_theme.qml` also covers the resolution factor and the readable high-contrast primary button, `tst_settings_ui.qml` the eliding plain-text value — 31 test functions |

### System (3)

| Test | Covers |
| --- | --- |
| `test_update_helper` | `nexora-updated --once` and `nexora-bootok` in their test modes on 16 MiB scratch slot files and a scratch `grubenv` (each helper run bounded in time): install into B and into A, tampered bundle (slot untouched), not newer, image larger than slot, rejected request lines (`..`, bad characters, wrong verb), symlinked image or manifest rejected, image swapped for a file or a FIFO after verification (the verified image is written), install refused while a trial boot is pending, grubenv replaced via a validated copy and backed up to `grubenv.bak`, `nexora_env_recreate` and `nexora_mask_secrets`; confirmation after a healthy boot, no confirmation with an unhealthy UI or a UI restarted during the window, tries used up → "rolled-back", previous slot picked by hand → trial cancelled, nothing pending → no change. GRUB itself is not executed; its decrement is simulated with `grub-editenv`. |
| `grub_cfg_syntax` | `grub-script-check` on `buildroot-external/board/nexora-x86_64/grub.cfg` |
| `shellcheck_image_scripts` | `shellcheck -s sh` on every init script and helper in the root filesystem overlay and the board scripts |

## Fake emulator

`nexora-fake-emulator` (`tests/fixtures/fake-emulator/main.cpp`) is a Qt-free program. Options (applied in a fixed order): `--startup-delay-ms N`, `--ignore-sigterm`, always prints `ready`, `--print-cwd`, `--echo-args`, `--stdout TEXT`, `--stderr TEXT`, `--sleep-ms N` (≤ 10 min), `--crash segv|abort`, `--exit-code N`, `--spawn-child` (forks a child that sleeps for `--sleep-ms` and prints `child: <pid>`, to test process-group signalling). Other arguments (a game path) are ignored; unknown `--` options exit 64. Tests get its path through `NEXORA_FAKE_EMULATOR`; the build also generates the `fake-ok`, `fake-fail`, `fake-crash`, `fake-long` development configurations.

## Test environment

`nexora_add_test()` sets `QT_QPA_PLATFORM=offscreen`, `QT_QUICK_BACKEND=software`, `QT_LOGGING_RULES=nexora.*.debug=true`, a 120 s timeout (300 s for `tst_qmlsmoke`) and the labels on every test, so a plain `ctest` behaves like the presets. Tests use `QTemporaryDir` and `DataLayout::underRoot()` for all files; they never touch your real `~/.config/nexora` or `~/.local/share/nexora`. Update tests generate keys with `EVP_PKEY_Q_keygen` and never write a key into the source tree; `tst_updateservice` is the CTest fixture (`nexora_update_tool_fixture`) that writes the signed bundles used by `test_update_tool` and `test_update_helper`.

## Tests skipped as root

| Test function | Skipped because |
| --- | --- |
| `tst_configloader::reportsUnreadableFiles` | root can read a `chmod 000` file |
| `tst_libraryscanner::skipsUnreadableEntries` | root can read unreadable entries |

`tst_evdevhotplug` is skipped as a whole (it passes) wherever `/dev/uinput` is not writable, e.g. in containers.

They pass overall when skipped. GitHub-hosted runners run as a non-root user and execute them; the 2026-10-16 container runs did not.

## Adding a test

1. Create `tests/unit/tst_<name>.cpp` (or `integration/`) ending with `QTEST_MAIN`/`QTEST_GUILESS_MAIN` and `#include "tst_<name>.moc"`; a QML test is a `tests/qml/tst_<name>.qml` file picked up by `tst_console_qml`.
2. Register it: `nexora_add_test(tst_<name> SOURCES tst_<name>.cpp LIBS <libs> LABELS unit [security] [GUI] [FAKE_EMULATOR])`. Add `security` for tests of path validation, redaction or signature/checksum rejection.
3. Use temporary directories, `QTRY_*` with timeouts for asynchronous behaviour, no real devices, emulators, display or home directory; guard permission tests with a root check.
4. It must build with `-Werror` under GCC and Clang and pass `clang-format-18`.

## Continuous integration

`.github/workflows/ci.yml` (push, pull request, manual):

| Job | Steps |
| --- | --- |
| `lint` | `clang-format-18 --dry-run --Werror` on tracked C++ (outside `prototypes/`), JSON syntax, `scripts/shellcheck.sh`, `bash -n` on scripts, `scripts/release/check-docs.sh` |
| `build-test` × 3 (GCC Debug `-Werror`, Clang Debug `-Werror`, GCC Release) | configure, build, `ctest -L unit`, `-L integration`, `-L security`, `-L system` (no error if none registered), `scripts/qmllint.sh` (GCC Debug), dashboard screenshots artifact (GCC Debug), `--check-config` for both config directories; `LastTest.log` uploaded on failure |
| `build-sdl3` | builds SDL 3.2.30 from source, configures with `-DNEXORA_WITH_SDL3=ON`, builds and runs all tests |

`.github/workflows/buildroot.yml`: `validate` (Buildroot 2026.02.3 `check-package` on the external tree and `check-defconfig.sh`) on changes under `buildroot-external/`; `image` (full image build) only on manual dispatch, never run on GitHub so far. `.github/workflows/release.yml`: see [release-process.md](release-process.md).

## Manual and system tests

| Area | How | Status |
| --- | --- | --- |
| Image build | `scripts/image/build-image.sh` (Buildroot 2026.02.3 into `~/nexora-br`) | **not yet built** <!-- TASK12-RESULT --> |
| QEMU boot | `scripts/image/qemu-boot-test.sh [disk.img]`: boots a copy with OVMF (KVM if `/dev/kvm` is writable, else TCG), `virtio-vga`, no display, serial to `qemu-serial.log`; requires `Starting nexora: OK` within `NEXORA_QEMU_TIMEOUT` (300 s), then watches `NEXORA_QEMU_SETTLE` (60 s) more and fails if `nexora-session` gives up or restarts the UI `NEXORA_QEMU_MAX_RESTARTS` (2) times or more (both reach the serial log via `/dev/kmsg`), then `system_powerdown` through the monitor (needs `socat`) | **not yet booted** <!-- TASK12-RESULT --> |
| A/B update and rollback in QEMU | install a bundle signed with a throwaway key that the test image trusts; observe the trial boot, confirmation, and a forced rollback | not executed |
| Controller hardware | [checklist](controller-support.md#manual-test-checklist) | not executed |
| Real emulators | definition with `extensions`, `systems`, `{file}`; launch, quick menu, exit | not executed |
| Display and audio on hardware | full screen on KMS, volume and mute via `amixer` | not executed |

## Performance measurements

Only one measurement exists, from MVP 2 (Release build, 4-core container, synthetic 10 000-file library, `NEXORA_LIBRARY_BENCH=1 tst_gamelibrary benchmarkEventLoopStall`): scan + merge 0.16–0.25 s on the worker thread, longest UI event-loop stall 5–6 ms (the timer resolution). Startup time, memory use over long sessions, frame times and crash frequency **have not been measured**, on any hardware.

## Not covered

- Real controllers, real emulators, real GPUs, audio and displays.
- Booting the image; GRUB's boot counting (only syntax-checked); the root helpers on a real block device.
- Screen readers and on-screen rendering beyond the offscreen software renderer.
- Large real game collections (limits are tested with fixtures).
