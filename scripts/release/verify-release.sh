#!/usr/bin/env bash
# Verifies a release directory produced by make-release.sh.
#   scripts/release/verify-release.sh DIR [--pubkey PUBLIC.pem]
#
# 1. Every entry of DIR/SHA256SUMS must be a plain relative path (no absolute paths, no
#    '..'), and every listed file must exist and match (sha256sum -c --strict).
# 2. If DIR/SHA256SUMS.sig exists it must be a valid raw Ed25519 signature over the exact
#    bytes of SHA256SUMS by PUBLIC.pem. A signature without --pubkey is an error (it cannot
#    be checked), and --pubkey without a signature is an error (a signed release was
#    expected).
# Exit status: 0 verified, 1 verification failed, 64 usage error.
set -euo pipefail

usage() { echo "usage: $0 DIR [--pubkey PUBLIC.pem]" >&2; exit 64; }
fail() { echo "verify-release: FAILED: $*" >&2; exit 1; }

dir="" pubkey=""
while [[ $# -gt 0 ]]; do
    case "$1" in
        --pubkey) [[ $# -ge 2 ]] || usage; pubkey="$2"; shift 2 ;;
        -h | --help) usage ;;
        -*) echo "unknown argument: $1" >&2; usage ;;
        *) [[ -z "$dir" ]] || usage; dir="$1"; shift ;;
    esac
done
[[ -n "$dir" ]] || usage
[[ -d "$dir" ]] || fail "'$dir' is not a directory"
[[ -f "$dir/SHA256SUMS" ]] || fail "'$dir/SHA256SUMS' does not exist"
if [[ -n "$pubkey" ]]; then
    [[ -f "$pubkey" ]] || fail "public key '$pubkey' does not exist"
    pubkey="$(realpath -e -- "$pubkey")"
fi

cd "$dir"

# --- 1. checksums -----------------------------------------------------------------------
[[ -s SHA256SUMS ]] || fail "SHA256SUMS is empty"
lineno=0
while IFS= read -r line || [[ -n "$line" ]]; do
    lineno=$((lineno + 1))
    if [[ ! "$line" =~ ^[0-9a-f]{64}\ [\ *]([^[:cntrl:]]+)$ ]]; then
        fail "SHA256SUMS line $lineno is malformed"
    fi
    name="${BASH_REMATCH[1]}"
    case "/$name/" in
        //* | */../* | */./*) fail "SHA256SUMS line $lineno names an unsafe path: $name" ;;
    esac
done < SHA256SUMS

sha256sum --check --strict --quiet SHA256SUMS || fail "checksum mismatch or missing file"
echo "verify-release: $(wc -l < SHA256SUMS | tr -d ' ') checksums OK"

# --- 2. signature -----------------------------------------------------------------------
if [[ -f SHA256SUMS.sig ]]; then
    [[ -n "$pubkey" ]] || fail "SHA256SUMS.sig exists but no --pubkey was given to check it"
    command -v openssl >/dev/null 2>&1 || fail "openssl not found"
    if ! openssl pkey -pubin -in "$pubkey" -noout -text 2>/dev/null | head -n 1 | grep -q 'ED25519 Public-Key'; then
        fail "'$pubkey' is not an Ed25519 public key in PEM format"
    fi
    [[ "$(stat -c '%s' SHA256SUMS.sig)" -eq 64 ]] || fail "SHA256SUMS.sig is not a 64-byte Ed25519 signature"
    if ! openssl pkeyutl -verify -pubin -inkey "$pubkey" -rawin -in SHA256SUMS \
        -sigfile SHA256SUMS.sig >/dev/null 2>&1; then
        fail "SHA256SUMS.sig does not verify with $pubkey"
    fi
    echo "verify-release: signature OK ($pubkey)"
elif [[ -n "$pubkey" ]]; then
    fail "--pubkey given but SHA256SUMS.sig does not exist (unsigned release)"
else
    echo "verify-release: note: SHA256SUMS is not signed"
fi
echo "verify-release: OK"
