#!/bin/bash
# Starts the Linux server, waits until it listens, joins it with the Wine client and stops the
# server when the client exits. Server console output goes to $BF2_LOG.
# Usage: Code/Tools/linux/play.sh  (BF2_PORT optional, else first free)
set -euo pipefail
here="$(dirname "$0")"
source "$here/common.sh"
bf2_require script:bsdutils ss:iproute2
bf2_pick_port
BF2_LOG="${BF2_LOG:-/tmp/bf2-server-$BF2_PORT.log}"
bf2_prepare_game_dir  # in the foreground, so setup errors show here and not only in the log

# The server's ncurses console needs a pty; script(1) provides one without a terminal window,
# and a known terminal type, which desktop launchers do not set.
[ "${TERM:-dumb}" = dumb ] && export TERM=xterm
script -qfc "$(printf '%q ' "$here/server.sh")" "$BF2_LOG" >/dev/null 2>&1 &
server=$!
# script(1) stops the server it runs when killed; wait so shutdown ends before the prompt returns.
trap 'kill $server 2>/dev/null; wait $server 2>/dev/null || true' EXIT

echo "Building and starting the server on UDP $BF2_PORT (progress: tail -f $BF2_LOG)"
for _ in $(seq 600); do  # the first run includes a full build
    ss -Hlun "sport = :$BF2_PORT" | grep -q . && break
    # jobs, not kill -0: an exited but unreaped job still answers kill -0.
    jobs -rp | grep -qx "$server" || { echo "Server exited:" >&2; tail -n 15 "$BF2_LOG" >&2; exit 1; }
    sleep 1
done
ss -Hlun "sport = :$BF2_PORT" | grep -q . || { echo "Server did not listen in time" >&2; exit 1; }

"$here/game.sh" +joinServer 127.0.0.1 +port "$BF2_PORT" "$@"
