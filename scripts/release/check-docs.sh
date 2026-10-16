#!/usr/bin/env bash
# Documentation check run by CI and before a release.
#   scripts/release/check-docs.sh [ROOT]
# ROOT defaults to the repository root. Fails (exit 1) when
#   - a document from the required list is missing or empty, or
#   - a relative Markdown link (inline, image or reference style) in README.md or any
#     docs/**/*.md file points at a file or directory that does not exist.
# Links with a URL scheme (https:, mailto:, ...) and pure in-page anchors (#section) are
# not checked; the #fragment of a relative link is ignored. Links inside fenced code
# blocks and inline code spans are ignored.
set -euo pipefail

usage() { echo "usage: $0 [ROOT]" >&2; exit 64; }
[[ $# -le 1 ]] || usage
[[ "${1:-}" != "-h" && "${1:-}" != "--help" ]] || usage

root="${1:-$(cd "$(dirname "$0")/../.." && pwd)}"
[[ -d "$root" ]] || { echo "error: '$root' is not a directory" >&2; exit 64; }
root="$(cd "$root" && pwd)"

# Required documentation set (docs/superpowers/plans/2026-10-11-mvp4.md, Task 13).
required=(
    README.md
    docs/architecture.md
    docs/roadmap.md
    docs/mvp-4.md
    docs/building.md
    docs/installation.md
    docs/hardware-compatibility.md
    docs/controller-support.md
    docs/game-library.md
    docs/emulator-integration.md
    docs/user-data.md
    docs/update-system.md
    docs/recovery.md
    docs/security.md
    docs/testing.md
    docs/release-process.md
    docs/troubleshooting.md
    docs/design-system.md
    docs/buildroot.md
    docs/settings.md
    docs/development.md
)

status=0
missing=0
for doc in "${required[@]}"; do
    if [[ ! -f "$root/$doc" ]]; then
        echo "::error file=$doc::required document is missing"
        missing=$((missing + 1))
        status=1
    elif ! grep -q '[^[:space:]]' "$root/$doc"; then
        echo "::error file=$doc::required document is empty"
        missing=$((missing + 1))
        status=1
    fi
done

# Prints "<line><TAB><target>" for every link target in a Markdown file.
extract_links() {
    awk '
        /^[[:space:]]*(```|~~~)/ { fence = !fence; next }
        fence { next }
        {
            line = $0
            gsub(/`[^`]*`/, "", line)
            # Reference definitions: [label]: target "title"
            if (match(line, /^[ ]?[ ]?[ ]?\[[^]]+\]:[[:space:]]*[^[:space:]]+/)) {
                def = substr(line, RSTART, RLENGTH)
                sub(/^[ ]*\[[^]]+\]:[[:space:]]*/, "", def)
                sub(/^</, "", def); sub(/>$/, "", def)
                printf "%d\t%s\n", NR, def
                next
            }
            # Inline links and images: [text](target "title")
            while (match(line, /\]\([^)]*\)/)) {
                target = substr(line, RSTART + 2, RLENGTH - 3)
                line = substr(line, RSTART + RLENGTH)
                sub(/^[[:space:]]+/, "", target)
                if (target ~ /^</) { sub(/^</, "", target); sub(/>.*$/, "", target) }
                else { sub(/[[:space:]].*$/, "", target) }
                printf "%d\t%s\n", NR, target
            }
        }
    ' "$1"
}

# Decodes %XX escapes (e.g. %20) in a link target.
url_decode() { printf '%b' "${1//%/\\x}"; }

broken=0
mapfile -t md_files < <(
    cd "$root"
    if [[ -f README.md ]]; then echo README.md; fi
    if [[ -d docs ]]; then find docs -type f -name '*.md'; fi | LC_ALL=C sort
)
for md in "${md_files[@]}"; do
    dir="$(dirname "$root/$md")"
    while IFS=$'\t' read -r lineno target; do
        [[ -n "$target" ]] || continue
        case "$target" in
            \#*) continue ;;                          # in-page anchor
            *://* | mailto:* | tel:* | data:*) continue ;; # absolute URL
        esac
        path="${target%%#*}"
        path="${path%%\?*}"
        [[ -n "$path" ]] || continue
        path="$(url_decode "$path")"
        if [[ "$path" == /* ]]; then
            resolved="$root$path" # site-absolute: relative to the repository root
        else
            resolved="$dir/$path"
        fi
        if [[ ! -e "$resolved" ]]; then
            echo "::error file=$md,line=$lineno::broken link: $target"
            broken=$((broken + 1))
            status=1
        fi
    done < <(extract_links "$root/$md")
done

echo "check-docs: ${#required[@]} required documents ($missing missing or empty)," \
    "${#md_files[@]} Markdown files scanned ($broken broken links)"
exit "$status"
