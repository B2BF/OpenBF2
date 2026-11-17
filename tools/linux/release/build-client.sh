#!/bin/bash
# Build and package the Linux graphical distribution in the pinned baseline SDK.
set -euo pipefail
if [[ $# != 4 ]]; then
  echo 'Usage: build-client.sh ENGINE_CHECKOUT SETUP_CHECKOUT NEW_OUTPUT SDK_MANIFEST_REFERENCE' >&2
  exit 2
fi
engine=$(realpath "$1")
setup=$(realpath "$2")
output=$(realpath -m "$3")
sdk_identity=$4
sdk=/opt/openbf2-sdk
sources=/opt/openbf2-sources
[[ ! -e "$output" ]] || { echo 'Choose a new output directory' >&2; exit 1; }
[[ $(uname -m) == x86_64 && $(getconf GNU_LIBC_VERSION) == 'glibc 2.35' ]] || { echo 'Use the x86-64 glibc 2.35 SDK' >&2; exit 1; }
[[ $(g++-15 -dumpfullversion) == 15.2.0 ]] || { echo 'GCC 15.2.0 is required' >&2; exit 1; }
[[ "$sdk_identity" =~ ^localhost/openbf2-linux-sdk@sha256:[0-9a-f]{64}$ ]] || { echo 'Use the exported OCI manifest identity' >&2; exit 1; }
[[ -z ${CFLAGS:-}${CXXFLAGS:-}${CPPFLAGS:-}${LDFLAGS:-}${RUSTFLAGS:-} ]] || { echo 'Unexpected inherited compiler flags' >&2; exit 1; }
git config --global --add safe.directory "$engine"
git config --global --add safe.directory "$setup"
[[ -z $(git -C "$engine" status --porcelain --untracked-files=normal) ]] || { echo 'Engine checkout must be clean and committed' >&2; exit 1; }
[[ -z $(git -C "$setup" status --porcelain --untracked-files=normal) ]] || { echo 'Setup checkout must be clean and committed' >&2; exit 1; }
revision=$(git -C "$engine" rev-parse HEAD)
mkdir -p "$output"
export CC=gcc-15 CXX=g++-15
export CMAKE_PREFIX_PATH="$sdk"
export PKG_CONFIG_PATH="$sdk/lib/pkgconfig"
export LD_LIBRARY_PATH="$sdk/lib"
# Setup remaps its own source/build roots in Rust. Keep registry source paths
# beneath that remapped source root too, in the fresh checkout's ignored cache.
export CARGO_HOME="$setup/out/linux-release-cargo"
jobs=${BF2_RELEASE_JOBS:-2}
[[ "$jobs" =~ ^[1-9][0-9]*$ ]] || { echo 'BF2_RELEASE_JOBS must be positive' >&2; exit 1; }

# Source and binary roots are local audit inputs, never part of the package.
python3 - "$output/build-roots.json" "$engine" "$setup" "$output" "$sdk" "$sources" /opt/openbf2-sdk-build /opt/boost-1.89.0 <<'PY'
import json
import sys
from pathlib import Path
Path(sys.argv[1]).write_text(json.dumps(sys.argv[2:], indent=2) + "\n")
PY
maps="-ffile-prefix-map=$engine=. -ffile-prefix-map=$output=./build -ffile-prefix-map=$sdk=./sdk -ffile-prefix-map=$sources=./sources"
flags="-march=x86-64 -mtune=generic $maps"
env CFLAGS="$flags" CXXFLAGS="$flags" CPPFLAGS="$flags" \
  python3 "$engine/tools/linux/build-nativefx.py" \
    --vkd3d-source "$sources/vkd3d-2.1" \
    --mojo-source "$sources/mojoshader" \
    --output "$output/nativefx" --jobs "$jobs"

# A separate SDK library path keeps build tests runnable while staging later
# assigns the distributable $ORIGIN paths. No build directory enters the package.
cmake --preset linux-client -S "$engine" -B "$output/engine-build" \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=gcc-15 -DCMAKE_CXX_COMPILER=g++-15 \
  -DBF2_FRONTIER_DEFAULT_CONTINUE=ON \
  -DCMAKE_C_FLAGS="$flags" -DCMAKE_CXX_FLAGS="$flags" \
  -DCMAKE_SKIP_RPATH=ON -DCMAKE_PREFIX_PATH="$sdk" \
  -DBF2_DXVK_ROOT="$sdk" -DBF2_FX_NATIVE_DEPS="$output/nativefx" \
  -DBF2_GAME_DIR:PATH="$output/no-game-content"
export LD_LIBRARY_PATH="$output/engine-build/bin_linux:$sdk/lib"
cmake --build "$output/engine-build" --target bf2_client bf2_linux_client_checks -j"$jobs"
python3 "$engine/tools/linux/release/stage-artifacts.py" \
  --engine-bin "$output/engine-build/bin_linux" --sdk-prefix "$sdk" \
  --sdk "$sdk_identity" --toolchain "$setup/linux/toolchain.json" \
  --catalog "$sdk/share/openbf2/sdk-provenance.json" \
  --setup-package "$setup/linux/package.py" --revision "$revision" \
  --output "$output/artifacts" --build-roots-file "$output/build-roots.json"

# Fetch only the locked Rust build inputs. Setup's release entry point and its
# CMake/Cargo compilation remain offline, with no game media in CI.
cargo fetch --locked --target x86_64-unknown-linux-gnu --manifest-path "$setup/core/rtp-bridge/Cargo.toml"
export LD_LIBRARY_PATH="$sdk/lib"
export SETUP_CLIENT_ARTIFACT="$output/artifacts/engine"
export SETUP_RUNTIME_ARTIFACT="$output/artifacts/runtime"
export SETUP_SDK_PREFIX="$sdk" SETUP_BOOST_ROOT=/opt/boost-1.89.0
export SETUP_ARTIFACT_BUILD_ROOTS_FILE="$output/build-roots.json" SETUP_JOBS="$jobs"
bash "$setup/linux/build-release.sh" \
  "$output/packages/openbf2-linux-x86_64-client.tar.gz" "$output/setup-build"
(
  cd "$output/packages"
  sha256sum --check openbf2-linux-x86_64-client.tar.gz.sha256
)
