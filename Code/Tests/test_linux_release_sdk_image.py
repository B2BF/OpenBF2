#!/usr/bin/env python3
"""[TEMP] OCI identity fixtures: tiny synthetic metadata, no image import or execution."""

import hashlib
import importlib.util
import io
import json
from pathlib import Path
import subprocess
import sys
import tarfile
import tempfile
import unittest


SOURCE = Path(__file__).resolve().parents[2]
SCRIPT = SOURCE / "tools/linux/release/sdk-image.py"
SPEC = importlib.util.spec_from_file_location("linux_sdk_image", SCRIPT)
SDK_IMAGE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(SDK_IMAGE)

INDEX_TYPE = "application/vnd.oci.image.index.v1+json"
MANIFEST_TYPE = "application/vnd.oci.image.manifest.v1+json"
CONFIG_TYPE = "application/vnd.oci.image.config.v1+json"


def encoded(value):
    return json.dumps(value, sort_keys=True, separators=(",", ":")).encode("utf-8")


def descriptor(data, media_type):
    return {"mediaType": media_type, "size": len(data),
            "digest": "sha256:" + hashlib.sha256(data).hexdigest()}


def fixture(*, architecture="amd64", system="linux", config_bytes=None,
            manifest_changes=None, index_changes=None, config_type=CONFIG_TYPE):
    """Construct metadata independently of the helper, with an empty image filesystem."""
    config = config_bytes if config_bytes is not None else encoded({
        "architecture": architecture, "os": system,
        "config": {}, "rootfs": {"type": "layers", "diff_ids": []},
    })
    config_descriptor = descriptor(config, config_type)
    manifest = {"schemaVersion": 2, "mediaType": MANIFEST_TYPE,
                "config": config_descriptor, "layers": []}
    manifest.update(manifest_changes or {})
    manifest_bytes = encoded(manifest)
    manifest_descriptor = descriptor(manifest_bytes, MANIFEST_TYPE)
    index = {"schemaVersion": 2, "mediaType": INDEX_TYPE,
             "manifests": [manifest_descriptor]}
    index.update(index_changes or {})
    entries = {
        "oci-layout": encoded({"imageLayoutVersion": "1.0.0"}),
        "index.json": encoded(index),
        "blobs/sha256/" + manifest_descriptor["digest"][7:]: manifest_bytes,
        "blobs/sha256/" + config_descriptor["digest"][7:]: config,
    }
    return entries, manifest_descriptor, config_descriptor


class SdkImageTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="openbf2-sdk-identity-")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)

    def archive(self, entries, extra=()):
        path = self.root / "sdk.oci.tar"
        with tarfile.open(path, "w") as output:
            for name, data in list(entries.items()) + list(extra):
                entry = tarfile.TarInfo(name)
                entry.size = len(data)
                output.addfile(entry, io.BytesIO(data))
        return path

    def test_identity_uses_manifest_digest_and_separate_config_id(self):
        entries, manifest, config = fixture()
        result = SDK_IMAGE.identity(self.archive(entries))
        self.assertEqual(result, {
            "reference": "localhost/openbf2-linux-sdk@" + manifest["digest"],
            "manifest_digest": manifest["digest"],
            "image_id": config["digest"],
        })
        self.assertNotEqual(result["manifest_digest"], result["image_id"])

    def test_optional_index_media_type_can_be_omitted(self):
        entries, manifest, _ = fixture()
        index = json.loads(entries["index.json"])
        del index["mediaType"]
        entries["index.json"] = encoded(index)
        self.assertEqual(SDK_IMAGE.identity(self.archive(entries))["manifest_digest"], manifest["digest"])

    def test_manifest_and_config_byte_tampering(self):
        for kind in ("manifest", "config"):
            with self.subTest(kind=kind):
                entries, manifest, config = fixture()
                selected = manifest if kind == "manifest" else config
                name = "blobs/sha256/" + selected["digest"][7:]
                entries[name] = entries[name] + b" "
                with self.assertRaises(ValueError):
                    SDK_IMAGE.identity(self.archive(entries))

    def test_descriptor_size_and_hash_are_both_checked(self):
        for wrong_size in (True, False):
            with self.subTest(wrong_size=wrong_size):
                entries, _, _ = fixture()
                index = json.loads(entries["index.json"])
                entry = index["manifests"][0]
                if wrong_size:
                    entry["size"] += 1
                else:
                    original = "blobs/sha256/" + entry["digest"][7:]
                    entry["digest"] = "sha256:" + "0" * 64
                    entries["blobs/sha256/" + "0" * 64] = entries.pop(original)
                entries["index.json"] = encoded(index)
                with self.assertRaises(ValueError):
                    SDK_IMAGE.identity(self.archive(entries))

    def test_only_linux_amd64_configuration_is_accepted(self):
        for architecture, system in (("arm64", "linux"), ("amd64", "windows"), ("386", "linux")):
            with self.subTest(architecture=architecture, system=system):
                entries, _, _ = fixture(architecture=architecture, system=system)
                with self.assertRaises(ValueError):
                    SDK_IMAGE.identity(self.archive(entries))

    def test_ambiguous_manifest_selection_is_rejected(self):
        for count in (0, 2):
            with self.subTest(count=count):
                entries, _, _ = fixture()
                index = json.loads(entries["index.json"])
                index["manifests"] *= count
                entries["index.json"] = encoded(index)
                with self.assertRaises(ValueError):
                    SDK_IMAGE.identity(self.archive(entries))

    def test_duplicate_tar_metadata_names_are_rejected(self):
        for kind in ("index", "manifest", "config"):
            with self.subTest(kind=kind):
                entries, manifest, config = fixture()
                name = "index.json" if kind == "index" else "blobs/sha256/" + (
                    manifest if kind == "manifest" else config)["digest"][7:]
                with self.assertRaises(ValueError):
                    SDK_IMAGE.identity(self.archive(entries, [(name, entries[name])]))

    def test_duplicate_json_identity_keys_are_rejected(self):
        entries, _, _ = fixture(config_bytes=b'{"architecture":"arm64","architecture":"amd64","os":"linux"}')
        with self.assertRaises(ValueError):
            SDK_IMAGE.identity(self.archive(entries))

    def test_wrong_schema_and_media_types_are_rejected(self):
        cases = (
            {"index_changes": {"schemaVersion": 1}},
            {"index_changes": {"mediaType": "application/example"}},
            {"manifest_changes": {"schemaVersion": 1}},
            {"manifest_changes": {"mediaType": "application/example"}},
            {"config_type": "application/example"},
        )
        for changes in cases:
            with self.subTest(changes=changes):
                entries, _, _ = fixture(**changes)
                with self.assertRaises(ValueError):
                    SDK_IMAGE.identity(self.archive(entries))

    def test_malformed_json_is_rejected(self):
        entries, _, _ = fixture()
        entries["index.json"] = b'{"manifests":'
        with self.assertRaises(ValueError):
            SDK_IMAGE.identity(self.archive(entries))

    def test_metadata_symlink_is_rejected_without_extracting(self):
        entries, _, _ = fixture()
        path = self.archive({name: data for name, data in entries.items() if name != "index.json"})
        with tarfile.open(path, "a") as output:
            link = tarfile.TarInfo("index.json")
            link.type = tarfile.SYMTYPE
            link.linkname = "/does-not-exist"
            output.addfile(link)
        with self.assertRaises(ValueError):
            SDK_IMAGE.identity(path)
        self.assertEqual(sorted(p.name for p in self.root.iterdir()), ["sdk.oci.tar"])

    def test_cli_appends_github_outputs_with_checked_identity(self):
        entries, manifest, config = fixture()
        output = self.root / "github-output.txt"
        output.write_text("existing=value\n")
        completed = subprocess.run([sys.executable, str(SCRIPT), str(self.archive(entries)),
                                    "--github-output", str(output)], check=True,
                                   text=True, stdout=subprocess.PIPE)
        self.assertEqual(json.loads(completed.stdout)["image_id"], config["digest"])
        self.assertEqual(output.read_text().splitlines(), [
            "existing=value",
            "reference=localhost/openbf2-linux-sdk@" + manifest["digest"],
            "image_id=" + config["digest"],
            "manifest_digest=" + manifest["digest"],
        ])


if __name__ == "__main__":
    unittest.main()
