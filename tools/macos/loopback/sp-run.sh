#!/bin/bash
# [TEMP] [CHOSEN, not recovered] Client-hosted singleplayer run: holds the machine-wide loopback
# lock (the same flock loopback.py takes; BF2_LOOPBACK_LOCK overrides it) for the whole run, waits
# until UDP 16890/29911 are free, then calls the client runner named by RUNNER.
#
# Why: a client-hosted SP game binds 127.0.0.1:16890 only ~10 s after launch (when PLAY is clicked).
# loopback.py checks the ports once at its start, so without this lock another agent's dedicated
# server could take 16890 in that gap; NetServer::init then fails with EADDRINUSE, startServer
# returns false and the menu reloads onto the SP page (docs/macos-c-input-script.md).
#
# usage: RUNNER=<client runner> sp-run.sh <worktree> <build-tag>   (env as for the runner: BF2_RUN_SECONDS, BF2_INPUT_SCRIPT, ...)
# Waits at most 30 min for the lock and 10 min for the ports.
exec /usr/bin/python3 -I - "$@" <<'PY'
import fcntl, os, subprocess, sys, time
runner = os.environ.get('RUNNER')
if not runner:
    sys.exit('[sp-run] set RUNNER to a client runner called as <runner> <worktree> <build-tag>')
lock_path = os.environ.get('BF2_LOOPBACK_LOCK') or os.path.expanduser('~/Library/Caches/OpenBF2-loopback/loopback.lock')
os.makedirs(os.path.dirname(lock_path), exist_ok=True)
lock = open(lock_path, 'a')
deadline = time.monotonic() + 1800
while True:
    try:
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        break
    except BlockingIOError:
        if time.monotonic() > deadline:
            sys.exit('[sp-run] loopback.lock still held after 30 min; nothing started')
        time.sleep(5)
def busy():
    return subprocess.run(['/usr/sbin/lsof', '-nP', '-iUDP:16890', '-iTCP:16890', '-iUDP:29911', '-t'],
                          capture_output=True, text=True).stdout.strip()
for _ in range(120):
    if not busy():
        break
    time.sleep(5)
else:
    sys.exit('[sp-run] UDP 16890 or 29911 still in use after 10 min (pid %s); nothing started' % busy().replace('\n', ','))
sys.exit(subprocess.call([runner] + sys.argv[1:]))
PY
