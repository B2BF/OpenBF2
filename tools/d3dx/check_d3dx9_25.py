#!/usr/bin/env python3
"""Checks the no-SDK D3DX declarations against the real d3dx9_25.dll (x86 and x64).

    python3 tools/d3dx/check_d3dx9_25.py --dir out/win64/d3dx [--retail "<BF2 dir>"] [-v]
    python3 tools/d3dx/check_d3dx9_25.py --dir out/win64/d3dx --pdb-urls

[CHOSEN, not recovered] Validation tool for the D3DX fallback (docs/building.md, "D3DX without the
DirectX SDK"). The rebuilt RendDX9.dll calls d3dx9_25.dll through Code/BF2/Platform/Gfx/D3DX9Subset.h,
so the declared COM method order must be the DLL's vtable order, and every name in
Code/RendDX9Abi/D3dx9_25.def must be a DLL export.

Inputs (never committed; copy them under out/):
  <dir>/x86/d3dx9_25.dll   32-bit DLL (Windows\\SysWOW64 or the game directory)
  <dir>/x64/d3dx9_25.dll   64-bit DLL (Windows\\System32, DirectX End-User Runtime)
  <dir>/<arch>/d3dx9_25.pdb  Microsoft's public symbols for each DLL; --pdb-urls prints the
                           symbol-server URL from the DLL's CodeView record.

Checks:
  1. Each D3DX9Subset.h and D3dx9Fallback.h interface against the vtable of its d3dx9_25 implementation class, slot by
     slot, by the method names in the public symbols. The linker folded identical stub bodies
     (identical-COMDAT folding), so a slot can carry another function's name; ID3DXBaseEffect slots
     are therefore accepted when either implementation (CEffect, CEffectCompiler) names them.
  2. ID3DXEffectStateManager and ID3DXInclude, which the engine implements and D3DX calls: the slot
     each CLoggableDevice setter and CPPInclude calls through (needs objdump; skipped without it).
  3. D3dx9_25.def names against both export tables; with --retail, the d3dx9_25 imports of the
     retail RendDX9.dll and TextureAtlasBuilder.dll as well.
Exit status 1 on any mismatch. Python 3 standard library only.
"""

import argparse
import os
import re
import shutil
import struct
import subprocess
import sys

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
SUBSET = os.path.join(REPO, "Code", "BF2", "Platform", "Gfx", "D3DX9Subset.h")
# Windows-only additions on top of the subset (interfaces only the no-SDK Windows build declares).
FALLBACK = os.path.join(REPO, "Code", "RendDX9Abi", "D3dx9Fallback.h")
DEF = os.path.join(REPO, "Code", "RendDX9Abi", "D3dx9_25.def")


