#!/bin/bash
# [CHOSEN, not recovered] Quit-after-level check: client-hosted single player on Dalian Plant with
# 4x antialiasing, then the shipped QUIT dialog, once from the main menu after leaving the level
# (scripts/sp-quit-after-map.txt) and once from the in-game menu (scripts/sp-quit-in-map.txt).
# Both quits run BF2Engine::shutdown and the RendDX9 deinit, whose final device release must reach
# zero (RendDX9Init.cpp:738, "Direct3DDevice release unsuccessful").
#
#   sp-quit-check.sh BUILD_DIR [SCENARIO...]     SCENARIO: sp-quit-after-map (default both)
#
# PASS per scenario: the script reached the YES click, the client exited with status 0 before the
# timeout, and the log has no RendDX9Init.cpp release fatal. Extra loopback.py arguments go in
# SP_QUIT_ARGS (e.g. --client-env BF2_METAL_REFTRACE=1, which also prints the device references
# left at the final release). Exit 1 when any scenario fails.
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
REPO=$(cd "$HERE/../../.." && pwd)
BUILD=$(cd "$1" && pwd); shift
SCENARIOS=${*:-sp-quit-after-map sp-quit-in-map}
fail=0
for sc in $SCENARIOS; do
    out=$(mktemp)
    # shellcheck disable=SC2086
    /usr/bin/python3 "$REPO/tools/macos/loopback/loopback.py" --build-dir "$BUILD" --no-server --ai-spawn-only \
        --antialiasing 4Samples --input-script "$sc" --client-seconds 420 --client-args "+noSound 1" \
        ${SP_QUIT_ARGS:-} > "$out" 2>&1
    run=$(grep 'Run artifacts' "$out" | tail -1 | awk '{print $NF}')
    rm -f "$out"
    log="$run/client.log"
    result=$(cat "$run/client.result" 2>/dev/null)
    confirmed=$(grep -c 'sp-quit: confirming the quit' "$log" 2>/dev/null); confirmed=${confirmed:-0}
    fatal=$(grep -c 'release unsuccessful' "$log" 2>/dev/null); fatal=${fatal:-0}
    refs=$(grep -m1 '^\[REFTRACE\] device' "$log" 2>/dev/null | sed -E 's/.* refs ([0-9]+);.*/ device refs before the final release: \1/')
    status=PASS
    case "$result" in exit=0\ timeout=False*) ;; *) status=FAIL ;; esac
    [ "$confirmed" -gt 0 ] && [ "$fatal" = 0 ] || status=FAIL
    [ "$status" = PASS ] || fail=1
    echo "$sc: $status (${result:-no result}; YES clicked=$confirmed, release fatal=$fatal;${refs:-} run=$run)"
done
exit $fail
