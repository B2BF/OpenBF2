#!/usr/bin/env python3
"""move_files.py <repo> <old>=<new> ... : git mv files (paths relative to repo) and rewrite every
relative #include "..." in Code/ that resolves to a moved file, plus the moved files' own relative
includes. Prints includes it could not resolve that mention a moved basename."""
import os as _os
_ROOT = _os.path.abspath(_os.path.join(_os.path.dirname(__file__), '..', '..', '..'))
# Ghidra exports live in <repo>/out/dbg-ghidra (never committed); evidence images outside the repo
# ($BF2_DEBUG_BUILDS_DIR for the DICE *_r builds, $BF2_LINUX_SERVER_DIR, $BF2_WIN_GAME_DIR).
import os, re, subprocess, sys

repo = os.path.abspath(sys.argv[1])
moves = {}
for a in (open(sys.argv[2]).read().split() if len(sys.argv) == 3 and not "=" in sys.argv[2] else sys.argv[2:]):
    o, n = a.split('=')
    moves[os.path.normpath(os.path.join(repo, o))] = os.path.normpath(os.path.join(repo, n))

inc_re = re.compile(r'^(\s*#\s*include\s*")([^"]+)(")', re.M)
code = os.path.join(repo, 'Code')
files = []
for root, _, fs in os.walk(code):
    for f in fs:
        if f.endswith(('.cpp', '.h', '.hpp', '.inl', '.c', '.mm')):
            files.append(os.path.join(root, f))

basenames = {os.path.basename(o) for o in moves}
edits = {}
for path in files:
    src = open(path, encoding='utf-8', errors='surrogateescape').read()
    old_dir = os.path.dirname(path)
    new_path = moves.get(path, path)
    new_dir = os.path.dirname(new_path)

    def fix(m):
        inc = m.group(2)
        target = os.path.normpath(os.path.join(old_dir, inc))
        if not os.path.exists(target):
            if os.path.basename(inc) in basenames:
                print('UNRESOLVED', os.path.relpath(path, repo), inc)
            return m.group(0)
        new_target = moves.get(target, target)
        if new_target == target and new_dir == old_dir:
            return m.group(0)
        rel = os.path.relpath(new_target, new_dir)
        if rel == inc:
            return m.group(0)
        return m.group(1) + rel + m.group(3)

    out = inc_re.sub(fix, src)
    if out != src:
        edits[path] = out

for path, out in edits.items():
    open(path, 'w', encoding='utf-8', errors='surrogateescape').write(out)
    print('includes', os.path.relpath(path, repo))
for o, n in moves.items():
    os.makedirs(os.path.dirname(n), exist_ok=True)
    subprocess.check_call(['git', '-C', repo, 'mv', o, n])
    print('mv', os.path.relpath(o, repo), '->', os.path.relpath(n, repo))
