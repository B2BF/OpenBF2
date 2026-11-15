#!/usr/bin/python3 -I
"""[TEMP] [CHOSEN, not recovered] c-loopback: a sandboxed dedicated server and a sandboxed
client joining it over loopback UDP 127.0.0.1:16890, offline, no menu interaction.

Usage: loopback.py [--build TAG] [--ai] [--client-seconds N] [--client-args "..."] [--content DIR]
       loopback.py --no-server [--build TAG] [--input-script SCRIPT] [--content DIR]

Server: bf2 under expect (server.tcl) and sandbox-exec (server.sb), from an APFS clone of the
reference content under out/loopback/run-*/server-root, with the milestone-1 offline settings.
Client: bf2_client under sandbox-exec (client.sb), from its own clone, started once the server
status monitor shows "playing", with the shipped command-line join
(+joinServer 127.0.0.1 +port 16890 +playerName <name>) and +restart 1 (skipMovies).
Both logs, the client's Logs/ and any crash report land in the run directory.
Never binds or probes 16567; refuses to start when 16890/29911 are taken.
"""
import argparse
import datetime
import os
import pathlib
import pwd
import re
import shlex
import shutil
import signal
import subprocess
import sys
import time

HERE = pathlib.Path(__file__).resolve().parent


def descendants(root):
    """All live descendant pids of root (from one ps snapshot), deepest last."""
    out = subprocess.run(['/bin/ps', '-axo', 'pid=,ppid='], capture_output=True, text=True).stdout
    children = {}
    for line in out.split('\n'):
        f = line.split()
        if len(f) == 2:
            children.setdefault(int(f[1]), []).append(int(f[0]))
    found, todo = [], [root]
    while todo:
        for c in children.get(todo.pop(), []):
            found.append(c)
            todo.append(c)
    return found


def signal_tree(root, pids, sig):
    for target in [root] + pids:
        for kill in (os.killpg, os.kill):
            try:
                kill(target, sig)
            except (ProcessLookupError, PermissionError):
                pass
REPO = HERE.parents[2]
OWNER_HOME = pathlib.Path(pwd.getpwuid(os.getuid()).pw_dir).resolve()
OUT = REPO / 'out'

p = argparse.ArgumentParser()
p.add_argument('--build', default='c-loopback')
p.add_argument('--build-dir', type=pathlib.Path,
               help='[TEMP] a build directory (holding bin_macos/) or its bin_macos/ itself, e.g. another '
                    "worktree's out/build/<tag>; overrides --build")
p.add_argument('--content', type=pathlib.Path,
               default=OWNER_HOME / 'Applications/Battlefield 2.app/Contents/drive_c/Battlefield 2')
p.add_argument('--ai', action='store_true', help='server +ai 1 with a gpm_coop map list (bots)')
p.add_argument('--map', default='dalian_plant')
p.add_argument('--mod', default='bf2',
               help='[TEMP] +modPath mods/<MOD> for server and client (e.g. xpack, which needs the Special Forces '
                    'levels); ServerSettings.con falls back to mods/bf2 when the mod has none. The pose/mark '
                    'fixtures stay under mods/bf2/python')
p.add_argument('--bots', type=int, help='sv.coopBotCount for --ai (stock: 16)')
p.add_argument('--bot-ratio', type=float, help='[TEMP] sv.coopBotRatio for server-info checks')
p.add_argument('--bot-difficulty', type=float, help='[TEMP] sv.coopBotDifficulty for server-info checks')
p.add_argument('--check-bot-info', action='store_true',
               help='[TEMP] check the published GameSpy bot fields inside the client sandbox; '
                    'requires --ai, --bots, --bot-ratio and --bot-difficulty')
p.add_argument('--spawn-time', type=int, metavar='SECONDS',
               help='[TEMP] sv.spawnTime and sv.manDownTime (stock: 15 / 15); renderer runs use 1 so '
                    'suicide/respawn cycles are quick')
p.add_argument('--client-seconds', type=int, default=int(os.environ.get('BF2_RUN_SECONDS', '90')))
p.add_argument('--client-args', default=os.environ.get('BF2_EXTRA_ARGS', ''))
p.add_argument('--player', default='LoopBot')
p.add_argument('--tutorial-messages', choices=('on', 'off'),
               help='[TEMP] set the tutorial pop-up preference in the isolated profile General.con; '
                    'omitting it tests the default ON behavior')
p.add_argument('--default-profile', choices=('none', 'default'),
               help='[TEMP] model the Profiles/Default state of a real install instead of copying the '
                    'content Profiles/Local/Default: none = first start, the client creates it; default = '
                    'an install whose earlier build wrote it lowercase (docs/macos-profile-prefix.md)')
p.add_argument('--profiles-from', type=pathlib.Path, metavar='RUN_DIR',
               help='[TEMP] start with the Profiles directory an earlier run left in its client home '
                    '(a second start of the same install); replaces the generated profile fixture')
p.add_argument('--profile-resolution', action='store_true',
               help='[CHOSEN, not recovered] omit +szx/+szy so the profile Video.con resolution applies')
p.add_argument('--fullscreen', action='store_true', help='pass +fullscreen 1 instead of 0')
p.add_argument('--antialiasing', default='', metavar='NSamples',
               help='[CHOSEN, not recovered] profile VideoSettings.setAntialiasing value, e.g. 4Samples')
p.add_argument('--video-high', action='store_true',
               help='profile Video.con at the highest settings (owner plays at High on Windows)')
p.add_argument('--video-con', type=pathlib.Path,
               help='[TEMP] copy this file as the profile Video.con (settings bisects; overrides --video-high)')
p.add_argument('--ai-spawn-only', action='store_true',
               help='[TEMP] bots spawn but run no per-tick AI (BF2_AI_SPAWN_ONLY=1)')
p.add_argument('--no-client', action='store_true', help='server only (checks the server half)')
p.add_argument('--no-server', action='store_true',
               help='[TEMP] client only, with no command-line join; reserves 16890/29911 for the '
                    "client's singleplayer listen server")
