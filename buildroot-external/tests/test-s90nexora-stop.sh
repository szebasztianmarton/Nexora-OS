#!/usr/bin/env bash
# Exercises "S90nexora stop" (rootfs-overlay/etc/init.d/S90nexora) on the
# build host with stand-ins for start-stop-daemon and sleep on PATH: the
# supervisor and the UI are simulated, and "sleep 0.1" only advances a
# counter, so a 35 s bound runs in well under a second.
#
# Required environment (set by tests/system/CMakeLists.txt):
#   NEXORA_S90NEXORA   path to S90nexora
set -euo pipefail

: "${NEXORA_S90NEXORA:?}"

if command -v busybox > /dev/null 2>&1; then
    INIT_SHELL=(busybox sh)
else
    INIT_SHELL=(sh)
fi

tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
mkdir "$tmp/bin"

# start-stop-daemon stand-in. State files in $STATE: supervisor (alive),
# ui (alive), ticks (number of "sleep 0.1" calls), supervisor-exit-tick
# (tick at which the supervisor exits on its own after TERM, empty = never).
cat > "$tmp/bin/start-stop-daemon" << 'EOF'
#!/bin/sh
test=0; signal=TERM; pidfile=""; exec_path=""
while [ $# -gt 0 ]; do
    case "$1" in
        --test) test=1 ;;
        --signal) signal=$2; shift ;;
        --pidfile) pidfile=$2; shift ;;
        --exec) exec_path=$2; shift ;;
    esac
    shift
done
echo "stop test=$test signal=$signal pidfile=$pidfile exec=$exec_path" >> "$STATE/calls"
case "$pidfile" in
    */nexora-recovery.pid) exit 1 ;;
esac
if [ -n "$pidfile" ]; then
    target=supervisor
else
    target=ui
fi
if [ "$target" = supervisor ] && [ -f "$STATE/supervisor" ] && [ -s "$STATE/supervisor-exit-tick" ] &&
    [ "$(cat "$STATE/ticks")" -ge "$(cat "$STATE/supervisor-exit-tick")" ]; then
    rm -f "$STATE/supervisor" "$STATE/ui"
fi
[ -f "$STATE/$target" ] || exit 1
[ "$test" -eq 1 ] && exit 0
[ "$signal" = KILL ] && rm -f "$STATE/$target"
exit 0
EOF
cat > "$tmp/bin/sleep" << 'EOF'
#!/bin/sh
echo $(( $(cat "$STATE/ticks") + 1 )) > "$STATE/ticks"
EOF
chmod +x "$tmp/bin/start-stop-daemon" "$tmp/bin/sleep"

failures=0
fail() {
    echo "FAIL: $*" >&2
    failures=$((failures + 1))
}

# run_stop EXIT_TICK: starts a fresh simulation and runs "S90nexora stop".
run_stop() {
    export STATE="$tmp/state-$1"
    mkdir -p "$STATE"
    : > "$STATE/supervisor"
    : > "$STATE/ui"
    : > "$STATE/calls"
    echo 0 > "$STATE/ticks"
    printf '%s' "$1" > "$STATE/supervisor-exit-tick"
    PATH="$tmp/bin:$PATH" "${INIT_SHELL[@]}" "$NEXORA_S90NEXORA" stop > "$STATE/out" 2>&1 || true
}

# A UI that needs 28 s (the default Quick Resume exit budget: 2 x 10 s + 5 s
# for the save, 3 s for the emulator) to exit: no KILL, nothing left over.
run_stop 280
if grep -q 'signal=KILL' "$STATE/calls"; then
    fail "stop sent KILL to a session that exits after 28 s"
fi
[ -f "$STATE/ui" ] && fail "UI still running after stop (28 s exit)"
[ -f "$STATE/supervisor" ] && fail "supervisor still running after stop (28 s exit)"

# A session that never exits: after the bound both the UI and the
# supervisor are KILLed, the UI first.
run_stop ""
[ -f "$STATE/ui" ] && fail "UI still running after stop (hung session)"
[ -f "$STATE/supervisor" ] && fail "supervisor still running after stop (hung session)"
ticks=$(cat "$STATE/ticks")
if [ "$ticks" -lt 350 ]; then
    fail "stop gave up after ${ticks} ticks of 0.1 s, before 35 s"
fi
ui_kill=$(grep -n 'signal=KILL .*exec=/usr/bin/nexora-console-ui' "$STATE/calls" | head -n 1 | cut -d: -f1)
sup_kill=$(grep -n 'signal=KILL pidfile=/var/run/nexora.pid' "$STATE/calls" | head -n 1 | cut -d: -f1)
if [ -z "$ui_kill" ] || [ -z "$sup_kill" ] || [ "$ui_kill" -gt "$sup_kill" ]; then
    fail "expected KILL of the UI and then of the supervisor, got: $(cat "$STATE/calls")"
fi

if [ "$failures" -ne 0 ]; then
    exit 1
fi
echo "test-s90nexora-stop.sh: ok"
