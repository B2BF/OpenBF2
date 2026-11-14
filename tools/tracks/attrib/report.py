"""Deterministic per-row evidence and a reviewable functional coverage report."""
import bisect
import collections
import csv
import json
from pathlib import Path
import re
import tempfile

from classify import hx
from extract import REPO

CLASSES = ("cited", "matched", "inlined", "stl", "crt", "compiler", "editor", "missing")


def all_callers(runtime):
    """Keep callsites even where Ghidra never created the caller function."""
    starts = sorted(set(runtime.F) | set(runtime.static_initializers) | set(runtime.eh_handlers) |
                    set(runtime.eh_actions) | set(runtime.eh_catches))
    result = collections.defaultdict(list)
    function_instructions = {a: {site for site, _, _ in f["insns"]} for a, f in runtime.F.items()}
    for site, m, o in zip(runtime.A, runtime.M, runtime.O):
        if m not in ("call", "jmp") or not re.fullmatch(r"0x[0-9a-f]+", o):
            continue
        target = int(o, 16)
        i = bisect.bisect_right(starts, site) - 1
        owner = starts[i] if i >= 0 else None
        if owner in runtime.F:
            f = runtime.F[owner]
            if site not in function_instructions[owner]:
                owner = None
        if m == "jmp" and (target not in starts or target == owner):
            continue
        result[target].append(dict(site=hx(site), caller=hx(owner) if owner is not None else None,
                                   kind="call" if m == "call" else "tail-jump",
                                   caller_basis="function entry" if owner in runtime.F else
                                   "CRT/EH table entry" if owner is not None else "callsite only"))
    return result


def path_in_string(s):
    m = re.search(r"code[\\/]+bf2[\\/]+(.+)$", s, re.I)
    return "Code/BF2/" + re.sub(r"[\\/]+", "/", m[1]) if m else ""


def probable_file(a, f, ledger, rmap, callers):
    own = sorted({path_in_string(s) for s in f["strings"] if path_in_string(s)})
    if len(own) == 1:
        return own[0], "retail __FILE__ (may be an inlined header site)"
    r = rmap.get(a)
    if r and r["file"] and r["confidence"] == "high":
        return "Code/BF2/" + r["file"], ("high _r own __FILE__" if r["file_basis"] == "site" else
                                        "[INFERRED] high _r address neighbours")
    counts = collections.Counter()
    for c in callers.get(a, []):
        if c["caller"]:
            r = ledger.get(int(c["caller"], 16), {})
            if r.get("file") and r.get("file_inferred") == "0":
                counts[r["file"]] += 1
    if counts:
        ordered = sorted(counts.items(), key=lambda kv: (-kv[1], kv[0]))
        if len(ordered) == 1 or ordered[0][1] > ordered[1][1]:
            return ordered[0][0], "[INFERRED] plurality of cited callers' files"
    r = rmap.get(a)
    if r and r["file"]:
        return "Code/BF2/" + r["file"], "[INFERRED] %s _r %s" % (r["confidence"], r["file_basis"])
    r = ledger.get(a, {})
    if r.get("file"):
        return r["file"], "[INFERRED] input ledger citation/address neighbours"
    return "(unknown)", "no attested file or unique caller-file vote"


def frontier_index(matcher):
    import rend_ledger as rl
    funcs = {"RendDX9.dll": matcher.F, "BF2.exe": {}}
    result = collections.defaultdict(list)
    for d in matcher.sources:
        if d["stopping"]:
            cites = rl.addresses(d["comment"], funcs)
            if cites:
                result[cites[0][1]].append("%s:%d %s" % (d["file"], d["line"], d["name"]))
    return result


