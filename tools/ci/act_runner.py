#!/usr/bin/env python3
"""Run frozen Linux workflow inputs locally; intentional tooling for OpenBF2 issue #47."""

from contextlib import contextmanager
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import shutil
import signal
import socket
import subprocess
import tarfile
import tempfile
import time
import urllib.request
import uuid
import zipfile


class RunnerError(RuntimeError):
    """A prerequisite or required local check failed; no passing evidence exists."""


RESOURCE_DIR = Path(__file__).resolve().parent
REQUIRED_ARTIFACTS = (
    "packages-linux-x64", "packages-linux-client", "release-linux", "release-linux-client",
)


def _sha(path):
    digest = hashlib.sha256()
    with Path(path).open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def config_identity(repo):
    """Return configuration inputs without starting containers or downloading tools."""
    repo = Path(repo)
    config = json.loads((RESOURCE_DIR / "runner-config.json").read_text())
    resources = [p for p in RESOURCE_DIR.rglob("*") if p.is_file()
                 and (p.name in ("act_runner.py", "runner-config.json", "Containerfile", "podman-local")
                      or "patches" in p.relative_to(RESOURCE_DIR).parts)]
    fixtures = Path(__file__).resolve().parents[2] / "Code/Tests/local_ci"
    resources += sorted(fixtures.glob("*")) if fixtures.is_dir() else []
    config["resource_hashes"] = {str(p.relative_to(RESOURCE_DIR.parents[1])): _sha(p)
                                 for p in sorted(resources) if p.is_file()}
    config["host"] = {"system": platform.system(), "machine": platform.machine()}
    config["container_engine"] = os.environ.get("OPENBF2_CI_ENGINE", "podman")
    config["workflow_hashes"] = {name: _sha(repo / name)
                                 for name in (".github/workflows/ci.yml", ".github/workflows/build.yml")
                                 if (repo / name).is_file()}
    return config


def _clean_env():
    # Do not forward host account tokens, signing credentials, or arbitrary act options.
    names = ("PATH", "HOME", "USER", "LOGNAME", "LANG", "LC_ALL", "TMPDIR",
             "XDG_RUNTIME_DIR", "DBUS_SESSION_BUS_ADDRESS", "SSL_CERT_FILE", "SSL_CERT_DIR")
    return {key: os.environ[key] for key in names if key in os.environ}


def _capture(args, *, cwd=None, env=None):
    result = subprocess.run([str(a) for a in args], cwd=cwd, env=env,
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, timeout=45)
    if result.returncode:
        raise RunnerError(f"{args[0]} failed: {result.stderr.strip() or result.stdout.strip()}")
    return result.stdout.strip()


def _logged(args, log, *, cwd=None, env=None, timeout=900):
    """Bound the process group even though act does not implement workflow timeouts."""
    log = Path(log)
    log.parent.mkdir(parents=True, exist_ok=True)
    print(f"Local CI: {log.stem}; log: {log}", flush=True)
    with log.open("a") as stream:
        process = subprocess.Popen([str(a) for a in args], cwd=cwd, env=env,
                                   stdout=stream, stderr=subprocess.STDOUT, start_new_session=True)
        try:
            code = process.wait(timeout=timeout)
        except BaseException:
            try:
                os.killpg(process.pid, signal.SIGTERM)
            except ProcessLookupError:
                pass
            try:
                process.wait(timeout=10)
            except subprocess.TimeoutExpired:
                try:
                    os.killpg(process.pid, signal.SIGKILL)
                except ProcessLookupError:
                    pass
                process.wait()
            raise
    if code:
        raise RunnerError(f"{args[0]} exited {code}; see {log}")


def _download(url, path, expected_hash):
    path = Path(path)
    if path.is_file() and _sha(path) == expected_hash:
        return
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_name(path.name + "." + uuid.uuid4().hex + ".part")
    try:
        with urllib.request.urlopen(url, timeout=60) as response, temporary.open("wb") as target:
            shutil.copyfileobj(response, target)
        if _sha(temporary) != expected_hash:
            raise RunnerError(f"Checksum mismatch downloading {url}")
        temporary.replace(path)
    finally:
        temporary.unlink(missing_ok=True)


