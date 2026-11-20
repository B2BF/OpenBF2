#!/bin/sh
# [TEMP] [CHOSEN, not recovered] Build/run Intel checks and package M2 checks.
set -eu
numeric_root=$(CDPATH= cd -- "$(dirname -- "$0")/../../.." && pwd)
cd "$numeric_root"
numeric_sdk=$(xcrun --sdk macosx --show-sdk-path)
numeric_out=out/s2-10-numerics/checks-x86_64
numeric_bundle=out/m2/numerics
mkdir -p "$numeric_out" "$numeric_bundle"

compile_numeric()
{
    numeric_arch=$1; numeric_exe=$2; numeric_image=$3; shift 3
    xcrun clang++ -arch "$numeric_arch" -isysroot "$numeric_sdk" \
        -std=c++14 -O2 -ffp-contract=off -frounding-math -fms-extensions \
        "-DDEDICATED=$numeric_image" -include Code/BF2/Core/BuildConfig.h \
        -include Code/BF2/Core/PlatformCompat.h "$@" -o "$numeric_exe"
}

for numeric_target in x86_64 arm64; do
    if [ "$numeric_target" = arm64 ]; then numeric_dest=$numeric_bundle
    else numeric_dest=$numeric_out; fi
    compile_numeric "$numeric_target" "$numeric_dest/numerics" 0 Code/Tests/macos_numerics.cpp
    compile_numeric "$numeric_target" "$numeric_dest/portable_x87" 0 \
        Code/Tests/portable_x87.cpp Code/BF2/Math/Mat4Inverse.cpp
    # The client's exact arm on arm64 (binary32 is the default there).
    compile_numeric "$numeric_target" "$numeric_dest/portable_x87_exact" 0 \
        -DBF2X87_CLIENT_EXACT_EXTENDED Code/Tests/portable_x87.cpp Code/BF2/Math/Mat4Inverse.cpp
    compile_numeric "$numeric_target" "$numeric_dest/bitstream_numeric" 1 \
        Code/Tests/macos_bitstream_numeric.cpp Code/BF2/IO/BitStream.cpp \
        Code/BF2/Core/Debug.cpp Code/BF2/Core/StringUtil.cpp
done

# Exercise the exact software-extended arm on Intel, including its FISTP edges.
compile_numeric x86_64 "$numeric_out/numerics_soft" 0 \
    -DBF2_TEST_SOFT_EXTENDED Code/Tests/macos_numerics.cpp
compile_numeric x86_64 "$numeric_out/portable_x87_soft" 0 \
    -DBF2_TEST_SOFT_EXTENDED Code/Tests/portable_x87.cpp Code/BF2/Math/Mat4Inverse.cpp
# The binary32 client arm of Apple silicon, checked against the host x87 at 24-bit precision.
compile_numeric x86_64 "$numeric_out/portable_x87_float" 0 \
    -DBF2_TEST_CLIENT_FLOAT Code/Tests/portable_x87.cpp Code/BF2/Math/Mat4Inverse.cpp
"$numeric_out/numerics"
"$numeric_out/numerics_soft"
"$numeric_out/portable_x87"
"$numeric_out/portable_x87_soft"
"$numeric_out/portable_x87_float"
"$numeric_out/bitstream_numeric"
cp Code/Tests/macos_numerics/run.sh "$numeric_bundle/run.sh"
chmod +x "$numeric_bundle/run.sh"
printf '%s\n' 'Copy this entire numerics directory to the M2 and run: sh run.sh' \
    'No CLT, game content, network, or server required. Return results.txt to the coordinator.' \
    > "$numeric_bundle/README.txt"
file "$numeric_bundle/numerics" "$numeric_bundle/portable_x87" "$numeric_bundle/bitstream_numeric"
