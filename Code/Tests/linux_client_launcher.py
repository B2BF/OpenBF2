#!/usr/bin/env python3
"""[TEMP] Synthetic launcher lifecycle/port checks; no game, graphics or microphone use."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import signal
import socket
import subprocess
import sys
import tempfile
import time
import zipfile


CHILD = r'''#!/usr/bin/env python3
import json, os, pathlib, signal, subprocess, sys, time
mode = os.environ['BF2_LAUNCHER_TEST_MODE']
record = pathlib.Path(os.environ['BF2_LAUNCHER_TEST_CHILD'])
if mode == 'ignore-term':
    signal.signal(signal.SIGTERM, signal.SIG_IGN)
descendant = None
if mode == 'descendant':
    descendant = subprocess.Popen([sys.executable, '-c',
        'import signal,time; signal.signal(signal.SIGTERM, signal.SIG_IGN); time.sleep(60)'])
    time.sleep(0.1)
record.write_text(json.dumps({'pid': os.getpid(), 'pgid': os.getpgrp(),
                             'descendant': descendant.pid if descendant else None,
                             'argv': sys.argv[1:]}))
if mode == 'metadata':
    video = pathlib.Path(os.environ['XDG_DATA_HOME']) / 'OpenBF2/Profiles/Default/Video.con'
    video.write_text('VideoSettings.setAntialiasing 8Samples\nLocalProfile.setPassword secret-child-value\n')
    (pathlib.Path(os.environ['BF2_FX_NATIVE_CACHE']) / 'compiled.bin').write_bytes(b'cache')
    (pathlib.Path(os.environ['DXVK_LOG_PATH']) / 'synthetic-client_d3d9.log').write_text(
        'info:  DXVK: v3.1.1\ninfo:  Found device: Synthetic GPU\n'
        'info:  Creating device:\ninfo:  Synthetic GPU:\ninfo:    Driver   : synthetic 1.0\n'
        'info:  Presenter: Actual swapchain properties:\ninfo:    Buffer size:  1024x768\n'
        'info:    Present mode: VK_PRESENT_MODE_FIFO_KHR\nsecret-driver-value\n')
if mode in ('normal', 'metadata'):
    time.sleep(0.1)
    raise SystemExit(0)
while True:
    time.sleep(0.05)
'''


def running(pid):
    try:
        state = Path(f"/proc/{pid}/stat").read_text().split(") ", 1)[1].split()[0]
        return state != "Z"
    except FileNotFoundError:
        return False


def unused_port():
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as probe:
        probe.bind(("127.0.0.1", 0))
        port = probe.getsockname()[1]
    if port == 16567:
        return unused_port()
    return port


def wait_ready(process, record, limit=10):
    deadline = time.monotonic() + limit
    while time.monotonic() < deadline:
        if record.exists():
            return json.loads(record.read_text())
        if process.poll() is not None:
            raise AssertionError("launcher exited before synthetic child started")
        time.sleep(0.02)
    raise AssertionError("synthetic child did not start")


def run_case(launcher, root, name, mode="hang", sent=None, repeat_signal=False, seconds=None,
             marker_port=None, config_port=None, profile_port=None, expect_refusal=False,
             singleplayer_map=None, map_mode="sp1", login_profile="offline"):
    case = root / name
    case.mkdir()
    content = case / "content"
    settings = content / "mods/bf2/Settings"
    settings.mkdir(parents=True)
    port = marker_port if marker_port is not None else unused_port()
    (content / ".openbf2-linux-test-content.json").write_text(json.dumps(
        {"source": str(case / "synthetic-reference"), "server_port": port}))
    (settings / "ServerSettings.con").write_text(f"sv.serverPort {port if config_port is None else config_port}\n")
    state = case / "state"
    saved_video = None
    if singleplayer_map is not None:
        level = content / "mods/bf2/Levels/Operation_Clean_Sweep"
        level.mkdir(parents=True)
        with zipfile.ZipFile(level / "server.zip", "w") as archive:
            archive.writestr(f"GameModes/{map_mode}/16/GamePlayObjects.con", "rem synthetic map contract\n")
        profiles = state / "OpenBF2/Profiles"
        profile = profiles / "0001"
        profile.mkdir(parents=True)
        (profiles / "Global.con").write_text('GlobalSettings.setDefaultUser "0001"\n')
        profile_name = "Offline Fixture" if login_profile != "unsafe-name" else "Offline+port 16567"
        online_name = "online-account" if login_profile == "online" else ""
        (profile / "Profile.con").write_text(
            f'LocalProfile.setName "{profile_name}"\nLocalProfile.setGamespyNick "{online_name}"\n')
        saved_video = b"VideoSettings.setTerrainQuality 3\nVideoSettings.setAntialiasing 4Samples\n"
        (profile / "Video.con").write_bytes(saved_video)
    if mode == "metadata":
        profile = state / "OpenBF2/Profiles/Default"
        profile.mkdir(parents=True)
        (profile / "Video.con").write_text("VideoSettings.setAntialiasing Off\nLocalProfile.setPassword secret-start-value\n")
        (profile / "Profile.con").write_text("secret-profile-value\n")
        (content / "dxvk.conf").write_text("secret-config-value\n")
    if profile_port is not None:
        profile = state / "OpenBF2/Profiles/Default"
        profile.mkdir(parents=True)
        (profile / "ServerSettings.con").write_text(f"sv.serverPort {profile_port}\n")
    binary = case / "synthetic-client"
    binary.write_text(CHILD)
    binary.chmod(0o755)
    child_record = case / "child.json"
    command = [sys.executable, str(launcher), str(binary), "--content", str(content),
               "--state", str(state), "--output-root", str(case / "runs"), "--purpose", "Synthetic launcher check"]
    if seconds is not None:
        command += ["--seconds", str(seconds)]
    if singleplayer_map is not None:
        command += ["--singleplayer-map", singleplayer_map]
    env = dict(os.environ, BF2_LAUNCHER_TEST_MODE=mode, BF2_LAUNCHER_TEST_CHILD=str(child_record))
    if mode == "metadata":
        env.update(DXVK_CONFIG_FILE="dxvk.conf", DXVK_CONFIG="secret-inline-value", MESA_SHADER_CACHE_DISABLE="true")
    child = None
    with (case / "launcher.log").open("w") as log:
        process = subprocess.Popen(command, stdout=log, stderr=subprocess.STDOUT, env=env)
        try:
            if expect_refusal:
                code = process.wait(timeout=10)
                assert code != 0 and not child_record.exists(), "invalid launch accepted and child started"
                return
            child = wait_ready(process, child_record)
            time.sleep(0.1)  # The manifest and signal handlers must be ready before interruption.
            if sent:
                process.send_signal(sent)
                if repeat_signal:
                    time.sleep(0.3)
                    process.send_signal(sent)
            process.wait(timeout=15)
            manifest, = (case / "runs").glob("*/run.json")
            result = json.loads(manifest.read_text())
            assert not running(child["pid"]), "direct child left running"
            if child["descendant"]:
                assert not running(child["descendant"]), "descendant left running"
            assert result.get("diagnostic_timeout") is (seconds is not None and mode not in ("normal", "metadata")), "wrong timeout metadata"
            expected_signal = signal.Signals(sent).name if sent else None
            assert result.get("interrupted") == expected_signal, "wrong/missing interruption metadata"
            if mode in ("normal", "metadata"):
                assert result.get("exit_code") == 0, "normal quit did not preserve successful exit"
            assert "utc_end" in result, "final metadata missing"
            port_argument = child["argv"].index("+port")
            assert child["argv"][port_argument + 1] == str(port), "validated port missing from child command"
            if singleplayer_map is not None:
                assert child["argv"].count("+port") == 1, "singleplayer arguments replaced protected port"
                expected = ["+hostServer", "1", "+loadLevel", "Operation_Clean_Sweep", "+ai", "1",
                            "+gameMode", "sp1", "+maxPlayers", "16", "+playerName", "Offline Fixture"]
                assert child["argv"][-len(expected):] == expected, "singleplayer command-line contract changed"
                assert (state / "OpenBF2/Profiles/0001/Video.con").read_bytes() == saved_video, "saved video settings changed"
            if mode == "metadata":
                def aa(phase):
                    return result[f"saved_video_{phase}"]["profiles"]["Default"]["commands"][0]["value"]
                assert aa("start") == "Off" and aa("end") == "8Samples", "saved video snapshots did not bracket the process"
                assert "not verified applied" in result["saved_video_end"]["meaning"], "saved settings claimed as applied"
                assert result["requested_display"] == {"width": 1280, "height": 800, "fullscreen": False}
                assert result["cache_start"]["files"] == 0 and result["cache_end"]["bytes"] == 5, "cache inventory did not bracket the process"
                assert "do not establish" in result["cache_end"]["meaning"], "cache warmth inferred from file counts"
                assert result["graphics_overrides"]["config_file"]["sha256"] == hashlib.sha256(b"secret-config-value\n").hexdigest()
                assert result["graphics_overrides"]["environment"]["MESA_SHADER_CACHE_DISABLE"] == "true"
                records = result["graphics_evidence"]["records"]
                assert any(r["kind"] == "actual_presenter" and "1024x768" in r["text"] and r["line"] == 7 for r in records)
                assert any(r["kind"] == "selected_device" and "Synthetic GPU" in r["text"] for r in records)
                assert "power_start" in result and "power_end" in result and result["power"] == "unknown"
                assert "secret-" not in manifest.read_text(), "non-video or arbitrary config/log text leaked into metadata"
        finally:
            # Clean only processes created by this fixture, including counterexamples.
            if child is None and child_record.exists():
                child = json.loads(child_record.read_text())
            if child:
                try:
                    os.killpg(child["pgid"], signal.SIGKILL)
                except ProcessLookupError:
                    pass
            if process.poll() is None:
                process.kill()
            process.wait()


def metadata_contract(repo, root):
    sys.path.insert(0, str(repo / "tools/linux"))
    from run_metadata import cache_snapshot, graphics_evidence, graphics_overrides, power_snapshot, video_snapshot
    case = root / "metadata-contract"
    case.mkdir()
    state = case / "state"
    profiles = state / "OpenBF2/Profiles"
    for name in ("Default", "0001", "private-profile"):
        profile = profiles / name
        profile.mkdir(parents=True)
        (profile / "Video.con").write_text(
            'VideoSettings.setResolution "1280x800@60Hz"\n'
            'VideoSettings.setAntialiasing 4Samples\n'
            'VideoSettings.setResolution secret-invalid-resolution\n'
            'VideoSettings.setPassword secret-unknown-command\n'
            'LocalProfile.setPassword secret-password\n')
        (profile / "Profile.con").write_text("secret-unrelated-file")
    (profiles / "0002").symlink_to(profiles / "Default", target_is_directory=True)
    saved = video_snapshot(state)
    assert set(saved["profiles"]) == {"Default", "0001"}, "non-numeric/symlink profile was read"
    assert len(saved["profiles"]["0001"]["commands"]) == 2, "unknown command or invalid value recorded"
    assert "secret-" not in json.dumps(saved) and saved["errors"], "profile privacy/error evidence failed"
    assert video_snapshot(case / "missing")["status"] == "profiles absent"

    sysfs = case / "sys"
    fields = {"class/power_supply/AC/type": "Mains", "class/power_supply/AC/online": "1",
              "class/power_supply/BAT0/type": "Battery", "class/power_supply/BAT0/status": "Discharging",
              "class/power_supply/BAT0/capacity": "72",
              "class/power_supply/secret-peripheral/type": "Battery", "class/power_supply/secret-peripheral/scope": "Device",
              "class/power_supply/secret-peripheral/capacity": "99",
              "devices/system/cpu/cpu0/cpufreq/scaling_governor": "powersave",
              "class/drm/card0/device/power_dpm_force_performance_level": "auto",
              "class/hwmon/hwmon0/name": "amdgpu", "class/hwmon/hwmon0/power1_cap": "15000000",
              "class/hwmon/hwmon0/power1_cap_max": "invalid"}
    for name, value in fields.items():
        path = sysfs / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(value + "\n")
    power = power_snapshot(sysfs)
    assert any(f["value"] == 15000000 and f["unit"] == "microwatts" for f in power["fields"])
    assert any(f["value"] == 72 and f["unit"] == "percent" for f in power["fields"])
    assert power["errors"] and not power["unknown"] and "secret-peripheral" not in json.dumps(power)
    assert len(power_snapshot(case / "absent-sys")["unknown"]) == 4, "missing power fields were not marked unknown"

    cache = case / "cache"
    cache.mkdir()
    (cache / "one.bin").write_bytes(b"1234")
    (cache / "external").symlink_to(profiles, target_is_directory=True)
    counts = cache_snapshot(cache)
    assert counts["files"] == 1 and counts["bytes"] == 4 and counts["skipped_symlinks"] == 1
    assert graphics_overrides({}, case)["config_file"]["status"] == "absent"
    assert graphics_evidence(case)["status"].startswith("unknown"), "absent logs claimed driver evidence"
    print("PASS metadata privacy, missing fields, power units and cache symlink contracts", flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--scratch", type=Path, required=True)
    parser.add_argument("--edge-cases", action="store_true")
    parser.add_argument("--profile-port-check", action="store_true")
    args = parser.parse_args()
    repo = Path(__file__).resolve().parents[2]
    args.scratch.mkdir(parents=True, exist_ok=True)
    root = Path(tempfile.mkdtemp(prefix="launcher-", dir=args.scratch.resolve()))
    metadata_contract(repo, root)
    cases = [
        ("normal", dict(mode="normal", seconds=2)),
        ("metadata", dict(mode="metadata", seconds=2)),
        ("timeout", dict(seconds=0.4)),
        ("sigint", dict(sent=signal.SIGINT)),
        ("sigterm", dict(sent=signal.SIGTERM)),
        ("reserved-port", dict(marker_port=16567, expect_refusal=True)),
        ("mismatched-config", dict(config_port=16567, expect_refusal=True)),
        ("singleplayer", dict(mode="normal", singleplayer_map="operation_clean_sweep")),
        ("singleplayer-missing-map", dict(singleplayer_map="missing_map", expect_refusal=True)),
        ("singleplayer-missing-mode", dict(singleplayer_map="Operation_Clean_Sweep", map_mode="gpm_cq", expect_refusal=True)),
        ("singleplayer-map-escape", dict(singleplayer_map="../Operation_Clean_Sweep", expect_refusal=True)),
        ("singleplayer-option-escape", dict(singleplayer_map="Operation_Clean_Sweep+port 16567", expect_refusal=True)),
        ("singleplayer-unsafe-profile", dict(singleplayer_map="Operation_Clean_Sweep", login_profile="unsafe-name", expect_refusal=True)),
        ("singleplayer-online-profile", dict(singleplayer_map="Operation_Clean_Sweep", login_profile="online", expect_refusal=True)),
        ("singleplayer-reserved-port", dict(singleplayer_map="Operation_Clean_Sweep", marker_port=16567, expect_refusal=True)),
    ]
    if args.edge_cases:
        cases += [("repeated-sigint", dict(mode="ignore-term", sent=signal.SIGINT, repeat_signal=True)),
                  ("repeated-sigterm", dict(mode="ignore-term", sent=signal.SIGTERM, repeat_signal=True)),
                  ("surviving-descendant", dict(mode="descendant", sent=signal.SIGTERM))]
    if args.profile_port_check:
        cases += [("reserved-profile", dict(profile_port=16567, expect_refusal=True))]
    failures = 0
    for name, options in cases:
        try:
            run_case(repo / "tools/linux/client.py", root, name, **options)
            print("PASS", name, flush=True)
        except (AssertionError, subprocess.TimeoutExpired) as error:
            failures += 1
            print("FAIL", name, str(error), flush=True)
    print(f"{failures} failure(s); artifacts: {root}")
    return bool(failures)


if __name__ == "__main__":
    raise SystemExit(main())
