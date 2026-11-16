#!/bin/bash
# Runs the retail Windows BF2 client under Wine + DXVK from the prepared game dir.
# Usage: Code/Tests/run_game/linux_ubuntu24.04_ZorinOS.sh [extra BF2.exe args]
set -euo pipefail
source "$(dirname "$0")/../run_common/linux_ubuntu24.04_ZorinOS.sh"

bf2_prepare_game_dir
cd "$BF2_GAME_DIR"
export WINEDEBUG="${WINEDEBUG:--all}"
exec wine bf2.exe +menu 1 "$@"  # e.g. +fullscreen 1 +szx 1920 +szy 1080
