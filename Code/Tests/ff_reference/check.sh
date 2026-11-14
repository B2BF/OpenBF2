#!/bin/sh
# [CHOSEN, not recovered] Fixed-function vertex checks (0.3.0 pieces 1-3), no engine:
#   ff_check        CPU model (Platform/Gfx/Metal/FFVertex.cpp) vs ff_reference.txt (real d3d9
#                   ProcessVertices, generated on Windows by ff_reference.cmd)
#   ff_metal_check  the generated MSL on the Metal GPU vs the CPU model
#   with HARNESS (a macos_metal_device binary): the port's ProcessVertices driven with the
#   generator's own calls (--ff-device) vs the reference, every variant
#   check.sh [HARNESS]
set -eu
ROOT=$(cd "$(dirname "$0")/../../.." && pwd)
SRC="$ROOT/Code/Tests/ff_reference"
OUT="$ROOT/out/build/ff-reference-check"
mkdir -p "$OUT"
c++ -std=c++17 -O1 -DBF2_FF_STANDALONE "$SRC/ff_check.cpp" "$SRC/ff_cases.cpp" "$ROOT/Code/BF2/Platform/Gfx/Metal/FFVertex.cpp" -o "$OUT/ff_check"
c++ -std=c++17 -O1 -DBF2_FF_STANDALONE -fobjc-arc "$SRC/ff_metal_check.mm" "$SRC/ff_cases.cpp" "$ROOT/Code/BF2/Platform/Gfx/Metal/FFVertex.cpp" \
    -framework Metal -framework Foundation -o "$OUT/ff_metal_check"
"$OUT/ff_check" "$SRC/ff_reference.txt"
"$OUT/ff_metal_check"
if [ $# -ge 1 ]; then
    "$1" --ff-device "$OUT/ff_device.txt" | tail -1
    "$OUT/ff_check" "$SRC/ff_reference.txt" --device "$OUT/ff_device.txt" | tail -1
fi
