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
- Every QML `Text` item renders plain text (`textFormat: Text.PlainText`), so a file name, edited title, note, release note or update version that looks like HTML is shown literally, never as styled text or a link (fixed during the MVP 4 verification pass; tested for a scanned file name with HTML tags on the Home and Games tiles and the details heading in `tst_qmlsmoke::markupInFileNameIsShownLiterally`, for edited titles and notes in `tst_gamedetails.qml::test_markupInTitleIsShownLiterally` and for settings rows in `tst_settings_ui.qml`).

### 3. Malicious emulator definition file

Emulator definitions name an executable and its arguments, so whoever can write the definition directory can run code as the `nexora` user.

- On the image the directory is `/usr/share/nexora/emulators` inside the root-owned, read-only-to-users root filesystem; the UI cannot change it.
- `ConfigLoader` (`shared/src/ConfigLoader.cpp`) rejects files over 256 KiB, invalid ids, non-string arguments, unknown system ids, and relative executables that are not anchored to the definition file; executables are never looked up in `PATH`. Arguments are passed verbatim with no shell expansion; `{file}` is the only placeholder. Tested: `tst_configloader`, `tst_launchprofiles`, `tst_launchvalidation`.
- The executable is re-validated (absolute, regular file, executable) before every launch; the health check only `stat()`s it and never runs it (`apps/emulator-manager/src/EmulatorHealth.cpp`, `tst_emulatorhealth`).

### 4. Tampered update bundle

Covered in detail in [update-system.md](update-system.md). In short: signature over exact manifest bytes, strict manifest schema (unknown keys rejected, image file name restricted to `^[A-Za-z0-9._-]+$`, no `/`, no `..`), size + SHA-256 of the image, and a second, independent verification by the root helper on a private copy of the manifest and a single open descriptor of the image (symlinks rejected), followed by a re-hash of the written slot. Tested: `tst_updatemanifest`, `tst_signatureverifier`, `tst_updateservice`, `test_update_tool` (all `security`), `test_update_helper` (label `system`, helper against scratch files).

### 5. Compromised UI process

Assume an attacker controls `nexora-console-ui` (or an emulator, which runs as the same user). What do the root helpers still enforce?

| Channel | What the attacker can do | What they cannot do |
| --- | --- | --- |
| `/run/nexora/power-request` | power off or reboot | anything else: only the exact lines `poweroff` and `reboot` are accepted (`nexora-powerd`) |
| `/run/nexora/update-request` | ask for a bundle at a path to be installed | install anything not signed by an installed key, not newer than the running version, or for another board; pass a path with `..` or characters outside `[A-Za-z0-9._/ -]`; make the helper execute bundle content; swap the image after it was verified (it is read through one open descriptor); block the helper with a FIFO in place of a bundle file (bounded reads with a watchdog); overwrite the only known-good slot during a trial boot (refused) (`nexora-updated`) |
| `grubenv`, root slots, ESP | nothing directly: the ESP is mounted root-only, the slot block devices are root-only | change the boot slot or the trial counter |
| `last-update.json` (user state) | forge the "update completed / rolled back" note shown by its own UI | affect boot behaviour (the helpers only write it) |
| `/run/nexora/update-status` | nothing: it is `root:nexora 0640`, readable but not writable | fake an install result; the UI also accepts only a status written after its request that names the pending version |
| User data | read, change or delete everything under `/home/nexora` | — (same user; see limitations) |

A flood of bad requests is throttled (1 s pause after each invalid request in both helpers).

## Mitigations

### Least privilege

- The UI refuses to run as root unless `--allow-root` / `NEXORA_ALLOW_ROOT=1` (`apps/console-ui/src/main.cpp`, exit code 2).
- On the image the supervisor starts it as `nexora` with `start-stop-daemon --chuid` (`rootfs-overlay/usr/libexec/nexora/nexora-session`); the user is in `input`, `video`, `render`, `audio` and `nexora` only (`board/nexora-x86_64/users.table`).
- Privileged work happens in two root shell helpers that each accept one fixed-format line over a FIFO (`nexora-powerd`, `nexora-updated`); QML has no generic "run command" or "write file" entry point. All system operations go through C++ controllers (`apps/console-ui/src/*Controller.cpp`).

### No shell, explicit arguments

