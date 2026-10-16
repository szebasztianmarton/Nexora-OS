#!/usr/bin/env bash
# Lints every QML file of the main tree with qmllint.
#   scripts/qmllint.sh BUILD_DIR
#
# BUILD_DIR is a configured and built tree (e.g. build/ci): its qml/ directory holds the
# generated Nexora.Console module (qmldir + C++ type information) that the files import.
# QML files are discovered by glob (apps/**/*.qml, tests/qml/**/*.qml; prototypes/ is not
# part of the main build and is skipped), so new files are picked up automatically.
#
# Exit status: 1 when qmllint reports an error ("critical": syntax errors, unreadable
# files) for any file; warnings (unqualified access, unresolved types, ...) are printed and
# counted but do not fail. QMLLINT overrides the qmllint binary.
set -euo pipefail

usage() { echo "usage: $0 BUILD_DIR" >&2; exit 64; }
[[ $# -eq 1 ]] || usage
build="$1"
[[ -d "$build" ]] || { echo "error: build directory '$build' does not exist" >&2; exit 64; }
build="$(cd "$build" && pwd)"
root="$(cd "$(dirname "$0")/.." && pwd)"

qmllint="${QMLLINT:-}"
if [[ -z "$qmllint" ]]; then
    for candidate in qmllint6 qmllint /usr/lib/qt6/bin/qmllint /usr/lib/x86_64-linux-gnu/qt6/bin/qmllint; do
        if command -v "$candidate" >/dev/null 2>&1; then
            qmllint="$(command -v "$candidate")"
            break
        fi
    done
fi
[[ -n "$qmllint" ]] || { echo "error: qmllint not found (Ubuntu: qt6-declarative-dev-tools)" >&2; exit 1; }
command -v python3 >/dev/null 2>&1 || { echo "error: python3 not found" >&2; exit 1; }

cd "$root"
shopt -s globstar nullglob
files=(apps/**/*.qml tests/qml/**/*.qml)
[[ ${#files[@]} -gt 0 ]] || { echo "error: no QML files found" >&2; exit 1; }

imports=()
if [[ -d "$build/qml" ]]; then
    imports+=(-I "$build/qml")
else
    echo "warning: $build/qml does not exist; Nexora.Console imports will not resolve" >&2
fi

json="$(mktemp)"
trap 'rm -f "$json"' EXIT
echo "qmllint: $("$qmllint" --version 2>/dev/null | head -n 1), ${#files[@]} files"
# qmllint's own exit status mixes warnings and errors; classify from the JSON report.
"$qmllint" --json "$json" "${imports[@]}" "${files[@]}" >/dev/null 2>&1 || true

python3 -I - "$json" "$root" <<'PY'
import json, os, sys

path, root = sys.argv[1], sys.argv[2]
try:
    with open(path, encoding="utf-8") as f:
        report = json.load(f)
except (OSError, ValueError) as e:
    print(f"::error::qmllint produced no readable JSON report ({e})")
    sys.exit(1)

errors = warnings = infos = 0
for entry in report.get("files", []):
    name = os.path.relpath(entry.get("filename", "?"), root)
    # "success" is false for any warning, so it is not used; syntax errors and unreadable
    # files are reported as messages of type "critical".
    for m in entry.get("warnings", []):
        kind = m.get("type", "warning")
        line = m.get("line", 0)
        text = m.get("message", "").strip().replace("\n", " ")
        if kind == "critical":
            errors += 1
            print(f"::error file={name},line={line}::{text}")
        elif kind == "warning":
            warnings += 1
            print(f"::warning file={name},line={line}::{text}")
        else:
            infos += 1
print(f"qmllint: {errors} errors, {warnings} warnings, {infos} infos")
sys.exit(1 if errors else 0)
PY
