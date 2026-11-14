"""Evidence rules for VC7.1 runtime code. No address ranges imply a class.

EH cleanup membership is read from FuncInfo tables *referenced by actual frame
handlers*. Merely calling an STL import never makes a game method an STL helper.
"""
import bisect
import collections
import json
import re
import struct

import asm
from pe import PE

from classify import hx, literal

STL_ERRORS = {"map/set<T> too long", "vector<T> too long", "list<T> too long",
              "deque<T> too long", "string too long", "invalid string position",
              "invalid vector<T> subscript", "invalid deque<T> subscript"}


class RuntimeRules:
    def __init__(self, data, ledger, image, listing):
        self.F = {int(k, 16): v for k, v in data["retail"]["functions"].items()}
        self.L = {int(r["address"], 16): r for r in ledger if r["image"] == "RendDX9.dll"}
        self.pe = PE(image)
        self.imports = self.pe.imports()
        self.A, self.M, self.O = asm.load(str(listing))
        for f in self.F.values():
            if f.get("decode_basis") == "entry-specific objdump":
                lo, hi = bisect.bisect_left(self.A, f["address"]), bisect.bisect_left(self.A, f["analysis_end"])
                decoded = f.get("entry_decode_insns") or f["insns"]
                self.A[lo:hi] = [a for a, _, _ in decoded]
                self.M[lo:hi] = [m for _, m, _ in decoded]
                self.O[lo:hi] = [o for _, _, o in decoded]
        self.index = {a: i for i, a in enumerate(self.A)}
        self.result = {}
        self.eh_actions, self.eh_catches, self.eh_handlers = {}, {}, {}
        self.static_initializers = {}
        self.free = set()
        self.alloc = set()
        for a, f in self.F.items():
            if f["n"] <= 8:
                if any("!" in n and ("bf_free" in n or n.split("!")[-1] in ("free", "_free"))
                       for n in f["imports"]):
                    self.free.add(a)
                if any("bf_malloc" in n or "bf_alloc" in n or "operator_new" in f["name"]
                       for n in f["imports"]):
                    self.alloc.add(a)

    def add(self, a, kind, confidence, *evidence):
        if a not in self.L or self.L[a]["status"] != "todo":
            return
        if a not in self.result:
            self.result[a] = dict(classification=kind, confidence=confidence,
                                  evidence=list(evidence), our_definition="")

    def import_target(self, address):
        """Resolve only pure jump-to-IAT stubs, not arbitrary wrappers."""
        for _ in range(3):
            i = self.index.get(address)
            if i is None or self.M[i] != "jmp":
                return ""
            o = self.O[i]
            m = re.fullmatch(r"dword ptr \[(0x[0-9a-f]+)\]", o)
            if m:
                return self.imports.get(int(m[1], 16), "")
            if re.fullmatch(r"0x[0-9a-f]+", o):
                address = int(o, 16)
            else:
                return ""
        return ""

    def u32(self, a):
        return self.pe.u32(a)

    def eh(self):
        for i, (a, m, o) in enumerate(zip(self.A, self.M, self.O)):
            mm = re.fullmatch(r"eax, (0x[0-9a-f]+)", o)
            if m != "mov" or not mm or i + 1 >= len(self.A) or self.M[i + 1] != "jmp":
                continue
            dest = self.O[i + 1]
            if not re.fullmatch(r"0x[0-9a-f]+", dest):
                continue
            imported = self.import_target(int(dest, 16))
            if not imported.endswith("!__CxxFrameHandler"):
                continue
            info = int(mm[1], 16)
            if self.u32(info) not in (0x19930520, 0x19930521, 0x19930522):
                continue
            self.eh_handlers[a] = info
            self.add(a, "compiler", "high", "frame-handler mov eax, " + hx(info),
                     "tail call " + imported + "; FuncInfo magic " + hex(self.u32(info)))
            maxstate, unwind = self.u32(info + 4), self.u32(info + 8)
            if maxstate is not None and maxstate < 10000 and unwind:
                for j in range(maxstate):
                    tostate, action = self.u32(unwind + 8 * j), self.u32(unwind + 8 * j + 4)
                    if action in self.F and self.pe.in_sec(action, ".text"):
                        self.eh_actions.setdefault(action, []).append(
                            "FuncInfo %s UnwindMap %s state %d -> %s (handler %s -> %s)" %
                            (hx(info), hx(unwind), j, hex(tostate), hx(a), imported))
            ntry, tries = self.u32(info + 12), self.u32(info + 16)
            if ntry is not None and ntry < 10000 and tries:
                for j in range(ntry):
                    ncatches, handlers = self.u32(tries + 20 * j + 12), self.u32(tries + 20 * j + 16)
                    if ncatches is None or ncatches > 1000 or not handlers:
                        continue
                    for k in range(ncatches):
                        catch = self.u32(handlers + 16 * k + 12)
                        if catch in self.F and self.pe.in_sec(catch, ".text"):
                            self.eh_catches.setdefault(catch, []).append(
                                "FuncInfo %s TryBlockMap %s entry %d HandlerType %s catch target" %
                                (hx(info), hx(tries), j, hx(handlers + 16 * k)))
        for a, proof in sorted(self.eh_actions.items()):
            f = self.F[a]
            frame = any("ebp" in o for _, _, o in f["insns"])
            self.add(a, "compiler", "high" if frame else "medium", proof[0],
                     "EH cleanup funclet" + (" accesses enclosing ebp frame" if frame else ""))
        for a, proof in sorted(self.eh_catches.items()):
            self.add(a, "compiler", "high", proof[0], "outlined EH catch funclet")
        for a, r in sorted(self.L.items()):
            if r["symbol"].startswith("Catch@"):
                self.add(a, "compiler", "medium", "Ghidra EH catch-function label " + r["symbol"],
                         "outlined catch body; inspect caller's EH frame rather than recover as a method")

    def crt_init(self):
        # Recognize the exact two pushed range bounds passed to _initterm.
        for i, (a, m, o) in enumerate(zip(self.A, self.M, self.O)):
            if m != "call":
                continue
            imported = ""
            mm = re.fullmatch(r"dword ptr \[(0x[0-9a-f]+)\]", o)
            if mm:
                imported = self.imports.get(int(mm[1], 16), "")
            elif re.fullmatch(r"0x[0-9a-f]+", o):
                imported = self.import_target(int(o, 16))
            if imported.split("!")[-1] != "_initterm":
                continue
            stack = []
            for k in range(max(0, i - 10), i):
                mm = re.fullmatch(r"dword ptr \[esp(?: \+ (0x[0-9a-f]+))?\], (0x[0-9a-f]+)", self.O[k])
                if self.M[k] == "push":
                    stack.insert(0, int(self.O[k], 16) if re.fullmatch(r"0x[0-9a-f]+", self.O[k]) else None)
                elif self.M[k] == "mov" and mm:
                    slot = int(mm[1] or "0", 16) // 4
                    while len(stack) <= slot:
                        stack.append(None)
                    stack[slot] = int(mm[2], 16)
            if len(stack) < 2 or None in stack[:2]:
                continue
            lo, hi = sorted(stack[:2])
            if hi - lo > 0x10000 or (hi - lo) % 4 or not (
                    self.pe.in_sec(lo, ".rdata") or self.pe.in_sec(lo, ".data")):
                continue
            values = [self.u32(p) for p in range(lo, hi, 4)]
            if any(v and v not in self.index for v in values):
                continue
            for p in range(lo, hi, 4):
                target = self.u32(p)
                if target:
                    evidence = "CRT initializer table %s..%s slot %s passed to %s at %s" % (
                        hx(lo), hx(hi), hx(p), imported, hx(a))
                    self.static_initializers[target] = evidence
                    self.add(target, "compiler", "high", evidence, "dynamic global initializer ($E machinery)")
        # Initializer-registered global destruction thunks: require a short
        # argument-free wrapper over an absolute global object, rather than
        # declaring every user atexit callback to be compiler-generated.
        known_entries = sorted(set(self.F) | set(self.static_initializers) | set(self.eh_handlers) |
                               set(self.eh_actions) | set(self.eh_catches))
        for init in sorted(self.static_initializers):
            f = self.F.get(init)
            if not f:
                # Most CRT-table entries were never made into Ghidra functions.
                # Their entry is directly attested by the table; follow that
                # entry's instructions rather than losing its atexit registration.
                from extract import reachable_instructions
                i = bisect.bisect_right(known_entries, init)
                end = known_entries[i] if i < len(known_entries) else self.pe.section(".text")[1] + self.pe.section(".text")[2]
                lo, hi = bisect.bisect_left(self.A, init), bisect.bisect_left(self.A, end)
                insns, _ = reachable_instructions(self.pe, list(zip(self.A[lo:hi], self.M[lo:hi], self.O[lo:hi])), init, end)
                f = dict(insns=insns)
            insns = f["insns"]
            for i, (a, m, o) in enumerate(insns):
                if m != "call" or not re.fullmatch(r"0x[0-9a-f]+", o):
                    continue
                target_address = int(o, 16)
                imported = self.import_target(target_address)
                if not imported and self.F.get(target_address, {}).get("name") in ("_atexit", "atexit"):
                    imported = "static CRT!_atexit"
                if imported.split("!")[-1] not in ("atexit", "_atexit"):
                    continue
                if not i or insns[i - 1][1] != "push" or not re.fullmatch(r"0x[0-9a-f]+", insns[i - 1][2]):
                    continue
                target = int(insns[i - 1][2], 16)
                g = self.F.get(target)
                if not g or g["n"] > 35 or g["strings"]:
                    continue
                ops = ";".join(o for _, _, o in g["insns"])
                absolute = re.search(r"(?:ecx, |ptr \[)(0x10[0-9a-f]+)", ops)
                args = re.search(r"\[(?:esp|ebp) \+", ops)
                if absolute and not args:
                    self.add(target, "compiler", "high", "global destructor thunk registered at " + hx(a) +
                             " by CRT initializer " + hx(init), "absolute global-object reference " +
                             absolute[1] + "; no incoming stack arguments")

    def machinery(self):
        for a, r in sorted(self.L.items()):
            f = self.F.get(a)
            if not f or a in self.result:
                continue
            insns = [(m, o) for _, m, o in f["insns"] if m not in ("nop", "int3")]
            if not insns:
                continue
            name = r["symbol"]
            if name.startswith(("__CRT_", "__RTC_", "__ArrayUnwind", "__ehvec_", "__SEH_")):
                self.add(a, "crt", "medium", "Ghidra recognized runtime symbol " + name,
                         "imports=" + json.dumps(f["imports"]))
                continue
            if name == "operator_new":
                self.add(a, "crt", "medium", "Ghidra recognized operator_new runtime allocation wrapper",
                         "callees=" + ",".join(hx(c) for c in f["calls"]))
                continue
            # A pure this-adjusting jump cannot implement a second game body.
            core = [(m, o) for m, o in insns if (m, o) not in (
                ("push", "ebp"), ("mov", "ebp, esp"), ("pop", "ebp"))]
            if len(core) == 2 and core[0][0] in ("add", "sub") and re.fullmatch(
                    r"ecx, 0x[0-9a-f]+", core[0][1]) and core[1][0] == "jmp" and re.fullmatch(
                    r"0x[0-9a-f]+", core[1][1]):
                self.add(a, "compiler", "high", "this-adjustor " + " ".join(core[0]),
                         "tail dispatch to " + core[1][1] + "; no other body instructions")
                continue
            # Scalar/vector deleting destructors are compiler ABI wrappers. An
            # ordinary destructor that frees an owned member doesn't test the
            # caller's deleting flag and return the original this pointer.
            flag = any(m == "test" and re.fullmatch(r"byte ptr \[ebp \+ 0x8\], 0x1", o)
                       for m, o in insns)
            nonfree = set(f["calls"]) - self.free
            returns_self = any(m == "mov" and o in ("eax, esi", "eax, edi", "eax, ebx")
                               for m, o in insns)
            if (flag and returns_self and len(nonfree) == 1 and set(f["calls"]) & self.free and
                    not f["strings"] and not f["imports"] and f["n"] <= 45 and
                    insns[-1] == ("ret", "0x4")):
                self.add(a, "compiler", "high", "deleting-destructor wrapper tests stack flags & 1",
                         "calls " + hx(next(iter(nonfree))) + ", conditionally deallocates through " +
                         ",".join(hx(v) for v in sorted(set(f["calls"]) & self.free)) +
                         ", returns original this, ret 4")
                continue
            if (f["n"] <= 8 and f["imports"] and all(n.startswith("msvcr71.dll!") for n in f["imports"])
                    and not f["strings"] and not f["calls"]):
                self.add(a, "crt", "high", "pure CRT import wrapper", "imports=" + json.dumps(f["imports"]))
            # Constructor/destructor array iterator: four stack arguments (base,
            # stride, count, callback); callback receives successive this values.
            ops = ";".join(o for _, o in insns)
            if (f["n"] <= 45 and not f["strings"] and not f["imports"] and not f["calls"] and
                    all("[ebp + %s]" % offset in ops for offset in ("0x8", "0xc", "0x10", "0x14")) and
                    any(m == "call" and re.fullmatch(r"e[abcds][xip]", o) for m, o in insns) and
                    any(m in ("dec", "sub") and (m == "dec" or o.endswith(", 0x1")) for m, o in insns)):
                self.add(a, "compiler", "high", "four-argument array ctor/dtor iterator (base,stride,count,callback)",
                         "indirect callback, count decrement, element stride; no game strings/imports/direct calls")

    def stl(self):
        for a, r in sorted(self.L.items()):
            f = self.F.get(a)
            if not f or a in self.result:
                continue
            if r["symbol"].startswith("std::"):
                self.add(a, "stl", "medium", "recognized template symbol " + r["symbol"])
                continue
            messages = sorted(set(f["strings"]) & STL_ERRORS)
            other = set(f["strings"]) - STL_ERRORS
            throws = any("_CxxThrowException" in n or "_Xlen" in n or "_Xran" in n
                         for n in f["imports"])
            throw_calls = [c for c in f["calls"] if "_CxxThrowException" in self.import_target(c)]
            if messages and not other and (throws or throw_calls):
                # The standard diagnostic alone can be inlined into arbitrary
                # engine code. Require a bounded container kernel with no virtual
                # calls or unrelated imports before excluding it from recovery.
                virtual = any(m == "call" and "[" in o and not re.fullmatch(r"dword ptr \[0x[0-9a-f]+\]", o)
                              for _, m, o in f["insns"])
                imports_ok = all(n.startswith(("msvcp71.dll!", "msvcr71.dll!", "memory.dll!"))
                                 for n in f["imports"])
                if not virtual and imports_ok and f["n"] <= 240:
                    self.add(a, "stl", "high", "VC7.1 container diagnostic " + json.dumps(messages),
                             "length/range throw via " + (",".join(hx(c) for c in throw_calls) or "STL import") +
                             "; no business strings, virtual calls or unrelated imports")
        # The two rotations below were read instruction-by-instruction: each
        # rewires left/parent/right links, checks the _Isnil byte and replaces
        # the head's root link. Their callers include the diagnostic-attested
        # _Tree::_Insert at 0x10004990 and 0x100132E0. Match the full instruction
        # shape, not merely the existence of pointer loads or a tree-like loop.
        def rotation_shape(f):
            if f["calls"] or f["imports"] or f["strings"] or f["n"] != 35:
                return None
            insns = f["insns"]
            labels = {a: i for i, (a, _, _) in enumerate(insns)}
            result = []
            for _, m, o in insns:
                if m.startswith("j") and re.fullmatch(r"0x[0-9a-f]+", o):
                    if int(o, 16) not in labels:
                        return None
                    o = "label%d" % labels[int(o, 16)]
                if m == "cmp" and o.startswith("byte ptr ["):
                    o = re.sub(r"\+ 0x[0-9a-f]+", "+ _Isnil", o)
                result.append((m, o))
            return tuple(result)
        exemplars = {}
        for a, direction in ((0x100EFDB0, "left"), (0x10024C00, "right")):
            if a in self.F:
                shape = rotation_shape(self.F[a])
                if shape:
                    exemplars[shape] = (a, direction)
        for a, f in sorted(self.F.items()):
            exemplar = exemplars.get(rotation_shape(f))
            if exemplar:
                seed, direction = exemplar
                callers = [c for c in f["callers"] if self.result.get(c, {}).get("classification") == "stl"]
                self.add(a, "stl", "high" if callers else "medium",
                         "full 35-instruction VC7.1 _Tree %s-rotation shape = %s (local branches normalized)" %
                         (direction, hx(seed)), "root-link replacement and _Isnil guard; STL callers=" +
                         ",".join(hx(c) for c in callers))

    def run(self):
        self.eh()
        self.crt_init()
        self.machinery()
        self.stl()
        self.stl_kernels()
        return self.result

    def stl_kernels(self):
        """Full-shape copies of three directly inspected template kernels.

        The erase seed follows right recursively, left iteratively, tests _Isnil
        and frees each node. Range erase resets all three sentinel links and size,
        or advances/erases individual iterators. Vector insertion checks _Xlen,
        computes 1.5x capacity, copies/fills/copies and replaces its three pointers.
        Do not propagate 'STL' to arbitrary callees of these kernels.
        """
        def shape(f, kind):
            if f["strings"] or f["imports"] or not f.get("cfg_complete", True):
                return None
            insns = [(a, m, o) for a, m, o in f["insns"] if m not in ("nop", "int3") and
                     not (m == "lea" and re.fullmatch(r"(e\w+), \[\1\]", o))]
            addresses = [a for a, _, _ in insns]
            others, result = {}, []
            for _, m, o in insns:
                if m.startswith("j"):
                    if not re.fullmatch(r"0x[0-9a-f]+", o):
                        return None
                    t = int(o, 16)
                    if not f["address"] <= t < f.get("analysis_end", f["address"] + f["size"]):
                        return None
                    o = "label%d" % bisect.bisect_left(addresses, t)
                elif m == "call":
                    if not re.fullmatch(r"0x[0-9a-f]+", o):
                        return None
                    t = int(o, 16)
                    if t == f["address"]:
                        o = "SELF"
                    elif t in self.free:
                        o = "DEALLOCATE"
                    elif t in self.alloc:
                        o = "ALLOCATE"
                    elif t in self.F and "vector<T> too long" in self.F[t]["strings"]:
                        o = "VECTOR_XLEN"
                    elif kind == "range" and t in self.F and self.result.get(t, {}).get("kernel") == "tree-erase":
                        o = "TREE_ERASE"
                    else:
                        o = "HELPER%d" % others.setdefault(t, len(others))
                else:
                    if m == "cmp" and o.startswith("byte ptr ["):
                        o = re.sub(r"\+ 0x[0-9a-f]+", "+ _Isnil", o)
                    o = re.sub(r"0x10[0-9a-f]{6}", "IMAGE_ADDRESS", o)
                result.append((m, o))
            return tuple(result)
        seeds = ((0x100047A0, "erase", "tree-erase"), (0x10004CD0, "range", "tree-erase-range"),
                 (0x10007C70, "vector", "vector-insert-n"))
        for address, kind, kernel in seeds:
            f = self.F.get(address)
            if not f:
                continue
            exemplar = shape(f, kind)
            if not exemplar:
                continue
            for a, g in sorted(self.F.items()):
                if abs(g["n"] - f["n"]) > 10:
                    continue
                if shape(g, kind) != exemplar:
                    continue
                proof = ["full reachable instruction shape matches inspected %s kernel %s; local labels, "
                         "image pointers and type-dependent _Isnil byte position normalized" % (kernel, hx(address))]
                if kind == "erase":
                    if not (a in g["calls"] and set(g["calls"]) & self.free):
                        continue
                    proof.append("right-child recursion, left-child iteration, sentinel guard and deallocation")
                elif kind == "range":
                    if not any(self.result.get(c, {}).get("kernel") == "tree-erase" for c in g["calls"]):
                        continue
                    proof.append("calls identified tree-erase kernel; resets head left/root/right and size")
                else:
                    if not any(c in self.F and "vector<T> too long" in self.F[c]["strings"] for c in g["calls"]):
                        continue
                    proof.append("calls diagnostic-attested vector _Xlen; 1.5x growth and copy/fill/copy pointer replacement")
                self.add(a, "stl", "high", *proof)
                # Internal role tag supports range-kernel verification; it is not
                # written as a new attribution class or a reconstructed name.
                if a in self.result and self.result[a]["classification"] == "stl":
                    self.result[a]["kernel"] = kernel