def records(data, ledger, matcher, runtime, secondary):
    library = runtime.result
    xrefs, frontiers = all_callers(runtime), frontier_index(matcher)
    result = []
    for r in ledger:
        if r["image"] != "RendDX9.dll" or r["status"] != "todo":
            continue
        a = int(r["address"], 16)
        f, pair = matcher.F[a], matcher.pairs.get(a)
        record = dict(address=hx(a), classification="missing", confidence="low", our_definition="",
                      evidence=["no unambiguous complete definition, inlining or exclusion rule established"])
        # Runtime class comes first: outlined catch bodies and generated global
        # initialization may share literals with an ordinary native definition.
        if a in library:
            record.update(library[a])
        elif pair and not pair["seed"]:
            record.update(classification="cited" if any("own attached PE citation" in e for e in pair["evidence"])
                          else "matched", confidence=pair["confidence"],
                          our_definition=matcher.source_label(pair["native"]), evidence=pair["evidence"])
        elif a in secondary:
            record.update(secondary[a])
        path, basis = probable_file(a, f, matcher.L, matcher.rmap, xrefs)
        direct = set()
        for _, m, o in f["insns"]:
            if re.fullmatch(r"0x[0-9a-f]+", o):
                target = int(o, 16)
                if m == "call" or (m == "jmp" and target != a and target in matcher.F):
                    direct.add((target, "call" if m == "call" else "tail-jump"))
        indirect = [dict(site=hx(site), operand=o) for site, m, o in f["insns"]
                    if m == "call" and not re.fullmatch(r"0x[0-9a-f]+", o)]
        evidence = dict(anchors=record["evidence"], probable_file=path, file_basis=basis,
                        decode_basis=f.get("decode_basis", "linear listing"),
                        cfg_complete=f.get("cfg_complete"),
                        instruction_count=f["n"], float_constants=f["floats"],
                        integer_constants=f["consts"], indirect_jump_count=f["switches"],
                        retail_name=r["symbol"], size=int(r["size"]),
                        callers=xrefs.get(a, []), callees=[dict(address=hx(c), kind=kind,
                            ledger_symbol=matcher.L.get(c, {}).get("symbol", "")) for c, kind in sorted(direct)],
                        indirect_calls=indirect, imports=list(dict.fromkeys(f["imports"])),
                        indirect_jumps=[dict(site=hx(site), operand=o) for site, m, o in f["insns"] if m == "jmp" and
                                        not re.fullmatch(r"0x[0-9a-f]+", o)],
                        strings=f["strings"], rdebug=matcher.rmap.get(a))
        known_frontiers = frontiers.get(a, []) + record.get("frontier_definitions", [])
        if known_frontiers:
            evidence["known_source_frontiers"] = sorted(set(known_frontiers))
        if record["classification"] == "missing":
            candidates = []
            for (ad, group), votes in sorted(matcher.votes.items()):
                if ad == a:
                    b = min(matcher.native_groups[group])
                    candidates.append(dict(definition=matcher.source_label(b),
                                           anchors=[votes[k] for k in sorted(votes)],
                                           rejected=matcher.rejected.get((ad, group), "ambiguous/unaccepted")))
            if candidates:
                evidence["review_candidates"] = candidates
        record["evidence"] = evidence
        record["probable_file"], record["file_basis"] = path, basis
        record["size"] = int(r["size"])
        result.append(record)
    return sorted(result, key=lambda r: r["address"])


# The committed CSV keeps only the anchors and the probable file of each row; the full evidence cell
# (callers, callees, strings, imports, rdebug, ...) goes to out/attrib/attribution-evidence.csv.
COMMITTED_EVIDENCE_KEYS = ("anchors", "probable_file")


def _write_rows(path, records, keys):
    path.parent.mkdir(parents=True, exist_ok=True)
    scratch = REPO / "out/attrib"
    scratch.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile("w", newline="", dir=scratch, prefix="attribution-", delete=False) as stream:
        staged = Path(stream.name)
        writer = csv.DictWriter(stream, fieldnames=["address", "class", "confidence", "our_definition", "evidence"],
                                lineterminator="\n")
        writer.writeheader()
        for r in records:
            evidence = r["evidence"]
            if keys is not None:
                evidence = {k: evidence[k] for k in keys if k in evidence}
            writer.writerow(dict(address=r["address"], **{"class": r["classification"]},
                                 confidence=r["confidence"], our_definition=r["our_definition"],
                                 evidence=json.dumps(evidence, sort_keys=True, separators=(",", ":"))))
    staged.chmod(0o644)
    staged.replace(path)


def write_csv(path, records):
    _write_rows(path, records, COMMITTED_EVIDENCE_KEYS)
    _write_rows(REPO / "out/attrib/attribution-evidence.csv", records, None)


