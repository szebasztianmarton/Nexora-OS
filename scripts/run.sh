#!/usr/bin/env bash
# Build (if needed) and start the Nexora console UI from the build tree.
#   scripts/run.sh [debug|release|ci] [-- app arguments...]
# Examples:
#   scripts/run.sh
#   scripts/run.sh release -- --fullscreen
#   QT_QPA_PLATFORM=offscreen scripts/run.sh   # headless smoke run
set -euo pipefail

preset=debug
if [[ $# -gt 0 && "$1" != "--" ]]; then
    preset="$1"
    shift
fi
[[ $# -gt 0 && "$1" == "--" ]] && shift

root="$(cd "$(dirname "$0")/.." && pwd)"
binary="$root/build/$preset/bin/nexora-console-ui"

if [[ ! -x "$binary" ]]; then
    "$root/scripts/build.sh" "$preset"
else
    (cd "$root" && cmake --build --preset "$preset" >/dev/null)
fi

if [[ $(id -u) -eq 0 ]] && [[ " $* " != *" --allow-root "* ]] && [[ "${NEXORA_ALLOW_ROOT:-}" != 1 ]]; then
    echo "note: running as root; Nexora refuses this unless you pass '-- --allow-root'." >&2
fi

exec "$binary" "$@"
