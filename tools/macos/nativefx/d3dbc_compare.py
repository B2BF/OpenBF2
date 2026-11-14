#!/usr/bin/env python3
"""[CHOSEN, not recovered] Semantic comparison of two compilers' D3D9 bytecode for the same jobs.

  d3dbc_compare.py OLD_DIR NEW_DIR [--runs N] [--seed S] [--show K]

OLD_DIR and NEW_DIR hold <key>.bin files written by `nativefx memo-guard` with
MEMO_GUARD_BYTECODE_DIR set (the key hashes profile, entry and trimmed source, so equal keys are the
same job). Every key present in both is run through a small D3D9 shader interpreter (vs_1_1, vs_2_0,
ps_2_0 arithmetic, flow control, texld returning a fixed function of its coordinate) with N random
input sets (inputs, constants not set by def, int/bool constants). Outputs (oPos, oD#, oT#, oFog,
oPts, oC#, oDepth, texkill) must agree within 1e-4 relative wherever the old program's value is
finite: a component the old program computed from an uninitialized temp (NaN here) may differ.
Exit status 1 on any mismatch; jobs using an instruction the interpreter lacks are counted, not run.
"""
import argparse, math, random, struct, sys
from pathlib import Path

NAN = float("nan")


class Unsupported(Exception):
    pass


def frac(x):
    return x - math.floor(x) if math.isfinite(x) else NAN


def mul(a, b):
    # D3D9 shader multiplication: 0 times anything (also Inf or NaN) is 0, the rule pow() expansions
    # (log, mul, exp) rely on for pow(0, y).
    if a == 0 or b == 0:
        return 0.0
    try:
        return a * b
    except OverflowError:
        return math.inf


