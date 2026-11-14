"""Minimal PE32 reader for the rend-track evidence tools: sections, VA<->file offset, reads."""
import struct


class PE:
    def __init__(self, path):
        with open(path, 'rb') as f:
            self.data = f.read()
        d = self.data
        e = struct.unpack_from('<I', d, 0x3C)[0]
        nsec = struct.unpack_from('<H', d, e + 6)[0]
        optsz = struct.unpack_from('<H', d, e + 20)[0]
        opt = e + 24
        self.base = struct.unpack_from('<I', d, opt + 28)[0]
        self.ddirs = [struct.unpack_from('<II', d, opt + 96 + 8 * i) for i in range(16)]
        self.sections = []
        s = opt + optsz
        for i in range(nsec):
            name = d[s:s + 8].rstrip(b'\0').decode()
            vsize, va, rsize, raw = struct.unpack_from('<IIII', d, s + 8)
            self.sections.append((name, self.base + va, vsize, raw, rsize))
            s += 40

    def section(self, name):
        for sec in self.sections:
            if sec[0] == name:
                return sec
        return None

    def off(self, va):
        for name, sva, vsize, raw, rsize in self.sections:
            if sva <= va < sva + max(vsize, rsize):
                o = va - sva
                return raw + o if o < rsize else None
        return None

    def va(self, off):
        for name, sva, vsize, raw, rsize in self.sections:
            if raw <= off < raw + rsize:
                return sva + off - raw
        return None

    def u32(self, va):
        o = self.off(va)
        return None if o is None else struct.unpack_from('<I', self.data, o)[0]

    def cstr(self, va, maxlen=1024):
        o = self.off(va)
        if o is None:
            return None
        end = self.data.find(b'\0', o, o + maxlen)
        if end < 0:
            return None
        return self.data[o:end].decode('latin-1')

    def in_sec(self, va, name):
        s = self.section(name)
        return s is not None and s[1] <= va < s[1] + s[2]

    def imports(self):
        """Return {iat_va: 'dll!name'} from the import directory."""
        rva, size = self.ddirs[1]
        res = {}
        if not rva:
            return res
        p = self.base + rva
        while True:
            ilt, _, _, name_rva, iat = struct.unpack_from('<IIIII', self.data, self.off(p))
            if not name_rva:
                break
            dll = self.cstr(self.base + name_rva).lower()
            t = ilt or iat
            i = 0
            while True:
                ent = self.u32(self.base + t + 4 * i)
                if not ent:
                    break
                if ent & 0x80000000:
                    nm = '#%d' % (ent & 0xFFFF)
                else:
                    nm = self.cstr(self.base + ent + 2)
                res[self.base + iat + 4 * i] = dll + '!' + nm
                i += 1
            p += 20
        return res
