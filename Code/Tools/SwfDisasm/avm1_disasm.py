#!/usr/bin/env python
"""Linear AVM1 (ActionScript 1/2) disassembler for Battlefield 2 Flash menu movies.

The BF2 menus (for example External/FlashMenu/mainMenu.swf inside a mod's
menu_client.zip) call the engine through dice.bf2.* methods. This tool lists the
action stream of every DoAction, DoInitAction and nested DefineSprite tag, with
ConstantPool references resolved, so the arguments the movie passes to the
engine can be read directly.

Limitations: actions attached to buttons (DefineButton2) and clip events
(PlaceObject2/3) are not decoded. Disassembly is linear; function bodies appear
in place after their DefineFunction/DefineFunction2 line. Branch targets are not
resolved.

Usage:
    python avm1_disasm.py <movie.swf | menu.zip> [-o OUT] [--member PATH]

When given a .zip, every .swf member is disassembled (or only --member). Output
lines have the form "<tag path> <action> <operands>", where the tag path is
/DoAction, /Init<sprite id> or /S<sprite id>/DoAction. Works with Python 2.7
and Python 3.
"""

from __future__ import print_function

import argparse
import struct
import sys
import zipfile
import zlib

ACTION_NAMES = {
    0x04: 'NextFrame', 0x05: 'PrevFrame', 0x06: 'Play', 0x07: 'Stop',
    0x0A: 'Add', 0x0B: 'Subtract', 0x0C: 'Multiply', 0x0D: 'Divide',
    0x0E: 'Equals', 0x0F: 'Less', 0x10: 'And', 0x11: 'Or', 0x12: 'Not',
    0x13: 'StringEquals', 0x14: 'StringLength', 0x15: 'StringExtract',
    0x17: 'Pop', 0x18: 'ToInteger', 0x1C: 'GetVariable', 0x1D: 'SetVariable',
    0x20: 'SetTarget2', 0x21: 'StringAdd', 0x22: 'GetProperty', 0x23: 'SetProperty',
    0x24: 'CloneSprite', 0x25: 'RemoveSprite', 0x26: 'Trace', 0x27: 'StartDrag',
    0x28: 'EndDrag', 0x29: 'StringLess', 0x2A: 'Throw', 0x2B: 'CastOp',
    0x2C: 'ImplementsOp', 0x30: 'RandomNumber', 0x34: 'GetTime',
    0x3A: 'Delete', 0x3B: 'Delete2', 0x3C: 'DefineLocal', 0x3D: 'CallFunction',
    0x3E: 'Return', 0x3F: 'Modulo', 0x40: 'NewObject', 0x41: 'DefineLocal2',
    0x42: 'InitArray', 0x43: 'InitObject', 0x44: 'TypeOf', 0x45: 'TargetPath',
    0x46: 'Enumerate', 0x47: 'Add2', 0x48: 'Less2', 0x49: 'Equals2',
    0x4A: 'ToNumber', 0x4B: 'ToString', 0x4C: 'PushDuplicate', 0x4D: 'StackSwap',
    0x4E: 'GetMember', 0x4F: 'SetMember', 0x50: 'Increment', 0x51: 'Decrement',
    0x52: 'CallMethod', 0x53: 'NewMethod', 0x54: 'InstanceOf', 0x55: 'Enumerate2',
    0x60: 'BitAnd', 0x61: 'BitOr', 0x62: 'BitXor', 0x63: 'BitLShift',
    0x64: 'BitRShift', 0x65: 'BitURShift', 0x66: 'StrictEquals', 0x67: 'Greater',
    0x68: 'StringGreater', 0x69: 'Extends',
    0x81: 'GotoFrame', 0x83: 'GetURL', 0x87: 'StoreRegister', 0x88: 'ConstantPool',
    0x8A: 'WaitForFrame', 0x8B: 'SetTarget', 0x8C: 'GoToLabel', 0x8D: 'WaitForFrame2',
    0x8E: 'DefineFunction2', 0x8F: 'Try', 0x94: 'With', 0x96: 'Push', 0x99: 'Jump',
    0x9A: 'GetURL2', 0x9B: 'DefineFunction', 0x9D: 'If', 0x9E: 'Call',
    0x9F: 'GotoFrame2',
}

TAG_END, TAG_SHOW_FRAME, TAG_DO_ACTION = 0, 1, 12
TAG_DEFINE_SPRITE, TAG_FRAME_LABEL, TAG_DO_INIT_ACTION = 39, 43, 59


def _cstr(data, start):
    """Returns (text, index after the terminating NUL) for a NUL-terminated string."""
    end = data.index(b'\0', start)
    return data[start:end].decode('latin-1'), end + 1