p.add_argument('--movies', action='store_true',
               help='[TEMP] bink track: leave out +restart 1, so the client plays the intro movie queue first')
p.add_argument('--server-extra', default='')
p.add_argument('--lldb', action='store_true',
               help='run the client under lldb inside the same sandbox; prints the stop backtrace')
p.add_argument('--lldb-cmd', action='append', default=[],
               help='extra lldb -o command run before "run" (repeatable), e.g. a breakpoint')
p.add_argument('--lldb-post', action='append', default=[],
               help='extra lldb -k command run when the process stops (repeatable)')
p.add_argument('--menu-join', action='store_true',
               help='leave the join to the menu ActionScript (shipped); default sets BF2_MAC_COMMANDLINE_JOIN=1')
p.add_argument('--no-join', action='store_true',
               help='[TEMP] start the client without +joinServer/+port: it stays in the menus (main menu and '
                    'multiplayer screen captures); the server still runs')
p.add_argument('--auto-record', action='store_true',
               help='server sv.autoRecord 1: the round is recorded to mods/bf2/demos/auto_*.bf2demo; after the '
                    'run every recorded .bf2demo is copied to <run>/demos/ (docs/tracks/game/demo.md)')
p.add_argument('--play-demo', type=pathlib.Path, metavar='BF2DEMO',
               help='client only (implies --no-server --no-join): the demo is copied into the client root and '
                    'played from the command line with +demo demo-play.con (a playback config next to it)')
p.add_argument('--demo-config-line', action='append', default=[], metavar='LINE',
               help='with --play-demo: an extra line for demo-play.con after demo.demofile (repeatable), '
                    'e.g. "demo.timedemo 1" (docs/tracks/game/frontier-closure.md)')
p.add_argument('--input-script',
               help='[TEMP] scripted client input (BF2_INPUT_SCRIPT): a file, or a name in scripts/ '
                    '(e.g. join-and-spawn); see docs/macos-c-input-script.md')
p.add_argument('--present-png', action='store_true',
               help='BF2_PRESENT_PNG captures into <run>/present (on by default with --input-script)')
p.add_argument('--present-every', type=int, default=0, help='BF2_PRESENT_PNG_EVERY (default 600 with a script, else 60)')
p.add_argument('--voip', action='store_true',
               help='server sv.voipEnabled 1 (the local voice server): the client starts its voice session on the '
                    'synthetic capture device (BF2_VOIP_CAPTURE=fake, set for every run)')
p.add_argument('--interface-ip', metavar='IPV4',
               help='server sv.serverIP/sv.interfaceIP (default 127.0.0.1), e.g. the host side of a VM network '
                    'so a client inside the VM can join; the local client still joins over 127.0.0.1')
p.add_argument('--server-password', metavar='TEXT',
               help='server sv.password; the client joins with +password TEXT (empty: the client passes none)')
p.add_argument('--client-password', metavar='TEXT',
               help='[TEST] the client\'s +password when it differs from --server-password (wrong-password check)')
p.add_argument('--bundle-id', default='', metavar='ID',
               help='[TEMP] run the client from <run>/client.app (a clone of the build\'s binaries) with this '
                    'CFBundleIdentifier, so Metal keeps its shader caches in $DARWIN_USER_CACHE_DIR/<ID>/ instead of '
                    'the one every unbundled build shares: a new ID is a fresh install\'s cold cache, the same ID '
                    'again its second start (remove that directory afterwards)')
p.add_argument('--hitch-trace', action='store_true',
               help='[TEMP] BF2_HITCH_TRACE=<run>/hitch.trace: per-frame times and what the main thread spent on '
                    'effect builds, Metal compiles and texture loads (hitch_report.py summarises it)')
p.add_argument('--client-env', action='append', default=[], metavar='KEY=VALUE',
               help='extra client environment (repeatable), e.g. BF2_METAL_SKIP_REFUSED_DRAWS=1 (debug aid)')
p.add_argument('--name-redirect', metavar='DOMAIN',
               help='[TEMP] test-only: load <build>/test_tools/libbf2_test_name_redirect.dylib (build target '
                    'bf2_test_name_redirect first) into server and client, so DOMAIN and *.DOMAIN resolve to '
                    '127.0.0.1, and add +gamespy DOMAIN to both command lines. The sandbox profiles then also '
                    'allow the GameSpy service ports on loopback (Code/Tests/name_redirect/README.md)')
p.add_argument('--name-redirect-http-port', type=int, metavar='PORT',
               help='[TEMP] with --name-redirect: TCP connections to 127.0.0.1:80 (the stats web host) go to '
                    '127.0.0.1:PORT instead')
p.add_argument('--pose', metavar='X/Y/Z[/YAW[/PITCH[/ROLL]]]',
               help='[TEMP] play-test fixture (pose/playtest_pose.py): every spawn of a human player is moved to '
                    'this world position and rotation (degrees), so scripted runs start from the same place; '
                    '"log" only logs spawn positions ([pose] lines in playtest_pose.log, copied to the run directory); '
                    'with --scenario it replaces the scenario\'s player line')
p.add_argument('--map2', help='[TEMP] append a second map (same mode) to the map list')
p.add_argument('--server-send', action='append', default=[], metavar='SECONDS:COMMAND',
               help='[TEMP] type COMMAND at the server console SECONDS after playing (--no-client) or '
                    'after the client start / --server-send-after text (with a client); COMMAND !kill '
                    'SIGKILLs the server')
p.add_argument('--server-send-after', metavar='TEXT',
               help='[TEMP] with a client: --server-send SECONDS count from the first TEXT in client.log')
p.add_argument('--no-spawn-aid', action='store_true',
               help='[TEMP] --ai --no-client without BF2_AI_SPAWN_WITHOUT_HUMAN (AI loads, bots stay unspawned)')
