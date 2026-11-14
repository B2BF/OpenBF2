#!/usr/bin/env python3
"""[CHOSEN, not recovered] Content-local offline FX2/MSL cache builder; never runs BF2.

Both manifest prewarming and runtime request replay use cache-tool's production key
and wire codec. A compiler refusal is a failed job, never a usable cache entry.

Validator caveat: fxcompile loads the retail d3dx9_25.dll natively but Wine's builtin d3d9.dll,
whose shader validator (Direct3DShaderValidatorCreate9) is a stub that accepts everything. Outcomes
here therefore miss validator refusals (X5111, X5500, ...) that native Windows reports; the refusal
table takes those from a native run (refusal_table.py --native-result, Code/Tests/effect_check).
"""
import argparse
import hashlib
import json
import os
import re
from pathlib import Path
import shutil
import subprocess
import sys
import uuid
from concurrent.futures import ThreadPoolExecutor

REPO = Path(__file__).resolve().parents[3]
BUILD = REPO / "out/build/s2-fx-runtime"
TOOL = BUILD / "Code/BF2/Platform/Gfx/Effect/bf2_fx_cache_tool"
APP = Path.home() / "Applications/Battlefield 2.app/Contents/SharedSupport"


def owned(path):
    path = path.resolve()
    if not path.is_relative_to(REPO / "out") or path == REPO / "out":
        raise ValueError(f"Writes must be below this worktree's out/: {path}")
    return path


def winpath(path):
    return "Z:" + str(path.resolve()).replace("/", "\\")


def run(*args):
    return subprocess.check_output([str(TOOL), *map(str, args)], text=True).strip()


def large_address_aware(path):
    """Mark the per-run compiler host copy large-address-aware, as BF2.exe 1.50 is.

    D3DX runs inside BF2.exe, whose PE header sets IMAGE_FILE_LARGE_ADDRESS_AWARE (4 GiB
    under a 64-bit host). Without it some RaShaderSM ps_2_a compiles fail with
    E_OUTOFMEMORY, a refusal the engine would not see. Only the address-space limit
    changes; the pinned build artifact and the compiler output are untouched.
    """
    data = bytearray(path.read_bytes())
    header = int.from_bytes(data[0x3C:0x40], "little")
    if data[header:header + 4] != b"PE\0\0":
        raise ValueError(f"Not a PE image: {path}")
    flags = int.from_bytes(data[header + 22:header + 24], "little") | 0x20
    data[header + 22:header + 24] = flags.to_bytes(2, "little")
    path.write_bytes(bytes(data))


def cache_version():
    text = (REPO / "Code/BF2/Platform/Gfx/Effect/EffectData.h").read_text()
    return int(re.search(r"constexpr uint32_t CacheVersion = (\d+);", text)[1])


REFLECTION_TECHNIQUE = b"\ntechnique bf2fx_reflection { pass p { } }\n"


