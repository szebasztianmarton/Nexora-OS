#!/usr/bin/env bash
# Exercises scripts/release/{sign-update,verify-release,check-docs}.sh with a throwaway
# Ed25519 key generated in a temporary directory (never inside the repository, except for
# the one negative check that sign-update.sh refuses such a key, which removes it again).
set -euo pipefail

repo="$(cd "$(dirname "$0")/../.." && pwd -P)"
sign="$repo/scripts/release/sign-update.sh"
verify="$repo/scripts/release/verify-release.sh"
check_docs="$repo/scripts/release/check-docs.sh"

command -v openssl >/dev/null 2>&1 || { echo "openssl not found" >&2; exit 1; }

tmp="$(mktemp -d "${TMPDIR:-/tmp}/nexora-release-test.XXXXXX")"
repo_key=""
cleanup() {
    rm -rf "$tmp"
    if [[ -n "$repo_key" ]]; then rm -f "$repo_key"; fi
}
trap cleanup EXIT

failures=0
pass() { echo "PASS: $*"; }
failed() { echo "FAIL: $*" >&2; failures=$((failures + 1)); }
expect_ok() {
    local what="$1"; shift
    if "$@" > "$tmp/out.log" 2>&1; then pass "$what"; else failed "$what"; cat "$tmp/out.log" >&2; fi
}
expect_fail() {
    local what="$1"; shift
    if "$@" > "$tmp/out.log" 2>&1; then failed "$what (unexpectedly succeeded)"; cat "$tmp/out.log" >&2; else pass "$what"; fi
}

# --- keys -------------------------------------------------------------------------------
umask 077
openssl genpkey -algorithm ED25519 -out "$tmp/key.pem" 2>/dev/null
openssl pkey -in "$tmp/key.pem" -pubout -out "$tmp/pub.pem"
openssl genpkey -algorithm ED25519 -out "$tmp/other.pem" 2>/dev/null
openssl pkey -in "$tmp/other.pem" -pubout -out "$tmp/other-pub.pem"
openssl genpkey -algorithm RSA -pkeyopt rsa_keygen_bits:2048 -out "$tmp/rsa.pem" 2>/dev/null
umask 022

# --- update bundle signing ----------------------------------------------------------------
bundle="$tmp/nexora-update-0.4.1"
mkdir -p "$bundle"
head -c 4096 /dev/urandom > "$bundle/rootfs.ext4"
printf '{"format":1,"product":"nexora-os","version":"0.4.1"}\n' > "$bundle/manifest.json"

expect_ok "sign-update signs a bundle" "$sign" --key "$tmp/key.pem" --bundle "$bundle"
if [[ -f "$bundle/manifest.json.sig" && "$(stat -c '%s' "$bundle/manifest.json.sig")" -eq 64 ]]; then
    pass "manifest.json.sig is a 64-byte raw signature"
else
    failed "manifest.json.sig missing or not 64 bytes"
fi
expect_ok "bundle signature verifies with openssl" \
    openssl pkeyutl -verify -pubin -inkey "$tmp/pub.pem" -rawin \
    -in "$bundle/manifest.json" -sigfile "$bundle/manifest.json.sig"
expect_ok "sign-update accepts the matching --pubkey" \
    "$sign" --key "$tmp/key.pem" --bundle "$bundle" --pubkey "$tmp/pub.pem"
cp "$bundle/manifest.json.sig" "$tmp/good.sig"
expect_fail "sign-update rejects a non-matching --pubkey" \
    "$sign" --key "$tmp/key.pem" --bundle "$bundle" --pubkey "$tmp/other-pub.pem"
if cmp -s "$tmp/good.sig" "$bundle/manifest.json.sig"; then
    pass "a failed signing run leaves the existing signature untouched"
else
    failed "a failed signing run replaced the existing signature"
fi
expect_fail "sign-update rejects a non-Ed25519 key" "$sign" --key "$tmp/rsa.pem" --bundle "$bundle"
expect_fail "sign-update rejects a bundle without manifest.json" \
    "$sign" --key "$tmp/key.pem" --bundle "$tmp/does-not-exist"
printf '{"format":1,"product":"nexora-os","version":"9.9.9"}\n' > "$bundle/manifest.json"
expect_fail "tampered manifest no longer verifies" \
    openssl pkeyutl -verify -pubin -inkey "$tmp/pub.pem" -rawin \
    -in "$bundle/manifest.json" -sigfile "$bundle/manifest.json.sig"

# --- keys inside a git work tree are refused -------------------------------------------------
repo_key="$(mktemp "$repo/.nexora-test-key.XXXXXX")"
cp "$tmp/key.pem" "$repo_key"
expect_fail "sign-update refuses a key inside the repository" \
    "$sign" --key "$repo_key" --bundle "$bundle"
