#!/bin/sh
# Build the Nexora OS disk image with Buildroot.
#
# Fetches Buildroot (shallow clone of a fixed tag) into $NEXORA_BR_DIR,
# configures it with this repository's BR2_EXTERNAL tree and runs the full
# build. A first build takes several hours; later runs are incremental.
#
# Usage: scripts/image/build-image.sh [make target...]
#   NEXORA_BR_DIR      work directory (default: ~/nexora-br)
#   BUILDROOT_VERSION  Buildroot tag (default: 2026.02.3, see docs/buildroot.md)
#
# Examples:
#   scripts/image/build-image.sh                       # full build
#   scripts/image/build-image.sh nexora-os-rebuild all # rebuild the app + image

set -eu

BUILDROOT_VERSION="${BUILDROOT_VERSION:-2026.02.3}"
BUILDROOT_GIT="${BUILDROOT_GIT:-https://gitlab.com/buildroot.org/buildroot.git}"
NEXORA_BR_DIR="${NEXORA_BR_DIR:-${HOME}/nexora-br}"
DEFCONFIG=nexora_x86_64_defconfig

REPO_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
BR_DIR="${NEXORA_BR_DIR}/buildroot"
OUT_DIR="${NEXORA_BR_DIR}/out"

mkdir -p "${NEXORA_BR_DIR}"

if [ ! -d "${BR_DIR}/.git" ]; then
	git clone --depth 1 --branch "${BUILDROOT_VERSION}" "${BUILDROOT_GIT}" "${BR_DIR}"
fi

if [ ! -f "${OUT_DIR}/.config" ]; then
	make -C "${BR_DIR}" BR2_EXTERNAL="${REPO_DIR}/buildroot-external" \
		O="${OUT_DIR}" "${DEFCONFIG}"
fi

make -C "${BR_DIR}" O="${OUT_DIR}" "$@"

if [ -f "${OUT_DIR}/images/disk.img" ]; then
	echo "build-image.sh: image ready: ${OUT_DIR}/images/disk.img"
fi
