#!/usr/bin/env python3
"""[TEMP] Release packaging fixtures: tiny C ELFs, never game code or executable tests.

Uses the pinned setup checkout's real artifact auditor. No input ELF is executed;
the compiler, readelf, strip and patchelf are the only native tools invoked.
"""
import argparse
from contextlib import redirect_stdout
import importlib.util
import io
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch


ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("linux_stage", ROOT / "tools/linux/release/stage-artifacts.py")
stage = importlib.util.module_from_spec(spec)
spec.loader.exec_module(stage)
SETUP_PACKAGE = ROOT.parent / "openbf2-setup/linux/package.py"


class Artifacts(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        for tool in ("cc", "readelf", "strip", "patchelf"):
            if not shutil.which(tool):
                raise RuntimeError(f"Packaging fixture prerequisite missing: {tool}")
        if not SETUP_PACKAGE.is_file():
            raise RuntimeError("Provide the pinned setup auditor with --setup-package")
        cls.auditor = stage.load_auditor(SETUP_PACKAGE)
        cls.base_temp = tempfile.TemporaryDirectory(prefix="openbf2-artifact-fixture-")
        base = cls.base = Path(cls.base_temp.name)
        sdk = base / "sdk"
        libraries = sdk / "lib"
        libraries.mkdir(parents=True)
        engine = base / "bin_linux"
        engine.mkdir()
        (sdk / "COPYING.fixture").write_text("Synthetic packaging fixture; not a redistributable engine dependency.\n")

        def shared(path, name, body="int fixture(void) { return 7; }", link=()):
            path.parent.mkdir(parents=True, exist_ok=True)
            source = base / "fixture.c"
            source.write_text(body)
            subprocess.run(["cc", "-shared", "-fPIC", "-g", str(source), "-Wl,-soname," + name,
                            "-Wl,-rpath," + str(base / "build"), *link, "-o", str(path)], check=True)

        shared(libraries / "libfixture-leaf.so.1.2", "libfixture-leaf.so.1", "int leaf(void) { return 7; }")
        (libraries / "libfixture-leaf.so.1").symlink_to("libfixture-leaf.so.1.2")
        shared(libraries / "libfixture-root.so.1", "libfixture-root.so.1",
               "int leaf(void); int root(void) { return leaf(); }",
               ("-L" + str(libraries), "-Wl,--no-as-needed", "-l:libfixture-leaf.so.1"))
        names = ("libdxvk_d3d9.so.0", "libSDL3.so.0", "libopenal.so.1", "libpulse.so.0", "libasound.so.2", "libudev.so.1", "libdbus-1.so.3",
                 "libQt6Widgets.so.6", "libQt6Gui.so.6", "libQt6Core.so.6", "libQt6DBus.so.6")
        for name in names:
            shared(libraries / name, name)
        # This library is absent from every link-time dependency list; it must
        # still be found from the explicitly requested dynamic roots.
        shared(libraries / "libfixture-dlopen.so.1", "libfixture-dlopen.so.1")
        for relative in cls.auditor.PLUGINS + ("lib/qt6/plugins/platforms/libqwayland-generic.so",):
            shared(sdk / relative, Path(relative).name)
        shared(engine / "libdice_py.so", "libdice_py.so", "int root(void); int dice(void) { return root(); }",
               ("-L" + str(libraries), "-Wl,--no-as-needed", "-l:libfixture-root.so.1"))
        shared(engine / "libaidll.so", "libaidll.so")
        source = base / "main.c"
        source.write_text("int dice(void); int main(void) { return dice(); }\n")
        subprocess.run(["cc", str(source), "-g", "-L" + str(engine), "-ldice_py",
                        "-Wl,-rpath-link," + str(libraries), "-Wl,-rpath," + str(base / "build"),
                        "-o", str(engine / "bf2_client")], check=True)
        record = {"name": "fixture", "version": "1", "source": "https://example.invalid/fixture.tar.gz",
                  "sha256": "a" * 64, "license": "COPYING.fixture"}
        stage.write_json(sdk / "catalog.json", {
            "schema": 1, "dependencies": [record] + [dict(record, name=name) for name in ("vkd3d", "MojoShader")] + [
                dict(record, name="Boost", version="1.89.0", source="https://example.invalid/boost.tar.bz2", sha256="b" * 64),
                dict(record, name="Rust", version="1.90.0", source="https://example.invalid/rust-dist.tar.xz", sha256="c" * 64)],
            "files": {file.relative_to(sdk).as_posix(): "fixture" for file in sdk.rglob("*") if file.is_file()},
            "system_packages": {}})

    @classmethod
    def tearDownClass(cls):
        cls.base_temp.cleanup()

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="openbf2-artifact-test-")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.sdk = self.root / "sdk"
        shutil.copytree(self.base / "sdk", self.sdk, symlinks=True)
        self.engine_bin = self.root / "bin_linux"
        shutil.copytree(self.base / "bin_linux", self.engine_bin)
        self.args = SimpleNamespace(engine_bin=self.engine_bin, sdk_prefix=self.sdk,
                                    catalog=self.sdk / "catalog.json", toolchain=SETUP_PACKAGE.parent / "toolchain.json",
                                    setup_package=SETUP_PACKAGE, output=self.root / "artifacts", engine_source=ROOT,
                                    revision="1" * 40, sdk="registry.example/sdk@sha256:" + "2" * 64,
                                    library_dir=[], dynamic_library=["libfixture-dlopen.so.1"],
                                    build_root=[str(self.base / "build")], build_roots_file="")
        stage.write_json(self.engine_bin / "bf2_client.build.json", {
            "commit": self.args.revision, "architecture": "x86_64", "compiler": "GNU 15.2.0", "worktree": "",
            "untracked_sha256": {}, "tracked_diff_sha256": stage.hashlib.sha256(b"").hexdigest(),
            "binary_sha256": stage.digest(self.engine_bin / "bf2_client")})

    def build(self):
        with redirect_stdout(io.StringIO()):
            stage.stage(self.args)
        return self.args.output / "engine", self.args.output / "runtime"

    def test_relocated_closure_materialized_aliases_and_shared_bytes(self):
        source_digest = stage.digest(self.engine_bin / "bf2_client")
        engine, runtime = self.build()
        self.assertEqual(source_digest, stage.digest(self.engine_bin / "bf2_client"))
        self.assertFalse((engine / "lib/libfixture-leaf.so.1").is_symlink())
        self.assertTrue((engine / "lib/libfixture-dlopen.so.1").is_file())
        self.assertTrue((runtime / "lib/libudev.so.1").is_file())
        runtime_info = self.auditor.metadata(runtime, "runtime-artifact.json")
        engine_info = self.auditor.metadata(engine, "engine-artifact.json")
        records = {record["name"]: record for record in runtime_info["dependencies"]}
        catalog = json.loads(self.args.catalog.read_text())
        for source in catalog["dependencies"]:
            if source["name"] in ("Boost", "Rust"):
                record = records[source["name"]]
                for field in ("version", "source", "sha256"):
                    self.assertEqual(record[field], source[field])
                self.assertEqual(runtime_info["files"][record["license"]], stage.digest(self.sdk / source["license"]))
                self.assertNotIn(source["name"], {item["name"] for item in engine_info["dependencies"]})
        engine_files = {file.relative_to(engine): stage.digest(file) for file in self.auditor.files(engine)}
        for file in self.auditor.files(runtime):
            if file.relative_to(runtime) in engine_files:
                self.assertEqual(engine_files[file.relative_to(runtime)], stage.digest(file))
        relocated = self.root / "relocated space"
        shutil.move(self.args.output, relocated)
        self.auditor.check_inputs(relocated / "engine", relocated / "runtime", self.args.build_root)
        stage.audit_closure(relocated / "runtime", self.auditor, self.args.build_root)
        self.assertNotIn(".debug_info", stage.command("readelf", "-SW", str(relocated / "engine/bin/bf2_client")))

    def test_missing_transitive_library_refused(self):
        (self.sdk / "lib/libfixture-leaf.so.1").unlink()
        with self.assertRaisesRegex(ValueError, "Missing runtime library: libfixture-leaf"):
            self.build()

    def test_missing_dlopen_root_refused(self):
        (self.sdk / "lib/libfixture-dlopen.so.1").unlink()
        with self.assertRaisesRegex(ValueError, "Missing runtime library: libfixture-dlopen"):
            self.build()

    def test_missing_provenance_refused(self):
        data = json.loads(self.args.catalog.read_text())
        del data["files"]["lib/libfixture-root.so.1"]
        stage.write_json(self.args.catalog, data)
        with self.assertRaisesRegex(ValueError, "SDK file has no provenance"):
            self.build()

    def test_missing_setup_static_input_provenance_refused(self):
        original = json.loads(self.args.catalog.read_text())
        for name in ("Boost", "Rust"):
            with self.subTest(name=name):
                changed = dict(original, dependencies=[record for record in original["dependencies"] if record["name"] != name])
                stage.write_json(self.args.catalog, changed)
                with self.assertRaisesRegex(ValueError, "Missing setup static input provenance: " + name):
                    self.build()

    def test_setup_static_input_version_mismatch_refused(self):
        original = json.loads(self.args.catalog.read_text())
        for name in ("Boost", "Rust"):
            with self.subTest(name=name):
                changed = dict(original, dependencies=[dict(record, version="incorrect") if record["name"] == name else record
                                                       for record in original["dependencies"]])
                stage.write_json(self.args.catalog, changed)
                with self.assertRaisesRegex(ValueError, "Setup static input differs from pinned toolchain: " + name):
                    self.build()

    def test_missing_qt_plugin_refused(self):
        (self.sdk / "lib/qt6/plugins/platformthemes/libqxdgdesktopportal.so").unlink()
        with self.assertRaisesRegex(ValueError, "Missing Qt plugin"):
            self.build()

    def test_incomplete_runtime_closure_refused(self):
        _, runtime = self.build()
        # Rewrite a fixture's dependency using patchelf, never execute it.
        subprocess.run(["patchelf", "--add-needed", "libnot-bundled.so.1", str(runtime / "lib/libQt6Core.so.6")], check=True)
        with self.assertRaisesRegex(ValueError, "Unresolved artifact dependency"):
            stage.audit_closure(runtime, self.auditor)

    def test_escaping_runtime_path_refused(self):
        _, runtime = self.build()
        subprocess.run(["patchelf", "--set-rpath", "$ORIGIN/../../outside", str(runtime / "lib/libQt6Core.so.6")], check=True)
        with self.assertRaisesRegex(ValueError, "Runtime path escapes"):
            stage.audit_closure(runtime, self.auditor)

    def test_actual_source_prefix_in_rodata_refused(self):
        source = self.root / "leak.c"
        source.write_text('const char *leak = "/private/sdk-build/secret.c"; int fixture(void) { return leak[0]; }\n')
        subprocess.run(["cc", "-shared", "-fPIC", str(source), "-Wl,-soname,libSDL3.so.0",
                        "-o", str(self.sdk / "lib/libSDL3.so.0")], check=True)
        self.args.build_root.append("/private/sdk-build")
        with self.assertRaisesRegex(ValueError, "Build/source directory embedded"):
            self.build()

    def test_host_library_cannot_be_added_as_dynamic_dependency(self):
        self.args.dynamic_library.append("libc.so.6")
        with self.assertRaisesRegex(ValueError, "Host library must not be bundled"):
            self.build()

    def test_exact_installed_package_identity_required(self):
        catalog = stage.Catalog(self.args.catalog, self.sdk)
        library = self.root / "libdebian-fixture.so.1"
        library.write_bytes(b"not executed")
        ownership = subprocess.CompletedProcess([], 0, "fixture:amd64: " + str(library) + "\n", "")
        with patch.object(stage.subprocess, "run", return_value=ownership), patch.object(stage, "command", return_value="fixture:amd64=1.2"):
            with self.assertRaisesRegex(ValueError, "Installed package absent"):
                catalog.record_for(library)
            record = {"name": "fixture-deb", "version": "1.2", "source": "https://example.invalid/fixture.deb",
                      "sha256": "b" * 64, "license": str(self.sdk / "COPYING.fixture")}
            catalog.packages["fixture:amd64=1.2"] = record
            self.assertEqual(catalog.record_for(library), record)

    def test_common_license_references_copy_texts_despite_sentence_punctuation(self):
        common = self.root / "common-licenses"
        common.mkdir()
        for name in ("GPL", "GPL-3", "LGPL-2.1", "Local."):
            (common / name).write_text("Complete fixture license: " + name + "\n")
        (self.sdk / "COPYING.fixture").write_text(
            "The complete text is in /usr/share/common-licenses/GPL.\n"
            "Also see '/usr/share/common-licenses/LGPL-2.1', "
            "/usr/share/common-licenses/LGPL-2.1.\n"
            "Other terms (/usr/share/common-licenses/GPL-3); "
            "literal filename /usr/share/common-licenses/Local.\n")
        catalog = stage.Catalog(self.args.catalog, self.sdk)
        artifact = stage.Artifact(self.root / "license-artifact", "runtime", catalog, None, self.auditor,
                                  self.args.revision, self.args.sdk, json.loads(self.args.toolchain.read_text()), ())
        with patch.object(stage, "COMMON_LICENSES", common):
            artifact.add_record(catalog.dependencies["fixture"])
        artifact.finish()
        metadata = self.auditor.metadata(artifact.root, "runtime-artifact.json")
        copied = artifact.root / "share/licenses/common"
        self.assertEqual({path.name for path in copied.iterdir()}, {"GPL", "GPL-3", "LGPL-2.1", "Local."})
        for original in common.iterdir():
            target = copied / original.name
            self.assertEqual(target.read_bytes(), original.read_bytes())
            self.assertEqual(metadata["files"][target.relative_to(artifact.root).as_posix()], stage.digest(original))

    def test_missing_common_license_is_not_silently_skipped(self):
        common = self.root / "common-licenses"
        common.mkdir()
        (self.sdk / "COPYING.fixture").write_text("See /usr/share/common-licenses/Unavailable-2.1.\n")
        catalog = stage.Catalog(self.args.catalog, self.sdk)
        artifact = stage.Artifact(self.root / "license-artifact", "runtime", catalog, None, self.auditor,
                                  self.args.revision, self.args.sdk, json.loads(self.args.toolchain.read_text()), ())
        with patch.object(stage, "COMMON_LICENSES", common):
            with self.assertRaisesRegex(ValueError, r"Missing referenced common license: .*Unavailable-2\.1$"):
                artifact.add_record(catalog.dependencies["fixture"])

    def test_mislabeled_client_build_refused(self):
        filename = self.engine_bin / "bf2_client.build.json"
        record = json.loads(filename.read_text())
        for field, value, error in (("commit", "9" * 40, "revision"), ("compiler", "GNU 14.2.0", "pinned GCC"),
                                    ("binary_sha256", "0" * 64, "hash differs"), ("worktree", " M CMakeLists.txt", "dirty")):
            with self.subTest(field=field):
                stage.write_json(filename, dict(record, **{field: value}))
                with self.assertRaisesRegex(ValueError, error):
                    self.build()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--setup-package", type=Path, default=SETUP_PACKAGE)
    args, rest = parser.parse_known_args()
    SETUP_PACKAGE = args.setup_package.resolve()
    unittest.main(argv=[__file__] + rest)
