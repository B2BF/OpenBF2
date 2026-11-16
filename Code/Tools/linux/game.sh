#!/bin/bash
# Runs the retail Windows BF2 client under Wine + DXVK from the prepared game dir.
# Usage: Code/Tools/linux/game.sh [extra BF2.exe args]
set -euo pipefail
source "$(dirname "$0")/common.sh"
bf2_require wine:wine python3:python3 pgrep:procps
# BF2.exe is 32-bit; Debian/Ubuntu ship the 32-bit Wine parts separately.
if command -v dpkg >/dev/null && ! dpkg -s wine32:i386 >/dev/null 2>&1; then
    echo "Missing 32-bit Wine. Install it with:" >&2
    echo "  sudo dpkg --add-architecture i386 && sudo apt update && sudo apt install wine32:i386" >&2
    exit 1
fi

# BF2 allows one instance and a second start exits silently, so say so instead.
if pid="$(pgrep -i -x bf2.exe)"; then
    echo "BF2 is already running (pid $pid). Close it or run: kill $pid" >&2
    exit 1
fi

bf2_prepare_game_dir
cd "$BF2_GAME_DIR"

# Skip the intro movies by renaming them: Init.con must stay unmodified, because the server's
# content check hashes it, while BF2 simply skips movies it cannot find. BF2_INTROS=1 restores them.
for movie in ea dice legal intro welcome; do
    f="mods/bf2/movies/$movie.bik"
    if [ "${BF2_INTROS:-0}" = 1 ]; then
        [ -e "$f.off" ] && mv "$f.off" "$f"
    else
        [ -e "$f" ] && mv "$f" "$f.off"
    fi
done
export WINEDEBUG="${WINEDEBUG:--all}" DXVK_LOG_LEVEL="${DXVK_LOG_LEVEL:-warn}"

# A copied (not installed) game has no CD-key value. BF2.exe then encrypts an empty key on join,
# ignores the failure and dereferences a null blob (PE 0x59F78D -> 0x5B0400). LAN play needs any value.
ergc='HKLM\SOFTWARE\Electronic Arts\EA Games\Battlefield 2\ergc'
if ! WINEDEBUG=-all wine reg query "$ergc" /reg:32 >/dev/null 2>&1; then
    echo "Adding a placeholder CD-key registry value (needed to join servers)"
    WINEDEBUG=-all wine reg add "$ergc" /ve /t REG_SZ /d OPENBF2-LAN-PLACEHOLDER /f /reg:32 >/dev/null
fi
# Player name: keep exactly one local profile, the one Global.con selects. In a terminal, ask
# for the name each start (Enter keeps it); BF2_PLAYER_NAME sets it without asking.
profiles="$WINEPREFIX/drive_c/users/$USER/Documents/Battlefield 2/Profiles"
current="$(sed -n 's/^GlobalSettings.setDefaultUser "\([0-9]*\)".*/\1/p' "$profiles/Global.con" 2>/dev/null | tr -d '\r' || true)"
current_name="$(sed -n 's/^LocalProfile.setName "\(.*\)".*/\1/p' "$profiles/$current/Profile.con" 2>/dev/null | tr -d '\r' || true)"
name="${BF2_PLAYER_NAME:-}"
# BF2 lists a profile only with Profile.con, General.con, ServerSettings.con and Audio.con, and
# only the game creates those, so a name can be set from the second start on.
if [ -z "$current" ] || [ ! -f "$profiles/$current/General.con" ]; then
    [ -n "$name" ] && echo "No BF2 profile yet: start the game once, then set the player name." >&2
    name=""
elif [ -z "$name" ] && [ -t 0 ]; then
    read -r -p "Player name [$current_name]: " name
fi
if [ -n "$name" ] && [ "$name" != "$current_name" ]; then
    # The name is written into a .con script the game executes, so only allow plain characters.
    [[ "$name" =~ ^[A-Za-z0-9_.-]{1,20}$ ]] || { echo "Player name: 1-20 of A-Z a-z 0-9 _ . -" >&2; exit 1; }
    others=()
    for d in "$profiles"/[0-9][0-9][0-9][0-9]; do
        [ -d "$d" ] && [ "$d" != "$profiles/$current" ] && others+=("$d")
    done
    if [ -z "${BF2_PLAYER_NAME:-}" ]; then
        read -r -p "This deletes the account \"$current_name\"$([ ${#others[@]} -gt 0 ] && echo " and ${#others[@]} other account(s)"). Continue? [y/N] " ok
        [[ "$ok" =~ ^[yY]$ ]] || name=""
    fi
fi
if [ -n "$name" ] && [ "$name" != "$current_name" ]; then
    # Replace the account but keep its settings files; fresh stats, no stored password.
    printf 'LocalProfile.setName "%s"\r\nLocalProfile.setNick "%s"\r\nLocalProfile.setGamespyNick ""\r\nLocalProfile.setTotalPlayedTime 0\r\nLocalProfile.setNumTimesLoggedIn 0\r\n' \
        "$name" "$name" > "$profiles/$current/Profile.con"
    rm -f "$profiles/$current/Profile.con.bak"
    for d in "${others[@]}"; do rm -r -- "$d"; done
    echo "Account is now $name"
fi

# BF2's windowed mode renders badly under Wine; default to fullscreen at the desktop resolution.
mode="$(xrandr --current 2>/dev/null | grep -m1 -o '[0-9]*x[0-9]* *[0-9.]*\*' | cut -d' ' -f1 || true)"
mode="${mode:-1280x720}"
exec wine bf2.exe +menu 1 +fullscreen "${BF2_FULLSCREEN:-1}" \
    +szx "${BF2_SZX:-${mode%x*}}" +szy "${BF2_SZY:-${mode#*x}}" "$@"
