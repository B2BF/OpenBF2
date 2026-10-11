#!/usr/bin/env python3
"""Package rebuilt images using explicit runtime manifests, without game content."""

import argparse
import hashlib
import json
from pathlib import Path
import re
import stat
import zipfile


# Keep these manifests aligned with the image targets in CMakeLists.txt.
PACKAGES = {
    "windows-x86": {
        "server-diagnostic": ("bin", (
            "bf2_w32ded.exe", "dice_py.dll", "AIDLL_w32ded.dll",
        )),
        "client-diagnostic": ("bin_client", (
            "BF2.exe", "dice_py.dll", "RendDX9.dll", "SwiffPlayer.dll",
            "BF2Audio.dll", "TextureAtlasBuilder.dll",
        )),
        "editor-diagnostic": ("bin_editor", (
            "BF2Editor.exe", "dice_py.dll", "RendDX9.dll", "TextureAtlasBuilder.dll",
        )),
    },
    "windows-x64": {
        "server": ("bin_w64ded", ("bf2_w64ded.exe", "dice_py.dll")),
    },
    "linux-x64": {
        "server": ("bin_linux", ("bf2", "libdice_py.so")),
    },
}


def sha256(path):
    """Hash a file without loading an executable-sized buffer into memory."""
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def leading_notice(path):
    """Preserve notices from dependencies that keep them in source comments."""
    content = path.read_text(encoding="utf-8-sig")
    match = re.match(r"(?:\s|/\*.*?\*/|//[^\n]*(?:\n|$))+", content, re.DOTALL)
    if not match or not match.group().strip():
        raise ValueError("Missing leading dependency notice: {}".format(path))
    return match.group().rstrip() + "\n"


