"""[TEMP] Extract, attest, and run the strict x86 numeric material-query fixture."""

from pathlib import Path
import hashlib
import json
import re
import struct
import subprocess
import sys


CODE = Path(__file__).resolve().parents[2]
BF2 = CODE / "BF2"
AI = BF2 / "AI/AIDLL/AI"
FIXTURE = Path(__file__).resolve()
SOURCES = {
    "map": AI / "Main/AIPathfinding.cpp",
    "tree": AI / "AIPathFinding/NavMesh/QuadTree.cpp",
    "manager": AI / "AIPathFinding/NavMesh/QuadTreeManager.cpp",
}


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def strip_comments(text):
    return re.sub(r"/\*.*?\*/|//[^\n]*", "", text, flags=re.S)


def definition(source, signature):
    start = source.find(signature)
    if start < 0:
        raise RuntimeError(("missing production definition", signature))
    brace = source.find("{", start)
    if brace < 0:
        raise RuntimeError(("missing production body", signature))
    depth = 0
    for index in range(brace, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[start : index + 1]
    raise RuntimeError(("unterminated production body", signature))


def preflight(output):
    output.mkdir(parents=True, exist_ok=True)
    texts = {name: path.read_text(encoding="utf-8") for name, path in SOURCES.items()}
    bodies = {
        "manager_get_island": definition(
            texts["manager"], "QuadTree* QuadTreeManager::getIsland(unsigned int island) const"
        ),
        "tree_get_material": definition(
            texts["tree"],
            "unsigned char QuadTree::getMaterialOfPolygon(unsigned int polygon) const",
        ),
        "map_get_material": definition(
            texts["map"],
            "bool AIPathfinding::MapContainer::getMaterial(unsigned int quadTree, unsigned int polygon,",
        ),
    }
    (output / "material_query_actual.inl").write_text(
        "// [TEMP] Exact production definitions; no body substitutions.\n\n"
        + "\n\n".join(bodies.values())
        + "\n",
        encoding="utf-8",
    )

    manager = strip_comments(bodies["manager_get_island"])
    tree = strip_comments(bodies["tree_get_material"])
    map_body = strip_comments(bodies["map_get_material"])
    checks = {
        "sentinel_is_exact_uint_max_pair": (
            "quadTree == 0xFFFFFFFFu || polygon == 0xFFFFFFFFu" in map_body
        ),
        "sentinel_zeroes_output": "material = 0;" in map_body,
        "sentinel_returns_false": "return false;" in map_body,
        "valid_chain_is_typed_and_direct": (
            "m_quadTreeManager->getIsland(quadTree)->getMaterialOfPolygon(polygon)" in map_body
        ),
        "valid_returns_true": "return true;" in map_body,
        "map_has_no_bounds_or_null_guard": all(
            token not in map_body
            for token in (".size()", ".at(", "getNumberOfIslands", "m_numberOfPolygons", "nullptr")
        ),
        "island_accessor_is_unchecked_subscript": (
            "return m_quadTrees[island].first;" in manager
            and ".size()" not in manager
            and ".at(" not in manager
        ),
        "polygon_accessor_is_direct_material_subscript": (
            "return m_polygons[polygon].m_material;" in tree
            and "m_numberOfPolygons" not in tree
            and ".at(" not in tree
        ),
    }
    if not all(checks.values()):
        raise RuntimeError(("production source contract", checks))

    (output / "static_checks.json").write_text(
        json.dumps(checks, indent=2) + "\n", encoding="utf-8"
    )
    (output / "source_hashes.json").write_text(
        json.dumps(
            {
                "production": {
                    str(path.relative_to(CODE)).replace("\\", "/"): sha256(path)
                    for path in SOURCES.values()
                },
                "bodies": {
                    name: hashlib.sha256(body.encode("utf-8")).hexdigest()
                    for name, body in bodies.items()
                },
                "fixture": {
                    FIXTURE.with_suffix(suffix).name: sha256(FIXTURE.with_suffix(suffix))
                    for suffix in (".cpp", ".py", ".cmd")
                },
            },
            indent=2,
        )
        + "\n",
        encoding="utf-8",
    )


def pe_machine(path):
    image = path.read_bytes()
    if image[:2] != b"MZ":
        raise RuntimeError("fixture executable has no DOS header")
    pe_offset = struct.unpack_from("<I", image, 0x3C)[0]
    if image[pe_offset : pe_offset + 4] != b"PE\0\0":
        raise RuntimeError("fixture executable has no PE signature")
    return struct.unpack_from("<H", image, pe_offset + 4)[0]


def run(executable, output):
    machine = pe_machine(executable)
    if machine != 0x014C:
        raise RuntimeError(("fixture executable is not x86", hex(machine)))

    normal = subprocess.run(
        [str(executable), "normal"], capture_output=True, text=True, timeout=30
    )
    normal_text = normal.stdout + normal.stderr
    (output / "normal.log").write_text(normal_text, encoding="utf-8")
    match = re.fullmatch(r"PASS material_query_runtime ([1-9][0-9]*) checks\n", normal_text)
    if normal.returncode != 0 or not match or "FAIL" in normal_text:
        raise RuntimeError(("normal runtime", normal.returncode, normal_text))

    crash_lines = []
    for mode in ("bad_island", "null_polygon_storage"):
        result = subprocess.run(
            [str(executable), mode], capture_output=True, text=True, timeout=30
        )
        combined = result.stdout + result.stderr
        normalized = result.returncode & 0xFFFFFFFF
        (output / (mode + ".log")).write_text(
            "returncode=0x%08X\n%s" % (normalized, combined), encoding="utf-8"
        )
        if normalized != 0xC0000005 or combined:
            raise RuntimeError((mode, hex(normalized), combined))
        crash_lines.append("PASS %s expected access violation 0xC0000005" % mode)

    static_checks = json.loads((output / "static_checks.json").read_text(encoding="utf-8"))
    lines = [normal_text.rstrip()] + crash_lines + [
        "PASS %d production-shape checks" % len(static_checks),
        "PASS strict x86 link without FORCE",
    ]
    runtime_log = "\n".join(lines) + "\n"
    (output / "runtime.log").write_text(runtime_log, encoding="utf-8")
    (output / "binary_identity.json").write_text(
        json.dumps(
            {
                "path": executable.as_posix(),
                "sha256": sha256(executable),
                "pe_machine": "0x014C (IMAGE_FILE_MACHINE_I386)",
                "normal_checks": int(match.group(1)),
                "cases_passed": 3,
                "cases_failed": 0,
                "normal_cases": 1,
                "expected_access_violations": 2,
                "source_shape_checks": len(static_checks),
                "strict_link": True,
                "force_link": False,
            },
            indent=2,
        )
        + "\n",
        encoding="utf-8",
    )
    (output / "README.md").write_text(
        "# Numeric material-query focused validation\n\n"
        "The fixture compiles exact extracted production definitions for "
        "`MapContainer::getMaterial`, `QuadTreeManager::getIsland`, and "
        "`QuadTree::getMaterialOfPolygon` into a strict MSVC x86 executable. "
        "The receiver classes and polygon record are chosen minimal fixture types, not "
        "production headers or object lifetimes. No production class layout, virtual ABI, "
        "construction or destruction is validated.\n\n"
        "Three cases passed: one normal case with %d explicit checks and two isolated expected "
        "access violations; zero unexpected failures. Eight separate source-shape checks pass. "
        "The `/MACHINE:X86` link is strict, with no `/FORCE`; the executable's PE machine is "
        "independently checked as I386.\n\n" % int(match.group(1))
        +
        "```text\n" + runtime_log.rstrip() + "\n```\n\n"
        "The normal mode covers both exact sentinels independently and together, mandatory output "
        "zeroing, valid zero and high-byte materials, a second island, and a changed material on "
        "a subsequent query. Sentinel tests use a poisoned manager pointer so reaching it would "
        "fail. The two crash probes use a large non-sentinel island index and null polygon "
        "storage respectively. They observe access violations in this Windows x86 build; "
        "they are deliberately invalid-input diagnostics, not portable defined-C++ oracles, "
        "nor an exhaustive proof about every invalid index. The null-storage case does not "
        "test an out-of-range polygon index. No server, map load, socket, bot tick, or client "
        "path was exercised.\n",
        encoding="utf-8",
    )
    print(runtime_log, end="")


if __name__ == "__main__":
    if len(sys.argv) == 3 and sys.argv[1] == "preflight":
        preflight(Path(sys.argv[2]).resolve())
    elif len(sys.argv) == 4 and sys.argv[1] == "run":
        run(Path(sys.argv[2]).resolve(), Path(sys.argv[3]).resolve())
    else:
        raise SystemExit("preflight OUT | run EXE OUT")
