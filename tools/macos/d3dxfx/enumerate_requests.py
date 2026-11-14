#!/usr/bin/env python3
"""[CHOSEN, not recovered] Static enumeration of every effect compile request.

Derives, from the recovered engine source in this tree and the read-only game
content, every (root bytes, macros, include bytes, flags) request the macOS client
would hand to the effect cache, per video-quality configuration. Zip archives are
read in place; nothing is written to the game folder. Outputs go below out/.

The grammar and its code references are documented in docs/macos-c-fx-permutations.md.
This models the Metal device's reported caps (Platform/Gfx/Metal/Device.cpp) and
the default settings (no debug defines, GSQuickShaderPermutation and
GSDeferredShaderPermutation false, GSRenderPath 3). It does not run the engine.
"""
import argparse
import hashlib
import itertools
import json
from pathlib import Path
import re
import sys
import zipfile

REPO = Path(__file__).resolve().parents[3]
RENDER = REPO / "Code/BF2/RendDX9"
GEOM = REPO / "Code/BF2/Geom"
DEFAULT_MOD = (Path.home() / "Applications/Battlefield 2.app/Contents/SharedSupport/prefix/drive_c"
               / "Battlefield 2/mods/bf2")


def src(path):
    return (REPO / path).read_text(encoding="utf-8", errors="replace")


def need(text, literal, where):
    if literal not in text:
        raise ValueError(f"Engine source drift: {literal!r} no longer in {where}; review enumerate_requests.py")


# ---------------------------------------------------------------------------------------------
# Device and engine tables, extracted from source (drift refuses).

def metal_caps():
    text = src("Code/BF2/Platform/Gfx/Metal/Device.cpp")
    def value(name):
        m = re.search(r"\bc\." + name + r"\s*=\s*(0x[0-9a-fA-F]+|\d+)", text)
        if not m:
            raise ValueError(f"Metal caps field {name} not found")
        return int(m[1], 0)
    filt = value("TextureFilterCaps")
    caps = dict(ps=value("PixelShaderVersion") & 0xFFFF, vs=value("VertexShaderVersion") & 0xFFFF,
                max_aniso=value("MaxAnisotropy"), min_aniso=bool(filt & 0x400), mag_aniso=bool(filt & 0x4000000),
                vendor=0, multisample=False, alpha_to_coverage=False,
                ps20_swizzle=bool(int(re.search(r"c\.PS20Caps\s*=\s*\{\s*(0x[0-9a-fA-F]+|\d+)", text)[1], 0) & 1))
    # GetAdapterIdentifier zero-fills the identifier (VendorId 0); CheckDeviceMultiSampleType
    # refuses everything but D3DMULTISAMPLE_NONE.
    need(text, "*out = {};", "Device.cpp GetAdapterIdentifier")
    need(text, "samples != D3DMULTISAMPLE_NONE", "Device.cpp CheckDeviceMultiSampleType")
    return caps


def ra_tables():
    text = src("Code/BF2/RendDX9/Ra/EffectManager.cpp")
    def table(name):
        m = re.search(r"\b" + name + r"\[\d+\]\[2\]\s*=\s*\{(.*?)\};", text, re.S)
        if not m:
            raise ValueError(f"Missing engine profile table {name}")
        return re.findall(r'\{\s*"([^"]+)",\s*"([^"]+)"\s*\}', m[1])
    mins = re.search(r"kMinPsVersion\[5\]\s*=\s*\{([^}]+)\}", text)
    thresholds = [int(v, 16) for v in re.findall(r"0x([0-9a-fA-F]+)u", mins[1])]
    for literal in ('defines[3].Name = "_FORCE_1_3_SHADERS_"; defines[3].Definition = "0";',
                    'defines[4].Name = "LOWPSMODEL";          defines[4].Definition = "ps_1_4";',
                    'if (g_noPreshader)      flags  = 0x100u;', 'unsigned char g_noPreshader      = 1;',
                    'const int firstPs = quick ? 3 : 0;', "#define RAPATH %i \\n", "#define A2MEnable 0x%X \\n"):
        need(text, literal, "Ra/EffectManager.cpp")
    return dict(vs=table("kVsModel"), ps=table("kPsModel"), psversion=table("kPsVersion"),
                force14=table("kForce14"), min_ps=thresholds)


def ra_attempts(caps, tables):
    """compileEffect's profile loop: vs outer, ps inner, capability/vendor skips."""
    out = []
    force = tables["force14"][1 if caps["ps"] < 0x200 else 0]
    for vs in tables["vs"]:
        for i, (ps, ver) in enumerate(zip(tables["ps"], tables["psversion"])):
            if caps["vendor"] == 4098 and i == 2 and not caps["ps20_swizzle"]:  # ATI without arbitrary swizzle (EffectManager.cpp skipPs2a)
                continue
            if caps["vendor"] == 4318 and i == 3:      # nvidia skips ps_2_b
                continue
            if caps["ps"] < tables["min_ps"][i]:
                continue
            # Engine macro order: VSMODEL, PSMODEL, _FORCE_1_4_SHADERS_, _FORCE_1_3_SHADERS_, LOWPSMODEL, PSVERSION
            out.append([list(vs), list(ps), list(force), ["_FORCE_1_3_SHADERS_", "0"], ["LOWPSMODEL", "ps_1_4"], list(ver)])
    return out