def package(args):
    """Validate all inputs before writing any of this platform's ZIP files."""
    source = args.source_dir.resolve()
    external = source / "Code/BF2/External"
    common = {
        "LICENSE": source / "LICENSE",
        "licenses/Python-LICENSE.txt": external / "python-embedded/py-src-2.3.4/LICENSE",
    }
    notices = {
        "licenses/zlib.txt": leading_notice(external / "zlib-1.2.3/zlib.h"),
        "licenses/Minizip.txt": leading_notice(external / "zlib-1.2.3/contrib/minizip/unzip.h"),
        "licenses/TinyXML.txt": leading_notice(external / "TinyXML/tinyxml.h"),
        "licenses/RSA-MD5.txt": leading_notice(external / "GameSpy/md5c.c"),
        "licenses/GameSpy.txt": leading_notice(external / "GameSpy/gcdkey/gcdkeys.h"),
    }
    roles = PACKAGES[args.platform]
    unknown = sorted(set(args.omit) - set(roles))
    if unknown:
        raise ValueError("Cannot omit {}: {} packages only {}".format(
            ", ".join(unknown), args.platform, ", ".join(roles)))
    plans = []
    for role, (directory, names) in roles.items():
        if role in args.omit:
            print("Omitted by request: openbf2-{}-{}".format(args.platform, role))
            continue
        files = {name: args.build_dir / directory / name for name in names}
        files.update(common)
        package_notices = dict(notices)
        if role.startswith("client"):
            files["licenses/Ogg-COPYING.txt"] = external / "libogg-1.3.6/COPYING"
            files["licenses/Vorbis-COPYING.txt"] = external / "libvorbis-1.3.7/COPYING"
            package_notices["licenses/gameswf.txt"] = leading_notice(external / "gameswf/gameswf.h")
            package_notices["licenses/gameswf-base.txt"] = leading_notice(external / "base/tu_random.cpp")
        if role.startswith("editor"):
            files["licenses/ImGui-LICENSE.txt"] = external / "imgui/LICENSE.txt"
            files["licenses/ImGuizmo-LICENSE.txt"] = external / "ImGuizmo/LICENSE"
        for name, path in files.items():
            if not path.is_file() or path.stat().st_size == 0:
                raise ValueError("Required package input is missing or empty: {} ({})".format(path, name))
        plans.append((role, names, files, package_notices))

    args.output_dir.mkdir(parents=True, exist_ok=True)
    for role, names, files, package_notices in plans:
        name = "openbf2-{}-{}".format(args.platform, role)
        archive_path = args.output_dir / (name + ".zip")
        diagnostic = role.endswith("-diagnostic")
        source_url = "https://github.com/{}/tree/{}".format(args.repository, args.revision)
        metadata = {
            "package": name,
            "platform": args.platform,
            "revision": args.revision,
            "source": source_url,
            "link_contract": "diagnostic /FORCE" if diagnostic else "strict",
            "windows_signing": "not configured",
            "runtime_files": {entry: sha256(files[entry]) for entry in names},
        }
        readme = (
            "OpenBF2: {name}\n\n"
            "Source revision: {revision}\nSource and build instructions: {source}\n\n"
            "This archive contains rebuilt runtime binaries. It does not contain a\n"
            "Battlefield 2 installation. Supply your own compatible 1.50 game content,\n"
            "configuration and Python game scripts. Keep the original game binaries\n"
            "separate and run these binaries with the game content as working directory.\n"
            "Do not mix rebuilt client/editor engine plugins with retail engine plugins.\n\n"
            "Windows requires the matching x86/x64 Microsoft Visual C++ runtime.\n"
            "Client/editor runtime dependencies such as Bink, NovintHFX, d3dx9_25 and\n"
            "an OpenAL provider must be supplied separately where required. Windows\n"
            "binaries are not code-signed by this pipeline.\n"
            "Linux is built on Ubuntu 22.04 with dynamic glibc/ncurses dependencies.\n"
            "Keep libdice_py.so beside bf2; use chmod +x bf2 if your extractor drops modes.\n\n"
            "Build and fixture success do not establish loading, joining or gameplay.\n"
            "Automated server tests must explicitly use an unused port other than 16567.\n"
        ).format(name=name, revision=args.revision, source=source_url)
        if diagnostic:
            readme += (
                "\nThis is a diagnostic /FORCE package. Its executable and any AI library\n"
                "may contain unresolved calls. A successful build is not a strict-link\n"
                "or playability guarantee; existing engine frontiers remain.\n"
            )
        with zipfile.ZipFile(archive_path, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
            for entry, path in sorted(files.items()):
                info = zipfile.ZipInfo.from_file(path, arcname=name + "/" + entry)
                # Record Unix modes even when fixtures/package tooling run on Windows.
                info.create_system = 3
                mode = 0o755 if args.platform == "linux-x64" and entry in names else 0o644
                info.external_attr = (stat.S_IFREG | mode) << 16
                info.compress_type = zipfile.ZIP_DEFLATED
                with path.open("rb") as src, archive.open(info, "w") as dst:
                    for block in iter(lambda: src.read(1024 * 1024), b""):
                        dst.write(block)
            archive.writestr(name + "/build-info.json", json.dumps(metadata, indent=2) + "\n")
            archive.writestr(name + "/README.txt", readme)
            for entry, notice in sorted(package_notices.items()):
                archive.writestr(name + "/" + entry, notice)
        checksum = archive_path.with_suffix(".zip.sha256")
        # Linux sha256sum reads CR from Windows CRLF as part of the filename.
        checksum.write_bytes("{}  {}\n".format(sha256(archive_path), archive_path.name).encode("ascii"))
        print(archive_path)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--platform", choices=PACKAGES, required=True)
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--source-dir", type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument("--revision", required=True, help="Full source commit SHA")
    parser.add_argument("--repository", required=True, help="GitHub owner/repository")
    parser.add_argument("--omit", action="append", default=[], metavar="ROLE",
                        help="Skip one package role of this platform (repeatable); every other "
                             "role stays required")
    args = parser.parse_args()
    if not re.fullmatch(r"[0-9a-fA-F]{40}", args.revision):
        parser.error("--revision must be a full 40-character commit SHA")
    if not re.fullmatch(r"[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+", args.repository):
        parser.error("--repository must be owner/repository")
    try:
        package(args)
    except (OSError, ValueError) as error:
        parser.exit(1, "Packaging failed: {}\n".format(error))


if __name__ == "__main__":
    main()
