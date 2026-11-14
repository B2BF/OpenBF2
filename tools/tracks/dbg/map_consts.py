#!/usr/bin/env python3
"""map_consts.py : extend map.tsv (strings/calls) with float-constant fingerprint matches, then
re-run call-graph propagation. Writes aidll_r/map2.tsv (same columns, how = string|calls|consts)."""
import os as _os
_ROOT = _os.path.abspath(_os.path.join(_os.path.dirname(__file__), '..', '..', '..'))
# Ghidra exports live in <repo>/out/dbg-ghidra (never committed); evidence images outside the repo
# ($BF2_DEBUG_BUILDS_DIR for the DICE *_r builds, $BF2_LINUX_SERVER_DIR, $BF2_WIN_GAME_DIR).
import csv, collections, math, re
O = _ROOT + '/out/dbg-ghidra/'
LEDGER = _ROOT + '/docs/ai-reconstruct/ledger.csv'
BASE = 0x10000
PRE = 'Code/BF2/AI/AIDLL/AI/'


def consts(path, base=0):
    d = {}
    for line in open(path):
        p = line.rstrip('\n').split('\t')
        a = int(p[0], 16) - base
        fs = set(x for x in (p[1].split(',') if len(p) > 1 and p[1] else []) if x[0] in 'fd')
        # a double and its float rounding are the same literal for matching
        d[a] = set(x[1:] for x in fs)
    return d


def calls(path, base=0):
    c, sz = {}, {}
    for line in open(path, encoding='utf-8', errors='replace'):
        p = line.rstrip('\n').split('\t')
        a = int(p[0], 16) - base
        sz[a] = int(p[1])
        c[a] = [int(x, 16) - base for x in (p[3].split(',') if len(p) > 3 and p[3] else []) if 'EXTERNAL' not in x]
    return c, sz


rk, ek = consts(O + 'aidll_r/consts.tsv'), consts(O + 'elf/consts.tsv', BASE)
rc, rs = calls(O + 'aidll_r/calls.tsv')
ec, es = calls(O + 'elf/calls.tsv', BASE)
ledger = {int(r['address'], 16): r for r in csv.DictReader(open(LEDGER))}

# existing map
r2e, how = {}, {}
rtu = {}
for row in csv.DictReader(open(O + 'aidll_r/map.tsv'), delimiter='\t'):
    a = int(row['r_addr'], 16)
    rtu[a] = row['tu']
    if row['elf_addr']:
        r2e[a] = int(row['elf_addr'], 16)
        how[a] = row['how']
e2r = {v: k for k, v in r2e.items()}

# idf over ELF ledger functions (AI, non std)
df = collections.Counter()
for a, ks in ek.items():
    if a in ledger:
        for k in ks:
            df[k] += 1
N = len(ledger)
TRIVIAL = {'1.0', '0.5', '2.0', '-1.0', '10.0', '100.0', '0.1', '3.0', '4.0', '1000.0', '0.01', '0.001'}


def w(k):
    if k in TRIVIAL:
        return 0.2
    return math.log(N / (1 + df.get(k, 0)))


inv = collections.defaultdict(set)
for a, ks in ek.items():
    if a in ledger and ledger[a]['status'] != 'skip-lib':
        for k in ks:
            inv[k].add(a)

cand = {}
for ra, ks in rk.items():
    if ra in r2e or len(ks) < 2:
        continue
    sc = collections.Counter()
    for k in ks:
        if len(inv.get(k, ())) > 40:
            continue
        for ea in inv.get(k, ()):
            if ea in e2r:
                continue
            sc[ea] += w(k)
    best = sc.most_common(2)
    if not best:
        continue
    ea, s = best[0]
    second = best[1][1] if len(best) > 1 else 0.0
    ek_ea = ek.get(ea, set())
    jac = len(ks & ek_ea) / max(1, len(ks | ek_ea))
    if s >= 8.0 and s >= second * 1.5 and jac >= 0.35:
        cand[ra] = (ea, s)
# mutual uniqueness
byE = collections.defaultdict(list)
for ra, (ea, s) in cand.items():
    byE[ea].append((s, ra))
for ea, lst in byE.items():
    lst.sort(reverse=True)
    if len(lst) > 1 and lst[1][0] * 1.5 > lst[0][0]:
        continue
    ra = lst[0][1]
    r2e[ra], e2r[ea], how[ra] = ea, ra, 'consts'

# propagate along calls again
for _ in range(20):
    prop = collections.defaultdict(set)
    for r, e in list(r2e.items()):
        cr = list(dict.fromkeys(c for c in rc.get(r, []) if c not in r2e))
        ce = list(dict.fromkeys(c for c in ec.get(e, []) if c not in e2r))
        if cr and len(cr) == len(ce):
            for a, b in zip(cr, ce):
                prop[a].add(b)
    rev = collections.defaultdict(set)
    for a, bs in prop.items():
        for b in bs:
            rev[b].add(a)
    added = 0
    for a, bs in prop.items():
        if len(bs) == 1:
            b = next(iter(bs))
            if len(rev[b]) == 1 and b in ledger and a not in r2e and b not in e2r:
                r2e[a], e2r[b], how[a] = b, a, 'calls'
                added += 1
    if not added:
        break

with open(O + 'aidll_r/map2.tsv', 'w') as f:
    f.write('r_addr\tr_size\ttu\telf_addr\telf_symbol\ttree_file\thow\n')
    for a in sorted(rs):
        e = r2e.get(a)
        row = ledger.get(e) if e is not None else None
        f.write('0x%08X\t%d\t%s\t%s\t%s\t%s\t%s\n' % (a, rs[a], rtu.get(a, ''), '0x%X' % e if e is not None else '',
                row['symbol'] if row else '', row['file'] if row else '', how.get(a, '')))
c = collections.Counter(how.values())
print('mapped', len(r2e), dict(c))
