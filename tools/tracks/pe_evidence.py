#!/usr/bin/env python3
"""Read-only evidence for the retail Win32 images of the rend track (RendDX9.dll, BF2.exe).

    python3 tools/tracks/pe_evidence.py fn 0x1002BC50 [--image BF2.exe] [--no-c]
    python3 tools/tracks/pe_evidence.py refs 0x1023F3D8          # instructions mentioning a value
    python3 tools/tracks/pe_evidence.py words 0x101DE2A4 40      # dwords (vtables, tables)
    python3 tools/tracks/pe_evidence.py str "renderer.drawHud"   # string -> address -> refs
    python3 tools/tracks/pe_evidence.py callers 0x1002BC50

Python 3 standard library plus `objdump` (Xcode llvm). Inputs, all read-only:
  the image          ~/Applications/Battlefield 2.app/.../drive_c/Battlefield 2/<image>
                     (override with REND_GAME_DIR)
  function list      out/rend-ghidra/<RendDX9|BF2exe>/functions.tsv (tools/tracks/ghidra/ExportFunctions.java)
  decompile cache    out/rend-evidence/<RendDX9|BF2exe>/0xADDR.c (tools/tracks/ghidra/DecompPE.java)
  listing cache      out/rend-evidence/<RendDX9|BF2exe>.s (objdump -d -M intel; built on first use)
  ledger             docs/tracks/rend/ledger.csv (names for call targets)
REND_EVIDENCE_ROOT overrides the checkout whose out/ and docs/ are read (agents in other worktrees
point it at the rend-track worktree).

`fn` prints the function's instructions (from its entry to the next function entry), with call
targets named from the ledger, import-table slots named (`[IAT Memory.dll!bf_free]`), and
.rdata/.data operands decoded (strings, float/double constants, vtable/function pointers), then the
cached Ghidra decompile. The listing is the source of truth; the decompile is a structural hint
(it drops x87 detail and stack arguments after calls).
"""

import argparse
import bisect
import csv
import os
import re
import struct
import subprocess
import sys

REPO = os.environ.get("REND_EVIDENCE_ROOT") or os.path.abspath(
    os.path.join(os.path.dirname(__file__), "..", ".."))
GAME = os.environ.get("REND_GAME_DIR") or os.path.expanduser(
    "~/Applications/Battlefield 2.app/Contents/SharedSupport/prefix/drive_c/Battlefield 2")
SUB = {"RendDX9.dll": "RendDX9", "BF2.exe": "BF2exe"}


class PE:
    def __init__(self, path):
        with open(path, "rb") as f:
            self.data = f.read()
        d = self.data
        pe = struct.unpack_from("<I", d, 0x3C)[0]
        nsec = struct.unpack_from("<H", d, pe + 6)[0]
        optsz = struct.unpack_from("<H", d, pe + 20)[0]
        opt = pe + 24
        self.base = struct.unpack_from("<I", d, opt + 28)[0]
        dirs = opt + 96
        self.sections = []
        s = opt + optsz
        for i in range(nsec):
            name = d[s:s + 8].rstrip(b"\0").decode()
            vsize, va, rsize, raw = struct.unpack_from("<IIII", d, s + 8)
            self.sections.append((name, self.base + va, max(vsize, rsize), raw, rsize))
            s += 40
        self.imports = {}
        imp_rva = struct.unpack_from("<I", d, dirs + 8)[0]
        if imp_rva:
            p = self.off(self.base + imp_rva)
            while True:
                lookup, _, _, name_rva, iat = struct.unpack_from("<IIIII", d, p)
                if not name_rva:
                    break
                dll = self.cstr(self.base + name_rva)
                q, slot = self.off(self.base + (lookup or iat)), self.base + iat
                while True:
                    e = struct.unpack_from("<I", d, q)[0]
                    if not e:
                        break
                    sym = "#%d" % (e & 0xFFFF) if e & 0x80000000 else self.cstr(self.base + e + 2)
                    self.imports[slot] = "%s!%s" % (dll, sym)
                    q += 4
                    slot += 4
                p += 20

    def off(self, va):
        for name, sva, size, raw, rsize in self.sections:
            if sva <= va < sva + size:
                o = va - sva
                return raw + o if o < rsize else None
        return None

    def section(self, va):
        for name, sva, size, raw, rsize in self.sections:
            if sva <= va < sva + size:
                return name
        return None

    def cstr(self, va, limit=200):
        o = self.off(va)
        if o is None:
            return None
        e = self.data.find(b"\0", o, o + limit)
        return self.data[o:e if e >= 0 else o + limit].decode("latin-1")

    def dword(self, va):
        o = self.off(va)
        return None if o is None or o + 4 > len(self.data) else struct.unpack_from("<I", self.data, o)[0]


