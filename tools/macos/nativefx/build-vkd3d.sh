#!/bin/sh
# [CHOSEN, not recovered] Offline build of vkd3d-shader (LGPL-2.1+, tag vkd3d-2.1) as a static
# library for native effect compilation. The pristine source in out/third_party/vkd3d is copied
# to out/build/nativefx/vkd3d-src, the patches in tools/macos/vkd3d-patches/ are applied there,
# and only libvkd3d-shader is built. No download, no install.
set -eu
cd "$(dirname "$0")/../../.."
root=$PWD
# [CHOSEN, not recovered] Separate thin builds keep configure's architecture probes
# honest; universal archives contain those same two slices, without executing arm64 tools.
arch=${1:-x86_64}
case "$arch" in
    x86_64) out=out/build/nativefx ;;
    arm64|universal) out=out/build/nativefx-$arch ;;
    *) echo "Usage: $0 [x86_64|arm64|universal]" >&2; exit 2 ;;
esac
export MACOSX_DEPLOYMENT_TARGET=${MACOSX_DEPLOYMENT_TARGET:-11.0}
if [ "$arch" = universal ]; then
    sh tools/macos/nativefx/build-vkd3d.sh x86_64 > /dev/null
    sh tools/macos/nativefx/build-vkd3d.sh arm64 > /dev/null
    mkdir -p "$out/vkd3d/.libs" "$out/vkd3d-src"
    rsync -a out/build/nativefx/vkd3d-src/ "$out/vkd3d-src/"
    for lib in libvkd3d-shader.a libvkd3d-common.a; do
        lipo -create "out/build/nativefx/vkd3d/.libs/$lib" "out/build/nativefx-arm64/vkd3d/.libs/$lib" \
            -output "$out/vkd3d/.libs/$lib"
    done
    echo "$root/$out/vkd3d/.libs/libvkd3d-shader.a"
    exit 0
fi
export PATH="/usr/local/opt/flex/bin:/usr/local/opt/bison/bin:$PATH"
WIDL=${WIDL:-/usr/local/opt/mingw-w64/toolchain-x86_64/bin/x86_64-w64-mingw32-widl}
export WIDL
src=$out/vkd3d-src
bld=$out/vkd3d
log=$out/logs
mkdir -p "$src" "$bld" "$log"
# Re-sync the pristine source and re-apply patches only when either changed.
stamp=$( (cd out/third_party/vkd3d && git rev-parse HEAD; cat "$root"/tools/macos/vkd3d-patches/*.patch 2>/dev/null) | shasum -a 256 | cut -c1-16)
if [ "$(cat "$src/.bf2-stamp" 2>/dev/null)" != "$stamp" ]; then
    rsync -a --delete --exclude .git out/third_party/vkd3d/ "$src/"
    for p in "$root"/tools/macos/vkd3d-patches/*.patch; do
        [ -f "$p" ] || continue
        (cd "$src" && patch -p1 -F0 --no-backup-if-mismatch -s < "$p") || { echo "patch failed: $p" >&2; exit 1; }
    done
    echo "$stamp" > "$src/.bf2-stamp"
fi
# The applied patch names, for the configure-time check in Code/BF2/Platform/Gfx/Effect/CMakeLists.txt.
(cd "$root/tools/macos/vkd3d-patches" && ls *.patch 2>/dev/null) > "$src/.bf2-patches"
# [CHOSEN, not recovered] TRACE/WARN/FIXME arguments are evaluated even with VKD3D_SHADER_DEBUG=none
# (about 5 % of an effect build went to debug-string formatting); compile the debug log out.
cflags="-O2 -g -arch $arch -DVKD3D_NO_TRACE_MESSAGES -DVKD3D_NO_DEBUG_MESSAGES"
if [ -f "$bld/Makefile" ] && [ "$(cat "$bld/.bf2-cflags" 2>/dev/null)" != "$cflags" ]; then
    rm -f "$bld/Makefile"   # reconfigure for the new flags
fi
if [ ! -f "$bld/Makefile" ]; then
    host_args=
    if [ "$arch" != "$(uname -m)" ]; then
        host_args="--build=$(sh "$src/bin/config.guess") --host=$arch-apple-darwin"
    fi
    (cd "$bld" && CC=clang OBJC=clang CFLAGS="$cflags" LDFLAGS="-arch $arch" SONAME_LIBVULKAN=libvulkan.dylib \
        "$root/$src/configure" $host_args --disable-tests --disable-demos --disable-shared \
        --without-ncurses --without-opengl --without-xcb --without-spirv-tools) > "$log/vkd3d-configure.log" 2>&1
    echo "$cflags" > "$bld/.bf2-cflags"
    (cd "$bld" && make clean) > /dev/null 2>&1 || true
fi
(cd "$bld" && make -f Makefile -f "$root/tools/macos/nativefx/headers.mk" nativefx-headers) > "$log/vkd3d-generated.log" 2>&1
(cd "$bld" && make -j6 libvkd3d-shader.la vkd3d-compiler) > "$log/vkd3d-build.log" 2>&1
echo "$root/$bld/.libs/libvkd3d-shader.a"
