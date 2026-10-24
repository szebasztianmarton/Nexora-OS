#!/usr/bin/env bash
# Builds the release artifacts for one tagged version, reproducibly.
#   scripts/release/make-release.sh VERSION
#
# Requirements: a clean work tree (no modified, staged or untracked files) and a tag
# vVERSION pointing at HEAD.
#
# Output in dist/ (override with NEXORA_DIST_DIR):
#   nexora-os-VERSION-linux-x86_64.tar.gz   Release build installed with `cmake --install`
#   SHA256SUMS                              checksums of every file this run produced
# and, when a Buildroot image exists in NEXORA_BR_IMAGES (default ~/nexora-br/out/images):
#   nexora-os-VERSION-x86_64-disk.img.xz    full disk image
#   nexora-update-VERSION/                  update bundle: rootfs.ext4 + manifest.json
#                                           (unsigned; sign offline with sign-update.sh)
# The Buildroot rootfs.ext4 must contain /etc/nexora-release with NEXORA_VERSION=VERSION
# (checked with check-image-version.sh, needs debugfs from e2fsprogs); otherwise the run
# fails instead of shipping a stale image under the new version.
#
# Determinism: every timestamp comes from SOURCE_DATE_EPOCH (default: committer time of
# HEAD), the archive is written with sorted names, numeric owner 0:0, normalised modes and
# `gzip -n`, compiler paths are mapped with -ffile-prefix-map, and the build runs on a
# `git archive` export of HEAD at a fixed path with QT_HASH_SEED=0 and ASLR disabled
# (setarch -R), because Qt 6.4's qmlcachegen output otherwise varies per run and per path.
# Two runs on the same commit with the same toolchain produce byte-identical archives.
#
# Environment:
#   NEXORA_DIST_DIR          output directory (default: <repo>/dist)
#   NEXORA_RELEASE_WORKDIR   fixed source export + build directory, wiped before and after
#                            (default: /tmp/nexora-os-release; keep it identical between
#                            builds that must match)
#   NEXORA_BR_IMAGES         Buildroot images directory (default: ~/nexora-br/out/images)
#   NEXORA_UPDATE_MINIMUM_VERSION  minimumVersion written to the update manifest (default 0.4.0)
#   NEXORA_RELEASE_JOBS      parallel build jobs (default: nproc)
set -euo pipefail

usage() { echo "usage: $0 VERSION   (e.g. $0 0.4.1)" >&2; exit 64; }
die() { echo "make-release: error: $*" >&2; exit 1; }

