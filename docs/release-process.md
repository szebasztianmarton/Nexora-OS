# Release process

How a Nexora OS release is versioned, built, checked, signed and published, and the checklist to run before calling a version released. The scripts live in `scripts/release/`; the tag-triggered workflow is `.github/workflows/release.yml`.

> **Status:** no release has been published yet. The scripts and their tests exist (`test_release_scripts` passes); the GitHub release workflow has not run on a real tag; the image/update-bundle part of `make-release.sh` has not been run because no A/B image has been built yet. <!-- TASK12-RESULT: note here if make-release.sh was run against a built image -->

## Contents

- [Versions and identifiers](#versions-and-identifiers)
- [Keys](#keys)
- [Artifacts](#artifacts)
- [Scripts](#scripts)
- [The release workflow](#the-release-workflow)
- [Release checklist](#release-checklist)
- [Verifying a release as a user](#verifying-a-release-as-a-user)
- [Reproducibility](#reproducibility)
- [Limitations](#limitations)

## Versions and identifiers

| Identifier | Source | Example |
| --- | --- | --- |
| Version | `project(NexoraOS VERSION x.y.z)` in `CMakeLists.txt` (strict `major.minor.patch`, compared by the updater) | `0.4.0` |
| Package version | `NEXORA_OS_VERSION` in `buildroot-external/package/nexora-os/nexora-os.mk`; keep it equal to the project version | `0.4.0` |
| Build id | `NEXORA_BUILD_ID` CMake variable; default `git describe --always --dirty`, recomputed at every configure; `make-release.sh` passes `git describe --tags --always`; the Buildroot package passes the source tree's `git describe` | `v0.4.0` or `388781b` |
| Board | `NEXORA_BOARD` cache variable | `nexora-x86_64` |
| Tag | `v<version>`, must point at the released commit | `v0.4.0` |

`nexora-console-ui --version` prints all three (`0.4.0 388781b nexora-x86_64`); on the image `/etc/nexora-release` carries the same values (filled by `post-build.sh` from the installed `share/nexora/build-info`), and the System tab and diagnostics reports show them.

## Keys

Two kinds of Ed25519 keys are involved. They can be the same key, but keeping them separate limits the damage of a leaked CI secret.

| Key | Signs | Where the private half lives | Where the public half goes |
| --- | --- | --- | --- |
| Release key | `SHA256SUMS` of a release (`SHA256SUMS.sig`) | GitHub Actions secret `NEXORA_RELEASE_SIGNING_KEY` (PEM) | published with the release notes, for users running `verify-release.sh` |
| Update key | `manifest.json` of an update bundle (`manifest.json.sig`) | **offline**, with the maintainers | `buildroot-external/board/nexora-x86_64/update-keys/*.pem` before the image is built → `/usr/share/nexora/update-keys/` in the image |

Generate a key (on an offline machine):

```sh
openssl genpkey -algorithm ED25519 -out nexora-update.key
chmod 600 nexora-update.key
openssl pkey -in nexora-update.key -pubout -out nexora-update.pem
```

Never commit a private key: the update-keys directory has a `.gitignore`, tests generate throwaway keys, and `sign-update.sh` refuses a key located inside any git work tree. Rotation and revocation are described in [update-system.md](update-system.md#trust-model).

## Artifacts

`make-release.sh VERSION` writes to `dist/` (override with `NEXORA_DIST_DIR`):

| File | Content |
| --- | --- |
| `nexora-os-VERSION-linux-x86_64.tar.gz` | Release build (`-DBUILD_TESTING=OFF -DNEXORA_BUILD_FAKE_EMULATOR=OFF`) installed with `cmake --install` (prefix `/usr`, shipped relative), plus `README.md` and a `VERSION` file (version and build id) |
| `nexora-os-VERSION-x86_64-disk.img.xz` | only if a Buildroot image exists in `NEXORA_BR_IMAGES` (default `~/nexora-br/out/images`): the full disk image |
| `nexora-update-VERSION/` | only with an image whose `/etc/nexora-release` says `NEXORA_VERSION=VERSION` (otherwise the run fails: "the Buildroot image … is not version VERSION; rebuild it first"): `rootfs.ext4` + generated `manifest.json` (unsigned; notes taken from `docs/release-notes/VERSION.md`, `minimumVersion` from `NEXORA_UPDATE_MINIMUM_VERSION`, default `0.4.0`) |
| `SHA256SUMS` | SHA-256 of every file produced by that run |
| `SHA256SUMS.sig` | added by the release workflow (or by hand with `sign-update.sh --file`) |

## Scripts

| Script | What it does | Exit status |
| --- | --- | --- |
| `scripts/release/make-release.sh VERSION` | Requires a clean work tree (no modified, staged or untracked files) and tag `vVERSION` on `HEAD`; warns if the CMake project version differs. Builds a Release from a `git archive` export of `HEAD` in a fixed work directory (`NEXORA_RELEASE_WORKDIR`, default `/tmp/nexora-os-release`) with `SOURCE_DATE_EPOCH` (default: commit time), `QT_HASH_SEED=0`, `LC_ALL=C`, `TZ=UTC`, `-ffile-prefix-map` and ASLR disabled via `setarch -R` when allowed; writes a deterministic tarball (`tar --sort=name --owner=0 --group=0 --numeric-owner --mtime=@$SOURCE_DATE_EPOCH`, `gzip -n`), the optional image and bundle, and `SHA256SUMS`. Before packaging a Buildroot `rootfs.ext4` it runs `check-image-version.sh`, so a stale image is never shipped under a newer version number | 0 ok, 1 error, 64 usage |
| `scripts/release/check-image-version.sh ROOTFS_EXT4 VERSION` | Reads `/etc/nexora-release` from the ext4 image without mounting it (`debugfs` from e2fsprogs, also looked up in `/sbin` and `/usr/sbin`) and fails unless its `NEXORA_VERSION` equals `VERSION`; also fails when the file or the variable is missing or the file is not ext4 | 0 ok, 1 mismatch or error, 64 usage |
| `scripts/release/sign-update.sh --key KEY (--bundle DIR \| --file FILE) [--pubkey PEM]` | Signs `DIR/manifest.json` (≤ 64 KiB) → `manifest.json.sig`, or `FILE` → `FILE.sig`, with `openssl pkeyutl -sign -rawin`; checks the key is an Ed25519 PEM private key outside every git work tree (warns if group/others can read it); verifies the 64-byte signature with the derived public key or `--pubkey` before replacing the old signature | 0 ok, 1 error, 64 usage |
| `scripts/release/verify-release.sh DIR [--pubkey PEM]` | Rejects malformed `SHA256SUMS` lines and unsafe paths (absolute, `..`, `.`), runs `sha256sum --check --strict`; if `SHA256SUMS.sig` exists it must verify with `--pubkey`; a signature without `--pubkey`, or `--pubkey` without a signature, is an error | 0 verified, 1 failed, 64 usage |
| `scripts/release/check-docs.sh [ROOT]` | Every required document (the 18 MVP 4 documents plus `buildroot.md`, `settings.md`, `development.md`) exists and is non-empty, and every relative Markdown link in `README.md` and `docs/**/*.md` resolves (code blocks, inline code, URLs and in-page anchors are skipped) | 0 ok, 1 problems |
| `scripts/qmllint.sh BUILD_DIR` | `qmllint` on all QML of the main tree with the build's `qml/` import path; fails on errors only, prints warnings | 0 ok, 1 errors |
| `scripts/shellcheck.sh` | shellcheck on all shell scripts of the repository (`-s sh` for `#!/bin/sh` scripts) | 0 ok, 1 findings |

`tests/integration/test_release_scripts.sh` (CTest `test_release_scripts`, labels `integration`, `security`) exercises signing a bundle and a `SHA256SUMS` with a throwaway key in `$TMPDIR`, verification, tamper detection, refusal of a key inside a work tree, `check-docs.sh` on a scratch tree, and `check-image-version.sh` on scratch ext4 images (matching version, stale version, no `/etc/nexora-release`, not ext4; skipped when `mkfs.ext4`/`debugfs` are missing); it checks that `make-release.sh` calls `check-image-version.sh` but does not run `make-release.sh`.

## The release workflow

`.github/workflows/release.yml` runs on a pushed tag `v*`:

1. **Fails immediately if the secret `NEXORA_RELEASE_SIGNING_KEY` is not set** — there is no unsigned release.
2. Checks out with full history; the tag must be `vMAJOR.MINOR.PATCH`, and `docs/release-notes/<version>.md` must exist and be non-empty.
3. Installs the Ubuntu 24.04 packages, builds the `ci` preset (`-Werror`), runs `ctest --preset ci`, `ctest --preset ci -L security`, `scripts/qmllint.sh build/ci` and `scripts/release/check-docs.sh`.
4. `make-release.sh <version>` (CI has no Buildroot image, so only the tarball and `SHA256SUMS`).
5. Writes the key to a 0600 file in `$RUNNER_TEMP` only for the signing step, derives the public key, signs `dist/SHA256SUMS`, then `verify-release.sh dist --pubkey …`.
6. Creates a **draft** GitHub release with every regular file in `dist/` and the release notes; uploads `dist/` as a workflow artifact (14 days). A maintainer reviews and publishes the draft by hand.

The workflow never builds the disk image and never signs update bundles; those steps are manual and offline (below).

## Release checklist

Run in order and record the results in the release notes.

**Preparation**

- [ ] `CMakeLists.txt` project version and `NEXORA_OS_VERSION` in `buildroot-external/package/nexora-os/nexora-os.mk` set to the new version.
- [ ] `docs/release-notes/<version>.md` written: changes, known limitations ([mvp-4.md](mvp-4.md#limitations)), tested hardware ([hardware-compatibility.md](hardware-compatibility.md)), installation and recovery links.
- [ ] Documentation current; `scripts/release/check-docs.sh` passes.

**Checks (clean checkout, Ubuntu 24.04, Qt 6.4.2)**

- [ ] `cmake --preset ci && cmake --build --preset ci && ctest --preset ci` — all tests pass with GCC.
- [ ] Same with `CC=clang CXX=clang++`.
- [ ] `ctest --preset ci -L security` and `ctest --preset ci -L system` pass.
- [ ] `scripts/qmllint.sh build/ci` reports 0 errors; `scripts/shellcheck.sh` clean; `clang-format-18 --dry-run --Werror` on all tracked C++.
- [ ] `nexora-console-ui --check-config` passes for `config/emulators` and `config/examples/emulators`.
- [ ] CI (`ci.yml`, `buildroot.yml` validate job) green on the release commit.

**Image (manual)**

- [ ] Release public update key(s) copied into `buildroot-external/board/nexora-x86_64/update-keys/` (public only).
- [ ] `scripts/image/build-image.sh` from a clean output directory (or `make nexora-os-dirclean all` after a key change).
- [ ] `scripts/image/qemu-boot-test.sh` prints PASS; manual QEMU checks from [testing.md](testing.md#manual-and-system-tests) recorded.
- [ ] Update path tested: install a bundle signed with a test key onto an image that trusts that key; confirm and rollback observed.
- [ ] Physical hardware tested and [hardware-compatibility.md](hardware-compatibility.md) updated, or the release notes state that it was not.

**Tag, build, sign**

- [ ] `git tag -s v<version>` (or annotated) on the release commit and push the tag; the release workflow creates the draft.
- [ ] Locally (with the image): `scripts/release/make-release.sh <version>` → `dist/` with disk image and unsigned bundle.
- [ ] Offline: `scripts/release/sign-update.sh --key /offline/nexora-update.key --bundle dist/nexora-update-<version> --pubkey buildroot-external/board/nexora-x86_64/update-keys/<name>.pem` (the `--pubkey` check proves the bundle matches the key shipped in the image).
- [ ] `nexora-update-tool verify --keys buildroot-external/board/nexora-x86_64/update-keys --bundle dist/nexora-update-<version> --current-version <previous> --board nexora-x86_64` prints `ok`.
- [ ] Sign `SHA256SUMS` of the image artifacts with the release key (`sign-update.sh --file dist/SHA256SUMS`), then `scripts/release/verify-release.sh dist --pubkey release-public.pem`.

**Publish**

- [ ] Attach the disk image, `SHA256SUMS`, `SHA256SUMS.sig` and the bundle (as an archive) to the draft release; publish.
- [ ] Publish the release public key fingerprint with the release notes.

## Verifying a release as a user

```sh
scripts/release/verify-release.sh <download dir> --pubkey nexora-release.pem
# or, without the script:
sha256sum --check --strict SHA256SUMS
openssl pkeyutl -verify -pubin -inkey nexora-release.pem -rawin -in SHA256SUMS -sigfile SHA256SUMS.sig
```

## Reproducibility

`make-release.sh` is designed so that two runs on the same commit with the same toolchain produce byte-identical archives: fixed timestamps, sorted archive entries, numeric owner, `gzip -n`, path prefix mapping, a fixed source/build path, `QT_HASH_SEED=0` and ASLR disabled (Qt 6.4's `qmlcachegen` otherwise emits its generated C++ in a run- and path-dependent order). This was checked when the script was written by building two clones at different paths and comparing the SHA-256 of the archives; it is not re-checked by CI. Reproducibility depends on the toolchain: a different compiler or Qt version gives different bytes. The Buildroot image is not claimed to be reproducible (the data filesystem UUID is fixed, but other image parts were not examined).

## Limitations

- No release has been made with this process yet; the workflow has not run on GitHub.
- The image and update-bundle part of `make-release.sh` has not been run (no A/B image has been built); only its version check (`check-image-version.sh`, on small scratch ext4 images) is tested in `test_release_scripts`.
- No package-manager packages (deb/rpm); the tarball is an install tree for `/usr`.
- `docs/release-notes/` holds only its [README](release-notes/README.md) (file naming and contents) so far; the first tagged release must add its notes file or the workflow fails.
- The update bundle is published as an archive of a directory; there is no single-file bundle format.
