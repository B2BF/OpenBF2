#!/bin/bash
# [TEMP] [CHOSEN, not recovered] Stage the owner's M2 bundle for the arm64 renderer-ON client and
# the loopback play harness (docs/macos-c-arm64-client.md, "M2 bundle").
# Usage: package.sh ARM64_BUILD_DIR METAL_HARNESS_BUILD_DIR [BUNDLE_DIR]
#   ARM64_BUILD_DIR          an arm64 engine build (bf2_client bf2 bf2_aidll bf2_vtable_audit and the
#                            checks dice_py_check bf2_aidll_loader_check bf2_native_vtable_check
#                            bf2_meme_dispatch_check macos_gfx_forward; BUILD_TESTING=ON)
#   METAL_HARNESS_BUILD_DIR  an arm64 build of Code/Tests/macos_metal_device
#   BUNDLE_DIR               default out/arm64-bundle/openbf2-arm64; a .tgz is written beside it
# Optional: BF2_INTEL_REFERENCE_RUN=<loopback run dir> records that Intel gate run's frontier set
# in reference/ so the M2 summary can show which frontiers differ.
# Optional: BF2_BASELINES=<dir> holds the Intel reference views, one folder per scenario
# (render-view-dalian/, render-view-ocs/, as compare_views.py --baseline-save writes them); default
# <repo>/out/baselines. They are copied to reference/baselines/ for the `gate` mode. A missing
# folder is reported and the bundle is staged without views to compare.
# Rehearsal on an Intel Mac (the bundle then only runs with BF2_BUNDLE_REHEARSAL=1):
#   BF2_BUNDLE_ARCH=x86_64 BF2_BUNDLE_SKIP_CHECKS=1 package.sh X86_64_BUILD - [BUNDLE_DIR]
# stages that build under out/build/x86_64 and leaves out the check tools (HARNESS_BUILD is "-").
# No proprietary game content is copied; run.sh takes the content folder on the M2.
set -euo pipefail
repo=$(cd "$(dirname "$0")/../../.." && pwd -P)
usage='usage: package.sh ARM64_BUILD HARNESS_BUILD [BUNDLE]'
build=$(cd "${1:?$usage}" && pwd -P)
arch=${BF2_BUNDLE_ARCH:-arm64}
skip_checks=${BF2_BUNDLE_SKIP_CHECKS:-0}
[ "$skip_checks" = 1 ] || harness=$(cd "${2:?$usage}" && pwd -P)
baselines=${BF2_BASELINES:-"$repo/out/baselines"}
dest=${3:-"$repo/out/arm64-bundle/openbf2-arm64"}
here="$repo/Code/Tests/macos_c_arm64"
loop="$repo/tools/macos/loopback"
bin="$build/bin_macos"

find_exe() {   # the first executable named $1 under the build tree
    local hit
    hit=$(find "$build" -name "$1" -type f -perm -u+x 2>/dev/null | head -1)
    [ -n "$hit" ] || { echo "package.sh: $1 not built under $build" >&2; exit 1; }
    echo "$hit"
}

rm -rf "$dest"
mkdir -p "$dest/bin" "$dest/check-lib" "$dest/out/build/$arch/bin_macos" \
    "$dest/tools/macos/loopback/scripts" "$dest/reference/baselines"
echo "$arch" > "$dest/build-tag.txt"   # run.sh passes it to loopback.py --build

# Game images, in the layout loopback.py expects: <repo>/out/build/<tag>/bin_macos. The bundle root
# plays the repository root, so the harness runs unchanged with --build arm64.
for name in bf2_client bf2 libdice_py.dylib libaidll.dylib; do
    cp "$bin/$name" "$dest/out/build/$arch/bin_macos/$name"
done
# Standalone checks; the loaders find their dylibs beside them (@loader_path).
cp "$bin/libdice_py.dylib" "$bin/libaidll.dylib" "$dest/bin/"
if [ "$skip_checks" != 1 ]; then
for name in dice_py_check bf2_aidll_loader_check bf2_native_vtable_check bf2_meme_dispatch_check \
            macos_gfx_forward; do
    cp "$(find_exe "$name")" "$dest/bin/"
done
cp "$harness/macos_metal_device" "$dest/bin/"
# The harness's menu JPEG checks open BF2_MENU_FIXTURES. Built with the relative path "fixtures"
# (docs/macos-c-arm64-client.md), it reads them from bin/fixtures, where run.sh starts it.
if LC_ALL=C grep -a -q 'macos_d3dx_helpers/fixtures' "$harness/macos_metal_device"; then
    echo "package.sh: macos_metal_device names this checkout's fixtures. Configure the harness with" >&2
    echo "  -DCMAKE_CXX_FLAGS='-UBF2_MENU_FIXTURES -DBF2_MENU_FIXTURES=\\\"fixtures\\\"' (and CMAKE_OBJCXX_FLAGS)" >&2
    exit 1
