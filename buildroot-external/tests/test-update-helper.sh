#!/usr/bin/env bash
# Exercises the image's root update helper (rootfs-overlay/usr/libexec/nexora/
# nexora-updated) and the boot confirmation (nexora-bootok, found next to it)
# on the build host, in their test modes: the A/B root slots are
# 16 MiB scratch files, grubenv is a scratch file handled with the host's
# grub-editenv, and the bundles are the signed fixtures written by
# tst_updateservice (CTest fixture nexora_update_tool_fixture, the same one
# test_update_tool.sh uses). Nothing here needs root, loop devices or a VM.
#
# Required environment (set by tests/system/CMakeLists.txt):
#   NEXORA_UPDATED           path to nexora-updated
#   NEXORA_UPDATE_TOOL       path to the built nexora-update-tool
#   NEXORA_TEST_BUNDLE_DIR   good bundle (version 0.4.1, 2 MiB image)
#   NEXORA_TEST_BAD_BUNDLE_DIR  same bundle with the image's first byte flipped
#   NEXORA_TEST_KEYS_DIR     the trusted public key for both
set -euo pipefail

: "${NEXORA_UPDATED:?}"
NEXORA_BOOTOK=${NEXORA_BOOTOK:-$(dirname "$NEXORA_UPDATED")/nexora-bootok}
: "${NEXORA_UPDATE_TOOL:?}"
: "${NEXORA_TEST_BUNDLE_DIR:?}"
: "${NEXORA_TEST_BAD_BUNDLE_DIR:?}"
: "${NEXORA_TEST_KEYS_DIR:?}"

# The helper is written for BusyBox sh; use it when available, otherwise
# bash (which also implements "read -n"). dash cannot run it.
if command -v busybox > /dev/null 2>&1; then
    HELPER_SHELL=(busybox sh)
else
    HELPER_SHELL=(bash)
fi

tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

# Bundle paths must match the helper's request syntax, so use copies under
# the (mktemp-named) scratch directory rather than the build tree path.
cp -r "$NEXORA_TEST_BUNDLE_DIR" "$tmp/good"
cp -r "$NEXORA_TEST_BAD_BUNDLE_DIR" "$tmp/bad"
image="$tmp/good/rootfs.ext4"
image_size=$(wc -c < "$image")

failures=0
fail() {
    echo "FAIL: $*" >&2
    failures=$((failures + 1))
}

hash_of() { sha256sum "$1" | cut -d ' ' -f 1; }

env_get() { grub-editenv "$tmp/grubenv" list | sed -n "s/^$1=//p"; }

# reset [slot size in MiB] [running slot] [installed version]
reset() {
    local mib=${1:-16} running=${2:-A} version=${3:-0.4.0}
    rm -rf "$tmp/run" "$tmp/work" "$tmp/esp"
    mkdir -p "$tmp/run" "$tmp/esp"
    head -c $((mib * 1024 * 1024)) /dev/urandom > "$tmp/rootA.img"
    head -c $((mib * 1024 * 1024)) /dev/urandom > "$tmp/rootB.img"
    rm -f "$tmp/grubenv"
    grub-editenv "$tmp/grubenv" create
    grub-editenv "$tmp/grubenv" set nexora_slot="$running" nexora_try= nexora_tries=0
    printf 'NAME="Nexora OS"\nNEXORA_VERSION="%s"\nNEXORA_BUILD_ID="v%s-0-gtest"\n' \
        "$version" "$version" > "$tmp/nexora-release"
    printf 'BOOT_IMAGE=/boot/bzImage root=PARTUUID=x rootwait nexora.slot=%s quiet\n' \
        "$running" > "$tmp/cmdline"
    hashA=$(hash_of "$tmp/rootA.img")
    hashB=$(hash_of "$tmp/rootB.img")
}

