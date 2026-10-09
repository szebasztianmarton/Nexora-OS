#!/usr/bin/env bash
# Configure and build Nexora OS with a CMake preset; optionally run the test suite.
#   scripts/build.sh [debug|release|ci] [--test]
set -euo pipefail

usage() { echo "usage: $0 [debug|release|ci] [--test]" >&2; exit 64; }

preset=debug
run_tests=0
for arg in "$@"; do
    case "$arg" in
        debug|release|ci) preset="$arg" ;;
        --test) run_tests=1 ;;
        -h|--help) usage ;;
        *) echo "unknown argument: $arg" >&2; usage ;;
    esac
done

for tool in cmake ninja; do
    command -v "$tool" >/dev/null 2>&1 || { echo "error: '$tool' not found; see docs/development.md" >&2; exit 1; }
done
command -v c++ >/dev/null 2>&1 || command -v g++ >/dev/null 2>&1 || command -v clang++ >/dev/null 2>&1 \
    || { echo "error: no C++ compiler found; see docs/development.md" >&2; exit 1; }

cd "$(dirname "$0")/.."
cmake --preset "$preset"
cmake --build --preset "$preset"
if [[ $run_tests -eq 1 ]]; then
    ctest --preset "$preset"
fi
