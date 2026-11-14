#!/usr/bin/env python3
"""[CHOSEN, not recovered] Batch compiler jobs and isolated MojoShader collection."""
import argparse
import csv
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import engine_inputs

REPO = Path(__file__).resolve().parents[3]
S0 = REPO.parent / "s0-shader-proto"
BUILD = REPO / "out/build/s1c-fx-coverage/tools"
APP = Path.home() / "Applications/Battlefield 2.app/Contents/SharedSupport"
FIELDS = ["stage", "file", "defines", "compile", "parse", "translate", "error_summary",
          "job", "shader_count", "msl_count", "metal_file", "metal"]


def dump(path, value):
    path.write_text(json.dumps(value, indent=2, sort_keys=True) + "\n")


def clear_parser_artifacts(directory):
    if directory.exists():
        for path in directory.iterdir():
            if path.is_file() and (path.suffix in (".metal", ".bin", ".log") or
                                   path.name in ("result.json", "inventory.txt")):
                path.unlink()


def owned(path):
    path = path.resolve()
    if not path.is_relative_to((REPO / "out").resolve()) or path == (REPO / "out").resolve():
        raise ValueError(f"Output must be a child of this worktree's out/: {path}")
    return path


def winpath(path):
    # Wine's default Z: mapping, independent of winepath / another Wine startup.
    return "Z:" + str(path.resolve()).replace("/", "\\")


def strings(text, name):
    match = re.search(r"\bstring\s+" + re.escape(name) + r"\s*\[\]\s*=\s*\{(.*?)\}\s*;", text, re.S)
    return re.findall(r'"([^"\n]*)"', match[1]) if match else []


def ra_jobs(source, output):
    # Adapted from s0/permutations.py, following current MetaFx::generateShader.
    vocabulary = (REPO / "Code/BF2/RendDX9/ShaderManagerDefines.inc").read_text()
    common = dict(re.findall(r'\{\s*"([^"\n]+)",\s*"([^"\n]+)"\s*\}', vocabulary))
    common.update(PSVERSION="20", PSMODEL="ps_2_0", VSMODEL="vs_2_0",
                  _FORCE_1_4_SHADERS_="0", _FORCE_1_3_SHADERS_="0")
    for family in ("RaShaderBM", "RaShaderSM", "RaShaderSTM"):
        metadata = source / (family + ".mfx")
        text = re.sub(r"/\*.*?\*/|//[^\n]*", "", metadata.read_text(), flags=re.S)
        permutations = strings(text, "GenerateShaderPermutations")[:3]
        if len(permutations) != 3:
            raise ValueError(f"Need at least three listed permutations: {metadata}")
        literals = re.findall(r"const\s+dword\s+(\w+)\s*:\s*LITERAL\s*(?:<(.*?)>)?\s*;", text, re.S)
        for index, permutation in enumerate(permutations):
            selected = set(permutation.split("|"))
            if not selected.issubset({name for name, _ in literals}):
                raise ValueError(f"Unknown literal in {permutation}")
            definitions = {name.upper(): str(int(name in selected)) for name, _ in literals}
            lines = ["// [CHOSEN, not recovered] Experiment following current MetaFx generated input."]
            lines += [f"#define {key} {value}" for key, value in definitions.items()]
            lines += [f"static const dword {name.lower()} = {definitions[name.upper()]};" for name, _ in literals]
            texsets, arrays = [], {}
            for structure in strings(text, "GenerateStructs"):
                members = strings(text, structure + "Base")
                for literal, annotations in literals:
                    if literal in selected:
                        members += strings(annotations or "", structure)
                members = list(dict.fromkeys(members))
                arrays[structure] = members
                if members:
                    lines.append(f"string {structure}[] = {{" + ",".join(json.dumps(s) for s in members) + "};")
                for member in members:
                    if "2D" in member and member.startswith("T"):
                        texsets.append("Tex" + member[1:-2] + "Ind")
            generated = {"NUM_LIGHTS": "1", "NUM_TEXSETS": str(len(texsets))}
            generated.update({name: str(i) for i, name in enumerate(texsets)})
            lines += [f"#define {key} {value}" for key, value in generated.items()]
            lines.append(f'#include "shaders/{family}.fx"')
            identifier = f"ra-{family}-{index}"
            wrapper = output / "generated" / (identifier + ".fx")
            wrapper.write_text("\n".join(lines) + "\n")
            yield dict(id=identifier, file=family + ".fx", source=str(wrapper),
                       kind="ra", permutation=permutation, defines={**common, **definitions, **generated},
                       compiler_defines=common, arrays=arrays,
                       metadata_sha256=hashlib.sha256(metadata.read_bytes()).hexdigest())


