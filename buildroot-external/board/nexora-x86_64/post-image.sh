#!/bin/sh
# Buildroot post-image script for Nexora OS (x86-64): creates disk.img
# (GPT, EFI system partition + ext4 root) with genimage.

set -e

BOARD_DIR="$(dirname "$0")"

UUID=$(dumpe2fs "${BINARIES_DIR}/rootfs.ext2" 2>/dev/null | \
	sed -n 's/^Filesystem UUID: *\(.*\)/\1/p')
if [ -z "${UUID}" ]; then
	echo "post-image.sh: cannot read the root filesystem UUID" >&2
	exit 1
fi

sed -i "s/UUID_TMP/${UUID}/g" "${BINARIES_DIR}/efi-part/EFI/BOOT/grub.cfg"
sed "s/UUID_TMP/${UUID}/g" "${BOARD_DIR}/genimage.cfg" \
	> "${BINARIES_DIR}/genimage.cfg"

support/scripts/genimage.sh -c "${BINARIES_DIR}/genimage.cfg"
