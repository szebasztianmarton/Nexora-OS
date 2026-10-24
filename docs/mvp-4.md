# Nexora OS — MVP 4 status and acceptance report

MVP 4 turns the MVP 2 console UI and the MVP 3 Buildroot tree into a reliable, controller-first, local-only gaming OS. The brief is in [mvp-4-brief.md](mvp-4-brief.md); the task plan is in [superpowers/plans/2026-10-11-mvp4.md](superpowers/plans/2026-10-11-mvp4.md).

> **Summary (2026-10-16, commit `388781b`, version 0.4.0):** the application side is implemented and tested on a Linux host: **41/41** CTest tests pass with GCC 13 and Clang 18 under `-Werror`. The image side (A/B slots, data partition, update helper, boot counting, recovery menu) is implemented and its helpers are tested against scratch files, but **the A/B image has not been built or booted** yet, and **no physical hardware, controller or real emulator has been tested**. Acceptance boxes are ticked only where the evidence column names a test or a command that was run. <!-- TASK12-RESULT: update this summary and the "bootable image" row with the image build and QEMU boot result -->

## Contents

- [Audit (before MVP 4)](#audit-before-mvp-4)
- [What MVP 4 delivered](#what-mvp-4-delivered)
- [Test results](#test-results)
- [Fixed during verification](#fixed-during-verification)
- [Acceptance](#acceptance)
- [Limitations](#limitations)
- [Documentation](#documentation)

## Audit (before MVP 4)

Verified 2026-10-11. Baseline: `feature/mvp4` branched from `main` at `c0aa6c9`. Built in WSL 2 Ubuntu 24.04 (GCC 13, Qt 6.4.2, CMake, Ninja) with `-DNEXORA_WARNINGS_AS_ERRORS=ON`: build succeeded, **19/19 tests passed** (14 unit, 5 integration).

| Area | State |
| --- | --- |
| MVP 1 | Done: Qt 6/QML app, `EmulatorManager` (QProcess, SIGTERM then bounded SIGKILL), logging with rotation, fake emulator, CMake/Ninja, CTest. |
| MVP 2 | Done: dashboard (Home, Games, Emulators, Settings, Diagnostics), SQLite library (schema v1), layered validated settings, evdev and optional SDL3 gamepad backends, remapping, desktop/embedded `SystemIntegration`. |
| MVP 3 | Partial: Buildroot external tree (defconfig, kernel fragment, GRUB EFI, single-root GPT image, BusyBox init scripts, session supervisor with crash backoff and a tty2 recovery login, power-request FIFO helper, `nexora.noui` boot entry). Configuration validated; an image build was recorded on another branch (`73a4df0`) but not on the audited line. |
| Prototype UI | `prototypes/console-ui` is a separate UI (JSON stores, SDL2) not built by the main CMake. Reference only. |

Missing at the start: gaming systems, user metadata, remove-from-library, migration with backup, scan progress/cancel in the UI, emulator health/version/history, hardware information, build identifier, data layout, backup/restore, redacted diagnostics export, verbosity setting, dead zone, device list, input test, quick menu, game details, systems/recent screens, high contrast/scale/reduced motion, an update system, A/B slots with a data partition, a release workflow, 13 of the 18 required documents.

Risks recorded at the start: single `Main.qml` with tabs; user data on the root filesystem (an image update would wipe it); kernel on the shared ESP; Qt 6.4 on the host vs 6.9 in the image.

## What MVP 4 delivered

| Plan task | Delivered | Main evidence |
| --- | --- | --- |
| 1 Shared foundation | `BuildInfo` (version/build id/board), `DataLayout`, `AtomicFile`, `Redaction`, `SystemCatalog` (20 systems), new settings keys, emulator `version`/`systems`/`pausable` | `tst_datalayout`, `tst_atomicfile`, `tst_redaction`, `tst_systemcatalog`, `tst_configloader`, `tst_settings` |
| 2 Library v2 | systems, metadata overrides, hidden entries, v1→v2 migration with backup, scan progress and cancel, system-directory skipping | `tst_librarymigration`, `tst_gamelibrary`, `tst_libraryscanner`, `tst_libraryquery` |
| 3 Emulators | health checks, launch history, startup-failure detection, pause/resume | `tst_emulatorhealth`, `tst_launchhistory`, `tst_emulatormanager` |
| 4 System | hardware info, backup/restore, redacted diagnostics, log level | `tst_hardwareinfo`, `tst_backupservice`, `tst_diagnosticreport`, `tst_logging` |
| 5 Updates | strict manifest, Ed25519 verification, hashing, state machine, `nexora-update-tool` | `tst_version`, `tst_updatemanifest`, `tst_signatureverifier`, `tst_updateservice`, `test_update_tool` |
| 6 Controllers | dead zone, device list, disconnect notice, input test mode, key-repeat filter, Start hold | `tst_gamepadnavigator`, `tst_inputcontroller`, `tst_appcontroller` |
| 7 Controllers in the UI | systems model, game details API, backups/diagnostics/updates/pause exposed, headless CLI actions | `tst_uicontext`, `tst_appcontroller`, `tst_updatecontroller`, `tst_systemlistmodel` |
| 8 Design system | tokens, high contrast, scale, reduced motion, seven settings sections, `ConfirmDialog`, input test panel | `tst_console_qml` (`tst_theme.qml`, `tst_settings_ui.qml`), `tst_qmlsmoke` |
| 9 Navigation and screens | 8 tabs, Systems, Favorites, Recent, game details with on-screen keyboard, quick menu, System tab, update panel | `tst_console_qml` (`tst_navigation.qml`, `tst_gamedetails.qml`, `tst_quickmenu.qml`), `tst_qmlsmoke` |
| 10 Image | A/B root slots, data partition, GRUB boot counting, `nexora-updated`, `nexora-bootok`, recovery menu | `test_update_helper`, `grub_cfg_syntax`, `shellcheck_image_scripts` (host only) |
| 11 CI and release | `security` label, `system` label, qmllint and docs checks in CI, `make-release.sh`, `sign-update.sh`, `verify-release.sh`, `check-docs.sh`, `release.yml` | `test_release_scripts`; `ci.yml`/`release.yml` not observed running on GitHub for this report |
| 12 Image build and QEMU boot | scripts `build-image.sh`, `qemu-boot-test.sh` | **not yet built/booted** <!-- TASK12-RESULT --> |
| 13 Documentation | this document set | `scripts/release/check-docs.sh` |

## Test results

Run on 2026-10-16 in an Ubuntu 24.04 container (4 vCPU, Qt 6.4.2, CMake 3.28.3, Ninja), commit `388781b`:

| Check | Command | Result |
| --- | --- | --- |
| GCC 13.3 Debug `-Werror` | `cmake -S . -B ~/nxbuild/docs -G Ninja -DCMAKE_BUILD_TYPE=Debug -DNEXORA_WARNINGS_AS_ERRORS=ON && cmake --build ~/nxbuild/docs -j2 && QT_QPA_PLATFORM=offscreen ctest -j2` | build clean; **41/41 passed** (unit 27, integration 11, system 3; security 9) |
| Clang 18.1.3 Debug `-Werror` | same with `CC=clang CXX=clang++` in a second build directory | build clean; **41/41 passed** |
| QML lint | `scripts/qmllint.sh ~/nxbuild/docs` | 0 errors, 149 warnings |
| Shell lint | `scripts/shellcheck.sh` | clean (16 sh, 11 bash scripts) |
| Formatting | `clang-format-18 --dry-run --Werror` on all tracked C++ outside `prototypes/` | clean |
| Configuration check | `nexora-console-ui --check-config` for `config/emulators` and `config/examples/emulators` | `OK`, exit 0 |
| Version | `nexora-console-ui --version` | `0.4.0 388781b nexora-x86_64` |
| Headless backup/restore | `--backup`, then change `settings.json`, `--restore <dir>` (scratch XDG directories) | backup with `manifest.json`, `config/settings.json`, `data/library.sqlite`; restore brought the old settings back and left `settings.json.pre-restore-*`; after editing a file inside the backup, restore failed with "checksum mismatch for config/settings.json; the backup is damaged" (exit 1) |
| Headless diagnostics | `--export-diagnostics` | report written, mode 0600, data directories mode 0700, no occurrence of the home path in the report |
| Headless update verification | `NEXORA_UPDATE_TEST=1 --verify-update <signed test bundle>` with the test key in the build's key directory | the good bundle reached `readytoinstall` once the state directory `~/.local/state/nexora/update` existed; the tampered bundle failed with "Update image checksum does not match the manifest". The run without that directory failed the free-space precondition; this was fixed afterwards (`verify()` now creates the directory and needs only ~1 MiB there, see [Fixed during verification](#fixed-during-verification)); the headless run was not repeated after the fix |

After the verification fixes (commit `64b014f`, 2026-10-17) the suite was re-run with a GCC 13.3 Debug build without the `-Werror` option: **41/41 passed** (unit 27, integration 11, system 3; security 9); `scripts/qmllint.sh` 0 errors, 155 warnings; `scripts/shellcheck.sh` clean (16 sh, 12 bash). The `-Werror` GCC/Clang builds, clang-format and the headless runs above were not repeated for that commit.

The container runs as root: the two permission tests that skip themselves as root were not executed ([testing.md](testing.md#tests-skipped-as-root)). No startup time, memory, frame-time or crash-frequency measurements were made ([testing.md](testing.md#performance-measurements)).

## Fixed during verification

A review of the MVP 4 code after commit `388781b` found the following problems; each was fixed in the code (commits up to `64b014f`) and, where noted in [testing.md](testing.md), covered by new tests. Image-side fixes were tested on the build host only; **nothing has been booted**.

| Area | Finding | Fix |
| --- | --- | --- |
| Update verification | `verify()` required free space for a full copy of the image (image size + 64 MiB) in the state directory, which is never used, and failed when the directory did not exist | only ~1 MiB is required for the state files, and the directory is created ([update-system.md](update-system.md#verification-steps-and-errors)) |
| Update result | `last-update.json` was never read at startup, and a completed update was forgotten after being shown once | read at startup; "Update installed" / "Update rolled back" stays on the Home card and in the panel until the user presses OK |
| Install status | a stale `ok <version>` from an earlier install or boot confirmation could be taken as the answer; no timeout | only a status written after the request that names the pending version counts; 15 min timeout (`InstallTimedOut`) |
| Install confirmation text | claimed that Nexora restarts by itself | says to choose Restart now |
| `SystemIntegration` | reported that in-place updates were not implemented on the image | reports the A/B update helper truthfully |
| `--verify-update` | gave up after 30 s, too short for a 2 GiB image on USB | waits for the result (bounded at 2 h) |
| `nexora-updated` | could overwrite the only known-good slot during a trial boot; read the image through a path that could be swapped after verification; followed symlinks; requester-controlled reads could block | refuses during a trial boot; opens the image once on a descriptor; rejects symlinks; bounded reads with a watchdog |
| grubenv | rewritten in place on vfat, so a power cut could leave it invalid and break every later update | validated temporary copy + rename, `grubenv.bak` fallback in `grub.cfg` and the helpers, recovery menu option "Repair boot environment" |
| `nexora-bootok` | confirmed a trial slot if the UI happened to be running after 45 s, even after crashes and restarts | requires an unchanged UI start counter and supervisor PID for the whole window |
| Recovery diagnostics | raw, unredacted logs | uses the application's redacting export; its own file is masked |
| QEMU boot test | passed as soon as the session started, even if the UI then crash-looped | settle window and restart limit; `nexora-session` logs restarts to `/dev/kmsg` |
| Release | `make-release.sh` could package a stale Buildroot image under a newer version; `/etc/issue` said 0.2.0 | `check-image-version.sh` refuses a mismatch; `/etc/issue` says 0.4.0 |
| Emulator stop/pause | signals reached only the direct child; helpers and wrapper children survived | the whole process group is signalled; SIGCONT before SIGTERM for a paused group |
| Redaction | missed `apikey`, `passphrase`, `auth`, `credential`, `session`, `cookie` keys, `--flag value` secrets, `Authorization`/`Cookie` headers and Bearer tokens | all of these are masked ([security.md](security.md#privacy-and-redaction)) |
| Rich text | titles, notes and bundle text were rendered with `Text.AutoText`, so markup in a file name was styled | all UI text is plain text |
| Navigation and dialogs | Back inside a page did nothing; stopping a game from the running overlay had no confirmation and Restart/Power off in the game bar only a second press; "Return to game" left a paused emulator paused; modal dialogs leaked some keys and stayed armed when hidden | Back moves to the tab bar; confirmation dialogs (default Cancel); resume on return; modals trap every key and cancel when hidden |
| Default emulator | offered for every system and every emulator, without validation | only for systems with games or compatible emulators, only compatible emulators, validated in `LibraryController` |
| "Other" games | the Systems tab's "Other" card showed every game | new `@other` filter for games without a system |
| Layout at large scale | 1080p was drawn at 720p sizes; cards, tabs, keyboard and editor overflowed at 150 % | `Theme.scale` = user scale × resolution factor; grid falls back to list rows; tabs, keyboard and editor fit |
| Feedback and performance | Settings › System actions failed silently; failed game edits were not reported; the synchronous hardware probe ran on every visit | errors next to the actions and as notices; lazy probe (once per 30 s) |

## Verification matrix

Statuses follow the project's verification guide: **PASS** (direct evidence), **PARTIAL**, **FAIL**, **NOT RUN**. A NOT RUN is never counted as a PASS. Recorded 2026-10-17 on branch `claude/mvp4-remaining`.

| Area | Status | Evidence / reason |
| --- | --- | --- |
| Repository changes | PASS | Diff `2636d9a..HEAD` reviewed by two independent review passes (backend/shell and QML UI); 50 confirmed findings fixed, each with a regression test that failed before the fix (see [Fixed during verification](#fixed-during-verification)). `git diff --check` clean; no TODO/FIXME/"not implemented" in `apps/`, `shared/`, `buildroot-external/`, `scripts/`. |
| Desktop build | PASS | Ubuntu 24.04 container, Qt 6.4.2, `-DNEXORA_WARNINGS_AS_ERRORS=ON`: GCC 13 Debug (`16f3ab3`), GCC 13 Release (`388781b`), Clang 18 Debug (`64b014f`) all build with exit code 0. |
| Automated tests | PASS | `ctest`: 41/41 pass on each build above (27 unit, 11 integration, 3 system; 9 also labelled security). |
| UI runtime | PARTIAL | The real binary starts offscreen with the example emulator configs and prints no QML warnings; every screen, dialog and the quick menu were driven by keys in `tst_qmlsmoke` / QML tests at 1280×720 and 1920×1080, incl. high contrast and 150 % scale, with screenshots inspected. Not run on a real display, EGLFS or the image. |
| Controller | PARTIAL | Mapping, navigation, deadzone, hold/chord logic unit-tested (`tst_gamepadnavigator`, input tests); UI driven by synthesized key events. **Physical controller: NOT RUN** (no device available). |
| Emulator Manager | PASS (fake) | Fake-emulator lifecycle in `tst_emulatormanager`: launch, normal/non-zero exit, crash, missing executable, startup failure, bounded stop with SIGKILL escalation, stop during Starting, whole process group (grandchildren), pause/resume, second launch blocked, argument list (no shell). **Real emulators: NOT RUN.** |
| Game library | PASS | Scanner and library tests on temporary directories with known files (`tst_libraryscanner`, `tst_gamelibrary`, `tst_libraryquery`, integration cancel test): only configured dirs, worker-thread merge, missing/duplicate files, hide-not-delete, `@other` filter. |
| Persistence | PASS (process restart) / NOT RUN (reboot) | `tst_uicontext` restart simulation (settings, library, removed games persist); headless CLI run as a non-root user: `--backup` → modify settings → `--restore` restored the original file; corrupt settings moved aside. Persistence across a reboot of the image: NOT RUN. |
| Buildroot | PARTIAL | `check-defconfig.sh` 0 problems, `check-package` 0 warnings, kernel fragment checked against a generated config, savedefconfig round trip identical; CI "Buildroot" validation green on `16f3ab3`. A full build is in progress in this container (toolchain, host LLVM/Clang, target LLVM and ~80 packages built); the `nexora-os` package itself not yet built for the target. <!-- TASK12-RESULT --> |
| Image generation | NOT RUN | Image build not finished: the container was reclaimed repeatedly while idle and the multi-hour build had to be resumed several times. <!-- TASK12-RESULT --> |
| Boot (VM / hardware) | NOT RUN | No image yet; `scripts/image/qemu-boot-test.sh` ready (UI must start and stay up for a settle window). <!-- TASK12-RESULT --> |
| Recovery / update / rollback | PARTIAL | Helpers tested on the build host against scratch partitions and grubenv, under bash and the target's BusyBox `sh` (`test_update_helper`: install, tamper, trial-boot refusal, symlink/FIFO swaps, torn grubenv repair, bootok confirm/rollback); GRUB script syntax-checked; boot-counting state machine reviewed by hand. On a booted image: NOT RUN. |
| CI | PASS | GitHub Actions "CI" run 73 and "Buildroot" run 12 green on `16f3ab3` (lint, shellcheck, docs check, GCC/Clang `-Werror`, Release, security and system labels, qmllint, screenshots). `release.yml` has never run (needs a tag and the signing secret). |
| Documentation | PASS | `scripts/release/check-docs.sh`: 21 required documents, 0 broken links; docs re-synced with the code after the fixes. Mermaid diagrams not rendered (no renderer). |
| Security | PARTIAL | Targeted review of the update chain, root helpers, redaction, path handling and process launching; findings fixed; 9 `security`-labelled tests pass. Not a full audit; no secure boot or disk encryption (see [security.md](security.md)). |

## Acceptance

Criteria from the brief, §18.

| | Criterion | Evidence / reason |
| --- | --- | --- |
| [x] | The project builds using documented instructions. | [building.md](building.md) commands run on 2026-10-16: GCC and Clang, `-Werror`, clean builds ([Test results](#test-results)). |
| [ ] | Existing MVP functionality remains operational. | Partly verified: all MVP 1/2 tests still pass (e.g. `tst_emulatormanager`, `tst_launchprofiles`, `tst_gamelibrary`, `tst_qmlsmoke` journeys, `tst_systemintegration`). **Not verified:** the MVP 3 boot path, because the image was not rebuilt or booted. |
| [ ] | The gaming shell starts in the intended environment. | **Not verified:** the intended environment is the image, which has not been booted. On the desktop host the real `Main.qml` loads against a real backend in `tst_qmlsmoke` (offscreen). |
| [ ] | Essential navigation works with a supported controller. | **Not verified:** no physical controller tested. Controller-only operation is tested with synthesized actions/keys (`tst_navigation.qml::test_eightTabsWithKeysOnly`, `tst_qmlsmoke::allSectionsWithKeysOnly`, `tst_gamepadnavigator`, `tst_inputcontroller`). Search and library-folder entry still need a keyboard. |
| [x] | Games can be imported from approved local directories. | `tst_gamelibrary::scansAsynchronouslyAndPersists`, `tst_libraryscanner` (only configured roots, system directories refused), `tst_uicontext::launchGameFromLibrary`. |
| [x] | Library metadata persists across restarts. | `tst_uicontext::settingsPersistAcrossRestarts`, `tst_uicontext::removedGamePersistsAcrossRestart`, `tst_gamelibrary::userMetadataSurvivesRescans`, `tst_librarymigration`. |
| [x] | Duplicate and missing game entries are handled safely. | `tst_gamelibrary::overlappingRootsYieldOneEntry`, `tst_libraryscanner::detectsDuplicates`, `tst_gamelibrary::rescanKeepsUserDataAndTracksMissingFiles`, `unavailableRootKeepsEntries`, `cancelledScanDoesNotMarkEntriesMissing`, `tst_uicontext::missingGameFileAndRecovery`. |
| [x] | A mock emulator can be launched and terminated through the process manager. | `tst_emulatormanager::controlledStop`, `stopEscalatesToKill`, `stopWhilePausedTerminates`, `tst_appcontroller::launchAndReturn`. |
| [x] | The shell returns to a valid state after emulator exit. | `tst_appcontroller::launchAndReturn`, `suspendsControllerInputWhileRunning`, `tst_qmlsmoke::gamesSearchLaunchAndFavorite` (focus returns to the game), `tst_quickmenu.qml::test_opensOverARunningGameAndReturns`. |
| [x] | Emulator failures are reported clearly. | `tst_appcontroller::failureIsReportedAndRecoverable`, `tst_emulatormanager::immediateNonZeroExitIsStartupFailure`, `immediateCrashIsStartupFailure`, `missingExecutable`, `failedToStart`, `tst_emulatorhealth`. |
| [x] | User settings persist across restarts. | `tst_settings` (persistence across instances), `tst_uicontext::settingsPersistAcrossRestarts`, `restoreBackupReappliesLiveSettingsNotJustTheStore`. |
| [x] | Logs and diagnostics are available. | `tst_logging` (rotation, level filter), `tst_diagnosticreport`, `--export-diagnostics` run ([Test results](#test-results)); System tab shows log and launch history (`tst_qmlsmoke`). |
| [x] | Backup and restore procedures are tested. | `tst_backupservice`, `tst_uicontext::restoreBackupRoundTripsThroughTheController`, headless `--backup`/`--restore` run including a tampered backup. The recovery-menu restore path was **not** tested (image not booted). |
| [x] | Update artifacts are authenticated and integrity-checked. | Application and host side: `tst_signatureverifier`, `tst_updatemanifest`, `tst_updateservice`, `test_update_tool`, root helper re-verification in `test_update_helper` (scratch files), release checksums/signatures in `test_release_scripts`, `--verify-update` run. **Not verified on the image.** |
| [x] | Unsupported update or rollback scenarios are explicitly documented. | [update-system.md](update-system.md#supported-and-unsupported-scenarios), [recovery.md](recovery.md#limitations). |
| [x] | CI tests pass in the supported build environment. | The full CTest suite (the same labels CI runs) passes in Ubuntu 24.04 with GCC and Clang, plus qmllint, shellcheck, clang-format and `--check-config`. A GitHub Actions run of commit `388781b` was not checked for this report. |
| [ ] | A bootable image is generated and tested where the build environment permits. | **Not yet built/booted.** <!-- TASK12-RESULT: tick with the build and qemu-boot-test.sh evidence, or record the failure verbatim --> |
| [x] | Installation, compatibility, recovery, and troubleshooting documentation is complete. | [installation.md](installation.md), [hardware-compatibility.md](hardware-compatibility.md), [recovery.md](recovery.md), [troubleshooting.md](troubleshooting.md); `scripts/release/check-docs.sh` passes. Installation and recovery describe the designed procedure and say that it is unverified. |
| [ ] | No critical release-blocking defects remain. | **Not verified:** a review of the application and image code found the defects listed under [Fixed during verification](#fixed-during-verification), which were fixed and tested on the host; but the image has not been booted and the end-to-end UI update flow has not run on the image, so defects that only show there are unknown. |

Result: 14 of 19 criteria met with evidence; MVP 4 is **not complete** until the image is built and booted and the remaining items are verified.

## Limitations

- **Image unverified:** the A/B image has not been built or booted; GRUB boot counting was only syntax-checked; the root helpers ran only against scratch files; the end-to-end update and rollback have not been executed. An earlier single-root image boot (commit `73a4df0`) predates MVP 4.
- **No hardware tested:** no physical PC, GPU, controller or audio device ([hardware-compatibility.md](hardware-compatibility.md)).
- **No real emulator** tested or packaged; the image ships only `example.json` (`/bin/true`).
- **Controller-first gaps:** search and adding library folders need a keyboard; GRUB and the recovery menu need a keyboard.
- **Getting content onto the image** needs a root shell (no automounter, no network, no file manager); hand-added emulator definitions live on the root slot and do not survive a slot switch.
- **Logs on the image are volatile** (tmpfs); export diagnostics to keep them.
- **Updates:** local bundles only, no downgrade, no automatic rollback after a slot was confirmed, ESP/GRUB never updated, fixed 2 GiB slots and 1 GiB data partition.
- **Security:** no Secure Boot, no disk encryption, empty root password on the development image, UI and emulators share one user, pattern-based redaction ([security.md](security.md#known-limitations)).
- **Not measured:** startup time, memory over long sessions, frame times, crash frequency.
- **Not localized:** English only.
- **Licensing undecided** at the time of this report; decided afterwards (2026-10-17): GPL-3.0-or-later ([LICENSE](../LICENSE)).

## Documentation

| Document | Content |
| --- | --- |
| [README.md](../README.md) | overview, status, quick start |
| [architecture.md](architecture.md) | components, diagrams, APIs, threads |
| [roadmap.md](roadmap.md) | milestones and next steps |
| [building.md](building.md) | prerequisites, build, options, CLI, conventions ([development.md](development.md) points here) |
| [installation.md](installation.md) | desktop and image installation |
| [hardware-compatibility.md](hardware-compatibility.md) | tested / expected / unsupported |
| [controller-support.md](controller-support.md) | input flow, mapping, dead zone, quick menu, checklist |
| [game-library.md](game-library.md) | scanning, schema v2, systems, details, removal |
| [emulator-integration.md](emulator-integration.md) | definitions, health, lifecycle, history |
| [user-data.md](user-data.md) | layout, atomic writes, migrations, backup/restore |
| [update-system.md](update-system.md) | trust model, bundles, state machines, helper, slots |
| [recovery.md](recovery.md) | boot/service order, recovery menu, rollback, procedures |
| [security.md](security.md) | threat model, mitigations, limitations |
| [testing.md](testing.md) | tests, CI, manual tests |
| [release-process.md](release-process.md) | versions, keys, scripts, checklist |
| [troubleshooting.md](troubleshooting.md) | symptoms and fixes |
| [design-system.md](design-system.md) | tokens, accessibility modes, components |
| [buildroot.md](buildroot.md), [settings.md](settings.md), [game-bar.md](game-bar.md) | image build, settings keys, game bar |
