#!/usr/bin/env python3
"""Capture an issue #36 native-client run using a prepared copy of game content."""
import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import shlex
import signal
import socket
import subprocess
import time
import uuid
import zipfile

from presentation import distribution, source_metadata
from run_metadata import cache_snapshot, graphics_evidence, graphics_overrides, power_snapshot, video_snapshot


def singleplayer_arguments(content, state, requested):
    """Select an existing copied sp1 map and password-free default local profile."""
    if not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9_-]*", requested):
        raise ValueError("--singleplayer-map requires one map directory name")
    levels = content / "mods/bf2/Levels"
    if not levels.is_dir() or not levels.resolve().is_relative_to(content):
        raise ValueError("Copied content has no local Levels directory")
    matches = [path for path in levels.iterdir() if path.name.casefold() == requested.casefold()]
    if len(matches) != 1 or not matches[0].is_dir() or matches[0].is_symlink():
        raise ValueError("--singleplayer-map must identify one existing copied map directory")
    level = matches[0]
    archive = level / "server.zip"
    if archive.is_symlink():
        raise ValueError("Singleplayer map archive must be a copied file")
    with zipfile.ZipFile(archive) as source:
        matches = [item for item in source.infolist()
                   if item.filename.casefold() == "gamemodes/sp1/16/gameplayobjects.con"]
        if len(matches) != 1 or not matches[0].file_size:
            raise ValueError("Singleplayer map requires sp1/16/GamePlayObjects.con in server.zip")

    def saved_value(path, command, default=None):
        if path.is_symlink() or not path.is_file():
            raise ValueError("Singleplayer requires an existing saved default local profile")
        values = []
        for line in path.read_text().splitlines():
            fields = line.split(maxsplit=1)
            if fields and fields[0].casefold() == command.casefold():
                tokens = shlex.split(fields[1]) if len(fields) == 2 else []
                if len(tokens) != 1:
                    raise ValueError("Invalid saved singleplayer profile setting")
                values.append(tokens[0])
        if len(values) > 1 or not values and default is None:
            raise ValueError("Missing or ambiguous saved singleplayer profile setting")
        return values[0] if values else default

    profiles = state / "OpenBF2/Profiles"
    directory = saved_value(profiles / "Global.con", "GlobalSettings.setDefaultUser")
    if not re.fullmatch(r"[0-9]+", directory) or (profiles / directory).is_symlink():
        raise ValueError("Singleplayer requires a numeric saved default profile directory")
    profile = profiles / directory / "Profile.con"
    name = saved_value(profile, "LocalProfile.setName")
    if not re.fullmatch(r"[A-Za-z0-9 _.-]{1,64}", name):
        raise ValueError("Singleplayer profile name contains unsupported command-line characters")
    if saved_value(profile, "LocalProfile.setGamespyNick", "") or saved_value(profile, "LocalProfile.setPassword", ""):
        raise ValueError("Singleplayer automation requires an offline profile without a password")
    return ["+hostServer", "1", "+loadLevel", level.name, "+ai", "1", "+gameMode", "sp1",
            "+maxPlayers", "16", "+playerName", name]


