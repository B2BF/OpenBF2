#!/usr/bin/env python3
"""[TEMP] Fit binkw32's YUV -> BGRx conversion from bink_harness dumps (black box).

usage: fit_colour.py <out dir> <movie:w:h:f1,f2,...> ...

Reads <out>/r_f_<movie>/f<N>_s15.raw (retail BinkCopyToBuffer YV12: Y, V, U planes, which equal
the port's decoded planes) and f<N>_s3.raw (retail BGRx). Prints, step by step, the evidence
behind the conversion in Code/RendDX9Abi/BinkFFmpeg.cpp:
  1. each output channel is a function of (Y, nearest-neighbour U/V) - no chroma filtering;
  2. B - Y', R - Y', G - Y' depend on chroma only (separable integer tables);
  3. the coefficient bands for which every pixel fits floor/trunc tables;
  4. how the luma term clamps (below / above).
Nothing game-derived is written; only counts and coefficients are printed.
"""
import sys, math, collections

out = sys.argv[1]
px = []   # (Y, U, V, B, G, R)
for spec in sys.argv[2:]:
    n, w, h, fr = spec.split(':'); w = int(w); h = int(h)
    for f in map(int, fr.split(',')):
        yv = open('%s/r_f_%s/f%d_s15.raw' % (out, n, f), 'rb').read()
        bg = open('%s/r_f_%s/f%d_s3.raw' % (out, n, f), 'rb').read()
        ys = w * h; cw = w // 2; cs = cw * (h // 2)
        for y in range(h):
            for x in range(w):
                c = (y // 2) * cw + x // 2; o = (y * w + x) * 4
                px.append((yv[y * w + x], yv[ys + cs + c], yv[ys + c], bg[o], bg[o + 1], bg[o + 2]))
print('pixels', len(px))

# 1. determinism with nearest-neighbour chroma
for ch, name, key in ((3, 'B', lambda p: (p[0], p[1])), (4, 'G', lambda p: p[:3]), (5, 'R', lambda p: (p[0], p[2]))):
    m = collections.defaultdict(set)
    for p in px:
        m[key(p)].add(p[ch])
    print('1. %s: %d keys, %d ambiguous' % (name, len(m), sum(1 for v in m.values() if len(v) > 1)))

tr = lambda x: int(x / 65536)          # toward zero
cl = lambda v: 0 if v < 0 else (255 if v > 255 else v)
gray = {}
for p in px:
    if p[1] == 128 and p[2] == 128:
        gray[p[0]] = p[3]
band = [cy for cy in range(75000, 78000) if all(cl(((k - 16) * cy) >> 16) == v for k, v in gray.items())]
print('3. luma: floor((Y-16)*cy/65536) fits for cy in [%d, %d]' % (band[0], band[-1]))
cy = band[len(band) // 2]
ytab = lambda Y: math.floor((Y - 16) * cy / 65536)

# 2. separability (unclamped pixels with Y in 16..235)
def table(ch, key):
    d = collections.defaultdict(set)
    for p in px:
        if 16 <= p[0] <= 235 and 0 < p[ch] < 255:
            d[key(p)].add(p[ch] - ytab(p[0]))
    print('2. channel %d - Y\': %d chroma keys, %d not constant' % (ch, len(d), sum(1 for v in d.values() if len(v) > 1)))
    return {k: next(iter(v)) for k, v in d.items()}
bU = table(3, lambda p: p[1]); rV = table(5, lambda p: p[2]); gUV = table(4, lambda p: (p[1], p[2]))

def bandfit(t, lo, hi, f):
    b = [k for k in range(lo, hi) if all(f(k, c) == v for c, v in t.items())]
    return (b[0], b[-1]) if b else None
print('3. B = Y\' + trunc(cbu*(U-128)/65536): cbu in', bandfit(bU, 131000, 133500, lambda k, c: tr(k * (c - 128))),
      'U range', min(bU), max(bU))
print('3. R = Y\' + trunc(crv*(V-128)/65536): crv in', bandfit(rV, 103500, 105500, lambda k, c: tr(k * (c - 128))),
      'V range', min(rV), max(rV))
print('3. G = Y\' + trunc(-cgu*(U-128)) + trunc(-cgv*(V-128)): cgu in',
      bandfit(gUV, 25400, 25900, lambda k, c: tr(-k * (c[0] - 128)) + tr(-53275 * (c[1] - 128))), 'cgv in',
      bandfit(gUV, 53000, 53600, lambda k, c: tr(-25672 * (c[0] - 128)) + tr(-k * (c[1] - 128))))

# 4. clamping of the luma term
votes = collections.Counter()
for p in px:
    Y, U, V = p[:3]
    if 16 <= Y <= 235:
        continue
    for ch, term in ((3, tr(132212 * (U - 128))), (5, tr(104592 * (V - 128)))):
        a = cl(max(0, ytab(Y)) + term)              # clamp below only
        b = cl(min(255, max(0, ytab(Y))) + term)    # clamp both ends
        c = cl(ytab(Y) + term)                      # no clamp
        if len({a, b, c}) > 1:
            label = ' '.join(n for n, v in (('below-only', a), ('both', b), ('none', c)) if v == p[ch]) or 'neither'
            votes[label] += 1
print('4. luma clamp votes (pixels where the variants differ):', dict(votes))
