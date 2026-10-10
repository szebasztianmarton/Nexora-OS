#!/bin/sh
# Buildroot post-build script for Nexora OS (x86-64).
# $1: target directory

set -e

BOARD_DIR="$(dirname "$0")"
TARGET="$1"

# GRUB2 (x86_64-efi) reads its configuration from the EFI system
# partition; the partition UUID placeholder is filled in by post-image.sh.
install -D -m 0644 "${BOARD_DIR}/grub.cfg" \
	"${BINARIES_DIR}/efi-part/EFI/BOOT/grub.cfg"

# Recovery login on tty2 in addition to the serial getty configured by
# BR2_TARGET_GENERIC_GETTY_PORT (ttyS0). tty1 is used by the console UI.
if ! grep -q '^tty2::' "${TARGET}/etc/inittab"; then
	printf '%s\n' \
		'# Recovery login on the second virtual terminal (Nexora)' \
		'tty2::respawn:/sbin/getty 38400 tty2 linux' \
		>> "${TARGET}/etc/inittab"
fi
