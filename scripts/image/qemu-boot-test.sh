#!/bin/sh
# Boot a Nexora OS disk image in QEMU (UEFI via OVMF) and check that the
# console UI session starts.
#
# The serial console is captured to a log. The init script must report
# "Starting nexora: OK" within the timeout, and then the console UI must
# stay up: the log is watched for NEXORA_QEMU_SETTLE more seconds, and the
# test fails if nexora-session gives up ("nexora-session: giving up") or
# restarts the UI NEXORA_QEMU_MAX_RESTARTS times or more ("... exited with
# status ...; restarting"; nexora-session writes both to the kernel log,
# which reaches the serial console). The VM is then asked to power off
# through the QEMU monitor (system_powerdown).
#
# Usage: scripts/image/qemu-boot-test.sh [disk.img]
#   NEXORA_QEMU_TIMEOUT  seconds to wait for the marker (default 300;
#                        without KVM a boot can take several minutes)
#   NEXORA_QEMU_SETTLE   seconds to keep watching after the marker
#                        (default 60)
#   NEXORA_QEMU_MAX_RESTARTS  UI restarts that fail the test (default 2)
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
SETTLE="${NEXORA_QEMU_SETTLE:-60}"
MAX_RESTARTS="${NEXORA_QEMU_MAX_RESTARTS:-2}"
MARKER='Starting nexora: OK'
GIVE_UP='nexora-session: giving up'
RESTART='nexora-session: nexora-console-ui exited with status .*; restarting'

for f in "${IMAGE}" "${OVMF_CODE}" "${OVMF_VARS}"; do
	if [ ! -f "${f}" ]; then
		echo "qemu-boot-test.sh: missing ${f}" >&2
		exit 2
	fi
done

WORK="$(mktemp -d)"
trap '[ -n "${QEMU_PID:-}" ] && kill "${QEMU_PID}" 2>/dev/null; rm -rf "${WORK}"' EXIT INT TERM

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
REASON="no '${MARKER}' within ${TIMEOUT}s"
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

# session_failed: sets REASON and succeeds if the session gave up or the
# UI was restarted too often.
session_failed() {
	if grep -q "${GIVE_UP}" "${LOG}" 2>/dev/null; then
		REASON="nexora-session gave up (UI crash loop)"
		return 0
	fi
	restarts=$(grep -c "${RESTART}" "${LOG}" 2>/dev/null || true)
	if [ "${restarts:-0}" -ge "${MAX_RESTARTS}" ]; then
		REASON="console UI restarted ${restarts} times"
		return 0
	fi
	return 1
}

if [ "${RESULT}" = PASS ]; then
	SETTLE_START=$(date +%s)
	while [ $(($(date +%s) - SETTLE_START)) -lt "${SETTLE}" ]; do
		if ! kill -0 "${QEMU_PID}" 2>/dev/null; then
			RESULT=FAIL
			REASON="QEMU exited while watching the session"
			break
		fi
		if session_failed; then
			RESULT=FAIL
			break
		fi
		sleep 2
	done
	if [ "${RESULT}" = PASS ] && session_failed; then
		RESULT=FAIL
	fi
fi
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

if [ "${RESULT}" = PASS ]; then
	echo "qemu-boot-test.sh: PASS after ${ELAPSED}s, UI stayed up for ${SETTLE}s (accel=${ACCEL}, serial log: ${LOG})"
else
	echo "qemu-boot-test.sh: FAIL after ${ELAPSED}s: ${REASON} (accel=${ACCEL}, serial log: ${LOG})"
fi
[ "${RESULT}" = PASS ]
