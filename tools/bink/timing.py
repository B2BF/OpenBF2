#!/usr/bin/env python3
"""[TEMP] Summarise a bink_harness play log (video.csv): frame pacing against the movie clock.

usage: timing.py <run dir> [--detail F0:F1]
Prints the total, the per-frame interval distribution, the lag of each shown frame behind the
ideal schedule (frame k due at t_first + (k-1)/fps, k counted across loops), and events.
"""
import sys, re

def load(d):
    rows, ev, head = [], [], ''
    for line in open(d + '/video.csv'):
        if line.startswith('#'):
            head = line; continue
        if line.startswith('t_ms'):
            continue
        p = line.rstrip('\n').split(',')
        if p[6]:
            ev.append((float(p[0]), int(p[1]), p[6], int(p[5])))
        else:
            rows.append((float(p[0]), int(p[1]), int(p[2]), int(p[4]), int(p[5])))
    return head, rows, ev

def main():
    d = sys.argv[1]
    head, rows, ev = load(d)
    summ = dict(l.split() for l in open(d + '/summary.txt') if len(l.split()) == 2)
    fps = int(summ['FrameRate']) / int(summ['FrameRateDiv'])
    period = 1000.0 / fps
    t0 = rows[0][0]
    n = len(rows)
    iv = sorted(rows[i][0] - rows[i - 1][0] for i in range(1, n))
    jumps = sum(1 for r in rows if r[2] != r[1] + 1 and r[2] != 1)
    lag = [r[0] - (t0 + i * period) for i, r in enumerate(rows)]
    print('%s: %d frames shown, %.1f ms (ideal %.1f), fps %.3f, nonsequential %d' %
          (d.rsplit('/', 1)[-1], n, rows[-1][0] - t0, (n - 1) * period, fps, jumps))
    print('  interval ms: min %.1f p10 %.1f median %.1f p90 %.1f max %.1f' %
          (iv[0], iv[len(iv) // 10], iv[len(iv) // 2], iv[len(iv) * 9 // 10], iv[-1]))
    print('  lag behind schedule ms: start %.1f end %.1f max %.1f' % (lag[0], lag[-1], max(lag)))
    print('  summary: played %s skipped %s skippedblits %s soundskips %s' %
          (summ.get('TotalPlayedFrames'), summ.get('SkippedFrames'), summ.get('SkippedBlits'), summ.get('SoundSkips')))
    for e in ev:
        print('  event %.1f ms frame %d: %s' % (e[0] - t0, e[1], e[2]))
    if '--detail' in sys.argv:
        a, b = map(int, sys.argv[sys.argv.index('--detail') + 1].split(':'))
        prev = None
        for i, r in enumerate(rows):
            if a <= i + 1 <= b:
                print('   #%d t=%.1f %d->%d waits=%d dt=%.1f lag=%.1f heard=%d' %
                      (i + 1, r[0] - t0, r[1], r[2], r[3], r[0] - prev if prev else 0, lag[i], r[4]))
            prev = r[0]

main()