fi
mkdir -p "$dest/bin/fixtures"
fixtures="$repo/Code/Tests/macos_d3dx_helpers/fixtures"
cp "$fixtures"/*.jpg "$fixtures"/*.bgra "$fixtures"/*.l8 "$dest/bin/fixtures/"
for name in copy_reg types; do
    cp "$repo/Code/BF2/External/python-embedded/py-src-2.3.4/Lib/$name.py" "$dest/check-lib/"
done
fi

# The loopback harness: sandbox profiles, server driver and input scripts unchanged. The only
# edit to the copy moves loopback.py's machine-wide run lock (a path in this Mac's checkout
# layout) into the bundle's out/; BF2_LOOPBACK_LOCK overrides it (an Intel rehearsal points it
# at the shared lock).
cp "$loop/client.sb" "$loop/server.sb" "$loop/server.tcl" "$dest/tools/macos/loopback/"
cp "$loop"/scripts/*.txt "$dest/tools/macos/loopback/scripts/"
# The standard scenarios (loopback.py --scenario), the pose module they install, and the view
# comparison. compare_views.py finds scenarios/<name>/limits.cfg beside itself.
cp -R "$loop/scenarios" "$loop/pose" "$loop/compare_views.py" "$dest/tools/macos/loopback/"
find "$dest/tools/macos/loopback" -name __pycache__ -prune -exec rm -rf {} +
# The Intel reference views, one folder per scenario that has limits.cfg (render-view-*).
nbase=0
for lim in "$loop"/scenarios/*/limits.cfg; do
    sc=$(basename "$(dirname "$lim")")
    if [ -d "$baselines/$sc" ] && ls "$baselines/$sc"/view-*.png >/dev/null 2>&1; then
        mkdir -p "$dest/reference/baselines/$sc"
        cp "$baselines/$sc"/view-*.png "$baselines/$sc"/scenario.txt "$dest/reference/baselines/$sc/"
        nbase=$((nbase + 1))
    else
        echo "package.sh: no Intel baseline for $sc under $baselines (set BF2_BASELINES)" >&2
    fi
done
echo "package.sh: $nbase Intel baseline folder(s) staged"
LOCK_LINE="_lock_path = pathlib.Path(os.environ.get('BF2_LOOPBACK_LOCK') or OWNER_HOME / 'Library/Caches/OpenBF2-loopback/loopback.lock')" \
/usr/bin/python3 -I - "$loop/loopback.py" "$dest/tools/macos/loopback/loopback.py" <<'PY'
import os, sys
src, dst = sys.argv[1], sys.argv[2]
old = os.environ['LOCK_LINE'] + '\n'
new = ("_lock_path = pathlib.Path(os.environ.get('BF2_LOOPBACK_LOCK') or OUT / 'run/loopback.lock')"
       "  # [TEMP] M2 bundle copy (Code/Tests/macos_c_arm64/package.sh)\n")
text = open(src).read()
if text.count(old) != 1:
    sys.exit('package.sh: the loopback.py lock line changed; update package.sh')
open(dst, 'w').write(text.replace(old, new))
PY
diff "$loop/loopback.py" "$dest/tools/macos/loopback/loopback.py" > "$dest/reference/loopback.py.diff" || true

if [ -n "${BF2_INTEL_REFERENCE_RUN:-}" ]; then
    ref=$BF2_INTEL_REFERENCE_RUN
    {
        echo "# Intel x86_64 gate run $(basename "$ref"): unique [FRONTIER] lines (client.log, server.pty.log)"
        grep -h -o '\[FRONTIER\].*' "$ref/client.log" "$ref/server.pty.log" 2>/dev/null \
            | sed 's/[[:space:]]*$//' | LC_ALL=C sort -u || true   # a clean run has none (grep exits 1)
    } > "$dest/reference/intel-frontiers.txt"
    cp "$ref/client.result" "$dest/reference/intel-client.result" 2>/dev/null || true
    grep -c '\[INPUT-SCRIPT\]' "$ref/client.log" > "$dest/reference/intel-input-script-lines.txt" || true
fi