def prepare(args, output):
    source = args.source.resolve()
    files = sorted(p for p in source.iterdir() if p.suffix.lower() in (".fx", ".dfx", ".mfx"))
    if not files:
        raise ValueError(f"No effect corpus found in {source}")
    previous_manifest = output / "jobs.json"
    if previous_manifest.exists():
        for job in json.loads(previous_manifest.read_text()):
            clear_parser_artifacts(owned(output / "msl" / job["id"]))
    if args.inputs == "empty":
        jobs = [dict(id="empty-" + p.name, file=p.name, source=str(p), kind="empty",
                     defines={}, compiler_defines={}) for p in files]
        jobs += list(ra_jobs(source, output))
    else:
        jobs = list(engine_inputs.jobs(source, output, args.all_permutations, args.profiles))
    if args.controls:
        for name in ("control.fx", "metadata.fx"):
            jobs.append(dict(id="control-" + name, file=name,
                             source=str(REPO / "Code/Tests/macos_shaderproto" / name),
                             kind="control", defines={}, compiler_defines={}))
    if args.coverage_controls:
        for path in sorted((REPO / "Code/Tests/macos_d3dxfx").glob("*.fx")):
            jobs.append(dict(id="regression-" + path.name, file=path.name, source=str(path),
                             kind="regression", defines={}, compiler_defines={}))
    manifest = []
    for job in jobs:
        job["defines"] = {**job.get("compiler_defines", {}), **job.get("virtual_defines", {}),
                          **job.get("source_defines", {}), **job.get("defines", {})}
        job.setdefault("flags", 0)
        job.setdefault("include_mode", "baseline")
        job["runtime_pool"] = None
        job["runtime_create_flags"] = job["flags"] | (0x800 if job["include_mode"] == "ra" else 0)
        job["source_sha256"] = hashlib.sha256(Path(job["source"]).read_bytes()).hexdigest()
        job["binary"] = str(output / "bin" / (job["id"] + ".fxo"))
        job["log"] = str(output / "logs" / (job["id"] + ".compile.log"))
        fields = [winpath(Path(job["source"])), winpath(Path(job["binary"])),
                  winpath(Path(job["log"])), winpath(source)]
        fields += [f'--flags={job["flags"]}', f'--include-mode={job["include_mode"]}']
        if job["include_mode"] == "ra":
            fields += ["--virtual-root=" + winpath(output / "virtual")]
        fields += [f"{key}={value}" for key, value in job["compiler_defines"].items()]
        if any(any(c in field for c in "\t\r\n") for field in fields):
            raise ValueError("TSV fields cannot contain tabs or newlines")
        manifest.append("\t".join(fields))
    (output / "jobs.tsv").write_text("\n".join(manifest) + "\n")
    dump(output / "jobs.json", jobs)
    dump(output / "inputs.json", {str(p.relative_to(source)): hashlib.sha256(p.read_bytes()).hexdigest()
                                  for p in sorted(source.rglob("*")) if p.is_file()})
    return jobs


