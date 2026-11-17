#!/usr/bin/env python3
"""[CHOSEN, not recovered] Isolate native ELF plugin globals (kr4uzi/OpenBF2#36)."""
import argparse
import fnmatch
import json
from pathlib import Path
import re
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--linker", required=True)
    parser.add_argument("--objcopy", required=True)
    parser.add_argument("--nm", required=True)
    parser.add_argument("--symbols", type=Path, required=True)
    parser.add_argument("--image", required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("archives", nargs="+", type=Path)
    args = parser.parse_args()
    if not re.fullmatch(r"[A-Za-z0-9_]+", args.image):
        parser.error("image name must be a C identifier")
    patterns = []
    for line in args.symbols.read_text().splitlines():
        line = line.strip()
        if line and not line.startswith("#"):
            # The shared lists use Mach-O's additional leading underscore.
            if not line.startswith("__Z"):
                parser.error(f"Unexpected symbol pattern: {line}")
            patterns.append(line[1:])
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix=args.image + "-", dir=args.output.parent) as scratch:
        scratch = Path(scratch)
        linked, renamed, final = (scratch / name for name in ("linked.o", "renamed.o", "final.o"))
        subprocess.run([args.linker, "-r", "--whole-archive", *map(str, args.archives),
                        "--no-whole-archive", "-o", str(linked)], check=True)
        # Local symbols include GCC's synthetic D5 destructor COMDAT signature.
        # Renaming only externals leaves that group shared between plugin images,
        # and the final linker can discard a plugin's otherwise private methods.
        symbols = subprocess.check_output([args.nm, "--defined-only", "--format=posix", str(linked)], text=True)
        private = sorted({line.split()[0] for line in symbols.splitlines()
                          if line.split() and any(fnmatch.fnmatchcase(line.split()[0], pattern) for pattern in patterns)})
        if not private:
            raise RuntimeError(f"No image-private definitions found in {args.image}")
        # Rename before localizing so ELF COMDAT signatures cannot coalesce with another image.
        names = {name: "bf2_private_" + args.image + "_" + name for name in private}
        mapping, local = scratch / "rename.txt", scratch / "local.txt"
        mapping.write_text("".join(f"{old} {new}\n" for old, new in names.items()))
        local.write_text("".join(name + "\n" for name in names.values()))
        subprocess.run([args.objcopy, "--redefine-syms=" + str(mapping), str(linked), str(renamed)], check=True)
        subprocess.run([args.objcopy, "--localize-symbols=" + str(local), str(renamed), str(final)], check=True)
        visible = subprocess.check_output([args.nm, "--extern-only", "--defined-only", "--format=posix", str(final)], text=True)
        visible_names = {line.split()[0] for line in visible.splitlines() if line.split()}
        if visible_names.intersection(names) or visible_names.intersection(names.values()):
            raise RuntimeError("Plugin-private symbols remain externally visible")
        final.replace(args.output)
        args.output.with_suffix(".symbols.json").write_text(json.dumps(names, indent=2) + "\n")
        print(f"{args.image}: isolated {len(private)} ELF symbols")


if __name__ == "__main__":
    main()
