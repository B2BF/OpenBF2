#!/usr/bin/env python3
# [CHOSEN, not recovered] Compares the real HAL caps (caps_reference.txt from caps_reference.cmd)
# with the macOS device's (macos_metal_device --caps): prints each field that differs.
# Usage: caps_compare.py caps_reference.txt mac_caps.txt
import sys

def read(path):
    out, started = {}, False
    for line in open(path, encoding='utf-8', errors='replace'):
        parts = line.split()
        if not parts:
            continue
        if parts[0] == 'device':
            started = True
            out['device'] = line[7:].strip()
            continue
        if started and len(parts) == 2:
            out[parts[0]] = parts[1]
    return out

ref, mac = read(sys.argv[1]), read(sys.argv[2])
for name, d in (('reference', ref), ('macOS', mac)):
    want = d.pop('fields', None)
    have = len(d) - 1
    if want is not None and int(want) != have:
        sys.exit('%s: %d fields read, the writer printed %s' % (name, have, want))
print('reference:', ref.get('device'), '| macOS:', mac.get('device'))
diff = [k for k in ref if k != 'device' and ref.get(k) != mac.get(k)]
for k in diff:
    print('%-34s reference %-12s macOS %s' % (k, ref[k], mac.get(k, '(missing)')))
print('%d of %d fields differ' % (len(diff), len(ref) - 1))