def _extract(archive, destination):
    """Extract a verified archive into an empty directory without escaping it."""
    destination = Path(destination).resolve()
    destination.mkdir(parents=True, exist_ok=False)
    with tarfile.open(archive, "r:gz") as source:
        members = source.getmembers()
        for member in members:
            target = (destination / member.name).resolve()
            if not target.is_relative_to(destination) or member.isdev() or member.isfifo():
                raise RunnerError(f"Unsafe archive member: {member.name}")
            if member.issym() or member.islnk():
                base = target.parent if member.issym() else destination
                if not (base / member.linkname).resolve().is_relative_to(destination):
                    raise RunnerError(f"Unsafe archive link: {member.name}")
        # Python's data filter also protects against link chains when available.
        if hasattr(tarfile, "data_filter"):
            source.extractall(destination, filter="data")
        else:
            # Pinned archives are trusted, but never extract links on older Python.
            if any(m.issym() or m.islnk() for m in members):
                raise RunnerError("Python with tarfile.data_filter is required for this source archive")
            source.extractall(destination)


def _toolchain(cache, output, config):
    inputs = {key: value for key, value in config.items() if key.startswith(("act_", "go_"))}
    inputs["sources"] = {name: value for name, value in config["resource_hashes"].items()
                         if name.endswith((".patch", "_test.go"))}
    key = hashlib.sha256(json.dumps(inputs, sort_keys=True).encode()).hexdigest()[:20]
    tools_dir = Path(cache) / "tools" / key
    binary = tools_dir / "act"
    receipt = tools_dir / "act.sha256"
    if binary.is_file() and receipt.is_file() and receipt.read_text().strip() == _sha(binary):
        return binary
    downloads = Path(cache) / "downloads"
    go_archive = downloads / f"go{config['go_version']}.linux-amd64.tar.gz"
    _download("https://go.dev/dl/" + go_archive.name, go_archive, config["go_archive_sha256"])
    act_archive = downloads / f"act-{config['act_commit']}.tar.gz"
    _download(f"https://codeload.github.com/nektos/act/tar.gz/{config['act_commit']}",
              act_archive, config["act_archive_sha256"])
    tools_dir.mkdir(parents=True, exist_ok=True)
    stage = tools_dir / ("bootstrap-" + uuid.uuid4().hex)
    _extract(go_archive, stage / "go-toolchain")
    _extract(act_archive, stage / "source")
    source = stage / "source" / ("act-" + config["act_commit"])
    env = _clean_env()
    env.update(GOTOOLCHAIN="local", GOMAXPROCS="2", GOFLAGS="-p=2", CGO_ENABLED="0",
               GOCACHE=str(Path(cache) / "go-build"), GOMODCACHE=str(Path(cache) / "go-modules"))
    go = stage / "go-toolchain/go/bin/go"
    # The extraction may live inside the engine checkout. Without its own .git,
    # git apply treats that ancestor as its root and can silently skip our patch.
    _capture(["git", "init", "--quiet"], cwd=source, env=env)
    _logged(["git", "apply", RESOURCE_DIR / "patches/act-local.patch"],
            output / "bootstrap.log", cwd=source, env=env)
    fixtures = Path(__file__).resolve().parents[2] / "Code/Tests/local_ci"
    for fixture, package in (("artifact_compat_test.go", "pkg/artifacts"),
                             ("checkout_compat_test.go", "pkg/runner"),
                             ("config_compat_test.go", "cmd")):
        shutil.copyfile(fixtures / fixture, source / package / ("openbf2_" + fixture))
    _logged([go, "test", "./pkg/artifacts", "./pkg/runner", "./cmd", "-run", "TestOpenBF2", "-count=1"],
            output / "bootstrap.log", cwd=source, env=env, timeout=1800)
    _logged([go, "build", "-trimpath", "-buildvcs=false", "-ldflags",
             f"-X main.version={config['act_version']}", "-o", binary, "."],
            output / "bootstrap.log", cwd=source, env=env, timeout=1800)
    receipt.write_text(_sha(binary) + "\n")
    return binary


def _host(config):
    if config["host"]["system"] != "Linux" or config["host"]["machine"] not in ("x86_64", "AMD64"):
        raise RunnerError("The local workflow runner currently requires Linux x86-64")
    engine = config["container_engine"]
    if engine not in ("podman", "docker") or not shutil.which(engine):
        raise RunnerError("Install Podman, or select an installed Docker with OPENBF2_CI_ENGINE=docker")
    return engine