def ra_defines(caps, raPath, filterQuality):
    """EffectManager::rebuildShaderDefines + empty rebuildDebugDefines: the virtual RaDefines.fx bytes."""
    s = "#define NVIDIA %d\n" % (caps["vendor"] == 4318)
    s += "#define ATI %d\n" % (caps["vendor"] == 4098)
    ps3, vs3 = caps["ps"] >> 8, caps["vs"] >> 8
    s += "#define SM30 0\n" if (ps3 < 3 or vs3 < 3) else "#define SM30 1\n"
    s += "#define SM20 0\n" if (ps3 < 2 or vs3 < 2) else "#define SM20 1\n"
    s += "#define RAPATH %i \n" % raPath
    s += "#define HASALPHA2MASK %i \n" % int(caps["multisample"] and caps["alpha_to_coverage"])
    s += "#define Alpha2Mask AdaptiveTess_Y \n" if caps["vendor"] == 4318 else "#define Alpha2Mask PointSize \n"
    s += "#define A2MEnable 0x%X \n" % 0
    s += "#define A2MDisable 0x%X \n" % 0
    stm_mip, bm_mip = "#define FILTER_STM_NORM_MIP LINEAR \n", "#define FILTER_BM_NORM_MIP LINEAR \n"
    if filterQuality in (1, 2):
        if filterQuality == 1:
            stm_mip = "#define FILTER_STM_NORM_MIP POINT \n"
        bm_mip = "#define FILTER_BM_NORM_MIP POINT \n"
        s += ("#define FILTER_STM_DIFF_MIN LINEAR \n#define FILTER_STM_DIFF_MAG LINEAR \n"
              "#define FILTER_BM_DIFF_MIN LINEAR \n#define FILTER_BM_DIFF_MAG LINEAR \n")
    elif filterQuality == 3:
        bm, stm = min(caps["max_aniso"], 4), min(caps["max_aniso"], 2)
        mn = "ANISOTROPIC" if caps["min_aniso"] else "LINEAR"
        mg = "ANISOTROPIC" if caps["mag_aniso"] else "LINEAR"
        s += "#define FILTER_STM_DIFF_MIN %s \n" % mn + "#define FILTER_STM_DIFF_MAG %s \n" % mg
        s += "#define FILTER_STM_DIFF_MAX_ANISOTROPY %i\n" % stm
        s += "#define FILTER_BM_DIFF_MIN %s \n" % mn + "#define FILTER_BM_DIFF_MAG %s \n" % mg
        s += "#define FILTER_BM_DIFF_MAX_ANISOTROPY %i\n" % bm
    return (s + stm_mip + bm_mip).encode()


def legacy_macros(caps, filterQuality, terrainQuality, raPath=0):
    """ShaderManager::updateShaderDefines for a non-nvidia device, in engine order."""
    inc = src("Code/BF2/RendDX9/ShaderManagerDefines.inc")
    table = re.findall(r'\{\s*"([^"\n]+)",\s*"([^"\n]*)"\s*\}', inc)
    if len(table) != 120:
        raise ValueError("ShaderManager vocabulary changed; review enumerate_requests.py")
    sm = src("Code/BF2/RendDX9/ShaderManager.cpp")
    for literal in ('pushDefine(sm, "0");', 'pushDefine(sm, sm->flag78 ? "ps_2_a" : "ps_2_0");',
                    '"%X",', 'pushDefine(sm, "FILTER_PARTICLE_MIP"); pushDefine(sm, roadMip);'):
        need(sm, literal, "ShaderManager.cpp")
    nv, ati = caps["vendor"] == 4318, caps["vendor"] == 4098
    sm30 = not ((caps["ps"] >> 8) < 3 or (caps["vs"] >> 8) < 3)
    # RAPATH: ShaderManager::updateShaderDefines pushes the Renderer's raPath (+0x6CC, PE
    # 0x10023793), or "0" while no Renderer exists; the caller enumerates both.
    special = dict(RAPATH=str(raPath),
                   USEPARTIALPRECISIONTYPES="1" if nv else "0", PS2_EXT="ps_2_a" if nv else "ps_2_0",
                   NVIDIA="1" if nv else "0", ATI="1" if ati else "0", SM30="1" if sm30 else "0",
                   IS_NV4X="0", HASALPHA2MASK=str(int(caps["multisample"] and caps["alpha_to_coverage"])))
    out = []
    for name, value in table:
        if name == "Alpha2Mask":
            value = "AdaptiveTess_Y" if nv else value
        out.append([name, special.get(name, value)])
    out += [["A2MEnable", "%X" % 0], ["A2MDisable", "%X" % 0]]
    out += [["_FORCE_1_4_SHADERS_", "1" if (caps["ps"] >> 8) < 2 else "0"], ["_FORCE_1_3_SHADERS_", "0"]]
    if filterQuality in (1, 2):
        out += [["FILTER_TRN_DIFF_MIN", "LINEAR"], ["FILTER_TRN_DIFF_MAG", "LINEAR"],
                ["FILTER_ROAD_DIFF_MIN", "LINEAR"], ["FILTER_ROAD_DIFF_MAG", "LINEAR"]]
    elif filterQuality == 3:
        mn = "ANISOTROPIC" if caps["min_aniso"] else "LINEAR"
        mg = "ANISOTROPIC" if caps["mag_aniso"] else "LINEAR"
        a = caps["max_aniso"]
        out += [["FILTER_TRN_DIFF_MIN", mn], ["FILTER_TRN_DIFF_MAG", mg], ["FILTER_TRN_DIFF_MAX_ANISOTROPY", str(min(a, 2))],
                ["FILTER_ROAD_DIFF_MIN", mn], ["FILTER_ROAD_DIFF_MAG", mg], ["FILTER_ROAD_DIFF_MAX_ANISOTROPY", str(min(a, 4))]]
    trn = "POINT" if filterQuality == 1 else "LINEAR"
    road = "POINT" if filterQuality in (1, 2) else "LINEAR"
    out += [["FILTER_TRN_MIP", trn], ["FILTER_ROAD_MIP", road], ["FILTER_PARTICLE_MIP", road]]
    if terrainQuality == 2:
        out += [["HIGHTERRAIN", "0"], ["MIDTERRAIN", "1"]]
    elif terrainQuality == 3:
        out += [["HIGHTERRAIN", "1"], ["MIDTERRAIN", "0"]]
    names = [n for n, _ in out]
    if len(set(names)) != len(names):
        raise ValueError("Duplicate legacy macro; the cache capture would refuse it")
    return out


