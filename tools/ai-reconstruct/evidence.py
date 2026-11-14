#!/usr/bin/env python3
"""Evidence cache and listing tool for the AI image (libaidll.so, ia-32 canonical, amd-64 helper).

    python3 tools/ai-reconstruct/evidence.py <command> [options]

Commands (all print to stdout; `--image amd-64` switches every command to the x86-64 image):
    fn <symbol|0xADDR> [--att] [--decompile]
                       annotated listing of one function (Intel syntax by default). GOT slots, GOT-relative
                       data, rodata constants (float/double/long double by operand width, strings), jump
                       tables and call targets are resolved and demangled; `call [reg+N]` gets its
                       vtable slot. Prints the cached .s/.c/.amd64.* paths when the cache has the function.
                       --decompile also runs Ghidra headless when the cache has no decompile (~30 s).
                       A symbol argument is a demangled substring or an exact mangled name; several
                       matches are listed instead.
    decompile <0xADDR...> [--out DIR]
                       Ghidra headless decompile of ELF addresses into DIR (default: a temp dir under out/).
    vtable <class-substring>
                       slot dump(s) of every matching "vtable for" symbol.
    calls <listing.s>  per call: callee plus the stack-argument stores since the previous call, with
                       register sources followed back to their last known value (Intel listings).
    strip-checkpointer <file.c>
                       a Ghidra .c with inlined checkPointer failure blocks collapsed to one comment.
    project [--from GPR] [--create]
                       print (and if needed make) the private Ghidra project for the image, see below.
    build [--jobs N] [--batch N] [--images ia-32,amd-64] [--no-decompile] [--only 0xADDR,...] [--force]
                       (re)build the cache under out/ai-evidence/ (git-ignored).

Cache location: out/ai-evidence/ of this checkout, or $EVIDENCE_OUT (to read a cache built in another
worktree; `build`, `decompile`, `fn --decompile` and `project` write there).

Inputs. Binaries are read from $EVIDENCE_BIN_DIR/<image>/libaidll.so, or from
$BF2_LINUX_SERVER_DIR/bin/<image>/libaidll.so (the Linux 1.5 server installation); --binary overrides
both. Ghidra comes from $GHIDRA_INSTALL_DIR (required for the Ghidra commands). Python 3 standard library only; the ELF symbol, section
and relocation tables are read directly, the disassembly comes from `objdump` (llvm-objdump, as shipped
with Xcode or LLVM; GNU objdump is not supported) and demangling from `c++filt`.

Addresses. Every address this tool prints or accepts is an ELF virtual address. Ghidra rebases the
images (ia-32 Ghidra = ELF + 0x10000, amd-64 Ghidra = ELF + 0x100000); the listing header shows both.

Ghidra project (`project`). The cache never touches an original project. Lookup order for image I:
  1. out/ai-evidence/ghidra/I/libaidll.gpr (the private project the cache uses), if it exists;
  2. --from GPR, or the first `find $GHIDRA_PROJECTS_DIR -name '*.gpr'` project (when that variable is
     set) whose index lists a program named after the binary: copied into place 1 (the original stays read-only);
  3. otherwise (or with --create) `analyzeHeadless ... -import <binary>` with full auto-analysis into
     place 1 (ia-32 ~5 min, amd-64 ~5 min on an 8-core Mac).
Parallel decompile jobs each run on their own copy (out/ai-evidence/ghidra/I/job<N>/), because a
Ghidra project admits one process at a time. Ghidra's user settings and temp files are redirected
into out/ai-evidence/ghidra/home (JAVA_TOOL_OPTIONS -Duser.home/-Djava.io.tmpdir), so a sandbox that
only allows writes in the worktree can still run it.

Cache layout (`build`), for every libaidll.so ledger row with status todo, stop or skip-debug:
    out/ai-evidence/<ADDR>.s          ia-32 annotated listing (Intel)       ADDR = ia-32 ELF address, 0x%X
    out/ai-evidence/<ADDR>.c          ia-32 Ghidra decompile
    out/ai-evidence/<ADDR>.amd64.s    amd-64 listing of the symbol with the same mangled name
    out/ai-evidence/<ADDR>.amd64.c    amd-64 Ghidra decompile
    out/ai-evidence/index.csv         address,symbol,size,file,status,kind,amd64_address,amd64_size,
                                      listing,decompile,decompile_status,amd64_listing,amd64_decompile,
                                      amd64_decompile_status
    out/ai-evidence/vtables/<class>.txt   one file per "vtable for" symbol in the ia-32 image (class name
                                      without dice::hfe::, '::' -> '.', other unsafe characters -> '_');
                                      vtables/index.csv maps class -> file, vtable address, size.
decompile_status: ok | fail: <Ghidra message> | nofunc | missing (no status line: Ghidra died or timed
out on the batch). The ia-32 listing is the source of truth: Ghidra drops bool (byte) arguments,
merges 64-bit masks, loses the stack after sret calls and misreads x87 stacks. The amd-64 image is
for understanding a body (register arguments decompile more cleanly); addresses, layouts, offsets
and stack slots come from ia-32 only.
"""

import argparse
import bisect
import csv
import os
import re
import shutil
import struct
import subprocess
import sys
import tempfile
import time

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
OUT = os.path.abspath(os.environ.get("EVIDENCE_OUT", os.path.join(REPO, "out", "ai-evidence")))
LEDGER = os.path.join(REPO, "docs", "ai-reconstruct", "ledger.csv")
BIN_DIR = os.environ.get("EVIDENCE_BIN_DIR") or (
    os.path.join(os.environ["BF2_LINUX_SERVER_DIR"], "bin") if os.environ.get("BF2_LINUX_SERVER_DIR") else None)
GHIDRA = os.environ.get("GHIDRA_INSTALL_DIR")
GHIDRA_BASE = {"ia-32": 0x10000, "amd-64": 0x100000}
CACHE_STATUSES = ("todo", "stop", "skip-debug")


def run(cmd, inp=None):
    return subprocess.run(cmd, input=inp, check=True, capture_output=True, text=True).stdout


_dem_cache = {}


def demangle(names):
    """Demangle a batch of names through c++filt; unknown names come back unchanged."""
    todo = sorted({n for n in names if n not in _dem_cache})
    if todo:
        out = run(["c++filt", "-n"], "\n".join(todo) + "\n").split("\n")
        for n, d in zip(todo, out):
            _dem_cache[n] = d if d else n
    return {n: _dem_cache.get(n, n) for n in names}


# ---------------------------------------------------------------------------------------------
# ELF reader (32 and 64 bit little endian): sections, symbols, relocations, memory reads.

