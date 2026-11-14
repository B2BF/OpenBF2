#!/usr/bin/env python3
"""[CHOSEN, not recovered] Test literal shader entry/profile pairs without FX serialization."""
import csv
import json
from pathlib import Path
import re
import subprocess

repo = Path(__file__).resolve().parents[3]
source = repo / "out/fx-sample/src"
effect = source / "SwiffMenu.fx"
output = repo / "out/shaderproto/swiff-entries"
output.mkdir(parents=True, exist_ok=True)
text = re.sub(r"/\*.*?\*/|//[^\n]*", "", effect.read_text(), flags=re.S)
entries = list(dict.fromkeys(re.findall(
    r"\b(?:VertexShader|PixelShader)\s*=\s*compile\s+(\w+)\s+(\w+)\s*\(", text, re.I)))
rows = []
for profile, entry in entries:
    directory = output / (entry + "-" + profile)
    directory.mkdir(parents=True, exist_ok=True)
    (directory / "result.json").unlink(missing_ok=True)
    command = [str(repo / "out/build/s0-shader-proto/shaderproto"), "--shader", profile, entry,
               str(effect), str(source), str(directory)]
    (directory / "command.json").write_text(json.dumps(command, indent=2) + "\n")
    with (directory / "stdout.log").open("w") as stdout, (directory / "stderr.log").open("w") as stderr:
        child = subprocess.run(command, stdout=stdout, stderr=stderr, timeout=120)
    result = json.loads((directory / "result.json").read_text()) if (directory / "result.json").exists() else {}
    rows.append(dict(entry=entry, profile=profile,
                     **{key: int(result.get(key, False)) for key in ("compile_ok", "parse_ok", "translate_ok", "metal_ok")},
                     exit_code=child.returncode, error_summary=" ".join(" | ".join(result.get("errors", ["No result"])).split())[:2000]))
    print(entry, profile, rows[-1]["compile_ok"], rows[-1]["translate_ok"], flush=True)
with (output / "results.csv").open("w", newline="") as stream:
    writer = csv.DictWriter(stream, fieldnames=list(rows[0]))
    writer.writeheader()
    writer.writerows(rows)
print({key: sum(row[key] for row in rows) for key in ("compile_ok", "parse_ok", "translate_ok", "metal_ok")})
