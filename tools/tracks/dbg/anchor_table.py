#!/usr/bin/env python3
"""anchor_table.py : docs/tracks/dbg/aidll_r_functions.csv - every AIDLL_r.dll function that carries a
__FILE__ path (assert sites), plus every other AIDLL_r function mapped to libaidll.so 1.50 with high
confidence (shared unique strings, shared rare float constants, or call propagation that agrees with
the TU). No assert text is copied: addresses, TU paths, assert line numbers and symbols only."""
import os as _os
_ROOT = _os.path.abspath(_os.path.join(_os.path.dirname(__file__), '..', '..', '..'))
# Ghidra exports live in <repo>/out/dbg-ghidra (never committed); evidence images outside the repo
# ($BF2_DEBUG_BUILDS_DIR for the DICE *_r builds, $BF2_LINUX_SERVER_DIR, $BF2_WIN_GAME_DIR).
import csv, glob, os, re
O = _ROOT + '/out/dbg-ghidra/'
REPO = _ROOT + '/'
PRE = 'Code/BF2/AI/AIDLL/AI/'
lines = {}
for p in glob.glob(O + 'aidll_r/dec/*.c'):
    a = int(os.path.basename(p)[:-2], 16)
    s = open(p, encoding='utf-8', errors='replace').read()
    ls = re.findall(r'FUN_10052160\(\s*\d+\s*,\s*\w+\s*,\s*(0x[0-9a-fA-F]+|\d+)\s*,', s)
    lines[a] = sorted({int(x, 0) for x in ls})
rows = []
for r in csv.DictReader(open(O + 'aidll_r/map2.tsv'), delimiter='\t'):
    a = int(r['r_addr'], 16)
    anchored = a in lines
    mapped = bool(r['elf_addr'])
    tu = r['tu']
    tree = r['tree_file'][len(PRE):] if r['tree_file'].startswith(PRE) else r['tree_file']
    if mapped and r['how'] == 'calls' and tu and not r['tree_file'].endswith('/' + tu):
        mapped = False                       # call propagation disagreeing with the TU: dropped
    if not anchored and not mapped:
        continue
    if mapped:
        if not tu:
            status = 'match'
        elif r['tree_file'].endswith('/' + tu):
            status = 'match'
        else:
            status = 'tu-differs'
    else:
        status = 'unmapped'
    rows.append({'aidll_r_addr': r['r_addr'], 'aidll_r_size': r['r_size'], 'file_tu': tu,
                 'assert_lines': ' '.join(str(x) for x in lines.get(a, [])),
                 'elf150_addr': r['elf_addr'] if mapped else '', 'elf150_symbol': r['elf_symbol'] if mapped else '',
                 'tree_file': tree if mapped else '', 'how': r['how'] if mapped else '', 'status': status})
cols = ['aidll_r_addr', 'aidll_r_size', 'file_tu', 'assert_lines', 'elf150_addr', 'elf150_symbol', 'tree_file', 'how', 'status']
with open(REPO + 'docs/tracks/dbg/aidll_r_functions.csv', 'w', newline='') as f:
    w = csv.DictWriter(f, cols)
    w.writeheader()
    w.writerows(rows)
import collections
print(len(rows), collections.Counter(r['status'] for r in rows))
