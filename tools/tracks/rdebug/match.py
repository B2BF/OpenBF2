#!/usr/bin/env python3
"""Match retail RendDX9.dll 1.50 functions to the 2006 BoosterPack2 RendDX9_r.dll build.

Same source, older version, different compiler settings (RendDX9_r keeps every Debug/assert),
so only robust anchors are used, in this order:
  site     a Debug site (file basename, type, streamed message) found in exactly one function
           of each image (1.50 keeps its Error/Info sites; debug_sites.py lists both images)
  string   a C string literal referenced by exactly one function in each image
  import   an imported function called by exactly one function in each image
  vtable   the same slot of two vtables already tied by >= 2 agreeing matched slots
  callee   the single unmatched callee between the same matched neighbours in two matched
           callers' ordered call lists (Debug-only helpers of RendDX9_r are left out)
  caller   the single unmatched caller of two matched functions whose matched callers agree
  order    link order survives between the builds: see order_pass (upgrades and 1:1 gaps)
  gapsig   equal, gap-unique matched-callee sets inside an order-consistent gap
Every candidate must also pass a compatibility check (shared-vocabulary strings, imports, size).
Confidence: high = >= 2 independent agreeing anchors and no conflicting vote for either side;
medium = one anchor that passes the checks; low = weak single evidence (listed for review only).

usage: match.py <evidence-root> <out-map.csv> <debug_sites_r.csv> <debug_sites_150.csv> [asserts.csv]
  asserts.csv (optional output): the RendDX9_r Debug sites with Code/BF2-relative paths and the
  matched 1.50 function (addr_150, confidence) of the containing _r function.
  evidence-root holds rend-evidence/{RendDX9.s,RendDX9_r.s,debug-r/RendDX9_r.dll} and
  rend-ghidra/{RendDX9,RendDX9_r}/functions.tsv; the retail DLL path comes from $RENDDX9_150.
"""
import csv
import os
import pickle
import re
import sys
from collections import Counter, defaultdict

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from pe import PE          # noqa: E402
import asm                 # noqa: E402
import features            # noqa: E402

DEBUG_CTOR, DEBUG_DTOR = 0x1011D2B0, 0x1011D8F0          # RendDX9_r dice::hfe::Debug
DEBUG_150 = (0x10074300, 0x10074690)                     # the same pair in 1.50
PATH_RE = re.compile(r'code[\\/]+bf2[\\/]+(.*)$', re.I)


def norm_path(p):
    m = PATH_RE.search(p.replace('/', '\\'))
    if not m:
        return None
    s = re.sub(r'\\+', '/', m.group(1))
    return s


def load(root, cache):
    if os.path.exists(cache):
        with open(cache, 'rb') as f:
            return pickle.load(f)
    p150 = os.environ.get('RENDDX9_150') or os.path.join(root, 'RendDX9.dll')
    pe150, per = PE(p150), PE(os.path.join(root, 'rend-evidence/debug-r/RendDX9_r.dll'))
    L150 = asm.load(os.path.join(root, 'rend-evidence/RendDX9.s'))
    Lr = asm.load(os.path.join(root, 'rend-evidence/RendDX9_r.s'))
    F150, V150, X150 = features.extract(pe150, L150, os.path.join(root, 'rend-ghidra/RendDX9/functions.tsv'))
    Fr, Vr, Xr = features.extract(per, Lr, os.path.join(root, 'rend-ghidra/RendDX9_r/functions.tsv'))
    # Debug-only helpers of _r: callees whose calls all sit inside a Debug ctor..dtor window
    A, M, O = Lr
    idx = {a: i for i, a in enumerate(A)}

    def resolve(t):
        for _ in range(4):
            i = idx.get(t)
            if i is None or M[i] != 'jmp' or not O[i].startswith('0x'):
                return t
            t = int(O[i], 16)
        return t
    inwin, total = Counter(), Counter()
    open_ = False
    for i in range(len(A)):
        if M[i] == 'call':
            o = O[i]
            if o == '0x%x' % DEBUG_CTOR:
                open_ = True
                continue
            if o == '0x%x' % DEBUG_DTOR:
                open_ = False
                continue
            if o.startswith('0x'):
                t = resolve(int(o, 16))
                total[t] += 1
                if open_:
                    inwin[t] += 1
        elif M[i].startswith('ret'):
            open_ = False
    dbg = {t for t in total if inwin[t] >= 0.9 * total[t]} | {DEBUG_CTOR, DEBUG_DTOR}
    data = (F150, V150, X150, Fr, Vr, Xr, dbg)
    with open(cache, 'wb') as f:
        pickle.dump(data, f)
    return data


