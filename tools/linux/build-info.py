#!/usr/bin/env python3
"""Record the native client artifact and source state after a successful link."""
import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import subprocess

from presentation import source_metadata


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("binary", type=Path)
    parser.add_argument("--compiler", required=True)
    parser.add_argument("--architecture", required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    binary = args.binary.resolve(strict=True)
    metadata = dict(binary=str(binary), binary_sha256=hashlib.sha256(binary.read_bytes()).hexdigest(),
                    compiler=args.compiler, architecture=args.architecture,
                    utc=datetime.now(timezone.utc).isoformat(), **source_metadata(root))
    try:
        git = ["git", "-c", "safe.directory=" + str(root)]
        diff = subprocess.check_output(git + ["diff", "--binary", "HEAD"], cwd=root, stderr=subprocess.PIPE)
        metadata["tracked_diff_sha256"] = hashlib.sha256(diff).hexdigest()
        paths = subprocess.check_output(git + ["ls-files", "--others", "--exclude-standard", "-z"], cwd=root, stderr=subprocess.PIPE)
        metadata["untracked_sha256"] = {
            path.decode(): hashlib.sha256((root / path.decode()).read_bytes()).hexdigest()
            for path in paths.split(b"\0") if path and (root / path.decode()).is_file()
        }
    except (OSError, subprocess.CalledProcessError) as error:
        metadata["source_hash_error"] = str(error)
    binary.with_suffix(".build.json").write_text(json.dumps(metadata, indent=2) + "\n")


if __name__ == "__main__":
    main()
