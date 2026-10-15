#!/bin/sh
# Buildroot post-image script for Nexora OS (x86-64): creates the initial
# GRUB environment block and the empty data filesystem, then disk.img (GPT:
# ESP, root slots A and B, data) with genimage. See genimage.cfg.

set -e

BOARD_DIR="$(dirname "$0")"
GRUBENV="${BINARIES_DIR}/efi-part/EFI/BOOT/grubenv"
DATA_IMG="${BINARIES_DIR}/data.ext4"
# Fixed so that identical inputs give an identical data filesystem.
DATA_FS_UUID=0f9a3c8e-5d7b-4c2e-9a61-3b8d2e4f7c15

# grubenv: a 1024-byte GRUB environment block (the format grub-editenv
# writes): header line, NAME=VALUE lines, padded with '#'. Slot A is the
# confirmed slot, no trial boot.
{
	printf '# GRUB Environment Block\n'
	printf 'nexora_slot=A\nnexora_try=\nnexora_tries=0\n'
} > "${GRUBENV}"
pad=$((1024 - $(wc -c < "${GRUBENV}")))
head -c "${pad}" /dev/zero | tr '\0' '#' >> "${GRUBENV}"
if [ "$(wc -c < "${GRUBENV}")" -ne 1024 ]; then
	echo "post-image.sh: grubenv is not 1024 bytes" >&2
	exit 1
fi

# Empty ext4 data filesystem; /home/nexora is created on first boot by
# /etc/init.d/S01nexora-data. Resizing it to fill a larger disk is not
# implemented.
rm -f "${DATA_IMG}"
truncate -s 1024M "${DATA_IMG}"
"${HOST_DIR}/sbin/mkfs.ext4" -q -F -L nexora-data -U "${DATA_FS_UUID}" \
	-E root_owner=0:0 "${DATA_IMG}"

support/scripts/genimage.sh -c "${BOARD_DIR}/genimage.cfg"