p.add_argument('--scenario',
               help='[TEMP] standard play-test scenario: scenarios/<name>/ (pose.cfg installed as the server '
                    'scenario file, script.txt as the input script, loopback.args as default options); see '
                    'the "Standard scenarios" section of docs/macos-c-input-script.md')
p.add_argument('--mark-session', action='store_true',
               help='[TEMP] interactive position picking: no input script, --client-seconds 1800 unless given, '
                    'pose module installed (spawn log only, unless --pose/--scenario); type "mark NAME" in chat '
                    'to record a position into playtest_marks.txt (copied to the run directory at the end)')
p.add_argument('--ai-trace', action='store_true', help='BF2_AI_TRACE=1 on the server (retired: the upstream sync removed the [TEMP] AI trace; no output)')
# [TEMP] --scenario NAME: the scenario's loopback.args come first, so the command line overrides them.
argv = sys.argv[1:]
scenario_dir = None
for i, a in enumerate(argv):
    name = a.split('=', 1)[1] if a.startswith('--scenario=') else (argv[i + 1] if a == '--scenario' and i + 1 < len(argv) else None)
    if name:
        scenario_dir = pathlib.Path(name) if os.sep in name else HERE / 'scenarios' / name   # a name or a directory
        if not (scenario_dir / 'pose.cfg').is_file():
            sys.exit('no scenario ' + name + ' (' + str(scenario_dir) + '/pose.cfg)')
        extra = scenario_dir / 'loopback.args'
        if extra.is_file():
            argv = shlex.split(extra.read_text(), comments=True) + argv
        break
args = p.parse_args(argv)
if args.no_server:
    if args.no_client or args.server_send or args.server_send_after or args.server_extra:
        p.error('--no-server cannot be combined with server-only actions')
    args.no_join = True
if args.check_bot_info and (not args.ai or any(v is None for v in
                           (args.bots, args.bot_ratio, args.bot_difficulty))):
    p.error('--check-bot-info requires --ai, --bots, --bot-ratio and --bot-difficulty')
if args.mark_session:
    if '--client-seconds' not in ' '.join(sys.argv[1:]):
        args.client_seconds = 1800
    args.input_script = None
elif scenario_dir and not args.input_script and (scenario_dir / 'script.txt').is_file():
    args.input_script = str(scenario_dir / 'script.txt')

script = None
if args.input_script:
    script = pathlib.Path(args.input_script)
    if not script.is_file():
        script = HERE / 'scripts' / (args.input_script + ('' if args.input_script.endswith('.txt') else '.txt'))
    if not script.is_file():
        sys.exit('no input script ' + args.input_script)

content = args.content.resolve(strict=True)
build = OUT / 'build' / args.build / 'bin_macos'
if args.build_dir:
    build = args.build_dir.resolve()
    if (build / 'bin_macos').is_dir():
        build = build / 'bin_macos'
server_bin, client_bin = build / 'bf2', build / 'bf2_client'
for b in (() if args.no_server else (server_bin,)) + (() if args.no_client else (client_bin,)):
    if not b.is_file():
        sys.exit('missing ' + str(b))

# [TEMP] --name-redirect: sandbox-exec is a protected system binary, so the system drops DYLD_* from
# its environment; /usr/bin/env sets them again for the game image it starts.
redirect_wrap = []
if args.name_redirect_http_port and not args.name_redirect:
    p.error('--name-redirect-http-port requires --name-redirect')
if args.name_redirect:
    if args.lldb:
        p.error('--name-redirect cannot be combined with --lldb')
    redirect_lib = build.parent / 'test_tools' / 'libbf2_test_name_redirect.dylib'
    if not redirect_lib.is_file():
        sys.exit('missing ' + str(redirect_lib) + ' (build the bf2_test_name_redirect target)')
    redirect_wrap = ['/usr/bin/env', 'DYLD_INSERT_LIBRARIES=' + str(redirect_lib),
                     'OPENBF2_TEST_REDIRECT_DOMAIN=' + args.name_redirect, 'OPENBF2_TEST_REDIRECT_LOG=1']
    if args.name_redirect_http_port:
        redirect_wrap.append('OPENBF2_TEST_REDIRECT_HTTP_PORT=%d' % args.name_redirect_http_port)
if content == REPO or REPO in content.parents or content in REPO.parents:
    sys.exit('Content must be a separate read-only reference installation.')

def sandbox(profile):
    return ['/usr/bin/sandbox-exec', '-D', 'OWNER_HOME=' + str(OWNER_HOME), '-D', 'RUN_OUT=' + str(OUT),
            '-D', 'GAME_ROOT=' + str(content), '-f', str(profile)]

# [TEMP] Several agents share one machine and one port pair: hold a machine-wide lock for the
# whole run so concurrent loopback runs queue instead of racing for 16890/29911. BF2_LOOPBACK_LOCK
# names another lock file (e.g. one shared with an external runner).
import fcntl
_lock_path = pathlib.Path(os.environ.get('BF2_LOOPBACK_LOCK') or OWNER_HOME / 'Library/Caches/OpenBF2-loopback/loopback.lock')
_lock_path.parent.mkdir(parents=True, exist_ok=True)
# flock also works on a read-only descriptor; preserve an existing coordinator lock file.
_lock = open(_lock_path, 'r' if _lock_path.exists() else 'a')
try:
    fcntl.flock(_lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
except BlockingIOError:
    print('[loopback] another loopback run holds %s; waiting' % _lock_path, flush=True)
    fcntl.flock(_lock, fcntl.LOCK_EX)
_deadline = time.monotonic() + 600
while True:
    busy = subprocess.run(['/usr/sbin/lsof', '-nP', '-iUDP:16890', '-iTCP:16890', '-iUDP:29911', '-t'],
                          capture_output=True, text=True).stdout.strip()
    if not busy or time.monotonic() > _deadline:
        break
    time.sleep(5)   # a non-loopback user of the ports (e.g. a client-hosted game) may still be closing
busy = subprocess.run(['/usr/sbin/lsof', '-nP', '-iUDP:16890', '-iTCP:16890', '-iUDP:29911', '-t'],
                      capture_output=True, text=True).stdout.strip()
if busy:
    sys.exit('UDP 16890 or 29911 already in use (pid ' + busy.replace('\n', ',') + '); nothing started.')

run = OUT / 'loopback' / ('run-' + datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))
run.mkdir(parents=True)
if scenario_dir:
    (run / 'scenario.txt').write_text(scenario_dir.name + '\n')   # compare_views.py picks the limits by it
