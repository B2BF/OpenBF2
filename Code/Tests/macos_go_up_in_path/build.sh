#!/bin/sh
# [CHOSEN, not recovered] Engine-free goUpInPath leading-'/' check, client and dedicated builds.
set -eu
ROOT=$(cd "$(dirname "$0")/../../.." && pwd)
OUT="$ROOT/out/build/go-up-in-path-check"
mkdir -p "$OUT"
for ded in 0 1; do
    c++ -arch x86_64 -std=gnu++14 -DDEDICATED=$ded \
        -include "$ROOT/Code/BF2/Core/BuildConfig.h" \
        -include "$ROOT/Code/BF2/Core/PlatformCompat.h" \
        "$ROOT/Code/Tests/macos_go_up_in_path/go_up_in_path_check.cpp" \
        "$ROOT/Code/BF2/Core/StringUtil.cpp" -o "$OUT/check_$ded"
    "$OUT/check_$ded"
done