[[ $# -eq 1 ]] || usage
version="$1"
[[ "$version" =~ ^[0-9]{1,4}\.[0-9]{1,4}\.[0-9]{1,4}$ ]] || die "VERSION must look like 1.2.3, got '$version'"
tag="v$version"

for tool in git cmake ninja tar gzip sha256sum python3; do
    command -v "$tool" >/dev/null 2>&1 || die "'$tool' not found"
done

root="$(cd "$(dirname "$0")/../.." && pwd -P)"
cd "$root"

# --- preconditions --------------------------------------------------------------------
if [[ -n "$(git status --porcelain --untracked-files=normal)" ]]; then
    git status --short >&2
    die "the work tree is not clean; commit or remove the changes above first"
fi
git rev-parse -q --verify "refs/tags/$tag" >/dev/null || die "tag $tag does not exist"
tag_commit="$(git rev-parse "$tag^{commit}")"
head_commit="$(git rev-parse HEAD)"
[[ "$tag_commit" == "$head_commit" ]] || die "tag $tag points at ${tag_commit:0:12}, but HEAD is ${head_commit:0:12}"

project_version="$(sed -n '/^project(/,/)/s/^[[:space:]]*VERSION[[:space:]]\{1,\}\([0-9.]*\).*/\1/p' CMakeLists.txt | head -n 1)"
if [[ "$project_version" != "$version" ]]; then
    echo "make-release: warning: CMakeLists.txt project VERSION is '$project_version', releasing '$version'" >&2
fi

: "${SOURCE_DATE_EPOCH:=$(git log -1 --format=%ct HEAD)}"
export SOURCE_DATE_EPOCH
[[ "$SOURCE_DATE_EPOCH" =~ ^[0-9]+$ ]] || die "SOURCE_DATE_EPOCH must be an integer"
build_id="$(git describe --tags --always)"
dist="${NEXORA_DIST_DIR:-$root/dist}"
work="${NEXORA_RELEASE_WORKDIR:-/tmp/nexora-os-release}"
br_images="${NEXORA_BR_IMAGES:-$HOME/nexora-br/out/images}"
jobs="${NEXORA_RELEASE_JOBS:-$(nproc)}"
name="nexora-os-$version-linux-x86_64"

echo "make-release: $tag ($build_id), SOURCE_DATE_EPOCH=$SOURCE_DATE_EPOCH"

# Fixed locale, timezone and umask so tools behave the same everywhere. QT_HASH_SEED=0
# disables Qt's per-process QHash randomisation: without it qmlcachegen emits the
# generated C++ for the same QML in a different order on every run.
export LC_ALL=C TZ=UTC QT_HASH_SEED=0
umask 022

# --- build + install ------------------------------------------------------------------
# Build an export of exactly the tagged commit (git archive: committed files only) at a
# FIXED path. qmlcachegen (Qt 6.4) orders some generated C++ declarations by heap address;
# with ASLR disabled that order is stable, but it still depends on the exact source and
# build paths (their bytes end up in heap allocations), so a fixed path is required for
# the same output from checkouts at different locations.
if [[ -L "$work" ]]; then die "$work is a symlink; refusing to use it"; fi
if [[ -e "$work" ]]; then
    [[ -O "$work" ]] || die "$work exists and is not owned by $(id -un); remove it or set NEXORA_RELEASE_WORKDIR"
    rm -rf "$work"
fi
mkdir -m 0700 "$work"
stage=""
trap 'rm -rf "${stage:-}" "$work"' EXIT
mkdir "$work/src"
git archive --format=tar HEAD | tar -x -C "$work/src"
src="$work/src"
build="$work/build"
cmake -S "$src" -B "$build" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_TESTING=OFF \
    -DNEXORA_BUILD_FAKE_EMULATOR=OFF \
    -DNEXORA_BUILD_ID="$build_id" \
    -DCMAKE_INSTALL_PREFIX=/usr \
    "-DCMAKE_CXX_FLAGS=-ffile-prefix-map=$src=. -ffile-prefix-map=$build=."
# With address space layout randomisation qmlcachegen's output changes from run to run.
# Build with ASLR disabled for the build's process tree (setarch -R) when allowed.
norandom=()
if setarch "$(uname -m)" -R true >/dev/null 2>&1; then
    norandom=(setarch "$(uname -m)" -R)
else
    echo "make-release: warning: cannot disable ASLR (setarch -R); the QML cache code and" \
        "therefore the archive may not be byte-for-byte reproducible" >&2
fi
"${norandom[@]}" cmake --build "$build" -j "$jobs"

stage="$(mktemp -d)"
DESTDIR="$stage/$name" cmake --install "$build" >/dev/null
# Everything was installed under DESTDIR/usr; ship the tree relative to the prefix.
mv "$stage/$name/usr" "$stage/$name.tmp"
rmdir "$stage/$name"
mv "$stage/$name.tmp" "$stage/$name"
install -m 0644 "$src/README.md" "$stage/$name/README.md"
printf '%s\n' "$version" "$build_id" > "$stage/$name/VERSION"

# --- deterministic archive --------------------------------------------------------------
mkdir -p "$dist"
produced=()
archive="$name.tar.gz"
tar --sort=name --format=gnu \
    --owner=0 --group=0 --numeric-owner \
    --mtime="@$SOURCE_DATE_EPOCH" \
    --mode='u+rwX,go+rX,go-w' \
    -C "$stage" -cf - "$name" | gzip -n -9 > "$dist/$archive.tmp"
mv -f "$dist/$archive.tmp" "$dist/$archive"
produced+=("$archive")

# --- optional Buildroot image + update bundle ------------------------------------------
if [[ -f "$br_images/rootfs.ext4" ]]; then
    echo "make-release: including Buildroot images from $br_images"
    # Never package a stale image under this version: its /etc/nexora-release must match.
    "$root/scripts/release/check-image-version.sh" "$br_images/rootfs.ext4" "$version" ||
        die "the Buildroot image in $br_images is not version $version; rebuild it first"
    disk="nexora-os-$version-x86_64-disk.img.xz"
    if [[ -f "$br_images/disk.img.xz" ]]; then
        cp -f "$br_images/disk.img.xz" "$dist/$disk"
        produced+=("$disk")
    elif [[ -f "$br_images/disk.img" ]]; then
        command -v xz >/dev/null 2>&1 || die "'xz' not found"
        # Fixed thread count and block size: xz output depends on both.
        xz -6 --threads=2 --block-size=67108864 -c "$br_images/disk.img" > "$dist/$disk.tmp"
        mv -f "$dist/$disk.tmp" "$dist/$disk"
        produced+=("$disk")
    fi

    bundle="nexora-update-$version"
    rm -rf "${dist:?}/$bundle"
    mkdir -p "$dist/$bundle"
    cp -f "$br_images/rootfs.ext4" "$dist/$bundle/rootfs.ext4"
    notes_file="$root/docs/release-notes/$version.md"
    NX_VERSION="$version" NX_BUILD_ID="$build_id" \
        NX_MIN_VERSION="${NEXORA_UPDATE_MINIMUM_VERSION:-0.4.0}" \
        NX_IMAGE="$dist/$bundle/rootfs.ext4" NX_NOTES_FILE="$notes_file" \
        python3 -I - > "$dist/$bundle/manifest.json" <<'PY'
import hashlib, json, os, time
image = os.environ["NX_IMAGE"]
digest = hashlib.sha256()
with open(image, "rb") as f:
    for chunk in iter(lambda: f.read(1 << 20), b""):
        digest.update(chunk)
notes = ""
if os.path.isfile(os.environ["NX_NOTES_FILE"]):
    with open(os.environ["NX_NOTES_FILE"], encoding="utf-8") as f:
        notes = f.read().strip()[:4000]
manifest = {
    "format": 1,
    "product": "nexora-os",
    "board": "nexora-x86_64",
    "version": os.environ["NX_VERSION"],
    "buildId": os.environ["NX_BUILD_ID"],
    "minimumVersion": os.environ["NX_MIN_VERSION"],
    "releasedAt": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime(int(os.environ["SOURCE_DATE_EPOCH"]))),
    "image": {"file": "rootfs.ext4", "size": os.path.getsize(image), "sha256": digest.hexdigest()},
    "notes": notes,
}
print(json.dumps(manifest, indent=2, sort_keys=False))
PY
    produced+=("$bundle/manifest.json" "$bundle/rootfs.ext4")
    echo "make-release: update bundle $bundle is UNSIGNED; sign it offline with scripts/release/sign-update.sh"
else
    echo "make-release: no Buildroot image in $br_images; skipping disk image and update bundle"
fi

# --- checksums ------------------------------------------------------------------------
(
    cd "$dist"
    rm -f SHA256SUMS.sig
    printf '%s\n' "${produced[@]}" | sort | xargs -d '\n' sha256sum -- > SHA256SUMS.tmp
    mv -f SHA256SUMS.tmp SHA256SUMS
)

echo "make-release: done"
(cd "$dist" && cat SHA256SUMS)
