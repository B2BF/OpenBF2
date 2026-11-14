#!/usr/bin/env python3
"""map_functions.py : map AIDLL_r.dll functions to libaidll.so 1.50 functions (and so to tree files).

Inputs (out/dbg-ghidra): aidll_r/{functions,strxrefs,calls}.tsv, elf/{strxrefs,calls}.tsv,
aidll_r/tu_ranges.tsv, docs/ai-reconstruct/ledger.csv (ELF address -> symbol, tree file).
Method:
  1. string anchors: a string referenced by exactly one function in each image pairs them
     (assert expressions and __FILE__ paths are skipped: retail has no asserts);
  2. call-graph propagation: for a mapped pair whose direct-callee lists have equal length after
     dropping callees already mapped elsewhere, the remaining callees pair up positionally;
     a pair is accepted when it is proposed without conflict.
  3. TU of an AIDLL_r function: the __FILE__ TU when the function lies between two anchors of the
     same TU (or is one), else blank.
Output: aidll_r/map.tsv  r_addr r_size tu elf_addr elf_symbol tree_file how
"""
import os as _os
_ROOT = _os.path.abspath(_os.path.join(_os.path.dirname(__file__), '..', '..', '..'))
# Ghidra exports live in <repo>/out/dbg-ghidra (never committed); evidence images outside the repo
# ($BF2_DEBUG_BUILDS_DIR for the DICE *_r builds, $BF2_LINUX_SERVER_DIR, $BF2_WIN_GAME_DIR).
import csv, collections, re, sys
O = _ROOT + '/out/dbg-ghidra/'
LEDGER = _ROOT + '/docs/ai-reconstruct/ledger.csv'
ELF_GHIDRA_BASE = 0x10000  # Ghidra loads libaidll.so at +0x10000 over the ledger's ELF addresses


def load_calls(path, base=0):
    calls, size, name = {}, {}, {}
    for line in open(path, encoding='utf-8', errors='replace'):
        p = line.rstrip('\n').split('\t')
        if len(p) < 3:
            continue
        a = int(p[0], 16) - base
        size[a] = int(p[1])
        name[a] = p[2]
        calls[a] = [int(x, 16) - base for x in p[3].split(',') if 'EXTERNAL' not in x] if len(p) > 3 and p[3] else []
    return calls, size, name


def load_strings(path, base=0):
    by = collections.defaultdict(set)
    for line in open(path, encoding='utf-8', errors='replace'):
        p = line.rstrip('\n').split('\t')
        if len(p) < 5 or p[0] == '-':
            continue
        by[p[4]].add(int(p[0], 16) - base)
    return by


def is_assertish(s):
    if re.search(r'(?i)dicecanada|code\\bf2|\.cpp$|\.h$', s):
        return True
    if re.search(r'(->|==|!=|<=|>=|&&|\|\||\bm_[a-z])', s):
        return True
    return len(s) < 4


rcalls, rsize, rname = load_calls(O + 'aidll_r/calls.tsv')
ecalls, esize, ename = load_calls(O + 'elf/calls.tsv', ELF_GHIDRA_BASE)
rstr = load_strings(O + 'aidll_r/strxrefs.tsv')
estr = load_strings(O + 'elf/strxrefs.tsv', ELF_GHIDRA_BASE)

ledger = {}
for row in csv.DictReader(open(LEDGER)):
    ledger[int(row['address'], 16)] = row

votes = collections.Counter()
for s, rf in rstr.items():
    if is_assertish(s) or len(rf) != 1:
        continue
    ef = estr.get(s)
    if not ef or len(ef) != 1:
        continue
    votes[(next(iter(rf)), next(iter(ef)))] += 1

r2e, e2r, how = {}, {}, {}
byr = collections.defaultdict(list)
for (r, e), n in votes.items():
    byr[r].append((n, e))
bye = collections.defaultdict(list)
for (r, e), n in votes.items():
    bye[e].append((n, r))
for r, lst in byr.items():
    lst.sort(reverse=True)
    n, e = lst[0]
    if len(lst) > 1 and lst[1][0] == n:
        continue
    el = sorted(bye[e], reverse=True)
    if el[0][1] != r or (len(el) > 1 and el[1][0] == el[0][0]):
        continue
    r2e[r], e2r[e], how[r] = e, r, 'string'

changed = True
rounds = 0
while changed and rounds < 50:
    rounds += 1
    changed = False
    prop = collections.defaultdict(set)
    for r, e in list(r2e.items()):
        cr = [c for c in rcalls.get(r, []) if c not in r2e]
        ce = [c for c in ecalls.get(e, []) if c not in e2r]
        # drop duplicates, keep order
        cr = list(dict.fromkeys(cr))
        ce = list(dict.fromkeys(ce))
        if cr and len(cr) == len(ce):
            for a, b in zip(cr, ce):
                prop[a].add(b)
    rev = collections.defaultdict(set)
    for a, bs in prop.items():
        for b in bs:
            rev[b].add(a)
    for a, bs in prop.items():
        if len(bs) != 1:
            continue
        b = next(iter(bs))
        if len(rev[b]) != 1 or b in e2r or a in r2e:
            continue
        if b not in ledger:
            continue
        r2e[a], e2r[b], how[a] = b, a, 'calls'
        changed = True

# TU assignment from anchors
anchors = []
for line in open(O + 'aidll_r/tu_ranges.tsv'):
    p = line.rstrip('\n').split('\t')
    anchors.append((int(p[0], 16), int(p[1], 16), p[3]))
anchors.sort()


def tu_of(a):
    lo = None
    for i, (s, e, t) in enumerate(anchors):
        if s <= a <= e:
            return t
        if e < a:
            lo = i
    if lo is not None and lo + 1 < len(anchors):
        prev, nxt = anchors[lo], anchors[lo + 1]
        if prev[2] == nxt[2]:
            return prev[2]
    return ''


with open(O + 'aidll_r/map.tsv', 'w') as w:
    w.write('r_addr\tr_size\ttu\telf_addr\telf_symbol\ttree_file\thow\n')
    for r in sorted(rsize):
        e = r2e.get(r)
        row = ledger.get(e) if e is not None else None
        w.write('0x%08X\t%d\t%s\t%s\t%s\t%s\t%s\n' % (
            r, rsize[r], tu_of(r), '0x%X' % e if e is not None else '',
            row['symbol'] if row else (ename.get(e, '') if e is not None else ''),
            row['file'] if row else '', how.get(r, '')))
print('mapped', len(r2e), 'of', len(rsize), '; by string', sum(1 for v in how.values() if v == 'string'),
      'rounds', rounds)
