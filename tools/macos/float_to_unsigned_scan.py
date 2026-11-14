#!/usr/bin/env python3
"""[CHOSEN, not recovered] List float->unsigned (and float->narrow) conversions in project code.
x86_64 cvttss2si wraps negative/out-of-range values modulo 2^N for unsigned targets;
arm64 fcvtzu/fcvtzs saturate (and NaN -> 0). Usage: SCAN_ROOT=<repo> float_to_unsigned_scan.py compile_commands.json out.tsv [jobs]"""
import json, re, shlex, subprocess, sys, os
from concurrent.futures import ThreadPoolExecutor
db = json.load(open(sys.argv[1])); out = sys.argv[2]; jobs = int(sys.argv[3]) if len(sys.argv) > 3 else 4
ROOT = os.environ.get('SCAN_ROOT') or os.path.realpath(os.path.join(os.path.dirname(sys.argv[1]), '../../..'))
SKIP = ('/External/python-embedded/', '/External/zlib', '/External/libvorbis', '/External/imgui', '/External/libogg')
locre = re.compile(r"<(?:(?P<file>/[^:>]+):(?P<l1>\d+):\d+|line:(?P<l2>\d+):\d+|col:\d+)")
seen = set()
def scan(e):
    f = e['file']
    if not f.startswith(ROOT) or any(s in f for s in SKIP) or not f.endswith(('.cpp', '.cc', '.mm')):
        return []
    args = shlex.split(e['command']) if 'command' in e else e['arguments']
    a = [x for x in args[1:]]
    # drop -o X and -c
    cl = [args[0]]; i = 0
    while i < len(a):
        if a[i] == '-o': i += 2; continue
        if a[i] in ('-c',) : i += 1; continue
        cl.append(a[i]); i += 1
    cl += ['-fsyntax-only', '-Xclang', '-ast-dump', '-fno-color-diagnostics', '-w']
    p = subprocess.Popen(cl, cwd=e['directory'], stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, text=True, errors='replace')
    cur_file = None; cur_line = None; res = []
    for line in p.stdout:
        m = locre.search(line)
        if m:
            if m.group('file'): cur_file = m.group('file'); cur_line = m.group('l1')
            elif m.group('l2'): cur_line = m.group('l2')
        if '<FloatingToIntegral>' in line:
            t = re.findall(r"'([^']*)'", line.split('<FloatingToIntegral>')[0])
            ty = t[-1] if t else '?'
            if cur_file and cur_file.startswith(ROOT) and not any(s in cur_file for s in SKIP):
                if re.fullmatch(r"(const )?(unsigned int|unsigned long|unsigned long long|uint32_t|uint64_t|size_t|DWORD|U32|UINT|uint|unsigned)", ty):
                    res.append((os.path.relpath(cur_file, ROOT), cur_line, ty))
    p.wait()
    return res
with ThreadPoolExecutor(jobs) as ex:
    allres = set()
    for r in ex.map(scan, db):
        allres.update(r)
with open(out, 'w') as o:
    for f, l, t in sorted(allres, key=lambda x: (x[0], int(x[1] or 0))):
        o.write('%s\t%s\t%s\n' % (f, l, t))
print(len(allres), 'sites')