def _push_operands(body, pool):
    values = []
    j = 0
    while j < len(body):
        kind = body[j]
        j += 1
        if kind == 0:
            text, j = _cstr(body, j)
            values.append(repr(str(text)))
        elif kind == 1:
            values.append(repr(struct.unpack('<f', bytes(body[j:j + 4]))[0]))
            j += 4
        elif kind == 2:
            values.append('null')
        elif kind == 3:
            values.append('undefined')
        elif kind == 4:
            values.append('r%d' % body[j])
            j += 1
        elif kind == 5:
            values.append('true' if body[j] else 'false')
            j += 1
        elif kind == 6:
            raw = bytes(body[j:j + 8])
            values.append(repr(struct.unpack('<d', raw[4:] + raw[:4])[0]))
            j += 8
        elif kind == 7:
            values.append(str(struct.unpack('<i', bytes(body[j:j + 4]))[0]))
            j += 4
        elif kind in (8, 9):
            if kind == 8:
                index = body[j]
                j += 1
            else:
                index = struct.unpack('<H', bytes(body[j:j + 2]))[0]
                j += 2
            values.append(repr(str(pool[index])) if index < len(pool) else 'c%d' % index)
        else:
            values.append('<unknown push type %d>' % kind)
            break
    return values


def disassemble_actions(data, tag_path, out):
    """Appends one line per action in an action record stream."""
    pool = []
    i = 0
    while i < len(data):
        code = data[i]
        i += 1
        if code == 0:
            out.append('%s End' % tag_path)
            continue
        length = 0
        if code >= 0x80:
            length = struct.unpack('<H', bytes(data[i:i + 2]))[0]
            i += 2
        body = data[i:i + length]
        i += length
        name = ACTION_NAMES.get(code, 'Action%02X' % code)
        if code == 0x88:
            count = struct.unpack('<H', bytes(body[:2]))[0]
            pool = [s.decode('latin-1') for s in bytes(body[2:]).split(b'\0')[:count]]
            out.append('%s ConstantPool(%d)' % (tag_path, count))
        elif code == 0x96:
            out.append('%s Push %s' % (tag_path, ', '.join(_push_operands(body, pool))))
        elif code in (0x8E, 0x9B):
            function_name, _ = _cstr(body, 0)
            out.append('%s %s %r' % (tag_path, name, str(function_name)))
        elif code in (0x99, 0x9D):
            offset = struct.unpack('<h', bytes(body[:2]))[0]
            out.append('%s %s %+d' % (tag_path, name, offset))
        elif code == 0x87:
            out.append('%s StoreRegister r%d' % (tag_path, body[0]))
        else:
            out.append('%s %s' % (tag_path, name))


def walk_tags(data, tag_path, out):
    """Disassembles the action tags in a tag stream, recursing into sprites.

    Each timeline frame is introduced by a "<path> Frame <n>" line, followed by
    its label when the frame has one, so frame actions can be tied to the
    gotoAndStop/gotoAndPlay targets that reach them.
    """
    frame = 1
    out.append('%s Frame %d' % (tag_path or '/', frame))
    i = 0
    while i + 2 <= len(data):
        header = struct.unpack('<H', bytes(data[i:i + 2]))[0]
        i += 2
        code, length = header >> 6, header & 0x3F
        if length == 0x3F:
            length = struct.unpack('<I', bytes(data[i:i + 4]))[0]
            i += 4
        body = data[i:i + length]
        i += length
        if code == TAG_END:
            break
        if code == TAG_SHOW_FRAME:
            frame += 1
            if i + 2 <= len(data):
                out.append('%s Frame %d' % (tag_path or '/', frame))
        elif code == TAG_FRAME_LABEL:
            label, _ = _cstr(body, 0)
            out.append('%s FrameLabel %r' % (tag_path or '/', str(label)))
        elif code == TAG_DO_ACTION:
            disassemble_actions(body, tag_path + '/DoAction', out)
        elif code == TAG_DO_INIT_ACTION:
            sprite = struct.unpack('<H', bytes(body[:2]))[0]
            disassemble_actions(body[2:], '%s/Init%d' % (tag_path, sprite), out)
        elif code == TAG_DEFINE_SPRITE:
            sprite = struct.unpack('<H', bytes(body[:2]))[0]
            walk_tags(body[4:], '%s/S%d' % (tag_path, sprite), out)


def disassemble_swf(raw):
    """Returns the disassembly of an SWF file image as a list of lines."""
    raw = bytearray(raw)
    signature = bytes(raw[:3])
    if signature == b'CWS':
        raw = raw[:8] + bytearray(zlib.decompress(bytes(raw[8:])))
    elif signature != b'FWS':
        raise ValueError('not an uncompressed or zlib-compressed SWF (signature %r)' % signature)
    rect_bits = raw[8] >> 3
    rect_bytes = (5 + 4 * rect_bits + 7) // 8
    out = []
    walk_tags(raw[8 + rect_bytes + 4:], '', out)
    return out


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    parser.add_argument('input', help='an .swf file, or a .zip archive containing .swf members')
    parser.add_argument('-o', '--output', help='write the listing here instead of stdout')
    parser.add_argument('--member', help='zip member to disassemble (default: every .swf member)')
    args = parser.parse_args(argv)

    lines = []
    if args.input.lower().endswith('.zip'):
        archive = zipfile.ZipFile(args.input)
        names = [args.member] if args.member else [
            n for n in archive.namelist() if n.lower().endswith('.swf')]
        for name in names:
            lines.append('# %s' % name)
            lines.extend(disassemble_swf(archive.read(name)))
    else:
        with open(args.input, 'rb') as handle:
            lines = disassemble_swf(handle.read())

    text = '\n'.join(lines) + '\n'
    if args.output:
        with open(args.output, 'w') as handle:
            handle.write(text)
    else:
        sys.stdout.write(text)
    return 0


if __name__ == '__main__':
    sys.exit(main())
