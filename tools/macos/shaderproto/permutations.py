#!/usr/bin/env python3
"""[CHOSEN, not recovered] Three local-only inputs following MetaFx::generateShader.

Read the current source algorithm and .mfx literal annotations, rather than guessing
permutation flags. This does not implement or replace the engine's permutator.
"""
import json
from pathlib import Path
import re
import subprocess

repo = Path(__file__).resolve().parents[3]
source = repo / "out/fx-sample/src"
output = repo / "out/shaderproto/permutations"
output.mkdir(parents=True, exist_ok=True)
binary = repo / "out/build/s0-shader-proto/shaderproto"


def strings(text, name):
    match = re.search(r"\bstring\s+" + re.escape(name) + r"\s*\[\]\s*=\s*\{(.*?)\}\s*;", text, re.S)
    return re.findall(r'"([^"\n]*)"', match[1]) if match else []


# Common vocabulary from ShaderManager; choose its non-vendor SM2 branch. Shader
# model choices are in Ra/EffectManager.cpp's kPixel*/kVertex* define tables.
vocabulary = (repo / "Code/BF2/RendDX9/ShaderManagerDefines.inc").read_text()
common = dict(re.findall(r'\{\s*"([^"\n]+)",\s*"([^"\n]+)"\s*\}', vocabulary))
common.update(PSVERSION="20", PSMODEL="ps_2_0", VSMODEL="vs_2_0",
              _FORCE_1_4_SHADERS_="0", _FORCE_1_3_SHADERS_="0")
examples = [("RaShaderBM", "_nothing_"), ("RaShaderSM", "_hasnormalmap_"),
            ("RaShaderSTM", "_base_|_detail_")]
for base, permutation in examples:
    # Strip comments so disabled array members cannot enter the wrapper.
    text = (source / (base + ".mfx")).read_text()
    text = re.sub(r"/\*.*?\*/|//[^\n]*", "", text, flags=re.S)
    if permutation not in strings(text, "GenerateShaderPermutations"):
        raise RuntimeError("Selected permutation is not present in the metadata")
    literals = re.findall(r"const\s+dword\s+(\w+)\s*:\s*LITERAL\s*(?:<(.*?)>)?\s*;", text, re.S)
    selected = set(permutation.split("|"))
    definitions = {name.upper(): str(int(name in selected)) for name, _ in literals}
    lines = ["// [CHOSEN, not recovered] Prototype of the current MetaFx generated input."]
    lines += [f"#define {name} {value}" for name, value in definitions.items()]
    lines += [f"static const dword {name.lower()} = {definitions[name.upper()]};" for name, _ in literals]
    texsets = []
    arrays = {}
    for structure in strings(text, "GenerateStructs"):
        members = strings(text, structure + "Base")
        for literal, annotations in literals:
            if literal in selected:
                members += strings(annotations or "", structure)
        members = list(dict.fromkeys(members))
        arrays[structure] = members
        if members:
            lines.append(f"string {structure}[] = {{" + ",".join(json.dumps(s) for s in members) + "};")
        for member in members:
            if "2D" in member and member.startswith("T"):
                texsets.append("Tex" + member[1:-2] + "Ind")
    definitions.update(common)
    definitions["NUM_LIGHTS"] = "1"
    definitions["NUM_TEXSETS"] = str(len(texsets))
    definitions.update({name: str(i) for i, name in enumerate(texsets)})
    lines += ["#define NUM_LIGHTS 1", f"#define NUM_TEXSETS {len(texsets)}"]
    lines += [f"#define {name} {i}" for i, name in enumerate(texsets)]
    lines += [f'#include "shaders/{base}.fx"']
    directory = output / base
    directory.mkdir(parents=True, exist_ok=True)
    wrapper = directory / "generated.fx"
    wrapper.write_text("\n".join(lines) + "\n")
    # Literal and generated texture macros are already in the source wrapper.
    macros = [f"{key}={value}" for key, value in common.items()]
    (directory / "defines.json").write_text(json.dumps(dict(permutation=permutation, defines=definitions,
                                                           arrays=arrays), indent=2) + "\n")
    command = [str(binary), str(wrapper), str(source), str(directory), *macros]
    (directory / "command.json").write_text(json.dumps(command, indent=2) + "\n")
    with (directory / "stdout.log").open("w") as stdout, (directory / "stderr.log").open("w") as stderr:
        child = subprocess.run(command, stdout=stdout, stderr=stderr, timeout=120)
    print(base, permutation, "exit", child.returncode)
