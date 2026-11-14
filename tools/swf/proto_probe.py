#!/usr/bin/env python3
"""Self-authored retail probe: a movie clip as an object's prototype.

The spec is docs/tracks/game/swf-proto-retail-probe.md. Everything here is generated from Python;
no game data goes into the SWF. Output is deterministic: the same arguments give the same bytes.

  python3 -I tools/swf/proto_probe.py swf --out OUT_DIR
      writes probe_all.swf (the nine tests) and probe1.swf .. probe9.swf (one test each)

  python3 -I tools/swf/proto_probe.py menuzip --bf2-mod PATH/mods/bf2 --out OUT_DIR [--test all|1|2|3|4]
      writes OUT_DIR/Menu_client.zip: a copy of PATH/mods/bf2/Menu_client.zip with
      External/FlashMenu/mainMenu.swf replaced by the probe; every other entry is copied unchanged

  python3 -I tools/swf/proto_probe.py mod --bf2-mod PATH/mods/bf2 --out OUT_DIR [--test ...]
      copies PATH/mods/bf2 to OUT_DIR/protoprobe and puts the probe Menu_client.zip into it; copy
      OUT_DIR/protoprobe into the retail copy's mods/ and start BF2.exe +modPath mods/protoprobe

The source mod is only read: the tool refuses an output directory inside it, and it never writes into
the source tree.
"""
import argparse
import shutil
import struct
import sys
import zipfile
from pathlib import Path

MENU_ENTRY = 'External/FlashMenu/mainMenu.swf'
TWIPS = 20


# ---- SWF primitives ---------------------------------------------------------------------------------

class Bits:
    def __init__(self):
        self.bits = []

    def ub(self, value, n):
        for i in range(n - 1, -1, -1):
            self.bits.append((value >> i) & 1)

    def sb(self, value, n):
        self.ub(value & ((1 << n) - 1), n)

    def bytes(self):
        out = bytearray()
        for i in range(0, len(self.bits), 8):
            chunk = self.bits[i:i + 8] + [0] * (8 - len(self.bits[i:i + 8]))
            out.append(int(''.join(map(str, chunk)), 2))
        return bytes(out)


def sbits(*values):
    """Bits needed to hold every signed value."""
    n = 1
    for v in values:
        while not (-(1 << (n - 1)) <= v < (1 << (n - 1))):
            n += 1
    return n


def rect(xmin, xmax, ymin, ymax):
    n = sbits(xmin, xmax, ymin, ymax)
    b = Bits()
    b.ub(n, 5)
    for v in (xmin, xmax, ymin, ymax):
        b.sb(v, n)
    return b.bytes()


def matrix(tx, ty):
    b = Bits()
    b.ub(0, 1)          # HasScale
    b.ub(0, 1)          # HasRotate
    n = sbits(tx, ty)
    b.ub(n, 5)
    b.sb(tx, n)
    b.sb(ty, n)
    return b.bytes()


def tag(code, body=b''):
    if len(body) < 63:
        return struct.pack('<H', (code << 6) | len(body)) + body
    return struct.pack('<HI', (code << 6) | 63, len(body)) + body


def cstr(s):
    return s.encode('latin-1') + b'\0'


def define_square(shape_id, rgb, size_px=60):
    w = size_px * TWIPS
    body = struct.pack('<H', shape_id) + rect(0, w, 0, w)
    body += bytes([1, 0x00]) + bytes(rgb)           # one solid fill
    body += bytes([0])                              # no line styles
    b = Bits()
    b.ub(1, 4)                                      # NumFillBits
    b.ub(0, 4)                                      # NumLineBits
    # style change: fill style 1, move to (0, 0)
    b.ub(0, 1); b.ub(0, 1); b.ub(0, 1); b.ub(1, 1); b.ub(0, 1); b.ub(1, 1)
    b.ub(2, 5); b.sb(0, 2); b.sb(0, 2)
    b.ub(1, 1)                                      # FillStyle1 = 1
    n = sbits(w, -w)
    for dx, dy in ((w, 0), (0, w), (-w, 0), (0, -w)):   # clockwise, y down: inside on the right
        b.ub(1, 1); b.ub(1, 1); b.ub(n - 2, 4); b.ub(1, 1)   # straight, general line
        b.sb(dx, n); b.sb(dy, n)
    b.ub(0, 1); b.ub(0, 5)                          # end of shape
    return tag(2, body + b.bytes())


