#!/usr/bin/env python3
"""Withhold a deterministic subset of cited pairs and audit x86 predictions.

This is a calibration check against existing source identities, not an independent
proof of retail behavior. It never writes the committed classification.
"""
import argparse
import collections
import json
from pathlib import Path

from classify import Matcher, hx, rows
from extract import REPO
from x86 import X86Matcher


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--features", type=Path, default=REPO / "out/attrib/features.json")
    ap.add_argument("--x86-features", type=Path, default=REPO / "out/attrib/x86-features.json")
    ap.add_argument("--pairs", type=Path, default=REPO / "out/attrib/pairs.json")
    ap.add_argument("--out", type=Path, default=REPO / "out/attrib/x86-holdout.json")
    a = ap.parse_args()
    data = json.loads(a.features.read_text())
    baseline = Matcher(data, rows(REPO / "docs/tracks/rend/ledger.csv"),
                       rows(REPO / "docs/tracks/rend/rdebug-map.csv"))
    baseline.pairs = {int(k, 16): v for k, v in json.loads(a.pairs.read_text())["pairs"].items()}
    baseline.refresh_reverse()
    matcher = X86Matcher(json.loads(a.x86_features.read_text()), baseline)
    # Remove every fifth sorted source anchor. No retail source-citation shortcut
    # is used by the prediction passes, so the hidden identity cannot reseed itself.
    truth = {k: matcher.pairs[k] for k in sorted(matcher.pairs)[::5]}
    for k in truth:
        del matcher.pairs[k]
        matcher.L[k] = dict(matcher.L[k], status="todo")
    matcher.refresh()
    matcher.run()
    counts = collections.Counter()
    predictions = []
    for k, expected in sorted(truth.items()):
        predicted = matcher.pairs.get(k)
        if predicted:
            correct = predicted["x86"] == expected["x86"]
            alias = (not correct and matcher.G[predicted["x86"]]["calls"] == [] and
                     [(m, o) for _, m, o in matcher.G[predicted["x86"]]["insns"]] ==
                     [(m, o) for _, m, o in matcher.G[expected["x86"]]["insns"]])
            counts[("correct" if correct else "identical_code_alias" if alias else "incorrect") +
                   "_" + predicted["confidence"]] += 1
            predictions.append(dict(address=hx(k), expected=matcher.label(expected["x86"]),
                                     predicted=matcher.label(predicted["x86"]), correct=correct,
                                     identical_code_alias=alias,
                                     confidence=predicted["confidence"], evidence=predicted["evidence"]))
        else:
            counts["unresolved"] += 1
    result = dict(withheld=len(truth), counts=dict(sorted(counts.items())), predictions=predictions)
    a.out.write_text(json.dumps(result, sort_keys=True, indent=2) + "\n")
    print("x86 holdout:", json.dumps(result["counts"], sort_keys=True), "withheld:", len(truth))
    if any(k.startswith("incorrect") for k in counts):
        raise SystemExit(1)


if __name__ == "__main__":
    main()
