#!/bin/bash
# [CHOSEN, not recovered] Warm-up gate (+warmupLevel, port addition): a cold warm-up run of MAP from an
# empty FX cache, then the normal render-gate load of the same map on that cache.
#
#   warmup.sh BUILD_DIR OUT_DIR [MAP] [SF_CONTENT_DIR]     MAP: karkand (default) or devils
#
# PASS needs: warm-up exit status 0 with a "warmup: done" line, then the normal load with 0 native
# compiles (deferred 0, compiled now 0) and the render gate's own checks. Writes OUT_DIR/warmup.txt.
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
REPO=$(cd "$HERE/../../.." && pwd)
BUILD=$(cd "$1" && pwd); OUT=$2; MAP=${3:-karkand}; SF=${4:-}
mkdir -p "$OUT"; OUT=$(cd "$OUT" && pwd)
level=strike_at_karkand; extra=()
if [ "$MAP" = devils ]; then level=devils_perch; extra=(--mod xpack --content "$SF"); fi
cache="$REPO/out/rendergate-cache/$(basename "$OUT")-$MAP"
rm -rf "$cache"
python3 "$REPO/tools/macos/loopback/loopback.py" --build-dir "$BUILD" --no-server --map "$level" --video-high \
    ${extra[@]+"${extra[@]}"} --client-args "+warmupLevel $level" --client-seconds 3600 \
    --client-env BF2_FX_NATIVE_CACHE="$cache" > "$OUT/warmup-loopback.log" 2>&1
run=$(grep 'Run artifacts' "$OUT/warmup-loopback.log" | tail -1 | awk '{print $NF}')
result=$(cat "$run/client.result" 2>/dev/null)
grep '^warmup:' "$run/client.log" | grep -v '^warmup: effects' > "$OUT/warmup-lines.txt"
grep '^warmup: effects' "$run/client.log" | tail -1 >> "$OUT/warmup-lines.txt"
# The render gate's normal load reuses the warmed cache (its cache name for OUT and MAP).
RENDERGATE_MAPS=$MAP RENDERGATE_KEEP_CACHE=1 "$HERE/run.sh" "$BUILD" "$OUT" ${SF:+"$SF"} > /dev/null 2>&1
check=$(grep "^$MAP:" "$OUT/checks.txt")
status=PASS
case "$result" in exit=0\ *) ;; *) status=FAIL ;; esac
grep -q '^warmup: done' "$OUT/warmup-lines.txt" || status=FAIL
echo "$check" | grep -q 'deferred 0, compiled now 0' || status=FAIL
echo "$check" | grep -q "^$MAP: PASS" || status=FAIL
{ echo "warmup $MAP: $status ($result) run=$run"; cat "$OUT/warmup-lines.txt"; echo "normal load: $check"; } | tee "$OUT/warmup.txt"
[ "$status" = PASS ]
