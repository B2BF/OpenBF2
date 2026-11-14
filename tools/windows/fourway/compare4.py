#!/usr/bin/python3 -I
"""[TEMP] [CHOSEN, not recovered] Test tool: four-up images and numbers for the fourway comparison
(retail | rebuilt Windows x86 | rebuilt Windows x64 | rebuilt macOS).

  compare4.py SETS_DIR OUT_DIR [--shift N] [--clients retail,x86,mac] [--prev RUN1/numbers.csv]

SETS_DIR holds one directory per view or screen with retail.png, x86.png, x64.png, mac.png (any may
be missing; 800x600, other sizes are resampled with sips). Directories named view-* compare the scene
with the first-person weapon corner masked; every other one compares the whole frame.
For each candidate the alignment search (default +-40 px, fwdiff.c) finds the shift with the
smallest mean |diff| against retail; screens that are not view-* (menus, loading, HUD: fixed layout)
search only +-2 px (--ui-shift). OUT_DIR gets <name>.png (top row the four frames, bottom row
the aligned |diff| x4 against retail; half size) and numbers.csv / numbers.md:
mean |diff| at the found shift, the shift, mean |diff| unaligned, and the share of pixels whose
largest channel difference exceeds 24.
--clients picks the columns (default all four; retail must come first). --prev adds the numbers of an
earlier run (its numbers.csv) beside each client's value in numbers.md, with the change.
"""
import argparse
import pathlib
import subprocess
import sys
import tempfile

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parents[1] / 'macos/loopback'))
from compare_views import read_png, write_png   # noqa: E402  (zlib-only PNG IO)

FWDIFF = HERE.parents[2] / 'out/fourway/bin/fwdiff'
CLIENTS = [('retail', 'RETAIL 1.50'), ('x86', 'OURS WIN X86'), ('x64', 'OURS WIN X64'), ('mac', 'OURS MACOS')]
W, H = 800, 600

# 5x7 glyphs, rows top to bottom, 5 bits each (MSB left).
FONT = {
    'A': [14, 17, 17, 31, 17, 17, 17], 'B': [30, 17, 17, 30, 17, 17, 30], 'C': [14, 17, 16, 16, 16, 17, 14],
    'D': [30, 17, 17, 17, 17, 17, 30], 'E': [31, 16, 16, 30, 16, 16, 31], 'F': [31, 16, 16, 30, 16, 16, 16],
    'G': [14, 17, 16, 23, 17, 17, 15], 'H': [17, 17, 17, 31, 17, 17, 17], 'I': [14, 4, 4, 4, 4, 4, 14],
    'J': [7, 2, 2, 2, 2, 18, 12], 'K': [17, 18, 20, 24, 20, 18, 17], 'L': [16, 16, 16, 16, 16, 16, 31],
    'M': [17, 27, 21, 21, 17, 17, 17], 'N': [17, 17, 25, 21, 19, 17, 17], 'O': [14, 17, 17, 17, 17, 17, 14],
    'P': [30, 17, 17, 30, 16, 16, 16], 'Q': [14, 17, 17, 17, 21, 18, 13], 'R': [30, 17, 17, 30, 20, 18, 17],
    'S': [15, 16, 16, 14, 1, 1, 30], 'T': [31, 4, 4, 4, 4, 4, 4], 'U': [17, 17, 17, 17, 17, 17, 14],
    'V': [17, 17, 17, 17, 17, 10, 4], 'W': [17, 17, 17, 21, 21, 21, 10], 'X': [17, 17, 10, 4, 10, 17, 17],
    'Y': [17, 17, 10, 4, 4, 4, 4], 'Z': [31, 1, 2, 4, 8, 16, 31],
    '0': [14, 17, 19, 21, 25, 17, 14], '1': [4, 12, 4, 4, 4, 4, 14], '2': [14, 17, 1, 2, 4, 8, 31],
    '3': [31, 2, 4, 2, 1, 17, 14], '4': [2, 6, 10, 18, 31, 2, 2], '5': [31, 16, 30, 1, 1, 17, 14],
    '6': [6, 8, 16, 30, 17, 17, 14], '7': [31, 1, 2, 4, 8, 8, 8], '8': [14, 17, 17, 14, 17, 17, 14],
    '9': [14, 17, 17, 15, 1, 2, 12], '.': [0, 0, 0, 0, 0, 12, 12], '-': [0, 0, 0, 31, 0, 0, 0],
    '+': [0, 4, 4, 31, 4, 4, 0], '/': [1, 1, 2, 4, 8, 16, 16], ' ': [0] * 7, ':': [0, 12, 12, 0, 12, 12, 0],
    '|': [4, 4, 4, 4, 4, 4, 4], '(': [2, 4, 8, 8, 8, 4, 2], ')': [8, 4, 2, 2, 2, 4, 8], '_': [0, 0, 0, 0, 0, 0, 31],
}