def _free_port():
    while True:
        with socket.socket() as probe:
            probe.bind(("127.0.0.1", 0))
            port = probe.getsockname()[1]
        if port != 16567:
            return port


def _repository(source, env):
    origin = _capture(["git", "remote", "get-url", "origin"], cwd=source, env=env)
    match = re.fullmatch(r"(?:https://github\.com/|git@github\.com:)([A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+?)(?:\.git)?/?", origin)
    if not match:
        raise RunnerError("The frozen source needs a canonical GitHub origin without credentials")
    return match.group(1)


@contextmanager
def _daemon(engine, output, env):
    if engine == "docker":
        _capture([engine, "info"], env=env)
        # A local daemon is required because local artifact services bind loopback.
        context = _capture([engine, "context", "inspect"], env=env)
        endpoint = json.loads(context)[0]["Endpoints"]["docker"]["Host"]
        if not endpoint.startswith("unix://"):
            raise RunnerError("Local CI requires a local Unix Docker socket")
        yield endpoint
        return
    with tempfile.TemporaryDirectory(prefix="openbf2-act-") as temp:
        endpoint = "unix://" + str(Path(temp) / "podman.sock")
        with (output / "podman-service.log").open("w") as log:
            process = subprocess.Popen([engine, "system", "service", "--time=0", endpoint],
                                       stdout=log, stderr=subprocess.STDOUT, env=env)
            try:
                for unused in range(100):
                    if Path(temp, "podman.sock").exists():
                        break
                    if process.poll() is not None:
                        raise RunnerError(f"Podman API service failed; see {output / 'podman-service.log'}")
                    time.sleep(0.1)
                else:
                    raise RunnerError("Timed out starting the local Podman API service")
                yield endpoint
            finally:
                if process.poll() is None:
                    process.terminate()
                    try:
                        process.wait(timeout=10)
                    except subprocess.TimeoutExpired:
                        process.kill()
                        process.wait()


def _runner_image(engine, config, output, env):
    inputs = {"base": config["runner_base"], "containerfile": _sha(RESOURCE_DIR / "Containerfile"),
              "podman": _sha(RESOURCE_DIR / "podman-local")}
    key = hashlib.sha256(json.dumps(inputs, sort_keys=True).encode()).hexdigest()[:20]
    image = "localhost/openbf2-local-ci:" + key
    inspect = subprocess.run([engine, "image", "inspect", image], stdout=subprocess.DEVNULL,
                             stderr=subprocess.DEVNULL, env=env, timeout=60)
    if inspect.returncode:
        _logged([engine, "build", "--build-arg", "RUNNER_BASE=" + config["runner_base"],
                 "--tag", image, "--file", RESOURCE_DIR / "Containerfile", RESOURCE_DIR],
                output / "runner-image.log", env=env, timeout=1800)
    return _capture([engine, "image", "inspect", image, "--format", "{{.Id}}"], env=env)


def _pins(source):
    workflow = (Path(source) / ".github/workflows/build.yml").read_text()
    result = {}
    for action in ("checkout", "upload-artifact", "download-artifact", "cache/restore", "cache/save"):
        pins = set(re.findall(r"uses:\s+actions/" + action + r"@([0-9a-f]{40})\b", workflow))
        if len(pins) != 1:
            raise RunnerError(f"Expected one immutable actions/{action} pin in the build workflow")
        result[action] = pins.pop()
    return result


def _preflight_workflow(source, output):
    template = (Path(__file__).resolve().parents[2] / "Code/Tests/local_ci/artifacts.yml").read_text()
    for action, pin in _pins(source).items():
        template = template.replace("@" + action.upper().replace("-", "_").replace("/", "_") + "@", pin)
    for name, value in _setup_pin(source).items():
        template = template.replace("@SETUP_" + name.upper() + "@", value)
    target = output / "artifact-preflight.yml"
    target.write_text(template)
    return target


