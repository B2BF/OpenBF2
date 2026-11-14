#!/usr/bin/env python3
"""Generate the srv track function ledger (docs/tracks/srv/ledger.csv) and its summary.

One row per function of the Linux retail server `bf2` (ia-32, with symbols) that belongs to the srv
track: the original GameServer, IO and POSIX platform (thread/process/system) translation units.
Python 3 standard library only; reuses the source parser and demangled-name helpers of
tools/ai-reconstruct/build_ledger.py and shells out to `objdump -t`, `c++filt -n` and `git`.
Re-run after every merge into the track branch:

    python3 tools/tracks/srv_ledger.py [--binary PATH] [--ledger PATH --summary PATH]

Columns (the first seven are the docs/tracks/README.md contract):
    image          always `bf2` (Linux retail server, ELF ia-32).
    address        ELF virtual address, hex. Ghidra shows the same address for this executable.
    size           symbol size in bytes.
    symbol         demangled name; aliases at one address joined with " | ".
    file           repo file holding (or expected to hold) the body; blank when nothing names one.
    status         todo | done | stop | skip | chosen (below).
    commit         short HEAD hash when the row was first seen done (kept across re-runs).
    tu             original translation unit (FILE symbol basename); "A|B" when the layout leaves
                   two candidates; blank for weak (inline/template) functions, which carry no TU.
    file_inferred  1 when `file` is not backed by a matched source body.

Ownership. The original image lays translation units out in link order, which is the order of the
FILE symbols, with each TU's local functions (static init, statics) at the end of its text. A TU
with local functions therefore ends at its last local function; strong global functions between
two such TUs belong to the later one or to a TU without locals listed between them. Candidates are
narrowed by evidence: the file of a matched source body, a class named like a candidate TU, the
TU of the class's other functions, then the agreeing TUs of both address neighbours. The srv spans
(FILE order, both ends inclusive) are listed in SPANS; TUs owned by another track inside a span
are in EXCLUDE. Weak functions are owned when their namespaced class has owned strong functions, or is declared
under the srv paths and its unqualified name is unique in the binary. std:: and __gnu_cxx::
instantiations are not listed.

Status rules (first that applies):
  skip    compiler thunks (the target function carries the status).
  stop    a matched body contains "[FRONTIER]" or a [FRONTIER] declaration matched by name.
  chosen  the matched definition's attached comment says "[CHOSEN, not recovered]" (a platform
          replacement, not a recovery).
  done    a source body matched.
  todo    everything else.

Matching: every .cpp/.h/.inl/.mm under Code/BF2 except External, Platform/Gfx and AI is parsed.
  A. An address in the definition's comment that is a ledger row with the same function and class
     name (addresses tagged PE, vtable, .rodata, ds:, .data, .bss are ignored).
  B. Else the qualified name (class chain + function), split by parameter count/types/const; an
     explicit namespace on the definition must be among the symbol's namespaces.
  When several files define a row, a file under the srv paths wins; remaining conflicts are listed.
"""

import argparse
import bisect
import collections
import csv
import os
import re
import subprocess
import sys

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
sys.path.insert(0, os.path.join(REPO, "tools", "ai-reconstruct"))
import build_ledger as bl  # noqa: E402

DEFAULT_BINARY = (os.path.join(os.environ["BF2_LINUX_SERVER_DIR"], "bin", "ia-32", "bf2")
                  if os.environ.get("BF2_LINUX_SERVER_DIR") else None)
LEDGER = os.path.join(REPO, "docs", "tracks", "srv", "ledger.csv")
SUMMARY = os.path.join(REPO, "docs", "tracks", "srv", "ledger-summary.md")

