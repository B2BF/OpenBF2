#!/bin/bash
# [TEMP] fourway: guest desktop capture (works with the VM in the background): gshot.sh NAME
# -> out/fourway/gshot/NAME.png and NAME-s.png (1400 px wide)
V="${FOURWAY_VM:-My Boot Camp}"; R="$(cd "$(dirname "$0")/../../.." && pwd)"; D="$R/out/fourway/gshot"; mkdir -p "$D"
# The worktree as the guest sees it through the Parallels home share.
S="\\\\Mac\\Home\\$(printf %s "${R#"$HOME"/}" | tr / '\\\\')"
prlctl exec "$V" cmd /c 'del C:\rendfw\shot.png 2>nul & schtasks /run /tn rendfw_shot >nul' >/dev/null 2>&1
for i in $(seq 1 12); do sleep 1; prlctl exec "$V" cmd /c "if exist C:\\rendfw\\shot.png (copy /y C:\\rendfw\\shot.png $S\\out\\fourway\\gshot\\$1.png >nul && echo y)" 2>/dev/null | grep -q y && break; done
sips -Z 1400 "$D/$1.png" --out "$D/$1-s.png" >/dev/null && echo "$D/$1-s.png"
