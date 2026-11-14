#!/usr/bin/env python3
"""[CHOSEN, not recovered] Developer oracle, not used by the client.

Finds the attempts of enumerated effect groups whose D3DX outcome the refusal table lacks, after
outcomes changed (piece 17a: native Windows CompileEffect refuses permutations the Wine prebuild
accepted, so the engine moves on to attempts the prebuild never ran).

For each SET (an enumerated requests directory with groups.json and jobs.json, the prebuild's
state.json and its cache requests directory), every group is walked in attempt order:
an attempt with a request snapshot takes the table's outcome for its refusal key (A ends the
group); the first attempt without a snapshot or without a table outcome is the frontier. Writes
OUT/<set>-jobs.json (the frontier jobs, a build-cache.py --capture-only manifest) and prints the
counts. Groups of `.mfx` metadata (ra_metadata) are skipped (reflection only, not in the table).

  next_attempts.py --set REQDIR:STATE:CACHE_REQUESTS[:EXTRA_REQUESTS...] [--set ...] --table FILE --out DIR
"""
import argparse, json, subprocess, sys, tempfile
from pathlib import Path

REPO = Path(__file__).resolve().parents[3]
ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
ap.add_argument("--set", action="append", required=True)
ap.add_argument("--table", type=Path, default=REPO / "Code/BF2/Platform/Gfx/Effect/data/d3dx-refusals.txt")
ap.add_argument("--tool", type=Path, default=REPO / "out/build/nativefx/tool/nativefx")
ap.add_argument("--out", type=Path, required=True)
a = ap.parse_args()
a.out.mkdir(parents=True, exist_ok=True)
table = {}
for line in a.table.read_text().splitlines():
    if line and not line.startswith("#"):
        k, v = line.split()
        table[k] = v


def refusal_keys(paths):
    with tempfile.NamedTemporaryFile("w", dir=REPO / "out", delete=False, suffix=".txt") as f:
        f.write("".join(str(p) + "\n" for p in paths))
        listing = Path(f.name)
    out = subprocess.run([str(a.tool), "refusal-keys", str(listing)], capture_output=True, text=True, check=True).stdout
    listing.unlink()
    return {l.split()[0]: l.split()[1][:16] for l in out.splitlines()}


total = dict(groups=0, resolved=0, exhausted=0, frontier=0)
for spec in a.set:
    parts = spec.split(":")
    reqdir, state_path, cache_dirs = Path(parts[0]), Path(parts[1]), [Path(p) for p in parts[2:]]
    groups = json.load(open(reqdir / "groups.json"))
    jobs = {j["id"]: j for j in json.load(open(reqdir / "jobs.json"))}
    state = json.load(open(state_path))["results"]
    # Snapshot per job: the prebuild's cache key (state) or a later capture (capture work dirs keep
    # job -> key in results.json); then the refusal key of that snapshot.
    snapshot = {}
    for job, r in state.items():
        for d in cache_dirs:
            p = d / f"{r['key']}.request"
            if p.exists():
                snapshot[job] = p
                break
    for d in cache_dirs:
        res = d.parent / "capture-index.json"
        if res.exists():
            for job, key in json.load(open(res)).items():
                p = d / f"{key}.request"
                if p.exists() and job not in snapshot:
                    snapshot[job] = p
    keys = refusal_keys(sorted(set(snapshot.values())))
    rkey = {job: keys.get(p.stem) for job, p in snapshot.items()}
    frontier, resolved, exhausted, counted = [], 0, 0, 0
    for g in groups:
        if g["kind"] == "ra_metadata":
            continue
        counted += 1
        for job in g["attempts"]:
            outcome = table.get(rkey.get(job) or "")
            if outcome == "A":
                resolved += 1
                break
            if outcome is None:
                frontier.append(job)
                break
        else:
            exhausted += 1
    need = sorted(set(frontier))
    name = reqdir.name
    (a.out / f"{name}-jobs.json").write_text(json.dumps([jobs[j] for j in need], indent=0))
    print(f"{name}: {counted} groups, {resolved} resolved (A), {exhausted} without an accepted attempt, "
          f"{len(frontier)} at a frontier ({len(need)} distinct jobs)")
    total["groups"] += counted; total["resolved"] += resolved; total["exhausted"] += exhausted; total["frontier"] += len(need)
print("next-attempts: " + ", ".join(f"{k} {v}" for k, v in total.items()))