# (first TU, last TU, group, occurrence of the first name): FILE-order spans owned by srv.
SPANS = [
    ("ClientInfo.cpp", "ClientInfo.cpp", "GameServer", 1),
    ("ServerInfo.cpp", "ServerInfo.cpp", "GameServer", 1),
    ("autodemo_process_hook.cpp", "ServerSettings.cpp", "GameServer", 1),
    ("ServerVoteManager.cpp", "Vote.cpp", "GameServer", 1),
    ("Dumper.cpp", "windows-emul.cpp", "Platform", 1),
    ("BitStream.cpp", "UnixFileSystem.cpp", "IO", 1),
    ("Module.cpp", "ModuleInput.cpp", "IO", 1),   # IO's module block: Module, ModuleIDs, Module{Console,Network,File,Input}
]
EXCLUDE = {"PunkBusterFilterAgent.cpp": "feat (PunkBuster interface)"}
# srv paths per docs/tracks/README.md (the Core platform files were assigned on 2026-11-03)
OWNED_PATHS = ("Code/BF2/Game/GameServer/", "Code/BF2/IO/", "Code/BF2/Core/Thread", "Code/BF2/Platform/",
               "Code/BF2/Core/Mutex.", "Code/BF2/Core/SystemInfo.", "Code/BF2/Core/Timer.",
               "Code/BF2/Core/PluginDll.", "Code/BF2/Core/Module.", "Code/BF2/Core/ModuleRegistry.")
NOT_OWNED_PATHS = ("Code/BF2/Game/GameServer/VoipServer", "Code/BF2/Platform/Gfx/")
LIB_NAMESPACES = ("std", "__gnu_cxx")
CRT_FILES = {"init.c", "initfini.c", "crtstuff.c"}
# [INFERRED] class or free-function name -> TU, for segments where several TUs without local
# functions share one layout gap and no name matches a TU. Each name sits, in address order,
# exactly where the FILE order puts that TU (Timer.cpp, Dumper.cpp, SystemInfo_posix.cpp,
# Threading_pthreads.cpp, PerformanceCounter.cpp, windows-emul.cpp, zip.c), or the name is the TU's
# subject (safeRead -> SafeStreamUtil.cpp, FileModule -> ModuleFile.cpp); listed in the summary.
TU_HINTS = {
    "PosixSystemInfo": "SystemInfo_posix.cpp",
    "Mutex": "Threading_pthreads.cpp",
    "Thread": "Threading_pthreads.cpp",
    "pthreads_thread_trampoline": "Threading_pthreads.cpp",
    "LoadLibrary": "windows-emul.cpp",
    "FreeLibrary": "windows-emul.cpp",
    "GetProcAddress": "windows-emul.cpp",
    "safeRead": "SafeStreamUtil.cpp",
    "safeWrite": "SafeStreamUtil.cpp",
    "readBasicHeader": "ProtocolHeader.cpp",
    "readExtendedHeader": "ProtocolHeader.cpp",
    "writeBasicHeader": "ProtocolHeader.cpp",
    "writeExtendedHeader": "ProtocolHeader.cpp",
    "AsyncMultiStream::Entry": "AsyncMultiStreamer.cpp",
    "FileModule": "ModuleFile.cpp",
    "InputDX8Module": "ModuleInput.cpp",
}
SYM_RE = re.compile(r"^([0-9a-f]{8}) (.{7}) (\S+)\t([0-9a-f]{8}) (?:\.hidden )?(.+)$")
ADDR_RE = re.compile(r"(?<![+\w])0x(0?8[0-9A-Fa-f]{6})\b")
ADDR_EXCLUDE_RE = re.compile(r"(PE|BF2\.exe|w32ded|vtable|Vtable|GOT|rodata|\.data|\.bss|BSS|ds:)[^,;.()]{0,12}$")
CHOSEN = "[CHOSEN, not recovered]"


def owned_path(rel):
    return rel.startswith(OWNED_PATHS) and not rel.startswith(NOT_OWNED_PATHS)


def stem(name):
    return os.path.splitext(os.path.basename(name))[0].lower()


