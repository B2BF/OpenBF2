#!/usr/bin/env python3
"""[CHOSEN, not recovered] Compare clang record layouts of every TU compiled for arm64 and x86_64.
Usage: SCAN_ROOT=<repo> layout_diff_arm64.py compile_commands.json out.json [jobs]"""
import json, re, shlex, subprocess, sys, os
from concurrent.futures import ThreadPoolExecutor
db = json.load(open(sys.argv[1])); out = sys.argv[2]; jobs = int(sys.argv[3]) if len(sys.argv) > 3 else 6
ROOT = os.environ['SCAN_ROOT']
SKIP = ('/External/python-embedded/', '/External/zlib', '/External/libvorbis', '/External/libogg')
blk = re.compile(r"\*\*\* Dumping AST Record Layout\nType: (?P<t>[^\n]*)\n\nLayout: <ASTRecordLayout\n  Size:(?P<s>\d+)\n  DataSize:\d+\n  Alignment:(?P<a>\d+)\n(?P<rest>.*?)>\n\n", re.S)
def cmd(e, arch):
    args = shlex.split(e['command']) if 'command' in e else list(e['arguments'])
    a = args[1:]; cl = [args[0]]; i = 0
    while i < len(a):
        if a[i] == '-o': i += 2; continue
        if a[i] == '-c': i += 1; continue
        if a[i] == '-arch': cl += ['-arch', arch]; i += 2; continue
        cl.append(a[i]); i += 1
    return cl + ['-fsyntax-only', '-Xclang', '-fdump-record-layouts-simple', '-w']
def lay(e, arch):
    p = subprocess.run(cmd(e, arch), cwd=e['directory'], capture_output=True, text=True, errors='replace')
    d = {}
    for m in blk.finditer(p.stdout):
        d[m.group('t')] = (int(m.group('s')), int(m.group('a')), ' '.join(m.group('rest').split()))
    return p.returncode, d
def one(e):
    f = e['file']
    if not f.startswith(ROOT) or any(s in f for s in SKIP) or not f.endswith(('.cpp', '.cc', '.mm', '.c')):
        return None
    ra, a = lay(e, 'arm64'); rx, x = lay(e, 'x86_64')
    diffs = [(t, x[t], a[t]) for t in sorted(set(a) & set(x)) if a[t] != x[t]]
    return (os.path.relpath(f, ROOT), ra, rx, len(a), diffs)
res = []
with ThreadPoolExecutor(jobs) as ex:
    for r in ex.map(one, db):
        if r: res.append(r)
types = {}
for f, ra, rx, n, diffs in res:
    for t, xv, av in diffs:
        types.setdefault(t, {'x86_64': xv[:2], 'arm64': av[:2], 'tus': []})['tus'].append(f)
json.dump({'tus': len(res), 'failed_arm64': [r[0] for r in res if r[1]], 'failed_x86_64': [r[0] for r in res if r[2]],
           'records_checked': sum(r[3] for r in res), 'differences': types}, open(out, 'w'), indent=1)
print('tus', len(res), 'diff types', len(types), 'arm64 fails', sum(1 for r in res if r[1]), 'x86 fails', sum(1 for r in res if r[2]))
for t, v in sorted(types.items()):
    print('%-70s x86_64 size/align %s arm64 %s (%d TUs)' % (t[:70], v['x86_64'], v['arm64'], len(v['tus'])))