if args.bundle_id and not args.no_client:
    macos = run / 'client.app/Contents/MacOS'
    macos.parent.mkdir(parents=True)
    subprocess.run(['/bin/cp', '-Rc', str(build), str(macos)], check=True)
    (macos.parent / 'Info.plist').write_text(
        '<?xml version="1.0" encoding="UTF-8"?>\n<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" '
        '"http://www.apple.com/DTDs/PropertyList-1.0.dtd">\n<plist version="1.0"><dict>'
        '<key>CFBundleIdentifier</key><string>%s</string><key>CFBundleExecutable</key><string>bf2_client</string>'
        '<key>CFBundleName</key><string>OpenBF2 loopback</string><key>CFBundlePackageType</key><string>APPL</string>'
        '<key>NSHighResolutionCapable</key><true/></dict></plist>\n' % args.bundle_id)
    client_bin = macos / 'bf2_client'
for name in ('server.sb', 'client.sb'):
    (run / name).write_bytes((HERE / name).read_bytes())
if args.no_server:
    (run / 'client.sb').write_bytes((HERE / 'singleplayer.sb').read_bytes())
if args.voip:
    # The voice server binds sv.voipServerPort 55125 and the game server's voice channel
    # sv.voipBFServerPort 55124; the client's voice session talks to 55125 from sv.voipBFClientPort
    # 55123. Without these rules the sandbox refused the binds and no voice server ever started.
    if not args.no_server:
        profile = run / 'server.sb'
        profile.write_text(profile.read_text() + '(allow network-bind (local ip "*:55124") (local ip "*:55125"))\n')
    profile = run / 'client.sb'
    profile.write_text(profile.read_text() + '(allow network-outbound (remote ip "localhost:55125"))\n'
                       '(allow network-bind (local ip "*:55123"))\n')
if args.interface_ip:
    # The server must answer a client on the VM network: its replies go to that host, not localhost.
    # The game and query sockets stay bound to --interface-ip (a host-only bridge), see SETTINGS.
    profile = run / 'server.sb'
    profile.write_text(profile.read_text() + '(allow network-outbound (remote ip "*:*"))\n')
if args.name_redirect:
    # The GameSpy services' fixed ports and the stats web port, on loopback only (the server profile
    # already allows every loopback peer).
    ports = [80, 27900, 27901, 28910, 29900, 29901, 29910, 29920] + (
        [args.name_redirect_http_port] if args.name_redirect_http_port else [])
    profile = run / 'client.sb'
    profile.write_text(profile.read_text() + ''.join(
        '(allow network-outbound (remote ip "localhost:%d"))\n' % port for port in ports))
for profile in (('client.sb',) if args.no_server else ('server.sb', 'client.sb')):
    check = subprocess.run(sandbox(run / profile) + ['/usr/bin/true'], capture_output=True, text=True)
    if check.returncode:
        sys.exit('Sandbox unavailable (' + profile + '): ' + check.stderr)

if args.play_demo:
    if not args.play_demo.is_file():
        sys.exit('no demo ' + str(args.play_demo))
    args.play_demo = args.play_demo.resolve()
    args.no_server = True
    args.no_join = True

SETTINGS = {
    'serverPort': '16890', 'internet': '0', 'serverIP': '"127.0.0.1"',
    'interfaceIP': '"127.0.0.1"', 'gameSpyPort': '29911', 'voipEnabled': '0',
    'useGlobalRank': '0', 'useGlobalUnlocks': '0', 'autoRecord': '0',
    'numPlayersNeededToStart': '1',
}

if args.auto_record:
    SETTINGS['autoRecord'] = '1'
if args.voip:
    SETTINGS['voipEnabled'] = '1'
if args.server_password:
    if not re.fullmatch(r'[A-Za-z0-9_-]+', args.server_password):
        sys.exit('--server-password: letters, digits, _ and - only')
    SETTINGS['password'] = '"' + args.server_password + '"'
if args.interface_ip:
    if not re.fullmatch(r'\d{1,3}(\.\d{1,3}){3}', args.interface_ip):
        sys.exit('--interface-ip needs a dotted IPv4 address')
    SETTINGS['serverIP'] = SETTINGS['interfaceIP'] = '"' + args.interface_ip + '"'

def configure(path):
    if run not in path.resolve().parents:
        sys.exit('Refusing to edit a configuration outside the run clone: ' + str(path))
    text = path.read_text(encoding='latin1')
    for key, value in SETTINGS.items():
        line = 'sv.' + key + ' ' + value
        pattern = r'(?mi)^sv\.' + key + r'\s+[^\r\n]*'
        text = re.sub(pattern, lambda _: line, text) if re.search(pattern, text) else text + '\n' + line + '\n'
    path.write_text(text, encoding='latin1')