# Fixed ShaderManager::getEffect call sites: (raw path, source file, class).
# class: client = requested by the game client; conditional = only on a setting/debug path;
# editor = editor image only (not built into the client cache).
LEGACY_CALLS = [
    ("mods/xpack/Shaders/PortedFontShader", "Code/BF2/GeomMisc/FontSystem/FontSystem.cpp", "client"),
    ("mods/xpack/Shaders/PortedFontShader", "Code/BF2/GeomMisc/OverlayTexture.cpp", "client"),
    ("mods/xpack/Shaders/LoadingScreen", "Code/BF2/GeomMisc/MenuRend/FSMoviePlayer.cpp", "client"),
    ("mods/xpack/shaders/PortedMenuShader", "Code/BF2/GeomMisc/MenuRend/QuadDrawer.cpp", "client"),
    ("mods/xpack/Shaders/FSQuadDrawer", "Code/BF2/GeomMisc/FSQuadDrawer.cpp", "client"),
    ("mods/xpack/Shaders/Font", "Code/BF2/GeomMisc/FontSystem/SwiffFont.cpp", "client"),
    ("Shaders/SwiffMenu", "Code/BF2/Geom/SwiffRender.cpp", "client"),
    ("mods/xpack/shaders/SkyDome", "Code/BF2/Geom/SkyDome.cpp", "client"),
    ("mods/xpack/shaders/RoadCompiled", "Code/BF2/Geom/RoadCompiledRenderer.cpp", "client"),
    ("mods/xpack/shaders/Particles", "Code/BF2/Geom/ParticleSystem/ParticleRendererConstruction.inl", "client"),
    ("mods/xpack/shaders/Trail", "Code/BF2/Geom/ParticleSystem/ParticleRendererConstruction.inl", "client"),
    ("mods/xpack/shaders/NonScreenAlignedParticles", "Code/BF2/Geom/ParticleSystem/ParticleRendererConstruction.inl", "client"),
    ("mods/xpack/shaders/Decals", "Code/BF2/Geom/DecalSystem/DecalRenderer.cpp", "client"),
    ("mods/xpack/Shaders/StaticMesh", "Code/BF2/Geom/StaticMeshRenderer.cpp", "client"),
    ("mods/xpack/Shaders/BundledMesh", "Code/BF2/Geom/StaticMeshRenderer.cpp", "client"),
    ("mods/xpack/shaders/Nametag", "Code/BF2/Geom/NameTagRenderer.cpp", "client"),
    ("Shaders/Undergrowth", "Code/BF2/Geom/UndergrowthSystemLoad.cpp", "client"),
    ("Shaders/TerrainShader", "Code/BF2/Geom/Terrain.cpp", "client"),
    ("Shaders/\" + \"SkinnedMesh", "Code/BF2/Geom/SkinnedMesh.cpp", "client"),
    ("Shaders/\" + \"MeshParticleMesh", "Code/BF2/Geom/ParticleSystem/MeshParticleMesh/MeshParticleMeshTemplate.cpp", "client"),
    ("Shaders/LightManager_r3x0", "Code/BF2/Geom/XpLightManager.cpp", "conditional:GSRenderPath=2"),
    ("Shaders/%s\" % DebugLineGraph", "Code/BF2/GeomMisc/DebugLineGraphD3d.cpp", "conditional:debug line graph"),
    # Debug drawers the client creates at startup (recovered GeomMisc; their effects are requested in
    # normal play, seen in fresh-cache client captures 2026-11-12).
    ("Shaders/\" + m_shaderName (DebugCircleShader)", "Code/BF2/GeomMisc/DebugCircleDrawer.cpp", "client"),
    ("Shaders/\" + m_shaderName (DebugLineGraph)", "Code/BF2/GeomMisc/DebugBlockGraphD3D.cpp", "client"),
    ("Shaders/SplineShader", "Code/BF2/GeomMisc/EditorSpline.cpp", "editor"),
    ("Shaders/Road", "Code/BF2/Geom/RoadEditable.cpp", "editor"),
    ("shaders/LightGeom", "Code/BF2/Geom/LightmapGeneration/LightmapGeneration.cpp", "editor"),
    ("shaders/QuadGeom", "Code/BF2/Geom/LightmapGeneration/LightmapGeneration.cpp", "editor"),
    ("Shaders/TerrainEditorShader", "Code/BF2/Geom/TerrainEditable.cpp", "editor"),
]
# Retail RendDX9.dll requests whose callers are not recovered in this tree (string evidence
# in the shipped DLL; weather and post-production paths). Built so the cache covers them
# once those callers exist; the current port never requests them.
LEGACY_RETAIL = [("Shaders/Rain", "unrecovered"), ("Shaders/Lightning", "unrecovered"),
                 ("mods/xpack/Shaders/PostProduction_nv3x", "unrecovered"),
                 ("Shaders/DebugD3DXMeshShapeShader", "unrecovered-debug")]