class Elf:
    def __init__(self, path):
        self.path = path
        self.data = d = open(path, "rb").read()
        if d[:4] != b"\x7fELF":
            raise SystemExit("%s: not an ELF file" % path)
        self.is64 = d[4] == 2
        self.ws = 8 if self.is64 else 4
        if self.is64:
            shoff, = struct.unpack_from("<Q", d, 0x28)
            shentsize, shnum, shstrndx = struct.unpack_from("<HHH", d, 0x3A)
        else:
            shoff, = struct.unpack_from("<I", d, 0x20)
            shentsize, shnum, shstrndx = struct.unpack_from("<HHH", d, 0x2E)
        self.secs = []
        for i in range(shnum):
            o = shoff + i * shentsize
            if self.is64:
                name, typ, flags, addr, off, size, link, info, align, entsz = struct.unpack_from("<IIQQQQIIQQ", d, o)
            else:
                name, typ, flags, addr, off, size, link, info, align, entsz = struct.unpack_from("<IIIIIIIIII", d, o)
            self.secs.append(dict(name=name, type=typ, flags=flags, addr=addr, off=off, size=size,
                                  link=link, info=info, entsize=entsz))
        strtab = self.secs[shstrndx]
        for s in self.secs:
            s["name"] = self._cstr_at(strtab["off"] + s["name"])
        self.sec = {s["name"]: s for s in self.secs}
        self.got = self.sec.get(".got.plt", self.sec.get(".got"))["addr"]
        self.syms = []          # (addr, size, name, type, shndx) from .symtab
        self.dynsyms = []
        for s in self.secs:
            if s["type"] in (2, 11):  # SYMTAB, DYNSYM
                lst = self.syms if s["type"] == 2 else self.dynsyms
                stro = self.secs[s["link"]]["off"]
                n = s["size"] // s["entsize"]
                for i in range(n):
                    o = s["off"] + i * s["entsize"]
                    if self.is64:
                        nm, info, other, shndx, val, size = struct.unpack_from("<IBBHQQ", d, o)
                    else:
                        nm, val, size, info, other, shndx = struct.unpack_from("<IIIBBH", d, o)
                    lst.append((val, size, self._cstr_at(stro + nm), info & 0xF, shndx))
        # relocations: addr -> (type, symbol name or None, addend or None)
        self.rel = {}
        for s in self.secs:
            if s["type"] not in (4, 9):  # RELA, REL
                continue
            symsec = self.secs[s["link"]]
            stro = self.secs[symsec["link"]]["off"]
            n = s["size"] // s["entsize"]
            for i in range(n):
                o = s["off"] + i * s["entsize"]
                if self.is64:
                    if s["type"] == 4:
                        off, info, add = struct.unpack_from("<QQq", d, o)
                    else:
                        off, info = struct.unpack_from("<QQ", d, o); add = None
                    typ, si = info & 0xFFFFFFFF, info >> 32
                else:
                    if s["type"] == 4:
                        off, info, add = struct.unpack_from("<IIi", d, o)
                    else:
                        off, info = struct.unpack_from("<II", d, o); add = None
                    typ, si = info & 0xFF, info >> 8
                sname = None
                if si:
                    so = symsec["off"] + si * symsec["entsize"]
                    nm = struct.unpack_from("<I", d, so)[0]
                    sname = self._cstr_at(stro + nm)
                self.rel[off] = (typ, sname, add)
        # address-sorted named symbols (functions and objects) for name lookup
        named = [(a, sz, n, t) for a, sz, n, t, sh in self.syms if n and a and t in (0, 1, 2) and sh != 0
                 and not n.startswith(".L") and not n.startswith("$")]
        named.sort(key=lambda x: (x[0], -x[1], x[3] != 2))
        self.sym_list = named
        self.sym_addrs = [x[0] for x in named]
        self.addr_of = {n: a for a, sz, n, t, sh in self.syms if sh != 0 and a}
        self.funcs = sorted({(a, sz, n) for a, sz, n, t, sh in self.syms if t == 2 and sh != 0 and a})
        dm = demangle([n for _, _, n, _ in named])
        self.dem = dm

    def _cstr_at(self, off):
        e = self.data.find(b"\0", off)
        return self.data[off:e].decode("latin-1")

    def section_of(self, va):
        for s in self.secs:
            if s["addr"] and s["addr"] <= va < s["addr"] + s["size"] and s["flags"] & 2:
                return s
        return None

    def read(self, va, n):
        s = self.section_of(va)
        if s is None or s["type"] == 8:  # NOBITS
            return None
        o = s["off"] + va - s["addr"]
        return self.data[o:o + n]

    def word(self, va):
        b = self.read(va, self.ws)
        if b is None or len(b) < self.ws:
            return None
        return struct.unpack("<Q" if self.is64 else "<I", b)[0]

    def cstr(self, va, limit=120):
        b = self.read(va, limit)
        if not b:
            return None
        e = b.find(b"\0")
        if e < 0:
            return None
        s = b[:e]
        if len(s) < 1 or any(c < 0x20 and c not in (9, 10, 13) or c > 0x7E for c in s):
            return None
        return s.decode("ascii")

    def sym_at(self, va):
        """(name, offset) of the symbol covering va, exact symbols preferred; None outside all."""
        i = bisect.bisect_right(self.sym_addrs, va) - 1
        best = None
        j = i
        while j >= 0 and j > i - 64:
            a, sz, n, t = self.sym_list[j]
            if a == va:
                return n, 0
            if a < va < a + max(sz, 1) and best is None:
                best = (n, va - a)
            if a + 0x100000 < va:
                break
            j -= 1
        if best:
            return best
        if i >= 0:
            a, sz, n, t = self.sym_list[i]
            s1, s2 = self.section_of(a), self.section_of(va)
            if s1 is s2 and s1 is not None and s1["name"] == ".text":
                return n, va - a
        return None

    def name(self, va):
        r = self.sym_at(va)
        if not r:
            return None
        n, o = r
        n = self.dem.get(n, n)
        return n if o == 0 else "%s+0x%x" % (n, o)

    def slot_target(self, va):
        """What a pointer-sized word at va points to: relocation symbol or symbol at its value."""
        r = self.rel.get(va)
        if r:
            typ, sname, add = r
            if sname:
                d = demangle([sname])[sname]
                if add:
                    d += "%+#x" % add
                return d
            val = add if add is not None else self.word(va)
            if val is None:
                return None
            return self.name(val) or "0x%x" % val
        return None


# ---------------------------------------------------------------------------------------------
# Disassembly and annotation

LINE_RE = re.compile(r"^\s*([0-9a-f]+):\s+(.*)$")
FUNC_RE = re.compile(r"^([0-9a-f]+) <(.*)>:$")
MEM_RE = re.compile(r"(?:(byte|word|dword|qword|tbyte|xmmword|ymmword) ptr )?(?:[a-z]s:)?\[([^\]]+)\]")
REG32 = ("eax", "ebx", "ecx", "edx", "esi", "edi", "ebp", "esp")
REG64 = ("rax", "rbx", "rcx", "rdx", "rsi", "rdi", "rbp", "rsp") + tuple("r%d" % i for i in range(8, 16))
SUBREG = {}
for _r in "abcd":
    for _f in ("e%sx", "r%sx", "%sx", "%sl", "%sh"):
        SUBREG[_f % _r] = "r%sx" % _r
for _r in ("si", "di", "bp", "sp"):
    for _f in ("e%s", "r%s", "%s", "%sl"):
        SUBREG[_f % _r] = "r%s" % _r
