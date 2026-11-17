#!/usr/bin/env python3
"""Build local vkd3d 2.1/MojoShader sources for Linux, applying the shared port patches.

[CHOSEN, not recovered] kr4uzi/OpenBF2#36. No downloads or package installation.
Run natively on x86-64 or ARM64; output must be new or an identical prior invocation.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import shlex
import shutil
import subprocess


def source_hash(root):
    digest = hashlib.sha256()
    for path in sorted(root.rglob("*")):
        relative = path.relative_to(root)
        if ".git" in relative.parts or not path.is_file():
            continue
        digest.update(str(relative).encode() + b"\0")
        digest.update(hashlib.sha256(path.read_bytes()).digest())
    return digest.hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--vkd3d-source", type=Path, required=True)
    parser.add_argument("--mojo-source", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--jobs", type=int, default=4)
    args = parser.parse_args()
    if platform.system() != "Linux" or platform.machine() not in ("x86_64", "aarch64", "arm64"):
        parser.error("Run this native build on Linux x86-64 or ARM64")
    if args.jobs < 1:
        parser.error("--jobs must be positive")
    root = Path(__file__).resolve().parents[2]
    source, mojo_source, out = (p.resolve() for p in (args.vkd3d_source, args.mojo_source, args.output))
    for path in (source / "configure", mojo_source / "mojoshader.h"):
        if not path.is_file():
            parser.error(f"Missing local source: {path}")
    for path in (source, mojo_source):
        if out == path or out.is_relative_to(path) or path.is_relative_to(out):
            parser.error("Output and pristine source trees must be separate")
    patches = root / "tools/macos/vkd3d-patches"
    # Native object code keeps these C archives usable by another GCC/Clang version.
    flags = "-O2 -g -fPIC -fno-lto -DVKD3D_NO_TRACE_MESSAGES -DVKD3D_NO_DEBUG_MESSAGES"
    inputs = dict(vkd3d=source_hash(source), mojo=source_hash(mojo_source),
                  vkd3d_patches=source_hash(patches),
                  mojo_patches=source_hash(root / "tools/macos/d3dxfx/patches"),
                  machine=platform.machine(), flags=flags,
                  builder=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
                  compiler=subprocess.check_output(shlex.split(os.environ.get("CC", "cc")) + ["--version"], text=True),
                  environment={k: os.environ.get(k) for k in ("CC", "CXX", "CPPFLAGS", "LDFLAGS", "WIDL")})
    stamp = out / "inputs.json"
    if out.exists() and any(out.iterdir()):
        if not stamp.is_file() or json.loads(stamp.read_text()) != inputs:
            parser.error("Output has different or incomplete source inputs; choose a new output directory")
    else:
        out.mkdir(parents=True, exist_ok=True)
        stamp.write_text(json.dumps(inputs, indent=2) + "\n")
    logs = out / "logs"
    logs.mkdir(exist_ok=True)

    def run(label, command, cwd=out, env=None):
        print(label, flush=True)
        log = logs / (label + ".log")
        with log.open("w") as stream:
            result = subprocess.run([str(arg) for arg in command], cwd=cwd, env=env,
                                    stdout=stream, stderr=subprocess.STDOUT)
        if result.returncode:
            raise RuntimeError(f"{label} failed; {log}\n" + "\n".join(log.read_text().splitlines()[-30:]))

    # Once copied, these private trees are patched in place. Never resync or delete user trees.
    for original, name in ((source, "vkd3d-src"), (mojo_source, "mojo-src")):
        if not (out / name).exists():
            shutil.copytree(original, out / name, ignore=shutil.ignore_patterns(".git"))
    for patch in sorted(patches.glob("*.patch")):
        command = ["patch", "-p1", "--batch", "--fuzz=0", "-i", str(patch)]
        probe = subprocess.run(command + ["--forward", "--dry-run"], cwd=out / "vkd3d-src",
                               stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        if probe.returncode == 0:
            run("patch-" + patch.stem, command + ["--forward"], cwd=out / "vkd3d-src")
        else:
            run("patch-check-" + patch.stem, command + ["--reverse", "--dry-run"], cwd=out / "vkd3d-src")
    (out / "vkd3d-src/.bf2-patches").write_text("".join(p.name + "\n" for p in sorted(patches.glob("*.patch"))))
    run("mojo-patches", ["python3", root / "tools/macos/nativefx/patch-mojo.py",
                         out / "mojo-src", out / "mojo-patches.json"])
    build = out / "vkd3d"
    build.mkdir(exist_ok=True)
    env = dict(os.environ, CFLAGS=flags)
    run("vkd3d-configure", [out / "vkd3d-src/configure", "--disable-tests", "--disable-demos",
        "--disable-shared", "--without-ncurses", "--without-opengl", "--without-xcb", "--without-spirv-tools"],
        cwd=build, env=env)
    run("vkd3d-headers", ["make", "-f", "Makefile", "-f", root / "tools/macos/nativefx/headers.mk", "nativefx-headers"], cwd=build)
    run("vkd3d-build", ["make", f"-j{args.jobs}", "libvkd3d-shader.la"], cwd=build)
    run("mojo-configure", ["cmake", "-S", out / "mojo-src", "-B", out / "mojo", "-G", "Ninja",
        "-DCMAKE_BUILD_TYPE=Release", "-DCMAKE_POSITION_INDEPENDENT_CODE=ON", "-DBUILD_SHARED_LIBS=OFF",
        "-DPROFILE_METAL=OFF", "-DPROFILE_BYTECODE=ON", "-DEFFECT_SUPPORT=ON",
        "-DFLIP_VIEWPORT=OFF", "-DDEPTH_CLIPPING=OFF", "-DXNA4_VERTEXTEXTURE=OFF", "-DGIT=GIT-NOTFOUND"])
    run("mojo-build", ["cmake", "--build", out / "mojo", "--target", "mojoshader", f"-j{args.jobs}"])
    print(f"BF2_FX_NATIVE_DEPS={out}", flush=True)


if __name__ == "__main__":
    main()
