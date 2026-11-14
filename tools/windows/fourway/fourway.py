#!/usr/bin/env python3
# [TEMP] [CHOSEN, not recovered] Test tool: the four-way screenshot comparison (retail BF2 | rebuilt
# client Windows x86 | Windows x64 | macOS) of the rend track. Drives the "My Boot Camp" guest from the
# Mac (prlctl exec + scheduled tasks in the signed-in session) and writes the guest's input scripts.
#
#   fourway.py gen                 write the scripts and server job files into out/fourway/guest
#   fourway.py push                copy out/fourway/guest and the guest .cmd tools into C:\rendfw
#   fourway.py setup               one-time guest setup (C:\rendsrv server root, profile template)
#   fourway.py build [x86|x64]     sync this worktree into C:\rendbuild\src, build bf2_client and stage it
#                                  into C:\rendx86 / C:\rendx64 (build.cmd; logs C:\rendbuild\logs\fw*)
#   fourway.py srv JOB             (re)start the retail server in C:\rendsrv with job JOB (dalian, ocs)
#   fourway.py run KIND MODE JOIN TAG SCRIPT [SECONDS]
#                                  one client run (KIND ret|x86|x64, MODE px|is, JOIN join|menu);
#                                  waits for it, then fetches the captures into out/fourway/raw/TAG
#   fourway.py stop                stop the C:\rendsrv server and any fourway client
#   fourway.py collect MAC_DALIAN MAC_OCS MAC_UI MAC_MENU [RETAIL_OUT]
#                                  gather out/fourway/sets/<item>/{retail,x86,x64,mac}.png for compare4.py;
#                                  RETAIL_OUT: an earlier run's out/fourway for the retail frames this run
#                                  did not capture again
#   fourway.py macmatch SCEN VIEW=DX/DY,...
#                                  the macOS scenario script with its aims moved onto the retail views
#
# Views: our clients use the scenario scripts' "aim" (exact world-point views, BF2_INPUT_SCRIPT). The
# retail client has no aim, so its proxy script turns by fixed mouse counts: the spawn point (read from
# the server's playtest_pose.log) gives the initial yaw, the pitch is set from the bottom clamp, and
# every view turns by the yaw difference to the next target. A "mark" after each capture makes the
# server log the soldier's rotation, so the reached yaw can be checked afterwards.
# Port 16890 (never 16567). Nothing is written into C:\Battlefield 2.
import math
import os
import pathlib
import re
import shutil
import subprocess
import sys
import time

HERE = pathlib.Path(__file__).resolve().parent
REPO = HERE.parents[2]
OUT = REPO / 'out/fourway'
GUEST = OUT / 'guest'
VM = os.environ.get('FOURWAY_VM', 'My Boot Camp')
# The guest account the scheduled tasks run as (signed in, interactive).
GUEST_USER = os.environ.get('FOURWAY_GUEST_USER')
SHARE = '\\\\Mac\\Home\\' + str(REPO.relative_to(pathlib.Path.home())).replace('/', '\\')
LOOP = REPO / 'tools/macos/loopback'
CONTENT = pathlib.Path.home() / 'Applications/Battlefield 2.app/Contents/SharedSupport/prefix/drive_c/Battlefield 2'

# Mouse-look gain: 0.150 degrees per count on the retail client in 10-count events 100 ms apart
# (calib3-ret: one nominal 360-degree turn of 2483 counts overshot by 101 and 113 px at 1.25 px per
# count, i.e. 360 degrees took 2402 / 2393 counts; +2 and -2 turns came back within 11 px). Larger
# or denser events were not linear (calib2-ret: +2 and -2 turns in 20-count events 50 ms apart ended
# 136 px off). The soldier rotation the server reports is the body yaw, which follows the view only
# beyond about 45 degrees, so it cannot close an aim loop. The rebuilt macOS client measured 0.1487
# (pitch moves of +150/-300/+150 counts gave -22.3/+44.6/-22.3 degrees, run-20261106-112720). The
# pitch stops at +-84.9 degrees. +x turns right (yaw = atan2(fwd.x, fwd.z) grows), +y looks down.
# The pitch uses the same gain; the retail views of 2026-11-06 then came out 3.2 degrees low on every
# view (27 px at 8.3 px per degree against the macOS aim views), i.e. about 0.144 deg/count for pitch.
# The scripts keep 0.150 so all three Windows clients take the same steps; compare4 searches +-40 px.
DEG_PER_COUNT = 0.150
# Turns are sent as small events far apart, so no frame takes two (STEP_COUNTS per STEP_WAIT_MS).
STEP_COUNTS = 10.0
STEP_WAIT_MS = 100
PITCH_CLAMP = 84.9

