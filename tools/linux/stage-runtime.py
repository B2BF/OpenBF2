#!/usr/bin/env python3
"""Copy explicitly missing SONAMEs from a dependency container to a private runtime directory.

[CHOSEN, not recovered] Issue #36. Run inside the dependency image, after inspecting ldd
in the actual launch environment. This does not install packages or replace system libraries.
Repeat with only the new missing SONAMEs until ldd resolves the private runtime's closure.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def protected(name):
    # These belong to the launch environment and its graphics driver, not an audio bundle.
    return name.startswith(("libc.so", "libm.so", "libdl.so", "libpthread.so", "librt.so",
                            "libstdc++", "libgcc", "libdrm", "libgbm", "libwayland",
                            "libGL", "libEGL", "libvulkan", "libMesa", "libnvidia",
                            "libxcb", "libX11", "libXext", "libXcursor", "libXi.so",
                            "libXfixes", "libXrandr", "libXrender", "libxkbcommon"))


def library_paths(names):
    cache = subprocess.check_output(["ldconfig", "-p"], text=True)
    result = {}
    for name in names:
        candidates = {Path(match).resolve() for match in
                      re.findall(r"^\s*" + re.escape(name) + r"\s+\([^\n]+?\)\s+=>\s+(\S+)", cache, re.M)}
        if not candidates:
            # PulseAudio's private libpulsecommon is outside the ldconfig cache.
            candidates = {path.resolve() for root in (Path("/usr/lib"), Path("/lib"))
                          for path in root.rglob(name) if path.is_file()}
        candidates = {path for path in candidates if path.is_file()}
        if len(candidates) != 1:
            raise RuntimeError(f"Expected one source for {name}, found: {sorted(map(str, candidates))}")
        result[name] = candidates.pop()
    return result


def package_owner(path):
    # Debian usr-merge installations can record either spelling in the package database.
    variants = [str(path)]
    if str(path).startswith("/usr/lib/"):
        variants.append(str(path)[4:])
    for variant in variants:
        result = subprocess.run(["dpkg-query", "-S", variant], text=True,
                                stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)
        if result.returncode == 0:
            owner = result.stdout.split(": ", 1)[0]
            version = subprocess.check_output(["dpkg-query", "-W", "-f=${Version}", owner], text=True)
            return {"package": owner, "version": version}
    return {"package": None, "version": None}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--destination", required=True, type=Path)
    parser.add_argument("--source-image", required=True, help="Dependency image tag/digest for provenance")
    parser.add_argument("sonames", nargs="+")
    args = parser.parse_args()
    destination = args.destination.resolve()
    if destination == Path("/") or any(destination == Path(root) or Path(root) in destination.parents
                                        for root in ("/usr", "/lib", "/lib64", "/bin", "/sbin", "/etc")):
        parser.error("Choose a private output directory outside system paths")
    for name in args.sonames:
        if not re.fullmatch(r"lib[A-Za-z0-9_.+-]+\.so(?:\.[A-Za-z0-9_.+-]+)?", name):
            parser.error(f"Not a SONAME: {name}")
        if protected(name):
            parser.error(f"Keep this library supplied by the launch environment: {name}")
    sources = library_paths(args.sonames)
    destination.mkdir(parents=True, exist_ok=True)
    manifest = destination / "staged-libraries.json"
    records = json.loads(manifest.read_text()) if manifest.exists() else {}
    for name, source in sources.items():
        target = destination / name
        checksum = digest(source)
        if target.exists() and digest(target) != checksum:
            raise RuntimeError(f"Refusing to replace a different existing library: {target}")
        if not target.exists():
            # Follow SONAME symlinks so the private directory is self-contained.
            shutil.copyfile(source, target)
        records[name] = {"source": str(source), "source_image": args.source_image,
                         "sha256": checksum, **package_owner(source)}
        print(f"{name}: {source}", flush=True)
    manifest.write_text(json.dumps(records, indent=2, sort_keys=True) + "\n")


if __name__ == "__main__":
    main()
