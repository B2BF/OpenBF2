#!/usr/bin/env python3
"""Generate the rend track function ledger (docs/tracks/rend/ledger.csv) and its summary.

Rows: every function Ghidra finds in retail RendDX9.dll (PE, no symbols), plus the BF2.exe functions
that a definition under the rend paths cites by a PE address (Scene, Geom, RendMisc code that lives
in the executable). Python 3 standard library only; the source parser and stop classification come
from tools/tracks/marker_check.py (which uses tools/ai-reconstruct/build_ledger.py).

    python3 tools/tracks/rend_ledger.py [--export DIR] [--ledger PATH --summary PATH]

The function lists come from a Ghidra headless export (tools/tracks/ghidra/ExportFunctions.java)
in --export (default out/rend-ghidra/{RendDX9,BF2exe}/functions.tsv); see the summary for the
command. Re-run after every merge into the track branch.

Columns (the first seven are the docs/tracks/README.md contract):
    image          RendDX9.dll | BF2.exe (retail 1.50 Win32 client images).
    address        PE virtual address (image base 0x10000000 / 0x00400000), hex.
    size           Ghidra function body size in bytes.
    symbol         the matched source definition's qualified name; Ghidra's name (FUN_...) otherwise.
    file           repo file holding (or expected to hold) the body; blank when unknown.
    status         todo | done | stop | skip | chosen (below).
    commit         short HEAD hash when the row was first seen done (kept across re-runs).
    file_inferred  1 when `file` comes from the address neighbours or a mere citation, not a match.
    cited          `file:line` of the first other definition that cites this address (callee or
                   alias mentions), when no definition claims it.
    r_file         only with --rdebug: the source file that the 2006 RendDX9_r.dll debug build attests
                   for the matched function (docs/tracks/rend/rdebug-map.csv, `high` pairs only, own
                   __FILE__ sites only; see docs/tracks/rend/rdebug-findings.md). An older build: a
                   placement hint for the 1.50 body, never proof of it. Without --rdebug the
                   ledger keeps its nine columns.
    class          only with --attribution [CSV]: functional attribution of RendDX9 todo rows;
                   done/chosen/stop rows are cited. Existing skip and BF2.exe rows stay blank.
                   This does not change status matching or stop classification. Regenerate
                   the attribution CSV when new todo addresses appear.

Matching. A definition's *own* address is the first address in its attached comment (the block
directly above it, its declarator and opening line) that is a function entry of an image. For a
member declared in a header, the end-of-line or attached comment of the declaration counts the same
way and resolves to the definition with the same qualified name and parameter count (or stays
declaration-only). Address forms: RendDX9.dll `0x10xxxxxx` or a bare `10xxxxxx`; BF2.exe `0x`
followed by 6-8 hex digits, only when the nearest image tag before it is `PE` / `BF2.exe` / `client`
(the Linux ELF and bf2_w32ded.exe addresses overlap the BF2.exe range). `sub_10xxxxxx` counts;
addresses after `loc_`, `off_`, `dword_` and the other IDA data prefixes, or after `+`, do not.

Several addresses per definition. A comment line that starts with `PE 0xA` (also `BF2.exe` /
`client`) and continues with `, 0xB`, `/ 0xB`, `and 0xB` and/or `(also 0xB, 0xC)`, then ends or
carries a `.`/`:`/`;` remark, claims every listed address for that definition (an inline function
or a body retail emitted several times); several such lines in one comment block all count. The
first address of a comment keeps priority: when two definitions claim an address, the one whose
first address it is wins, and the conflict is still listed in the summary.

Macro invocations. A comment directly above (or on the line of) a namespace-scope macro invocation
such as a BF2_CONSOLE_* binding cites its address(es) like a definition comment: the row is named
by the invocation's first argument and is `done` unless the macro or its arguments stop.

Status rules (first that applies):
  skip    thunks and the MSVC CRT startup functions Ghidra names (entry, __chkstk, ...).
  stop    the matched body stops (std::abort, a frontier call or macro, or a thin forwarder to
          one; see marker_check.py), or contains "[FRONTIER]"; or a declaration-only match whose
          comment carries [FRONTIER].
  chosen  the matched definition's attached comment says "[CHOSEN, not recovered]" and its body
          does not stop (a platform replacement or chosen scaffolding, not a recovery).
  done    a source body matched.
  todo    everything else (unmatched, cited only, or declaration-only without a tag).

File attribution of unmatched rows: when the nearest matched rows on both sides (by address) have
the same file, that file (file_inferred=1); else the citing definition's file; else blank.
"""

