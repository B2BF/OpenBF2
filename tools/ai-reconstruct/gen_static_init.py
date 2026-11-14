#!/usr/bin/env python3
"""Recover source-level static definitions from the ia-32 AI image, conservatively.

Dry run (all static rows, including already documented rows):
    python3 tools/ai-reconstruct/gen_static_init.py --report out/static-init.json
Apply one directory, then build and record that unit before applying another:
    python3 tools/ai-reconstruct/gen_static_init.py --apply --directory AIBotActionPlan

Uses evidence.py's ELF reader and instruction listings; never writes the evidence cache.
Only complete instruction shapes are eligible for a source edit. Call lists alone cannot
establish an iostream-only initializer: inlined constructors can add stores without calls.
Unknown shapes, ambiguous TU placement, excluded files and missing TUs remain in the report.
No compiler-generated function is emitted. <iostream> supplies its own Init object; ordinary
definitions supply constructors and reverse-order teardown on the host C++ implementation.
"""

import argparse
import collections
import csv
import hashlib
import json
import os
from pathlib import Path
import re
import struct
import sys

import build_ledger
import evidence

REPO = Path(__file__).resolve().parents[2]
AI_ROOT = "Code/BF2/AI/AIDLL/AI/"
EXCLUDED = {AI_ROOT + p + ".cpp" for p in (
    "AIBotBehaviourPattern/BBPMedicAssist", "AIBotBehaviourPattern/BBPRevive",
    "AIBotActionPlan/BAPAUpdateVehicle", "AIBot/BotMain", "AIBot/SquadMember")}
STATIC = re.compile(r"^(?:__static_initialization_and_destruction_\d+|__tcf_\d+|_GLOBAL__[ID]_)")
MEM = re.compile(r"(?:(byte|word|dword) ptr )?\[([^\]]+)\]$")
REGS = set(evidence.REG32)
STACK = 0x70000000
IOS_CTOR = "std::ios_base::Init::Init()"
IOS_DTOR = "std::ios_base::Init::~Init()"
STRING_CTOR = ("std::basic_string<char, std::char_traits<char>, std::allocator<char>>"
               "::basic_string(char const*, std::allocator<char> const&)")


def is_static(row):
    return bool(STATIC.match(row["symbol"]))


def instructions(image, row):
    a = int(row["address"], 16)
    return image.insns(*image.func_range(a, int(row["size"])), whole=True)


def call_name(text):
    text = evidence.demangle_target(text)
    m = re.search(r"<(.*)>$", text)
    return m.group(1).removesuffix("@plt") if m else text


def normalized(image, row):
    """Complete instructions, with only PIC relocation fields and branch labels abstracted."""
    ins = instructions(image, row)
    labels = {a: i for i, (a, _) in enumerate(ins)}
    ann = image.ann.annotate(ins, int(row["address"], 16),
                             int(row["address"], 16) + int(row["size"]))
    result = []
    for (a, text), (_, _, notes, _) in zip(ins, ann):
        op, _, args = text.partition(" ")
        if op == "call":
            text = "call " + call_name(text)
        elif op.startswith("j"):
            target = int(args.split()[0], 16)
            text = op + " @" + str(labels.get(target, "outside"))
        elif op == "add" and args.startswith("ebx,"):
            # Verify the actual PIC base, rather than accepting any add immediate.
            i = labels[a]
            if i and ins[i - 1][1].startswith("call ") and "get_pc_thunk.bx" in ins[i - 1][1]:
                if (a + int(args.split(",")[1], 0)) & 0xffffffff == image.elf.got:
                    text = "add ebx, GOT-PC"
        if notes and MEM.search(args.split(", ", 1)[-1]):
            note = notes[0]
            if note.startswith("&std::__ioinit "):
                text = "lea eax, IOINIT"
            elif note == "GOT[__dso_handle]":
                text = "mov eax, DSO"
            elif note.startswith("&__tcf_"):
                text = "lea eax, TCF"
        result.append(text)
    return result


