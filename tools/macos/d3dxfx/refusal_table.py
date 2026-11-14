#!/usr/bin/env python3
"""[CHOSEN, not recovered] Developer oracle, not used by the client.

Optional offline generator for the D3DX refusal metadata; never invoked by the client build.

From a Wine/D3DX prebuild (c-fx-permutations prebuild.py: state.json with the D3DX outcome of
every enumerated request, and the cache's requests/<key>.request snapshots) it writes
Code/BF2/Platform/Gfx/Effect/data/d3dx-refusals.txt: one line per request, the first 16 hex
digits of the toolchain-independent refusal key (bf2fx::native::refusalKey) and A (D3DX
accepted) or R (refused). Metadata only: hashes and outcomes, no game content.

Not recorded (the client treats them as unknown and applies Rule R): `.mfx` metadata (reflection
only, ra_metadata jobs) and, by default, E_OUTOFMEMORY refusals (c-fx-permutations gap 3: first
taken for a Wine heap artifact). `--include-oom` records them as R: native Windows D3DX9_25 (the
same retail DLL, large-address-aware host, one process per job) returns the same
E_OUTOFMEMORY (0x8007000E) from CompileEffect for all 336 such requests (2026-11-12), so it is
the D3DX outcome the engine sees, not a Wine artifact.
`--merge-with TABLE` extends an existing table (ATI=1 keys added to the ATI=0 table): its header
lines are kept, `--note` adds lines, and a key with a different outcome stops the run.

Wine validator stub (2026-11-13): the prebuild runs the retail d3dx9_25.dll under Wine with Wine's
builtin d3d9.dll, whose IDirect3DShaderValidator9 (Direct3DShaderValidatorCreate9) is a stub that
accepts everything, so CompileEffect never reports a validator error (X5111, X5500, ...). Native
Windows refuses those requests. A table from Wine outcomes alone over-reports A, so the run stops
unless `--native-result` gives the native Windows CompileEffect outcome of the accepted keys
(Code/Tests/effect_check --compile-only, effect_compile_result.txt): its refusals become R, and every
A key must be covered (`--allow-unchecked` lists uncovered keys instead of failing)."""
import argparse, json, subprocess, sys, tempfile
from pathlib import Path

REPO = Path(__file__).resolve().parents[3]
ap = argparse.ArgumentParser(description=__doc__)
ap.add_argument("--state", type=Path, action="append", required=True, help="prebuild state.json (repeatable)")
ap.add_argument("--requests", type=Path, action="append", required=True, help="cache requests directory (repeatable)")
ap.add_argument("--tool", type=Path, default=REPO / "out/build/nativefx/tool/nativefx")
ap.add_argument("--output", type=Path, default=REPO / "Code/BF2/Platform/Gfx/Effect/data/d3dx-refusals.txt")
ap.add_argument("--merge-with", type=Path,
                help="existing table to extend (e.g. the ATI=0 table with ATI=1 keys); keeps its header lines, "
                     "fails on a key with a different outcome")
ap.add_argument("--include-oom", action="store_true",
                help="record E_OUTOFMEMORY refusals as R (confirmed as the native D3DX9_25 outcome)")
ap.add_argument("--note", action="append", default=[], help="extra header line for the merged table (repeatable)")
ap.add_argument("--native-result", type=Path, action="append", default=[],
                help="effect_compile_result.txt of a native Windows CompileEffect run over the A keys (repeatable)")
ap.add_argument("--native-fresh", type=Path, action="append", default=[],
                help="effect_compile_result.txt of a --fresh-process run (one process per effect): E_OUTOFMEMORY "
                     "there is the native outcome (R); in --native-result runs it is skipped as unconfirmed")
ap.add_argument("--allow-unchecked", action="store_true",
                help="keep A keys the native run did not cover (listed) instead of stopping")
a = ap.parse_args()

outcome = {}
for s in a.state:
    for job, r in json.load(open(s))["results"].items():
        cls = r.get("class") or ""
        if ("E_OUTOFMEMORY" in cls and not a.include_oom) or "no techniques" in cls: continue
        # .mfx metadata (reflection only, ra_metadata jobs) is not a D3DX acceptance outcome.
        if job.startswith("ra_metadata-") or "reflection-only" in (r.get("result") or ""): continue
        outcome[r["key"]] = "A" if r["ok"] else "R"
paths = []
for d in a.requests:
    for k in outcome:
        p = d / f"{k}.request"
        if p.exists(): paths.append(p)
