#!/usr/bin/env python3
"""jt.py <table_va> <count> : decode an AIDLL_r jump table; print each case's first pushed string."""
import os as _os
_ROOT = _os.path.abspath(_os.path.join(_os.path.dirname(__file__), '..', '..', '..'))
# Ghidra exports live in <repo>/out/dbg-ghidra (never committed); evidence images outside the repo
# ($BF2_DEBUG_BUILDS_DIR for the DICE *_r builds, $BF2_LINUX_SERVER_DIR, $BF2_WIN_GAME_DIR).
import struct, sys
if not _os.environ.get('BF2_DEBUG_BUILDS_DIR'):
    sys.exit('jt.py: set BF2_DEBUG_BUILDS_DIR to the directory holding AIDLL_r.dll')
d = open(_os.path.join(_os.environ['BF2_DEBUG_BUILDS_DIR'], 'AIDLL_r.dll'), 'rb').read()
base = 0x10000000
def cstr(va):
    o = va - base
    return d[o:d.index(b'\0', o)].decode('latin-1')
jt, n = int(sys.argv[1], 16), int(sys.argv[2])
for i in range(n):
    t = struct.unpack_from('<I', d, jt - base + 4 * i)[0]
    b = d[t - base:t - base + 24]
    s = '?'
    for k in range(20):
        if b[k] == 0x68:
            v = struct.unpack_from('<I', b, k + 1)[0]
            if 0x10285000 < v < 0x102d0000:
                s = cstr(v)
                break
    print(i, hex(t), s)
