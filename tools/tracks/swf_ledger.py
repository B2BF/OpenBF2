#!/usr/bin/env python3
"""Generate the S1 ledger and summary from metadata-only alignment output.

[CHOSEN, not recovered] bookkeeping, not an implementation or equivalence claim.
Run swf_align.py first; all seven contract columns precede analysis columns.
"""

import argparse
from collections import Counter, defaultdict
import csv
import importlib.util
import json
from pathlib import Path
import sys

SPEC = importlib.util.spec_from_file_location("swf_align", Path(__file__).with_name("swf_align.py"))
sa = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = sa
SPEC.loader.exec_module(sa)
FIELDS = ["image", "address", "size", "symbol", "file", "status", "commit",
          "base_rev", "match", "confidence", "anchor", "boundary", "reason",
          "csv_size", "ninstr", "file_inferred", "change_reason"]
CLASSES = ("unchanged", "changed", "new", "inline", "unknown")


def ledger_rows(aligned, static=()):
    rows = [{**r, "image": "SwiffPlayer.dll", "status": "todo", "commit": ""}
            for r in [*aligned, *static]]
    seen = set()
    for r in rows:
        a = int(r["address"], 16)
        if a in seen:
            raise ValueError("duplicate ledger address")
        seen.add(a)
        if r["match"] not in CLASSES:
            raise ValueError("invalid classification")
    return sorted(rows, key=lambda r: int(r["address"], 16))


def apply_completion_overrides(rows, path):
    """Apply reviewed S2 identifiers/status without changing S1 classifications."""
    if not path.exists():
        return rows
    indexed = {r["address"]: r for r in rows}
    seen = set()
    for override in sa.read_csv(path):
        address = override["address"]
        if address not in indexed or address in seen:
            raise ValueError("unknown or duplicate completion override address")
        seen.add(address)
        row = indexed[address]
        status = override["status"]
        if status not in ("done", "stop", "chosen", "unattributed") or not override["symbol"]:
            raise ValueError("completion override requires done/stop/chosen/unattributed and symbol")
        if status in ("done", "stop") and not override["file"]:
            raise ValueError("recovery/stop override requires a source file")
        if status in ("chosen", "unattributed") and not override.get("reason"):
            raise ValueError("non-recovery disposition requires a reason")
        if status == "unattributed" and any(override[k] != row[k] for k in ("symbol", "file")):
            raise ValueError("unattributed disposition must preserve existing attribution")
        row.update({k: override[k] for k in ("symbol", "file", "status")})
        if override.get("reason"):
            row["reason"] += ";" + override["reason"]
    return rows


def bucket(n):
    for limit, label in ((10, "1-10"), (30, "11-30"), (100, "31-100"),
                         (300, "101-300"), (1000, "301-1000")):
        if n <= limit:
            return label
    return ">1000"


def tu_runs(rows):
    """Contiguous runs, never a min/max envelope across unrelated translation units."""
    runs = []
    for r in rows:
        f = r["file"] or "(unplaced)"
        if runs and runs[-1][2] == f:
            runs[-1][1] = r["address"]
            runs[-1][3] += 1
        else:
            runs.append([r["address"], r["address"], f, 1])
    return runs