def clone(dest):
    dest.mkdir()
    for name in ('mods', 'python', 'pylib-2.3.4.zip', 'Settings', 'Profiles'):
        if (content / name).exists():
            subprocess.run(['/bin/cp', '-Rc', str(content / name), str(dest / name)], check=True)
    for path in dest.rglob('*'):
        if path.name.lower() == 'serversettings.con':
            configure(path)
    cfg = dest / 'smoke_cfg'
    cfg.mkdir()
    settings = dest / ('mods/%s/Settings/ServerSettings.con' % args.mod)
    if not settings.exists():
        settings = dest / 'mods/bf2/Settings/ServerSettings.con'
    subprocess.run(['/bin/cp', '-c', str(settings),
                    str(cfg / 'ServerSettings.con')], check=True)
    if args.pose or scenario_dir or args.mark_session:
        game = dest / 'mods/bf2/python/game'
        if (dest / ('mods/%s/python/game' % args.mod)).is_dir():   # the mod's own game package (xpack)
            game = dest / ('mods/%s/python/game' % args.mod)
        subprocess.run(['/bin/cp', str(HERE / 'pose/playtest_pose.py'), str(game / 'playtest_pose.py')], check=True)
        cfg_text = (scenario_dir / 'pose.cfg').read_text() if scenario_dir else ''
        if args.pose and args.pose != 'log':
            cfg_text += '\nplayer ' + ' '.join(args.pose.split('/'))   # the last player line wins
        (game / 'playtest_pose.cfg').write_text(cfg_text + '\n')
        init = game / '__init__.py'
        old = init.read_text(encoding='latin1') if init.exists() else ''
        init.unlink(missing_ok=True)   # the clone shares blocks with the content (cp -c); write a new file
        init.write_text(old + '\nimport playtest_pose\n', encoding='latin1')
    mode = 'gpm_coop' if args.ai else 'gpm_cq'
    maps = [args.map] + ([args.map2] if args.map2 else [])
    (cfg / 'maplist.con').write_text(''.join('mapList.append %s %s 16\n' % (m, mode) for m in maps))

if args.bots is not None:
    SETTINGS['coopBotCount'] = str(args.bots)
if args.bot_ratio is not None:
    SETTINGS['coopBotRatio'] = str(args.bot_ratio)
if args.bot_difficulty is not None:
    SETTINGS['coopBotDifficulty'] = str(args.bot_difficulty)
if args.spawn_time is not None:
    SETTINGS['spawnTime'] = str(args.spawn_time)
    SETTINGS['manDownTime'] = str(args.spawn_time)
t0 = time.monotonic()
def stamp(msg):
    print('[%7.1fs] %s' % (time.monotonic() - t0, msg), flush=True)

server_extra = (['+ai', '1'] if args.ai else []) + shlex.split(args.server_extra)
if args.name_redirect:
    server_extra = ['+gamespy', args.name_redirect] + server_extra
senv = dict(os.environ, LB_OWNER_HOME=str(OWNER_HOME), LB_RUN_OUT=str(OUT), LB_GAME_ROOT=str(content),
            TERM='xterm')
if redirect_wrap:
    senv['LB_WRAP'] = '\n'.join(redirect_wrap)   # server.tcl: the words before the server binary
if args.mod != 'bf2':
    senv['LB_MOD'] = args.mod   # server.tcl: +modPath mods/$LB_MOD (default bf2)
if args.ai_trace:
    senv['BF2_AI_TRACE'] = '1'
# [TEMP] read-only round-state lines ('[round] state a -> b ...') in server.pty.log
senv['BF2_ROUND_TRACE'] = '1'
if args.ai_spawn_only:
    senv['BF2_AI_SPAWN_ONLY'] = '1'
if args.ai and args.no_client and not args.no_spawn_aid:
    # [TEMP] Server-only AI runs: bots take the first spawn wave without a human client.
    senv['BF2_AI_SPAWN_WITHOUT_HUMAN'] = '1'
marker = pathlib.Path(subprocess.check_output(['/usr/bin/mktemp']).decode().strip())
server = None
if not args.no_server:
    if args.no_join:
        stamp('--no-join keeps the dedicated server on 16890; use --no-server for singleplayer')
    clone(run / 'server-root')
    stamp('server content cloned')
    (run / 'server.command.txt').write_text(repr([str(server_bin)] + server_extra) + '\n')
    server = subprocess.Popen(['/usr/bin/expect', str(HERE / 'server.tcl'), str(run), str(server_bin)] + server_extra,
                              env=senv, stdout=(run / 'server.expect.log').open('wb'), stderr=subprocess.STDOUT,
                              start_new_session=True)
else:
    stamp('client only: ports 16890/29911 reserved for singleplayer hosting')

client_done = None
if not args.no_client:
    clone(run / 'client-root')
    stamp('client content cloned')
home = run / 'client-home'
support = home / 'Library/Application Support/OpenBF2'
profiles = support / 'Profiles'
profiles.mkdir(parents=True)
# [TEMP] Placeholder for offline loopback challenges; never copies the owner's real key.
# An xpack server's challenge also asks for the Special Forces key (GameClient::handleChallenge,
# getExpansionProductName "xpack" -> "Battlefield 2 Special Forces"); without it the client
# disconnects (reason 7) before answering and the server keeps the player "connecting".
(support / 'Registry.con').write_text(
    'HKEY_LOCAL_MACHINE\\SOFTWARE\\Electronic Arts\\EA Games\\Battlefield 2\\ergc = LOOPBACKTESTKEY00000\n'
    + ('HKEY_LOCAL_MACHINE\\SOFTWARE\\Electronic Arts\\EA Games\\Battlefield 2 Special Forces\\ergc = LOOPBACKTESTKEY00001\n'
       if args.mod.lower() == 'xpack' else ''))
# A password-free local profile whose name matches +playerName, so BF2Engine::startGame's
# quickLogin(GSPlayerName, "") finds it (LocalProfileManager::quickLogin). The settings files
# only have to exist (validateProfileFiles); empty files keep every default.
prof = profiles / '0001'
prof.mkdir()
if args.play_demo:
    # The demo and its config go into the client root (the working directory): loadDemoConfig turns
    # '/' into '\\' before it opens the config, so the config must be a bare name there. Not into
    # @HOME@/Profiles/Default/demos/ (the menu list): creating Profiles/Default before the first start
    # makes LocalProfileManager::init skip createDefaultProfile, so no audio settings exist and the
    # sound engine never initializes (run-20261112-012928 vs run-20261112-014943).
    shutil.copyfile(args.play_demo, run / 'client-root' / args.play_demo.name)
    (run / 'client-root' / 'demo-play.con').write_text(
        'demo.demofile ' + args.play_demo.name + '\n' + ''.join(line + '\n' for line in args.demo_config_line))
    stamp('demo for playback: ' + str(run / 'client-root' / args.play_demo.name))
