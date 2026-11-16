#!/usr/bin/env python3
"""Build the issue #36 Linux SDK from checksum-locked upstream distributions.

This is distribution policy, not recovered engine behavior. The exported OCI
manifest identifies the complete result, including Ubuntu security updates.
"""

import argparse
import hashlib
import json
import os
import re
import shlex
import shutil
import subprocess
import tarfile
import time
import urllib.request
import urllib.parse
from pathlib import Path


PREFIX = Path("/opt/openbf2-sdk")
SOURCES = Path("/opt/openbf2-sources")
BUILD = Path("/opt/openbf2-sdk-build")
DOWNLOADS = BUILD / "downloads"


def run(*args, cwd=None, capture=False, env=None):
    print("+", shlex.join(map(str, args)), flush=True)
    return subprocess.run(list(map(str, args)), cwd=cwd, check=True, env=env,
                          text=True, stdout=subprocess.PIPE if capture else None).stdout


def download(item):
    """Never reuse a cached distribution without checking its locked digest."""
    print(f"Verifying/downloading {item['url']}", flush=True)
    target = DOWNLOADS / (item["sha256"] + "-" + item["url"].rsplit("/", 1)[-1])
    for attempt in range(4):
        if target.exists() and file_hash(target) == item["sha256"]:
            return target
        try:
            with urllib.request.urlopen(item["url"], timeout=120) as source, target.open("wb") as output:
                shutil.copyfileobj(source, output)
            with target.open("rb") as source:
                digest = hashlib.sha256()
                for block in iter(lambda: source.read(1024 * 1024), b""):
                    digest.update(block)
            if digest.hexdigest() != item["sha256"]:
                raise ValueError(f"Checksum mismatch: {item['url']}")
            return target
        except (OSError, ValueError):
            target.unlink(missing_ok=True)
            if attempt == 3:
                raise
            time.sleep(2 ** attempt)
    raise AssertionError("unreachable")


def file_hash(path):
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def unpack(archive, destination):
    """Strip the distribution's one top directory; reject escaping members."""
    print(f"Extracting {archive.name} to {destination}", flush=True)
    if destination.exists() and any(destination.iterdir()):
        raise ValueError(f"Refusing to replace populated source directory: {destination}")
    destination.mkdir(parents=True, exist_ok=True)
    with tarfile.open(archive) as source:
        members = source.getmembers()
        roots = {Path(member.name).parts[0] for member in members if Path(member.name).parts}
        if len(roots) != 1:
            raise ValueError(f"Expected one archive root: {archive}")
        root = roots.pop()
        extraction = destination.parent / (destination.name + ".extract")
        extraction.mkdir()
        for member in members:
            path = Path(member.name)
            if path.is_absolute() or ".." in path.parts or member.isdev():
                raise ValueError(f"Unsafe archive member: {member.name}")
            if member.issym() or member.islnk():
                target = (path.parent / member.linkname) if member.issym() else Path(member.linkname)
                resolved = (extraction / target).resolve()
                if not resolved.is_relative_to(extraction.resolve()):
                    raise ValueError(f"Escaping archive link: {member.name}")
        source.extractall(extraction)
        for child in (extraction / root).iterdir():
            shutil.move(str(child), destination / child.name)
        shutil.rmtree(extraction)


def snapshot():
    return {str(path.relative_to(PREFIX)) for path in PREFIX.rglob("*") if path.is_file()}


def copy_notices(item):
    root = SOURCES / item["destination"]
    primary = root / item["license"]
    if not primary.is_file():
        raise ValueError(f"Missing upstream license: {primary}")
    notices = {primary}
    for path in root.rglob("*"):
        if path.is_file() and ("LICENSES" in path.parts or re.match(
                r"^(LICENSE|LICENCE|COPYING|COPYRIGHT|NOTICE)(\b|[._-])", path.name, re.I)):
            notices.add(path)
    target = PREFIX / "share/licenses" / item["name"] / "LICENSES.txt"
    target.parent.mkdir(parents=True, exist_ok=True)
    with target.open("w") as output:
        for path in sorted(notices):
            output.write(f"\n--- {path.relative_to(root)} ---\n")
            output.write(path.read_text(errors="replace"))
            output.write("\n")
    return str(target.relative_to(PREFIX))


