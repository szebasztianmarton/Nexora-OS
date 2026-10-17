#!/usr/bin/env bash
# Checks that an ext4 root filesystem image is the release it claims to be.
#   scripts/release/check-image-version.sh ROOTFS_EXT4 VERSION
#
# Reads /etc/nexora-release from the image without mounting it (debugfs from e2fsprogs)
# and fails unless its NEXORA_VERSION equals VERSION. make-release.sh runs it before
# packaging rootfs.ext4 into an update bundle, so a stale Buildroot image is never shipped
# under a newer version number.
set -euo pipefail

die() { echo "check-image-version: error: $*" >&2; exit 1; }

[[ $# -eq 2 ]] || { echo "usage: $0 ROOTFS_EXT4 VERSION" >&2; exit 64; }
image="$1"
version="$2"
[[ -f "$image" ]] || die "$image not found"

debugfs=""
for candidate in "$(command -v debugfs 2>/dev/null || true)" /sbin/debugfs /usr/sbin/debugfs; do
    if [[ -n "$candidate" && -x "$candidate" ]]; then
        debugfs="$candidate"
        break
    fi
done
[[ -n "$debugfs" ]] || die "'debugfs' (e2fsprogs) not found"

release="$("$debugfs" -R 'cat /etc/nexora-release' "$image" 2>/dev/null)" ||
    die "cannot read /etc/nexora-release from $image"
image_version="$(sed -n 's/^NEXORA_VERSION=//p' <<< "$release" | tail -n 1 | tr -d "\"'")"
[[ -n "$image_version" ]] || die "$image has no NEXORA_VERSION in /etc/nexora-release"
if [[ "$image_version" != "$version" ]]; then
    die "$image is Nexora OS $image_version, not $version (stale Buildroot image?)"
fi
echo "check-image-version: $image is Nexora OS $version"
