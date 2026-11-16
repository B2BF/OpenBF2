#!/usr/bin/env python3
"""Stage the native client and Qt runtime for openbf2-setup without executing their ELFs.

[CHOSEN, not recovered] https://github.com/kr4uzi/OpenBF2/issues/36#issuecomment-6932201666
The pinned SDK catalog supplies source/package identities; the setup checkout supplies
the authoritative artifact contract and ELF auditor. Only new staging copies are modified.
"""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys


def require(condition, message):
    if not condition:
        raise ValueError(message)


def command(*args):
    return subprocess.check_output(args, text=True, env={**os.environ, "LC_ALL": "C"})


def digest(path):
    value = hashlib.sha256()
    with Path(path).open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            value.update(block)
    return value.hexdigest()


def write_json(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2, sort_keys=True) + "\n")


def safe_relative(value):
    path = Path(value)
    require(value and not path.is_absolute() and ".." not in path.parts
            and not any(c in value for c in "\0\r\n\t"), f"Invalid relative artifact path: {value!r}")
    return path


def load_auditor(path):
    spec = importlib.util.spec_from_file_location("openbf2_setup_package", path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class Catalog:
    """Map each copied library to the exact SDK source or installed Debian package."""

    def __init__(self, path, prefix):
        self.prefix = prefix.resolve()
        data = json.loads(path.read_text())
        require(data.get("schema") == 1, "Unsupported SDK provenance schema")
        self.dependencies = {}
        for record in data.get("dependencies", []):
            name = record.get("name", "")
            require(name and name not in self.dependencies, f"Duplicate/empty SDK dependency: {name}")
            self.dependencies[name] = record
        self.files = data.get("files", {})
        for name, dependency in self.files.items():
            safe_relative(name)
            require(dependency in self.dependencies, f"Unknown SDK file owner: {name}: {dependency}")
        self.packages = data.get("system_packages", {})

    def record_for(self, path):
        path = path.resolve(strict=True)
        if path.is_relative_to(self.prefix):
            relative = path.relative_to(self.prefix).as_posix()
            require(relative in self.files, f"SDK file has no provenance: {relative}")
            return self.dependencies[self.files[relative]]
        # Query installed ownership only. Package URLs/hashes were pinned when building
        # the immutable SDK; staging neither downloads nor trusts changing apt indexes.
        variants = [path]
        if str(path).startswith("/usr/lib/"):
            variants.append(Path(str(path)[4:]))
        owners = set()
        for candidate in variants:
            result = subprocess.run(["dpkg-query", "-S", str(candidate)], text=True,
                                    stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                    env={**os.environ, "LC_ALL": "C"})
            if result.returncode == 0:
                for line in result.stdout.splitlines():
                    if ": " in line:
                        owner, recorded_path = line.split(": ", 1)
                        if recorded_path == str(candidate):
                            owners.update(owner.split(", "))
        require(len(owners) == 1, f"Expected one Debian package owner for {path}: {sorted(owners)}")
        owner = owners.pop()
        identity = command("dpkg-query", "-W", "-f=${binary:Package}=${Version}", owner).strip()
        require(identity in self.packages, f"Installed package absent from SDK provenance: {identity}")
        return self.packages[identity]

    def license_file(self, record):
        value = record.get("license", "")
        path = Path(value)
        if not path.is_absolute():
            path = self.prefix / safe_relative(value)
        require(path.is_file(), f"Missing dependency license: {record.get('name')}: {path}")
        return path.resolve()


class Resolver:
    def __init__(self, directories, host):
        self.directories = list(dict.fromkeys(Path(path).resolve() for path in directories))
        self.host = host

    def find(self, soname):
        require(re.fullmatch(r"[A-Za-z0-9_.+-]+", soname), f"Invalid dependency SONAME: {soname}")
        require(soname not in self.host, f"Host library must not be bundled: {soname}")
        # Directory order deliberately prefers the SDK over its baseline OS. A
        # different file in a later directory is never silently merged into a role.
        for directory in self.directories:
            candidate = directory / soname
            if candidate.is_file():
                return candidate.resolve(strict=True)
        raise ValueError(f"Missing runtime library: {soname}; searched {self.directories}")


class Artifact:
    def __init__(self, root, kind, catalog, resolver, auditor, revision, sdk, toolchain, roots):
        self.root, self.kind = root, kind
        self.catalog, self.resolver, self.auditor = catalog, resolver, auditor
        self.revision, self.sdk, self.toolchain, self.roots = revision, sdk, toolchain, roots
        self.inputs = {}
        self.dependencies = {}
        self.libraries = {}

    def copy_data(self, source, relative):
        relative = safe_relative(relative)
        target = self.root / relative
        if target.exists():
            require(target.is_file() and digest(source) == digest(target), f"Conflicting artifact file: {relative}")
            return target
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source, target)
        target.chmod(0o644)
        return target

    def add_record(self, record):
        for field in ("name", "version", "source", "sha256", "license"):
            require(isinstance(record.get(field), str) and record[field], f"Incomplete dependency provenance: {field}")
        require(re.fullmatch(r"[0-9a-f]{64}", record["sha256"]), f"Invalid source/package checksum: {record['name']}")
        name = record["name"]
        if name in self.dependencies:
            require(self.dependencies[name]["source"] == record["source"]
                    and self.dependencies[name]["sha256"] == record["sha256"]
                    and self.dependencies[name]["version"] == record["version"], f"Conflicting provenance: {name}")
            return
        slug = re.sub(r"[^A-Za-z0-9_.+-]", "_", name)
        license_name = f"share/licenses/{slug}-{record['sha256'][:12]}/COPYING"
        license_file = self.catalog.license_file(record)
        self.copy_data(license_file, license_name)
        # Debian copyright files often refer to the distribution's shared full
        # license texts. Preserve those texts too, rather than shipping dangling
        # references in an otherwise self-contained notices directory.
        for common in sorted(set(re.findall(r"/usr/share/common-licenses/([A-Za-z0-9_.+-]+)",
                                           license_file.read_text(errors="replace")))):
            self.copy_data(Path("/usr/share/common-licenses") / common, "share/licenses/common/" + common)
        public = {key: value for key, value in record.items() if key != "license"}
        public["license"] = license_name
        self.dependencies[name] = public
        for dependency in record.get("requires", []):
            require(dependency in self.catalog.dependencies, f"Missing static dependency provenance: {dependency}")
            self.add_record(self.catalog.dependencies[dependency])

    def add_elf(self, source, relative, provenance=True):
        source = source.resolve(strict=True)
        relative = safe_relative(relative).as_posix()
        require(source.is_file(), f"Not a regular ELF source: {source}")
        if relative in self.inputs:
            require(self.inputs[relative] == source, f"Conflicting dependency source: {relative}")
            return
        self.inputs[relative] = source
        if provenance:
            self.add_record(self.catalog.record_for(source))
        info = self.auditor.elf(source)
        require(info is not None, f"Expected ELF runtime: {source}")
        target = self.copy_data(source, relative)
        target.chmod(0o755)
        # Strip only the new staging copy, then replace build RPATHs. The final
        # audit still rejects source/build strings anywhere else in the ELF.
        subprocess.run(["strip", "--strip-unneeded", str(target)], check=True)
        to_lib = os.path.relpath(self.root / "lib", target.parent)
        rpath = "$ORIGIN" if to_lib == "." else "$ORIGIN:$ORIGIN/" + to_lib
        subprocess.run(["patchelf", "--set-rpath", rpath, str(target)], check=True)
        self.auditor.elf(target, roots=self.roots)
        for dependency in info["needed"]:
            if dependency not in self.resolver.host:
                self.add_library(dependency)

    def add_library(self, soname):
        if soname in self.libraries:
            return
        source = self.resolver.find(soname)
        self.libraries[soname] = source
        self.add_elf(source, "lib/" + soname)

    def finish(self):
        metadata = self.root / "share/openbf2" / (self.kind + "-artifact.json")
        inventory = {path.relative_to(self.root).as_posix(): digest(path)
                     for path in self.auditor.files(self.root)}
        write_json(metadata, {"schema": 1, "architecture": "x86_64", "glibc": "2.35",
                              "revision": self.revision, "sdk": self.sdk, "toolchain": self.toolchain,
                              "dependencies": sorted(self.dependencies.values(), key=lambda item: item["name"]),
                              "files": inventory})