def place(depth, char_id, x_px=0, y_px=0, name=None):
    flags = 0x02 | 0x04 | (0x20 if name else 0)
    body = bytes([flags]) + struct.pack('<HH', depth, char_id) + matrix(x_px * TWIPS, y_px * TWIPS)
    if name:
        body += cstr(name)
    return tag(26, body)


def define_sprite(sprite_id, tags):
    return tag(39, struct.pack('<HH', sprite_id, 1) + b''.join(tags) + tag(1) + tag(0))


# ---- a small AS1/AS2 bytecode assembler -------------------------------------------------------------

class Asm:
    """Actions with labels; If/Jump offsets resolve on assembly."""

    def __init__(self):
        self.items = []

    def raw(self, code, payload=b''):
        if code >= 0x80:
            self.items.append(bytes([code]) + struct.pack('<H', len(payload)) + payload)
        else:
            self.items.append(bytes([code]))
        return self

    def push(self, *values):
        payload = b''
        for v in values:
            if v is None:
                payload += b'\x02'                                   # null
            elif v is Undefined:
                payload += b'\x03'
            elif isinstance(v, bool):
                payload += b'\x05' + bytes([1 if v else 0])
            elif isinstance(v, int):
                payload += b'\x07' + struct.pack('<i', v)
            else:
                payload += b'\x00' + cstr(v)
        return self.raw(0x96, payload)

    def label(self, name):
        self.items.append(('label', name))
        return self

    def branch(self, code, name):
        self.items.append(('branch', code, name))
        return self

    def function(self, name, params, body):
        code = body.assemble()
        payload = cstr(name) + struct.pack('<H', len(params)) + b''.join(cstr(p) for p in params)
        payload += struct.pack('<H', len(code))
        self.raw(0x9B, payload)
        self.items.append(code)
        return self

    def assemble(self):
        def size(item):
            if isinstance(item, bytes):
                return len(item)
            return 5 if item[0] == 'branch' else 0
        positions, pos = {}, 0
        for item in self.items:
            if isinstance(item, tuple) and item[0] == 'label':
                positions[item[1]] = pos
            pos += size(item)
        out, pos = bytearray(), 0
        for item in self.items:
            if isinstance(item, bytes):
                out += item
            elif item[0] == 'branch':
                offset = positions[item[2]] - (pos + 5)
                out += bytes([item[1]]) + struct.pack('<Hh', 2, offset)
            pos += size(item)
        return bytes(out)

    # helpers
    def get(self, var):
        return self.push(var).raw(0x1C)                              # GetVariable

    def root_inc(self, var):
        # _root[var] = _root[var] + 1. Functions capture no environment (SwiffPlayer.dll 0x10094AEA),
        # so a bare name inside a getter/setter/constructor resolves in the CALLER's environment
        # (for a constructor: the attached clip). Every write from a function body names _root.
        return self.get('_root').push(var).get('_root').push(var).raw(0x4E).raw(0x50).raw(0x4F)

    def root_get(self, var):
        return self.get('_root').push(var).raw(0x4E)

    def root_set_from(self, var, local):
        return self.get('_root').push(var).get(local).raw(0x4F)

    def show(self, name, visible=True):
        return self.push('_root').raw(0x1C).push(name).raw(0x4E).push('_visible', visible).raw(0x4F)

    def if_not_equal_goto(self, value, label):
        """Stack top compared with value; jump when they differ."""
        return self.push(value).raw(0x49).raw(0x12).branch(0x9D, label)   # Equals2, Not, If

    def end_check(self, n):
        return (self.show('ok%d' % n).branch(0x99, 'end%d' % n)
                .label('bad%d' % n).show('bad%d' % n).label('end%d' % n))


