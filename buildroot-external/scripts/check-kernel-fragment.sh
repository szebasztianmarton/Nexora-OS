#!/bin/sh
# Verify that every option of the Nexora kernel configuration fragment
# survives the kernel's Kconfig (unknown symbols or unmet dependencies are
# dropped silently by the kernel's merge step).
#
# Run after "make linux-configure" (or a full build) in <output-dir>.
#
# Usage: check-kernel-fragment.sh <output-dir>

set -eu

if [ $# -ne 1 ]; then
	echo "usage: $0 <output-dir>" >&2
	exit 2
fi

EXTERNAL_DIR=$(cd "$(dirname "$0")/.." && pwd)
FRAGMENT="${EXTERNAL_DIR}/board/nexora-x86_64/linux.fragment"

set -- "$1"/build/linux-*/.config
if [ ! -f "$1" ]; then
	echo "no kernel .config found; run 'make linux-configure' first" >&2
	exit 2
fi
KCONFIG="$1"

checked=0
missing=0
while IFS= read -r line; do
	case "${line}" in
	CONFIG_*=*)
		sym=${line%%=*}
		if ! grep -qxF "${line}" "${KCONFIG}"; then
			actual=$(grep -E "^(# )?${sym}[= ]" "${KCONFIG}" || true)
			echo "MISMATCH: ${line}  ->  ${actual:-<symbol absent>}"
			missing=$((missing + 1))
		fi
		checked=$((checked + 1))
		;;
	esac
done < "${FRAGMENT}"

echo "linux.fragment: ${checked} options checked against ${KCONFIG}, ${missing} problem(s)"
[ "${missing}" -eq 0 ]
