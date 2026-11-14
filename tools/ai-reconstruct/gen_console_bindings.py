#!/usr/bin/env python3
"""Generate the AI image's console bindings (ConsoleClass<line>) from libaidll.so.

    python3 tools/ai-reconstruct/gen_console_bindings.py [--binary PATH] [--engine PATH] [--verify-slots BUILD_DIR]
                                                          [--content MOD_DIR] [--check]

Python 3 standard library; shells out to objdump (llvm-objdump AT&T output) and c++filt. Writes
Code/BF2/AI/AIDLL/AI/Main/AIModuleConsole.inc (included by AIModule.cpp) and
docs/ai-reconstruct/console-bindings.json (per class: verdict, reason, ELF function and teardown
addresses, plus the retail-script coverage table; build_ledger.py marks the emitted rows done).

What it reads (all ELF addresses):
  - AIModule.cpp's static initializer (the __static_initialization_and_destruction_0 that stores the
    consoleObject<line> instances): a symbolic walk of the construction path records every store into
    each instance (+0 vtable, +4 m_type, +0xC function name, +0x18/+0x1C min/max, +0x4C return type,
    +0x50.. argument types, +0xA1 result flag), the SetConsoleClassDefaults in force (object name,
    access, category) and the __cxa_atexit teardown that follows each construction.
  - each class's isObjectActive (the receiver: a GOT-loaded global or an AIModule.cpp getter),
    executeObjectMethod emulated once per argument count (m_hasReturnValue store, target call,
    result store), setArgFromString emulated per argument index (the extractor called).
  - virtual targets: the slot is resolved through the image's own typeinfo (base offsets) and
    vtable groups of the receiver type's implementers; a slot past the interface's vtable is the
    implementer's own virtual and is called through a static_cast to it.
  - inline field accessors: a load/store at receiver+offset, named through the tree header's
    layout comment. The 17 manually inspected inline executors below additionally require an exact
    address, size and SHA-256 of their instructions, matching field declarations and base offsets.
    Their accepted arities, argument copies, flags, stores and discarded results are explicit.
  - IBFTimer's two pause slots: the engine bf2's BFTimer primary vtable and single-base typeinfo.
It never guesses: a class is left out (listed in the .inc and on stdout) when its target or an
argument extractor has no body in the tree (ledger todo/absent), when an inline body has no named
member, or when a slot does not resolve in either its image or the verified engine timer table.

--verify-slots compiles a probe of AIModule.cpp with the bf2_aidll flags and clang's
-fdump-vtable-layouts and checks every virtual target's native slot and the whole primary vtable of
each static type against the image. --content scans the retail mod's .ai/.con scripts (and zips)
for commands on the AI image's objects and reports the unbound ones.

Hand verification against the listings (objdump -d, relocations resolved through the GOT at
0x4B30C4), two or more bodies per kind:
  void method, fixed arity:     206 addSAIStrategy (0x41CEC0), 298 setVehicleBehaviour (0x431D90)
  void method, default arg:     161 createHeightMap (0x4102D0: argc 0 passes 4), 196
                                botSetWaypoints_Patrol (0x418570: argc 5 passes a zero Vec3)
  overloads per arity:          300 addInterpreterEntry, 389 addCondition (emulated arms)
  returning method:             186 addBot int (0x413FC0: store +0xA4), 571 search bool
                                (0x485900: store +0xA2), 190 killBotId std::string (assign)
  property, direct calls:       364 aggression float (0x446120), 204 saiMapXDimension (0x41C0F0)
  property, virtual:            174 aiSystemQuotient (0x4113D0: AIMain slots 0x58/0x5C), 290
                                overrideMenuSettings (0x42F160: slots 0xBC / 0x15C via AISettings)
  void virtual:                 171 writeBotPlans (0x410D80: IBotManager slot 0x7C)
  inline field property:        426 layer (0x45C800, +0x190), 442 vehicleSearchRadius (0x462480, +0x18C)
  argument extraction:          206 setArgFromString (0x41C9F0: static, istringstream, >>, no fail
                                check), 269 materialId (0x427C20: ai::operator>>(istream&, unsigned char&))
  getReturnValueAsString:       161 (constant empty, 0x40FF30), 211 bool (ostringstream, 0x41D6B0)
"""

import argparse
import collections
import hashlib
import json
import os
import re
import subprocess
import sys

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
# Inputs come from the Linux 1.5 server installation in $BF2_LINUX_SERVER_DIR, or from the options.
_SERVER_DIR = os.environ.get("BF2_LINUX_SERVER_DIR")
DEFAULT_BINARY = os.path.join(_SERVER_DIR, "bin", "ia-32", "libaidll.so") if _SERVER_DIR else None
ENGINE_BINARY = os.path.join(_SERVER_DIR, "bin", "ia-32", "bf2") if _SERVER_DIR else None
_ENGINE_ELF = None

# ----------------------------------------------------------------------------------------------
# ELF access through objdump (llvm-objdump AT&T output, as shipped with Xcode)
# ----------------------------------------------------------------------------------------------


def run(cmd, inp=None):
    return subprocess.run(cmd, input=inp, check=True, capture_output=True, text=True).stdout


_DEMANGLED = {}


def demangle(names):
    if not names:
        return []
    missing = list(dict.fromkeys(n for n in names if n not in _DEMANGLED))
    if missing:
        out = run(["c++filt", "-n"], "\n".join(missing) + "\n").splitlines()
        if len(out) != len(missing):
            sys.exit("c++filt returned %d lines for %d names" % (len(out), len(missing)))
        _DEMANGLED.update(zip(missing, (o.strip() for o in out)))
    return [_DEMANGLED[n] for n in names]


class Elf:
    SYM_RE = re.compile(r"^([0-9a-f]{8}) (.{7}) (\S+)\t([0-9a-f]{8}) (?:\.hidden )?(.+)$")
    SEC_RE = re.compile(r"^\s*\d+\s+(\S+)\s+([0-9a-f]+)\s+([0-9a-f]+)")

    def __init__(self, path):
        self.path = path
        self.data = open(path, "rb").read()
        self.sections = {}
        # file offsets: llvm-objdump -h does not print them, read the section headers ourselves
        self._read_shdrs()
        self.syms = []
        for line in run(["objdump", "-t", path]).splitlines():
            m = self.SYM_RE.match(line)
            if m:
                a, fl, sec, size, name = m.groups()
                self.syms.append({"addr": int(a, 16), "size": int(size, 16), "flags": fl,
                                  "sec": sec, "mangled": name.strip()})
        for s, d in zip(self.syms, demangle([s["mangled"] for s in self.syms])):
            s["dem"] = d
        self.by_addr = collections.defaultdict(list)
        self.by_mangled = {}
        for s in self.syms:
            self.by_addr[s["addr"]].append(s)
            self.by_mangled[s["mangled"]] = s
        self.rel = {}           # address -> (type, mangled symbol or None)
        for line in run(["objdump", "-R", path]).splitlines():
            m = re.match(r"^([0-9a-f]{8}) (R_386_\w+)\s+(.*)$", line)
            if m:
                self.rel[int(m.group(1), 16)] = (m.group(2), m.group(3).strip())
        rel_names = sorted({v[1] for v in self.rel.values() if v[1] and v[1] != "*ABS*"})
        self.rel_dem = dict(zip(rel_names, demangle(rel_names)))
        self.got = self.sections[".got.plt"][0]

    def _read_shdrs(self):
        import struct
        d = self.data
        shoff, = struct.unpack_from("<I", d, 0x20)
        shentsize, shnum, shstrndx = struct.unpack_from("<HHH", d, 0x2E)
        hdrs = [struct.unpack_from("<IIIIIIIIII", d, shoff + i * shentsize) for i in range(shnum)]
        stroff = hdrs[shstrndx][4]
        for h in hdrs:
            name = d[stroff + h[0]:d.index(b"\0", stroff + h[0])].decode()
            self.sections[name] = (h[3], h[4], h[5], h[1])   # addr, offset, size, type

    def offset(self, addr):
        for name, (a, off, size, typ) in self.sections.items():
            if a and a <= addr < a + size and typ != 8:      # not NOBITS
                return off + addr - a
        return None

    def u32(self, addr):
        off = self.offset(addr)
        return None if off is None else int.from_bytes(self.data[off:off + 4], "little")

    def cstr(self, addr):
        off = self.offset(addr)
        if off is None:
            return None
        end = self.data.index(b"\0", off)
        return self.data[off:end].decode("latin-1")

    def sym_at(self, addr):
        return self.by_addr.get(addr, [])

    def func_syms(self):
        return [s for s in self.syms if "F" in s["flags"] and s["sec"] == ".text"]

    def word_symbol(self, addr):
        """The symbol a relocated data word refers to (vtable slots, GOT entries)."""
        r = self.rel.get(addr)
        if r and r[1] and r[1] != "*ABS*":
            return r[1]
        val = self.u32(addr)
        if val is None:
            return None
        cands = [s for s in self.sym_at(val) if "F" in s["flags"]]
        return cands[0]["mangled"] if cands else ("0x%x" % val)


INSN_RE = re.compile(r"^\s+([0-9a-f]+):\s+(\S+)\s*(.*?)\s*(?:#.*)?$")
LABEL_RE = re.compile(r"^([0-9a-f]+) <(.+)>:$")


class Insn:
    __slots__ = ("addr", "mn", "ops", "target", "tsym", "text")

    def __init__(self, addr, mn, rest):
        self.addr, self.mn, self.text = addr, mn, rest
        self.target, self.tsym = None, None
        m = re.match(r"^0x([0-9a-f]+)(?: <([^>]+)>)?$", rest)
        if mn.startswith("j") or mn.startswith("call"):
            if m:
                self.target = int(m.group(1), 16)
                self.tsym = m.group(2)
                rest = ""
        self.ops = split_ops(rest) if rest else []

    def __repr__(self):
        return "%x %s %s" % (self.addr, self.mn, self.text)


def split_ops(s):
    out, depth, cur = [], 0, ""
    for c in s:
        if c == "(":
            depth += 1
        elif c == ")":
            depth -= 1
        if c == "," and depth == 0:
            out.append(cur.strip())
            cur = ""
        else:
            cur += c
    if cur.strip():
        out.append(cur.strip())
    return out


def disasm(elf, start, stop):
    """{function mangled name: (start address, [Insn])} for [start, stop)."""
    out = run(["objdump", "-d", "--no-show-raw-insn", "--start-address=0x%x" % start,
               "--stop-address=0x%x" % stop, elf.path])
    funcs, cur = collections.OrderedDict(), None
    for line in out.splitlines():
        m = LABEL_RE.match(line)
        if m:
            cur = m.group(2)
            funcs[cur] = (int(m.group(1), 16), [])
            continue
        m = INSN_RE.match(line)
        if m and cur:
            if m.group(2) == "nop":
                continue
            funcs[cur][1].append(Insn(int(m.group(1), 16), m.group(2), m.group(3)))
    return funcs


