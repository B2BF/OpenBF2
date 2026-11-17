# Local checks before a pull request

Run the Linux build and packaging workflow locally before spending GitHub Actions time.
This is developer tooling under [issue #47](https://github.com/kr4uzi/OpenBF2/issues/47),
not an engine behavior change.

The normal sequence is **commit, verify, push, prepare the PR**:

```sh
python3 tools/ci/local-ci.py run
git push -u origin your-branch
python3 tools/ci/local-ci.py prepare-pr
```

`prepare-pr` runs missing checks, verifies that the pushed branch still matches the
tested commit, and opens GitHub's PR form. You review and submit it. For an existing
PR it opens that PR instead. It never pushes, creates a PR, dispatches Actions or
publishes a release. Use `--no-browser` to print the link instead.

## What runs

The first version runs on Linux x86-64. It requires Python 3.10 or newer, Git, authenticated
GitHub CLI (`gh`), a supported container engine and enough disk for the SDK, engine,
installer and packages. The runner's pinned configuration lives in
`tools/ci/runner-config.json`; the first run downloads/builds its tools and container
image. Caches and all generated files stay under `out/local-ci/`.

Podman is the default container engine. Set `OPENBF2_CI_ENGINE=docker` to use a
local Docker daemon instead. Disposable job containers run with the privileges
needed for nested Podman; the host daemon socket is not mounted into them.

The tool invokes the actual CI entry workflow locally with `linux_only=true`:
the server and client jobs, graphical installer, package audits, and local artifact
uploads/downloads through the Linux package job. Production workflow definitions
are not replaced with a second build recipe. A small preflight runs before the
expensive build, using the same pinned checkout, artifact and cache actions. It
checks transfers between jobs, a cache miss followed by a hit, and normal/cold
nested container builds using a tiny executable. This tests the runner mechanics;
it does not establish that the full SDK recipe builds successfully.

Windows and macOS execution, production signing/notarization, GitHub service policy
and gameplay remain **unverified**. A local Linux pass cannot establish that those
paths work. No production credentials or release event are supplied to `act`.
Signing and platform configuration still need review and validation on suitable hosts.

```sh
# Check runner setup and artifact compatibility without building the engine.
python3 tools/ci/local-ci.py preflight

# Show the saved result for the current inputs; does not build anything.
python3 tools/ci/local-ci.py status

# Exercise the SDK recipe without its saved build cache.
python3 tools/ci/local-ci.py run --fresh-sdk
```

The first SDK cache miss can take substantially longer than later runs. Changing
the SDK recipe requires a successful cold build automatically. A preflight alone
never approves a push or PR.

## Frozen revisions and results

Verification requires committed inputs. It creates an independent checkout and
tests the proposed commit merged with the resolved target branch. Merge conflicts
stop locally. Direct `main` pushes test the proposed commit itself. The live branch
can advance during the build without changing the snapshot being tested.

Results are tied to the proposed commit, target revision, runner/tool configuration
and check scope. The setup revision and workflows come from the frozen source.
Changing any of those inputs requires new verification. A failed or interrupted
recheck supersedes an earlier pass for the same inputs.

Each invocation keeps a JSON report, readable summary and logs under
`out/local-ci/runs/`. A passing result means the required Linux workflow completed;
the report also lists the paths that were not verified. Reports are local developer
evidence, not signed attestations or GitHub required checks.

Interrupting verification stops its process group and owned job containers. An
interrupted run never supplies a passing result. Dependency caches are retained
so the next explicit run can reuse completed downloads and builds.

The default target is `main` in the selected push remote, normally `origin`.
For a PR into another repository or branch, pass the same selection to each command:

```sh
python3 tools/ci/local-ci.py run --target-repo OWNER/REPO --base TARGET --remote origin
python3 tools/ci/local-ci.py prepare-pr --target-repo OWNER/REPO --base TARGET --remote origin
```

`--branch NAME` selects a different local branch without checking it out.
`--revision SHA --head-branch NAME` verifies an exact commit intended for a differently
named remote branch, as suggested by the push guard when necessary.

## Optional push guard

```sh
python3 tools/ci/local-ci.py install-hook
```

Installation is explicit and local to this repository clone, including linked
worktrees. The checked-out branch must contain the tooling for its hook to run.
Existing hooks or
`core.hooksPath` configuration are never overwritten. To integrate with an existing
hook, invoke `python3 tools/ci/local-ci.py pre-push "$@"` from the repository root,
preserve Git's stdin ref list, and propagate a nonzero result.

The guard runs during Git pushes, including those made by VS Code. It checks every
proposed branch update using its actual SHA, not the checked-out `HEAD`. It guards
`main` and branches with an open PR. Ordinary backup pushes, branch deletions and
tag pushes do not trigger this repository's current CI and do not require results.

Missing or stale verification blocks the push and prints the exact command to run.
The hook never starts a build. GitHub lookup errors block the push because the hook
cannot establish whether it would start CI. Retry after restoring access to GitHub.

Creating a PR directly in GitHub bypasses local tooling. Hooks are also opt-in and
can be bypassed with Git's `--no-verify`; they prevent accidental unverified pushes,
not deliberate bypasses. Hosted CI remains the final cross-platform check.

The tooling's lightweight regression tests use temporary repositories and synthetic
inputs; they do not start hosted CI or compile the engine:

```sh
python3 -m unittest discover -s Code/Tests -p 'test_local_ci*.py'
```