- Every child process is started with `QProcess` and an argument list: emulators (`EmulatorManager`), `amixer` (`apps/system/src/SystemIntegration.cpp`). Nexora never builds a shell command string from metadata or user input.
- Each emulator runs in its own process group (`setpgid(0, 0)` between fork and exec); stop, kill, pause and resume signal the whole group, so wrapper scripts and helper processes an emulator spawns are stopped with it (a paused group gets SIGCONT before SIGTERM; after a requested stop anything left in the group gets SIGKILL). The direct child also gets `PR_SET_PDEATHSIG(SIGTERM)`, so it does not outlive a crashed UI (`apps/emulator-manager/src/EmulatorManager.cpp`, `tst_emulatormanager::stopTerminatesGrandchildren`).

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
| Emulator control-socket answers | one line ≤ 4096 bytes, one request at a time, timeout; a state file must be a non-empty regular file inside the session directory (no symlinks, no paths outside) | `apps/emulator-manager/src/SaveStateAdapter.cpp`, `tst_savestateadapter` |
| Quick Resume `meta.json` | 64 KiB, strict schema (version, id = directory name, file names `^[A-Za-z0-9._-]{1,128}$`, SHA-256 length), unreadable files moved aside; the state's SHA-256 is checked before every resume | `apps/console-ui/src/GameSessionManager.cpp`, `tst_gamesessionmanager` |
| Notification history | 256 KiB, versioned, broken file moved aside | `apps/console-ui/src/NotificationService.cpp`, `tst_notificationservice` |

### Signed updates

- Ed25519 verification with OpenSSL 3 (`apps/update/src/SignatureVerifier.cpp`); zero keys means every bundle is rejected (fail closed). Tested: `tst_signatureverifier::failsClosedWithZeroKeys`.
- No private key is in the repository: tests generate throwaway keys at run time; `buildroot-external/board/nexora-x86_64/update-keys/` contains only a README and a `.gitignore`; `scripts/release/sign-update.sh` refuses a key that lives inside a git work tree (tested by `test_release_scripts`).
- The release workflow (`.github/workflows/release.yml`) fails when the signing secret is missing, so there is no unsigned release.

### Data safety

