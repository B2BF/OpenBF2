#!/usr/bin/env python3
"""[CHOSEN, not recovered] Our bytecode against retail D3DX's for every exported shader job.

  d3dx_jobs_compare.py RESULT OURS_DIR [--runs N] [--show K] [--list FILE]

RESULT is d3dx_jobs_result.txt (Code/Tests/d3dx_jobs, run on Windows over `nativefx export-jobs`
output); OURS_DIR holds <key>.bin from `nativefx compile-jobs`. Each job D3DX compiled is run in
the D3D9 interpreter of d3dbc_compare.py against ours with constants mapped by CTAB name and vertex
inputs by usage (compare_named). Reported per profile and for loop-bearing jobs; exit status 1 on
any semantic difference. Programs the interpreter cannot run (ps_1_x texture instructions, for
example) are counted separately with the reason.
"""
import argparse, collections, sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import d3dbc_compare  # noqa: E402


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("result")
    ap.add_argument("ours")
    ap.add_argument("--runs", type=int, default=4)
    ap.add_argument("--show", type=int, default=20)
    ap.add_argument("--list", help="write one line per difference here")
    a = ap.parse_args()
    ours = Path(a.ours)
    stats = collections.defaultdict(collections.Counter)
    reasons = collections.Counter()
    diffs = []
    for line in Path(a.result).read_text().splitlines():
        if not line.startswith("job "):
            continue
        f = line.split()
        key, profile, loop = f[1], f[2], f[3] == "loop=1"
        group = profile + (" loop" if loop else "")
        if not any(x.startswith("words=") for x in f):
            stats[group]["d3dx refused"] += 1
            continue
        words = f[f.index(next(x for x in f if x.startswith("words="))) + 1:]
        theirs = b"".join(int(w, 16).to_bytes(4, "little") for w in words)
        path = ours / (key + ".bin")
        if not path.exists():
            stats[group]["ours missing"] += 1
            continue
        try:
            diff = d3dbc_compare.compare_named(theirs, path.read_bytes(), runs=a.runs)
        except d3dbc_compare.Unsupported as e:
            stats[group]["not interpretable"] += 1
            reasons[str(e)] += 1
            continue
        except Exception as e:  # interpreter defect: count loudly, never as agreement
            stats[group]["interpreter error"] += 1
            reasons["error: %s" % type(e).__name__] += 1
            continue
        if diff:
            stats[group]["differ"] += 1
            diffs.append((key, profile, loop, diff))
        else:
            stats[group]["agree"] += 1
    for group in sorted(stats):
        print("%-14s %s" % (group, ", ".join("%s %d" % kv for kv in sorted(stats[group].items()))))
    if reasons:
        print("not interpretable: " + ", ".join("%s: %d" % kv for kv in reasons.most_common()))
    for key, profile, loop, diff in diffs[:a.show]:
        print("DIFFER %s %s%s: %s" % (key, profile, " loop" if loop else "", diff))
    if a.list:
        Path(a.list).write_text("".join("%s\t%s\t%d\t%s\n" % (k, p, l, d) for k, p, l, d in diffs))
    total = collections.Counter()
    for c in stats.values():
        total.update(c)
    print("d3dx-jobs-compare: " + ", ".join("%s %d" % kv for kv in sorted(total.items())))
    return 1 if diffs or total["interpreter error"] else 0


if __name__ == "__main__":
    sys.exit(main())
