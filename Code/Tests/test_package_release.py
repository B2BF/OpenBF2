#!/usr/bin/env python3
"""[TEMP] Packaging fixtures: synthetic binaries only; no engine execution or game content."""

import contextlib
import hashlib
import importlib.util
import io
import json
from pathlib import Path
import subprocess
import sys
import tempfile
from types import SimpleNamespace
import unittest
import zipfile


SOURCE = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location("package_release", SOURCE / "Code/Build/package_release.py")
PACKAGER = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(PACKAGER)


class PackageReleaseTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        root = Path(self.temp.name)
        self.args = SimpleNamespace(
            source_dir=SOURCE, build_dir=root / "build", output_dir=root / "packages",
            native_sources=root / "native", platform="windows-x64", omit=[],
            revision="a" * 40, repository="kr4uzi/OpenBF2",
        )

    def populate(self):
        for directory, names in PACKAGER.PACKAGES[self.args.platform].values():
            for name in names:
                path = self.args.build_dir / directory / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(b"synthetic fixture: " + name.encode("ascii"))
        if self.args.platform.startswith("windows-"):
            # A synthetic OpenAL Soft stage (Code/Build/build_openal_soft.py writes the real one).
            openal = self.args.build_dir / "openal-soft"
            openal.mkdir(parents=True, exist_ok=True)
            (openal / "COPYING").write_text("Synthetic LGPL text\n")
            tarball = openal / "openal-soft-0.0.0.tar.bz2"
            tarball.write_bytes(b"synthetic source tarball")
            (openal / "BUILD.txt").write_text("OpenAL Soft 0.0.0\nSource SHA-256: {}\nCMake options: -DALSOFT_EAX=ON\n".format(
                hashlib.sha256(tarball.read_bytes()).hexdigest()))
        # Synthetic dependency notices let macOS archive fixtures stay offline.
        for name in ("vkd3d/COPYING", "vkd3d/LICENSE", "mojoshader/LICENSE.txt"):
            path = self.args.native_sources / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text("Synthetic packaging-fixture notice\n")

    def package(self):
        with contextlib.redirect_stdout(io.StringIO()):
            PACKAGER.package(self.args)

    def test_x64_client_contents_and_contract(self):
        self.populate()
        # Unlisted build artifacts and retail companions must never enter the ZIP.
        for name in ("BF2.pdb", "BF2.map", "NovintHFX.dll", "binkw32.dll"):
            (self.args.build_dir / "bin_client" / name).write_bytes(b"exclude")
        self.package()
        name = "openbf2-windows-x64-client"
        path = self.args.output_dir / (name + ".zip")
        with zipfile.ZipFile(path) as archive:
            entries = {entry.removeprefix(name + "/") for entry in archive.namelist()}
            runtime = {"BF2.exe", "dice_py.dll", "RendDX9.dll", "SwiffPlayer.dll",
                       "BF2Audio.dll", "TextureAtlasBuilder.dll", "aidll.dll", "BF2OpenAL.dll"}
            self.assertEqual({entry for entry in entries if entry.endswith((".exe", ".dll"))}, runtime)
            for license_name in ("Ogg-COPYING.txt", "Vorbis-COPYING.txt", "gameswf.txt",
                                 "gameswf-base.txt", "FFmpeg-COPYING.LGPLv2.1.txt", "OpenAL-Soft-COPYING.txt"):
                self.assertIn("licenses/" + license_name, entries)
            notice = archive.read(name + "/OpenAL-Soft-NOTICE.txt").decode()
            for text in ("OpenAL Soft 0.0.0", "-DALSOFT_EAX=ON", "unmodified", "Replacing it", "System OpenAL"):
                self.assertIn(text, notice)
            metadata = json.loads(archive.read(name + "/build-info.json"))
            self.assertEqual(set(metadata["runtime_files"]), runtime)
            for entry, digest in metadata["runtime_files"].items():
                self.assertEqual(hashlib.sha256(archive.read(name + "/" + entry)).hexdigest(), digest)
            self.assertEqual(metadata["link_contract"], "strict")
            self.assertNotIn(b"diagnostic /FORCE package", archive.read(name + "/README.txt"))
            self.assertNotIn(b"No Windows x64 AI library", archive.read(name + "/README.txt"))
        source = self.args.output_dir / "openal-soft-0.0.0.tar.bz2"
        self.assertEqual(source.read_bytes(), b"synthetic source tarball")
        self.assertEqual(source.with_name(source.name + ".sha256").read_bytes(),
                         (hashlib.sha256(source.read_bytes()).hexdigest() + "  " + source.name + "\n").encode("ascii"))
        checksum = path.with_suffix(".zip.sha256").read_bytes()
        self.assertEqual(checksum, (hashlib.sha256(path.read_bytes()).hexdigest()
                                   + "  " + path.name + "\n").encode("ascii"))

    def test_x64_client_requires_the_pinned_openal_source(self):
        self.populate()
        openal = self.args.build_dir / "openal-soft"
        (openal / "openal-soft-0.0.0.tar.bz2").write_bytes(b"tampered")
        with self.assertRaisesRegex(ValueError, "does not match the SHA-256"):
            self.package()
        (openal / "BUILD.txt").unlink()
        with self.assertRaisesRegex(ValueError, "OpenAL Soft"):
            self.package()
        self.assertFalse(self.args.output_dir.exists())

    def test_x64_server_contents_and_contract(self):
        self.populate()
        self.package()
        name = "openbf2-windows-x64-server"
        with zipfile.ZipFile(self.args.output_dir / (name + ".zip")) as archive:
            runtime = {"bf2_w64ded.exe", "dice_py.dll", "aidll.dll"}
            entries = {entry.removeprefix(name + "/") for entry in archive.namelist()}
            self.assertEqual({entry for entry in entries if entry.endswith((".exe", ".dll"))}, runtime)
            metadata = json.loads(archive.read(name + "/build-info.json"))
            self.assertEqual(set(metadata["runtime_files"]), runtime)
            self.assertEqual(metadata["link_contract"], "strict")
            for entry, digest in metadata["runtime_files"].items():
                self.assertEqual(hashlib.sha256(archive.read(name + "/" + entry)).hexdigest(), digest)

    def test_omit_unbuilt_windows_client_and_editor(self):
        for platform in ("windows-x86", "windows-x64"):
            with self.subTest(platform=platform):
                self.args.platform = platform
                self.args.output_dir = self.args.output_dir.parent / platform
                self.populate()
                for directory in ("bin_client", "bin_editor"):
                    for path in (self.args.build_dir / directory).glob("*"):
                        path.unlink()
                self.args.omit = ["client", "editor"] if platform == "windows-x86" else ["client"]
                self.package()
                server = "server"
                self.assertEqual({p.name for p in self.args.output_dir.glob("*.zip")},
                                 {"openbf2-{}-{}.zip".format(platform, server)})

    def test_each_windows_input_is_required_before_any_zip_is_written(self):
        for platform in ("windows-x86", "windows-x64"):
            self.args.platform = platform
            self.populate()
            for role, (directory, runtime) in PACKAGER.PACKAGES[platform].items():
                for entry in runtime:
                    path = self.args.build_dir / directory / entry
                    content = path.read_bytes()
                    for condition in ("missing", "empty"):
                        with self.subTest(platform=platform, role=role, entry=entry, condition=condition):
                            if condition == "missing":
                                path.unlink()
                            else:
                                path.write_bytes(b"")
                            with self.assertRaisesRegex(ValueError, "missing or empty"):
                                self.package()
                            self.assertFalse(self.args.output_dir.exists())
                            path.write_bytes(content)

    def test_existing_windows_x86_roles(self):
        self.args.platform = "windows-x86"
        self.populate()
        self.package()
        self.assertEqual({p.stem for p in self.args.output_dir.glob("*.zip")}, {
            "openbf2-windows-x86-server", "openbf2-windows-x86-client",
            "openbf2-windows-x86-editor",
        })

    def test_windows_ships_both_clients(self):
        self.assertIn("client", PACKAGER.PACKAGES["windows-x86"])
        self.assertIn("client", PACKAGER.PACKAGES["windows-x64"])
        # Both bundle OpenAL Soft as BF2OpenAL.dll, with its licence, notice and source tarball.
        for platform, bits in (("windows-x86", "32-bit"), ("windows-x64", "64-bit")):
            with self.subTest(platform=platform):
                self.assertIn("BF2OpenAL.dll", PACKAGER.PACKAGES[platform]["client"][1])
                self.args.platform = platform
                self.args.output_dir = self.args.output_dir.parent / platform
                self.populate()
                self.package()
                name = "openbf2-{}-client".format(platform)
                with zipfile.ZipFile(self.args.output_dir / (name + ".zip")) as archive:
                    entries = archive.namelist()
                    self.assertIn(name + "/licenses/OpenAL-Soft-COPYING.txt", entries)
                    notice = archive.read(name + "/OpenAL-Soft-NOTICE.txt").decode()
                self.assertIn("any other\n{} OpenAL Soft build".format(bits), notice)
                self.assertTrue((self.args.output_dir / "openal-soft-0.0.0.tar.bz2").is_file())

    def test_install_instructions_for_every_package(self):
        for platform, roles in PACKAGER.PACKAGES.items():
            self.args.platform = platform
            self.populate()
            self.package()
            for role, (_, runtime) in roles.items():
                with self.subTest(platform=platform, role=role):
                    name = "openbf2-{}-{}".format(platform, role)
                    with zipfile.ZipFile(self.args.output_dir / (name + ".zip")) as archive:
                        readme = archive.read(name + "/README.txt").decode()
                        metadata = json.loads(archive.read(name + "/build-info.json"))
                    self.assertEqual(metadata["link_contract"],
                                     "diagnostic /FORCE" if role.endswith("-diagnostic") else "strict")
                    self.assertEqual("diagnostic /FORCE package" in readme, role.endswith("-diagnostic"))
                    if platform == "windows-x86":
                        self.assertEqual(metadata["link_contract"], "strict")
                        voice = "BF2VoipServer_w32ded.dll" if role == "server" else "BF2VoipServer.dll"
                        self.assertIn(voice, metadata["runtime_files"])
                        self.assertIn("independent diagnostic /FORCE link", readme)
                        if role == "server":
                            self.assertIn("AI library also retains its diagnostic /FORCE link", readme)
                    self.assertIn("Install (", readme)
                    self.assertIn("1. ", readme)
                    self.assertIn("Battlefield 2 1.50", readme)
                    for entry in ("python", "pylib-2.3.4.zip", runtime[0]):
                        self.assertIn(entry, readme)
                    if platform.startswith("windows-"):
                        self.assertIn('cd /d "C:\\Games\\Battlefield 2', readme)
                        self.assertIn('"C:\\OpenBF2\\{}\\{}"'.format(name, runtime[0]), readme)
                        self.assertIn("Start in", readme)
                        self.assertIn("No original EXE or DLL is replaced", readme)
                        if role == "client":
                            self.assertIn(r"%LOCALAPPDATA%\OpenBF2\Registry.dat", readme)
                        if role == "client" and platform == "windows-x86":
                            for entry in ("binkw32.dll", "d3dx9_25.dll", "bundled BF2OpenAL.dll",
                                          "OpenAL-Soft-NOTICE.txt", "application folder first", "pb\\",
                                          "working directory", "Checked against the code",
                                          "not runtime-tested here"):
                                self.assertIn(entry, readme)
                            self.assertNotIn("NovintHFX", readme)
                            self.assertNotIn("No PunkBuster", readme)
                        elif role == "client":
                            self.assertIn("cannot join servers that require PunkBuster", readme)
                            self.assertIn("Use the x86\n   client for those", readme)
                            for entry in ("64-bit d3dx9_25.dll", "bundled 64-bit BF2OpenAL.dll", "OpenAL-Soft-NOTICE.txt",
                                          "Checked against the code",
                                          "not runtime-tested here"):
                                self.assertIn(entry, readme)
                            self.assertNotIn("NovintHFX", readme)
                    else:
                        self.assertIn('cd "$HOME/Games/{}"'.format(name), readme)
                        self.assertIn("./" + runtime[0], readme)
                    if platform.startswith("macos-"):
                        for entry in ("Windows/Wine", "Settings/", "Profiles/",
                                      "case-insensitive", "xattr -dr com.apple.quarantine ."):
                            self.assertIn(entry, readme)
                        if role == "client":
                            self.assertIn("Application Support/OpenBF2/Profiles/", readme)
                            self.assertIn("Application Support/OpenBF2/Registry.con", readme)
                            self.assertIn(r"HKEY_LOCAL_MACHINE\SOFTWARE\Electronic Arts\EA Games\Battlefield 2\ergc = YOUR-CD-KEY", readme)
                            self.assertNotIn("macos-r1-registry-file.md", readme)
                    if role.startswith("editor"):
                        self.assertIn("+loadMod bf2", readme)
                        self.assertNotIn("+modPath mods/bf2", readme)
                    else:
                        self.assertIn("+modPath mods/bf2", readme)
                    if role.startswith("server"):
                        for entry in ("stock mods/bf2/Settings/ServerSettings.con",
                                      "mods/bf2/Settings/maplist.con", "Optional overrides: +config",
                                      "+mapList", "relative to the content root", "other than 16567"):
                            self.assertIn(entry, readme)
                        for entry in ("openbf2_cfg", "16890", "sv.internet 0", "mapList.append"):
                            self.assertNotIn(entry, readme)

    def test_macos_signing_notes(self):
        # Release CI packages Developer ID signed binaries; every other run packages ad-hoc ones.
        # The per-architecture ZIPs are CI artifacts and never notarized (the release ships the DMG).
        for signing in ("adhoc", "developer-id"):
            self.args.platform = "macos-arm64"
            self.args.macos_signing = signing
            self.args.output_dir = Path(self.temp.name) / ("packages-" + signing)
            self.populate()
            self.package()
            for role in ("client", "server"):
                with self.subTest(signing=signing, role=role):
                    name = "openbf2-macos-arm64-" + role
                    with zipfile.ZipFile(self.args.output_dir / (name + ".zip")) as archive:
                        readme = archive.read(name + "/README.txt").decode()
                        metadata = json.loads(archive.read(name + "/build-info.json"))
                    self.assertEqual(metadata["macos_signing"], signing)
                    developer_id = signing == "developer-id"
                    self.assertEqual("signed with a Developer ID" in readme, developer_id)
                    self.assertEqual("ad-hoc signed" in readme, not developer_id)
                    self.assertIn("CI artifact and not notarized", readme)
                    self.assertIn("openbf2-macos.dmg", readme)
                    self.assertNotIn("this archive is notarized", readme)
                    self.assertIn("xattr -dr com.apple.quarantine .", readme)


