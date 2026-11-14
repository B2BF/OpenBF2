#!/usr/bin/env python3
"""Classify every todo RendDX9 row using cross-architecture and optional x86 anchors.

Usage: python3 tools/tracks/attrib/classify.py --features out/attrib/features.json
  --csv docs/tracks/rend/attribution.csv --report docs/tracks/rend/attribution.md
Never changes the input ledger or source. Ambiguous candidates stay missing, with
their evidence available for review. The feature cache pins the archive and sources.
"""
import argparse
import collections
import csv
import difflib
import hashlib
import json
import os
from pathlib import Path
import re

from extract import REPO, digest, norm_name, source_digest, source_paths


def rows(path):
    csv.field_size_limit(16 * 1024 * 1024)
    with path.open(newline="") as stream:
        return list(csv.DictReader(stream))


def hx(address):
    return "0x%08X" % address


def literal(s):
    return (len(s) >= 5 and not re.search(r"(?:code[\\/]+bf2[\\/]|\[FRONTIER\]|\[CHOSEN)", s, re.I)
            and s not in {"false", "true", "basic_string", "vector<T> too long", "string too long",
                          "invalid vector<T> subscript", "invalid string position"})


def float_key(value):
    # x87 double loads and native SSE float constants may differ in the last bits.
    return "%.6g" % value


