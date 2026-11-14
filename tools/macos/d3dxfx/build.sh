#!/bin/sh
# [CHOSEN, not recovered] Offline tooling only; no engine sources or game installation writes.
set -eu
cd "$(dirname "$0")/../../.."
root=$PWD
stage="$root/out/d3dxfx"
build="$root/out/build/s1c-fx-coverage/tools"
upstream=${MOJOSHADER_SOURCE:-}
if [ ! -f out/third_party/mojoshader/CMakeLists.txt ] && [ -z "$upstream" ]; then
    echo "Set MOJOSHADER_SOURCE to a local MojoShader checkout (or provide out/third_party/mojoshader)." >&2; exit 2
fi
mingw=${MINGW_CC:-/usr/local/bin/i686-w64-mingw32-gcc}
mkdir -p "$build" "$stage/logs" "$root/out/third_party"
"$mingw" -std=c99 -O2 -Wall -Wextra -Werror -static-libgcc \
    tools/macos/d3dxfx/fxcompile.c -o "$build/fxcompile.exe"
# Upstream CMake writes its version header in the source tree. Build our own copy.
if [ ! -f out/third_party/mojoshader/CMakeLists.txt ]; then
    [ -f "$upstream/CMakeLists.txt" ] || { echo "Missing local MojoShader: $upstream" >&2; exit 2; }
    cp -R "$upstream" out/third_party/mojoshader
fi
python3 tools/macos/d3dxfx/patch-mojo.py out/third_party/mojoshader "$build/patches.json"
cmake -S out/third_party/mojoshader -B "$build/mojoshader" -G 'Unix Makefiles' \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=x86_64 -DCMAKE_OSX_SYSROOT=macosx \
    -DPROFILE_METAL=ON -DPROFILE_BYTECODE=ON -DEFFECT_SUPPORT=ON \
    > "$stage/logs/mojo-configure.log" 2>&1
cmake --build "$build/mojoshader" --target mojoshader -j4 > "$stage/logs/mojo-build.log" 2>&1
sdk=$(xcrun --sdk macosx --show-sdk-path)
clang++ -std=c++17 -arch x86_64 -isysroot "$sdk" -fobjc-arc -Wall -Wextra -Werror \
    -DMOJOSHADER_EFFECT_SUPPORT -I out/third_party/mojoshader \
    tools/macos/d3dxfx/effectparse.mm "$build/mojoshader/libmojoshader.a" \
    -framework Foundation -o "$build/effectparse"
clang++ -std=c++17 -arch x86_64 -isysroot "$sdk" -fobjc-arc -Wall -Wextra -Werror \
    tools/macos/d3dxfx/metal-compile.mm -framework Foundation -framework Metal -o "$build/metal-compile"
echo "Built fxcompile.exe, effectparse and metal-compile in $build"