def wine_compile(args, output, jobs):
    prefix = owned(REPO / "out/wineprefix-s1-d3dx-wine")
    # Refuse an existing unowned prefix; the app's prefix is never used or copied.
    marker = prefix / ".openbf2-d3dxfx"
    if prefix.exists() and not marker.is_file():
        raise ValueError(f"Refusing existing unmarked prefix {prefix}")
    prefix.mkdir(parents=True, exist_ok=True)
    marker.write_text("Standalone s1-d3dx-wine prefix\n")
    win32 = output / "win32"
    win32.mkdir(exist_ok=True)
    shutil.copy2(BUILD / "fxcompile.exe", win32 / "fxcompile.exe")
    dll = APP / "prefix/drive_c/Battlefield 2/d3dx9_25.dll"
    shutil.copy2(dll, win32 / "d3dx9_25.dll")
    dump(output / "compiler.json", dict(dll_source=str(dll), dll_sha256=hashlib.sha256(dll.read_bytes()).hexdigest(),
         wine=str(args.wine), prefix=str(prefix), flags="per job (jobs.json)", effect_format="fx_2_0 (D3DX CompileEffect)"))
    for job in jobs:
        for path in (Path(job["binary"]), Path(job["binary"] + ".status"), Path(job["log"])):
            path.unlink(missing_ok=True)
    cache = output / "wine-cache"
    cache.mkdir(exist_ok=True)
    home, temporary = output / "wine-home", output / "wine-tmp"
    home.mkdir(exist_ok=True)
    temporary.mkdir(exist_ok=True)
    env = dict(os.environ, WINEPREFIX=str(prefix), WINEARCH="win64", WINEDEBUG="-all",
               WINEDLLOVERRIDES="d3dx9_25=n;mscoree,mshtml=", XDG_CACHE_HOME=str(cache),
               HOME=str(home), TMPDIR=str(temporary),
               DYLD_LIBRARY_PATH=str(APP.parent / "Frameworks"))
    command = [str(args.wine), winpath(win32 / "fxcompile.exe"), "--batch", winpath(output / "jobs.tsv")]
    dump(output / "wine-command.json", dict(argv=command, environment={k: env[k] for k in
         ("WINEPREFIX", "WINEARCH", "WINEDEBUG", "WINEDLLOVERRIDES", "XDG_CACHE_HOME", "DYLD_LIBRARY_PATH", "HOME", "TMPDIR")}))
    print(f"Wine batch: {len(jobs)} jobs, isolated prefix {prefix}", flush=True)
    with (output / "logs/wine.stdout.log").open("w") as stdout, (output / "logs/wine.stderr.log").open("w") as stderr:
        try:
            child = subprocess.run(command, cwd=win32, env=env, stdout=stdout, stderr=stderr, timeout=args.timeout)
            result = dict(returncode=child.returncode, timed_out=False)
        except subprocess.TimeoutExpired:
            result = dict(returncode=None, timed_out=True)
            # Terminate only this marked prefix's daemon, never the application's prefix.
            try:
                stop = subprocess.run([str(args.wine.parent / "wineserver"), "-k"], env=env,
                                      stdout=stdout, stderr=stderr, timeout=10)
                result["prefix_cleanup_returncode"] = stop.returncode
            except (OSError, subprocess.TimeoutExpired) as error:
                result["prefix_cleanup_error"] = str(error)
        except OSError as error:
            result = dict(returncode=None, timed_out=False, error=str(error))
    dump(output / "wine-result.json", result)
    return result


