#!/bin/sh
# [CHOSEN, not recovered] Offline, shader-only build of the owner's local inputs.
set -eu
cd "$(dirname "$0")/../../.."
root=$PWD
export PATH="/usr/local/opt/flex/bin:/usr/local/opt/bison/bin:$PATH"
WIDL=${WIDL:-/usr/local/opt/mingw-w64/toolchain-x86_64/bin/x86_64-w64-mingw32-widl}
export WIDL
[ -x "$WIDL" ] || { echo "Missing local widl: $WIDL" >&2; exit 1; }
mkdir -p out/third_party/build/vkd3d out/shaderproto
if [ ! -f out/third_party/vkd3d/configure ]; then
    (cd out/third_party/vkd3d && ./autogen.sh) > out/shaderproto/vkd3d-autogen.log 2>&1
fi
if [ ! -f out/third_party/build/vkd3d/Makefile ] || \
    ! grep -q '^WIDL = /' out/third_party/build/vkd3d/Makefile; then
    # configure probes a Vulkan runtime for the unrelated renderer library. The
    # shader-only targets never load this nominal SONAME and need no Vulkan runtime.
    (cd out/third_party/build/vkd3d && CC=clang OBJC=clang SONAME_LIBVULKAN=libvulkan.dylib \
        ../../vkd3d/configure --disable-tests --disable-demos --disable-shared \
        --without-ncurses --without-opengl --without-xcb --without-spirv-tools) \
        > out/shaderproto/vkd3d-configure.log 2>&1
fi
(cd out/third_party/build/vkd3d && make -f Makefile -f "$root/tools/macos/shaderproto/headers.mk" shaderproto-headers) \
    > out/shaderproto/vkd3d-generated.log 2>&1
(cd out/third_party/build/vkd3d && make -j4 libvkd3d-shader.la vkd3d-compiler) \
    > out/shaderproto/vkd3d-build.log 2>&1
cmake -S out/third_party/mojoshader -B out/third_party/build/mojoshader \
    -G 'Unix Makefiles' -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_OSX_ARCHITECTURES=x86_64 -DCMAKE_OSX_SYSROOT=macosx \
    -DPROFILE_METAL=ON -DEFFECT_SUPPORT=ON > out/shaderproto/mojo-configure.log 2>&1
cmake --build out/third_party/build/mojoshader --target mojoshader testparse -j4 \
    > out/shaderproto/mojo-build.log 2>&1
cmake -S tools/macos/shaderproto -B out/build/s0-shader-proto -G 'Unix Makefiles' \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=x86_64 -DCMAKE_OSX_SYSROOT=macosx \
    > out/shaderproto/harness-configure.log 2>&1
cmake --build out/build/s0-shader-proto -j4 > out/shaderproto/harness-build.log 2>&1
echo "Built $root/out/build/s0-shader-proto/shaderproto"