# Spawn point (x of the position the server logs before the teleport) -> the client's initial yaw in
# degrees, from the first [PLAY] line after spawning in the macOS runs of 2026-11-05/06.
FACING = {
    'dalian': [('-145.5', 90.0), ('-149.2', 180.0), ('-175.7', -158.3), ('-179.0', 180.0),
               ('-181.0', 180.0), ('-186.0', 180.0)],
    'ocs': [('-520.8', -113.7), ('-574.1', 132.6), ('-588.8', 180.0)],
}
# Final aim geometry of the macOS reference run (horizontal distance, dy from the eye), so the pitch
# matches the reached view: render-view-dalian run-20261106-100247, render-view-ocs run-20261106-100648.
AIM_DY = {
    '-67.5/155.5/157.65': (25.0, 0.7), '-80/154.5/106': (30.0, -0.3), '-100/150.5/157': (10.4, -1.9),
    '-212/153.2/-52': (66.9, -3.5), '-223/158/-248': (33.1, 3.3),
    '-560/29/440': (65.0, -2.2), '-560/60/420': (85.0, 28.8), '-680/29/552': (75.0, -2.1),
    '-560/29/450': (180.5, -21.8),
}
# The load end is in no retail log: wait until the JOIN GAME button's left border (client pixel
# 694,580, RGB 104/88/29 on the loaded-map screen) appears.
LOADED_PIXEL = ['wait-pixel-else 400 loaded 694 580 104 88 29 30']
SCENARIOS = {'dalian': 'render-view-dalian', 'ocs': 'render-view-ocs'}
SPAWN_CLICK = {'dalian': (432, 282), 'ocs': (466, 138)}


def wrap(a):
    while a > 180: a -= 360
    while a < -180: a += 360
    return a


def expand(path, depth=0):
    out = []
    for line in path.read_text().splitlines():
        w = line.strip().split(None, 1)
        if len(w) == 2 and w[0] == 'include' and depth < 8:
            name = w[1] + ('' if w[1].endswith('.txt') else '.txt')
            inc = path.parent / name
            if not inc.is_file():
                inc = LOOP / 'scripts' / name
            out += ['# >>> include ' + w[1]] + expand(inc, depth + 1) + ['# <<< include ' + w[1]]
        else:
            out.append(line)
    return out


def poses(scen):
    player, named = None, {}
    for raw in (LOOP / 'scenarios' / SCENARIOS[scen] / 'pose.cfg').read_text().splitlines():
        w = raw.split('#')[0].split()
        if not w: continue
        if w[0] == 'player': player = tuple(map(float, w[1:4]))
        elif w[0] == 'pose': named[w[1]] = tuple(map(float, w[2:5]))
    return player, named


def win_is_script(scen):
    """Our Windows clients: the scenario script, with the Win32 log markers."""
    lines = expand(LOOP / 'scenarios' / SCENARIOS[scen] / 'script.txt')
    out = ['# [TEMP] fourway: %s for the rebuilt Windows client (BF2_INPUT_SCRIPT), generated by fourway.py' % SCENARIOS[scen]]
    for l in lines:
        l = l.replace('wait-log ingameMap.tga', 'wait-log postLoad #1')
        m = re.match(r'\s*wait-log (view-\S+)\.png\s*$', l)
        if m: l = 'wait-log %s.bmp' % m.group(1)
        out.append(l)
    return out


def turn(counts_x, counts_y=0.0):
    # in small DirectInput events far apart (STEP_COUNTS / STEP_WAIT_MS, see DEG_PER_COUNT)
    out = []
    for axis, total in ((0, counts_x), (1, counts_y)):
        n = max(1, int(math.ceil(abs(total) / STEP_COUNTS)))
        step = total / n
        for _ in range(n):
            if abs(step) >= 0.5:
                out += ['move-rel %.2f %.2f' % ((step, 0) if axis == 0 else (0, step)), 'wait-ms %d' % STEP_WAIT_MS]
    return out


