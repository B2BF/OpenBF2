#!/bin/sh
# [TEMP] [CHOSEN, not recovered] Engine-free checks of the macOS .ani/.cur decoder with synthetic
# files built in the check itself (no game content).
set -eu
ROOT=$(cd "$(dirname "$0")/../../.." && pwd)
OUT="$ROOT/out/build/cursor-file-check"
mkdir -p "$OUT"
c++ -std=gnu++14 -Wall -Wextra -fsanitize=address,undefined \
    "$ROOT/Code/Tests/macos_cursor_file/cursor_file_check.cpp" -o "$OUT/cursor_file_check"
"$OUT/cursor_file_check"