# Literal evidence actually present at each call site (composite names are built in pieces).
LEGACY_EVIDENCE = {"Shaders/\" + \"SkinnedMesh": ('"SkinnedMesh"', "Shaders/SkinnedMesh"),
                   "Shaders/\" + \"MeshParticleMesh": ('"MeshParticleMesh"', "Shaders/MeshParticleMesh"),
                   "Shaders/%s\" % DebugLineGraph": ('"DebugLineGraph"', "Shaders/DebugLineGraph"),
                   "Shaders/\" + m_shaderName (DebugCircleShader)": ('m_shaderName("DebugCircleShader")', "Shaders/DebugCircleShader"),
                   "Shaders/\" + m_shaderName (DebugLineGraph)": ('m_shaderName("DebugLineGraph")', "Shaders/DebugLineGraph")}


def legacy_effects(retail_dll=None):
    out = {}
    calls = list(LEGACY_CALLS)
    if retail_dll is not None:
        image = retail_dll.read_bytes()
        for raw, cls in LEGACY_RETAIL:
            if raw.encode() not in image:
                raise ValueError(f"{raw} not found in {retail_dll}")
            calls.append((raw, None, cls))
    for raw, where, cls in calls:
        evidence, path = LEGACY_EVIDENCE.get(raw, ('"' + raw + '"', raw))
        if where is None:
            where = "RendDX9.dll string (caller not recovered)"
        else:
            need(src(where), evidence, where)
        # ShaderManager::getEffect key: FileManager::stripModPath (cut before "shaders"), lowercase.
        low = path.lower()
        cut = low.find("shaders")
        key = (path[cut:] if "mods/" in low and cut > 0 else path).lower()
        rec = out.setdefault(key, dict(key=key, raw=path, file=path.split("/")[-1] + ".fx", classes=set(), sites=[]))
        rec["classes"].add(cls)
        rec["sites"].append(where)
    for rec in out.values():
        rec["class"] = "client" if "client" in rec["classes"] else sorted(rec["classes"])[0]
        rec["class"] = rec["class"].replace("unrecovered-debug", "unrecovered")
        rec["classes"] = sorted(rec["classes"])
    return out


# Ra fixed getShader names (all also reached by the directory scan): name -> call site.
RA_FIXED = [("Default", "Code/BF2/Geom/BundledMeshTemplate.cpp"), ("BMZOnly", "Code/BF2/Geom/BundledMeshTemplate.cpp"),
            ("BMadditive", "Code/BF2/Geom/BundledMeshDraw.cpp"), ("LeafOG", "Code/BF2/Geom/Overgrowth/OverGrowthCells.cpp"),
            ("TrunkOG", "Code/BF2/Geom/Overgrowth/OverGrowthCells.cpp"), ("Water2D", "Code/BF2/Geom/Terrain.cpp"),
            ("WaterDistant2D", "Code/BF2/Geom/Terrain.cpp"), ("WaterSurrounding2D", "Code/BF2/Geom/Terrain.cpp"),
            ("Water3D", "Code/BF2/Geom/Terrain.cpp"), ("Leafpointlight", "Code/BF2/Geom/StaticMeshTemplate.cpp"),
            ("TrunkSTMDetailShadowed", "Code/BF2/Geom/StaticMeshTemplate.cpp"), ("TrunkSTMBaseShadowed", "Code/BF2/Geom/StaticMeshTemplate.cpp"),
            ("LeafShadowed", "Code/BF2/Geom/StaticMeshTemplate.cpp"), ("LeafpointlightShadowed", "Code/BF2/Geom/StaticMeshTemplate.cpp"),
            ("TrunkSTMDetail", "Code/BF2/Geom/StaticMeshTemplate.cpp"), ("TrunkSTMBase", "Code/BF2/Geom/StaticMeshTemplate.cpp"),
            ("Leaf", "Code/BF2/Geom/StaticMeshTemplate.cpp"), ("Road", "Code/BF2/Geom/StaticMeshTemplate.cpp"),
            ("RoadDetailNoBlend", "Code/BF2/Geom/StaticMeshTemplate.cpp"), ("RoadDetail", "Code/BF2/Geom/StaticMeshTemplate.cpp"),
            ("Water", "Code/BF2/Geom/StaticMeshTemplate.cpp"), ("WaterBase", "Code/BF2/Geom/StaticMeshTemplate.cpp"),
            ("default", "Code/BF2/RendDX9/RenderQueues.cpp")]

# Mesh families: (mfx, technique prefix, bit-name table source, table symbol).
FAMILIES = [("RaShaderBM", "BM", "Code/BF2/Geom/BundledMeshTemplate.cpp", r"kBits\[\]\s*=\s*\{(.*?)\};"),
            ("RaShaderSM", "SM", "Code/BF2/Geom/SkinnedMesh.cpp", r"kSkinnedMeshShaderBitNames\[\]\s*=\s*\{(.*?)\};"),
            ("RaShaderSTM", "STM", "Code/BF2/Geom/StaticMeshTemplate.cpp", r"kStaticMeshShaderBitNames\[\]\s*=\s*\{(.*?)\};")]


def bit_names(path, pattern):
    m = re.search(pattern, src(path), re.S)
    if not m:
        raise ValueError(f"Missing shader bit table in {path}")
    return re.findall(r'"([^"]+)"', re.sub(r"//[^\n]*", "", m[1]))


# ---------------------------------------------------------------------------------------------
# Content: mounted Shaders archive, read in place.