def engine_notices(artifact, source):
    """Keep the engine's existing package notices, including statically linked code."""
    spec = importlib.util.spec_from_file_location("openbf2_existing_packager", source / "Code/Build/package_release.py")
    package = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(package)
    external = source / "Code/BF2/External"
    files = {"OpenBF2-LICENSE": source / "LICENSE",
             "Python-LICENSE": external / "python-embedded/py-src-2.3.4/LICENSE",
             "Ogg-COPYING": external / "libogg-1.3.6/COPYING",
             "Vorbis-COPYING": external / "libvorbis-1.3.7/COPYING",
             "Speex-COPYING": external / "speex-1.0.5/COPYING",
             "Expat-COPYING": external / "expat/COPYING",
             "meshoptimizer-LICENSE": external / "meshoptimizer/LICENSE.md",
             "FFmpeg-COPYING.LGPLv2.1": external / "ffmpeg-bink/COPYING.LGPLv2.1",
             "FFmpeg-LICENSE": external / "ffmpeg-bink/LICENSE.md"}
    for name, path in files.items():
        artifact.copy_data(path, "share/licenses/engine/" + name)
    for name, filename in (("zlib", "zlib-1.2.3/zlib.h"), ("Minizip", "zlib-1.2.3/contrib/minizip/unzip.h"),
                           ("TinyXML", "TinyXML/tinyxml.h"), ("RSA-MD5", "GameSpy/md5c.c"),
                           ("GameSpy", "GameSpy/gcdkey/gcdkeys.h"), ("gameswf", "gameswf/gameswf.h"),
                           ("gameswf-base", "base/tu_random.cpp")):
        (artifact.root / "share/licenses/engine" / (name + ".txt")).write_text(package.leading_notice(external / filename))