def translator_identity():
    # Pin parser/emitter patches AND the exporter/runtime serialization contract.
    source = REPO / "out/third_party/mojoshader"
    paths = sorted(p for p in source.rglob("*") if p.suffix in (".c", ".h") and p.name != "mojoshader_version.h")
    paths += [Path(__file__).with_name(x) for x in ("cache-tool.cpp", "fxcompile.c", "build-runtime-tools.sh")]
    paths += sorted((REPO / "Code/BF2/Platform/Gfx/Effect").glob("*.h"))
    paths += sorted((REPO / "Code/BF2/Platform/Gfx/Effect").glob("*.cpp"))
    return "mojo-ad5dff8-s2-" + hashlib.sha256(b"".join(hashlib.sha256(p.read_bytes()).digest() for p in paths)).hexdigest()


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--manifest", type=Path, help="s1b/s1c jobs.json (not summarized engine-inputs.json)")
    p.add_argument("--source", type=Path, help="extracted shader include directory")
    p.add_argument("--virtual-root", type=Path, help="RaDefines.fx directory for this engine snapshot")
    p.add_argument("--job", action="append", default=[], help="exact job ID; repeat to restrict prewarming")
    p.add_argument("--cache", type=Path, default=REPO / "out/fx-cache")
    p.add_argument("--requests", type=Path, help="requests.txt; replay exactly these captured requests")
    p.add_argument("--work", type=Path, default=REPO / "out/fx-cache-build")
    p.add_argument("--dll", type=Path, default=APP / "prefix/drive_c/Battlefield 2/d3dx9_25.dll")
    p.add_argument("--wine", type=Path, default=APP / "wine/bin/wine64")
    p.add_argument("--timeout", type=int, default=900)
    p.add_argument("--parallel", type=int, default=1, help="concurrent fxcompile processes in the prefix")
    p.add_argument("--build-tools", action="store_true")
    p.add_argument("--capture-only", action="store_true", help="record manifest requests without Wine")
    p.add_argument("--flags", type=lambda s: int(s, 0), help="explicit compiler-flag override for preshader controls")
    p.add_argument("--retarget", action="store_true",
                   help="rebuild every request snapshot in --cache/requests with the current toolchain "
                        "(older cache format or translator); drops the old entries")
    p.add_argument("--identity-only", action="store_true",
                   help="write the cache's identity.json (DLL and translator hashes) and stop")
    p.add_argument("--record-toolchain", action="store_true", help=argparse.SUPPRESS)
    a = p.parse_args()
    if a.record_toolchain:
        metadata = dict(translator=translator_identity(), artifacts={str(path.relative_to(BUILD)):
            hashlib.sha256(path.read_bytes()).hexdigest() for path in (TOOL, BUILD / "fxcompile.exe", BUILD / "mojo/libmojoshader.a")})
        (BUILD / "toolchain.json").write_text(json.dumps(metadata, indent=2) + "\n")
        return 0
    cache, work = owned(a.cache), owned(a.work)
    cache.mkdir(parents=True, exist_ok=True)
    work.mkdir(parents=True, exist_ok=True)
    if a.build_tools:
        subprocess.run(["sh", str(Path(__file__).with_name("build-runtime-tools.sh"))], check=True)
    if not TOOL.is_file():
        raise ValueError("Build tools first: sh tools/macos/d3dxfx/build-runtime-tools.sh")
    dll_hash = hashlib.sha256(a.dll.read_bytes()).hexdigest()
    identity = translator_identity()
    metadata_path = BUILD / "toolchain.json"
    if not metadata_path.is_file():
        raise ValueError("Missing toolchain manifest; rebuild with build-runtime-tools.sh")
    metadata = json.loads(metadata_path.read_text())
    if metadata["translator"] != identity or any(hashlib.sha256((BUILD / name).read_bytes()).hexdigest() != digest
                                               for name, digest in metadata["artifacts"].items()):
        raise ValueError("Source/toolchain drift; rebuild with build-runtime-tools.sh")
    (cache / "identity.json").write_text(json.dumps(dict(version=cache_version(), dll_sha256=dll_hash,
                                                       translator=identity), indent=2) + "\n")
    if a.identity_only:
        print(f"Wrote {cache / 'identity.json'}; developer oracle metadata, not used by the client")
        return 0
    requests = []
    aliases = {}   # key -> every manifest job ID that formed it (identical requests share one key)
    if a.manifest:
        if not a.source or not a.source.is_dir():
            raise ValueError("--manifest requires an existing --source include corpus")
        jobs = json.loads(a.manifest.read_text())
        found = set()
        selected = []
        for index, job in enumerate(jobs):
            if a.job and job["id"] not in a.job:
                continue
            if job.get("kind") == "metadata":
                continue
            found.add(job["id"])
            selected.append((index, job))

        def capture(item):
            index, job = item
            directory = work / f"m{index}"
            directory.mkdir(exist_ok=True)
            defs = directory / "defines.txt"
            defs.write_text("".join(f"{k}={v}\n" for k, v in sorted(job["compiler_defines"].items())))
            source = Path(job["source"])
            # Generated inputs may move with a clone of the previous stage's outputs.
            if not source.is_file():
                source = a.manifest.parent / "generated" / source.name
            if not source.is_file():
                raise ValueError(f"Missing source for {job['id']}: {job['source']}")
            request = directory / "input.request"
            key = run("capture", source, defs, job.get("flags", 0) if a.flags is None else a.flags, dll_hash, identity,
                      a.source, job.get("include_mode", "baseline"),
                      job.get("virtual_root") or a.virtual_root or a.manifest.parent / "virtual",
                      request, directory / "key.txt")
            target = cache / "requests" / (key + ".request")
            target.parent.mkdir(exist_ok=True)
            if not target.is_file():
                tmp = target.with_suffix(f".tmp{index}")
                shutil.copyfile(request, tmp)
                os.replace(tmp, target)
            return key, target, job["id"]

        with ThreadPoolExecutor(max(1, a.parallel)) as pool:
            for key, target, job_id in pool.map(capture, selected):
                aliases.setdefault(key, []).append(job_id)
                requests.append((key, target, job_id))
        if set(a.job) - found:
            raise ValueError(f"Unknown job IDs: {set(a.job) - found}")
    if a.retarget:
        # Snapshots keep source, macros, flags and include bytes; only the toolchain identity
        # (and with it the key) changes. Old-format entries can never be loaded again.
        snapshots = sorted((cache / "requests").glob("*.request"))
        if not snapshots:
            raise ValueError(f"No request snapshots in {cache / 'requests'}; nothing to retarget")
        for old in snapshots:
            tmp = old.with_suffix(".retarget")
            key = run("retarget", old, dll_hash, identity, tmp)
            os.replace(tmp, cache / "requests" / (key + ".request"))
            if old.stem != key:
                old.unlink()
            requests.append((key, cache / "requests" / (key + ".request"), "retarget:" + old.stem))
        for stale in ("entries", "binary", "refused"):
            shutil.rmtree(cache / stale, ignore_errors=True)
        (cache / "requests.txt").write_text("")
        print(f"Retargeted {len(snapshots)} request snapshots to cache format {cache_version()}")
    if a.requests:
        for key in dict.fromkeys(a.requests.read_text().splitlines()):
            if len(key) != 64 or any(c not in "0123456789abcdef" for c in key):
                raise ValueError("Invalid request key")
            request = a.requests.parent / "requests" / (key + ".request")
            if run("key", request) != key:
                raise ValueError("Request key/hash mismatch")
            requests.append((key, request, "runtime"))
    requests = list({key: (key, request, job) for key, request, job in requests}.values())
    if not requests:
        raise ValueError("No requests selected; supply --manifest, --requests or --retarget")
    if a.capture_only:
        (cache / "requests.txt").write_text("".join(key + "\n" for key, _, _ in requests))
        # job id -> request key of every captured job (next_attempts.py reads it).
        index_path = cache / "capture-index.json"
        index = json.loads(index_path.read_text()) if index_path.exists() else {}
        index.update({job: key for key, jobs in aliases.items() for job in jobs})
        index_path.write_text(json.dumps(index, indent=0, sort_keys=True) + "\n")
        print(f"Captured {len(requests)} unique requests; no compiler was run")
        return 0
    batch = []
    pending = []
    results = []
    for index, (key, request, job) in enumerate(requests):
        directory = work / f"j{index}"
        directory.mkdir(exist_ok=True)
        run("unpack", request, directory)
        flags, compiler, translator = (directory / "info.txt").read_text().splitlines()
        if compiler != dll_hash or translator != identity:
            raise ValueError(f"Compiler/translator mismatch for {key}; retain the requested toolchain")
        if (directory / "source.fx").read_bytes().startswith(bytes((1, 9, 255, 254))):
            run("publish", request, directory / "source.fx", cache)
            run("verify", request, cache)
            results.append(dict(key=key, job=job, aliases=aliases.get(key, [job]), ok=True, result="published FX2 input",
                                directory=str(directory)))
            continue
        fields = [winpath(directory / "source.fx"), winpath(directory / "effect.fxo"),
                  winpath(directory / "compile.log"), winpath(directory), f"--flags={flags}",
                  "--snapshot=" + winpath(directory / "includes.tsv"),
                  "@" + winpath(directory / "defines.txt")]
        if any(any(c in field for c in "\t\r\n") for field in fields):
            raise ValueError("Invalid compiler manifest field")
        batch.append("\t".join(fields))
        for name in ("effect.fxo", "effect.fxo.status", "compile.log"):
            (directory / name).unlink(missing_ok=True)
        pending.append((key, request, job, directory))
    if not pending:
        (work / "results.json").write_text(json.dumps(dict(wine_exit=None, entries=results), indent=2) + "\n")
        print(f"Published {len(results)} FX2 entries; no Wine compiler needed")
        return 0
    def compile_batch(lines, tag):
        """One fresh marked prefix per batch; --parallel N round-robin chunks."""
        win32 = work / "win32"
        win32.mkdir(exist_ok=True)
        shutil.copyfile(BUILD / "fxcompile.exe", win32 / "fxcompile.exe")
        large_address_aware(win32 / "fxcompile.exe")
        shutil.copyfile(a.dll, win32 / "d3dx9_25.dll")
        # Fresh prefix for every invocation; all HOME/cache/temp writes remain under out/.
        prefix = work / ("wine-" + uuid.uuid4().hex)
        prefix.mkdir()
        (prefix / ".openbf2-s2").write_text("s2 effect cache compiler only\n")
        for name in ("home", "tmp", "cache"):
            (prefix / name).mkdir()
        env = dict(os.environ, WINEPREFIX=str(prefix), WINEARCH="win64", WINEDEBUG="-all",
                   WINEDLLOVERRIDES="d3dx9_25=n;mscoree,mshtml=", HOME=str(prefix / "home"),
                   TMPDIR=str(prefix / "tmp"), XDG_CACHE_HOME=str(prefix / "cache"),
                   DYLD_LIBRARY_PATH=str(APP.parent / "Frameworks"))
        manifest = work / f"{tag}.tsv"
        manifest.write_text("\n".join(lines) + "\n")
        chunks = [manifest] if a.parallel <= 1 else []
        if a.parallel > 1:
            for i in range(min(a.parallel, len(lines))):
                chunk = work / f"{tag}-{i}.tsv"
                chunk.write_text("\n".join(lines[i::a.parallel]) + "\n")
                chunks.append(chunk)
        with (work / f"wine-{tag}.stdout.log").open("w") as stdout, (work / f"wine-{tag}.stderr.log").open("w") as stderr:
            try:
                if len(chunks) > 1:
                    (work / "init.tsv").write_text("")
                    subprocess.run([str(a.wine), winpath(win32 / "fxcompile.exe"), "--batch", winpath(work / "init.tsv")],
                                   cwd=win32, env=env, stdout=stdout, stderr=stderr, timeout=600)
                procs = [subprocess.Popen([str(a.wine), winpath(win32 / "fxcompile.exe"), "--batch", winpath(chunk)],
                                          cwd=win32, env=env, stdout=stdout, stderr=stderr) for chunk in chunks]
                code = 0
                for proc in procs:
                    code = max(code, proc.wait(timeout=a.timeout))
            finally:
                # Stop only our fresh prefix, including after compiler failure or timeout.
                subprocess.run([str(a.wine.parent / "wineserver"), "-k"], env=env,
                               stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=30)
        return code

    rc = compile_batch(batch, "compile")
    reflections = []

    def finish(item):
        key, request, job, directory = item
        status = directory / "effect.fxo.status"
        ok = status.is_file() and status.read_text().splitlines()[0] == "ok"
        message = "compiler unavailable (no status; see wine.stderr.log)"
        refused = False
        if ok:
            try:
                run("publish", request, directory / "effect.fxo", cache)
                run("verify", request, cache)
                message = "published"
            except subprocess.CalledProcessError:
                ok, message = False, "FX2 parse/translation/publication failed"
        elif status.is_file():
            log = (directory / "compile.log").read_bytes() if (directory / "compile.log").is_file() else b""
            if "parse=0x00000000" in status.read_text() and b"There were no techniques" in log:
                # The compiler object exists (BF2's .mfx reflection); only CompileEffect fails.
                return dict(key=key, job=job, aliases=aliases.get(key, [job]), ok=False, reflection=True,
                            result="reflection pending", directory=str(directory), request=str(request))
            # A D3DX refusal: publish it so the client fails exactly as D3DX without a new request.
            run("publish-refused", request, status, directory / "compile.log", cache)
            refused, message = True, "refused by D3DX (refusal record published)"
        return dict(key=key, job=job, aliases=aliases.get(key, [job]), ok=ok, refused=refused, result=message,
                    directory=str(directory))

    with ThreadPoolExecutor(max(1, a.parallel)) as pool:
        results += list(pool.map(finish, pending))
    reflections = [r for r in results if r.get("reflection")]
    if reflections:
        lines = []
        for r in reflections:
            d = Path(r["directory"])
            (d / "reflect.fx").write_bytes((d / "source.fx").read_bytes().rstrip(b"\0") + REFLECTION_TECHNIQUE)
            for name in ("reflect.fxo", "reflect.fxo.status", "reflect.log"):
                (d / name).unlink(missing_ok=True)
            flags = (d / "info.txt").read_text().splitlines()[0]
            lines.append("\t".join([winpath(d / "reflect.fx"), winpath(d / "reflect.fxo"), winpath(d / "reflect.log"),
                                    winpath(d), f"--flags={flags}", "--snapshot=" + winpath(d / "includes.tsv"),
                                    "@" + winpath(d / "defines.txt")]))
        compile_batch(lines, "reflect")
        for r in reflections:
            d = Path(r["directory"])
            r.pop("reflection")
            try:
                run("publish-reflection", r.pop("request"), d / "effect.fxo.status", d / "compile.log",
                    d / "reflect.fxo", cache)
                r.update(ok=True, result="reflection-only record published")
            except subprocess.CalledProcessError:
                r.update(ok=False, result="reflection compile/publication failed (see reflect.log)")
    (work / "results.json").write_text(json.dumps(dict(wine_exit=rc, entries=results), indent=2) + "\n")
    refused = sum(bool(r.get("refused")) for r in results)
    print(f"Published {sum(r['ok'] for r in results)}/{len(results)} entries and {refused} refusal records; "
          f"{work / 'results.json'}")
    return 0 if all(r["ok"] or r.get("refused") for r in results) else 1


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        print(error, file=sys.stderr)
        sys.exit(2)