def text(canvas, cw, x, y, s, scale=2, color=(255, 255, 255)):
    for ch in s.upper():
        g = FONT.get(ch, FONT[' '])
        for r, bits in enumerate(g):
            for c in range(5):
                if bits & (16 >> c):
                    for sy in range(scale):
                        for sx in range(scale):
                            o = 3 * ((y + r * scale + sy) * cw + x + c * scale + sx)
                            canvas[o:o + 3] = bytes(color)
        x += 6 * scale


def load(path, tmp):
    w, h, rgb = read_png(path)
    if (w, h) != (W, H):
        out = pathlib.Path(tmp) / ('rs_' + path.parent.name + '_' + path.name)
        subprocess.run(['sips', '-z', str(H), str(W), '-s', 'format', 'png', str(path), '--out', str(out)],
                       capture_output=True, check=True)
        w, h, rgb = read_png(out)
    return rgb


def half(rgb, w, h):
    out = bytearray(3 * (w // 2) * (h // 2))
    for y in range(h // 2):
        r0 = 3 * (2 * y) * w
        r1 = r0 + 3 * w
        for x in range(w // 2):
            a = r0 + 6 * x
            b = r1 + 6 * x
            o = 3 * (y * (w // 2) + x)
            for c in range(3):
                out[o + c] = (rgb[a + c] + rgb[a + 3 + c] + rgb[b + c] + rgb[b + 3 + c]) >> 2
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('sets', type=pathlib.Path)
    ap.add_argument('out', type=pathlib.Path)
    ap.add_argument('--shift', type=int, default=40)
    ap.add_argument('--ui-shift', type=int, default=2)
    ap.add_argument('--only', default='')
    ap.add_argument('--clients', default=','.join(k for k, _ in CLIENTS))
    ap.add_argument('--prev', type=pathlib.Path, help="an earlier run's numbers.csv for the run-1 columns")
    args = ap.parse_args()
    keys = args.clients.split(',')
    clients = [c for k in keys for c in CLIENTS if c[0] == k]
    args.out.mkdir(parents=True, exist_ok=True)
    rows = []
    tw, th, head = W // 2, H // 2, 24
    for d in sorted(p for p in args.sets.iterdir() if p.is_dir()):
        if args.only and args.only not in d.name:
            continue
        name = d.name
        mask = 1 if name.startswith('view-') else 0
        cw, ch = len(clients) * tw, 2 * (th + head)
        canvas = bytearray(3 * cw * ch)
        with tempfile.TemporaryDirectory() as tmp:
            frames = {}
            for i, (key, label) in enumerate(clients):
                p = d / (key + '.png')
                if p.is_file():
                    frames[key] = load(p, tmp)
            ref = frames.get('retail')
            if ref:
                (pathlib.Path(tmp) / 'ref.rgb').write_bytes(ref)
            result = {}
            for i, (key, label) in enumerate(clients):
                x0 = i * tw
                text(canvas, cw, x0 + 6, 5, label if key in frames else label + ' (MISSING)')
                if key in frames:
                    small = half(frames[key], W, H)
                    for y in range(th):
                        o = 3 * ((head + y) * cw + x0)
                        canvas[o:o + 3 * tw] = small[3 * y * tw:3 * (y + 1) * tw]
                if key == 'retail' or key not in frames or ref is None:
                    continue
                (pathlib.Path(tmp) / 'cand.rgb').write_bytes(frames[key])
                r = subprocess.run([str(FWDIFF), str(pathlib.Path(tmp) / 'ref.rgb'), str(pathlib.Path(tmp) / 'cand.rgb'),
                                    str(W), str(H), str(args.shift if mask else args.ui_shift), str(mask),
                                    str(pathlib.Path(tmp) / 'heat.rgb')],
                                   capture_output=True, text=True, check=True)
                dx, dy, m0, m, pct = r.stdout.split()
                result[key] = (int(dx), int(dy), float(m0), float(m), float(pct))
                heat = half((pathlib.Path(tmp) / 'heat.rgb').read_bytes(), W, H)
                y0 = th + head
                text(canvas, cw, x0 + 6, y0 + 5, 'DIFF %.1f SHIFT %+d/%+d' % (float(m), int(dx), int(dy)), color=(255, 220, 120))
                for y in range(th):
                    o = 3 * ((y0 + head + y) * cw + x0)
                    canvas[o:o + 3 * tw] = heat[3 * y * tw:3 * (y + 1) * tw]
            text(canvas, cw, 6, th + head + 5, name, color=(160, 200, 255))
            text(canvas, cw, 6, th + head + 25, 'BELOW: ALIGNED', scale=2, color=(160, 160, 160))
            text(canvas, cw, 6, th + head + 45, 'ABS DIFF X4', scale=2, color=(160, 160, 160))
            write_png(args.out / (name + '.png'), cw, ch, bytes(canvas))
        rows.append((name, result))
        print('%-28s' % name + ''.join('  %s %6.2f (%+d/%+d, raw %.2f)' % (k, v[3], v[0], v[1], v[2])
                                        for k, v in result.items()), flush=True)
    with open(args.out / 'numbers.csv', 'w') as f:
        f.write('item,client,mean_aligned,dx,dy,mean_unaligned,pct_over24\n')
        for name, result in rows:
            for k, v in result.items():
                f.write('%s,%s,%.2f,%d,%d,%.2f,%.2f\n' % (name, k, v[3], v[0], v[1], v[2], v[4]))
    ours = [k for k in keys if k != 'retail']
    names = {'x86': 'ours x86', 'x64': 'ours x64', 'mac': 'ours macOS'}
    if args.prev:
        prev = {}
        for line in args.prev.read_text().splitlines()[1:]:
            item, client, mean, dx, dy = line.split(',')[:5]
            prev[(item, client)] = (float(mean), int(dx), int(dy))
        with open(args.out / 'numbers.md', 'w') as f:
            f.write('Mean |diff| against retail at the found shift (dx/dy). run 1 = %s\n\n' % args.prev)
            f.write('| item | ' + ' | '.join('%s run 1 | %s run 2 | change' % (names[k], names[k]) for k in ours) + ' |\n')
            f.write('| --- |' + ' --- | --- | --- |' * len(ours) + '\n')
            for name, result in rows:
                cells = []
                for k in ours:
                    a, b = prev.get((name, k)), result.get(k)
                    cells.append('%.2f (%+d/%+d)' % a if a else 'n/a')
                    cells.append('%.2f (%+d/%+d)' % (b[3], b[0], b[1]) if b else 'n/a')
                    cells.append('%+.2f' % (b[3] - a[0]) if a and b else '')
                f.write('| %s | %s |\n' % (name, ' | '.join(cells)))
        return
    with open(args.out / 'numbers.md', 'w') as f:
        f.write('| item | ' + ' | '.join('%s vs retail' % names[k] for k in ours) + ' |\n| --- |' + ' --- |' * len(ours) + '\n')
        for name, result in rows:
            cells = []
            for k in ours:
                v = result.get(k)
                cells.append('%.2f (shift %+d/%+d; raw %.2f; %.1f%% >24)' % (v[3], v[0], v[1], v[2], v[4]) if v else 'n/a')
            f.write('| %s | %s |\n' % (name, ' | '.join(cells)))


if __name__ == '__main__':
    main()
