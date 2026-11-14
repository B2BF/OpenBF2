#!/usr/bin/env python3
"""retail_paths.py : AI source paths embedded in retail 1.50 AI images and AIDLL_r, checked against the tree."""
import os as _os
_ROOT = _os.path.abspath(_os.path.join(_os.path.dirname(__file__), '..', '..', '..'))
# Ghidra exports live in <repo>/out/dbg-ghidra (never committed); evidence images outside the repo
# ($BF2_DEBUG_BUILDS_DIR for the DICE *_r builds, $BF2_LINUX_SERVER_DIR, $BF2_WIN_GAME_DIR).
import os, re
_need = [v for v in ('BF2_WIN_GAME_DIR', 'BF2_LINUX_SERVER_DIR', 'BF2_DEBUG_BUILDS_DIR') if not _os.environ.get(v)]
if _need:
    raise SystemExit('retail_paths.py: set ' + ', '.join(_need) + ' (Windows 1.50 game folder, Linux 1.5 server, DICE debug builds)')
R = _os.environ['BF2_WIN_GAME_DIR'] + '/'
imgs = {'elf': _os.path.join(_os.environ['BF2_LINUX_SERVER_DIR'], 'bin/ia-32/libaidll.so'),
        'pe': R + 'AIDLL.dll', 'ded': R + 'AIDLL_w32ded.dll',
        'r': _os.path.join(_os.environ['BF2_DEBUG_BUILDS_DIR'], 'AIDLL_r.dll')}
TREE = _ROOT + '/Code/BF2/AI/AIDLL/AI/'
rx = re.compile(rb'(?i)AIDLL[/\\]+AI[/\\]+([A-Za-z0-9_/\\]+\.(?:cpp|h|inl))')
seen = {}
for k, p in imgs.items():
    d = open(p, 'rb').read()
    for m in rx.finditer(d):
        path = m.group(1).decode().replace('\\', '/')
        seen.setdefault(path, set()).add(k)
treel = {}
for dp, _, fs in os.walk(TREE):
    for f in fs:
        rel = os.path.relpath(os.path.join(dp, f), TREE)
        treel[rel.lower()] = rel
for path in sorted(seen, key=str.lower):
    t = treel.get(path.lower())
    st = 'ok' if t == path else ('case:' + t if t else 'MISSING')
    print('%-8s %-12s %s' % (st if st == 'ok' or st == 'MISSING' else 'CASE', ','.join(sorted(seen[path])), path), '' if st in ('ok', 'MISSING') else st)