with tempfile.NamedTemporaryFile("w", dir=REPO / "out", delete=False) as f:
    f.write("\n".join(map(str, paths)) + "\n"); listing = f.name
out = subprocess.run([str(a.tool), "refusal-keys", listing], capture_output=True, text=True, check=True).stdout
Path(listing).unlink()
table = {}
conflicts = 0
for line in out.splitlines():
    key, rkey = line.split()
    prefix = rkey[:16]
    state = outcome[key]
    if table.get(prefix, state) != state: conflicts += 1
    table[prefix] = state
if conflicts: sys.exit(f"{conflicts} refusal keys with both outcomes")
header = ("# [CHOSEN, not recovered] D3DX refusal table: refusal-key prefix, A accepted / R refused.\n"
          "# Generated by tools/macos/d3dxfx/refusal_table.py from a Wine/D3DX prebuild; metadata only.\n")
if a.merge_with:
    existing = a.merge_with.read_text().splitlines()
    header = "".join(l + "\n" for l in existing if l.startswith("#"))
    old = dict(l.split() for l in existing if l.strip() and not l.startswith("#"))
    clash = [k for k in table if k in old and old[k] != table[k]]
    if clash: sys.exit(f"{len(clash)} keys conflict with {a.merge_with}: {clash[:5]}")
    print(f"merge: {sum(k in old for k in table)} keys already present (identical), "
          f"{sum(k not in old for k in table)} new")
    old.update(table); table = old
# Native Windows outcome of the keys the Wine prebuild accepted (see the docstring).
native = {}
oom_skipped = 0
for path, fresh in [(p, False) for p in a.native_result] + [(p, True) for p in a.native_fresh]:
    for line in path.read_text().splitlines():
        f = line.split()
        if len(f) < 6 or f[0] != "compile" or f[2] == "-": continue
        # .mfx metadata (no techniques) is not an acceptance outcome; the table leaves it out.
        if "There were no techniques" in line: continue
        hr = int(f[5].split("=")[1], 16)
        # E_OUTOFMEMORY in a long single-process run may be address-space exhaustion of the run itself:
        # only a one-process-per-effect run (--native-fresh) records it.
        if hr == 0x8007000E and not fresh:
            oom_skipped += 1
            continue
        native[f[2][:16]] = "A" if hr < 0x80000000 else "R"
if oom_skipped: print(f"native: {oom_skipped} E_OUTOFMEMORY results skipped (rerun them with --fresh-process)")
if not native:
    sys.exit("refusing to write a table from Wine outcomes alone: Wine's d3d9 shader validator is a stub, so "
             "validator refusals read as A; pass --native-result (Code/Tests/effect_check --compile-only)")
flipped = [k for k, v in table.items() if v == "A" and native.get(k) == "R"]
unchecked = [k for k, v in table.items() if v == "A" and k not in native]
for k in flipped: table[k] = "R"
# Keys only the native run knows (attempts the Wine prebuild never formed): their native outcome.
added = {k: v for k, v in native.items() if k not in table}
table.update(added)
print(f"native: {len(native)} keys checked, {len(flipped)} A -> R, {len(added)} new keys "
      f"({sum(v == 'R' for v in added.values())} refused), {len(unchecked)} A keys not checked")
if unchecked and not a.allow_unchecked: sys.exit(f"{len(unchecked)} A keys without a native outcome: {unchecked[:5]}")
for k in unchecked: print("unchecked", k)
if added:
    a.note.append(f"Native Windows CompileEffect outcomes for {len(added)} next-attempt requests the Wine prebuild never "
                  "formed (tools/macos/d3dxfx/next_attempts.py); refusal_table.py --native-result")
if flipped:
    a.note.append(f"Native Windows CompileEffect (retail d3dx9_25 with the real d3d9 shader validator): {len(flipped)} keys "
                  "the Wine prebuild accepted are validator refusals (Wine's d3d9 validator is a stub), recorded as R; "
                  "refusal_table.py --native-result")
header += "".join("# " + n + "\n" for n in a.note)
lines = [f"{k} {v}" for k, v in sorted(table.items())]
a.output.write_text(header + "\n".join(lines) + "\n")
print(f"{len(lines)} keys ({sum(v == 'R' for v in table.values())} refused) from {len(paths)} snapshots, "
      f"{len(outcome) - len(paths)} outcomes without a snapshot")
