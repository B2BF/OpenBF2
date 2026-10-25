#!/usr/bin/env python3
"""[TEMP] Strict optimized spawn occupancy regression with exact bodies and actual class headers."""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import subprocess
import sys

sys.dont_write_bytecode = True
TESTS = Path(__file__).resolve().parent
BF2 = TESTS.parent / "BF2"
# Reuse the existing real-header declaration parser, not a second hand-maintained virtual table.
spec = importlib.util.spec_from_file_location("object_environment_fixture", TESTS / "aidll/object_environment_runtime.py")
helper = importlib.util.module_from_spec(spec)
spec.loader.exec_module(helper)

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("output", type=Path)
parser.add_argument("--baseline", type=Path, help="saved complete pre-fix SpawnPoint.cpp")
args = parser.parse_args()
out = args.output.resolve()
out.mkdir(parents=True, exist_ok=True)

records = []
def body(path, expression):
    source = path.read_text(encoding="utf-8")
    found = re.findall(expression, source, re.M | re.S)
    if len(found) != 1 or "FRONTIER" in found[0]:
        raise RuntimeError(("expected exactly one implemented body", path, expression))
    records.append(dict(source=str(path), source_sha256=hashlib.sha256(source.encode()).hexdigest(),
                        body=found[0], body_sha256=hashlib.sha256(found[0].encode()).hexdigest()))
    return found[0]

spawn = args.baseline or BF2 / "Game/Objects/SpawnPoint.cpp"
actual = ['// [TEMP] Mechanically copied production bodies; original declarations remain intact.',
          '#include "' + (TESTS / "spawnpoint_soldier_dispatch_runtime.h").as_posix() + '"',
          'namespace dice { namespace hfe {',
          'static unsigned char g_spawnPointDynamicVeto = 0;',
          'namespace world { extern IBase* heightmapCluster; }',
          body(spawn, r'^bool SpawnPoint::getActive\(bool isAI, int team\)\n\{.*?^}'),
          'namespace world {',
          body(BF2 / "Game/Objects/Game/Soldier.cpp", r'^IPlayer\* Soldier::getOccupingPlayer\(\) const\n\{.*?^}'),
          '} } }']
(out / "spawnpoint_actual.cpp").write_text("\n\n".join(actual))
(out / "source.json").write_text(json.dumps(records, indent=2))

cold = [
    ("world", "IObject", "Game/Objects/IObject.h", set()),
    ("world", "Object", "Game/Objects/Object.h", set()),
    ("world", "SimpleObject", "Game/Objects/Base/SimpleObject.h", set()),
    ("world", "Bundle", "Game/Objects/Base/Bundle.h", set()),
    ("world", "Soldier", "Game/Objects/Game/Soldier.h", {"getOccupingPlayer"}),
    ("world", "IPlayerControlObject", "Scene/Object/ObjectTypes/Game/IPlayerControlObject.h", set()),
    ("world", "IPlayer", "Game/Objects/IPlayer.h", set()),
    ("world", "IObjectTemplate", "Game/Objects/IObjectTemplate.h", set()),
    ("world", "ObjectTemplate", "Game/Objects/ObjectTemplate.h", set()),
    ("world", "ObjectManager", "Scene/Object/ObjectManager.h", set()),
    ("", "SpawnPointTemplate", "Game/Objects/SpawnPointTemplate.h", set()),
    ("", "SpawnPoint", "Game/Objects/SpawnPoint.h", {"getActive"}),
]
generated = ['// [TEMP] Cold methods generated from real headers; all abort if reached.']
for space, owner, header, skip in cold:
    generated.append('namespace dice { namespace hfe {' + ('namespace ' + space + ' {' if space else ''))
    seen = set()
    for name, declaration, pure in helper.declarations(owner, header):
        if pure or name in skip or declaration in seen:
            continue
        seen.add(declaration)
        generated.append(helper.out_of_line(owner, declaration, name) +
                         ' { spawnUnexpected("' + owner + '::' + name + '"); }')
    generated.append('} }' + (' }' if space else ''))
for observer, owner, header in [('ObservedPlayer', 'IPlayer', 'Game/Objects/IPlayer.h'),
                                ('ObservedTemplate', 'IObjectTemplate', 'Game/Objects/IObjectTemplate.h')]:
    generated.append('struct ' + observer + ' : dice::hfe::world::' + owner + ' {')
    for base, relative in [('IBase', 'Core/IBase.h'), (owner, header)]:
        for name, declaration, pure in helper.declarations(base, relative):
            if pure:
                generated.append(declaration + ' override { spawnUnexpected("' + observer + '::' + name + '"); }')
    generated.append('};')
(out / "spawnpoint_collaborators.inl").write_text("\n".join(generated))

sources = [TESTS / "spawnpoint_soldier_dispatch_runtime.cpp", out / "spawnpoint_actual.cpp",
           BF2 / "Scene/Object/ObjectPredicator.cpp"]
exe = out / ("spawnpoint_soldier_runtime.exe" if os.name == "nt" else "spawnpoint_soldier_runtime")
if os.name == "nt":
    command = ["cl", "/nologo", "/EHsc", "/O2", "/std:c++14", "/permissive", "/Gy",
               "/DDEDICATED=1", "/FI" + str(BF2 / "Core/BuildConfig.h"), "/I" + str(out)]
    command += list(map(str, sources)) + ["/link", "/OPT:REF", "/MANIFEST:EMBED",
                                        "/MANIFESTUAC:level='asInvoker' uiAccess='false'", "/OUT:" + str(exe)]
else:
    command = [os.environ.get("CXX", "g++"), "-std=c++14", "-O2", "-g", "-fpermissive",
               "-fno-strict-aliasing", "-fwrapv", "-fno-delete-null-pointer-checks",
               "-ffp-contract=off", "-fno-omit-frame-pointer", "-fms-extensions",
               "-Wno-narrowing", "-Wno-unknown-pragmas", "-Wno-multichar", "-pthread",
               "-ffunction-sections", "-fdata-sections", "-DDEDICATED=1", "-D_LINUX",
               "-include", str(BF2 / "Core/BuildConfig.h"),
               "-include", str(BF2 / "Core/PlatformCompat.h"), "-I", str(out)]
    command += list(map(str, sources)) + ["-Wl,--gc-sections", "-o", str(exe)]
(out / "command.json").write_text(json.dumps(command, indent=2))
with (out / "build.log").open("w") as log:
    result = subprocess.run(command, cwd=out, stdout=log, stderr=subprocess.STDOUT)
if result.returncode:
    print((out / "build.log").read_text(errors="replace"))
    raise SystemExit(result.returncode)
with (out / "runtime.log").open("w") as log:
    result = subprocess.run([str(exe)], cwd=out, stdout=log, stderr=subprocess.STDOUT)
print((out / "runtime.log").read_text(errors="replace"), end="")
if result.returncode < 0:
    print("Fixture terminated by signal", -result.returncode)
raise SystemExit(result.returncode if result.returncode >= 0 else 128 - result.returncode)
