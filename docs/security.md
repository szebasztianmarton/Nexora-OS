# Security

This document describes the threat model of Nexora OS 0.4.0 (MVP 4), the mitigations that exist in the code (with file references), and the known limitations. Everything marked "tested" is covered by an automated test that runs in CI (the CTest label `security` groups the tests for path traversal, redaction and signature/manifest/checksum rejection). The image-side mitigations (root helpers, GRUB, partition layout) are **implemented but not verified on a booted image**: the A/B image has not been built or booted yet (see [mvp-4.md](mvp-4.md)).

Related: [update-system.md](update-system.md) (signed updates), [recovery.md](recovery.md), [user-data.md](user-data.md), [buildroot.md](buildroot.md#power-requests-poweroffreboot) (power FIFO protocol).

## Contents

- [Assets and trust boundaries](#assets-and-trust-boundaries)
- [Threat model](#threat-model)
- [Mitigations](#mitigations)
- [Known limitations](#known-limitations)
- [Reporting](#reporting)

## Assets and trust boundaries

| Asset | Where | Why it matters |
| --- | --- | --- |
| The running system image | root slots `nexora-rootA` / `nexora-rootB` (ext4, root-owned) | Code that runs as root at boot |
| Boot state | `grubenv` on the EFI system partition (mounted root-only, `umask=0077`) | Decides which slot boots and when to roll back |
| Update trust anchors | `/usr/share/nexora/update-keys/*.pem` inside the root image | Decide which bundles are installable |
| Release signing keys | offline, with the release maintainers; CI secret `NEXORA_RELEASE_SIGNING_KEY` for `SHA256SUMS` | Whoever holds them can produce installable updates |
| User data | `/home/nexora` on `nexora-data` (settings, library, saves, backups, logs) | Must not be lost or silently corrupted |
| Game files | user-chosen library directories | Must never be deleted or modified by Nexora |

Processes on the image (from `buildroot-external/board/nexora-x86_64/rootfs-overlay/`):

| Process | User | Accepts input from |
| --- | --- | --- |
| `nexora-console-ui` | `nexora` (uid 1000) | controllers, keyboard, library files (names only), emulator definitions (root-owned directory), update bundles (as data) |
| emulators | `nexora` (started by the UI) | game files |
| `nexora-session` (supervisor) | root | nothing from the UI (it only runs fixed commands) |
| `nexora-powerd` | root | one line on `/run/nexora/power-request` (`root:nexora 0620`) |
| `nexora-updated` | root | one line on `/run/nexora/update-request` (`root:nexora 0620`) plus the bundle directory it names |
| `nexora-bootok` | root | `grubenv`, `/proc/cmdline`, the pending-update record on the ESP |
| `nexora-recovery` | root | keyboard on tty1 (only when booted with `nexora.recovery`) |

The boundary that matters most is between the unprivileged `nexora` user (UI **and** emulators) and the two root FIFO helpers.

## Threat model

### 1. Local attacker with a USB stick

Someone with physical access plugs in a stick with a crafted update bundle or crafted game files.

- **Crafted update bundle.** Accepted only if `manifest.json.sig` is a valid Ed25519 signature over the exact bytes of `manifest.json` by a key in the image. The manifest is parsed strictly, pins the image by size and SHA-256, must name the right product and board, and must be newer than the installed version. The root helper repeats every check itself, on a private copy of the manifest, and hashes the bytes that actually landed on the inactive slot before arming a trial boot. Nothing from a bundle is executed. See [mitigations](#signed-updates).
- **Crafted game files.** Game files are never opened, parsed or executed by Nexora; only their names and `stat()` data are used. A file name reaches an emulator only as a single argument.
- **Out of scope / not mitigated:** an attacker with physical access and a keyboard can get a root shell on the current development image (empty root password on ttyS0/tty2, editable GRUB menu, no Secure Boot, no disk encryption). See [known limitations](#known-limitations).

### 2. Malicious ROM metadata and file names

File names are attacker-controlled text: titles are derived from them, and they are shown in the UI, logged, and passed to emulators.

- The scanner only lists directories and `stat()`s files (`apps/game-library/src/LibraryScanner.cpp`). It skips hidden entries, symlinks by default (and links that leave the library root when following is enabled), system directories, and bounds depth (16), file count (100 000) and issues (200). Tested: `tst_libraryscanner` (label `security`).
- Paths are stored canonicalised; the game path is passed to the emulator as one `QProcess` argument (`apps/emulator-manager/src/EmulatorManager.cpp`), never through a shell.
- User-edited metadata (title ≤ 200 characters, notes ≤ 2000, no control characters except newline in notes) is validated in `GameDatabase::setMetadata()` (`apps/game-library/src/GameDatabase.cpp`).
- The launch history stores the game **title** only, never its path (`apps/emulator-manager/src/LaunchHistory.cpp`; tested by `tst_launchhistory`).
- Not mitigated: QML `Text` items render titles with the default `Text.AutoText` format, so a file name that looks like rich text is shown as styled text (see [known limitations](#known-limitations)).

### 3. Malicious emulator definition file

Emulator definitions name an executable and its arguments, so whoever can write the definition directory can run code as the `nexora` user.

- On the image the directory is `/usr/share/nexora/emulators` inside the root-owned, read-only-to-users root filesystem; the UI cannot change it.
- `ConfigLoader` (`shared/src/ConfigLoader.cpp`) rejects files over 256 KiB, invalid ids, non-string arguments, unknown system ids, and relative executables that are not anchored to the definition file; executables are never looked up in `PATH`. Arguments are passed verbatim with no shell expansion; `{file}` is the only placeholder. Tested: `tst_configloader`, `tst_launchprofiles`, `tst_launchvalidation`.
- The executable is re-validated (absolute, regular file, executable) before every launch; the health check only `stat()`s it and never runs it (`apps/emulator-manager/src/EmulatorHealth.cpp`, `tst_emulatorhealth`).

### 4. Tampered update bundle

Covered in detail in [update-system.md](update-system.md). In short: signature over exact manifest bytes, strict manifest schema (unknown keys rejected, image file name restricted to `^[A-Za-z0-9._-]+$`, no `/`, no `..`), size + SHA-256 of the image, free-space check, and a second, independent verification by the root helper with a re-hash of the written slot. Tested: `tst_updatemanifest`, `tst_signatureverifier`, `tst_updateservice`, `test_update_tool` (all `security`), `test_update_helper` (label `system`, helper against scratch files).

### 5. Compromised UI process

Assume an attacker controls `nexora-console-ui` (or an emulator, which runs as the same user). What do the root helpers still enforce?

| Channel | What the attacker can do | What they cannot do |
| --- | --- | --- |
| `/run/nexora/power-request` | power off or reboot | anything else: only the exact lines `poweroff` and `reboot` are accepted (`nexora-powerd`) |
| `/run/nexora/update-request` | ask for a bundle at a path to be installed | install anything not signed by an installed key, not newer than the running version, or for another board; pass a path with `..` or characters outside `[A-Za-z0-9._/ -]`; make the helper execute bundle content (`nexora-updated`) |
| `grubenv`, root slots, ESP | nothing directly: the ESP is mounted root-only, the slot block devices are root-only | change the boot slot or the trial counter |
| `last-update.json` (user state) | forge the "update completed / rolled back" note shown by its own UI | affect boot behaviour (the helpers do not read it) |
| User data | read, change or delete everything under `/home/nexora` | — (same user; see limitations) |

A flood of bad requests is throttled (1 s pause after each invalid request in both helpers).

## Mitigations

### Least privilege

- The UI refuses to run as root unless `--allow-root` / `NEXORA_ALLOW_ROOT=1` (`apps/console-ui/src/main.cpp`, exit code 2).
- On the image the supervisor starts it as `nexora` with `start-stop-daemon --chuid` (`rootfs-overlay/usr/libexec/nexora/nexora-session`); the user is in `input`, `video`, `render`, `audio` and `nexora` only (`board/nexora-x86_64/users.table`).
- Privileged work happens in two root shell helpers that each accept one fixed-format line over a FIFO (`nexora-powerd`, `nexora-updated`); QML has no generic "run command" or "write file" entry point. All system operations go through C++ controllers (`apps/console-ui/src/*Controller.cpp`).

### No shell, explicit arguments

- Every child process is started with `QProcess` and an argument list: emulators (`EmulatorManager`), `amixer` (`apps/system/src/SystemIntegration.cpp`). Nexora never builds a shell command string from metadata or user input.
- Emulators get `PR_SET_PDEATHSIG(SIGTERM)`, so they do not outlive the UI.

### Input validation and bounded reads

| Input | Limits | Code / test |
| --- | --- | --- |
| Settings file | 256 KiB, versioned, every key validated, broken file moved aside | `shared/src/Settings.cpp`, `tst_settings` |
| Emulator definitions | 256 KiB, strict schema | `shared/src/ConfigLoader.cpp`, `tst_configloader` |
| System catalog | 256 KiB, ids `^[a-z0-9][a-z0-9_-]{0,31}$`, extensions `^\.[a-z0-9]{1,16}$` | `shared/src/SystemCatalog.cpp`, `tst_systemcatalog` |
| Update manifest | 64 KiB, strict schema, unknown keys rejected | `apps/update/src/UpdateManifest.cpp`, `tst_updatemanifest` |
| Backup manifest | paths must stay inside the backup (no absolute, no `..`, no symlinks, no unlisted files), every SHA-256 verified before anything is touched | `apps/system/src/BackupService.cpp`, `tst_backupservice` (`security`) |
| `/proc`, `/sys` files | each ≤ 1 MiB | `apps/system/src/HardwareInfo.cpp`, `tst_hardwareinfo` |
| Launch history | 50 records, stderr tail ≤ 4 KiB, redacted | `apps/emulator-manager/src/LaunchHistory.cpp`, `tst_launchhistory` |

### Signed updates

- Ed25519 verification with OpenSSL 3 (`apps/update/src/SignatureVerifier.cpp`); zero keys means every bundle is rejected (fail closed). Tested: `tst_signatureverifier::failsClosedWithZeroKeys`.
- No private key is in the repository: tests generate throwaway keys at run time; `buildroot-external/board/nexora-x86_64/update-keys/` contains only a README and a `.gitignore`; `scripts/release/sign-update.sh` refuses a key that lives inside a git work tree (tested by `test_release_scripts`).
- The release workflow (`.github/workflows/release.yml`) fails when the signing secret is missing, so there is no unsigned release.

### Data safety

- Atomic writes with `QSaveFile` and mode 0600 for every file the application owns (`shared/src/AtomicFile.cpp`, `tst_atomicfile`); user data directories are created with mode 0700 (`shared/src/DataLayout.cpp`, `tst_datalayout`).
- Corrupt files are moved aside (`*.corrupt-<timestamp>`), never silently overwritten; schema migrations back up the database first (`tst_librarymigration`).
- "Remove from library" hides the entry; Nexora never deletes a game file (`GameLibrary::removeFromLibrary`, tested in `tst_gamelibrary`).
- "Clear artwork cache" removes only the cache directory (`DataLayout::cacheOnlyDirs()`), never saves or data (`tst_datalayout`).

### Privacy and redaction

- No telemetry and no network code: the application links no Qt Network module and opens no sockets; the image runs no network services.
- Diagnostic reports and the launch history pass every line through `shared::redact()` (`shared/src/Redaction.cpp`): the home directory becomes `~`, `user:pass@` is stripped from URLs, and values of keys named like `password`, `token`, `secret` or `key` become `***`. Reports are capped at 512 KiB, written 0600, newest 10 kept (`apps/system/src/DiagnosticReport.cpp`). Tested: `tst_redaction`, `tst_diagnosticreport` (both `security`).
- `gameDetails()` returns the game path redacted (`apps/console-ui/src/LibraryController.cpp`, tested in `tst_uicontext`).

### Removable media and disappearing mounts

- A library root that is missing or unreadable keeps its entries instead of marking them missing; launching a vanished file fails with "Game file is missing" and the UI stays usable (`tst_gamelibrary`, `tst_uicontext`).
- The update panel only lists `nexora-update-*` directories at a fixed depth under `/media`, `/run/media` and `$HOME`, without following symlinks (`apps/console-ui/src/UpdateController.cpp`).
- `S01nexora-data` never blocks the boot for more than 10 s waiting for the data partition and mounts it read-only if `e2fsck` cannot finish or reports uncorrected errors.

## Known limitations

- **No Secure Boot.** GRUB and the kernel are unsigned; Secure Boot must be disabled to boot the image. Anyone who can change the disk can change the system.
- **No disk encryption.** User data, saves and backups are stored in plain ext4.
- **Development root access.** The image's root password is empty and logins on ttyS0 and tty2 are always enabled (by design for recovery); the GRUB menu is not password-protected, so kernel arguments can be edited at boot. Set a root password before distributing an image ([buildroot.md](buildroot.md#start-up-design-and-recovery-access)).
- **Update keys are only as strong as their storage.** Whoever obtains a release private key can sign updates that every image trusting the matching public key will install. Rotation and revocation require shipping an update signed by a still-trusted key; there is no separate revocation list and no hardware-backed key storage.
- **UI and emulators run as the same user.** A compromised emulator can read and modify all user data and use both FIFOs exactly as the UI can. Emulators are not sandboxed (no namespaces, seccomp or separate users).
- **Rich text in titles.** QML `Text` elements use the default `Text.AutoText` format; a game file name that Qt's heuristic treats as rich text (for example one containing HTML tags) is rendered styled. No network is involved, but display spoofing of a title is possible.
- **Logs on the image are volatile.** The supervisor passes `--log-file /var/log/nexora/console-ui.log`, and `/var/log` is tmpfs, so logs are lost on reboot unless a diagnostics report was exported to the data partition first.
- **Same-version or downgrade bundles** are refused, but there is no protection against a validly signed, *newer* bundle being installed by anyone with access to the update FIFO (which includes a compromised UI). That is by design: authorization is "signed by the release key", not "requested by the owner".
- **Unverified on hardware.** The root helpers were tested only on the build host against scratch files; GRUB's boot counting was only syntax-checked with `grub-script-check`.

## Reporting

There is no security contact or disclosure policy yet; open a GitHub issue without exploit details and ask for a private channel.
