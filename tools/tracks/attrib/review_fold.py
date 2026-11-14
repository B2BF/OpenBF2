#!/usr/bin/env python3
"""Folds the review rounds' decisions into the attribution's open list.

attribution.csv classifies rows from evidence only; the review progress files
(docs/tracks/rend/codex/*-progress.md) hold the decisions agents made row by row: STL/compiler
instances, unreachable functions, citations, fixes, recoveries, and rows deferred or handed off to
another file's owner. This script reads both and reports what is still genuinely open.

A row counts as settled when attribution.csv already gives a non-missing class or a review recorded
it as cited/fixed/recovered/stl/compiler/unreachable/upstream. Rows only recorded as deferred/handoff/
pending, or never mentioned, stay open. When a later review settles a row an earlier one deferred,
the settlement wins.

Usage: review_fold.py [--queue OUT.csv]   (prints a summary; --queue writes the open rows)
"""
import argparse
import csv
import glob
import os
import re

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..', '..'))
ADDR = re.compile(r'\b0x(10[0-9A-Fa-f]{6})\b')
SETTLED = ('cited', 'fixed', 'recovered', 'stl', 'compiler', 'unreachable', 'upstream', 'crt', 'inlined',
           'named', 'confirmed', 'corrected', 'new named')
OPEN = ('deferred', 'handoff', 'hand-off', 'pending', 'blocked', 'outside', 'owner', 'partial')


def kind_of(text):
    t = text.lower()
    for k in OPEN:
        if k in t:
            return 'open'
    for k in SETTLED:
        if k in t:
            return 'settled'
    return None


def scan(path, decisions):
    for line in open(path, encoding='utf-8', errors='replace'):
        # unit lines: "cited 3 (0x..., 0x...), stl/compiler 11 (0x...)" -> class before each group
        for m in re.finditer(r'([A-Za-z/ -]{3,40}?)\s+\d+\s*\(([^)]*)\)', line):
            k = kind_of(m.group(1))
            if k:
                for a in ADDR.findall(m.group(2)):
                    decisions.setdefault(a.upper(), []).append(k)
        # table rows: every address in the row takes the class named in the row's other cells
        # ("| 0x1000... | class | ...", "| 0x... | 57 | cited | ...", "| group | 141 | stl | 0x.., 0x.. |")
        if line.lstrip().startswith('|'):
            cells = [c.strip() for c in line.strip().strip('|').split('|')]
            addrs = [a for c in cells for a in ADDR.findall(c)]
            if addrs:
                k = None
                for c in cells:
                    if ADDR.search(c) and len(ADDR.findall(c)) > 1:
                        continue                      # an address list, not a class cell
                    k = kind_of(re.sub(ADDR, '', c)[:60])
                    if k:
                        break
                if k:
                    for a in addrs:
                        decisions.setdefault(a.upper(), []).append(k)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--queue')
    args = ap.parse_args()
    rows = list(csv.DictReader(open(os.path.join(ROOT, 'docs/tracks/rend/attribution.csv'))))
    decisions = {}
    for p in sorted(glob.glob(os.path.join(ROOT, 'docs/tracks/rend/codex/*-progress.md'))):
        scan(p, decisions)
    open_rows, settled_by_review, still = [], 0, 0
    for r in rows:
        if r['class'] != 'missing':
            continue
        a = r['address'][2:].upper()
        d = decisions.get(a, [])
        if 'settled' in d:
            settled_by_review += 1
        else:
            open_rows.append((r['address'], 'deferred' if d else 'unreviewed'))
    total = len(rows)
    missing = sum(1 for r in rows if r['class'] == 'missing')
    print('attribution rows %d, missing %d; settled by reviews %d; open %d (deferred %d, unreviewed %d)' % (
        total, missing, settled_by_review, len(open_rows),
        sum(1 for o in open_rows if o[1] == 'deferred'), sum(1 for o in open_rows if o[1] == 'unreviewed')))
    if args.queue:
        with open(args.queue, 'w') as f:
            f.write('address,state\n')
            for a, s in open_rows:
                f.write('%s,%s\n' % (a, s))


if __name__ == '__main__':
    main()