# request LINE: runs "nexora-updated --once" and sends LINE through its FIFO.
# Sets rc (exit status) and status (content of the status file).
request() {
    local line=$1 pid _
    NEXORA_UPDATED_TEST=1 \
        NEXORA_TEST_RUN_DIR="$tmp/run" \
        NEXORA_TEST_WORK_DIR="$tmp/work" \
        NEXORA_TEST_KEYS_DIR="$NEXORA_TEST_KEYS_DIR" \
        NEXORA_TEST_UPDATE_TOOL="$NEXORA_UPDATE_TOOL" \
        NEXORA_TEST_SLOT_A="$tmp/rootA.img" \
        NEXORA_TEST_SLOT_B="$tmp/rootB.img" \
        NEXORA_TEST_GRUBENV="$tmp/grubenv" \
        NEXORA_TEST_PENDING_FILE="$tmp/esp/nexora/update-pending" \
        NEXORA_TEST_RELEASE_FILE="$tmp/nexora-release" \
        NEXORA_TEST_CMDLINE_FILE="$tmp/cmdline" \
        "${HELPER_SHELL[@]}" "$NEXORA_UPDATED" --once &
    pid=$!
    for _ in $(seq 1 100); do
        [ -p "$tmp/run/update-request" ] && break
        sleep 0.1
    done
    if [ ! -p "$tmp/run/update-request" ]; then
        kill "$pid" 2> /dev/null || true
        wait "$pid" || true
        rc=99
        status="(no FIFO)"
        return
    fi
    printf '%s\n' "$line" > "$tmp/run/update-request"
    rc=0
    wait "$pid" || rc=$?
    status=$(cat "$tmp/run/update-status" 2> /dev/null || echo "(no status)")
    echo "  request: $line -> exit $rc, status: $status"
}

expect_untouched() {
    [ "$(hash_of "$tmp/rootA.img")" = "$hashA" ] || fail "$1: rootA was modified"
    [ "$(hash_of "$tmp/rootB.img")" = "$hashB" ] || fail "$1: rootB was modified"
    [ -z "$(env_get nexora_try)" ] || fail "$1: nexora_try was set"
    [ ! -e "$tmp/esp/nexora/update-pending" ] || fail "$1: pending record written"
}

echo "== happy path: running A, install into B"
reset
head -c $((16 * 1024 * 1024)) "$tmp/rootB.img" | tail -c $((16 * 1024 * 1024 - image_size)) > "$tmp/rootB-tail.before"
request "install $tmp/good"
[ "$rc" -eq 0 ] || fail "happy path: exit status $rc"
[ "$status" = "ok 0.4.1" ] || fail "happy path: status '$status'"
[ "$(head -c "$image_size" "$tmp/rootB.img" | sha256sum | cut -d ' ' -f 1)" = "$(hash_of "$image")" ] ||
    fail "happy path: rootB does not start with the image"
[ "$(wc -c < "$tmp/rootB.img")" -eq $((16 * 1024 * 1024)) ] || fail "happy path: rootB was truncated"
tail -c $((16 * 1024 * 1024 - image_size)) "$tmp/rootB.img" | cmp -s - "$tmp/rootB-tail.before" ||
    fail "happy path: rootB beyond the image changed"
[ "$(hash_of "$tmp/rootA.img")" = "$hashA" ] || fail "happy path: rootA (running slot) was modified"
[ "$(env_get nexora_try)" = "B" ] || fail "happy path: nexora_try='$(env_get nexora_try)'"
[ "$(env_get nexora_tries)" = "3" ] || fail "happy path: nexora_tries='$(env_get nexora_tries)'"
[ "$(env_get nexora_slot)" = "A" ] || fail "happy path: nexora_slot='$(env_get nexora_slot)'"
grep -qx 'target=B' "$tmp/esp/nexora/update-pending" || fail "happy path: pending target"
grep -qx 'new_version=0.4.1' "$tmp/esp/nexora/update-pending" || fail "happy path: pending new_version"
grep -qx 'new_build=v0.4.1-0-gabcdef0' "$tmp/esp/nexora/update-pending" || fail "happy path: pending new_build"
grep -qx 'previous_version=0.4.0' "$tmp/esp/nexora/update-pending" || fail "happy path: pending previous_version"
[ ! -e "$tmp/work" ] || fail "happy path: work directory left behind"
[ ! -e "$tmp/run/update-request" ] || fail "happy path: FIFO left behind"

