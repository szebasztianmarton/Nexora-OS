#!/bin/sh
# Verify that every option of a Nexora defconfig survives Kconfig.
#
# Kconfig silently drops symbols that do not exist or whose dependencies
# are not met. This script loads the defconfig into a scratch output
# directory and checks that each line of the defconfig is present,
# unchanged, in the resulting .config.
#
# Usage: check-defconfig.sh <buildroot-dir> <output-dir> [defconfig-name]
#   defconfig-name defaults to nexora_x86_64_defconfig.

set -eu

if [ $# -lt 2 ]; then
	echo "usage: $0 <buildroot-dir> <output-dir> [defconfig-name]" >&2
	exit 2
fi

BR_DIR=$(cd "$1" && pwd)
mkdir -p "$2"
OUT_DIR=$(cd "$2" && pwd)
NAME=${3:-nexora_x86_64_defconfig}
EXTERNAL_DIR=$(cd "$(dirname "$0")/.." && pwd)
DEFCONFIG="${EXTERNAL_DIR}/configs/${NAME}"

[ -f "${DEFCONFIG}" ] || { echo "no such defconfig: ${DEFCONFIG}" >&2; exit 2; }

make -s -C "${BR_DIR}" BR2_EXTERNAL="${EXTERNAL_DIR}" O="${OUT_DIR}" "${NAME}"

CONFIG="${OUT_DIR}/.config"
checked=0
missing=0
while IFS= read -r line; do
	case "${line}" in
	"" | "#"[!\ ]* | "# "[!B]*)
		continue
		;;
	"# BR2_"*" is not set")
		sym=${line#\# }
		sym=${sym%% *}
		if grep -q "^${sym}=" "${CONFIG}"; then
			echo "MISMATCH: ${line}  ->  $(grep "^${sym}=" "${CONFIG}")"
			missing=$((missing + 1))
		fi
		;;
	BR2_*=*)
		sym=${line%%=*}
		if ! grep -qxF "${line}" "${CONFIG}"; then
			actual=$(grep -E "^(# )?${sym}[= ]" "${CONFIG}" || true)
			echo "MISMATCH: ${line}  ->  ${actual:-<symbol absent>}"
			missing=$((missing + 1))
		fi
		;;
	*)
		echo "UNPARSED: ${line}"
		missing=$((missing + 1))
		continue
		;;
	esac
	checked=$((checked + 1))
done < "${DEFCONFIG}"

echo "${NAME}: ${checked} options checked, ${missing} problem(s)"
[ "${missing}" -eq 0 ]
