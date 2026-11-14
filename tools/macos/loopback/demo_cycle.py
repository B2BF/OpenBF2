#!/usr/bin/env python3
# [TEMP] Demo gate (game track; docs/tracks/game/demo.md): record a short round with bots on the
# loopback server (sv.autoRecord 1, the retail auto-recorder), then play the recorded .bf2demo on a
# client from the command line (+demo). Both phases are ordinary loopback.py runs; this script only
# chains them and judges the result. Options before '--' are passed to both runs (e.g. --build-dir).
#
#   python3 tools/macos/loopback/demo_cycle.py --build-dir out/build/<track>
#
# Phase 1 (scripts/demo-record.txt): the joined player spawns, moves and fires while bots play.
# Phase 2 (scripts/demo-play.txt): follow-player camera, next / previous player, a speed change and
# pause / play, with the shipped keys.
#
# Pass: phase 1 ends with a recorded demo and no [FRONTIER] on either side; phase 2 runs to its
# client timeout with the input script complete, no [FRONTIER] and no crash report, AND playback is
# shown to move:
# - consecutive follow captures differ below the horizon (rows 300..599 of the 800x600 frame; the
#   sky animates on its own, so the whole frame would differ even with a still camera: the old
#   free-camera views differed by 0.0 there and by about 5 over the whole frame);
# - the followed player's position ("[PLAY] demo ... followPos=" lines, BF2_PLAY_TRACE) changes
#   while the camera follows a live player;
# - Space and then Shift+Space each change the followed player (state 2 s after each press), and in
#   every traced follow-mode second the render camera ("cam=", the matrix BF2Render hands the view)
#   is within CAMERA_RANGE of the followed player, and the HUD's DemoPlayerNameString ("hudName=")
#   names that player;
# - while paused (key 1) the followed position stays put and the two paused captures match below
#   the horizon.
# The numbers go to <phase 2 run>/demo-judge.txt.
import pathlib
import re
import subprocess
import sys

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
from compare_views import read_png   # noqa: E402  (zlib-only PNG IO)

common = sys.argv[1:]
FOLLOW_PAIRS = (('follow-1', 'follow-2'), ('follow-2', 'follow-3'), ('follow-3', 'follow-4'),
                ('follow-resumed-1', 'follow-resumed-2'))
GROUND_ROW = 300          # first row compared (below the horizon)
FRAME_NOISE = 0.5         # mean |diff| (0-255, RGB) a moving view must exceed below the horizon;
                          # a still view gives 0.000 there, a slow follow second about 1.4
MIN_TRAVEL = 1.0          # metres the followed player must cover while followed
PAUSE_NOISE = 0.5         # mean |diff| two paused captures may differ by below the horizon
CAMERA_RANGE = 10.0       # metres between the follow camera and the followed player
DEMO_LINE = re.compile(r'\[PLAY\] demo free=(\d) follow=(-?\d+) followAlive=(-?\d) '
                       r'followPos=(\S+)/(\S+)/(\S+)(?: cam=(\S+)/(\S+)/(\S+) camInv=\S+)?'
                       r'(?: hudName=(.*?)\|)?(?: followName=(.*))?')


def run(extra):
    cmd = [sys.executable, str(HERE / 'loopback.py')] + common + extra
    print('$ ' + ' '.join(cmd), flush=True)
    proc = subprocess.run(cmd, capture_output=True, text=True)
    sys.stdout.write(proc.stdout[-3000:])
    found = re.findall(r'Run artifacts: (\S+)', proc.stdout)
    if proc.returncode or not found:
        sys.exit('demo_cycle: loopback failed (exit %d)\n%s' % (proc.returncode, proc.stderr[-2000:]))
    return pathlib.Path(found[-1]), proc.stdout


def capture(run_dir, name):
    found = sorted((run_dir / 'present').glob('*_' + name + '.png'))
    return read_png(found[-1]) if found else None