def retail_px_script(scen):
    """The Windows clients through the dinput8 proxy (retail and ours alike): the loaded screen by
    pixel, chat-goto, the yaw from the spawn point's facing and the pitch from the bottom clamp by
    mouse counts (DEG_PER_COUNT)."""
    player, named = poses(scen)
    lines = expand(LOOP / 'scenarios' / SCENARIOS[scen] / 'script.txt')
    sx, sy = SPAWN_CLICK[scen]
    out = ['# [TEMP] fourway: %s for the retail client through the dinput8 proxy, generated by fourway.py.' % SCENARIOS[scen],
           '# wait-log reads the server\'s playtest_pose.log (DI8PROXY_LOG). Views by mouse counts: %.4f deg/count.' % DEG_PER_COUNT,
           'timeout 300', 'log fourway: waiting for the level load (PlayerConnect)',
           'wait-log-else 240 connected [pose] PlayerConnect', 'label connected'] + LOADED_PIXEL + ['label loaded', 'wait-ms 3000', 'capture loaded']
    # JOIN GAME, kit 0, the spawn point, DONE; tried three times (the load end is not logged)
    for n in range(3):
        out += ['click 745 583', 'wait-ms 5000', 'capture spawn-screen-%d' % n,
                'hud-click 130 107', 'wait-ms 1000', 'hud-click %d %d' % (sx, sy), 'wait-ms 1000',
                'capture selected-%d' % n, 'hud-click 728 546',
                'wait-log-else 20 retry%d [pose] spawn ' % n, 'wait-log-else 0 spawned @@never@@',
                'label retry%d' % n, 'log fourway: no spawn yet (attempt %d)' % n, 'wait-ms 15000']
    out += ['label spawned', 'wait-ms 3000', 'capture spawned-1']
    # skip the scenario's own join include
    body, skipping = [], False
    for l in lines:
        if l.startswith('# >>> include join'): skipping = True; continue
        if l.startswith('# <<< include join'): skipping = False; continue
        if not skipping: body.append(l)
    pos = player
    yaw = None              # None until the facing branch has run
    last_target = None
    for l in body:
        s = l.strip()
        if not s or s.startswith('#'):
            out.append(l); continue
        w = s.split()
        if w[0] == 'aim':
            target = w[1]
            if target == last_target:
                continue
            last_target = target
            tx, ty, tz = map(float, target.split('/'))
            want_yaw = math.degrees(math.atan2(tx - pos[0], tz - pos[2]))
            h, dy = AIM_DY[target]
            want_pitch = math.degrees(math.atan2(dy, h))
            pitch = ['move-rel 0 4000', 'wait-ms 300'] + turn(0, -(want_pitch + PITCH_CLAMP) / DEG_PER_COUNT)
            out.append('# aim %s: yaw %.1f pitch %.1f from %.1f/%.1f/%.1f' % (target, want_yaw, want_pitch, *pos))
            if yaw is None:
                for i, (x, face) in enumerate(FACING[scen]):
                    out += ['wait-log-else 1 face%d at %s/' % (i, x),
                            'log fourway: spawn point x=%s, initial yaw %.1f' % (x, face)]
                    out += turn(wrap(want_yaw - face) / DEG_PER_COUNT)
                    out += ['wait-log-else 0 faced @@never@@', 'label face%d' % i]
                out += ['log fourway: unknown spawn point, initial yaw 180 assumed'] + turn(wrap(want_yaw - 180.0) / DEG_PER_COUNT)
                out += ['label faced']
            else:
                out += turn(wrap(want_yaw - yaw) / DEG_PER_COUNT)
            out += pitch + ['wait-ms 500']
            yaw = want_yaw
            continue
        if w[0] == 'wait-log' and len(w) > 1 and w[1].startswith('view-'):
            out.append('wait-ms 500'); continue
        if w[0] == 'capture':
            out.append(l)
            # (no "mark" chat here: a chat box left open swallowed the next turn in the first runs)
            continue
        if w[0] == 'type' and len(w) == 3 and w[1] == 'goto':
            pos = named[w[2]]
            out.append(l); continue
        if w[0] == 'wait-log-else' and len(w) >= 4 and w[3].startswith('pos='):
            name = w[2][3:] if w[2].startswith('at-') else w[2]
            out.append('wait-log-else %s %s [goto] %s ' % (w[1], w[2], name)); continue
        if w[0] == 'wait-log':
            out.append('wait-ms 1500  # (was: %s)' % s); continue
        out.append(l)
    # slower chat around the scenario's goto lines: key j, then the text, then Enter
    slow = []
    for i, l in enumerate(out):
        if l.strip() == 'wait-ms 300' and slow and slow[-1].strip() == 'key j':
            l = 'wait-ms 800'
        if l.strip() == 'key enter' and slow and slow[-1].startswith('type goto'):
            slow += ['wait-ms 300', l, 'wait-ms 700']
            continue
        slow.append(l)
    return slow + ['log fourway: done', 'wait-ms 1500', 'quit']