def load_symbols(binary):
    """FUNC symbols of .text with the FILE ordinal of local symbols; and the FILE list."""
    out = subprocess.run(["objdump", "-t", binary], check=True, capture_output=True, text=True).stdout
    files, syms = [], []
    for line in out.splitlines():
        m = SYM_RE.match(line)
        if not m:
            continue
        addr, flags, section, size, name = m.groups()
        if section == "*ABS*" and "df" in flags:
            files.append(name.strip())
            continue
        if "F" not in flags or section != ".text":
            continue
        local = flags[0] == "l"
        syms.append({"addr": int(addr, 16), "size": int(size, 16), "mangled": name.strip(),
                     "weak": flags[1] == "w", "fileno": len(files) - 1 if local else None})
    dem = subprocess.run(["c++filt", "-n"], input="\n".join(s["mangled"] for s in syms) + "\n",
                         check=True, capture_output=True, text=True).stdout.splitlines()
    if len(dem) != len(syms):
        sys.exit("c++filt returned %d lines for %d symbols" % (len(dem), len(syms)))
    for s, d in zip(syms, dem):
        s["demangled"] = d.strip()
    return files, syms


def span_owner(files):
    owner = {}
    for first, last, group, occ in SPANS:
        seen, start = 0, None
        for i, f in enumerate(files):
            if f == first:
                seen += 1
                if seen == occ:
                    start = i
                    break
        if start is None:
            sys.exit("span start %s not found" % first)
        end = files.index(last, start)
        for i in range(start, end + 1):
            if files[i] not in EXCLUDE:
                owner[i] = group
    return owner


def source_files():
    out = []
    for root, dirs, names in os.walk(os.path.join(REPO, "Code", "BF2")):
        rel_root = os.path.relpath(root, REPO)
        if rel_root.startswith(("Code/BF2/External", "Code/BF2/Platform/Gfx", "Code/BF2/AI")):
            dirs[:] = []
            continue
        for n in names:
            if n.endswith((".cpp", ".h", ".inl", ".mm")):
                out.append(os.path.join(rel_root, n))
    return sorted(out)


