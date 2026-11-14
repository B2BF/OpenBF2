#!/usr/bin/python3 -I
"""[TEMP] [CHOSEN, not recovered] Compare the view-*.png captures of two scenario runs (or a run and a
saved baseline) against the scenario's per-view limits (docs/macos-c-input-script.md, "Standard
scenarios").

  compare_views.py REFERENCE CANDIDATE [--scenario NAME] [--out DIR] [--pixel-threshold N] [--all-pngs]
  compare_views.py --baseline-save NAME RUN [--scenario NAME]

REFERENCE / CANDIDATE: a loopback run directory (its present/present_<n>_view-<name>.png files) or a
baseline directory (view-<name>.png). For every view in both, the script computes these on the masked
800x600 frame:
- the mean |diff| over RGB, 0-255;
- the share of pixels whose largest channel difference exceeds --pixel-threshold (default 24).

The mask leaves out the first-person pistol corner (x >= 520 and y >= 340), the bottom HUD bars
(y >= 560), the minimap (x >= 590 and y < 210) and the chat lines (x < 300 and 60 <= y < 160). The HUD
parts matter only for captures taken with the HUD on.

Limits come from scenarios/<name>/limits.cfg, one "VIEW MEAN" per line (VIEW without "view-").
The scenario is taken from --scenario, else from <run>/scenario.txt (written by loopback.py
--scenario), else from the limits file whose views match. Each view over its limit (or missing on
one side) gets a side-by-side + diff-heatmap PNG in --out (default: the candidate directory's
compare/ folder); --all-pngs writes one for every view that exists on both sides. The exit status is 1 when any view fails, 2 on a usage error, 0 otherwise.

--baseline-save NAME RUN copies the run's view captures to out/baselines/NAME/ (or NAME itself when it
is a path inside out/), as the accepted reference for later comparisons. Baselines are never
committed.

PNG decoding and encoding use zlib only (no PIL on the test Mac): 8-bit RGB / RGBA, non-interlaced.
"""
import argparse
import os
import pathlib
import re
import shutil
import struct
import sys
import zlib

HERE = pathlib.Path(__file__).resolve().parent
REPO = HERE.parents[2]
OUT = REPO / 'out'
SCENARIOS = HERE / 'scenarios'
W, H = 800, 600


def read_png(path):
    """Returns (width, height, rgb bytes) of an 8-bit RGB or RGBA, non-interlaced PNG."""
    data = pathlib.Path(path).read_bytes()
    if data[:8] != b'\x89PNG\r\n\x1a\n':
        raise ValueError('%s: not a PNG' % path)
    pos, idat, width = 8, [], None
    while pos < len(data):
        length, kind = struct.unpack('>I4s', data[pos:pos + 8])
        body = data[pos + 8:pos + 8 + length]
        pos += 12 + length
        if kind == b'IHDR':
            width, height, depth, ctype, _, _, interlace = struct.unpack('>IIBBBBB', body)
            if depth != 8 or ctype not in (2, 6) or interlace:
                raise ValueError('%s: only 8-bit RGB/RGBA non-interlaced PNGs (depth %d type %d)' % (path, depth, ctype))
            bpp = 3 if ctype == 2 else 4
        elif kind == b'IDAT':
            idat.append(body)
        elif kind == b'IEND':
            break
    raw = zlib.decompress(b''.join(idat))
    stride = width * bpp
    out = bytearray(height * stride)
    prev = bytearray(stride)
    src = 0
    for y in range(height):
        ftype = raw[src]
        line = bytearray(raw[src + 1:src + 1 + stride])
        src += 1 + stride
        if ftype == 1:
            for i in range(bpp, stride):
                line[i] = (line[i] + line[i - bpp]) & 0xFF
        elif ftype == 2:
            for i in range(stride):
                line[i] = (line[i] + prev[i]) & 0xFF
        elif ftype == 3:
            for i in range(stride):
                left = line[i - bpp] if i >= bpp else 0
                line[i] = (line[i] + ((left + prev[i]) >> 1)) & 0xFF
        elif ftype == 4:
            for i in range(stride):
                a = line[i - bpp] if i >= bpp else 0
                b = prev[i]
                c = prev[i - bpp] if i >= bpp else 0
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                line[i] = (line[i] + (a if pa <= pb and pa <= pc else b if pb <= pc else c)) & 0xFF
        out[y * stride:(y + 1) * stride] = line
        prev = line
    if bpp == 4:
        rgb = bytearray(width * height * 3)
        rgb[0::3], rgb[1::3], rgb[2::3] = out[0::4], out[1::4], out[2::4]
        out = rgb
    return width, height, bytes(out)


