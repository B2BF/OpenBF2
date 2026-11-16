#!/usr/bin/env python3
"""Build a game directory the native Linux server can read from a Windows BF2 install.

The retail Linux file system lower-cases every engine path (UnixFileSystem::fixUnixPath), so engine
content must be stored lower-case. Python imports stay case-sensitive, so the trees below python/,
admin/ and mods/<mod>/python/ keep their original names.

Files of 1 MiB and more (archives, movies, binaries) are hard-linked, so they take no extra space
when source and destination share a file system. Smaller files are copied, because the game and
its Python rewrite settings and .pyc files in place, which would modify the install through a link.

Usage: prepare_linux_game_dir.py "<Battlefield 2 install>" <new game dir>
"""
import os
import shutil
import sys

LINK_MIN_SIZE = 1 << 20


def linux_path(rel):
    """Map an install-relative path to its Linux game-dir path."""
    out = []
    for part in rel.split("/"):
        low = [p.lower() for p in out]
        in_python = low[:1] in (["python"], ["admin"]) or (low[:1] == ["mods"] and low[2:3] == ["python"])
        out.append(part if in_python else part.lower())
    return "/".join(out)


def main(src, dst):
    if os.path.exists(dst):
        sys.exit("destination already exists: " + dst)
    warned = False
    for root, _, files in os.walk(src):
        for name in files:
            source = os.path.join(root, name)
            if os.path.islink(source):  # a stray link in a copied install may dangle
                continue
            rel = os.path.relpath(source, src).replace(os.sep, "/")
            target = os.path.join(dst, linux_path(rel))
            os.makedirs(os.path.dirname(target), exist_ok=True)
            if os.path.lexists(target):
                sys.exit("two files differ only in case: " + target)
            if os.path.getsize(source) < LINK_MIN_SIZE:
                shutil.copy2(source, target)
                continue
            try:
                os.link(source, target)
            except OSError:
                if not warned:
                    print("Cannot hard-link (different file system?): copying the whole game, "
                          "which needs about 4 GB.", file=sys.stderr)
                    warned = True
                shutil.copy2(source, target)


assert linux_path("Levels/Strike_at_Karkand/Server.zip") == "levels/strike_at_karkand/server.zip"
assert linux_path("python/bf2/Timer.py") == "python/bf2/Timer.py"
assert linux_path("Admin/standard_admin/autobalance.py") == "admin/standard_admin/autobalance.py"
assert linux_path("mods/bf2/Python/Game/Foo.py") == "mods/bf2/python/Game/Foo.py"
assert linux_path("mods/bf2/Settings/ServerSettings.con") == "mods/bf2/settings/serversettings.con"

if __name__ == "__main__":
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    main(sys.argv[1], sys.argv[2])