class PE:
    """The few PE fields this tool reads: sections, exports, imports, CodeView record."""

    def __init__(self, path):
        with open(path, "rb") as f:
            self.data = d = f.read()
        pe = struct.unpack_from("<I", d, 0x3C)[0]
        nsec = struct.unpack_from("<H", d, pe + 6)[0]
        optsz = struct.unpack_from("<H", d, pe + 20)[0]
        opt = pe + 24
        self.pe32plus = struct.unpack_from("<H", d, opt)[0] == 0x20B
        self.ptr = 8 if self.pe32plus else 4
        self.base = struct.unpack_from("<Q" if self.pe32plus else "<I", d, opt + (24 if self.pe32plus else 28))[0]
        dd = opt + (112 if self.pe32plus else 96)
        self.dirs = [struct.unpack_from("<II", d, dd + 8 * i) for i in range(16)]
        self.sections = []
        for i in range(nsec):
            o = opt + optsz + 40 * i
            name = d[o:o + 8].rstrip(b"\0").decode()
            vsize, va, rsize, raw = struct.unpack_from("<IIII", d, o + 8)
            self.sections.append((name, va, max(vsize, rsize), raw))

    def off(self, rva):
        for _, va, size, raw in self.sections:
            if va <= rva < va + size:
                return rva - va + raw
        raise ValueError("rva 0x%x outside the image" % rva)

    def cstr(self, rva):
        o = self.off(rva)
        return self.data[o:self.data.index(b"\0", o)].decode("latin-1")

    def pointer(self, rva):
        return struct.unpack_from("<Q" if self.pe32plus else "<I", self.data, self.off(rva))[0]

    def code(self, rva):
        return any(n == ".text" and va <= rva < va + size for n, va, size, _ in self.sections)

    def exports(self):
        rva, _ = self.dirs[0]
        o = self.off(rva)
        base, nfun, nname, afun, aname, aord = struct.unpack_from("<IIIIII", self.data, o + 16)
        out = {}
        for i in range(nname):
            name = self.cstr(struct.unpack_from("<I", self.data, self.off(aname) + 4 * i)[0])
            out[name] = base + struct.unpack_from("<H", self.data, self.off(aord) + 2 * i)[0]
        return out

    def imports(self, dll):
        rva, _ = self.dirs[1]
        o = self.off(rva)
        while True:
            ilt, _, _, name, iat = struct.unpack_from("<IIIII", self.data, o)
            if not name:
                return []
            if self.cstr(name).lower() == dll.lower():
                names, k, flag = [], 0, 1 << (63 if self.pe32plus else 31)
                while True:
                    v = self.pointer((ilt or iat) + self.ptr * k)
                    if not v:
                        return names
                    names.append("#%d" % (v & 0xFFFF) if v & flag else self.cstr((v & 0x7FFFFFFF) + 2))
                    k += 1
            o += 20

    def codeview(self):
        rva, size = self.dirs[6]
        o = self.off(rva)
        for i in range(size // 28):
            typ, dsize, _, raw = struct.unpack_from("<IIII", self.data, o + 28 * i + 12)
            blob = self.data[raw:raw + dsize]
            if typ == 2 and blob[:4] == b"RSDS":
                d1, d2, d3 = struct.unpack_from("<IHH", blob, 4)
                guid = "%08X%04X%04X%s" % (d1, d2, d3, blob[12:20].hex().upper())
                age = struct.unpack_from("<I", blob, 20)[0]
                return blob[24:].split(b"\0")[0].decode(), "%s%X" % (guid, age)
        return None


def pdb_publics(path, pe):
    """S_PUB32 records of an MSF 7.00 PDB as {rva: [names]}."""
    with open(path, "rb") as f:
        d = f.read()
    if not d.startswith(b"Microsoft C/C++ MSF 7.00"):
        raise SystemExit("%s: not an MSF 7.00 PDB" % path)
    bs, _, _, dirbytes, _, blockmap = struct.unpack_from("<IIIIII", d, 32)
    nblk = (dirbytes + bs - 1) // bs
    blocks = struct.unpack_from("<%dI" % nblk, d, blockmap * bs)
    directory = b"".join(d[b * bs:(b + 1) * bs] for b in blocks)[:dirbytes]
    count = struct.unpack_from("<I", directory, 0)[0]
    sizes = struct.unpack_from("<%dI" % count, directory, 4)
    streams, o = [], 4 + 4 * count
    for size in sizes:
        if size == 0xFFFFFFFF:
            streams.append(b"")
            continue
        n = (size + bs - 1) // bs
        bl = struct.unpack_from("<%dI" % n, directory, o)
        o += 4 * n
        streams.append(b"".join(d[b * bs:(b + 1) * bs] for b in bl)[:size])
    symbols = streams[struct.unpack_from("<H", streams[3], 20)[0]]
    out, o = {}, 0
    while o + 4 <= len(symbols):
        length, kind = struct.unpack_from("<HH", symbols, o)
        if kind == 0x110E:  # S_PUB32
            _, offset, seg = struct.unpack_from("<IIH", symbols, o + 4)
            name = symbols[o + 14:symbols.index(b"\0", o + 14)].decode("latin-1")
            if 1 <= seg <= len(pe.sections):
                out.setdefault(pe.sections[seg - 1][1] + offset, []).append(name)
        o += length + 2
    return out


def method(symbol):
    m = re.match(r"\?(\w+)@", symbol)
    return m.group(1) if m else symbol


def vtable(pe, pubs, symbol):
    """Per slot, the method names of every public symbol at the slot's target."""
    va = [a for a, names in pubs.items() if symbol in names]
    if not va:
        raise SystemExit("vtable %s not in the public symbols" % symbol)
    va, slots = va[0], []
    while True:
        if slots and va + pe.ptr * len(slots) in pubs:
            return slots
        target = pe.pointer(va + pe.ptr * len(slots)) - pe.base
        if not pe.code(target):
            return slots
        slots.append({method(n) for n in pubs.get(target, ["?"])})


def declared(*headers):
    text = "".join(open(h).read() for h in headers)
    structs = {}
    for m in re.finditer(r"struct (ID3DX\w+)\s*(?::\s*(\w+))?\s*\{(.*?)\};", text, re.S):
        structs[m.group(1)] = (m.group(2), re.findall(r"BF2_D3DX_METHOD\(\s*[^,]+,\s*(\w+)", m.group(3)))

    def flat(name):
        if name == "IUnknown":
            return ["QueryInterface", "AddRef", "Release"]
        base, methods = structs.get(name, (None, []))
        return (flat(base) if base else []) + methods
    return {name: flat(name) for name in structs}


# Interface -> the d3dx9_25 classes whose vtables implement it (first: the class it is checked against).
IMPLEMENTATIONS = {
    "ID3DXLine": ["??_7CLine@D3DXCore@@6B@"],
    "ID3DXBuffer": ["??_7CBuffer@D3DXCore@@6B@"],
    "ID3DXEffectPool": ["??_7CEffectPool@D3DXEffects@@6B@"],
    "ID3DXBaseEffect": ["??_7CEffect@D3DXEffects@@6B@", "??_7CEffectCompiler@Compiler@D3DXEffects@@6B@"],
    "ID3DXEffect": ["??_7CEffect@D3DXEffects@@6B@"],
    "ID3DXEffectCompiler": ["??_7CEffectCompiler@Compiler@D3DXEffects@@6B@"],
    "ID3DXBaseMesh": ["??_7?$GXTri3Mesh@G$00$0PPPP@@D3DXMesh@@6B@", "??_7?$GXTri3Mesh@I$0A@$0PPPPPPPP@@D3DXMesh@@6B@"],
    "ID3DXMesh": ["??_7?$GXTri3Mesh@G$00$0PPPP@@D3DXMesh@@6B@", "??_7?$GXTri3Mesh@I$0A@$0PPPPPPPP@@D3DXMesh@@6B@"],
    "ID3DXRenderToSurface": ["??_7CRenderToSurface@D3DXCore@@6B@"],
    "ID3DXConstantTable": ["??_7CConstantTable@D3DXShader@@6B@"],
}


def check_vtables(images, decl, verbose):
    bad = 0
    base_len = {"ID3DXBaseEffect": len(decl.get("ID3DXBaseEffect", [])),
                "ID3DXBaseMesh": len(decl.get("ID3DXBaseMesh", []))}
    for iface, methods in decl.items():
        if iface in ("ID3DXInclude", "ID3DXEffectStateManager"):
            continue
        if iface not in IMPLEMENTATIONS:
            print("  %-22s no d3dx9_25 implementation class mapped - add one to IMPLEMENTATIONS" % iface)
            bad += 1
            continue
        tables = {arch: [vtable(pe, pubs, s) for s in IMPLEMENTATIONS[iface]] for arch, (pe, pubs) in images.items()}
        # Slots of the shared base interfaces may be checked against every implementation.
        shared = {"ID3DXEffect": "ID3DXBaseEffect", "ID3DXEffectCompiler": "ID3DXBaseEffect"}.get(iface)
        sizes = ", ".join("%s %d" % (a, len(t[0])) for a, t in tables.items())
        errors = []
        for i, name in enumerate(methods):
            pool = []
            for arch, ts in tables.items():
                pool += ts if not shared or i >= base_len[shared] else \
                    [vtable(images[arch][0], images[arch][1], s) for s in IMPLEMENTATIONS[shared]]
            if not any(i < len(t) and name in t[i] for t in pool):
                seen = sorted({n for t in pool if i < len(t) for n in t[i]})
                errors.append("    slot %2d %-30s dll: %s" % (i, name, ", ".join(seen) or "-"))
        longest = max(len(t[0]) for t in tables.values())
        status = "OK " if not errors else "BAD"
        print("  %s %-22s %2d declared slots; vtable %s" % (status, iface, len(methods), sizes))
        for e in errors:
            print(e)
        if verbose and longest > len(methods):
            first = next(iter(tables.values()))[0]
            print("      undeclared trailing slots:", ", ".join(sorted(first[j])[0] for j in range(len(methods), longest)))
        bad += len(errors)
    return bad


def objdump():
    for tool in ("llvm-objdump", "objdump"):
        path = shutil.which(tool)
        if path:
            return path
    return None


def call_slots(tool, path, pe, rva, length):
    """Offsets of the indirect `call [reg+N]` instructions in [rva, rva+length)."""
    start = pe.base + rva
    out = subprocess.run([tool, "-d", "--no-show-raw-insn", "-M", "intel", "--start-address=%d" % start,
                          "--stop-address=%d" % (start + length), path], capture_output=True, text=True).stdout
    return [int(m.group(1) or "0", 16) // pe.ptr
            for m in re.finditer(r"call\s+[dq]word ptr \[[a-z0-9]+(?: \+ 0x([0-9a-f]+))?\]", out)]


def check_callbacks(images, decl, verbose):
    tool = objdump()
    if not tool:
        print("  -- objdump not found: state-manager and include call slots not checked")
        return 0
    bad = 0
    sm = decl.get("ID3DXEffectStateManager", [])
    for arch, (pe, pubs) in images.items():
        path = pe.path
        by_name = {}
        for rva, names in pubs.items():
            for n in names:
                m = re.match(r"\?(\w+)@CLoggableDevice@D3DXEffects@@U[AE]", n)
                if m:
                    by_name.setdefault(m.group(1), rva)
        for slot, name in enumerate(sm):
            if slot < 3:
                continue
            calls = call_slots(tool, path, pe, by_name[name], 0x40) if name in by_name else []
            ok = calls[:1] == [slot]
            bad += not ok
            if verbose or not ok:
                print("  %s %s ID3DXEffectStateManager::%-26s slot %2d; CLoggableDevice calls slot %s"
                      % ("OK " if ok else "BAD", arch, name, slot, calls[:1] or "-"))
        include = decl.get("ID3DXInclude", [])
        found = {}
        # The destructor's Close call is inlined into the deleting destructor (??_G) on x64.
        for syms, want in ((("?InitializeFromFile@CPPInclude@D3DXShader@@",), "Open"),
                           (("??1CPPInclude@D3DXShader@@", "??_GCPPInclude@D3DXShader@@"), "Close")):
            calls = []
            for rva in [a for a, names in pubs.items() if any(n.startswith(syms) for n in names)]:
                calls += call_slots(tool, path, pe, rva, 0x300)
            found[want] = include.index(want) in calls if want in include else False
        ok = all(found.values())
        bad += not ok
        print("  %s %s ID3DXEffectStateManager %d slots, ID3DXInclude Open/Close %s" %
              ("OK " if ok else "BAD", arch, len(sm), "slots 0/1" if ok else found))
    return bad


def check_exports(images, retail):
    names = [l.split()[0] for l in open(DEF) if l.startswith("    ") and l.strip() and not l.strip().startswith(";")]
    bad = 0
    tables = {arch: pe.exports() for arch, (pe, _) in images.items()}
    for arch, table in tables.items():
        missing = [n for n in names if n not in table]
        bad += len(missing)
        print("  %s %s D3dx9_25.def: %d names, %s" % ("OK " if not missing else "BAD", arch, len(names),
                                                     "all exported" if not missing else "not exported: " + ", ".join(missing)))
    if len(tables) == 2:
        a, b = tables.values()
        moved = sorted(n for n in a if n in b and a[n] != b[n])
        print("     the architectures export %d / %d names; ordinals differ for: %s" % (len(a), len(b), ", ".join(moved) or "none"))
    if retail:
        union = set()
        for image in ("RendDX9.dll", "TextureAtlasBuilder.dll"):
            path = os.path.join(retail, image)
            if not os.path.exists(path):
                continue
            imports = PE(path).imports("d3dx9_25.dll")
            union.update(imports)
            unexported = sorted({n for t in tables.values() for n in imports if n not in t})
            bad += len(unexported)
            print("  %s retail %s imports %d d3dx9_25 names%s" % ("OK " if not unexported else "BAD", image,
                  len(imports), ", all exported in both" if not unexported else "; NOT EXPORTED: " + ", ".join(unexported)))
        extra = [n for n in names if n not in union]
        print("     .def names neither retail image imports: %s" % (", ".join(extra) or "none"))
    return bad


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--dir", required=True, help="directory holding x86/ and x64/ d3dx9_25.dll and .pdb")
    ap.add_argument("--retail", help="retail Battlefield 2 directory (RendDX9.dll, TextureAtlasBuilder.dll)")
    ap.add_argument("--pdb-urls", action="store_true", help="print the symbol-server URL of each DLL's PDB")
    ap.add_argument("-v", action="store_true", help="list every checked slot")
    args = ap.parse_args()
    images = {}
    for arch in ("x86", "x64"):
        dll = os.path.join(args.dir, arch, "d3dx9_25.dll")
        if not os.path.exists(dll):
            continue
        pe = PE(dll)
        pe.path = dll
        cv = pe.codeview()
        if args.pdb_urls:
            print("%s https://msdl.microsoft.com/download/symbols/%s/%s/%s" % (arch, cv[0], cv[1], cv[0]))
            continue
        pdb = os.path.join(args.dir, arch, "d3dx9_25.pdb")
        if not os.path.exists(pdb):
            raise SystemExit("%s missing; --pdb-urls prints where Microsoft's symbol server has it" % pdb)
        images[arch] = (pe, pdb_publics(pdb, pe))
    if args.pdb_urls:
        return 0
    if not images:
        raise SystemExit("no d3dx9_25.dll under %s/x86 or %s/x64" % (args.dir, args.dir))
    decl = declared(SUBSET, FALLBACK)
    print("COM interfaces of D3DX9Subset.h and D3dx9Fallback.h against d3dx9_25.dll (%s):" % ", ".join(images))
    bad = check_vtables(images, decl, args.v)
    print("Interfaces the engine implements, as d3dx9_25.dll calls them:")
    bad += check_callbacks(images, decl, args.v)
    print("Exports:")
    bad += check_exports(images, args.retail)
    print("result: %s" % ("OK" if not bad else "%d mismatch(es)" % bad))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