for _i in range(8, 16):
    for _f in ("r%d", "r%dd", "r%dw", "r%db"):
        SUBREG[_f % _i] = "r%d" % _i
NO_DEST = ("cmp", "test", "push", "jmp", "call", "ret", "nop", "ucomis", "comis", "fucom", "fcom", "fst", "fist",
           "bt ", "leave")


def canon(reg):
    return SUBREG.get(reg)


def disassemble(elf, start=None, stop=None, att=False):
    """[(addr, text)] for the range (whole .text/.plt/.init/.fini when start is None)."""
    cmd = ["objdump", "-d", "--no-show-raw-insn"]
    if not att:
        cmd += ["-M", "intel"]
    if start is not None:
        cmd += ["--start-address=0x%x" % start, "--stop-address=0x%x" % stop]
    out = run(cmd + [elf.path])
    res = []
    for line in out.splitlines():
        m = LINE_RE.match(line)
        if m:
            t = m.group(2).replace("\t", " ").strip()
            t = re.sub(r"\s+", " ", t)
            res.append((int(m.group(1), 16), t))
    return res


def demangle_target(text):
    """Demangle the `<symbol[+off][@plt]>` label objdump prints after a branch target."""
    m = re.search(r"<([^>]+)>", text)
    if not m:
        return text
    nm = m.group(1)
    base, plus, off = nm.split("@")[0].partition("+")
    d = demangle([base])[base]
    return text.replace("<%s>" % nm, "<%s%s%s>" % (d, "+" + off if plus else "", "@plt" if "@plt" in nm else ""))


