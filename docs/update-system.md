# Update system

This document covers the part of the update system built in MVP 4 Task 5: a library
(`nexora_update`) that parses and cryptographically verifies a signed update bundle, hashes
its root filesystem image, and tracks the resulting state machine; and a stand-alone CLI
tool (`nexora-update-tool`) that runs the same checks outside any running service.

**What this task does not build**, and what you should not infer from it: the privileged
helper that actually installs a verified bundle (switches the A/B root filesystem slot,
sets up GRUB boot counting, and reports back over `installStatusFile`), and any UI wiring
in `apps/console-ui`. `UpdateService` only *requests* an install by writing one line to a
FIFO; it has no way to perform the install itself, and never runs as root. That helper is
Buildroot update-helper work for a later task. Nothing in this document about installing,
rebooting, A/B slots or rollback has been exercised on real hardware or in a VM — it
describes the protocol `UpdateService` speaks, not a tested end-to-end install. Everything
in the "Manifest parsing", "Signature verification" and "State machine" sections below
*has* been exercised, by the test suite named alongside it.

Code: `apps/update/` (`Version`, `UpdateManifest`, `SignatureVerifier`, `sha256File`,
`UpdateService`), CLI tool in `apps/update/tools/nexora-update-tool.cpp`, reusable test
bundle/key builder in `tests/support/UpdateBundleFixture.h`.

## Contents