echo "== running B, install into A"
reset 16 B
request "install $tmp/good"
if [ "$rc" -ne 0 ] || [ "$status" != "ok 0.4.1" ]; then
    fail "slot A target: exit $rc, status '$status'"
fi
[ "$(head -c "$image_size" "$tmp/rootA.img" | sha256sum | cut -d ' ' -f 1)" = "$(hash_of "$image")" ] ||
    fail "slot A target: rootA does not start with the image"
[ "$(hash_of "$tmp/rootB.img")" = "$hashB" ] || fail "slot A target: rootB (running slot) was modified"
[ "$(env_get nexora_try)" = "A" ] || fail "slot A target: nexora_try='$(env_get nexora_try)'"

echo "== tampered bundle"
reset
request "install $tmp/bad"
[ "$rc" -ne 0 ] || fail "tampered: exit status 0"
case "$status" in "error "*) ;; *) fail "tampered: status '$status'" ;; esac
expect_untouched "tampered"

echo "== not newer than the installed version"
reset 16 A 0.4.1
request "install $tmp/good"
case "$status" in "error verification failed: NotNewer"*) ;; *) fail "not newer: status '$status'" ;; esac
expect_untouched "not newer"

echo "== image larger than the slot"
reset 1
request "install $tmp/good"
case "$status" in "error image"*"larger than slot B"*) ;; *) fail "too large: status '$status'" ;; esac
expect_untouched "too large"

echo "== rejected request lines"
for line in \
    "install $tmp/good/../good" \
    "install $tmp/../$(basename "$tmp")/good" \
    "install good" \
    "install  $tmp/good" \
    "install $tmp/good;touch $tmp/pwned" \
    "install \$(touch $tmp/pwned)" \
    "INSTALL $tmp/good" \
    "install /" \
    "install /$(printf 'a%.0s' $(seq 1 300))"; do
    reset
    request "$line"
    [ "$rc" -ne 0 ] || fail "rejected '$line': exit status 0"
    [ "$status" = "error invalid request" ] || fail "rejected '$line': status '$status'"
    expect_untouched "rejected '$line'"
done
[ ! -e "$tmp/pwned" ] || fail "request text was executed"

# ---- nexora-bootok: the other half of the trial-boot protocol ----------------
# (GRUB itself is not executed here; its decrement/rollback is simulated with
# grub-editenv the way grub.cfg performs it.)

# boot_as SLOT VERSION BUILD: pretend the system booted SLOT running VERSION.
boot_as() {
    printf 'BOOT_IMAGE=/boot/bzImage root=PARTUUID=x rootwait nexora.slot=%s\n' "$1" > "$tmp/cmdline"
    printf 'NAME="Nexora OS"\nNEXORA_VERSION="%s"\nNEXORA_BUILD_ID="%s"\n' "$2" "$3" > "$tmp/nexora-release"
}

# bootok HEALTHY: runs nexora-bootok in test mode; sets rc.
bootok() {
    rm -rf "$tmp/state"
    rc=0
    NEXORA_BOOTOK_TEST=1 \
        NEXORA_TEST_RUN_DIR="$tmp/run" \
        NEXORA_TEST_STATE_DIR="$tmp/state" \
        NEXORA_TEST_GRUBENV="$tmp/grubenv" \
        NEXORA_TEST_PENDING_FILE="$tmp/esp/nexora/update-pending" \
        NEXORA_TEST_RELEASE_FILE="$tmp/nexora-release" \
        NEXORA_TEST_CMDLINE_FILE="$tmp/cmdline" \
        NEXORA_TEST_UI_HEALTHY="$1" \
        "${HELPER_SHELL[@]}" "$NEXORA_BOOTOK" || rc=$?
    echo "  bootok (UI healthy=$1) -> exit $rc; grubenv: slot=$(env_get nexora_slot) try=$(env_get nexora_try) tries=$(env_get nexora_tries)"
}

