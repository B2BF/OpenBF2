#!/usr/bin/python3
"""[TEMP] Summarises a BF2_HITCH_TRACE file (Code/BF2/Platform/macOS/HitchTrace.h).

    hitch_report.py <run dir or trace file> [--from TEXT] [--to TEXT] [--limit MS] [--moments TEXT]

The play window runs from the first labelled moment containing --from (default "first-shots start")
to the first one containing --to (default "first-shots done"), or to the end of the trace. Frames
that wrote a BF2_PRESENT_PNG capture are left out of the frame statistics. Prints:
  - the frame-time statistics of the play window (frames over 33 ms and over 100 ms);
  - one row per labelled moment containing --moments (default "shot:"): the worst frame up to the
    next moment and what the main thread spent in each traced category;
  - every frame over --limit (default 33 ms) with its mainLoop sections, categories and events;
  - effect requests and native builds before and inside the window, and how many of the libraries
    and pipeline states the window's draws used for the first time were already built.
"""
import argparse
import pathlib
import statistics
import sys

CATEGORIES = ['fx-request', 'fx-wait', 'shader-create', 'metal-library', 'metal-pipeline', 'metal-state',
              'texture', 'capture', 'gpu-wait', 'drawable', 'library-ready', 'pipeline-ready']
# gpu-wait and drawable are part of every frame (the present); they are listed for a hitch frame but
# not summed as build work.
BUILD = CATEGORIES[:7]
SECTIONS = ['pre', 'update', 'script', 'objects', 'render', 'tail']

p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
p.add_argument('trace', type=pathlib.Path)
p.add_argument('--from', dest='start', default='first-shots start')
p.add_argument('--to', dest='end', default='first-shots done')
p.add_argument('--limit', type=float, default=33.0)
p.add_argument('--moments', default='shot:')
p.add_argument('--events', type=int, default=6, help='events listed per hitch frame')
p.add_argument('--tsv', type=pathlib.Path, help='also write the per-moment table as TSV')
args = p.parse_args()