# Every instruction is checked, including both argument tests, atexit arguments and branches.
IOS_INIT = """push ebp
mov ebp, esp
sub esp, 0x18
mov dword ptr [ebp - 0x4], ebx
call __i686.get_pc_thunk.bx
add ebx, GOT-PC
cmp dword ptr [ebp + 0xc], 0xffff
je @12
mov ebx, dword ptr [ebp - 0x4]
mov esp, ebp
pop ebp
ret
cmp dword ptr [ebp + 0x8], 0x1
jne @8
lea eax, IOINIT
mov dword ptr [esp], eax
call std::ios_base::Init::Init()
mov eax, DSO
mov dword ptr [esp + 0x8], eax
xor eax, eax
mov dword ptr [esp + 0x4], eax
lea eax, TCF
mov dword ptr [esp], eax
call __cxa_atexit
jmp @8""".splitlines()

# The shipped compiler uses three scratch-register/store-order variants for atexit's null arg.
IOS_INITS = [IOS_INIT]
for _scratch in ("ecx", "edx"):
    IOS_INITS.append(IOS_INIT[:18] + ["xor %s, %s" % (_scratch, _scratch),
        "mov dword ptr [esp + 0x4], " + _scratch,
        "mov dword ptr [esp + 0x8], eax"] + IOS_INIT[21:])
IOS_TCF = """push ebp
mov ebp, esp
sub esp, 0x8
mov dword ptr [ebp - 0x4], ebx
call __i686.get_pc_thunk.bx
add ebx, GOT-PC
lea eax, IOINIT
mov dword ptr [esp], eax
call std::ios_base::Init::~Init()
mov ebx, dword ptr [ebp - 0x4]
mov esp, ebp
pop ebp
ret""".splitlines()


def string_array_teardown(image, row, base, count):
    """The GCC-3 reverse string-array loop, including its refcount/destruction arm."""
    displacement = base - image.elf.got
    load = "mov eax, dword ptr [ebx %s 0x%x]" % (
        "+" if displacement >= 0 else "-", abs(displacement))
    expected = ["push ebp", "mov ebp, esp", "push edi", "push esi", "push ebx",
        "sub esp, 0x2c", "call __i686.get_pc_thunk.bx", "add ebx, GOT-PC", load,
        "test eax, eax", "lea esi, [eax + 0x%x]" % (4 * count), "jne @19",
        "add esp, 0x2c", "pop ebx", "pop esi", "pop edi", "pop ebp", "ret",
        "lea esi, [esi + eiz]", "cmp esi, eax", "je @12", "sub esi, 0x4",
        "lea edi, [ebp - 0x28]", "mov edx, dword ptr [esi]", "mov eax, 0xffffffff",
        "lea ecx, [edx - 0xc]", "lock", "xadd dword ptr [edx - 0x4], eax",
        "test eax, eax", "jle @32", load, "jmp @19", "mov dword ptr [esp + 0x4], edi",
        "mov dword ptr [esp], ecx",
        "call std::string::_Rep::_M_destroy(std::allocator<char> const&)", "jmp @30"]
    # The body loads the array through its GOT SLOT, not through the array address itself.
    norm = normalized(image, row)
    slot_loads = []
    for address, (_, name, addend) in image.elf.rel.items():
        if name and image.elf.addr_of.get(name) == base and not addend:
            slot_loads.append(address)
    return any(norm == [t.replace(load, "mov eax, dword ptr [ebx %s 0x%x]" % (
        "+" if slot >= image.elf.got else "-", abs(slot - image.elf.got)))
        for t in expected] for slot in slot_loads)


class Unsupported(ValueError):
    pass