- [Trust model](#trust-model)
- [Bundle format](#bundle-format)
- [Manifest parsing](#manifest-parsing)
- [Signature verification](#signature-verification)
- [State machine](#state-machine)
- [The install request protocol](#the-install-request-protocol)
- [last-update.json](#last-updatejson)
- [`nexora-update-tool`](#nexora-update-tool)
- [Limitations / not verified](#limitations--not-verified)

## Trust model

Release maintainers hold one or more Ed25519 private keys **offline**; this repository
never contains one (tests generate a throwaway keypair at run time with
`EVP_PKEY_Q_keygen(nullptr, nullptr, "ED25519")` and discard it afterwards).

The corresponding public keys (PEM, `SubjectPublicKeyInfo` form) are installed read-only in
the image at `/usr/share/nexora/update-keys/*.pem` (dev build tree:
`${PROJECT_BINARY_DIR}/share/nexora/update-keys`; empty by default in both places).

A bundle is accepted only if `manifest.json.sig` is a valid Ed25519 signature, by **any**
one of the installed keys, over the exact bytes of `manifest.json` (not a re-serialised or
re-ordered form of it — the literal file contents, as a canonical reading of the manifest
would otherwise open a signature-malleability gap). The manifest in turn pins the root
filesystem image by exact size and SHA-256.

- **Key rotation**: ship the new public key as a `*.pem` file inside an update that is
  itself signed by a still-trusted (old) key.
- **Revocation**: ship an update, signed by a still-trusted key, that removes the
  compromised key's `.pem` file.
- **No key installed → updates are impossible.** `SignatureVerifier` fails closed: zero
  loaded keys means `verify()` always returns `false`, regardless of what is being
  verified. This is covered by `tst_signatureverifier`'s `failsClosedWithZeroKeys` and
  `fromDirectoryFailsClosedWithNoKeys`.

A key file that exists but is not a valid Ed25519 public key (wrong algorithm, e.g. RSA, or
not a PEM public key at all) is skipped with a reported issue rather than rejecting the
whole directory — see `SignatureVerifier::fromDirectory`.

## Bundle format

A bundle is a plain directory (e.g. the root of a USB stick, or a mounted ISO):

```
nexora-update-0.4.1/
  manifest.json       ≤ 64 KiB
  manifest.json.sig   raw 64-byte Ed25519 signature over manifest.json's exact bytes
  rootfs.ext4          the root filesystem image for one slot
```

`manifest.json`:

```json
{
  "format": 1,
  "product": "nexora-os",
  "board": "nexora-x86_64",
  "version": "0.4.1",
  "buildId": "v0.4.1-0-gabcdef0",
  "minimumVersion": "0.4.0",
  "releasedAt": "2026-10-11T00:00:00Z",
  "image": {"file": "rootfs.ext4", "size": 2147483648, "sha256": "<64 lowercase hex>"},
  "notes": "≤ 4000 chars"
}
```

| Field | Rule |
| --- | --- |
| `format` | Must be exactly `1`. Any other value is rejected outright (this is how a future incompatible format change would be detected by old software, instead of being half-parsed). |
| `product` | Non-empty string; must equal `"nexora-os"` (`UpdateError::WrongProduct` otherwise). |
| `board` | Non-empty string; must equal the running `UpdateEnvironment::board` (`UpdateError::WrongBoard` otherwise). |
| `version`, `minimumVersion` | Strict `major.minor.patch`, see [`Version`](#manifest-parsing). |
| `buildId` | Non-empty string; the git-describe-style identifier shown in diagnostics. |
| `releasedAt` | A valid ISO-8601 date-time (`QDateTime::fromString(..., Qt::ISODate)`). |
| `image.file` | A plain file name: no `/`, no `..`, ≤ 128 characters, matching `^[A-Za-z0-9._-]+$`. |
| `image.size` | Integer, 1 byte to 64 GiB inclusive. |
| `image.sha256` | Exactly 64 **lowercase** hex characters. |
| `notes` | Optional; a string, ≤ 4000 characters. |
| *(top level)* | No keys beyond the ones above are accepted — an unrecognised key rejects the whole manifest, because this document is exactly what the signature is pinned to. |

The manifest is capped at 64 KiB before it is even parsed as JSON.

## Manifest parsing

`nexora::update::Version::parse()` accepts only `^\d{1,4}\.\d{1,4}\.\d{1,4}$` — three
components, no leading `v`, no pre-release/build suffix (`1.2`, `v1.2.3`, `1.2.3-rc1` and
the empty string are all rejected; `01.2.3` is accepted, as leading zeros are still just
digits under that regular expression). Comparison is the default lexicographic
`<=>` over `(major, minor, patch)`.

`nexora::update::UpdateManifest::parse()` implements the table above; see
`tests/unit/tst_updatemanifest.cpp` for the exact set of malformed inputs it rejects
(unknown key, missing `image`, `file: "../x"`, `file: "a/b"`, uppercase/short hash, size 0,
oversize image, `format: 2`, overlong notes, non-object root, input over 64 KiB, an invalid
version string).

## Signature verification

`SignatureVerifier` wraps OpenSSL's one-shot Ed25519 EVP API
(`EVP_DigestVerifyInit`/`EVP_DigestVerify`, no intermediate digest since Ed25519 signs the
whole message internally). A signature must be exactly 64 bytes; it is checked against
every loaded key until one succeeds or all have been tried.

`sha256File()` hashes a file in 1 MiB chunks via the streaming EVP SHA-256 API, reporting
`(bytesDone, totalBytes)` after every chunk and checking a caller-supplied
`std::atomic_bool *cancel` at the same point, so a cancellation lands within one chunk
(≤ 1 MiB) of being requested rather than only after the whole file has been hashed.

## State machine

```
  Idle ──checkBundle (ok)────────────► Checking ──compatible──────► Available
  Idle ──checkBundle (bad/incompat)──► Checking ──bad/incompatible─► Failed
  Available ──verify()──► Verifying ──hash ok──────────► ReadyToInstall
                           Verifying ──cancel/mismatch/low space──► Failed
  ReadyToInstall ──install()──► Installing ──helper "ok"────► RestartRequired
                                 Installing ──helper "error"/no FIFO reader──► Failed
  Available / ReadyToInstall / Failed / Completed / RecoveryRequired ──checkBundle──► Checking
  Idle ──loadPersistedState (new build == running build)──► Completed
  Idle ──loadPersistedState (helper reported "rolled-back")──► RecoveryRequired
```

`Downloading` exists in `UpdateState` and is accepted between `Available` and
`Verifying`/`Failed` in `isValidTransition()`, but nothing in this task produces it — bundle
delivery today is "already a local directory" (a USB stick, a mounted image). It is
reserved so a future network-delivered-bundle path does not need to widen the transition
table. `RestartRequired` is terminal in this process: a real install reboots the machine,
so there is nothing for `UpdateService` to transition to from there.

`checkBundle()` runs synchronously (it only touches `manifest.json` and
`manifest.json.sig`, both small and size-capped) and checks, in order: the manifest parses;
the signature is valid against an installed key (`NoKeys` if the key directory has none
usable, `BadSignature` if no key validates it); `product` and `board` match
(`WrongProduct`/`WrongBoard`); the bundle's `version` is strictly newer than
`UpdateEnvironment::currentVersion` (`NotNewer` — this also catches a same-version bundle);
the running version is not older than `minimumVersion` (`TooOld`); the image file exists
and its size on disk matches the manifest (`ImageMissing`/`SizeMismatch`, a cheap `stat()`
only — this is why a byte-for-byte tamper that preserves the file size is *not* caught
here).

`verify()` first checks that the filesystem holding `stagingDir` has at least
`image.size + 64 MiB` free (`InsufficientStorage`, checked synchronously — no worker thread
is even started if this fails), then hashes the image on a low-priority worker thread
(same pattern as `GameLibrary`'s scan thread) and compares the digest against
`image.sha256`. This is the only point that catches a tamper which did not change the file
size (`HashMismatch`). `cancel()` sets the shared atomic flag checked by `sha256File()`.

`install()` is the one synchronous, privilege-crossing step: it opens
`installRequestFifo` for writing with `O_NONBLOCK`. If nothing is reading the FIFO, `open()`
fails with `ENXIO` and `install()` fails immediately with `InstallRequestFailed` — there is
no retry, because in production this means the privileged helper daemon is not running,
which a retry would not fix. On success it writes exactly one line,
`"install <absolute bundle dir>\n"`, and starts polling `installStatusFile` every 500 ms for
a line starting `"ok "` (→ `RestartRequired`) or `"error "` (→ `Failed`/`InstallFailed`).

## The install request protocol

This is the full extent of what `UpdateService` knows about installing: write one line to
a FIFO, then poll a status file. Everything on the other side of that FIFO — re-verifying
the bundle as root, writing the image to the inactive A/B slot, flipping the GRUB default
entry with boot-count-based fallback, writing `last-update.json`, and rebooting — is a
privileged helper that does not exist yet in this repository. The FIFO and status file
paths (`/run/nexora/update-request`, `/run/nexora/update-status`) are therefore a documented
*contract* for that future helper, not something this task has tested against a real
implementation; `tst_updateservice` only exercises them against a `mkfifo`'d test FIFO and a
plain file written by a test-local reader thread.

## last-update.json

`UpdateService::loadPersistedState()` reads (but never writes)
`<stagingDir>/last-update.json`:

```json
{"previousBuildId": "v0.4.0-0-gcafebabe", "newBuildId": "v0.4.1-0-gabcdef0", "result": "ok"}
```

- `result: "rolled-back"` → `RecoveryRequired`, with a message naming both build IDs. The
  file is left in place.
- `newBuildId` equal to the running build's id → `Completed`, and the file is then removed
  so this is only reported once (on the very next start after the update that produced it).
- Anything else (e.g. `newBuildId` naming neither the previous nor the current build) is a
  no-op; the service stays `Idle`.

This file is written by the privileged helper (Task 10), not by this task — the format
above is this task's best-effort documentation of what `loadPersistedState()` expects, so
it is a good idea, but not yet verified end-to-end, that the helper also adds a `"format"`
field so a future incompatible change to this file is detectable the same way
`manifest.json`'s is.

## `nexora-update-tool`

```
nexora-update-tool verify --keys DIR --bundle DIR [--current-version X --board B]
```

Runs the same signature, size and hash checks as `UpdateService::checkBundle()` +
`verify()`, without a staging directory, a FIFO, or any side effects — useful for CI and
release scripts sanity-checking a bundle before it ships. `--current-version` and `--board`
are optional: without them the tool only answers "is this bundle authentic and intact", not
"is it applicable to this machine" (so a CI check that just signed a bundle for an
arbitrary board doesn't need to fake one up). Prints `ok <version> <sha256>` and exits 0 on
success, or `error <code> <message>` (`<code>` one of the `UpdateError` names) and exits 1
otherwise.

There is deliberately no `sign` subcommand: signing happens with the plain `openssl` CLI in
`scripts/release/sign-update.sh` (Task 10), so a private key is never handled by
Nexora-authored code.

## Limitations / not verified

- No install helper exists yet; `install()`'s FIFO protocol is implemented and tested
  against a fake reader, but **not verified against the real privileged helper**, which is
  Task 10 work.
- No A/B slot switching, no GRUB boot counting, no rollback — none of that is implemented
  in this repository yet. `RecoveryRequired` only means `loadPersistedState()` saw a
  `last-update.json` claiming a rollback happened; this task cannot make a rollback happen.
- No network delivery of bundles; a bundle is always a pre-existing local directory. The
  `Downloading` state is reserved but unused.
- `nexora_update` and `nexora-update-tool` have been built and their test suite run only in
  the WSL Ubuntu 24.04 dev environment described in `docs/development.md`, not on actual
  Nexora hardware or a Buildroot image.