def md(s):
    return str(s).replace("|", "\\|").replace("\n", " ").replace("`", "'")


def write_report(path, csv_path, records, data, matcher, runtime, console_tables=()):
    counts, sizes, confidence = collections.Counter(), collections.Counter(), collections.Counter()
    for r in records:
        kind = r["classification"]
        counts[kind] += 1
        sizes[kind] += r["size"]
        confidence[(kind, r["confidence"])] += 1
    missing = [r for r in records if r["classification"] == "missing"]
    extra_init = sorted(set(runtime.static_initializers) - set(matcher.L))
    extra_vt = sorted(set(data["retail"]["vtable_only"]) - set(matcher.L))
    all_rows = list(matcher.L.values())
    cited = [r for r in all_rows if r["status"] in ("done", "chosen", "stop")]
    skipped = [r for r in all_rows if r["status"] == "skip"]
    L = ["# RendDX9.dll functional attribution", "",
         "Generated by `tools/tracks/attrib/classify.py`; rerun instructions are in "
         "[the tool README](../../../tools/tracks/attrib/README.md). No engine source or input ledger was changed.", "",
         "## Scope and result", "",
         "The input ledger has %d RendDX9 rows: %d already cited (`done`/`chosen`/`stop`), %d existing skips, "
         "and %d `todo` rows. [attribution.csv](attribution.csv) contains exactly the %d todo addresses, once each. "
         "BF2.exe rows are outside this task." % (len(all_rows), len(cited), len(skipped), len(records), len(records)), "",
         "**`missing` is a conservative review queue, not a proven count of unwritten engine bodies.** "
         "Its low confidence means that these passes did not establish a safe pairing or exclusion. It can still "
         "contain unrecognized templates, generated wrappers, inlined helpers, upstream library code, and existing "
         "implementations that lack a strong cross-architecture anchor. Do not turn this count into a recovery "
         "percentage or a list of bodies to rewrite without inspecting the recorded evidence.", "",
         "| class (input todo rows only) | rows | bytes | high | medium | low / review |",
         "|---|---:|---:|---:|---:|---:|"]
    for kind in CLASSES:
        L.append("| %s | %d | %d | %d | %d | %d |" % (kind, counts[kind], sizes[kind],
                 confidence[(kind, "high")], confidence[(kind, "medium")], confidence[(kind, "low")]))
    L.extend(["| total | %d | %d | | | |" % (len(records), sum(r["size"] for r in records)), "",
              "Bytes are the input Ghidra body sizes, not unique executed-byte coverage. Existing source citation "
              "is a bookkeeping class: it does not establish completion, gameplay correctness or frontier removal.", "",
              "## Method and confidence", "",
              "1. Pin a private copy of the symbolized Mach-O x86_64 archive. Verify the original DLL is PE i386 "
              "at 0x10000000. Extract exact strings, import/API names, ordered direct calls, literal constants, "
              "indirect jumps, PE vtables and native pointer-table relocations in both `__text` and `__StaticInit`. "
              "Keep duplicate archive member names separate. Follow reachable instruction edges rather than "
              "using Ghidra's summed body size as an end address; a vtable leaf ends at its own return. "
              "Restart disassembly at entries where the linear listing lost synchronization. Unresolved "
              "indirect jumps retain the bounded listing, with the incomplete-CFG flag visible in CSV.",
              "2. Associate native symbols with brace-parsed definitions and ConsoleClass<__LINE__> symbols "
              "whose TU, actual source macro and Clang closing-line expansion agree. Seed from existing cited definitions; "
              "reject extreme size contradictions and tiny import helpers misclaimed as large engine methods. "
              "New own-address comments in the current source are `cited`, since the input ledger predates them.",
              "3. Pair unique shared literals and D3D/D3DX APIs, then propagate through explicit MSVC-order adapter "
              "tables (three agreeing paired slots and equal table length), bracketed callee gaps, unique ordered "
              "paired-callee lists and single unmatched callers with agreeing already-paired caller sets. "
              "The explicit adapter pass excludes Itanium vtables. A separate C++ table pass requires a paired "
              "owner's retail table reference and native relocation, three agreeing ordinary method identities, "
              "one measured slot delta and matching full lengths. Destructor positions never provide anchors. "
              "Conflicting candidate votes, disjoint shared literals, contradictory high `_r` own-file sites and "
              "stopping/partial source bodies block pair acceptance.",
              "4. Read EH FuncInfo/TryBlockMap tables reached through actual `__CxxFrameHandler` thunks. "
              "Classify outlined catch functions and exact this-adjusting/deleting-destructor wrappers as compiler "
              "machinery. Read CRT `_initterm` range arguments and its actual initializer pointers. "
              "Classify only short global-object destruction thunks registered by those initializers. "
              "Plain calls to `std::string` do not establish an STL function: require container diagnostics plus "
              "a throw in a bounded kernel without business strings/virtual calls, a recognized STL symbol, "
              "or the full instruction shape of an inspected `_Tree` rotation.",
              "Full reachable-instruction signatures additionally recognize subtree erase, iterator-range "
              "erase and vector insert-n kernels. Their sentinel/deallocation or diagnostic/capacity-growth "
              "anchors must agree; classification is never propagated to their arbitrary callees. "
              "Initializer-registered destruction wrappers are traced even when Ghidra omitted the initializer entry.",
              "5. Generated console constructors require their unique command literal, an actual retail CRT "
              "initializer caller, the current source macro invocation and the corresponding native class "
              "vtable installation in `__StaticInit`. Their constructors are inlined in the native TU initializer. "
              "For each console class separately, derive the ordinary-slot mapping from at least four exact "
              "retail metadata getter bodies and the native getters' symbol names. Require every delta and full "
              "table length to agree; never assume a destructor shift. %d console tables passed these checks. "
              "The deleting-destructor position is excluded from that ordinary-slot pass." % len(console_tables),
              "6. Other `inlined` helpers have a retail direct caller already paired with a complete native body, and "
              "the helper's unique literal/API relocation resides in that native caller. Editor scope requires "
              "a retained retail file site, a high `_r` own-file site, or a named TerrainEditable interface "
              "declaration; neighbour guesses alone never exclude game work.", "",
              "Category precedence is compiler/CRT/STL, then a cited or matched complete definition, then "
              "inlining, then editor scope, then missing. Thus an implemented editor function stays cited/matched, "
              "and an outlined catch does not become a false method match merely by sharing its message.", "",
              "`high` requires two independent agreeing anchors (for example two distinct unique literals, "
              "a table reference plus the matching compiler pattern, or an STL diagnostic plus its exception path). "
              "`medium` has one accepted identity/scope anchor; `low` is review only. File neighbours and "
              "caller-file votes are always marked `[INFERRED]`. Candidate conflicts remain visible in CSV evidence.", "",
              "Pairings identify source definitions and compiler roles; they are not new prototype/layout "
              "recovery. A native signature, chosen helper name or inferred file is not proof of the original "
              "PE ABI. Check caller instructions, argument preparation and return use before any subsequent code recovery.", "",
              "The committed CSV evidence keeps only anchors and probable_file. Full evidence is in "
              "`out/attrib/attribution-evidence.csv`, including all retained strings, direct callees, imports, indirect call "
              "operands, raw caller callsites, `_r` match/file evidence and probable-file basis. Calls from code "
              "Ghidra did not make into a function retain the callsite; a CRT/EH-table-derived caller is labeled "
              "as such. Virtual-call reachability and register-indirect targets are not fabricated.", "",
              "## Denominator gaps", "",
              "%d vtable-only entries were already outside the input ledger. The CRT initializer table additionally "
              "references %d entries outside it (%d overlap those vtable entries). These entries participate in "
              "evidence extraction where available but are not extra rows in the requested CSV. The %d discovered "
              "frame-handler thunks and other unlisted EH pieces also make a Ghidra-only function count unsuitable "
              "as a total-image denominator. An all-retail percentage requires reconciling these sets first." %
              (len(extra_vt), len(extra_init), len(set(extra_vt) & set(extra_init)), len(runtime.eh_handlers)), "",
              "## Reproduction and validation", "",
              "```sh",
              "export REND_EVIDENCE_ROOT=/path/to/rend-track",
              "python3 tools/tracks/attrib/extract.py",
              "python3 tools/tracks/attrib/classify.py",
              "python3 -m unittest discover -s tools/tracks/attrib -p 'test_*.py'",
              "python3 tools/tracks/rend_ledger.py --attribution \\",
              "  --export \"$REND_EVIDENCE_ROOT/out/rend-ghidra\" \\",
              "  --ledger out/attrib/ledger.csv --summary out/attrib/ledger-summary.md",
              "```", "",
              "The output overrides above keep generated ledger/summary changes out of the input documents. "
              "Compare an unflagged run with a flagged run: their status/definition columns must agree. "
              "`--attribution` only adds a class column and class summary table. " +
              ("No Code/CMake change: the C++ build gate is not applicable. " if "x86_image_sha256" not in data["inputs"]
               else "The required macOS build/audit and tool-validation results are recorded in "
                    "[attrib2 progress](codex/attrib2-progress.md). No engine source was changed. ") +
              "Runtime: coordinator; this task "
              "changes no renderer behavior and requests no runtime harness run.", "",
              "Input fingerprints:", ""])
    for key, value in sorted(data["inputs"].items()):
        L.append("- `%s`: `%s`" % (key, value))
    x86_rows = [r for r in records if r["evidence"].get("x86_pair")]
    if "x86_image_sha256" in data["inputs"]:
        L.extend(["", "## PDB-backed x86 pass (round 10)", "",
                  "The rebuilt RendDX9.dll is MSVC 2022 x86 with /Zi and /DEBUG per SOURCE.txt; "
                  "retail is VC7.1 x86 /O2 /GL. No optimization-level equivalence is assumed. "
                  "This is same-architecture functional attribution, not same-compiler or byte matching. "
                  "Ghidra 12.1.4 imports the local PDB in a private project. Its file/line maps and "
                  "symbol aliases associate instructions with current definitions; ambiguous folded "
                  "addresses and overloads are retained for review. PDB source paths alone never pair a retail function.", "",
                  "The existing passes and category precedence remain intact. The additional pass seeds "
                  "from cited/round-9 source identities, pairs unique literals/imports, then unique "
                  "displacement/virtual-slot/return-cleanup and constant signatures. It propagates through "
                  "bracketed call gaps, unique ordered paired-callee sequences and equal-length tables "
                  "with three agreeing slots. Stack displacements and indexed arrays are excluded from "
                  "field signatures. Conflicting candidates, disjoint shared literals and contradictory "
                  "high-confidence own-file sites block acceptance. Layout, slot and retn observations "
                  "count as one evidence family; graph observations count as one family. High confidence "
                  "requires two independent agreeing families or two distinct unique literals/imports. "
                  "Stopping/partial identities are listed below and never counted as complete matches.", "",
                  "Explicit return-cleanup contradictions and leaves misclaimed as multi-call methods "
                  "veto x86 pairings. One shared literal must have full-body support rather than only "
                  "appearing in an outer wrapper. Weak round-9 pairs and contradicted cited identities "
                  "are excluded from new propagation seeds; their prior classifications stay intact. "
                  "Explicit source-file split notes explain original-TU filename differences without "
                  "supplying identity votes. The PDB build description and current source hash above "
                  "identify distinct snapshots; source names and file/line maps are re-associated with "
                  "the current brace-parsed definitions. The audit withholds every fifth eligible seed, "
                  "retaining unresolved rows and exact instruction-identical, call-free aliases separately.", "",
                  "%d todo rows have an x86 pairing (including prior anchors); %d rows name the PDB-backed "
                  "definition as their classification basis. Full PDB maps, aliases, pairs and rejected "
                  "votes remain under `out/attrib/`." % (len(x86_rows), sum("[PDB " in r["our_definition"] for r in records)), "",
                  "Re-run after the baseline extraction with the headless import and x86 extraction "
                  "commands in the tool README, then `python3 tools/tracks/attrib/classify.py "
                  "--x86-features out/attrib/x86-features.json`. Input hashes above pin both images and the PDB."])
        summary = data.get("x86_summary", {})
        if "exported_functions" in summary:
            L.extend(["", "Ghidra exported %d function entries and %d source-line mappings. After "
                      "link-thunk resolution the cache has %d functions, %d unique source associations "
                      "and %d ambiguous associations. %d direct-jump link thunks and %d calls through "
                      "exact one-jump IAT thunks were resolved. The API thunk itself is excluded from "
                      "business-body API uniqueness counts on both sides; APIs do not propagate through "
                      "ordinary helpers." % tuple(summary[k] for k in ("exported_functions", "source_maps",
                          "functions", "source_associations", "ambiguous_associations", "link_thunks",
                          "resolved_import_calls")), ""])
        L.extend(["", "The x86 graph starts with %d eligible source anchors. Seed exclusions below "
                  "are conservative propagation decisions, not edits to the original cited/round-9 "
                  "classification. Their addresses and reasons are retained with the full pairing evidence." %
                  summary.get("seeds", 0), ""])
        for reason, count in sorted(summary.get("seed_rejections", {}).items()):
            L.append("- %s: %d" % (reason, count))
    L.extend(["", "## Remaining source frontiers", "",
              "The input ledger's %d `stop` rows stay untouched and outside this CSV. They are listed below "
              "as input bookkeeping, not an assertion that all still stop in the newer source tree. A normal "
              "ledger regeneration supplies the current-source statuses. The following todo rows have a brace-parsed "
              "stopping/partial source definition with an own-address citation or an accepted x86 identity; they were not accepted as "
              "complete matches:" % sum(r["status"] == "stop" for r in all_rows), ""])
    frontier_rows = [r for r in records if r["evidence"].get("known_source_frontiers")]
    for r in frontier_rows:
        L.append("- `%s` (%s): %s" % (r["address"], r["classification"], "; ".join(
            md(v) for v in r["evidence"]["known_source_frontiers"])))
    if not frontier_rows:
        L.append("- None newly identified by the source parser; declaration-only/unknown-signature boundaries "
                 "can still be in the missing queue.")
    L.extend(["", "Input ledger stops (not reclassified):", "",
              "| address | input source symbol | file |", "|---|---|---|"])
    for r in sorted((r for r in all_rows if r["status"] == "stop"), key=lambda r: r["address"]):
        L.append("| `%s` | %s | %s |" % (r["address"], md(r["symbol"]), md(r["file"])))
    L.extend(["", "## 50 largest missing functions", "",
              "These are investigation priorities, not recovery prescriptions. In particular, the two DXErr "
              "lookup tables near 0x1005F380/0x1007102E are upstream DirectX helper candidates, not evidence "
              "of missing renderer algorithms. No unsupported extra class was invented for them.", "",
              "| address | bytes | probable file | retained string / known symbol |", "|---|---:|---|---|"])
    for r in sorted(missing, key=lambda r: (-r["size"], r["address"]))[:50]:
        strings = r["evidence"]["strings"]
        hint = "; ".join(strings[:2])[:160] if strings else r["evidence"]["retail_name"]
        L.append("| `%s` | %d | %s | %s |" % (r["address"], r["size"], md(r["probable_file"]), md(hint)))
    L.extend(["", "## Missing queue grouped by probable file", "",
              "Every missing address appears below with its Ghidra size in parentheses. File grouping is a "
              "placement hint; the evidence cell records whether it is an own site, an older-build match or "
              "an inferred caller/neighbour placement. `(unknown)` is intentional."])
    groups = collections.defaultdict(list)
    for r in missing:
        groups[r["probable_file"]].append(r)
    for file, group in sorted(groups.items()):
        L.extend(["", "### %s" % md(file), "",
                  "%d functions, %d bytes. Bases: %s." % (len(group), sum(r["size"] for r in group),
                  "; ".join("%s (%d)" % (basis, n) for basis, n in sorted(
                      collections.Counter(r["file_basis"] for r in group).items()))), ""])
        for i in range(0, len(group), 6):
            L.append("- " + ", ".join("`%s` (%d)" % (r["address"], r["size"]) for r in group[i:i + 6]))
    path.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile("w", dir=REPO / "out/attrib", prefix="attribution-report-", delete=False) as stream:
        stream.write("\n".join(L) + "\n")
        staged = Path(stream.name)
    staged.chmod(0o644)
    staged.replace(path)
