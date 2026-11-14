#!/bin/bash
# [TEMP] [CHOSEN, not recovered] External focus driver for unattended deactivate/reactivate runs
# (the client must run with BF2_INPUT_SCRIPT_REAL_FOCUS=1). Waits for the newest run directory under
# <runs-dir> whose client.log is newer than this script's start, then at each marker the client logs
# (an input-script "log" line): screenshot, activate Finder, wait <away-seconds>, screenshot, bring
# bf2_client back to the front, screenshot after 3 s and 9 s (<shots-dir>/<marker>-{1..4}-*.png).
# Ends by activating Terminal and printing the focus transitions and device-reset broadcasts.
#   usage: focus-cycle.sh <runs-dir> <shots-dir> <away-seconds> <marker>...
#   runs-dir: out/loopback for loopback.py, out/runs for sp-run.sh
RUNS=$1; SH=$2; AWAY=$3; shift 3; mkdir -p "$SH"
start=$(date +%s)
run=""
while [ -z "$run" ]; do
  d=$(ls -td "$RUNS"/run-* 2>/dev/null | head -1)
  if [ -n "$d" ] && [ -f "$d/client.log" ]; then
    m=$(stat -f %m "$d/client.log"); [ "$m" -ge "$start" ] && run=$d
  fi
  sleep 1
  [ $(( $(date +%s) - start )) -gt 2400 ] && { echo "focus-cycle: no run"; exit 1; }
done
echo "run=$run"
for mk in "$@"; do
  until grep -a -q "$mk" "$run/client.log" 2>/dev/null; do
    sleep 1
    [ $(( $(date +%s) - start )) -gt 2400 ] && { echo "focus-cycle: timeout at $mk"; exit 1; }
  done
  echo "$mk $(date +%T)"
  sleep 2; screencapture -x "$SH/$mk-1-before.png"
  echo "deactivate $(date +%T)"; osascript -e 'tell application "Finder" to activate'
  sleep "$AWAY"; screencapture -x "$SH/$mk-2-away.png"
  osascript -e 'tell application "System Events" to get name of first application process whose frontmost is true'
  echo "reactivate $(date +%T)"
  osascript -e 'tell application "System Events" to set frontmost of (first process whose name is "bf2_client") to true'
  sleep 3
  osascript -e 'tell application "System Events" to get name of first application process whose frontmost is true'
  screencapture -x "$SH/$mk-3-back.png"
  sleep 6; screencapture -x "$SH/$mk-4-back2.png"
done
echo "done $(date +%T)"
sleep 15
osascript -e 'tell application "Terminal" to activate'
grep -a -n "\[host\] focus\|TABOUT\|broadcast #" "$run/client.log"
echo "run=$run"
