"""Editor-only scope and inlining rules; filenames alone are never identity anchors."""
import collections
import json
from pathlib import Path
import re

from classify import hx, literal
from extract import REPO, console_macro_sites, norm_name


def editor_reason(path):
    path = path.replace("\\", "/")
    if re.search(r"(?:^|/)(?:TerrainEditable[^/]*|LightmapGeneration[^/]*|StaticMeshLightmapGeneration)\.(?:cpp|h|inl)$", path):
        return ("TerrainEditable operations require the editable terrain/editor content branch" if
                "TerrainEditable" in path else
                "lightmap generation path; 1.50 never creates LightmapGeneration (docs/architecture.md)")
    if "/LightmapGeneration/" in "/" + path:
        return "lightmap generator implementation; instantiated only by the editor renderer flavour"
    return ""


def editor(data, ledger, rmap, matcher):
    result = {}
    for r in ledger:
        if r["image"] != "RendDX9.dll" or r["status"] != "todo":
            continue
        a = int(r["address"], 16)
        f = matcher.F[a]
        evidence = []
        for s in f["strings"]:
            if re.search(r"code[\\/]+bf2[\\/]", s, re.I) and editor_reason(s):
                evidence.append("retail __FILE__ " + json.dumps(s))
        matched = rmap.get(a)
        if matched and matched["confidence"] == "high" and matched["file_basis"] == "site":
            if editor_reason(matched["file"]):
                evidence.append("high _r own __FILE__ site " + matched["file"] +
                                " at " + matched["addr_r"] + " via " + matched["method"])
        # A specifically named interface declaration is a stronger scope signal
        # than the ledger's address-neighbour file guess. It remains unrecovered.
        if r["symbol"].startswith("ITerrainEditable::"):
            evidence.append("attested TerrainEditable vtable/interface declaration " + r["symbol"])
        if evidence:
            evidence.append("editor-only editable terrain/lightmap operation; no general STL-helper propagation")
            result[a] = dict(classification="editor", confidence="medium", our_definition="", evidence=evidence)
    return result


def inlined(data, matcher):
    left, right = collections.defaultdict(set), collections.defaultdict(set)
    for a, f in matcher.F.items():
        for s in f["strings"]:
            if literal(s):
                left[("string", s)].add(a)
        for api in f["apis"]:
            if api.startswith(("D3D", "D3DX")):
                left[("api", api)].add(a)
    for b, g in matcher.G.items():
        if not matcher.usable(b):
            continue
        for s in g["strings"]:
            if literal(s):
                right[("string", s)].add(matcher.group(b))
        for api in g["apis"]:
            if api.startswith(("D3D", "D3DX")):
                right[("api", api)].add(matcher.group(b))
    votes = collections.defaultdict(lambda: collections.defaultdict(list))
    for token in sorted(left.keys() & right.keys()):
        if len(left[token]) != 1 or len(right[token]) != 1:
            continue
        a, group = next(iter(left[token])), next(iter(right[token]))
        if a in matcher.pairs or a not in matcher.L or matcher.L[a]["status"] != "todo":
            continue
        for caller in matcher.F[a]["callers"]:
            parent = matcher.pairs.get(caller)
            if parent and matcher.group(parent["native"]) == group and matcher.usable(parent["native"]):
                votes[a][caller].append("retail helper's unique %s %s is emitted inside native caller %s" %
                                        (token[0], json.dumps(token[1]), hx(caller)))
    result = {}
    for a, callers in sorted(votes.items()):
        evidence, definitions, anchors = [], [], 0
        for caller, proofs in sorted(callers.items()):
            evidence.extend(proofs)
            evidence.append("retail direct call " + hx(caller) + " -> " + hx(a))
            definitions.append(matcher.source_label(matcher.pairs[caller]["native"]))
            anchors += len(proofs)
        result[a] = dict(classification="inlined", confidence="high" if anchors >= 2 else "medium",
                         our_definition=" | ".join(definitions), evidence=evidence)
    return result


