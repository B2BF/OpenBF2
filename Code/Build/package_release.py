#!/usr/bin/env python3
"""Package rebuilt images using explicit runtime manifests, without game content."""

import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import stat
import zipfile


# Keep these manifests aligned with the image targets in CMakeLists.txt.
PACKAGES = {
    # [CHOSEN, not recovered] Windows ships the client for x64 only. The x86 build keeps the
    # dedicated server and the editor (which links the x86 client objects); an x64 editor package
    # is a future goal.
    "windows-x86": {
        "server": ("bin", (
            "bf2_w32ded.exe", "dice_py.dll", "AIDLL_w32ded.dll", "BF2VoipServer_w32ded.dll",
        )),
        "editor": ("bin_editor", (
            "BF2Editor.exe", "dice_py.dll", "RendDX9.dll", "TextureAtlasBuilder.dll", "BF2VoipServer.dll",
        )),
    },
    "windows-x64": {
        # [CHOSEN, not recovered] Native AI in both hosts (kr4uzi/OpenBF2 issue #11).
        "server": ("bin_w64ded", ("bf2_w64ded.exe", "dice_py.dll", "aidll.dll")),
        # BF2OpenAL.dll: OpenAL Soft, built by Code/Build/build_openal_soft.py ([CHOSEN, not recovered]).
        "client": ("bin_client", (
            "BF2.exe", "dice_py.dll", "RendDX9.dll", "SwiffPlayer.dll",
            "BF2Audio.dll", "TextureAtlasBuilder.dll", "aidll.dll", "BF2OpenAL.dll",
        )),
    },
    "linux-x64": {
        "server": ("bin_linux", ("bf2", "libdice_py.so")),
    },
    # [CHOSEN, not recovered] The macOS port: one package set per architecture. The client hosts
    # local games, so it carries the AI library as the server does.
    "macos-arm64": {
        "client": ("bin_macos", ("bf2_client", "libdice_py.dylib", "libaidll.dylib")),
        "server": ("bin_macos", ("bf2", "libdice_py.dylib", "libaidll.dylib")),
    },
    "macos-x86_64": {
        "client": ("bin_macos", ("bf2_client", "libdice_py.dylib", "libaidll.dylib")),
        "server": ("bin_macos", ("bf2", "libdice_py.dylib", "libaidll.dylib")),
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


def openal_notice(build_info):
    """[CHOSEN, not recovered] The LGPL notice for the x64 client's bundled BF2OpenAL.dll."""
    return (
        "BF2OpenAL.dll in this package is OpenAL Soft (https://openal-soft.org), unmodified,\n"
        "licensed under the GNU Library General Public License version 2 or later; see\n"
        "licenses/OpenAL-Soft-COPYING.txt. The exact source tarball it was built from is\n"
        "attached to the same release (openal-soft-<version>.tar.bz2) and can be downloaded\n"
        "from the source URL below.\n\n"
        + build_info +
        "\nReplacing it: BF2.exe loads BF2OpenAL.dll from its own folder first. Put any other\n"
        "64-bit OpenAL Soft build (or OpenAL implementation with the standard exports) there\n"
        "under that name. Alternatively choose \"System OpenAL\" in the audio settings to use\n"
        "an installed 64-bit OpenAL32.dll.\n"
    )


def installation(platform, role, name, executable, macos_signing="adhoc"):
    """[CHOSEN, not recovered] Owner-requested install policy."""
    server = role.startswith("server")
    options = "+loadMod bf2" if role.startswith("editor") else "+modPath mods/bf2"
    if role.startswith("client"):
        options += " +fullscreen 0"
    if platform.startswith("windows-"):
        text = (
            "Install (Windows, Command Prompt)\n"
            "1. Use your own Battlefield 2 1.50 {content} installation. Extract this\n"
            "   package to C:\\OpenBF2\\{name}; keep all its DLLs beside {exe}.\n"
            "   Keep client, server and editor packages in separate folders.\n"
            "2. Set the working directory to the installation root containing mods\\,\n"
            "   python\\ and pylib-2.3.4.zip (not to mods\\bf2):\n"
            "   cd /d \"C:\\Games\\{root}\"\n"
        ).format(content="dedicated-server" if server else "retail game",
                 name=name, exe=executable, root="Battlefield 2 Server" if server else "Battlefield 2")
        command = '"C:\\OpenBF2\\{}\\{}" {}'.format(name, executable, options)
        if role.startswith("client"):
            text += ("   This x64 client does not use the game folder's 32-bit DLLs. It needs the\n"
                     "   64-bit d3dx9_25.dll that the DirectX End-User Runtime installs in System32.\n"
                     "   Sound uses the bundled 64-bit BF2OpenAL.dll (OpenAL Soft; see\n"
                     "   OpenAL-Soft-NOTICE.txt), so the default audio providers work.\n"
                     "   Checked against the code; not runtime-tested here.\n")
    else:
        macos = platform.startswith("macos-")
        text = (
            "Install ({system}, Terminal)\n"
            "1. Extract this package to ~/Games/{name}.\n"
            "2. From your own {content} installation, copy mods/, python/ and\n"
            "   pylib-2.3.4.zip into that folder, beside {exe}. Also copy Settings/\n"
            "   and Profiles/ if present, and admin/ if you use its server scripts.\n"
            "   Keep the complete mods/ tree, including its archives and movies.\n"
            "{case_note}"
            "3. In Terminal (keep the bundled libraries beside the executable):\n"
            "   cd \"$HOME/Games/{name}\"\n"
        ).format(system="macOS 11 or later; arm64 for Apple silicon, x86_64 for Intel" if macos else "Linux",
                 content="Windows/Wine Battlefield 2 1.50" if macos else "Battlefield 2 1.50 server",
                 name=name, exe=executable,
                 case_note=("   Use a case-insensitive volume for Windows/Wine content; the engine\n"
                            "   lowercases game paths. Do not rename individual files or folders.\n")
                 if macos else "   Use Linux server content with lowercase game paths.\n")
        if macos and macos_signing == "adhoc":
            text += "   xattr -dr com.apple.quarantine .\n"
        text += "   chmod +x {}\n".format(executable)
        command = "./{} {}".format(executable, options)
        if macos and not server:
            text += ("   New user profiles live in ~/Library/Application Support/OpenBF2/Profiles/,\n"
                     "   independently of the copied installation's Profiles/ folder.\n"
                     "   Before joining, create ~/Library/Application Support/OpenBF2/Registry.con:\n"
                     "   HKEY_LOCAL_MACHINE\\SOFTWARE\\Electronic Arts\\EA Games\\Battlefield 2\\ergc = YOUR-CD-KEY\n"
                     "   Use your own unmarked key, single backslashes, no quotes; keep the file private.\n")
    if server:
        text += (
            "   Use the server's stock mods/bf2/Settings/ServerSettings.con and\n"
            "   mods/bf2/Settings/maplist.con from this working directory.\n"
            "   Optional overrides: +config path/to/ServerSettings.con and\n"
            "   +mapList path/to/maplist.con, with paths relative to the content root.\n"
        )
    text += (
        "{step}. Start with:\n   {command}\n"
        "{help_note}"
        "   Replace the example paths with your own.\n"
    ).format(step=3 if platform.startswith("windows-") else 4, command=command,
             help_note="   add +help 0 to hide the tutorial messages\n" if role.startswith("client") else "")
    if platform.startswith("windows-"):
        text += ("   A shortcut can use the same command as Target and the installation\n"
                 "   root as Start in. No original EXE or DLL is replaced; to go back,\n"
                 "   launch the original executable/shortcut.\n")
        if not role.startswith("editor"):
            text += ("   +modPath selects a mod, not the entire installation; keep the\n"
                     "   working directory above.\n")
    return text + "\n"


def package(args):
    """Validate all inputs before writing any of this platform's ZIP files."""
    macos_signing = getattr(args, "macos_signing", "adhoc")
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
    companions = []
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
        if role.startswith("client") and args.platform.startswith("macos-"):
            # Statically linked into the macOS client: the effect compiler (vkd3d-shader,
            # MojoShader, from the native dependency build), the mesh stripifier and the Bink decoder.
            deps = args.native_sources
            files["licenses/vkd3d-COPYING.txt"] = deps / "vkd3d/COPYING"
            files["licenses/vkd3d-LICENSE.txt"] = deps / "vkd3d/LICENSE"
            files["licenses/MojoShader-LICENSE.txt"] = deps / "mojoshader/LICENSE.txt"
            files["licenses/meshoptimizer-LICENSE.md"] = external / "meshoptimizer/LICENSE.md"
            files["licenses/FFmpeg-COPYING.LGPLv2.1.txt"] = external / "ffmpeg-bink/COPYING.LGPLv2.1"
        if role.startswith("client") and args.platform == "windows-x64":
            # [CHOSEN, not recovered] The bundled OpenAL Soft (LGPL): its licence, a notice and the
            # exact source tarball (written beside the ZIP; release.yml attaches it).
            openal = args.build_dir / "openal-soft"
            files["licenses/OpenAL-Soft-COPYING.txt"] = openal / "COPYING"
            build_info = openal / "BUILD.txt"
            if not build_info.is_file():
                raise ValueError("Required package input is missing or empty: {} (OpenAL Soft)".format(build_info))
            package_notices["OpenAL-Soft-NOTICE.txt"] = openal_notice(build_info.read_text(encoding="utf-8"))
            sources = sorted(openal.glob("openal-soft-*.tar.bz2"))
            if len(sources) != 1:
                raise ValueError("Expected one OpenAL Soft source tarball in {}".format(openal))
            pinned = re.search(r"^Source SHA-256: ([0-9a-f]{64})$", build_info.read_text(encoding="utf-8"), re.M)
            if not pinned or pinned.group(1) != sha256(sources[0]):
                raise ValueError("{} does not match the SHA-256 in {}".format(sources[0], build_info))
            companions.append(sources[0])
            # renddx9.cmd x64 statically links the FFmpeg-based Bink replacement.
            files["licenses/FFmpeg-COPYING.LGPLv2.1.txt"] = external / "ffmpeg-bink/COPYING.LGPLv2.1"
        if role.startswith("editor"):
            files["licenses/ImGui-LICENSE.txt"] = external / "imgui/LICENSE.txt"
            files["licenses/ImGuizmo-LICENSE.txt"] = external / "ImGuizmo/LICENSE"
        for name, path in files.items():
            if not path.is_file() or path.stat().st_size == 0:
                raise ValueError("Required package input is missing or empty: {} ({})".format(path, name))
        plans.append((role, names, files, package_notices))

    args.output_dir.mkdir(parents=True, exist_ok=True)
    for source in companions:
        target = args.output_dir / source.name
        shutil.copyfile(source, target)
        target.with_name(target.name + ".sha256").write_bytes(
            "{}  {}\n".format(sha256(target), target.name).encode("ascii"))
        print(target)
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
            **({"macos_signing": macos_signing} if args.platform.startswith("macos-") else {}),
            "runtime_files": {entry: sha256(files[entry]) for entry in names},
        }
        if args.platform.startswith("windows-"):
            requirements = "Windows requires the matching x86/x64 Microsoft Visual C++ runtime.\n"
            if role.startswith("editor"):
                requirements += ("Editor dependencies such as Bink, d3dx9_25 and an\n"
                                 "OpenAL provider must be supplied separately where required.\n")
            elif role.startswith("client"):
                requirements += "The client's own dependencies are listed in the install steps below.\n"
            requirements += "These Windows binaries are not code-signed.\n"
        elif args.platform.startswith("macos-"):
            requirements = ("macOS binaries are signed with a Developer ID (hardened runtime) and\n"
                            "this archive is notarized.\n" if macos_signing == "developer-id"
                            else "macOS binaries are ad-hoc signed, not notarized.\n")
        else:
            requirements = (
                "Linux is built on Ubuntu 22.04 with dynamic glibc/ncurses dependencies.\n"
                "Keep libdice_py.so beside bf2.\n"
            )
        readme = (
            "OpenBF2: {name}\n\n"
            "Source revision: {revision}\nSource and build instructions: {source}\n\n"
            "This archive contains rebuilt runtime binaries. It does not contain a\n"
            "Battlefield 2 installation. Supply your own compatible 1.50 game content,\n"
            "configuration and Python game scripts.\n"
            "Do not mix rebuilt client/editor engine plugins with retail engine plugins.\n\n"
            "{requirements}\n"
            "Build and fixture success do not establish loading, joining or gameplay.\n"
            "Automated server tests must explicitly use an unused port other than 16567.\n"
        ).format(name=name, revision=args.revision, source=source_url, requirements=requirements)
        readme += "\n" + installation(args.platform, role, name, names[0], macos_signing)
        if args.platform == "windows-x86":
            readme += (
                "\nThe executable links strictly. The bundled voice-server DLL retains\n"
                "its independent diagnostic /FORCE link and may contain unresolved calls.\n"
                "A strict executable link does not establish side-DLL completeness.\n"
            )
            if role == "server":
                readme += "The bundled AI library also retains its diagnostic /FORCE link.\n"
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
                executable = args.platform == "linux-x64" or args.platform.startswith("macos-")
                mode = 0o755 if executable and entry in names else 0o644
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
    parser.add_argument("--native-sources", type=Path, default=None,
                        help="macOS: the vkd3d and MojoShader source checkouts (default "
                             "<source-dir>/out/third_party), for the client's licence files")
    parser.add_argument("--revision", required=True, help="Full source commit SHA")
    parser.add_argument("--repository", required=True, help="GitHub owner/repository")
    parser.add_argument("--omit", action="append", default=[], metavar="ROLE",
                        help="Skip one package role of this platform (repeatable); every other "
                             "role stays required")
    parser.add_argument("--macos-signing", choices=("adhoc", "developer-id"), default="adhoc",
                        help="macOS: how the binaries in --build-dir were signed (release CI signs "
                             "them with a Developer ID before packaging); changes only the README "
                             "and build-info.json")
    args = parser.parse_args()
    if args.native_sources is None:
        args.native_sources = args.source_dir / "out/third_party"
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
