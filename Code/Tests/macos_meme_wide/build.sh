#!/bin/sh
# [TEMP] [CHOSEN, not recovered] Engine-free BFMeme wide-text check:
# a plain build, measured with malloc_size, and the same sources under AddressSanitizer.
set -eu
ROOT=$(cd "$(dirname "$0")/../../.." && pwd)
OUT="$ROOT/out/build/meme-wide-check"
M="$ROOT/Code/BF2/External/BFMeme"
mkdir -p "$OUT"
for variant in plain asan; do
    flags=""
    [ "$variant" = asan ] && flags="-fsanitize=address,undefined -fno-sanitize=vptr -fno-omit-frame-pointer"
    c++ -arch "${ARCH:-x86_64}" -std=gnu++14 -g $flags -DDEDICATED=0 \
        -include "$ROOT/Code/BF2/Core/BuildConfig.h" \
        -include "$ROOT/Code/BF2/Core/PlatformCompat.h" \
        "$ROOT/Code/Tests/macos_meme_wide/meme_wide_check.cpp" \
        "$M/Data.cpp" "$M/Object.cpp" "$M/TypeDescriptor.cpp" "$M/Stream.cpp" \
        "$M/IStream.cpp" "$M/OStream.cpp" -o "$OUT/meme_wide_check_$variant"
    ASAN_OPTIONS=detect_leaks=0 "$OUT/meme_wide_check_$variant"
done