def package_catalog(lock):
    """Record actual installed Debian providers while repository metadata exists."""
    inventory = run("dpkg-query", "-W", "-f=${binary:Package}\t${Version}\t${source:Package}\t${source:Version}\n", capture=True)
    (PREFIX / "share/openbf2/packages.tsv").write_text(inventory)
    packages = [line.split("\t") for line in inventory.splitlines()]
    pinned = {item["name"]: item for item in lock["debs"]}
    ordinary = [f"{name}={version}" for name, version, _, _ in packages if name.split(":")[0] not in pinned]
    # apt download prints the selected mirror URL and cryptographic archive hash;
    # it does not download a second copy of every installed development package.
    uris = run("apt-get", "--print-uris", "download", *ordinary, capture=True)
    by_filename = {}
    for line in uris.splitlines():
        fields = shlex.split(line)
        if len(fields) == 4 and ":" in fields[3]:
            filename = urllib.parse.unquote(urllib.parse.urlparse(fields[0]).path).rsplit("/", 1)[-1]
            by_filename[filename] = fields[0]
    records = run("apt-cache", "show", *ordinary, capture=True)
    available = {}
    for paragraph in records.split("\n\n"):
        record = dict(line.split(": ", 1) for line in paragraph.splitlines() if ": " in line and not line.startswith(" "))
        if "Package" in record and "Version" in record and "SHA256" in record:
            available[(record["Package"], record["Version"])] = record
    result = {}
    for name, version, source, source_version in packages:
        base = name.split(":")[0]
        if base in pinned:
            item = pinned[base]
            if item["version"] != version:
                raise ValueError(f"Pinned compiler package changed: {name}={version}")
            url, digest = item["url"], item["sha256"]
        else:
            record = available[(base, version)]
            digest = record["SHA256"]
            url = by_filename[Path(record["Filename"]).name]
        result[f"{name}={version}"] = {
            "name": name, "version": version, "source": url, "sha256": digest,
            "source_package": source, "source_version": source_version,
            "license": f"/usr/share/doc/{base}/copyright",
        }
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--revision", required=True)
    parser.add_argument("--jobs", type=int, default=2)
    args = parser.parse_args()
    if not re.fullmatch(r"[0-9a-f]{40}", args.revision) or args.jobs < 1:
        parser.error("Use a complete SDK source revision and positive job count")
    lock_path = Path(__file__).with_name("sdk-sources.json")
    lock = json.loads(lock_path.read_text())
    for directory in (PREFIX, SOURCES, BUILD, DOWNLOADS, PREFIX / "share/openbf2"):
        directory.mkdir(parents=True, exist_ok=True)
    debs = [download(item) for item in lock["debs"]]
    run("apt-get", "install", "-y", "--no-install-recommends", *debs)
    if run("g++-15", "-dumpfullversion", capture=True).strip() != "15.2.0":
        raise ValueError("Unexpected C++ compiler")
    if run("getconf", "GNU_LIBC_VERSION", capture=True).strip() != "glibc 2.35":
        raise ValueError("Unexpected glibc baseline")
    system_packages = package_catalog(lock)

    dependencies, owners = [], {}
    for item in lock["sources"]:
        unpack(download(item), SOURCES / item["destination"])
        dependencies.append({
            "name": item["name"], "version": item["version"],
            "source": item["url"], "sha256": item["sha256"],
            "license": copy_notices(item), "requires": item.get("requires", []),
        })

    shutil.copytree(SOURCES / "cmake", "/opt/cmake", symlinks=True)
    run("sh", SOURCES / "rust/install.sh", "--prefix=/opt/rust", "--disable-ldconfig",
        "--components=rustc,cargo,rust-std-x86_64-unknown-linux-gnu")
    shutil.copytree(SOURCES / "boost", "/opt/boost-1.89.0", symlinks=True)
    # Meson's source entry point has no third-party Python dependencies.
    (PREFIX / "bin").mkdir(exist_ok=True)
    (PREFIX / "bin/meson").symlink_to(SOURCES / "meson/meson.py")
    os.environ.update({
        "PATH": f"/opt/cmake/bin:/opt/rust/bin:{PREFIX}/bin:" + os.environ["PATH"],
        "CC": "gcc-15", "CXX": "g++-15", "CMAKE_PREFIX_PATH": str(PREFIX),
        "PKG_CONFIG_PATH": str(PREFIX / "lib/pkgconfig"),
        "LD_LIBRARY_PATH": str(PREFIX / "lib"),
    })
    flags = (f"-march=x86-64 -mtune=generic -ffile-prefix-map={SOURCES}=./sources "
             f"-ffile-prefix-map={BUILD}=./build -ffile-prefix-map={PREFIX}=./sdk")
    os.environ.update(CFLAGS=flags, CXXFLAGS=flags)

    def cmake(name, source, *options):
        before = snapshot()
        build = BUILD / source
        run("cmake", "-S", SOURCES / source, "-B", build, "-G", "Ninja",
            "-DCMAKE_BUILD_TYPE=Release", f"-DCMAKE_INSTALL_PREFIX={PREFIX}",
            "-DCMAKE_INSTALL_LIBDIR=lib", "-DCMAKE_SKIP_INSTALL_RPATH=ON",
            "-DBUILD_TESTING=OFF", *options)
        run("cmake", "--build", build, "--parallel", args.jobs)
        run("cmake", "--install", build)
        owners.update({path: name for path in snapshot() - before})

    cmake("glslang", "glslang", "-DENABLE_OPT=OFF", "-DENABLE_HLSL=OFF",
          "-DGLSLANG_TESTS=OFF", "-DENABLE_GLSLANG_BINARIES=ON",
          "-DBUILD_EXTERNAL=OFF", "-DBUILD_SHARED_LIBS=OFF")
    cmake("SDL3", "sdl", "-DSDL_SHARED=ON", "-DSDL_STATIC=OFF", "-DSDL_TESTS=OFF",
          "-DSDL_TEST_LIBRARY=OFF", "-DSDL_ALSA=ON", "-DSDL_PULSEAUDIO=ON",
          "-DSDL_PIPEWIRE=OFF", "-DSDL_JACK=OFF", "-DSDL_SNDIO=OFF",
          "-DSDL_OSS=OFF", "-DSDL_IBUS=OFF", "-DSDL_KMSDRM=OFF",
          "-DSDL_WAYLAND_LIBDECOR=OFF", "-DSDL_X11=ON", "-DSDL_WAYLAND=ON",
          "-DSDL_X11_SHARED=OFF", "-DSDL_WAYLAND_SHARED=OFF",
          "-DSDL_LIBUDEV=ON", "-DSDL_HIDAPI_LIBUSB=OFF")
    cmake("OpenAL-Soft", "openal", "-DALSOFT_UTILS=OFF", "-DALSOFT_EXAMPLES=OFF",
          "-DALSOFT_TESTS=OFF", "-DALSOFT_BACKEND_PIPEWIRE=OFF",
          "-DALSOFT_BACKEND_JACK=OFF", "-DALSOFT_BACKEND_OSS=OFF",
          "-DALSOFT_BACKEND_SNDIO=OFF", "-DALSOFT_BACKEND_ALSA=ON",
          "-DALSOFT_BACKEND_PULSEAUDIO=ON", "-DALSOFT_BACKEND_PORTAUDIO=OFF",
          "-DALSOFT_BACKEND_SDL2=OFF", "-DALSOFT_BACKEND_SDL3=OFF",
          "-DALSOFT_SEARCH_INSTALL_DATADIR=OFF")
    cmake("QtBase", "qtbase", "-DQT_BUILD_EXAMPLES=OFF", "-DQT_BUILD_TESTS=OFF",
          "-DQT_BUILD_BENCHMARKS=OFF", "-DFEATURE_relocatable=ON",
          "-DFEATURE_dbus=ON", "-DFEATURE_mimetype=ON", "-DFEATURE_regularexpression=ON",
          "-DFEATURE_xcb=ON", "-DFEATURE_xkbcommon_x11=ON", "-DFEATURE_opengl=ON",
          "-DINSTALL_PLUGINSDIR=lib/qt6/plugins")
    cmake("QtWayland", "qtwayland", "-DQT_BUILD_EXAMPLES=OFF", "-DQT_BUILD_TESTS=OFF",
          "-DFEATURE_wayland_client=ON", "-DFEATURE_wayland_server=OFF",
          "-DINSTALL_PLUGINSDIR=lib/qt6/plugins")

    # DXVK Native 3.1.1 unnecessarily raises its compiler baseline to SSE3.
    # The x86-64 package contract is SSE2; retain all other upstream switches.
    meson_file = SOURCES / "dxvk/meson.build"
    text = meson_file.read_text()
    if text.count("  '-msse3',\n") != 1:
        raise ValueError("DXVK's SSE3 option moved; reassess the baseline patch")
    meson_file.write_text(text.replace("  '-msse3',\n", ""))
    next(item for item in dependencies if item["name"] == "DXVK")["patches"] = [
        {"description": "Remove compiler -msse3 option to preserve x86-64/SSE2 baseline",
         "file": "meson.build", "removed": "  '-msse3',"}]
    before = snapshot()
    run("meson", "setup", BUILD / "dxvk", SOURCES / "dxvk", "--buildtype=release",
        f"--prefix={PREFIX}", "--libdir=lib", "--wrap-mode=nodownload",
        "-Denable_dxgi=false", "-Denable_d3d8=false", "-Denable_d3d9=true",
        "-Denable_d3d10=false", "-Denable_d3d11=false", "-Dnative_sdl3=enabled",
        "-Dnative_sdl2=disabled", "-Dnative_glfw=disabled")
    run("meson", "compile", "-C", BUILD / "dxvk", "-j", args.jobs)
    run("meson", "install", "-C", BUILD / "dxvk")
    owners.update({path: "DXVK" for path in snapshot() - before})

    # A successful configure can silently omit a platform plugin. Fail at SDK
    # construction, before an unusable runtime reaches the release workflow.
    for pattern in ("lib/libSDL3.so.*", "lib/libopenal.so.*", "lib/libdxvk_d3d9.so*",
                    "lib/libQt6Widgets.so.*", "lib/libQt6DBus.so.*",
                    "lib/qt6/plugins/platforms/libqxcb.so",
                    "lib/qt6/plugins/platforms/libqwayland*.so"):
        if not list(PREFIX.glob(pattern)):
            raise ValueError(f"Required SDK output is missing: {pattern}")
    run("rustc", "--version")
    if not run("rustc", "--version", capture=True).startswith("rustc 1.90.0 "):
        raise ValueError("Unexpected Rust compiler")
    catalog = {
        "schema": 1, "revision": args.revision, "toolchain": lock["toolchain"],
        "dependencies": dependencies, "files": owners,
        "system_packages": system_packages, "runtime_note": lock["runtime_note"],
    }
    (PREFIX / "share/openbf2/sdk-provenance.json").write_text(json.dumps(catalog, indent=2) + "\n")
    shutil.copyfile(lock_path, PREFIX / "share/openbf2/sdk-sources.json")
    run("ldconfig")
    # Preserve pristine effect-compiler sources and all license/provenance data.
    # Other third-party source trees are unnecessary once the SDK is built.
    shutil.rmtree(BUILD)
    for path in SOURCES.iterdir():
        if path.name not in {"vkd3d-2.1", "mojoshader", "meson"}:
            shutil.rmtree(path)


if __name__ == "__main__":
    main()