# ----------------------------------------------------------------------------------------------
# Static initializer: the generated constructors, inlined into __static_initialization_and_destruction_0
# ----------------------------------------------------------------------------------------------

REG32 = {"%eax", "%ebx", "%ecx", "%edx", "%esi", "%edi", "%ebp", "%esp"}
REG8 = {"%al": "%eax", "%cl": "%ecx", "%dl": "%edx", "%bl": "%ebx"}
MEM_RE = re.compile(r"^(-?0x[0-9a-f]+|-?\d+)?\((%e\w\w)(?:,(%e\w\w),(\d))?\)$")


def parse_mem(op):
    m = MEM_RE.match(op)
    if not m:
        return None
    disp = int(m.group(1), 0) if m.group(1) else 0
    return disp, m.group(2), m.group(3)


def imm(op):
    if op.startswith("$"):
        return int(op[1:], 0) & 0xFFFFFFFF
    return None


class StaticInitWalker:
    """Follows the construction path (__initialize_p == 1, __priority == 0xffff) through the static
    initializer and records every store into the GOT-relative data area. Values are symbolic:
      ("imm", n)  ("addr", a)  ("got", a)  ("deref", v)  ("add", v, n)  ("ret", callee)."""

    def __init__(self, elf, insns):
        self.elf, self.insns = elf, insns
        self.index = {i.addr: k for k, i in enumerate(insns)}
        self.stores = []          # (insn addr, data address, value, width)
        self.calls = []           # (insn addr, callee, [stack args])
        self.seq = []             # execution order: ("s", store index) / ("c", call index)

    def walk(self):
        regs = {}
        stack = {}
        got = self.elf.got
        k = 0
        steps = 0
        last_cmp = None
        visited = set()
        while k < len(self.insns):
            steps += 1
            if steps > 400000:
                raise RuntimeError("static init walk does not terminate")
            i = self.insns[k]
            mn, ops = i.mn, i.ops
            nxt = k + 1
            if mn == "calll" and i.target is not None:
                if i.tsym and "get_pc_thunk.bx" in i.tsym:
                    regs["%ebx"] = ("gotbase",)
                else:
                    args = [stack.get(o) for o in range(0, 0x20, 4)]
                    self.seq.append(("c", len(self.calls)))
                    self.calls.append((i.addr, (i.tsym or "").replace("@plt", ""), args))
                    for r in ("%eax", "%ecx", "%edx"):
                        regs.pop(r, None)
                    regs["%eax"] = ("ret", i.tsym)
            elif mn in ("movl", "movb", "movzbl", "leal") and len(ops) == 2:
                src, dst = ops
                val = None
                if imm(src) is not None:
                    val = ("imm", imm(src))
                elif src in REG32:
                    val = regs.get(src)
                elif src in REG8:
                    val = regs.get(REG8[src])
                else:
                    pm = parse_mem(src)
                    if pm:
                        disp, base, idx = pm
                        if mn == "leal":
                            if base == "%ebx" and not idx:
                                val = ("addr", got + disp)
                        elif base == "%ebx" and not idx:
                            val = ("got", got + disp)
                        elif base in ("%esp", "%ebp"):
                            val = stack.get((base, disp)) if base == "%ebp" else stack.get(disp)
                        elif not idx and base in regs and disp == 0:
                            val = ("deref", regs[base])
                        elif not idx and base in regs:
                            val = ("deref", ("add", regs[base], disp))
                if dst in REG32:
                    if val is None:
                        regs.pop(dst, None)
                    else:
                        regs[dst] = val
                else:
                    pm = parse_mem(dst)
                    if pm:
                        disp, base, idx = pm
                        if base == "%ebx" and not idx:
                            self.seq.append(("s", len(self.stores)))
                            self.stores.append((i.addr, got + disp, val, 1 if mn == "movb" else 4))
                        elif base == "%esp":
                            stack[disp] = val
                        elif base == "%ebp":
                            stack[("%ebp", disp)] = val
            elif mn == "xorl" and len(ops) == 2 and ops[0] == ops[1]:
                regs[ops[1]] = ("imm", 0)
            elif mn == "addl" and len(ops) == 2 and imm(ops[0]) is not None and ops[1] in REG32:
                v = regs.get(ops[1])
                if ops[1] == "%ebx":
                    pass
                elif v is not None:
                    regs[ops[1]] = ("add", v, imm(ops[0]))
            elif mn == "cmpl":
                last_cmp = ops
            elif mn.startswith("j"):
                taken = None
                if mn == "jmp":
                    taken = True
                elif last_cmp and len(last_cmp) == 2 and last_cmp[1] in ("0xc(%ebp)", "0x8(%ebp)"):
                    want = {"0xc(%ebp)": 0xFFFF, "0x8(%ebp)": 1}[last_cmp[1]]
                    eq = imm(last_cmp[0]) == want
                    taken = (mn == "je" and eq) or (mn == "jne" and not eq)
                else:
                    taken = False            # refcount/compare branches: the fall-through path
                last_cmp = None if mn != "jmp" else last_cmp
                if taken:
                    if i.target not in self.index:
                        break                # left the function (the return path)
                    key = (i.addr, i.target)
                    if key in visited and mn != "jmp":
                        break
                    visited.add(key)
                    nxt = self.index[i.target]
            elif mn in ("retl", "ret"):
                break
            elif mn in ("sete", "setne", "decl", "incl", "lock", "xaddl", "testl", "subl", "andl",
                        "orl", "shll", "sarl", "shrl", "imull", "negl", "notl"):
                if ops and ops[-1] in REG32 and mn not in ("testl", "lock", "subl"):
                    regs.pop(ops[-1], None)
                if mn == "sete" or mn == "setne":
                    regs.pop("%eax", None)
            else:
                if ops and ops[-1] in REG32:
                    regs.pop(ops[-1], None)
            k = nxt
        return self


def resolve_value(elf, v):
    """Turn a symbolic value into something printable/comparable."""
    if v is None:
        return None
    t = v[0]
    if t == "imm":
        return v[1]
    if t == "addr":
        return ("addr", v[1])
    if t == "got":
        return ("gotslot", elf.word_symbol(v[1]))
    if t == "deref" and v[1][0] == "got":
        return ("var", elf.word_symbol(v[1][1]))
    if t == "add" and v[1][0] == "got":
        return ("symoff", elf.word_symbol(v[1][1]), v[2])
    return ("?", v)


CC_RE = re.compile(r"ConsoleClass(\d+)")
FIELD = {0x04: "type", 0x08: "objectName", 0x0C: "functionName", 0x10: "access", 0x14: "category",
         0x18: "minArgs", 0x1C: "maxArgs", 0x4C: "returnType", 0xA0: "hasReturnValue",
         0xA1: "isDynamic"}