path = args.trace / 'hitch.trace' if args.trace.is_dir() else args.trace
frames, events, requests, builds, notes = [], {}, [], [], []
for line in path.read_text(errors='replace').splitlines():
    w = line.split(' ')
    try:
        if w[0] == 'F':
            v = [float(x) for x in w[2:]]
            cats = v[3 + len(SECTIONS):]
            frames.append({'n': int(w[1]), 't': v[0], 'ms': v[1], 'game': v[2] != 0,
                           'sec': v[3:3 + len(SECTIONS)],
                           'cat': {CATEGORIES[i]: cats[2 * i] for i in range(min(len(CATEGORIES), len(cats) // 2))},
                           'cnt': {CATEGORIES[i]: int(cats[2 * i + 1]) for i in range(min(len(CATEGORIES), len(cats) // 2))}})
        elif w[0] == 'E':
            events.setdefault(int(w[1]), []).append((w[3], float(w[4]), w[5] == '1', ' '.join(w[6:])))
        elif w[0] == 'R':
            requests.append({'frame': int(w[1]), 't': float(w[2]), 'outcome': w[3], 'ms': float(w[4]), 'key': w[5]})
        elif w[0] == 'B':
            builds.append({'t': float(w[1]), 'ms': float(w[2]), 'key': w[-1]})
        elif w[0] == 'N':
            notes.append({'frame': int(w[1]), 't': float(w[2]), 'text': ' '.join(w[3:])})
    except (ValueError, IndexError):
        continue          # a line cut off when the client was stopped
if not frames:
    sys.exit('no frames in ' + str(path))

t0 = next((n['t'] for n in notes if args.start in n['text']), None)
t1 = next((n['t'] for n in notes if args.end in n['text'] and (t0 is None or n['t'] > t0)), frames[-1]['t'])
if t0 is None:
    t0 = next((f['t'] for f in frames if f['game']), frames[0]['t'])
    print('no "%s" moment: the window starts at the first in-game frame' % args.start)
# A frame belongs to the window by its end time; its start is t - ms.
play = [f for f in frames if t0 < f['t'] <= t1]
clean = [f for f in play if f['cnt'].get('capture', 0) == 0]
if not clean:
    sys.exit('no frames in the play window')


def pct(values, q):
    values = sorted(values)
    return values[min(len(values) - 1, int(q * len(values)))]


ms = [f['ms'] for f in clean]
print('trace %s' % path)
print('play window %.1f s .. %.1f s (%.1f s), %d frames (%d capture frames left out)' %
      (t0, t1, t1 - t0, len(clean), len(play) - len(clean)))
print('frame ms: mean %.1f  median %.1f  p95 %.1f  p99 %.1f  max %.1f' %
      (statistics.mean(ms), statistics.median(ms), pct(ms, 0.95), pct(ms, 0.99), max(ms)))
for f in clean:
    f['build'] = sum(f['cat'].get(c, 0.0) for c in BUILD)
print('frames over 33 ms: %d   over 50 ms: %d   over 100 ms: %d   time in frames over 33 ms: %.0f ms' %
      (sum(1 for f in clean if f['ms'] > 33), sum(1 for f in clean if f['ms'] > 50),
       sum(1 for f in clean if f['ms'] > 100), sum(f['ms'] for f in clean if f['ms'] > 33)))
# A build hitch: a frame over the limit in which traced build work is at least 5 ms and at least a
# third of the time above the median frame.
med = statistics.median(ms)
built = [f for f in clean if f['ms'] > 33 and f['build'] >= max(5.0, 0.33 * (f['ms'] - med))]
other = [f for f in clean if f['ms'] > 33 and f not in built]
print('  caused by builds (effects, shaders, pipelines, textures): %d over 33 ms, %d over 50 ms, %d over 100 ms; '
      'build time in them %.0f ms (%s)' %
      (len(built), sum(1 for f in built if f['ms'] > 50), sum(1 for f in built if f['ms'] > 100), sum(f['build'] for f in built),
       ', '.join('%s %.0f' % (c, sum(f['cat'].get(c, 0.0) for f in built)) for c in BUILD
                 if sum(f['cat'].get(c, 0.0) for f in built) >= 0.5) or '-'))
print('  other frames over 33 ms: %d (over 50 ms: %d, over 100 ms: %d), e.g. %s' %
      (len(other), sum(1 for f in other if f['ms'] > 50), sum(1 for f in other if f['ms'] > 100),
       '; '.join('frame %d %.0f ms (gpu-wait %.0f, drawable %.0f)' % (f['n'], f['ms'], f['cat'].get('gpu-wait', 0), f['cat'].get('drawable', 0))
                 for f in sorted(other, key=lambda f: -f['ms'])[:4]) or '-'))

print('\n== labelled moments ("%s"): worst frame until the next moment ==' % args.moments)
print('%-34s %8s %5s %5s  %s' % ('moment', 'worst ms', '>33', '>100', 'main-thread ms by category (window)'))
rows = []
window_notes = [n for n in notes if t0 <= n['t'] <= t1]
for i, n in enumerate(window_notes):
    if args.moments not in n['text']:
        continue
    end = window_notes[i + 1]['t'] if i + 1 < len(window_notes) else t1
    fs = [f for f in clean if n['t'] < f['t'] <= end + 0.001 or (f['t'] - f['ms'] / 1000.0 <= n['t'] < f['t'])]
    if not fs:
        continue
    worst = max(fs, key=lambda f: f['ms'])
    cat = {c: sum(f['cat'].get(c, 0.0) for f in fs) for c in BUILD}
    scenario = next((m['text'] for m in reversed(window_notes[:i]) if 'scenario: kit' in m['text'] or 'vehicle' in m['text']), '')
    name = (scenario.replace('scenario: ', '').split(':')[0][:14] + ' / ' if scenario else '') + n['text'].replace(args.moments, '').strip()
    rows.append((name, worst['ms'], sum(1 for f in fs if f['ms'] > 33), sum(1 for f in fs if f['ms'] > 100), cat, n['t']))
    print('%-34s %8.1f %5d %5d  %s' % (name[:34], rows[-1][1], rows[-1][2], rows[-1][3],
                                      ' '.join('%s %.0f' % (c, v) for c, v in cat.items() if v >= 0.5)))
if rows:
    print('worst first-use frame: %.1f ms; moments with a frame over 33 ms: %d of %d; over 100 ms: %d' %
          (max(r[1] for r in rows), sum(1 for r in rows if r[2]), len(rows), sum(1 for r in rows if r[3])))
if args.tsv:
    with args.tsv.open('w') as out:
        out.write('moment\tt\tworst_ms\tover33\tover100\t' + '\t'.join(BUILD) + '\n')
        for name, worst, a, b, cat, t in rows:
            out.write('%s\t%.2f\t%.1f\t%d\t%d\t%s\n' % (name, t, worst, a, b, '\t'.join('%.1f' % cat[c] for c in BUILD)))

print('\n== frames over %.0f ms in the play window ==' % args.limit)
for f in clean:
    if f['ms'] <= args.limit:
        continue
    label = next((n['text'] for n in reversed(notes) if n['t'] <= f['t']), '')
    traced = f['build']
    print('frame %d t=%.2f %.1f ms after "%s"' % (f['n'], f['t'], f['ms'], label[:60]))
    print('    sections: %s' % ' '.join('%s %.1f' % (SECTIONS[i], v) for i, v in enumerate(f['sec']) if v >= 0.5))
    print('    build work %.1f ms; %s' % (traced, ' '.join('%s %.1f/%d' % (c, f['cat'][c], f['cnt'][c])
                                                            for c in CATEGORIES if f['cnt'].get(c))))
    main = sorted((e for e in events.get(f['n'], []) if e[2]), key=lambda e: -e[1])
    for e in main[:args.events]:
        print('      %-14s %7.1f ms  %s' % (e[0], e[1], e[3][:90]))

print('\n== effect requests and native builds ==')
for name, lo, hi in (('before the window', -1e9, t0), ('in the window', t0, t1), ('after the window', t1, 1e9)):
    rs = [r for r in requests if lo < r['t'] <= hi]
    bs = [b for b in builds if lo < b['t'] <= hi]
    outcomes = {}
    for r in rs:
        outcomes[r['outcome']] = outcomes.get(r['outcome'], 0) + 1
    print('%-18s requests %d (%s), main-thread %.0f ms; native builds finished %d' %
          (name, len(rs), ', '.join('%s %d' % kv for kv in sorted(outcomes.items())) or '-', sum(r['ms'] for r in rs), len(bs)))
if builds:
    bm = sorted(b['ms'] for b in builds)
    print('native builds: %d, summed %.1f s from submission; per build median %.0f ms, p90 %.0f, max %.0f; '
          'first finished at %.1f s, last at %.1f s; the play window starts at %.1f s' %
          (len(builds), sum(bm) / 1000.0, statistics.median(bm), pct(bm, 0.9), bm[-1], builds[0]['t'], builds[-1]['t'], t0))
bg = {}
for evs in events.values():
    for e in evs:
        if not e[2]:
            bg.setdefault(e[0], [0, 0.0])
            bg[e[0]][0] += 1
            bg[e[0]][1] += e[1]
if bg:
    print('off the main thread: ' + ', '.join('%s %d events %.1f s' % (c, v[0], v[1] / 1000.0) for c, v in sorted(bg.items())))
allcat = {c: [0, 0.0] for c in CATEGORIES}
for f in frames:
    for c in CATEGORIES:
        allcat[c][0] += f['cnt'].get(c, 0)
        allcat[c][1] += f['cat'].get(c, 0.0)
print('whole run, main thread: ' + ', '.join('%s %d / %.1f s' % (c, v[0], v[1] / 1000.0) for c, v in allcat.items() if v[0]))
wincat = {c: [0, 0.0] for c in CATEGORIES}
for f in play:
    for c in CATEGORIES:
        wincat[c][0] += f['cnt'].get(c, 0)
        wincat[c][1] += f['cat'].get(c, 0.0)
print('play window, main thread: ' + ', '.join('%s %d / %.0f ms' % (c, v[0], v[1]) for c, v in wincat.items() if v[0]))
# First uses by a draw in the play window: found built (prebuilt in the background, or shared with an
# equal program) against built or waited for at the draw.
for name, ready, built in (('libraries', 'library-ready', 'metal-library'), ('pipelines', 'pipeline-ready', 'metal-pipeline')):
    r, b = wincat[ready][0], wincat[built][0]
    if r + b:
        print('play window, first use of %s: %d ready, %d built or waited for at the draw (%.0f%% ready)' %
              (name, r, b, 100.0 * r / (r + b)))
