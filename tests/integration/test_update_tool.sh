#!/usr/bin/env bash
# Exercises nexora-update-tool against the fixture written by tst_updateservice's
# writesToolFixture() test (see tests/integration/CMakeLists.txt: this test's
# FIXTURES_REQUIRED depends on tst_updateservice's FIXTURES_SETUP, so it always runs
# after that fixture has been written).
set -euo pipefail

: "${NEXORA_UPDATE_TOOL:?NEXORA_UPDATE_TOOL is not set}"
: "${NEXORA_TEST_KEYS_DIR:?NEXORA_TEST_KEYS_DIR is not set}"
: "${NEXORA_TEST_BUNDLE_DIR:?NEXORA_TEST_BUNDLE_DIR is not set}"
: "${NEXORA_TEST_BAD_BUNDLE_DIR:?NEXORA_TEST_BAD_BUNDLE_DIR is not set}"

echo "verifying the good bundle"
output=$("$NEXORA_UPDATE_TOOL" verify --keys "$NEXORA_TEST_KEYS_DIR" --bundle "$NEXORA_TEST_BUNDLE_DIR" \
    --current-version 0.4.0 --board nexora-x86_64)
echo "$output"
case "$output" in
    "ok "*) ;;
    *)
        echo "expected an 'ok <version> <sha256>' line, got: $output" >&2
        exit 1
        ;;
esac

echo "verifying the tampered bundle"
status=0
output=$("$NEXORA_UPDATE_TOOL" verify --keys "$NEXORA_TEST_KEYS_DIR" --bundle "$NEXORA_TEST_BAD_BUNDLE_DIR" \
    --current-version 0.4.0 --board nexora-x86_64) || status=$?
echo "$output"
if [ "$status" -eq 0 ]; then
    echo "expected a non-zero exit status for the tampered bundle" >&2
    exit 1
fi
case "$output" in
    "error "*) ;;
    *)
        echo "expected an 'error <code> <message>' line, got: $output" >&2
        exit 1
        ;;
esac

echo "test_update_tool.sh: ok"
