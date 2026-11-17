"""Git and GitHub handoff for local verification; never publishes changes.

[CHOSEN, not recovered] https://github.com/kr4uzi/OpenBF2/issues/47
"""

from __future__ import annotations

import json
import os
from pathlib import Path
import re
import subprocess
from urllib.parse import quote, urlencode, urlparse


class GateError(RuntimeError):
    """The local gate could not establish that an operation is safe to continue."""


HOOK = '''#!/bin/sh
# OpenBF2 local CI guard; issue #47. Builds are explicitly requested, never run here.
root=$(git rev-parse --show-toplevel) || exit 1
exec python3 "$root/tools/ci/local-ci.py" pre-push "$@"
'''


def _environment():
    # A hook exports its repository environment. Do not let that redirect commands
    # which inspect another checkout, or let an inherited index affect validation.
    env = {key: value for key, value in os.environ.items() if not key.startswith("GIT_")}
    env.update(GH_PROMPT_DISABLED="1", GH_PAGER="cat")
    return env


def _command(repo, args, allow_failure=False):
    try:
        result = subprocess.run(args, cwd=repo, env=_environment(), text=True,
                                stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=45)
    except (OSError, subprocess.TimeoutExpired) as exc:
        raise GateError(f"Cannot run {args[0]}: {exc}") from exc
    if result.returncode and not allow_failure:
        detail = result.stderr.strip()[-500:]
        raise GateError(f"{args[0]} failed: {detail or 'no diagnostic returned'}")
    return result


def _git(repo, *args, optional=False):
    result = _command(repo, ["git", *args], allow_failure=optional)
    return result.stdout.strip() if result.returncode == 0 else None


def _api(repo, endpoint):
    result = _command(repo, ["gh", "api", "--method", "GET", endpoint])
    try:
        return json.loads(result.stdout)
    except ValueError as exc:
        raise GateError("GitHub returned an invalid response; the push was not verified.") from exc


def _repository_name(value):
    if not isinstance(value, str) or not re.fullmatch(r"[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+", value):
        raise GateError("Expected a GitHub repository in OWNER/REPOSITORY form.")
    return value


def repository_from_url(value):
    """Resolve common GitHub HTTPS and SSH remote URLs without displaying credentials."""
    if value.startswith("git@github.com:"):
        path = value[len("git@github.com:"):]
    else:
        parsed = urlparse(value)
        if parsed.hostname != "github.com" or parsed.scheme not in ("https", "ssh"):
            raise GateError("The local CI guard requires a github.com push remote.")
        if parsed.password or (parsed.scheme == "https" and parsed.username) or parsed.query or parsed.fragment:
            raise GateError("Use a GitHub remote URL without embedded credentials or query parameters.")
        path = parsed.path.lstrip("/")
    path = path.rstrip("/")
    if path.endswith(".git"):
        path = path[:-4]
    return _repository_name(path)


def _sha(value):
    if not isinstance(value, str) or not re.fullmatch(r"[0-9a-fA-F]{40}|[0-9a-fA-F]{64}", value):
        raise GateError("Git or GitHub returned an invalid commit identifier.")
    return value.lower()


def _branch_sha(repo, repository, branch, missing_ok=False):
    endpoint = f"repos/{_repository_name(repository)}/git/ref/heads/{quote(branch, safe='')}"
    result = _command(repo, ["gh", "api", "--method", "GET", endpoint], allow_failure=True)
    if result.returncode:
        if missing_ok and "HTTP 404" in result.stderr:
            return None
        raise GateError("Cannot read the remote branch from GitHub; retry after checking connectivity and gh authentication.")
    try:
        return _sha(json.loads(result.stdout)["object"]["sha"])
    except (ValueError, KeyError, TypeError) as exc:
        raise GateError("GitHub returned an invalid branch response.") from exc


