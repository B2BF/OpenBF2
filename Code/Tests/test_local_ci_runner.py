#!/usr/bin/env python3
"""Synthetic checks for local CI evidence, archive handling and runner isolation."""

import importlib.util
import io
import json
import os
from pathlib import Path
import sys
import tarfile
import tempfile
import unittest
from unittest.mock import patch
import zipfile

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("act_runner", ROOT / "tools/ci/act_runner.py")
runner = importlib.util.module_from_spec(spec)
spec.loader.exec_module(runner)


class RunnerTests(unittest.TestCase):
    def test_identity_is_offline_and_tracks_backend(self):
        with patch.object(runner.subprocess, "run", side_effect=AssertionError("must stay offline")):
            with patch.dict(os.environ, {"OPENBF2_CI_ENGINE": "podman"}):
                before = runner.config_identity(ROOT)
            with patch.dict(os.environ, {"OPENBF2_CI_ENGINE": "docker"}):
                after = runner.config_identity(ROOT)
        self.assertNotEqual(before, after)
        self.assertIn("tools/ci/patches/act-local.patch", before["resource_hashes"])
        self.assertIn(".github/workflows/ci.yml", before["workflow_hashes"])

    def test_host_tokens_and_signing_secrets_are_not_forwarded(self):
        with patch.dict(os.environ, {"GITHUB_TOKEN": "secret", "APPLE_ID": "private",
                                    "OPENBF2_ACT_NO_CONFIG": "0", "AWS_SECRET_ACCESS_KEY": "private"}):
            env = runner._clean_env()
        self.assertNotIn("GITHUB_TOKEN", env)
        self.assertNotIn("APPLE_ID", env)
        self.assertNotIn("AWS_SECRET_ACCESS_KEY", env)
        self.assertNotIn("OPENBF2_ACT_NO_CONFIG", env)

    def test_archive_traversal_is_rejected_before_writes(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            archive = root / "bad.tar.gz"
            with tarfile.open(archive, "w:gz") as out:
                member = tarfile.TarInfo("../escaped")
                member.size = 3
                out.addfile(member, io.BytesIO(b"bad"))
            with self.assertRaises(runner.RunnerError):
                runner._extract(archive, root / "extract")
            self.assertFalse((root / "escaped").exists())

    def test_archive_external_link_is_rejected(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            archive = root / "bad.tar.gz"
            with tarfile.open(archive, "w:gz") as out:
                member = tarfile.TarInfo("link")
                member.type = tarfile.SYMTYPE
                member.linkname = "../../outside"
                out.addfile(member)
            with self.assertRaises(runner.RunnerError):
                runner._extract(archive, root / "extract")

    def test_missing_or_empty_artifact_cannot_pass(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            with self.assertRaises(runner.RunnerError):
                runner._artifact_evidence(root, ["release-linux"])
            archive = root / "1/release-linux/release-linux.zip"
            archive.parent.mkdir(parents=True)
            with zipfile.ZipFile(archive, "w"):
                pass
            with self.assertRaises(runner.RunnerError):
                runner._artifact_evidence(root, ["release-linux"])
            with zipfile.ZipFile(archive, "w") as out:
                out.writestr("package.txt", "real fixture bytes")
            evidence = runner._artifact_evidence(root, ["release-linux"])
            self.assertEqual(evidence["release-linux"]["sha256"], runner._sha(archive))

    def test_sdk_identity_requires_consistent_verified_manifest(self):
        with tempfile.TemporaryDirectory() as temporary:
            log = Path(temporary) / "workflow.log"
            identity = {"reference": "localhost/openbf2-linux-sdk@sha256:" + "a" * 64,
                        "image_id": "sha256:" + "b" * 64, "manifest_digest": "sha256:" + "a" * 64}
            log.write_text("\n".join("[act] | " + line for line in json.dumps(identity, indent=2).splitlines()))
            self.assertEqual(runner._sdk_evidence(log), identity)
            identity["manifest_digest"] = "sha256:" + "c" * 64
            log.write_text(json.dumps(identity))
            with self.assertRaises(runner.RunnerError):
                runner._sdk_evidence(log)

    def test_preflight_uses_actual_pinned_actions(self):
        with tempfile.TemporaryDirectory() as temporary:
            path = runner._preflight_workflow(ROOT, Path(temporary))
            workflow = path.read_text()
            for action, sha in runner._pins(ROOT).items():
                self.assertIn("actions/" + action + "@" + sha, workflow)
            self.assertNotIn("@CHECKOUT@", workflow)
            self.assertIn("ref: ${{ github.sha }}", workflow)
            self.assertIn("needs: upload", workflow)

    def test_repository_uses_frozen_origin(self):
        for origin in ("https://github.com/example/fork.git", "git@github.com:example/fork.git"):
            with patch.object(runner, "_capture", return_value=origin):
                self.assertEqual(runner._repository(ROOT, {}), "example/fork")
        with patch.object(runner, "_capture", return_value="https://secret@github.com/example/fork"):
            with self.assertRaises(runner.RunnerError):
                runner._repository(ROOT, {})

    def test_concurrent_cache_use_is_refused(self):
        with tempfile.TemporaryDirectory() as temporary:
            with runner._cache_lock(temporary):
                with self.assertRaises(runner.RunnerError):
                    with runner._cache_lock(temporary):
                        self.fail("concurrent runner entered cache")


if __name__ == "__main__":
    unittest.main()
