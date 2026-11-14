#!/usr/bin/env python3
"""Extract both renderer images' anchors into an agent-owned JSON cache.

Usage: REND_EVIDENCE_ROOT=/path/to/rend-track python3 tools/tracks/attrib/extract.py
  --archive out/attrib/libbf2_renddx9_static.a --out out/attrib/features.json
The archive must be a private snapshot, never the live build output. No game files,
Ghidra databases, source files or ledgers are modified. Python standard library only.
"""
import argparse
import bisect
import collections
import csv
import hashlib
import json
import math
import os
from pathlib import Path
import re
import struct
import subprocess
import sys

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[2]
sys.path[:0] = [str(HERE.parent / "rdebug"), str(HERE.parent)]
import asm  # noqa: E402
import features  # noqa: E402
from pe import PE  # noqa: E402
import marker_check as mc  # noqa: E402
import native  # noqa: E402


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def api_name(name):
    """Normalize only attested names, not presumed API equivalents."""
    name = name.split("!", 1)[-1]
    # VC7.1 import names are C++ decorated for the STL. Do not conflate those.
    if name.startswith("?") or "::" in name or name.startswith("operator "):
        return None
    name = re.sub(r"@\d+$", "", name).lstrip("_")
    if re.fullmatch(r"[A-Za-z][A-Za-z_0-9]*", name):
        return name
    return None


def norm_name(name):
    """Parameter-free, template-free scope, for source-to-symbol association only."""
    name = name.replace("(anonymous namespace)", "{anon}")
    name = re.sub(r"\[abi:[^]]+\]", "", name)
    out, depth = [], 0
    for c in name:
        if c == "<":
            depth += 1
        elif c == ">" and depth:
            depth -= 1
        elif not depth:
            if c == "(":
                break
            out.append(c)
    return "".join(out).strip()


def source_paths():
    roots = ["RendDX9", "Geom", "GeomMisc", "RendMisc", "Scene", "Math", "Core", "IO"]
    return mc.files_under(["Code/BF2/" + p for p in roots] + ["Code/RendDX9Abi"])


def source_digest(paths):
    source_hash = hashlib.sha256()
    for p in paths:
        source_hash.update(p.encode() + b"\0" + (REPO / p).read_bytes())
    return source_hash.hexdigest()


def source_definitions(objects, cache):
    paths = source_paths()
    source_hash = source_digest(paths)
    saved = json.loads(cache.read_text()) if cache.exists() else {}
    if saved.get("source_sha256") == source_hash:
        result = saved["sources"]
    else:
        defs, _, _, stopping, _, _ = mc.analyse(paths)
        result = []
        for d in defs:
            qualified = "::".join(d["namespaces"] + d["comps"])
            result.append(dict(file=d["file"], line=d["line"], name=qualified,
                               short="::".join(d["comps"]), params=d["nparams"],
                               comment=d["comment"], body=d["body"],
                               stopping=id(d) in stopping or "[FRONTIER]" in d["body"]))
        cache.write_text(json.dumps(dict(source_sha256=source_hash, sources=result),
                                    sort_keys=True) + "\n")
    # All candidates are kept if overloads cannot be separated. A matching name is
    # evidence of a native symbol's source, not evidence of a retail/native pairing.
    byshort = collections.defaultdict(list)
    for i, d in enumerate(result):
        parts = norm_name(d["name"]).split("::")
        byshort["::".join(parts[-2:])].append(i)
        if len(parts) > 1:
            byshort[parts[-1]].append(i)
    files = collections.defaultdict(list)
    for p in paths:
        if p.endswith((".cpp", ".mm")):
            files[Path(p).name + ".o"].append(p)
    for f in objects["functions"].values():
        name = norm_name(f["name"])
        parts = name.split("::")
        candidates = sorted(set(byshort.get("::".join(parts[-2:]), [])))
        exact = [i for i in candidates if norm_name(result[i]["name"]) == name]
        if exact:
            candidates = exact
        if len(candidates) > 1:
            local = [i for i in candidates if result[i]["file"] in files[f["object"].split("#")[0]]]
            if local:
                candidates = local
        # Source overloads with different arity can be disambiguated without
        # guessing argument types from instructions.
        if len(candidates) > 1:
            p = f["name"].find("(", len(name))
            if p >= 0:
                tail = f["name"][p + 1:]
                end = mc.bl.find_matching("(" + tail, 0, "(", ")")
                arity = mc.bl.count_params(tail[:end - 1])
                narrower = [i for i in candidates if result[i]["params"] == arity]
                if narrower:
                    candidates = narrower
        f["source_candidates"] = candidates
        f["source"] = candidates[0] if len(candidates) == 1 else None
        f["source_file"] = result[candidates[0]]["file"] if len(candidates) == 1 else (
            files[f["object"].split("#")[0]][0] if len(files[f["object"].split("#")[0]]) == 1 else "")
    associate_console_macros(objects, result)
    return result, source_hash, len(paths)


