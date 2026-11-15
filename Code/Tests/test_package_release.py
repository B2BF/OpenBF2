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