def console_slot_mapping(retail_slots, native_slots, functions, native_count):
    """Derive ordinary-slot alignment from actual field getters, or refuse it."""
    fields = {"getObjectName": ("mov", "eax, dword ptr [ecx + 0x8]"),
              "getFunctionName": ("mov", "eax, dword ptr [ecx + 0xc]"),
              "getType": ("mov", "eax, dword ptr [ecx + 0x4]"),
              "getAccess": ("mov", "eax, dword ptr [ecx + 0x10]"),
              "getCategory": ("mov", "al, byte ptr [ecx + 0x14]")}
    prolog = {("push", "ebp"), ("mov", "ebp, esp"), ("pop", "ebp"), ("ret", "")}
    agreeing = []
    for name, getter in fields.items():
        ns = [slot for slot, target in native_slots.items() if "::" + name + "(" in target["name"]]
        rs = []
        for slot, target in enumerate(retail_slots):
            body = [(m, o) for _, m, o in functions[target]["insns"] if m not in ("nop", "int3")]
            if [i for i in body if i not in prolog] == [getter]:
                rs.append(slot)
        if len(ns) == len(rs) == 1:
            agreeing.append((name, rs[0], ns[0]))
    deltas = {n - r for _, r, n in agreeing}
    if len(agreeing) < 4 or len(deltas) != 1:
        return None
    delta = next(iter(deltas))
    return (delta, agreeing) if len(retail_slots) + delta == native_count else None