json_has() { grep -q "\"$1\": \"$2\"" "$tmp/state/last-update.json" 2> /dev/null; }

echo "== bootok: new slot boots and the UI stays up -> confirmed"
reset
request "install $tmp/good"
grub-editenv "$tmp/grubenv" set nexora_tries=2 # GRUB consumed one try
boot_as B 0.4.1 v0.4.1-0-gabcdef0
rm -f "$tmp/run/update-status" # /run is empty after the reboot
bootok 1
[ "$rc" -eq 0 ] || fail "confirm: exit $rc"
[ "$(env_get nexora_slot)" = "B" ] || fail "confirm: nexora_slot not B"
[ -z "$(env_get nexora_try)" ] || fail "confirm: nexora_try not cleared"
[ "$(env_get nexora_tries)" = "0" ] || fail "confirm: nexora_tries not 0"
[ "$(cat "$tmp/run/update-status")" = "ok 0.4.1" ] || fail "confirm: status"
json_has result ok || fail "confirm: last-update.json result"
json_has newBuildId v0.4.1-0-gabcdef0 || fail "confirm: last-update.json newBuildId"
json_has previousBuildId v0.4.0-0-gtest || fail "confirm: last-update.json previousBuildId"
[ ! -e "$tmp/esp/nexora/update-pending" ] || fail "confirm: pending record not removed"

echo "== bootok: new slot boots but the UI is not running -> not confirmed"
reset
request "install $tmp/good"
grub-editenv "$tmp/grubenv" set nexora_tries=2
boot_as B 0.4.1 v0.4.1-0-gabcdef0
bootok 0
[ "$rc" -ne 0 ] || fail "unhealthy: exit 0"
[ "$(env_get nexora_slot)" = "A" ] || fail "unhealthy: nexora_slot changed"
[ "$(env_get nexora_try)" = "B" ] || fail "unhealthy: trial cancelled"
[ -e "$tmp/esp/nexora/update-pending" ] || fail "unhealthy: pending record removed"
[ ! -e "$tmp/state/last-update.json" ] || fail "unhealthy: result recorded"

echo "== bootok: GRUB used up the tries and booted the old slot -> rolled back"
reset
request "install $tmp/good"
grub-editenv "$tmp/grubenv" set nexora_try= nexora_tries=0 # grub.cfg rollback
boot_as A 0.4.0 v0.4.0-0-gtest
bootok 1
[ "$rc" -eq 0 ] || fail "rollback: exit $rc"
[ "$(env_get nexora_slot)" = "A" ] || fail "rollback: nexora_slot changed"
json_has result rolled-back || fail "rollback: last-update.json result"
json_has newBuildId v0.4.1-0-gabcdef0 || fail "rollback: last-update.json newBuildId"
[ ! -e "$tmp/esp/nexora/update-pending" ] || fail "rollback: pending record not removed"

echo "== bootok: previous slot picked by hand during a trial -> trial cancelled"
reset
request "install $tmp/good"
grub-editenv "$tmp/grubenv" set nexora_tries=2
boot_as A 0.4.0 v0.4.0-0-gtest
bootok 1
[ -z "$(env_get nexora_try)" ] || fail "manual previous: nexora_try not cleared"
[ "$(env_get nexora_slot)" = "A" ] || fail "manual previous: nexora_slot changed"
json_has result rolled-back || fail "manual previous: last-update.json result"

echo "== bootok: normal boot, nothing pending -> no change"
reset
bootok 1
[ "$rc" -eq 0 ] || fail "idle: exit $rc"
if [ "$(env_get nexora_slot)" != "A" ] || [ -n "$(env_get nexora_try)" ]; then
    fail "idle: grubenv changed"
fi
[ ! -e "$tmp/state/last-update.json" ] || fail "idle: result recorded"

if [ "$failures" -ne 0 ]; then
    echo "test-update-helper.sh: $failures failure(s)" >&2
    exit 1
fi
echo "test-update-helper.sh: ok"