class Annotator:
    def __init__(self, elf):
        self.elf = elf
        self.pc_reg = "rip" if elf.is64 else None

    def describe(self, va, width=None, is_lea=False):
        """Human description of the datum at va (GOT slot, symbol, rodata constant, string)."""
        e = self.elf
        r = e.rel.get(va)
        s = e.section_of(va)
        sname = s["name"] if s else None
        if r and sname in (".got", ".got.plt"):
            return ("&" if is_lea else "") + "GOT[%s]" % (e.slot_target(va) or "?")
        if r:  # an initialised pointer variable in .data/.data.rel.ro
            return "%s%s = &%s" % ("&" if is_lea else "", e.name(va) or "0x%x" % va, e.slot_target(va) or "?")
        nm = e.name(va)
        if sname == ".rodata" or (sname is None and nm is None):
            st = e.cstr(va)
            if st is not None and (is_lea or width is None) and len(st) >= 1:
                return 'rodata 0x%x "%s"' % (va, st.replace("\n", "\\n")[:100])
            b = e.read(va, 16)
            if b is None:
                return "0x%x" % va
            if width == "dword":
                return "rodata 0x%x float %r int %d" % (va, round(struct.unpack("<f", b[:4])[0], 9),
                                                        struct.unpack("<i", b[:4])[0])
            if width == "qword":
                return "rodata 0x%x double %r (lo int %d)" % (va, struct.unpack("<d", b[:8])[0],
                                                             struct.unpack("<i", b[:4])[0])
            if width == "tbyte":
                return "rodata 0x%x long double %r" % (va, f80(b[:10]))
            if width in ("xmmword", "ymmword"):
                return "rodata 0x%x %s" % (va, b.hex())
            if width == "word":
                return "rodata 0x%x short %d" % (va, struct.unpack("<h", b[:2])[0])
            if width == "byte":
                return "rodata 0x%x byte %d" % (va, b[0])
            if nm:
                return ("&" if is_lea else "") + nm
            if is_lea:
                return "&rodata 0x%x" % va
            return "rodata 0x%x (f %g, i %d)" % (va, struct.unpack("<f", b[:4])[0], struct.unpack("<i", b[:4])[0])
        if nm:
            return ("&" if is_lea else "") + nm + ("" if sname is None else " [%s]" % sname)
        return "0x%x [%s]" % (va, sname)

    def annotate(self, insns, fstart, fend):
        """[(addr, text, notes, extra_lines)] for a function's instructions (Intel syntax)."""
        e = self.elf
        known = {}       # canonical reg -> address value
        got_regs = set()
        out = []
        prev = []        # recent (op, ops) for jump table bounds
        last_thunk = None
        self.snap = []
        for idx, (a, t) in enumerate(insns):
            self.snap.append(dict(known))
            notes, extra = [], []
            op, _, ops = t.partition(" ")
            ops = ops.strip()
            # objdump's own rip-relative comment "# 0xADDR <sym>": replace with our description
            ripaddr = None
            m = re.search(r"\s*# (0x[0-9a-f]+)(?: <[^>]*>)?$", ops)
            if m:
                ripaddr = int(m.group(1), 16)
                ops = ops[:m.start()]
            text = op + (" " + ops if ops else "")
            # call / jmp targets with symbol names: demangle mangled names (plt stubs)
            if op.startswith(("call", "j")):
                text = demangle_target(text)
            # memory operands
            for mm in MEM_RE.finditer(ops):
                width, expr = mm.group(1), mm.group(2)
                parts = re.findall(r"([+-]?)\s*([0-9]\*[a-z0-9]+|[a-z0-9]+\*[0-9]|0x[0-9a-f]+|[a-z][a-z0-9]*|[0-9]+)", expr)
                base_regs, disp, index = [], 0, None
                for sign, p in parts:
                    if p.startswith("0x") or p.isdigit():
                        disp += (-1 if sign == "-" else 1) * int(p, 0)
                    elif "*" in p:
                        index = p
                    else:
                        base_regs.append(p)
                target = None
                if "rip" in base_regs and ripaddr is not None:
                    target = ripaddr
                elif len(base_regs) == 1 and canon(base_regs[0]) in known and index is None:
                    target = known[canon(base_regs[0])] + disp
                elif len(base_regs) == 1 and canon(base_regs[0]) in known and index:
                    tbl = known[canon(base_regs[0])] + disp
                    notes.append("table %s" % self.describe(tbl, None, True))
                if target is not None:
                    notes.append(self.describe(target, width, op == "lea"))
                elif (op.startswith("call") or op.startswith("jmp")) and len(base_regs) == 1 and not index \
                        and canon(base_regs[0]) not in known and canon(base_regs[0]) not in ("rsp", "rbp"):
                    notes.append("vcall? slot +0x%x (index %d)" % (disp, disp // e.ws))
            # jump tables
            if op == "jmp" and re.fullmatch(r"[re][a-z]{2}|r\d+", ops):
                jt = self.jump_table(insns, idx, known, fstart, fend)
                if jt:
                    extra.extend(jt)
            # register tracking
            dest = None
            if op not in ("call", "ret") and not op.startswith(NO_DEST) and not op.startswith("j"):
                first = ops.split(",")[0].strip()
                if canon(first):
                    dest = canon(first)
            if op == "call":
                tm = re.search(r"get_pc_thunk\.(\w\w)", ops)
                for r_ in ("rax", "rcx", "rdx"):
                    known.pop(r_, None)
                if tm:
                    last_thunk = canon(tm.group(1))
                    known[last_thunk] = a + len_after(insns, idx)  # return address (pc)
            elif dest:
                newval = None
                srcs = ops.split(",", 1)[1].strip() if "," in ops else ""
                if op == "add" and dest == last_thunk and dest in known and re.fullmatch(r"0x[0-9a-f]+", srcs):
                    newval = known[dest] + int(srcs, 16)
                    if newval & 0xFFFFFFFF == e.got:
                        newval = e.got
                        got_regs.add(dest)
                    last_thunk = None
                elif op == "lea":
                    mm = MEM_RE.search(srcs)
                    if mm:
                        expr = mm.group(2)
                        if "rip" in expr and ripaddr is not None:
                            newval = ripaddr
                        else:
                            ps = re.fullmatch(r"([a-z0-9]+)\s*([+-])\s*(0x[0-9a-f]+)", expr.strip())
                            if ps and canon(ps.group(1)) in known:
                                newval = known[canon(ps.group(1))] + (int(ps.group(3), 16) * (1 if ps.group(2) == "+" else -1))
                            elif canon(expr.strip()) in known:
                                newval = known[canon(expr.strip())]
                elif op == "mov" and canon(srcs) in known:
                    newval = known[canon(srcs)]
                if newval is not None:
                    known[dest] = newval
                elif dest in got_regs and dest == "rbx" and not e.is64:
                    pass  # gcc's PIC register: epilogue restores do not end its use on other paths
                else:
                    known.pop(dest, None)
                    got_regs.discard(dest)
            out.append((a, text, notes, extra))
        return out

    def jump_table(self, insns, idx, known, fstart, fend):
        """Decode `mov r,[T+4*i]; add r,base; jmp r` (ia-32 GOT-relative) or the amd-64
        `movsxd r,[T+4*i]; add r,T; jmp r` table; the bound comes from the last `cmp i, N` before."""
        e = self.elf
        tbl = None
        for j in range(idx - 1, max(idx - 6, -1), -1):
            t = insns[j][1]
            m = re.match(r"(mov|movsxd) \w+, dword ptr \[(\w+) \+ 4\*(\w+)(?: ([+-]) (0x[0-9a-f]+))?\]", t)
            if m:
                b = canon(m.group(2))
                if b in self._known_at(insns, j, known):
                    tbl = self._known_at(insns, j, known)[b]
                    if m.group(5):
                        tbl += int(m.group(5), 16) * (1 if m.group(4) == "+" else -1)
                break
        if tbl is None:
            return None
        bound = None
        for j in range(idx - 1, max(idx - 25, -1), -1):
            m = re.match(r"cmp \w+(?: ptr \[[^\]]+\])?, (0x[0-9a-f]+|\d+)$", insns[j][1])
            if m:
                bound = int(m.group(1), 0) + 1
                break
        base = tbl if e.is64 else e.got
        lines = []
        n = bound or 512
        for k in range(n):
            b = e.read(tbl + 4 * k, 4)
            if not b:
                break
            tgt = (base + struct.unpack("<i", b)[0]) & ((1 << 64) - 1 if e.is64 else 0xFFFFFFFF)
            if not (fstart <= tgt < fend):
                if bound is None:
                    break
            lines.append("      ;   case %d -> 0x%x" % (k, tgt))
        if not lines:
            return None
        return ["      ; jump table at 0x%x, %d entries%s" % (tbl, len(lines), "" if bound else " (bound not found)")] + lines

    def _known_at(self, insns, j, known):
        return self.snap[j] if j < len(self.snap) else known


def len_after(insns, idx):
    if idx + 1 < len(insns):
        return insns[idx + 1][0] - insns[idx][0]
    return 5


def f80(b):
    mant, se = struct.unpack("<QH", b)
    sign = -1.0 if se & 0x8000 else 1.0
    ex = se & 0x7FFF
    if ex == 0 and mant == 0:
        return 0.0 * sign
    if ex == 0x7FFF:
        return sign * float("inf") if (mant << 1) & ((1 << 64) - 1) == 0 else float("nan")
    return sign * (mant / float(1 << 63)) * 2.0 ** (ex - 16383)


# ---------------------------------------------------------------------------------------------
# Function lookup and listing

class Image:
    def __init__(self, image, binary=None):
        self.image = image
        if not binary and not BIN_DIR:
            raise SystemExit("no binary: pass --binary or set EVIDENCE_BIN_DIR or BF2_LINUX_SERVER_DIR")
        self.binary = binary or os.path.join(BIN_DIR, image, "libaidll.so")
        if not os.path.exists(self.binary):
            raise SystemExit("binary not found: %s" % self.binary)
        self.elf = Elf(self.binary)
        self.ann = Annotator(self.elf)
        self._all = None

    def func_range(self, addr, size=None):
        fs = self.elf.funcs
        if not size:
            for a, sz, n in fs:
                if a == addr and sz:
                    size = sz
                    break
        if not size:
            i = bisect.bisect_right(self.elf.sym_addrs, addr)
            nxt = [x for x in self.elf.sym_addrs[i:i + 50] if x > addr]
            size = (nxt[0] - addr) if nxt else 0x100
        return addr, addr + size

    def lookup(self, arg):
        """[(addr, size, mangled)] matching an address, exact mangled name or demangled substring."""
        if re.fullmatch(r"0x[0-9a-fA-F]+", arg):
            a = int(arg, 16)
            hits = [f for f in self.elf.funcs if f[0] == a]
            if not hits:
                hits = [f for f in self.elf.funcs if f[0] < a < f[0] + f[1]]
            if not hits:
                r = self.elf.sym_at(a)
                hits = [(a, 0, r[0] if r else "?")]
            return hits[:1]
        exact = [f for f in self.elf.funcs if f[2] == arg]
        if exact:
            return exact
        dm = self.elf.dem
        return [f for f in self.elf.funcs if arg in dm.get(f[2], f[2])]

    def all_insns(self, att=False):
        key = "att" if att else "intel"
        if self._all is None:
            self._all = {}
        if key not in self._all:
            ins = disassemble(self.elf, att=att)
            self._all[key] = (ins, [x[0] for x in ins])
        return self._all[key]

    def insns(self, start, end, att=False, whole=False):
        if whole:
            ins, addrs = self.all_insns(att)
            i, j = bisect.bisect_left(addrs, start), bisect.bisect_left(addrs, end)
            return ins[i:j]
        return disassemble(self.elf, start, end, att)

    def listing(self, addr, size=None, att=False, whole=False, header_extra=""):
        start, end = self.func_range(addr, size)
        intel = self.insns(start, end, False, whole)
        rows = self.ann.annotate(intel, start, end)
        text_by_addr = None
        if att:
            text_by_addr = {}
            for a, t in self.insns(start, end, True, whole):
                t = re.sub(r"\s*# 0x[0-9a-f]+(?: <[^>]*>)?$", "", t)
                text_by_addr[a] = demangle_target(t) if t.startswith(("call", "j")) else t
        r = self.elf.sym_at(start)
        nm = self.elf.dem.get(r[0], r[0]) if r and r[1] == 0 else ("?" if not r else self.elf.name(start))
        lines = ["; %s" % nm,
                 "; %s ELF 0x%x..0x%x (%d bytes)  Ghidra 0x%x  %s syntax%s" % (
                     self.image, start, end, end - start, start + GHIDRA_BASE[self.image],
                     "AT&T" if att else "Intel", header_extra)]
        for a, t, notes, extra in rows:
            if att:
                t = text_by_addr.get(a, t)
            if t in ("nop",) or re.fullmatch(r"(lea|nop) .*\[[re]?[sd]i \+ [re]iz.*\]|nop.*|xchg ax, ax", t):
                continue
            # drop objdump's own `<fn+0x..>` labels on near branches inside the same function
            t = re.sub(r" <(?:[^>]*\+0x[0-9a-f]+|\.L\w+)>$", "", t) if t.startswith("j") else t
            lines.append("%x: %s%s" % (a, t, ("   ; " + " | ".join(notes)) if notes else ""))
            lines.extend(extra)
        return "\n".join(lines) + "\n"


# ---------------------------------------------------------------------------------------------
# Vtables

def vtable_dump(img, va, size, name):
    """Word-by-word dump. A typeinfo word starts a (primary or secondary) vtable group; the slots
    after it are numbered from that group's address point (the value a vptr holds)."""
    e = img.elf
    ws = e.ws
    lines = ["# %s  %s ELF 0x%x size 0x%x (%d words)" % (name, img.image, va, size, size // ws),
             "# offset  vptr slot          value       target"]
    point = None
    for k in range(size // ws):
        a = va + k * ws
        raw = e.word(a)
        r = e.rel.get(a)
        tgt = e.slot_target(a)
        if r is None:
            sv = raw - (1 << (8 * ws)) if raw is not None and raw >> (8 * ws - 1) else raw
            lines.append("+0x%03x  %-17s %-11d %s" % (k * ws, "", sv if sv is not None else 0,
                                                      "offset-to-top / vcall offset" if point is None or sv else "null"))
            continue
        if tgt and tgt.startswith("typeinfo for"):
            lines.append("+0x%03x  %-17s %-11s %s" % (k * ws, "", "", tgt))
            point = a + ws
            continue
        so = a - point if point is not None else None
        slot = "vptr+0x%02x [%d]" % (so, so // ws) if so is not None else ""
        if r[1]:
            val = e.addr_of.get(r[1])
            val = "0x%x" % (val + (r[2] or 0) + (raw if r[2] is None else 0)) if val else "extern"
        else:
            val = "0x%x" % (r[2] if r[2] is not None else raw)
        lines.append("+0x%03x  %-17s %-11s %s" % (k * ws, slot, val, tgt))
    return "\n".join(lines) + "\n"


def vtables(img):
    res = []
    for a, sz, n, t, sh in img.elf.syms:
        if n.startswith("_ZTV") and sz:
            res.append((a, sz, n, demangle([n])[n]))
    return sorted(set(res))


def class_file(dname):
    c = dname[len("vtable for "):] if dname.startswith("vtable for ") else dname
    if c.startswith("dice::hfe::"):
        c = c[len("dice::hfe::"):]
    c = c.replace("::", ".")
    return re.sub(r"[^A-Za-z0-9_.]+", "_", c).strip("_")[:150]


# ---------------------------------------------------------------------------------------------
# Ghidra project handling and headless decompile

def ghidra_env():
    home = os.path.join(OUT, "ghidra", "home")
    tmp = os.path.join(OUT, "ghidra", "tmp")
    os.makedirs(home, exist_ok=True)
    os.makedirs(tmp, exist_ok=True)
    env = dict(os.environ)
    env["JAVA_TOOL_OPTIONS"] = (env.get("JAVA_TOOL_OPTIONS", "") + " -Duser.home=%s -Djava.io.tmpdir=%s" % (home, tmp)).strip()
    return env


def headless():
    if not GHIDRA:
        raise SystemExit("Ghidra not found: set GHIDRA_INSTALL_DIR")
    p = os.path.join(GHIDRA, "support", "analyzeHeadless")
    if not os.path.exists(p):
        raise SystemExit("Ghidra not found: %s (set GHIDRA_INSTALL_DIR)" % p)
    return p


def project_dir(image):
    return os.path.join(OUT, "ghidra", image)


def project_has_program(gpr, program):
    rep = gpr[:-4] + ".rep"
    idx = os.path.join(rep, "idata", "~index.dat")
    try:
        return any(":%s:" % program in l for l in open(idx, errors="replace"))
    except OSError:
        return False


def ensure_project(img, src=None, create=False, quiet=False):
    """Path of the private .gpr for img (see module doc for the lookup order)."""
    d = project_dir(img.image)
    gpr = os.path.join(d, "libaidll.gpr")
    prog = os.path.basename(img.binary)
    if os.path.exists(gpr) and project_has_program(gpr, prog) and not create:
        return gpr
    os.makedirs(d, exist_ok=True)
    if not create:
        cands = [src] if src else []
        root = os.environ.get("GHIDRA_PROJECTS_DIR")
        if not src and root:
            for dp, dn, fn in os.walk(root):
                dn[:] = [x for x in dn if not x.endswith(".rep") and x not in ("download", ".git")]
                cands += [os.path.join(dp, f) for f in fn if f.endswith(".gpr")]
        for c in cands:
            if project_has_program(c, prog) and program_matches_arch(c, img):
                if not quiet:
                    print("copying Ghidra project %s -> %s" % (c, gpr), file=sys.stderr)
                copy_project(c, gpr)
                return gpr
    if not quiet:
        print("importing %s into %s (full auto-analysis, several minutes)" % (img.binary, gpr), file=sys.stderr)
    for x in (gpr, gpr[:-4] + ".rep"):
        if os.path.isdir(x):
            shutil.rmtree(x)
        elif os.path.exists(x):
            os.remove(x)
    log = os.path.join(d, "import.log")
    with open(log, "w") as lf:
        subprocess.run([headless(), d, "libaidll", "-import", img.binary, "-overwrite",
                        "-scriptPath", os.path.join(HERE, "ghidra"), "-postScript", "ImportInfo.java"],
                       stdout=lf, stderr=subprocess.STDOUT, env=ghidra_env(), check=True)
    if not project_has_program(gpr, prog):
        raise SystemExit("import failed, see %s" % log)
    return gpr


def program_matches_arch(gpr, img):
    """True when a program in the project was imported from this exact binary (Ghidra keeps the
    executable's MD5 near the start of the program database)."""
    import hashlib
    md5 = hashlib.md5(img.elf.data).hexdigest().encode()
    for dp, dn, fn in os.walk(gpr[:-4] + ".rep"):
        for f in fn:
            if f.endswith(".gbf"):
                try:
                    with open(os.path.join(dp, f), "rb") as fh:
                        if md5 in fh.read(8 << 20):
                            return True
                except OSError:
                    pass
    return False


def copy_project(src_gpr, dst_gpr):
    src_rep, dst_rep = src_gpr[:-4] + ".rep", dst_gpr[:-4] + ".rep"
    if os.path.exists(dst_rep):
        shutil.rmtree(dst_rep)
    shutil.copytree(src_rep, dst_rep, ignore=shutil.ignore_patterns("*.lock", "*.lock~"))
    shutil.copyfile(src_gpr, dst_gpr)


def run_decompile(img, gpr, items, outdir, timeout=180, suffix=".c", log=None):
    """items: [(elf_addr, stem)]. One headless session; returns the analyzeHeadless exit code."""
    os.makedirs(outdir, exist_ok=True)
    fd, lst = tempfile.mkstemp(prefix="declist", suffix=".txt", dir=os.path.join(OUT, "ghidra", "tmp")
                               if os.path.isdir(os.path.join(OUT, "ghidra", "tmp")) else None)
    with os.fdopen(fd, "w") as f:
        for a, stem in items:
            f.write("0x%x\t%s\n" % (a, stem))
    d, name = os.path.dirname(gpr), os.path.basename(gpr)[:-4]
    cmd = [headless(), d, name, "-process", os.path.basename(img.binary), "-noanalysis", "-readOnly",
           "-scriptPath", os.path.join(HERE, "ghidra"), "-postScript", "DecompList.java", lst, outdir,
           str(timeout), suffix]
    with open(log or os.devnull, "a") as lf:
        rc = subprocess.run(cmd, stdout=lf, stderr=subprocess.STDOUT, env=ghidra_env()).returncode
    os.remove(lst)
    return rc


# ---------------------------------------------------------------------------------------------
# calls / strip-checkpointer (ported from the recovery agents' calls.py / cp2.py)

def cmd_calls(path):
    args, reg, st = {}, {}, []
    skip = re.compile(r"basic_string|Debug|operator<<|get_pc_thunk|_M_destroy|_Unwind_Resume")
    for line in open(path):
        m = re.match(r"([0-9a-f]+): (\S+) ?(.*?)(   ; (.*))?$", line.rstrip())
        if not m:
            continue
        a, op, ops, _, note = m.groups()
        ops = ops.strip()
        sm = re.search(r"\[esp(?: \+ (0x[0-9a-f]+))?\]", ops)
        if sm and op not in ("call", "lea") and ops.startswith(("dword ptr [esp", "qword ptr [esp", "byte ptr [esp",
                                                                 "word ptr [esp", "tbyte ptr [esp")):
            off = int(sm.group(1), 16) if sm.group(1) else 0
            src = ops.split(",", 1)[1].strip() if "," in ops else ""
            if op.startswith("fst"):
                v = "%s st0=%s" % (op, st[-1] if st else "?")
                if op.endswith("p") and st:
                    st.pop()
            else:
                v = "%s %s" % (op, "%s=%s" % (src, reg[src]) if src in reg else src)
            args[off] = v + (" [%s]" % note if note else "")
            continue
        if op == "call":
            tm = re.search(r"<([^>]*)>", ops)
            tgt = tm.group(1).replace("@plt", "") if tm else ops
            if note:
                tgt += " ; " + note
            if not skip.search(tgt):
                print("%s: CALL %s" % (a, tgt))
                for k in sorted(args):
                    print("      +%#x: %s" % (k, args[k]))
            args = {}
            for r in ("eax", "ecx", "edx"):
                reg.pop(r, None)
            reg["eax"] = "ret@" + a
            continue
        parts = [p.strip() for p in ops.split(",", 1)]
        if op.startswith("fld") or op.startswith("fild"):
            st.append(note or ops)
            continue
        if op.startswith("fstp"):
            if st:
                st.pop()
            continue
        if len(parts) == 2 and canon(parts[0]):
            d, s = parts
            if op == "mov":
                reg[d] = reg.get(s, s) + (" [%s]" % note if note else "")
            elif op == "xor" and d == s:
                reg[d] = "0"
            elif op in ("movzx", "movsx") or op.startswith("set"):
                reg[d] = "(%s %s)" % (op, s)
            elif op == "lea":
                reg[d] = "&" + s + (" [%s]" % note if note else "")
            elif op not in ("cmp", "test") and d in reg:
                reg[d] = "%s(%s,%s)" % (op, reg[d], s)
        elif len(parts) == 1 and op.startswith("set") and canon(parts[0]):
            reg[parts[0]] = "(%s)" % op


def cmd_strip_checkpointer(path):
    L = open(path).read().split("\n")
    out, i = [], 0
    while i < len(L):
        l = L[i]
        if l.strip().startswith("if") and re.search(r"== \([^)]*\)0x0\) \{$", l.rstrip()) \
                and any("checkPointer.h" in x for x in L[i + 1:i + 4]):
            d, j = 0, i
            while j < len(L):
                d += L[j].count("{") - L[j].count("}")
                j += 1
                if d == 0:
                    break
            v = re.search(r"if \(\(?([\w]+)", l)
            out.append(l.split("if")[0] + "/* checkPointer(%s) */" % (v.group(1) if v else "?"))
            i = j
            continue
        out.append(l)
        i += 1
    sys.stdout.write("\n".join(out))


# ---------------------------------------------------------------------------------------------
# Cache build

def is_static_init(sym):
    return sym.startswith(("__static_initialization_and_destruction", "__tcf_", "_GLOBAL__I_", "_GLOBAL__D_",
                           "global constructors keyed to", "global destructors keyed to"))


def stem(a):
    return "0x%X" % a


def load_rows(statuses):
    rows = []
    with open(LEDGER, newline="") as f:
        for r in csv.DictReader(f):
            if r["status"] in statuses:
                rows.append(r)
    return rows


def read_status(outdir):
    st = {}
    p = os.path.join(outdir, "decompile-status.tsv")
    if os.path.exists(p):
        for line in open(p):
            parts = line.rstrip("\n").split("\t")
            if len(parts) >= 2:
                st[parts[0]] = parts[1] if parts[1] != "fail" else "fail: " + (parts[3] if len(parts) > 3 else "")
    return st


def decompile_parallel(img, items, suffix, jobs, batch, timeout, logdir):
    """Shard items over `jobs` project copies, `batch` functions per headless session."""
    gpr = ensure_project(img)
    # items come sorted by size, largest first: deal them round-robin so that every session gets
    # its share of the big functions (one session with all of them would run for hours)
    nb = max(1, -(-len(items) // batch))
    batches = [items[k::nb] for k in range(nb)]
    copies = []
    for j in range(min(jobs, len(batches))):
        jd = os.path.join(project_dir(img.image), "job%d" % j)
        jg = os.path.join(jd, "libaidll.gpr")
        if not project_has_program(jg, os.path.basename(img.binary)) or \
                os.path.getmtime(jg[:-4] + ".rep") < os.path.getmtime(gpr[:-4] + ".rep"):
            os.makedirs(jd, exist_ok=True)
            copy_project(gpr, jg)
        copies.append(jg)
    pending = list(enumerate(batches))
    running = {}
    lists = {}
    while pending or running:
        for j, jg in enumerate(copies):
            if j not in running and pending:
                bi, b = pending.pop(0)
                fd, lst = tempfile.mkstemp(prefix="b%03d_" % bi, suffix=".txt", dir=os.path.join(OUT, "ghidra", "tmp"))
                with os.fdopen(fd, "w") as f:
                    for a, s in b:
                        f.write("0x%x\t%s\n" % (a, s))
                cmd = [headless(), os.path.dirname(jg), "libaidll", "-process", os.path.basename(img.binary),
                       "-noanalysis", "-readOnly", "-scriptPath", os.path.join(HERE, "ghidra"),
                       "-postScript", "DecompList.java", lst, OUT, str(timeout), suffix]
                lf = open(os.path.join(logdir, "%s-batch%03d.log" % (img.image, bi)), "w")
                running[j] = (subprocess.Popen(cmd, stdout=lf, stderr=subprocess.STDOUT, env=ghidra_env()), lf, bi, len(b))
                lists[j] = lst
                print("  %s batch %d/%d (%d functions) on job%d" % (img.image, bi + 1, len(batches), len(b), j),
                      file=sys.stderr)
        time.sleep(2)
        for j in list(running):
            p, lf, bi, n = running[j]
            if p.poll() is not None:
                lf.close()
                os.remove(lists[j])
                if p.returncode:
                    print("  %s batch %d exited %d" % (img.image, bi + 1, p.returncode), file=sys.stderr)
                del running[j]


def is_static_mangled(n):
    return "__static_initialization_and_destruction" in n or n.startswith(("__tcf_", "_GLOBAL__I_", "_GLOBAL__D_"))


def norm_demangled(d):
    """Demangled name with the integer spellings that differ between ILP32 and LP64 merged."""
    d = re.sub(r"\bunsigned (long long|long|int)\b", "U", d)
    d = re.sub(r"\b(long long|long)\b", "S", d)
    return d.replace(" ", "")


def map_amd64(ia, am):
    """ia-32 function address -> ((amd64 addr, size), method). Methods:
    exact      the same mangled name, unique in both images;
    local      a name repeated in both images (local statics: __tcf_N, __static_initialization_...):
               the same-named function between the amd-64 images of the nearest uniquely mapped
               neighbours, by position;
    normalized the demangled names agree once unsigned int/long/long long (size_t, uint64) are merged;
               unique in both images."""
    from collections import defaultdict
    ia_by, am_by = defaultdict(list), defaultdict(list)
    for a, sz, n in ia.elf.funcs:
        ia_by[n].append((a, sz))
    for a, sz, n in am.elf.funcs:
        am_by[n].append((a, sz))
    res = {}
    for n, lst in ia_by.items():
        if len(lst) == 1 and len(am_by.get(n, ())) == 1:
            res[lst[0][0]] = (am_by[n][0], "exact")
    names_ia = [n for n in ia_by if not any(x[0] in res for x in ia_by[n])]
    names_am = list(am_by)
    def split(n):
        for pre in ("_GLOBAL__I_", "_GLOBAL__D_"):
            if n.startswith(pre):
                return pre, n[len(pre):]
        return "", n
    dm = demangle([split(n)[1] for n in names_ia + names_am])
    def nkey(n):
        pre, core = split(n)
        v = re.search(r"(C[123]|D[012])E", core)  # constructor/destructor variants demangle alike
        return pre + norm_demangled(dm[core]) + "|" + (v.group(1) if v else "")
    nam, nia = defaultdict(list), defaultdict(list)
    for n in names_am:
        nam[nkey(n)].extend(am_by[n])
    for n in names_ia:
        nia[nkey(n)].extend(ia_by[n])
    for k, lst in nia.items():
        if len(lst) == 1 and len(nam.get(k, ())) == 1:
            res[lst[0][0]] = (nam[k][0], "normalized")
    # local static-init names: match inside the TU's static block, located through its _GLOBAL__I_
    ia_f, am_f = ia.elf.funcs, am.elf.funcs
    def block(funcs, i):
        lo = hi = i
        while lo > 0 and is_static_mangled(funcs[lo - 1][2]):
            lo -= 1
        while hi + 1 < len(funcs) and is_static_mangled(funcs[hi + 1][2]):
            hi += 1
        return funcs[lo:hi + 1]
    am_idx = {a: i for i, (a, sz, n) in enumerate(am_f)}
    for i, (a, sz, n) in enumerate(ia_f):
        if a in res or not is_static_mangled(n) or len(ia_by[n]) < 2:
            continue
        keys = [f for f in block(ia_f, i) if f[2].startswith("_GLOBAL__") and f[0] in res]
        if not keys:
            continue
        ka = res[keys[0][0]][0][0]
        cand = [f for f in block(am_f, am_idx[ka]) if f[2] == n]
        if len(cand) == 1:
            res[a] = ((cand[0][0], cand[0][1]), "local")
    return res


def cmd_build(a):
    t0 = time.time()
    images = a.images.split(",")
    os.makedirs(OUT, exist_ok=True)
    logdir = os.path.join(OUT, "logs")
    os.makedirs(logdir, exist_ok=True)
    os.makedirs(os.path.join(OUT, "ghidra", "tmp"), exist_ok=True)
    rows = load_rows(CACHE_STATUSES)
    if a.only:
        want = {int(x, 16) for x in a.only.split(",")}
        rows = [r for r in rows if int(r["address"], 16) in want]
    ia = Image("ia-32", a.binary)
    am = Image("amd-64", a.binary64) if "amd-64" in images else None
    amd_map = map_amd64(ia, am) if am else {}
    print("%d ledger rows (%s)" % (len(rows), ",".join(CACHE_STATUSES)), file=sys.stderr)
    entries = []
    for r in rows:
        ad = int(r["address"], 16)
        sym = r["symbol"]
        kind = "static-init" if any(is_static_init(s.strip()) for s in sym.split(" | ")) else "function"
        amd, how = amd_map.get(ad, (None, ""))
        entries.append(dict(r=r, addr=ad, size=int(r["size"]), kind=kind, amd=amd, how=how))
    # listings
    print("listings ...", file=sys.stderr)
    for e in entries:
        p = os.path.join(OUT, stem(e["addr"]) + ".s")
        if a.force or not os.path.exists(p):
            hdr = "  ledger: %s %s" % (e["r"]["status"], e["r"]["file"])
            open(p, "w").write(ia.listing(e["addr"], e["size"] or None, whole=True, header_extra=hdr))
        if am and e["amd"]:
            p = os.path.join(OUT, stem(e["addr"]) + ".amd64.s")
            if a.force or not os.path.exists(p):
                hdr = "  (%s match of ia-32 0x%x; ia-32 stays canonical)" % (e["how"], e["addr"])
                open(p, "w").write(am.listing(e["amd"][0], e["amd"][1] or None, whole=True, header_extra=hdr))
    t1 = time.time()
    print("listings done in %.0f s" % (t1 - t0), file=sys.stderr)
    # decompiles
    if not a.no_decompile:
        for img, suffix, sel in ((ia, ".c", lambda e: e["addr"]), (am, ".amd64.c", lambda e: e["amd"] and e["amd"][0])):
            if img is None:
                continue
            done = read_status(OUT)
            items = [(sel(e), stem(e["addr"])) for e in entries if sel(e)
                     and (a.force or stem(e["addr"]) + suffix not in done)]
            size_of = {stem(e["addr"]): e["size"] for e in entries}
            items.sort(key=lambda x: -size_of.get(x[1], 0))
            if items:
                print("decompiling %d %s functions" % (len(items), img.image), file=sys.stderr)
                decompile_parallel(img, items, suffix, a.jobs, a.batch, a.timeout, logdir)
    t2 = time.time()
    # vtables
    vd = os.path.join(OUT, "vtables")
    os.makedirs(vd, exist_ok=True)
    seen = {}
    with open(os.path.join(vd, "index.csv"), "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["class", "file", "vtable_address", "size", "mangled"])
        for va, sz, mn, dn in vtables(ia):
            cf = class_file(dn)
            if cf in seen:
                cf = "%s_%x" % (cf, va)
            seen[cf] = 1
            open(os.path.join(vd, cf + ".txt"), "w").write(vtable_dump(ia, va, sz, dn))
            w.writerow([dn[len("vtable for "):], "vtables/%s.txt" % cf, "0x%x" % va, sz, mn])
    # index
    done = read_status(OUT)
    fails = 0
    with open(os.path.join(OUT, "index.csv"), "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["address", "symbol", "size", "file", "status", "kind", "amd64_address", "amd64_size", "amd64_match",
                    "listing", "decompile", "decompile_status", "amd64_listing", "amd64_decompile",
                    "amd64_decompile_status"])
        for e in entries:
            s = stem(e["addr"])
            def ds(suffix):
                if not os.path.exists(os.path.join(OUT, s + suffix)):
                    return "", "missing" if not a.no_decompile else "not-run"
                return s + suffix, done.get(s + suffix, "missing")
            c, cs = ds(".c")
            if e["amd"]:
                c6, cs6 = ds(".amd64.c")
                l6 = s + ".amd64.s"
            else:
                c6, cs6, l6 = "", "no-amd64-symbol", ""
            if cs != "ok":
                fails += 1
            r = e["r"]
            w.writerow([r["address"], r["symbol"], r["size"], r["file"], r["status"], e["kind"],
                        "0x%x" % e["amd"][0] if e["amd"] else "", e["amd"][1] if e["amd"] else "", e["how"],
                        s + ".s", c, cs, l6, c6, cs6])
    print("index: %d rows, %d without an ok ia-32 decompile, %d with an amd-64 symbol; "
          "listings %.0f s, decompiles %.0f s, total %.0f s" % (
              len(entries), fails, sum(1 for e in entries if e["amd"]), t1 - t0, t2 - t1, time.time() - t0),
          file=sys.stderr)


# ---------------------------------------------------------------------------------------------

def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--image", default="ia-32", choices=("ia-32", "amd-64"))
    ap.add_argument("--binary", help="ia-32 (or --image) binary path")
    sub = ap.add_subparsers(dest="cmd", required=True)
    p = sub.add_parser("fn")
    p.add_argument("what")
    p.add_argument("--att", action="store_true")
    p.add_argument("--decompile", action="store_true")
    p = sub.add_parser("decompile")
    p.add_argument("addrs", nargs="+")
    p.add_argument("--out")
    p = sub.add_parser("vtable")
    p.add_argument("cls")
    p = sub.add_parser("calls")
    p.add_argument("listing")
    p = sub.add_parser("strip-checkpointer")
    p.add_argument("file")
    p = sub.add_parser("project")
    p.add_argument("--from", dest="src")
    p.add_argument("--create", action="store_true")
    p = sub.add_parser("build")
    p.add_argument("--jobs", type=int, default=3)
    p.add_argument("--batch", type=int, default=250)
    p.add_argument("--timeout", type=int, default=240, help="per-function decompile timeout, seconds")
    p.add_argument("--images", default="ia-32,amd-64")
    p.add_argument("--binary64")
    p.add_argument("--no-decompile", action="store_true")
    p.add_argument("--only", help="comma-separated ia-32 addresses")
    p.add_argument("--force", action="store_true", help="regenerate existing files")
    a = ap.parse_args()

    if a.cmd == "calls":
        return cmd_calls(a.listing)
    if a.cmd == "strip-checkpointer":
        return cmd_strip_checkpointer(a.file)
    if a.cmd == "build":
        return cmd_build(a)
    img = Image(a.image, a.binary)
    if a.cmd == "project":
        print(ensure_project(img, a.src, a.create))
    elif a.cmd == "fn":
        hits = img.lookup(a.what)
        if len(hits) != 1:
            for ad, sz, n in hits[:60]:
                print("0x%x %5d %s" % (ad, sz, img.elf.dem.get(n, n)))
            if len(hits) > 60:
                print("... %d more" % (len(hits) - 60))
            if not hits:
                print("no match", file=sys.stderr)
            return 1 if not hits else 0
        ad, sz, n = hits[0]
        sys.stdout.write(img.listing(ad, sz or None, att=a.att))
        s = stem(ad) if a.image == "ia-32" else None
        if s is None:
            # amd-64 address: map back to the ia-32 cache stem through the mangled name
            ia = Image("ia-32")
            m = [f for f in ia.elf.funcs if f[2] == n]
            s = stem(m[0][0]) if m else None
        paths = []
        for suf in (".s", ".c", ".amd64.s", ".amd64.c"):
            if s and os.path.exists(os.path.join(OUT, s + suf)):
                paths.append(os.path.relpath(os.path.join(OUT, s + suf), os.getcwd()))
        print("; cache: %s" % (" ".join(paths) if paths else "none"))
        if a.decompile and not any(p.endswith(".c") and (".amd64" in p) == (a.image == "amd-64") for p in paths):
            outdir = os.path.join(OUT, "adhoc")
            run_decompile(img, ensure_project(img), [(ad, stem(ad) + ("" if a.image == "ia-32" else ".amd64"))],
                          outdir, suffix=".c", log=os.path.join(outdir, "decompile.log"))
            print("; decompile: %s" % os.path.relpath(os.path.join(outdir, stem(ad) + ("" if a.image == "ia-32" else ".amd64") + ".c")))
    elif a.cmd == "decompile":
        outdir = a.out or os.path.join(OUT, "adhoc")
        items = [(int(x, 16), stem(int(x, 16)) + ("" if a.image == "ia-32" else ".amd64")) for x in a.addrs]
        rc = run_decompile(img, ensure_project(img), items, outdir, log=os.path.join(outdir, "decompile.log"))
        for _, s in items:
            print(os.path.join(outdir, s + ".c"))
        return rc
    elif a.cmd == "vtable":
        hits = [v for v in vtables(img) if a.cls in v[3]]
        exact = [v for v in hits if v[3] == "vtable for " + a.cls or v[3].endswith("::" + a.cls)]
        for va, sz, mn, dn in (exact or hits)[:20]:
            sys.stdout.write(vtable_dump(img, va, sz, dn))
        if not hits:
            print("no vtable matches", file=sys.stderr)
            return 1
    return 0


if __name__ == "__main__":
    sys.exit(main() or 0)
