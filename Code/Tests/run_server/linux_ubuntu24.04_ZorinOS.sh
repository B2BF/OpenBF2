#!/bin/bash
# Builds the native Linux server from this checkout and runs it in the foreground (needs a terminal).
# Usage: Code/Tests/run_server/linux_ubuntu24.04_ZorinOS.sh [extra bf2 args]  (BF2_PORT optional, else first free)
set -euo pipefail
source "$(dirname "$0")/../run_common/linux_ubuntu24.04_ZorinOS.sh"
bf2_pick_port

[ -d "$BF2_BUILD" ] || cmake --preset linux-x64 -S "$REPO" -B "$BF2_BUILD"
cmake --build "$BF2_BUILD" --target bf2
bf2_prepare_game_dir

echo "Server ports (game, GameSpy, VoIP x3): $(bf2_ports "$BF2_PORT")"
read -r game gamespy voip voipclient voipserver <<< "$(bf2_ports "$BF2_PORT")"
settings="$BF2_GAME_DIR/mods/bf2/settings"
sed -e "s/^sv.serverPort .*/sv.serverPort $game/" \
    -e "s/^sv.gameSpyPort .*/sv.gameSpyPort $gamespy/" \
    -e "s/^sv.voipServerPort .*/sv.voipServerPort $voip/" \
    -e "s/^sv.voipBFClientPort .*/sv.voipBFClientPort $voipclient/" \
    -e "s/^sv.voipBFServerPort .*/sv.voipBFServerPort $voipserver/" \
    "$settings/serversettings.con" > "$settings/run_server_$BF2_PORT.con"
echo "mapList.append $BF2_MAP $BF2_MODE $BF2_MAP_SIZE" > "$settings/run_maps_$BF2_PORT.con"

cd "$BF2_GAME_DIR"
# +config/+mapList must be relative to the game dir; absolute paths are ignored.
exec "$BF2_BUILD/bin_linux/bf2" +modPath mods/bf2 \
    +config "mods/bf2/settings/run_server_$BF2_PORT.con" \
    +mapList "mods/bf2/settings/run_maps_$BF2_PORT.con" "$@"