def namespaces_ok(d, info, namespaces):
    """An explicit namespace on the definition must appear among the symbol's namespaces."""
    dns = set(d["namespaces"]) | {c for c in d["comps"][:-1] if c in namespaces}
    dns.discard("{anon}")
    if not dns:
        return True
    lead = []
    for c in info["comps"][:-1]:
        if c in namespaces:
            lead.append(c)
        else:
            break
    return dns <= set(lead)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--binary", default=DEFAULT_BINARY, required=DEFAULT_BINARY is None,
                    help="ia-32 bf2 server image (default: $BF2_LINUX_SERVER_DIR/bin/ia-32/bf2)")
    ap.add_argument("--ledger", default=LEDGER)
    ap.add_argument("--summary", default=SUMMARY)
    args = ap.parse_args()

    head = subprocess.run(["git", "-C", REPO, "rev-parse", "--short", "HEAD"], capture_output=True,
                          text=True).stdout.strip()
    previous = {}
    if os.path.exists(args.ledger):
        with open(args.ledger, newline="") as f:
            for r in csv.DictReader(f):
                previous[r["address"]] = r

    files, syms = load_symbols(args.binary)
    owner = span_owner(files)

    # one row per address
    by_addr = collections.OrderedDict()
    for s in sorted(syms, key=lambda s: (s["addr"], s["mangled"])):
        by_addr.setdefault(s["addr"], []).append(s)
    rows = []
    for addr, ss in by_addr.items():
        s0 = ss[0]
        rows.append({"addr": addr, "size": max(s["size"] for s in ss),
                     "symbol": " | ".join(s["demangled"] for s in ss),
                     "info": bl.parse_demangled(s0["demangled"]), "weak": all(s["weak"] for s in ss),
                     "fileno": next((s["fileno"] for s in ss if s["fileno"] is not None), None),
                     "cands": None, "tu": None, "file": "", "inferred": 1, "status": None, "why": ""})

    # TU segments: local functions mark the end of their TU's text
    local_rng = {}
    for r in rows:
        if r["fileno"] is not None:
            lo, hi = local_rng.get(r["fileno"], (r["addr"], r["addr"]))
            local_rng[r["fileno"]] = (min(lo, r["addr"]), max(hi, r["addr"]))
    # the CRT objects (crti/crtbegin/crtend) bracket .text and are not BF2 TUs
    order = sorted(i for i in local_rng if files[i] not in CRT_FILES)
    ends = [local_rng[i][1] for i in order]
    if ends != sorted(ends):
        sys.exit("FILE order and address order disagree; the segment rule does not hold")
    for r in rows:
        if r["fileno"] is not None:
            r["cands"] = [r["fileno"]]
        elif not r["weak"]:
            j = bisect.bisect_left(ends, r["addr"])
            if j < len(order):
                prev = order[j - 1] if j else -1
                r["cands"] = list(range(prev + 1, order[j] + 1))
            else:
                r["cands"] = list(range(order[-1] + 1, len(files)))

    # source side
    defs, decls, classes = [], [], []
    for rel in source_files():
        d, dc, cl = bl.parse_file(rel)
        defs += d
        decls += dc
        classes += cl
    namespaces = set(bl.KNOWN_NAMESPACES)
    for d in defs:
        namespaces.update(d["namespaces"])

    def is_lib(r):
        comps = r["info"]["comps"]
        return bool(comps) and comps[0] in LIB_NAMESPACES

    def class_key(r):
        return bl.scope_key(r["info"]["comps"], namespaces)[0]

    # candidate rows for matching: anything that could be owned
    def maybe_owned(r):
        return r["cands"] and any(c in owner for c in r["cands"])

    # classes declared only under the srv paths (a .cpp re-declaring another track's class to call
    # it, like MemoryPool in Core/Module.cpp, does not make that class srv's)
    decl_files = collections.defaultdict(set)
    for name, chain, rel, _ in classes:
        decl_files["::".join(chain + [name])].add(rel)
    owned_classes = {k for k, fs in decl_files.items() if all(owned_path(f) for f in fs)}

    def candidate(r):
        if r["info"]["thunk"] or is_lib(r):
            return False
        return bool(maybe_owned(r) or r["weak"])

    pool = [r for r in rows if candidate(r)]
    row_at = {r["addr"]: r for r in pool}
    groups = collections.defaultdict(list)
    for r in pool:
        groups[r["info"]["sig"]].append(r)
    by_key = collections.defaultdict(set)
    for sig, rs in groups.items():
        k = bl.scope_key(rs[0]["info"]["comps"], namespaces)
        by_key[(k[0], bl.norm_fn(k[1]))].add(sig)

    matched = collections.defaultdict(list)   # sig -> [(def, how)]
    ambiguous = []
    for d in defs:
        hit = set()
        for m in ADDR_RE.finditer(d["comment"]):
            if ADDR_EXCLUDE_RE.search(d["comment"][max(0, m.start() - 24):m.start()]):
                continue
            r = row_at.get(int(m.group(1), 16))
            if r and bl.same_linkage_scope(d, r["info"]) and bl.names_agree(d["comps"], r["info"]["comps"]):
                hit.add(r["info"]["sig"])
        if hit:
            for sig in hit:
                matched[sig].append((d, "address"))
            continue
        k = bl.scope_key(d["comps"], namespaces)
        cands = {c for c in by_key.get((k[0], bl.norm_fn(k[1])), set())
                 if bl.same_linkage_scope(d, groups[c][0]["info"])
                 and namespaces_ok(d, groups[c][0]["info"], namespaces)}
        if not cands:
            continue
        if d["template"] and len(cands) > 1:
            same = {c for c in cands if groups[c][0]["info"]["nparams"] == d["nparams"]}
            if same:
                for sig in same:
                    matched[sig].append((d, "template"))
                continue
        cands = bl.disambiguate(cands, d, groups)
        if len(cands) == 1:
            matched[next(iter(cands))].append((d, "name"))
        elif len(cands) > 1:
            ambiguous.append((cands, "%s:%d `%s` (%d params) matches %d overloads by name" % (
                d["file"], d["line"], d["qual"], d["nparams"], len(cands))))

    decl_frontier = set()
    for dc in decls:
        if "[FRONTIER]" not in dc["comment"]:
            continue
        k = bl.scope_key(dc["comps"], namespaces)
        cands = bl.disambiguate(by_key.get((k[0], bl.norm_fn(k[1])), set()), dc, groups)
        if len(cands) == 1:
            decl_frontier.add(next(iter(cands)))

    conflicts = []
    for sig, ms in matched.items():
        fs = sorted({d["file"] for d, _ in ms})
        own = [f for f in fs if owned_path(f)]
        pick = own or fs
        if len(set(stem(f) for f in pick)) > 1 and len(pick) > 1:
            conflicts.append((sig, "`%s`: %s" % (sig[:100], ", ".join(fs))))
        best = [(d, h) for d, h in ms if d["file"] == pick[0]] + [(d, h) for d, h in ms if d["file"] != pick[0]]
        stop = any(bl.frontier_in(d["body"]) for d, _ in ms)
        chosen = all(CHOSEN in d["comment"] for d, _ in ms)
        for r in groups[sig]:
            r["file"], r["inferred"] = pick[0], 0
            r["status"] = "stop" if stop else ("chosen" if chosen else "done")
            r["why"] = "%s %s:%d" % (best[0][1], best[0][0]["file"], best[0][0]["line"])

    # resolve TU candidates
    def tu_by_evidence(r):
        cs = r["cands"]
        if len(cs) == 1:
            return cs[0]
        comps = r["info"]["comps"]
        hint = TU_HINTS.get(class_key(r)) or (TU_HINTS.get(comps[-1]) if comps else None)
        hit = [c for c in cs if files[c] == hint]
        if len(hit) == 1:
            return hit[0]
        ck = class_key(r)
        if ck:
            names = {stem(p) for p in ck.split("::")}
            hit = [c for c in cs if stem(files[c]) in names]
            if len(hit) == 1:
                return hit[0]
        if r["file"]:   # weaker: the repo may merge original TUs (PerformanceCounter is in Timer.cpp)
            hit = [c for c in cs if stem(files[c]) == stem(r["file"])]
            if len(hit) == 1:
                return hit[0]
        return None

    seg_rows = [r for r in rows if r["cands"]]
    for r in seg_rows:
        r["tu"] = tu_by_evidence(r)
    class_tu = collections.defaultdict(collections.Counter)
    for r in seg_rows:
        if r["tu"] is not None and class_key(r):
            class_tu[class_key(r)][r["tu"]] += 1
    for r in seg_rows:
        if r["tu"] is None and class_key(r) in class_tu:
            hit = [c for c in r["cands"] if class_tu[class_key(r)][c]]
            if len(hit) == 1:
                r["tu"] = hit[0]
    for _ in range(3):   # a TU's text is contiguous: agreeing resolved neighbours decide
        for i, r in enumerate(seg_rows):
            if r["tu"] is not None:
                continue
            before = next((x["tu"] for x in reversed(seg_rows[:i]) if x["tu"] is not None), None)
            after = next((x["tu"] for x in seg_rows[i + 1:] if x["tu"] is not None), None)
            if before is not None and before == after and before in r["cands"]:
                r["tu"] = before

    # weak rows: owned by their full (namespaced) class; a class declared under the srv paths
    # counts only when its unqualified chain names one class in the binary (io::Object is not
    # world::Object)
    def full_scope(r):
        return "::".join(r["info"]["comps"][:-1])

    owned_full = set()
    for r in seg_rows:
        tus = [r["tu"]] if r["tu"] is not None else r["cands"]
        if class_key(r) and all(c in owner for c in tus):
            owned_full.add(full_scope(r))
    chain_scopes = collections.defaultdict(set)
    for r in rows:
        if class_key(r) and not is_lib(r):
            chain_scopes[class_key(r)].add(full_scope(r))

    def weak_owned(r):
        if full_scope(r) in owned_full:
            return True
        return class_key(r) in owned_classes and len(chain_scopes[class_key(r)]) == 1

    # unique repo basenames for TU -> file inference
    basename_files = collections.defaultdict(list)
    for rel in source_files():
        if rel.endswith((".cpp", ".mm")):
            basename_files[os.path.basename(rel)].append(rel)
    class_defs = collections.defaultdict(list)
    for d in defs:
        k = bl.scope_key(d["comps"], namespaces)[0]
        if k:
            class_defs[k].append(d)
    class_decl = collections.defaultdict(list)
    for name, chain, rel, _ in classes:
        cpp = os.path.splitext(rel)[0] + ".cpp"
        class_decl["::".join(chain + [name])].append(
            cpp if rel.endswith(".h") and os.path.exists(os.path.join(REPO, cpp)) else rel)

    def prefer_owned(fs):
        own = [f for f in fs if owned_path(f)]
        return collections.Counter(own or fs).most_common(1)[0][0] if fs else ""

    def fallback_file(r):
        """Repo file for a row without a matched body: the TU's repo file (unique basename under the
        srv paths), then the
        files defining other members of the same namespaced class, then the class declaration when
        its unqualified name is unique in the binary."""
        if r["tu"] is not None:
            fs = [f for f in basename_files.get(files[r["tu"]], []) if owned_path(f)]
            if len(fs) == 1:   # another library's file of the same name is not this TU
                return fs[0]
        ck = class_key(r)
        if not ck:
            return ""
        fs = [d["file"] for d in class_defs.get(ck, []) if namespaces_ok(d, r["info"], namespaces)
              and bl.same_linkage_scope(d, r["info"])]
        if fs and (len(chain_scopes[ck]) == 1 or any(owned_path(f) for f in fs)):
            return prefer_owned(fs)
        if len(chain_scopes[ck]) == 1:
            return prefer_owned(class_decl.get(ck, []))
        return ""

    ledger, ambiguous_tu, excluded_partial = [], [], 0
    for r in rows:
        if is_lib(r):
            continue
        if r["cands"]:
            tus = [r["tu"]] if r["tu"] is not None else r["cands"]
            groups_of = {owner.get(c) for c in tus}
            if groups_of == {None}:
                continue
            if None in groups_of:
                # still mixed: owned only when a matched body lives under the srv paths
                if not (r["file"] and not r["inferred"] and owned_path(r["file"])):
                    excluded_partial += 1
                    continue
            if r["tu"] is None:
                ambiguous_tu.append(r)
            r["tuname"] = "|".join(files[c] for c in tus)
        elif r["weak"]:
            if r["info"]["thunk"] or not class_key(r) or not weak_owned(r):
                continue
            r["tuname"] = ""
        else:
            continue
        if not r["file"]:
            r["file"] = fallback_file(r)
        if r["status"] is None:
            if r["info"]["thunk"]:
                r["status"] = "skip"
            elif r["info"]["sig"] in decl_frontier:
                r["status"], r["why"] = "stop", "decl"
            else:
                r["status"] = "todo"
        ledger.append(r)

    listed = {r["info"]["sig"] for r in ledger}
    ambiguous = [text for cands, text in ambiguous if cands & listed and owned_path(text)]
    conflicts = [text for sig, text in conflicts if sig in listed]

    os.makedirs(os.path.dirname(args.ledger), exist_ok=True)
    with open(args.ledger, "w", newline="") as f:
        w = csv.writer(f, lineterminator="\n")
        w.writerow(["image", "address", "size", "symbol", "file", "status", "commit", "tu", "file_inferred"])
        for r in ledger:
            a = "0x%08x" % r["addr"]
            commit = ""
            if r["status"] == "done":
                p = previous.get(a)
                commit = p["commit"] if p and p.get("status") == "done" and p.get("commit") else head
            w.writerow(["bf2", a, r["size"], r["symbol"], r["file"], r["status"], commit, r["tuname"],
                        r["inferred"]])
    write_summary(args.summary, ledger, files, owner, head, ambiguous, conflicts, ambiguous_tu,
                  excluded_partial)
    c = collections.Counter(r["status"] for r in ledger)
    print("%d rows: %s" % (len(ledger), ", ".join("%s %d" % kv for kv in sorted(c.items()))))