class Matcher:
    def __init__(self, data, ledger, rmap):
        self.data, self.ledger = data, ledger
        self.F = {int(k, 16): v for k, v in data["retail"]["functions"].items()}
        self.G = data["native"]["functions"]
        self.sources = data["sources"]
        self.L = {int(r["address"], 16): r for r in ledger if r["image"] == "RendDX9.dll"}
        self.rmap = {int(r["addr_150"], 16): r for r in rmap}
        self.pairs, self.votes, self.rejected = {}, collections.defaultdict(dict), {}
        self.native_groups = collections.defaultdict(set)
        for b, g in self.G.items():
            # D1/D2 aliases and inline copies in different objects share one source
            # identity; otherwise an ambiguous native symbol cannot be paired.
            key = self.group(b)
            self.native_groups[key].add(b)
        self.reverse = {}
        self.native_calls = {}
        for b, g in self.G.items():
            called = []
            for sym in g["calls"]:
                targets = data["native"]["symbols"].get(sym, [])
                local = [t for t in targets if self.G[t].get("object") == g.get("object")]
                if local:
                    targets = local
                groups = {self.group(t) for t in targets}
                if len(groups) == 1:
                    called.append(next(iter(groups)))
            self.native_calls[b] = called
        self.shared_strings = (set(s for f in self.F.values() for s in f["strings"] if literal(s)) &
                               set(s for g in self.G.values() for s in g["strings"] if literal(s)))
        self.seed()

    def group(self, b):
        g = self.G[b]
        # Same source overloads are still distinct native signatures.
        return (g["source_file"], g["name"])

    def usable(self, b):
        g = self.G[b]
        if g["source"] is None or g["name"].startswith(("std::", "__", "non-virtual thunk")):
            return False
        d = self.sources[g["source"]]
        if d["stopping"] or re.search(r"\b(?:frontier|abort)\s*\(", d["body"], re.I):
            return False
        return True

    def source_label(self, b):
        g = self.G[b]
        if g["source"] is not None:
            d = self.sources[g["source"]]
            return "%s:%d %s%s" % (d["file"], d["line"], g["name"],
                                      " [" + d["generated_macro"] + " expansion]" if d.get("generated_macro") else "")
        return "%s %s [source association unresolved]" % (g["source_file"] or g["object"], g["name"])

    def seed(self):
        index = collections.defaultdict(list)
        for b, g in self.G.items():
            index[(g["source_file"], norm_name(g["name"]).split("::")[-1])].append(b)
        for a, r in sorted(self.L.items()):
            if r["status"] not in ("done", "stop", "chosen"):
                continue
            wanted = norm_name(r["symbol"])
            cand = [b for b in index[(r["file"], wanted.split("::")[-1])]
                    if norm_name(self.G[b]["name"]).endswith(wanted)]
            groups = {self.group(b) for b in cand}
            if len(groups) == 1:
                b = min(cand)
                f, g = self.F[a], self.G[b]
                # Some ledger comments claim a CRT/import helper as a game
                # method. Keep the ledger untouched, but do not propagate it.
                if f["n"] <= 5 and f["imports"] and g["n"] > 25:
                    continue
                if f["strings"] and not g["strings"] and f["n"] >= 25 and g["n"] <= 15:
                    continue  # metadata constructor comments sometimes cite it beside a tiny property callback
                if f["n"] and g["n"] and not 0.08 <= (g["n"] + 8) / (f["n"] + 8) <= 15:
                    continue
                self.pairs[a] = dict(native=b, confidence="cited", evidence=["ledger definition"], seed=True)
        self.refresh_reverse()

    def refresh_reverse(self):
        groups = collections.defaultdict(set)
        for a, p in self.pairs.items():
            groups[self.group(p["native"])].add(a)
        # Retail folding and source aliases can produce many-to-one seeds. These
        # stay anchors, but cannot serve as a unique call-list token.
        self.reverse = {g: next(iter(v)) for g, v in groups.items() if len(v) == 1}

    def vote(self, a, b, key, description):
        if a in self.pairs or a not in self.L or self.L[a]["status"] != "todo" or not self.usable(b):
            return
        if self.group(b) in self.reverse:
            return
        self.votes[(a, self.group(b))][key] = description

    def compatible(self, a, b):
        f, g = self.F[a], self.G[b]
        s1, s2 = set(f["strings"]) & self.shared_strings, set(g["strings"]) & self.shared_strings
        if s1 and s2 and not (s1 & s2):
            return False, "disjoint shared strings"
        # A reliable _r own-file site is a useful veto, not a function-identity anchor.
        r = self.rmap.get(a)
        if r and r["confidence"] == "high" and r["file_basis"] == "site" and g["source_file"]:
            if Path(r["file"]).name.lower() != Path(g["source_file"]).name.lower():
                # Recovered inl/header splits are not a file contradiction.
                if Path(g["source_file"]).suffix not in (".h", ".inl", ".hpp"):
                    return False, "rdebug own-site file contradicts source"
        if f["n"] and g["n"] and not 0.08 <= (g["n"] + 8) / (f["n"] + 8) <= 15:
            return False, "extreme instruction-count mismatch"
        return True, ""

    def accept(self):
        candidates = collections.defaultdict(list)
        bygroup = collections.defaultdict(set)
        for (a, group), evidence in sorted(self.votes.items()):
            if a in self.pairs or group in self.reverse:
                continue
            b = min(self.native_groups[group])
            if not self.usable(b):
                b = next((v for v in sorted(self.native_groups[group]) if self.usable(v)), b)
            ok, why = self.compatible(a, b)
            if not ok:
                self.rejected[(a, group)] = why
                continue
            candidates[a].append((b, evidence))
            bygroup[group].add(a)
        added = 0
        for a, options in sorted(candidates.items()):
            if len(options) != 1:
                continue
            b, evidence = options[0]
            if len(bygroup[self.group(b)]) != 1:
                continue
            # Multiple literals count independently; slot and call propagation
            # from the same parent are not two independent anchors.
            kinds = {key.split(":", 1)[0] for key in evidence}
            strong = [k for k in evidence if k.startswith(("string:", "api:", "address:"))]
            confidence = "high" if len(kinds) >= 2 or len(strong) >= 2 else "medium"
            self.pairs[a] = dict(native=b, confidence=confidence,
                                 evidence=[evidence[k] for k in sorted(evidence)], seed=False)
            added += 1
        self.refresh_reverse()
        return added

    def anchor_pass(self):
        # The checked-in ledger can predate source recovery. Reuse its own
        # attached-comment convention, without changing a single ledger status.
        # These are reported as newly detected citations, rather than pretending
        # their PE comments are absent.
        import rend_ledger as rl
        image_functions = {"RendDX9.dll": self.F, "BF2.exe": {}}
        for b, g in sorted(self.G.items()):
            if not self.usable(b):
                continue
            d = self.sources[g["source"]]
            if "[CHOSEN, not recovered]" in d["comment"]:
                continue
            cites = rl.addresses(d["comment"], image_functions)
            if cites:
                self.vote(cites[0][1], b, "address:source", "definition's own attached PE citation " +
                          hx(cites[0][1]))
        for field, pred in (("strings", literal), ("apis", lambda s: s.startswith(("D3D", "D3DX")))):
            left, right = collections.defaultdict(set), collections.defaultdict(set)
            for a, f in self.F.items():
                for value in set(f[field]):
                    if pred(value):
                        left[value].add(a)
            for b, g in self.G.items():
                if not self.usable(b):
                    continue
                for value in set(g[field]):
                    if pred(value):
                        right[value].add(self.group(b))
            for value in sorted(left.keys() & right.keys()):
                if len(left[value]) == len(right[value]) == 1:
                    a, group = next(iter(left[value])), next(iter(right[value]))
                    b = min(self.native_groups[group])
                    key = "string" if field == "strings" else "api"
                    self.vote(a, b, key + ":" + value, "unique %s %s" % (key, json.dumps(value)))
        # A source's chosen sub_ADDRESS name records a direct address association.
        # This doesn't bypass the complete-body or compatibility checks.
        for b, g in sorted(self.G.items()):
            for m in re.finditer(r"(?:sub_|FUN_)(10[0-9a-fA-F]{6})\b", norm_name(g["name"])):
                a = int(m[1], 16)
                if a in self.F:
                    self.vote(a, b, "address:name", "native definition name carries " + hx(a))
        return self.accept()

    def corroborate(self):
        for (a, group), evidence in list(self.votes.items()):
            if a in self.pairs:
                continue
            b = min(self.native_groups[group])
            f, g = self.F[a], self.G[b]
            floats = sorted({float_key(v) for v in f["floats"]} & {float_key(v) for v in g["floats"]})
            informative = [v for v in floats if v not in ("0", "-0", "1", "-1", "2", "0.5")]
            if len(informative) >= 2:
                self.vote(a, b, "float:set", "agreeing nontrivial float constants " + ",".join(informative))
            consts = sorted(set(f["consts"]) & set(g["consts"]))
            if len(consts) >= 2:
                self.vote(a, b, "constant:set", "agreeing integer constants " + ",".join(hex(v) for v in consts))
        return self.accept()

    def call_pass(self):
        # Align only already-paired tokens, retaining the unresolved direct calls
        # between them. libc++/VC7.1 internals and imported calls are not assumed equal.
        for a, pair in sorted(list(self.pairs.items())):
            b = pair["native"]
            seq1, seq2 = self.F[a]["calls"], self.native_calls[b]
            tok1 = [(i, c) for i, c in enumerate(seq1) if c in self.pairs]
            tok2 = [(i, self.reverse[c]) for i, c in enumerate(seq2) if c in self.reverse]
            if not tok1 or not tok2:
                continue
            sm = difflib.SequenceMatcher(a=[c for _, c in tok1], b=[c for _, c in tok2], autojunk=False)
            align = []
            for block in sm.get_matching_blocks():
                for j in range(block.size):
                    align.append((tok1[block.a + j][0], tok2[block.b + j][0]))
            for (l1, l2), (h1, h2) in zip(align, align[1:]):
                unknown1 = [c for c in seq1[l1 + 1:h1] if c not in self.pairs]
                unknown2 = [c for c in seq2[l2 + 1:h2] if c not in self.reverse and
                            any(self.usable(t) for t in self.native_groups[c])]
                if len(set(unknown1)) == len(set(unknown2)) == 1:
                    ca, group = unknown1[0], unknown2[0]
                    cb = min(t for t in self.native_groups[group] if self.usable(t))
                    self.vote(ca, cb, "call-gap:" + hx(a), "single unmatched callee between " +
                              hx(seq1[l1]) + " and " + hx(seq1[h1]) + " in paired caller " + hx(a))
        # Unique sequences of >=2 paired callees: preserves order, tolerates
        # compiler-specific allocator/exception helper calls between the anchors.
        left, right = collections.defaultdict(set), collections.defaultdict(set)
        for a, f in self.F.items():
            if a not in self.pairs:
                sig = tuple(c for c in f["calls"] if c in self.pairs)
                if len(set(sig)) >= 2:
                    left[sig].add(a)
        for b in self.G:
            if self.usable(b) and self.group(b) not in self.reverse:
                sig = tuple(self.reverse[c] for c in self.native_calls[b] if c in self.reverse)
                if len(set(sig)) >= 2:
                    right[sig].add(self.group(b))
        for sig in sorted(left.keys() & right.keys()):
            if len(left[sig]) == len(right[sig]) == 1:
                a, group = next(iter(left[sig])), next(iter(right[sig]))
                b = min(t for t in self.native_groups[group] if self.usable(t))
                self.vote(a, b, "callee-sequence:exact", "unique ordered paired callees " +
                          ",".join(hx(c) for c in sig))
        # Reverse propagation: a callee with one remaining caller on each side,
        # after all already-paired callers agree. Useful for console binding
        # wrappers whose only non-library callee is the bound engine method.
        native_callers = collections.defaultdict(set)
        for b, called in self.native_calls.items():
            for group in set(called):
                native_callers[group].add(self.group(b))
        for ca, pair in sorted(list(self.pairs.items())):
            group = self.group(pair["native"])
            callers1 = set(self.F[ca]["callers"])
            callers2 = native_callers[group]
            unresolved1 = [a for a in callers1 if a not in self.pairs and
                           a in self.L and self.L[a]["status"] == "todo"]
            unresolved2 = [g for g in callers2 if g not in self.reverse and
                           any(self.usable(b) for b in self.native_groups[g])]
            known1 = {a for a in callers1 if a in self.pairs}
            known2 = {self.reverse[g] for g in callers2 if g in self.reverse}
            if len(unresolved1) == len(unresolved2) == 1 and known1 == known2:
                cb = min(b for b in self.native_groups[unresolved2[0]] if self.usable(b))
                self.vote(unresolved1[0], cb, "caller:" + hx(ca),
                          "single unmatched caller of paired callee " + hx(ca) +
                          "; already-paired caller sets agree " + ",".join(hx(c) for c in sorted(known1)))
        return self.accept()

    def vtable_pass(self):
        symbols = self.data["native"]["symbols"]
        for table in self.data["native"]["tables"]:
            # Genuine Itanium vtables have a different destructor slot convention.
            # Only the explicit MSVC-order adapter arrays preserve retail indices.
            if "vtable for " in table["name"] or "VTT for " in table["name"]:
                continue
            slots = {}
            for slot, sym in table["slots"]:
                targets = symbols.get(sym, [])
                local = [t for t in targets if self.G[t].get("object") == table.get("object")]
                groups = {self.group(t) for t in (local or targets)}
                if len(groups) == 1:
                    slots[slot] = next(iter(groups))
            tied = []
            for va, retail_slots in self.data["retail"]["vtables"].items():
                agreeing = {self.reverse[group] for slot, group in slots.items()
                            if group in self.reverse and slot < len(retail_slots) and
                            retail_slots[slot] == self.reverse[group]}
                conflict = any(group in self.reverse and slot < len(retail_slots) and
                               retail_slots[slot] in self.pairs and
                               retail_slots[slot] != self.reverse[group] for slot, group in slots.items())
                if len(agreeing) >= 3 and not conflict and max(slots, default=-1) + 1 == len(retail_slots):
                    tied.append((va, retail_slots, agreeing))
            if len(tied) != 1:
                continue
            va, retail_slots, agreeing = tied[0]
            for slot, group in slots.items():
                b = next((v for v in sorted(self.native_groups[group]) if self.usable(v)), None)
                if b is not None:
                    self.vote(retail_slots[slot], b, "vtable:" + va,
                              "explicit adapter table %s slot %d = retail %s; %d agreeing cited/paired slots" %
                              (table["name"], slot, va, len(agreeing)))
        return self.accept()

    def cpp_vtable_pass(self):
        """Tie a C++ table only through a paired owner and a measured slot delta.

        This is distinct from the explicit adapter-array pass: both the retail
        owner's immediate table reference and the native owner's relocation must
        agree, then three ordinary method identities must establish one delta.
        """
        retail_tables = {int(k, 16): v for k, v in self.data["retail"]["vtables"].items()}
        byname = collections.defaultdict(list)
        for t in self.data["native"]["tables"]:
            if t["name"].startswith("vtable for ") and "std::" not in t["name"]:
                byname[t["name"]].append(t)
        candidates = {}
        for owner, p in sorted(self.pairs.items()):
            g = self.G[p["native"]]
            if not g.get("vtrefs"):
                continue
            references = {int(h, 16) for _, _, o in self.F[owner]["insns"]
                          for h in re.findall(r"0x[0-9a-f]+", o) if int(h, 16) in retail_tables}
            for name in g["vtrefs"]:
                options = byname.get(name, [])
                local_tables = [t for t in options if t["object"] == g.get("object")]
                for t in local_tables or options:
                    slots = {}
                    for slot, symbol in t["slots"]:
                        ids = self.data["native"]["symbols"].get(symbol, [])
                        local = [b for b in ids if self.G[b].get("object") == t["object"]]
                        groups = {self.group(b) for b in local or ids}
                        if len(groups) == 1:
                            slots[slot] = next(iter(groups))
                    for va in sorted(references):
                        rt = retail_tables[va]
                        votes = collections.defaultdict(set)
                        for ri, target in enumerate(rt):
                            pair = self.pairs.get(target)
                            if not pair or "::~" in self.G[pair["native"]]["name"]:
                                continue
                            group = self.group(pair["native"])
                            ns = [slot for slot, value in slots.items() if value == group]
                            if len(ns) == 1:
                                votes[ns[0] - ri].add(target)
                        if len(votes) != 1:
                            continue
                        delta, anchors = next(iter(votes.items()))
                        raw_count = max((slot for slot, _ in t["slots"]), default=-1) + 1
                        if len(anchors) < 3 or len(rt) + delta != raw_count:
                            continue
                        conflict = False
                        for ri, target in enumerate(rt):
                            pair = self.pairs.get(target)
                            if pair and "::~" not in self.G[pair["native"]]["name"] and ri + delta in slots:
                                if slots[ri + delta] != self.group(pair["native"]):
                                    conflict = True
                        if not conflict:
                            key = (va, t["object"], name, delta)
                            candidates[key] = (owner, slots, anchors)
        # A retail table tied to several native classes can be an ICF alias. It
        # needs explicit alias review rather than a link-order tie breaker.
        owners = collections.Counter(key[0] for key in candidates)
        for (va, object_name, name, delta), (owner, slots, anchors) in sorted(candidates.items()):
            if owners[va] != 1:
                continue
            for ri, target in enumerate(retail_tables[va]):
                group = slots.get(ri + delta)
                if group is None:
                    continue
                b = next((b for b in sorted(self.native_groups[group]) if self.usable(b) and
                          "::~" not in self.G[b]["name"]), None)
                if b:
                    self.vote(target, b, "cpp-table:" + hx(va),
                              "paired owner %s references retail %s and installs %s; measured ordinary-slot delta %d "
                              "from %s; full lengths agree; slot %d->%d" %
                              (hx(owner), hx(va), name, delta, ",".join(hx(a) for a in sorted(anchors)), ri, ri + delta))
        return self.accept()

    def run(self):
        print("cited native anchors:", len(self.pairs), flush=True)
        print("unique anchor pass:", self.anchor_pass(), flush=True)
        for i in range(12):
            counts = [self.vtable_pass(), self.cpp_vtable_pass(), self.call_pass(), self.corroborate()]
            print("propagation %d: adapter=%d cpp-table=%d calls=%d corroboration=%d" % (i + 1, *counts), flush=True)
            if not sum(counts):
                break
        return self.pairs


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--features", type=Path, default=REPO / "out/attrib/features.json")
    ap.add_argument("--ledger", type=Path, default=REPO / "docs/tracks/rend/ledger.csv")
    ap.add_argument("--rdebug-map", type=Path, default=REPO / "docs/tracks/rend/rdebug-map.csv")
    ap.add_argument("--pairs", type=Path, default=REPO / "out/attrib/pairs.json")
    ap.add_argument("--x86-features", type=Path,
                    help="optional PDB-backed i386 cache from x86_extract.py")
    ap.add_argument("--csv", type=Path, default=REPO / "docs/tracks/rend/attribution.csv")
    ap.add_argument("--report", type=Path, default=REPO / "docs/tracks/rend/attribution.md")
    ap.add_argument("--evidence-root", type=Path,
                    default=Path(os.environ.get("REND_EVIDENCE_ROOT", REPO)))
    ap.add_argument("--image", type=Path, default=Path(os.environ.get("REND_GAME_DIR", str(Path.home() /
                    "Applications/Battlefield 2.app/Contents/SharedSupport/prefix/drive_c/Battlefield 2"))) /
                    "RendDX9.dll")
    a = ap.parse_args()
    data = json.loads(a.features.read_text())
    if hashlib.sha256(a.ledger.read_bytes()).hexdigest() != data["inputs"]["ledger_sha256"]:
        ap.error("ledger changed since feature extraction; regenerate features")
    if source_digest(source_paths()) != data["inputs"]["source_sha256"]:
        ap.error("sources changed since feature extraction; regenerate features")
    checks = ((a.image, "retail_sha256"), (a.rdebug_map, "rdebug_map_sha256"),
              (a.evidence_root / "out/rend-evidence/RendDX9.s", "retail_listing_sha256"))
    for path, key in checks:
        if key in data["inputs"] and digest(path) != data["inputs"][key]:
            ap.error("input changed since feature extraction: " + str(path))
    matcher = Matcher(data, rows(a.ledger), rows(a.rdebug_map))
    pairs = matcher.run()
    print("new matched:", sum(not p["seed"] for p in pairs.values()))
    from rules import RuntimeRules
    from secondary import console_bindings, editor, inlined
    from report import records, write_csv, write_report
    runtime = RuntimeRules(data, matcher.ledger, a.image, a.evidence_root / "out/rend-evidence/RendDX9.s")
    runtime.run()
    console, tables = console_bindings(data, matcher, runtime)
    print("console classes: %d tied tables, %d body attributions" % (len(tables), len(console)), flush=True)
    for address, result in console.items():
        if result["classification"] == "matched" and result["confidence"] == "high" and result.get("native"):
            matcher.pairs[address] = dict(native=result["native"], confidence=result["confidence"],
                                          evidence=result["evidence"], seed=False)
    matcher.refresh_reverse()
    for i in range(4):
        count = matcher.call_pass()
        if not count:
            break
        print("post-console call propagation %d: %d" % (i + 1, count), flush=True)
    scope = editor(data, matcher.ledger, matcher.rmap, matcher)
    scope.update(inlined(data, matcher))
    scope.update(console)
    classified = records(data, matcher.ledger, matcher, runtime, scope)
    x86 = None
    if a.x86_features:
        from x86 import X86Matcher
        xdata = json.loads(a.x86_features.read_text())
        if xdata["inputs"]["features_sha256"] != digest(a.features):
            ap.error("baseline features changed since x86 extraction; regenerate x86 features")
        x86 = X86Matcher(xdata, matcher)
        x86.run()
        x86.apply(classified)
        data["x86_summary"] = dict(xdata.get("summary", {}), seeds=sum(p["seed"] for p in x86.pairs.values()),
                                   seed_rejections=dict(collections.Counter(x86.seed_rejected.values())))
        data["inputs"].update({"x86_" + k: v for k, v in xdata["inputs"].items()})
    expected = {r["address"] for r in matcher.ledger if r["image"] == "RendDX9.dll" and r["status"] == "todo"}
    if {r["address"] for r in classified} != expected or len(classified) != len(expected):
        ap.error("attribution does not exactly cover the input RendDX9 todo rows")
    write_csv(a.csv, classified)
    write_report(a.report, a.csv, classified, data, matcher, runtime, tables)
    a.pairs.parent.mkdir(parents=True, exist_ok=True)
    saved = dict(pairs={hx(k): v for k, v in sorted(matcher.pairs.items())}, console_tables=tables,
                 votes=[dict(address=hx(address), group=list(group), evidence=v,
                             rejected=matcher.rejected.get((address, group), ""))
                        for (address, group), v in sorted(matcher.votes.items())])
    if x86:
        saved["x86_pairs"] = {hx(k): v for k, v in sorted(x86.pairs.items())}
        saved["x86_seed_rejections"] = [dict(address=hx(ad), x86=hx(b) if b is not None else None,
                                              reason=why)
                                         for (ad, b), why in sorted(x86.seed_rejected.items(),
                                             key=lambda item: (item[0][0], item[0][1] or -1))]
        saved["x86_votes"] = [dict(address=hx(ad), x86=hx(b), evidence=v,
                                    rejected=x86.rejected.get((ad, b), ""))
                               for (ad, b), v in sorted(x86.votes.items())]
    a.pairs.write_text(json.dumps(saved, sort_keys=True, indent=2) + "\n")
    print("attribution:", ", ".join("%s=%d" % (k, v) for k, v in sorted(
        collections.Counter(r["classification"] for r in classified).items())))
    print("CSV:", a.csv)
    print("report:", a.report)


if __name__ == "__main__":
    main()