class Matcher:
    def __init__(self, F150, V150, Fr, Vr, dbg):
        self.F150, self.V150, self.Fr, self.Vr, self.dbg = F150, V150, Fr, Vr, dbg
        self.vocab150 = set().union(*(f.strs for f in F150.values()))
        self.vocabr = set().union(*(f.strs for f in Fr.values()))
        self.shared = self.vocab150 & self.vocabr
        self.m = {}      # 150 -> (r, conf, method)
        self.rm = {}     # r -> 150
        self.rcalls = {e: [c for c in f.calls if c not in dbg] for e, f in Fr.items()}

    # compatibility -----------------------------------------------------------------------
    def compat(self, a, b):
        f, g = self.F150[a], self.Fr[b]
        if f.n and g.n and not (0.25 <= (g.n + 5) / (f.n + 5) <= 10):
            return False, 'size'
        s1, s2 = f.strs & self.shared, g.strs & self.shared
        if s1 and s2 and not (s1 & s2):
            return False, 'strings'
        # a shared string referenced by f but absent from g (and g not small) is suspicious only
        # if many; tolerate inlining differences
        i1, i2 = set(f.imps), set(g.imps)
        sig = {x for x in i1 | i2 if 'basic_string' not in x and 'ostream' not in x and 'ios' not in x}
        if (i1 & sig) and (i2 & sig) and not (i1 & i2 & sig):
            return False, 'imports'
        return True, ''

    def add(self, a, b, conf, method):
        if a in self.m or b in self.rm:
            return False
        ok, why = self.compat(a, b)
        if not ok:
            return False
        self.m[a] = (b, conf, method)
        self.rm[b] = a
        return True

    # phases ------------------------------------------------------------------------------
    def unique_votes(self, key):
        """key(func) -> iterable of tokens; vote pairs for tokens unique on both sides."""
        idx1, idx2 = defaultdict(set), defaultdict(set)
        for e, f in self.F150.items():
            for t in key(f):
                idx1[t].add(e)
        for e, f in self.Fr.items():
            for t in key(f):
                idx2[t].add(e)
        votes = defaultdict(list)
        for t, s1 in idx1.items():
            s2 = idx2.get(t)
            if s2 and len(s1) == 1 and len(s2) == 1:
                votes[(next(iter(s1)), next(iter(s2)))].append(t)
        return votes

    def anchors(self, sites_r, sites_150):
        def tokens(rows):
            t = defaultdict(set)
            for r in rows:
                if r['func_entry_r'] and r['message']:
                    base = re.split(r'[\\/]', r['file'])[-1].lower()
                    t[int(r['func_entry_r'], 16)].add((base, r['type'], r['message']))
            return t
        t150, tr = tokens(sites_150), tokens(sites_r)
        self.F150_site, self.Fr_site = t150, tr
        stv = self.unique_votes(lambda f: t150.get(f.entry, ()) if f is self.F150.get(f.entry) else tr.get(f.entry, ()))
        sv = self.unique_votes(lambda f: f.strs)
        iv = self.unique_votes(lambda f: {x for x in f.imps if not x.startswith('msvcp71')})
        allv = defaultdict(lambda: {'site': [], 'string': [], 'import': []})
        for k, v in stv.items():
            allv[k]['site'] += v
        for k, v in sv.items():
            allv[k]['string'] += v
        for k, v in iv.items():
            allv[k]['import'] += v
        by150, byr = defaultdict(set), defaultdict(set)
        for (a, b) in allv:
            by150[a].add(b)
            byr[b].add(a)
        res = []
        for (a, b), v in allv.items():
            # a site token and its message string are one piece of evidence, not two
            msgs = {t[2] for t in v['site']}
            strs = [x for x in v['string'] if not any(x in m for m in msgs)]
            n = len(v['site']) + len(strs) + len(v['import'])
            conflict = len(by150[a]) > 1 or len(byr[b]) > 1
            longest = max([len(s) for s in v['string']] or [0])
            method = '+'.join(k for k in ('site', 'string', 'import') if v[k])
            if conflict:
                # majority only: this pair has >= 2 tokens and every rival has at most 1
                def cnt(k):
                    return sum(len(allv[k][x]) for x in ('site', 'string', 'import'))
                rivals = [cnt((a, x)) for x in by150[a] if x != b] + \
                         [cnt((y, b)) for y in byr[b] if y != a]
                if n >= 3 and max(rivals) <= 1:
                    res.append((a, b, 'medium', method + '-majority', n))
                continue
            if n >= 2:
                res.append((a, b, 'high', method, n))
            elif v['site'] or (v['string'] and longest >= 12):
                res.append((a, b, 'medium', method, n))
            else:
                res.append((a, b, 'low', method, n))
        res.sort(key=lambda r: -r[4])
        for a, b, conf, method, n in res:
            self.add(a, b, conf, method)

    def vtables(self):
        """Tie vtables by agreeing matched slots, then pair same-index unmatched slots."""
        votes = defaultdict(list)
        occ1, occ2 = Counter(), Counter()
        for sl in self.V150.values():
            occ1.update(set(sl))
        for sl in self.Vr.values():
            occ2.update(set(sl))
        tied = []
        for v1, s1 in self.V150.items():
            best = []
            for v2, s2 in self.Vr.items():
                agree = dis = 0
                for i in range(min(len(s1), len(s2))):
                    x = self.m.get(s1[i])
                    if x and x[1] in ('high', 'medium'):
                        if x[0] == s2[i]:
                            agree += 1
                        else:
                            dis += 1
                if agree >= 2:
                    best.append((agree, -dis, v2))
            if not best:
                continue
            best.sort(reverse=True)
            if len(best) > 1 and best[1][0] == best[0][0]:
                continue
            agree, ndis, v2 = best[0]
            if -ndis > agree // 4:
                continue
            tied.append((v1, v2, agree, -ndis))
        for v1, v2, agree, dis in tied:
            s1, s2 = self.V150[v1], self.Vr[v2]
            same_len = len(s1) == len(s2)
            for i in range(min(len(s1), len(s2))):
                a, b = s1[i], s2[i]
                votes[(a, b)].append((v1, v2, same_len and dis == 0 and agree >= 3))
        self._accept(votes, 'vtable')
        return tied

    def callees(self):
        votes = defaultdict(list)
        for a, (b, conf, _) in list(self.m.items()):
            if conf == 'low':
                continue
            c1 = [c for c in self.F150[a].calls if c not in DEBUG_150]
            c2 = self.rcalls.get(b, [])
            # anchor positions
            seq1 = [(i, c) for i, c in enumerate(c1) if c in self.m and self.m[c][1] != 'low']
            pos2 = defaultdict(list)
            for j, c in enumerate(c2):
                pos2[c].append(j)
            # greedy monotone alignment on anchors
            pairs = [(-1, -1)]
            lastj = -1
            for i, c in seq1:
                js = [j for j in pos2.get(self.m[c][0], []) if j > lastj]
                if js:
                    pairs.append((i, js[0]))
                    lastj = js[0]
            pairs.append((len(c1), len(c2)))
            for (i0, j0), (i1, j1) in zip(pairs, pairs[1:]):
                g1 = {c for c in c1[i0 + 1:i1] if c not in self.m}
                g2 = {c for c in c2[j0 + 1:j1] if c not in self.rm}
                if len(g1) == 1 and len(g2) == 1:
                    votes[(next(iter(g1)), next(iter(g2)))].append((a, b, conf == 'high'))
        self._accept(votes, 'callee')

    def callers(self):
        """The single unmatched caller of two matched callees (ignoring Debug helpers)."""
        votes = defaultdict(list)
        rcallers = defaultdict(set)
        for e, cs in self.rcalls.items():
            for c in cs:
                rcallers[c].add(e)
        for a, (b, conf, _) in list(self.m.items()):
            if conf == 'low' or a in DEBUG_150:
                continue
            u1 = {c for c in self.F150[a].callers if c not in self.m}
            u2 = {c for c in rcallers.get(b, ()) if c not in self.rm}
            m1 = {self.m[c][0] for c in self.F150[a].callers if c in self.m}
            m2 = {c for c in rcallers.get(b, ()) if c in self.rm}
            if len(u1) == 1 and len(u2) == 1 and m1 == m2:
                votes[(next(iter(u1)), next(iter(u2)))].append((a, b, conf == 'high'))
        self._accept(votes, 'caller')

    def gap_signatures(self):
        """Inside an order-consistent gap between two high pairs, pair the functions whose
        matched-callee sets are equal (>= 1 callee) and unique within the gap on both sides,
        with compatible imports and size. >= 2 shared callees: high (callee identity plus order)."""
        import bisect
        H = sorted((a, v[0]) for a, v in self.m.items() if v[1] == 'high')
        e150 = sorted(self.F150)
        er = sorted(e for e in self.Fr if e not in self.dbg)
        for (a1, b1), (a2, b2) in zip(H, H[1:]):
            if not b1 < b2:
                continue
            g1 = [e for e in e150[bisect.bisect_right(e150, a1):bisect.bisect_left(e150, a2)]
                  if e not in self.m]
            g2 = [e for e in er[bisect.bisect_right(er, b1):bisect.bisect_left(er, b2)]
                  if e not in self.rm]
            if not g1 or not g2:
                continue
            s1, s2 = defaultdict(list), defaultdict(list)
            for e in g1:
                k = frozenset(self.m[c][0] for c in self.F150[e].calls
                              if c in self.m and self.m[c][1] != 'low' and c not in DEBUG_150)
                if k:
                    s1[k].append(e)
            for e in g2:
                k = frozenset(c for c in self.rcalls.get(e, ()) if c in self.rm
                              and self.m[self.rm[c]][1] != 'low' and c not in (DEBUG_CTOR, DEBUG_DTOR))
                if k:
                    s2[k].append(e)
            for k, l1 in s1.items():
                l2 = s2.get(k)
                if len(l1) == 1 and l2 and len(l2) == 1:
                    a, b = l1[0], l2[0]
                    if 0.4 <= (self.Fr[b].n + 3) / (self.F150[a].n + 3) <= 8:
                        self.add(a, b, 'high' if len(k) >= 2 else 'medium', 'gapsig')

    def _accept(self, votes, method):
        by150, byr = defaultdict(set), defaultdict(set)
        for (a, b) in votes:
            if a in self.m or b in self.rm:
                continue
            by150[a].add(b)
            byr[b].add(a)
        for (a, b), vs in sorted(votes.items(), key=lambda kv: -len(kv[1])):
            if a in self.m or b in self.rm:
                continue
            if len(by150[a]) > 1 or len(byr[b]) > 1:
                continue
            parents = {v[:2] for v in vs}
            strong = sum(1 for v in vs if v[2])
            if len(parents) >= 2 and strong >= 1:
                conf = 'high'
            elif strong >= 1:
                conf = 'medium'
            else:
                conf = 'low'
            self.add(a, b, conf, method)