def _setup_pin(source):
    production = (Path(source) / ".github/workflows/build.yml").read_text()
    result = {}
    for name, pattern in (("REPOSITORY", r"[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+"),
                          ("REVISION", r"[0-9a-f]{40}")):
        value = re.search(r"^  LINUX_SETUP_" + name + r":\s*(" + pattern + r")\s*(?:#.*)?$", production, re.M)
        if not value:
            raise RunnerError("Missing immutable Linux setup " + name.lower())
        result[name.lower()] = value.group(1)
    return result


def _artifact_evidence(directory, names):
    result = {}
    for name in names:
        matches = list(Path(directory).glob(f"*/{name}/{name}.zip"))
        if len(matches) != 1:
            raise RunnerError(f"Missing or ambiguous completed local artifact: {name}")
        with zipfile.ZipFile(matches[0]) as archive:
            if not archive.namelist() or archive.testzip() is not None:
                raise RunnerError(f"Invalid local artifact: {name}")
        result[name] = {"path": str(matches[0]), "sha256": _sha(matches[0])}
    return result


def _sdk_evidence(log):
    text = Path(log).read_text(errors="replace")
    result = {}
    for key, prefix in (("reference", "localhost/openbf2-linux-sdk@sha256:"),
                        ("image_id", "sha256:"), ("manifest_digest", "sha256:")):
        values = set(re.findall('"' + key + r'":\s*"(' + re.escape(prefix) + r'[0-9a-f]{64})"', text))
        if len(values) != 1:
            raise RunnerError(f"Workflow did not report one verified SDK {key}")
        result[key] = values.pop()
    if result["reference"].split("@", 1)[1] != result["manifest_digest"]:
        raise RunnerError("SDK identity report is inconsistent")
    return result


def _stop_containers(engine, label, env):
    result = subprocess.run([engine, "ps", "-aq", "--filter", "label=" + label],
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, env=env, timeout=15)
    if result.returncode == 0 and result.stdout.strip():
        subprocess.run([engine, "rm", "-f", *result.stdout.split()], env=env,
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=60)


