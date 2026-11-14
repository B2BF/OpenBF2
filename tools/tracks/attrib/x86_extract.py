#!/usr/bin/env python3
"""Extract a local PDB-backed PE i386 image using the retail feature vocabulary.

Run ImportPdb.java / ExportPdb.java first; see README.md. All generated artifacts
stay under out/. No original image or Ghidra project is modified by this script.
"""
import argparse
import bisect
import collections
import csv
import json
from pathlib import Path
import re
import struct
import subprocess

from extract import REPO, PE, api_name, asm, digest, norm_name, retail


def read_tsv(path):
    with path.open() as stream:
        return list(csv.DictReader(stream, delimiter="\t"))


def source_path(path):
    """Keep the PDB's repository-relative path, never a basename guess."""
    path = path.replace("\\", "/")
    m = re.search(r"(?:^|/)(Code/.+)$", path, re.I)
    if not m:
        return ""
    # PDB records Windows path casing; return the checkout's actual spelling.
    target = m[1].lower()
    if target not in source_path.paths:
        return ""
    return source_path.paths[target]


source_path.paths = {}


def identities(name):
    name = norm_name(name).replace("`anonymous namespace'", "{anon}")
    name = name.replace("_anonymous_namespace_", "{anon}")
    return name, "::".join(name.split("::")[-2:])


def documented_tus(file):
    """Read explicit current-file split notes, never infer from similar names.

    These notes explain why current PDB placement may differ from retail __FILE__.
    They do not provide a function-identity anchor.
    """
    head = (REPO / file).read_text(errors="replace")[:1200]
    result = []
    for name in re.findall(r"(?:split from|attested)\s+[`\"']?([\w./\\-]+\.cpp)", head):
        name = name.replace("\\", "/")
        path = (Path(file).parent / name) if "/" not in name else Path("Code/BF2") / name
        if (REPO / path).is_file():
            result.append(str(path))
    return sorted(set(result))


def link_thunks(listing, exported):
    """Resolve only Ghidra-identified, single direct-jump incremental-link thunks."""
    addresses, mnemonics, operands = asm.load(str(listing))
    instructions = dict(zip(addresses, zip(mnemonics, operands)))
    thunks = {}
    for row in exported:
        a = int(row["entry"], 16)
        m, o = instructions.get(a, ("", ""))
        if row["thunk"] == "1" and m == "jmp" and re.fullmatch(r"0x[0-9a-f]+", o):
            thunks[a] = int(o, 16)

    def resolve(a):
        visited = set()
        while a in thunks and a not in visited:
            visited.add(a)
            a = thunks[a]
        return a

    return thunks, resolve


def import_thunks(result):
    """Expose an API reached through an exact one-jump IAT thunk, not a helper."""
    funcs = result["functions"]
    leaves = {f["address"]: f["imports"][0] for f in funcs.values()
              if len(f["insns"]) == 1 and f["insns"][0][1] == "jmp" and
              re.fullmatch(r"dword ptr \[0x[0-9a-f]+\]", f["insns"][0][2]) and len(f["imports"]) == 1}
    links = result["link_thunks"]
    for f in funcs.values():
        resolved = []
        for site, m, o in f["insns"]:
            if m not in ("call", "jmp") or not re.fullmatch(r"0x[0-9a-f]+", o):
                continue
            target = links.get(hex(int(o, 16)), int(o, 16))
            if target in leaves:
                resolved.append(dict(site=site, thunk=target, imported=leaves[target]))
        f["resolved_import_calls"] = resolved
        f["apis"] = sorted(set(f["apis"]) | {api_name(r["imported"]) for r in resolved if api_name(r["imported"])})


