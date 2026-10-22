#!/usr/bin/env python3
"""Build the actual BundleTemplate TU with existing runtime-pair support; no server is launched."""
import argparse
import os
from pathlib import Path
import re
import subprocess
import sys

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("output", type=Path, help="isolated build directory")
parser.add_argument("--baseline", type=Path, help="saved complete pre-fix BundleTemplate.cpp")
parser.add_argument("--mode", choices=("all", "sound", "decal"), default="all")
parser.add_argument("--full-pair", action="store_true", help="run the complete existing MSVC x86 suite")
args = parser.parse_args()
tests = Path(__file__).resolve().parent
repo = tests.parent.parent
bf2 = repo / "Code/BF2"
out = args.output.resolve()
out.mkdir(parents=True, exist_ok=True)
sources = [tests / "bundle_sound_dispatch_runtime.cpp"]
if args.full_pair:
    if os.name != "nt" or args.mode != "all":
        raise SystemExit("--full-pair is the MSVC x86 suite and cannot select a focused mode")
    sources[0] = tests / "runtime_pair.cpp"
manifest = (tests / "runtime_pair.cmd").read_text()
sources += [(tests / name.replace("\\", "/")).resolve()
            for name in re.findall(r"(\.\.\\BF2\\[^\s]+\.cpp)", manifest)]
if not sources[1:] or len(set(sources)) != len(sources):
    raise SystemExit("runtime_pair.cmd source manifest is missing or contains duplicates")
if args.baseline:
    original = bf2 / "Game/Objects/Base/BundleTemplate.cpp"
    source = args.baseline.read_text()
    # Relocate includes only; preserve the complete saved production TU and all its method bodies.
    source = re.sub(r'^#include "([^"]+)"',
                    lambda m: '#include "' + (original.parent / m[1]).resolve().as_posix() + '"',
                    source, flags=re.MULTILINE)
    saved = out / "BundleTemplate.baseline.cpp"
    saved.write_text(source)
    if sources.count(original) != 1:
        raise SystemExit("expected exactly one production BundleTemplate source")
    sources[sources.index(original)] = saved

if os.name == "nt":
    command = ["cl", "/nologo", "/EHsc", "/O2", "/std:c++14", "/permissive", "/Gy",
               "/DDEDICATED=1", "/FI" + str(bf2 / "Core/BuildConfig.h")]
    executable = out / "bundle_sound_dispatch_runtime.exe"
    command += list(map(str, sources)) + ["/link", "/OPT:REF", "/MANIFEST:EMBED",
                                        "/MANIFESTUAC:level='asInvoker' uiAccess='false'",
                                        "/OUT:" + str(executable)]
else:
    command = [os.environ.get("CXX", "g++"), "-std=c++14", "-O2", "-g", "-fpermissive",
               "-fno-strict-aliasing", "-fwrapv", "-fno-delete-null-pointer-checks",
               "-ffp-contract=off", "-fno-omit-frame-pointer", "-fms-extensions",
               "-Wno-narrowing", "-Wno-unknown-pragmas", "-Wno-multichar",
               "-Wno-deprecated-declarations", "-Wno-write-strings", "-pthread",
               "-ffunction-sections", "-fdata-sections", "-DDEDICATED=1", "-D_LINUX",
               "-include", str(bf2 / "Core/BuildConfig.h"),
               "-include", str(bf2 / "Core/PlatformCompat.h")]
    executable = out / "bundle_sound_dispatch_runtime"
    command += list(map(str, sources)) + ["-Wl,--gc-sections", "-o", str(executable)]
(out / "command.txt").write_text(subprocess.list2cmdline(command) + "\n")
with (out / "build.log").open("w") as log:
    result = subprocess.run(command, cwd=out, stdout=log, stderr=subprocess.STDOUT)
if result.returncode:
    print((out / "build.log").read_text(errors="replace"))
    raise SystemExit(result.returncode)
with (out / "runtime.log").open("w") as log:
    result = subprocess.run([str(executable)] + ([] if args.full_pair else [args.mode]),
                            cwd=out, stdout=log, stderr=subprocess.STDOUT)
print((out / "runtime.log").read_text(errors="replace"), end="")
if result.returncode < 0:
    print("Fixture terminated by signal", -result.returncode)
raise SystemExit(result.returncode if result.returncode >= 0 else 128 - result.returncode)