class InitTrace:
    """Bounded constant propagation along init(1, 65535), not an x86 emulator.

    Unknown data/flags/opcodes stop analysis. Stack stores are local; global stores and call
    arguments are retained for review. Only relocation slots and read-only bytes may be read
    from the image. In particular, BSS zeroes are not assumed to describe another TU's state.
    """
    def __init__(self, image, row):
        self.image, self.elf, self.row = image, image.elf, row
        self.reg = dict.fromkeys(REGS)
        self.reg.update(esp=STACK, ebp=STACK + 0x100)
        self.mem = {STACK + 4: 1, STACK + 8: 65535}
        self.calls, self.writes = [], []
        self.zf = None

    def number(self, expr):
        expr = expr.strip()
        if expr in REGS:
            return self.reg[expr]
        try:
            return int(expr, 0) & 0xffffffff
        except ValueError:
            raise Unsupported("unresolved operand " + expr)

    def address(self, expr):
        parts = re.findall(r"[+-]?\s*(?:0x[0-9a-f]+|[a-z]+|[0-9]+)", expr)
        if "*" in expr or "".join(parts).replace(" ", "") != expr.replace(" ", ""):
            raise Unsupported("unsupported address " + expr)
        n = 0
        for part in parts:
            part = part.replace(" ", "")
            sign = -1 if part.startswith("-") else 1
            value = self.number(part.lstrip("+-"))
            if value is None:
                raise Unsupported("unknown address register " + expr)
            n += sign * value
        return n & 0xffffffff

    def value(self, operand):
        m = MEM.fullmatch(operand)
        if not m:
            return self.number(operand)
        address = self.address(m.group(2))
        if address in self.mem:
            return self.mem[address]
        rel = self.elf.rel.get(address)
        if rel:
            _, name, addend = rel
            if name:
                value = self.elf.addr_of.get(name)
                if value is None:
                    raise Unsupported("undefined relocation " + name)
                return value + (addend or 0)
            return addend if addend is not None else self.elf.word(address)
        section = self.elf.section_of(address)
        if section and section["name"] == ".rodata":
            width = {"byte": 1, "word": 2, "dword": 4}[m.group(1)]
            return int.from_bytes(self.elf.read(address, width), "little")
        if STACK - 0x10000 < address < STACK + 0x10000:
            return None
        raise Unsupported("read of mutable storage at 0x%X" % address)

    def store(self, operand, value):
        if operand in REGS:
            self.reg[operand] = value
            return
        m = MEM.fullmatch(operand)
        if not m:
            raise Unsupported("unsupported destination " + operand)
        address = self.address(m.group(2))
        self.mem[address] = value
        if self.elf.section_of(address):
            self.writes.append(dict(address=address, symbol=self.elf.name(address),
                                    width=m.group(1), value=value))

    def run(self):
        ins = instructions(self.image, self.row)
        labels = {a: i for i, (a, _) in enumerate(ins)}
        pc, steps = 0, 0
        while pc < len(ins):
            steps += 1
            if steps > 4096:
                raise Unsupported("instruction budget exceeded")
            a, text = ins[pc]
            op, _, args = text.partition(" ")
            operands = args.split(", ")
            next_pc = pc + 1
            if op == "ret":
                return self
            if op == "push":
                self.reg["esp"] -= 4
                self.mem[self.reg["esp"]] = self.value(args)
            elif op == "pop":
                self.reg[args] = self.mem.get(self.reg["esp"])
                self.reg["esp"] += 4
            elif op == "mov":
                self.store(operands[0], self.value(operands[1]))
            elif op == "lea":
                m = MEM.fullmatch(operands[1])
                self.store(operands[0], self.address(m.group(2)))
            elif op in ("add", "sub", "xor", "dec", "inc", "cmp", "test"):
                left = self.value(operands[0])
                right = self.value(operands[1]) if len(operands) > 1 else 1
                if op == "xor" and operands[0] == operands[1]:
                    value = 0
                elif left is None or right is None:
                    raise Unsupported("unknown arithmetic " + text)
                elif op in ("add", "inc"):
                    value = left + right
                elif op in ("sub", "dec", "cmp"):
                    value = left - right
                elif op == "xor":
                    value = left ^ right
                else:
                    value = left & right
                value &= 0xffffffff
                self.zf = value == 0
                if op not in ("cmp", "test"):
                    self.store(operands[0], value)
            elif op in ("je", "jne", "jmp"):
                if op != "jmp" and self.zf is None:
                    raise Unsupported("unknown branch flags")
                if op == "jmp" or (op == "je") == self.zf:
                    target = int(args.split()[0], 16)
                    if target not in labels:
                        raise Unsupported("branch outside initializer")
                    next_pc = labels[target]
            elif op == "call":
                name = call_name(text)
                if name == "__i686.get_pc_thunk.bx":
                    self.reg["ebx"] = ins[pc + 1][0]
                else:
                    values = [self.mem.get(self.reg["esp"] + i * 4) for i in range(4)]
                    self.calls.append(dict(address=a, callee=name, arguments=values))
                    for reg in ("eax", "ecx", "edx"):
                        self.reg[reg] = None
                    self.zf = None
                    if name not in (IOS_CTOR, "__cxa_atexit", STRING_CTOR):
                        raise Unsupported("callee requires separate recovery: " + name)
            elif op != "nop":
                raise Unsupported("unsupported instruction " + text)
            pc = next_pc
        raise Unsupported("no return")


