"""Local workflow verification and saved-result gate.

[CHOSEN, not recovered] Developer tooling for
https://github.com/kr4uzi/OpenBF2/issues/47; no engine compatibility changes.
"""
from __future__ import annotations

import argparse
from contextlib import contextmanager
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import signal
import subprocess
import sys
import uuid
import webbrowser

import act_runner
import git_gate


SCHEMA = 1
SCOPE = "linux-build-installers-packages"
UNVERIFIED = ["Windows execution", "macOS execution", "production signing and notarization",
              "GitHub runner allocation and service policy", "gameplay"]
SDK_PATHS = ["tools/linux/release/Containerfile", "tools/linux/release/build-sdk.py",
             "tools/linux/release/sdk-image.py", "tools/linux/release/sdk-sources.json"]


class LocalCIError(RuntimeError):
    pass


@contextmanager
def interruption_handlers():
    """Unwind runner cleanup for terminal and task-manager cancellation."""
    signals = [signal.SIGINT, signal.SIGTERM]
    if hasattr(signal, "SIGHUP"):
        signals.append(signal.SIGHUP)
    previous = {number: signal.getsignal(number) for number in signals}

    def interrupted(number, frame):
        # A second signal must not interrupt process-group/container cleanup.
        for watched in signals:
            signal.signal(watched, signal.SIG_IGN)
        raise KeyboardInterrupt("Interrupted by " + signal.Signals(number).name)

    try:
        for number in signals:
            signal.signal(number, interrupted)
        yield
    finally:
        for number, handler in previous.items():
            signal.signal(number, handler)


def git_env():
    # pre-push exports repository variables which must not leak into the snapshot.
    env = {key: value for key, value in os.environ.items()
           if not key.startswith("GIT_")}
    env.update(GIT_TERMINAL_PROMPT="0", GIT_CONFIG_NOSYSTEM="1")
    return env


