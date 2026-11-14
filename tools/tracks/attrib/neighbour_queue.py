#!/usr/bin/env python3
"""Builds review queues for missing RendDX9 rows that have no probable file.

Each such row is placed by its address neighbours in docs/tracks/rend/attribution.csv: the nearest
attributed function below and above it. MSVC links a translation unit's functions contiguously, so a
row bracketed by one file is very likely that file's ("bracketed"); otherwise both neighbour files
are listed ("between"). Both are [INFERRED] hints for the reviewer, never placement evidence.

Usage: neighbour_queue.py <out-dir> <groups>
Writes <out-dir>/neighbour-<n>.queue (address,size,hint,low_file,high_file), split into <groups>
address-contiguous groups of about equal size, so each group stays within neighbouring TUs.
"""
import bisect
import csv
import json
import os
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..', '..'))


def file_of(row):
    ev = json.loads(row['evidence']) if row['evidence'] else {}
    p = ev.get('probable_file')
    if p and p != '(unknown)':
        return p.replace('Code/BF2/', '')
    d = row['our_definition']
    if d and ':' in d:
        return d.split(':')[0].replace('Code/BF2/', '')
    return None


def main():
    out_dir, groups = sys.argv[1], int(sys.argv[2])
    rows = list(csv.DictReader(open(os.path.join(ROOT, 'docs/tracks/rend/attribution.csv'))))
    sizes = {}
    ledger = os.path.join(ROOT, 'docs/tracks/rend/ledger.csv')
    if os.path.exists(ledger):
        for r in csv.DictReader(open(ledger)):
            a, s = r.get('address') or r.get('addr'), r.get('size')
            if a and s:
                sizes[a.lower()] = s
    known = sorted((int(r['address'], 16), file_of(r)) for r in rows if file_of(r))
    addrs = [a for a, _ in known]
    todo = []
    for r in rows:
        if r['class'] != 'missing' or file_of(r):
            continue
        a = int(r['address'], 16)
        i = bisect.bisect_left(addrs, a)
        lo = known[i - 1][1] if i > 0 else ''
        hi = known[i][1] if i < len(known) else ''
        kind = 'bracketed' if lo and lo.lower() == hi.lower() else 'between'
        todo.append((a, sizes.get(r['address'].lower(), ''), kind, lo, hi))
    todo.sort()
    os.makedirs(out_dir, exist_ok=True)
    per = (len(todo) + groups - 1) // groups
    for g in range(groups):
        part = todo[g * per:(g + 1) * per]
        if not part:
            continue
        with open(os.path.join(out_dir, 'neighbour-%d.queue' % (g + 1)), 'w') as f:
            f.write('address,size,hint,low_file,high_file\n')
            for a, s, k, lo, hi in part:
                f.write('0x%08X,%s,%s,%s,%s\n' % (a, s, k, lo, hi))
        print('neighbour-%d: %d rows, 0x%08X..0x%08X, bracketed %d' % (
            g + 1, len(part), part[0][0], part[-1][0], sum(1 for p in part if p[2] == 'bracketed')))


if __name__ == '__main__':
    main()