def symbol_at(elf, address):
    # Unlike sym_at(), do not let a nearby function or zero-sized label supply a data type.
    matches = [(a, size, name) for a, size, name, typ, sh in elf.syms
               if typ == 1 and sh and a <= address < a + size]
    return min(matches, key=lambda s: s[1]) if matches else None


def facts(image, row):
    ins = instructions(image, row)
    ann = image.ann.annotate(ins, int(row["address"], 16),
                             int(row["address"], 16) + int(row["size"]))
    return dict(callees=[call_name(t) for _, t in ins if t.startswith("call ")
                         and "get_pc_thunk" not in t],
                references=sorted({n for _, _, notes, _ in ann for n in notes
                                   if "GOT[" in n or "[.bss]" in n or "[.data]" in n}),
                listing="%s fn %s" % ("tools/ai-reconstruct/evidence.py", row["address"]))


def console_teardown(image, row):
    """Verify a fully inlined ConsoleObject base teardown (only its final vptr store)."""
    ins = instructions(image, row)
    if len(ins) != 9:
        return None
    texts = [t for _, t in ins]
    thunk = re.fullmatch(r"call 0x[0-9a-f]+ <__i686\.get_pc_thunk\.([cd]x)>", texts[2])
    if not thunk:
        return None
    reg = "e" + thunk.group(1)
    add = re.fullmatch(r"add " + reg + r", (0x[0-9a-f]+)", texts[3])
    if not add or (ins[3][0] + int(add.group(1), 16)) & 0xffffffff != image.elf.got:
        return None
    load = re.fullmatch(r"mov eax, dword ptr \[" + reg + r" ([+-]) (0x[0-9a-f]+)\]", texts[5])
    store = re.fullmatch(r"mov dword ptr \[" + reg + r" ([+-]) (0x[0-9a-f]+)\], eax", texts[7])
    if not load or not store:
        return None
    def address(match):
        return image.elf.got + int(match.group(2), 16) * (1 if match.group(1) == "+" else -1)
    target = image.elf.slot_target(address(load))
    obj = symbol_at(image.elf, address(store))
    if target != "vtable for dice::hfe::ConsoleObject" or not obj or obj[0] != address(store):
        return None
    name = image.elf.dem[obj[2]]
    if not re.fullmatch(r"dice::hfe::ai::\(anonymous namespace\)::consoleObject\d+", name):
        return None
    expected = ["push ebp", "mov ebp, esp", texts[2], texts[3], "pop ebp", texts[5],
                "add eax, 0x8", texts[7], "ret"]
    return name if texts == expected else None


def row_shape(image, row):
    """Independent teardown classification does not authorize documenting its TU initializer."""
    if not row["symbol"].startswith("__tcf_"):
        return "initializer" if row["symbol"].startswith("__static_") else "wrapper"
    if normalized(image, row) == IOS_TCF:
        return "ios-teardown"
    if console_teardown(image, row):
        return "console-vptr-teardown"
    return "unclassified-teardown"


def static_groups(rows):
    """Use ledger adjacency, but never merge a lone function-local teardown into another TU."""
    groups, pending = [], []
    for row in rows:
        if is_static(row):
            pending.append(row)
            if row["symbol"].startswith("_GLOBAL__I_"):
                groups.append(pending)
                pending = []
        elif pending:
            groups.extend([[r] for r in pending])
            pending = []
    groups.extend([[r] for r in pending])
    return groups