def _associated_prs(repo, head_repository, branch):
    """Read all open PRs headed by this ref, including PRs against another fork."""
    query = """query($owner:String!,$name:String!,$ref:String!,$cursor:String) {
      repository(owner:$owner,name:$name) {
        ref(qualifiedName:$ref) {
          associatedPullRequests(states:OPEN,first:100,after:$cursor) {
            nodes { url headRefName headRepository { nameWithOwner }
                    baseRefName baseRepository { nameWithOwner } }
            pageInfo { hasNextPage endCursor }
          }
        }
      }
    }"""
    owner, name = _repository_name(head_repository).split("/")
    found, cursors = [], set()
    cursor = None
    while True:
        args = ["gh", "api", "graphql", "-f", "query=" + query, "-f", "owner=" + owner,
                "-f", "name=" + name, "-f", "ref=refs/heads/" + branch]
        if cursor:
            args.extend(["-f", "cursor=" + cursor])
        result = _command(repo, args)
        try:
            response = json.loads(result.stdout)
            if response.get("errors"):
                raise GateError("GitHub could not resolve all associated PRs; the push was not verified.")
            repository = response["data"]["repository"]
            if repository is None:
                raise GateError("GitHub could not read the push repository; check gh authentication.")
            ref = repository["ref"]
            if ref is None:
                return []  # A branch not yet pushed cannot head an existing PR.
            connection = ref["associatedPullRequests"]
            for entry in connection["nodes"]:
                if (entry["headRepository"]["nameWithOwner"].lower() == head_repository.lower()
                        and entry["headRefName"] == branch):
                    found.append({"head": {"repo": {"full_name": head_repository}, "ref": branch},
                                  "base": {"repo": {"full_name": entry["baseRepository"]["nameWithOwner"]},
                                           "ref": entry["baseRefName"]}, "html_url": entry["url"]})
            page = connection["pageInfo"]
            if page["hasNextPage"] is False:
                return found
            cursor = page["endCursor"]
            if page["hasNextPage"] is not True or not isinstance(cursor, str) or not cursor or cursor in cursors:
                raise GateError("GitHub returned invalid PR pagination; the push was not verified.")
            cursors.add(cursor)
        except (ValueError, KeyError, TypeError, AttributeError) as exc:
            raise GateError("GitHub returned incomplete associated-PR information.") from exc


def _open_prs(repo, targets, head_repository, branch):
    owner = head_repository.split("/", 1)[0]
    found = []
    for target in targets:
        page = 1
        while True:
            query = urlencode({"state": "open", "head": f"{owner}:{branch}", "per_page": 100, "page": page})
            entries = _api(repo, f"repos/{_repository_name(target)}/pulls?{query}")
            if not isinstance(entries, list):
                raise GateError("GitHub returned an invalid pull-request list.")
            for entry in entries:
                try:
                    head = entry["head"]
                    if (head["repo"]["full_name"].lower() == head_repository.lower()
                            and head["ref"] == branch):
                        found.append(entry)
                except (KeyError, TypeError):
                    raise GateError("GitHub returned incomplete pull-request information.")
            if len(entries) < 100:
                break
            page += 1
    return found


def _pr_context(repo, pr, head_sha):
    try:
        target = _repository_name(pr["base"]["repo"]["full_name"])
        base_ref = pr["base"]["ref"]
        return {"kind": "pr", "head_sha": _sha(head_sha),
                "base_sha": _branch_sha(repo, target, base_ref), "base_ref": base_ref,
                "target_repo": target, "head_repo": _repository_name(pr["head"]["repo"]["full_name"]),
                "head_branch": pr["head"]["ref"], "pr_url": pr["html_url"]}
    except (KeyError, TypeError) as exc:
        raise GateError("GitHub returned incomplete pull-request information.") from exc


def protected_pushes(repo, remote_name, remote_url, stdin_lines):
    """Describe CI-triggering updates from pre-push stdin, using each proposed SHA.

    Backup branches, deletions and tags are not protected by the current CI triggers.
    GitHub errors fail closed because an absent PR cannot otherwise be established.
    """
    del remote_name  # The hook's actual destination URL is authoritative, including pushurl.
    updates = []
    for line in stdin_lines.splitlines() if isinstance(stdin_lines, str) else stdin_lines:
        if not line.strip():
            continue
        parts = line.split()
        if len(parts) != 4:
            raise GateError("Malformed pre-push input: expected four fields per ref update.")
        local_ref, local_sha, remote_ref, remote_sha = parts
        local_sha, remote_sha = _sha(local_sha), _sha(remote_sha)
        if set(local_sha) == {"0"} or local_sha == remote_sha or not remote_ref.startswith("refs/heads/"):
            continue
        if _git(repo, "check-ref-format", remote_ref, optional=True) is None:
            raise GateError("Malformed destination branch in pre-push input.")
        updates.append((local_ref, local_sha, remote_ref[len("refs/heads/"):]))
    if not updates:
        return []
    repository = repository_from_url(remote_url)
    protected = []
    for local_ref, head_sha, branch in updates:
        contexts = []
        if branch == "main":
            contexts.append({"kind": "main", "head_sha": head_sha,
                             "base_sha": _branch_sha(repo, repository, "main"),
                             "base_ref": "main", "target_repo": repository,
                             "head_repo": repository, "head_branch": branch})
        for pr in _associated_prs(repo, repository, branch):
            contexts.append(_pr_context(repo, pr, head_sha))
        for context in contexts:
            context["local_ref"] = local_ref
            context["local_branch"] = local_ref[len("refs/heads/"):] if local_ref.startswith("refs/heads/") else None
        protected.extend(contexts)
    return protected


