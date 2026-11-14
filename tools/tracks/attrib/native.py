"""Read x86_64 Mach-O archive anchors, without depending on a Python object-file package.

Only text symbols delimit functions. Relocations identify calls, data references and
pointer-table slots; RIP-relative addends are resolved against the *object's* sections.
The LLVM disassembler supplies instruction boundaries and mnemonics, never symbol identity.
"""
import bisect
import math
import re
import struct
import subprocess


def members(path):
    data = path.read_bytes()
    if data[:8] != b"!<arch>\n":
        raise ValueError("expected an ar archive: %s" % path)
    p = 8
    while p < len(data):
        header = data[p:p + 60]
        if len(header) != 60 or header[58:] != b"`\n":
            raise ValueError("invalid archive member")
        size = int(header[48:58])
        name = header[:16].decode().strip()
        body = data[p + 60:p + 60 + size]
        if name.startswith("#1/"):
            n = int(name[3:])
            name, body = body[:n].rstrip(b"\0").decode(), body[n:]
        else:
            name = name.rstrip("/")
        if not name.startswith("__.SYMDEF"):
            yield name, body
        p += 60 + size + (size & 1)


class Object:
    def __init__(self, name, data):
        self.name, self.data = name, data
        magic, cpu, _, kind, ncmds = struct.unpack_from("<IIIII", data)
        if (magic, cpu, kind) != (0xFEEDFACF, 0x01000007, 1):
            raise ValueError("%s: expected Mach-O x86_64 object" % name)
        self.sections, self.symbols = [], []
        p, symtab = 32, None
        for _ in range(ncmds):
            cmd, size = struct.unpack_from("<II", data, p)
            if cmd == 0x19:  # LC_SEGMENT_64
                nsecs = struct.unpack_from("<I", data, p + 64)[0]
                for i in range(nsecs):
                    q = p + 72 + i * 80
                    nm = data[q:q + 16].rstrip(b"\0").decode()
                    addr, length, off, _, reloff, nreloc, flags = struct.unpack_from("<QQIIIII", data, q + 32)
                    self.sections.append(dict(name=nm, addr=addr, size=length, off=off,
                                              reloff=reloff, nreloc=nreloc, flags=flags))
            elif cmd == 2:
                symtab = struct.unpack_from("<IIII", data, p + 8)
            p += size
        # A TU wholly removed by preprocessor selection can be an empty object.
        symoff, nsyms, stroff, _ = symtab or (0, 0, 0, 0)
        for i in range(nsyms):
            si, typ, sec, desc, value = struct.unpack_from("<IBBHQ", data, symoff + i * 16)
            end = data.index(b"\0", stroff + si)
            self.symbols.append(dict(name=data[stroff + si:end].decode(), type=typ,
                                     sec=sec, desc=desc, addr=value))
        self.texts = [s for s in self.sections if s["flags"] & 0x80000400]
        self.code_sections = {self.sections.index(s) + 1 for s in self.texts}
        self.text = self.texts[0] if self.texts else None
        self.entries = sorted({s["addr"] for s in self.symbols
                               if s["sec"] in self.code_sections and not s["type"] & 0xE0
                               and not s["name"].startswith(("ltmp", "L"))})

    def at(self, address, size):
        for s in self.sections:
            if s["addr"] <= address and address + size <= s["addr"] + s["size"]:
                if s["flags"] & 0xFF in (1, 0xC, 0x12):  # zero-fill
                    return None, s
                p = s["off"] + address - s["addr"]
                return self.data[p:p + size], s
        return None, None

    def relocations(self, sec):
        for i in range(sec["nreloc"]):
            off, bits = struct.unpack_from("<II", self.data, sec["reloff"] + 8 * i)
            if off & 0x80000000:
                raise ValueError("unexpected scattered relocation in x86_64 object")
            symbolnum, pcrel, length, external, kind = (bits & 0xFFFFFF, (bits >> 24) & 1,
                                                       1 << ((bits >> 25) & 3),
                                                       (bits >> 27) & 1, bits >> 28)
            raw = self.data[sec["off"] + off:sec["off"] + off + length]
            addend = int.from_bytes(raw, "little", signed=bool(pcrel))
            target, symbol = None, None
            if external:
                s = self.symbols[symbolnum]
                symbol = s["name"]
                if s["sec"] and kind not in (3, 4):  # not GOT/GOT_LOAD
                    target = s["addr"] + addend
            elif symbolnum:
                # Local PC-relative relocations already carry target - (place + 4).
                target = sec["addr"] + off + length + addend if pcrel else addend
                if pcrel and kind in (6, 7, 8):  # SIGNED_1/2/4
                    target += {6: 1, 7: 2, 8: 4}[kind]
            yield dict(place=sec["addr"] + off, symbol=symbol, target=target,
                       kind=kind, external=bool(external), width=length)


