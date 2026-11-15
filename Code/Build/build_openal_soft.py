#!/usr/bin/env python3
"""Build OpenAL Soft as the Windows clients' BF2OpenAL.dll from a pinned release.

[CHOSEN, not recovered] Retail BF2 ships Creative's 32-bit OpenAL wrapper renamed BF2OpenAL.dll,
which an x64 process cannot load (docs/building.md, "Native x64 client"). Both Windows client
packages (x86 and x64) carry an unmodified OpenAL Soft build under that name instead, so both use
the same provider (docs/ci-releases.md). Run inside the MSVC developer environment of the target
architecture (Code/Build/vsdevcmd.cmd x86|x64) with CMake and Ninja on PATH, and pass --arch.

Outputs in --stage-dir: BF2OpenAL.dll, COPYING, BUILD.txt (version, source, options) and the exact
source tarball. With --client-dir the DLL is also copied there. A stage directory whose BUILD.txt
matches this script's pins is reused as it is (the CI cache).
"""

import argparse
import hashlib
from pathlib import Path
import shutil
import subprocess
import sys
import tarfile
import urllib.request

VERSION = "1.25.2"
TARBALL = "openal-soft-{}.tar.bz2".format(VERSION)
URL = "https://openal-soft.org/openal-releases/" + TARBALL
# The official tarball; its contents match the upstream tag 1.25.2 (github.com/kcat/openal-soft).
SHA256 = "1dbaac44e7579d5bc8847ca8db4b2e8b9fd3961041f35ee20def4958301e1089"

# Shared library with the legacy EAX extensions BF2's "+ EAX" providers ask for, the static MSVC
# runtime (no VC++ redistributable needed for this DLL), and no utilities, examples or tests.
# FORCE_STATIC_VCRT is OpenAL Soft's own switch; CMAKE_MSVC_RUNTIME_LIBRARY covers CMake's runtime
# abstraction (policy CMP0091), under which the switch's flag rewrite finds nothing to rewrite.
CMAKE_OPTIONS = [
    "-DCMAKE_BUILD_TYPE=Release",
    "-DLIBTYPE=SHARED",
    "-DALSOFT_EAX=ON",
    "-DFORCE_STATIC_VCRT=ON",
    "-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded",
    "-DALSOFT_UTILS=OFF",
    "-DALSOFT_NO_CONFIG_UTIL=ON",
    "-DALSOFT_EXAMPLES=OFF",
    "-DALSOFT_TESTS=OFF",
    "-DALSOFT_INSTALL=OFF",
    "-DALSOFT_UPDATE_BUILD_VERSION=OFF",
    "-DALSOFT_ENABLE_MODULES=OFF",
]


def build_info(arch):
    return ("OpenAL Soft {version}\n"
            "Source: {url}\n"
            "Source SHA-256: {sha}\n"
            "Source changes: none\n"
            "Built with: MSVC {arch}, CMake/Ninja, target OpenAL (OpenAL32.dll renamed BF2OpenAL.dll)\n"
            "CMake options: {options}\n").format(version=VERSION, url=URL, sha=SHA256, arch=arch,
                                                 options=" ".join(CMAKE_OPTIONS))


def sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def fetch(path):
    if not path.is_file() or sha256(path) != SHA256:
        print("Downloading", URL)
        with urllib.request.urlopen(URL, timeout=120) as response, path.open("wb") as out:
            shutil.copyfileobj(response, out)
    actual = sha256(path)
    if actual != SHA256:
        path.unlink()
        sys.exit("{}: SHA-256 {} does not match the pinned {}".format(TARBALL, actual, SHA256))


def extract(tarball, destination):
    root = destination.resolve()
    with tarfile.open(tarball, "r:bz2") as archive:
        for member in archive.getmembers():
            target = (root / member.name).resolve()
            if root not in target.parents and target != root:
                sys.exit("{}: unexpected path {}".format(TARBALL, member.name))
            if not (member.isfile() or member.isdir()):
                sys.exit("{}: unexpected entry type for {}".format(TARBALL, member.name))
        archive.extractall(root)
    return root / "openal-soft-{}".format(VERSION)


def check_runtime(dll):
    """The DLL must not need the VC++ redistributable (static runtime)."""
    if not shutil.which("dumpbin"):
        print("dumpbin not on PATH; runtime dependency check skipped")
        return
    listing = subprocess.run(["dumpbin", "/nologo", "/dependents", str(dll)], check=True,
                             capture_output=True, text=True).stdout
    print(listing)
    for name in ("VCRUNTIME", "MSVCP", "api-ms-win-crt-", "ucrtbase"):
        if name.lower() in listing.lower():
            sys.exit("{}: depends on the shared MSVC runtime ({})".format(dll.name, name))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--arch", choices=("x86", "x64"), required=True,
                        help="the architecture of the developer environment this runs in")
    parser.add_argument("--stage-dir", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    parser.add_argument("--client-dir", type=Path)
    args = parser.parse_args()

    stage = args.stage_dir
    info = build_info(args.arch)
    staged = all((stage / name).is_file() for name in ("BF2OpenAL.dll", "COPYING", "BUILD.txt", TARBALL))
    if staged and (stage / "BUILD.txt").read_text() == info and sha256(stage / TARBALL) == SHA256:
        print("Reusing", stage)
    else:
        args.work_dir.mkdir(parents=True, exist_ok=True)
        tarball = args.work_dir / TARBALL
        fetch(tarball)
        source = extract(tarball, args.work_dir / "src")
        build = args.work_dir / "build"
        compilers = ["-DCMAKE_C_COMPILER=cl", "-DCMAKE_CXX_COMPILER=cl"] if sys.platform == "win32" else []
        subprocess.run(["cmake", "-S", str(source), "-B", str(build), "-G", "Ninja"] + compilers + CMAKE_OPTIONS,
                       check=True)
        subprocess.run(["cmake", "--build", str(build), "--target", "OpenAL"], check=True)
        dll = build / "OpenAL32.dll"
        if not dll.is_file():
            sys.exit("OpenAL32.dll was not built in {}".format(build))
        check_runtime(dll)
        stage.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(dll, stage / "BF2OpenAL.dll")
        shutil.copyfile(source / "COPYING", stage / "COPYING")
        shutil.copyfile(tarball, stage / TARBALL)
        (stage / "BUILD.txt").write_text(info)
    if args.client_dir:
        args.client_dir.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(stage / "BF2OpenAL.dll", args.client_dir / "BF2OpenAL.dll")
        print("Staged", args.client_dir / "BF2OpenAL.dll")


if __name__ == "__main__":
    main()