def audit_closure(root, auditor, roots=()):
    """Check both artifacts independently, including the Qt-only runtime tree."""
    binaries = {path: info for path in auditor.files(root)
                if (info := auditor.elf(path, roots=roots)) is not None}
    for path, info in binaries.items():
        require(path.name not in auditor.HOST, f"Host library bundled: {path}")
        require(info["rpaths"], f"Missing relative runtime path: {path}")
        search = []
        for entry in info["rpaths"]:
            require(entry == "$ORIGIN" or entry.startswith("$ORIGIN/"), f"Nonrelative runtime path: {path}: {entry}")
            directory = Path(entry.replace("$ORIGIN", str(path.parent))).resolve()
            require(directory.is_relative_to(root.resolve()), f"Runtime path escapes artifact: {path}")
            search.append(directory)
        for name in info["needed"]:
            if name in auditor.HOST:
                continue
            dependency = next((directory / name for directory in search if directory / name in binaries), None)
            require(dependency is not None, f"Unresolved artifact dependency: {path} -> {name}")
            blocks = re.split(r"File: ", info["versions"])[1:]
            block = next((item for item in blocks if item.split()[0] == name), "")
            definitions = command("readelf", "-VW", str(dependency)).split("Version needs section", 1)[0]
            exported = set(re.findall(r"Name: (\S+)", definitions))
            for version in re.findall(r"Name: (\S+)", block):
                require(version in exported, f"Missing symbol version {version}: {path} -> {name}")


def verify_engine_build(engine_bin, revision, toolchain):
    record = json.loads((engine_bin / "bf2_client.build.json").read_text())
    require(record.get("commit") == revision, "Client build revision differs from requested release")
    require(record.get("architecture") == "x86_64", "Client build architecture is not x86_64")
    require(record.get("compiler") == "GNU " + toolchain["gcc"], "Client was not built with the pinned GCC")
    require(record.get("worktree") == "" and record.get("untracked_sha256") == {}
            and record.get("tracked_diff_sha256") == hashlib.sha256(b"").hexdigest()
            and not record.get("source_hash_error"), "Client build source state is dirty or unknown")
    require(record.get("binary_sha256") == digest(engine_bin / "bf2_client"), "Client build hash differs from binary")
    # Do not publish the development record's absolute binary/source paths.
    return {key: record[key] for key in ("commit", "architecture", "compiler", "binary_sha256", "tracked_diff_sha256")}


