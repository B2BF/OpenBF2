#!/usr/bin/env python3
"""[TEMP] Packaging fixtures: synthetic binaries only; no engine execution or game content."""

import contextlib
import hashlib
import importlib.util
import io
import json
from pathlib import Path
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
                       "BF2Audio.dll", "TextureAtlasBuilder.dll", "aidll.dll"}
            self.assertEqual({entry for entry in entries if entry.endswith((".exe", ".dll"))}, runtime)
            for license_name in ("Ogg-COPYING.txt", "Vorbis-COPYING.txt", "gameswf.txt",
                                 "gameswf-base.txt", "FFmpeg-COPYING.LGPLv2.1.txt"):
                self.assertIn("licenses/" + license_name, entries)
            metadata = json.loads(archive.read(name + "/build-info.json"))
            self.assertEqual(set(metadata["runtime_files"]), runtime)
            for entry, digest in metadata["runtime_files"].items():
                self.assertEqual(hashlib.sha256(archive.read(name + "/" + entry)).hexdigest(), digest)
            self.assertEqual(metadata["link_contract"], "strict")
            self.assertNotIn(b"diagnostic /FORCE package", archive.read(name + "/README.txt"))
            self.assertNotIn(b"No Windows x64 AI library", archive.read(name + "/README.txt"))
        checksum = path.with_suffix(".zip.sha256").read_bytes()
        self.assertEqual(checksum, (hashlib.sha256(path.read_bytes()).hexdigest()
                                   + "  " + path.name + "\n").encode("ascii"))

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
                self.args.omit = ["editor"] if platform == "windows-x86" else ["client"]
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
            "openbf2-windows-x86-server", "openbf2-windows-x86-editor",
        })

    def test_windows_client_is_x64_only(self):
        self.assertNotIn("client", PACKAGER.PACKAGES["windows-x86"])
        self.assertIn("client", PACKAGER.PACKAGES["windows-x64"])
        self.args.platform = "windows-x86"
        with self.assertRaisesRegex(ValueError, "Cannot omit client"):
            self.args.omit = ["client"]
            self.package()

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
                            for entry in ("64-bit d3dx9_25.dll", "System OpenAL", "64-bit BF2OpenAL.dll",
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
                    notarized = signing == "developer-id"
                    self.assertEqual("this archive is notarized" in readme, notarized)
                    self.assertEqual("ad-hoc signed, not notarized" in readme, not notarized)
                    self.assertEqual("xattr -dr com.apple.quarantine ." in readme, not notarized)


if __name__ == "__main__":
    unittest.main()
