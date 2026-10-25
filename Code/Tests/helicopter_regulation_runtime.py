#!/usr/bin/env python3
"""Strict Linux console regression using an existing complete dedicated CMake build.

Example: python3 Code/Tests/helicopter_regulation_runtime.py --engine-build ~/bf2-build \
    --output ~/bf2-helicopter-test --link-command ~/saved-link-command.txt
Use --module-object with a saved pre-fix module object for the expected-failure control.
Only the isolated output is written; no engine object or server process is modified.
"""
import argparse
import json
import os
from pathlib import Path
import shlex
import subprocess
import sys


def run(argv, cwd, log, env=None):
    log.write("$ " + shlex.join(str(x) for x in argv) + "\n")
    log.flush()
    subprocess.run(argv, cwd=cwd, env=env, stdout=log, stderr=subprocess.STDOUT, check=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--engine-build", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--link-command", required=True, type=Path)
    parser.add_argument("--module-object", type=Path)
    args = parser.parse_args()
    if sys.platform != "linux":
        parser.error("Run this engine-object fixture inside the configured Linux environment")
    build = args.engine_build.resolve()
    output = args.output.resolve()
    if output == build or build in output.parents:
        parser.error("The fixture output must be outside the engine build")
    output.mkdir(parents=True, exist_ok=True)
    commands = json.loads((build / "compile_commands.json").read_text())
    entry = next(row for row in commands if row["file"].endswith("/Game/Main/BF2.cpp"))
    compile_argv = shlex.split(entry["command"])
    original_main = (build / compile_argv[compile_argv.index("-o") + 1]).resolve()
    compile_argv[compile_argv.index("-o") + 1] = str(output / "fixture.o")
    compile_argv[compile_argv.index("-c") + 1] = str(Path(__file__).with_suffix(".cpp").resolve())
    main_copy = output / "BF2.fixture-main.o"
    link = shlex.split(args.link_command.read_text().strip())
    if link[:2] != [":", "&&"] or link[-2:] != ["&&", ":"]:
        parser.error("Expected one CMake Ninja link command with explicit && wrappers")
    link = link[2:-2]
    if any(flag.startswith("-Wl,--unresolved-symbols") or flag == "-Wl,--allow-shlib-undefined"
           for flag in link):
        parser.error("The input link must not suppress unresolved symbols")
    exe = output / "helicopter_regulation_runtime"
    link[link.index("-o") + 1] = str(exe)
    main_count = module_count = 0
    for i, arg in enumerate(link):
        if arg.startswith("-Wl,-Map="):
            link[i] = "-Wl,-Map=" + str(output / "fixture.map")
        elif arg.startswith("-Wl,--dependency-file="):
            link[i] = "-Wl,--dependency-file=" + str(output / "fixture.link.d")
        elif arg.endswith("/Game/Main/BF2.cpp.o"):
            assert (build / arg).resolve() == original_main
            link[i] = str(main_copy)
            main_count += 1
        elif arg.endswith("/Game/Objects/Game/ModuleWorldObjTemplGame.cpp.o"):
            if args.module_object:
                link[i] = str(args.module_object.resolve(strict=True))
            module_count += 1
    if main_count != 1 or module_count != 1:
        parser.error("Expected exactly one main object and one game-template module object")
    link.append(str(output / "fixture.o"))
    with (output / "build.log").open("w") as log:
        run(compile_argv, build, log)
        run(["objcopy", "--redefine-sym", "main=bf2_fixture_unused_main",
             str(original_main), str(main_copy)], build, log)
        run(link, build, log)
    env = os.environ.copy()
    library_dirs = [str(build / "bin_linux")]
    for arg in link:
        if not arg.startswith("-") and ".so" in Path(arg).name:
            directory = str((build / arg).resolve().parent)
            if directory not in library_dirs:
                library_dirs.append(directory)
    if env.get("LD_LIBRARY_PATH"):
        library_dirs.append(env["LD_LIBRARY_PATH"])
    env["LD_LIBRARY_PATH"] = os.pathsep.join(library_dirs)
    with (output / "runtime.log").open("w") as log:
        result = subprocess.run([str(exe)], cwd=output, env=env, stdout=log,
                                stderr=subprocess.STDOUT)
    print((output / "runtime.log").read_text(), end="")
    return result.returncode


if __name__ == "__main__":
    sys.exit(main())