WINDOWS_RUNTIME = {
    "x64": {"BF2.exe", "dice_py.dll", "RendDX9.dll", "SwiffPlayer.dll", "BF2Audio.dll",
            "TextureAtlasBuilder.dll", "aidll.dll", "BF2OpenAL.dll"},
    "x86": {"BF2.exe", "dice_py.dll", "RendDX9.dll", "SwiffPlayer.dll", "BF2Audio.dll",
            "TextureAtlasBuilder.dll", "BF2VoipServer.dll", "BF2OpenAL.dll"},
    "server": {"bf2_w32ded.exe", "dice_py.dll", "AIDLL_w32ded.dll", "BF2VoipServer_w32ded.dll"},
    "editor": {"BF2Editor.exe", "dice_py.dll", "RendDX9.dll", "TextureAtlasBuilder.dll", "BF2VoipServer.dll"},
}
WINDOWS_LICENCES = {
    "LICENSE", "licenses/Python-LICENSE.txt", "licenses/zlib.txt", "licenses/Minizip.txt",
    "licenses/TinyXML.txt", "licenses/RSA-MD5.txt", "licenses/GameSpy.txt",
    "licenses/Ogg-COPYING.txt", "licenses/Vorbis-COPYING.txt", "licenses/gameswf.txt",
    "licenses/gameswf-base.txt", "licenses/FFmpeg-COPYING.LGPLv2.1.txt",
    "licenses/OpenAL-Soft-COPYING.txt", "licenses/ImGui-LICENSE.txt", "licenses/ImGuizmo-LICENSE.txt",
}