def classify(image, group):
    init = [r for r in group if r["symbol"].startswith("__static_initialization")]
    result = dict(shape="unclassified", objects=[], row_details={r["address"]: facts(image, r)
                                                               for r in group})
    if len(init) != 1:
        result["reason"] = "standalone teardown; needs its owning function/guard or console binding"
        return result
    initial = init[0]
    norm = normalized(image, initial)
    # The plain 88-byte shape has no alignment lea in current llvm-objdump output.
    ios_only = norm in IOS_INITS
    trace = InitTrace(image, initial)
    try:
        trace.run()
    except Unsupported as exc:
        result.update(reason=str(exc), calls=trace.calls, writes=trace.writes)
        return result
    result.update(calls=trace.calls, writes=trace.writes)
    calls = trace.calls
    if not calls or calls[0]["callee"] != IOS_CTOR:
        result["reason"] = "initializer does not start with the supported iostream guard"
        return result
    io = calls[0]["arguments"][0]
    io_sym = symbol_at(image.elf, io)
    if not io_sym or image.elf.dem[io_sym[2]] != "std::__ioinit":
        result["reason"] = "iostream receiver symbol not confirmed"
        return result
    tcfs = {int(r["address"], 16): r for r in group if r["symbol"].startswith("__tcf_")}
    exits = [c for c in calls if c["callee"] == "__cxa_atexit"]
    for c in exits:
        args = c["arguments"]
        if args[0] not in tcfs or args[1] != 0 or args[2] != image.elf.addr_of.get("__dso_handle"):
            result["reason"] = "atexit callback/argument/DSO not confirmed"
            return result
    if not exits or normalized(image, tcfs[exits[0]["arguments"][0]]) != IOS_TCF:
        result["reason"] = "iostream teardown has an unsupported instruction shape"
        return result
    # Prove the same local Init object is destroyed, not just another identically named one.
    tcf = tcfs[exits[0]["arguments"][0]]
    tcf_text = image.listing(int(tcf["address"], 16), int(tcf["size"]), whole=True)
    io_displacement = io - image.elf.got
    io_expr = "[ebx %s 0x%x]" % ("+" if io_displacement >= 0 else "-", abs(io_displacement))
    if io_expr not in tcf_text:
        result["reason"] = "iostream constructor/destructor receivers differ"
        return result
    result["objects"] = [dict(symbol="std::__ioinit", type="std::ios_base::Init",
                              constructor=IOS_CTOR, arguments=[],
                              teardown=tcf["address"], definition="#include <iostream>")]
    wrapper = group[-1]
    wrap_ins = instructions(image, wrapper)
    scratch = wrap_ins[1][1].split(",")[0].removeprefix("mov ") if len(wrap_ins) > 1 else ""
    expected_wrapper = ["push ebp", "mov %s, 0xffff" % scratch, "mov ebp, esp", "sub esp, 0x8",
                        "mov dword ptr [esp + 0x4], " + scratch, "mov dword ptr [esp], 0x1",
                        "call " + initial["address"].lower(), "mov esp, ebp", "pop ebp", "ret"]
    actual_wrapper = [t.split(" <", 1)[0] for _, t in wrap_ins]
    if (scratch not in ("eax", "ecx", "edx") or
            not wrapper["symbol"].startswith("_GLOBAL__I_") or actual_wrapper != expected_wrapper):
        result["reason"] = "_GLOBAL__I_ wrapper is not init(1,65535)"
        return result
    if ios_only and len(group) == 3 and len(calls) == 2 and not trace.writes:
        result.update(shape="ios-only", reason="complete initializer, teardown and wrapper verified")
        return result
    # Decode string-array constructors in execution order. Unsupported teardown stays open.
    strings = [c for c in calls if c["callee"] == STRING_CTOR]
    if strings and not trace.writes:
        start = strings[0]["arguments"][0]
        symbol = symbol_at(image.elf, start)
        if symbol and symbol[0] == start and symbol[1] == 4 * len(strings):
            values = []
            for i, c in enumerate(strings):
                receiver, literal, allocator = c["arguments"][:3]
                value = image.elf.cstr(literal, limit=4096) if literal else None
                if receiver != start + i * 4 or value is None or not allocator or not (
                        STACK - 0x10000 < allocator < STACK):
                    break
                values.append(value)
            if len(values) == len(strings) and len(exits) == 2 and len(calls) == len(strings) + 3:
                verified = string_array_teardown(image, tcfs[exits[1]["arguments"][0]], start, len(values))
                result.update(shape="ios+string-array" if verified else "unclassified",
                              reason="string array constructor order and reverse teardown verified" if verified
                              else "array teardown needs full-shape verification")
                result["objects"].append(dict(symbol=image.elf.dem[symbol[2]], type="std::string",
                    count=len(values), constructor=STRING_CTOR, arguments=values,
                    teardown="0x%X" % exits[1]["arguments"][0]))
                return result
    result.update(shape="ios+stores", reason="inlined static construction/stores need separate type evidence")
    return result