# [TEMP] [CHOSEN, not recovered] Model a previously created/used profile, whose
# directory retail saves as GlobalSettings.defaultUser. The native localProfiles
# list marks that directory selected on startup and after Manage -> Back; without
# Global.con this fixture instead models an account with no saved selection.
(profiles / 'Global.con').write_text(
    'GlobalSettings.setDefaultUser "%s"\nGlobalSettings.setNamePrefix ""\n' % prof.name)
(prof / 'Profile.con').write_text(
    'LocalProfile.setName "%s"\nLocalProfile.setNick "%s"\nLocalProfile.setGamespyNick ""\n'
    'LocalProfile.setEmail ""\nLocalProfile.setTotalPlayedTime 0\nLocalProfile.setNumTimesLoggedIn 1\n'
    % (args.player, args.player))
for name in ('General.con', 'ServerSettings.con', 'Audio.con', 'Controls.con', 'Video.con', 'Haptic.con'):
    (prof / name).write_text('')
if args.default_profile == 'default':
    default = profiles / 'default'
    default.mkdir()
    (default / 'profile.con').write_text(
        'LocalProfile.setName "Default"\nLocalProfile.setNick "Default"\nLocalProfile.setGamespyNick ""\n'
        'LocalProfile.setTotalPlayedTime 0\nLocalProfile.setNumTimesLoggedIn 0\n')
    for name in ('audio.con', 'video.con', 'serversettings.con'):
        (default / name).write_text('')
if args.tutorial_messages is not None:
    (prof / 'General.con').write_text('GeneralSettings.setShowTutorialMessages %d\n' %
                                    (args.tutorial_messages == 'on'))
if args.video_high:
    (prof / 'Video.con').write_text(
        'VideoSettings.setTerrainQuality 3\nVideoSettings.setGeometryQuality 3\n'
        'VideoSettings.setLightingQuality 3\nVideoSettings.setDynamicLightingQuality 3\n'
        'VideoSettings.setDynamicShadowsQuality 3\nVideoSettings.setEffectsQuality 3\n'
        'VideoSettings.setTextureQuality 3\nVideoSettings.setTextureFilteringQuality 3\n'
        'VideoSettings.setResolution 800x600\nVideoSettings.setAntialiasing %s\n'
        'VideoSettings.setViewDistanceScale 1\nVideoSettings.setVideoOptionScheme 3\n'
        % (args.antialiasing or 'Off'))
if args.video_con:
    (prof / 'Video.con').write_bytes(args.video_con.read_bytes())
elif args.antialiasing and not args.video_high:
    (prof / 'Video.con').write_text('VideoSettings.setAntialiasing %s\n' % args.antialiasing)
# The shipped default control maps, as a profile the game saved holds them: with an empty
# Controls.con the profile load leaves the maps without mouse/keyboard mappings (the HUD pointer
# and mouse look never move). Scripted input needs them (docs/macos-c-input-script.md).
# mods/bf2/Settings/Controls.con is what ControlSettings::setDefaults runs (all seven maps:
# infantry, land, air, helicopter, sea, default game input); Settings/Default/Controls.con holds
# only the default game-input map, so with it the soldier ignored fire, movement and mouse look.
default_controls = content / 'mods/bf2/Settings/Controls.con'
if default_controls.is_file():
    (prof / 'Controls.con').write_bytes(default_controls.read_bytes())
if args.default_profile is None and (content / 'Profiles/Local/Default').is_dir():
    subprocess.run(['/bin/cp', '-Rc', str(content / 'Profiles/Local/Default'), str(profiles / 'Default')], check=True)
if args.profiles_from:
    previous = args.profiles_from / 'client-home/Library/Application Support/OpenBF2/Profiles'
    if not previous.is_dir():
        sys.exit('no Profiles in ' + str(args.profiles_from))
    shutil.rmtree(profiles)
    subprocess.run(['/bin/cp', '-Rc', str(previous), str(profiles)], check=True)

deadline = time.monotonic() + 300
while server is not None and not (run / 'server.playing').exists():
    if server.poll() is not None or (run / 'server.result').exists() or time.monotonic() > deadline:
        stamp('server did not reach playing: ' + ((run / 'server.result').read_text().strip()
                                                  if (run / 'server.result').exists() else 'no result'))
        break
    time.sleep(0.5)
else:
    if server is not None:
        stamp('server playing')

bot_info_status = None
if args.check_bot_info and (run / 'server.playing').exists():
    # [TEMP] [CHOSEN, not recovered] Check the real snapshot through the same
    # loopback-only query permission the movie's server browser requires.
    with (run / 'server-info-bots.log').open('wb') as log:
        try:
            checked = subprocess.run(sandbox(run / 'client.sb') + [sys.executable,
                str(REPO / 'Code/Tests/server_info_bots.py'), str(server_bin.parent.parent),
                '--query', '--expect', str(args.bot_ratio), str(args.bots), str(args.bot_difficulty)],
                stdout=log, stderr=subprocess.STDOUT, timeout=60)
            bot_info_status = checked.returncode
        except subprocess.TimeoutExpired:
            bot_info_status = 124
        except OSError as error:
            log.write(('Bot query could not start: %s\n' % error).encode())
            bot_info_status = 1
    stamp('server-info bot query exit=%d; log: %s' % (bot_info_status, run / 'server-info-bots.log'))