def order_pass(M):
    """Link order survives between the two builds (both images keep the obj order). A pair is
    order-consistent when its _r address lies strictly between the _r addresses of the nearest
    high pairs before and after it in 1.50 order. A single anchor plus order consistency is two
    independent pieces of evidence: medium -> high, low -> medium. A vtable/callee high pair that
    is not order-consistent drops to medium. Then 1:1 gaps between consistent high neighbours
    (one 1.50 function, one _r function, both >= 8 instructions, compatible) pair as `order`."""
    import bisect
    changed = 0
    H = sorted((a, v[0]) for a, v in M.m.items() if v[1] == 'high')
    ha = [h[0] for h in H]

    def bracket(a):
        i = bisect.bisect_left(ha, a)
        lo = H[i - 1] if i > 0 else None
        j = i + 1 if i < len(H) and H[i][0] == a else i
        hi = H[j] if j < len(H) else None
        return lo, hi

    for a, (b, conf, method) in list(M.m.items()):
        lo, hi = bracket(a)
        if not lo or not hi:
            continue
        ok = lo[1] < b < hi[1]
        base = method.split('+order')[0]
        if ok and conf in ('medium', 'low') and '+order' not in method and base != 'order':
            M.m[a] = (b, 'high' if conf == 'medium' else 'medium', base + '+order')
            changed += 1
        elif not ok and conf == 'high' and base.split('-')[0] in ('vtable', 'callee'):
            M.m[a] = (b, 'medium', base)
            changed += 1
    # 1:1 gaps
    H = sorted((a, v[0]) for a, v in M.m.items() if v[1] == 'high')
    e150 = sorted(M.F150)
    er = sorted(e for e in M.Fr if e not in M.dbg)
    for (a1, b1), (a2, b2) in zip(H, H[1:]):
        if not b1 < b2:
            continue
        g1 = [e for e in e150[bisect.bisect_right(e150, a1):bisect.bisect_left(e150, a2)]
              if e not in M.m]
        g2 = [e for e in er[bisect.bisect_right(er, b1):bisect.bisect_left(er, b2)]
              if e not in M.rm]
        if len(g1) == 1 and len(g2) == 1:
            a, b = g1[0], g2[0]
            if M.F150[a].n >= 8 and M.Fr[b].n >= 8 and 0.5 <= M.Fr[b].n / M.F150[a].n <= 4:
                if M.add(a, b, 'medium', 'order'):
                    changed += 1
    return changed