def parse(data):
    words = struct.unpack("<%dI" % (len(data) // 4), data[: len(data) // 4 * 4])
    version = words[0]
    pixel = (version >> 16) == 0xFFFF
    major, minor = (version >> 8) & 0xFF, version & 0xFF
    sm1_len = {0: 0, 1: 2, 2: 3, 3: 3, 4: 4, 5: 3, 6: 2, 7: 2, 8: 3, 9: 3, 10: 3, 11: 3, 12: 3, 13: 3, 14: 2, 15: 2,
               16: 2, 17: 3, 18: 4, 19: 2, 20: 3, 21: 3, 22: 3, 23: 3, 24: 3, 31: 2, 78: 2, 79: 2, 80: 4, 81: 5,
               88: 4, 89: 3}
    ins, i = [], 1
    while i < len(words):
        op = words[i]
        if op == 0xFFFF:
            break
        code = op & 0xFFFF
        if code == 0xFFFE:
            i += 1 + ((op >> 16) & 0x7FFF)
            continue
        if major >= 2:
            n = (op >> 24) & 0xF
        else:
            if code not in sm1_len:
                raise Unsupported("sm1 opcode %d" % code)
            n = sm1_len[code]
        ins.append((code, (op >> 16) & 0xFF, words[i + 1:i + 1 + n]))
        i += 1 + n
    return pixel, major, minor, ins


def reg_type(t):
    return ((t >> 28) & 7) | ((t >> 8) & 0x18)


class Machine:
    def __init__(self, pixel, major, rng):
        self.pixel, self.major = pixel, major
        self.r = {}
        self.v = {k: [rng.uniform(-2, 2) for _ in range(4)] for k in range(16)}
        self.t = {k: [rng.uniform(-2, 2) for _ in range(4)] for k in range(8)}
        self.c = {k: [rng.uniform(-2, 2) for _ in range(4)] for k in range(256)}
        self.i = {k: [rng.randint(1, 4), rng.randint(0, 2), rng.randint(1, 2), 0] for k in range(16)}
        self.b = {k: rng.random() < 0.5 for k in range(16)}
        self.a = [0, 0, 0, 0]
        self.aL = 0
        self.p = [False] * 4
        self.out = {}
        self.killed = False
        self.sampler_ids = {}

    def read_reg(self, t, extra):
        typ, idx = reg_type(t), t & 0x7FF
        if t & 0x2000:  # relative
            if self.major >= 2:
                rt = extra
                comp = (rt >> 16) & 3
                base = self.aL if reg_type(rt) == 15 else self.a[comp]
            else:
                base = self.a[0]
            idx = idx + int(base)
        if typ == 0:
            return self.r.get(idx, [NAN] * 4)
        if typ == 1:
            return self.v.get(idx, [0.0] * 4)
        if typ == 2:
            return self.c.get(idx, [0.0] * 4) if 0 <= idx < 256 else [0.0] * 4
        if typ == 3:
            return self.t.get(idx, [0.0] * 4) if self.pixel else list(self.a)
        if typ == 7:
            return [float(x) for x in self.i.get(idx, [0] * 4)]
        if typ == 14:
            return [1.0 if self.b.get(idx) else 0.0] * 4
        if typ == 15:
            return [float(self.aL)] * 4
        if typ == 13:
            return [0.0, 0.0, 0.0, 1.0]
        if typ == 10:  # sampler operand of texld (the sampler index is read from the token)
            return [0.0] * 4
        raise Unsupported("source register type %d" % typ)

    def src(self, t, extra=None):
        v = self.read_reg(t, extra)
        sw = (t >> 16) & 0xFF
        v = [v[(sw >> (2 * k)) & 3] for k in range(4)]
        mod = (t >> 24) & 0xF
        if mod == 0:
            return v
        if mod == 1:
            return [-x for x in v]
        if mod == 11:
            return [abs(x) for x in v]
        if mod == 12:
            return [-abs(x) for x in v]
        if mod == 6:
            return [1 - x for x in v]
        if mod == 7:
            return [2 * x for x in v]
        if mod == 8:
            return [-2 * x for x in v]
        if mod == 2:
            return [x - 0.5 for x in v]
        if mod == 3:
            return [0.5 - x for x in v]
        if mod == 4:
            return [2 * x - 1 for x in v]
        if mod == 5:
            return [1 - 2 * x for x in v]
        raise Unsupported("source modifier %d" % mod)

    def write(self, t, val):
        typ, idx, mask = reg_type(t), t & 0x7FF, (t >> 16) & 0xF
        dmod, shift = (t >> 20) & 0xF, (t >> 24) & 0xF
        if shift:
            s = shift - 16 if shift > 7 else shift
            val = [mul(x, 2.0 ** s) for x in val]
        if dmod & 1:
            val = [min(max(x, 0.0), 1.0) if x == x else 0.0 for x in val]
        if typ == 0:
            dst = self.r.setdefault(idx, [NAN] * 4)
        elif typ == 3 and not self.pixel:
            dst = self.a
            val = [float(math.floor(x + 0.5)) if math.isfinite(x) else 0.0 for x in val] if self.major >= 2 else \
                  [float(math.floor(x)) if math.isfinite(x) else 0.0 for x in val]
        elif typ in (4, 5, 6, 8, 9):
            dst = self.out.setdefault((typ, idx), [None] * 4)
        else:
            raise Unsupported("destination register type %d" % typ)
        for k in range(4):
            if mask & (1 << k):
                dst[k] = val[k]


def dot(a, b, n):
    s = 0.0
    for k in range(n):
        s += mul(a[k], b[k])
    return s


def tex_sample(sampler, c, dim=2):
    u, v, w = (x if math.isfinite(x) else 0.0 for x in c[:3])
    if dim == 2:  # a 2D texture reads .xy only
        w = 0.0
    return [frac(u * 1.7 + 0.3 * sampler), frac(v * 2.3 + 0.1), frac((u + v + w) * 0.7), 0.5 + 0.25 * math.sin(u)]


def run(prog, rng, preset=None):
    pixel, major, minor, ins = prog
    m = Machine(pixel, major, rng)
    if preset:
        preset(m, prog)
    # Defs first (they apply wherever they appear).
    for code, ctrl, p in ins:
        if code == 81:
            m.c[p[0] & 0x7FF] = [struct.unpack("<f", struct.pack("<I", w))[0] for w in p[1:5]]
        elif code == 48:
            m.i[p[0] & 0x7FF] = [struct.unpack("<i", struct.pack("<I", w))[0] for w in p[1:5]]
        elif code == 47:
            m.b[p[0] & 0x7FF] = bool(p[1])
    sampler_dims = {p[1] & 0x7FF: (p[0] >> 27) & 0xF for code, ctrl, p in ins
                    if code == 31 and len(p) >= 2 and reg_type(p[1]) == 10}
    # Matching control flow.
    stack, else_of, end_of = [], {}, {}
    for n, (code, ctrl, p) in enumerate(ins):
        if code in (38, 27, 40, 41):
            stack.append(n)
        elif code == 42:
            else_of[stack[-1]] = n
        elif code in (39, 29, 43):
            open_ = stack.pop()
            end_of[open_] = n
            if open_ in else_of:
                end_of[else_of[open_]] = n
    loops = []  # (start index, remaining, aL step)
    pc, steps = 0, 0
    while pc < len(ins):
        steps += 1
        if steps > 200000:
            raise Unsupported("runaway")
        code, ctrl, p = ins[pc]
        pc += 1
        if code in (0, 31, 81, 47, 48, 0xFFFD):
            continue
        ops = list(p)
        # Source operands (with SM2 relative-address tokens).
        def sources(start):
            out, k = [], start
            while k < len(ops):
                t = ops[k]
                extra = None
                if major >= 2 and (t & 0x2000):
                    extra = ops[k + 1]
                    k += 1
                out.append(m.src(t, extra))
                k += 1
            return out
        if code == 38:  # rep
            n = int(sources(0)[0][0])
            n = max(0, min(n, 255))
            if n == 0:
                pc = end_of[pc - 1] + 1
            else:
                loops.append([pc - 1, n, 0])
            continue
        if code == 27:  # loop aL, i#
            iv = m.read_reg(ops[1], None)
            n, start, step = int(iv[0]), int(iv[1]), int(iv[2])
            n = max(0, min(n, 255))
            if n == 0:
                pc = end_of[pc - 1] + 1
            else:
                m.aL = start
                loops.append([pc - 1, n, step])
            continue
        if code in (39, 29):
            lp = loops[-1]
            lp[1] -= 1
            m.aL += lp[2]
            if lp[1] > 0:
                pc = lp[0] + 1
            else:
                loops.pop()
            continue
        if code in (44, 45):
            take = True
            if code == 45:
                a, b = sources(0)
                take = compare(ctrl, a[0], b[0])
            if take:
                lp = loops.pop()
                pc = end_of[lp[0]] + 1
            continue
        if code in (40, 41):
            if code == 40:
                cond = sources(0)[0][0] != 0.0
            else:
                a, b = sources(0)
                cond = compare(ctrl, a[0], b[0])
            if not cond:
                pc = else_of.get(pc - 1, end_of[pc - 1]) + 1
            continue
        if code == 42:  # else reached from the then-block: skip to endif
            pc = end_of[pc - 1] + 1
            continue
        if code == 43:
            continue
        if code == 65:  # texkill
            v = m.read_reg(ops[0], None)
            mask = (ops[0] >> 16) & 0xF
            if any((mask & (1 << q)) and v[q] < 0 for q in range(4)):
                m.killed = True
            continue
        dst = ops[0]
        s = sources(1)
        if code == 1 or code == 46:
            val = s[0]
        elif code == 2:
            val = [a + b for a, b in zip(s[0], s[1])]
        elif code == 3:
            val = [a - b for a, b in zip(s[0], s[1])]
        elif code == 4:
            val = [mul(a, b) + c for a, b, c in zip(*s)]
        elif code == 5:
            val = [mul(a, b) for a, b in zip(s[0], s[1])]
        elif code in (6, 7, 14, 15, 78, 79):
            x = s[0][3]  # replicate swizzle (or .w when unswizzled)
            if code == 6:
                r = math.inf if x == 0 else 1.0 / x
            elif code == 7:
                r = math.inf if x == 0 else (1.0 / math.sqrt(abs(x)) if x == x else NAN)
            elif code in (14, 78):
                r = 2.0 ** x if x < 1000 else math.inf
            else:
                r = -math.inf if x == 0 else math.log2(abs(x)) if math.isfinite(x) else (math.inf if x == x else NAN)
            val = [r] * 4
            if not pixel and major == 1 and code == 78 and math.isfinite(x):
                # vs_1_x expp: x = 2^floor(s), y = s - floor(s), z = 2^s, w = 1 (the frc idiom uses .y).
                fl = math.floor(x)
                val = [2.0 ** fl if fl < 1000 else math.inf, x - fl, r, 1.0]
            elif not pixel and major == 1 and code == 79 and math.isfinite(x) and x != 0:
                # vs_1_x logp: x = exponent, y = mantissa in [1, 2), z = log2|s|, w = 1.
                e = math.floor(math.log2(abs(x)))
                val = [float(e), abs(x) / 2.0 ** e, r, 1.0]
        elif code == 8:
            val = [dot(s[0], s[1], 3)] * 4
        elif code == 9:
            val = [dot(s[0], s[1], 4)] * 4
        elif code == 10:
            val = [min(a, b) if a == a and b == b else NAN for a, b in zip(s[0], s[1])]
        elif code == 11:
            val = [max(a, b) if a == a and b == b else NAN for a, b in zip(s[0], s[1])]
        elif code == 12:
            val = [1.0 if a < b else 0.0 for a, b in zip(s[0], s[1])]
        elif code == 13:
            val = [1.0 if a >= b else 0.0 for a, b in zip(s[0], s[1])]
        elif code == 16:
            x, y, w = s[0][0], s[0][1], s[0][3]
            pw = min(max(w, -128.0), 128.0)
            val = [1.0, max(x, 0.0), (max(y, 0.0) ** pw if max(y, 0.0) > 0 else 0.0) if x > 0 else 0.0, 1.0]
        elif code == 17:
            val = [1.0, mul(s[0][1], s[1][1]), s[0][2], s[1][3]]
        elif code == 18:
            val = [mul(a, b - c) + c for a, b, c in zip(*s)]
        elif code == 19:
            val = [frac(x) for x in s[0]]
        elif code in (20, 21, 22, 23, 24):
            cols = 4 if code <= 21 else 3
            rows = {20: 4, 21: 3, 22: 4, 23: 3, 24: 2}[code]
            base = ops[2]
            val = [0.0] * 4
            for row in range(rows):
                rv = m.src(base + row, None)
                val[row] = dot(s[0], rv, cols)
        elif code == 32:
            a, b = s[0][3], s[1][3]
            try:
                r = abs(a) ** b
            except (OverflowError, ZeroDivisionError):
                r = math.inf
            val = [r] * 4
        elif code == 33:
            a, b = s[0], s[1]
            val = [a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0], 0.0]
        elif code == 34:
            val = [(1.0 if x > 0 else -1.0 if x < 0 else 0.0) for x in s[0]]
        elif code == 35:
            val = [abs(x) for x in s[0]]
        elif code == 36:
            d = dot(s[0], s[0], 3)
            f = math.inf if d == 0 else 1.0 / math.sqrt(d) if d == d else NAN
            val = [mul(x, f) for x in s[0]]
        elif code == 37:
            x = s[0][3]
            val = [math.cos(x), math.sin(x), 0.0, 0.0] if math.isfinite(x) else [NAN] * 4
        elif code == 80:
            val = [b if a > 0.5 else c for a, b, c in zip(*s)]
        elif code == 88:
            val = [b if a >= 0 else c for a, b, c in zip(*s)]
        elif code == 90:
            val = [mul(s[0][0], s[1][0]) + mul(s[0][1], s[1][1]) + s[2][3]] * 4
        elif code == 66 and major >= 2:
            sampler = m.sampler_ids.get(ops[2] & 0x7FF, ops[2] & 0x7FF)
            dim = sampler_dims.get(ops[2] & 0x7FF, 2)
            coord = s[0]
            if ctrl == 1 and coord[3] not in (0.0,):
                coord = [x / coord[3] for x in coord]
            val = tex_sample(sampler, coord, dim)
        else:
            raise Unsupported("opcode %d" % code)
        m.write(dst, val)
    return m


def ctab(data):
    """Constant table of a D3D9 shader: [(name, regset, index, count, type)] (D3DXRS_*, D3DXPT_*)."""
    words = struct.unpack("<%dI" % (len(data) // 4), data[: len(data) // 4 * 4])
    i = 1
    while i < len(words):
        op = words[i]
        if (op & 0xFFFF) != 0xFFFE:
            break
        n = (op >> 16) & 0x7FFF
        if n and words[i + 1] == 0x42415443:
            blob = data[(i + 2) * 4:(i + 1 + n) * 4]
            _, _, _, count, info = struct.unpack_from("<5I", blob, 0)
            out = []
            for k in range(count):
                name_off, regset, index, regcount, _, type_off, _ = struct.unpack_from("<IHHHHII", blob, info + 20 * k)
                name = blob[name_off:blob.index(b"\0", name_off)].decode("latin-1")
                ptype = struct.unpack_from("<HH", blob, type_off)[1] if type_off else 3
                out.append((name, regset, index, regcount, ptype))
            return out
        i += 1 + n
    return []


def input_registers(prog):
    """{(usage, usage_index): v#} for vertex shaders, {('t', n): n} implied for pixel shaders."""
    pixel, major, minor, ins = prog
    regs = {}
    for code, ctrl, p in ins:
        if code == 31 and len(p) >= 2 and reg_type(p[1]) == 1:
            regs[(p[0] & 0x1F, (p[0] >> 16) & 0xF)] = p[1] & 0x7FF
    return regs


def named_preset(data, seed):
    """Fills uniforms by name (CTAB) and vertex inputs by usage, so two compilers' programs for the same
    source see the same values whatever registers they chose."""
    table = ctab(data)

    def value(key, ptype):
        r = random.Random("%d|%s" % (seed, key))
        if ptype in (1, 2):
            return [float(r.randint(0, 5)), 0.0, 1.0, 0.0]
        return [r.uniform(-2, 2) for _ in range(4)]

    def preset(m, prog):
        for name, regset, index, count, ptype in table:
            for k in range(count):
                v = value("%s|%d" % (name, k), ptype)
                if regset == 2:
                    m.c[index + k] = v
                elif regset == 1:
                    m.i[index + k] = [int(v[0]), 0, 1, 0]
                elif regset == 0:
                    m.b[index + k] = v[0] != 0.0
                elif regset == 3:
                    m.sampler_ids[index + k] = random.Random("%d|s|%s|%d" % (seed, name, k)).randint(0, 7)
        if m.pixel:  # colour interpolators (v#) arrive clamped to [0, 1]
            for k in m.v:
                m.v[k] = [min(max(x, 0.0), 1.0) for x in m.v[k]]
        if not m.pixel:
            for (usage, usage_index), reg in input_registers(prog).items():
                r = random.Random("%d|v|%d|%d" % (seed, usage, usage_index))
                if usage == 2:  # BLENDINDICES as D3DCOLOR bytes: small palette indices
                    m.v[reg] = [r.randint(0, 3) / 255.0 for _ in range(4)]
                elif usage in (1, 10):  # BLENDWEIGHT, COLOR: unit range
                    m.v[reg] = [r.random() for _ in range(4)]
                else:
                    m.v[reg] = value("v|%d|%d" % (usage, usage_index), 3)
    return preset


def semantic_outputs(prog, m):
    """Outputs keyed like vs_1_x/2_x registers: vs_3_0 o# by their dcl semantic (position ->
    oPos, color n -> oDn, texcoord n -> oTn, fog -> oFog, psize -> oPts); vs_1_x/2_x/3_0 colours
    clamped to [0, 1] as the rasterizer receives them below vs_3_0 (and as vs_3_0 COLOR outputs are
    clamped for a vs_1_1 request kept at its reported version)."""
    pixel, major, minor, ins = prog
    if pixel:
        return dict(m.out)
    out = {}
    if major >= 3:
        sem = {}
        for code, ctrl, p in ins:
            if code == 31 and len(p) >= 2 and reg_type(p[1]) == 6:
                usage, index = p[0] & 0x1F, (p[0] >> 16) & 0xF
                sem[p[1] & 0x7FF] = {0: (4, 0), 10: (5, index), 5: (6, index), 11: (4, 1), 4: (4, 2)}.get(usage, (99, usage * 16 + index))
        for (typ, idx), vals in m.out.items():
            out[sem.get(idx, (98, idx)) if typ == 6 else (typ, idx)] = vals
    else:
        out = dict(m.out)
    for key in out:
        if key in ((4, 1), (4, 2)):  # oFog and oPts are scalars: only .x reaches the rasterizer
            out[key] = out[key][:1] + [None] * 3
        if key[0] == 5:
            out[key] = [None if v is None else (min(max(v, 0.0), 1.0) if v == v else v) for v in out[key]]
    return out


def compare_named(theirs, ours, runs=8, seed=1):
    """Runs the reference (theirs) and our program with name-mapped inputs; returns None or the first
    difference in an output the reference computes finitely."""
    pt, po = parse(theirs), parse(ours)
    for run_index in range(runs):
        mt = run(pt, random.Random(seed * 1000 + run_index), named_preset(theirs, seed * 1000 + run_index))
        mo = run(po, random.Random(seed * 1000 + run_index), named_preset(ours, seed * 1000 + run_index))
        if mt.killed != mo.killed:
            return "texkill differs (run %d)" % run_index
        ot, oo = semantic_outputs(pt, mt), semantic_outputs(po, mo)
        for reg, vals in ot.items():
            nv = oo.get(reg, [None] * 4)
            for q in range(4):
                if vals[q] is not None and not close(vals[q], nv[q]):
                    return "output %s.%s: reference %r ours %r (run %d)" % (reg, "xyzw"[q], vals[q], nv[q], run_index)
    return None


def compare(ctrl, a, b):
    return {1: a > b, 2: a == b, 3: a >= b, 4: a < b, 5: a != b, 6: a <= b}.get(ctrl, False)


def close(a, b):
    if not math.isfinite(a):
        return True
    if b is None or not math.isfinite(b):
        return False
    return abs(a - b) <= 1e-4 * (1 + abs(a))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("old")
    ap.add_argument("new")
    ap.add_argument("--runs", type=int, default=4)
    ap.add_argument("--seed", type=int, default=1)
    ap.add_argument("--show", type=int, default=5)
    a = ap.parse_args()
    old, new = Path(a.old), Path(a.new)
    keys = sorted({p.name for p in old.glob("*.bin") if p.stat().st_size} & {p.name for p in new.glob("*.bin") if p.stat().st_size})
    same = compared = mismatched = unsupported = 0
    reasons = {}
    for key in keys:
        bo, bn = (old / key).read_bytes(), (new / key).read_bytes()
        if bo == bn:
            same += 1
            continue
        try:
            po, pn = parse(bo), parse(bn)
            bad = None
            for run_index in range(a.runs):
                mo = run(po, random.Random(a.seed * 1000 + run_index))
                mn = run(pn, random.Random(a.seed * 1000 + run_index))
                if mo.killed != mn.killed:
                    bad = "texkill differs"
                for reg, vals in mo.out.items():
                    nv = mn.out.get(reg, [None] * 4)
                    for q in range(4):
                        if vals[q] is not None and not close(vals[q], nv[q]):
                            bad = bad or "output %s.%s: old %r new %r (run %d)" % (reg, "xyzw"[q], vals[q], nv[q], run_index)
                if bad:
                    break
        except Unsupported as e:
            unsupported += 1
            reasons[str(e)] = reasons.get(str(e), 0) + 1
            continue
        compared += 1
        if bad:
            mismatched += 1
            if mismatched <= a.show:
                print("MISMATCH", key, bad)
    print("d3dbc-compare: %d common jobs, %d identical bytecode, %d compared, %d mismatched, %d not interpretable%s"
          % (len(keys), same, compared, mismatched, unsupported,
             (" (" + ", ".join("%s: %d" % kv for kv in sorted(reasons.items())) + ")") if reasons else ""))
    return 1 if mismatched else 0


if __name__ == "__main__":
    sys.exit(main())