# Optional: BF2_BUNDLE_PYTHON=<python-build-standalone "install_only" .tar.gz for $arch>. Staged as
# python/, run.sh prefers it, so the M2 needs neither Xcode nor the Command Line Tools (macOS's
# /usr/bin/python3 is only a stub that offers that download). Tk is dropped: the harness never
# imports it and its libraries are the only ones that name paths outside the bundle.
if [ -n "${BF2_BUNDLE_PYTHON:-}" ]; then
    tar -xzf "$BF2_BUNDLE_PYTHON" -C "$dest"
    [ -x "$dest/python/bin/python3" ] || { echo "package.sh: $BF2_BUNDLE_PYTHON has no python/bin/python3" >&2; exit 1; }
    rm -rf "$dest"/python/lib/python3.*/{test,idlelib,tkinter,turtledemo} "$dest"/python/lib/tcl* \
        "$dest"/python/lib/tk* "$dest"/python/lib/itcl* "$dest"/python/lib/thread* \
        "$dest"/python/lib/libtcl* "$dest"/python/lib/libtk* "$dest"/python/lib/python3.*/lib-dynload/_tkinter*
    find "$dest/python" -name __pycache__ -prune -exec rm -rf {} +
    echo "$(basename "$BF2_BUNDLE_PYTHON") sha256 $(shasum -a 256 "$BF2_BUNDLE_PYTHON" | cut -d' ' -f1)" \
        > "$dest/reference/python.txt"
else
    echo "package.sh: no BF2_BUNDLE_PYTHON; the M2 then needs the Command Line Tools' python3" >&2
fi

cp "$here/run.sh" "$here/README.txt" "$dest/"
chmod +x "$dest/run.sh"

# Every Mach-O must be a thin arm64 image with a valid ad-hoc signature (arm64 refuses to run an
# unsigned image). The linker already signs; sign again explicitly so the copy is self-contained.
machos=()
while IFS= read -r f; do
    if file -b "$f" | grep -q 'Mach-O'; then machos+=("$f"); fi
done < <(find "$dest" -type f | sort)
for f in "${machos[@]}"; do
    codesign --force --sign - "$f" 2>/dev/null
    codesign -v "$f"
done

{
    echo "source: $(git -C "$repo" rev-parse HEAD) ($(git -C "$repo" status --porcelain | grep -c . || true) uncommitted paths)"
    echo "built: $(date -u '+%Y-%m-%d %H:%M UTC') on $(uname -m) macOS $(sw_vers -productVersion), $(clang --version | head -1)"
    for f in "${machos[@]}"; do
        echo "== ${f#"$dest"/}"
        echo "   $(file -b "$f")"
        echo "   sha256 $(shasum -a 256 "$f" | cut -d' ' -f1)"
        otool -L "$f" | tail -n +2 | grep -v -E '^[[:space:]]*/(System/Library|usr/lib)/' \
            | sed -e 's/^[[:space:]]*/   dep /' -e 's/ (compatibility.*//' || true
        otool -l "$f" | awk '/cmd LC_RPATH/ {r=1} r && /path / {print "   rpath " $2; r=0}'
    done
} > "$dest/artifacts.txt"

# Gates: arm64 only; dependencies and rpaths stay inside the bundle or the OS.
if grep -A1 '^== ' "$dest/artifacts.txt" | grep 'Mach-O' | grep -v -q "$arch"; then
    echo "package.sh: a non-$arch Mach-O was staged" >&2; exit 1
fi
if grep -E '^   (dep|rpath) ' "$dest/artifacts.txt" | grep -v -E ' @(rpath|loader_path|executable_path)(/|$)' | grep -q .; then
    echo 'package.sh: a staged image references a path outside the bundle and the OS:' >&2
    grep -E '^   (dep|rpath) ' "$dest/artifacts.txt" | grep -v -E ' @(rpath|loader_path|executable_path)(/|$)' >&2
    exit 1
fi
# Debug info and __FILE__ strings name the build machine's source tree; harmless at run time, listed only.
LC_ALL=C grep -a -l -E "$HOME/|/opt/homebrew|/usr/local/(Cellar|opt)" "${machos[@]}" \
    | sed "s|^$dest/||" > "$dest/reference/build-path-strings.txt" || true

(cd "$dest" && find . -type f ! -name MANIFEST.sha256 -print0 | sort -z | xargs -0 shasum -a 256 > MANIFEST.sha256)

tgz="$(dirname "$dest")/$(basename "$dest")-$(git -C "$repo" rev-parse --short HEAD).tgz"
COPYFILE_DISABLE=1 tar -C "$(dirname "$dest")" -czf "$tgz" "$(basename "$dest")"
echo "Bundle staged at $dest ($(du -sh "$dest" | cut -f1)); archive $tgz ($(du -h "$tgz" | cut -f1))"