def ground_diff(a, b, step=2):
    # mean |diff| over RGB below the horizon, every second pixel in both directions
    (w, h, x), (w2, h2, y) = a, b
    if (w, h) != (w2, h2):
        return None
    total = count = 0
    for row in range(GROUND_ROW * h // 600, h, step):
        base = row * w
        for col in range(0, w, step):
            i = (base + col) * 3
            total += abs(x[i] - y[i]) + abs(x[i + 1] - y[i + 1]) + abs(x[i + 2] - y[i + 2])
            count += 3
    return total / count


def followed_travel(log_text):
    # largest distance a followed live player moved from its first followed position
    first, best = {}, (0.0, None)
    for m in re.finditer(r'\[PLAY\] demo free=0 follow=(\d+) followAlive=1 followPos=(\S+)/(\S+)/(\S+)', log_text):
        pid, pos = m.group(1), tuple(float(v) for v in m.group(2, 3, 4))
        start = first.setdefault(pid, pos)
        dist = sum((pos[k] - start[k]) ** 2 for k in range(3)) ** 0.5
        if dist > best[0]:
            best = (dist, pid)
    return best, len(first)


def state_before(log_lines, marker):
    # the last '[PLAY] demo' state before the input-script line containing marker
    last = None
    for line in log_lines:
        if '[INPUT-SCRIPT]' in line and marker in line:
            return last
        m = DEMO_LINE.search(line)
        if m:
            last = m
    return None


def demo_states_between(log_lines, start_marker, end_marker):
    out, inside = [], False
    for line in log_lines:
        if '[INPUT-SCRIPT]' in line and start_marker in line:
            inside = True
        elif '[INPUT-SCRIPT]' in line and end_marker in line:
            break
        elif inside:
            m = DEMO_LINE.search(line)
            if m:
                out.append(m)
    return out


def frontiers(run_dir):
    count = 0
    for name in ('client.log', 'server.pty.log'):
        log = run_dir / name
        if log.is_file():
            count += log.read_text(errors='replace').count('[FRONTIER]')
    return count


def judge_play(play):
    failures = []
    if 'script complete' not in (play / 'client.log').read_text(errors='replace'):
        failures.append('phase 2: the input script did not complete')
    if frontiers(play):
        failures.append('phase 2: [FRONTIER] in ' + str(play))
    if list(play.glob('*.ips')):
        failures.append('phase 2: crash report in ' + str(play))

    judge = []
    for first, second in FOLLOW_PAIRS:
        a, b = capture(play, first), capture(play, second)
        d = ground_diff(a, b) if a and b else None
        judge.append('%s -> %s: ground mean |diff| %s' % (first, second, 'missing' if d is None else '%.3f' % d))
        if d is None or d <= FRAME_NOISE:
            failures.append('phase 2: %s -> %s does not move below the horizon (%s <= %.1f)'
                            % (first, second, 'missing' if d is None else '%.3f' % d, FRAME_NOISE))
    a, b = capture(play, 'paused-1'), capture(play, 'paused-2')
    d = ground_diff(a, b) if a and b else None
    judge.append('paused-1 -> paused-2: ground mean |diff| %s' % ('missing' if d is None else '%.3f' % d))
    if d is None or d > PAUSE_NOISE:
        failures.append('phase 2: the paused captures differ below the horizon (%s > %.1f)'
                        % ('missing' if d is None else '%.3f' % d, PAUSE_NOISE))
    log_text = (play / 'client.log').read_text(errors='replace')
    log_lines = log_text.splitlines()
    def who(m):
        return 'none' if m is None else '%s (%s)' % (m.group(2), (m.group(11) or '?').strip())
    before = state_before(log_lines, 'demo: space - next player')
    after_next = state_before(log_lines, 'demo: shift+space - previous player')
    after_prev = state_before(log_lines, 'demo: key 7 - 150 percent')
    judge.append('followed: before Space %s, after Space %s, after Shift+Space %s'
                 % (who(before), who(after_next), who(after_prev)))
    if not (before and after_next and before.group(2) != after_next.group(2)):
        failures.append('phase 2: Space did not change the followed player')
    if not (after_next and after_prev and after_next.group(2) != after_prev.group(2)):
        failures.append('phase 2: Shift+Space did not change the followed player')
    follow_lines = [m for m in demo_states_between(log_lines, 'demo: follow phase', 'demo: watched')
                    if m.group(1) == '0' and m.group(3) == '1' and m.group(7)]
    far = []
    for m in follow_lines:
        gap = sum((float(m.group(4 + k)) - float(m.group(7 + k))) ** 2 for k in range(3)) ** 0.5
        if gap > CAMERA_RANGE:
            far.append('%s %.1f m' % (m.group(2), gap))
    judge.append('follow camera: %d traced seconds, %d farther than %.0f m from the followed player%s'
                 % (len(follow_lines), len(far), CAMERA_RANGE, (' (' + ', '.join(far[:4]) + ')') if far else ''))
    if len(follow_lines) < 5 or far:
        failures.append('phase 2: the follow camera is not at the followed player (%d traced, %d far)'
                        % (len(follow_lines), len(far)))
    named = [m for m in follow_lines if m.group(10) is not None and m.group(11) is not None]
    wrong = ['%s: hud %r, followed %r' % (m.group(2), m.group(10).strip(), m.group(11).strip())
             for m in named if m.group(10).strip() != m.group(11).strip()]
    judge.append('HUD name: %d traced follow seconds, %d not the followed player%s'
                 % (len(named), len(wrong), (' (' + '; '.join(wrong[:3]) + ')') if wrong else ''))
    if len(named) < 5 or wrong:
        failures.append('phase 2: the HUD name does not follow the followed player (%d traced, %d wrong)'
                        % (len(named), len(wrong)))
    paused = demo_states_between(log_lines, 'demo: key 1 - pause', 'demo: key 2 - normal speed')[1:]
    positions = sorted(set(m.group(4, 5, 6) for m in paused))
    judge.append('positions while paused (after the first line): %s' % (positions or 'none'))
    if len(paused) < 1 or len(positions) != 1:
        failures.append('phase 2: the followed position changed (or was not traced) while paused')
    (travel, pid), followed = followed_travel(log_text)
    judge.append('followed players: %d; largest travel %.2f m (player %s)' % (followed, travel, pid))
    if travel <= MIN_TRAVEL:
        failures.append('phase 2: no followed live player moved more than %.1f m (%.2f)' % (MIN_TRAVEL, travel))
    (play / 'demo-judge.txt').write_text('\n'.join(judge) + '\n')
    print('demo_cycle judge:\n  ' + '\n  '.join(judge))
    return failures


if len(sys.argv) == 3 and sys.argv[1] == '--judge':
    # re-judge an existing phase 2 run directory (no recording, no playback)
    failures = judge_play(pathlib.Path(sys.argv[2]))
    if failures:
        sys.exit('demo_cycle judge FAILED:\n  ' + '\n  '.join(failures))
    print('demo_cycle judge: PASS')
    sys.exit(0)

failures = []
record, out = run(['--ai', '--bots', '8', '--spawn-time', '1', '--auto-record',
                   '--input-script', 'demo-record', '--client-seconds', '100'])
demos = sorted((record / 'demos').glob('*.bf2demo'))
if not demos or demos[-1].stat().st_size < 1024:
    failures.append('phase 1: no recorded demo in ' + str(record / 'demos'))
if frontiers(record):
    failures.append('phase 1: [FRONTIER] in ' + str(record))
if failures:
    sys.exit('demo_cycle FAILED:\n  ' + '\n  '.join(failures))

play, out = run(['--play-demo', str(demos[-1]), '--input-script', 'demo-play', '--client-seconds', '240'])
if 'client exit=-15 timeout=True' not in out:
    failures.append('phase 2: the client did not run to its timeout')
failures += judge_play(play)

print('demo_cycle: recorded %s (%d bytes) in %s; played in %s'
      % (demos[-1].name, demos[-1].stat().st_size, record, play))
if failures:
    sys.exit('demo_cycle FAILED:\n  ' + '\n  '.join(failures))
print('demo_cycle: PASS')
