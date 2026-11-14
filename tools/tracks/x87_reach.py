#!/usr/bin/env python3
"""Lists bf2x87 uses that stay active in a given build configuration.

bf2x87::wide (Platform/X87Compat.h) stops with a [FRONTIER] on every non-Apple build whose long double
has fewer than 64 mantissa digits: MSVC x86 and x64. This scanner evaluates the #if/#ifdef/#elif/#else
nesting of each source file under a fixed macro set and prints the lines that use bf2x87 in active
regions. Conditions it cannot decide are treated as possibly active and flagged with '?'.

Usage: x87_reach.py [msvc-x64|msvc-x86|linux-x64] [paths...]   (default: msvc-x64, Code/BF2)
"""
import os
import re
import sys

CONFIGS = {
    'msvc-x64': {'_WIN32': 1, '_WIN64': 1, '_MSC_VER': 1940, '_M_X64': 1, '_M_AMD64': 1},
    'msvc-x86': {'_WIN32': 1, '_MSC_VER': 1940, '_M_IX86': 600},
    'linux-x64': {'__linux__': 1, '__GNUC__': 13, '__x86_64__': 1, '__unix__': 1},
}
# Every client image and the non-dedicated build; other project macros stay undecided ('?').
COMMON = {'BF2_IMAGE_RENDDX9': 1, 'DEDICATED': 0, 'BF2_RENDDX9': 1}
for _c in CONFIGS.values():
    _c.update(COMMON)
# Platform/compiler macros a configuration does not define are known to be undefined.
PLATFORM = {'_WIN32', '_WIN64', '_MSC_VER', '_M_X64', '_M_AMD64', '_M_IX86', '_M_ARM64', '__APPLE__',
            '__MACH__', '__linux__', '__unix__', '__GNUC__', '__clang__', '__x86_64__', '__i386__',
            '__aarch64__', '__arm64__', 'BF2_TEST_SOFT_EXTENDED'}
SKIP_FILES = ('X87Compat.h', 'ExtendedFloat.h', 'PortableX87.h')
USE = re.compile(r'\bbf2x87::|using namespace bf2x87|using bf2x87::')


class Unknown(Exception):
    pass


def evaluate(expr, defs):
    expr = re.sub(r'//.*', '', expr)
    expr = re.sub(r'/\*.*?\*/', '', expr)

    def repl_defined(m):
        n = m.group(1)
        if n in defs:
            return '1'
        if n in PLATFORM:
            return '0'
        return 'UNKNOWN_MACRO_' + n
    e = re.sub(r'defined\s*\(\s*(\w+)\s*\)', repl_defined, expr)
    e = re.sub(r'defined\s+(\w+)', repl_defined, e)

    def repl_ident(m):
        w = m.group(0)
        if w in defs:
            return str(defs[w])
        if re.match(r'^\d', w):
            return w
        if w in PLATFORM:
            return '0'
        raise Unknown(w)
    e = re.sub(r'\b[A-Za-z_]\w*\b', repl_ident, e)
    e = e.replace('&&', ' and ').replace('||', ' or ')
    e = re.sub(r'!(?!=)', ' not ', e)
    e = re.sub(r'(\d+)[uUlL]+', r'\1', e)
    try:
        return bool(eval(e, {}, {}))
    except Exception:
        raise Unknown(expr.strip())


def scan(path, defs, out):
    # stack entries: [state, taken] state in {True, False, None(unknown)}
    stack = []
    active = lambda: all(s[0] is not False for s in stack)
    unsure = lambda: any(s[0] is None for s in stack)
    with open(path, encoding='utf-8', errors='replace') as f:
        lines = f.readlines()
    i = 0
    while i < len(lines):
        line = lines[i]
        full = line
        while full.rstrip().endswith('\\') and i + 1 < len(lines):
            i += 1
            full = full.rstrip()[:-1] + ' ' + lines[i]
        m = re.match(r'\s*#\s*(if|ifdef|ifndef|elif|else|endif)\b(.*)', full)
        if m:
            kw, rest = m.group(1), m.group(2)
            if kw in ('if', 'ifdef', 'ifndef'):
                name = rest.split()[0] if rest.split() else ''
                known = name in defs or name in PLATFORM
                if kw == 'ifdef':
                    st = (name in defs) if known else None
                elif kw == 'ifndef':
                    st = (name not in defs) if known else None
                else:
                    try:
                        st = evaluate(rest, defs)
                    except Unknown:
                        st = None
                stack.append([st, st is True])
            elif kw == 'elif' and stack:
                top = stack[-1]
                if top[1]:
                    top[0] = False
                else:
                    try:
                        st = evaluate(rest, defs)
                    except Unknown:
                        st = None
                    top[0] = st
                    top[1] = st is True
            elif kw == 'else' and stack:
                top = stack[-1]
                top[0] = False if top[1] else (None if top[0] is None else True)
                top[1] = True
            elif kw == 'endif' and stack:
                stack.pop()
        elif active() and USE.search(line) and not line.lstrip().startswith('//'):
            out.append('%s:%d:%s %s' % (path, i + 1, '?' if unsure() else ' ', line.strip()[:110]))
        i += 1


def main():
    args = sys.argv[1:]
    cfg = 'msvc-x64'
    if args and args[0] in CONFIGS:
        cfg = args.pop(0)
    roots = args or ['Code/BF2']
    out = []
    for root in roots:
        for dp, _, fs in os.walk(root):
            for fn in fs:
                if fn.endswith(('.cpp', '.h', '.inl')) and fn not in SKIP_FILES:
                    scan(os.path.join(dp, fn), CONFIGS[cfg], out)
    for o in sorted(out):
        print(o)
    print('%s: %d active bf2x87 lines' % (cfg, len(out)), file=sys.stderr)


if __name__ == '__main__':
    main()