def demangle(symbols):
    syms = sorted(symbols)
    # Mach-O prepends '_' to both C and Itanium names; c++filt expects _Z, not __Z.
    proc = subprocess.run(["c++filt", "-n"], input="\n".join(s[1:] if s.startswith("_") else s
                                                            for s in syms) + "\n",
                          text=True, capture_output=True, check=True)
    vals = proc.stdout.splitlines()
    if len(vals) != len(syms):
        raise ValueError("c++filt changed the number of symbols")
    return dict(zip(syms, vals))


def extract(archive, listing, canonical_api):
    objs, occurrences = {}, {}
    for name, data in members(archive):
        occurrences[name] = occurrences.get(name, 0) + 1
        key = name if occurrences[name] == 1 else name + "#" + str(occurrences[name])
        objs[key] = Object(key, data)
    names = demangle({s["name"] for o in objs.values() for s in o.symbols})
    functions, symbol_ids, tables = {}, {}, []
    for obj in objs.values():
        if obj.text is None:
            continue
        for s in obj.symbols:
            if s["sec"] not in obj.code_sections or s["addr"] not in obj.entries or s["type"] & 0xE0:
                continue
            key = obj.name + ":" + hex(s["addr"])
            symbol_ids.setdefault(s["name"], []).append(key)
            if key in functions:
                continue
            i = bisect.bisect_right(obj.entries, s["addr"])
            sec = obj.sections[s["sec"] - 1]
            end = min(obj.entries[i], sec["addr"] + sec["size"]) if i < len(obj.entries) else sec["addr"] + sec["size"]
            functions[key] = dict(id=key, object=obj.name, address=s["addr"], size=end - s["addr"],
                                  code_section=sec["name"],
                                  name=names[s["name"]], mangled=s["name"], strings=[], apis=[],
                                  calls=[], floats=[], consts=[], switches=0, n=0, vslots=[], vtrefs=[])
    def owner(obj, address):
        i = bisect.bisect_right(obj.entries, address) - 1
        if i < 0:
            return None
        return functions.get(obj.name + ":" + hex(obj.entries[i]))

    # Call relocations and literal references, in instruction order.
    for obj in objs.values():
        if obj.text is None:
            continue
        for r in sorted((r for sec in obj.texts for r in obj.relocations(sec)), key=lambda r: r["place"]):
            f = owner(obj, r["place"])
            if not f:
                continue
            symbol, target = r["symbol"], r["target"]
            if symbol and symbol.startswith("__ZTV"):
                f["vtrefs"].append(names[symbol])
            if r["kind"] == 2:  # BRANCH (call or tail jump)
                if symbol:
                    f["calls"].append(symbol)
                    api = canonical_api(names[symbol])
                    if api:
                        f["apis"].append(api)
                elif target is not None:
                    g = functions.get(obj.name + ":" + hex(target))
                    if g and g["id"] != f["id"]:
                        f["calls"].append(g["mangled"])
                continue
            if target is None:
                continue
            raw, sec = obj.at(target, 1)
            if sec and sec["name"] == "__cstring":
                p = sec["off"] + target - sec["addr"]
                end = obj.data.find(b"\0", p, sec["off"] + sec["size"])
                if end >= 0:
                    f["strings"].append(obj.data[p:end].decode("latin-1"))
            elif sec and sec["name"] in ("__literal4", "__literal8", "__literal16"):
                width = 8 if sec["name"] == "__literal8" else 4
                raw, _ = obj.at(target, width)
                if raw:
                    f["floats"].append(struct.unpack("<d" if width == 8 else "<f", raw)[0])
        # Named pointer tables, including the project's explicit RD9 thunk vtables.
        for sec in obj.sections:
            if sec in obj.texts or not sec["nreloc"]:
                continue
            data_syms = sorted((s for s in obj.symbols if s["sec"] and
                                obj.sections[s["sec"] - 1] == sec and not s["type"] & 0xE0),
                               key=lambda s: (s["addr"], s["name"]))
            starts = [s["addr"] for s in data_syms]
            grouped = {}
            for r in sorted(obj.relocations(sec), key=lambda r: r["place"]):
                if r["kind"] != 0 or r["width"] != 8 or not r["symbol"]:
                    continue
                i = bisect.bisect_right(starts, r["place"]) - 1
                if i >= 0:
                    s = data_syms[i]
                    # Base methods live in the host archive, not this renderer
                    # archive. Preserve their typed undefined vtable references.
                    if r["symbol"] not in symbol_ids and not (
                            s["name"].startswith("__ZTV") and not r["symbol"].startswith("__ZTI")):
                        continue
                    grouped.setdefault(s["name"], []).append([int((r["place"] - s["addr"]) / 8),
                                                             r["symbol"]])
            for sym, slots in grouped.items():
                if len(slots) >= 2:
                    tables.append(dict(object=obj.name, name=names[sym], slots=slots))

    # LLVM text is used for instruction counts, non-relocated local calls, integer
    # constants and indirect jump shapes; symbolic relocation names above stay authoritative.
    obj, f, occurrences = None, None, {}
    header = re.compile(r"^.*\(([^()]*)\):\s+file format mach-o 64-bit x86-64$")
    insn = re.compile(r"^\s*([0-9a-f]+):\s+(\S+)(?:\s+(.*))?$")
    with listing.open(errors="replace") as stream:
        for line in stream:
            m = header.match(line.rstrip())
            if m:
                name = m[1]
                occurrences[name] = occurrences.get(name, 0) + 1
                key = name if occurrences[name] == 1 else name + "#" + str(occurrences[name])
                obj = objs[key]
                continue
            m = insn.match(line)
            if not m or obj is None:
                continue
            address, mnemonic, operands = int(m[1], 16), m[2], m[3] or ""
            if mnemonic.startswith("X86_64_RELOC_"):
                continue
            f = owner(obj, address)
            if not f:
                continue
            f["n"] += 1
            if mnemonic == "jmp" and "[" in operands:
                f["switches"] += 1
            if mnemonic in ("call", "jmp"):
                mtarget = re.match(r"0x([0-9a-f]+)\s+<", operands)
                if mtarget:
                    g = functions.get(obj.name + ":" + hex(int(mtarget[1], 16)))
                    # External branch placeholders point immediately after the call, not
                    # an entry, except coincidentally at function end. Require no relocation.
                    relplaces = getattr(obj, "branch_places", None)
                    if relplaces is None:
                        obj.branch_places = {r["place"] for sec in obj.texts for r in obj.relocations(sec)
                                             if r["kind"] == 2}
                    if g and g is not f and not any(address < p <= address + 4 for p in obj.branch_places):
                        f["calls"].append(g["mangled"])
            if "[" not in operands and mnemonic in ("mov", "cmp", "and", "or", "xor", "imul"):
                for h in re.findall(r"(?<![-\w])0x[0-9a-f]+", operands.split("##")[0]):
                    v = int(h, 16)
                    if 0x1000 <= v < 0xFFFFFFFF:
                        f["consts"].append(v)
    for f in functions.values():
        f["floats"] = [v for v in f["floats"] if math.isfinite(v)]
        for field in ("strings", "floats", "consts", "vtrefs"):
            f[field] = sorted(set(f[field]))
        f["call_names"] = [names[s] for s in f["calls"]]
    return dict(functions=functions, tables=tables, symbols=symbol_ids,
                externals={s: names[s] for s in sorted(names) if s not in symbol_ids},
                objects=len(objs), architecture="Mach-O x86_64")