def associate(functions, exported, symbols, lines, sources):
    """Require PDB file/line evidence and a unique source name, keeping ICF aliases.

    A line in an inlined callee is not proof that it owns the surrounding body.
    Name and file must agree, with line containment used to separate overloads.
    """
    source_path.paths = {str(p.relative_to(REPO)).lower(): str(p.relative_to(REPO))
                         for p in (REPO / "Code").rglob("*") if p.is_file()}
    byname = collections.defaultdict(set)
    for i, d in enumerate(sources):
        for name in set(identities(d["name"])):
            byname[name].add(i)
    byaddress = collections.defaultdict(list)
    for s in symbols:
        if s["source"] == "IMPORTED":
            byaddress[int(s["address"], 16)].append(s["name"])
    line_addresses = [int(r["address"], 16) for r in lines]
    origins = {}
    for row in exported:
        a = int(row["entry"], 16)
        f = functions.get(hex(a))
        if f is None:
            continue
        qualified = "::".join(v for v in (row["namespace"], row["name"]) if v)
        f["name"] = qualified
        f["prototype"] = row["prototype"]
        f["symbol_source"] = row["symbol_source"]
        aliases = sorted(set(byaddress[a] + [qualified]))
        f["aliases"] = aliases
        lo = bisect.bisect_left(line_addresses, a)
        hi = bisect.bisect_left(line_addresses, f["analysis_end"])
        maps = [dict(file=source_path(r["file"]), line=int(r["line"]),
                     address=int(r["address"], 16), length=int(r["length"]))
                for r in lines[lo:hi]]
        f["source_maps"] = maps
        paths = {r["file"] for r in maps if r["file"]}
        candidates = set()
        for alias in aliases:
            # Decorated public names are retained as evidence, not parsed here.
            if alias.startswith("?"):
                continue
            full, short = identities(alias)
            named = byname[full] or byname[short]
            candidates.update(i for i in named if sources[i]["file"] in paths)
        if len(candidates) > 1:
            contained = {i for i in candidates if any(r["file"] == sources[i]["file"] and
                         sources[i]["line"] <= r["line"] <= sources[i]["line"] +
                         sources[i]["body"].count("\n") for r in maps)}
            if contained:
                candidates = contained
        f["source_candidates"] = sorted(candidates)
        f["source"] = next(iter(candidates)) if len(candidates) == 1 else None
        f["source_file"] = sources[f["source"]]["file"] if f["source"] is not None else ""
        file = f["source_file"]
        if file and file not in origins:
            origins[file] = documented_tus(file)
        f["documented_original_tus"] = origins.get(file, [])
        # A folded address with distinct repository definitions must be reviewed.
        f["source_ambiguous"] = len(candidates) > 1


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--image", type=Path, required=True)
    ap.add_argument("--pdb", type=Path, required=True)
    ap.add_argument("--export", type=Path, default=REPO / "out/attrib/our-x86")
    ap.add_argument("--features", type=Path, default=REPO / "out/attrib/features.json")
    ap.add_argument("--out", type=Path, default=REPO / "out/attrib/x86-features.json")
    ap.add_argument("--reuse-instructions", action="store_true",
                    help="reuse pinned binary features, refreshing source associations only")
    a = ap.parse_args()
    pe = PE(a.image)
    if struct.unpack_from("<H", pe.data, struct.unpack_from("<I", pe.data, 0x3c)[0] + 4)[0] != 0x14c:
        ap.error("PDB-backed image must be PE i386")
    meta = read_tsv(a.export / "metadata.tsv")[0]
    if meta["sha256"].lower() != digest(a.image) or meta["architecture"] != "x86:LE:32:default":
        ap.error("Ghidra export does not belong to the supplied i386 image")
    a.out.parent.mkdir(parents=True, exist_ok=True)
    listing = a.out.parent / "our-x86.s"
    data = json.loads(a.features.read_text())
    exported = read_tsv(a.export / "functions.tsv")
    if a.reuse_instructions:
        result = json.loads(a.out.read_text())
        checks = [(a.image, "image_sha256"), (a.pdb, "pdb_sha256"), (listing, "listing_sha256")]
        checks.extend((a.export / file, file + "_sha256") for file in
                      ("functions.tsv", "lines.tsv", "symbols.tsv", "ranges.tsv", "metadata.tsv"))
        for path, key in checks:
            if digest(path) != result["inputs"][key]:
                ap.error("pinned binary/export changed; cannot reuse instructions: " + str(path))
    else:
        with listing.open("w") as stream:
            subprocess.run(["objdump", "-d", "--no-show-raw-insn", "--x86-asm-syntax=intel", str(a.image)],
                           stdout=stream, check=True)
        thunks, resolve = link_thunks(listing, exported)
        print("x86: %d exported entries, %d direct-jump link thunks" % (len(exported), len(thunks)), flush=True)
        result = retail(pe, listing, a.export / "functions.tsv", a.image, call_resolver=resolve)
        print("x86 instruction features extracted", flush=True)
        result["link_thunks"] = {hex(k): resolve(k) for k in sorted(thunks)}
    import_thunks(result)
    lines = sorted(read_tsv(a.export / "lines.tsv"), key=lambda r: int(r["address"], 16))
    if not lines:
        ap.error("PDB source line maps are required")
    associate(result["functions"], exported, read_tsv(a.export / "symbols.tsv"), lines, data["sources"])
    fs = result["functions"].values()
    result["summary"] = dict(exported_functions=len(exported), source_maps=len(lines),
                             functions=len(result["functions"]), link_thunks=len(result["link_thunks"]),
                             source_associations=sum(f.get("source") is not None for f in fs),
                             ambiguous_associations=sum(f.get("source_ambiguous", False) for f in fs),
                             resolved_import_calls=sum(len(f["resolved_import_calls"]) for f in fs))
    result["inputs"] = dict(image_sha256=digest(a.image), pdb_sha256=digest(a.pdb),
                            source_sha256=data["inputs"]["source_sha256"],
                            features_sha256=digest(a.features), listing_sha256=digest(listing),
                            **{file + "_sha256": digest(a.export / file) for file in
                               ("functions.tsv", "lines.tsv", "symbols.tsv", "ranges.tsv", "metadata.tsv")})
    provenance = a.image.parent / "SOURCE.txt"
    if provenance.exists():
        result["inputs"].update(build_description=provenance.read_text().strip(),
                                build_description_sha256=digest(provenance))
    a.out.write_text(json.dumps(result, sort_keys=True, separators=(",", ":"), allow_nan=False) + "\n")
    fs = result["functions"].values()
    print("x86: %d functions, %d unique source associations, %d ambiguous, %d line entries" %
          (len(result["functions"]), sum(f.get("source") is not None for f in fs),
           sum(f.get("source_ambiguous", False) for f in fs), len(lines)))


if __name__ == "__main__":
    main()
