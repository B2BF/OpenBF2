#!/usr/bin/env python3
"""Identify a cached OCI SDK by its checked manifest and image configuration."""

import argparse
import hashlib
import json
import re
import tarfile
from pathlib import Path


def identity(archive):
    def unique_object(pairs):
        result = {}
        for key, value in pairs:
            if key in result:
                raise ValueError(f"Duplicate OCI JSON key: {key}")
            result[key] = value
        return result

    def decode(data):
        return json.loads(data, object_pairs_hook=unique_object)

    with tarfile.open(archive, "r:*") as source:
        entries = {}
        for entry in source:
            if entry.name in entries:
                raise ValueError(f"Duplicate OCI archive entry: {entry.name}")
            entries[entry.name] = entry

        def read(name, limit=4 * 1024 * 1024):
            if name not in entries:
                raise ValueError(f"Missing OCI metadata: {name}")
            entry = entries[name]
            if not entry.isfile() or entry.size > limit:
                raise ValueError(f"Invalid OCI metadata entry: {name}")
            return source.extractfile(entry).read()

        def blob(descriptor):
            digest = descriptor.get("digest", "")
            if not re.fullmatch(r"sha256:[0-9a-f]{64}", digest):
                raise ValueError("OCI metadata must use SHA-256")
            data = read("blobs/sha256/" + digest.split(":", 1)[1])
            if len(data) != descriptor.get("size") or hashlib.sha256(data).hexdigest() != digest[7:]:
                raise ValueError("OCI metadata checksum/size mismatch")
            return decode(data)

        index = decode(read("index.json"))
        if index.get("schemaVersion") != 2 or index.get("mediaType", "application/vnd.oci.image.index.v1+json") != "application/vnd.oci.image.index.v1+json":
            raise ValueError("Expected an OCI image index")
        manifests = index.get("manifests", [])
        if len(manifests) != 1:
            raise ValueError("Expected one SDK image manifest")
        descriptor = manifests[0]
        if descriptor.get("mediaType") != "application/vnd.oci.image.manifest.v1+json":
            raise ValueError("Expected an OCI image manifest descriptor")
        manifest = blob(descriptor)
        if manifest.get("schemaVersion") != 2 or manifest.get("mediaType", "application/vnd.oci.image.manifest.v1+json") != "application/vnd.oci.image.manifest.v1+json":
            raise ValueError("Expected an OCI image manifest")
        if manifest["config"].get("mediaType") != "application/vnd.oci.image.config.v1+json":
            raise ValueError("Expected an OCI image configuration")
        config = blob(manifest["config"])
        if config.get("architecture") != "amd64" or config.get("os") != "linux":
            raise ValueError("SDK must be Linux x86-64")
        return {
            "reference": "localhost/openbf2-linux-sdk@" + descriptor["digest"],
            "image_id": manifest["config"]["digest"],
            "manifest_digest": descriptor["digest"],
        }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("archive", type=Path)
    parser.add_argument("--github-output", type=Path)
    args = parser.parse_args()
    result = identity(args.archive)
    if args.github_output:
        with args.github_output.open("a") as output:
            for key, value in result.items():
                output.write(f"{key}={value}\n")
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