def git(repo, *args, env=None, check=True):
    result = subprocess.run(["git", "-C", str(repo), *args], env=env or git_env(),
                            text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if check and result.returncode:
        raise LocalCIError(result.stderr.strip() or result.stdout.strip() or "Git command failed")
    return result.stdout.strip()


def repository():
    return Path(git(Path.cwd(), "rev-parse", "--show-toplevel")).resolve()


def require_clean(repo):
    if git(repo, "status", "--porcelain", "--untracked-files=normal"):
        raise LocalCIError("Commit or set aside working-tree changes before verification. "
                           "Only committed inputs can approve a push or PR.")


def tools_digest(repo):
    digest = hashlib.sha256()
    for path in sorted((repo / "tools/ci").rglob("*")):
        if path.is_file() and "__pycache__" not in path.parts and path.suffix != ".pyc":
            digest.update(path.relative_to(repo).as_posix().encode() + b"\0")
            digest.update(path.read_bytes())
    return digest.hexdigest()


def identity(repo, context):
    fields = ("head_sha", "base_sha", "base_ref", "target_repo", "kind")
    missing = [key for key in fields if not context.get(key)]
    if missing:
        raise LocalCIError("Incomplete verification context: " + ", ".join(missing))
    return {"schema": SCHEMA, "scope": SCOPE,
            "context": {key: context[key] for key in fields},
            "tools_sha256": tools_digest(repo), "runner": act_runner.config_identity(repo)}


def key_for(value):
    return hashlib.sha256(json.dumps(value, sort_keys=True, separators=(",", ":")).encode()).hexdigest()


def output_root(repo):
    return repo / "out/local-ci"


def saved_result(repo, expected):
    path = output_root(repo) / "results" / (key_for(expected) + ".json")
    try:
        result = json.loads(path.read_text())
    except (OSError, ValueError):
        return None
    if (result.get("identity") != expected or result.get("status") != "passed"
            or result.get("scope") != SCOPE
            or not result.get("tested_sha")
            or result.get("evidence", {}).get("status") != "passed"
            or result["evidence"].get("engine_revision") != result["tested_sha"]
            or (result.get("fresh_sdk_required") and not result.get("fresh_sdk"))):
        return None
    return result


def write_json(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_name(path.name + "." + uuid.uuid4().hex + ".tmp")
    temporary.write_text(json.dumps(value, indent=2, sort_keys=True) + "\n")
    os.replace(temporary, path)


@contextmanager
def run_lock(root):
    if os.name != "posix":
        raise LocalCIError("The local workflow runner currently requires Linux x86-64.")
    import fcntl
    root.mkdir(parents=True, exist_ok=True)
    with (root / "run.lock").open("a") as lock:
        try:
            fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError as exc:
            raise LocalCIError("Another local CI run is already active in this checkout.") from exc
        yield


def freeze_source(repo, destination, context):
    """Create an independent checkout and deterministic PR merge without moving user refs."""
    result = subprocess.run(["git", "clone", "--no-checkout", "--no-hardlinks", "--",
                             str(repo), str(destination)], env=git_env(),
                            text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if result.returncode:
        raise LocalCIError("Cannot create frozen checkout: " + result.stderr.strip())
    origin = "https://github.com/" + context["target_repo"] + ".git"
    git(destination, "remote", "set-url", "origin", origin)
    for revision in (context["head_sha"], context["base_sha"]):
        if not git(destination, "cat-file", "-t", revision, check=False):
            git(destination, "-c", "core.hooksPath=/dev/null", "fetch", "--no-tags", "origin", revision)
    git(destination, "-c", "core.hooksPath=/dev/null", "checkout", "--detach", context["head_sha"])
    sdk_changed = bool(git(destination, "diff", "--name-only", context["base_sha"],
                           context["head_sha"], "--", *SDK_PATHS))
    if context["kind"] == "pr" and context["base_sha"] != context["head_sha"]:
        git(destination, "-c", "core.hooksPath=/dev/null", "checkout", "--detach", context["base_sha"])
        env = git_env()
        env.update(GIT_AUTHOR_NAME="OpenBF2 local CI", GIT_AUTHOR_EMAIL="local-ci@invalid",
                   GIT_COMMITTER_NAME="OpenBF2 local CI", GIT_COMMITTER_EMAIL="local-ci@invalid",
                   GIT_AUTHOR_DATE="2000-01-01T00:00:00Z", GIT_COMMITTER_DATE="2000-01-01T00:00:00Z")
        try:
            git(destination, "-c", "core.hooksPath=/dev/null", "-c", "commit.gpgsign=false",
                "merge", "--no-ff", "--no-edit", "-m", "Local CI integration check",
                context["head_sha"], env=env)
        except LocalCIError as exc:
            raise LocalCIError("Cannot merge the proposed commit with the target branch in the "
                               "isolated checkout. Resolve the integration conflict first.\n" + str(exc)) from exc
    tested = git(destination, "rev-parse", "HEAD")
    git(destination, "diff", "--check", context["base_sha"], tested)
    return {"tested_sha": tested, "tree_sha": git(destination, "rev-parse", "HEAD^{tree}"),
            "fresh_sdk_required": sdk_changed}


def new_run_dir(repo, prefix="run"):
    stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    path = output_root(repo) / "runs" / (prefix + "-" + stamp + "-" + uuid.uuid4().hex[:8])
    path.mkdir(parents=True)
    return path


def render_report(result):
    head = result.get("identity", {}).get("context", {}).get("head_sha", "unknown")
    lines = [f"Local CI: {result['status']} ({head[:12]})", f"Scope: {SCOPE}"]
    if result.get("cached"):
        lines.append("Using the saved result for these exact inputs.")
    if result.get("tested_sha"):
        lines.append("Tested revision: " + result["tested_sha"])
    if result.get("fresh_sdk"):
        lines.append("SDK: cold build requested" + (" (recipe changed)" if result.get("fresh_sdk_required") else ""))
    if result.get("error"):
        lines.append("Reason: " + result["error"])
    lines.append("Unverified: " + "; ".join(result.get("unverified", UNVERIFIED)))
    if result.get("directory"):
        lines.append("Logs and report: " + result["directory"])
    return "\n".join(lines)


def verify(repo, context, *, fresh_sdk=False):
    require_clean(repo)
    expected = identity(repo, context)
    root = output_root(repo)
    with run_lock(root):
        prior = saved_result(repo, expected)
        if prior and (not fresh_sdk or prior.get("fresh_sdk")):
            return dict(prior, cached=True)
        directory = new_run_dir(repo)
        print("Verifying " + context["head_sha"][:12] + " against " + context["base_sha"][:12]
              + ". Logs: " + str(directory), flush=True)
        result = {"schema": SCHEMA, "scope": SCOPE, "status": "running", "identity": expected,
                  "directory": str(directory), "unverified": UNVERIFIED,
                  "started": datetime.now(timezone.utc).isoformat()}
        write_json(directory / "report.json", result)
        result_path = root / "results" / (key_for(expected) + ".json")
        # An explicit recheck supersedes earlier evidence, including a failed cold build.
        write_json(result_path, result)
        try:
            result.update(freeze_source(repo, directory / "source", context))
            result["fresh_sdk"] = fresh_sdk or result["fresh_sdk_required"]
            write_json(directory / "report.json", result)
            evidence = act_runner.run_workflow(directory / "source", directory / "workflow",
                                               root / "cache", fresh_sdk=result["fresh_sdk"])
            if evidence.get("status") != "passed" or evidence.get("engine_revision") != result["tested_sha"]:
                raise LocalCIError("The workflow did not return successful evidence for the frozen revision.")
            if identity(repo, context) != expected:
                raise LocalCIError("Local verification tooling or runner configuration changed during the run.")
            result.update(status="passed", evidence=evidence)
        except (Exception, KeyboardInterrupt) as exc:
            result.update(status="interrupted" if isinstance(exc, KeyboardInterrupt) else "failed",
                          error=str(exc) or "Interrupted by user")
        result["finished"] = datetime.now(timezone.utc).isoformat()
        write_json(directory / "report.json", result)
        (directory / "report.txt").write_text(render_report(result) + "\n")
        write_json(result_path, result)
        return result


def context_for(repo, args):
    return git_gate.prepare_pr(repo, target_repo=args.target_repo, base_ref=args.base,
                               remote_name=args.remote, branch=args.branch,
                               revision=args.revision, head_branch=args.head_branch)


def prepare_pr(repo, args):
    require_clean(repo)
    context = context_for(repo, args)
    if context["kind"] != "pr":
        raise LocalCIError("Create a feature branch before preparing a pull request.")
    result = verify(repo, context, fresh_sdk=args.fresh_sdk)
    print(render_report(result))
    if result["status"] != "passed":
        return 1
    current = context_for(repo, args)
    if identity(repo, current) != result["identity"]:
        raise LocalCIError("The branch or target revision changed during verification. Run prepare-pr again.")
    require_clean(repo)
    if not current.get("pushed"):
        import shlex
        command = shlex.join(["git", "push", current["remote_name"],
                              current["head_sha"] + ":refs/heads/" + current["head_branch"]])
        raise LocalCIError("The remote branch does not contain the tested commit. Run:\n  "
                           + command + "\nThen run prepare-pr again; the saved checks will be reused.")
    url = current["compare_url"]
    print("Review and submit the PR yourself: " + url)
    if not args.no_browser and not webbrowser.open(url):
        print("Could not open a browser. Use the link above.", file=sys.stderr)
    return 0


def guard_push(repo, remote, url, lines):
    contexts = git_gate.protected_pushes(repo, remote, url, lines)
    failures = []
    for context in contexts:
        if not saved_result(repo, identity(repo, context)):
            failures.append(context)
    if failures:
        import shlex
        commands = []
        for context in failures:
            command = ["python3", "tools/ci/local-ci.py", "run", "--revision", context["head_sha"],
                       "--head-branch", context["head_branch"],
                       "--target-repo", context["target_repo"], "--base", context["base_ref"],
                       "--remote", remote]
            commands.append("  " + shlex.join(command) + "  # " + context["head_sha"][:12])
        raise LocalCIError("Push stopped: these CI-triggering commits lack matching local verification.\n"
                           + "\n".join(commands) + "\nNo build was started by the hook.")
    print("Local CI guard: " + ("matching verification found." if contexts else "no CI-triggering branch update."))
    return 0


def parser():
    result = argparse.ArgumentParser(description="Run Linux GitHub workflow checks locally before a PR.")
    commands = result.add_subparsers(dest="command", required=True)
    descriptions = {"run": "Verify a committed revision using the Linux workflow",
                    "status": "Show whether matching verification is available",
                    "prepare-pr": "Verify and open the PR form for human submission"}
    for name in ("run", "status", "prepare-pr"):
        command = commands.add_parser(name, help=descriptions[name])
        command.add_argument("--branch", help="Local branch to verify (default: current branch)")
        command.add_argument("--revision", help="Exact local commit to verify, including a pre-push refspec")
        command.add_argument("--head-branch", help="Remote branch name when it differs from the local branch")
        command.add_argument("--target-repo", help="Target GitHub owner/repository (default: push remote)")
        command.add_argument("--base", default="main", help="Target branch (default: main)")
        command.add_argument("--remote", help="Push remote (default: configured branch remote or origin)")
        if name != "status":
            command.add_argument("--fresh-sdk", action="store_true", help="Require a cold SDK build")
        if name == "prepare-pr":
            command.add_argument("--no-browser", action="store_true", help="Print the PR form link only")
    commands.add_parser("install-hook", help="Install the optional fast pre-push guard")
    commands.add_parser("preflight", help="Test the local runner and artifact transfers without an engine build")
    hook = commands.add_parser("pre-push", help="Internal Git hook entry point; does not build")
    hook.add_argument("remote")
    hook.add_argument("url")
    return result


def execute(args):
    try:
        repo = repository()
        if args.command == "install-hook":
            print("Installed optional guard: " + str(git_gate.install_hook(repo)))
            return 0
        if args.command == "pre-push":
            return guard_push(repo, args.remote, args.url, sys.stdin.read().splitlines())
        if args.command == "prepare-pr":
            return prepare_pr(repo, args)
        if args.command == "preflight":
            with run_lock(output_root(repo)):
                directory = new_run_dir(repo, "preflight")
                evidence = act_runner.run_preflight(repo, directory, output_root(repo) / "cache")
                write_json(directory / "report.json", evidence)
                print("Runner preflight: " + evidence.get("status", "unknown") + ". Logs: " + str(directory))
                print("This preflight does not approve a push or PR.")
                return 0 if evidence.get("status") == "passed" else 1
        if args.command == "run":
            require_clean(repo)
        context = context_for(repo, args)
        if args.command == "status":
            saved = saved_result(repo, identity(repo, context))
            if saved:
                print(render_report(dict(saved, cached=True)))
                return 0
            print("Local CI: no matching successful result. Run python3 tools/ci/local-ci.py run.")
            print("Required local scope: " + SCOPE + "; other platforms/signing remain unverified.")
            return 1
        result = verify(repo, context, fresh_sdk=args.fresh_sdk)
        print(render_report(result))
        return 0 if result["status"] == "passed" else 1
    except (LocalCIError, git_gate.GateError, act_runner.RunnerError,
            OSError, ValueError, subprocess.SubprocessError) as exc:
        print("Local CI: " + str(exc), file=sys.stderr)
        return 1
    except KeyboardInterrupt:
        print("Local CI interrupted; no new passing result was saved.", file=sys.stderr)
        return 130


def main(argv=None):
    args = parser().parse_args(argv)
    with interruption_handlers():
        return execute(args)