status = None
if (args.no_server or (run / 'server.playing').exists()) and not args.no_client and bot_info_status in (None, 0):
    env = dict(os.environ, HOME=str(home), BF2_PORT='16890')
    # [TEMP] Effects compile natively into a persistent cache under out/ (BF2_FX_NATIVE_CACHE), as the
    # owner's real HOME keeps one. Under the fresh run HOME every run started cold: ~1300 synchronous
    # effect compiles in the first minutes of play. Pass
    # --client-env BF2_FX_NATIVE_CACHE= for the old cold-cache-under-HOME behaviour.
    env['BF2_FX_NATIVE_CACHE'] = str(OUT / 'fx-native-cache')
    # [CHOSEN, not recovered] The loopback server announces VoIP (ServerSettings sv.voipEnabled 1), so
    # the client starts its voice session. The synthetic capture device keeps gate runs off the
    # microphone and away from the macOS permission prompt (owner, 2026-11-13). Pass
    # --client-env BF2_VOIP_CAPTURE= to use the real devices.
    env['BF2_VOIP_CAPTURE'] = 'fake'
    if args.no_server:
        env.pop('BF2_MAC_COMMANDLINE_JOIN', None)
    if args.ai_spawn_only:
        env['BF2_AI_SPAWN_ONLY'] = '1'
    if not args.menu_join and not args.no_join:
        env['BF2_MAC_COMMANDLINE_JOIN'] = '1'
    if args.mark_session:
        env['BF2_PLAY_TRACE'] = '1'
        stamp('mark session: in game, type "mark NAME" in chat (J / K); marks are appended to ' +
              str(run / 'server-root/mods/bf2/python/game/playtest_marks.txt'))
    if script:
        # [CHOSEN, not recovered] 'include NAME' lines pull in another script from scripts/
        # (or a path, or the including file's directory), so the play scripts share join-and-spawn.
        def expand(path, depth=0):
            out = []
            for line in path.read_text().splitlines():
                word = line.strip().split(None, 1)
                if len(word) == 2 and word[0] == 'include' and depth < 8:
                    name = word[1] + ('' if word[1].endswith('.txt') else '.txt')
                    # a path, then the including file's directory (scenarios), then scripts/
                    inc = next((c for c in (pathlib.Path(word[1]), path.parent / name) if c.is_file()),
                               HERE / 'scripts' / name)
                    out += ['# >>> include ' + word[1]] + expand(inc, depth + 1) + ['# <<< include ' + word[1]]
                else:
                    out.append(line)
            return out
        (run / 'input-script.txt').write_text('\n'.join(expand(script)) + '\n')
        env['BF2_INPUT_SCRIPT'] = str(run / 'input-script.txt')
        env['BF2_INPUT_SCRIPT_LOG'] = str(run / 'client.log')
        # [TEMP] read-only once-a-second '[PLAY]' lines (local player, teams, sound engine)
        env['BF2_PLAY_TRACE'] = '1'
    if args.hitch_trace:
        env['BF2_HITCH_TRACE'] = str(run / 'hitch.trace')
    for item in args.client_env:
        key, _, value = item.partition('=')
        env[key] = value
    if script or args.present_png:
        (run / 'present').mkdir(exist_ok=True)
        env['BF2_PRESENT_PNG'] = str(run / 'present')
        env['BF2_PRESENT_PNG_EVERY'] = str(args.present_every or (600 if script else 60))
    debugger = (['/usr/bin/lldb', '--batch', '-o', 'process handle SIGPIPE -n true -p true -s false']
                + [x for c in args.lldb_cmd for x in ('-o', c)] + ['-o', 'run', '-k', 'thread backtrace all'] + [x for c in args.lldb_post for x in ('-k', c)]
                + ['-k', 'quit 1', '--']
                if args.lldb else [])
    command = sandbox(run / 'client.sb') + debugger + redirect_wrap + [str(client_bin)] + (
        ['+gamespy', args.name_redirect] if args.name_redirect else []) + ['+modPath', 'mods/' + args.mod, '+fullscreen', '1' if args.fullscreen else '0'] + ([] if args.profile_resolution else
        ['+szx', '800', '+szy', '600']) + ([] if args.movies else ['+restart', '1']) + [
        ] + ([] if args.no_join else ['+joinServer', '127.0.0.1', '+port', '16890']) + (
        ['+password', args.client_password if args.client_password is not None else args.server_password]
        if (args.client_password is not None or args.server_password) and not args.no_join else []) + (
        ['+port', '16890'] if args.no_server else []) + [
        '+playerName', args.player] + (['+demo', 'demo-play.con'] if args.play_demo else []) + shlex.split(args.client_args)
    (run / 'client.command.txt').write_text(repr(command) + '\nHOME=' + str(home) +
                                            '\nEffects=native (cache ' + (env.get('BF2_FX_NATIVE_CACHE') or 'FxNative under HOME') + ')' +
                                            '\nBF2_INPUT_SCRIPT=' + env.get('BF2_INPUT_SCRIPT', '(none)') + '\n')
    stamp('client start (%d s)' % args.client_seconds)
    started = time.monotonic()
    timed_out = False
    with (run / 'client.log').open('wb') as log:
        child = subprocess.Popen(command, cwd=run / 'client-root', env=env, stdout=log,
                                 stderr=subprocess.STDOUT, start_new_session=True)
        # A SIGTERM/SIGHUP to this script (e.g. a tool timeout) must still stop the client.
        def _stop_on_signal(signum, frame):
            raise SystemExit(128 + signum)
        signal.signal(signal.SIGTERM, _stop_on_signal)
        signal.signal(signal.SIGHUP, _stop_on_signal)
        status = None
        if args.server_send:
            # [TEMP] c-swf-disconnect: server commands while the client plays (e.g. quit = server loss).
            import threading
            def _sender():
                t0 = time.monotonic()
                if args.server_send_after:
                    while args.server_send_after not in (run / 'client.log').read_text(errors='replace'):
                        if time.monotonic() - started > args.client_seconds or child.poll() is not None:
                            return
                        time.sleep(1)
                    t0 = time.monotonic()
                    stamp('client log: ' + args.server_send_after)
                for at, command in sorted((float(i.partition(':')[0]), i.partition(':')[2]) for i in args.server_send):
                    time.sleep(max(0.0, at - (time.monotonic() - t0)))
                    if child.poll() is not None:
                        return
                    if command == '!kill':
                        # [TEMP] an abrupt server death (as a [FRONTIER] abort): SIGKILL the server binary.
                        subprocess.run(['/usr/bin/pkill', '-KILL', '-f', re.escape(str(server_bin)) + '( |$)'])  # not bf2_client
                        stamp('server killed (SIGKILL)')
                        continue
                    (run / 'server.send').write_text(command + '\n')
                    stamp('server console: ' + command)
            threading.Thread(target=_sender, daemon=True).start()
        try:
            status = child.wait(timeout=args.client_seconds)
        except subprocess.TimeoutExpired:
            timed_out = True
        finally:
            if status is None:
                # Under --lldb, debugserver starts the client in its own session, so killpg on
                # lldb's group misses it and the client outlives the run (seen 2026-11-01: two
                # orphaned clients in the menu at ~60% CPU each). Kill the whole process tree.
                tree = descendants(child.pid)
                signal_tree(child.pid, tree, signal.SIGTERM)
                try:
                    status = child.wait(timeout=3)
                except subprocess.TimeoutExpired:
                    pass
                signal_tree(child.pid, tree + descendants(child.pid), signal.SIGKILL)
                if status is None:
                    status = child.wait()
    (run / 'client.result').write_text('exit={} timeout={} duration={:.2f}s\n'.format(
        status, timed_out, time.monotonic() - started))
    stamp('client ' + (run / 'client.result').read_text().strip())