import argparse
import collections
import csv
import os
import re
import subprocess
import sys

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import marker_check as mc  # noqa: E402

bl = mc.bl
LEDGER = os.path.join(REPO, "docs", "tracks", "rend", "ledger.csv")
SUMMARY = os.path.join(REPO, "docs", "tracks", "rend", "ledger-summary.md")
RDEBUG_MAP = os.path.join(REPO, "docs", "tracks", "rend", "rdebug-map.csv")
ATTRIBUTION = os.path.join(REPO, "docs", "tracks", "rend", "attribution.csv")
EXPORT = os.path.join(REPO, "out", "rend-ghidra")
IMAGES = (("RendDX9.dll", "RendDX9"), ("BF2.exe", "BF2exe"))
REND_PATHS = tuple(p + "/" for p in mc.REND_PATHS)
CRT_NAMES = {"entry", "__chkstk", "_atexit", "__onexit", "__dllonexit", "___security_init_cookie",
             "initterm", "`eh_vector_destructor_iterator'", "~type_info", "__SEH_prolog",
             "__SEH_epilog", "__security_check_cookie", "_DllMainCRTStartup", "__DllMainCRTStartup"}
CHOSEN = "[CHOSEN, not recovered]"
# a candidate address and the text right before it
# `sub_XXXXXXXX` names a function (often the definition's own address); the other IDA-style data
# and label prefixes never do.
ADDR_RE = re.compile(r"(?:(?<![\w+])|(?<=\bsub_))(0x)?([0-9A-Fa-f]{6,8})\b")
TAG_RE = re.compile(r"\b(PE|BF2\.exe|client|ELF|bf2_w32ded(?:\.exe)?|w32ded|Linux|amd64|x64|ia-32)\b")
OFFSET_BEFORE_RE = re.compile(r"(\+|-)\s*$")


def load_functions(path):
    rows = {}
    with open(path, newline="") as f:
        for r in csv.DictReader(f, delimiter="\t"):
            a = int(r["entry"], 16)
            rows[a] = {"addr": a, "size": int(r["size"]), "thunk": r["thunk"] == "1",
                       "name": r["name"], "ns": r["namespace"]}
    return rows


def image_of(value, has0x, before, funcs):
    """The image whose function entry `value` is, or None."""
    if 0x10000000 <= value < 0x11000000:
        if value in funcs["RendDX9.dll"]:
            return "RendDX9.dll"
        return None
    if not has0x or not (0x00401000 <= value < 0x01000000):
        return None
    tags = TAG_RE.findall(before)
    if not tags or tags[-1] not in ("PE", "BF2.exe", "client"):
        return None
    return "BF2.exe" if value in funcs["BF2.exe"] else None


def addresses(ctext, funcs):
    """[(image, address)] function entries cited in a comment, in order."""
    out = []
    for m in ADDR_RE.finditer(ctext):
        digits = m.group(2)
        has0x = bool(m.group(1))
        if not has0x and len(digits) != 8:
            continue
        before = ctext[max(0, m.start() - 40):m.start()]
        if OFFSET_BEFORE_RE.search(before):
            continue
        img = image_of(int(digits, 16), has0x, before, funcs)
        if img:
            out.append((img, int(digits, 16)))
    return out


COMMENT_LEAD_RE = re.compile(r"^\s*(?:/\*+|//+|\*+)\s*")
CITE_LINE_RE = re.compile(r"^(?:PE|BF2\.exe|client)\s+((?:0x)?[0-9A-Fa-f]{6,8})\b(.*)$")
CITE_NEXT_RE = re.compile(r"^\s*(?:,\s*and|,|/|;|\band\b)\s*(?:(?:PE|BF2\.exe|client)\s+)?((?:0x)?[0-9A-Fa-f]{6,8})\b(.*)$")
CITE_ALSO_RE = re.compile(r"^\s*\(\s*also\s+([^)]*)\)(.*)$")
CITE_END_RE = re.compile(r"^\s*(?:[.:;]|-\s|$)")


