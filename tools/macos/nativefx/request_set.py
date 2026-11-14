#!/usr/bin/env python3
"""[CHOSEN, not recovered] Request list and engine groups for one configuration of the
c-fx-permutations prebuild (read only): LIST.tsv rows id, key, d3dx_ok, d3dx_class, and the
groups JSON with each group's attempts and first D3DX success."""
import argparse, json
from pathlib import Path

ap = argparse.ArgumentParser()
ap.add_argument("--cfx", required=True, help="the c-fx-permutations prebuild output directory (out/c-fx)")
ap.add_argument("--config", default="L3S1F3T3")
ap.add_argument("--output", default="out/nativefx")
a = ap.parse_args()
cfx, out = Path(a.cfx), Path(a.output)
out.mkdir(parents=True, exist_ok=True)
state = json.load(open(cfx / f"prebuild-v1-{a.config}/state.json"))["results"]
with open(out / f"{a.config}.tsv", "w") as f:
    for k, v in sorted(state.items()):
        f.write(f"{k}\t{v['key']}\t{1 if v['ok'] else 0}\t{v.get('class') or ''}\n")
with open(out / f"{a.config}-accepted.tsv", "w") as f:
    for k, v in sorted(state.items()):
        if v["ok"]: f.write(f"{k}\t{v['key']}\t1\t\n")
groups = []
for g in json.load(open(cfx / "requests/groups.json")):
    if g["config"] != a.config: continue
    first = next((i for i, x in enumerate(g["attempts"]) if state.get(x, {}).get("ok")), None)
    groups.append(dict(effect=g["effect"], kind=g["kind"], attempts=g["attempts"], d3dx_first=first))
json.dump(groups, open(out / f"groups-{a.config}.json", "w"), indent=0)
print(len(state), "requests,", sum(v["ok"] for v in state.values()), "accepted,", len(groups), "groups")
