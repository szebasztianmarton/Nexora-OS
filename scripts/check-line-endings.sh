#!/bin/sh
# Fails when a tracked text file has CRLF line endings, or a mix of LF and
# CRLF, in the index (i/crlf, i/mixed) or in the work tree (w/crlf, w/mixed).
# A CRLF shell script breaks the Buildroot image build; on Windows, set
# "git config core.autocrlf false" in this repository (see
# docs/testing-windows11.md).
#   scripts/check-line-endings.sh
# Exit status: 0 clean, 1 CRLF or mixed files found, 77 not a usable git checkout
# (for example a source archive), reported as skipped by CTest.
set -eu

cd "$(dirname "$0")/.."
if ! git rev-parse --is-inside-work-tree > /dev/null 2>&1; then
    echo "check-line-endings.sh: not a git checkout; skipped" >&2
    exit 77
fi

crlf=$(git ls-files --eol | grep -E '(^|[[:space:]])[iw]/(crlf|mixed)[[:space:]]' || true)
if [ -n "${crlf}" ]; then
    echo "check-line-endings.sh: files with CRLF or mixed line endings:" >&2
    printf '%s\n' "${crlf}" >&2
    echo "fix: git config core.autocrlf false, then delete each listed file and" \
        "check it out again (git checkout -- FILE)" >&2
    exit 1
fi
echo "check-line-endings.sh: ok"