class _Undefined:
    pass


Undefined = _Undefined()


def script(tests):
    """Two DoActions. Frame 1 sets up and attaches; frame 2 checks what an attached clip's load
    (class binding and constructor, run on the frame after attachMovie) produced, then stops."""
    a = Asm()          # frame 1
    b = Asm()          # frame 2
    for n in ('ok1', 'ok2', 'ok3', 'ok4', 'ok5', 'ok6', 'ok7', 'ok8', 'ok9',
              'bad1', 'bad2', 'bad3', 'bad4', 'bad5', 'bad6', 'bad7', 'bad8', 'bad9', 'done'):
        a.show(n, False)
    # var mc = _root.createEmptyMovieClip("protoClip", 100);
    a.push('mc', 100, 'protoClip', 2).get('_root').push('createEmptyMovieClip').raw(0x52).raw(0x1D)

    def new_object_with_proto(var):
        a.push(var, 0, 'Object').raw(0x40).raw(0x1D)                          # var = new Object()
        a.get(var).push('__proto__').get('mc').raw(0x4F)                      # var.__proto__ = mc

    if 1 in tests or 8 in tests:
        a.get('mc').push('plain', 42).raw(0x4F)
    if 1 in tests:
        new_object_with_proto('o1')
        a.push('r1').get('o1').push('plain').raw(0x4E).raw(0x1D)
        a.get('r1').if_not_equal_goto(42, 'bad1').end_check(1)
    if 2 in tests:
        a.push('getCalls', 0).raw(0x1D)
        getter = Asm().root_inc('getCalls').push(7).raw(0x3E)
        a.push(None).function('', [], getter).push('g', 3).get('mc').push('addProperty').raw(0x52).raw(0x17)
        new_object_with_proto('o2')
        a.push('r2').get('o2').push('g').raw(0x4E).raw(0x1D)
        a.get('r2').if_not_equal_goto(7, 'bad2').end_check(2)      # the getter's value proves it ran
    if 3 in tests:
        a.push('setValue', 0).raw(0x1D)
        setter = Asm().root_set_from('setValue', 'v')
        getter = Asm().root_get('setValue').raw(0x3E)
        a.function('', ['v'], setter).function('', [], getter).push('s', 3).get('mc').push('addProperty').raw(0x52).raw(0x17)
        new_object_with_proto('o3')
        a.get('o3').push('s', 5).raw(0x4F)
        a.get('setValue').if_not_equal_goto(5, 'bad3').end_check(3)
    if 4 in tests:
        a.push('ctorCalls', 0).raw(0x1D)
        ctor = Asm().root_inc('ctorCalls')
        a.function('ProbeClass', [], ctor)
        a.get('ProbeClass').push('prototype').get('mc').raw(0x4F)
        a.get('ProbeClass').push('probeSymbol', 2).get('Object').push('registerClass').raw(0x52).raw(0x17)
        a.push('inst', 200, 'probeInstance', 'probeSymbol', 3).get('_root').push('attachMovie').raw(0x52).raw(0x1D)
        b.get('inst').push(Undefined).raw(0x49).branch(0x9D, 'bad4')          # inst == undefined -> bad
        b.get('ctorCalls').if_not_equal_goto(1, 'bad4').end_check(4)
    if 5 in tests:
        # addProperty on a movie clip itself, read directly (no prototype): the native gameswf faults in
        # as_object_addproperty (null cast_to_as_object); retail 0x1009A0F0 writes through the clip's
        # secondary interface.
        a.push('c5', 101, 'directClip', 2).get('_root').push('createEmptyMovieClip').raw(0x52).raw(0x1D)
        a.push('calls5', 0).raw(0x1D)
        getter = Asm().root_inc('calls5').push(9).raw(0x3E)
        a.push(None).function('', [], getter).push('d', 3).get('c5').push('addProperty').raw(0x52).raw(0x17)
        a.push('r5').get('c5').push('d').raw(0x4E).raw(0x1D)
        a.get('r5').if_not_equal_goto(9, 'bad5').end_check(5)
    if 6 in tests:
        # Test 3's red, split: after the assignment through a movie-clip prototype with a setter, does
        # the value read back from the object (an own property was created)?
        a.push('set6', 0).raw(0x1D)
        setter = Asm().root_set_from('set6', 'v')
        getter = Asm().root_get('set6').raw(0x3E)
        a.function('', ['v'], setter).function('', [], getter).push('s6', 3).get('mc').push('addProperty').raw(0x52).raw(0x17)
        new_object_with_proto('o6')
        a.get('o6').push('s6', 5).raw(0x4F)
        a.push('r6').get('o6').push('s6').raw(0x4E).raw(0x1D)
        a.get('r6').if_not_equal_goto(5, 'bad6').end_check(6)
    if 7 in tests or 8 in tests:
        # Test 4's red, split: a class whose prototype is the clip, registered to an exported symbol.
        a.push('ctor7', 0).raw(0x1D)
        ctor = Asm().root_inc('ctor7')
        a.function('ProbeClass7', [], ctor)
        a.get('ProbeClass7').push('prototype').get('mc').raw(0x4F)
        a.get('ProbeClass7').push('probeSymbol', 2).get('Object').push('registerClass').raw(0x52).raw(0x17)
        a.push('inst7', 210, 'probeInstance7', 'probeSymbol', 3).get('_root').push('attachMovie').raw(0x52).raw(0x1D)
    if 7 in tests:
        # 7: the instance itself was created (checked at once, in frame 1).
        a.get('inst7').push(Undefined).raw(0x49).branch(0x9D, 'bad7').end_check(7)
    if 8 in tests:
        # 8: after its load (frame 2), the instance sees the clip's plain member through the class prototype.
        b.push('r8').get('inst7').push('plain').raw(0x4E).raw(0x1D)
        b.get('r8').if_not_equal_goto(42, 'bad8').end_check(8)
    if 9 in tests:
        # Control for 4/7/8: the same registerClass + attachMovie with an ORDINARY class (an explicit
        # new Object() prototype, where test 4 assigns the clip) on a second exported empty sprite.
        a.push('ctor9', 0).raw(0x1D)
        ctor = Asm().root_inc('ctor9')
        a.function('ProbeClass9', [], ctor)
        a.get('ProbeClass9').push('prototype', 0, 'Object').raw(0x40).raw(0x4F)   # an explicit object prototype
        a.get('ProbeClass9').push('probeSymbol9', 2).get('Object').push('registerClass').raw(0x52).raw(0x17)
        a.push('inst9', 220, 'probeInstance9', 'probeSymbol9', 3).get('_root').push('attachMovie').raw(0x52).raw(0x1D)
        b.get('inst9').push(Undefined).raw(0x49).branch(0x9D, 'bad9')
        b.get('ctor9').if_not_equal_goto(1, 'bad9').end_check(9)
    b.show('done', True)
    b.raw(0x07)                                                               # stop
    return a.assemble() + b'\0', b.assemble() + b'\0'