def load_funcs(image):
    path = os.path.join(REPO, "out", "rend-ghidra", SUB[image], "functions.tsv")
    with open(path, newline="") as f:
        rows = list(csv.DictReader(f, delimiter="\t"))
    return sorted(int(r["entry"], 16) for r in rows)


def load_names(image):
    names = {}
    path = os.path.join(REPO, "docs", "tracks", "rend", "ledger.csv")
    if os.path.exists(path):
        with open(path, newline="") as f:
            for r in csv.DictReader(f):
                if r["image"] == image and not r["symbol"].startswith("FUN_"):
                    names[int(r["address"], 16)] = "%s [%s]" % (r["symbol"], r["status"])
    return names


def listing(image):
    path = os.path.join(REPO, "out", "rend-evidence", SUB[image] + ".s")
    if not os.path.exists(path):
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "w") as f:
            subprocess.run(["objdump", "-d", "--no-show-raw-insn", "-M", "intel",
                            os.path.join(GAME, image)], check=True, stdout=f)
    return path


LINE_RE = re.compile(r"^\s*([0-9a-f]{6,8}):\s*(.*)$")
HEX_RE = re.compile(r"0x([0-9a-f]{6,8})\b")


def describe(pe, funcs, names, v, mem_operand, width):
    if v in pe.imports:
        return "IAT " + pe.imports[v]
    sec = pe.section(v)
    if sec is None:
        return None
    if sec == ".text":
        i = bisect.bisect_left(funcs, v)
        if i < len(funcs) and funcs[i] == v:
            return names.get(v, "fn")
        return None
    out = []
    s = pe.cstr(v, 80)
    if s and len(s) >= 2 and all(32 <= ord(c) < 127 or c in "\t\n" for c in s):
        out.append(repr(s))
    if mem_operand:
        o = pe.off(v)
        if o is not None:
            if width == "qword":
                out.append("double %r" % struct.unpack_from("<d", pe.data, o)[0])
            elif width == "dword":
                w = struct.unpack_from("<I", pe.data, o)[0]
                fl = struct.unpack_from("<f", pe.data, o)[0]
                if w in pe.imports:
                    out.append("-> IAT " + pe.imports[w])
                elif pe.section(w) == ".text":
                    out.append("-> 0x%08X %s" % (w, names.get(w, "")))
                else:
                    out.append("dword 0x%X / float %g" % (w, fl))
            elif width == "tbyte":
                out.append("long double")
    else:
        w = pe.dword(v)
        if w is not None and pe.section(w) == ".text" and not out:
            out.append("table: [0]=0x%08X %s" % (w, names.get(w, "")))
    return "%s %s" % (sec, "; ".join(out)) if out else sec


def annotate(pe, funcs, names, text):
    notes = []
    width = None
    m = re.search(r"\b(byte|word|dword|qword|tbyte|xmmword) ptr \[0x([0-9a-f]+)\]", text)
    mem = set()
    if m:
        width = m.group(1)
        mem.add(int(m.group(2), 16))
    for h in HEX_RE.finditer(text):
        v = int(h.group(1), 16)
        d = describe(pe, funcs, names, v, v in mem, width)
        if d and d not in ("fn",):
            notes.append("0x%X=%s" % (v, d))
    return text + ("    ; " + " | ".join(notes) if notes else "")