def associate_console_macros(objects, sources):
    """Resolve the actual ConsoleClass<__LINE__> emitted by ConsoleObjectMacro.h.

    This is a source association, not a retail pairing. The compiler symbol, TU,
    invocation line and macro name must all agree. No synthetic PE comment is added.
    """
    contents = {}
    function_sources = collections.defaultdict(list)
    for f in objects["functions"].values():
        function_sources[f["mangled"]].append(f["source"])
    for f in objects["functions"].values():
        if f["source"] is not None or not f["source_file"]:
            continue
        m = re.search(r"::ConsoleClass(\d+)::(?:~?ConsoleClass\d+|[A-Za-z_]\w*)\(", f["name"])
        if not m:
            continue
        file, line = f["source_file"], int(m[1])
        if file not in contents:
            raw = (REPO / file).read_text()
            contents[file] = {site["end_line"]: site for site in console_macro_sites(raw)}
        site = contents[file].get(line)
        if not site:
            continue
        body = site["body"]
        stopping = any("frontier" in n.lower() or n == "abort" for n in f["call_names"])
        for sym in f["calls"]:
            if any(i is not None and sources[i]["stopping"] for i in function_sources.get(sym, [])):
                stopping = True
        f["source"] = len(sources)
        f["source_candidates"] = [len(sources)]
        sources.append(dict(file=file, line=site["line"], name=f["name"], short=norm_name(f["name"]), params=-1,
                            comment="", body=body, stopping=stopping,
                            generated_macro=site["macro"], generated_class_line=line, generated_symbol=f["mangled"]))


def console_macro_sites(raw):
    """Actual macro invocations, including Clang's closing-line __LINE__ expansion."""
    code, _ = mc.bl.blank_code(raw)
    starts = [0] + [i + 1 for i, c in enumerate(raw) if c == "\n"]
    result = []
    for m in re.finditer(r"(?m)^[ \t]*(BF2_[A-Z_0-9]+)\s*\(", code):
        paren = code.find("(", m.start(1))
        end = mc.bl.find_matching(code, paren, "(", ")")
        if end >= len(code) or code[end] != ")":
            continue
        result.append(dict(macro=m[1], line=bisect.bisect_right(starts, m.start(1)),
                           end_line=bisect.bisect_right(starts, end), body=raw[m.start(1):end + 1]))
    return result


def reachable_instructions(pe, insns, entry, end):
    """Follow instruction edges; a vtable-only getter ends at its own return.

    Keep the bounded listing on an unresolved indirect jump, rather than deleting
    unvisited switch arms and manufacturing a small function signature.
    """
    byaddr = {a: i for i, (a, _, _) in enumerate(insns)}
    todo, visited, complete = [entry], set(), True
    while todo:
        address = todo.pop()
        if address in visited:
            continue
        i = byaddr.get(address)
        if i is None:
            complete = False
            continue
        visited.add(address)
        _, m, o = insns[i]
        if m.startswith(("ret", "iret")) or m in ("int3", "ud2", "hlt"):
            continue
        if m.startswith("j") or m.startswith("loop"):
            if re.fullmatch(r"0x[0-9a-f]+", o):
                target = int(o, 16)
                if entry <= target < end:
                    todo.append(target)
            elif m == "jmp":
                targets = []
                absolute = re.fullmatch(r"dword ptr \[(0x[0-9a-f]+)\]", o)
                if absolute and int(absolute[1], 16) in pe.imports():
                    continue  # known import tail call, not an unresolved local switch
                # Common VC7.1 switch: jmp dword ptr [4*reg + table].
                if "*" in o and "[" in o:
                    for h in features.HEX.findall(o):
                        table = int(h, 16)
                        if not (pe.in_sec(table, ".rdata") or pe.in_sec(table, ".text")):
                            continue
                        for j in range(4096):
                            target = pe.u32(table + 4 * j)
                            if target is None or not entry <= target < end:
                                break
                            targets.append(target)
                if targets:
                    todo.extend(targets)
                else:
                    complete = False
            if m == "jmp":
                continue
        if i + 1 < len(insns):
            todo.append(insns[i + 1][0])
    return ([insn for insn in insns if insn[0] in visited] if complete else insns), complete


