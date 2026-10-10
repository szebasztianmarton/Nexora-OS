#!/usr/bin/env bash
# Development sandbox: builds a throw-away config + library that uses the mock
# emulator, then starts the console UI in a window. Production builds never
# contain this data; it only exists in the sandbox directory created here.
#
# Usage: scripts/run-dev.sh [build-dir] [extra nexora-console-ui args...]
#   NEXORA_SANDBOX=/path   sandbox location (default: <build-dir>/dev-sandbox)
#   NEXORA_SANDBOX_RESET=1 recreate the sandbox from scratch
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="${1:-$ROOT/build}"
shift || true
SANDBOX="${NEXORA_SANDBOX:-$BUILD/dev-sandbox}"
MOCK="$BUILD/tools/mock-emulator/nexora-mock-emulator"
APP="$BUILD/apps/console-ui/nexora-console-ui"

[[ -x "$APP" && -x "$MOCK" ]] || { echo "Build first: cmake --build $BUILD" >&2; exit 1; }

if [[ "${NEXORA_SANDBOX_RESET:-0}" == "1" ]]; then rm -rf "$SANDBOX"; fi

if [[ ! -d "$SANDBOX" ]]; then
    mkdir -p "$SANDBOX/config" "$SANDBOX/logs" "$SANDBOX/games/covers" "$SANDBOX/games/More Games"
    cat > "$SANDBOX/config/emulators.json" <<JSON
{
  "version": 1,
  "emulators": [
    {
      "id": "mock",
      "name": "Mock Emulator",
      "executable": "$MOCK",
      "arguments": ["--sleep-ms", "4000", "{game}"],
      "platforms": ["Test Platform"],
      "extensions": ["mock"],
      "profiles": [
        { "id": "quick", "name": "Quick exit", "arguments": ["--sleep-ms", "500", "{game}"] },
        { "id": "crash", "name": "Simulated crash", "arguments": ["--sleep-ms", "800", "--crash", "{game}"] }
      ]
    },
    {
      "id": "mock-failing",
      "name": "Failing Mock",
      "executable": "$MOCK",
      "arguments": ["--sleep-ms", "1500", "--exit-code", "3", "{game}"],
      "platforms": ["Test Platform"],
      "extensions": ["fail"]
    },
    {
      "id": "missing",
      "name": "Not Installed Emulator",
      "executable": "/opt/nexora/does-not-exist/emulator",
      "extensions": ["nothere"]
    },
    {
      "id": "disabled",
      "name": "Disabled Emulator",
      "executable": "$MOCK",
      "enabled": false,
      "extensions": ["off"]
    }
  ]
}
JSON
    for t in "Aurora_Drift" "Crystal Caverns" "Neon Runner 2" "Neon Runner 10" "Starfall Odyssey" \
             "The Extraordinarily Long Adventure of the Wandering Knight and the Seven Forgotten Kingdoms" \
             "Pixel Quest" "Moonlit Harbor" "Rally Horizon"; do
        echo "sandbox game file" > "$SANDBOX/games/$t.mock"
    done
    echo "sandbox game file" > "$SANDBOX/games/More Games/Broken Save.fail"
    echo "sandbox game file" > "$SANDBOX/games/More Games/Orphan Disc.iso"
    # Generated cover art (sandbox only) to exercise artwork handling.
    if command -v convert >/dev/null 2>&1; then
        convert -size 400x600 gradient:'#1b4f72-#0b1a2b' -gravity center -fill '#6CE5E8' \
            -pointsize 46 -annotate 0 'CRYSTAL\nCAVERNS' "$SANDBOX/games/covers/Crystal Caverns.png" || true
        convert -size 640x360 gradient:'#5b2c6f-#140b1f' -gravity center -fill '#F2F5FA' \
            -pointsize 40 -annotate 0 'STARFALL ODYSSEY' "$SANDBOX/games/Starfall Odyssey.jpg" || true
    fi
    cat > "$SANDBOX/config/settings.json" <<JSON
{ "libraryDirectories": ["$SANDBOX/games"], "fullscreen": false }
JSON
    echo "Created sandbox at $SANDBOX"
fi

exec "$APP" --config-dir "$SANDBOX/config" --log-dir "$SANDBOX/logs" --windowed "$@"
