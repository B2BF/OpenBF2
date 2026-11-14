#!/usr/bin/env python3
"""fstr.py <func_addr>... : list strings referenced by each AIDLL_r function (from strxrefs.tsv)."""
import os as _os
_ROOT = _os.path.abspath(_os.path.join(_os.path.dirname(__file__), '..', '..', '..'))
# Ghidra exports live in <repo>/out/dbg-ghidra (never committed); evidence images outside the repo
# ($BF2_DEBUG_BUILDS_DIR for the DICE *_r builds, $BF2_LINUX_SERVER_DIR, $BF2_WIN_GAME_DIR).
import sys, collections
D = _ROOT + '/out/dbg-ghidra/aidll_r/'
by = collections.defaultdict(list)
names = {}
for line in open(D + 'strxrefs.tsv', encoding='utf-8', errors='replace'):
    p = line.rstrip('\n').split('\t')
    if len(p) < 5:
        continue
    by[p[0].lower()].append(p[4])
    names[p[0].lower()] = p[1]
full = '-full' in sys.argv
for a in sys.argv[1:]:
    if a.startswith('-'):
        continue
    a = a.lower()
    print('==', a, names.get(a, '?'))
    seen = set()
    for s in by.get(a, []):
        if s in seen:
            continue
        seen.add(s)
        if 'dicecanada' in s.lower() and not full:
            s = '[FILE] ' + s.split('\\AI\\')[-1]
        print('   ', s[:220])
