#!/usr/bin/env python3
"""Run the issue #36 presentation check in an isolated, timestamped capture directory."""
import argparse
import csv
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import platform
import subprocess
import time
import uuid


def distribution(values):
    if not values:
        return None
    values = sorted(values)
    def percentile(p):
        index = (len(values) - 1) * p
        lo = int(index)
        hi = min(lo + 1, len(values) - 1)
        return values[lo] + (values[hi] - values[lo]) * (index - lo)
    return {"count": len(values), "median_ms": percentile(0.5),
            "p95_ms": percentile(0.95), "p99_ms": percentile(0.99), "max_ms": values[-1]}


def source_metadata(root):
    try:
        # The checkout may be bind-mounted into a container under another UID.
        # Trust only this explicitly selected source root, without changing Git config.
        git = ["git", "-c", "safe.directory=" + str(root)]
        return {
            "commit": subprocess.check_output(git + ["rev-parse", "HEAD"], cwd=root,
                                              text=True, stderr=subprocess.PIPE).strip(),
            "worktree": subprocess.check_output(git + ["status", "--short"], cwd=root,
                                                text=True, stderr=subprocess.PIPE),
        }
    except (OSError, subprocess.CalledProcessError) as error:
        # Packaged checks and minimal runtime containers need no Git installation.
        return {"commit": None, "worktree": None, "source_metadata_error": str(error)}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("binary", type=Path)
    parser.add_argument("--output-root", type=Path, default=Path("out/linux-vulkan-runs"))
    parser.add_argument("--label", default="window-presentation")
    parser.add_argument("--purpose", required=True, help="Human-written visual purpose and defect criteria")
    parser.add_argument("--frames", type=int, help="Optional bounded run; never grants visual acceptance")
    parser.add_argument("--detail", choices=("light", "detailed"), default="light")
    parser.add_argument("--warm-cache", type=Path, help="Reuse a prior run's cache directory")
    parser.add_argument("--power-note", default="unknown")
    parser.add_argument("--driver-note", default="unknown; inspect the DXVK log")
    args = parser.parse_args()
    binary = args.binary.resolve(strict=True)
    if args.frames is not None and args.frames <= 0:
        parser.error("--frames must be positive")
    if args.warm_cache and not args.warm_cache.is_dir():
        parser.error("--warm-cache must be an existing cache directory")
    root = Path(__file__).resolve().parents[2]
    run_id = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ") + "-" + uuid.uuid4().hex[:8]
    run = args.output_root.resolve() / run_id
    run.mkdir(parents=True)
    cache = args.warm_cache.resolve() if args.warm_cache else run / "cache"
    if not args.warm_cache:
        cache.mkdir()
    env = os.environ.copy()
    env.update(DXVK_WSI_DRIVER="SDL3", DXVK_LOG_PATH=str(run),
               DXVK_SHADER_CACHE_PATH=str(cache), MESA_SHADER_CACHE_DIR=str(cache))
    env["DXVK_HUD"] = "" if args.detail == "light" else "devinfo,fps,frametimes,drawcalls,pipelines,compiler,memory,gpuload"
    command = [str(binary), str(run)]
    if args.frames:
        command.append(str(args.frames))
    metadata = {
        "run_id": run_id, "label": args.label, "purpose": args.purpose,
        "kind": "presentation-boundary", "acceptance": "pending human review",
        "binary": str(binary), "binary_sha256": hashlib.sha256(binary.read_bytes()).hexdigest(),
        **source_metadata(root),
        "platform": platform.platform(), "architecture": platform.machine(),
        "driver": args.driver_note, "power": args.power_note,
        "graphics": "D3D9 fixed-function triangle, default state, vsync interval 1; initial 1280x800",
        "cache": str(cache), "cache_state": "reused; contents not verified" if args.warm_cache else "cold, new directory",
        "instrumentation": args.detail, "command": command,
        "utc_start": datetime.now(timezone.utc).isoformat(), "monotonic_start_ns": time.monotonic_ns(),
        "frame_clock": "SDL_GetTicksNS, nanoseconds since SDL initialization; same clock as event log",
        "environment": {key: env.get(key) for key in (
            "DXVK_WSI_DRIVER", "DXVK_HUD", "DXVK_SHADER_CACHE_PATH", "MESA_SHADER_CACHE_DIR",
            "DXVK_CONFIG", "DXVK_CONFIG_FILE", "SDL_VIDEODRIVER", "VK_DRIVER_FILES",
            "VK_ICD_FILENAMES", "VK_INSTANCE_LAYERS")},
        "unknown_measurements": ["GPU timings", "CPU call stacks", "upload timings", "synchronization attribution"],
    }
    manifest = run / "run.json"
    manifest.write_text(json.dumps(metadata, indent=2) + "\n")
    print(f"Run {run_id}\nArtifacts: {run}\nF11 fullscreen; F6 mouse capture; F12 screenshot; Escape exit", flush=True)
    with (run / "events.log").open("w") as log:
        result = subprocess.run(command, env=env, stdout=log, stderr=subprocess.STDOUT)
    metadata.update(exit_code=result.returncode, utc_end=datetime.now(timezone.utc).isoformat(),
                    monotonic_end_ns=time.monotonic_ns())
    manifest.write_text(json.dumps(metadata, indent=2) + "\n")
    rows = []
    if (run / "frames.csv").exists():
        with (run / "frames.csv").open() as source:
            rows = list(csv.DictReader(source))
    # Exclude first-frame startup and the interval following readback, which includes
    # screenshot overhead. Keep all rows in the raw timeline for investigation.
    intervals = [float(row["interval_ms"]) for previous, row in zip(rows, rows[1:])
                 if previous["captured"] == "0"]
    summary = {"run_id": run_id, "frames": len(rows), "frame_intervals": distribution(intervals),
               "present_cpu": distribution([float(row["present_ms"]) for row in rows]),
               "hitches_over_50ms": [row for row in rows[1:] if float(row["interval_ms"]) > 50],
               "memory_metric": "process peak RSS, KiB; not current allocation or VRAM",
               "acceptance": "pending human review"}
    (run / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
    print(f"Exit {result.returncode}; {len(rows)} frames. Visual acceptance remains pending.")
    return result.returncode


if __name__ == "__main__":
    raise SystemExit(main())