def source_target(group, rows, manifest, data_files=None):
    """Prefer an attested path in the key, then the key function's source, then ledger TU."""
    wrapper = group[-1]
    key = wrapper["mangled"].removeprefix("_GLOBAL__I_")
    m = re.search(r"(?:AI_AIDLL_AI_|Core_|Math_|IO_).*?\.cpp", key)
    if m:
        flat = {f.removeprefix("Code/BF2/").replace("/", "_"): f for f in manifest}
        if m.group(0) in flat:
            return flat[m.group(0)], "key source path"
        return "", "key attests a source path absent from the manifest"
    matches = [r for r in rows if key in r["mangled"].split(" | ") and not is_static(r)]
    files = {r["file"] for r in matches if r["file"].endswith(".cpp") and r["file_inferred"] == "0"}
    if len(files) == 1:
        return files.pop(), "key symbol"
    files = (data_files or {}).get(key, set())
    if len(files) == 1:
        return next(iter(files)), "key data definition"
    if len(files) > 1:
        return "", "key data has competing definitions in manifest TUs"
    files = {r["file"] for r in group}
    if len(files) == 1:
        return files.pop(), "ledger adjacency [INFERRED]"
    return "", "inconsistent ledger attribution"


def data_key_files(image, groups, source_texts, manifest):
    """A _GLOBAL__I_ key can name a data member, which has no function-ledger row.

    Locate its actual namespace-scope definition instead of letting TU adjacency put an
    interface singleton in the preceding class's source file. This also detects merged TUs.
    """
    keys = {g[-1]["mangled"].removeprefix("_GLOBAL__I_") for g in groups
            if g[-1]["mangled"].startswith("_GLOBAL__I_")}
    objects = {name for _, _, name, typ, sh in image.elf.syms if typ == 1 and sh}
    result, code_cache = {}, {}
    for key in keys & objects:
        name = image.elf.dem.get(key, key)
        if not re.fullmatch(r"[A-Za-z_]\w*(?:::[A-Za-z_]\w*)+", name):
            continue
        components = name.split("::")
        for filename in manifest:
            text = source_texts.get(filename, "")
            if components[-1] not in text:
                continue
            if filename not in code_cache:
                code_cache[filename] = build_ledger.blank_code(text)[0]
            code = code_cache[filename]
            for i in range(len(components)):
                suffix = r"\s*::\s*".join(map(re.escape, components[i:]))
                pattern = r"(?<![\w:])" + suffix + r"\s*(?=[=;{\[(])"
                for match in re.finditer(pattern, code):
                    if namespace_at(text, match.start(), code) != components[:i]:
                        continue
                    boundary = max(code.rfind(c, 0, match.start()) for c in ";{}") + 1
                    if re.search(r"\bextern\b", code[boundary:match.start()]):
                        continue
                    result.setdefault(key, set()).add(filename)
    return result


def include_edit(text, comment):
    """Place the Init definition before every source-level static definition in the TU."""
    include = re.search(r"^\s*#\s*include\s*<iostream>[^\n]*\n", text, re.M)
    if include:
        # Move a late include only through the report: moving it could reorder other statics.
        prefix_code, _ = build_ledger.blank_code(text[:include.start()])
        if prefix_code.strip():
            return None, "iostream include follows source declarations; verify initialization order"
        return text[:include.start()] + comment + text[include.start():], "marked existing include"
    first_include = re.search(r"^[ \t]*#\s*include\b", text, re.M)
    if not first_include:
        return None, "no include block"
    prefix_code, _ = build_ledger.blank_code(text[:first_include.start()])
    if prefix_code.strip():
        return None, "source declaration before include block"
    return (text[:first_include.start()] + comment + "#include <iostream>\n\n" +
            text[first_include.start():]), "added iostream definition"


def namespace_at(text, position, code=None):
    """Return the namespace chain; a class/function/block makes a definition ineligible."""
    if code is None:
        code, _ = build_ledger.blank_code(text)
    stack, boundary = [], 0
    for i, char in enumerate(code[:position]):
        if char == "{":
            kind, name = build_ledger.parse_head(code[boundary:i])
            stack.append(name if kind == "namespace" else None)
            boundary = i + 1
        elif char == "}":
            if not stack:
                return None
            stack.pop()
            boundary = i + 1
        elif char == ";":
            boundary = i + 1
    return stack if None not in stack else None