def stop_child(process):
    # The session/process group belongs to this launch. A reaped leader can still leave
    # compiler/helper descendants behind, so terminate the group even after leader exit.
    try:
        os.killpg(process.pid, signal.SIGTERM)
    except ProcessLookupError:
        pass
    try:
        process.wait(timeout=5)
    except subprocess.TimeoutExpired:
        pass
    try:
        # Also catches descendants that ignored TERM while the leader exited normally.
        os.killpg(process.pid, signal.SIGKILL)
    except ProcessLookupError:
        pass
    process.wait(timeout=5)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("binary", type=Path)
    parser.add_argument("--content", type=Path, required=True)
    parser.add_argument("--runtime", type=Path)
    parser.add_argument("--state", type=Path, default=Path("out/linux-vulkan-client-state"))
    parser.add_argument("--output-root", type=Path, default=Path("out/linux-vulkan-client-runs"))
    parser.add_argument("--purpose", required=True)
    parser.add_argument("--seconds", type=float, help="Optional diagnostic time limit")
    parser.add_argument("--skip-intros", action="store_true",
                        help="Use the existing restart path for menu diagnostics; skips intro audio initialization")
    parser.add_argument("--warm-cache", type=Path)
    parser.add_argument("--detail", choices=("light", "detailed"), default="light")
    parser.add_argument("--power-note", default="unknown")
    parser.add_argument("--width", type=int, default=1280)
    parser.add_argument("--height", type=int, default=800)
    parser.add_argument("--singleplayer-map", metavar="NAME",
                        help="Host an existing copied sp1/16 map using the saved default offline profile")
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    binary = args.binary.resolve(strict=True)
    content = args.content.resolve(strict=True)
    state = args.state.resolve()
    marker = content / ".openbf2-linux-test-content.json"
    if not marker.is_file():
        parser.error("Content must be a prepared test copy with .openbf2-linux-test-content.json")
    prepared = json.loads(marker.read_text())
    if content == Path(prepared["source"]).resolve():
        parser.error("The reference installation cannot be used as the test copy")
    port = int(prepared["server_port"])
    if port == 16567 or not 1024 <= port <= 65535:
        parser.error("Test content requires an unprivileged server port other than 16567")
    def check_port_config(config):
        ports = []
        for line in config.read_text().splitlines():
            fields = line.split()
            if fields and fields[0].casefold() == "sv.serverport":
                ports.append(fields[1] if len(fields) > 1 else None)
        if ports != [str(port)]:
            parser.error(f"{config} does not match the recorded test port")
    check_port_config(content / "mods/bf2/Settings/ServerSettings.con")
    # LocalProfileManager selects this file when opening/creating a local profile.
    profile_config = state / "OpenBF2/Profiles/Default/ServerSettings.con"
    if profile_config.exists():
        check_port_config(profile_config)
    singleplayer = []
    if args.singleplayer_map:
        try:
            singleplayer = singleplayer_arguments(content, state, args.singleplayer_map)
        except (OSError, UnicodeError, ValueError, zipfile.BadZipFile) as error:
            parser.error(str(error))
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as probe:
        probe.bind(("0.0.0.0", port))
    if args.seconds is not None and args.seconds <= 0:
        parser.error("--seconds must be positive")
    if not 640 <= args.width <= 8192 or not 480 <= args.height <= 8192:
        parser.error("Invalid window dimensions")
    run_id = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ") + "-" + uuid.uuid4().hex[:8]
    run = args.output_root.resolve() / run_id
    run.mkdir(parents=True)
    (run / "captures").mkdir()
    state.mkdir(parents=True, exist_ok=True)
    cache = args.warm_cache.resolve(strict=True) if args.warm_cache else run / "cache"
    cache.mkdir(exist_ok=True)
    env = os.environ.copy()
    env.update(XDG_DATA_HOME=str(state), XDG_CACHE_HOME=str(cache),
               BF2_FX_NATIVE_CACHE=str(cache / "effects"),
               DXVK_WSI_DRIVER="SDL3", DXVK_LOG_PATH=str(run),
               DXVK_SHADER_CACHE_PATH=str(cache / "dxvk"), MESA_SHADER_CACHE_DIR=str(cache / "mesa"),
               BF2_HITCH_TRACE=str(run / "hitches.log"), BF2_FRAME_TRACE="1",
               BF2_ABORT_TRACE="1", BF2_EXIT_TRACE="1", BF2_CAPTURE_DIR=str(run / "captures"))
    for name in ("effects", "dxvk", "mesa"):
        (cache / name).mkdir(exist_ok=True)
    if args.runtime:
        runtime = args.runtime.resolve(strict=True)
        env["LD_LIBRARY_PATH"] = str(runtime) + (":" + env["LD_LIBRARY_PATH"] if env.get("LD_LIBRARY_PATH") else "")
    env["DXVK_HUD"] = "" if args.detail == "light" else "devinfo,fps,frametimes,drawcalls,pipelines,compiler,memory,gpuload"
    command = [str(binary), "+modPath", "mods/bf2", "+multi", "1", "+fullscreen", "0",
               "+szx", str(args.width), "+szy", str(args.height), "+port", str(port)]
    if args.skip_intros:
        command += ["+restart", "1"]
    command += singleplayer
    # BF2Engine lets nondefault GSPort override the stored server port at local startup.
    # A later explicit server-browser selection can still set its own destination port.
    metadata = dict(run_id=run_id, kind="native-client", purpose=args.purpose,
                    acceptance="pending human review", command=command, content=str(content),
                    state=str(state), server_port=port, binary_sha256=hashlib.sha256(binary.read_bytes()).hexdigest(),
                    platform=platform.platform(), architecture=platform.machine(), power=args.power_note,
                    requested_display=dict(width=args.width, height=args.height, fullscreen=False),
                    saved_video_start=video_snapshot(state), power_start=power_snapshot(),
                    cache_start=cache_snapshot(cache), graphics_overrides=graphics_overrides(env, content),
                    cache=str(cache), cache_state="reused; contents not verified" if args.warm_cache else "new",
                    utc_start=datetime.now(timezone.utc).isoformat(), instrumentation=args.detail,
                    environment={key: env.get(key) for key in (
                        "XDG_DATA_HOME", "XDG_CACHE_HOME", "BF2_FX_NATIVE_CACHE", "BF2_HITCH_TRACE",
                        "BF2_FRAME_TRACE", "BF2_ABORT_TRACE", "BF2_EXIT_TRACE", "BF2_CAPTURE_DIR",
                        "BF2_RENDDX9_SWIFFLOG", "BF2_FX_NATIVE_TRACE",
                        "BF2_SWIFFPLAYER_ASLOG", "BF2_SWIFFPLAYER_STREAMLOG",
                        "BF2_INPUT_SCRIPT", "BF2_INPUT_SCRIPT_LOG", "BF2_INPUT_SCRIPT_REAL_FOCUS", "DXVK_WSI_DRIVER",
                        "DXVK_LOG_PATH", "DXVK_SHADER_CACHE_PATH", "MESA_SHADER_CACHE_DIR", "DXVK_HUD",
                        "SDL_VIDEODRIVER", "SDL_AUDIODRIVER", "ALSOFT_DRIVERS", "VK_DRIVER_FILES", "VK_ICD_FILENAMES",
                        "VK_INSTANCE_LAYERS", "LD_LIBRARY_PATH")},
                    unknown_measurements=["GPU timings", "CPU stacks", "upload and synchronization attribution"],
                    **source_metadata(root))
    build_record = binary.with_suffix(".build.json")
    if build_record.is_file():
        build = json.loads(build_record.read_text())
        if build.get("binary_sha256") == metadata["binary_sha256"]:
            metadata["build"] = build
    if env.get("BF2_INPUT_SCRIPT"):
        script = Path(env["BF2_INPUT_SCRIPT"])
        if script.is_file():
            metadata["input_script_sha256"] = hashlib.sha256(script.read_bytes()).hexdigest()
    manifest = run / "run.json"
    manifest.write_text(json.dumps(metadata, indent=2) + "\n")
    print(f"Run: {run_id}\nArtifacts: {run}\nProfiles: {state}\nContent: {content}", flush=True)
    timed_out, peak_rss_kib, interrupted = False, 0, None
    start = time.monotonic()
    def terminate(signum, _frame):
        nonlocal interrupted
        if interrupted is None:
            interrupted = signal.Signals(signum).name
    previous_handlers = {sig: signal.signal(sig, terminate) for sig in (signal.SIGINT, signal.SIGTERM)}
    process = None
    try:
        with (run / "client.log").open("w") as log:
            try:
                process = subprocess.Popen(command, cwd=content, env=env, stdout=log,
                                           stderr=subprocess.STDOUT, start_new_session=True)
                metadata["pid"] = process.pid
                manifest.write_text(json.dumps(metadata, indent=2) + "\n")
                while process.poll() is None:
                    if interrupted is not None:
                        break
                    try:
                        for line in Path(f"/proc/{process.pid}/status").read_text().splitlines():
                            if line.startswith("VmHWM:"):
                                peak_rss_kib = max(peak_rss_kib, int(line.split()[1]))
                    except FileNotFoundError:
                        pass
                    if args.seconds and time.monotonic() - start >= args.seconds:
                        timed_out = True
                        break
                    time.sleep(0.1)
            finally:
                # Repeated Ctrl+C/TERM must not interrupt child termination or the final manifest.
                for sig in previous_handlers:
                    signal.signal(sig, signal.SIG_IGN)
                if process is not None:
                    stop_child(process)
            code = process.returncode
        metadata.update(exit_code=code, diagnostic_timeout=timed_out, interrupted=interrupted, peak_rss_kib=peak_rss_kib,
                        duration_seconds=time.monotonic() - start, utc_end=datetime.now(timezone.utc).isoformat())
        metadata.update(saved_video_end=video_snapshot(state), power_end=power_snapshot(),
                        cache_end=cache_snapshot(cache), graphics_evidence=graphics_evidence(run))
        manifest.write_text(json.dumps(metadata, indent=2) + "\n")
    finally:
        for sig, handler in previous_handlers.items():
            signal.signal(sig, handler)
    frames, capture_frames = [], set()
    trace = run / "hitches.log"
    if trace.exists():
        for line in trace.read_text().splitlines():
            parts = line.split()
            if len(parts) >= 5 and parts[0] == "F":
                frames.append(dict(frame=int(parts[1]), seconds=float(parts[2]), ms=float(parts[3]), in_game=parts[4] == "1"))
            elif len(parts) >= 4 and parts[0] == "N" and parts[3] == "linux-capture":
                capture_frames.add(int(parts[1]))
    summary = dict(run_id=run_id, frame_count=len(frames),
                   frame_intervals=distribution([f["ms"] for f in frames[1:] if f["frame"] not in capture_frames]),
                   excluded_capture_frames=sorted(capture_frames),
                   hitches_over_50ms=[f for f in frames if f["ms"] > 50], peak_rss_kib=peak_rss_kib,
                   acceptance="pending human review")
    (run / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
    print(f"Exit {code}; {len(frames)} traced frames; peak RSS {peak_rss_kib} KiB. Artifacts: {run}")
    return code if code >= 0 else 128 - code


if __name__ == "__main__":
    raise SystemExit(main())
