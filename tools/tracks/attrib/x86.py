"""Conservative same-architecture functional attribution from a PDB-backed image.

The compilers differ (VC7.1 versus MSVC 2022). Instruction similarity generates
candidates; independently agreeing literals, layouts, constants and graph anchors
establish confidence. These pairings never establish a retail C++ prototype.
"""
import collections
import difflib
import json
import re

from classify import float_key, hx, literal
from extract import norm_name


def signature(f):
    """Register-independent displacements/slots and explicit return cleanup.

    Stack accesses, scaled array indexing and absolute addresses do not identify
    object fields. Offsets describe instructions, not asserted class layouts.
    """
    fields, slots, returns, immediates = set(), set(), set(), set()
    for _, m, o in f["insns"]:
        if m.startswith("ret"):
            returns.add(int(o, 0) if o else 0)
        if m == "call":
            match = re.fullmatch(r"dword ptr \[(e(?:ax|bx|cx|dx|si|di))(?: \+ (0x[0-9a-f]+))?\]", o)
            if match:
                slots.add(int(match[2], 16) if match[2] else 0)
        for reg, sign, value in re.findall(
                r"\[(e(?:ax|bx|cx|dx|si|di)) ([+-]) (0x[0-9a-f]+)\]", o):
            if m != "call":
                fields.add(int(value, 16) * (-1 if sign == "-" else 1))
        # Exclude relocations and address-like immediates. Keep small enum/mask
        # constants too; the older cross-architecture extractor excludes these.
        if m in ("push", "mov", "cmp", "test", "and", "or", "xor", "imul"):
            value = o.rsplit(", ", 1)[-1]
            if re.fullmatch(r"(?:0x[0-9a-f]+|[0-9]+)", value):
                v = int(value, 0)
                if 2 < v < 0x1000000 and v not in (0xcc, 0xcccc):
                    immediates.add(v)
    floats = {float_key(v) for v in f["floats"]} - {"0", "-0", "1", "-1", "2", "0.5"}
    return dict(fields=tuple(sorted(fields)), slots=tuple(sorted(slots)),
                returns=tuple(sorted(returns)), constants=tuple(sorted(immediates)),
                floats=tuple(sorted(floats)))


