#!/usr/bin/python3 -I
"""[CHOSEN, not recovered] Render gate: per-view pixel diff of two gate runs (run.sh output).

  diff.py BEFORE AFTER OUT [--retail MAP:VIEW=PNG ...]

BEFORE / AFTER: gate directories as run.sh writes them (<map>/view-<name>.png plus checks.txt).
For every map/view present on either side it writes OUT/<map>-<view>.png (top: before | after,
bottom: |diff| x4 | retail reference when given; half size) and OUT/summary.md with, per view:
- mean |diff| over RGB, 0-255, on the whole 800x600 frame;
- the share of pixels whose largest channel difference exceeds 24;
- the mean |diff| without the first-person weapon corner (x >= 520 and y >= 340), the part that
  idle sway moves between runs of the same build.
Retail references are side-by-side only (different engine, camera and timing): no numbers.
The log checks of both runs (checks.txt) are copied into the summary. Exit status 0.
"""
import argparse
import pathlib
import sys

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent / 'loopback'))
from compare_views import read_png, write_png   # noqa: E402  (zlib-only PNG IO)

W, H = 800, 600


def load(path):
    if not path or not path.is_file():
        return None
    w, h, rgb = read_png(path)
    if (w, h) != (W, H):   # nearest-neighbour resample to the gate's 800x600
        out = bytearray(W * H * 3)
        for y in range(H):
            sy = y * h // H
            for x in range(W):
                sx = x * w // W
                out[(y * W + x) * 3:(y * W + x) * 3 + 3] = rgb[(sy * w + sx) * 3:(sy * w + sx) * 3 + 3]
        rgb = bytes(out)
    return rgb


def compare(a, b):
    total, changed, scene, sceneN, heat = 0, 0, 0, 0, bytearray(W * H * 3)
    for i in range(0, W * H * 3, 3):
        d0, d1, d2 = abs(a[i] - b[i]), abs(a[i + 1] - b[i + 1]), abs(a[i + 2] - b[i + 2])
        total += d0 + d1 + d2
        if max(d0, d1, d2) > 24:
            changed += 1
        p = i // 3
        if not (p % W >= 520 and p // W >= 340):   # the first-person weapon corner (sway, idle animation)
            scene += d0 + d1 + d2; sceneN += 1
        heat[i], heat[i + 1], heat[i + 2] = min(255, d0 * 4), min(255, d1 * 4), min(255, d2 * 4)
    return total / (W * H * 3), 100.0 * changed / (W * H), scene / (sceneN * 3), bytes(heat)


def half(rgb):
    out = bytearray((W // 2) * (H // 2) * 3)
    if rgb is None:
        return bytes(out)
    for y in range(H // 2):
        for x in range(W // 2):
            s = ((2 * y) * W + 2 * x) * 3
            o = (y * (W // 2) + x) * 3
            out[o:o + 3] = rgb[s:s + 3]
    return bytes(out)


def grid(tiles):
    """2x2 grid of half-size tiles (row-major)."""
    hw, hh = W // 2, H // 2
    out = bytearray(W * H * 3)
    for k, t in enumerate(tiles):
        ox, oy = (k % 2) * hw, (k // 2) * hh
        for y in range(hh):
            out[((oy + y) * W + ox) * 3:((oy + y) * W + ox + hw) * 3] = t[y * hw * 3:(y + 1) * hw * 3]
    return bytes(out)


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('before', type=pathlib.Path)
    p.add_argument('after', type=pathlib.Path)
    p.add_argument('out', type=pathlib.Path)
    p.add_argument('--retail', action='append', default=[], metavar='MAP:VIEW=PNG')
    a = p.parse_args()
    a.out.mkdir(parents=True, exist_ok=True)
    retail = {}
    for r in a.retail:
        key, png = r.split('=', 1)
        retail[key] = pathlib.Path(png)
    views = sorted({(d.name, f.stem) for side in (a.before, a.after) for d in side.iterdir() if d.is_dir()
                    for f in d.glob('view-*.png')})
    lines = ['# Render gate diff', '', 'before: `%s`' % a.before, 'after: `%s`' % a.after, '',
             '| map | view | mean abs diff (0-255) | pixels > 24 | scene mean, weapon corner masked | composite |', '|---|---|---|---|---|---|']
    for m, v in views:
        b, c = load(a.before / m / (v + '.png')), load(a.after / m / (v + '.png'))
        ref = load(retail.get('%s:%s' % (m, v)))
        if b is not None and c is not None:
            mean, pct, scene, heat = compare(b, c)
            nums = '%.2f | %.1f %% | %.2f' % (mean, pct, scene)
        else:
            heat, nums = None, 'missing on %s | - | -' % ('before' if b is None else 'after')
        name = '%s-%s.png' % (m, v)
        write_png(a.out / name, W, H, grid([half(b), half(c), half(heat), half(ref)]))
        lines.append('| %s | %s | %s | %s |' % (m, v, nums, name))
    lines += ['', 'Composite layout: before | after (top), |diff| x4 | retail reference or black (bottom).', '']
    for label, side in (('before', a.before), ('after', a.after)):
        checks = side / 'checks.txt'
        lines += ['## Log checks, %s' % label, '', '```', checks.read_text().rstrip() if checks.is_file() else '(none)', '```', '']
    (a.out / 'summary.md').write_text('\n'.join(lines))
    print((a.out / 'summary.md').read_text())
    return 0


if __name__ == '__main__':
    sys.exit(main())
