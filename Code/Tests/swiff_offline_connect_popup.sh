#!/bin/sh
# [TEMP] [CHOSEN, not recovered] No game content, sockets or engine startup.
set -eu
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
OUT="$ROOT/out/feat-connectip3/unit"
BUILD="${1:-$ROOT/out/build/feat-connectip3}"
mkdir -p "$OUT"
cmake --build "$BUILD" --target bf2_swiff_offline_connect_popup_check -j4
python3 - "$OUT" <<'PY'
# Synthetic SWF: one popup sprite and an authored callback that records intent.
# It models only the offline gate's input state; no retail content is embedded.
import pathlib, struct, sys
out = pathlib.Path(sys.argv[1])
def u16(n): return struct.pack('<H', n)
def tag(code, body=b''):
    return u16(code << 6 | min(len(body), 63)) + (struct.pack('<I', len(body)) if len(body) >= 63 else b'') + body
def action(code, body=b''): return bytes([code]) + (u16(len(body)) if code >= 128 else b'') + body
def push(*strings): return action(0x96, b''.join(b'\0' + s.encode() + b'\0' for s in strings))
get, member, put = b'\x1c', b'\x4e', b'\x4f'
body = push('_root') + get + push('selectedPopup', 'name') + get + put
body += push('_root') + get + push('popups') + member + push('_visible') + action(0x96, b'\x05\x01') + put + b'\0'
function = action(0x9b, b'openPopup\0' + u16(1) + b'name\0' + u16(len(body))) + body
bits = '01100' + format(0, '012b') + format(2000, '012b') + format(0, '012b') + format(2000, '012b')
rect = int(bits.ljust((len(bits) + 7) // 8 * 8, '0'), 2).to_bytes((len(bits) + 7) // 8, 'big')
for variant in ('popup', 'missing', 'missing-popup'):
    sprite = u16(1) + u16(3)
    for label in ('noDialog', 'wrongLabel' if variant == 'missing' else 'connecttoip', 'quit'):
        sprite += tag(43, label.encode() + b'\0') + tag(12, b'\x07\0') + tag(1)
    sprite += tag(0)
    data = rect + u16(24 << 8) + u16(1) + tag(39, sprite)
    name = b'otherPopups' if variant == 'missing-popup' else b'popups'
    data += tag(26, b'\x26' + u16(1) + u16(1) + b'\0' + name + b'\0')
    data += tag(12, function + b'\x07\0') + tag(1) + tag(0)
    (out / (variant + '.swf')).write_bytes(b'FWS\x07' + struct.pack('<I', len(data) + 8) + data)
PY
EXE="$BUILD/bf2_swiff_offline_connect_popup_check"
"$EXE" "$OUT/popup.swf" 2>"$OUT/popup.stderr"
grep -q "offline connecttoip declined: popups connecttoip navigation failed" "$OUT/popup.stderr"
if grep -q "FRONTIER" "$OUT/popup.stderr"; then cat "$OUT/popup.stderr"; exit 1; fi
python3 - "$EXE" "$OUT" <<'PY'
import pathlib, subprocess, sys
exe, out = sys.argv[1], pathlib.Path(sys.argv[2])
for fixture, mode, reason in [('missing-popup.swf', 'missing-popup', 'main-menu popups clip absent'),
                              ('missing.swf', 'missing-label', 'popups connecttoip frame absent')]:
    result = subprocess.run([exe, str(out / fixture), mode], capture_output=True, text=True)
    assert result.returncode == 0, (fixture, result.returncode, result.stderr)
    assert '[FRONTIER]' not in result.stderr, result.stderr
    line = '[SwiffPlayer] offline connecttoip declined: ' + reason
    assert result.stderr.count(line) == 1, result.stderr
print('Offline Connect to IP declines: 2/2 quiet declines passed')
PY
