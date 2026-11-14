#!/usr/bin/env python3
"""[CHOSEN, not recovered] Isolate failures per effect and retain local-only artifacts."""
import argparse
import collections
import csv
import hashlib
import json
from pathlib import Path
import subprocess


def main():
    repo = Path(__file__).resolve().parents[3]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, default=repo / "out/fx-sample/src")
    parser.add_argument("--output", type=Path, default=repo / "out/shaderproto")
    parser.add_argument("--binary", type=Path, default=repo / "out/build/s0-shader-proto/shaderproto")
    parser.add_argument("--file", help="Run just this source filename")
    parser.add_argument("--define", action="append", default=[], help="NAME=VALUE, repeatable")
    args = parser.parse_args()
    source, output, binary = args.source.resolve(), args.output.resolve(), args.binary.resolve()
    output.mkdir(parents=True, exist_ok=True)
    files = sorted(p for p in source.iterdir() if p.suffix.lower() in (".fx", ".dfx", ".mfx"))
    if args.file:
        files = [p for p in files if p.name == args.file]
    if not files:
        parser.error("No matching input files")
    # Stable input hashes, including includes; never save copyrighted input in tracked files.
    (output / "inputs.json").write_text(json.dumps({
        "source": str(source), "defines": args.define,
        "sha256": {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(source.iterdir()) if p.is_file()},
    }, indent=2) + "\n")
    rows = []
    for file in files:
        artifact = output / "effects" / file.name
        artifact.mkdir(parents=True, exist_ok=True)
        # Only remove our result files so a crash cannot reuse a previous success.
        for name in ("result.json", "effect.fxo"):
            (artifact / name).unlink(missing_ok=True)
        command = [str(binary), str(file), str(source), str(artifact), *args.define]
        (artifact / "command.json").write_text(json.dumps(command, indent=2) + "\n")
        with (artifact / "stdout.log").open("w") as stdout, (artifact / "stderr.log").open("w") as stderr:
            try:
                child = subprocess.run(command, stdout=stdout, stderr=stderr, timeout=120)
                returncode = child.returncode
            except subprocess.TimeoutExpired:
                returncode = "timeout"
        if (artifact / "result.json").exists():
            result = json.loads((artifact / "result.json").read_text())
        else:
            compiled = (artifact / "effect.fxo").exists()
            diagnostic = " ".join((artifact / "stderr.log").read_text(errors="replace").split())[-1600:]
            result = dict(stage_reached="parse" if compiled else "compile", compile_ok=compiled,
                          parse_ok=False, translate_ok=False, metal_ok=False,
                          errors=[f"process exited {returncode}: {diagnostic}"])
        summary = " | ".join(result.get("errors", []))
        row = dict(file=file.name, stage_reached=result["stage_reached"],
                   **{key: int(result.get(key, False)) for key in ("compile_ok", "parse_ok", "translate_ok", "metal_ok")},
                   shader_count=result.get("shader_count", 0), msl_count=result.get("msl_count", 0),
                   library_count=result.get("library_count", 0), exit_code=returncode,
                   error_summary=" ".join(summary.split())[:4000])
        rows.append(row)
        # Save incrementally so a restart preserves completed work.
        with (output / "results.csv").open("w", newline="") as stream:
            writer = csv.DictWriter(stream, fieldnames=list(row))
            writer.writeheader()
            writer.writerows(rows)
        print(f"{file.name}: {row['stage_reached']} compile={row['compile_ok']} parse={row['parse_ok']} "
              f"translate={row['translate_ok']} metal={row['metal_ok']}", flush=True)
    totals = {key: sum(r[key] for r in rows) for key in ("compile_ok", "parse_ok", "translate_ok", "metal_ok", "shader_count", "msl_count", "library_count")}
    totals["files"] = len(rows)
    totals["stages_reached"] = dict(collections.Counter(r["stage_reached"] for r in rows))
    (output / "summary.json").write_text(json.dumps(totals, indent=2) + "\n")
    print(json.dumps(totals, indent=2))


if __name__ == "__main__":
    main()
