#!/usr/bin/env python3
"""Conservative, offline SWF name alignment; Python stdlib only, use python3 -I.

[CHOSEN, not recovered] analysis policy. Inputs are untrusted text, never imported
or executed. Only identifiers, addresses, counts and evidence labels are written.
Name alignment is not behavioral equivalence. Unreviewed bodies remain unknown.
The local AI ledger's brace-aware parser supplies syntax, not recovery evidence.
"""

import argparse
import bisect
from collections import Counter, defaultdict
import csv
from dataclasses import dataclass, field
import importlib.util
import hashlib
import json
import os
from pathlib import Path
import re
import statistics
import struct
import subprocess
import sys

REPO = Path(__file__).resolve().parents[2]
# Inputs: GAMESWF_HISTORY_DIR (gameswf svn-r586 and tu-testbed rel-2004-05-12 checkouts) and
# SWF_EXPORT_DIR (the Ghidra export of SwiffPlayer.dll), or --history/--export.
HISTORY = Path(os.environ["GAMESWF_HISTORY_DIR"]) if os.environ.get("GAMESWF_HISTORY_DIR") else None
EXPORT = Path(os.environ["SWF_EXPORT_DIR"]) if os.environ.get("SWF_EXPORT_DIR") else None
INVENTORY = REPO / "docs/tracks/swf/swiffplayer-gameswf-functions.csv"
REVIEWS = REPO / "docs/tracks/swf/s1-reviews.csv"
FIELDS = ["address", "size", "symbol", "file", "base_rev", "match", "confidence",
          "anchor", "boundary", "reason", "csv_size", "ninstr", "file_inferred", "change_reason"]