def attach_files(Fr, sites):
    """_r entry -> (primary file, line range, basis, all files)."""
    own = defaultdict(lambda: defaultdict(list))
    for e, f in Fr.items():
        for s in f.strs:
            p = norm_path(s)
            if p:
                own[e][p]
    for r in sites:
        if r['func_entry_r'] and r['file'] and r['line']:
            p = norm_path(r['file'])
            if p:
                own[int(r['func_entry_r'], 16)][p].append(int(r['line']))
    # one spelling per path: the most frequent spelling with capitals (some __FILE__ are lowercase)
    spell = defaultdict(Counter)
    for files in own.values():
        for p in files:
            spell[p.lower()][p] += 1
    canon = {k: max(c, key=lambda p: (p != p.lower(), c[p])) for k, c in spell.items()}
    merged = {}
    for e, files in own.items():
        m = defaultdict(list)
        for p, ls in files.items():
            m[canon[p.lower()]].extend(ls)
        merged[e] = m
    own = merged
    res = {}
    for e, files in own.items():
        cpp = [p for p in files if p.lower().endswith(('.cpp', '.c'))]
        prim = cpp[0] if len(cpp) == 1 else (None if cpp else (list(files)[0] if len(files) == 1 else None))
        if prim is None and cpp:
            prim = max(cpp, key=lambda p: len(files[p]))
        lines = files.get(prim, [])
        lr = '%d-%d' % (min(lines), max(lines)) if lines else ''
        res[e] = (prim, lr, 'site', ';'.join(sorted(files)))
    # neighbours: between two functions of the same primary .cpp
    order = sorted(Fr)
    att = [(e, res[e][0]) for e in order if e in res and res[e][0] and res[e][0].lower().endswith('.cpp')]
    import bisect
    keys = [e for e, _ in att]
    for e in order:
        if e in res:
            continue
        k = bisect.bisect_left(keys, e)
        if 0 < k < len(keys) and att[k - 1][1] == att[k][1]:
            res[e] = (att[k][1], '', 'neighbour', '')
    return res