def prepare_pr(repo, target_repo=None, base_ref="main", remote_name=None, branch=None,
               revision=None, head_branch=None):
    """Resolve committed local and remote inputs without pushing or opening a browser.

    The caller must verify the result, then resolve it again immediately before opening
    compare_url. A pushed=False result requires a separate human-requested push.
    """
    if branch is None:
        branch = _git(repo, "symbolic-ref", "--quiet", "--short", "HEAD", optional=True)
        if not branch and not (revision and head_branch):
            raise GateError("Select a local branch before preparing a pull request.")
    if branch and _git(repo, "check-ref-format", f"refs/heads/{branch}", optional=True) is None:
        raise GateError("Invalid local branch name.")
    head_branch = head_branch or branch
    if _git(repo, "check-ref-format", f"refs/heads/{head_branch}", optional=True) is None:
        raise GateError("Invalid pushed branch name.")
    source = revision if revision is not None else f"refs/heads/{branch}"
    head_sha = _sha(_git(repo, "rev-parse", "--verify", "--end-of-options", f"{source}^{{commit}}"))
    if remote_name is None:
        remote_name = (_git(repo, "config", "--get", f"branch.{branch}.pushRemote", optional=True)
                       or _git(repo, "config", "--get", "remote.pushDefault", optional=True)
                       or _git(repo, "config", "--get", f"branch.{branch}.remote", optional=True)
                       or "origin")
    if remote_name == ".":
        raise GateError("Select a GitHub push remote with --remote.")
    if "://" in remote_name or remote_name.startswith("git@"):
        urls = [remote_name]  # Git also accepts a destination URL instead of a remote name.
    else:
        urls = _git(repo, "remote", "get-url", "--push", "--all", remote_name).splitlines()
    if len(urls) != 1:
        raise GateError("PR preparation requires exactly one push URL for the selected remote.")
    head_repo = repository_from_url(urls[0])
    target_repo = _repository_name(target_repo or head_repo)
    if _git(repo, "check-ref-format", f"refs/heads/{base_ref}", optional=True) is None:
        raise GateError("Invalid target branch name.")
    remote_sha = _branch_sha(repo, head_repo, head_branch, missing_ok=True)
    base_sha = _branch_sha(repo, target_repo, base_ref)
    existing = [pr for pr in _open_prs(repo, [target_repo], head_repo, head_branch)
                if pr.get("base", {}).get("ref") == base_ref]
    if len(existing) > 1:
        raise GateError("Multiple open pull requests match this branch and target.")
    head_label = head_branch if head_repo.lower() == target_repo.lower() else f"{head_repo.split('/')[0]}:{head_branch}"
    compare = f"https://github.com/{target_repo}/compare/{quote(base_ref, safe='')}...{quote(head_label, safe='')}?expand=1"
    if existing:
        compare = existing[0]["html_url"]
    return {"kind": "main" if head_branch == base_ref == "main" and head_repo.lower() == target_repo.lower() else "pr",
            "head_sha": head_sha, "base_sha": base_sha, "base_ref": base_ref,
            "target_repo": target_repo, "head_repo": head_repo, "head_branch": head_branch,
            "remote_name": remote_name, "remote_sha": remote_sha, "pushed": remote_sha == head_sha,
            "compare_url": compare, "pr_url": existing[0]["html_url"] if existing else None}


def install_hook(repo):
    """Install the opt-in guard without overwriting existing hook configuration."""
    if _git(repo, "config", "--get", "core.hooksPath", optional=True) is not None:
        raise GateError("core.hooksPath is already configured; add the local-ci.py pre-push command to that hook manually.")
    hook = Path(_git(repo, "rev-parse", "--git-path", "hooks/pre-push"))
    if not hook.is_absolute():
        hook = Path(repo) / hook
    if hook.is_symlink() or (hook.exists() and (not hook.is_file() or hook.read_text() != HOOK)):
        raise GateError("A pre-push hook already exists; chain the local-ci.py pre-push command manually.")
    hook.parent.mkdir(parents=True, exist_ok=True)
    if not hook.exists():
        try:
            with hook.open("x") as output:
                output.write(HOOK)
        except FileExistsError as exc:
            raise GateError("A pre-push hook appeared during installation; it was not overwritten.") from exc
    hook.chmod(hook.stat().st_mode | 0o111)
    return hook
