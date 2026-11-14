"""Per-function features of a PE image for cross-version matching.

features(pe, listing, functions.tsv) -> dict entry -> Func with
    strs     set of C string literals referenced by immediate operands (len >= 4, printable)
    imps     list of imported functions called, in order (call [iat] or call reg after mov reg,[iat])
    calls    list of direct call targets that are function entries, in order (jmp tail calls too)
    fptrs    set of function entries used as immediates (callbacks, vtable-free pointers)
    vtrefs   set of vtable addresses used as immediates
    floats   set of float/double constants loaded from .rdata ([0x...] operands)
    consts   set of immediate integers >= 0x1000 that are not addresses
    n        instruction count
Vtables: runs of >= 2 consecutive .rdata words that are function entries, starting at an address
referenced as an immediate somewhere in .text.
"""
import re
import struct

import asm

HEX = re.compile(r'0x[0-9a-f]+')
MEMABS = re.compile(r'(byte|word|dword|qword|tbyte) ptr \[(0x[0-9a-f]+)\]')
REG32 = re.compile(r'^(eax|ebx|ecx|edx|esi|edi|ebp)$')


class Func:
    __slots__ = ('entry', 'size', 'name', 'strs', 'imps', 'calls', 'fptrs', 'vtrefs', 'floats',
                 'consts', 'n', 'callers', 'paths')

    def __init__(self, entry, size, name):
        self.entry, self.size, self.name = entry, size, name
        self.strs, self.imps, self.calls = set(), [], []
        self.fptrs, self.vtrefs, self.floats, self.consts = set(), set(), set(), set()
        self.n = 0
        self.callers = set()
        self.paths = set()


def printable(s):
    return s is not None and len(s) >= 4 and all(32 <= ord(c) < 127 or c in '\t\r\n' for c in s)


def extract(pe, listing, ftsv):
    A, M, O = asm.load(listing) if isinstance(listing, str) else listing
    funcs = asm.functions_tsv(ftsv)
    # incremental-link thunks (RendDX9_r): `jmp target` stubs; calls and vtable slots go through
    # them, so every code address is resolved to the final target first.
    idx = {a: i for i, a in enumerate(A)}

    def resolve(t):
        for _ in range(4):
            i = idx.get(t)
            if i is None or M[i] != 'jmp' or not HEX.fullmatch(O[i]):
                return t
            t = int(O[i], 16)
        return t
    thunks = {f[0] for f in funcs if resolve(f[0]) != f[0]}
    funcs = [f for f in funcs if f[0] not in thunks]
    imps = pe.imports()
    rdata = pe.section('.rdata')
    data = pe.section('.data')
    text = pe.section('.text')

    def in_(sec, v):
        return sec and sec[1] <= v < sec[1] + sec[2]

    # instruction starts that follow padding, a return or an unconditional jump: plausible entries
    # for code Ghidra did not make a function (vtable-only targets have no direct caller).
    boundary = set()
    for i in range(1, len(A)):
        pm = M[i - 1]
        if pm == 'int3' or pm.startswith('ret') or pm == 'jmp' or pm == 'nop':
            boundary.add(A[i])
    entryset = set(f[0] for f in funcs)
    # vtables: immediates into .rdata whose words are code entries
    imm_rdata = set()
    for i in range(len(A)):
        for h in HEX.findall(re.sub(r'\[[^\]]*\]', '', O[i])):
            v = int(h, 16)
            if in_(rdata, v):
                imm_rdata.add(v)
    vtables = {}
    for v in sorted(imm_rdata):
        slots = []
        p = v
        while True:
            w = pe.u32(p)
            if w is None or not (w in entryset or w in boundary or w in thunks):
                break
            w = resolve(w)
            if slots and p in imm_rdata:
                break
            slots.append(w)
            p += 4
        if len(slots) >= 2:
            vtables[v] = slots
    extra = {w for sl in vtables.values() for w in sl} - entryset
    funcs = sorted(funcs + [(w, 0, 'VT_%08x' % w) for w in extra])
    entries = [f[0] for f in funcs]
    entryset = set(entries)
    F = {e: Func(e, s, n) for e, s, n in funcs}

    strcache = {}

    def string_at(v):
        if v in strcache:
            return strcache[v]
        s = None
        if in_(rdata, v) or in_(data, v):
            t = pe.cstr(v, 2048)
            if printable(t):
                s = t
        strcache[v] = s
        return s

    fi = -1
    lastmov = {}
    for i in range(len(A)):
        a = A[i]
        while fi + 1 < len(entries) and entries[fi + 1] <= a:
            fi += 1
            lastmov = {}
        if fi < 0:
            continue
        f = F[entries[fi]]
        f.n += 1
        m, o = M[i], O[i]
        if m == 'call' or m == 'jmp':
            mm = MEMABS.fullmatch(o) if o.startswith('dword ptr') else None
            if mm:
                t = int(mm.group(2), 16)
                if t in imps:
                    f.imps.append(imps[t])
                continue
            if REG32.match(o):
                if o in lastmov:
                    f.imps.append(lastmov[o])
                continue
            if HEX.fullmatch(o):
                t = resolve(int(o, 16))
                if t in entryset and (m == 'call' or t != f.entry):
                    f.calls.append(t)
                continue
        if m == 'mov':
            mm = re.fullmatch(r'(e[a-ds][xip]|e[sd]i|ebp), dword ptr \[(0x[0-9a-f]+)\]', o)
            if mm and int(mm.group(2), 16) in imps:
                lastmov[mm.group(1)] = imps[int(mm.group(2), 16)]
        for mm in MEMABS.finditer(o):
            v = int(mm.group(2), 16)
            if in_(rdata, v) and m.startswith('f') or (in_(rdata, v) and mm.group(1) == 'qword'):
                if mm.group(1) == 'qword':
                    raw = pe.data[pe.off(v):pe.off(v) + 8]
                    if len(raw) == 8:
                        f.floats.add(round(struct.unpack('<d', raw)[0], 6))
                elif mm.group(1) == 'dword':
                    raw = pe.data[pe.off(v):pe.off(v) + 4]
                    if len(raw) == 4:
                        f.floats.add(round(struct.unpack('<f', raw)[0], 6))
        if '[' in o:
            stripped = MEMABS.sub('', o)
        else:
            stripped = o
        for h in HEX.findall(stripped):
            v = int(h, 16)
            if v in thunks:
                v = resolve(v)
            if '[' in stripped and ('+ ' + h in stripped or '- ' + h in stripped):
                continue      # displacement
            if v in vtables:
                f.vtrefs.add(v)
                continue
            s = string_at(v)
            if s is not None:
                f.strs.add(s)
                continue
            if v in entryset:
                f.fptrs.add(v)
                continue
            if 0x1000 <= v and not (in_(text, v) or in_(rdata, v) or in_(data, v)) \
                    and v not in (0xffffffff,):
                if m in ('mov', 'push', 'cmp', 'and', 'or', 'xor', 'add', 'sub', 'test', 'imul'):
                    f.consts.add(v)
    for f in F.values():
        for c in f.calls:
            if c in F:
                F[c].callers.add(f.entry)
    return F, vtables, extra
