"""Load an llvm-objdump Intel listing (objdump -d --x86-asm-syntax=intel --no-show-raw-insn)."""
import re

LINE = re.compile(r'^([0-9a-f]{8}):\s+(\S+)(?:\s+(.*?))?\s*$')
SYM = re.compile(r'\s*<[^>]*>')


def load(path):
    """Return (addrs, mnems, ops) lists in address order. Symbolic <...> suffixes are stripped."""
    addrs, mn, ops = [], [], []
    with open(path, 'r', errors='replace') as f:
        for line in f:
            m = LINE.match(line)
            if not m:
                continue
            addrs.append(int(m.group(1), 16))
            mn.append(m.group(2))
            o = m.group(3) or ''
            ops.append(SYM.sub('', o))
    return addrs, mn, ops


def functions_tsv(path):
    """Ghidra ExportFunctions functions.tsv -> sorted list of (entry, size, name)."""
    res = []
    with open(path) as f:
        next(f)
        for line in f:
            p = line.rstrip('\n').split('\t')
            res.append((int(p[0], 16), int(p[1]), p[3]))
    res.sort()
    return res