def string_definition(text, obj):
    """Validate type, scope, extent and every literal, preserving the evidenced element order."""
    components = obj["symbol"].split("::")
    code, _ = build_ledger.blank_code(text)
    name = components[-1]
    pat = r"\bstd::string\s+(%s)\s*\[\s*(\d+)\s*\]\s*=\s*\{([^;]*)\}\s*;" % re.escape(name)
    hits = list(re.finditer(pat, code))
    if len(hits) != 1:
        return None
    m = hits[0]
    boundary = max(code.rfind(c, 0, m.start()) for c in ";{}") + 1
    if code[boundary:m.start()].strip():
        return None
    prefix = re.sub(r"\bnamespace\s+[A-Za-z_]\w*\s*\{", "", code[:m.start()])
    if prefix.replace("}", "").strip():
        # This supported initializer constructs only the Init guard and this array. Do not
        # document an array placed after another source-level object without reviewing order.
        return None
    if namespace_at(text, m.start()) != components[:-1] or int(m.group(2)) != obj["count"]:
        return None
    raw = text[m.start(3):m.end(3)]
    literals = re.findall(r'"(?:[^"\\]|\\.)*"', raw)
    remainder = re.sub(r'"(?:[^"\\]|\\.)*"', "", raw)
    if remainder.strip(" \t\r\n,"):
        return None
    try:
        values = [json.loads(s) for s in literals]
    except ValueError:
        return None
    return m.start() if values == obj["arguments"] else None


def string_edit(text, obj, target, source_texts):
    """Emit only an absent, evidenced string array; any competing definition blocks emission."""
    components = obj["symbol"].split("::")
    name = components[-1]
    if string_definition(text, obj) is not None:
        return text, "matching string-array definition"
    # A use alone is not proof of another definition, but is enough to demand manual review.
    # This deliberately errs toward a report rather than risking an ODR violation.
    for path, source in source_texts.items():
        if path == target:
            continue
        code, _ = build_ledger.blank_code(source)
        if re.search(r"\b" + re.escape(name) + r"\b", code):
            return None, "string-array symbol occurs in another TU: " + path
    code, _ = build_ledger.blank_code(text)
    for occurrence in re.finditer(r"\b" + re.escape(name) + r"\b", code):
        if namespace_at(text, occurrence.start()) != components[:-1]:
            continue
        boundary = max(code.rfind(c, 0, occurrence.start()) for c in ";{}") + 1
        if not re.search(r"\bextern\b", code[boundary:occurrence.start()]):
            return None, "existing namespace-scope symbol differs in type, value, extent or order"
    first = re.search(r"\S", code)
    if not first or not code[first.start():].startswith("namespace"):
        return None, "no safe namespace-scope insertion point"
    definition = "".join("namespace " + c + " { " for c in components[:-1]) + "\n"
    definition += "std::string %s[%d] = { %s };\n" % (
        name, obj["count"], ", ".join(json.dumps(v) for v in obj["arguments"]))
    definition += "}" * len(components[:-1]) + "\n\n"
    return text[:first.start()] + definition + text[first.start():], "added string-array definition"