def build_swf(tests):
    green, red, blue = (0x20, 0xC0, 0x20), (0xD0, 0x20, 0x20), (0x20, 0x40, 0xD0)
    tags = [tag(9, bytes((255, 255, 255)))]                                   # white background
    tags += [define_square(1, green), define_square(2, red), define_square(3, blue)]
    sprite_of = {1: 11, 2: 12, 3: 13}
    for shape_id, sprite_id in sprite_of.items():
        tags.append(define_sprite(sprite_id, [place(1, shape_id)]))
    tags.append(define_sprite(20, []))                                        # probeSymbol, empty
    tags.append(define_sprite(21, []))                                        # probeSymbol9, empty
    tags.append(tag(56, struct.pack('<HH', 2, 20) + cstr('probeSymbol')       # ExportAssets
                    + struct.pack('<H', 21) + cstr('probeSymbol9')))
    depth = 1
    for i in range(5):
        tags.append(place(depth, 11, 20 + i * 120, 80, 'ok%d' % (i + 1))); depth += 1
        tags.append(place(depth, 12, 20 + i * 120, 180, 'bad%d' % (i + 1))); depth += 1
    for i in range(4):
        tags.append(place(depth, 11, 20 + i * 120, 280, 'ok%d' % (i + 6))); depth += 1
        tags.append(place(depth, 12, 20 + i * 120, 380, 'bad%d' % (i + 6))); depth += 1
    tags.append(place(depth, 13, 500, 330, 'done'))
    frame1, frame2 = script(tests)
    tags.append(tag(12, frame1))                                              # DoAction, frame 1
    tags.append(tag(1))                                                       # ShowFrame
    tags.append(tag(12, frame2))                                              # DoAction, frame 2
    tags.append(tag(1))                                                       # ShowFrame
    tags.append(tag(0))
    body = rect(0, 640 * TWIPS, 0, 480 * TWIPS) + struct.pack('<HH', 12 << 8, 2) + b''.join(tags)
    return b'FWS' + bytes([7]) + struct.pack('<I', len(body) + 8) + body


