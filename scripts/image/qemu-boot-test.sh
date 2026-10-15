#!/bin/sh
# Boot a Nexora OS disk image in QEMU (UEFI via OVMF) and check that the
# console UI session starts.
#
# The serial console is captured to a log. The test passes when the init
# script reports "Starting nexora: OK" within the timeout; the VM is then
# asked to power off through the QEMU monitor (system_powerdown).
#
# Usage: scripts/image/qemu-boot-test.sh [disk.img]
#   NEXORA_QEMU_TIMEOUT  seconds to wait for the marker (default 300;
#                        without KVM a boot can take several minutes)
#   NEXORA_QEMU_LOG      serial log path (default: ./qemu-serial.log)
#   OVMF_CODE / OVMF_VARS  firmware files (default: Ubuntu's ovmf package)
#
# Prints PASS or FAIL and exits 0 / 1. Never modifies the given image: it
# boots a temporary copy.

set -eu

IMAGE="${1:-${HOME}/nexora-br/out/images/disk.img}"
TIMEOUT="${NEXORA_QEMU_TIMEOUT:-300}"
LOG="${NEXORA_QEMU_LOG:-$(pwd)/qemu-serial.log}"
OVMF_CODE="${OVMF_CODE:-/usr/share/OVMF/OVMF_CODE_4M.fd}"
OVMF_VARS="${OVMF_VARS:-/usr/share/OVMF/OVMF_VARS_4M.fd}"
MARKER='Starting nexora: OK'

for f in "${IMAGE}" "${OVMF_CODE}" "${OVMF_VARS}"; do
	if [ ! -f "${f}" ]; then
		echo "qemu-boot-test.sh: missing ${f}" >&2
		exit 2
	fi
done

WORK="$(mktemp -d)"
trap 'kill "${QEMU_PID:-0}" 2>/dev/null || true; rm -rf "${WORK}"' EXIT INT TERM

cp "${IMAGE}" "${WORK}/disk.img"
cp "${OVMF_VARS}" "${WORK}/vars.fd"
: > "${LOG}"

ACCEL=tcg
if [ -w /dev/kvm ]; then
	ACCEL=kvm
fi

qemu-system-x86_64 \
	-machine q35,accel="${ACCEL}" -m 2048 -smp 2 \
	-drive if=pflash,format=raw,readonly=on,file="${OVMF_CODE}" \
	-drive if=pflash,format=raw,file="${WORK}/vars.fd" \
	-drive file="${WORK}/disk.img",format=raw,if=virtio \
	-device virtio-vga \
	-display none \
	-serial file:"${LOG}" \
	-monitor unix:"${WORK}/monitor.sock",server,nowait \
	-nic none &
QEMU_PID=$!

START=$(date +%s)
RESULT=FAIL
while kill -0 "${QEMU_PID}" 2>/dev/null; do
	if grep -q "${MARKER}" "${LOG}" 2>/dev/null; then
		RESULT=PASS
		break
	fi
	if [ $(($(date +%s) - START)) -ge "${TIMEOUT}" ]; then
		break
	fi
	sleep 2
done
ELAPSED=$(($(date +%s) - START))

if [ -S "${WORK}/monitor.sock" ] && command -v socat >/dev/null 2>&1; then
	echo system_powerdown | socat - UNIX-CONNECT:"${WORK}/monitor.sock" >/dev/null 2>&1 || true
	# Give the guest time to shut down cleanly before the trap kills QEMU.
	i=0
	while kill -0 "${QEMU_PID}" 2>/dev/null && [ "${i}" -lt 60 ]; do
		sleep 1
		i=$((i + 1))
	done
fi

echo "qemu-boot-test.sh: ${RESULT} after ${ELAPSED}s (accel=${ACCEL}, serial log: ${LOG})"
[ "${RESULT}" = PASS ]