def console_bindings(data, matcher, runtime):
    """Tie generated console classes through their actual constructor and tables.

    Clang outlines TU initialization into __StaticInit, and inlines generated
    binding constructors there. Infer no Itanium/MSVC slot delta: derive and
    verify it separately for each table from four exact retail getter bodies.
    """
    from report import all_callers
    xrefs = all_callers(runtime)
    vt = {int(k, 16): v for k, v in data["retail"]["vtables"].items()}
    strings_pe, strings_native = collections.defaultdict(set), collections.defaultdict(set)
    for a, f in matcher.F.items():
        for s in f["strings"]:
            if literal(s):
                strings_pe[s].add(a)
    for b, g in matcher.G.items():
        for s in g["strings"]:
            if literal(s):
                strings_native[s].add(b)
    source_macros = {}
    def invocation(file, spelling):
        if file not in source_macros:
            text = (REPO / file).read_text() if file and (REPO / file).is_file() else ""
            found = collections.defaultdict(list)
            for site in console_macro_sites(text):
                if site["macro"] == "BF2_CONSOLE_OBJECT":
                    continue
                m = re.match(r'\w+\s*\(\s*(?:"([^"\n]+)"|([A-Za-z_]\w*))', site["body"])
                if m:
                    found[m[1] or m[2]].append(site)
            source_macros[file] = found
        return source_macros[file].get(spelling, [])
    result, method_votes = {}, collections.defaultdict(dict)
    used_tables = []
    source_names = collections.defaultdict(list)
    for d in matcher.sources:
        source_names[norm_name(d["name"])].append(d)
    def descriptor(sym, object_name):
        targets = data["native"]["symbols"].get(sym, [])
        local = [t for t in targets if matcher.G[t]["object"] == object_name]
        groups = {matcher.group(t) for t in (local or targets)}
        if len(groups) == 1:
            target = min(local or targets)
            source = matcher.G[target]["source"]
            return dict(name=matcher.G[target]["name"], native=target,
                        usable=matcher.usable(target), key=matcher.group(target),
                        frontier=source is not None and matcher.sources[source]["stopping"],
                        definition=matcher.source_label(target))
        name = data["native"].get("externals", {}).get(sym, "")
        source = source_names.get(norm_name(name), [])
        if len(source) == 1:
            d = source[0]
            return dict(name=name, native=None, usable=not d["stopping"], frontier=d["stopping"], key=(d["file"], name),
                        definition="%s:%d %s [typed host method referenced by renderer vtable]" %
                        (d["file"], d["line"], name))
        return None
    for spelling in sorted(strings_pe.keys() & strings_native.keys()):
        if len(strings_pe[spelling]) != 1 or len(strings_native[spelling]) != 1:
            continue
        a, b = next(iter(strings_pe[spelling])), next(iter(strings_native[spelling]))
        f, g = matcher.F[a], matcher.G[b]
        if g.get("code_section") != "__StaticInit":
            continue
        macros = invocation(g["source_file"], spelling)
        if len(macros) != 1:
            continue
        site = macros[0]
        line, class_line, macro = site["line"], site["end_line"], site["macro"]
        # The line is not a placement guess: it must name the class whose vtable
        # the compiler actually installs in this TU initializer.
        class_tables = [t for t in data["native"]["tables"] if t["object"] == g["object"] and
                        re.search(r"::ConsoleClass%d$" % class_line, t["name"]) and
                        t["name"] in g.get("vtrefs", [])]
        retail_tables = sorted({int(h, 16) for _, _, o in f["insns"] for h in re.findall(r"0x[0-9a-f]+", o)
                                if int(h, 16) in vt and 30 <= len(vt[int(h, 16)]) <= 40})
        parents = [c for c in xrefs.get(a, []) if c["caller"] and
                   int(c["caller"], 16) in runtime.static_initializers]
        if len(class_tables) != 1 or len(retail_tables) != 1 or not parents:
            continue
        table, va = class_tables[0], retail_tables[0]
        slots, rt = {}, vt[va]
        for slot, sym in table["slots"]:
            value = descriptor(sym, table["object"])
            if value:
                slots[slot] = value
        mapping = console_slot_mapping(rt, slots, matcher.F,
                                       max((slot for slot, _ in table["slots"]), default=-1) + 1)
        if mapping is None:
            continue
        delta, agreeing = mapping
        proof = ["unique console spelling " + json.dumps(spelling) + " in retail constructor and native __StaticInit",
                 "source %s:%d %s emits ConsoleClass%d, whose vtable is installed by %s" %
                 (g["source_file"], line, macro, class_line, g["name"]),
                 "constructor called from CRT initializer(s) " + ",".join(sorted({c["caller"] for c in parents})),
                 "retail table %s -> %s; slot delta %d independently agrees for %s; full table lengths agree" %
                 (hx(va), table["name"], delta, ",".join("%s %d->%d" % t for t in agreeing))]
        used_tables.append(dict(constructor=hx(a), native_initializer=b, retail_table=hx(va),
                                native_table=table["name"], slot_delta=delta, getters=agreeing))
        if a in matcher.L and matcher.L[a]["status"] == "todo" and a not in matcher.pairs:
            class_name = table["name"].removeprefix("vtable for ")
            result[a] = dict(classification="inlined", confidence="high", evidence=proof,
                             our_definition="%s:%d %s::%s() (%s expansion), inlined into %s" %
                             (g["source_file"], line, class_name, class_name.rsplit("::", 1)[-1], macro, g["name"]))
        for slot, target in enumerate(rt):
            if slot == 0 or target in matcher.pairs or slot + delta not in slots:
                continue  # deleting-destructor position is compiler machinery, not a shifted ordinary slot
            native_target = slots[slot + delta]
            if target not in matcher.L or matcher.L[target]["status"] != "todo":
                continue
            if not native_target["usable"]:
                if native_target["frontier"]:
                    entry = result.setdefault(target, dict(classification="missing", confidence="low",
                        our_definition="", evidence=proof + ["table-bound source body still stops/is partial"],
                        frontier_definitions=[]))
                    entry.setdefault("frontier_definitions", []).append(native_target["definition"])
                continue
            method_votes[target][native_target["key"]] = (native_target, proof + [
                "ordinary virtual slot %d -> %d; retail target %s" % (slot, slot + delta, hx(target))])
    for a, aliases in sorted(method_votes.items()):
        if result.get(a, {}).get("frontier_definitions"):
            continue
        entries = sorted(aliases.values(), key=lambda p: p[0]["definition"])
        result[a] = dict(classification="matched", confidence="high" if len(entries) == 1 else "medium",
                         native=entries[0][0]["native"],
                         our_definition=" | ".join(b["definition"] for b, _ in entries),
                         evidence=entries[0][1] + (["retail-folded entry has %d complete native definition aliases" %
                                                  len(entries)] if len(entries) > 1 else []))
    return result, used_tables
