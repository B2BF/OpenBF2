#!/usr/bin/env python3
"""Package rebuilt images using explicit runtime manifests, without game content.

--platform writes one platform's per-role ZIPs (CI artifacts). --release windows/linux assembles
the platform's single release ZIP from those; --release macos records the disk image's build-info."""

import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import stat
import time
import zipfile


# Keep these manifests aligned with the image targets in CMakeLists.txt.
PACKAGES = {
    # Windows ships both clients: the x86 one uses retail's 32-bit PunkBuster, D3DX and OpenAL DLLs
    # from the game folder; the x64 one has no PunkBuster. An x64 editor package is a future goal.
    "windows-x86": {
        "server": ("bin", (
            "bf2_w32ded.exe", "dice_py.dll", "AIDLL_w32ded.dll", "BF2VoipServer_w32ded.dll",
        )),
        # BF2OpenAL.dll: OpenAL Soft, built by Code/Build/build_openal_soft.py ([CHOSEN, not recovered]).
        "client": ("bin_client", (
            "BF2.exe", "dice_py.dll", "RendDX9.dll", "SwiffPlayer.dll",
            "BF2Audio.dll", "TextureAtlasBuilder.dll", "BF2VoipServer.dll", "BF2OpenAL.dll",
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


def openal_notice(build_info, bits):
    """[CHOSEN, not recovered] The LGPL notice for a Windows client's bundled BF2OpenAL.dll.

    It lies beside that DLL; the wording holds both in a per-role ZIP and in a client folder of
    the combined Windows release package."""
    return (
        "BF2OpenAL.dll beside this notice is OpenAL Soft (https://openal-soft.org), unmodified,\n"
        "licensed under the GNU Library General Public License version 2 or later; see\n"
        "OpenAL-Soft-COPYING.txt in the package's licenses folder. The exact source tarball\n"
        "it was built from is attached to the same release (openal-soft-<version>.tar.bz2)\n"
        "and can be downloaded from the source URL below.\n\n"
        + build_info +
        "\nReplacing it: BF2.exe loads BF2OpenAL.dll from its own folder first. Put any other\n"
        "{bits} OpenAL Soft build (or OpenAL implementation with the standard exports) there\n"
        "under that name. Alternatively choose \"System OpenAL\" in the audio settings to use\n"
        "an installed {bits} OpenAL32.dll.\n".format(bits=bits)
    )


def indent(text, prefix="   "):
    return "".join(prefix + line if line.strip() else line for line in text.splitlines(True))


def windows_client_notes(platform, notice="OpenAL-Soft-NOTICE.txt", folder="this package's folder"):
    """[CHOSEN, not recovered] A Windows client's dependencies, shared by the per-role README and
    the combined Windows README (which names the client's folder and notice path)."""
    if platform == "windows-x86":
        text = ("The original folder supplies binkw32.dll, d3dx9_25.dll and PunkBuster's\n"
                "pb\\ folder through this working directory where required. Sound uses\n"
                "the bundled BF2OpenAL.dll (OpenAL Soft; see {notice}), not\n"
                "the game folder's retail copy: BF2.exe runs from {folder},\n"
                "and Windows searches the application folder first.\n").format(notice=notice, folder=folder)
    else:
        text = ("This x64 client does not use the game folder's 32-bit DLLs. It needs the\n"
                "64-bit d3dx9_25.dll that the DirectX End-User Runtime installs in System32.\n"
                "Sound uses the bundled 64-bit BF2OpenAL.dll (OpenAL Soft; see\n"
                "{notice}), so the default audio providers work.\n"
                "No PunkBuster: PunkBuster exists only as 32-bit DLLs, so this x64\n"
                "client cannot join servers that require PunkBuster. Use the x86\n"
                "client for those.\n").format(notice=notice)
    return text + ("CD keys: the retail registry, or a per-user\n"
                   "%LOCALAPPDATA%\\OpenBF2\\Registry.dat (docs/registry-file.md).\n"
                   "Checked against the code; not runtime-tested here.\n")


SERVER_SETTINGS = (
    "Use the server's stock mods/bf2/Settings/ServerSettings.con and\n"
    "mods/bf2/Settings/maplist.con from this working directory.\n"
    "Optional overrides: +config path/to/ServerSettings.con and\n"
    "+mapList path/to/maplist.con, with paths relative to the content root.\n"
)

EDITOR_DEPENDENCIES = (
    "Editor dependencies such as Bink, d3dx9_25 and an\n"
    "OpenAL provider must be supplied separately where required.\n"
)

TEST_NOTE = (
    "Build and fixture success do not establish loading, joining or gameplay.\n"
    "Automated server tests must explicitly use an unused port other than 16567.\n"
)


def installation(platform, role, name, executable):
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
            text += indent(windows_client_notes(platform))
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
        if macos:
            # Per-architecture macOS ZIPs are CI artifacts and never notarized, even when signed
            # with the Developer ID; the release ships the notarized openbf2-macos.dmg instead.
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
        text += indent(SERVER_SETTINGS)
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


HEADER = (
    "Source revision: {revision}\nSource and build instructions: {source}\n\n"
    "This archive contains rebuilt runtime binaries. It does not contain a\n"
    "Battlefield 2 installation. Supply your own compatible 1.50 game content,\n"
    "configuration and Python game scripts.\n"
    "Do not mix rebuilt client/editor engine plugins with retail engine plugins.\n\n"
)


def role_readme(platform, role, name, executable, revision, source_url, macos_signing="adhoc", contents=""):
    """The README of one per-role package (also the Linux release package's, with its contents)."""
    diagnostic = role.endswith("-diagnostic")
    if platform.startswith("windows-"):
        requirements = "Windows requires the matching x86/x64 Microsoft Visual C++ runtime.\n"
        if role.startswith("editor"):
            requirements += EDITOR_DEPENDENCIES
        elif role.startswith("client"):
            requirements += "The client's own dependencies are listed in the install steps below.\n"
        requirements += "These Windows binaries are not code-signed.\n"
    elif platform.startswith("macos-"):
        requirements = ("macOS binaries are signed with a Developer ID (hardened runtime).\n"
                        if macos_signing == "developer-id" else "macOS binaries are ad-hoc signed.\n")
        requirements += ("This archive is a CI artifact and not notarized; releases ship the\n"
                         "notarized openbf2-macos.dmg instead.\n")
    else:
        requirements = (
            "Linux is built on Ubuntu 22.04 with dynamic glibc/ncurses dependencies.\n"
            "Keep libdice_py.so beside bf2.\n"
        )
    readme = ("OpenBF2: {name}\n\n" + HEADER).format(name=name, revision=revision, source=source_url)
    readme += contents + requirements + "\n" + TEST_NOTE
    readme += "\n" + installation(platform, role, name, executable)
    if platform == "windows-x86":
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
    return readme


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
        if role.startswith("client") and args.platform.startswith("windows-"):
            # [CHOSEN, not recovered] The bundled OpenAL Soft (LGPL), in both Windows clients: its
            # licence, a notice and the exact source tarball (written beside the ZIP; release.yml
            # attaches one copy).
            openal = args.build_dir / "openal-soft"
            files["licenses/OpenAL-Soft-COPYING.txt"] = openal / "COPYING"
            build_info = openal / "BUILD.txt"
            if not build_info.is_file():
                raise ValueError("Required package input is missing or empty: {} (OpenAL Soft)".format(build_info))
            package_notices["OpenAL-Soft-NOTICE.txt"] = openal_notice(
                build_info.read_text(encoding="utf-8"), "64-bit" if args.platform == "windows-x64" else "32-bit")
            sources = sorted(openal.glob("openal-soft-*.tar.bz2"))
            if len(sources) != 1:
                raise ValueError("Expected one OpenAL Soft source tarball in {}".format(openal))
            pinned = re.search(r"^Source SHA-256: ([0-9a-f]{64})$", build_info.read_text(encoding="utf-8"), re.M)
            if not pinned or pinned.group(1) != sha256(sources[0]):
                raise ValueError("{} does not match the SHA-256 in {}".format(sources[0], build_info))
            companions.append(sources[0])
        if role.startswith("client") and args.platform == "windows-x64":
            # renddx9.cmd x64 statically links the FFmpeg-based Bink replacement; x86 keeps binkw32.dll.
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
        readme = role_readme(args.platform, role, name, names[0], args.revision, source_url, macos_signing)
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
            archive.writestr(text_entry(name + "/build-info.json"), json.dumps(metadata, indent=2) + "\n")
            archive.writestr(text_entry(name + "/README.txt"), readme)
            for entry, notice in sorted(package_notices.items()):
                archive.writestr(text_entry(name + "/" + entry), notice)
        write_checksum(archive_path)
        print(archive_path)


def text_entry(arcname, mode=0o644, date_time=None):
    """A regular-file ZIP entry with Unix mode bits (writestr with a bare name records none)."""
    info = zipfile.ZipInfo(arcname, date_time=date_time or time.localtime()[:6])
    info.create_system = 3
    info.external_attr = (stat.S_IFREG | mode) << 16
    info.compress_type = zipfile.ZIP_DEFLATED
    return info


def write_checksum(path):
    """<file>.sha256 for sha256sum --check. LF only: Linux sha256sum reads CR from Windows CRLF
    as part of the filename."""
    path.with_name(path.name + ".sha256").write_bytes(
        "{}  {}\n".format(sha256(path), path.name).encode("ascii"))


# [CHOSEN, not recovered] One release package per platform, named by the platform only (owner
# decision 2026-11-15). The per-role ZIPs above stay CI artifacts; the Windows and Linux release
# packages are assembled from them, so a release file holds the same verified binaries as the CI
# artifacts. Not released: per-architecture Windows ZIPs, the x64 Windows server, the macOS ZIPs
# and the macOS server. macOS releases the setup app's disk image (build.yml, macos-app).
RELEASES = {
    "windows": (
        # (folder in the package, per-role platform, role, contents line)
        ("x64", "windows-x64", "client", "the 64-bit client (BF2.exe); no PunkBuster"),
        ("x86", "windows-x86", "client", "the 32-bit client (BF2.exe); PunkBuster-capable"),
        ("server", "windows-x86", "server", "the 32-bit dedicated server (bf2_w32ded.exe)"),
        ("editor", "windows-x86", "editor", "the 32-bit editor (BF2Editor.exe)"),
    ),
    "linux": (
        ("", "linux-x64", "server", "the x64 dedicated server (bf2)"),
    ),
}

# What each licence file covers, for the release READMEs. Files without an entry are listed by name.
LICENCES = {
    "LICENSE": "OpenBF2 itself",
    "licenses/Python-LICENSE.txt": "embedded Python 2.3.4 (dice_py)",
    "licenses/zlib.txt": "zlib",
    "licenses/Minizip.txt": "Minizip",
    "licenses/TinyXML.txt": "TinyXML",
    "licenses/RSA-MD5.txt": "RSA Data Security MD5",
    "licenses/GameSpy.txt": "GameSpy SDK",
    "licenses/Ogg-COPYING.txt": "libogg (clients)",
    "licenses/Vorbis-COPYING.txt": "libvorbis (clients)",
    "licenses/gameswf.txt": "gameswf (clients)",
    "licenses/gameswf-base.txt": "gameswf base library (clients)",
    "licenses/FFmpeg-COPYING.LGPLv2.1.txt": "FFmpeg Bink decoders in the x64 client's RendDX9.dll",
    "licenses/OpenAL-Soft-COPYING.txt": "OpenAL Soft, the clients' BF2OpenAL.dll (LGPL)",
    "licenses/ImGui-LICENSE.txt": "Dear ImGui (editor)",
    "licenses/ImGuizmo-LICENSE.txt": "ImGuizmo (editor)",
}


def find_input(directory, pattern):
    """Every file matching pattern below directory (CI downloads each artifact into its own
    folder); copies of one file must be identical."""
    matches = sorted(path for path in directory.rglob(pattern) if path.is_file())
    if len({(path.name, sha256(path)) for path in matches}) > 1:
        raise ValueError("Conflicting inputs for {} below {}: {}".format(
            pattern, directory, ", ".join(str(path) for path in matches)))
    return matches[0] if matches else None


def read_component(path, package_name, revision):
    """A per-role ZIP, checked against its .sha256 and its build-info.json."""
    checksum = path.with_name(path.name + ".sha256")
    expected = "{}  {}\n".format(sha256(path), path.name)
    if not checksum.is_file() or checksum.read_text(encoding="ascii") != expected:
        raise ValueError("{} does not match {}".format(path, checksum.name))
    prefix = package_name + "/"
    entries = {}
    with zipfile.ZipFile(path) as archive:
        for info in archive.infolist():
            if info.is_dir():
                continue
            if not info.filename.startswith(prefix):
                raise ValueError("{}: unexpected entry {}".format(path, info.filename))
            entries[info.filename[len(prefix):]] = (info, archive.read(info))
    if "build-info.json" not in entries:
        raise ValueError("{}: no build-info.json".format(path))
    metadata = json.loads(entries["build-info.json"][1].decode("utf-8"))
    if metadata.get("package") != package_name or metadata.get("revision") != revision:
        raise ValueError("{} is {} at {}, expected {} at {}".format(
            path, metadata.get("package"), metadata.get("revision"), package_name, revision))
    for entry, digest in metadata["runtime_files"].items():
        if entry not in entries or hashlib.sha256(entries[entry][1]).hexdigest() != digest:
            raise ValueError("{}: {} does not match its build-info.json SHA-256".format(path, entry))
    return metadata, entries


def licence_list(entries, separator):
    width = max(len(entry) for entry in entries) + 2
    return "".join("  {}{}\n".format(entry.replace("/", separator).ljust(width), LICENCES.get(entry, ""))
                   .rstrip() + "\n" for entry in entries)


def windows_readme(revision, source_url, included, omitted, licences):
    """[CHOSEN, not recovered] The combined Windows README: the per-role READMEs' facts, per folder."""
    folders = [component[0] for component in included]
    text = ("OpenBF2: openbf2-windows\n\n" + HEADER).format(revision=revision, source=source_url)
    text += "Contents (keep each folder's DLLs beside its EXE; do not mix folders):\n"
    for folder, _, _, description in included:
        text += "  {:<10}{}\n".format(folder + "\\", description)
    text += ("  licenses\\ licence texts and dependency notices; LICENSE is OpenBF2's own\n"
             "  build-info.json  source revision and the SHA-256 of every runtime file\n")
    if omitted:
        text += ("\nThis CI build lacks {}: not built on the CI runner (for example without a\n"
                 "usable D3DX source). Release packages always contain all four folders.\n").format(
                     ", ".join(folder + "\\" for folder in omitted))
    text += (
        "\nRequirements\n"
        "- The Microsoft Visual C++ runtime of the folder's architecture: x64 for x64\\,\n"
        "  x86 for x86\\, server\\ and editor\\.\n"
        "- These Windows binaries are not code-signed.\n"
        "- The x64 client needs the 64-bit d3dx9_25.dll of the DirectX End-User Runtime\n"
        "  (System32); the x86 client and the editor use the game folder's 32-bit copy.\n"
        "- PunkBuster works only with the x86 client.\n"
        "- The clients read CD keys from the retail registry or from a per-user\n"
        "  %LOCALAPPDATA%\\OpenBF2\\Registry.dat (docs/registry-file.md).\n"
        "\n" + TEST_NOTE
    )
    commands = {
        "x64": ("x64 client", '"C:\\OpenBF2\\openbf2-windows\\x64\\BF2.exe" +modPath mods/bf2 +fullscreen 0'),
        "x86": ("x86 client", '"C:\\OpenBF2\\openbf2-windows\\x86\\BF2.exe" +modPath mods/bf2 +fullscreen 0'),
        "server": ("server", '"C:\\OpenBF2\\openbf2-windows\\server\\bf2_w32ded.exe" +modPath mods/bf2'),
        "editor": ("editor", '"C:\\OpenBF2\\openbf2-windows\\editor\\BF2Editor.exe" +loadMod bf2'),
    }
    text += (
        "\nInstall (Windows, Command Prompt)\n"
        "1. Use your own Battlefield 2 1.50 installation: the retail game for the\n"
        "   clients and the editor, a 1.50 dedicated-server installation for the\n"
        "   server. Extract this archive to C:\\OpenBF2, which gives\n"
        "   C:\\OpenBF2\\openbf2-windows. Keep each folder's DLLs beside its EXE.\n"
        "2. Set the working directory to the installation root containing mods\\,\n"
        "   python\\ and pylib-2.3.4.zip (not to mods\\bf2):\n"
        "   cd /d \"C:\\Games\\Battlefield 2\"          (clients and editor)\n"
        "   cd /d \"C:\\Games\\Battlefield 2 Server\"   (server)\n"
        "3. Start with:\n"
    )
    for folder in folders:
        text += "   {}:\n      {}\n".format(*commands[folder])
    if "x64" in folders or "x86" in folders:
        text += "   add +help 0 to hide the clients' tutorial messages\n"
    text += (
        "   Replace the example paths with your own.\n"
        "   A shortcut can use the same command as Target and the installation\n"
        "   root as Start in. No original EXE or DLL is replaced; to go back,\n"
        "   launch the original executable/shortcut.\n"
        "   +modPath selects a mod, not the entire installation; keep the\n"
        "   working directory above.\n"
    )
    if "x64" in folders:
        text += "\nx64 client (x64\\)\n" + indent(windows_client_notes(
            "windows-x64", notice="x64\\OpenAL-Soft-NOTICE.txt"))
    if "x86" in folders:
        text += "\nx86 client (x86\\)\n" + indent(windows_client_notes(
            "windows-x86", notice="x86\\OpenAL-Soft-NOTICE.txt", folder="the x86 folder"))
    if "server" in folders:
        text += "\nDedicated server (server\\)\n" + indent(SERVER_SETTINGS)
    if "editor" in folders:
        text += ("\nEditor (editor\\)\n" + indent(EDITOR_DEPENDENCIES) +
                 "   The game folder supplies binkw32.dll, d3dx9_25.dll and BF2OpenAL.dll\n"
                 "   through the working directory where required.\n")
    text += "\nLink contracts (build-info.json: components)\n   Every executable links strictly.\n"
    voice = [folder + "\\" + name for folder, name in (
        ("x86", "BF2VoipServer.dll"), ("server", "BF2VoipServer_w32ded.dll"),
        ("editor", "BF2VoipServer.dll")) if folder in folders]
    if voice:
        text += ("   The bundled voice-server DLLs retain their independent diagnostic /FORCE\n"
                 "   links and may contain unresolved calls:\n{}"
                 "   A strict executable link does not establish side-DLL completeness.\n").format(
                     "".join("      {}\n".format(path) for path in voice))
    if "server" in folders:
        text += ("   The server's bundled AI library (server\\AIDLL_w32ded.dll) also retains\n"
                 "   its diagnostic /FORCE link.\n")
    text += "\nLicences and notices\n" + licence_list(licences, "\\")
    clients = [folder for folder in ("x64", "x86") if folder in folders]
    if clients:
        text += ("  {}: OpenAL Soft's version, source,\n"
                 "  build options and how to replace BF2OpenAL.dll\n"
                 "The OpenAL Soft source tarball the clients' BF2OpenAL.dll was built from is\n"
                 "attached to the same release (openal-soft-<version>.tar.bz2).\n").format(
                     ", ".join(folder + "\\OpenAL-Soft-NOTICE.txt" for folder in clients))
    return text


def linux_readme(revision, source_url, licences):
    contents = ("Contents: the x64 dedicated server (bf2) with libdice_py.so; licenses/ and\n"
                "LICENSE (OpenBF2's own); build-info.json with the source revision and the\n"
                "SHA-256 of every runtime file.\n\n")
    text = role_readme("linux-x64", "server", "openbf2-linux", "bf2", revision, source_url, contents=contents)
    return text.rstrip("\n") + "\n\nLicences and notices\n" + licence_list(licences, "/")


def assemble_release(args):
    """[CHOSEN, not recovered] Build openbf2-<platform>.zip from the per-role ZIPs below
    --packages-dir. Every component is required unless --allow-missing (CI runs only); every input
    is verified before anything is written."""
    platform = args.release
    name = "openbf2-" + platform
    source_url = "https://github.com/{}/tree/{}".format(args.repository, args.revision)
    files = {}
    runtime = {}
    components = []
    included = []
    omitted = []
    for component in RELEASES[platform]:
        folder, role_platform, role, _ = component
        package_name = "openbf2-{}-{}".format(role_platform, role)
        path = find_input(args.packages_dir, package_name + ".zip")
        if path is None:
            if not args.allow_missing:
                raise ValueError("Missing {}.zip below {}: a {} release needs every component ({})".format(
                    package_name, args.packages_dir, platform,
                    ", ".join("openbf2-{}-{}".format(c[1], c[2]) for c in RELEASES[platform])))
            omitted.append(folder or role)
            print("Not built, left out of this CI package: {}".format(package_name))
            continue
        metadata, entries = read_component(path, package_name, args.revision)
        prefix = folder + "/" if folder else ""
        for entry, (info, data) in sorted(entries.items()):
            if entry in ("README.txt", "build-info.json"):
                continue
            if entry in metadata["runtime_files"] or entry == "OpenAL-Soft-NOTICE.txt":
                target = prefix + entry
            elif entry == "LICENSE" or entry.startswith("licenses/"):
                target = entry
            else:
                raise ValueError("{}: unexpected entry {}".format(path, entry))
            if target in files and files[target][1] != data:
                raise ValueError("{} differs between the components of {}".format(target, name))
            files[target] = (info, data)
        runtime.update((prefix + entry, digest) for entry, digest in metadata["runtime_files"].items())
        components.append({
            "folder": folder or ".", "package": package_name, "platform": role_platform, "role": role,
            "link_contract": metadata["link_contract"],
        })
        included.append(component)
    if not included:
        raise ValueError("No component of {} below {}".format(name, args.packages_dir))

    # Both Windows clients bundle OpenAL Soft (LGPL): the release carries its exact source.
    companions = []
    notices = [data.decode("utf-8") for target, (_, data) in files.items() if target.endswith("OpenAL-Soft-NOTICE.txt")]
    if notices:
        tarball = find_input(args.packages_dir, "openal-soft-*.tar.bz2")
        if tarball is None:
            raise ValueError("No OpenAL Soft source tarball below {}".format(args.packages_dir))
        digest = sha256(tarball)
        for notice in notices:
            pinned = re.search(r"^Source SHA-256: ([0-9a-f]{64})$", notice, re.M)
            if not pinned or pinned.group(1) != digest:
                raise ValueError("{} does not match the SHA-256 in OpenAL-Soft-NOTICE.txt".format(tarball))
        companions.append(tarball)

    licences = sorted(target for target in files if target == "LICENSE" or target.startswith("licenses/"))
    metadata = {
        "package": name,
        "platform": platform,
        "revision": args.revision,
        "source": source_url,
        **({"windows_signing": "not configured"} if platform == "windows" else {}),
        "components": components,
        **({"omitted": omitted} if omitted else {}),
        "runtime_files": dict(sorted(runtime.items())),
    }
    if platform == "windows":
        readme = windows_readme(args.revision, source_url, included, omitted, licences)
    else:
        readme = linux_readme(args.revision, source_url, licences)

    args.output_dir.mkdir(parents=True, exist_ok=True)
    for source in companions:
        target = args.output_dir / source.name
        shutil.copyfile(source, target)
        write_checksum(target)
        print(target)
    archive_path = args.output_dir / (name + ".zip")
    with zipfile.ZipFile(archive_path, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
        for target, (info, data) in sorted(files.items()):
            # Keep the per-role executable bits (Linux); everything else is a plain 0644 file.
            executable = (info.external_attr >> 16) & 0o111
            entry = text_entry(name + "/" + target, 0o755 if executable else 0o644, info.date_time)
            archive.writestr(entry, data)
        archive.writestr(text_entry(name + "/build-info.json"), json.dumps(metadata, indent=2) + "\n")
        archive.writestr(text_entry(name + "/README.txt"), readme)
    write_checksum(archive_path)
    print(archive_path)


MACOS_CLIENT_FILES = ("bf2_client", "libdice_py.dylib", "libaidll.dylib")


def macos_build_info(args):
    """[CHOSEN, not recovered] build-info for openbf2-macos.dmg, written beside it. The disk image
    is built by the setup repository, which copies only the client's three files, so the record
    cannot live inside the image."""
    for path in [args.dmg] + [args.client_dir / item for item in MACOS_CLIENT_FILES]:
        if not path.is_file() or path.stat().st_size == 0:
            raise ValueError("Required input is missing or empty: {}".format(path))
    if not re.fullmatch(r"[0-9a-f]{40}", args.setup_revision or ""):
        raise ValueError("--setup-revision must be a full 40-character commit SHA")
    release = args.macos_signing == "developer-id"
    metadata = {
        "package": "openbf2-macos",
        "platform": "macos",
        "revision": args.revision,
        "source": "https://github.com/{}/tree/{}".format(args.repository, args.revision),
        "contents": "OpenBF2.app: setup assistant, launcher and the universal (arm64 + x86_64) client",
        "setup_app": {
            "repository": args.setup_repository,
            "revision": args.setup_revision,
            "source": "https://github.com/{}/tree/{}".format(args.setup_repository, args.setup_revision),
        },
        "macos_signing": args.macos_signing,
        "notarized": release,
        "stapled": release,
        "disk_image": {"file": args.dmg.name, "sha256": sha256(args.dmg)},
        "runtime_files": {item: sha256(args.client_dir / item) for item in MACOS_CLIENT_FILES},
        "runtime_files_note": ("SHA-256 of the universal client files handed to the setup build, which checks "
                               "its copies against them and then signs those copies inside the app"),
    }
    args.output_dir.mkdir(parents=True, exist_ok=True)
    target = args.output_dir / "openbf2-macos.build-info.json"
    target.write_text(json.dumps(metadata, indent=2) + "\n", encoding="utf-8")
    print(target)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--platform", choices=PACKAGES, help="Package one platform's per-role ZIPs")
    mode.add_argument("--release", choices=tuple(RELEASES) + ("macos",),
                      help="windows/linux: assemble openbf2-<platform>.zip from the per-role ZIPs below "
                           "--packages-dir; macos: write openbf2-macos.build-info.json for the disk image")
    parser.add_argument("--build-dir", type=Path, help="--platform: the CMake build directory")
    parser.add_argument("--packages-dir", type=Path,
                        help="--release windows/linux: searched recursively for the per-role ZIPs, "
                             "their .sha256 files and the OpenAL Soft source tarball")
    parser.add_argument("--allow-missing", action="store_true",
                        help="--release windows/linux, CI runs only: package the components that were "
                             "built and list the others as omitted; release runs need every component")
    parser.add_argument("--client-dir", type=Path, help="--release macos: the universal client directory")
    parser.add_argument("--dmg", type=Path, help="--release macos: the final (stapled) disk image")
    parser.add_argument("--setup-repository", help="--release macos: the setup app's owner/repository")
    parser.add_argument("--setup-revision", help="--release macos: the setup app's commit SHA")
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
                        help="macOS: how the binaries were signed (release CI signs them with a "
                             "Developer ID, and notarizes the disk image); changes only the README "
                             "and build-info")
    args = parser.parse_args()
    if args.native_sources is None:
        args.native_sources = args.source_dir / "out/third_party"
    if not re.fullmatch(r"[0-9a-fA-F]{40}", args.revision):
        parser.error("--revision must be a full 40-character commit SHA")
    owner_repo = r"[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+"
    if not re.fullmatch(owner_repo, args.repository):
        parser.error("--repository must be owner/repository")
    if args.platform:
        needed = ("build_dir",)
    elif args.release == "macos":
        needed = ("client_dir", "dmg", "setup_repository", "setup_revision")
    else:
        needed = ("packages_dir",)
    for option in needed:
        if getattr(args, option) is None:
            parser.error("--{} is required here".format(option.replace("_", "-")))
    if args.release == "macos" and not re.fullmatch(owner_repo, args.setup_repository):
        parser.error("--setup-repository must be owner/repository")
    try:
        if args.platform:
            package(args)
        elif args.release == "macos":
            macos_build_info(args)
        else:
            assemble_release(args)
    except (OSError, ValueError, KeyError, zipfile.BadZipFile) as error:
        parser.exit(1, "Packaging failed: {}\n".format(error))


if __name__ == "__main__":
    main()