def citation_list(ctext, funcs):
    """[(image, address)] claimed by multi-address citation lines in a comment: a line that starts
    with `PE 0xA` (or BF2.exe/client) followed by `, 0xB` / `/ 0xB` / `and 0xB` items and/or a
    `(also 0xB, 0xC)` group, then the end of the line or a `.`/`:`/`;` remark. A line whose
    addresses are followed by other prose claims nothing here (the caller's first-address rule
    still applies)."""
    found = []
    for raw in ctext.split("\n"):
        line = COMMENT_LEAD_RE.sub("", raw).rstrip(" */\t")
        m = CITE_LINE_RE.match(line)
        if not m:
            continue
        items, rest = [m.group(1)], m.group(2)
        while True:
            n = CITE_NEXT_RE.match(rest)
            if n:
                items.append(n.group(1))
                rest = n.group(2)
                continue
            a = CITE_ALSO_RE.match(rest)
            if a:
                inner = a.group(1)
                first = True
                while inner.strip():
                    n = CITE_NEXT_RE.match(inner) if not first else re.match(
                        r"^\s*(?:(?:PE|BF2\.exe|client)\s+)?((?:0x)?[0-9A-Fa-f]{6,8})\b(.*)$", inner)
                    if not n:
                        break
                    items.append(n.group(1))
                    inner = n.group(2)
                    first = False
                rest = a.group(2)
            break
        if not CITE_END_RE.match(rest):
            continue
        found.extend(addresses(" PE " + " ".join(items), funcs))
    return found


def claimed(ctext, funcs):
    """([claimed addresses], [other cited addresses]) of a definition's comment: the first address
    plus those of citation_list; everything else the comment mentions is a mere citation."""
    cand = addresses(ctext, funcs)
    if not cand:
        return [], []
    mine = [cand[0]]
    for c in citation_list(ctext, funcs):
        if c not in mine:
            mine.append(c)
    return mine, [c for c in cand if c not in mine]


def source_files():
    """Every Code/BF2 and Code/RendDX9Abi source file whose text cites a candidate address."""
    probe = re.compile(r"(?<![\w+])(0x10[0-9A-Fa-f]{6}|10[0-9A-Fa-f]{6}\b|\bPE\b)")
    out = []
    for top in ("Code/BF2", "Code/RendDX9Abi"):
        for root, dirs, names in os.walk(os.path.join(REPO, top)):
            rel_root = os.path.relpath(root, REPO)
            # External/BFMeme is DICE's own menu library and ships inside RendDX9.dll, so its citations
            # count; the rest of External is third-party upstream code.
            if (rel_root.startswith(("Code/BF2/External", "Code/BF2/AI"))
                    and not rel_root.startswith("Code/BF2/External/BFMeme")
                    and rel_root != "Code/BF2/External"):
                dirs[:] = []
                continue
            # Code/BF2/Memory is Memory.dll, its own image (issue #7); its PE addresses share
            # RendDX9.dll's 0x10000000 base and must not claim RendDX9 rows.
            if rel_root == "Code/BF2/Memory" or rel_root.startswith("Code/BF2/Memory" + os.sep):
                dirs[:] = []
                continue
            if rel_root == "Code/BF2/External":
                names = []
            for n in sorted(names):
                if not n.endswith(mc.EXTS):
                    continue
                rel = os.path.join(rel_root, n)
                if rel.startswith(REND_PATHS) or probe.search(mc.read(rel)):
                    out.append(rel)
    return sorted(out)


def owned(rel):
    return rel.startswith(REND_PATHS)


def head_commit():
    return subprocess.run(["git", "-C", REPO, "rev-parse", "--short", "HEAD"], check=True,
                          capture_output=True, text=True).stdout.strip()