def retail(pe, listing, ftsv, image_path=None, call_resolver=None):
    A, M, O = asm.load(str(listing))
    F, V, extra = features.extract(pe, (A, M, O), str(ftsv))
    entries = sorted(F)
    imports = pe.imports()
    funcs = {}
    instruction_owner = {}
    for i, e in enumerate(entries):
        f = F[e]
        end = entries[i + 1] if i + 1 < len(entries) else e + f.size
        lo, hi = bisect.bisect_left(A, e), bisect.bisect_left(A, end)
        insns = list(zip(A[lo:hi], M[lo:hi], O[lo:hi]))
        decode_basis = "linear listing"
        entry_decode = None
        if (not insns or insns[0][0] != e) and image_path is not None:
            # A linear disassembly can lose synchronization after embedded data.
            # Restart exactly at the Ghidra entry instead of interpreting the
            # following instruction as the function's first instruction.
            decoded = subprocess.check_output(["objdump", "-d", "--no-show-raw-insn",
                "--x86-asm-syntax=intel", "--start-address=" + hex(e),
                "--stop-address=" + hex(end), str(image_path)], text=True)
            insns = []
            for line in decoded.splitlines():
                m = asm.LINE.match(line)
                if m:
                    insns.append((int(m[1], 16), m[2], asm.SYM.sub("", m[3] or "")))
            decode_basis = "entry-specific objdump"
            entry_decode = insns[:]
        # Ghidra's size is the sum of body ranges, not necessarily end-entry:
        # shared return tails make it unsafe as an instruction cutoff.
        while insns and insns[-1][1] in ("int3", "nop"):
            insns.pop()
        insns, cfg_complete = reachable_instructions(pe, insns, e, end)
        for a, _, _ in insns:
            instruction_owner[a] = e
        # Re-extract within the explicit function bounds. features.extract also
        # discovers vtable-only entries, but its nearest-entry walk includes gaps.
        strings, calls, imps, floats, consts, pointers = set(), [], [], set(), set(), set()
        registers = {}
        for a, m, o in insns:
            if m in ("call", "jmp"):
                if re.fullmatch(r"0x[0-9a-f]+", o):
                    t = int(o, 16)
                    if call_resolver is not None:
                        t = call_resolver(t)
                    if t in F and (m == "call" or t != e):
                        calls.append(t)
                mm = re.fullmatch(r"dword ptr \[(0x[0-9a-f]+)\]", o)
                if mm and int(mm[1], 16) in imports:
                    imps.append(imports[int(mm[1], 16)])
                if o in registers:
                    imps.append(registers[o])
            if m == "mov":
                mm = re.fullmatch(r"(e[a-z]{2}), dword ptr \[(0x[0-9a-f]+)\]", o)
                dest = o.split(",", 1)[0]
                registers.pop(dest, None)
                if mm and int(mm[2], 16) in imports:
                    registers[mm[1]] = imports[int(mm[2], 16)]
            for h in features.HEX.findall(o):
                v = int(h, 16)
                if pe.in_sec(v, ".rdata") or pe.in_sec(v, ".data"):
                    if "[" not in o or not re.search(r"\[[^]]*" + re.escape(h), o):
                        s = pe.cstr(v, 4096)
                        if features.printable(s):
                            strings.add(s)
                if v in F and m not in ("call", "jmp"):
                    pointers.add(v)
            for mm in features.MEMABS.finditer(o):
                v = int(mm[2], 16)
                if pe.in_sec(v, ".rdata") and (m.startswith("f") or mm[1] == "qword"):
                    width = 8 if mm[1] == "qword" else 4 if mm[1] == "dword" else 0
                    off = pe.off(v)
                    if width and off is not None:
                        val = struct.unpack_from("<d" if width == 8 else "<f", pe.data, off)[0]
                        if math.isfinite(val):
                            floats.add(val)
            if "[" not in o and m in ("mov", "push", "cmp", "and", "or", "xor", "imul"):
                for h in features.HEX.findall(o):
                    v = int(h, 16)
                    if 0x1000 <= v < 0x10000000:
                        consts.add(v)
        funcs[hex(e)] = dict(address=e, size=f.size, name=f.name, strings=sorted(strings),
                             analysis_end=end, listing_entry_present=bool(insns and insns[0][0] == e),
                             decode_basis=decode_basis,
                             entry_decode_insns=entry_decode, cfg_complete=cfg_complete,
                             imports=imps, apis=[api_name(n) for n in imps if api_name(n)],
                             calls=calls, pointers=sorted(pointers), callers=[], floats=sorted(floats),
                             consts=sorted(consts), n=len(insns), insns=insns,
                             switches=sum(m == "jmp" and "[" in o for _, m, o in insns))
    for f in funcs.values():
        for c in set(f["calls"]):
            funcs[hex(c)]["callers"].append(f["address"])
    # Record data references to entries too: static initializers and EH cleanup
    # functions often have no direct callers. This is evidence, not reachability.
    for f in funcs.values():
        f["pointer_users"] = []
    for f in funcs.values():
        for c in f["pointers"]:
            funcs[hex(c)]["pointer_users"].append(f["address"])
    return dict(functions=funcs, vtables={hex(v): s for v, s in sorted(V.items())},
                vtable_only=sorted(extra), architecture="PE i386", base=hex(pe.base))


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--evidence-root", type=Path,
                    default=Path(os.environ.get("REND_EVIDENCE_ROOT", REPO)))
    ap.add_argument("--archive", type=Path, default=REPO / "out/attrib/libbf2_renddx9_static.a")
    ap.add_argument("--listing", type=Path, default=REPO / "out/attrib/native.s")
    ap.add_argument("--out", type=Path, default=REPO / "out/attrib/features.json")
    ap.add_argument("--image", type=Path, default=Path(os.environ.get("REND_GAME_DIR", str(Path.home() /
                    "Applications/Battlefield 2.app/Contents/SharedSupport/prefix/drive_c/Battlefield 2"))) /
                    "RendDX9.dll")
    a = ap.parse_args()
    a.out.parent.mkdir(parents=True, exist_ok=True)
    if not a.listing.exists():
        a.listing.parent.mkdir(parents=True, exist_ok=True)
        with a.listing.open("w") as output:
            subprocess.run(["objdump", "-d", "-r", "--demangle", "--no-show-raw-insn",
                            "--x86-asm-syntax=intel", str(a.archive)], stdout=output, check=True)
    # Always bind a listing to the exact archive. Refuse stale listings on reruns.
    sidecar = a.listing.with_suffix(".archive-sha256")
    archive_hash = digest(a.archive)
    if sidecar.exists() and sidecar.read_text().strip() != archive_hash:
        sys.exit("archive changed: remove the private native listing and regenerate it")
    sidecar.write_text(archive_hash + "\n")
    pe = PE(a.image)
    if struct.unpack_from("<H", pe.data, struct.unpack_from("<I", pe.data, 0x3C)[0] + 4)[0] != 0x14C:
        sys.exit("retail image is not i386")
    if pe.base != 0x10000000:
        sys.exit("retail image does not have the RendDX9 image base 0x10000000")
    root = a.evidence_root / "out"
    result = dict(schema=1, retail=retail(pe, root / "rend-evidence/RendDX9.s",
                                        root / "rend-ghidra/RendDX9/functions.tsv", a.image))
    print("retail: %d functions (%d vtable-only)" % (len(result["retail"]["functions"]),
                                                    len(result["retail"]["vtable_only"])), flush=True)
    result["native"] = native.extract(a.archive, a.listing, api_name)
    print("native relocations extracted", flush=True)
    result["sources"], source_hash, nfiles = source_definitions(result["native"],
                                                              a.out.parent / "source-cache.json")
    for f in result["native"]["functions"].values():
        f["floats"] = [v for v in f["floats"] if math.isfinite(v)]
    result["inputs"] = dict(retail_sha256=digest(a.image), archive_sha256=archive_hash,
                             listing_sha256=digest(a.listing), source_sha256=source_hash,
                             source_files=nfiles, ledger_sha256=digest(REPO / "docs/tracks/rend/ledger.csv"),
                             source_head=subprocess.check_output(["git", "rev-parse", "HEAD"],
                                                                 cwd=REPO, text=True).strip())
    result["inputs"].update(retail_listing_sha256=digest(root / "rend-evidence/RendDX9.s"),
                             functions_tsv_sha256=digest(root / "rend-ghidra/RendDX9/functions.tsv"),
                             rdebug_map_sha256=digest(REPO / "docs/tracks/rend/rdebug-map.csv"))
    a.out.write_text(json.dumps(result, sort_keys=True, separators=(",", ":"), allow_nan=False) + "\n")
    fs = result["native"]["functions"]
    print("native: %d objects, %d functions, %d source associations, %d pointer tables" % (
        result["native"]["objects"], len(fs), sum(f["source"] is not None for f in fs.values()),
        len(result["native"]["tables"])))
    print("features: %s" % a.out)


if __name__ == "__main__":
    main()
