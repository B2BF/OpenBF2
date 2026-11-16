#!/bin/bash
# Starts the Linux server, waits until it listens, joins it with the Wine client and stops the
# server when the client exits. Server console output goes to $BF2_LOG.
# Usage: Code/Tests/run_game_join_server/linux_ubuntu24.04_ZorinOS.sh  (BF2_PORT optional, else first free)
set -euo pipefail
here="$(dirname "$0")"
source "$here/../run_common/linux_ubuntu24.04_ZorinOS.sh"
bf2_pick_port
BF2_LOG="${BF2_LOG:-/tmp/bf2-server-$BF2_PORT.log}"

# The server's ncurses console needs a pty; script(1) provides one without a terminal window.
script -qfc "$(printf '%q ' "$here/../run_server/linux_ubuntu24.04_ZorinOS.sh")" "$BF2_LOG" >/dev/null &
server=$!
trap 'kill $server 2>/dev/null; pkill -f "run_server_$BF2_PORT.con" 2>/dev/null || true' EXIT

echo "Waiting for the server on UDP $BF2_PORT (log: $BF2_LOG)"
for _ in $(seq 600); do  # first run includes the build and game-dir preparation
    ss -Hlun "sport = :$BF2_PORT" | grep -q . && break
    kill -0 $server 2>/dev/null || { echo "Server exited, see $BF2_LOG" >&2; exit 1; }
    sleep 1
done
ss -Hlun "sport = :$BF2_PORT" | grep -q . || { echo "Server did not listen in time" >&2; exit 1; }

"$here/../run_game/linux_ubuntu24.04_ZorinOS.sh" +joinServer 127.0.0.1 +port "$BF2_PORT" "$@"
