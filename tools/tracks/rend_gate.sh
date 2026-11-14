#!/bin/zsh
# [TEMP] rend track runtime gate (macOS loopback). Builds the client, server and vtable audit, runs the
# standard scenarios (tools/macos/loopback/scenarios), compares the render views against the accepted
# baselines (out/baselines/<scenario>, saved with compare_views.py --baseline-save) and runs the map2
# reload smoke, which no scenario covers yet. Prints one line per step and exits non-zero on any failure.
#   tools/tracks/rend_gate.sh [build]            (default build: rend-track)
#   REND_GATE_SERVER=1 adds a dedicated-server-only run (shared Core/Math changes).
B=${1:-rend-track}
cd "${0:A:h}/../.."
LB=tools/macos/loopback/loopback.py
# Refuse to gate a half-merged tree (unmerged paths or leftover conflict markers).
if [[ -n "$(git diff --name-only --diff-filter=U)" ]] || git grep -qE '^(<<<<<<<|>>>>>>>) ' -- Code tools 2>/dev/null; then
  echo "gate: refused - unresolved merge conflicts"; exit 2
fi
fail=0
last() { ls -td out/loopback/run-* | head -1 }
check() {   # run label
  local R=$1 n ips res
  n=$(grep -rh '\[FRONTIER\]' $R 2>/dev/null | wc -l | tr -d ' ')
  ips=$(find $R -name '*.ips' | wc -l | tr -d ' ')
  res=$(cat $R/client.result 2>/dev/null)
  echo "$2 -> $R frontier=$n ips=$ips $res"
  [[ $n == 0 && $ips == 0 ]] || fail=1
}
cmake --build out/build/$B --target bf2_client bf2 bf2_vtable_audit -j4 2>&1 \
  | grep -E "error:|failures|Built target (bf2_client|bf2)$" | sort -u
for sc in infantry-range render-view-dalian render-view-ocs; do
  python3 $LB --build $B --scenario $sc >/dev/null 2>&1
  R=$(last); check $R $sc
  grep -q "timeout=True" $R/client.result 2>/dev/null && { echo "  $sc did not finish its script"; fail=1; }
  if [[ $sc == render-view-* && -d out/baselines/$sc ]]; then
    python3 tools/macos/loopback/compare_views.py out/baselines/$sc $R 2>&1 | tail -12 | sed 's/^/  /'
    [[ ${pipestatus[1]} == 0 ]] || fail=1
  fi
done
python3 $LB --build $B --ai --bots 16 --ai-spawn-only --spawn-time 1 --client-seconds 300 \
  --map2 operation_clean_sweep --server-send 120:admin.runNextLevel --input-script play-combat >/dev/null 2>&1
check $(last) map2-reload
echo "gate: $([[ $fail == 0 ]] && echo PASS || echo FAIL)"
exit $fail
