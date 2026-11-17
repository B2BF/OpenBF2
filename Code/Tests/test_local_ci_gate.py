#!/usr/bin/env python3
"""[TEMP] Isolated Git/GitHub fixtures for the issue #47 push and PR gate."""

import importlib.util
import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
from types import SimpleNamespace
from unittest.mock import patch


ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location("git_gate", ROOT / "tools/ci/git_gate.py")
gate = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(gate)

HEAD = "a" * 40
BASE = "b" * 40
OLD = "c" * 40
ZERO = "0" * 40


def pr(head_repo="owner/engine", branch="feature", target="owner/engine", base="main"):
    return {"head": {"repo": {"full_name": head_repo}, "ref": branch, "sha": OLD},
            "base": {"repo": {"full_name": target}, "ref": base, "sha": BASE},
            "html_url": f"https://github.com/{target}/pull/47", "number": 47}


def associated_response(entries=(), more=False, cursor=None):
    nodes = [{"url": item["html_url"], "headRefName": item["head"]["ref"],
              "headRepository": {"nameWithOwner": item["head"]["repo"]["full_name"]},
              "baseRefName": item["base"]["ref"],
              "baseRepository": {"nameWithOwner": item["base"]["repo"]["full_name"]}} for item in entries]
    return SimpleNamespace(stdout=json.dumps({"data": {"repository": {"ref": {
        "associatedPullRequests": {"nodes": nodes, "pageInfo": {"hasNextPage": more, "endCursor": cursor}}}}}}))


class GateTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="openbf2-ci-gate-")
        self.addCleanup(self.temp.cleanup)
        self.repo = Path(self.temp.name) / "checkout with spaces"
        self.repo.mkdir()
        self.git("init", "-q", "-b", "feature")
        self.git("config", "user.name", "Fixture")
        self.git("config", "user.email", "fixture@example.invalid")
        (self.repo / "input.txt").write_text("fixture\n")
        self.git("add", "input.txt")
        self.git("commit", "-qm", "fixture")
        self.head = self.git("rev-parse", "HEAD")
        self.git("remote", "add", "origin", "https://github.com/owner/engine.git")

    def git(self, *args):
        return subprocess.check_output(["git", *args], cwd=self.repo,
                                       env=gate._environment(), text=True,
                                       stderr=subprocess.DEVNULL).strip()

    def updates(self, branch="feature", head=HEAD, old=OLD):
        return f"refs/heads/local {head} refs/heads/{branch} {old}\n"

    def protected(self, text):
        return gate.protected_pushes(self.repo, "origin", "https://github.com/owner/engine.git", text)

    def test_remote_url_formats(self):
        for url in ("https://github.com/owner/engine/", "git@github.com:owner/engine.git",
                    "ssh://git@github.com/owner/engine.git"):
            self.assertEqual(gate.repository_from_url(url), "owner/engine")
        for url in ("https://elsewhere.test/owner/engine", "/tmp/engine.git",
                    "https://token:secret@github.com/owner/engine", "https://token@github.com/o/r",
                    "https://github.com/o/r?token=secret"):
            with self.assertRaises(gate.GateError):
                gate.repository_from_url(url)

    def test_deleted_tag_and_unchanged_refs_need_no_github(self):
        text = self.updates(head=ZERO) + f"refs/tags/v1 {HEAD} refs/tags/v1 {ZERO}\n" + self.updates(head=OLD)
        with patch.object(gate, "_api", side_effect=AssertionError("unexpected network")):
            self.assertEqual(self.protected(text), [])

    def test_backup_push_is_not_protected(self):
        with patch.object(gate, "_associated_prs", return_value=[]):
            self.assertEqual(self.protected(self.updates()), [])

    def test_open_pr_uses_pushed_sha_not_checkout_head(self):
        with patch.object(gate, "_associated_prs", return_value=[pr()]), \
                patch.object(gate, "_branch_sha", return_value=BASE):
            result, = self.protected(self.updates())
        self.assertEqual(result["head_sha"], HEAD)
        self.assertNotEqual(result["head_sha"], self.head)
        self.assertEqual(result["kind"], "pr")
        self.assertEqual(result["local_branch"], "local")

    def test_main_and_multiple_ref_updates(self):
        with patch.object(gate, "_associated_prs", side_effect=[[], [pr()]]), \
                patch.object(gate, "_branch_sha", return_value=BASE):
            result = self.protected(self.updates("main") + self.updates("feature", head=OLD, old=BASE))
        self.assertEqual([entry["kind"] for entry in result], ["main", "pr"])
        self.assertEqual([entry["head_sha"] for entry in result], [HEAD, OLD])

    def test_github_failure_never_allows_unknown_backup_push(self):
        with patch.object(gate, "_associated_prs", side_effect=gate.GateError("offline")):
            with self.assertRaisesRegex(gate.GateError, "offline"):
                self.protected(self.updates())

    def test_malformed_push_input_is_rejected(self):
        for text in ("too few fields", self.updates(head="not-a-sha"), self.updates("../invalid")):
            with self.assertRaises(gate.GateError):
                self.protected(text)

    def test_associated_prs_cover_sibling_forks_without_local_remotes(self):
        entries = [pr(), pr(target="sibling/engine"), pr(head_repo="owner/unrelated")]
        with patch.object(gate, "_command", return_value=associated_response(entries)):
            result = gate._associated_prs(self.repo, "owner/engine", "feature")
        self.assertEqual([item["base"]["repo"]["full_name"] for item in result],
                         ["owner/engine", "sibling/engine"])

    def test_associated_prs_paginate_and_reject_repeated_cursor(self):
        responses = [associated_response([pr()], True, "next"), associated_response([pr(target="sibling/engine")])]
        with patch.object(gate, "_command", side_effect=responses) as command:
            self.assertEqual(len(gate._associated_prs(self.repo, "owner/engine", "feature")), 2)
        self.assertIn("cursor=next", command.call_args.args[1])
        with patch.object(gate, "_command", return_value=associated_response([], True, "next")):
            with self.assertRaisesRegex(gate.GateError, "pagination"):
                gate._associated_prs(self.repo, "owner/engine", "feature")

    def test_unpublished_ref_is_allowed_but_missing_repository_fails(self):
        response = SimpleNamespace(stdout=json.dumps({"data": {"repository": {"ref": None}}}))
        with patch.object(gate, "_command", return_value=response):
            self.assertEqual(gate._associated_prs(self.repo, "owner/engine", "new-branch"), [])
        response = SimpleNamespace(stdout=json.dumps({"data": {"repository": None}}))
        with patch.object(gate, "_command", return_value=response):
            with self.assertRaisesRegex(gate.GateError, "push repository"):
                gate._associated_prs(self.repo, "owner/engine", "feature")

    def test_partial_graphql_error_fails_closed(self):
        response = SimpleNamespace(stdout=json.dumps({"data": {"repository": {"ref": None}}, "errors": [{}]}))
        with patch.object(gate, "_command", return_value=response):
            with self.assertRaisesRegex(gate.GateError, "resolve all associated"):
                gate._associated_prs(self.repo, "owner/engine", "feature")

    def test_pull_requests_match_entire_head_repository_and_branch(self):
        with patch.object(gate, "_api", return_value=[pr(), pr("owner/other"), pr(branch="elsewhere")]):
            result = gate._open_prs(self.repo, ["owner/engine"], "owner/engine", "feature")
        self.assertEqual(result, [pr()])

    def test_pull_request_pages_are_not_truncated(self):
        with patch.object(gate, "_api", side_effect=[[pr("owner/other")] * 100, [pr()]]) as api:
            self.assertEqual(gate._open_prs(self.repo, ["owner/engine"], "owner/engine", "feature"), [pr()])
        self.assertIn("page=2", api.call_args.args[1])

    def test_prepare_pr_detects_remote_mismatch(self):
        with patch.object(gate, "_branch_sha", side_effect=[OLD, BASE]), \
                patch.object(gate, "_open_prs", return_value=[]):
            result = gate.prepare_pr(self.repo)
        self.assertFalse(result["pushed"])
        self.assertEqual(result["head_sha"], self.head)
        self.assertEqual(result["target_repo"], "owner/engine")
        self.assertEqual(result["compare_url"], "https://github.com/owner/engine/compare/main...feature?expand=1")

    def test_prepare_pr_existing_pr_opens_existing_page(self):
        with patch.object(gate, "_branch_sha", side_effect=[self.head, BASE]), \
                patch.object(gate, "_open_prs", return_value=[pr()]):
            result = gate.prepare_pr(self.repo)
        self.assertTrue(result["pushed"])
        self.assertEqual(result["compare_url"], pr()["html_url"])

    def test_prepare_pr_fork_target(self):
        with patch.object(gate, "_branch_sha", side_effect=[self.head, BASE]), \
                patch.object(gate, "_open_prs", return_value=[]):
            result = gate.prepare_pr(self.repo, target_repo="base/engine")
        self.assertEqual(result["compare_url"], "https://github.com/base/engine/compare/main...owner%3Afeature?expand=1")

    def test_prepare_non_checked_out_branch_and_main(self):
        self.git("branch", "main")
        with patch.object(gate, "_branch_sha", side_effect=[self.head, BASE]), \
                patch.object(gate, "_open_prs", return_value=[]):
            result = gate.prepare_pr(self.repo, branch="main")
        self.assertEqual(result["kind"], "main")
        self.assertEqual(self.git("branch", "--show-current"), "feature")

    def test_prepare_detached_checkout_requires_branch(self):
        self.git("checkout", "--detach", "-q")
        with self.assertRaisesRegex(gate.GateError, "Select a local branch"):
            gate.prepare_pr(self.repo)

    def test_prepare_exact_revision_with_different_remote_branch(self):
        self.git("checkout", "--detach", "-q")
        with patch.object(gate, "_branch_sha", side_effect=[OLD, BASE]), \
                patch.object(gate, "_open_prs", return_value=[]):
            result = gate.prepare_pr(self.repo, revision=self.head, head_branch="published/feature")
        self.assertEqual(result["head_sha"], self.head)
        self.assertEqual(result["head_branch"], "published/feature")
        self.assertEqual(result["compare_url"], "https://github.com/owner/engine/compare/main...published%2Ffeature?expand=1")

    def test_prepare_revision_cannot_inject_git_options(self):
        with self.assertRaises(gate.GateError):
            gate.prepare_pr(self.repo, revision="--help")

    def test_prepare_multiple_push_urls_refused(self):
        self.git("remote", "set-url", "--add", "--push", "origin", "https://github.com/a/engine.git")
        self.git("remote", "set-url", "--add", "--push", "origin", "https://github.com/b/engine.git")
        with self.assertRaisesRegex(gate.GateError, "exactly one push URL"):
            gate.prepare_pr(self.repo)

    def test_prepare_accepts_explicit_push_destination_url(self):
        for url in ("https://github.com/fork/engine.git", "git@github.com:fork/engine.git",
                    "ssh://git@github.com/fork/engine.git"):
            with patch.object(gate, "_branch_sha", side_effect=[self.head, BASE]), \
                    patch.object(gate, "_open_prs", return_value=[]):
                result = gate.prepare_pr(self.repo, remote_name=url, target_repo="owner/engine")
            self.assertEqual(result["head_repo"], "fork/engine")
            self.assertEqual(result["remote_name"], url)
            self.assertTrue(result["pushed"])

    def test_hook_install_is_idempotent_and_handles_spaces(self):
        hook = gate.install_hook(self.repo)
        self.assertEqual(hook.read_text(), gate.HOOK)
        self.assertTrue(os.access(hook, os.X_OK))
        self.assertEqual(gate.install_hook(self.repo), hook)
        script = self.repo / "tools/ci/local-ci.py"
        script.parent.mkdir(parents=True)
        script.write_text("import sys\nassert sys.argv[1:]==['pre-push','origin','remote url']\nassert sys.stdin.read()=='updates'\n")
        subprocess.run([str(hook), "origin", "remote url"], cwd=self.repo,
                       input="updates", text=True, check=True, env=gate._environment())

    def test_hook_install_preserves_existing_hook(self):
        hook = self.repo / ".git/hooks/pre-push"
        hook.write_text("owner's hook")
        with self.assertRaisesRegex(gate.GateError, "already exists"):
            gate.install_hook(self.repo)
        self.assertEqual(hook.read_text(), "owner's hook")

    def test_hook_install_preserves_core_hooks_path(self):
        self.git("config", "core.hooksPath", "custom-hooks")
        with self.assertRaisesRegex(gate.GateError, "core.hooksPath"):
            gate.install_hook(self.repo)
        self.assertFalse((self.repo / "custom-hooks").exists())

    def test_hook_install_rejects_symlinks(self):
        target = self.repo / "owner-hook"
        target.write_text(gate.HOOK)
        (self.repo / ".git/hooks/pre-push").symlink_to(target)
        with self.assertRaises(gate.GateError):
            gate.install_hook(self.repo)

    def test_inherited_git_environment_cannot_redirect_checkout(self):
        with patch.dict(os.environ, {"GIT_DIR": "/does/not/exist", "GIT_INDEX_FILE": "/does/not/exist",
                                     "GIT_WORK_TREE": "/does/not/exist"}):
            self.assertEqual(gate._git(self.repo, "rev-parse", "HEAD"), self.head)


if __name__ == "__main__":
    unittest.main()