def extract_registrations(elf):
    """Every ConsoleClass instance the static initializer constructs, in construction order, with
    the defaults (SetConsoleClassDefaults) in force and the constructor's stores decoded."""
    si = [s for s in elf.syms if s["mangled"] == "_Z41__static_initialization_and_destruction_0ii"
          and any(o["mangled"].startswith("_ZN4dice3hfe2ai58_GLOBAL__N__ZN4dice3hfe2ai28getActive")
                  for o in elf.syms if o["addr"] == s["addr"] + s["size"] or True)]
    # The AIModule TU's initializer is the one that stores ConsoleClass vtables.
    objects = {}
    for s in elf.syms:
        m = re.match(r"^dice::hfe::ai::\(anonymous namespace\)::consoleObject(\d+)$", s["dem"])
        if m:
            objects[s["addr"]] = int(m.group(1))
    defaults = {}
    for s in elf.syms:
        m = re.match(r"^dice::hfe::ai::\(anonymous namespace\)::(\w+)fullAccessSetConsoleClassDefaults$",
                     s["dem"])
        if m:
            defaults[s["addr"]] = m.group(1)
    target = None
    for s in si:
        body = disasm(elf, s["addr"], s["addr"] + s["size"])
        insns = list(body.values())[0][1]
        w = StaticInitWalker(elf, insns).walk()
        if any(st[1] in objects for st in w.stores):
            target = (s, w)
            break
    if not target:
        sys.exit("no static initializer constructs the consoleObject instances")
    s, w = target
    regs = collections.OrderedDict()
    current = None
    last = None
    for kind, n in w.seq:
        if kind == "c":
            addr, callee, args = w.calls[n]
            if callee.startswith("_ZN4dice3hfe23SetConsoleClassDefaultsC1"):
                obj = args[0][1] if args[0] and args[0][0] == "addr" else None
                current = {"object": defaults.get(obj),
                           "name": elf.cstr(args[1][1]) if args[1] and args[1][0] == "addr" else None,
                           "access": resolve_value(elf, args[2]),
                           "category": resolve_value(elf, args[3])}
            elif callee == "__cxa_atexit" and args[0] and args[0][0] == "addr" and last is not None:
                # __tcf_N takes no argument: it follows the construction of its object
                regs[last].setdefault("atexit", args[0])
                last = None
            continue
        addr, daddr, val, width = w.stores[n]
        base = max((o for o in objects if o <= daddr < o + 0xC0), default=None)
        if base is None:
            continue
        r = regs.setdefault(base, {"line": objects[base], "object": base, "stores": {},
                                   "argTypes": {}, "needsConv": {}, "defaults": current})
        last = base
        off = daddr - base
        rv = resolve_value(elf, val)
        if off == 0:
            r["vtable"] = rv
        elif off in FIELD:
            r["stores"][FIELD[off]] = rv
        elif 0x50 <= off < 0x78 and (off - 0x50) % 4 == 0:
            r["argTypes"][(off - 0x50) // 4] = rv
        elif 0x78 <= off < 0xA0:
            r["needsConv"][(off - 0x78) // 4] = rv
        else:
            r["stores"]["+0x%X" % off] = rv
    return list(regs.values())


def decode_registration(elf, r):
    st = r["stores"]
    def s_(v):
        return elf.cstr(v[1]) if isinstance(v, tuple) and v[0] == "addr" else v
    d = {"line": r["line"], "object": "0x%x" % r["object"],
         "vtable": r.get("vtable"),
         "functionName": s_(st.get("functionName")),
         "type": st.get("type"), "minArgs": st.get("minArgs"), "maxArgs": st.get("maxArgs"),
         "returnType": s_(st.get("returnType")),
         "isDynamic": st.get("isDynamic"), "hasReturnValue": st.get("hasReturnValue"),
         "objectNameSrc": st.get("objectName"), "access": st.get("access"),
         "category": st.get("category"),
         "argTypes": [s_(r["argTypes"][k]) for k in sorted(r["argTypes"])],
         "defaults": r["defaults"],
         "extra": {k: v for k, v in st.items() if k.startswith("+")}}
    return d


# ----------------------------------------------------------------------------------------------
# Generated member functions: executor emulation per argument count
# ----------------------------------------------------------------------------------------------

# Calls inside executors that are argument plumbing, not the bound target.
PLUMBING_RE = re.compile(r"^(_ZNSs|_ZNKSs|_ZdlPv|_Znwj|__cxa_|_Unwind_|__i686\.|_ZNSt|_ZNKSt|_ZSt|"
                         r"_ZN4dice3hfe4Vec[234]C|_ZN4dice3hfe4Vec[234]aS|memcpy|memmove|strlen)")


class ExecPath:
    def __init__(self):
        self.calls = []          # dicts: kind direct|virtual, sym, slot, base, args(stack values)
        self.stores = []         # (offset from this, value, width/kind)
        self.memstores = []      # (base value, offset, value, kind): stores through other pointers
        self.returned = None


def emulate_executor(elf, insns, argc, params=None):
    """Run the executor with m_argCount == argc. Values: ("this",), ("argp", k), ("argv", k, off),
    ("recv", how), ("vptr", base), ("ret", n), ("got", sym), ("imm", n), ("rodata", addr)."""
    index = {i.addr: k for k, i in enumerate(insns)}
    regs, stack, frame = {}, {}, {}
    flags = None
    fsrc = None
    path = ExecPath()
    k, steps = 0, 0
    got = elf.got

    def val(op):
        if imm(op) is not None:
            return ("imm", imm(op))
        if op in REG32:
            return regs.get(op)
        pm = parse_mem(op)
        if not pm:
            return None
        disp, base, idx = pm
        if idx:
            return None
        if base == "%ebp":
            if disp == 8:
                return ("this",)
            if params and disp in params:
                return params[disp]
            return frame.get(disp)
        if base == "%esp":
            return stack.get(disp)
        if base in ("%ebx", "%ecx") and regs.get(base) == ("gotbase",):
            return ("gotslot", got + disp)
        b = regs.get(base)
        if b is None:
            return None
        if b == ("this",):
            if disp == 0x20:
                return ("imm", argc)
            if 0x24 <= disp < 0x4C:
                return ("argp", (disp - 0x24) // 4)
            return ("field", disp)
        if b[0] == "argp":
            return ("argv", b[1], disp)
        if b[0] == "gotslot":
            sym = elf.word_symbol(b[1])
            return ("global", sym, disp)
        if b[0] in ("global", "ret") and disp == 0:
            return ("vptr", b)
        if b[0] == "vptr":
            return ("slot", b[1], disp)
        return ("mem", b, disp)

    def setflags(v):
        return v

    while k < len(insns):
        steps += 1
        if steps > 20000:
            raise RuntimeError("executor emulation does not terminate")
        i = insns[k]
        mn, ops = i.mn, i.ops
        nxt = k + 1
        if mn == "calll":
            if i.target is not None and i.tsym and "get_pc_thunk" in i.tsym:
                reg = "%ebx" if i.tsym.endswith(".bx") else "%ecx"
                regs[reg] = ("gotbase",)
            else:
                args = [stack.get(o) for o in range(0, 0x30, 4)]
                if i.target is not None:
                    sym = (i.tsym or "").replace("@plt", "")
                    c = {"kind": "direct", "sym": sym, "args": args, "addr": i.addr}
                else:
                    c = {"kind": "indirect", "op": ops[0] if ops else i.text, "args": args,
                         "addr": i.addr}
                    t = ops[0].lstrip("*") if ops else ""
                    v = val(t)
                    if v and v[0] == "slot":
                        c.update({"kind": "virtual", "slot": v[2], "base": v[1]})
                    elif v and v[0] == "global":     # call through a GOT-loaded pointer
                        c.update({"kind": "fnptr", "base": v})
                path.calls.append(c)
                n = len(path.calls) - 1
                for r in ("%eax", "%ecx", "%edx"):
                    regs.pop(r, None)
                regs["%eax"] = ("ret", n)
        elif mn in ("movl", "movb", "movw", "movzbl", "movsbl", "movzwl", "leal") and len(ops) == 2:
            src, dst = ops
            if mn == "leal":
                pm = parse_mem(src)
                v = None
                if pm and not pm[2]:
                    disp, base, _ = pm
                    b = regs.get(base) if base != "%ebp" else None
                    if base in ("%ebx", "%ecx") and b == ("gotbase",):
                        v = ("gotaddr", got + disp)
                    elif base == "%ebp":
                        v = ("frameaddr", disp)
                    elif base == "%esp":
                        v = ("stackaddr", disp)
                    elif b == ("this",):
                        v = ("fieldaddr", disp)
                    elif b is not None and disp == 0:
                        v = b
            else:
                v = val(src)
            if dst in REG32:
                if v is None:
                    regs.pop(dst, None)
                else:
                    regs[dst] = v
            else:
                pm = parse_mem(dst)
                if pm and not pm[2]:
                    disp, base, _ = pm
                    if base == "%esp":
                        stack[disp] = v
                    elif base == "%ebp":
                        frame[disp] = v
                    elif regs.get(base) == ("this",):
                        path.stores.append((disp, v, mn))
                    elif regs.get(base) is not None:
                        path.memstores.append((regs.get(base), disp, v, mn))
        elif mn in ("flds", "fldl", "fildl", "fld1", "fldz"):
            fsrc = val(ops[0]) if ops else None
        elif mn in ("fstps", "fstpl", "fsts", "fstl"):
            pm = parse_mem(ops[0]) if ops else None
            if pm and not pm[2]:
                disp, base, _ = pm
                if base == "%esp":
                    stack[disp] = ("float",)
                elif base == "%ebp":
                    frame[disp] = ("float",)
                elif regs.get(base) == ("this",):
                    path.stores.append((disp, ("float", fsrc), mn))
                elif regs.get(base) is not None:
                    path.memstores.append((regs.get(base), disp, ("float",), mn))
        elif mn == "xorl" and len(ops) == 2 and ops[0] == ops[1]:
            regs[ops[1]] = ("imm", 0)
            flags = 0
        elif mn in ("cmpl", "cmpb"):
            a, b = val(ops[1]), val(ops[0])
            if a and b and a[0] == "imm" and b[0] == "imm":
                flags = a[1] - b[1]
            else:
                flags = None
        elif mn in ("testl", "testb"):
            a = val(ops[1])
            if ops[0] == ops[1] and a and a[0] == "imm":
                flags = a[1]
            else:
                flags = None
        elif mn in ("decl", "incl", "subl", "addl"):
            r = ops[-1]
            a = regs.get(r) if r in REG32 else None
            if mn in ("subl", "addl") and r == "%esp":
                pass
            elif mn == "addl" and r in ("%ebx", "%ecx") and regs.get(r) == ("gotbase",):
                pass
            elif a and a[0] == "imm":
                d = {"decl": -1, "incl": 1}.get(mn)
                if d is None:
                    s = imm(ops[0])
                    d = None if s is None else (s if mn == "addl" else -s)
                if d is None:
                    regs.pop(r, None)
                    flags = None
                else:
                    regs[r] = ("imm", (a[1] + d) & 0xFFFFFFFF)
                    flags = regs[r][1] - (1 << 32 if regs[r][1] & 0x80000000 else 0)
            elif mn == "addl" and a is not None and imm(ops[0]) is not None:
                regs[r] = ("mem", a, imm(ops[0]))     # pointer adjustment
                flags = None
            else:
                if r in REG32:
                    regs.pop(r, None)
                flags = None
        elif mn.startswith("j"):
            if mn == "jmp":
                if i.target is None or i.target not in index:
                    break
                nxt = index[i.target]
            else:
                taken = False
                if flags is not None:
                    f = flags
                    taken = {"je": f == 0, "jne": f != 0, "jg": f > 0, "jge": f >= 0, "jl": f < 0,
                             "jle": f <= 0, "ja": f > 0, "jb": f < 0, "jae": f >= 0,
                             "jbe": f <= 0, "js": f < 0, "jns": f >= 0}.get(mn, False)
                if taken and i.target in index:
                    nxt = index[i.target]
        elif mn in ("retl", "ret"):
            path.returned = regs.get("%eax")
            break
        elif mn in ("sete", "setne", "movzbl"):
            regs.pop("%eax", None)
        elif mn == "lock":
            pass
        else:
            if ops and ops[-1] in REG32 and ops[-1] not in ("%esp", "%ebp"):
                regs.pop(ops[-1], None)
                flags = None
        k = nxt
    return path


# ----------------------------------------------------------------------------------------------
# Virtual slots: resolved through the image's own typeinfo and vtable groups
# ----------------------------------------------------------------------------------------------

class Vtables:
    def __init__(self, elf):
        self.elf = elf
        self.ti = {}          # mangled class name (no _ZTI) -> (kind, [(base, offset)])
        for s in elf.syms:
            m = s["mangled"]
            if not m.startswith("_ZTI") or s["size"] < 8:
                continue
            kind = elf.word_symbol(s["addr"]) or ""
            bases = []
            if "__si_class_type_info" in kind:
                b = elf.word_symbol(s["addr"] + 8)
                if b:
                    bases.append((b[4:], 0))
            elif "__vmi_class_type_info" in kind:
                n = elf.u32(s["addr"] + 12) or 0
                for j in range(n):
                    b = elf.word_symbol(s["addr"] + 16 + 8 * j)
                    of = elf.u32(s["addr"] + 20 + 8 * j) or 0
                    if of & 0x80000000:
                        of -= 1 << 32
                    if b:
                        bases.append((b[4:], of >> 8))
            self.ti[m[4:]] = (kind, bases)
        self.vt = {s["mangled"][4:]: s for s in elf.syms if s["mangled"].startswith("_ZTV")}

    def base_offsets(self, cls, target, off=0, seen=None):
        """Offsets of every `target` subobject inside `cls`."""
        if cls == target:
            return [off]
        out = []
        for b, o in self.ti.get(cls, ("", []))[1]:
            out += self.base_offsets(b, target, off + o)
        return out

    def implementers(self, target):
        return sorted(c for c in self.ti if c != target and c in self.vt and self.base_offsets(c, target))

    def slot(self, cls, target, slot_off):
        """(mangled function or None) at byte `slot_off` of the `target`-in-`cls` vtable."""
        offs = self.base_offsets(cls, target)
        if len(offs) != 1:
            return None
        sub = offs[0]
        v = self.vt[cls]
        a, end = v["addr"], v["addr"] + v["size"]
        while a < end:
            top = self.elf.u32(a)
            if top is not None and top & 0x80000000:
                top -= 1 << 32
            ti = self.elf.word_symbol(a + 4)
            if ti == "_ZTI" + cls and top == -sub:
                w = a + 8 + slot_off
                return self.elf.word_symbol(w) if w < end else None
            a += 4
        return None

    def resolve(self, target, slot_off):
        """Every implementer's function at the slot; the target's own vtable first when present."""
        res = []
        for c in [target] + self.implementers(target):
            if c not in self.vt:
                continue
            f = self.slot(c, target, slot_off)
            if f:
                res.append((c, f))
        return res


# ----------------------------------------------------------------------------------------------
# Receivers: the object expression of each binding (isObjectActive is `expr != 0` for all 298)
# ----------------------------------------------------------------------------------------------

# binary symbol (GOT-loaded global or called getter) -> (C++ expression, ELF class for vtables,
# tree type). Every entry's type is the tree's declaration of that global / getter, checked by hand;
# the getters are the AIModule.cpp helpers at ELF 0x3EE940..0x3EEB40.
RECEIVERS = {
    "_ZN4dice3hfe2ai15AerialHeightMap10m_instanceE": ("AerialHeightMap::m_instance", "N4dice3hfe2ai15AerialHeightMapE"),
    "_ZN4dice3hfe2ai9AIConsole10m_instanceE": ("AIConsole::m_instance", "N4dice3hfe2ai9AIConsoleE"),
    "_ZN4dice3hfe2ai11IBotManager8instanceE": ("IBotManager::instance", "N4dice3hfe2ai11IBotManagerE"),
    "_ZN4dice3hfe2ai15getActiveAIMainEv": ("getActiveAIMain()", "N4dice3hfe2ai6AIMainE"),
    "_ZN4dice3hfe2ai19BotPlacementManager10m_instanceE": ("BotPlacementManager::m_instance", "N4dice3hfe2ai19BotPlacementManagerE"),
    "_ZN4dice3hfe2ai21CommonSingletonServer10m_iBFTimerE": ("static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer)", "N4dice3hfe2ai8IBFTimerE"),
    "_ZN4dice3hfe2ai21CommonSingletonServer21m_iaiSettingsModifierE": ("static_cast<IAISettingsModifier*>(CommonSingletonServer::m_iaiSettingsModifier)", "N4dice3hfe2ai19IAISettingsModifierE"),
    "_ZN4dice3hfe2ai21CommonSingletonServer13m_iaiSettingsE": ("static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)", "N4dice3hfe2ai11IAISettingsE"),
    "_ZN4dice3hfe2ai13getAISettingsEv": ("getAISettings()", "N4dice3hfe2ai11IAISettingsE"),
    "_ZN4dice3hfe2ai12AILODManager10m_instanceE": ("AILODManager::m_instance", "N4dice3hfe2ai12AILODManagerE"),
    "_ZN4dice3hfe2ai19BotBehaviourManager10m_instanceE": ("BotBehaviourManager::m_instance", "N4dice3hfe2ai19BotBehaviourManagerE"),
    "_ZN4dice3hfe2ai19IBotManagerModifier8instanceE": ("IBotManagerModifier::instance", "N4dice3hfe2ai19IBotManagerModifierE"),
    "_ZN4dice3hfe2ai15StrategyManager11getInstanceEv": ("StrategyManager::getInstance()", "N4dice3hfe2ai15StrategyManagerE"),
    "_ZN4dice3hfe2ai17getActiveStrategyEv": ("getActiveStrategy()", "N4dice3hfe2ai8StrategyE"),
    "_ZN4dice3hfe2ai18getActiveConditionEv": ("getActiveCondition()", "N4dice3hfe2ai17StrategyConditionE"),
    "_ZN4dice3hfe2ai24AIStrategicObjectManager11getInstanceEv": ("AIStrategicObjectManager::getInstance()", "N4dice3hfe2ai24AIStrategicObjectManagerE"),
    "_ZN4dice3hfe2ai22getActiveStrategicAreaEv": ("getActiveStrategicArea()", "N4dice3hfe2ai15AIStrategicAreaE"),
    "_ZN4dice3hfe2ai20AILandingZoneManager10m_instanceE": ("AILandingZoneManager::m_instance", "N4dice3hfe2ai20AILandingZoneManagerE"),
    "_ZN4dice3hfe2ai20getActiveLandingZoneEv": ("getActiveLandingZone()", "N4dice3hfe2ai13AILandingZoneE"),
    "_ZN4dice3hfe2ai19IAIPathfindingSetup8instanceE": ("IAIPathfindingSetup::instance", "N4dice3hfe2ai19IAIPathfindingSetupE"),
    "_ZN4dice3hfe2ai21getActiveMapContainerEv": ("getActiveMapContainer()", "N4dice3hfe2ai19IAIPathfindingSetup13IMapContainerE"),
    "_ZN4dice3hfe2ai21CommonSingletonServer12m_iDPManagerE": ("static_cast<IDPManager*>(CommonSingletonServer::m_iDPManager)", "N4dice3hfe2ai10IDPManagerE"),
    "_ZN4dice3hfe2ai14IAIPathfinding8instanceE": ("IAIPathfinding::instance", "N4dice3hfe2ai14IAIPathfindingE"),
    "_ZN4dice3hfe2ai16UserOrderManager10m_instanceE": ("UserOrderManager::m_instance", "N4dice3hfe2ai16UserOrderManagerE"),
    "_ZN4dice3hfe2ai28getActivePatrolPathIfManagerEv": ("getActivePatrolPathIfManager()", "N4dice3hfe2ai10PatrolPathE"),
    "_ZN4dice3hfe2ai31getActiveGuardPositionIfManagerEv": ("getActiveGuardPositionIfManager()", "N4dice3hfe2ai13GuardPositionE"),
    "_ZN4dice3hfe2ai20CoverPositionManager10m_instanceE": ("CoverPositionManager::m_instance", "N4dice3hfe2ai20CoverPositionManagerE"),
    "_ZN4dice3hfe2ai8AIRandom10m_instanceE": ("AIRandom::m_instance", "N4dice3hfe2ai8AIRandomE"),
}


METHODS = ("isObjectActive", "executeObjectMethod", "setArgFromString", "getReturnValueAsString",
           "objectHasRange", "getObjectRange", "getObjectRangeAsString", "checkObjectRange")
EXTRACT_RE = re.compile(r"(_ZNSirs|_ZStrs|_ZN4dice3hfe\w*rs)")


def method_tables(elf):
    """{line: {method: (address, size, mangled)}} for every ConsoleClass function."""
    out = collections.defaultdict(dict)
    for s in elf.func_syms():
        m = re.search(r"ConsoleClass(\d+)::(~?\w+)\(", s["dem"])
        if not m:
            continue
        meth = m.group(2)
        if meth.startswith("~"):
            meth = "D0" if s["mangled"].endswith("D0Ev") else "D1"
        out[int(m.group(1))][meth] = (s["addr"], s["size"], s["mangled"])
    return out


def stores_return(p):
    """The arm writes m_returnValue (+0xA2 for one-byte types, +0xA4 otherwise): a direct store, a
    string assign / copy into the member, or the member passed as a call's sret/destination."""
    if any(off >= 0xA2 for off, v, mn in p.stores):
        return True
    for c in p.calls:
        if any(a in (("fieldaddr", 0xA2), ("fieldaddr", 0xA4)) for a in c["args"][:2] if a):
            return True
    return False


def field_access(p, rcalls, recv, argc):
    """An inline accessor: argc 0 reads receiver+off into m_returnValue, argc 1 stores the parsed
    argument at receiver+off. Returns a target record of kind 'field' or None."""
    def on_receiver(v):
        if not v:
            return False
        if v[0] == "ret":
            return any(c is p.calls[v[1]] for c in rcalls)
        return v[0] == "global" and v[1] == recv
    if argc == 0:
        for off, v, mn in p.stores:
            if off < 0xA2 or not v:
                continue
            src = v[1] if v[0] == "float" and len(v) > 1 else v
            if src and src[0] == "mem" and on_receiver(src[1]):
                return {"kind": "field", "offset": src[2], "access": "get", "addr": None}
    elif argc == 1:
        for base, off, v, mn in p.memstores:
            if on_receiver(base) and v and (v[0] in ("argv", "float")):
                return {"kind": "field", "offset": off, "access": "set", "addr": None}
    return None


def is_plumbing(c):
    return c["kind"] == "direct" and PLUMBING_RE.match(c["sym"])


# Each field record is (declaring class, ia-32 offset, type, member, array extent or None).
# The combined m_size storage is retained because other subsystems index its side radii.
_VISIBLE = ("AIStrategicArea", 404, "bool", "m_visible", None)
_STATUS = ("AIStrategicObject", 116, "StatusMap::Status", "m_status", 4)
_TAKEABLE = ("AIStrategicArea", 356, "bool", "m_sideEnabled", 2)
_NEIGHBOURS = ("AIStrategicArea", 358, "bool", "m_showNeighbours", None)

# ELF address, size, instruction hash, interpretation, fields, and per-argc
# (has-return-value, statements before the flag, statements after the flag).
# These are transcriptions of the complete executors, including their arity guards. In
# particular, discarded inline getters are not turned into returning console commands.
INLINE_EXECUTORS = {
    176: (0x411CE0, 53, "50ee9896d257782c5e7257fd95658e3e81f4ac94d8a2c5c0bf32714f793adbcb",
          "discarded quotient getter; evaluates only the receiver", {},
          {0: (False, [], ["(void){receiver};"])}),
    177: (0x411F00, 53, "7e255e179e69ad7d298869cbe25a0286fbd653d7afb4933c2cf452dd57d3b9bc",
          "discarded quotient getter; evaluates only the receiver", {},
          {0: (False, [], ["(void){receiver};"])}),
    178: (0x412550, 128, "175be289bae4a6aa273d1a5f7f19a6e59ca90beae4810f5c23df772cf1e77596",
          "statistic speed; ia-32 negates exactly when value < 0 (retains -0 and unordered values)",
          {"speed": ("AIMain", 40, "double", "m_botStatisticUpdateSpeed", None)},
          {0: (True, [], ["m_returnValue = {receiver}->{speed};"]),
           1: (False, ["const double value = {arg0};"],
               ["AIMain* const object = {receiver};",
                "object->{speed} = value < 0.0 ? -value : value;"])}),
    427: (0x45CC90, 72, "70f551d5a2a464056062eb3d30f35b51337d7b4d7bfb2370cd10780e89259fa0",
          "normalized visibility store", {"visible": _VISIBLE},
          {1: (False, ["const bool value = {arg0};"], ["{receiver}->{visible} = (value != 0);"])}),
    428: (0x45D0C0, 84, "b651ba868b5e8a4a4c9f21db25f2997ffc91cda64407b40c4c8b59c5f6cc272a",
          "normalized visibility load", {"visible": _VISIBLE},
          {0: (True, [], ["m_returnValue = ({receiver}->{visible} != 0);"])}),
    429: (0x45D730, 110, "2f6dd52dcb795778ba546bf6ac69e7ce452a75dd54e10d77370b8e8e94ed44bf",
          "vehicle-search-radius drawing switch",
          {"draw": ("AIStrategicObjectManager", 84, "bool", "m_drawVehicleSearchRadii", None)},
          {0: (True, [], ["m_returnValue = {receiver}->{draw};"]),
           1: (False, ["const bool value = {arg0};"], ["{receiver}->{draw} = value;"])}),
    440: (0x461740, 101, "663cb0d80b8c36e799770bf010c0786f0ce063d3ecdf206b2a7e209324926718",
          "unchecked status stores: saved pair first, current pair second, then return the status",
          {"status": _STATUS},
          {2: (True, ["const int side = {arg0};", "const StatusMap::Status value = {arg1};"],
               ["AIStrategicArea* const object = {receiver};",
                "object->{status}[side + 1] = value;", "object->{status}[side - 1] = value;",
                "m_returnValue = value;"])}),
    441: (0x461DF0, 113, "c9302556a9016ff9ef3f983087487a3d27d38739848818e3fc9a5ea77125d06c",
          "inherited category property",
          {"category": ("AIStrategicObject", 252, "int", "m_category", None)},
          {0: (True, [], ["m_returnValue = {receiver}->{category};"]),
           1: (False, ["const int value = {arg0};"], ["{receiver}->{category} = value;"])}),
    443: (0x462B50, 87, "554e07cdb7ab07a0e31f6757c6bea5d84f35b93a99ab002f575fff27af78eb70",
          "unchecked one-based takeability store", {"takeable": _TAKEABLE},
          {2: (False, ["const int side = {arg0};", "const bool value = {arg1};"],
               ["{receiver}->{takeable}[side - 1] = value;"])}),
    444: (0x4631C0, 89, "24c16e3fb313bfb355c63d2833e1b07caec13cf5afe31bae0dc58ca9ccbeebc6",
          "unchecked one-based takeability load", {"takeable": _TAKEABLE},
          {1: (True, ["const int side = {arg0};"], ["m_returnValue = {receiver}->{takeable}[side - 1];"])}),
    447: (0x463F40, 113, "6b0dd9a2da4b12dba0448967ac7d44293f3c5b2585830e2605d15f7554c7fb5a",
          "inherited baseline temperature, the first word of the shared temperature/radius storage",
          {"temperature": ("AIStrategicObject", 152, "float", "m_size", 3)},
          {0: (True, [], ["m_returnValue = {receiver}->{temperature}[AIStrategicObject::kBasicTemperature];"]),
           1: (False, ["const float value = {arg0};"],
               ["{receiver}->{temperature}[AIStrategicObject::kBasicTemperature] = value;"])}),
    458: (0x4695D0, 86, "ec8d8d36a7911c2cf3c366a44bd85fce4d0d76b85118585d2f1b2e3a0816a066",
          "store and return the debug switch",
          {"debug": ("AIStrategicObjectManager", 28, "bool", "m_debugEnabled", None)},
          {1: (True, ["const bool value = {arg0};"], ["{receiver}->{debug} = value;", "m_returnValue = value;"])}),
    462: (0x46AB10, 88, "8958ae349594e76793638adb2e9d9f859ec0e962174939d89ff0e3ef87b1f162",
          "unchecked one-based current-temperature load",
          {"temperature": ("AIStrategicObject", 164, "float", "m_f164", 4)},
          {1: (True, ["const int side = {arg0};"], ["m_returnValue = {receiver}->{temperature}[side - 1];"])}),
    463: (0x46B180, 85, "c6fa6594539d33f2f2122575113ec3af1e47d944f5d7fff11d0020c2cf7880ee",
          "unchecked one-based current-status load", {"status": _STATUS},
          {1: (True, ["const int side = {arg0};"], ["m_returnValue = {receiver}->{status}[side - 1];"])}),
    471: (0x46E5B0, 116, "1fc546d3121657ae43954c27fd1b20cb54073aaea216e4a940b07808a14868ac",
          "neighbour drawing switch", {"neighbours": _NEIGHBOURS},
          {0: (True, [], ["m_returnValue = {receiver}->{neighbours};"]),
           1: (False, ["const bool value = {arg0};"], ["{receiver}->{neighbours} = value;"])}),
    474: (0x46FB90, 158, "254b0202f7b7c2e75a68208e4b8e81ebd96d0996a992b2d3e630ec4b517be8a6",
          "name load; shipped defect: setter copies and destroys the argument without assigning it",
          {"name": ("AIStrategicArea", 308, "std::string", "m_name", None)},
          {0: (True, [], ["m_returnValue = {receiver}->{name};"]),
           1: (False, ["std::string value({arg0});"], ["(void){receiver};"])}),
    576: (0x485F70, 95, "109be1bd1cad3ae38f8a21fdbd072fc33ac77d8bb2b535222f379294136f4c06",
          "seed word at offset zero of a non-polymorphic receiver",
          {"seed": ("AIRandom", 0, "int", "m_seed", None)},
          {0: (True, [], ["m_returnValue = {receiver}->{seed};"]),
           1: (False, ["const u32 value = {arg0};"], ["{receiver}->{seed} = value;"])}),
}

INLINE_SIGNATURES = {
    # Function name, registration kind, result type, argument types, formatting flag.
    176: ("getBotManagerQuotient", 0, "double", (), 0),
    177: ("getSAIQuotient", 0, "double", (), 0),
    178: ("botStatisticUpdateSpeed", 1, "double", ("double",), 1),
    427: ("setVisible", 0, "void", ("bool",), 0),
    428: ("getVisible", 0, "bool", (), 1),
    429: ("drawVehicleSearchRadi", 1, "bool", ("bool",), 1),
    440: ("setStatus", 0, "StatusMap::Status", ("int", "StatusMap::Status"), 1),
    441: ("category", 1, "int", ("int",), 1),
    443: ("setTakeable", 0, "void", ("int", "bool"), 0),
    444: ("isTakeable", 0, "bool", ("int",), 1),
    447: ("basicTemperature", 1, "float", ("float",), 1),
    458: ("enableDebug", 0, "bool", ("bool",), 1),
    462: ("getTemperature", 0, "float", ("int",), 1),
    463: ("getStatus", 0, "StatusMap::Status", ("int",), 1),
    471: ("showNeighbours", 1, "bool", ("bool",), 1),
    474: ("strategicAreaName", 1, "std::string", ("std::string",), 1),
    576: ("seed", 1, "u32", ("u32",), 1),
}


def ai_type(cls):
    return "N4dice3hfe2ai" + "".join("%d%s" % (len(p), p) for p in cls.split("::")) + "E"


def recover_inline(elf, vt, b):
    """Return only a fully verified inline transcription; a mismatch keeps the binding omitted."""
    spec = INLINE_EXECUTORS.get(b["line"])
    if spec is None:
        return None
    addr, size, digest, description, fields, arms = spec
    off = elf.offset(addr)
    if (b["addr"]["executeObjectMethod"] != addr or b["size"]["executeObjectMethod"] != size
            or off is None or hashlib.sha256(elf.data[off:off + size]).hexdigest() != digest):
        b["problems"].append("inline executor instructions do not match the inspected image")
        return {}
    meta = b["meta"]
    signature = (meta["functionName"], meta["type"], meta["returnType"], tuple(meta["argTypes"]), meta["isDynamic"])
    if signature != INLINE_SIGNATURES[b["line"]]:
        b["problems"].append("inline executor signature does not match the registration")
        return {}
    if set(arms) != set(range(meta["minArgs"], meta["maxArgs"] + 1)):
        b["problems"].append("inline executor arities do not match the registration")
        return {}
    if b["line"] == 447:
        header = os.path.join(REPO, "Code/BF2/AI/AIDLL/AI/AIStrategicEntity/AIStrategicObject.h")
        if not re.search(r"\benum\s*\{\s*kBasicTemperature\s*=\s*0\s*\}", open(header).read()):
            b["problems"].append("basic-temperature index is not the inspected element zero")
            return {}
    rtype = RECEIVERS[b["receiver"]][1]
    values = {"receiver": RECEIVERS[b["receiver"]][0]}
    values.update({"arg%d" % k: "BF2_CC_ARG(%s, %d)" % (t, k) for k, t in enumerate(meta["argTypes"])})
    for token, (cls, field_off, typ, member, extent) in fields.items():
        if (ai_type(cls) != rtype and vt.base_offsets(rtype, ai_type(cls)) != [0]):
            b["problems"].append("inline field base %s is not at receiver offset zero" % cls)
            return {}
        if member_at(cls, field_off) != (typ, member, extent):
            b["problems"].append("inline field +0x%X of %s does not match %s" % (field_off, cls, member))
            return {}
        values[token] = member
    out = {}
    for argc, (hv, before, body) in arms.items():
        out[argc] = {"argc": argc, "hasReturnValue": hv, "storesReturn": hv,
                     "target": {"kind": "inline", "description": description, "addr": addr,
                                "beforeFlag": [s.format(**values) for s in before],
                                "body": [s.format(**values) for s in body]}}
    b["inlineEvidence"] = {"address": hexa(addr), "size": size, "sha256": digest,
                           "description": description}
    return out


def engine_timer_target(rtype, slot):
    """Resolve the two imported timer methods from the bf2 image, never a guessed slot name."""
    global _ENGINE_ELF
    expected = {0x18: "_ZN4dice3hfe2ai7BFTimer8setPauseEb", 0x1C: "_ZN4dice3hfe2ai7BFTimer8getPauseEv"}
    if rtype != ai_type("IBFTimer") or slot not in expected or not ENGINE_BINARY or not os.path.isfile(ENGINE_BINARY):
        return None
    if _ENGINE_ELF is None:
        _ENGINE_ELF = Elf(ENGINE_BINARY)
    e = _ENGINE_ELF
    v = e.by_mangled.get("_ZTV" + ai_type("BFTimer"))
    ti = e.by_mangled.get("_ZTI" + ai_type("BFTimer"))
    base = e.by_mangled.get("_ZTI" + rtype)
    f = e.by_mangled.get(expected[slot])
    if (not all((v, ti, base, f)) or ti["size"] != 12 or e.u32(ti["addr"] + 8) != base["addr"]
            or v["size"] != 0x2C or e.u32(v["addr"]) != 0 or e.u32(v["addr"] + 4) != ti["addr"]
            or e.u32(v["addr"] + 8 + slot) != f["addr"] or f["sec"] != ".text"):
        return None
    return {"class": ai_type("BFTimer"), "symbol": f["mangled"], "address": hexa(f["addr"])}


def analyse(elf, vt, reg, meths, code):
    """One binding record: metadata from the constructor, receiver, per-argc target calls,
    per-argument extraction, problems found."""
    d = decode_registration(elf, reg)
    line = d["line"]
    m = meths[line]
    b = {"line": line, "object": d["defaults"]["name"] if d["defaults"] else None, "meta": d,
         "addr": {k: v[0] for k, v in m.items()}, "size": {k: v[1] for k, v in m.items()},
         "problems": [], "arms": {}, "extract": {}}
    # receiver from isObjectActive: `return expr != 0;`
    ins = code[m["isObjectActive"][2]]
    p = emulate_executor(elf, ins, 0)
    recv = None
    direct = [c for c in p.calls if not is_plumbing(c)]
    if direct and direct[0]["kind"] == "direct":
        recv = direct[0]["sym"]
    else:
        for i in ins:
            if i.mn == "movl" and i.ops and re.match(r"^-0x[0-9a-f]+\(%e[bc]x\)$", i.ops[0]):
                recv = elf.word_symbol(elf.got + int(i.ops[0].split("(")[0], 16))
    b["receiver"] = recv
    if recv not in RECEIVERS:
        b["problems"].append("receiver %s not in RECEIVERS" % recv)
        return b
    # executor arms
    ins = code[m["executeObjectMethod"][2]]
    lo, hi = d["minArgs"], d["maxArgs"]
    inline = recover_inline(elf, vt, b)
    for argc in range(0, hi + 2):
        if inline is not None:
            if argc in inline:
                b["arms"][argc] = inline[argc]
            continue
        p = emulate_executor(elf, ins, argc)
        calls = [c for c in p.calls if not is_plumbing(c)]
        rcalls = [c for c in calls if c["kind"] == "direct" and c["sym"] == recv]
        tcalls = [c for c in calls if c not in rcalls]
        has_ret = {s[0]: s for s in p.stores}.get(0xA0)
        if not (lo <= argc <= hi):
            if calls:
                b["problems"].append("argc %d outside [%d,%d] still calls" % (argc, lo, hi))
            continue
        arm = {"argc": argc, "hasReturnValue": has_ret[1][1] if has_ret and has_ret[1] and has_ret[1][0] == "imm" else None,
               "storesReturn": stores_return(p), "target": None, "calls": tcalls,
               "receiverCalls": len(rcalls)}
        if len(tcalls) == 1:
            c = tcalls[0]
            if c["kind"] == "direct":
                arm["target"] = {"kind": "direct", "mangled": c["sym"], "dem": demangle([c["sym"]])[0],
                                 "addr": c["addr"]}
            elif c["kind"] == "virtual":
                rtype = RECEIVERS[recv][1]
                res = vt.resolve(rtype, c["slot"])
                named = [(cl, f) for cl, f in res if f != "__cxa_pure_virtual"]
                engine = engine_timer_target(rtype, c["slot"]) if not named else None
                if engine:
                    named = [(engine["class"], engine["symbol"])]
                names = sorted({re.sub(r"^.*::", "", demangle([f])[0].split("(")[0]) for cl, f in named})
                # A slot past the receiver type's own vtable is an implementer's own virtual: the
                # call goes through the implementer's primary vtable (receiver at offset 0).
                own = vt.vt.get(rtype)
                cast = None
                if own and c["slot"] >= own["size"] - 8:
                    impl = sorted({cl for cl, f in named if cl != rtype})
                    if len(impl) == 1 and vt.base_offsets(impl[0], rtype) == [0]:
                        cast = impl[0]
                    else:
                        b["problems"].append("argc %d: slot 0x%X beyond %s, implementer %s" % (argc, c["slot"], rtype, impl))
                arm["target"] = {"kind": "virtual", "slot": c["slot"], "resolved": named, "cast": cast,
                                 "name": names[0] if len(names) == 1 else None,
                                 "dem": demangle([named[0][1]])[0] if named else None,
                                 "addr": c["addr"], "engine": engine}
                if len(names) != 1:
                    b["problems"].append("argc %d: slot 0x%X of %s resolves to %s" % (argc, c["slot"], rtype, names))
            else:
                b["problems"].append("argc %d: call kind %s" % (argc, c["kind"]))
        elif not tcalls:
            fa = field_access(p, rcalls, recv, argc)
            if fa:
                arm["target"] = fa
            else:
                b["problems"].append("argc %d: no target call (inline body)" % argc)
        else:
            b["problems"].append("argc %d: %d target calls" % (argc, len(tcalls)))
        b["arms"][argc] = arm
    # argument extraction
    ins = code[m["setArgFromString"][2]]
    for k in range(1, hi + 1):
        p = emulate_executor(elf, ins, 0, params={0xC: ("imm", k)})
        ex = [c for c in p.calls if c["kind"] == "direct" and EXTRACT_RE.match(c["sym"])]
        if len(ex) != 1:
            b["problems"].append("arg %d: %d extraction calls" % (k, len(ex)))
            continue
        b["extract"][k] = {"mangled": ex[0]["sym"], "dem": demangle([ex[0]["sym"]])[0]}
    b["teardown"] = []
    ax = reg.get("atexit")
    if ax and ax[0] == "addr":
        b["teardown"].append(ax[1])
    # function-local statics (the std::string `arg`s): `lea __tcf_N@GOTOFF(%ebx)` before the
    # __cxa_atexit on their guard-initialisation path
    for i, insn in enumerate(ins):
        if insn.mn == "calll" and insn.tsym and insn.tsym.startswith("__cxa_atexit"):
            for prev in reversed(ins[max(0, i - 8):i]):
                pm = parse_mem(prev.ops[0]) if prev.mn == "leal" and prev.ops else None
                if pm and pm[1] == "%ebx" and not pm[2]:
                    b["teardown"].append(elf.got + pm[0])
                    break
    for k in (0, hi + 1):
        p = emulate_executor(elf, ins, 0, params={0xC: ("imm", k)})
        if any(c["kind"] == "direct" and EXTRACT_RE.match(c["sym"]) for c in p.calls):
            b["problems"].append("setArgFromString(%d) extracts" % k)
    return b


def analyse_all(elf):
    regs = extract_registrations(elf)
    meths = method_tables(elf)
    lo = min(v["isObjectActive"][0] for v in meths.values()) & ~0xF
    hi = max(a + s for v in meths.values() for a, s, _ in v.values())
    code = {k: v[1] for k, v in disasm(elf, lo, hi).items()}
    vt = Vtables(elf)
    return [analyse(elf, vt, r, meths, code) for r in regs], vt


# ----------------------------------------------------------------------------------------------
# Ledger: whether a bound target has a body in the tree
# ----------------------------------------------------------------------------------------------

def load_ledger(path):
    import csv
    led = {}
    if os.path.exists(path):
        for r in csv.DictReader(open(path)):
            for m in r["mangled"].split(" | "):
                led[m] = r
    return led


_LAYOUTS = {}


def member_at(class_name, off):
    """The tree member documented at `off` (a `/* +N */` or `/* +0xN */` layout comment in the
    class's header), or None."""
    if class_name not in _LAYOUTS:
        lay = {}
        root = os.path.join(REPO, "Code", "BF2", "AI", "AIDLL")
        for d, _, fs in os.walk(root):
            for f in fs:
                if not f.endswith(".h"):
                    continue
                text = open(os.path.join(d, f), errors="replace").read()
                m = re.search(r"^class\s+%s\b[^;]*?\{" % re.escape(class_name), text, re.M)
                if not m:
                    continue
                depth, i = 0, m.end() - 1
                while i < len(text):
                    if text[i] == "{":
                        depth += 1
                    elif text[i] == "}":
                        depth -= 1
                        if depth == 0:
                            break
                    i += 1
                for fm in re.finditer(r"/\*\s*\+?(0x[0-9A-Fa-f]+|\d+)\s*\*/\s*([\w:<>* ]+?)\s+(\w+)(?:\[(\d+)\])?\s*;",
                                      text[m.end():i]):
                    lay[int(fm.group(1), 0)] = (fm.group(2).strip(), fm.group(3),
                                              int(fm.group(4)) if fm.group(4) else None)
        _LAYOUTS[class_name] = lay
    return _LAYOUTS[class_name].get(off)


def target_symbol(t):
    if t["kind"] == "field":
        return None
    if t["kind"] == "direct":
        return t["mangled"]
    m = t["resolved"][0][1]
    return re.sub(r"^_ZThn\d+_", "_Z", m)


VEC_TYPES = {"Vec2", "Vec3", "hfe::Vec2", "hfe::Vec3"}


_EXTRACTORS = None


def tree_defines_extractor(type_name):
    """A body `operator>>(std::istream& ..., T& ...)` for the binding's own type spelling in the AI
    sources (the ledger misses typedef spellings such as materialId = unsigned char)."""
    global _EXTRACTORS
    if _EXTRACTORS is None:
        _EXTRACTORS = set()
        root = os.path.join(REPO, "Code", "BF2", "AI", "AIDLL")
        for d, _, fs in os.walk(root):
            for f in fs:
                if f.endswith(".cpp"):
                    for m in re.finditer(r"operator>>\(std::istream&\s*\w*,\s*([\w:]+)&\s*\w*\)\s*\{",
                                         open(os.path.join(d, f), errors="replace").read()):
                        _EXTRACTORS.add(m.group(1).split("::")[-1])
    return type_name.split("::")[-1] in _EXTRACTORS


def classify(b, led):
    """('emit', None) or ('omit', reason)."""
    for p in b["problems"]:
        if "extraction calls" in p:
            k = int(p.split()[1].rstrip(":"))
            if b["meta"]["argTypes"][k - 1] in VEC_TYPES:
                continue                 # Vec2/Vec3 extraction is an inline header operator>>
        return "omit", p
    for k, e in sorted(b["extract"].items()):
        if e["mangled"].startswith("_ZN4dice"):
            r = led.get(e["mangled"])
            st = r["status"] if r else "absent"
            if st not in ("done", "stop") and not tree_defines_extractor(b["meta"]["argTypes"][k - 1]):
                return "omit", "argument %d extractor %s is %s in the ledger" % (k, e["dem"], st)
    for argc, a in sorted(b["arms"].items()):
        t = a["target"]
        if t["kind"] == "inline":
            a["status"], a["targetSym"] = "inline", None
            continue
        if t["kind"] == "field":
            cls = cpp_class(RECEIVERS[b["receiver"]][1])
            mem = member_at(cls, t["offset"])
            if not mem:
                return "omit", "argc %d inline field +0x%X of %s has no named member in the tree" % (argc, t["offset"], cls)
            t["member"] = mem[1]
            a["status"], a["targetSym"] = "field", None
            continue
        sym = target_symbol(t)
        if t.get("engine"):
            a["status"], a["targetSym"] = "bf2 ELF " + t["engine"]["address"], sym
            continue
        r = led.get(sym)
        st = r["status"] if r else "absent"
        a["status"] = st
        a["targetSym"] = sym
        if st not in ("done", "stop"):
            return "omit", "argc %d target %s is %s in the ledger" % (argc, demangle([sym])[0], st)
    return "emit", None


def cxx_name(t):
    if t["kind"] == "virtual":
        return t["name"]
    d = t["dem"].split("(")[0]
    return d.split("::")[-1]


def nparams(dem):
    inner = dem[dem.index("(") + 1:dem.rindex(")")]
    if not inner.strip():
        return 0
    depth, n = 0, 1
    for c in inner:
        if c in "<(":
            depth += 1
        elif c in ">)":
            depth -= 1
        elif c == "," and depth == 0:
            n += 1
    return n


def cpp_class(mangled_type):
    """'N4dice3hfe2ai10AISettingsE' -> 'AISettings' (the generated code lives in dice::hfe::ai)."""
    d = demangle(["_ZTI" + mangled_type])[0].replace("typeinfo for ", "")
    return re.sub(r"^dice::hfe::ai::", "", d)


def hexa(a):
    return "0x%X" % a


def emit_class(b):
    m = b["meta"]
    line = b["line"]
    recv_expr = RECEIVERS[b["receiver"]][0]
    fn = m["functionName"]
    ret = m["returnType"]
    flag = m["isDynamic"]
    nargs = len(m["argTypes"])
    A = b["addr"]
    out = []
    kind = {0: "method", 1: "property", 2: "read-only", 3: "write-only"}.get(m["type"], "?")
    res = ("returns " + ret) if flag else ("no result" if ret == "void" else ret + " result discarded")
    out.append("// ConsoleClass%d %s.%s: %s, %d..%d arguments, %s." % (line, b["object"], fn, kind,
                                                                  m["minArgs"], m["maxArgs"], res))
    out.append("// ELF isObjectActive %s, executeObjectMethod %s," % (hexa(A["isObjectActive"]),
                                                                   hexa(A["executeObjectMethod"])))
    out.append("// setArgFromString %s, getReturnValueAsString %s, D1 %s, D0 %s."
               % (hexa(A["setArgFromString"]), hexa(A["getReturnValueAsString"]), hexa(A["D1"]), hexa(A["D0"])))
    for argc, a in sorted(b["arms"].items()):
        t = a["target"]
        if t["kind"] == "inline":
            out.append("// argc %d: verified inline body, %s" % (argc, t["description"]))
            continue
        if t["kind"] == "field":
            out.append("// argc %d: inline accessor, %s the member at +0x%X (%s)" % (
                argc, "reads" if t["access"] == "get" else "stores", t["offset"], t["member"]))
            continue
        how = "slot +0x%X" % t["slot"] if t["kind"] == "virtual" else "call"
        origin = a["status"] if t.get("engine") else "ledger " + a["status"]
        out.append("// argc %d: %s (%s at %s; %s)" % (argc, demangle([a["targetSym"]])[0], how,
                                                    hexa(t["addr"]), origin))
    out.append("#undef BF2_CC_NAME")
    out.append("#define BF2_CC_NAME ConsoleClass%d" % line)
    out.append("class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl")
    out.append("{")
    out.append("    BF2_CC_HEAD_NAMED(\"%s\", %d, %d, %d, \"%s\", %s)"
               % (fn, m["type"], m["minArgs"], m["maxArgs"], ret, "true" if flag else "false"))
    for i, t in enumerate(m["argTypes"]):
        out.append("            m_argTypes[%d] = \"%s\";" % (i, t))
    out.append("    AIMODULE_CC_TAIL(%d)" % nargs)
    out.append("    virtual bool isObjectActive() const { return %s != 0; }" % recv_expr)
    out.append("    virtual void executeObjectMethod()")
    out.append("    {")
    first = True
    for argc, a in sorted(b["arms"].items()):
        t = a["target"]
        if t["kind"] == "inline":
            out.append("        %sif (m_argCount == %d)" % ("" if first else "else ", argc))
            first = False
            out.append("        {")
            out.extend("            " + s for s in t["beforeFlag"])
            out.append("            m_hasReturnValue = %s;" % ("true" if a["hasReturnValue"] else "false"))
            out.extend("            " + s for s in t["body"])
            out.append("        }")
            continue
        call_args = ", ".join("BF2_CC_ARG(%s, %d)" % (m["argTypes"][k], k) for k in range(argc))
        recv = recv_expr
        if t.get("cast"):
            recv = "static_cast<%s*>(%s)" % (cpp_class(t["cast"]), recv_expr)
        if t["kind"] == "field":
            call = "%s->%s" % (recv, t["member"])
            if t["access"] == "set":
                call += " = BF2_CC_ARG(%s, 0)" % m["argTypes"][0]
        else:
            call = "%s->%s(%s)" % (recv, cxx_name(t), call_args)
        out.append("        %sif (m_argCount == %d)" % ("" if first else "else ", argc))
        first = False
        out.append("        {")
        hv = a["hasReturnValue"]
        out.append("            m_hasReturnValue = %s;" % ("true" if hv else "false"))
        if hv and flag:
            out.append("            m_returnValue = %s;" % call)
        else:
            out.append("            %s;" % call)
        out.append("        }")
    out.append("    }")
    out.append("    virtual void setArgFromString(int index, const std::string& value)")
    out.append("    {")
    for k in range(1, nargs + 1):
        e = b["extract"].get(k)
        t = m["argTypes"][k - 1]
        if e and e["mangled"].startswith("_ZN4dice"):
            q = e["dem"].split("(")[0]
            out.append("        AIMODULE_CC_EXTRACT_WITH(%s, %d, %s)" % (t, k, q))
        else:
            out.append("        AIMODULE_CC_EXTRACT(%s, %d)" % (t, k))
    out.append("    }")
    out.append("    BF2_CC_RETURN_AS_STRING" if flag else "    BF2_CC_NO_RETURN_AS_STRING")
    out.append("    BF2_CC_COMMON_HOOKS")
    if flag:
        out.append("private:")
        out.append("    %s m_returnValue;" % ret)
    out.append("};")
    out.append("BF2_CC_NAME consoleObject%d;" % line)
    return out


OUT = os.path.join(REPO, "Code", "BF2", "AI", "AIDLL", "AI", "Main", "AIModuleConsole.inc")
REPORT = os.path.join(REPO, "docs", "ai-reconstruct", "console-bindings.json")
LEDGER = os.path.join(REPO, "docs", "ai-reconstruct", "ledger.csv")

PRELUDE = r'''// GENERATED FILE - do not edit by hand. Regenerate with
//     python3 tools/ai-reconstruct/gen_console_bindings.py
// from libaidll.so (Linux 1.5.3153, ia-32, with symbols). Every address is an ELF address.
//
// The AI image's console bindings: %(total)d generated classes ConsoleClass<line>, all in the
// anonymous namespace of AIModule.cpp (the original __LINE__ is the class number). Emitted here:
// %(emitted)d; the %(omitted)d left out are listed in place with the reason (their commands stay
// unregistered, as before this file existed). Shared shapes recovered from the image:
//   - constructor (inlined into AIModule.cpp's static initializer, ELF 0x3EEE90): object name,
//     access and category from the SetConsoleClassDefaults in force; function name, m_type,
//     min/max argument count, return-type and argument-type strings (the stringized C++ types),
//     the +0xA1 flag (1 exactly when getReturnValueAsString formats a value), then the
//     needs-conversion flags (`argType != "std::string"`) and `aiList.push_back(this)`.
//   - isObjectActive: `receiver != 0`, the receiver being a global or an AIModule.cpp getter.
//   - executeObjectMethod: one arm per accepted argument count; m_hasReturnValue is stored before
//     the receiver is evaluated; the arm calls receiver->method(arguments) (virtual calls through
//     the receiver's static type) and stores the result when the flag is set. Arms that pass fewer
//     arguments than the target takes rely on the target's C++ default arguments.
//     The verified inline bodies preserve argument copies before that flag, unchecked indices,
//     repeated stores and discarded results; see each class's inline-body note and JSON hash.
//   - setArgFromString(index): `static T arg; std::istringstream in(value); in >> arg;
//     m_args[index - 1] = &arg;` with no fail check (no temporary, unlike Core's BF2_CC_CONVERT).
//   - getReturnValueAsString / range hooks: Core/ConsoleObjectMacro.h's BF2_CC_RETURN_AS_STRING,
//     BF2_CC_NO_RETURN_AS_STRING and BF2_CC_COMMON_HOOKS (one shape each in the image).
// [INFERRED] The original macro names; the class bodies are spelled out because the shared
// BF2_CONSOLE_* families hard-code a different +0xA1 value and the temp-and-commit conversion.

#pragma push_macro("BF2_CC_NAME")

// Converts argument `oneBasedIndex` into its function-local static (one per argument).
#define AIMODULE_CC_EXTRACT(T, oneBasedIndex)                                                     \
        if (index == (oneBasedIndex))                                                            \
        {                                                                                        \
            static T arg;                                                                        \
            std::istringstream in(value);                                                        \
            in >> arg;                                                                           \
            m_args[(oneBasedIndex) - 1] = reinterpret_cast<std::string*>(&arg);                  \
        }

// The same, where the image calls a named extractor that overload resolution would not pick.
#define AIMODULE_CC_EXTRACT_WITH(T, oneBasedIndex, extractor)                                     \
        if (index == (oneBasedIndex))                                                            \
        {                                                                                        \
            static T arg;                                                                        \
            std::istringstream in(value);                                                        \
            extractor(in, arg);                                                                  \
            m_args[(oneBasedIndex) - 1] = reinterpret_cast<std::string*>(&arg);                  \
        }

// Closes the constructor BF2_CC_HEAD_NAMED opened: conversion flags, then this file's list.
#define AIMODULE_CC_TAIL(nArgs)                                                                   \
            for (int i = 0; i < (nArgs); ++i)                                                    \
                m_argNeedsConversion[i] =                                                        \
                    (std::string(m_argTypes[i]) == "std::string") ? 0 : -1;                      \
            aiList.push_back(this);                                                              \
        }

'''

EPILOGUE = r'''
#undef AIMODULE_CC_TAIL
#undef AIMODULE_CC_EXTRACT_WITH
#undef AIMODULE_CC_EXTRACT
#pragma pop_macro("BF2_CC_NAME")
'''


def generate(elf, led):
    bs, vt = analyse_all(elf)
    emitted, omitted = [], []
    body = []
    current_obj = None
    defaults_seen = set()
    for b in bs:
        d = b["meta"]["defaults"]
        if d and d["object"] not in defaults_seen:
            # every defaults object up to this one (IAI has no bindings of its own)
            pass
        verdict, reason = classify(b, led)
        b["verdict"], b["reason"] = verdict, reason
    # defaults objects in construction order, each placed before its first class
    order = []
    for b in bs:
        d = b["meta"]["defaults"]
        if d and d["object"] not in [o["object"] for o in order]:
            order.append(d)
    all_defaults = []
    # IAI precedes AI with no class between them: recover it from the symbol table
    for s in elf.syms:
        mm = re.match(r"^dice::hfe::ai::\(anonymous namespace\)::(\w+)fullAccessSetConsoleClassDefaults$", s["dem"])
        if mm:
            all_defaults.append((s["addr"], mm.group(1)))
    all_defaults.sort()
    pending = [n for _, n in all_defaults]
    for b in bs:
        d = b["meta"]["defaults"]
        while d and d["object"] in pending and pending[0] != d["object"]:
            n = pending.pop(0)          # a defaults object with no binding of its own (IAI)
            body.append("")
            body.append("SetConsoleClassDefaults %sfullAccessSetConsoleClassDefaults(\"%s\", 2, 0);" % (n, n))
        if pending and d and pending[0] == d["object"]:
            n = pending.pop(0)
            body.append("")
            body.append("SetConsoleClassDefaults %sfullAccessSetConsoleClassDefaults(\"%s\", %d, %d);"
                        % (n, d["name"], d["access"], d["category"]))
        body.append("")
        if b["verdict"] == "emit":
            body += emit_class(b)
            emitted.append(b)
        else:
            body.append("// ConsoleClass%d %s.%s: left out, %s." % (b["line"], b["object"],
                                                                   b["meta"]["functionName"], b["reason"]))
            omitted.append(b)
    head = PRELUDE % {"total": len(bs), "emitted": len(emitted), "omitted": len(omitted)}
    text = head + "\n".join(body) + "\n" + EPILOGUE
    return text, bs, emitted, omitted


def verify_slots(emitted, build_dir, vt=None):
    """Every virtual call a generated binding makes must land, in the native build, on the same
    vtable slot the image calls. Clang dumps the static types' vtables (-fdump-vtable-layouts) for
    a probe that includes AIModule.cpp with the compile flags of the bf2_aidll build."""
    import shlex
    verify_slots.vt = vt
    need = collections.defaultdict(set)
    for b in emitted:
        for argc, a in b["arms"].items():
            t = a["target"]
            if t["kind"] != "virtual":
                continue
            cls = cpp_class(t["cast"] or RECEIVERS[b["receiver"]][1])
            need[cls].add((t["slot"], cxx_name(t), b["line"]))
    cc = json.load(open(os.path.join(build_dir, "compile_commands.json")))
    ref = next(e for e in cc if e["file"].endswith("AI/Main/AIModule.cpp"))
    args = shlex.split(ref["command"]) if "command" in ref else list(ref["arguments"])
    flags, skip = [], False
    for x in args[1:]:
        if skip:
            skip = False
            continue
        if x in ("-o", "-MF", "-MT", "-MQ"):
            skip = True
            continue
        if x in ("-c", "-MD", "-MMD") or x == ref["file"]:
            continue
        flags.append(x)
    probe = ['#include "%s"' % ref["file"]]
    for i, cls in enumerate(sorted(need)):
        probe.append("void gen_console_slot_probe_%d(dice::hfe::ai::%s* p) { (void)sizeof(*p); "
                     "typedef dice::hfe::ai::%s T; T* volatile q = p; (void)q; }" % (i, cls, cls))
    src = os.path.join(os.path.abspath(build_dir), "gen_console_slot_probe.cpp")
    open(src, "w").write("\n".join(probe) + "\n")
    r = subprocess.run([args[0]] + flags + ["-S", "-o", os.devnull, "-Xclang", "-fdump-vtable-layouts", src],
                       cwd=ref["directory"], capture_output=True, text=True)
    if r.returncode:
        print(r.stderr[-3000:])
        return 1
    tables, cur = {}, None
    for line in (r.stdout + r.stderr).splitlines():
        m = re.match(r"^Vtable for '(.+)' \(\d+ entries\)\.", line)
        if m:
            cur = m.group(1).replace("dice::hfe::ai::", "")
            tables[cur] = {}
            continue
        if cur is None:
            continue
        if not line.strip():
            cur = None
            continue
        m = re.match(r"^\s*(\d+) \| (.*)$", line)
        if m:
            tables[cur][int(m.group(1))] = m.group(2)
    bad = 0
    for cls, items in sorted(need.items()):
        tab = tables.get(cls)
        for slot, name, line in sorted(items):
            ent = tab.get(slot // 4 + 2, "") if tab else ""
            ok = re.search(r"::%s\(" % re.escape(name), ent) is not None
            if not ok:
                bad += 1
                print("SLOT MISMATCH ConsoleClass%d: %s +0x%X expects %s, native %s" % (line, cls, slot, name, ent or "(no vtable dumped)"))
    # The whole primary vtable of every static type, where the image has one with named entries.
    elf_vt = verify_slots.vt
    compared = 0
    for cls in sorted(need):
        mangled = "N4dice3hfe2ai" + "".join("%d%s" % (len(p), p) for p in cls.split("::")) + "E"
        if cls.count("::"):
            mangled = "N4dice3hfe2ai" + "".join("%d%s" % (len(p), p) for p in cls.split("::")) + "E"
        tab = tables.get(cls, {})
        engine_table = None
        if mangled == ai_type("IBFTimer") and _ENGINE_ELF is not None:
            engine_table = _ENGINE_ELF.by_mangled["_ZTV" + ai_type("BFTimer")]
        if (mangled not in elf_vt.vt and engine_table is None) or not tab:
            continue
        k = 0
        while True:
            if engine_table is not None:
                if 8 + 4 * k >= engine_table["size"]:
                    break
                f = _ENGINE_ELF.word_symbol(engine_table["addr"] + 8 + 4 * k)
            else:
                f = elf_vt.slot(mangled, mangled, 4 * k)
            ent = tab.get(k + 2)
            if f is None or ent is None or "offset_to_top" in ent or not (f.startswith("_Z") or f == "__cxa_pure_virtual"):
                break
            compared += 1
            name = re.sub(r"\(.*", "", demangle([f])[0]).split("::")[-1]
            if name.startswith("~"):
                ok = "~" in ent
            elif f == "__cxa_pure_virtual":
                ok = "[pure]" in ent
            else:
                ok = re.search(r"::%s\(" % re.escape(name), ent) is not None
            if not ok:
                bad += 1
                print("VTABLE ORDER %s slot %d (+0x%X): ELF %s, native %s" % (cls, k, 4 * k, demangle([f])[0], ent))
            k += 1
    print("verify-slots: %d virtual targets in %d static types, %d whole-vtable slots compared, %d mismatches"
          % (sum(len(v) for v in need.values()), len(need), compared, bad))
    return 1 if bad else 0


DEFAULT_CONTENT = os.path.join(_SERVER_DIR, "mods", "bf2") if _SERVER_DIR else None
CMD_RE = re.compile(r"^\s*([A-Za-z_][\w]*)\.([\w.]+)")


def script_texts(root):
    """(name, text) for every .ai/.con file under root, including those inside .zip archives."""
    import zipfile
    for d, _, fs in os.walk(root):
        for f in sorted(fs):
            path = os.path.join(d, f)
            low = f.lower()
            if low.endswith((".ai", ".con")):
                yield os.path.relpath(path, root), open(path, "rb").read().decode("latin-1")
            elif low.endswith(".zip"):
                try:
                    z = zipfile.ZipFile(path)
                except zipfile.BadZipFile:
                    continue
                for n in z.namelist():
                    if n.lower().endswith((".ai", ".con")):
                        yield os.path.relpath(path, root) + ":" + n, z.read(n).decode("latin-1")


def coverage(bs, root):
    """Every command on an AI-image console object used by the retail content, against the
    generated (registered) set. Lookup is case-insensitive; a property also answers set/get."""
    objs = {}
    for b in bs:
        objs.setdefault(b["object"].lower(), b["object"])
    bound, known = {}, {}
    for b in bs:
        key = (b["object"].lower(), b["meta"]["functionName"].lower())
        known[key] = b
        if b["verdict"] == "emit":
            bound[key] = b
    uses = collections.OrderedDict()
    for name, text in script_texts(root):
        for line in text.splitlines():
            m = CMD_RE.match(line)
            if not m or m.group(1).lower() not in objs:
                continue
            obj, fn = m.group(1).lower(), m.group(2).lower()
            uses.setdefault((obj, fn), []).append(name)
    rows = []
    for (obj, fn), files in uses.items():
        cand = [(obj, fn)]
        if fn.startswith(("set", "get")):
            cand.append((obj, fn[3:]))
        hit = next((c for c in cand if c in bound and (c == (obj, fn) or bound[c]["meta"]["type"] == 1)), None)
        k = next((c for c in cand if c in known), None)
        state = "bound" if hit else ("left out: %s" % known[k]["reason"] if k else "not in the image")
        rows.append({"command": "%s.%s" % (objs[obj], fn), "uses": len(files), "files": len(set(files)),
                     "class": (bound.get(hit) or known.get(k) or {}).get("line"), "state": state})
    return rows


def main():
    global ENGINE_BINARY, _ENGINE_ELF
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--binary", default=DEFAULT_BINARY, required=DEFAULT_BINARY is None,
                    help="ia-32 libaidll.so (default: $BF2_LINUX_SERVER_DIR/bin/ia-32/libaidll.so)")
    ap.add_argument("--engine", default=ENGINE_BINARY, help="ia-32 bf2 image for the imported timer slots")
    ap.add_argument("--out", default=OUT)
    ap.add_argument("--report", default=REPORT)
    ap.add_argument("--ledger", default=LEDGER)
    ap.add_argument("--check", action="store_true", help="fail when the output would change")
    ap.add_argument("--verify-slots", metavar="BUILD_DIR",
                    help="check the virtual targets' native slots against the image (clang vtable dump)")
    ap.add_argument("--content", default=DEFAULT_CONTENT,
                    help="retail mod directory whose .ai/.con scripts (and zips) are checked for coverage")
    a = ap.parse_args()
    ENGINE_BINARY, _ENGINE_ELF = a.engine, None
    elf = Elf(a.binary)
    led = load_ledger(a.ledger)
    text, bs, emitted, omitted = generate(elf, led)
    rep = {"binary": os.path.basename(a.binary), "output": os.path.relpath(a.out, REPO),
           "classes": [{"line": b["line"], "object": b["object"],
                        "function": b["meta"]["functionName"], "verdict": b["verdict"],
                        "reason": b["reason"],
                        "functions": {k: "0x%X" % v for k, v in sorted(b["addr"].items())},
                        "teardown": ["0x%X" % t for t in sorted(set(b.get("teardown", [])))],
                        **({"inlineEvidence": b["inlineEvidence"]} if "inlineEvidence" in b else {})}
                       for b in bs]}
    if a.content and os.path.isdir(a.content):
        rep["coverage"] = coverage(bs, a.content)
    rep_text = json.dumps(rep, indent=1) + "\n"
    if a.check:
        same = os.path.exists(a.out) and open(a.out).read() == text
        sys.exit(0 if same else 1)
    open(a.out, "w").write(text)
    open(a.report, "w").write(rep_text)
    print("gen_console_bindings: %d classes, %d emitted, %d left out -> %s"
          % (len(bs), len(emitted), len(omitted), os.path.relpath(a.out, REPO)))
    cov = rep.get("coverage", [])
    unb = [r for r in cov if r["state"] != "bound"]
    print("coverage: %d distinct AI commands in the retail scripts, %d bound, %d not bound"
          % (len(cov), len(cov) - len(unb), len(unb)))
    for r in unb:
        print("  unbound %s (%d uses in %d files): %s" % (r["command"], r["uses"], r["files"], r["state"]))
    if a.verify_slots:
        rc = verify_slots(emitted, a.verify_slots, Vtables(elf))
        if rc:
            sys.exit(rc)
    for b in omitted:
        print("  left out ConsoleClass%d %s.%s: %s" % (b["line"], b["object"], b["meta"]["functionName"],
                                                      b["reason"]))


if __name__ == "__main__":
    main()