def tests_for(name):
    return {1, 2, 3, 4, 5, 6, 7, 8, 9} if name == 'all' else {int(name)}


# ---- commands --------------------------------------------------------------------------------------

def check_output(out, source):
    out = out.resolve()
    source = source.resolve()
    if out == source or source in out.parents:
        sys.exit('Refusing to write inside the source mod: %s' % out)
    return out


def write_menu_zip(source_mod, out_zip, probe):
    src = source_mod / 'Menu_client.zip'
    if not src.is_file():
        sys.exit('No Menu_client.zip in %s' % source_mod)
    replaced = False
    with zipfile.ZipFile(src) as zin, zipfile.ZipFile(out_zip, 'w') as zout:
        for info in zin.infolist():
            data = zin.read(info.filename)
            if info.filename.replace('\\', '/').lower() == MENU_ENTRY.lower():
                data = probe
                replaced = True
            copy = zipfile.ZipInfo(info.filename, date_time=info.date_time)
            copy.compress_type = info.compress_type
            copy.external_attr = info.external_attr
            zout.writestr(copy, data)
    if not replaced:
        sys.exit('%s not found in %s' % (MENU_ENTRY, src))


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('command', choices=('swf', 'menuzip', 'mod'))
    p.add_argument('--out', type=Path, required=True)
    p.add_argument('--bf2-mod', type=Path, help='read-only retail mods/bf2 (menuzip, mod)')
    p.add_argument('--test', default='all', choices=('all', '1', '2', '3', '4', '5', '6', '7', '8', '9'))
    args = p.parse_args()

    if args.command == 'swf':
        args.out.mkdir(parents=True, exist_ok=True)
        (args.out / 'probe_all.swf').write_bytes(build_swf({1, 2, 3, 4, 5, 6, 7, 8, 9}))
        for n in (1, 2, 3, 4, 5, 6, 7, 8, 9):
            (args.out / ('probe%d.swf' % n)).write_bytes(build_swf({n}))
        print('wrote probe_all.swf and probe1..9.swf to %s' % args.out)
        return
    if not args.bf2_mod:
        sys.exit('--bf2-mod is required for %s' % args.command)
    source = args.bf2_mod
    out = check_output(args.out, source)
    probe = build_swf(tests_for(args.test))
    if args.command == 'menuzip':
        out.mkdir(parents=True, exist_ok=True)
        write_menu_zip(source, out / 'Menu_client.zip', probe)
        print('wrote %s' % (out / 'Menu_client.zip'))
        return
    target = out / 'protoprobe'
    if target.exists():
        sys.exit('%s exists; choose an empty output directory' % target)
    out.mkdir(parents=True, exist_ok=True)
    shutil.copytree(source, target, ignore=shutil.ignore_patterns('Menu_client.zip'))
    write_menu_zip(source, target / 'Menu_client.zip', probe)
    print('wrote %s (start BF2.exe +modPath mods/protoprobe from a retail copy)' % target)


if __name__ == '__main__':
    main()