class X86Matcher:
    def __init__(self, data, matcher):
        self.data, self.matcher = data, matcher
        self.F, self.L = matcher.F, matcher.L
        self.G = {int(k, 16): v for k, v in data["functions"].items()}
        self.sources = matcher.sources
        self.pairs, self.votes, self.rejected = {}, collections.defaultdict(dict), {}
        self.seed_rejected = {}
        self.reverse = {}
        self.left = {a: signature(f) for a, f in self.F.items()}
        self.right = {b: signature(g) for b, g in self.G.items()}
        self.usable = {b for b, g in self.G.items() if g.get("source") is not None
                       and not g["name"].startswith(("std::", "__", "thunk "))}
        self.shared = ({s for f in self.F.values() for s in f["strings"] if literal(s)} &
                       {s for b in self.usable for s in self.G[b]["strings"] if literal(s)})
        self.seed()

    def label(self, b):
        g = self.G[b]
        d = self.sources[g["source"]]
        return "%s:%d %s [PDB %s]" % (d["file"], d["line"], g["name"], hx(b))

    def stopping(self, b):
        d = self.sources[self.G[b]["source"]]
        return d["stopping"] or bool(re.search(r"\b(?:frontier|abort)\s*\(", d["body"], re.I))

    def compatible(self, a, b):
        f, g = self.F[a], self.G[b]
        l, r = self.left[a], self.right[b]
        if l["returns"] and r["returns"] and l["returns"] != r["returns"]:
            return False, "explicit return-cleanup contradiction"
        if f["n"] <= 5 and (len(g["calls"]) > 1 or g["n"] > 40):
            return False, "retail leaf misclaimed as a multi-call body"
        s1, s2 = set(f["strings"]) & self.shared, set(g["strings"]) & self.shared
        if s1 and s2 and not s1 & s2:
            return False, "disjoint shared literals"
        r = self.matcher.rmap.get(a)
        if r and r["confidence"] == "high" and r["file_basis"] == "site":
            from pathlib import Path
            p = g["source_file"]
            original_names = {Path(v).name.lower() for v in g.get("documented_original_tus", [])}
            if (Path(p).suffix == ".cpp" and Path(p).name.lower() != Path(r["file"]).name.lower()
                    and Path(r["file"]).name.lower() not in original_names):
                return False, "rdebug own-site file contradiction"
        if f["n"] and g["n"] and not .08 <= (g["n"] + 8) / (f["n"] + 8) <= 15:
            return False, "extreme instruction-count mismatch"
        return True, ""

    def body_support(self, a, b, evidence):
        """Distinguish a complete body from a wrapper containing its one literal.

        This is an acceptance guard, not another independent confidence vote.
        Layout changes and inlining can make it abstain on a true identity.
        """
        if any(k.startswith("graph:table:") for k in evidence):
            return "equal-length table with at least three agreeing slot identities"
        strong = [k for k in evidence if k.startswith(("string:", "api:"))]
        if len(strong) >= 2:
            return "two distinct unique named anchors"
        l, r = self.left[a], self.right[b]
        offsets1, offsets2 = set(l["fields"]) | set(l["slots"]), set(r["fields"]) | set(r["slots"])
        if offsets1 and offsets1 == offsets2 and l["returns"] == r["returns"]:
            return "same nonempty displacement/slot sets and return cleanup"
        if len(offsets1 & offsets2) >= 3 and len(offsets1 & offsets2) / len(offsets1 | offsets2) >= .5:
            return "displacement/slot overlap %d/%d (boundary guard, not a confidence vote)" % (
                len(offsets1 & offsets2), len(offsets1 | offsets2))
        # Out-of-line typed slot/field helpers in the rebuilt image move some
        # accesses into callees. A large body can still retain a substantial
        # common footprint. Require many offsets and comparable body sizes;
        # a little wrapper with a subset of its callee's fields does not qualify.
        ratio = (self.G[b]["n"] + 8) / (self.F[a]["n"] + 8)
        if (strong and len(offsets1 & offsets2) >= 8 and .5 <= ratio <= 3 and
                len(offsets1 & offsets2) / len(offsets1 | offsets2) >= .3):
            return "large-body displacement/slot overlap %d/%d, instruction ratio %.3f (boundary guard only)" % (
                len(offsets1 & offsets2), len(offsets1 | offsets2), ratio)
        for key, minimum in (("constants", 3), ("floats", 2)):
            if l[key] == r[key] and len(l[key]) >= minimum:
                return "same informative " + key
        s1 = tuple(c for c in self.F[a]["calls"] if c in self.pairs and not self.pairs[c]["frontier"])
        s2 = tuple(self.reverse[c] for c in self.G[b]["calls"] if c in self.reverse)
        return "two or more ordered paired callee identities agree" if len(set(s1)) >= 2 and s1 == s2 else ""

    def refresh(self):
        reverse = collections.defaultdict(set)
        for a, p in self.pairs.items():
            if not p["frontier"]:
                reverse[p["x86"]].add(a)
        self.reverse = {b: next(iter(v)) for b, v in reverse.items() if len(v) == 1}

    def seed(self):
        bysource = collections.defaultdict(set)
        for b in self.usable:
            bysource[self.G[b]["source"]].add(b)
        for a, p in sorted(self.matcher.pairs.items()):
            if p["confidence"] == "medium":
                self.seed_rejected[(a, None)] = "round-9 medium identity is not a fresh propagation seed"
                continue  # retained in round 9, but too weak to be a fresh graph seed
            source = self.matcher.G[p["native"]]["source"]
            candidates = bysource[source]
            if len(candidates) == 1:
                b = next(iter(candidates))
                if self.seed_candidate(a, b):
                    self.pairs[a] = dict(x86=b, confidence=p["confidence"],
                                         evidence=["cited/round-9 source identity " + self.label(b)],
                                         seed=True, frontier=False)
        # Not every cited definition is emitted in the macOS archive. The PDB
        # supplies independent source identity for those Windows-only anchors.
        byname = collections.defaultdict(set)
        for b in self.usable:
            d = self.sources[self.G[b]["source"]]
            byname[(d["file"], norm_name(d["name"]).split("::")[-1])].add(b)
        for a, row in sorted(self.L.items()):
            if a in self.pairs or row["status"] not in ("done", "chosen", "stop"):
                continue
            wanted = norm_name(row["symbol"])
            options = {b for b in byname[(row["file"], wanted.split("::")[-1])]
                       if norm_name(self.sources[self.G[b]["source"]]["name"]).endswith(wanted)}
            if len(options) == 1:
                b = next(iter(options))
                if self.seed_candidate(a, b):
                    self.pairs[a] = dict(x86=b, confidence="cited", seed=True, frontier=False,
                                         evidence=["input cited definition plus PDB file/name/line " + self.label(b)])
        self.refresh()

    def seed_candidate(self, a, b):
        ok, why = self.compatible(a, b)
        if ok and self.stopping(b):
            ok, why = False, "source body stops or is partial"
        if ok and not self.seed_support(a, b):
            ok, why = False, "large retail string-bearing body misclaimed as a tiny helper"
        if not ok:
            self.seed_rejected[(a, b)] = why
        return ok

    def seed_support(self, a, b):
        f, g = self.F[a], self.G[b]
        # Large retail business-string bodies cannot seed tiny current helpers
        # merely because an attached comment mentions their caller's address.
        return not (f["strings"] and not (set(g["strings"]) & self.shared) and
                    f["n"] >= 40 and g["n"] < f["n"] / 4)

    def vote(self, a, b, key, description):
        if (a in self.pairs or a not in self.L or self.L[a]["status"] != "todo" or
                b not in self.usable or b in self.reverse):
            return
        self.votes[(a, b)][key] = description

    def accept(self):
        candidates, reverse = collections.defaultdict(list), collections.defaultdict(set)
        for (a, b), evidence in sorted(self.votes.items()):
            if a in self.pairs or b in self.reverse:
                continue
            ok, why = self.compatible(a, b)
            if not ok:
                self.rejected[(a, b)] = why
                continue
            candidates[a].append((b, evidence))
            reverse[b].add(a)
        added = 0
        for a, options in sorted(candidates.items()):
            if len(options) != 1:
                continue
            b, evidence = options[0]
            if len(reverse[b]) != 1:
                continue
            support = self.body_support(a, b, evidence)
            if not support:
                self.rejected[(a, b)] = "wrapper/body boundary lacks corroboration"
                continue
            # Layout/slots/retn share one signature family; repeated propagation
            # from a single parent shares one graph family. They are not three
            # independent proofs simply because there are three fields.
            families = {k.split(":", 1)[0] for k in evidence}
            strong = [k for k in evidence if k.startswith(("string:", "api:"))]
            high = len(families) >= 2 or len(strong) >= 2
            self.pairs[a] = dict(x86=b, confidence="high" if high else "medium",
                                 evidence=[evidence[k] for k in sorted(evidence)], seed=False,
                                 body_support=support,
                                 frontier=self.stopping(b))
            self.rejected.pop((a, b), None)
            added += 1
        self.refresh()
        return added

    def anchors(self):
        for field, pred, kind in (("strings", literal, "string"),
                                  ("apis", lambda s: s.startswith(("D3D", "D3DX")), "api")):
            left, right = collections.defaultdict(set), collections.defaultdict(set)
            for a, f in self.F.items():
                if field == "apis" and self.import_leaf(f):
                    continue  # the API thunk is not another business-body user
                for v in set(f[field]):
                    if pred(v):
                        left[v].add(a)
            for b in sorted(self.G):
                if field == "apis" and self.import_leaf(self.G[b]):
                    continue
                for v in set(self.G[b][field]):
                    if pred(v):
                        right[v].add(b)
            for v in sorted(left.keys() & right.keys()):
                if len(left[v]) == len(right[v]) == 1:
                    self.vote(next(iter(left[v])), next(iter(right[v])), kind + ":" + v,
                              "x86 unique %s %s" % (kind, json.dumps(v)))
        # Corroboration runs before acceptance, so two agreeing independent
        # anchors can raise confidence even for a first-pass unique literal.
        self.corroborate()
        return self.accept()

    @staticmethod
    def import_leaf(f):
        return (len(f["insns"]) == 1 and f["insns"][0][1] == "jmp" and
                bool(re.fullmatch(r"dword ptr \[0x[0-9a-f]+\]", f["insns"][0][2])) and
                len(f.get("imports", [])) == 1)

    def signatures(self):
        for kind in ("layout", "constants", "floats"):
            left, right = collections.defaultdict(set), collections.defaultdict(set)
            for values, target, entries in ((self.left, left, self.F),
                                             (self.right, right, self.G)):
                for a in entries:
                    s = values[a]
                    if kind == "layout":
                        sig = (s["fields"], s["slots"], s["returns"])
                        informative = len(s["fields"]) + len(s["slots"]) >= 3
                    else:
                        sig = s[kind]
                        informative = len(sig) >= (3 if kind == "constants" else 2)
                    if informative:
                        target[sig].add(a)
            for sig in sorted(left.keys() & right.keys()):
                if len(left[sig]) == len(right[sig]) == 1:
                    self.vote(next(iter(left[sig])), next(iter(right[sig])), kind + ":unique",
                              "x86 unique %s signature %s" % (kind, sig))
        self.corroborate()
        return self.accept()

    def corroborate(self):
        for a, b in list(self.votes):
            l, r = self.left[a], self.right[b]
            if (l["fields"], l["slots"], l["returns"]) == (r["fields"], r["slots"], r["returns"]) and (
                    len(l["fields"]) + len(l["slots"]) >= 3):
                self.vote(a, b, "layout:agree", "x86 agreeing displacement/virtual-slot/retn signature " +
                          str((l["fields"], l["slots"], l["returns"])))
            for key, minimum in (("constants", 3), ("floats", 2)):
                if l[key] == r[key] and len(l[key]) >= minimum:
                    self.vote(a, b, key + ":agree", "x86 agreeing %s %s" % (key, l[key]))

    def calls(self):
        for a, p in sorted(list(self.pairs.items())):
            if p["frontier"]:
                continue
            s1, s2 = self.F[a]["calls"], self.G[p["x86"]]["calls"]
            t1 = [(i, c) for i, c in enumerate(s1) if c in self.pairs and not self.pairs[c]["frontier"]]
            t2 = [(i, self.reverse[c]) for i, c in enumerate(s2) if c in self.reverse]
            sm = difflib.SequenceMatcher(a=[v for _, v in t1], b=[v for _, v in t2], autojunk=False)
            aligned = [(t1[block.a + j][0], t2[block.b + j][0])
                       for block in sm.get_matching_blocks() for j in range(block.size)]
            for (l1, l2), (h1, h2) in zip(aligned, aligned[1:]):
                u1 = {c for c in s1[l1 + 1:h1] if c not in self.pairs}
                u2 = {c for c in s2[l2 + 1:h2] if c not in self.reverse and c in self.usable}
                if len(u1) == len(u2) == 1:
                    self.vote(next(iter(u1)), next(iter(u2)), "graph:" + hx(a),
                              "x86 single callee gap in paired caller %s between %s and %s" %
                              (hx(a), hx(s1[l1]), hx(s1[h1])))
        left, right = collections.defaultdict(set), collections.defaultdict(set)
        for a, f in self.F.items():
            sig = tuple(c for c in f["calls"] if c in self.pairs and not self.pairs[c]["frontier"])
            if len(set(sig)) >= 2:
                left[sig].add(a)
        for b in sorted(self.usable):
            sig = tuple(self.reverse[c] for c in self.G[b]["calls"] if c in self.reverse)
            if len(set(sig)) >= 2:
                right[sig].add(b)
        for sig in sorted(left.keys() & right.keys()):
            if len(left[sig]) == len(right[sig]) == 1:
                self.vote(next(iter(left[sig])), next(iter(right[sig])), "graph:callees",
                          "x86 unique ordered paired callees " + ",".join(hx(c) for c in sig))
        callers = collections.defaultdict(set)
        for b, g in self.G.items():
            for c in set(g["calls"]):
                callers[c].add(b)
        for ca, p in sorted(list(self.pairs.items())):
            if p["frontier"]:
                continue
            c1, c2 = set(self.F[ca]["callers"]), callers[p["x86"]]
            u1 = {a for a in c1 if a not in self.pairs and a in self.L and self.L[a]["status"] == "todo"}
            u2 = {b for b in c2 if b not in self.reverse and b in self.usable}
            k1 = {a for a in c1 if a in self.pairs and not self.pairs[a]["frontier"]}
            k2 = {self.reverse[b] for b in c2 if b in self.reverse}
            if len(u1) == len(u2) == 1 and k1 == k2:
                self.vote(next(iter(u1)), next(iter(u2)), "graph:caller:" + hx(ca),
                          "x86 single unmatched caller of %s; agreeing known caller sets %s" %
                          (hx(ca), ",".join(hx(a) for a in sorted(k1))))
        self.corroborate()
        return self.accept()

    def tables(self):
        left, right = self.matcher.data["retail"]["vtables"], self.data["vtables"]
        candidates = collections.defaultdict(list)
        for va, slots in sorted(left.items()):
            for vb, other in sorted(right.items()):
                if len(slots) != len(other):
                    continue
                agree = {a for a, b in zip(slots, other) if a in self.pairs and
                         not self.pairs[a]["frontier"] and self.pairs[a]["x86"] == b}
                if len(agree) < 3:
                    continue
                if any(a in self.pairs and not self.pairs[a]["frontier"] and b in self.reverse and
                       self.pairs[a]["x86"] != b for a, b in zip(slots, other)):
                    continue
                candidates[va].append((vb, agree))
        for va, options in sorted(candidates.items()):
            if len(options) != 1:
                continue
            vb, agree = options[0]
            for slot, (a, b) in enumerate(zip(left[va], right[vb])):
                self.vote(a, b, "graph:table:" + va, "x86 equal-length table %s -> %s slot %d; anchors %s" %
                          (va, vb, slot, ",".join(hx(c) for c in sorted(agree))))
        self.corroborate()
        return self.accept()

    def run(self):
        print("x86 cited/round-9 anchors:", len(self.pairs), flush=True)
        print("x86 unique strings/imports:", self.anchors(), flush=True)
        print("x86 signatures:", self.signatures(), flush=True)
        for i in range(12):
            counts = (self.tables(), self.calls())
            print("x86 propagation %d: tables=%d calls=%d" % (i + 1, *counts), flush=True)
            if not sum(counts):
                break
        return self.pairs

    def apply(self, records):
        candidates = collections.defaultdict(list)
        for (a, b), votes in sorted(self.votes.items()):
            candidates[a].append(dict(definition=self.label(b), x86=hx(b),
                                      anchors=[votes[k] for k in sorted(votes)],
                                      rejected=self.rejected.get((a, b), "ambiguous/unaccepted"),
                                      frontier=self.stopping(b)))
        for r in records:
            a = int(r["address"], 16)
            p = self.pairs.get(a)
            if not p:
                if candidates[a]:
                    r["evidence"]["x86_review_candidates"] = candidates[a]
                continue
            g = self.G[p["x86"]]
            r["evidence"]["x86_pair"] = dict(p, definition=self.label(p["x86"]),
                                                retail_signature=self.left[a],
                                                x86_signature=self.right[p["x86"]],
                                                resolved_import_calls=g.get("resolved_import_calls", []),
                                                source_maps=g.get("source_maps", []),
                                                documented_original_tus=g.get("documented_original_tus", []),
                                                aliases=g.get("aliases", []))
            if p["frontier"]:
                r["evidence"].setdefault("known_source_frontiers", []).append(self.label(p["x86"]))
            elif r["classification"] in ("missing", "editor"):
                r.update(classification="matched", confidence=p["confidence"], our_definition=self.label(p["x86"]))
                r["evidence"]["anchors"] = p["evidence"]
                r["probable_file"] = g["source_file"]
                r["file_basis"] = "PDB file/line plus accepted retail/x86 function pairing"
                r["evidence"].update(probable_file=r["probable_file"], file_basis=r["file_basis"])