UI_STEPS = r'''
# [TEMP] fourway UI: connecting / loading screens at fixed times, the loaded-map screen with and
# without hover, the first spawn screen, the HUD (looking at the sky, so only the HUD differs), the
# spawn screen after a suicide. 800x600 client / HUD coordinates, --spawn-time 1.
timeout 400
log fourway ui: connecting / loading
move 400 300
wait-ms 1500
capture boot
wait-ms 2500
capture load-04s
wait-ms 8000
capture load-12s
wait-ms 8000
capture load-20s
wait-ms 8000
capture load-28s
wait-ms 12000
capture load-40s
@LOADED@
wait-ms 4000
move 400 300
wait-ms 800
capture loaded
move 745 583
wait-ms 800
capture loaded-hover-join
move 400 300
wait-ms 800
capture loaded-away
wait-ms 8000
click 745 583
wait-ms 1000
capture spawn-1s
wait-ms 4000
capture spawn-5s
pointer 728 546
wait-ms 1000
capture spawn-hover-done
pointer 400 300
wait-ms 1000
capture spawn-pointer
hud-click 130 107
wait-ms 1000
hud-click 432 282
wait-ms 1000
capture spawn-selected
hud-click 728 546
@SPAWNED@
wait-ms 3000
move-rel 0 -4000
wait-ms 2500
capture hud-sky
key enter
wait-ms 2500
capture menu-for-suicide
hud-click 604 546
wait-ms 1500
capture dead-1s
wait-ms 3000
capture dead-4s
pointer 400 300
wait-ms 1000
capture dead-pointer
log fourway ui: done
wait-ms 1500
quit
'''

MENU_STEPS = r'''
# [TEMP] fourway menu: the main menu after login, a hovered / non-hovered top tab, the multiplayer
# page and its JOIN LOCAL page with buttons hovered and not. Started without +joinServer.
timeout 200
log fourway menu: waiting for the login screen (the retail copy showed a black window for ~20 s)
wait-ms 45000
move 400 300
wait-ms 1500
capture login
click 330 187
wait-ms 1500
capture login-selected
move 532 413
wait-ms 1000
capture login-hover-select
click 532 413
wait-ms 5000
move 400 300
wait-ms 1500
capture menu-main
move 200 15
wait-ms 1000
capture menu-hover-multiplayer
click 200 15
wait-ms 3000
move 400 250
wait-ms 1500
capture mp-page
click 200 47
wait-ms 3000
move 400 250
wait-ms 1500
capture mp-local
move 508 352
wait-ms 1000
capture mp-local-hover-update
move 731 585
wait-ms 1000
capture mp-local-hover-join
move 400 250
wait-ms 1000
capture mp-local-away
log fourway menu: done
wait-ms 1500
quit
'''


def ui_script(flavour):
    text = UI_STEPS
    if flavour == 'mac':
        text = text.replace('@LOADED@', 'wait-log ingameMap.tga').replace('@SPAWNED@', 'wait-log-else 15 spawned [PLAY] alive=1\nlabel spawned')
    elif flavour == 'is':
        text = text.replace('@LOADED@', 'wait-log postLoad #1').replace('@SPAWNED@', 'wait-log-else 15 spawned [PLAY] alive=1\nlabel spawned')
    else:   # px: the proxy reads the server's playtest_pose.log
        text = text.replace('@LOADED@', 'wait-log-else 240 connected [pose] PlayerConnect\nlabel connected\n' + LOADED_PIXEL[0] + '\nlabel loaded').replace(
            '@SPAWNED@', 'wait-log-else 15 spawned [pose] spawn \nlabel spawned')
    return text.strip().splitlines()


