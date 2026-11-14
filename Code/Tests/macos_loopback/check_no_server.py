# [TEMP] [CHOSEN, not recovered] Orchestration-only check. No game process or socket runs.
import contextlib
import datetime
import io
import os
import pathlib
import pwd
import runpy
import shutil
import signal
import subprocess
import sys
import tempfile
import time
from types import SimpleNamespace
from unittest.mock import patch

repo = pathlib.Path(__file__).resolve().parents[3]
runner = repo / 'tools/macos/loopback/loopback.py'

def check(label, options, sandbox_ok=True):
    with tempfile.TemporaryDirectory(prefix='ui-spstart-check-', dir='/private/tmp') as tmp:
        owner = pathlib.Path(tmp)
        lock = owner / 'Library/Caches/OpenBF2-loopback/loopback.lock'
        lock.parent.mkdir(parents=True)
        lock.write_text('existing lock\n')
        content = owner / 'content'
        settings = content / 'mods/bf2/Settings'
        settings.mkdir(parents=True)
        (settings / 'ServerSettings.con').write_text('sv.serverPort 16567\n')
        (settings / 'Controls.con').write_text('')
        bins = owner / 'bin'
        bins.mkdir()
        (bins / 'bf2_client').touch()
        if '--no-server' not in options:
            (bins / 'bf2').touch()
        run_name = 'test-ui-spstart-%d-%s' % (os.getpid(), label)
        run = repo / 'out/loopback' / ('run-' + run_name)
        assert not run.exists(), run
        calls = []

        class Process:
            pid = 999999
            def __init__(self, command, **kwargs):
                calls.append((command, kwargs))
                if command[0] == '/usr/bin/expect':
                    (run / 'server.playing').touch()
                    (run / 'server.result').write_text('orchestration fixture only\n')
            def poll(self):
                return None
            def wait(self, timeout=None):
                return 0

        def external(command, **kwargs):
            if command[0] == '/usr/sbin/lsof':
                return SimpleNamespace(stdout='')
            if command[0] == '/usr/bin/sandbox-exec':
                assert command[-1] == '/usr/bin/true'
                return SimpleNamespace(returncode=0 if sandbox_ok else 71, stderr='test preflight refusal')
            assert command[0] == '/bin/cp', command
            src, dest = map(pathlib.Path, command[-2:])
            if src.is_dir():
                shutil.copytree(src, dest)
            else:
                shutil.copyfile(src, dest)
            return SimpleNamespace(returncode=0)

        class Date:
            @staticmethod
            def now():
                return SimpleNamespace(strftime=lambda fmt: run_name)

        argv = [str(runner), '--content', str(content), '--build-dir', str(bins), '--client-seconds', '0'] + options
        try:
            with patch.object(sys, 'argv', argv), patch.object(pwd, 'getpwuid', return_value=SimpleNamespace(pw_dir=tmp)), \
                 patch.object(datetime, 'datetime', Date), patch.object(subprocess, 'Popen', Process), \
                 patch.object(subprocess, 'run', external), \
                 patch.object(subprocess, 'check_output', return_value=(str(owner / 'marker')+'\n').encode()), \
                 patch.object(signal, 'signal'), \
                 patch.dict(os.environ, {'BF2_MAC_COMMANDLINE_JOIN': '1'} if '--no-server' in options else {}), \
                 patch.object(time, 'sleep'), contextlib.redirect_stdout(io.StringIO()):
                (owner / 'marker').touch()
                try:
                    runpy.run_path(str(runner), run_name='__main__')
                except SystemExit as error:
                    assert not sandbox_ok and 'Sandbox unavailable' in str(error), error
            if not sandbox_ok:
                assert not calls
                assert not (run / 'client-root').exists()
                assert not (run / 'server-root').exists()
            else:
                clients = [c for c in calls if c[0][0] == '/usr/bin/sandbox-exec']
                servers = [c for c in calls if c[0][0] == '/usr/bin/expect']
                assert len(clients) == 1
                command, kwargs = clients[0]
                if '--no-server' in options:
                    assert not servers and not (run / 'server-root').exists()
                    assert not (run / 'server.playing').exists()
                    assert not (run / 'server.stop').exists()
                    assert '+joinServer' not in command
                    assert command[command.index('+port') + 1] == '16890'
                    assert 'BF2_MAC_COMMANDLINE_JOIN' not in kwargs['env']
                    assert 'localhost:*' in (run / 'client.sb').read_text()
                    if '--ai-spawn-only' in options:
                        assert kwargs['env']['BF2_AI_SPAWN_ONLY'] == '1'
                else:
                    assert len(servers) == 1 and (run / 'server.stop').exists()
                    assert ('+joinServer' in command) == ('--no-join' not in options)
                    assert 'localhost:16890' in (run / 'client.sb').read_text()
                cfg = run / 'client-root/mods/bf2/Settings/ServerSettings.con'
                assert 'sv.serverPort 16890' in cfg.read_text()
                assert 'sv.serverIP "127.0.0.1"' in cfg.read_text()
                assert 'sv.gameSpyPort 29911' in cfg.read_text()
                assert (settings / 'ServerSettings.con').read_text() == 'sv.serverPort 16567\n'
            assert lock.read_text() == 'existing lock\n'
            print('PASS', label, '(orchestration only)')
        finally:
            if run.exists():
                shutil.rmtree(run)

check('client-only', ['--no-server'])
check('client-spawn-only', ['--no-server', '--ai-spawn-only'])
check('mp-default', [])
check('mp-menu', ['--no-join'])
check('sandbox-refusal', ['--no-server'], False)