def _execute(source_snapshot, output, cache, *, fresh_sdk, preflight_only):
    source, output, cache = (Path(p).resolve() for p in (source_snapshot, output, cache))
    output.mkdir(parents=True, exist_ok=True)
    cache.mkdir(parents=True, exist_ok=True)
    config = config_identity(source)
    engine = _host(config)
    env = _clean_env()
    revision = _capture(["git", "rev-parse", "HEAD"], cwd=source, env=env)
    repository = _repository(source, env)
    if not re.fullmatch(r"[0-9a-f]{40}", revision):
        raise RunnerError("The source snapshot has no full commit SHA")
    if (source / ".git/objects/info/alternates").exists():
        raise RunnerError("Snapshot has Git object alternates; repack it before container checkout")
    binary = _toolchain(cache, output, config)
    image = _runner_image(engine, config, output, env)
    run_id = str(time.time_ns())
    label = "org.openbf2.local-ci=" + uuid.uuid4().hex
    event = output / "event.json"
    # GitHub sends workflow_dispatch inputs as strings. act reads a JSON boolean here as false for a
    # boolean input, and the event file takes precedence over --input, so Windows jobs would run.
    event.write_text(json.dumps({"inputs": {"linux_only": "true"},
                                "repository": {"full_name": repository, "default_branch": "main"}}))
    # A fresh namespace bypasses action caches as well as inner Podman build layers.
    action_cache = cache / "actions"
    sdk_cache = output / "cold-cache" if fresh_sdk else cache / "workflow-cache"
    env.update(OPENBF2_ACT_NO_CONFIG="1", OPENBF2_ACT_FROZEN_SHA=revision, OPENBF2_ACT_PUBLIC_CHECKOUT="1",
               XDG_CACHE_HOME=str(cache), XDG_CONFIG_HOME=str(output / "config"))
    artifacts = output / "artifacts"
    if artifacts.exists() and any(artifacts.iterdir()):
        raise RunnerError("Artifact output is not empty; use a new run directory")
    artifacts.mkdir(exist_ok=True)
    with _daemon(engine, output, env) as endpoint:
        env["DOCKER_HOST"] = endpoint
        args = [binary, "workflow_dispatch", "--directory", source, "--eventpath", event,
                "--input", "linux_only=true", "--env", "SHA_REF=" + revision,
                "--env", "GITHUB_REF=refs/heads/local-ci", "--env", "GITHUB_REPOSITORY=" + repository,
                "--env", "GITHUB_RUN_ID=" + run_id, "--env", "OPENBF2_CI_FRESH_SDK=" + ("1" if fresh_sdk else "0"),
                "--secret", "GITHUB_TOKEN=", "--env", "GIT_CONFIG_COUNT=1",
                "--env", "GIT_CONFIG_KEY_0=http.https://github.com/.extraheader", "--env", "GIT_CONFIG_VALUE_0=",
                "--platform", "ubuntu-22.04=" + image, "--platform", "windows-2022=-unsupported",
                "--container-architecture", config["architecture"], "--container-daemon-socket", "-",
                "--container-options-policy", "allow", "--container-options", "--privileged --label " + label,
                "--network", "host", "--pull=false", "--rm", "--concurrent-jobs", str(config["concurrent_jobs"]),
                "--action-cache-path", action_cache, "--cache-server-path", sdk_cache,
                "--cache-server-addr", "127.0.0.1", "--cache-server-port", "0",
                "--artifact-server-path", artifacts, "--artifact-server-addr", "127.0.0.1",
                "--artifact-server-port", str(_free_port()), "--secret-file", "/dev/null", "--env-file", "/dev/null",
                "--input-file", "/dev/null", "--var-file", "/dev/null"]
        try:
            preflight = _preflight_workflow(source, output)
            _logged([*args, "--workflows", preflight], output / "preflight.log", cwd=output,
                    env=env, timeout=config["preflight_timeout_seconds"])
            evidence = _artifact_evidence(artifacts, ("local-ci-proof", "local-ci-roundtrip"))
            if not preflight_only:
                _logged([*args, "--workflows", source / config["workflow"], "--list", "--validate"],
                        output / "workflow-plan.log", cwd=output, env=env)
                _logged([*args, "--workflows", source / config["workflow"]], output / "workflow.log",
                        cwd=output, env=env, timeout=config["workflow_timeout_seconds"])
                evidence.update(_artifact_evidence(artifacts, REQUIRED_ARTIFACTS))
        except subprocess.TimeoutExpired as error:
            raise RunnerError(f"Local verification exceeded its time limit; see {output}") from error
        finally:
            _stop_containers(engine, label, env)
    result = {"status": "passed", "engine_revision": revision,
              "checks": {"artifact-preflight": "passed", "packaging-fixtures": "passed",
                         "signing-script-syntax": "passed", "nested-oci-preflight": "passed",
                         "cache-preflight": "passed", "cold-oci-preflight": "passed"}, "artifacts": evidence,
              "act_version": config["act_version"], "act_sha256": _sha(binary),
              "runner_image_id": image, "fresh_sdk": fresh_sdk,
              "sdk_identity": None, "log_directory": str(output)}
    setup = _setup_pin(source)
    result.update(setup_repository=setup["repository"], setup_revision=setup["revision"])
    if not preflight_only:
        result["checks"]["linux-workflow"] = "passed"
        result["sdk_identity"] = _sdk_evidence(output / "workflow.log")
    (output / "runner-result.json").write_text(json.dumps(result, indent=2) + "\n")
    return result


def run_workflow(source_snapshot, output, cache, fresh_sdk=False):
    """Run the actual Linux workflow and return evidence only after all jobs pass."""
    with _cache_lock(cache):
        return _execute(source_snapshot, output, cache, fresh_sdk=fresh_sdk, preflight_only=False)


def run_preflight(source_snapshot, output, cache):
    """Check pinned artifact actions, frozen checkout and nested OCI tools without an engine build."""
    with _cache_lock(cache):
        return _execute(source_snapshot, output, cache, fresh_sdk=False, preflight_only=True)


@contextmanager
def _cache_lock(cache):
    try:
        import fcntl
    except ImportError as error:
        raise RunnerError("The local workflow runner currently requires Linux x86-64") from error
    cache = Path(cache)
    cache.mkdir(parents=True, exist_ok=True)
    with (cache / "runner.lock").open("a") as lock:
        try:
            fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError as error:
            raise RunnerError("Another local verification is using this cache") from error
        try:
            yield
        finally:
            fcntl.flock(lock, fcntl.LOCK_UN)