def summary(rows, reviews, boundaries, citations, audits=None):
    audits = audits or {}
    b = [r for r in rows if 0x1006DB10 <= int(r["address"], 16) <= 0x100AA56F]
    e = [r for r in rows if int(r["address"], 16) >= 0x100D0000]
    counts = Counter(r["match"] for r in b)
    named = sum(not r["symbol"].startswith("FUN_") for r in b)
    joint = sum(r["match"] != "unknown" and not r["symbol"].startswith("FUN_") for r in b)
    target = (85 * len(b) + 99) // 100
    sample = [r for r in reviews if r.get("spot_check") == "yes"]
    errors = sum(r.get("error") == "yes" for r in sample)
    lines = ["# SwiffPlayer S1 ledger", "",
             "[CHOSEN, not recovered] analysis tooling. S1 classifications remain independent "
             "of subsequent recovery. Reviewed S2 completion overrides preserve identifiers "
             f"and status ({sum(r['status'] == 'done' for r in rows)} done); commit fields remain blank.", "",
             "## Coverage and classification", "",
             f"**Round-two 85% acceptance target: {'met' if joint >= target else 'NOT MET'}.** "
             f"At least {target}/{len(b)} B rows must be both named and classified. "
             f"Current: {joint} ({100 * joint / len(b):.1f}%); "
             f"{max(target - joint, 0)} more are needed. Unknown must fall below 15%.", "",
             "| Class | B rows | Share |", "|---|---:|---:|"]
    lines += [f"| {k} | {counts[k]} | {100 * counts[k] / len(b):.1f}% |" for k in CLASSES]
    lines += ["", f"Named: {named}; classified: {len(b) - counts['unknown']}; "
              f"both: {joint}. E bookkeeping: {len(e)} rows, counted separately.", "",
              "Naming and body classification are independent. Unchanged requires an explicit "
              "source/decompile comparison of structure, calls and literals under the release "
              "configuration; it is not a matching-build claim. Changed records an observed "
              "difference, with a primary reason code. New means no r586 function counterpart "
              "for the reviewed role. Inline covers emitted templates, defaults and compiler "
              "veneers; inferred template roles leave type arguments open and do not assert "
              "unchanged behavior. Unknown is retained when the available evidence is insufficient.", "",
              "Template symbols identify an operation and an observed stride where available. "
              "They are not recovered template arguments or original DICE symbol spellings. "
              "Header placement is a definition location, not a recovered originating TU. "
              "All original source-path placement remains [INFERRED]; base/ is the approximate "
              "May 2004 release.", "",
              "## Reviewed sample", "",
              f"{len(sample)} purposive, same-analyst source/decompile comparisons; "
              f"{errors} initial naming/classification errors corrected "
              f"({100 * errors / len(sample) if sample else 0:.1f}%). "
              "This is a self-audit, not an independent random sample or a population error bound. "
              "Round one found two false anchors (String constructor and ref_counted::drop_ref). "
              "Round two corrected four reversed char/wchar_t overload labels and withdrew six "
              "unsupported names in the altered character root-slot block. "
              "Unresolved comparisons remain unknown. Rule exemplars are part of this review "
              "set; automatically propagated instances are not counted as hand reviews.", "",
              "| Instruction bucket | B rows | Reviews | Unknown |", "|---|---:|---:|---:|"]
    row_at = {r["address"]: r for r in b}
    for label in ("1-10", "11-30", "31-100", "101-300", "301-1000", ">1000"):
        group = [r for r in b if bucket(int(r["ninstr"])) == label]
        checked = sum(r["address"] in row_at and bucket(int(row_at[r["address"]]["ninstr"])) == label for r in sample)
        lines.append(f"| {label} | {len(group)} | {checked} | {sum(r['match'] == 'unknown' for r in group)} |")
    extra_reviews = [r['address'] for r in sample if r['address'] not in row_at]
    if extra_reviews:
        lines += ["", "Additional reviewed export entries outside the original inventory keys: "
                  + ", ".join(extra_reviews) + ". They are excluded from the bucket counts above."]
    order = audits.get("order", [])
    seq = audits.get("sequences", [])
    slots = audits.get("slots", [])
    templates = audits.get("templates", [])
    structural = audits.get("structural", [])
    bindings = audits.get("bindings", [])
    lines += ["", "## Method results", "",
              f"Link-order audit: {len(order)} adjacent out-of-line anchor pairs, "
              f"{sum('inversion' in r['decision'] for r in order)} inversions. "
              f"Gapped DP emitted {len(seq)} proposals; "
              f"{sum(r['decision'] == 'supported-name-proposal' for r in seq)} meet the "
              "literal/callee and local-margin threshold. Others remain candidates. "
              "Class-body definitions are excluded from this source sequence. A retail gap "
              "does not prove DICE-new code; a source gap does not prove inlining.", "",
              f"Reviewed vtable maps cover {len({r['vtable'] for r in slots})} primary tables "
              f"and {len(slots)} slots; {sum(r['decision'] == 'unresolved-slot' for r in slots)} "
              "slots remain explicit gaps. The export has 238 candidates, of which 73 contain "
              "B entries. Secondary/split tables and DICE-added slots preclude count-only "
              "class assignment. vtwrites are constructor/destructor candidates, not proof of which.", "",
              f"Exact-shape template audit: {len(templates)} instance/rule pairs, "
              f"{sum(r['decision'] == 'rejected-variant' for r in templates)} rejected by guards. "
              f"Structural field/control contracts added {len(structural)} inferred roles. "
              f"Explicit native-value/key/set-member triples identify {len(bindings)} bindings. "
              "Neither shape hashes nor member bindings imply unchanged bodies.", "",
              f"Manual sweep remains incomplete: {sum(r['match'] == 'unknown' and int(r['ninstr']) > 30 for r in b)} "
              "unknown rows exceed 30 instructions. s1-open.csv orders this remaining work "
              "by size. The 85% target must not be met by assigning names from order or size alone.", "",
              "## Per-TU attribution and retail ranges", "",
              "Rows include inferred neighbourhood attribution. Ranges below are min/max row-start "
              "envelopes, not claims that every intervening COMDAT belongs to the TU. Inline headers "
              "and unplaced code are reported separately. s1-order.csv records each tested anchor island.", "",
              "| File | B rows | Named | Unknown | First | Last | Order inversions |",
              "|---|---:|---:|---:|---|---|---:|"]
    groups = defaultdict(list)
    for r in b:
        groups[r["file"] or "(unplaced)"].append(r)
    for file, group in sorted(groups.items()):
        inversions = sum(r["file"] == file and 'inversion' in r["decision"] for r in order)
        lines.append(f"| {file} | {len(group)} | {sum(not r['symbol'].startswith('FUN_') for r in group)} | "
                     f"{sum(r['match'] == 'unknown' for r in group)} | {group[0]['address']} | "
                     f"{group[-1]['address']} | {inversions} |")
    lines += ["", "## DICE-new clusters", "",
              "Only reviewed new rows count below. Intervening helpers and unchanged/changed functions "
              "do not inherit a cluster classification. Endpoints are first/last row starts.", "",
              "| First | Last | File | New rows |", "|---|---|---|---:|"]
    clusters = []
    for r in (r for r in b if r["match"] == "new"):
        if clusters and clusters[-1][-1]["file"] == r["file"] and int(r["address"], 16) - int(clusters[-1][-1]["address"], 16) <= 0x1000:
            clusters[-1].append(r)
        else:
            clusters.append([r])
    for group in clusters:
        lines.append(f"| {group[0]['address']} | {group[-1]['address']} | {group[0]['file']} | {len(group)} |")
    lines += ["", "## Changed-reason counts", "",
              "One primary observed reason per changed row; additional differences remain in reason.", "",
              "| Reason | B rows |", "|---|---:|"]
    reasons = Counter(r.get("change_reason", "") for r in b if r["match"] == "changed")
    lines += [f"| {reason} | {reasons[reason]} |" for reason in sa.sequence.CHANGE_REASONS]
    u, c, n, i, q = (counts[k] for k in CLASSES)
    known_cost = (u + i) / 400 + (c + n) / 150
    low, high = known_cost + q / 400, known_cost + q / 120
    lines += ["", "## Revised effort (PROBE section 5)", "",
              f"Applying the probe's rates to the measured split: ({u} unchanged + {i} inline)/400 "
              f"+ ({c} changed + {n} new)/150 = {known_cost:.2f} agent-days. "
              f"Charging {q} unknown at 400/day versus 120/day gives **{low:.1f}-{high:.1f} "
              "agent-days of function work**. Add the probe's 1-2 tooling days and 2-3 menu-validation "
              f"days: **{low + 3:.1f}-{high + 5:.1f} agent-days total**, excluding incomplete E "
              "coverage and separate vendoring. Inline functions are not free.", "",
              "Per-function assumptions, using an eight-hour agent-day: 1.2 minutes for "
              "unchanged/inline, 3.2 minutes for changed/new, 1.2-4.0 minutes for unknown. "
              f"Weighted function-work average: {low * 480 / len(b):.1f}-{high * 480 / len(b):.1f} minutes. "
              "These are planning rates, not measured recovery timings. Large VM/text/shape bodies "
              "can cost much more. S2 must calibrate them; the current split cannot justify a 65/35 extrapolation.", "",
              "## Boundaries, provenance and S2", "",
              f"All 2164 B inventory keys plus {len(e)} E rows are retained. "
              "size is the export body size where present; csv_size is the padded inventory extent; "
              "ninstr keeps the PROBE count. 0x10095609 is interpreter jump-table data, retained as "
              "an unknown inventory artifact. s1-boundaries.csv retains the 48 jump-thunk decisions "
              "and 28 extra Ghidra starts; those starts do not become additional B keys.", "",
              f"Boundary audit rows: {len(boundaries)}; tree citations: {len(citations)}. "
              "Citation entry checks are separate from function identity. The export contains "
              "2142 B starts and only 79 E starts. s1-inputs.json fingerprints current input "
              "availability, decompile text, rules and tooling. S1 name alignment alone claims "
              "no recovered implementation. S2 uses a verified read-only Ghidra project copy; "
              "see s2-progress.md for its evidence, completed subset and validation limits.", "",
              "s2-scope.csv includes the button TU (0x100A1BE0-0x100A3B70), root/sprite/button "
              "virtual targets, mouse/event/action helpers, their direct B callees and the interpreter. "
              "Shared inherited targets occur once, with all scope reasons. Unknown slot/body identities "
              "remain visible. Indirect dispatch and callback effects still require instruction review; "
              "the scope is an explicit pilot list, not a proven whole-program call-graph closure.", "",
              "The retail sprite mouse pass snapshots child count, then reloads the display-list "
              "storage while iterating. S2 must examine callback-driven mutation and the quit dialog "
              "without silently introducing an empty-list guard. No runtime path was exercised.", "",
              "## Reproduction", "", "```sh",
              "python3 -I tools/tracks/swf_align.py",
              "python3 -I tools/tracks/swf_ledger.py",
              "python3 -I tools/tracks/test_swf_ledger.py",
              "python3 -I tools/tracks/swf_ledger.py --require-coverage",
              "```", "",
              "The optional coverage gate fails while fewer than 85% are both named and classified. "
              "Unit tests validate tooling contracts, not recovery fidelity or gameplay. No C++ builds, "
              "game runs, installs, network, commits or pushes. All production frontiers remain.", ""]
    return "\n".join(lines)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--alignment", type=Path, default=sa.REPO / "out/swf-align/align.csv")
    p.add_argument("--ledger", type=Path, default=sa.REPO / "docs/tracks/swf/ledger.csv")
    p.add_argument("--summary", type=Path, default=sa.REPO / "docs/tracks/swf/ledger-summary.md")
    p.add_argument("--require-coverage", action="store_true", help="fail unless 85%% of B rows are both named and classified")
    p.add_argument("--completion-overrides", type=Path, default=None,
                   help="one override file; default: every docs/tracks/swf/*-ledger-overrides.csv (per-tag files)")
    args = p.parse_args()
    directory = args.alignment.parent
    aligned = sa.read_csv(args.alignment)
    if len(aligned) != 2164:
        raise ValueError("expected 2164 part-B rows")
    static_path = sa.REPO / "docs/tracks/swf/s1-static.csv"
    rows = ledger_rows(aligned, sa.read_csv(static_path) if static_path.exists() else [])
    override_paths = ([args.completion_overrides] if args.completion_overrides else
                      sorted((sa.REPO / "docs/tracks/swf").glob("*-ledger-overrides.csv")))
    for override_path in override_paths:   # an address may appear in only one file (checked per file)
        rows = apply_completion_overrides(rows, override_path)
    sa.write_csv(args.ledger, FIELDS, rows)
    reviews = sa.read_csv(sa.REVIEWS) if sa.REVIEWS.exists() else []
    boundaries = sa.read_csv(directory / "boundaries.csv")
    citations = sa.read_csv(directory / "citations.csv")
    # These exports contain addresses/identifiers only and are safe to retain.
    sa.write_csv(sa.REPO / "docs/tracks/swf/s1-boundaries.csv", ["address", "decision"], boundaries)
    sa.write_csv(sa.REPO / "docs/tracks/swf/s1-citations.csv", ["address", "references", "entry", "aligned", "tree_candidates", "decision"], citations)
    vtables = sa.read_csv(directory / "vtables.csv")
    sa.write_csv(sa.REPO / "docs/tracks/swf/s1-vtables.csv", ["vtable", "class", "known_slots", "slots", "declared_virtuals", "writers", "decision"], vtables)
    inputs = json.loads((directory / "provenance.json").read_text())
    (sa.REPO / "docs/tracks/swf/s1-inputs.json").write_text(json.dumps(inputs, sort_keys=True, indent=2) + "\n")
    args.summary.parent.mkdir(parents=True, exist_ok=True)
    audits = {}
    for name in ("sequences", "order", "slots", "templates", "structural", "bindings", "open", "s2-scope"):
        path = directory / (name + ".csv")
        audits[name] = sa.read_csv(path)
        target = "s2-scope.csv" if name == "s2-scope" else "s1-" + name + ".csv"
        with path.open(newline="") as f:
            fields = next(csv.reader(f))
        sa.write_csv(sa.REPO / "docs/tracks/swf" / target, fields, audits[name])
    args.summary.write_text(summary(rows, reviews, boundaries, citations, audits))
    print(f"ledger={len(rows)} classifications={dict(Counter(r['match'] for r in rows))}")
    named_classified = sum(r["match"] != "unknown" and not r["symbol"].startswith("FUN_") for r in aligned)
    print(f"coverage={named_classified}/{len(aligned)} ({100 * named_classified / len(aligned):.1f}%) "
          f"target={'PASS' if named_classified >= 1840 else 'FAIL'} (1840 rows)")
    if args.require_coverage and named_classified < 1840:
        raise SystemExit(1)


if __name__ == "__main__":
    main()
