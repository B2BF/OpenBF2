#!/usr/bin/env python3
"""tu_ranges.py : AIDLL_r TU layout from __FILE__ anchors. Prints one line per run of anchors of the
same TU: first..last anchor function, count, TU. Output also written to aidll_r/tu_ranges.tsv."""
import os as _os
_ROOT = _os.path.abspath(_os.path.join(_os.path.dirname(__file__), '..', '..', '..'))
# Ghidra exports live in <repo>/out/dbg-ghidra (never committed); evidence images outside the repo
# ($BF2_DEBUG_BUILDS_DIR for the DICE *_r builds, $BF2_LINUX_SERVER_DIR, $BF2_WIN_GAME_DIR).
import re
D = _ROOT + '/out/dbg-ghidra/aidll_r/'
anch = {}
for line in open(D + 'strxrefs.tsv', encoding='utf-8', errors='replace'):
    p = line.rstrip('\n').split('\t')
    if len(p) < 5 or p[0] == '-':
        continue
    s = p[4]
    m = re.search(r'(?i)code\\bf2\\(.*\.(cpp|h|inl))$', s)
    if not m:
        continue
    tu = m.group(1).replace('\\', '/')
    tu = re.sub(r'(?i)^ai/aidll/ai/', '', tu)
    anch.setdefault(int(p[0], 16), set()).add(tu)
runs = []
for a in sorted(anch):
    tus = anch[a]
    tu = sorted(t for t in tus if t.endswith('.cpp'))
    tu = tu[0] if tu else sorted(tus)[0]
    if runs and runs[-1][3] == tu:
        runs[-1][1] = a
        runs[-1][2] += 1
    else:
        runs.append([a, a, 1, tu, tus])
with open(D + 'tu_ranges.tsv', 'w') as w:
    for r in runs:
        extra = ','.join(sorted(t for t in r[4] if t != r[3]))
        w.write('0x%08X\t0x%08X\t%d\t%s\t%s\n' % (r[0], r[1], r[2], r[3], extra))
        print('0x%08X 0x%08X %3d %s %s' % (r[0], r[1], r[2], r[3], extra))