class Content:
    def __init__(self, mod):
        self.mod = mod
        mounts = re.findall(r"fileManager\.mountArchive\s+(\S+)\s+(\S+)",
                            (mod / "ClientArchives.con").read_text(errors="replace"))
        self.archives = [(mod / z, m) for z, m in mounts]
        self.files = {}           # lowercase name in Shaders/ -> (archive, member, bytes)
        self.listing = []         # findFiles("shaders/") names, archive spelling
        for z, mount in self.archives:
            if mount.lower() != "shaders":
                continue
            with zipfile.ZipFile(z) as archive:
                for info in archive.infolist():
                    if info.is_dir() or "/" in info.filename.strip("/"):
                        continue
                    self.files.setdefault(info.filename.lower(), (z.name, info.filename, archive.read(info)))
                    self.listing.append(info.filename)
        loose = mod / "shaders"
        self.loose = sorted(p.name for p in loose.iterdir()) if loose.is_dir() else []
        if self.loose:
            raise ValueError(f"Loose shader overrides in {loose} are not modeled (FileManager precedence)")
        self.other_sources = []
        for z in sorted(mod.glob("*.zip")) + sorted(mod.glob("Levels/*/*.zip")):
            if any(z == a for a, m in self.archives if m.lower() == "shaders"):
                continue
            with zipfile.ZipFile(z) as archive:
                hits = [n for n in archive.namelist() if n.lower().endswith((".fx", ".dfx", ".mfx"))]
            if hits:
                self.other_sources.append((str(z.relative_to(mod)), hits))

    def read(self, name):
        n = name.replace("\\", "/").lower()
        for prefix in ("mods/bf2/", "mods/xpack/"):
            if n.startswith(prefix):
                n = n[len(prefix):]
        if not n.startswith("shaders/"):
            raise ValueError(f"Unexpected shader path {name}")
        hit = self.files.get(n[len("shaders/"):])
        if hit is None:
            return None
        data = hit[2]
        if OBFUSCATION_MAGIC_HEX is None:
            raise ValueError("Obfuscation magic not found in Effect.cpp")
        if data[:10] == bytes.fromhex(OBFUSCATION_MAGIC_HEX):
            raise ValueError(f"Obfuscated shader {name}: GSLoadObfuscated decoding not modeled")
        return data


OBFUSCATION_MAGIC_HEX = None


def obfuscation_magic():
    text = src("Code/BF2/RendDX9/Effect.cpp")
    m = re.search(r"kObfuscationMagic\[[^\]]*\]\s*=\s*\{([^}]*)\}", text)
    if m:
        vals = [int(v, 0) for v in re.findall(r"0x[0-9a-fA-F]+|\d+", m[1])][:10]
        return bytes(vals).hex()
    return None


# ---------------------------------------------------------------------------------------------
# MetaFx: engine-side literal regex on raw lines; D3DX-side reflection on the parsed effect.

def uncomment(text):
    return re.sub(r'"(?:\\.|[^"\\])*"|/\*.*?\*/|//[^\n]*',
                  lambda m: m[0] if m[0].startswith('"') else " " + "\n" * m[0].count("\n"), text, flags=re.S)


def engine_literals(raw):
    pattern = re.compile(r"^const\s+(?:(?:bool)|(?:int)|(?:dword))\s+([a-zA-Z0-9_]+)\s+:\s+LITERAL.*")
    names = []
    for line in raw.replace("\0", "\n").split("\n"):
        m = pattern.search(line.rstrip("\r"))
        if m:
            names.append(m[1])
    return names


def string_arrays(text):
    """Top-level `string NAME[] = { ... };` declarations: name -> flattened string elements."""
    out = {}
    for m in re.finditer(r"\bstring\s+(\w+)\s*\[\s*\]\s*=\s*\{", text):
        start = m.end() - 1
        depth, j, values = 0, start, []
        while True:
            c = text[j]
            if c == '"':
                end = text.index('"', j + 1)
                values.append(text[j + 1:end])
                j = end
            elif c == "{":
                depth += 1
            elif c == "}":
                depth -= 1
                if depth == 0:
                    break
            j += 1
        out.setdefault(m[1], values)
    return out


def preprocess_metadata(text, defines=None):
    """The metadata compile's preprocessor (MetaFx::shaderMacros: _FORCE_1_4_SHADERS_ = "0" on
    the Metal caps, PS 3.0 and EffectManager flag 0x40 clear). xpack's RaShaderSTM.mfx has
    #if/#else/#endif around requirement strings; anything beyond simple conditionals stops."""
    defines = {"_FORCE_1_4_SHADERS_": 0} if defines is None else defines
    def cond(expr):
        e = expr.strip()
        neg = e.startswith("!")
        e = e[1:].strip() if neg else e
        if re.fullmatch(r"\d+", e):
            v = int(e)
        elif re.fullmatch(r"\w+", e):
            v = int(defines.get(e, 0))
        else:
            raise ValueError(f"Metadata preprocessor condition not modelled: {expr!r}")
        return (not v) if neg else bool(v)
    out, stack = [], []
    for line in text.split("\n"):
        m = re.match(r"\s*#\s*(\w+)\s*(.*)$", line)
        if not m:
            out.append(line if all(stack) else "")
            continue
        d, arg = m[1], m[2]
        if d == "if":
            stack.append(cond(arg))
        elif d in ("ifdef", "ifndef"):
            stack.append((arg.strip() in defines) == (d == "ifdef"))
        elif d == "else":
            stack[-1] = not stack[-1]
        elif d == "endif":
            stack.pop()
        else:
            raise ValueError(f"Metadata preprocessor directive not modelled: #{d}")
        out.append("")
    if stack:
        raise ValueError("Unterminated #if in metadata")
    return "\n".join(out)