elif args.no_client and (run / 'server.playing').exists() and bot_info_status in (None, 0):
    sends = sorted((float(item.partition(':')[0]), item.partition(':')[2]) for item in args.server_send)
    started = time.monotonic()
    for at, command in sends:
        time.sleep(max(0.0, at - (time.monotonic() - started)))
        if command == '!kill':
            # [TEMP] an abrupt server death, e.g. under a retail client joined by hand.
            subprocess.run(['/usr/bin/pkill', '-KILL', '-f', re.escape(str(server_bin)) + '( |$)'])
            stamp('server killed (SIGKILL)')
            continue
        (run / 'server.send').write_text(command + '\n')
        stamp('server console: ' + command)
    time.sleep(max(0.0, args.client_seconds - (time.monotonic() - started)))

if server is not None:
    (run / 'server.stop').write_text('')
    try:
        server.wait(timeout=45)
    except subprocess.TimeoutExpired:
        os.killpg(server.pid, signal.SIGKILL)
        server.wait()
    stamp('server ' + ((run / 'server.result').read_text().strip() if (run / 'server.result').exists() else 'no result'))
# --auto-record: the server's recordings (Demo::recordDemo through AutoRecorder), next to the logs.
if args.auto_record and (run / 'server-root').is_dir():
    recorded = sorted(f for f in (run / 'server-root').rglob('*') if f.suffix.lower() == '.bf2demo')
    if recorded:
        (run / 'demos').mkdir(exist_ok=True)
    for f in recorded:
        shutil.copyfile(f, run / 'demos' / f.name)
        stamp('recorded demo %s (%d bytes): %s' % (f.relative_to(run / 'server-root'), f.stat().st_size,
                                                   run / 'demos' / f.name))
    if not recorded:
        stamp('--auto-record: no .bf2demo in server-root')
# [TEMP] play-test fixture outputs, copied next to the logs.
for name in ('playtest_pose.log', 'playtest_marks.txt'):
    src = run / 'server-root/mods/bf2/python/game' / name
    if src.is_file():
        (run / name).write_bytes(src.read_bytes())
        stamp(name + ': ' + str(run / name))
        if name == 'playtest_marks.txt':
            print(src.read_text(errors='replace'))

# Crash reports newer than the run start, from this run's binaries only: another track's death
# tests (bf2_*_check) write bf2*.ips reports into the same directory at the same time.
own_reports = (server_bin.name + '-', client_bin.name + '-')
for i in range(10 if status not in (None, 0, -15) else 1):
    found = [f for f in (OWNER_HOME / 'Library/Logs/DiagnosticReports').glob('bf2*.ips')
             if f.name.startswith(own_reports) and f.stat().st_mtime > marker.stat().st_mtime]
    if found:
        for f in found:
            (run / f.name).write_bytes(f.read_bytes())
            stamp('crash report ' + f.name)
            try:
                import json
                j = json.loads(f.read_text().split('\n', 1)[1])
                e = j.get('exception', {})
                print('  ', e.get('type'), e.get('signal'), e.get('subtype', ''))
                th = [t for t in j['threads'] if t.get('triggered')][0]
                im = j['usedImages']
                for fr in th['frames'][:18]:
                    n = im[fr['imageIndex']].get('name', '?') if fr.get('imageIndex') is not None else '?'
                    print('   %-16s %s +%s' % (n, fr.get('symbol', '0x%x' % fr.get('imageOffset', 0)),
                                             fr.get('symbolLocation', '')))
            except Exception as error:
                print('   (crash report unreadable: %s)' % error)
        break
    time.sleep(1)
marker.unlink()
if (run / 'client.log').exists():
    lines = (run / 'client.log').read_text(errors='replace').splitlines()
    if script:
        steps = [l for l in lines if '[INPUT-SCRIPT]' in l]
        print('--- input script: %d report lines; last 6' % len(steps))
        for line in steps[-6:]:
            print('   ' + line[:220])
    if (run / 'present').is_dir():
        print('--- captures: %d in %s' % (len(list((run / 'present').glob('*.png'))), run / 'present'))
    print('--- client.log: %d lines; last 12' % len(lines))
    for line in lines[-12:]:
        print('   ' + line[:220])
print('Run artifacts: ' + str(run))
if args.check_bot_info and bot_info_status != 0:
    sys.exit('Server-info bot query did not pass; see server-info-bots.log and server logs.')