def settings_text(bots):
    text = (CONTENT / 'mods/bf2/Settings/ServerSettings.con').read_text(encoding='latin1')
    over = {'serverPort': '16890', 'internet': '0', 'serverIP': '"127.0.0.1"', 'interfaceIP': '"127.0.0.1"',
            'gameSpyPort': '29911', 'voipEnabled': '0', 'useGlobalRank': '0', 'useGlobalUnlocks': '0',
            'autoRecord': '0', 'numPlayersNeededToStart': '1', 'spawnTime': '1', 'manDownTime': '1',
            'coopBotCount': str(bots)}
    for k, v in over.items():
        pat = r'(?mi)^sv\.' + k + r'\s+[^\r\n]*'
        line = 'sv.' + k + ' ' + v
        text = re.sub(pat, lambda _: line, text) if re.search(pat, text) else text + '\r\n' + line + '\r\n'
    return text


def join_px(scen):
    sx, sy = SPAWN_CLICK[scen]
    out = ['timeout 300', 'wait-log-else 240 connected [pose] PlayerConnect', 'label connected'] + LOADED_PIXEL + [
           'label loaded', 'wait-ms 3000', 'capture loaded']
    for n in range(3):
        out += ['click 745 583', 'wait-ms 5000', 'capture spawn-screen-%d' % n,
                'hud-click 130 107', 'wait-ms 1000', 'hud-click %d %d' % (sx, sy), 'wait-ms 1000',
                'capture selected-%d' % n, 'hud-click 728 546',
                'wait-log-else 20 retry%d [pose] spawn ' % n, 'wait-log-else 0 spawned @@never@@',
                'label retry%d' % n, 'log fourway: no spawn yet (attempt %d)' % n, 'wait-ms 15000']
    return out + ['label spawned', 'wait-ms 3000', 'capture spawned-1']


def calib2_script():
    """Yaw gain from images: captures before and after a 100-count turn and after one and two
    nominal full turns (20-count steps); compare4/fwdiff shifts give counts per degree."""
    full = int(round(360.0 / DEG_PER_COUNT))
    out = ['# [TEMP] fourway: image-based yaw calibration (%d counts = one nominal turn), generated by fourway.py' % full] + join_px('dalian')
    out += ['console renderer.drawHud 0', 'wait-ms 500', 'key 2', 'wait-ms 1500', 'move-rel 0 4000', 'wait-ms 300']
    out += turn(0, -PITCH_CLAMP / DEG_PER_COUNT) + ['wait-ms 2000', 'capture cal-a']
    out += turn(100) + ['wait-ms 2000', 'capture cal-a100'] + turn(-100) + ['wait-ms 2000', 'capture cal-a2']
    out += turn(full) + ['wait-ms 2000', 'capture cal-b']
    out += turn(full) + ['wait-ms 2000', 'capture cal-c']
    out += turn(-2 * full) + ['wait-ms 2000', 'capture cal-d']
    return out + ['log fourway: calib2 done', 'wait-ms 1500', 'quit']


def gen():
    if GUEST.exists(): shutil.rmtree(GUEST)
    (GUEST / 'scripts').mkdir(parents=True)
    for scen in SCENARIOS:
        (GUEST / 'scripts' / ('%s-is.txt' % scen)).write_text('\r\n'.join(win_is_script(scen)) + '\r\n')
        (GUEST / 'scripts' / ('%s-px.txt' % scen)).write_text('\r\n'.join(retail_px_script(scen)) + '\r\n')
    (GUEST / 'scripts' / 'calib2-px.txt').write_text('\r\n'.join(calib2_script()) + '\r\n')
    for fl in ('px', 'is'):
        (GUEST / 'scripts' / ('ui-%s.txt' % fl)).write_text('\r\n'.join(ui_script(fl)) + '\r\n')
        (GUEST / 'scripts' / ('menu-%s.txt' % fl)).write_text('\r\n'.join(MENU_STEPS.strip().splitlines()) + '\r\n')
    (OUT / 'mac').mkdir(parents=True, exist_ok=True)
    (OUT / 'mac/ui-mac.txt').write_text('\n'.join(ui_script('mac')) + '\n')
    (OUT / 'mac/menu-mac.txt').write_text(MENU_STEPS.strip() + '\n')
    prof = GUEST / 'prof'
    prof.mkdir()
    (prof / 'Profile.con').write_text('LocalProfile.setName "LoopBot"\r\nLocalProfile.setNick "LoopBot"\r\n'
                                      'LocalProfile.setGamespyNick ""\r\nLocalProfile.setEmail ""\r\n'
                                      'LocalProfile.setTotalPlayedTime 0\r\nLocalProfile.setNumTimesLoggedIn 1\r\n')
    shutil.copy(CONTENT / 'mods/bf2/Settings/Controls.con', prof / 'Controls.con')
    shutil.copy(OUT / 'guestprof/Video.con', prof / 'Video.con')
    jobs = {'dalian': ('dalian_plant gpm_cq 16', 0, 0), 'ocs': ('operation_clean_sweep gpm_coop 16', 1, 2)}
    for job, (maps, ai, bots) in jobs.items():
        d = GUEST / 'srv' / job
        d.mkdir(parents=True)
        (d / 'maplist.con').write_text('mapList.append %s\r\n' % maps)
        (d / 'ai.txt').write_text('%d\r\n' % ai)
        (d / 'ServerSettings.con').write_text(settings_text(bots), encoding='latin1')
        shutil.copy(LOOP / 'scenarios' / SCENARIOS[job] / 'pose.cfg', d / 'playtest_pose.cfg')
    print('generated', GUEST)