def cmd_fn(a, pe, funcs, names):
    va = int(a.addr, 16)
    i = bisect.bisect_right(funcs, va) - 1
    start = funcs[i] if i >= 0 else va
    end = funcs[i + 1] if i + 1 < len(funcs) else start + 0x1000
    if start != va:
        print("; 0x%08X is inside the function at 0x%08X" % (va, start))
    print("; %s 0x%08X..0x%08X (%d bytes)  %s" % (a.image, start, end, end - start, names.get(start, "")))
    with open(listing(a.image)) as f:
        for line in f:
            m = LINE_RE.match(line)
            if not m:
                continue
            ad = int(m.group(1), 16)
            if ad < start:
                continue
            if ad >= end:
                break
            print("%08X: %s" % (ad, annotate(pe, funcs, names, m.group(2).strip())))
    if not a.no_c:
        c = os.path.join(REPO, "out", "rend-evidence", SUB[a.image], "0x%08X.c" % start)
        if os.path.exists(c):
            print("\n" + open(c).read())
        else:
            print("\n; no cached decompile (%s)" % c)


def cmd_refs(a, pe, funcs, names, pattern=None):
    pat = pattern or ("0x%x" % int(a.addr, 16))
    rx = re.compile(re.escape(pat) + r"\b")
    with open(listing(a.image)) as f:
        for line in f:
            m = LINE_RE.match(line)
            if m and rx.search(m.group(2)):
                ad = int(m.group(1), 16)
                i = bisect.bisect_right(funcs, ad) - 1
                fn = funcs[i] if i >= 0 else 0
                print("%08X (in 0x%08X %s): %s" % (ad, fn, names.get(fn, ""), m.group(2).strip()))


def cmd_words(a, pe, funcs, names):
    va = int(a.addr, 16)
    for k in range(int(a.count)):
        w = pe.dword(va + 4 * k)
        if w is None:
            break
        d = describe(pe, funcs, names, w, False, None) or ""
        print("%08X +0x%02X: 0x%08X %s" % (va + 4 * k, 4 * k, w, d))


def cmd_str(a, pe, funcs, names):
    needle = a.addr.encode("latin-1")
    for name, sva, size, raw, rsize in pe.sections:
        if name not in (".rdata", ".data"):
            continue
        o = raw
        while True:
            j = pe.data.find(needle, o, raw + rsize)
            if j < 0:
                break
            s0 = j
            while s0 > raw and pe.data[s0 - 1] != 0:
                s0 -= 1
            va = sva + (s0 - raw)
            print("0x%08X %s %r" % (va, name, pe.cstr(va, 120)))
            cmd_refs(a, pe, funcs, names, pattern="0x%x" % va)
            o = j + 1


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("cmd", choices=["fn", "refs", "words", "str", "callers"])
    ap.add_argument("addr")
    ap.add_argument("count", nargs="?", default="32")
    ap.add_argument("--image", default=None, choices=list(SUB))
    ap.add_argument("--no-c", action="store_true")
    a = ap.parse_args()
    if a.image is None:
        a.image = "BF2.exe" if a.cmd != "str" and int(a.addr, 16) < 0x10000000 else "RendDX9.dll"
    pe = PE(os.path.join(GAME, a.image))
    funcs = load_funcs(a.image)
    names = load_names(a.image)
    if a.cmd == "fn":
        cmd_fn(a, pe, funcs, names)
    elif a.cmd in ("refs", "callers"):
        cmd_refs(a, pe, funcs, names)
    elif a.cmd == "words":
        cmd_words(a, pe, funcs, names)
    else:
        cmd_str(a, pe, funcs, names)


if __name__ == "__main__":
    main()
