#!/bin/sh
# [TEMP] [CHOSEN, not recovered] AddressSanitizer + UBSan renderer-ON macOS client: configure,
# build and run through a sandboxed client runner.
#
#   tools/macos/asan-client.sh configure <tag>   # out/build/<tag>-asan
#   tools/macos/asan-client.sh build <tag>       # bf2_client only
#   tools/macos/asan-client.sh run <tag> [secs]  # default 150 s; reports in out/asan-logs/
#
# The run needs RUNNER: a client runner called as `$RUNNER <worktree> <build-tag>`.
# Effects compile natively. ASan and UBSan share one log_path,
# so both kinds of report land in out/asan-logs/san.<pid>.
set -eu
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
CMD=${1:?configure|build|run}
TAG=${2:?tag}
BUILD="$TAG-asan"
# vptr/function: the renderer seam calls through explicit native tables on purpose
# (tools/macos/vtable_audit.py); alignment: retail packed structures. Everything else stays on.
SAN='-fsanitize=address,undefined -fno-sanitize=vptr,function,alignment -fno-omit-frame-pointer -fno-sanitize-recover=address'
case "$CMD" in
configure)
    cmake --preset macos-x64 -S "$ROOT" -B "$ROOT/out/build/$BUILD" -DBF2_MACOS_LINK_RENDDX9=ON \
        "-DCMAKE_C_FLAGS=$SAN" "-DCMAKE_CXX_FLAGS=$SAN" "-DCMAKE_OBJC_FLAGS=$SAN" "-DCMAKE_OBJCXX_FLAGS=$SAN" \
        "-DCMAKE_EXE_LINKER_FLAGS=-fsanitize=address,undefined" \
        "-DCMAKE_SHARED_LINKER_FLAGS=-fsanitize=address,undefined" \
        "-DCMAKE_MODULE_LINKER_FLAGS=-fsanitize=address,undefined" ;;
build)
    cmake --build "$ROOT/out/build/$BUILD" --target bf2_client -j"${JOBS:-6}" ;;
run)
    [ -n "${RUNNER:-}" ] || { echo "asan-client.sh run: set RUNNER to a client runner (<worktree> <build-tag>)" >&2; exit 2; }
    mkdir -p "$ROOT/out/asan-logs"
    LOG="$ROOT/out/asan-logs/san"
    ASAN_OPTIONS=${ASAN_OPTIONS:-abort_on_error=1:detect_leaks=0:halt_on_error=1:log_path=$LOG} \
    UBSAN_OPTIONS=${UBSAN_OPTIONS:-print_stacktrace=1:log_path=$LOG} \
    BF2_RUN_SECONDS=${3:-150} "$RUNNER" "$ROOT" "$BUILD" || true
    NEW=$(ls -t "$LOG".* 2>/dev/null | head -1 || true)
    if [ -n "$NEW" ]; then
        echo "--- sanitizer log $NEW"
        grep -c "runtime error" "$NEW" | sed 's/^/UBSan findings: /'
        grep -n "ERROR: AddressSanitizer" -A14 "$NEW" | cut -c1-200 || echo "no ASan error"
    fi ;;
*) echo "unknown command $CMD" >&2; exit 2 ;;
esac