def guest(cmd, check=False):
    r = subprocess.run(['prlctl', 'exec', VM, 'cmd', '/c', cmd], capture_output=True, text=True, errors='replace')
    out = (r.stdout + r.stderr).replace('\r', '')
    if check and r.returncode:
        sys.exit('guest command failed: %s\n%s' % (cmd, out))
    return out


def push():
    print(guest('robocopy "%s\\out\\fourway\\guest" C:\\rendfw /E /NFL /NDL /NJH /NJS /NP & '
                'copy /y "%s\\tools\\windows\\fourway\\*.cmd" C:\\rendfw\\ >nul & '
                'copy /y "%s\\tools\\windows\\fourway\\*.ps1" C:\\rendfw\\ >nul & '
                'copy /y "%s\\tools\\macos\\loopback\\pose\\playtest_pose.py" C:\\rendfw\\ >nul & echo pushed'
                % (SHARE, SHARE, SHARE, SHARE)))


def task(name, command):
    if not GUEST_USER:
        sys.exit('fourway.py: set FOURWAY_GUEST_USER to the signed-in guest account')
    out = guest('schtasks /create /tn %s /tr "%s" /sc once /st 23:59 /ru %s /it /f >nul && schtasks /run /tn %s'
                % (name, command, GUEST_USER, name))
    print(out.strip())


def fetch(tag):
    dest = OUT / 'raw' / tag
    if dest.exists(): shutil.rmtree(dest)
    dest.mkdir(parents=True)
    guest('robocopy C:\\rendfw\\shots\\%s "%s\\out\\fourway\\raw\\%s" /E /NFL /NDL /NJH /NJS /NP' % (tag, SHARE, tag))
    for bmp in dest.glob('*.bmp'):
        subprocess.run(['sips', '-s', 'format', 'png', str(bmp), '--out', str(bmp.with_suffix('.png'))],
                       capture_output=True)
        bmp.unlink()
    print('fetched', dest, len(list(dest.glob('*.png'))), 'png')


def stop_clients():
    guest('powershell -NoProfile -Command "Get-Process BF2 -ErrorAction SilentlyContinue | Where-Object { $_.Path -like \'C:\\rend*\' } | Stop-Process -Force"')


def run(kind, mode, join, tag, script, seconds=420):
    stop_clients()
    task('rendfw_cli', 'C:\\rendfw\\cli.cmd %s %s %s %s C:\\rendfw\\scripts\\%s' % (kind, mode, join, tag, script))
    t0 = time.time()
    while time.time() - t0 < float(seconds):
        time.sleep(10)
        if 'yes' in guest('if exist C:\\rendfw\\shots\\%s\\done.txt echo yes' % tag):
            break
    else:
        print('timeout: stopping the client')
        stop_clients()
        time.sleep(5)
    print('run took %.0f s' % (time.time() - t0))
    fetch(tag)