rm -f "$repo_key"
repo_key=""
if command -v git >/dev/null 2>&1; then
    git init -q "$tmp/other-repo"
    cp "$tmp/key.pem" "$tmp/other-repo/key.pem"
    expect_fail "sign-update refuses a key inside any git work tree" \
        "$sign" --key "$tmp/other-repo/key.pem" --bundle "$bundle"
fi

# --- release SHA256SUMS signing + verification ---------------------------------------------
rel="$tmp/dist"
mkdir -p "$rel/nexora-update-0.4.1"
echo "archive" > "$rel/nexora-os-0.4.1-linux-x86_64.tar.gz"
echo "image" > "$rel/nexora-update-0.4.1/rootfs.ext4"
(cd "$rel" && sha256sum nexora-os-0.4.1-linux-x86_64.tar.gz nexora-update-0.4.1/rootfs.ext4 > SHA256SUMS)

expect_ok "verify-release accepts an unsigned release without --pubkey" "$verify" "$rel"
expect_fail "verify-release with --pubkey requires a signature" "$verify" "$rel" --pubkey "$tmp/pub.pem"
expect_ok "sign-update signs SHA256SUMS" "$sign" --key "$tmp/key.pem" --file "$rel/SHA256SUMS"
expect_ok "verify-release accepts the signed release" "$verify" "$rel" --pubkey "$tmp/pub.pem"
expect_fail "verify-release refuses a signed release without --pubkey" "$verify" "$rel"
expect_fail "verify-release rejects the wrong public key" "$verify" "$rel" --pubkey "$tmp/other-pub.pem"

cp "$rel/nexora-os-0.4.1-linux-x86_64.tar.gz" "$tmp/archive.bak"
echo "tampered" >> "$rel/nexora-os-0.4.1-linux-x86_64.tar.gz"
expect_fail "verify-release detects a tampered artifact" "$verify" "$rel" --pubkey "$tmp/pub.pem"
cp "$tmp/archive.bak" "$rel/nexora-os-0.4.1-linux-x86_64.tar.gz"

cp "$rel/SHA256SUMS" "$tmp/sums.bak"
(cd "$rel" && sha256sum nexora-os-0.4.1-linux-x86_64.tar.gz > SHA256SUMS)
expect_fail "verify-release detects a modified SHA256SUMS (bad signature)" "$verify" "$rel" --pubkey "$tmp/pub.pem"
cp "$tmp/sums.bak" "$rel/SHA256SUMS"

rm "$rel/nexora-update-0.4.1/rootfs.ext4"
expect_fail "verify-release detects a missing artifact" "$verify" "$rel" --pubkey "$tmp/pub.pem"
echo "image" > "$rel/nexora-update-0.4.1/rootfs.ext4"
expect_ok "verify-release passes again after restoring everything" "$verify" "$rel" --pubkey "$tmp/pub.pem"

unsafe="$tmp/unsafe"
mkdir -p "$unsafe"
echo "x" > "$tmp/outside"
printf '%s  ../outside\n' "$(sha256sum < "$tmp/outside" | cut -d' ' -f1)" > "$unsafe/SHA256SUMS"
expect_fail "verify-release rejects '..' paths in SHA256SUMS" "$verify" "$unsafe"

# --- check-docs ---------------------------------------------------------------------------
docs_root="$tmp/docs-tree"
mkdir -p "$docs_root/docs/images"
for doc in architecture roadmap mvp-4 building installation hardware-compatibility controller-support \
    game-library emulator-integration user-data update-system recovery security testing \
    release-process troubleshooting design-system buildroot settings development; do
    echo "# $doc" > "$docs_root/docs/$doc.md"
