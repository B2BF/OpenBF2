#!/usr/bin/env python3
"""Synthetic Git/runner fixtures for the local CI gate; no engine or hosted CI."""
from pathlib import Path
import json
import os
import signal
import subprocess
import sys
import tempfile
import time
import unittest
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools/ci"))
import local_ci as ci


class LocalCITest(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="openbf2-local-ci-")
        self.addCleanup(self.temporary.cleanup)
        self.repo = Path(self.temporary.name) / "repo with spaces"
        self.repo.mkdir()
        self.git("init", "-b", "main")
        self.git("config", "user.name", "Fixture")
        self.git("config", "user.email", "fixture@invalid")
        self.git("config", "commit.gpgsign", "false")
        (self.repo / ".gitignore").write_text("out/\n")
        (self.repo / "tools/ci").mkdir(parents=True)
        (self.repo / "tools/ci/runner-config.json").write_text('{"version": 1}\n')
        (self.repo / "file.txt").write_text("base\n")
        self.commit("base")
        self.base = self.git("rev-parse", "HEAD")
        self.git("checkout", "-b", "feature")
        (self.repo / "feature.txt").write_text("feature\n")
        self.commit("feature")
        self.head = self.git("rev-parse", "HEAD")
        self.context = dict(head_sha=self.head, base_sha=self.base, base_ref="main",
                            target_repo="example/engine", head_repo="example/engine",
                            head_branch="feature", kind="pr", pushed=True,
                            remote_name="origin",
                            compare_url="https://github.com/example/engine/compare/main...feature")
        self.addCleanup(mock.patch.stopall)
        mock.patch.object(ci.act_runner, "config_identity", return_value={"version": "fixture"}).start()

    def git(self, *args):
        return ci.git(self.repo, *args)

    def commit(self, message):
        self.git("add", ".")
        self.git("commit", "-m", message)

    def passing_runner(self, source, output, cache, fresh_sdk=False):
        return {"status": "passed", "engine_revision": ci.git(source, "rev-parse", "HEAD"),
                "checks": {"fixture": "passed"}}

    def verify_pass(self):
        with mock.patch.object(ci.act_runner, "run_workflow", side_effect=self.passing_runner) as run:
            result = ci.verify(self.repo, self.context)
        self.assertEqual("passed", result["status"], result)
        self.assertEqual(1, run.call_count)
        return result

    def test_snapshot_merges_target_and_preserves_parent_checkout(self):
        self.git("checkout", "main")
        (self.repo / "target.txt").write_text("target advancement\n")
        self.commit("target advanced")
        self.context["base_sha"] = self.git("rev-parse", "HEAD")
        self.git("checkout", "feature")
        result = self.verify_pass()
        source = Path(result["directory"]) / "source"
        self.assertTrue((source / "target.txt").exists())
        self.assertTrue((source / "feature.txt").exists())
        self.assertEqual(self.head, self.git("rev-parse", "HEAD"))
        self.assertEqual("", self.git("status", "--porcelain"))
        self.assertFalse((source / ".git/objects/info/alternates").exists())

    def test_cache_requires_same_commit_target_and_configuration(self):
        self.verify_pass()
        with mock.patch.object(ci.act_runner, "run_workflow") as run:
            result = ci.verify(self.repo, self.context)
        self.assertTrue(result["cached"])
        run.assert_not_called()
        self.assertIsNone(ci.saved_result(self.repo, ci.identity(
            self.repo, dict(self.context, base_sha="a" * 40))))
        (self.repo / "tools/ci/runner-config.json").write_text('{"version": 2}\n')
        self.assertIsNone(ci.saved_result(self.repo, ci.identity(self.repo, self.context)))

    def test_failure_and_interruption_do_not_save_success(self):
        for failure in (RuntimeError("fixture failure"), KeyboardInterrupt()):
            with self.subTest(failure=failure), mock.patch.object(ci.act_runner, "run_workflow", side_effect=failure):
                result = ci.verify(self.repo, self.context)
            self.assertIn(result["status"], ("failed", "interrupted"))
            self.assertIsNone(ci.saved_result(self.repo, ci.identity(self.repo, self.context)))
            self.assertEqual(result["status"], json.loads(
                (Path(result["directory"]) / "report.json").read_text())["status"])

    def test_runner_wrong_revision_cannot_pass(self):
        with mock.patch.object(ci.act_runner, "run_workflow", return_value={"status": "passed", "engine_revision": self.head}):
            result = ci.verify(self.repo, self.context)
        self.assertEqual("failed", result["status"])

    def test_failed_explicit_recheck_invalidates_earlier_pass(self):
        self.verify_pass()
        with mock.patch.object(ci.act_runner, "run_workflow", side_effect=RuntimeError("cold SDK failed")):
            result = ci.verify(self.repo, self.context, fresh_sdk=True)
        self.assertEqual("failed", result["status"])
        self.assertIsNone(ci.saved_result(self.repo, ci.identity(self.repo, self.context)))

    def test_changed_live_head_does_not_change_frozen_input(self):
        def run(source, output, cache, fresh_sdk=False):
            frozen = ci.git(source, "rev-parse", "HEAD")
            (self.repo / "later.txt").write_text("created while running\n")
            self.commit("later")
            self.assertEqual(frozen, ci.git(source, "rev-parse", "HEAD"))
            self.assertFalse((source / "later.txt").exists())
            return self.passing_runner(source, output, cache, fresh_sdk)
        with mock.patch.object(ci.act_runner, "run_workflow", side_effect=run):
            result = ci.verify(self.repo, self.context)
        self.assertEqual("passed", result["status"])
        new_context = dict(self.context, head_sha=self.git("rev-parse", "HEAD"))
        self.assertIsNone(ci.saved_result(self.repo, ci.identity(self.repo, new_context)))

    def test_dirty_inputs_refused_before_runner(self):
        (self.repo / "file.txt").write_text("uncommitted\n")
        with mock.patch.object(ci.act_runner, "run_workflow") as run, self.assertRaises(ci.LocalCIError):
            ci.verify(self.repo, self.context)
        run.assert_not_called()

    def test_changed_sdk_requires_cold_run(self):
        sdk = self.repo / ci.SDK_PATHS[0]
        sdk.parent.mkdir(parents=True)
        sdk.write_text("fixture SDK recipe\n")
        self.commit("SDK changed")
        self.context["head_sha"] = self.git("rev-parse", "HEAD")
        with mock.patch.object(ci.act_runner, "run_workflow", side_effect=self.passing_runner) as run:
            result = ci.verify(self.repo, self.context)
        self.assertTrue(result["fresh_sdk_required"])
        self.assertTrue(run.call_args.kwargs["fresh_sdk"])

    def test_conflict_stops_before_runner(self):
        (self.repo / "file.txt").write_text("feature conflict\n")
        self.commit("feature conflict")
        self.context["head_sha"] = self.git("rev-parse", "HEAD")
        self.git("checkout", "main")
        (self.repo / "file.txt").write_text("target conflict\n")
        self.commit("target conflict")
        self.context["base_sha"] = self.git("rev-parse", "HEAD")
        self.git("checkout", "feature")
        with mock.patch.object(ci.act_runner, "run_workflow") as run:
            result = ci.verify(self.repo, self.context)
        self.assertEqual("failed", result["status"])
        self.assertIn("integration conflict", result["error"])
        run.assert_not_called()

    def test_guard_checks_every_proposed_commit_without_building(self):
        self.verify_pass()
        second = dict(self.context, head_sha="b" * 40, head_branch="another")
        with mock.patch.object(ci.git_gate, "protected_pushes", return_value=[self.context, second]), \
                mock.patch.object(ci.act_runner, "run_workflow") as run, self.assertRaises(ci.LocalCIError):
            ci.guard_push(self.repo, "origin", "url", ["synthetic push input"])
        run.assert_not_called()

    def test_backup_push_does_not_need_result(self):
        with mock.patch.object(ci.git_gate, "protected_pushes", return_value=[]):
            self.assertEqual(0, ci.guard_push(self.repo, "origin", "url", []))

    def test_prepare_pr_only_opens_matching_pushed_commit(self):
        args = ci.parser().parse_args(["prepare-pr"])
        with mock.patch.object(ci.git_gate, "prepare_pr", return_value=self.context), \
                mock.patch.object(ci.act_runner, "run_workflow", side_effect=self.passing_runner), \
                mock.patch.object(ci.webbrowser, "open", return_value=True) as browser:
            self.assertEqual(0, ci.prepare_pr(self.repo, args))
        browser.assert_called_once_with(self.context["compare_url"])
        with mock.patch.object(ci.git_gate, "prepare_pr", return_value=dict(self.context, pushed=False)), \
                mock.patch.object(ci.webbrowser, "open") as browser, self.assertRaises(ci.LocalCIError):
            ci.prepare_pr(self.repo, args)
        browser.assert_not_called()

    def test_prepare_pr_rechecks_target_after_validation(self):
        args = ci.parser().parse_args(["prepare-pr"])
        advanced = dict(self.context, base_sha="d" * 40)
        with mock.patch.object(ci.git_gate, "prepare_pr", side_effect=[self.context, advanced]), \
                mock.patch.object(ci.act_runner, "run_workflow", side_effect=self.passing_runner), \
                mock.patch.object(ci.webbrowser, "open") as browser, self.assertRaises(ci.LocalCIError):
            ci.prepare_pr(self.repo, args)
        browser.assert_not_called()

    def test_unpushed_handoff_uses_exact_revision_remote_and_refspec(self):
        args = ci.parser().parse_args(["prepare-pr"])
        context = dict(self.context, remote_name="fork", head_branch="published/name", pushed=False)
        with mock.patch.object(ci.git_gate, "prepare_pr", return_value=context), \
                mock.patch.object(ci.act_runner, "run_workflow", side_effect=self.passing_runner), \
                self.assertRaises(ci.LocalCIError) as error:
            ci.prepare_pr(self.repo, args)
        self.assertIn("git push fork " + self.head + ":refs/heads/published/name", str(error.exception))

    def test_inherited_hook_environment_does_not_redirect_clone(self):
        with mock.patch.dict(os.environ, {"GIT_DIR": str(self.repo / ".git"), "GIT_WORK_TREE": str(self.repo)}):
            result = self.verify_pass()
        self.assertEqual(self.head, self.git("rev-parse", "HEAD"))
        self.assertNotEqual(self.head, result["tested_sha"])

    @unittest.skipUnless(os.name == "posix", "POSIX process-group cancellation")
    def test_sigterm_unwinds_detached_runner_process_group(self):
        root = Path(self.temporary.name)
        record, log = root / "child-pid", root / "child.log"
        child_code = ("import os,time; from pathlib import Path; "
                      f"Path({str(record)!r}).write_text(str(os.getpid())); time.sleep(60)")
        code = ("import sys; from pathlib import Path; "
                f"sys.path.insert(0, {str(Path(ci.__file__).parent)!r}); import local_ci,act_runner\n"
                "with local_ci.interruption_handlers():\n"
                f"    act_runner._logged([sys.executable,'-c',{child_code!r}], Path({str(log)!r}), timeout=90)\n")
        process = subprocess.Popen([sys.executable, "-c", code], stdout=subprocess.DEVNULL,
                                   stderr=subprocess.DEVNULL)
        child_pid = None
        try:
            deadline = time.monotonic() + 10
            while not record.exists() and time.monotonic() < deadline:
                if process.poll() is not None:
                    self.fail("Fixture wrapper exited before starting its child")
                time.sleep(0.02)
            self.assertTrue(record.exists(), "Fixture child did not start")
            child_pid = int(record.read_text())
            process.terminate()
            self.assertNotEqual(0, process.wait(timeout=15))
            with self.assertRaises(ProcessLookupError):
                os.kill(child_pid, 0)
        finally:
            if process.poll() is None:
                process.kill()
                process.wait()
            if child_pid is not None:
                try:
                    os.killpg(child_pid, signal.SIGKILL)
                except ProcessLookupError:
                    pass


if __name__ == "__main__":
    unittest.main()