def collect(mac_dalian, mac_ocs, mac_ui, mac_menu, retail_out=None):
    """out/fourway/sets/<item>/{retail,x86,x64,mac}.png from out/fourway/raw/<job>-<client> and the
    macOS loopback runs (directories holding present/). With RETAIL_OUT (an earlier run's out/fourway),
    the retail frames come from RETAIL_OUT/raw/<job>-ret unless this run has its own raw/<job>-ret."""
    sets = OUT / 'sets'
    if sets.exists(): shutil.rmtree(sets)
    groups = {'dalian': mac_dalian, 'ocs': mac_ocs, 'ui': mac_ui, 'menu': mac_menu}
    for job, macrun in groups.items():
        retail = OUT / 'raw' / (job + '-ret')
        if retail_out and not retail.is_dir():
            retail = pathlib.Path(retail_out) / 'raw' / (job + '-ret')
        sources = {'retail': retail, 'x86': OUT / 'raw' / (job + '-x86'),
                   'x64': OUT / 'raw' / (job + '-x64'), 'mac': pathlib.Path(macrun) / 'present'}
        labels = set()
        for d in sources.values():
            for f in d.glob('present_*_*.png'):
                labels.add(f.stem.split('_', 2)[2])
        for label in sorted(labels):
            if job in ('dalian', 'ocs') and not label.startswith('view-'):
                continue
            item = label if label.startswith('view-') else '%s-%s' % (job, label)
            for key, d in sources.items():
                found = sorted(d.glob('present_*_%s.png' % label))
                if found:
                    (sets / item).mkdir(parents=True, exist_ok=True)
                    shutil.copy(found[-1], sets / item / (key + '.png'))
    print('collected', len(list(sets.iterdir())), 'items into', sets)


def macmatch(scen, offsets):
    """The scenario script for the macOS client with every aim target moved so the view matches the
    retail capture: OFFSETS is VIEW=DX/DY,... in pixels (compare4 shifts of the macOS capture against
    retail, 8.33 px per degree at 800x600), e.g. horizon=7/28. Writes out/fourway/mac/<scen>-matched.txt."""
    px_per_deg = 125.0 / (100 * DEG_PER_COUNT)       # calib3-ret: 100 counts moved the view 125 px
    shift = {}
    for item in offsets.split(','):
        name, d = item.split('=')
        dx, dy = d.split('/')
        shift[name] = (float(dx) / px_per_deg, float(dy) / px_per_deg)
    player, named = poses(scen)
    pos, view, out = player, None, []
    for l in expand(LOOP / 'scenarios' / SCENARIOS[scen] / 'script.txt'):
        w = l.split()
        if w[:1] == ['type'] and len(w) == 3 and w[1] == 'goto':
            pos = named[w[2]]
        if w[:1] == ['log'] and 'view ' in l:
            view = l.split('view ', 1)[1].split()[0]
        if w[:1] == ['aim'] and view in shift:
            tx, ty, tz = map(float, w[1].split('/'))
            h, dy = AIM_DY[w[1]]
            eye_y = ty - dy
            yaw = math.atan2(tx - pos[0], tz - pos[2]) + math.radians(shift[view][0])
            pitch = math.atan2(dy, h) - math.radians(shift[view][1])
            nt = '%.2f/%.2f/%.2f' % (pos[0] + h * math.sin(yaw), eye_y + h * math.tan(pitch), pos[2] + h * math.cos(yaw))
            l = l.replace(w[1], nt)
        out.append(l)
    path = OUT / 'mac' / ('%s-matched.txt' % scen)
    path.write_text('# [TEMP] fourway: %s with the aim targets moved onto the retail views (%s)\n' % (SCENARIOS[scen], offsets)
                    + '\n'.join(out) + '\n')
    print(path)


def main():
    a = sys.argv[1:]
    if not a: sys.exit(__doc__ if __doc__ else 'see the header')
    if a[0] == 'gen': gen()
    elif a[0] == 'push': push()
    elif a[0] == 'setup': print(guest('C:\\rendfw\\setup.cmd'))
    elif a[0] == 'build': task('rendfw_build', 'C:\\rendfw\\build.cmd %s %s' % (SHARE, a[1] if len(a) > 1 else 'x86'))
    elif a[0] == 'srv': task('rendfw_srv', 'C:\\rendfw\\srv.cmd %s' % a[1])
    elif a[0] == 'run': run(*a[1:])
    elif a[0] == 'fetch': fetch(a[1])
    elif a[0] == 'collect': collect(*a[1:6])
    elif a[0] == 'macmatch': macmatch(a[1], a[2])
    elif a[0] == 'stop':
        stop_clients()
        guest('powershell -NoProfile -Command "Get-Process bf2_w32ded -ErrorAction SilentlyContinue | Where-Object { $_.Path -like \'C:\\rendsrv\\*\' } | Stop-Process -Force"')
    else: sys.exit('unknown command ' + a[0])


if __name__ == '__main__':
    main()
