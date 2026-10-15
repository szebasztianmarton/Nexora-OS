#!/usr/bin/env bash
# Signs an update bundle (or any single release file) with an Ed25519 private key.
#   scripts/release/sign-update.sh --key PRIVATE.pem --bundle DIR [--pubkey PUBLIC.pem]
#   scripts/release/sign-update.sh --key PRIVATE.pem --file FILE  [--pubkey PUBLIC.pem]
#
# --bundle DIR  writes DIR/manifest.json.sig: the raw 64-byte Ed25519 signature over the
#               exact bytes of DIR/manifest.json (the format nexora-update-tool verifies,
#               see docs/update-system.md).
# --file FILE   writes FILE.sig the same way (used for SHA256SUMS of a release).
# --pubkey PEM  after signing, verify with this public key instead of the one derived from
#               the private key, proving the signature matches the key shipped in the image.
#
# The private key must live outside every git work tree (in particular outside this
# repository): keys are kept offline and must never be committed by accident. The script
# refuses to run otherwise. Nothing is printed about the key except its public half.
set -euo pipefail

usage() {
    echo "usage: $0 --key PRIVATE.pem (--bundle DIR | --file FILE) [--pubkey PUBLIC.pem]" >&2
    exit 64
}
die() { echo "sign-update: error: $*" >&2; exit 1; }

key="" bundle="" file="" pubkey=""
while [[ $# -gt 0 ]]; do
    case "$1" in
        --key) [[ $# -ge 2 ]] || usage; key="$2"; shift 2 ;;
        --bundle) [[ $# -ge 2 ]] || usage; bundle="$2"; shift 2 ;;
        --file) [[ $# -ge 2 ]] || usage; file="$2"; shift 2 ;;
        --pubkey) [[ $# -ge 2 ]] || usage; pubkey="$2"; shift 2 ;;
        -h | --help) usage ;;
        *) echo "unknown argument: $1" >&2; usage ;;
    esac
done
[[ -n "$key" ]] || usage
if [[ -n "$bundle" && -n "$file" ]] || [[ -z "$bundle" && -z "$file" ]]; then usage; fi

command -v openssl >/dev/null 2>&1 || die "openssl not found"
[[ -f "$key" ]] || die "key file '$key' does not exist"
[[ -r "$key" ]] || die "key file '$key' is not readable"

# --- refuse keys inside a git work tree --------------------------------------------------
key_real="$(realpath -e -- "$key")"
repo_root="$(cd "$(dirname "$0")/../.." && pwd -P)"
case "$key_real" in
    "$repo_root"/*) die "refusing to use a private key inside the repository ($repo_root); keep signing keys offline, outside any git work tree" ;;
esac
if command -v git >/dev/null 2>&1; then
    if key_tree="$(git -C "$(dirname "$key_real")" rev-parse --show-toplevel 2>/dev/null)"; then
        die "refusing to use a private key inside the git work tree $key_tree; keep signing keys offline, outside any git work tree"
    fi
fi

perm="$(stat -c '%a' "$key_real")"
if [[ "${perm: -2}" != "00" ]]; then
    echo "sign-update: warning: $key is readable by group/others (mode $perm); chmod 600 it" >&2
fi

if ! openssl pkey -in "$key_real" -noout -text 2>/dev/null | head -n 1 | grep -q 'ED25519 Private-Key'; then
    die "'$key' is not an Ed25519 private key in PEM format"
fi

# --- what to sign ---------------------------------------------------------------------
if [[ -n "$bundle" ]]; then
    [[ -d "$bundle" ]] || die "bundle directory '$bundle' does not exist"
    message="$bundle/manifest.json"
    [[ -f "$message" ]] || die "'$message' does not exist"
    msg_size="$(stat -c '%s' "$message")"
    [[ "$msg_size" -le 65536 ]] || die "manifest.json is larger than 64 KiB ($msg_size bytes)"
else
    message="$file"
    [[ -f "$message" ]] || die "'$message' does not exist"
fi
signature="$message.sig"

workdir="$(mktemp -d)"
trap 'rm -rf "$workdir"' EXIT

if [[ -n "$pubkey" ]]; then
    [[ -f "$pubkey" ]] || die "public key '$pubkey' does not exist"
    if ! openssl pkey -pubin -in "$pubkey" -noout -text 2>/dev/null | head -n 1 | grep -q 'ED25519 Public-Key'; then
        die "'$pubkey' is not an Ed25519 public key in PEM format"
    fi
    verify_key="$pubkey"
else
    verify_key="$workdir/public.pem"
    openssl pkey -in "$key_real" -pubout -out "$verify_key"
fi

# Sign into a temporary file first so a failure never leaves a truncated signature behind.
openssl pkeyutl -sign -inkey "$key_real" -rawin -in "$message" -out "$workdir/sig"
sig_size="$(stat -c '%s' "$workdir/sig")"
[[ "$sig_size" -eq 64 ]] || die "unexpected signature size $sig_size (expected 64 bytes)"

if ! openssl pkeyutl -verify -pubin -inkey "$verify_key" -rawin -in "$message" \
    -sigfile "$workdir/sig" >/dev/null 2>&1; then
    die "signature does not verify with $( [[ -n "$pubkey" ]] && echo "$pubkey" || echo "the derived public key")"
fi

cp "$workdir/sig" "$signature.tmp"
mv -f "$signature.tmp" "$signature"
echo "signed $message -> $signature"