def write_summary(path, ledger, files, owner, head, ambiguous, conflicts, ambiguous_tu, excluded_partial):
    L = ["# srv ledger summary", "",
         "Generated by `tools/tracks/srv_ledger.py` at %s from the Linux retail `bf2` (ia-32)." % head,
         "Do not edit by hand; re-run the script.", "",
         "`done` means a source body was matched by address or qualified name; it is not a fidelity",
         "review. `[UNRESOLVED]` notes inside a done body are not counted. Rows whose file lies outside",
         "the srv paths (Game/Common/GhostManager.cpp, other Core files) are srv-owned by original TU;",
         "edits there go through the owning track (docs/tracks/README.md rule 4).", "",
         "## Status by group", "",
         "| group | rows | done | stop | chosen | todo | skip |", "|---|---|---|---|---|---|---|"]
    by_group = collections.defaultdict(collections.Counter)
    by_file = collections.defaultdict(collections.Counter)
    for r in ledger:
        tuname = r["tuname"].split("|")[0]
        g = next((owner[i] for i, f in enumerate(files) if f == tuname and i in owner), None) if tuname else None
        g = g or ("weak/inline" if not r["tuname"] else "mixed")
        by_group[g][r["status"]] += 1
        by_group["total"][r["status"]] += 1
        by_file[r["file"] or "(no file)"][r["status"]] += 1
    for g in sorted(by_group, key=lambda g: (g == "total", g)):
        c = by_group[g]
        L.append("| %s | %d | %d | %d | %d | %d | %d |" % (g, sum(c.values()), c["done"], c["stop"],
                                                        c["chosen"], c["todo"], c["skip"]))
    L += ["", "## Original TUs owned by srv", "",
          ", ".join("%s (%s)" % (files[i], owner[i]) for i in sorted(owner)), "",
          "Layout hints [INFERRED]: " + ", ".join("%s -> %s" % kv for kv in sorted(TU_HINTS.items())) + ".",
          "Excluded inside the spans: " + ", ".join("%s -> %s" % kv for kv in sorted(EXCLUDE.items())) + ".",
          "Strong functions whose layout candidates mix srv and other TUs without resolving evidence: "
          "%d (not listed)." % excluded_partial, "",
          "## By repo file (open rows first)", "", "| file | rows | done | stop | chosen | todo |",
          "|---|---|---|---|---|---|"]
    for f, c in sorted(by_file.items(), key=lambda kv: (-(kv[1]["todo"] + kv[1]["stop"]), kv[0])):
        L.append("| %s | %d | %d | %d | %d | %d |" % (f, sum(c.values()), c["done"], c["stop"], c["chosen"],
                                                   c["todo"]))
    for title, items in (("Rows with an unresolved TU", ["`0x%08x` %s (%s)" % (r["addr"], r["symbol"][:100],
                                                                                r["tuname"]) for r in ambiguous_tu]),
                         ("Definitions matching several overloads", ambiguous),
                         ("Rows defined in several files", conflicts)):
        L += ["", "## %s (%d)" % (title, len(items)), ""]
        L += ["- " + x for x in items[:200]]
        if len(items) > 200:
            L.append("- ... %d more" % (len(items) - 200))
    with open(path, "w") as f:
        f.write("\n".join(L) + "\n")


if __name__ == "__main__":
    main()
