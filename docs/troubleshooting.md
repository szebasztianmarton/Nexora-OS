# Troubleshooting

Symptoms, likely causes and fixes, for the desktop build and for the image. For recovery procedures on the image (rollback, recovery menu, crash loop) see [recovery.md](recovery.md).

## Contents

- [Collecting information](#collecting-information)
- [Building](#building)
- [Starting the application](#starting-the-application)
- [The glass theme](#the-glass-theme)
- [Controllers](#controllers)
- [Library](#library)
- [Emulators](#emulators)
- [Quick Resume](#quick-resume)
- [Settings and user data](#settings-and-user-data)
- [Updates](#updates)
- [Image boot](#image-boot)

## Collecting information

| What | How |
| --- | --- |
| Version and build | `nexora-console-ui --version`, System tab, `/etc/nexora-release` on the image |
| Diagnostics report (redacted) | System tab › Export diagnostics, or `nexora-console-ui --export-diagnostics`; written to `~/.local/state/nexora/diagnostics/` |
| More log detail | Settings › System › Log detail = debug (`log.level`), or `QT_LOGGING_RULES="nexora.*.debug=true"`; add `--log-file nexora.log` on the desktop |
| Emulator output | `~/.local/state/nexora/emulator-logs/<id>.log` (and `.log.1`), launch history on the System tab |
| Image logs | `/run/nexora-session/session.log`, `/var/log/nexora/console-ui.log`, `/var/log/messages` (all tmpfs); copy them to `/home/nexora` from a root shell before rebooting. Recovery menu option 4 writes the UI's redacted report plus the boot state, and includes these logs (masked) only when the UI's export fails ([recovery.md](recovery.md#recovery-menu)) |
| Configuration check | `nexora-console-ui --check-config [--config-dir DIR] [--settings-file FILE]` |

## Building

| Symptom | Fix |
| --- | --- |
| CMake: `Qt 6.4+ with components [...] was not found` | install the packages in [building.md](building.md#prerequisites) (with tests: also `qml6-module-qttest`), or pass `-DCMAKE_PREFIX_PATH=/path/to/Qt/6.x/gcc_64` |
| CMake: `Could NOT find OpenSSL` | `sudo apt install libssl-dev` (OpenSSL 3 is required for update verification) |
| CMake: OpenGL not found while configuring Qt6Gui | `sudo apt install libgl-dev` |
| CMake refuses to configure in the source directory | in-source builds are rejected; use a preset (`build/<preset>`) or `-B <dir>` |
| `test_update_helper` / `grub_cfg_syntax` / `shellcheck_image_scripts` missing from `ctest -N` | install `grub-common` and `shellcheck`, then re-configure |
| `test_release_scripts` missing | install the `openssl` command, then re-configure |
| `scripts/qmllint.sh`: qmllint not found | `sudo apt install qt6-declarative-dev-tools` |

## Starting the application

| Symptom | Fix |
| --- | --- |
| `module "QtQuick.Controls" is not installed` (or similar) | install the matching `qml6-module-*` package |
| `could not connect to display` | run in a desktop session, or headless with `QT_QPA_PLATFORM=offscreen` |
| `Refusing to run as root` (exit code 2) | run as a regular user, or `--allow-root` / `NEXORA_ALLOW_ROOT=1` |
| `Nexora is already running` (exit code 3) | another instance holds `$XDG_RUNTIME_DIR/nexora-console-ui.lock`; close it |
| "Could not prepare data directories" in the log | a directory under `~/.config/nexora`, `~/.local/share/nexora`, `~/.local/state/nexora` or `~/.cache/nexora` cannot be created or set to mode 0700; check ownership and free space |
| Text too small/large, too much motion | Settings › General: Interface scale (75–150 %), Theme (high contrast), Reduce motion |

## The glass theme

| Symptom | Fix |
| --- | --- |
| Glitches, flicker or a slow UI with the glass theme (the backdrop blur under the bars and dialogs) | set `NEXORA_NO_GLASS_BLUR=1` in the environment that starts the UI (any non-empty value works), or turn on Settings › General › Reduce transparency (solid panels, no blur); choosing the `dark` theme avoids the glass look completely. The blur is also off by itself on the software renderer, on llvmpipe, with a Qt older than 6.5 and while a game runs ([design-system.md](design-system.md#liquid-glass)) |
| No blur at all | expected without a hardware renderer, with Qt older than 6.5, with Reduce transparency on, in high contrast, and while an emulator runs; the glass theme then draws the flat glass |

## Controllers

| Symptom | Fix |
| --- | --- |
| Controller not detected | System tab / Settings › Controllers show backend and status. "permission denied": `sudo usermod -aG input $USER` and log in again. Only devices reporting `BTN_GAMEPAD` are used. |
| Stick drifts or feels sluggish | Settings › Controllers › Analog stick dead zone (5–60 %); check raw values in "Test input" |
| Buttons swapped | remap in Settings › Controllers; see the west/north note in [controller-support.md](controller-support.md#button-names-and-controller-layouts) |
| Stuck in "Test input" | hold B (east) for 2 s, or press Esc |
| Cannot get back from a game | Guide or hold Start 1 s (quick menu › Exit game), or hold Select + Start 1 s (stop) |
| "Controller disconnected" notice during a game | the pad was unplugged or its connection dropped; the game keeps running; reconnect it |
| Typing a library folder or a search with a pad | not possible yet; connect a keyboard (only title/notes editing has an on-screen keyboard) |

## Library

| Symptom | Fix |
| --- | --- |
| No games found | a folder must be configured (Settings › Library), an enabled emulator must declare the files' extension, then "Rescan library"; scan issues are listed on the System tab |
| Scan takes long | the Games tab shows progress; Cancel stops it without changing the library |
| A game disappeared | it may be missing (drive unplugged, file moved) or you removed it: Settings › Library › Restore removed games |
| "Game file is missing" on launch | the file vanished after the last scan; rescan |
| Game has no system / wrong system | extensions like `.iso`, `.chd`, `.zip` match several systems; choose one in the game details (Change system) |
| "Library database unavailable" / "Qt SQLite driver (QSQLITE) is not available" | `sudo apt install libqt6sql6-sqlite` |
| Library disabled, log says the schema is newer | the database was written by a newer Nexora; it is left untouched. Use that version, or restore a backup |
| After upgrading from 0.2: `library.sqlite.v1-backup-*` appeared | expected: the backup taken before the v1 → v2 migration; keep it until you are satisfied, then delete it |

## Emulators

| Symptom | Fix |
| --- | --- |
| Emulator shown as unavailable | executable missing, not a regular file, not executable or not absolute; the Emulators tab shows the reason |
| "failed during startup (exit N)" | the emulator exited or crashed within 3 s; read its stderr tail on the System tab and its output log |
| A definition is ignored | it failed validation; see configuration issues on the System tab or `--check-config` |
| Pause is missing in the quick menu | the definition is not `"pausable": true` (or `capabilities.pause`) |
| Game launches the wrong emulator | per-game choice in the game details, or Settings › Emulators › default emulator per system (a row exists only for systems with games or a compatible emulator, and only emulators that declare the system can be chosen) |

## Quick Resume

| Symptom | Fix |
| --- | --- |
| "Suspend and go Home" is missing | no game session runs (e.g. an emulator started without a game from the Emulators tab) |
| "This emulator has no save-state support" | the definition has no `capabilities.saveState` ([emulator-integration.md](emulator-integration.md#capabilities)); a malformed block is reported as a warning in `--check-config` and ignored |
| "has not connected to Nexora's save-state control yet" | the emulator (or its wrapper) did not connect to the socket passed with `{control-socket}`; check `launchArguments` and the emulator output log |
| "Quick Resume is full" | remove a game from Home › Quick Resume (Select / O), or raise `sessions.maxCount` / `sessions.maxMegabytes` |
| "Could not save … The game is still running." | the emulator refused or did not answer in time; the game was not stopped. Its answer is in the notification history (System tab) |
| "Can't resume …" | the state is kept; the reason names the check that failed ([quick-resume.md](quick-resume.md#resume-checks)). "Start fresh" plays without it |
| "Power request cancelled" | the running game could not be saved before power-off; it keeps running. Save in the game or exit it, then power off again |
| A session disappeared from Home after a restart | its `meta.json` was unreadable and was moved aside (`meta.json.corrupt-*` in `<data>/states/<id>/`); the state files are still there |

## Settings and user data

| Symptom | Fix |
| --- | --- |
| Settings reset unexpectedly | the file was unusable and moved to `settings.json.corrupt-<timestamp>`; see [settings.md](settings.md#validation-and-recovery) |
| Restore refused | the backup failed verification (a changed file, an unlisted file, an unsafe path); nothing was changed. Use another backup |
| Backup fails with "saves are larger than 4 GiB" | back up saves separately or create the backup without saves |
| Volume or mute does nothing | on the desktop the volume is a stored preference only; on the image `amixer` needs a `Master` or `PCM` control |

## Updates

| Message / state | Meaning and fix |
| --- | --- |
| "Updates are installed only on the Nexora OS image" | desktop build; expected |
| No bundles listed | the panel looks for `nexora-update-*` in `/media/*/`, `/run/media/*/*/` and the home directory; the image has no automounter, so copy the bundle to `/home/nexora` or mount the stick by hand ([update-system.md](update-system.md#using-it)) |
| `NoKeys` | the image has no trusted update key: it was built without a public key in `update-keys/`. Every update is refused by design |
| `BadSignature` | the bundle is not signed by a key this image trusts, or `manifest.json` was changed after signing |
| `BadManifest` | malformed or extended manifest (unknown keys are rejected) |
| `WrongBoard`, `WrongProduct` | bundle for another target |
| `NotNewer`, `TooOld` | same/older version, or the running version is below the bundle's `minimumVersion` |
| `ImageMissing`, `SizeMismatch`, `HashMismatch` | incomplete or damaged copy of `rootfs.ext4`; copy the bundle again |
| `InsufficientStorage` | the update state directory `~/.local/state/nexora/update` cannot be created, or the data partition has less than 1 MiB free (the image itself needs no space there); free some space or check that the data partition is mounted read-write |
| `InstallRequestFailed` | the root helper `nexora-updated` is not running (`/etc/init.d/S85nexora-updated start` from a root shell) |
| `InstallFailed` + reason | the helper's own verification or the write failed; see `/var/log/messages` (`nexora-updated`) |
| `InstallFailed: trial boot pending; restart to confirm or roll back first` | the running slot is still on trial after an earlier update; restart so that `nexora-bootok` can confirm it (or GRUB rolls back), then try again |
| `InstallFailed: boot environment (grubenv) not found or damaged` | the ESP is not mounted or both `grubenv` and `grubenv.bak` are damaged; recovery menu option 6 (Repair boot environment) |
| `InstallTimedOut` | the helper did not answer within 15 minutes; restart before trying again and check `/var/log/messages` (`nexora-updated`) |
| After an update the old version is still running | the new slot did not keep the UI up within 3 boots and GRUB rolled back ([recovery.md](recovery.md#failed-update-automatic-rollback)); the Home card says "Update rolled back" and the update panel shows the message until you press OK; `/var/log/messages` (`nexora-bootok`) and the recovery menu's slot view confirm it |

## Image boot

| Symptom | Fix |
| --- | --- |
| PC does not boot the stick/disk | UEFI boot must be enabled, Secure Boot disabled; legacy BIOS is not supported |
| Black screen, then a login prompt on tty2 | the UI crashed 5 times in a row; read `/run/nexora-session/session.log` (early Qt/EGLFS errors are only there) |
| UI never starts, `no DRM device after 10s` in the log | no supported GPU driver loaded (NVIDIA is unsupported); see [hardware-compatibility.md](hardware-compatibility.md) |
| "Data partition: NOT mounted" in the recovery menu | `nexora-data` was not found within 10 s or failed to mount; `/home` is on the root slot and not persistent. Check the disk; run `e2fsck -f /dev/disk/by-partlabel/nexora-data` |
| Settings cannot be saved on the image | the data partition was mounted read-only after an unfinished or failed `e2fsck`; repair it as above |
| UI renders with missing elements in QEMU without KVM | observed with an earlier image under TCG with `-cpu max`; use `-cpu qemu64` |
| Need a shell | ttyS0 / tty2 login, or GRUB "maintenance shell: no console UI" ([recovery.md](recovery.md#get-a-root-shell)) |
