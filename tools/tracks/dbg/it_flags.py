#!/usr/bin/env python3
"""it_flags.py <repo> [--apply] : rewrite single-bit object-type tests on Information pointers,
  (X->getObjectTypeFlags() & 0xN) != 0  ->  X->isOfType(ITName)
  (X->getObjectTypeFlags() & 0xN) == 0  ->  !X->isOfType(ITName)
  !(X->getObjectTypeFlags() & 0xN)      ->  !X->isOfType(ITName)
  if (X->getObjectTypeFlags() & 0xN)    ->  if (X->isOfType(ITName))
  (X->getObjectTypeFlags() & 0xN)       ->  X->isOfType(ITName)  when followed by && || ) ? or ;
Prints every remaining getObjectTypeFlags() use with a hex mask for manual review."""
import os as _os
_ROOT = _os.path.abspath(_os.path.join(_os.path.dirname(__file__), '..', '..', '..'))
# Ghidra exports live in <repo>/out/dbg-ghidra (never committed); evidence images outside the repo
# ($BF2_DEBUG_BUILDS_DIR for the DICE *_r builds, $BF2_LINUX_SERVER_DIR, $BF2_WIN_GAME_DIR).
import os, re, sys

IT = ['ITProduction', 'ITAmmunition', 'ITRepair', 'ITUnit', 'ITAir', 'ITHelicopter', 'ITGround',
      'ITNaval', 'ITControlpoint', 'ITRadar', 'ITAirfield', 'ITTransportation', 'ITArtillery',
      'ITFixed', 'ITCover', 'ITMobile', 'ITUnmanned', 'ITLowPriority', 'ITStructure',
      'ITBiological', 'ITVegetation', 'ITSoldier', 'ITObstructedView', 'ITNoTemperature',
      'ITHasWreck', 'ITObstacle', 'ITHasCarHorn']

def name(hexs):
    v = int(hexs, 16)
    if v == 0 or v & (v - 1):
        return None
    b = v.bit_length() - 1
    return IT[b] if b < len(IT) else None

E = r'([A-Za-z_]\w*(?:(?:->|\.)[A-Za-z_]\w*(?:\(\))?)*)'
M = r'0x([0-9A-Fa-f]+)u?'
pats = [
    (re.compile(r'\(\s*' + E + r'->getObjectTypeFlags\(\)\s*&\s*' + M + r'\s*\)\s*!=\s*0(?![\w.])'), '{e}->isOfType({n})'),
    (re.compile(r'\(\s*' + E + r'->getObjectTypeFlags\(\)\s*&\s*' + M + r'\s*\)\s*==\s*0(?![\w.])'), '!{e}->isOfType({n})'),
    (re.compile(r'!\s*\(\s*' + E + r'->getObjectTypeFlags\(\)\s*&\s*' + M + r'\s*\)'), '!{e}->isOfType({n})'),
    (re.compile(r'(?<=if \()' + E + r'->getObjectTypeFlags\(\)\s*&\s*' + M + r'(?=\))'), '{e}->isOfType({n})'),
    (re.compile(r'\(\s*' + E + r'->getObjectTypeFlags\(\)\s*&\s*' + M + r'\s*\)(?=\s*(?:&&|\|\||\)|\?|;))'), '{e}->isOfType({n})'),
]

repo = sys.argv[1]
apply = '--apply' in sys.argv
root = os.path.join(repo, 'Code/BF2/AI/AIDLL/AI')
count = 0
for dp, _, fs in os.walk(root):
    for f in fs:
        if not f.endswith(('.cpp', '.h', '.inc')):
            continue
        p = os.path.join(dp, f)
        s = open(p, encoding='utf-8').read()
        o = s
        for rx, tmpl in pats:
            def sub(m):
                global count
                n = name(m.group(2))
                if not n:
                    return m.group(0)
                count += 1
                return tmpl.format(e=m.group(1), n=n)
            s = rx.sub(sub, s)
        if s != o:
            if apply:
                open(p, 'w', encoding='utf-8').write(s)
        for i, line in enumerate(s.split('\n'), 1):
            if re.search(r'getObjectTypeFlags\(\)\s*&\s*0x', line):
                print('LEFT %s:%d: %s' % (os.path.relpath(p, repo), i, line.strip()[:160]))
print('rewritten', count)
