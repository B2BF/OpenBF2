#!/usr/bin/env python3
"""List every dice::hfe::Debug construction in RendDX9_r.dll with its literal arguments.

RendDX9_r inlines nothing of Debug: each diagnostic is
    std::string func(<literal>); std::string file(__FILE__);
    Debug d(type, file, __LINE__, func);   // ctor 0x1011D2B0, thiscall, ret 0x10
    d << ...;  ~Debug()                    // 0x1011D8F0
This script walks back from each ctor call, resolves the four pushed arguments (immediates,
registers loaded by lea/mov, and stack std::string slots built from a literal), and writes CSV:
    site_r,type,file,line,expr,message,func_name_r,func_entry_r,status
message joins the string literals streamed into the Debug object before ~Debug (0x1011D8F0).
status is 'ok' or a short reason the backtrace stopped. func_entry_r comes from Ghidra's
functions.tsv (nearest preceding entry whose body covers the site).

usage: debug_sites.py RendDX9_r.dll RendDX9_r.s functions.tsv out.csv [ctor_va dtor_va]
"""
import bisect
import csv
import re
import sys

sys.path.insert(0, __import__('os').path.dirname(__file__))
from pe import PE          # noqa: E402
import asm                 # noqa: E402

STRING_CTOR = 'basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@QAE@PBD@Z'
TYPES = {0: 'Debug', 1: 'Info', 2: 'Warning', 3: 'Assert', 4: 'Assert(4)', 5: 'DxAssert',
         6: 'Error', 7: 'Error(7)', 8: 'Log'}


def norm_mem(s):
    return s.replace('dword ptr ', '').replace(' ', '')


def main():
    dll, lst, ftsv, out = sys.argv[1:5]
    ctor = int(sys.argv[5], 16) if len(sys.argv) > 5 else 0x1011D2B0
    dtor = int(sys.argv[6], 16) if len(sys.argv) > 6 else 0x1011D8F0
    pe = PE(dll)
    imps = pe.imports()
    sctor = {'dword ptr [0x%x]' % a for a, n in imps.items() if n.endswith(STRING_CTOR)}
    A, M, O = asm.load(lst)
    funcs = asm.functions_tsv(ftsv)
    fentries = [f[0] for f in funcs]
    rows = []
    for i in range(len(A)):
        if M[i] != 'call' or O[i] != '0x%x' % ctor:
            continue
        site = A[i]
        status = 'ok'
        # collect the four pushes going backwards; a call in between breaks the simple model
        pushes = []
        j = i - 1
        while j >= 0 and len(pushes) < 4 and i - j < 60:
            if M[j] == 'push':
                pushes.append(j)
            elif M[j] == 'call' and len(pushes) < 4:
                # the pushes before this call belong to it; we cannot model its arity
                status = 'call-between'
                break
            j -= 1
        vals = [None] * 4

        def reg_value(k, reg):
            """Value of reg at instruction k (walk back to its last write)."""
            for q in range(k - 1, max(k - 120, -1), -1):
                op = O[q]
                if M[q] in ('lea', 'mov') and op.startswith(reg + ','):
                    return M[q], op.split(',', 1)[1].strip()
                if M[q] == 'call' and reg in ('eax', 'ecx', 'edx'):
                    return 'clobbered', ''
            return None, ''

        def literal_for_slot(k, mem):
            """Literal passed to std::string(const char*) constructed at stack slot mem."""
            key = norm_mem(mem)
            for q in range(k - 1, max(k - 400, -1), -1):
                if M[q] == 'call' and O[q] in sctor:
                    # find the ecx load and push just before
                    ecx = None
                    lit = None
                    for r in range(q - 1, max(q - 8, -1), -1):
                        if ecx is None and M[r] == 'lea' and O[r].startswith('ecx,'):
                            ecx = norm_mem(O[r].split(',', 1)[1])
                        if lit is None and M[r] == 'push' and O[r].startswith('0x'):
                            lit = int(O[r], 16)
                        if ecx and lit is not None:
                            break
                    if ecx == key and lit is not None:
                        return pe.cstr(lit)
            return None

        if status == 'ok' and len(pushes) == 4:
            for n, k in enumerate(pushes):
                op = O[k]
                if re.fullmatch(r'0x[0-9a-f]+', op):
                    vals[n] = ('imm', int(op, 16))
                elif re.fullmatch(r'e[a-ds][xip]|e[sd]i|ebp', op):
                    how, src = reg_value(k, op)
                    if how == 'lea':
                        vals[n] = ('slot', src)
                    elif how == 'mov' and re.fullmatch(r'0x[0-9a-f]+', src):
                        vals[n] = ('imm', int(src, 16))
                    else:
                        vals[n] = ('reg', op + ':' + str(how) + ' ' + src)
                else:
                    vals[n] = ('mem', op)
        elif status == 'ok':
            status = 'few-pushes'
        typ = file_ = expr = ''
        line = ''
        if status == 'ok':
            t, f, l, e = vals
            typ = TYPES.get(t[1], str(t[1])) if t[0] == 'imm' else 'reg'
            line = str(l[1]) if l[0] == 'imm' else ''
            if f[0] == 'slot':
                file_ = literal_for_slot(i, f[1]) or ''
            if e[0] == 'slot':
                expr = literal_for_slot(i, e[1])
                expr = '' if expr is None else expr
            if not file_ or not line or typ == 'reg':
                status = 'partial'
            if e[0] == 'slot' and expr == '' and literal_for_slot(i, e[1]) is None:
                status = 'expr-unresolved'
            elif e[0] != 'slot':
                status = 'expr-' + e[0]
        fi = bisect.bisect_right(fentries, site) - 1
        fentry = fname = ''
        if fi >= 0 and funcs[fi][0] <= site:
            fentry = '0x%08X' % funcs[fi][0]
            fname = funcs[fi][2]
        msg = []
        for q in range(i + 1, min(i + 250, len(A))):
            if M[q] == 'call' and O[q] == '0x%x' % dtor:
                break
            if M[q] == 'push' and re.fullmatch(r'0x1[0-9a-f]{7}', O[q]):
                v = int(O[q], 16)
                if pe.in_sec(v, '.rdata'):
                    t = pe.cstr(v, 512)
                    if t and all(32 <= ord(c) < 127 or c in '\t\n' for c in t):
                        msg.append(t.replace('\n', '\\n'))
        rows.append(['0x%08X' % site, typ, file_, line, expr, ' | '.join(msg), fname, fentry, status])
    with open(out, 'w', newline='') as f:
        w = csv.writer(f)
        w.writerow(['site_r', 'type', 'file', 'line', 'expr', 'message', 'func_name_r', 'func_entry_r', 'status'])
        w.writerows(rows)
    from collections import Counter
    print('sites', len(rows), Counter(r[-1] for r in rows), Counter(r[1] for r in rows))


if __name__ == '__main__':
    main()