def load_previous(path):
    prev = {}
    if os.path.exists(path):
        with open(path, newline="") as f:
            for r in csv.DictReader(f):
                prev[(r["image"], r["address"])] = r
    return prev


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--export", default=EXPORT)
    ap.add_argument("--ledger", default=LEDGER)
    ap.add_argument("--summary", default=SUMMARY)
    ap.add_argument("--rdebug", nargs="?", const=RDEBUG_MAP, default=None, metavar="MAP",
                    help="add the r_file column from the RendDX9_r match (default %(const)s)")
    ap.add_argument("--attribution", nargs="?", const=ATTRIBUTION, default=None, metavar="CSV",
                    help="add functional class column and summary table (default %(const)s)")
    a = ap.parse_args()

    funcs = {}
    for image, sub in IMAGES:
        path = os.path.join(a.export, sub, "functions.tsv")
        if not os.path.exists(path):
            sys.exit("missing %s (run the Ghidra export, see the summary)" % path)
        funcs[image] = load_functions(path)

    rels = source_files()
    res = mc.analyse_ex(rels)
    defs, decls, stopping, invocations = res["defs"], res["decls"], res["stopping"], res["invocations"]
    citations = res["citations"]

    def_index = collections.defaultdict(list)
    for d in defs:
        def_index[(tuple(d["comps"][-2:]), d["nparams"])].append(d)

    claims = collections.defaultdict(list)   # (image, addr) -> [(kind, def-or-decl)], first address of a comment
    extra = collections.defaultdict(list)    # same, addresses a multi-address citation adds
    cites = {}                               # (image, addr) -> "file:line"
    for d in defs:
        mine, others = claimed(d["comment"], funcs)
        if mine:
            for i, c in enumerate(mine):
                (claims if i == 0 else extra)[c].append(("def", d))
            for c in others:
                cites.setdefault(c, "%s:%d" % (d["file"], d["line"]))
    for decl in decls:
        mine, others = claimed(decl["comment"], funcs)
        if not mine:
            continue
        targets = def_index.get((tuple(decl["comps"][-2:]), decl["nparams"]), [])
        for i, c in enumerate(mine):
            book = claims if i == 0 else extra
            if targets:
                for t in targets:
                    book[c].append(("def", t))
            else:
                book[c].append(("decl", decl))
        for c in others:
            cites.setdefault(c, "%s:%d" % (decl["file"], decl["line"]))
    # namespace-scope stop macros carry their address among the arguments
    for rel, line, text, fn in invocations:
        cand = addresses(text, funcs)
        if cand:
            claims[cand[0]].append(("macro", {"file": rel, "line": line, "comps": [fn]}))
    # a `// PE 0x...` comment above (or on the line of) a macro invocation, such as a console binding,
    # cites the retail function(s) that invocation generates; done unless the macro stops
    for c in citations:
        mine, others = claimed(c["comment"], funcs)
        owner = {"file": c["file"], "line": c["line"], "comps": [c["label"] or c["name"]],
                 "comment": c["comment"], "body": c["text"], "stops": c["stops"]}
        for i, addr in enumerate(mine):
            (claims if i == 0 else extra)[addr].append(("citation", owner))
        for addr in others:
            cites.setdefault(addr, "%s:%d" % (c["file"], c["line"]))

    prev = load_previous(a.ledger)
    head = head_commit()
    rows, conflicts = [], []
    for image, _ in IMAGES:
        if image == "BF2.exe":
            addrs = sorted({ad for (img, ad) in list(claims) + list(extra) + list(cites) if img == image})
        else:
            addrs = sorted(funcs[image])
        for ad in addrs:
            f = funcs[image][ad]
            key = (image, ad)
            row = {"image": image, "address": "0x%08X" % ad, "size": f["size"],
                   "symbol": f["name"] if not f["ns"] else f["ns"] + "::" + f["name"],
                   "file": "", "status": "todo", "commit": "", "file_inferred": "",
                   "cited": ""}
            uniq = []
            groups = []
            for book in (claims, extra):
                group = []
                for kind, d in book.get(key, []):
                    if not any(d is u[1] for u in uniq):
                        uniq.append((kind, d))
                        group.append((kind, d))
                groups.append(group)
            if len(uniq) > 1:
                # a comment's first address outranks one a multi-address citation adds; then prefer
                # definitions over declarations and macro bodies, rend paths, then source order
                order = lambda kd: (kd[0] != "def", not owned(kd[1]["file"]))
                uniq = sorted(groups[0], key=order) + sorted(groups[1], key=order)
                if len({u[1]["file"] + ":" + str(u[1]["line"]) for u in uniq}) > 1:
                    conflicts.append((image, ad, uniq))
            if f["thunk"] or f["name"] in CRT_NAMES:
                row["status"] = "skip"
            if uniq:
                kind, d = uniq[0]
                row["symbol"] = "::".join(d["comps"])
                row["file"] = d["file"]
                row["file_inferred"] = "0"
                if row["status"] != "skip":
                    if kind == "macro":
                        row["status"] = "stop"
                    elif kind == "citation":
                        row["status"] = ("stop" if d["stops"] or mc.TAG in d["comment"] else
                                         "chosen" if CHOSEN in d["comment"] else "done")
                    elif kind == "decl":
                        row["status"] = "stop" if mc.TAG in d["comment"] else "todo"
                    elif id(d) in stopping or mc.TAG in d["body"]:
                        row["status"] = "stop"
                    elif CHOSEN in d["comment"]:
                        row["status"] = "chosen"
                    else:
                        row["status"] = "done"
            elif key in cites:
                row["cited"] = cites[key]
            if row["status"] == "done":
                p = prev.get((image, row["address"]))
                row["commit"] = p["commit"] if p and p.get("status") == "done" and p.get("commit") else head
            rows.append(row)

    # neighbour attribution for unmatched rows
    for image, _ in IMAGES:
        img_rows = [r for r in rows if r["image"] == image]
        matched = [i for i, r in enumerate(img_rows) if r["file_inferred"] == "0"]
        for i, r in enumerate(img_rows):
            if r["file"]:
                continue
            lo = max((m for m in matched if m < i), default=None)
            hi = min((m for m in matched if m > i), default=None)
            if lo is not None and hi is not None and img_rows[lo]["file"] == img_rows[hi]["file"]:
                r["file"] = img_rows[lo]["file"]
                r["file_inferred"] = "1"
            elif r["cited"]:
                r["file"] = r["cited"].rsplit(":", 1)[0]
                r["file_inferred"] = "1"

    os.makedirs(os.path.dirname(a.ledger), exist_ok=True)
    cols = ["image", "address", "size", "symbol", "file", "status", "commit", "file_inferred", "cited"]
    if a.rdebug:
        rfile = load_rdebug(a.rdebug)
        cols.append("r_file")
        for r in rows:
            r["r_file"] = rfile.get(r["address"], "") if r["image"] == "RendDX9.dll" else ""
    if a.attribution:
        try:
            apply_attribution(rows, load_attribution(a.attribution))
        except ValueError as error:
            ap.error(str(error))
        cols.append("class")
    with open(a.ledger, "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=cols, lineterminator="\n")
        w.writeheader()
        w.writerows(rows)
    write_summary(a.summary, rows, conflicts, head, len(rels))
    counts = collections.Counter((r["image"], r["status"]) for r in rows)
    print("rend_ledger: %d rows; " % len(rows) + ", ".join(
        "%s %s %d" % (img, st, n) for (img, st), n in sorted(counts.items())))
    if a.attribution:
        classes = collections.Counter(r["class"] for r in rows if r.get("class"))
        print("rend_ledger attribution: " + ", ".join("%s %d" % (c, n) for c, n in sorted(classes.items())))


ATTRIBUTION_CLASSES = ("cited", "matched", "inlined", "stl", "crt", "compiler", "editor", "missing")


def load_attribution(path):
    """Read a unique, validated per-address attribution without inferring statuses."""
    result = {}
    csv.field_size_limit(16 * 1024 * 1024)
    with open(path, newline="") as stream:
        reader = csv.DictReader(stream)
        required = {"address", "class", "confidence", "our_definition", "evidence"}
        if not required.issubset(reader.fieldnames or []):
            raise ValueError("attribution CSV requires " + ",".join(sorted(required)))
        for r in reader:
            try:
                address = "0x%08X" % int(r["address"], 16)
            except ValueError:
                raise ValueError("invalid attribution address: " + r["address"])
            if address in result:
                raise ValueError("duplicate attribution address: " + address)
            if r["class"] not in ATTRIBUTION_CLASSES or r["confidence"] not in ("high", "medium", "low"):
                raise ValueError("invalid attribution class/confidence at " + address)
            result[address] = r["class"]
    return result


def apply_attribution(rows, attribution):
    """Append the optional class column; status and every existing column are unchanged."""
    absent = []
    for r in rows:
        r["class"] = ""
        if r["image"] != "RendDX9.dll":
            continue
        if r["status"] in ("done", "chosen", "stop"):
            r["class"] = "cited"
        elif r["status"] == "todo":
            if r["address"] not in attribution:
                absent.append(r["address"])
            else:
                r["class"] = attribution[r["address"]]
    if absent:
        raise ValueError("attribution missing %d current todo rows (first: %s); regenerate attribution" %
                         (len(absent), ", ".join(absent[:5])))


def load_rdebug(path):
    """rdebug-map.csv -> {"0x1xxxxxxx": "Code/BF2/..."} for high pairs with an own-site file."""
    res = {}
    with open(path, newline="") as f:
        for r in csv.DictReader(f):
            if r["confidence"] == "high" and r["file"] and r.get("file_basis") == "site":
                res[r["addr_150"]] = "Code/BF2/" + r["file"]
    return res


def write_summary(path, rows, conflicts, head, nfiles):
    by = collections.defaultdict(collections.Counter)
    sizes = collections.defaultdict(collections.Counter)
    for r in rows:
        by[r["image"]][r["status"]] += 1
        sizes[r["image"]][r["status"]] += int(r["size"])
    files = collections.defaultdict(collections.Counter)
    for r in rows:
        if r["file"]:
            files[r["file"]][r["status"]] += 1
    L = []
    L.append("# rend ledger summary")
    L.append("")
    L.append("Generated by `tools/tracks/rend_ledger.py` at %s from %d source files. Do not edit by hand."
             % (head, nfiles))
    L.append("")
    L.append("Function lists: Ghidra 12.1.4 headless auto-analysis of the retail 1.50 images, exported by")
    L.append("`tools/tracks/ghidra/ExportFunctions.java`:")
    L.append("")
    L.append("    analyzeHeadless out/rend-ghidra/proj renddx9 -import <game>/RendDX9.dll \\")
    L.append("        -scriptPath tools/tracks/ghidra -postScript ExportFunctions.java out/rend-ghidra/RendDX9")
    L.append("    analyzeHeadless out/rend-ghidra/proj-bf2exe bf2exe -import <game>/BF2.exe \\")
    L.append("        -scriptPath tools/tracks/ghidra -postScript ExportFunctions.java out/rend-ghidra/BF2exe")
    L.append("")
    L.append("RendDX9.dll carries no RTTI and no symbols, so a row is named only by a matching source comment;")
    L.append("`todo` rows are unattributed retail functions (game code, inlined STL and CRT helpers alike).")
    L.append("BF2.exe rows are only the functions that rend-path definitions cite.")
    L.append("")
    L.append("| image | status | rows | bytes |")
    L.append("|---|---|---:|---:|")
    for img in sorted(by):
        for st in ("done", "chosen", "stop", "todo", "skip"):
            if by[img][st]:
                L.append("| %s | %s | %d | %d |" % (img, st, by[img][st], sizes[img][st]))
    if any("class" in r for r in rows):
        L.extend(["", "## Functional attribution (RendDX9.dll)", "",
                  "Classes supplement the status table; they never change its matching/stop rules.",
                  "Existing skipped rows and BF2.exe rows are outside this attribution table.", "",
                  "| class | rows | bytes |", "|---|---:|---:|"])
        cb, cs = collections.Counter(), collections.Counter()
        for r in rows:
            if r.get("class"):
                cb[r["class"]] += 1
                cs[r["class"]] += int(r["size"])
        for kind in ATTRIBUTION_CLASSES:
            L.append("| %s | %d | %d |" % (kind, cb[kind], cs[kind]))
    L.append("")
    L.append("## Per file (matched or inferred rows)")
    L.append("")
    L.append("| file | done | chosen | stop | todo |")
    L.append("|---|---:|---:|---:|---:|")
    for f in sorted(files):
        c = files[f]
        L.append("| %s%s | %d | %d | %d | %d |" % (f, "" if owned(f) else " (not rend)", c["done"],
                                                   c["chosen"], c["stop"], c["todo"]))
    L.append("")
    L.append("## Address claimed by several definitions (%d)" % len(conflicts))
    L.append("")
    L.append("The first is used; check the others' comments (an alias, a caller, or a wrong address).")
    L.append("")
    for img, ad, uniq in conflicts:
        L.append("- %s 0x%08X: " % (img, ad) + "; ".join(
            "%s %s:%d" % ("::".join(d["comps"]), d["file"], d["line"]) for _, d in uniq))
    L.append("")
    with open(path, "w") as f:
        f.write("\n".join(L))


if __name__ == "__main__":
    main()
