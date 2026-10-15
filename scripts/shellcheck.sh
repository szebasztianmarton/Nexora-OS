#!/usr/bin/env bash
# Runs shellcheck over every shell script of the main tree.
#   scripts/shellcheck.sh
#
# Discovered by glob and shebang, so new scripts are picked up automatically:
#   scripts/**/*.sh, tests/**/*.sh            bash (or whatever their shebang says)
#   buildroot-external/**                     every file with a shell shebang, including
#                                             the rootfs-overlay init.d and libexec scripts
#                                             without a .sh suffix; '#!/bin/sh' scripts are
#                                             checked strictly as POSIX sh (-s sh) because
#                                             the image runs them with BusyBox ash.
# prototypes/ is not part of the main tree and is skipped.
set -euo pipefail

command -v shellcheck >/dev/null 2>&1 || { echo "error: shellcheck not found" >&2; exit 1; }
cd "$(dirname "$0")/.."
shopt -s globstar nullglob

posix=()
bash_scripts=()
classify() {
    local f="$1" first
    [[ -f "$f" ]] || return 0
    IFS= read -r first < "$f" || true
    case "$first" in
        '#!/bin/sh'* | '#!/usr/bin/env sh'* | '#!/bin/ash'* | '#!/bin/busybox sh'*) posix+=("$f") ;;
        '#!'*bash*) bash_scripts+=("$f") ;;
        *)
            # A *.sh file without a recognised shebang is still a shell script.
            [[ "$f" == *.sh ]] && bash_scripts+=("$f")
            ;;
    esac
    return 0
}

for f in scripts/**/*.sh tests/**/*.sh buildroot-external/**; do
    classify "$f"
done

status=0
if [[ ${#posix[@]} -gt 0 ]]; then
    echo "shellcheck -s sh: ${#posix[@]} POSIX scripts"
    shellcheck -s sh "${posix[@]}" || status=1
fi
if [[ ${#bash_scripts[@]} -gt 0 ]]; then
    echo "shellcheck -s bash: ${#bash_scripts[@]} bash scripts"
    shellcheck -s bash "${bash_scripts[@]}" || status=1
fi
exit "$status"