done
touch "$docs_root/docs/images/shot.png"
cat > "$docs_root/README.md" <<'MD'
# Nexora
See [architecture](docs/architecture.md#overview), [web](https://example.org/x.md),
![shot](docs/images/shot.png "Screenshot"), [anchor](#nexora) and [ref][r].

```text
[not a link](docs/missing-in-code-block.md)
```
Inline `[code](docs/missing-inline.md)` is ignored too.

[r]: docs/building.md
MD
echo "Back to [the README](../README.md) and [testing](testing.md)." >> "$docs_root/docs/building.md"
expect_ok "check-docs passes on a complete tree" "$check_docs" "$docs_root"

echo "A [broken link](no-such-file.md)." >> "$docs_root/docs/security.md"
expect_fail "check-docs detects a broken relative link" "$check_docs" "$docs_root"
if grep -q 'docs/security.md,line=2::broken link: no-such-file.md' "$tmp/out.log"; then
    pass "check-docs reports the file and line of the broken link"
else
    failed "check-docs did not report docs/security.md line 2"; cat "$tmp/out.log" >&2
fi
echo "# security" > "$docs_root/docs/security.md"

echo "[bad ref]: missing-ref.md" >> "$docs_root/docs/testing.md"
expect_fail "check-docs detects a broken reference-style link" "$check_docs" "$docs_root"
echo "# testing" > "$docs_root/docs/testing.md"

rm "$docs_root/docs/recovery.md"
expect_fail "check-docs detects a missing required document" "$check_docs" "$docs_root"
: > "$docs_root/docs/recovery.md"
expect_fail "check-docs detects an empty required document" "$check_docs" "$docs_root"
echo "# recovery" > "$docs_root/docs/recovery.md"
expect_ok "check-docs passes again after the fixes" "$check_docs" "$docs_root"

# --- the rootfs image must carry the release version -------------------------------------
check_image="$repo/scripts/release/check-image-version.sh"
mkfs_ext4="$(PATH="$PATH:/sbin:/usr/sbin" command -v mkfs.ext4 || true)"
debugfs_bin="$(PATH="$PATH:/sbin:/usr/sbin" command -v debugfs || true)"
if [[ -n "$mkfs_ext4" && -n "$debugfs_bin" ]]; then
    mkdir -p "$tmp/rootfs/etc"
    printf 'NAME="Nexora OS"\nNEXORA_VERSION="0.4.1"\nNEXORA_BUILD_ID="v0.4.1-0-gtest"\n' \
        > "$tmp/rootfs/etc/nexora-release"
    "$mkfs_ext4" -q -F -d "$tmp/rootfs" "$tmp/rootfs.ext4" 4M > /dev/null 2>&1
    expect_ok "check-image-version accepts the matching version" "$check_image" "$tmp/rootfs.ext4" 0.4.1
    expect_fail "check-image-version refuses a stale image" "$check_image" "$tmp/rootfs.ext4" 0.4.2
    rm "$tmp/rootfs/etc/nexora-release"
    "$mkfs_ext4" -q -F -d "$tmp/rootfs" "$tmp/rootfs-norelease.ext4" 4M > /dev/null 2>&1
    expect_fail "check-image-version refuses an image without /etc/nexora-release" \
        "$check_image" "$tmp/rootfs-norelease.ext4" 0.4.1
    expect_fail "check-image-version refuses a file that is not ext4" "$check_image" "$bundle/rootfs.ext4" 0.4.1
else
    echo "SKIP: mkfs.ext4/debugfs not found; check-image-version not exercised"
fi
# shellcheck disable=SC2016 # literal text of make-release.sh
if grep -qF 'check-image-version.sh" "$br_images/rootfs.ext4" "$version"' "$repo/scripts/release/make-release.sh"; then
    pass "make-release checks the image version before packaging it"
else
    failed "make-release does not run check-image-version.sh on rootfs.ext4"
fi

# --- check-line-endings ----------------------------------------------------------------
# A copy of the script in a scratch git repository (it checks the tree it lives in).
if command -v git >/dev/null 2>&1; then
    eol_root="$tmp/eol-repo"
    mkdir -p "$eol_root/scripts"
    cp "$repo/scripts/check-line-endings.sh" "$eol_root/scripts/"
    printf 'echo one\necho two\n' > "$eol_root/lf.sh"
    git -C "$eol_root" init -q
    git -C "$eol_root" config core.autocrlf false
    git -C "$eol_root" add .
    git -C "$eol_root" -c user.name=test -c user.email=test@example.invalid commit -qm lf
    expect_ok "check-line-endings passes on LF files" sh "$eol_root/scripts/check-line-endings.sh"
    printf 'echo one\r\necho two\r\n' > "$eol_root/lf.sh"
    expect_fail "check-line-endings detects a CRLF file" sh "$eol_root/scripts/check-line-endings.sh"
    printf 'echo one\necho two\r\n' > "$eol_root/lf.sh"
    expect_fail "check-line-endings detects mixed line endings in the work tree" \
        sh "$eol_root/scripts/check-line-endings.sh"
    git -C "$eol_root" add lf.sh
    git -C "$eol_root" -c user.name=test -c user.email=test@example.invalid commit -qm mixed
    expect_fail "check-line-endings detects mixed line endings in the index" \
        sh "$eol_root/scripts/check-line-endings.sh"
else
    echo "SKIP: git not found; check-line-endings not exercised"
fi

echo
if [[ $failures -ne 0 ]]; then
    echo "test_release_scripts.sh: $failures check(s) failed" >&2
    exit 1
fi
echo "test_release_scripts.sh: ok"
