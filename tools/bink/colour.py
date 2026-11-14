#!/usr/bin/env python3
"""[TEMP] Compare two BGRx frame dumps of the bink harness (retail vs port).

usage: colour.py <a.raw> <b.raw> <width> <height>
Prints per-channel max abs diff, differing pixel count, PSNR over B, G, R, and a diff histogram.
"""
import sys, math
a = open(sys.argv[1], 'rb').read(); b = open(sys.argv[2], 'rb').read()
w, h = int(sys.argv[3]), int(sys.argv[4])
n = w * h
mx = [0, 0, 0]; se = 0; diffpix = 0; hist = {}
for i in range(n):
    o = i * 4; d = False
    for c in range(3):
        e = a[o + c] - b[o + c]
        if e:
            d = True
            ae = abs(e); mx[c] = max(mx[c], ae); se += e * e
            hist[ae] = hist.get(ae, 0) + 1
    diffpix += d
mse = se / (3.0 * n)
psnr = 10 * math.log10(255 * 255 / mse) if mse else float('inf')
print('max |d| B %d G %d R %d, pixels differing %d of %d (%.2f%%), PSNR %.2f dB, hist %s' %
      (mx[0], mx[1], mx[2], diffpix, n, 100.0 * diffpix / n, psnr, dict(sorted(hist.items()))))