- Atomic writes with `QSaveFile` and mode 0600 for every file the application owns (`shared/src/AtomicFile.cpp`, `tst_atomicfile`); user data directories are created with mode 0700 (`shared/src/DataLayout.cpp`, `tst_datalayout`).
- Corrupt files are moved aside (`*.corrupt-<timestamp>`), never silently overwritten; schema migrations back up the database first (`tst_librarymigration`).
- "Remove from library" hides the entry; Nexora never deletes a game file (`GameLibrary::removeFromLibrary`, tested in `tst_gamelibrary`).
- Quick Resume never deletes a state because of a failure or a limit: only an explicit Remove (confirmed) or a newer verified state of the same session (newest two kept) removes one ([quick-resume.md](quick-resume.md#limits-and-removal)).
- "Clear artwork cache" removes only the cache directory (`DataLayout::cacheOnlyDirs()`), never saves or data (`tst_datalayout`).

### Privacy and redaction

- No telemetry and no network code: the application links no Qt Network module and opens no sockets; the image runs no network services.
- Diagnostic reports and the launch history pass every line through `shared::redact()` (`shared/src/Redaction.cpp`):
  - the home directory becomes `~` (whole path components only), and `user:pass@` is stripped from URLs;
  - values of keys whose name contains `password`, `passwd`, `passphrase`, `token`, `secret`, `credential`, `apikey` or `cookie`, or has one of the words `key`, `keys`, `pass`, `pwd`, `auth`, `authorization`, `session`, `sessionid`, `sid` or `bearer` (split at non-letters and camelCase humps, so `api_key`, `apiKey`, `session_id` and `--auth` match but `monkey`, `author`, `bypass` and `sessions` do not) become `***` in `k=v`, `k: v`, JSON `"k": "v"`, `--flag=v` and `--flag v` forms (quoted flag values keep their quotes);
  - the whole value of `Authorization`, `Proxy-Authorization`, `Cookie` and `Set-Cookie` header lines, and `Bearer <token>` anywhere, become `***`.

  This is pattern-based: a secret in a form it does not recognise (for example a bare token with no key name) is not masked. Reports are capped at 512 KiB, written 0600, newest 10 kept (`apps/system/src/DiagnosticReport.cpp`). Tested: `tst_redaction` (incl. the extended key names, command-line flags and headers), `tst_diagnosticreport` (both `security`).
- The recovery menu's diagnostics export runs the application's redacting export first; its own boot-state file and the appended tail of the session and UI logs go through `nexora_mask_secrets` (`nexora-functions`), a `sed` filter for password/token/secret/key/auth/cookie/session-like `k=v`/`k: v` values, URL credentials and bearer tokens, plus `/home/nexora` → `~` ([recovery.md](recovery.md#recovery-menu)). It is coarser than `shared::redact()`.
- `gameDetails()` returns the game path redacted (`apps/console-ui/src/LibraryController.cpp`, tested in `tst_uicontext`).

### Root helper hardening

`nexora-updated`, `nexora-bootok` and the recovery menu (`rootfs-overlay/usr/libexec/nexora/`):

- **No install during a trial boot.** `nexora-updated` refuses a request while `nexora_try` is set or an update record exists and the running slot is not the confirmed one, so the only known-good slot is never overwritten.
- **One descriptor for the image.** The image must be a regular file and not a symlink; it is opened once (fd 4) and the verifier and `dd` read it through `/proc/self/fd/4`, so swapping files in the user-writable bundle directory after verification has no effect. The written slot is re-hashed anyway.
- **Bounded reads.** Every read of a requester-controlled file is bounded in time (open 10 s via a watchdog, manifest copies 10 s, verification 15 min, writing 30 min, re-hash 15 min; `nexora_run_bounded`), so a FIFO or a stalled file system cannot hang the daemon.
- **Validated grubenv updates.** `grubenv` is replaced via a temporary copy that is checked to be a complete 1024-byte environment block, then renamed, and `grubenv.bak` is refreshed; GRUB and the helpers fall back to `grubenv.bak` if `grubenv` is damaged; the recovery menu can recreate both ("Repair boot environment").
- **Stricter boot confirmation.** `nexora-bootok` confirms a trial slot only if the UI stayed up for the whole 45 s window, with an unchanged UI start counter and supervisor PID.

Tested on the build host against scratch files only (`test_update_helper`).

### Removable media and disappearing mounts

- A library root that is missing or unreadable keeps its entries instead of marking them missing; launching a vanished file fails with "Game file is missing" and the UI stays usable (`tst_gamelibrary`, `tst_uicontext`).
- The update panel only lists `nexora-update-*` directories at a fixed depth under `/media`, `/run/media` and `$HOME`, without following symlinks (`apps/console-ui/src/UpdateController.cpp`).
- `S01nexora-data` never blocks the boot for more than 10 s waiting for the data partition and mounts it read-only if `e2fsck` cannot finish or reports uncorrected errors.

## Known limitations

- **No Secure Boot.** GRUB and the kernel are unsigned; Secure Boot must be disabled to boot the image. Anyone who can change the disk can change the system.
- **No disk encryption.** User data, saves and backups are stored in plain ext4.
- **Development root access.** The image's root password is empty and logins on ttyS0 and tty2 are always enabled (by design for recovery); the GRUB menu is not password-protected, so kernel arguments can be edited at boot. Set a root password before distributing an image ([buildroot.md](buildroot.md#start-up-design-and-recovery-access)).
- **Update keys are only as strong as their storage.** Whoever obtains a release private key can sign updates that every image trusting the matching public key will install. Rotation and revocation require shipping an update signed by a still-trusted key; there is no separate revocation list and no hardware-backed key storage.
- **Save-state control channels.** The control-socket adapter's Unix socket lives in the temporary directory with owner-only permissions; any process of the same user can connect first and answer instead of the emulator (it can then only point Nexora at files inside the session directory). The RetroArch adapter's UDP network commands on 127.0.0.1 are unauthenticated: any local process can send RetroArch commands while a game runs (unverified adapter; [emulator-integration.md](emulator-integration.md#retroarch-adapter-unverified)).
- **UI and emulators run as the same user.** A compromised emulator can read and modify all user data and use both FIFOs exactly as the UI can. Emulators are not sandboxed (no namespaces, seccomp or separate users).
- **Logs on the image are volatile.** The supervisor passes `--log-file /var/log/nexora/console-ui.log`, and `/var/log` is tmpfs, so logs are lost on reboot unless a diagnostics report was exported to the data partition first.
- **Same-version or downgrade bundles** are refused, but there is no protection against a validly signed, *newer* bundle being installed by anyone with access to the update FIFO (which includes a compromised UI). That is by design: authorization is "signed by the release key", not "requested by the owner".
- **Redaction is pattern-based.** Secrets in a form the patterns do not recognise are not masked, and the recovery menu's `sed` filter is coarser than the application's redaction; read a report before sharing it.
- **Unverified on hardware.** The root helpers were tested only on the build host against scratch files; GRUB's boot counting and the `grubenv.bak` fallback were only syntax-checked with `grub-script-check`.

## Reporting

There is no security contact or disclosure policy yet; open a GitHub issue without exploit details and ask for a private channel.