def collect(output, jobs):
    rows = []
    for job in jobs:
        binary = Path(job["binary"])
        directory = output / "msl" / job["id"]
        directory.mkdir(exist_ok=True)
        # Clear even failed compilation jobs, so an older MSL file cannot reach the Metal sweep.
        clear_parser_artifacts(directory)
        status = Path(str(binary) + ".status")
        ok = status.exists() and status.read_text().splitlines()[:1] == ["ok"] and binary.is_file() and binary.stat().st_size > 0
        row = dict(stage="effect", file=job["file"], defines=json.dumps(job["defines"], sort_keys=True),
                   compile="ok" if ok else "fail" if status.exists() else "not_run", parse="not_run",
                   translate="not_run", error_summary="", job=job["id"], shader_count=0, msl_count=0,
                   metal_file="", metal="not_run")
        if not ok:
            log = Path(job["log"])
            row["error_summary"] = log.read_text(errors="replace").replace("\0", "") if log.exists() else "No compiler status; inspect logs/wine.stderr.log"
        else:
            command = [str(BUILD / "effectparse"), str(binary), str(directory)]
            dump(directory / "command.json", command)
            with (directory / "stdout.log").open("w") as stdout, (directory / "stderr.log").open("w") as stderr:
                try:
                    child = subprocess.run(command, stdout=stdout, stderr=stderr, timeout=120)
                    rc = child.returncode
                except subprocess.TimeoutExpired:
                    rc = "timeout"
            result_path = directory / "result.json"
            result = json.loads(result_path.read_text()) if result_path.exists() else {}
            row.update(parse="ok" if result.get("parse_ok") else "fail",
                       translate=result.get("translate_status", "not_run"), shader_count=result.get("shader_count", 0),
                       msl_count=result.get("msl_count", 0), error_summary="; ".join(result.get("errors", [])))
            if rc not in (0, 1) or not result:
                row["error_summary"] += f" parser exit {rc}: " + (directory / "stderr.log").read_text(errors="replace")
                row["parse"] = "fail"
                row["translate"] = "not_run"
        row["error_summary"] = " ".join(row["error_summary"].split())[:4000]
        rows.append(row)
    with (output / "results.csv").open("w", newline="") as file:
        writer = csv.DictWriter(file, fieldnames=FIELDS, lineterminator="\n")
        writer.writeheader(); writer.writerows(rows)
    summary = {}
    for kind in ("empty", "legacy", "ra_direct", "ra", "control", "regression"):
        selected = [row for row, job in zip(rows, jobs) if job["kind"] == kind]
        if selected:
            summary[kind] = dict(inputs=len(selected), **{key + "_ok": sum(r[key] == "ok" for r in selected)
                                for key in ("compile", "parse", "translate")},
                                compile_not_run=sum(r["compile"] == "not_run" for r in selected),
                                no_shaders=sum(r["translate"] == "no_shaders" for r in selected),
                                shaders=sum(r["shader_count"] for r in selected), msl=sum(r["msl_count"] for r in selected))
    dump(output / "summary.json", summary)
    groups = {}
    for row, job in zip(rows, jobs):
        groups.setdefault(job.get("effect_group", job["id"]), []).append(row)
    dump(output / "effect-groups.json", {name: dict(attempts=len(attempts),
         compile_ok=any(r["compile"] == "ok" for r in attempts),
         fully_translated=any(r["translate"] == "ok" for r in attempts),
         note="Compiler acceptance only; runtime technique validation not exercised")
         for name, attempts in groups.items()})
    print(json.dumps(summary, indent=2), flush=True)
    return any(row["compile"] == "not_run" for row in rows)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, default=S0 / "out/fx-sample/src")
    parser.add_argument("--output", type=Path, default=REPO / "out/d3dxfx")
    parser.add_argument("--wine", type=Path, default=APP / "wine/bin/wine64")
    parser.add_argument("--timeout", type=int, default=900)
    parser.add_argument("--controls", action="store_true", help="Also compile the two s0 synthetic effect controls")
    parser.add_argument("--coverage-controls", action="store_true", help="Compile the s1c MojoShader regression effects")
    parser.add_argument("--inputs", choices=("engine", "empty"), default="engine",
                        help="Engine input snapshot (default) or original empty-define baseline")
    parser.add_argument("--all-permutations", action="store_true",
                        help="Generate every metadata permutation and modifier instead of first three base permutations")
    parser.add_argument("--profiles", choices=("engine", "sm2"), default="engine",
                        help="All six supported Ra profile attempts in engine order, or just vs_2_0/ps_2_0")
    parser.add_argument("--metal", action="store_true", help="After collection run Metal and classify its diagnostics")
    parser.add_argument("--skip-build", action="store_true")
    parser.add_argument("--prepare-only", action="store_true", help="Generate all jobs without invoking Wine")
    parser.add_argument("--stage2-only", action="store_true", help="Read existing jobs.json/binaries without Wine")
    args = parser.parse_args()
    if args.prepare_only and args.stage2_only:
        parser.error("--prepare-only and --stage2-only cannot be combined")
    if args.prepare_only and args.metal:
        parser.error("--prepare-only cannot run Metal")
    output = owned(args.output)
    for subdir in ("bin", "msl", "logs", "generated"):
        (output / subdir).mkdir(parents=True, exist_ok=True)
    if not args.skip_build:
        subprocess.run(["sh", str(REPO / "tools/macos/d3dxfx/build.sh")], cwd=REPO, check=True)
    if args.stage2_only:
        jobs = json.loads((output / "jobs.json").read_text())
    else:
        jobs = prepare(args, output)
        print(f"Prepared {len(jobs)} jobs in {output}", flush=True)
        if args.prepare_only:
            return 0
        wine_compile(args, output, jobs)
    incomplete = collect(output, jobs)
    if incomplete:
        print("Wine batch incomplete/unavailable; results mark unattempted jobs not_run. Coordinator must rerun.", file=sys.stderr)
    if args.metal:
        rc = subprocess.run([str(BUILD / "metal-compile"), str(output / "msl"), str(output / "results.csv")]).returncode
        subprocess.run([sys.executable, str(Path(__file__).with_name("classify-metal.py")),
                        str(output / "results.csv")], check=True)
        subprocess.run([sys.executable, str(Path(__file__).with_name("classify-stages.py")),
                        str(output)], check=True)
        if rc:
            return 3 if incomplete else rc
    else:
        subprocess.run([sys.executable, str(Path(__file__).with_name("classify-stages.py")),
                        str(output)], check=True)
    # Compiler/translator failures are measured outcomes; unavailable compilation is an incomplete run.
    return 3 if incomplete else 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        print(error, file=sys.stderr)
        sys.exit(2)