def metafx(raw_bytes, bits):
    raw = raw_bytes.decode("cp1252")
    text = preprocess_metadata(uncomment(raw))
    # Separate literal annotation blocks from top-level declarations.
    annotations, top, pos = {}, [], 0
    for m in re.finditer(r"\bconst\s+(?:bool|int|dword)\s+(\w+)\s*:\s*LITERAL\s*(<)?", text):
        if m.start() < pos:
            continue
        top.append(text[pos:m.start()])
        if m[2]:
            end = text.index(">;", m.end())
            annotations[m[1]] = string_arrays(text[m.end():end])
            pos = end + 2
        else:
            annotations[m[1]] = {}
            pos = m.end()
    top.append(text[pos:])
    arrays = string_arrays("".join(top))
    literals = engine_literals(raw)
    structs = arrays.get("GenerateStructs", [])
    perms = arrays.get("GenerateShaderPermutations", [])
    mods = arrays.get("GenerateShaderModifiers", [])
    # readDeprecationList: "" separates rows, first row always exists, trailing row retained.
    rows = [[]]
    for v in arrays.get("DeprecationList", []) if "DeprecationList" in arrays else []:
        if v:
            rows[-1].append(v)
        else:
            rows.append([])
    if "DeprecationList" not in arrays:
        rows = None
    struct_values = {s: list(arrays.get(s + "Base", [])) for s in structs}
    struct_annotations = {s: {l: annotations[l][s] for l in literals if l in annotations and s in annotations[l]} for s in structs}
    return dict(literals=literals, structs=structs, values=struct_values, annotations=struct_annotations,
                permutations=perms, modifiers=mods, deprecation=rows, bits=bits)


def clean(name):
    return name.replace("_", "").replace("|", "")


def generated_names(fx):
    out = []
    for p in fx["permutations"]:
        out.append(p)
        out += [p + "|" + m for m in fx["modifiers"]]
    return out


def bit_from_name(names, name):
    for i, n in enumerate(names):
        if n.lower() == name.lower():
            return 1 << i
    return 0


def wrapper_text(fx, m_name, permutation):
    """MetaFx::generateShader, byte for byte (std::endl is "\n")."""
    enabled = set()
    for token in permutation.split("|") if permutation else []:
        if token in fx["literals"]:          # vc71Equal: case-sensitive; unknown names only log
            enabled.add(token)
    def value(l):
        return 1 if l in enabled else 0
    base = m_name.replace("\\", "/").split("/")[-1]
    o = "//\n// AutoGenerated shader file " + m_name + clean(permutation) + "\n//\n\n// Shader setting \n"
    for l in fx["literals"]:
        o += "#define " + l.upper() + " " + str(value(l)) + "\n"
    for l in fx["literals"]:
        o += "static const dword " + l.lower() + " = " + str(value(l)) + ";\n"
    texsets = []
    for s in fx["structs"]:
        members = list(dict.fromkeys(fx["values"][s]))
        for l in fx["literals"]:
            if value(l) and l in fx["annotations"][s]:
                for v in fx["annotations"][s][l]:
                    if v not in members:
                        members.append(v)
        if not members:
            continue
        o += "\nstring " + s + "[] = {\n" + ",\n".join('\t"' + v + '"' for v in members) + "\n};\n"
        for v in members:
            if "2D" in v and v[0] == "T":
                texsets.append("Tex" + v[1:len(v) - 2] + "Ind")
    o += "#define NUM_LIGHTS 1 \n#define NUM_TEXSETS " + str(len(texsets)) + "\n"
    for i, t in enumerate(texsets):
        o += "#define " + t + " " + str(i) + "\n"
    o += '\n#include "shaders/' + base + '.fx"\n'
    return o.encode("cp1252")


def masks(fx):
    if fx["deprecation"] is None:
        return None
    vec, acc = [0xFFFFFFFF], 0
    for row in fx["deprecation"]:
        g = 0
        for n in row:
            g |= bit_from_name(fx["bits"], n)
        acc |= g
        vec.append(~acc & 0xFFFFFFFF)
    while len(vec) < 4:
        vec.append(0xFFFFFFFF)
    return vec


def registered(fx, raPath, shadows):
    """staticMeshBuildTechniqueMasks: the (bits, cleaned name) pairs built for one setting."""
    perm_bits = {}
    for g in generated_names(fx):
        bits = 0
        for t in g.split("|"):
            bits |= bit_from_name(fx["bits"], clean(t))
        perm_bits.setdefault(bits, clean(g))      # std::map::insert keeps the first
    vec = masks(fx)
    q = raPath if raPath <= len(vec) else len(vec)
    if q >= len(vec):
        raise ValueError("Quality index past the mask vector (retail reads out of bounds)")
    mask = vec[q]
    if not shadows:
        mask &= ~bit_from_name(fx["bits"], "hasshadow") & 0xFFFFFFFF
    return [(b, n) for b, n in sorted(perm_bits.items()) if (mask & b) == b]


# ---------------------------------------------------------------------------------------------

def configs():
    """lighting quality[2] -> raPath (3/1/0); low lighting forces dynamic shadows off."""
    out = []
    for lighting, shadows, filt, terrain in itertools.product((1, 2, 3), (0, 1), (1, 2, 3), (1, 2, 3)):
        if lighting == 1 and shadows:
            continue
        out.append(dict(id=f"L{lighting}S{shadows}F{filt}T{terrain}", lighting=lighting,
                        raPath={1: 3, 2: 1, 3: 0}[lighting], shadows=shadows, filter=filt, terrain=terrain))
    return out


def reaches_radefines(content, data, seen=None):
    """s2's literal-include closure (all branches, comments removed) reaches RaDefines.fx?"""
    seen = set() if seen is None else seen
    for name in re.findall(r'^\s*#\s*include\s+[<"]([^">]+)[">]', uncomment(data.decode("cp1252")), re.M):
        low = name.replace("\\", "/").lower()
        if low.endswith("shaders/radefines.fx"):
            return True
        if low in seen:
            continue
        seen.add(low)
        child = content.read(low if low.startswith(("shaders/", "mods/")) else "shaders/" + low)
        if child is not None and reaches_radefines(content, child, seen):
            return True
    return False