def write_png(path, width, height, rgb):
    rows = b''.join(b'\x00' + rgb[y * width * 3:(y + 1) * width * 3] for y in range(height))
    def chunk(kind, body):
        return struct.pack('>I', len(body)) + kind + body + struct.pack('>I', zlib.crc32(kind + body) & 0xFFFFFFFF)
    pathlib.Path(path).write_bytes(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 2, 0, 0, 0))
                                   + chunk(b'IDAT', zlib.compress(rows, 6)) + chunk(b'IEND', b''))


def masked(x, y):
    """True for pixels left out of the comparison (800x600 layout)."""
    return ((x >= 520 and y >= 340) or y >= 560 or (x >= 590 and y < 210) or (x < 300 and 60 <= y < 160))


ROW_SPANS = []  # per row: list of (x0, x1) kept spans
for _y in range(H):
    spans, start = [], None
    for _x in range(W + 1):
        keep = _x < W and not masked(_x, _y)
        if keep and start is None:
            start = _x
        elif not keep and start is not None:
            spans.append((start, _x))
            start = None
    ROW_SPANS.append(spans)


def compare(a, b, threshold):
    """Mean |diff| over RGB and the share of pixels over threshold, on the kept pixels; plus the
    per-pixel max channel difference (0 where masked) for the heatmap."""
    total = over = count = 0
    heat = bytearray(W * H)
    for y in range(H):
        for x0, x1 in ROW_SPANS[y]:
            o, e = (y * W + x0) * 3, (y * W + x1) * 3
            da = [abs(p - q) for p, q in zip(a[o:e], b[o:e])]
            total += sum(da)
            for i in range(0, len(da), 3):
                m = max(da[i], da[i + 1], da[i + 2])
                heat[y * W + x0 + i // 3] = m
                if m > threshold:
                    over += 1
            count += x1 - x0
    return total / (3.0 * count), 100.0 * over / count, heat


MAX_SHIFT = 4   # pixels; one mouse count turns the view about 0.15 degrees, 1-2 px at 800x600


def best_shift(a, b):
    """The integer (dx, dy) within +-MAX_SHIFT that best aligns candidate b to reference a.

    The scripted aim settles to one mouse count, so two runs of the same view can differ by a
    pixel or two of pure image shift; aligning first keeps that jitter out of the comparison.
    Searched on a 4x-subsampled green channel of the kept pixels, away from the borders."""
    samples = [(x, y) for y in range(MAX_SHIFT, H - MAX_SHIFT, 4) for x in range(MAX_SHIFT, W - MAX_SHIFT, 4)
               if not masked(x, y)]
    ga = [a[(y * W + x) * 3 + 1] for x, y in samples]
    best = (None, 0, 0)
    for dy in range(-MAX_SHIFT, MAX_SHIFT + 1):
        for dx in range(-MAX_SHIFT, MAX_SHIFT + 1):
            s = 0
            for (x, y), g in zip(samples, ga):
                s += abs(g - b[((y + dy) * W + x + dx) * 3 + 1])
            if best[0] is None or s < best[0]:
                best = (s, dx, dy)
    return best[1], best[2]


def shifted(b, dx, dy):
    """b moved so that out[x, y] = b[x + dx, y + dy], edges clamped."""
    if dx == 0 and dy == 0:
        return b
    out = bytearray(len(b))
    for y in range(H):
        sy = min(H - 1, max(0, y + dy))
        for x in range(W):
            sx = min(W - 1, max(0, x + dx))
            o, so = (y * W + x) * 3, (sy * W + sx) * 3
            out[o:o + 3] = b[so:so + 3]
    return bytes(out)


def heat_rgb(value, is_masked):
    """Black (equal) -> red -> yellow (max channel difference 64 and up); masked pixels dark blue."""
    if is_masked:
        return (40, 40, 64)
    return (min(255, value * 8), max(0, min(255, (value - 32) * 8)), 0)


def write_report(path, a, b, heat):
    """Side by side (reference | candidate | heatmap) at half size."""
    hw, hh = W // 2, H // 2
    out = bytearray(hw * 3 * hh * 3)
    for y in range(hh):
        sy = 2 * y
        for x in range(hw):
            sx = 2 * x
            s = (sy * W + sx) * 3
            d = (y * hw * 3 + x) * 3
            out[d:d + 3] = a[s:s + 3]
            out[d + hw * 3:d + hw * 3 + 3] = b[s:s + 3]
            out[d + 2 * hw * 3:d + 2 * hw * 3 + 3] = bytes(heat_rgb(heat[sy * W + sx], masked(sx, sy)))
    write_png(path, hw * 3, hh, bytes(out))


def views(directory):
    """{name: path} for view-<name> captures in a run (present/) or a baseline directory."""
    found = {}
    d = pathlib.Path(directory)
    for f in sorted((d / 'present').glob('present_*_view-*.png')) + sorted(d.glob('view-*.png')):
        m = re.search(r'view-([A-Za-z0-9_-]+)\.png$', f.name)
        if m:
            found[m.group(1)] = f
    return found


def load_limits(name):
    path = SCENARIOS / name / 'limits.cfg'
    limits = {}
    if path.is_file():
        for line in path.read_text().splitlines():
            words = line.split('#')[0].split()
            if len(words) >= 2:
                limits[words[0]] = float(words[1])
    return limits


def scenario_of(directory, names, explicit):
    if explicit:
        return explicit
    for d in directory:
        f = pathlib.Path(d) / 'scenario.txt'
        if f.is_file():
            return f.read_text().strip()
    best, score = None, 0
    for cfg in SCENARIOS.glob('*/limits.cfg'):
        s = len(set(load_limits(cfg.parent.name)) & set(names))
        if s > score:
            best, score = cfg.parent.name, s
    return best


def main():
    p = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    p.add_argument('dirs', nargs='+', help='REFERENCE CANDIDATE, or RUN with --baseline-save')
    p.add_argument('--scenario')
    p.add_argument('--out', type=pathlib.Path)
    p.add_argument('--pixel-threshold', type=int, default=24)
    p.add_argument('--no-align', action='store_true',
                   help='compare without the +-4 px alignment search (aim jitter then shows as edges)')
    p.add_argument('--all-pngs', action='store_true',
                   help='write the side-by-side + heatmap PNG for every view, not only the ones over the limit')
    p.add_argument('--baseline-save', metavar='NAME')
    args = p.parse_args()

    if args.baseline_save:
        if len(args.dirs) != 1:
            p.error('--baseline-save takes one run directory')
        run = pathlib.Path(args.dirs[0])
        found = views(run)
        if not found:
            sys.exit('no view-*.png captures in ' + str(run))
        dest = pathlib.Path(args.baseline_save)
        if os.sep not in args.baseline_save:
            dest = OUT / 'baselines' / args.baseline_save
        dest = dest.resolve()
        if OUT.resolve() not in dest.parents:
            sys.exit('baselines live under %s (not committed): %s' % (OUT, dest))
        dest.mkdir(parents=True, exist_ok=True)
        for name, f in found.items():
            shutil.copyfile(f, dest / ('view-%s.png' % name))
        scen = scenario_of([run], list(found), args.scenario)
        if scen:
            (dest / 'scenario.txt').write_text(scen + '\n')
        (dest / 'source.txt').write_text(str(run.resolve()) + '\n')
        print('saved %d views (%s) from %s to %s' % (len(found), scen or 'scenario unknown', run, dest))
        return 0

    if len(args.dirs) != 2:
        p.error('give REFERENCE and CANDIDATE')
    ref, cand = (pathlib.Path(d) for d in args.dirs)
    va, vb = views(ref), views(cand)
    names = sorted(set(va) | set(vb))
    if not names:
        sys.exit('no view-*.png captures in either directory')
    scen = scenario_of([cand, ref], names, args.scenario)
    limits = load_limits(scen) if scen else {}
    out = args.out or (cand / 'compare')
    print('scenario %s (limits %s); reference %s; candidate %s; pixel threshold %d' % (
        scen or '?', (SCENARIOS / scen / 'limits.cfg') if scen else 'none', ref, cand, args.pixel_threshold))
    print('%-12s %10s %8s %10s  %s' % ('view', 'mean|diff|', 'limit', '% > thr', 'result'))
    failed = 0
    for name in names:
        if name not in va or name not in vb:
            print('%-12s %10s %8s %10s  FAIL (missing in %s)' % (name, '-', '-', '-', 'reference' if name not in va else 'candidate'))
            failed += 1
            continue
        wa, ha, a = read_png(va[name])
        wb, hb, b = read_png(vb[name])
        if (wa, ha) != (W, H) or (wb, hb) != (W, H):
            print('%-12s size %dx%d / %dx%d, expected %dx%d  FAIL' % (name, wa, ha, wb, hb, W, H))
            failed += 1
            continue
        dx, dy = (0, 0) if args.no_align else best_shift(a, b)
        b = shifted(b, dx, dy)
        mean, pct, heat = compare(a, b, args.pixel_threshold)
        limit = limits.get(name)
        bad = limit is not None and mean > limit
        print('%-12s %10.2f %8s %9.2f%%  %s  shift %+d/%+d' % (name, mean, '%.1f' % limit if limit is not None else '-', pct,
                                               'FAIL' if bad else ('pass' if limit is not None else 'no limit'), dx, dy))
        if bad:
            failed += 1
        if bad or args.all_pngs:
            out.mkdir(parents=True, exist_ok=True)
            report = out / ('view-%s-compare.png' % name)
            write_report(report, a, b, heat)
            print('             reference | candidate | heatmap: %s' % report)
    print('%d view(s) over the limit or missing' % failed if failed else 'all views within their limits')
    return 1 if failed else 0


if __name__ == '__main__':
    sys.exit(main())
