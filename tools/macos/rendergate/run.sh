#!/bin/bash
# [CHOSEN, not recovered] Render gate: scripted capture run on Karkand (bf2 day) and Devils_Perch
# (xpack night) through tools/macos/loopback (fixed pose, world-point aims, no bots), then log checks.
#
#   run.sh BUILD_DIR OUT_DIR [SF_CONTENT_DIR]
#
# BUILD_DIR: a client build dir (bin_macos/bf2_client). OUT_DIR (outside the repo) gets <map>/view-*.png, <map>/run.txt and checks.txt. SF_CONTENT_DIR is
# the content root holding mods/xpack (default: loopback's default content; the Devils_Perch run is
# skipped when it has no mods/xpack). Each map uses a fresh FX cache under the worktree's out/
# (the loopback sandbox writes only there), so the run also shows a cold first load. Compare two gate dirs with diff.py.
# Checks per map (checks.txt; exit 1 when any fails): every view captured, 0 "Texture not found",
# 0 loads of common/textures/wantu.dds, 0 [FRONTIER] / [UNSUPPORTED].
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
REPO=$(cd "$HERE/../../.." && pwd)
BUILD=$(cd "$1" && pwd); OUT=$2; SF=${3:-}
mkdir -p "$OUT"; OUT=$(cd "$OUT" && pwd)
VIEWS="spawn right up down walk weapon"
MAPS=${RENDERGATE_MAPS:-karkand devils}   # a subset reruns those maps and keeps the others' check lines
touch "$OUT/checks.txt"
for map in $MAPS; do grep -v "^$map:" "$OUT/checks.txt" > "$OUT/checks.tmp"; mv "$OUT/checks.tmp" "$OUT/checks.txt"; done
fail=0
for map in $MAPS; do
    extra=()
    if [ "$map" = devils ]; then
        if [ -z "$SF" ] || [ ! -d "$SF/mods/xpack" ]; then echo "$map: skipped (no SF content)" >> "$OUT/checks.txt"; continue; fi
        extra=(--content "$SF")
    fi
    cache="$REPO/out/rendergate-cache/$(basename "$OUT")-$map"
    mkdir -p "$OUT/$map"; [ -n "${RENDERGATE_KEEP_CACHE:-}" ] || rm -rf "$cache"   # keep: warm rerun
    python3 "$REPO/tools/macos/loopback/loopback.py" --build-dir "$BUILD" --scenario "$HERE/scenarios/$map" ${extra[@]+"${extra[@]}"} \
        --client-env BF2_FX_NATIVE_CACHE="$cache" --client-env BF2_FX_NATIVE_TRACE=1 > "$OUT/$map/loopback.log" 2>&1
    run=$(grep 'Run artifacts' "$OUT/$map/loopback.log" | tail -1 | awk '{print $NF}')
    echo "$run" > "$OUT/$map/run.txt"
    log="$run/client.log"
    missing=""
    for v in $VIEWS; do
        f=$(ls "$run"/present/present_*_view-$v.png 2>/dev/null | tail -1)
        if [ -n "$f" ]; then cp "$f" "$OUT/$map/view-$v.png"; else missing="$missing $v"; fi
    done
    nf=$(grep -c 'Texture not found' "$log" 2>/dev/null); nf=${nf:-0}
    wantu=$(grep -ci 'common/textures/wantu' "$log" 2>/dev/null); wantu=${wantu:-0}
    fr=$(grep -cE '\[FRONTIER\]|\[UNSUPPORTED\]' "$log" 2>/dev/null); fr=${fr:-0}
    loaded=$(grep -m1 'INPUT-SCRIPT.*matched "ingameMap.tga"' "$log" | sed -E 's/.*t=([0-9.]+)s.*/\1/')
    native=$(grep 'BF2FX native:' "$log" | tail -1 | sed -E 's/.*\((.*)\).*/\1/')
    status=PASS
    if [ -n "$missing" ] || [ "$nf" != 0 ] || [ "$wantu" != 0 ] || [ "$fr" != 0 ]; then status=FAIL; fail=1; fi
    echo "$map: $status views-missing:[${missing# }] texture-not-found=$nf wantu=$wantu frontier/unsupported=$fr load=${loaded:-?}s run=$run native=[${native:-}]" >> "$OUT/checks.txt"
done
cat "$OUT/checks.txt"
grep -q FAIL "$OUT/checks.txt" && fail=1
exit $fail