def sha(b):
    return hashlib.sha256(b).hexdigest()


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--mod", type=Path, default=DEFAULT_MOD, help="read-only mod folder (zips read in place)")
    p.add_argument("--mod-name", default="bf2", choices=("bf2", "xpack"))
    p.add_argument("--output", type=Path, default=REPO / "out/c-fx/requests")
    p.add_argument("--config", action="append", default=[], help="restrict to config IDs (e.g. L3S1F3T3)")
    p.add_argument("--classes", default="client", help="legacy classes: client[,conditional][,unrecovered][,editor]")
    p.add_argument("--caps", default="metal", choices=("metal", "metal-ati", "s1b-sm2"),
                   help="device caps: the Metal device, the Metal device on an AMD GPU (VendorId 0x1002: ATI=1 "
                        "defines), or s1b's SM2 snapshot (validation against s1b/s2 keys)")
    a = p.parse_args()
    out = a.output.resolve()
    if not out.is_relative_to(REPO / "out"):
        raise ValueError("Output must be below this worktree's out/")
    global OBFUSCATION_MAGIC_HEX
    OBFUSCATION_MAGIC_HEX = obfuscation_magic()
    (out / "roots").mkdir(parents=True, exist_ok=True)
    (out / "virtual").mkdir(exist_ok=True)
    content = Content(a.mod)
    caps, tables = metal_caps(), ra_tables()
    if a.caps == "metal-ati":
        # Owner decision 2026-11-12: the adapter reports the real vendor; AMD Macs report 0x1002.
        caps.update(vendor=4098)
    if a.caps == "s1b-sm2":
        # engine_inputs.snapshot(): non-vendor SM2, 16x anisotropy, min and mag anisotropy.
        caps.update(ps=0x200, vs=0x200, max_aniso=16, min_aniso=True, mag_aniso=True)
    attempts = ra_attempts(caps, tables)
    xpack = a.mod_name == "xpack"
    prefix = "mods/xpack/" if xpack else "mods/bf2/"
    classes = set(a.classes.split(","))
    cfgs = [c for c in configs() if not a.config or c["id"] in a.config]

    roots = {}

    def root(name, data):
        h = sha(data)
        path = out / "roots" / (h[:16] + "-" + re.sub(r"[^A-Za-z0-9_.-]", "_", name)[-60:])
        if h not in roots:
            path.write_bytes(data)
            roots[h] = str(path)
        return roots[h], h

    virtual = {}

    def radefines(cfg):
        data = ra_defines(caps, cfg["raPath"], cfg["filter"])
        h = sha(data)
        if h not in virtual:
            d = out / "virtual" / h[:16]
            d.mkdir(exist_ok=True)
            (d / "RaDefines.fx").write_bytes(data)
            virtual[h] = str(d)
        return virtual[h], h

    requests = {}       # request identity -> record
    closure_cache = {}
    groups = []         # (config, effect) -> ordered request identities (engine attempt order)

    def add(identity, rec, cfg):
        r = requests.setdefault(identity, dict(rec, configs=[]))
        r["configs"].append(cfg["id"])
        return identity

    # Ra metadata reflection compiles (MetaFx::generate): one per .mfx, cfg-independent bytes.
    fams = {}
    for mfx, tprefix, bits_src, pattern in FAMILIES:
        listed = next((n for n in content.listing if n.lower() == (mfx + ".mfx").lower()), None)
        if not listed:
            raise ValueError(f"{mfx}.mfx missing from the Shaders archive")
        data = content.read("shaders/" + listed)
        fx = metafx(data, bit_names(bits_src, pattern))
        # The engine enumerates shaders/*.mfx through FileManager::findFiles, whose ZipArchive index
        # holds lowercased entry names (retail ELF ZipArchive::readEntries 0x0848B860 calls
        # getLowerCase; findFiles 0x0848ADD0 returns those keys): MetaFx's name, the generated
        # header and its #include are lowercase (EffectManager.cpp loadMetaFx, RaShaderPermutator.cpp).
        fx["m_name"] = prefix + "shaders/" + listed[:-4].lower()
        fx["prefix"] = tprefix
        fx["raw"] = data + b"\0"          # m_sourceSize counts the terminator
        fams[mfx] = fx
        cleaned = [clean(g) for g in generated_names(fx)]
        if len(set(c.lower() for c in cleaned)) != len(cleaned):
            raise ValueError(f"{mfx}: cleaned permutation names collide in the memory-file map")

    # Direct Ra directory scan (raLoadAllShaders flags 3) plus "default".
    direct = {}
    for n in content.listing:
        low = n.lower()
        if "rashader" in low and ".fx" in low and "common" not in low and low not in ("rashaderstm.fx", "rashadersm.fx", "rashaderbm.fx"):
            stem = low.rsplit("/", 1)[-1].rsplit(".", 1)[0]
            if len(stem) >= 8:
                direct[stem[8:]] = n
    fixed = []
    for name, where in RA_FIXED:
        need(src(where), '"' + name + '"', where)
        fixed.append(dict(name=name, site=where, resolves=name.lower() in direct))

    dll = a.mod.parents[1] / "RendDX9.dll"
    legacy = legacy_effects(dll if dll.is_file() else None)
    summary = dict(configs={}, mod=str(a.mod), mod_name=a.mod_name, caps=caps,
                   attempts=[[v for _, v in at[:2]] for at in attempts],
                   archives=[str(z.name) + " -> " + m for z, m in content.archives],
                   other_shader_sources=content.other_sources, loose_shader_files=content.loose,
                   ra_direct=sorted(direct), ra_fixed=fixed,
                   legacy={k: dict(v, sites=v["sites"]) for k, v in legacy.items()})

    for cfg in cfgs:
        vroot, vhash = radefines(cfg)
        counts = dict(ra_metadata=0, ra_direct=0, ra_generated=0, legacy=0, attempts=0)
        for mfx, fx in fams.items():
            path, h = root(mfx + ".mfx", fx["raw"])
            ident = ("ra_metadata", h)
            add(ident, dict(kind="ra_metadata", effect=mfx + ".mfx", source=path, root_sha256=h,
                            macros=[["_FORCE_1_4_SHADERS_", "1" if (caps["ps"] >> 8) < 2 else "0"]],
                            flags=0x100, include_mode="ra", virtual_root=vroot, profile_rank=0), cfg)
            groups.append(dict(config=cfg["id"], effect=mfx + ".mfx", kind="ra_metadata", attempts=[ident]))
            counts["ra_metadata"] += 1

        def ra_group(effect, kind, data, extra):
            def virtual_in_key(h, data):
                if h not in closure_cache:
                    closure_cache[h] = reaches_radefines(content, data)
                return closure_cache[h]
            path, h = root(effect, data)
            seq = []
            for rank, macros in enumerate(attempts):
                # Without RaDefines.fx in the closure the virtual text is not part of the key.
                ident = ("ra", h, vhash if virtual_in_key(h, data) else None, rank)
                add(ident, dict(kind=kind, effect=effect, source=path, root_sha256=h, macros=macros,
                                flags=0x100, include_mode="ra", virtual_root=vroot, radefines_sha256=vhash,
                                profile_rank=rank, profile=macros[0][1] + "/" + macros[1][1], **extra), cfg)
                seq.append(ident)
            groups.append(dict(config=cfg["id"], effect=effect, kind=kind, attempts=seq))
            counts["attempts"] += len(seq)

        for name in sorted(direct):
            ra_group("shaders/rashader" + name + ".fx", "ra_direct", content.read("shaders/" + direct[name]), {})
            counts["ra_direct"] += 1
        for mfx, fx in fams.items():
            for bits, cname in registered(fx, cfg["raPath"], cfg["shadows"]):
                original = next(g for g in generated_names(fx) if clean(g) == cname)
                data = wrapper_text(fx, fx["m_name"], original)
                ra_group("shaders/rashader" + (fx["prefix"] + cname).lower() + ".fx", "ra_generated", data,
                         dict(family=mfx, permutation=original, bits=bits))
                counts["ra_generated"] += 1
        for key, rec in sorted(legacy.items()):
            if rec["class"].split(":")[0] not in classes:
                continue
            data = content.read(key + ".fx")
            if data is None:
                raise ValueError(f"Legacy effect {key}.fx missing from the Shaders archive")
            path, h = root(rec["file"], data)
            # A legacy effect created before the Renderer publishes its path has RAPATH 0, one
            # created later (DebugLineGraph, effects reloaded on a video change) the config's.
            for rp in sorted({0, cfg["raPath"]}):
                macros = legacy_macros(caps, cfg["filter"], cfg["terrain"], rp)
                mh = sha(json.dumps(macros).encode())
                ident = ("legacy", h, mh)
                add(ident, dict(kind="legacy", effect=key + ".fx", source=path, root_sha256=h, macros=macros,
                                flags=0x100, include_mode="legacy", virtual_root=vroot, profile_rank=0,
                                legacy_class=rec["class"]), cfg)
                groups.append(dict(config=cfg["id"], effect=key + ".fx", kind="legacy", attempts=[ident]))
                counts["legacy"] += 1
        summary["configs"][cfg["id"]] = dict(cfg, **counts)

    ids = {}
    records = []
    for i, (ident, rec) in enumerate(requests.items()):
        rid = f"{rec['kind']}-{Path(rec['effect']).stem}-{rec['profile_rank']}-" + sha(repr(ident).encode())[:10]
        ids[ident] = rid
        records.append(dict(id=rid, **rec))
    for g in groups:
        g["attempts"] = [ids[x] for x in g["attempts"]]
    # build-cache.py --manifest contract: id, source, compiler_defines (name -> value), flags, include_mode.
    jobs = [dict(id=r["id"], file=Path(r["effect"]).name, source=r["source"], kind=r["kind"],
                 compiler_defines={k: v for k, v in r["macros"]}, flags=r["flags"],
                 include_mode=r["include_mode"], virtual_root=r["virtual_root"], profile_rank=r["profile_rank"])
            for r in records]
    by_kind = {}
    for r in records:
        by_kind[r["kind"]] = by_kind.get(r["kind"], 0) + 1
    summary["unique_requests"] = len(records)
    summary["roots_without_radefines"] = sorted(Path(roots[h]).name for h, v in closure_cache.items() if not v)
    summary["unique_requests_by_kind"] = by_kind
    summary["unique_rank0_requests"] = sum(r["profile_rank"] == 0 for r in records)
    summary["unique_roots"] = len(roots)
    summary["radefines_variants"] = len(virtual)
    (out / "requests.json").write_text(json.dumps(records, indent=1) + "\n")
    (out / "groups.json").write_text(json.dumps(groups, indent=1) + "\n")
    (out / "jobs.json").write_text(json.dumps(jobs, indent=1) + "\n")
    (out / "summary.json").write_text(json.dumps(summary, indent=1, default=list) + "\n")
    print(json.dumps(dict(unique_requests=len(records), by_kind=by_kind, rank0=summary["unique_rank0_requests"],
                          roots=len(roots), radefines=len(virtual), configs=len(cfgs), groups=len(groups))))
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, ValueError, zipfile.BadZipFile) as error:
        print(error, file=sys.stderr)
        sys.exit(2)
