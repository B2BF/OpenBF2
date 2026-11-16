# Shared settings for the run_* test launchers on Ubuntu 24.04 / Zorin OS 18. Source, do not run.
# Every value can be overridden from the environment.
# [TEMP] [CHOSEN, not recovered] local test tooling, not part of any shipped image.

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
BF2_INSTALL="${BF2_INSTALL:-}"                            # untouched Windows install, needed once
BF2_GAME_DIR="${BF2_GAME_DIR:-$HOME/bf2-game}"            # prepared by prepare_linux_game_dir.py
BF2_BUILD="${BF2_BUILD:-$HOME/bf2-linux-local}"           # linux-x64 CMake build directory
BF2_MAP="${BF2_MAP:-strike_at_karkand}"
BF2_MODE="${BF2_MODE:-gpm_cq}"
BF2_MAP_SIZE="${BF2_MAP_SIZE:-16}"
WINEPREFIX="${WINEPREFIX:-$HOME/.wine-bf2}"               # prefix with DXVK installed
export WINEPREFIX

# All UDP ports a server on game port $1 binds: game, GameSpy, VoIP server/BF client/BF server.
bf2_ports() {
    local off=$(($1 - 16567))
    echo "$1 $((29900 + off)) $((55125 + 3 * off)) $((55123 + 3 * off)) $((55124 + 3 * off))"
}
bf2_ports_free() {
    local p
    for p in $(bf2_ports "$1"); do
        ss -Hlun "sport = :$p" | grep -q . && return 1
    done
    return 0
}

# Sets and exports BF2_PORT for a new server. Without BF2_PORT, takes the first free set from 16568
# up, so 16567 stays reserved for the owner; an explicit BF2_PORT must be free.
bf2_pick_port() {
    if [ -z "${BF2_PORT:-}" ]; then
        BF2_PORT=16568
        until bf2_ports_free "$BF2_PORT"; do BF2_PORT=$((BF2_PORT + 1)); done
    elif ! bf2_ports_free "$BF2_PORT"; then
        echo "A port of the set $(bf2_ports "$BF2_PORT") is in use; pick another BF2_PORT or omit it." >&2
        exit 1
    fi
    export BF2_PORT
}

bf2_prepare_game_dir() {
    [ -d "$BF2_GAME_DIR" ] && return
    [ -n "$BF2_INSTALL" ] || { echo "Set BF2_INSTALL to your Battlefield 2 install to prepare $BF2_GAME_DIR." >&2; exit 1; }
    echo "Preparing $BF2_GAME_DIR from $BF2_INSTALL"
    python3 -I "$REPO/Code/Build/prepare_linux_game_dir.py" "$BF2_INSTALL" "$BF2_GAME_DIR"
}