def main():
    root, out = sys.argv[1], sys.argv[2]
    sites_r = list(csv.DictReader(open(sys.argv[3])))
    sites_150 = list(csv.DictReader(open(sys.argv[4])))
    cache = os.environ.get('RDEBUG_CACHE', os.path.join(root, 'rend-ghidra', 'rdebug-features.pkl'))
    F150, V150, X150, Fr, Vr, Xr, dbg = load(root, cache)
    M = Matcher(F150, V150, Fr, Vr, dbg)
    # seed: Debug::Debug / Debug::~Debug, read from both listings (same ctor body and field
    # stores; every Debug site of both images calls them), then left out of callee alignment
    M.m[DEBUG_150[0]] = (DEBUG_CTOR, 'high', 'seed')
    M.m[DEBUG_150[1]] = (DEBUG_DTOR, 'high', 'seed')
    M.rm[DEBUG_CTOR], M.rm[DEBUG_DTOR] = DEBUG_150
    M.anchors(sites_r, sites_150)
    n0 = len(M.m)
    for it in range(10):
        before = sorted((a, v[1]) for a, v in M.m.items())
        M.vtables()
        M.callees()
        M.callers()
        order_pass(M)
        M.gap_signatures()
        if sorted((a, v[1]) for a, v in M.m.items()) == before:
            break
    files = attach_files(Fr, sites_r)
    with open(out, 'w', newline='') as f:
        w = csv.writer(f)
        w.writerow(['addr_150', 'addr_r', 'confidence', 'method', 'file', 'line_range', 'file_basis'])
        for a in sorted(M.m):
            b, conf, method = M.m[a]
            fi = files.get(b, (None, '', '', ''))
            w.writerow(['0x%08X' % a, '0x%08X' % b, conf, method, fi[0] or '', fi[1],
                        fi[2] if fi[0] else ''])
    if len(sys.argv) > 5:
        back = {v[0]: (a, v[1]) for a, v in M.m.items()}
        with open(sys.argv[5], 'w', newline='') as f:
            w = csv.writer(f)
            w.writerow(['site_r', 'type', 'file', 'line', 'expr', 'message', 'func_entry_r',
                        'addr_150', 'confidence', 'status'])
            for r in sites_r:
                e = int(r['func_entry_r'], 16) if r['func_entry_r'] else None
                a, conf = back.get(e, (None, ''))
                w.writerow([r['site_r'], r['type'], norm_path(r['file']) or r['file'], r['line'],
                            r['expr'], r['message'][:240], r['func_entry_r'],
                            '0x%08X' % a if a else '', conf, r['status']])
    c = Counter(v[1] for v in M.m.values())
    print('functions 150=%d r=%d (vtable-only entries added: %d / %d); anchors %d; matched %d %s'
          % (len(F150), len(Fr), len(X150), len(Xr), n0, len(M.m), dict(c)))
    print('methods', Counter((v[2].split('-')[0], v[1]) for v in M.m.values()))


if __name__ == '__main__':
    main()