class ReleasePackageTests(unittest.TestCase):
    """The one-package-per-platform release assets, assembled from the per-role CI ZIPs."""

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.packages = self.root / "artifacts"
        self.output = self.root / "release"

    def build(self, platform, omit=()):
        """One CI leg: synthetic build outputs, packaged into that leg's artifact folder."""
        build_dir = self.root / ("build-" + platform)
        for directory, names in PACKAGER.PACKAGES[platform].values():
            for name in names:
                path = build_dir / directory / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(b"synthetic fixture: " + platform.encode("ascii") + b" " + name.encode("ascii"))
        if platform.startswith("windows-"):
            openal = build_dir / "openal-soft"
            openal.mkdir(parents=True, exist_ok=True)
            (openal / "COPYING").write_text("Synthetic LGPL text\n")
            tarball = openal / "openal-soft-0.0.0.tar.bz2"
            tarball.write_bytes(b"synthetic source tarball")
            (openal / "BUILD.txt").write_text("OpenAL Soft 0.0.0 (MSVC {})\nSource SHA-256: {}\n".format(
                platform, hashlib.sha256(tarball.read_bytes()).hexdigest()))
        args = SimpleNamespace(
            source_dir=SOURCE, build_dir=build_dir, output_dir=self.packages / ("packages-" + platform),
            native_sources=self.root / "native", platform=platform, omit=list(omit),
            revision="a" * 40, repository="kr4uzi/OpenBF2",
        )
        with contextlib.redirect_stdout(io.StringIO()):
            PACKAGER.package(args)

    def assemble(self, release, allow_missing=False):
        args = SimpleNamespace(release=release, packages_dir=self.packages, output_dir=self.output,
                               allow_missing=allow_missing, revision="a" * 40, repository="kr4uzi/OpenBF2")
        with contextlib.redirect_stdout(io.StringIO()):
            PACKAGER.assemble_release(args)

    def read(self, name):
        path = self.output / (name + ".zip")
        self.assertEqual(path.with_name(path.name + ".sha256").read_bytes(),
                         (hashlib.sha256(path.read_bytes()).hexdigest() + "  " + path.name + "\n").encode("ascii"))
        with zipfile.ZipFile(path) as archive:
            contents = {info.filename: (info, archive.read(info)) for info in archive.infolist()}
        for entry in contents:
            self.assertTrue(entry.startswith(name + "/"), entry)
        contents = {entry[len(name) + 1:]: value for entry, value in contents.items()}
        return contents, json.loads(contents["build-info.json"][1]), contents["README.txt"][1].decode()

    def test_windows_release_layout(self):
        self.build("windows-x86")
        self.build("windows-x64")
        self.assemble("windows")
        self.assertEqual({path.name for path in self.output.iterdir()}, {
            "openbf2-windows.zip", "openbf2-windows.zip.sha256",
            "openal-soft-0.0.0.tar.bz2", "openal-soft-0.0.0.tar.bz2.sha256",
        })
        source = self.output / "openal-soft-0.0.0.tar.bz2"
        self.assertEqual(source.read_bytes(), b"synthetic source tarball")
        self.assertEqual(source.with_name(source.name + ".sha256").read_bytes(),
                         (hashlib.sha256(source.read_bytes()).hexdigest() + "  " + source.name + "\n").encode("ascii"))
        contents, metadata, readme = self.read("openbf2-windows")
        runtime = {folder + "/" + name for folder, names in WINDOWS_RUNTIME.items() for name in names}
        notices = {"x64/OpenAL-Soft-NOTICE.txt", "x86/OpenAL-Soft-NOTICE.txt"}
        self.assertEqual(set(contents), runtime | notices | WINDOWS_LICENCES | {"README.txt", "build-info.json"})
        # Each folder holds its own leg's binaries, byte for byte.
        self.assertEqual(contents["x64/BF2.exe"][1], b"synthetic fixture: windows-x64 BF2.exe")
        self.assertEqual(contents["x86/BF2.exe"][1], b"synthetic fixture: windows-x86 BF2.exe")
        self.assertEqual(contents["server/dice_py.dll"][1], b"synthetic fixture: windows-x86 dice_py.dll")
        self.assertIn("any other\n64-bit OpenAL Soft build", contents["x64/OpenAL-Soft-NOTICE.txt"][1].decode())
        self.assertIn("any other\n32-bit OpenAL Soft build", contents["x86/OpenAL-Soft-NOTICE.txt"][1].decode())
        for notice in notices:
            self.assertIn("OpenAL-Soft-COPYING.txt in the package's licenses folder", contents[notice][1].decode())
        self.assertEqual(metadata["package"], "openbf2-windows")
        self.assertEqual(metadata["revision"], "a" * 40)
        self.assertEqual(metadata["source"], "https://github.com/kr4uzi/OpenBF2/tree/" + "a" * 40)
        self.assertEqual(metadata["windows_signing"], "not configured")
        self.assertNotIn("omitted", metadata)
        self.assertEqual(set(metadata["runtime_files"]), runtime)
        for entry, digest in metadata["runtime_files"].items():
            self.assertEqual(hashlib.sha256(contents[entry][1]).hexdigest(), digest)
        self.assertEqual([(c["folder"], c["package"], c["link_contract"]) for c in metadata["components"]], [
            ("x64", "openbf2-windows-x64-client", "strict"), ("x86", "openbf2-windows-x86-client", "strict"),
            ("server", "openbf2-windows-x86-server", "strict"), ("editor", "openbf2-windows-x86-editor", "strict"),
        ])
        # The facts of the four per-role READMEs, merged.
        for text in (
            "OpenBF2: openbf2-windows", "Source revision: " + "a" * 40, "does not contain a\nBattlefield 2 installation",
            "Do not mix rebuilt client/editor engine plugins with retail engine plugins",
            "x64\\", "x86\\", "server\\", "editor\\", "licenses\\", "build-info.json",
            "Microsoft Visual C++ runtime", "x64 for x64\\", "x86 for x86\\, server\\ and editor\\",
            "not code-signed", "PunkBuster works only with the x86 client",
            "64-bit d3dx9_25.dll", "%LOCALAPPDATA%\\OpenBF2\\Registry.dat", "docs/registry-file.md",
            "other than 16567", "do not establish loading, joining or gameplay",
            'cd /d "C:\\Games\\Battlefield 2"', 'cd /d "C:\\Games\\Battlefield 2 Server"',
            'x64 client:\n      "C:\\OpenBF2\\openbf2-windows\\x64\\BF2.exe" +modPath mods/bf2 +fullscreen 0',
            '"C:\\OpenBF2\\openbf2-windows\\x86\\BF2.exe" +modPath mods/bf2 +fullscreen 0',
            '"C:\\OpenBF2\\openbf2-windows\\server\\bf2_w32ded.exe" +modPath mods/bf2',
            '"C:\\OpenBF2\\openbf2-windows\\editor\\BF2Editor.exe" +loadMod bf2',
            "+help 0", "Start in", "No original EXE or DLL is replaced", "+modPath selects a mod",
            "pylib-2.3.4.zip",
            # x64 client
            "does not use the game folder's 32-bit DLLs", "bundled 64-bit BF2OpenAL.dll",
            "x64\\OpenAL-Soft-NOTICE.txt", "cannot join servers that require PunkBuster", "Use the x86\n   client for those",
            # x86 client
            "binkw32.dll, d3dx9_25.dll and PunkBuster's\n   pb\\ folder", "x86\\OpenAL-Soft-NOTICE.txt",
            "application folder first", "the x86 folder",
            "Checked against the code; not runtime-tested here",
            # server
            "stock mods/bf2/Settings/ServerSettings.con", "mods/bf2/Settings/maplist.con",
            "Optional overrides: +config", "+mapList", "relative to the content root",
            # editor
            "Bink, d3dx9_25 and an\n   OpenAL provider", "BF2OpenAL.dll\n   through the working directory",
            # link contracts
            "Every executable links strictly", "x86\\BF2VoipServer.dll", "server\\BF2VoipServer_w32ded.dll",
            "editor\\BF2VoipServer.dll", "independent diagnostic /FORCE\n   links",
            "server\\AIDLL_w32ded.dll) also retains\n   its diagnostic /FORCE link",
            "does not establish side-DLL completeness",
            # licences and notices
            "licenses\\Python-LICENSE.txt", "licenses\\FFmpeg-COPYING.LGPLv2.1.txt", "x64 client's RendDX9.dll",
            "licenses\\ImGui-LICENSE.txt", "licenses\\ImGuizmo-LICENSE.txt", "licenses\\OpenAL-Soft-COPYING.txt",
            "openal-soft-<version>.tar.bz2", "LICENSE",
        ):
            self.assertIn(text, readme)
        for text in ("NovintHFX", "This CI build lacks", "diagnostic /FORCE package", "macOS"):
            self.assertNotIn(text, readme)
        for entry in WINDOWS_LICENCES:
            self.assertIn(entry.replace("/", "\\"), readme)

    def test_windows_release_without_d3dx(self):
        # Without a usable D3DX source the legs package their servers only.
        self.build("windows-x86", omit=("client", "editor"))
        self.build("windows-x64", omit=("client",))
        # A release needs every component and writes nothing otherwise.
        with self.assertRaisesRegex(ValueError, "a windows release needs every component"):
            self.assemble("windows")
        self.assertFalse(self.output.exists())
        # CI runs package what was built and say what is missing.
        self.assemble("windows", allow_missing=True)
        self.assertEqual({path.name for path in self.output.iterdir()},
                         {"openbf2-windows.zip", "openbf2-windows.zip.sha256"})
        contents, metadata, readme = self.read("openbf2-windows")
        self.assertEqual(metadata["omitted"], ["x64", "x86", "editor"])
        self.assertEqual(set(metadata["runtime_files"]), {"server/" + name for name in WINDOWS_RUNTIME["server"]})
        self.assertEqual({entry for entry in contents if "/" in entry and not entry.startswith("licenses/")},
                         {"server/" + name for name in WINDOWS_RUNTIME["server"]})
        self.assertIn("This CI build lacks x64\\, x86\\, editor\\", readme)
        self.assertIn("Release packages always contain all four folders", readme)
        for text in ("x64\\BF2.exe", "editor\\BF2Editor.exe", "OpenAL-Soft-NOTICE.txt", "openal-soft-"):
            self.assertNotIn(text, readme)
        self.assertNotIn("licenses/OpenAL-Soft-COPYING.txt", contents)

    def test_windows_release_with_one_client_missing(self):
        # The legs are independent runners: only the x64 one may lack D3DX.
        self.build("windows-x86")
        self.build("windows-x64", omit=("client",))
        self.assemble("windows", allow_missing=True)
        contents, metadata, readme = self.read("openbf2-windows")
        self.assertEqual(metadata["omitted"], ["x64"])
        self.assertIn("x86/OpenAL-Soft-NOTICE.txt", contents)
        self.assertNotIn("licenses/FFmpeg-COPYING.LGPLv2.1.txt", contents)
        # The x86 leg's copy of the OpenAL Soft source serves the release.
        self.assertTrue((self.output / "openal-soft-0.0.0.tar.bz2").is_file())

    def test_release_inputs_are_verified(self):
        self.build("windows-x86")
        self.build("windows-x64")
        artifacts = self.packages / "packages-windows-x86"
        checksum = artifacts / "openbf2-windows-x86-server.zip.sha256"
        original = checksum.read_bytes()
        checksum.write_bytes(b"0" * 64 + b"  openbf2-windows-x86-server.zip\n")
        with self.assertRaisesRegex(ValueError, "does not match openbf2-windows-x86-server.zip.sha256"):
            self.assemble("windows")
        checksum.write_bytes(original)
        # The two legs' OpenAL Soft tarballs must be the same pinned file.
        (artifacts / "openal-soft-0.0.0.tar.bz2").write_bytes(b"other")
        with self.assertRaisesRegex(ValueError, "Conflicting inputs"):
            self.assemble("windows")
        self.assertFalse(self.output.exists())

    def test_release_rejects_another_revision(self):
        self.build("linux-x64")
        args = SimpleNamespace(release="linux", packages_dir=self.packages, output_dir=self.output,
                               allow_missing=False, revision="b" * 40, repository="kr4uzi/OpenBF2")
        with self.assertRaisesRegex(ValueError, "expected openbf2-linux-x64-server at b{40}"):
            PACKAGER.assemble_release(args)
        self.assertFalse(self.output.exists())

    def test_linux_release_layout(self):
        self.build("linux-x64")
        self.assemble("linux")
        self.assertEqual({path.name for path in self.output.iterdir()},
                         {"openbf2-linux.zip", "openbf2-linux.zip.sha256"})
        contents, metadata, readme = self.read("openbf2-linux")
        licences = {"LICENSE", "licenses/Python-LICENSE.txt", "licenses/zlib.txt", "licenses/Minizip.txt",
                    "licenses/TinyXML.txt", "licenses/RSA-MD5.txt", "licenses/GameSpy.txt"}
        self.assertEqual(set(contents), {"bf2", "libdice_py.so", "README.txt", "build-info.json"} | licences)
        for entry in ("bf2", "libdice_py.so"):
            self.assertEqual(contents[entry][0].external_attr >> 16 & 0o777, 0o755)
        self.assertEqual(contents["LICENSE"][0].external_attr >> 16 & 0o777, 0o644)
        self.assertEqual(metadata["package"], "openbf2-linux")
        self.assertEqual(set(metadata["runtime_files"]), {"bf2", "libdice_py.so"})
        for entry, digest in metadata["runtime_files"].items():
            self.assertEqual(hashlib.sha256(contents[entry][1]).hexdigest(), digest)
        self.assertEqual(metadata["components"], [{"folder": ".", "package": "openbf2-linux-x64-server",
                                                   "platform": "linux-x64", "role": "server",
                                                   "link_contract": "strict"}])
        self.assertNotIn("windows_signing", metadata)
        for text in ("OpenBF2: openbf2-linux", "x64 dedicated server", "Ubuntu 22.04", "Keep libdice_py.so beside bf2",
                     'cd "$HOME/Games/openbf2-linux"', "chmod +x bf2", "./bf2 +modPath mods/bf2",
                     "stock mods/bf2/Settings/ServerSettings.con", "other than 16567",
                     "Linux server content with lowercase game paths", "licenses/Python-LICENSE.txt",
                     "licenses/GameSpy.txt", "build-info.json"):
            self.assertIn(text, readme)
        for text in ("(clients)", "OpenAL", "xattr", "Windows requires"):
            self.assertNotIn(text, readme)

    def test_release_command_line(self):
        self.build("linux-x64")
        command = [sys.executable, str(SOURCE / "Code/Build/package_release.py"), "--release", "linux",
                   "--output-dir", str(self.output), "--revision", "a" * 40, "--repository", "kr4uzi/OpenBF2"]
        missing = subprocess.run(command, capture_output=True, text=True)
        self.assertNotEqual(missing.returncode, 0)
        self.assertIn("--packages-dir is required", missing.stderr)
        result = subprocess.run(command + ["--packages-dir", str(self.packages)], capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertTrue((self.output / "openbf2-linux.zip").is_file())
        failed = subprocess.run(command[:3] + ["windows"] + command[4:] + ["--packages-dir", str(self.packages)],
                                capture_output=True, text=True)
        self.assertEqual(failed.returncode, 1)
        self.assertIn("a windows release needs every component", failed.stderr)

    def test_macos_build_info(self):
        client = self.root / "client-universal"
        client.mkdir()
        for item in PACKAGER.MACOS_CLIENT_FILES:
            (client / item).write_bytes(b"universal " + item.encode("ascii"))
        dmg = self.root / "openbf2-macos.dmg"
        dmg.write_bytes(b"synthetic disk image")
        for signing in ("developer-id", "adhoc"):
            with self.subTest(signing=signing):
                args = SimpleNamespace(client_dir=client, dmg=dmg, setup_repository="kr4uzi/openbf2-setup",
                                       setup_revision="c" * 40, macos_signing=signing, output_dir=self.output / signing,
                                       revision="a" * 40, repository="kr4uzi/OpenBF2")
                with contextlib.redirect_stdout(io.StringIO()):
                    PACKAGER.macos_build_info(args)
                metadata = json.loads((args.output_dir / "openbf2-macos.build-info.json").read_text())
                self.assertEqual(metadata["package"], "openbf2-macos")
                self.assertEqual(metadata["revision"], "a" * 40)
                self.assertEqual(metadata["setup_app"]["revision"], "c" * 40)
                self.assertEqual(metadata["macos_signing"], signing)
                self.assertEqual(metadata["notarized"], signing == "developer-id")
                self.assertEqual(metadata["stapled"], signing == "developer-id")
                self.assertEqual(metadata["disk_image"], {"file": "openbf2-macos.dmg",
                                                          "sha256": hashlib.sha256(dmg.read_bytes()).hexdigest()})
                self.assertEqual(metadata["runtime_files"], {
                    item: hashlib.sha256((client / item).read_bytes()).hexdigest()
                    for item in PACKAGER.MACOS_CLIENT_FILES})
        (client / "libaidll.dylib").unlink()
        with self.assertRaisesRegex(ValueError, "missing or empty"):
            PACKAGER.macos_build_info(args)


class OpenALSoftPinTests(unittest.TestCase):
    def test_openal_soft_source_is_pinned(self):
        spec = importlib.util.spec_from_file_location("build_openal_soft", SOURCE / "Code/Build/build_openal_soft.py")
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        self.assertRegex(module.SHA256, r"^[0-9a-f]{64}$")
        self.assertTrue(module.URL.startswith("https://openal-soft.org/openal-releases/"))
        self.assertTrue(module.URL.endswith("/" + module.TARBALL))
        for option in ("-DLIBTYPE=SHARED", "-DALSOFT_EAX=ON", "-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded",
                       "-DALSOFT_UTILS=OFF", "-DALSOFT_EXAMPLES=OFF", "-DALSOFT_TESTS=OFF"):
            self.assertIn(option, module.CMAKE_OPTIONS)
        for arch in ("x86", "x64"):
            self.assertIn("Source SHA-256: " + module.SHA256, module.build_info(arch))
            self.assertIn("MSVC " + arch, module.build_info(arch))


if __name__ == "__main__":
    unittest.main()