def parser_module():
    spec = importlib.util.spec_from_file_location(
        "swf_source_parser", REPO / "tools/ai-reconstruct/build_ledger.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


BL = parser_module()
SEQUENCE_SPEC = importlib.util.spec_from_file_location("swf_sequence", Path(__file__).with_name("swf_sequence.py"))
sequence = importlib.util.module_from_spec(SEQUENCE_SPEC)
SEQUENCE_SPEC.loader.exec_module(sequence)
TOKENS = re.compile(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'', re.S)
CALL = re.compile(r'(?<![\w])([A-Za-z_]\w*(?:::[~\w]+)*)\s*\(')


def unescape(text):
    """Decode C/export escapes without evaluating any input."""
    escapes = {"n": "\n", "r": "\r", "t": "\t", "\\": "\\", '"': '"', "'": "'", "0": "\0"}
    def replace(m):
        value = m.group(1)
        if value.startswith("x"):
            return chr(int(value[1:], 16))
        if value.isdigit():
            return chr(int(value, 8))
        return escapes.get(value, "\\" + value)
    return re.sub(r'\\(x[0-9a-fA-F]+|[0-7]{1,3}|.)', replace, text)


def literals(text):
    # Comments cannot supply anchors; join adjacent C literals before comparing.
    clean = TOKENS.sub(lambda m: " " if m[0].startswith(("//", "/*")) else m[0], text)
    return {"".join(unescape(s[1:-1]) for s in re.findall(r'"(?:\\.|[^"\\])*"', m[0]))
            for m in re.finditer(r'(?:"(?:\\.|[^"\\])*"\s*)+', clean)}


@dataclass
class Source:
    key: str
    name: str
    file: str
    line: int
    strings: set
    calls: list
    tokens: int
    body: str = field(repr=False)
    out_of_line: bool = False


def parse_sources(directory, prefix):
    functions = {}
    layouts = {}
    for path in sorted(Path(directory).glob("*")):
        if path.suffix not in (".cpp", ".h", ".c", ".inl"):
            continue
        if re.search(r'handler_|test_|tools|parser|processor', path.name):
            continue
        defs, _, _ = BL.parse_file(str(path))
        for d in defs:
            name = "::".join(d["namespaces"] + d["comps"])
            key = name + "(" + ",".join(d["types"]) + ")" + (" const" if d["const"] else "")
            body = d["body"]
            code, _ = BL.blank_code(body)
            calls = [m[1] for m in CALL.finditer(code) if m[1] not in BL.KEYWORDS]
            # Same-signature alternatives (preprocessor branches) stay ambiguous.
            if key in functions:
                key += "@" + path.name + ":" + str(d["line"])
            functions[key] = Source(key, name, prefix + path.name, d["line"], literals(body), calls,
                                    len(re.findall(r'\w+|[^\w\s]', code)), body,
                                    path.suffix in (".cpp", ".c") and "::".join(d["comps"]) == d["qual"])
        layouts.update(class_layouts(path.read_text(errors="replace"), prefix + path.name))
        functions.update(macro_functions(path.read_text(errors="replace"), prefix + path.name))
    return functions, layouts


def macro_functions(text, file):
    """Text-only function macro expansion; never invokes a preprocessor/compiler."""
    definitions = {}
    lines = text.splitlines(keepends=True)
    i = 0
    while i < len(lines):
        line = lines[i]
        m = re.match(r'\s*#\s*define\s+(\w+)\(([^)]*)\)\s*(.*)', line)
        if m:
            expansion = m[3].rstrip("\n")
            while expansion.rstrip().endswith("\\") and i + 1 < len(lines):
                i += 1
                expansion = expansion.rstrip()[:-1] + "\n" + lines[i].rstrip("\n")
            if "{" in expansion and re.search(r'\bvoid\b', expansion):
                definitions[m[1]] = ([x.strip() for x in m[2].split(",")], expansion)
        i += 1
    code, _ = BL.blank_code(text)
    result = {}
    for name, (params, expansion) in definitions.items():
        for m in re.finditer(r'\b' + re.escape(name) + r'\s*\(', code):
            end = BL.find_matching(code, m.end() - 1, "(", ")")
            args = BL.split_top(text[m.end():end], ",")
            if len(args) != len(params):
                continue
            values = dict(zip(params, args))
            expanded = re.sub(r'\b(?:' + "|".join(re.escape(x) for x in params) + r')\b',
                              lambda hit: values[hit[0]].strip(), expansion)
            expanded = re.sub(r'\s*##\s*', "", expanded)
            brace = expanded.find("{")
            kind, info = BL.parse_head(expanded[:brace])
            if kind != "function":
                continue
            qual, parameters, const = info
            namespace = "gameswf::" if "/gameswf/" in file else ""
            qual = namespace + qual
            key = qual + "(" + ",".join(BL.norm_types(parameters, True)) + ")"
            body = expanded[brace:]
            bodycode, _ = BL.blank_code(body)
            result[key] = Source(key, qual, file, text.count("\n", 0, m.start()) + 1,
                                 literals(body), [x[1] for x in CALL.finditer(bodycode) if x[1] not in BL.KEYWORDS],
                                 len(re.findall(r'\w+|[^\w\s]', bodycode)), body)
    return result


def class_layouts(text, file):
    """Declaration order and member identifiers only; no inferred ABI byte offsets.

    Inheritance, conditional declarations and MSVC destructor variants require
    manual checking. This deliberately does not pretend to be a C++ compiler.
    """
    code, _ = BL.blank_code(text)
    result = {}
    for m in re.finditer(r'\b(?:struct|class)\s+(\w+)\s*([^;{}]*)\{', code):
        end = BL.find_matching(code, m.end() - 1, "{", "}")
        body = code[m.end():end]
        virtuals, members = [], []
        depth, start = 0, 0
        for i, c in enumerate(body):
            if c == "{" and depth == 0:
                head = body[start:i]
                v = re.search(r'\bvirtual\b[^;{}]*?([~\w]+)\s*\(', head)
                if v:
                    virtuals.append(v[1])
            if c == "{":
                depth += 1
            elif c == "}":
                depth -= 1
                if depth == 0:
                    start = i + 1
            elif c == ";" and depth == 0:
                head = body[start:i]
                v = re.search(r'\bvirtual\b[^;{}]*?([~\w]+)\s*\(', head)
                if v:
                    virtuals.append(v[1])
                elif "(" not in head:
                    members.extend(re.findall(r'\bm_\w+\b', head))
                start = i + 1
        result[m[1]] = {"file": file, "virtuals": virtuals, "members": members,
                        "bases": re.findall(r'\b(?!public\b|private\b|protected\b|virtual\b)\w+\b', m[2]),
                        "complex": "virtual" in m[2] or "," in m[2]}
    return result


def read_csv(path, delimiter=","):
    with Path(path).open(newline="") as f:
        return list(csv.DictReader(f, delimiter=delimiter))


def write_csv(path, fields, rows):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", newline="") as f:
        w = csv.DictWriter(f, fields, lineterminator="\n", extrasaction="ignore")
        w.writeheader()
        w.writerows(rows)


def addresses(text):
    return [int(x, 16) for x in text.split(";") if x.startswith("0x")]


def load_export(directory):
    funcs = {}
    for r in read_csv(Path(directory) / "functions.tsv", "\t"):
        a = int(r["addr"], 16)
        funcs[a] = {**r, "addr": a, "size": int(r["size"]), "ninstr": int(r["ninstr"])}
        for k in ("callees", "callers", "strings", "datarefs", "vtwrites"):
            funcs[a][k] = addresses(r[k])
    strings = {int(r["addr"], 16): unescape(r["text"])
               for r in read_csv(Path(directory) / "strings.tsv", "\t")}
    vtables = [{"addr": int(r["addr"], 16), "slots": addresses(r["slots"]),
                "xrefs": addresses(r["xrefs"])}
               for r in read_csv(Path(directory) / "vtables.tsv", "\t")]
    return funcs, strings, vtables


def decomp_path(directory, addr):
    return Path(directory) / "decomp" / (f"{addr:08X}" + ".c")


def retail_tags(text):
    """Read Ghidra's local assignments at the registration call, not source order."""
    values, tags = {}, {}
    for stmt in text.split(";"):
        m = re.search(r'\b(local_\w+)\s*=\s*(?:\([^;=]*?\)\s*)?(FUN_[0-9a-fA-F]+|0x[0-9a-fA-F]+|\d+)\s*$', stmt)
        if m:
            values[m[1]] = ("fn", int(m[2][4:], 16)) if m[2].startswith("FUN_") else ("tag", int(m[2], 0))
        m = re.search(r'FUN_10071e30\([^;]*?&(local_\w+),[^;]*?&(local_\w+)\)', stmt, re.I)
        if m and all(x in values for x in m.groups()):
            pair = [values[x] for x in m.groups()]
            if {x[0] for x in pair} == {"fn", "tag"}:
                d = dict(pair)
                tags[d["tag"]] = d["fn"]
    return tags


def disassemble(binary, start=None, end=None):
    command = ["/usr/bin/objdump", "-d", "--x86-asm-syntax=intel"]
    if start is not None:
        command.extend([f"--start-address=0x{start:X}", f"--stop-address=0x{end:X}"])
    result = subprocess.run(command + [str(binary)],
                            check=True, capture_output=True, text=True)
    ins = {}
    for line in result.stdout.splitlines():
        m = re.match(r'^([0-9a-fA-F]{8}):\s+((?:[0-9a-fA-F]{2}\s+)+)\s*(\S+)\s*(.*)', line)
        if m:
            ins[int(m[1], 16)] = (len(m[2].split()), m[3], m[4])
    if not ins:
        raise ValueError("objdump produced no instructions")
    return ins


def pe_sections(binary):
    data = Path(binary).read_bytes()
    pe = struct.unpack_from("<I", data, 0x3c)[0]
    if data[pe:pe + 4] != b"PE\0\0" or struct.unpack_from("<H", data, pe + 4)[0] != 0x14c:
        raise ValueError("expected i386 PE")
    count, opt_size = struct.unpack_from("<H", data, pe + 6)[0], struct.unpack_from("<H", data, pe + 20)[0]
    base = struct.unpack_from("<I", data, pe + 24 + 28)[0]
    sections = []
    for i in range(count):
        pos = pe + 24 + opt_size + i * 40
        _, rva, size, offset = struct.unpack_from("<IIII", data, pos + 8)
        sections.append((base + rva, data[offset:offset + size]))
    return sections


def read_u32(sections, address):
    for start, data in sections:
        if start <= address and address + 4 <= start + len(data):
            return struct.unpack_from("<I", data, address - start)[0]
    return None


def byte_at(sections, address):
    for start, data in sections:
        if start <= address < start + len(data):
            return data[address - start]
    return None


def entry_decision(sections, address):
    if byte_at(sections, address - 1) == 0xCC:
        return "separate-after-padding"
    if byte_at(sections, address - 1) == 0xC3 or byte_at(sections, address - 3) == 0xC2:
        return "separate-after-return"
    ptrs = [read_u32(sections, address - i * 4) for i in (1, 2, 3)]
    if all(p is not None and 0x1006DB10 <= p < address for p in ptrs):
        return "separate-after-jump-table"
    return "internal-entry-needs-CFG-review"


def boundary_notes(inventory, funcs, ins, sections=()):
    """Keep every inventory key; resolve missing starts with actual instructions."""
    notes = {}
    csv_addrs = {int(r["retail_addr"], 16) for r in inventory}
    for a in sorted(csv_addrs - funcs.keys()):
        inst = ins.get(a)
        aligned = a & ~3
        pointers = [read_u32(sections, aligned + d * 4) for d in range(-3, 4)]
        if all(p is not None and 0x100925A0 <= p < 0x10095594 for p in pointers):
            notes[a] = "csv-only:jump-table-data:0x100925A0;retain-inventory-key"
        elif inst and inst[1] == "jmp":
            target = re.match(r'0x([0-9a-fA-F]+)', inst[2])
            notes[a] = "csv-only:jump-thunk:" + ("0x" + target[1].upper() if target else "indirect")
        elif inst and inst[1].startswith("ret"):
            notes[a] = "csv-only:return-stub"
        elif inst:
            notes[a] = "csv-only:separate-code-after-padding:objdump" if byte_at(sections, a - 1) == 0xCC else "csv-only:instruction-entry:objdump;ghidra-missing"
        else:
            notes[a] = "csv-only:no-instruction-boundary;unresolved"
    # Record extra starts on the containing CSV interval as well as their audit rows.
    order = sorted(csv_addrs)
    extra = sorted(a for a in funcs if 0x1006DB10 <= a <= 0x100AA56F and a not in csv_addrs)
    for a in extra:
        i = bisect.bisect_right(order, a) - 1
        owner = order[i]
        suffix = "ghidra-extra:" + f"0x{a:08X}:" + entry_decision(sections, a)
        notes[owner] = (notes.get(owner, "") + ";" + suffix).strip(";")
    return notes, extra


def align(sources, funcs, strings, vtables, tags):
    matches, proposals, conflicts = {}, defaultdict(list), []
    by_name = defaultdict(list)
    for key, s in sources.items():
        by_name[s.name.split("::")[-1]].append(key)

    def offer(a, keys, anchor, confidence):
        if a not in funcs or len(keys) != 1:
            return
        key = next(iter(keys))
        if a in matches and matches[a][0] != key:
            conflicts.append((a, sources[key].name, anchor))
        elif a not in matches:
            proposals[a].append((key, anchor, confidence))

    def accept():
        # Reject conflicting anchors rather than taking whichever was visited first.
        for a in sorted(proposals):
            p = proposals[a]
            votes = Counter(x[0] for x in p)
            ranked = votes.most_common()
            # Inlined helpers can donate a literal to a larger caller. A clear
            # plurality of independent literals identifies the containing body.
            if len(ranked) == 1 or (ranked[0][1] >= 3 and ranked[0][1] >= 3 * ranked[1][1]):
                key = ranked[0][0]
                chosen = [x for x in p if x[0] == key]
                confidence = chosen[0][2]
                matches[a] = (key, ";".join(sorted({x[1] for x in chosen})), confidence)
            else:
                conflicts.extend((a, sources[k].name, anchor) for k, anchor, _ in p)
        proposals.clear()

    us, rs = defaultdict(set), defaultdict(set)
    for key, source in sources.items():
        for value in source.strings:
            if len(value) >= 6:
                us[value].add(key)
    for a, f in funcs.items():
        for p in f["strings"]:
            if p in strings:
                rs[strings[p]].add(a)
    for value in sorted(us.keys() & rs.keys()):
        if len(us[value]) == len(rs[value]) == 1:
            a = next(iter(rs[value]))
            # Address, never the literal itself, leaves scratch memory.
            ptrs = [p for p in funcs[a]["strings"] if strings.get(p) == value]
            offer(a, us[value], "string:" + f"0x{min(ptrs):08X}", "high")
    accept()
    for s in sources.values():
        code, _ = BL.blank_code(s.body)
        for m in re.finditer(r'\bregister_tag_loader\s*\(\s*(\d+)\s*,\s*(\w+)\s*\)', code):
            if int(m[1]) in tags:
                offer(tags[int(m[1])], by_name[m[2]], "tag:" + m[1], "high")
    offer(0x1007CF60, by_name["ensure_loaders_registered"], "tag:registrar", "high")
    accept()
    return matches, by_name, conflicts


def resolved_calls(source, sources, by_name):
    result = set()
    scope = source.name.rsplit("::", 1)[0] if "::" in source.name else ""
    for call in source.calls:
        keys = by_name.get(call.split("::")[-1], [])
        exact = [k for k in keys if sources[k].name == call or sources[k].name == scope + "::" + call]
        if len(exact) == 1:
            result.add(exact[0])
        elif len(keys) == 1:
            result.add(keys[0])
    return result


def graph_shape_agrees(source, retail):
    code, _ = BL.blank_code(source.body)
    # Common destructors in all callers can mimic an inlined accessor's entire
    # caller neighbourhood. A branch-free accessor cannot explain that body.
    simple = not source.calls and not re.search(r'\b(if|for|while|switch)\b|\?', code)
    return not (simple and retail.get("branch_ops", 0))


def registration_anchors(directory, sources, funcs, matches, by_name):
    """Match address-taken callbacks through member identifiers in a known registrar.

    These are source literals paired with code pointers, not direct call edges.
    Only Ghidra's explicit function-pointer assignment followed by the matching
    key constructor is accepted. Macros/unrecognised temporary forms stay open.
    """
    proposals = defaultdict(set)
    for a, (key, _, _) in sorted(matches.items()):
        s = sources[key]
        pairs = re.findall(r'\bset_member\s*\(\s*"(\w+)"\s*,\s*&\s*(\w+)\s*\)', s.body)
        if not pairs:
            continue
        upstream = {name: by_name.get(fn, []) for name, fn in pairs}
        path = decomp_path(directory, a)
        if not path.exists():
            continue
        pending = None
        for line in path.read_text().splitlines():
            m = re.search(r'=\s*(?:CONCAT44\([^,]+,\s*)?FUN_([0-9a-fA-F]{8})\)?\s*;', line)
            if m:
                pending = int(m[1], 16)
            m = re.search(r'FUN_10007680\([^,]+,"(\w+)"\)', line, re.I)
            if m:
                keys = upstream.get(m[1], [])
                if pending in funcs and len(keys) == 1:
                    proposals[pending].add((keys[0], f"member:0x{a:08X}:{m[1]}"))
                pending = None
    for a, pairs in sorted(proposals.items()):
        keys = {k for k, _ in pairs}
        if len(keys) == 1 and a not in matches:
            matches[a] = (next(iter(keys)), ";".join(sorted(v for _, v in pairs)), "high")


def propagate(sources, funcs, matches, by_name):
    """Mutually unique graph residuals; inline/virtual calls prevent proof of order.

    Requires two independent anchored neighbours. Single-parent proposals remain
    candidates and are not promoted by a size guess.
    """
    graph = {k: resolved_calls(s, sources, by_name) for k, s in sources.items()}
    for _ in range(12):
        support = defaultdict(set)
        used = {m[0] for m in matches.values()}
        # Compare caller neighbourhoods within the anchored subset. Deriving
        # these from callees recovers valid B edges even when ncallers > 40
        # caused the export to abbreviate its caller list to 'many'.
        source_parents, retail_parents = defaultdict(set), defaultdict(set)
        for a, (key, _, _) in sorted(matches.items()):
            for child in graph[key] - used:
                source_parents[child].add(key)
            for child in set(funcs[a]["callees"]) & funcs.keys() - matches.keys():
                retail_parents[child].add(key)
        left_groups, right_groups = defaultdict(list), defaultdict(list)
        for key, parents in source_parents.items():
            if len(parents) >= 2:
                left_groups[tuple(sorted(parents))].append(key)
        for a, parents in retail_parents.items():
            if len(parents) >= 2:
                right_groups[tuple(sorted(parents))].append(a)
        for parents in sorted(left_groups.keys() & right_groups.keys()):
            if len(left_groups[parents]) == len(right_groups[parents]) == 1:
                child, a = left_groups[parents][0], right_groups[parents][0]
                support[(a, child)].update(p for p, m in matches.items() if m[0] in parents)
        for a, (key, _, _) in sorted(matches.items()):
            left = graph[key] - used
            right = set(funcs[a]["callees"]) & funcs.keys() - matches.keys()
            if len(left) == len(right) == 1:
                support[(next(iter(right)), next(iter(left)))].add(a)
        # Also propagate unique callers (export's 'many' has no invented edges).
        for a, (key, _, _) in sorted(matches.items()):
            left = {k for k, v in graph.items() if key in v} - used
            right = set(funcs[a]["callers"]) & funcs.keys() - matches.keys()
            if len(left) == len(right) == 1:
                support[(next(iter(right)), next(iter(left)))].add(a)
        candidates = [(a, k, v) for (a, k), v in support.items() if len(v) >= 2]
        ac, kc = Counter(a for a, _, _ in candidates), Counter(k for _, k, _ in candidates)
        added = 0
        for a, k, refs in sorted(candidates):
            if (ac[a] == kc[k] == 1 and not funcs[a].get("alignment_rejected")
                    and graph_shape_agrees(sources[k], funcs[a])):
                matches[a] = (k, "graph:" + ";".join(f"0x{x:08X}" for x in sorted(refs)), "medium")
                added += 1
        if not added:
            break


def graph_candidates(sources, funcs, matches, by_name):
    """Keep weaker graph/size suggestions separate from accepted name matches."""
    graph = {k: resolved_calls(s, sources, by_name) for k, s in sources.items()}
    used = {v[0] for v in matches.values()}
    ratios = [funcs[a]["ninstr"] / max(sources[k].tokens, 1)
              for a, (k, _, confidence) in matches.items()
              if confidence == "high" and funcs[a]["ninstr"] >= 10]
    ratio = statistics.median(ratios) if ratios else 1
    inverted = defaultdict(set)
    for key, children in graph.items():
        for child in children & used:
            inverted[child].add(key)
    rows = []
    for a, f in sorted(funcs.items()):
        if a in matches or not (0x1006DB10 <= a <= 0x100AA56F):
            continue
        neighbours = {matches[c][0] for c in f["callees"] if c in matches}
        candidates = set().union(*(inverted[n] for n in neighbours)) if neighbours else set()
        ranked = []
        for key in candidates - used:
            shared = graph[key] & neighbours
            # Common logging helpers alone cannot identify a function.
            if len(shared) < 2:
                continue
            weight = sum(1 / len(inverted[n]) for n in sorted(shared))
            expected = max(sources[key].tokens * ratio, 1)
            size_distance = abs(f["ninstr"] - expected) / max(f["ninstr"], expected)
            ranked.append((-weight, size_distance, key, len(shared)))
        for rank, (_, distance, key, support) in enumerate(sorted(ranked)[:3], 1):
            rows.append({"address": f"0x{a:08X}", "rank": rank, "symbol": sources[key].name,
                         "file": sources[key].file, "shared_neighbours": support,
                         "size_distance": f"{distance:.6f}", "confidence": "low",
                         "decision": "candidate-only;graph-score;size-tiebreak;not-ledger-name"})
    return rows


def vtable_candidates(layouts, sources, funcs, vtables, matches, by_name):
    """Export class/order candidates, including rejected complex inheritance.

    No slot is automatically named from an approximate layout. Known slots seed
    order checks and vtwrites identify possible constructors/destructors, never
    discriminate them without a body review.
    """
    rows = []
    for table in vtables:
        known = [(i, sources[matches[a][0]].name) for i, a in enumerate(table["slots"]) if a in matches]
        classes = Counter(n.rsplit("::", 1)[0].split("::")[-1] for _, n in known if "::" in n)
        for cls, n in sorted(classes.items()):
            layout = layouts.get(cls)
            if not layout:
                continue
            writers = [a for a, f in funcs.items() if table["addr"] in f["vtwrites"]]
            rows.append({"vtable": f"0x{table['addr']:08X}", "class": cls, "known_slots": n,
                         "slots": len(table["slots"]), "declared_virtuals": len(layout["virtuals"]),
                         "writers": ";".join(f"0x{a:08X}" for a in sorted(writers)),
                         "decision": "candidate:manual-inheritance-and-slot-review"})
    return rows


def vtable_anchors(layouts, sources, funcs, vtables, matches):
    """Fill only slots bracketed by two agreeing upstream declaration anchors.

    MSVC's primary table follows the first nonvirtual base; a virtual base has
    a separate table. Additional slots or secondary tables break the bracket,
    rather than silently shifting everything after a DICE addition.
    Overload names are not enough to select a body and stay unresolved.
    """
    def primary(cls, seen=()):
        if cls in seen or cls not in layouts:
            return []
        layout = layouts[cls]
        bases = layout["bases"]
        # Virtual ref_counted is the only virtual-base case this parser knows.
        inherited = [] if bases == ["ref_counted"] else primary(bases[0], seen + (cls,)) if bases else []
        out = list(inherited)
        for name in layout["virtuals"]:
            if name not in out and not name.startswith("~"):
                out.append(name)
        return out

    proposals = defaultdict(set)
    for table in vtables:
        known = [(i, sources[matches[a][0]].name) for i, a in enumerate(table["slots"]) if a in matches]
        classes = Counter(n.rsplit("::", 1)[0] for _, n in known if n.count("::") >= 2)
        for qual, support in sorted(classes.items()):
            if support < 2:
                continue
            cls = qual.split("::")[-1]
            order = primary(cls)
            anchors = [(i, order.index(n.split("::")[-1])) for i, n in known
                       if n.startswith(qual + "::") and order.count(n.split("::")[-1]) == 1]
            for (left, sl), (right, sr) in zip(anchors, anchors[1:]):
                if right - left != sr - sl or sr <= sl:
                    continue
                for i in range(left + 1, right):
                    name = qual + "::" + order[sl + i - left]
                    keys = [k for k, s in sources.items() if s.name == name]
                    a = table["slots"][i]
                    if len(keys) == 1 and a in funcs and a not in matches:
                        proposals[a].add((keys[0], f"vtable:0x{table['addr']:08X}:slot{i}:bracket{left}-{right}"))
    for a, pairs in sorted(proposals.items()):
        if len({k for k, _ in pairs}) == 1:
            matches[a] = (next(iter(pairs))[0], ";".join(sorted(v for _, v in pairs)), "medium")


def tree_citations(root, inventory, funcs, matches, sources, ins=None):
    rows = []
    cited = {int(r["retail_addr"], 16) for r in inventory if r["cited_by_tree"]}
    references = defaultdict(list)
    owners = defaultdict(set)
    pattern = re.compile(r'(?:sub_|0x)(100[0-9a-f]{5})\b', re.I)
    for directory in ("gameswf", "base"):
        for path in sorted((root / "Code/BF2/External" / directory).glob("*")):
            if path.suffix not in (".cpp", ".h", ".inl"):
                continue
            definitions, declarations, _ = BL.parse_file(str(path))
            for d in definitions + declarations:
                # A citation in an attached comment is a proposal. References in
                # implementation text may name a callee and do not claim ownership.
                for m in pattern.finditer(d["comment"]):
                    a = int(m[1], 16)
                    if a in cited:
                        name = "::".join(d.get("namespaces", ["gameswf"] if directory == "gameswf" else []) + d["comps"])
                        owners[a].add(name)
            for line, text in enumerate(path.read_text(errors="replace").splitlines(), 1):
                for m in pattern.finditer(text):
                    a = int(m[1], 16)
                    if a in cited:
                        references[a].append(f"{path.relative_to(root)}:{line}")
    for a in sorted(cited):
        if ins is not None and a not in ins:
            raise ValueError(f"tree citation has no objdump entry: 0x{a:08X}")
        aligned = sources[matches[a][0]].name if a in matches else ""
        decision = "entry-verified;identity-unverified;citation-may-name-callee"
        if aligned and aligned in owners[a]:
            decision = "entry-and-identity-supported-by-independent-anchor;body-not-proven"
        elif aligned:
            decision = "entry-verified;independent-name;tree-owner-differs-or-missing"
        rows.append({"address": f"0x{a:08X}", "references": ";".join(sorted(set(references[a]))),
                     "entry": "ghidra+objdump" if a in funcs and ins is not None else "ghidra" if a in funcs else "csv-only",
                     "aligned": aligned, "tree_candidates": ";".join(sorted(owners[a])),
                     "decision": decision if a in funcs else "boundary-review"})
    return rows


def citation_anchors(citations, sources, funcs, matches, by_name):
    """Use tree citations only after two source/retail callee identities agree."""
    for row in citations:
        a = int(row["address"], 16)
        if a in matches or a not in funcs:
            continue
        names = set(row["tree_candidates"].split(";"))
        keys = [k for k, s in sources.items() if s.name in names]
        if len(keys) != 1:
            continue
        key = keys[0]
        upstream = resolved_calls(sources[key], sources, by_name)
        retail = {matches[c][0] for c in funcs[a]["callees"] if c in matches}
        shared = upstream & retail
        if len(shared) >= 2:
            matches[a] = (key, "citation:two-callee-cross-check", "medium")
    for row in citations:
        a = int(row["address"], 16)
        if a in matches:
            row["aligned"] = sources[matches[a][0]].name
            if row["aligned"] in row["tree_candidates"].split(";"):
                row["decision"] = "entry-and-identity-supported-by-independent-anchor;body-not-proven"


def make_rows(inventory, funcs, strings, matches, sources, notes, ins, reviews):
    rows = []
    review_at = {int(r["address"], 16): r for r in reviews}
    for item in inventory:
        a = int(item["retail_addr"], 16)
        f = funcs.get(a)
        row = dict(address=f"0x{a:08X}", size=f["size"] if f else item["bytes"],
                   symbol=f"FUN_{a:08X}", file="", base_rev="", match="unknown", confidence="low",
                   anchor="", boundary=notes.get(a, ""), reason="no-defensible-counterpart",
                   csv_size=item["bytes"], ninstr=item["instructions"], file_inferred=1)
        if a in matches:
            key, anchor, confidence = matches[a]
            s = sources[key]
            row.update(symbol=s.name, file=s.file, base_rev="r586" if "/gameswf/" in s.file else "release-2004-05-12-approx",
                       confidence=confidence, anchor=anchor, reason="name-aligned;body-not-reviewed", file_inferred=0)
        if row["boundary"].startswith("csv-only:jump-thunk:"):
            row.update(size=ins[a][0], match="inline", reason="compiler-jump-thunk:objdump", anchor="boundary:objdump")
        elif row["boundary"].startswith("csv-only:separate-code-after-padding:"):
            current = a
            for _ in range(20):
                if current not in ins:
                    break
                length, opcode, _ = ins[current]
                current += length
                if opcode.startswith("ret"):
                    row["size"] = current - a
                    break
        elif "jump-table-data" in row["boundary"]:
            row.update(reason="inventory-artifact:jump-table-data;not-a-function", confidence="high")
        r = review_at.get(a)
        if r:
            # Reviewed identifiers can identify DICE-only routines absent from the base.
            row.update(symbol=r["symbol"], file=r["file"], match=r["match"],
                       reason=r["reason"], confidence=r["confidence"],
                       anchor=(row["anchor"] + ";review").strip(";"),
                       file_inferred=int(r["match"] == "new" or not r["file"]))
            if r.get("base_rev"):
                row["base_rev"] = r["base_rev"]
            elif r["match"] == "new":
                row["base_rev"] = ""
        rows.append(row)
    # File placement is an explicit guess, only between consistent neighbours.
    known = [(int(r["address"], 16), r["file"]) for r in rows if r["file"]]
    order = [a for a, _ in known]
    for r in rows:
        if r["file"] or not known:
            continue
        a = int(r["address"], 16)
        i = bisect.bisect_left(order, a)
        if 0 < i < len(known) and known[i - 1][1] == known[i][1]:
            r["file"] = known[i][1]
            r["reason"] += ";[INFERRED]:bracketed-TU"
    return sorted(rows, key=lambda r: int(r["address"], 16))


def apply_review_names(matches, reviews, funcs, sources):
    """Reject disproven automatic anchors before any propagation uses them."""
    for review in reviews:
        a = int(review["address"], 16)
        if review["match"] == "new" or review["symbol"].startswith("FUN_"):
            matches.pop(a, None)
            if a in funcs:
                funcs[a]["alignment_rejected"] = True
        elif a in funcs:
            keys = [k for k, s in sources.items() if s.name == review["symbol"]]
            if len(keys) == 1:
                old_anchor = matches[a][1] if a in matches and matches[a][0] == keys[0] else ""
                matches[a] = (keys[0], (old_anchor + ";reviewed-name").strip(";"), review["confidence"])


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--export", type=Path, default=EXPORT, required=EXPORT is None,
                   help="Ghidra export of SwiffPlayer.dll (default: $SWF_EXPORT_DIR)")
    p.add_argument("--history", type=Path, default=HISTORY, required=HISTORY is None,
                   help="gameswf history checkouts (default: $GAMESWF_HISTORY_DIR)")
    p.add_argument("--binary", type=Path)
    p.add_argument("--output", type=Path, default=REPO / "out/swf-align")
    p.add_argument("--reviews", type=Path, default=REVIEWS)
    args = p.parse_args()
    binary = args.binary or args.export.parent / "SwiffPlayer.dll"
    sources, layouts = parse_sources(args.history / "svn-r586", "Code/BF2/External/gameswf/")
    base, base_layouts = parse_sources(args.history / "rel-2004-05-12/src/tu-testbed/base", "Code/BF2/External/base/")
    sources.update(base)
    layouts.update(base_layouts)
    inventory = read_csv(INVENTORY)
    if len(inventory) != 2164 or len({r["retail_addr"] for r in inventory}) != 2164:
        raise ValueError("inventory must contain exactly 2164 unique addresses")
    funcs, strings, vtables = load_export(args.export)
    ins = disassemble(binary)
    sections = pe_sections(binary)
    notes, extra = boundary_notes(inventory, funcs, ins, sections)
    # Linear objdump can consume the first bytes after an embedded jump table.
    # Restart at each disputed entry to check the real boundary independently.
    disputed = extra + [int(r["retail_addr"], 16) for r in inventory if int(r["retail_addr"], 16) not in funcs]
    for a in sorted(disputed):
        ins.update(disassemble(binary, a, a + 48))
    notes, extra = boundary_notes(inventory, funcs, ins, sections)
    branches = sorted(at for at, (_, op, _) in ins.items() if op.startswith("j") and op != "jmp")
    for a, f in funcs.items():
        f["branch_ops"] = bisect.bisect_left(branches, a + f["size"]) - bisect.bisect_left(branches, a)
    tags = retail_tags(decomp_path(args.export, 0x1007CF60).read_text())
    matches, by_name, conflicts = align(sources, funcs, strings, vtables, tags)
    reviews = read_csv(args.reviews) if args.reviews.exists() else []
    if len({r["address"] for r in reviews}) != len(reviews):
        raise ValueError("duplicate reviewed address")
    if any(r["match"] not in {"unchanged", "changed", "new", "inline", "unknown"} for r in reviews):
        raise ValueError("invalid review classification")
    apply_review_names(matches, reviews, funcs, sources)
    vtable_anchors(layouts, sources, funcs, vtables, matches)
    registration_anchors(args.export, sources, funcs, matches, by_name)
    apply_review_names(matches, reviews, funcs, sources)
    propagate(sources, funcs, matches, by_name)
    citations = tree_citations(REPO, inventory, funcs, matches, sources, ins)
    citation_anchors(citations, sources, funcs, matches, by_name)
    apply_review_names(matches, reviews, funcs, sources)
    propagate(sources, funcs, matches, by_name)
    decomp = {a: decomp_path(args.export, a).read_text()
              for a in sorted(funcs) if decomp_path(args.export, a).exists()}
    sequences, order_audit = sequence.sequence_proposals(
        sys.modules[__name__], sources, funcs, strings, matches, by_name, decomp)
    write_csv(args.output / "sequences.csv",
              ["address", "key", "symbol", "file", "score", "margin", "literals", "callees", "constants",
               "size_distance", "first", "last", "decision"], sequences)
    write_csv(args.output / "order.csv", ["file", "first", "last", "first_line", "last_line", "decision"], order_audit)
    rows = make_rows(inventory, funcs, strings, matches, sources, notes, ins, reviews)
    slots = sequence.reviewed_vtables(sys.modules[__name__], REPO / "docs/tracks/swf/s1-layouts.csv",
                                     vtables, funcs, rows, sources, decomp)
    write_csv(args.output / "slots.csv", ["vtable", "slot", "address", "symbol", "decision", "writers"], slots)
    templates = sequence.template_instances(sys.modules[__name__], REPO / "docs/tracks/swf/s1-template-rules.csv",
                                           decomp, funcs, rows)
    write_csv(args.output / "templates.csv", ["address", "exemplar", "shape", "variant", "symbol", "decision"], templates)
    structural = sequence.structural_templates(decomp, funcs, rows)
    write_csv(args.output / "structural.csv", ["address", "symbol", "rule", "decision"], structural)
    bindings = sequence.callback_bindings(decomp, rows)
    write_csv(args.output / "bindings.csv", ["address", "registrar", "member", "decision"], bindings)
    for row in rows:
        row["change_reason"] = sequence.change_reason(row)
    write_csv(args.output / "s2-scope.csv", ["address", "size", "ninstr", "symbol", "file", "match", "confidence",
                                           "change_reason", "scope", "open_item"],
              sequence.pilot_scope(rows, funcs, slots))
    write_csv(args.output / "open.csv", ["address", "ninstr", "size", "symbol", "file", "reason"],
              sorted((r for r in rows if r["match"] == "unknown" and int(r["ninstr"]) > 30),
                     key=lambda r: (-int(r["ninstr"]), r["address"])))
    write_csv(args.output / "align.csv", FIELDS, rows)
    write_csv(args.output / "vtables.csv", ["vtable", "class", "known_slots", "slots", "declared_virtuals", "writers", "decision"],
              vtable_candidates(layouts, sources, funcs, vtables, matches, by_name))
    write_csv(args.output / "citations.csv", ["address", "references", "entry", "aligned", "tree_candidates", "decision"],
              citations)
    boundaries = [{"address": f"0x{a:08X}", "decision": note} for a, note in sorted(notes.items())]
    boundaries.extend({"address": f"0x{a:08X}", "decision": "ghidra-only:objdump-entry;" + entry_decision(sections, a)
                       if a in ins else "ghidra-only:unresolved"} for a in extra)
    write_csv(args.output / "boundaries.csv", ["address", "decision"], boundaries)
    write_csv(args.output / "tags.csv", ["tag", "address"], [{"tag": k, "address": f"0x{v:08X}"} for k, v in sorted(tags.items())])
    write_csv(args.output / "conflicts.csv", ["address", "symbol", "anchor"],
              [{"address": f"0x{a:08X}", "symbol": name, "anchor": kind} for a, name, kind in sorted(set(conflicts))])
    write_csv(args.output / "source-index.csv", ["key", "file", "line", "tokens", "literals", "callees"],
              [{"key": k, "file": s.file, "line": s.line, "tokens": s.tokens,
                "literals": len(s.strings), "callees": len(s.calls)} for k, s in sorted(sources.items())])
    write_csv(args.output / "candidates.csv", ["address", "rank", "symbol", "file", "shared_neighbours", "size_distance", "confidence", "decision"],
              graph_candidates(sources, funcs, matches, by_name))
    provenance = {"image": "SwiffPlayer.dll", "architecture": "i386", "base_rev": "r586",
                  "base_support": "release-2004-05-12-approx", "sources": len(sources),
                  "ghidra_B": sum(0x1006DB10 <= a <= 0x100AA56F for a in funcs),
                  "ghidra_E": sum(a >= 0x100D0BA0 for a in funcs),
                  "hashes": {"retail": hashlib.sha256(binary.read_bytes()).hexdigest(),
                             "inventory": hashlib.sha256(INVENTORY.read_bytes()).hexdigest()}}
    for filename in ("functions.tsv", "strings.tsv", "vtables.tsv"):
        provenance["hashes"][filename] = hashlib.sha256((args.export / filename).read_bytes()).hexdigest()
    digest = hashlib.sha256()
    for a, text in sorted(decomp.items()):
        digest.update(f"{a:08X}".encode())
        digest.update(text.encode())
    provenance["hashes"]["available-decompile-text"] = digest.hexdigest()
    provenance["missing_decomp_B"] = [f"0x{a:08X}" for a in sorted(funcs)
                                      if 0x1006DB10 <= a <= 0x100AA56F and a not in decomp]
    provenance["B_vtable_candidates"] = sum(any(0x1006DB10 <= a <= 0x100AA56F for a in v["slots"]) for v in vtables)
    for path in (args.reviews, REPO / "docs/tracks/swf/s1-layouts.csv",
                 REPO / "docs/tracks/swf/s1-template-rules.csv", Path(__file__),
                 Path(__file__).with_name("swf_sequence.py"), Path(__file__).with_name("swf_ledger.py"),
                 Path(__file__).with_name("test_swf_ledger.py"),
                 REPO / "tools/ai-reconstruct/build_ledger.py"):
        provenance["hashes"][path.name] = hashlib.sha256(path.read_bytes()).hexdigest()
    digest = hashlib.sha256()
    for path in sorted((args.history / "rel-2004-05-12/src/tu-testbed/base").iterdir()):
        if path.suffix in (".cpp", ".h", ".c", ".inl"):
            digest.update(path.name.encode())
            digest.update(path.read_bytes())
    provenance["hashes"]["base-support-input"] = digest.hexdigest()
    revisions = {}
    for rev in ("r577", "r578", "r579", "r580", "r581", "r582", "r583", "r584", "r585", "r586", "r588"):
        path = args.history / ("svn-" + rev)
        if not path.exists():
            revisions[rev] = {"available": False}
            continue
        files = sorted(p for p in path.iterdir() if p.suffix in (".cpp", ".h"))
        digest = hashlib.sha256()
        different = []
        for file in files:
            digest.update(file.name.encode())
            digest.update(file.read_bytes())
            primary = args.history / "svn-r586" / file.name
            if not primary.exists() or file.read_bytes() != primary.read_bytes():
                different.append(file.name)
        revisions[rev] = {"available": True, "sha256": digest.hexdigest(), "different_files": different}
    provenance["revision_inputs"] = revisions
    (args.output / "provenance.json").write_text(json.dumps(provenance, sort_keys=True, indent=2) + "\n")
    print(f"sources={len(sources)} name_alignments={len(matches)} tags={len(tags)} "
          f"ghidra_extra_B={len(extra)} classifications={dict(Counter(r['match'] for r in rows))}")


if __name__ == "__main__":
    main()