def run(args):
    if args.apply and not args.directory:
        raise SystemExit("--apply requires one --directory; build/record it before starting another")
    with open(args.ledger) as ledger:
        rows = list(csv.DictReader(ledger))
    image = evidence.Image("ia-32", args.binary)
    if (image.elf.is64 or image.elf.data[5] != 1 or
            struct.unpack_from("<H", image.elf.data, 18)[0] != 3):
        raise SystemExit("expected original ia-32 ELF (EM_386)")
    manifest, _ = build_ledger.manifest_files()
    groups = static_groups(rows)
    source_texts = {str(p.relative_to(REPO)): p.read_text(errors="replace")
                    for p in (REPO / "Code/BF2").rglob("*.cpp")}
    data_files = data_key_files(image, groups, source_texts, manifest)
    targets = [source_target(g, rows, manifest, data_files) for g in groups]
    multiplicity = collections.Counter(f for f, _ in targets)
    results, changed, pending = [], [], []
    for group, (target, attribution) in zip(groups, targets):
        result = classify(image, group)
        result.update(file=target, attribution=attribution,
                      rows=[{k: r[k] for k in ("address", "size", "symbol", "file", "status", "mangled")}
                            for r in group], action="report")
        selected = not args.directory or (str(Path(target).parent) == args.directory or
                                         str(Path(target).parent) == AI_ROOT + args.directory)
        if not selected:
            result["reason"] = "outside selected directory; " + result["reason"]
        elif target in EXCLUDED:
            result["reason"] = "excluded by task ownership"
        elif target not in manifest or not (REPO / target).is_file():
            result["reason"] = ((attribution + "; ") if not target else "") + (
                "TU absent from source manifest/tree; " + result["reason"])
        elif multiplicity[target] > 1:
            result["reason"] = "multiple retail TUs mapped to one source file; " + result["reason"]
        elif result["shape"] in ("ios-only", "ios+string-array"):
            text = (REPO / target).read_text()
            original = text
            if result["shape"] == "ios+string-array":
                text, reason = string_edit(text, result["objects"][1], target, source_texts)
                result["definition_check"] = reason
                if text is None:
                    result["reason"] = reason
                    results.append(result)
                    continue
            addresses = [r["address"] for r in group]
            documented = set()
            for m in re.finditer(r"(?:^[ \t]*//.*\n)+", text, re.M):
                if re.search(r"static init(?:iali[sz]ation)?\b", m.group(0)):
                    documented.update(a.lower() for a in re.findall(r"0x[0-9a-fA-F]{5,8}", m.group(0)))
            if (text == original and all(a.lower() in documented for a in addresses)
                    and "#include <iostream>" in text):
                result.update(action="already documented", reason="matching include and address comment exist")
            else:
                entries = ", ".join("%s %s" % (r["symbol"], r["address"]) for r in group[1:-1])
                description = ("<iostream>'s Init object only" if result["shape"] == "ios-only" else
                               "<iostream>'s Init object, then " + result["objects"][1]["symbol"])
                comment = "// static init: ELF %s (%s, _GLOBAL__I %s): %s.\n" % (
                    group[0]["address"], entries, group[-1]["address"], description)
                edited, reason = include_edit(text, comment)
                result.update(action="eligible" if edited else "report", reason=reason)
                if edited and args.apply:
                    pending.append((REPO / target, original, edited))
                    changed.append(target)
                    result["action"] = "applied"
                result["definitions_added"] = int(edited is not None and edited.count("#include <iostream>") >
                    original.count("#include <iostream>")) + int(
                    result.get("definition_check") == "added string-array definition")
        results.append(result)
    by_shape, by_action, by_row_shape = collections.Counter(), collections.Counter(), collections.Counter()
    for result in results:
        by_shape[result["shape"]] += len(result["rows"])
        by_action[result["action"]] += len(result["rows"])
        for row in result["rows"]:
            detail = result["row_details"][row["address"]]
            detail["shape"] = row_shape(image, row)
            if detail["shape"] == "console-vptr-teardown":
                detail["destroyed_symbol"] = console_teardown(image, row)
            by_row_shape[detail["shape"]] += 1
    applied = [r for r in results if r["action"] == "applied"]
    summary = dict(rows=sum(len(g) for g in groups), rows_by_shape=dict(by_shape),
                   rows_by_instruction_shape=dict(by_row_shape),
                   rows_by_action=dict(by_action), changed_files=changed,
                   newly_marked_rows=sum(row["status"] != "done" for r in applied for row in r["rows"]),
                   definitions_added=sum(r.get("definitions_added", 0) for r in applied))
    report = dict(binary=image.binary, binary_sha256=hashlib.sha256(image.elf.data).hexdigest(),
                  architecture="ELF32 EM_386", directory=args.directory,
                  summary=summary, groups=results)
    # Finish all classification/report serialization before touching source. Roll back only our
    # writes on a failed write/report; detect concurrent changes rather than overwriting them.
    serialized = json.dumps(report, indent=2, sort_keys=True) + "\n"
    written = []
    try:
        for path, original, edited in pending:
            if path.read_text() != original:
                raise RuntimeError("source changed during analysis: " + str(path))
            path.write_text(edited)
            written.append((path, original))
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(serialized)
    except BaseException:
        for path, original in reversed(written):
            path.write_text(original)
        raise
    print(json.dumps({k: v if k != "changed_files" else len(v) for k, v in summary.items()}, sort_keys=True))
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--binary", default=os.path.join(evidence.BIN_DIR, "ia-32", "libaidll.so"))
    parser.add_argument("--ledger", type=Path, default=REPO / "docs/ai-reconstruct/ledger.csv")
    parser.add_argument("--report", type=Path, default=REPO / "out/g2-static/static-init.json")
    parser.add_argument("--directory", help="one exact directory, relative to AI/ or the repository")
    parser.add_argument("--apply", action="store_true")
    run(parser.parse_args())


if __name__ == "__main__":
    main()