def stage(args):
    output, engine_bin, prefix = (Path(value).resolve() for value in (args.output, args.engine_bin, args.sdk_prefix))
    require(not output.exists(), f"Output already exists; choose a fresh directory: {output}")
    require(not any(output.is_relative_to(path) or path.is_relative_to(output)
                    for path in (engine_bin, prefix)), "Output overlaps an input tree")
    require(re.fullmatch(r"[0-9a-f]{40}", args.revision), "Expected a full engine commit SHA")
    require(re.fullmatch(r"[^\s]+@sha256:[0-9a-f]{64}", args.sdk), "Expected an immutable SDK image identity")
    auditor = load_auditor(args.setup_package)
    roots = auditor.build_roots(args.build_root, args.build_roots_file)
    auditor.external_roots_file(args.build_roots_file, [output])
    toolchain = json.loads(args.toolchain.read_text())
    auditor.toolchain_metadata({"toolchain": toolchain, "sdk": args.sdk})
    build_record = verify_engine_build(engine_bin, args.revision, toolchain)
    catalog = Catalog(args.catalog, prefix)
    directories = [prefix / "lib", prefix / "lib64"] + args.library_dir + [
        Path("/usr/lib/x86_64-linux-gnu"), Path("/lib/x86_64-linux-gnu"),
        Path("/usr/lib/x86_64-linux-gnu/pulseaudio"), Path("/usr/lib"), Path("/lib")]
    resolver = Resolver(directories, auditor.HOST)
    output.mkdir(parents=True)
    artifacts = {kind: Artifact(output / kind, kind, catalog, resolver, auditor, args.revision,
                                args.sdk, toolchain, roots) for kind in ("engine", "runtime")}
    engine, runtime = artifacts["engine"], artifacts["runtime"]
    # Seed plugin identities before traversing dependencies so their source remains
    # the just-built client, never a system or retail library with the same name.
    for name in ("libdice_py.so", "libaidll.so"):
        engine.libraries[name] = engine_bin / name
        engine.add_elf(engine_bin / name, "lib/" + name, provenance=False)
    engine.add_elf(engine_bin / "bf2_client", "bin/bf2_client", provenance=False)
    for name in ("libdxvk_d3d9.so.0", "libSDL3.so.0", "libopenal.so.1"):
        engine.add_library(name)
    for name in ("vkd3d", "MojoShader"):
        require(name in catalog.dependencies, f"Missing native compiler provenance: {name}")
        engine.add_record(catalog.dependencies[name])
    engine_notices(engine, args.engine_source.resolve())
    write_json(engine.root / "share/openbf2/engine-build.json", build_record)
    for name in ("libQt6Widgets.so.6", "libQt6Gui.so.6", "libQt6Core.so.6", "libQt6DBus.so.6", "libSDL3.so.0"):
        runtime.add_library(name)
    plugin_root = prefix / "lib/qt6/plugins"
    plugin_directories = ("platforms", "platformthemes", "platforminputcontexts", "wayland-shell-integration",
                          "wayland-graphics-integration-client", "wayland-decoration-client", "xcbglintegrations",
                          "imageformats", "iconengines", "styles")
    for directory in plugin_directories:
        for plugin in sorted((plugin_root / directory).glob("*.so")):
            runtime.add_elf(plugin, plugin.relative_to(prefix).as_posix())
    for name in auditor.PLUGINS:
        require((runtime.root / name).is_file(), f"Missing Qt plugin: {name}")
    require(any((runtime.root / "lib/qt6/plugins/platforms" / name).is_file()
                for name in ("libqwayland.so", "libqwayland-generic.so")), "Missing Qt Wayland platform plugin")
    for artifact in artifacts.values():
        for name in ("libpulse.so.0", "libasound.so.2", "libudev.so.1", "libdbus-1.so.3") + tuple(args.dynamic_library):
            artifact.add_library(name)
        audit_closure(artifact.root, auditor, roots)
        artifact.finish()
    auditor.check_inputs(engine.root, runtime.root, roots)
    print(f"Audited engine artifact: {engine.root}\nAudited Qt runtime artifact: {runtime.root}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("engine-bin", "sdk-prefix", "catalog", "toolchain", "setup-package", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--engine-source", type=Path, default=Path(__file__).resolve().parents[3])
    parser.add_argument("--revision", required=True)
    parser.add_argument("--sdk", required=True)
    parser.add_argument("--library-dir", type=Path, action="append", default=[])
    parser.add_argument("--dynamic-library", action="append", default=[])
    parser.add_argument("--build-root", action="append", default=[])
    parser.add_argument("--build-roots-file", default="")
    stage(parser.parse_args())


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        sys.exit(f"linux-artifact-stage: {error}")
