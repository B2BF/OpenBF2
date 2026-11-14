#!/usr/bin/env python3
"""[CHOSEN, not recovered] Audit native static-image dispatch, without running BF2.

Use the configured client's compilation database (including forced BuildConfig.h).
Clang computes slots; this tool never predicts an ABI from declaration order. Raw
tables additionally get compiler type checks against the consumer's member types.
Successful probes are cached against their compiler flags and transitive depfiles.
"""

import argparse
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
from pathlib import Path
import re
import shlex
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[2]
H = "dice::hfe::"


def side(header, cls, image="host", source=None, probe="getRef", subobject=None):
    return dict(header=header, cls=H + cls, image=image, source=source, probe=probe, subobject=subobject)


# A pair is a consumer declaration and the actual provider declaration, compiled
# independently: several images deliberately declare the same qualified name.
PAIRS = [
    ("class-manager", side("Code/RendDX9Abi/HfeAbi.h", "IClassManager", "renderer"),
     side("Code/BF2/Core/ClassManager.h", "ClassManager")),
    ("module", side("Code/BF2/Core/IModule.h", "IModule"),
     side("Code/RendDX9Abi/HfeAbi.h", "IModule", "renderer")),
    ("swiff-player", side("Code/BF2/Menu/ISwiffPlayer.h", "ui::ISwiffPlayer"),
     side("Code/BF2/External/gameswf/SwiffPlayer/SwiffPlayer.h", "ui::SwiffPlayer", "swiff")),
    ("renderer", side("Code/BF2/RendMisc/IRenderer.h", "dx9rend::IRenderer"),
     side("Code/BF2/RendDX9/BF2Render.cpp", "dx9rend::Renderer", "renderer")),
    ("render-view", side("Code/BF2/RendMisc/IRenderView.h", "dx9rend::IRenderView"),
     side("Code/BF2/RendMisc/RenderView.cpp", "Rend::RenderView", "renderer")),
    ("sound-engine", side("Code/BF2/Sound/NullSound.h", "ISoundEngine", probe="isInitialized"),
     side("Code/BF2/Sound/SoundEngine/BfSoundEngine.h", "BfSoundEngine", "audio", probe="isInitialized")),
    ("sound-template", side("Code/BF2/Sound/NullSound.h", "ISoundTemplate", probe="createSound"),
     side("Code/BF2/Sound/SoundEngine/Sound.h", "SoundTemplate", "audio", probe="createSound")),
    ("sound", side("Code/BF2/Sound/NullSound.h", "ISound", probe="play"),
     side("Code/BF2/Sound/SoundEngine/Sound.h", "Sound", "audio", probe="play")),
]

# Shared declarations still need both compilation contexts checked. A future
# image-conditional method/destructor must not silently split their layouts.
for header, cls, probe, image in [
    ("Core/SettingsRepostitory.h", "SettingsRepostitory", "getRef", "renderer"),
    ("Core/System.h", "System", "getRef", "renderer"),
    ("Core/EventManager.h", "EventManager", "getRef", "renderer"),
    ("IO/Console/Console.h", "io::Console", "getEchoErrors", "renderer"),
    ("IO/File/IFileSystem.h", "io::IFileManager", "getRef", "renderer"),
    ("IO/File/IFileSystem.h", "io::IFileSystem", "getRef", "renderer"),
    ("IO/Stream/IStream.h", "io::IStream", "getRef", "swiff"),
    ("IO/Stream/IStream.h", "io::INonlinearStream", "getRef", "swiff"),
    ("IO/File/FileChangeManager.h", "io::IFileChangeManager", "getRef", "renderer"),
    ("Menu/ISwiffHost.h", "ui::ISwiffHost", "getRef", "swiff"),
    ("Menu/ISwiffRender.h", "ui::ISwiffRender", "getRef", "swiff"),
    ("Sound/ISoundFileIO.h", "ISoundFileIO", "open(nullptr)", "audio"),
    ("Sound/ISoundTrace.h", "ISoundTrace", "error(nullptr, nullptr, 0)", "audio"),
    ("Geom/IGeometryTemplate.h", "geom::IGeometryTemplate", "getRef", "renderer"),
    ("Geom/GeometryTemplateManager.h", "geom::IGeometryTemplateManager", "getRef", "renderer"),
    ("Geom/ParticleSystem/ParticleSystemManager.h", "geom::IParticleSystemInstance", "setPaused(false)", "renderer"),
    ("Geom/ParticleSystem/ParticleSystemManager.h", "geom::IParticleSystemManager", "getRef", "renderer"),
    ("Geom/ParticleSystem/ParticleSystemManager.h", "geom::IParticleSystemTemplate", "getName", "renderer"),
    ("Geom/ParticleSystem/ParticleSystemManager.h", "geom::IParticleSpriteSystemTemplate", "getName", "renderer"),
    *[("Geom/ParticleSystem/ParticleSystemManager.h", "geom::IParticleSystemTemplateType" + str(i), "getName", "renderer") for i in (0, 1, 2, 4)],
    ("Geom/ParticleSystem/ParticleSystemManager.h", "geom::IParticleSystemEmitterTemplate", "getName", "renderer"),
    ("Geom/DecalSystem/IDecalManager.h", "geom::IDecalManager", "getRef", "renderer"),
    ("Geom/DecalSystem/IDecalTemplate.h", "geom::IDecalTemplate", "getName", "renderer"),
    ("Geom/IRoadTemplate.h", "geom::IRoadTemplate", "getRef", "renderer"),
    ("Geom/IRoadEditable.h", "geom::IRoad", "getRef", "renderer"),
    ("Geom/IRoadEditable.h", "geom::IRoadEditable", "getRef", "renderer"),
    ("Geom/IRoadEditableRenderer.h", "geom::IRoadEditableRenderer", "getRef", "renderer"),
    ("Geom/ITerrainEditable.h", "geom::ITerrainEditable", "getRef", "renderer"),
    ("Geom/HemiMapManager/HemiMapManager.h", "geom::IHemiMapManager", "getRef", "renderer"),
    ("Geom/LightmapGeneration/ILightSampling.h", "geom::ILightSampling", "getRef", "renderer"),
    ("Geom/LightmapGeneration/ILightSampleable.h", "geom::ILightSampleable", "getRef", "renderer"),
    ("Geom/LightmapGeneration/ILightmapGeneration.h", "geom::ILightmapGeneration", "getRef", "renderer"),
    ("GeomMisc/IEditorSpline.h", "geom::IEditorSpline", "getRef", "renderer"),
    ("GeomMisc/IPerlinNoise.h", "geom::IPerlinNoise", "getRef", "renderer"),
    ("Sound/ISoundEvaluator.h", "ISoundEvaluator", "getRef", "audio"),
    ("Sound/ISoundObstructionEvaluator.h", "ISoundObstructionEvaluator", "getRef", "audio"),
]:
    header = "Code/BF2/" + header
    PAIRS.append((cls, side(header, cls, probe=probe), side(header, cls, image, probe=probe)))

for name, view_header, view_cls, provider_header, provider_cls, image, subobject in [
    ("file-manager-implementation", "IO/File/IFileSystem.h", "io::IFileManager", "IO/File/FileManager.h", "io::FileManager", "host", None),
    ("swiff-host-implementation", "Menu/ISwiffHost.h", "ui::ISwiffHost", "Menu/SwiffHost.h", "SwiffHost", "host", None),
    ("geometry-registry-implementation", "Geom/GeometryTemplateManager.h", "geom::IGeometryTemplateManager", "Geom/GeometryTemplateManager.cpp", "geom::GeometryTemplateManager", "renderer", None),
    ("decal-manager-implementation", "Geom/DecalSystem/IDecalManager.h", "geom::IDecalManager", "Geom/DecalSystem/DecalManager.h", "geom::DecalManager", "renderer", None),
    ("decal-template-implementation", "Geom/DecalSystem/IDecalTemplate.h", "geom::IDecalTemplate", "Geom/DecalSystem/DecalTemplate.h", "geom::DecalTemplate", "renderer", None),
    *[("geometry-template-" + cls, "Geom/IGeometryTemplate.h", "geom::IGeometryTemplate", "Geom/" + header, "geom::" + cls, "renderer", None) for cls, header in [
        ("GeometryTemplate", "MeshTemplate.h"), ("StaticMeshTemplate", "StaticMeshTemplate.h"),
        ("SkinnedMeshTemplate", "SkinnedMeshTemplate.h"), ("BundledMeshTemplate", "BundledMeshTemplate.h")]],
    *[("particle-instance-" + str(i), "Geom/ParticleSystem/ParticleSystemManager.h", "geom::IParticleSystemInstance", "Geom/ParticleSystem/ParticleSystemManager.cpp", "geom::(anonymous namespace)::ParticleInstanceType" + str(i), "renderer", None) for i in range(5)],
    ("sound-file-callback", "Sound/ISoundFileIO.h", "ISoundFileIO", "Sound/BfSound.h", "BfSound", "host", "ISoundFileIO"),
    ("sound-trace-callback", "Sound/ISoundTrace.h", "ISoundTrace", "Sound/BfSound.h", "BfSound", "host", "ISoundTrace"),
    ("sound-obstruction-callback", "Sound/ISoundObstructionEvaluator.h", "ISoundObstructionEvaluator", "Sound/BfSound.h", "BfSound", "host", "ISoundObstructionEvaluator"),
]:
    PAIRS.append((name, side("Code/BF2/" + view_header, view_cls),
                  side("Code/BF2/" + provider_header, provider_cls, image, subobject=subobject)))

for cls in ("dx9rend::DX9RendModule", "Rend::RendModule", "geom::ModuleGeom", "geom::ModuleGeomMisc"):
    PAIRS.append(("module-" + cls, side("Code/BF2/Core/IModule.h", "IModule"),
                  side("Code/RendDX9Abi/PluginModules.h", cls, "renderer")))
PAIRS.extend([
    ("decal-object-implementation", side("Code/BF2/Geom/DecalSystem/DecalObject.h", "geom::IDecalObject", "renderer"),
     side("Code/BF2/Geom/DecalSystem/DecalObject.h", "geom::DecalObject", "renderer")),
    ("module-swiff", side("Code/BF2/Core/IModule.h", "IModule"),
     side("Code/BF2/External/gameswf/SwiffPlayer/SwiffPlayerModule.h", "ui::SwiffModule", "swiff")),
    ("geom-module-event", side("Code/BF2/Game/IEventListener.h", "IEventListener"),
     side("Code/RendDX9Abi/PluginModules.h", "geom::ModuleGeom", "renderer", subobject="IEventListener")),
    ("swiff-module-event", side("Code/BF2/Game/IEventListener.h", "IEventListener"),
     side("Code/BF2/External/gameswf/SwiffPlayer/SwiffPlayerModule.h", "ui::SwiffModule", "swiff", subobject="IEventListener")),
])

# [CHOSEN, not recovered] Saved Data::setFloat uses a compiler-emitted table.
# Compare every current BFMeme Data provider, rather than transplanting PE slots.
for header, cls in [
    ("Data.h", "StringData"), ("Data.h", "FloatRefData"),
    ("Data.h", "BoolRefData"), ("Data.h", "IntRefData"),
    ("Data.h", "StringRefData"), ("Data.h", "WstringRefData"),
    ("Data.h", "WstringData"), ("FloatData.h", "FloatData"),
    ("IntData.h", "IntData"), ("BoolData.h", "BoolData"),
    ("LogicalData.h", "LogicalData"), ("AndData.h", "AndData"),
    ("OrData.h", "OrData"), ("NotData.h", "NotData"),
    ("EqualData.h", "EqualData"), ("ToggleData.h", "ToggleData"),
]:
    PAIRS.append(("meme-data-" + cls,
                  dict(header="Code/BF2/External/BFMeme/Data.h", cls="dice::meme::Data", image="host"),
                  dict(header="Code/BF2/External/BFMeme/" + header, cls="dice::meme::" + cls, image="host")))

# Retail tables read with pe_evidence.py words; constructor stores attest owners.
PAIRS.extend([
    # PE 0x1020CC6C (11 slots), stored by 0x10099E00 at 0x10099E0A.
    ("debug-renderer", side("Code/BF2/GeomMisc/DebugRenderer.h", "geom::IDebugRenderer"),
     side("Code/BF2/GeomMisc/DebugRenderer.cpp", "geom::(anonymous namespace)::DebugRenderer", "renderer")),
    # PE 0x1020CAD8 (6 slots), stored by 0x10097780 at secondary this+4, 0x10097791.
    ("debug-text-drawer-path-listener", side("Code/BF2/RendDX9/RendDX9Base.h", "dx9rend::RenderPathListener", "renderer"),
     side("Code/BF2/GeomMisc/DebugTextDrawer.cpp", "geom::(anonymous namespace)::DrawerPathListener", "renderer")),
    # PE 0x1020CD10 (19 slots), stored by 0x1009B7C0 at 0x1009B7C6.
    # Only IBase is independently declared; +0x10..+0x48 remain unpaired.
    ("debug-text-writer-base", side("Code/BF2/Core/IBase.h", "IBase"),
     side("Code/BF2/GeomMisc/DebugTextWriter.cpp", "geom::DebugTextWriter", "renderer")),
])


def normalize(method):
    method = method.replace("(anonymous namespace)::", "")
    method = re.sub(r"\s*\[(?:pure|complete|deleting)\]", lambda m: "" if "pure" in m[0] else m[0], method)
    for alias in ("renddx9abi::HostString", "ParticleSystemName", "DecalTemplateString"):
        method = method.replace(alias, "std::string")
    method = method.replace("HWndBf2TempSlt", "renddx9abi::HostDisplayMode").replace("dx9rend::IRenderView", "IRenderView")
    method = method.replace("ui::IServerBrowser", "IServerBrowser")
    # Clang qualifies owners differently for an inherited slot and an override.
    method = re.sub(r"(?:[\w]+::)+(?=[~\w]+\()", "", method)
    method = re.sub(r"~\w+\(", "~destructor(", method)
    return re.sub(r"\s+", " ", method).strip()


def layouts(output):
    result = {}
    for match in re.finditer(r"Vtable for '([^']+)' \(\d+ entries\)\.\n(.*?)(?=\n\n)", output, re.S):
        slots, bases, chunks = [], [], []
        def save():
            if slots:
                chunks.append(list(slots))
                for base in bases:
                    result[match[1] + "@" + base] = list(slots)
        for line in match[2].splitlines():
            marker = re.search(r"-- \((.+), -?\d+\) vtable address --", line)
            if marker:
                bases.append(marker[1])
            entry_match = re.match(r"\s*\d+ \| (.+)$", line)
            if not entry_match:
                continue
            entry = entry_match[1]
            if "offset_to_top" in entry:
                save()
                slots, bases = [], []
                continue
            if entry.endswith(" RTTI"):
                continue
            if "(" in entry:
                slots.append(normalize(entry))
        save()
        result[match[1]] = chunks[0] if chunks else []
    return result


class Compiler:
    def __init__(self, build, fresh):
        self.cache = build / "vtable-audit"
        self.cache.mkdir(parents=True, exist_ok=True)
        self.database = json.loads((build / "compile_commands.json").read_text())
        self.fresh = fresh
        compiler = shlex.split(self.database[0]["command"])[0]
        self.compiler_identity = subprocess.check_output([compiler, "--version"], text=True)

    def flags(self, image, source=None):
        targets = {"host": "bf2_client_objects", "renderer": "bf2_renddx9_static",
                   "swiff": "bf2_swiffplayer_static", "audio": "bf2_bf2audio_static"}
        entries = [x for x in self.database if targets[image] + ".dir/" in x["command"]]
        entry = next((x for x in entries if source and x["file"].endswith(source)), entries[0])
        args = shlex.split(entry["command"])
        # Everything before the output option is the real target's compile environment.
        return args[:args.index("-o")] + ["-O0", "-g0", "-I" + str(ROOT)]

    def compile(self, key, code, image, source=None, allow_failure=False, attempt=0):
        args = self.flags(image, source)
        digest = hashlib.sha256((self.compiler_identity + json.dumps(args) + code).encode()).hexdigest()
        stem = self.cache / re.sub(r"[^\w.-]", "_", key)
        record = stem.with_suffix(".json")
        if record.exists() and not self.fresh:
            cached = json.loads(record.read_text())
            if cached["digest"] == digest and all(Path(p).exists() and Path(p).stat().st_mtime_ns == t for p, t in cached["deps"]):
                return cached["output"]
        tu, obj, dep = (stem.with_suffix(s) for s in (".cpp", ".o", ".d"))
        tu.write_text(code)
        cmd = args + ["-ferror-limit=0", "-MD", "-MF", str(dep), "-Xclang", "-fdump-vtable-layouts", "-c", str(tu), "-o", str(obj)]
        started = time.time_ns()
        proc = subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True)
        stem.with_suffix(".log").write_text(proc.stdout + proc.stderr)
        if proc.returncode:
            if allow_failure and "raw-signature slot_" in proc.stderr:
                return proc.stdout + proc.stderr
            raise RuntimeError(f"{key}: compiler failed (see {stem.with_suffix('.log')})\n{proc.stderr[-6000:]}")
        dependencies = shlex.split(dep.read_text().replace("\\\n", " ").split(":", 1)[1])
        dependencies = [(str(Path(p).resolve()), Path(p).stat().st_mtime_ns) for p in dependencies if Path(p).resolve() != tu]
        if any(timestamp > started for path, timestamp in dependencies):
            record.unlink(missing_ok=True)
            if attempt >= 2:
                raise RuntimeError(f'{key}: inputs kept changing during compilation; rerun the audit after edits settle')
            return self.compile(key, code, image, source, allow_failure, attempt + 1)
        record.write_text(json.dumps(dict(digest=digest, deps=dependencies, output=proc.stdout)))
        return proc.stdout

    def inspect(self, key, spec):
        # Defining a key function on an abstract derived class forces Clang to
        # lay out inherited slots too. Merely taking &Derived::inheritedMethod
        # only requests the base table and would leave holes in coverage.
        cls = spec["cls"].replace("(anonymous namespace)::", "")
        code = (f'#include "{spec["header"]}"\n'
                f'struct AuditForce : {cls} {{ virtual void audit_force(); }};\n'
                'void AuditForce::audit_force() {}\n')
        output = self.compile(key, code, spec["image"], spec.get("source"))
        tables = layouts(output)
        if "AuditForce" not in tables:
            raise RuntimeError(f"{key}: Clang did not emit {spec['cls']}")
        key = "AuditForce" + ("@" + H + spec["subobject"] if spec.get("subobject") else "")
        if key not in tables:
            raise RuntimeError(f"{key}: secondary subobject not emitted")
        return [s for s in tables[key] if s != "void audit_force()"]


def check_pair(compiler, pair):
    name, view, host = pair
    a = compiler.inspect(name + "-view", view)
    b = compiler.inspect(name + "-host", host)
    errors, reserved = [], []
    for i, method in enumerate(a):
        actual = b[i] if i < len(b) else "<missing host slot>"
        if re.search(r"\b(?:reserved\w*|slot\w*|nullsub\w*)\(", method):
            reserved.append(dict(slot=i, view=method, host=actual))
            # Positional names aren't evidence of identity: require an explicit
            # reviewed correspondence, including the full provider signature.
            expected = RESERVED.get(name, {}).get(str(i), method)
        else:
            expected = method
        if actual != expected:
            errors.append(dict(slot=i, view=method, host=actual, expected=expected))
    if len(a) > len(b):
        errors.append(dict(error="view extends beyond provider"))
    if name == 'class-manager':
        # Unlike classes inheriting their shared interface, this independent
        # view needs canonical C++ type checks as well as the textual slot dump.
        code = '''#include "Code/RendDX9Abi/HfeAbi.h"
template<class, class> struct RebindOwner;
template<class R,class C,class D,class... A> struct RebindOwner<R(C::*)(A...), D> { using type=R(D::*)(A...); };
template<class R,class C,class D,class... A> struct RebindOwner<R(C::*)(A...) const, D> { using type=R(D::*)(A...) const; };
using namespace dice::hfe;
'''
        for method in a:
            method_name = re.search(r'([~\w]+)\(', method)[1]
            if method_name.startswith('reserved') or method_name == 'getClassID':
                continue
            code += f'using T_{method_name}=RebindOwner<decltype(&IClassManager::{method_name}),ClassManager>::type;\n'
            code += f'constexpr auto p_{method_name}=static_cast<T_{method_name}>(&ClassManager::{method_name});\n'
        code += 'constexpr auto id_view=static_cast<bool(IClassManager::*)(const std::string&,unsigned int&)>(&IClassManager::getClassID);\n'
        code += 'constexpr auto id_host=static_cast<bool(ClassManager::*)(const std::string&,unsigned int&)>(&ClassManager::getClassID);\n'
        compiler.compile(name + '-canonical-types', code, 'renderer')
    return dict(pair=name, view=view, host=host, slots=len(a), provider_slots=len(b), errors=errors, reserved=reserved)


# Every reserved-to-real mapping is explicit and printed in the report. A new
# reserved slot cannot pass just because both sides happen to have enough slots.
RESERVED = {"class-manager": {
    "4": "~destructor() [complete]",
    "5": "~destructor() [deleting]",
    "8": "void setDefaultClass(unsigned int, unsigned int)",
    "9": "bool getDefaultClass(unsigned int, unsigned int &)",
    "10": "bool getClassName(unsigned int, std::string &)",
    "12": "IBase *createInstance(unsigned int, IBase *)",
    "14": "IBase *createInstance(std::string, unsigned int, IBase *)",
    "15": "bool setSingletonClass(const std::string &, unsigned int)",
    "16": "bool getSingletonClass(const std::string &, unsigned int &)",
    "20": "void *getSingletonAs(const std::string &, unsigned int)",
    "22": "void unregisterSimpleSingleton(const std::string &)",
    "23": "void *getSimpleSingleton(const std::string &) const",
    "24": "std::string getPrintableSingletonList() const",
    "25": "void getAllClassIncludingSubstring(const std::string &, std::vector<std::string> &) const",
    "26": "void initSingletons()",
}, "geometry-registry-implementation": {
    "11": "const std::vector<std::uint32_t> &slot_0x2C()",
    "12": "const ::std::string &slot_0x30(std::uint32_t)", "13": "void slot_0x34()",
    "14": "void slot_0x38()", "18": "void slot_0x48()", "19": "void slot_0x4C()",
    "20": "bool registerOptimizer(const std::string &, const std::string &)",
    "21": "void runOptimizers()", "22": "void clearOptimizers()", "23": "void slot_0x5C()",
}}

# Hand-built tables have no C++ class for Clang to dump. Their entry names are
# mapped to consumer methods in the reviewed manifest; Clang checks each thunk's
# full function type after removing only its explicit self argument.
RAW_PAIRS = [
    ("swiff-render-table", "Geom/SwiffRender.cpp", "geom", "kSwiffRenderVtablePrimary", "Menu/ISwiffRender.h", "ui::ISwiffRender"),
    ("video-options-table", "RendDX9/VideoOptions.cpp", "dx9rend", "kVideoOptionsVtable", "Game/Main/IVideoOptions.h", "IVideoOptions"),
    ("fs-movie-table", "GeomMisc/MenuRend/FSMoviePlayer.cpp", "geom", "kFSMoviePlayerVtable", "Game/Main/IFSMoviePlayer.h", "IFSMoviePlayer"),
    ("meme-texture-table", "GeomMisc/MenuRend/MemeTexture.cpp", "geom", "kMemeTextureVtable", "GeomMisc/MenuRend/IMemeTexture.h", "geom::IMemeTexture"),
    ("quad-drawer-table", "GeomMisc/MenuRend/QuadDrawer.cpp", "geom", "kQuadDrawerVtable", "GeomMisc/MenuRend/IQuadDrawer.h", "geom::IQuadDrawer"),
    ("particle-manager-table", "Geom/ParticleSystem/ParticleSystemManager.cpp", "geom", "kVtable", "Geom/ParticleSystem/ParticleSystemManager.h", "geom::IParticleSystemManager"),
    *[("particle-template-" + str(i), "Geom/ParticleSystem/" + file, "geom", "kVtable", "Geom/ParticleSystem/ParticleSystemManager.h", "geom::" + cls) for i, file, cls in [
        (0, "ParticleSystemTemplate.cpp", "IParticleSystemTemplateType0"),
        (1, "ParticleSystemTemplateType1.cpp", "IParticleSystemTemplateType1"),
        (2, "TrailSystemTemplate.cpp", "IParticleSystemTemplateType2"),
        (3, "ParticleSystemTemplateType3.cpp", "IParticleSystemEmitterTemplate"),
        (4, "NSAParticleSystemTemplate.cpp", "IParticleSystemTemplateType4")]],
    ("road-table", "Geom/RoadCompiled.cpp", "geom", "kRoadVtable", "Geom/IRoadEditable.h", "geom::IRoad"),
    ("road-path-listener", "Geom/RoadCompiledRenderer.cpp", "geom", "kListenerVtable", "RendDX9/RendDX9Base.h", "dx9rend::RenderPathListener"),
    ("light-path-listener", "Geom/XpLightManager.cpp", "geom", "kRenderPathVtable", "RendDX9/RendDX9Base.h", "dx9rend::RenderPathListener"),
    ("renderer-path-listener", "RendDX9/BF2Render.cpp", "dx9rend", "kRendererRenderPathVtable", "RendDX9/RendDX9Base.h", "dx9rend::RenderPathListener"),
    ("light-manager-table", "Geom/XpLightManager.cpp", "geom", "kPrimaryVtable", "Geom/XpLightManager.h", "geom::LightManager"),
    ("static-mesh-sampler", "Geom/StaticMesh.cpp", "geom", "kStaticMeshLightSampleableVtable", "Geom/LightmapGeneration/ILightSampleable.h", "geom::ILightSampleable"),
    ("scaleable-table", "Geom/ParticleSystem/MeshParticleMesh/MeshParticleMesh.cpp", "geom", "g_meshParticleMeshScaleableVtable", "Geom/IScaleable.h", "geom::IScaleable"),
    *[(name + "-event", file, namespace, table, "Game/IEventListener.h", "IEventListener") for name, file, namespace, table in [
        ("renderer", "RendDX9/BF2Render.cpp", "dx9rend", "kECCoreEventHandlerVtable"),
        ("swiff-render", "Geom/SwiffRender.cpp", "geom", "kSwiffRenderVtableEcCore"),
        ("quad-drawer", "GeomMisc/MenuRend/QuadDrawer.cpp", "geom", "kQuadDrawerECCoreVtable"),
        ("video-options", "RendDX9/VideoOptions.cpp", "dx9rend", "kVideoOptionsEventVtable"),
        ("post-production", "RendDX9/PostProductionFrameWork.cpp", "dx9rend", "kOwnerVtable"),
    ]],
    *[(name + "-device-listener", file, namespace, table, "RendDX9/RendDX9Base.h", "dx9rend::DeviceLostResetListener") for name, file, namespace, table in [
        ("renderer", "RendDX9/BF2Render.cpp", "dx9rend", "kRendererDeviceLostVtable"),
        ("swiff-render", "Geom/SwiffRender.cpp", "geom", "kSwiffRenderVtableDevLost"),
        ("quad-drawer", "GeomMisc/MenuRend/QuadDrawer.cpp", "geom", "kQuadDrawerListenerVtable"),
        ("meme-texture", "GeomMisc/MenuRend/MemeTexture.cpp", "geom", "kMemeTextureListenerVtable"),
        ("movie", "GeomMisc/MenuRend/FSMoviePlayer.cpp", "geom", "kFSMoviePlayerListenerVtable"),
        ("light", "Geom/XpLightManager.cpp", "geom", "kDeviceLostVtable"),
        ("effect-manager", "RendDX9/Ra/EffectManager.cpp", "dx9rend::Ra", "kEffectManagerListenerVtable"),
        ("shader-manager", "RendDX9/ShaderManager.cpp", "dx9rend", "kShaderManagerListenerVtable"),
        ("texture-manager", "RendDX9/TextureManager.cpp", "dx9rend", "kTextureManagerListenerVtable"),
        ("state-manager", "RendDX9/StateManager.cpp", "dx9rend", "kStateManagerListenerVtable"),
        ("post-production", "RendDX9/PostProductionFrameWork.cpp", "dx9rend", "kDeviceLostVtable"),
        ("font-renderer", "GeomMisc/FontSystem/SwiffFont.cpp", "geom", "kFontRendererListenerVtable"),
        ("video", "GeomMisc/MenuRend/Video.cpp", "geom", "kVideoListenerVtable"),
        ("video-target", "GeomMisc/MenuRend/Video.cpp", "geom", "kVideoTargetListenerVtable"),
    ]],
]

# PE 0x1020D274 (5 slots), stored by 0x100A1740 at 0x100A1750.
# The four-slot IBase consumer bounds this check; destroy at +0x10 has no view.
# The secondary PE 0x1020D268 table is already font-renderer-device-listener.
RAW_PAIRS.extend([
    ("font-renderer-base-table", "GeomMisc/FontSystem/SwiffFont.cpp", "geom", "kFontRendererVtable", "Core/IBase.h", "IBase"),
])

RAW_TRAITS = r'''
#include <type_traits>
template<class> struct AuditMember;
template<class R, class C, class... A> struct AuditMember<R(C::*)(A...)> { using type = R(A...); };
template<class R, class C, class... A> struct AuditMember<R(C::*)(A...) const> { using type = R(A...); };
template<class> struct AuditFree;
template<class R, class S, class... A> struct AuditFree<R(*)(S,A...)> { using type = R(A...); };
'''


def raw_functions(source, table, retail=False):
    # The table initializer is data only: function-address casts, no nested braces.
    text = (ROOT / source).read_text()
    matches = list(re.finditer(r"\b" + re.escape(table) + r"\s*\[(?P<size>[^\]]*)\]\s*=\s*\{(?P<body>.*?)\};", text, re.S))
    if not matches:
        raise RuntimeError(f"cannot find {table} in {source}")
    match = matches[-1] if retail else matches[0]
    body = re.sub(r"//[^\n]*|/\*.*?\*/", "", match["body"], flags=re.S)
    result = []
    pattern = (r"BF2_NATIVE_SLOT\(([^,]+),\s*([^,]+),\s*([^)]+)\)|"
               r"BF2_NATIVE_DTOR\(([^,]+),\s*([^)]+)\)|"
               r"reinterpret_cast<(?:const )?void\s*\*>\(\s*&([\w:]+(?:<[^>]*>)?)\s*\)|"
               r"(?:BM_ENTRY|BF2_MPM_ENTRY)\((\w+)\)")
    for m in re.finditer(pattern, body):
        if m[1]:
            interface, method, function = (m[i].strip() for i in (1, 2, 3))
            result.append((function, f'&::renddx9abi::NativeSlot<decltype(&{interface}::{method}), decltype(&{function}), &{function}>::invoke', method))
        elif m[4]:
            function, which = m[4].strip(), m[5].strip()
            result.append((function + ":" + which, f'&::renddx9abi::NativeDestructor<decltype(&{function}), &{function}>::{which}', '~destructor'))
        elif m[6]:
            result.append((m[6], "&" + m[6], None))
        else:
            function = "bm_" + m[7] if m[0].startswith('BM_ENTRY') else m[7]
            result.append((function, "&" + function, None))
    residual = re.sub(pattern, "", body).replace(",", "").strip()
    if residual:
        raise RuntimeError(f"{source}: unrecognized {table} initializer: {residual[:120]}")
    if match["size"].isdigit() and int(match["size"]) != len(result):
        raise RuntimeError(f"{source}: {table} has implicit or unparsed entries")
    if not result:
        raise RuntimeError(f"{source}: {table} has no auditable function entries")
    return result


def raw_entries(source, table):
    return [entry[0] for entry in raw_functions(source, table)]


def check_raw(compiler, pair, manifest):
    name, file, namespace, table, header, cls = pair
    source = "Code/BF2/" + file
    view = side("Code/BF2/" + header, cls, "renderer" if "listener" in name else "host")
    slots = compiler.inspect(name + "-view", view)
    functions = raw_functions(source, table)
    entries = [entry[0] for entry in functions]
    mapping = manifest[name]
    errors = []
    checks, checked = [], []
    for i, method in enumerate(slots):
        function = entries[i] if i < len(entries) else "<missing>"
        expected = mapping[i]["method"] if i < len(mapping) else None
        method_name = re.search(r"([~\w]+)\(", method)[1]
        if (expected != method_name or function != mapping[i]["function"] or
                (i < len(functions) and functions[i][2] is not None and functions[i][2] != method_name)):
            errors.append(dict(slot=i, view=method, host=function, expected_method=expected))
        else:
            expected_type = f'AuditMember<decltype(&{H+cls}::{method_name})>::type'
            if method_name.startswith("~"):
                expected_type = 'void()'
                which = "complete" if "[complete]" in method else "deleting"
                if not function.endswith(":" + which):
                    errors.append(dict(slot=i, view=method, host=function, error="destructor convention mismatch"))
                    continue
            checks.append(f'static_assert(std::is_same<{expected_type}, AuditFree<decltype({functions[i][1]})>::type>::value, "raw-signature slot_{i}");')
            checked.append(i)
    namespaces = ['dice', 'hfe'] + namespace.split('::')
    code = (f'#include "{source}"\n#include "Code/BF2/{header}"\n' + RAW_TRAITS +
            ''.join(f'namespace {n} {{ ' for n in namespaces) + "\n" + "\n".join(checks) + '\n' + '}' * len(namespaces) + '\n')
    output = compiler.compile(name + "-types", code, "renderer", source, allow_failure=True)
    for slot in re.findall(r"error: static assertion failed[^\n]*raw-signature slot_(\d+)", output):
        i = int(slot)
        if not any(e["slot"] == i and e.get("error") == "signature mismatch" for e in errors):
            errors.append(dict(slot=i, view=slots[i], host=entries[i], error="signature mismatch"))
    reserved = [dict(slot=i, view=method, host=entries[i] if i < len(entries) else '<missing>')
                for i, method in enumerate(slots) if re.search(r'\b(?:reserved\w*|slot\w*|nullsub\w*)\(', method)]
    return dict(pair=name, view=view, host=dict(header=source, cls=table), slots=len(slots), provider_slots=len(entries), errors=errors, reserved=reserved)


OPACITY_PAIRS = [
    ("opacity-static-mesh", "Geom/StaticMesh.cpp", "kStaticMeshVtable", "sm_setFade"),
    ("opacity-skinned-mesh", "Geom/SkinnedMesh.cpp", "g_skinnedMeshVtable", "skm_setTransparency"),
    ("opacity-bundled-mesh", "Geom/BundledMesh.cpp", "g_bundledMeshVtable", "bm_setTransparency"),
    ("opacity-road", "Geom/RoadCompiled.cpp", "kRoadGeometryVtable", "rg_nativeOpacity"),
    ("opacity-mesh-particle", "Geom/ParticleSystem/MeshParticleMesh/MeshParticleMesh.cpp", "g_meshParticleMeshVtable", "mpm_nativeOpacity"),
]


def check_opacity(compiler, pair):
    name, file, table, function = pair
    source = "Code/BF2/" + file
    header = "Code/BF2/Geom/GeometryInstanceData.h"
    declaration = (ROOT / header).read_text()
    if 'm_vptr[nativeOpacitySlot]' not in declaration:
        raise RuntimeError('opacity consumer no longer uses the audited native slot constant')
    slot = int(re.search(r'nativeOpacitySlot\s*=\s*(\d+)', declaration)[1])
    functions = raw_functions(source, table)
    errors = []
    if slot >= len(functions) or functions[slot][0] != function:
        errors.append(dict(slot=slot, view='void setOpacity(float)', host=functions[slot][0] if slot < len(functions) else '<missing>'))
    code = (f'#include "{source}"\n#include "{header}"\n' + RAW_TRAITS +
            f'namespace dice {{ namespace hfe {{ namespace geom {{\n'
            f'static_assert(GeometryInstanceData::nativeOpacitySlot == {slot}, "opacity slot");\n'
            f'static_assert(std::is_same<void(float), AuditFree<decltype(&{function})>::type>::value, "opacity signature");\n}}}}}}\n')
    compiler.compile(name + '-types', code, 'renderer', source)
    return dict(pair=name, view=dict(header=header, cls='GeometryInstanceData::setOpacity'), host=dict(header=source, cls=table), slots=1, provider_slots=len(functions), errors=errors, reserved=[])


def self_test(compiler):
    # Negative fixtures are deliberately incompatible. They validate the Clang
    # parser, rather than inferring correctness from production pairs all passing.
    code = '''
struct AuditHost { virtual ~AuditHost() {} virtual int choose(int)=0; virtual int choose(float)=0; virtual int known(int) const=0; };
struct AuditShift { virtual void reservedDestructor()=0; virtual int choose(int)=0; virtual int choose(float)=0; virtual int known(int) const=0; };
struct AuditOrder { virtual ~AuditOrder() {} virtual int choose(float)=0; virtual int choose(int)=0; virtual int known(int) const=0; };
struct AuditSignature { virtual ~AuditSignature() {} virtual int choose(int)=0; virtual int choose(float)=0; virtual int known(float) const=0; };
struct AuditSecondary { virtual void callback(void*)=0; };
struct AuditMultiple : AuditHost, AuditSecondary { void callback(void*) override; };
void AuditMultiple::callback(void*) {}
struct AuditHostForce : AuditHost { virtual void force(); }; void AuditHostForce::force() {}
struct AuditShiftForce : AuditShift { virtual void force(); }; void AuditShiftForce::force() {}
struct AuditOrderForce : AuditOrder { virtual void force(); }; void AuditOrderForce::force() {}
struct AuditSignatureForce : AuditSignature { virtual void force(); }; void AuditSignatureForce::force() {}
'''
    tables = layouts(compiler.compile('negative-fixtures', code, 'host'))
    host = tables['AuditHostForce'][:-1]
    shifted = tables['AuditShiftForce'][:-1]
    ordered = tables['AuditOrderForce'][:-1]
    signature = tables['AuditSignatureForce'][:-1]
    checks = [len(host) == 5, len(shifted) == 4,
              host[2] != shifted[2], host[2] != ordered[2], host[3] != ordered[3],
              host[4] != signature[4],
              tables['AuditMultiple@AuditSecondary'][0] == 'void callback(void *)']
    if not all(checks):
        raise RuntimeError('audit negative fixtures failed: destructor, overload, signature or secondary-table parsing')
    print('PASS negative fixtures: destructor shift, overload order, signature and secondary subobject')


# Named geometry-instance ordinals (Geom/NativeGeometrySlots.h) and the provider entry each must
# select. Game code reaches them only through Game/Objects/GeometryPartAccess.h. Each role names
# the provider table that implements it (others may hold an empty/default stub at that ordinal).
GEOMETRY_TABLES = {
    'bundled': ('Code/BF2/Geom/BundledMesh.cpp', 'g_bundledMeshVtable'),
    'static': ('Code/BF2/Geom/StaticMesh.cpp', 'kStaticMeshVtable'),
    'skinned': ('Code/BF2/Geom/SkinnedMesh.cpp', 'g_skinnedMeshVtable'),
    'meshparticle': ('Code/BF2/Geom/ParticleSystem/MeshParticleMesh/MeshParticleMesh.cpp', 'g_meshParticleMeshVtable'),
}
GEOMETRY_ROLES = [
    ('SetFov', 'bundled', 'bm_setFov'), ('GetFov', 'bundled', 'bm_getFov'),
    ('GetTemplate', 'bundled', 'bm_getTemplate'), ('GetTemplate', 'static', 'sm_getTemplate'),
    ('GetTemplate', 'skinned', 'skm_getTemplate'), ('GetBoundingRadius', 'bundled', 'bm_getBoundingRadius'),
    ('SetPartTransform', 'bundled', 'bm_setPart'), ('SetGeometryTransform', 'bundled', 'bm_setGeometryTransformation'),
    ('SetGeometryTransform', 'static', 'sm_setGeometryTransformation'), ('SetBoneTransforms', 'bundled', 'bm_setBones'),
    ('SetBoneTransforms', 'skinned', 'skm_setBoneTransforms'), ('GatherUsedBones', 'skinned', 'skm_gatherVisibleParts'),
    ('GetPartCount', 'bundled', 'bm_getPartCount'), ('SetExcludedPart', 'skinned', 'skm_setExcludedPart'),
    ('SetSubgeom', 'bundled', 'bm_setGeomIndex'), ('SetSubgeom', 'static', 'sm_setGeom'),
    ('GetSubgeom', 'bundled', 'bm_getGeomIndex'), ('GetSubgeomCount', 'bundled', 'bm_getGeomCount'),
    ('GetSubgeomCount', 'static', 'sm_getGeomCount'), ('GetSubgeomCount', 'skinned', 'skm_getGeomCount'),
    ('SetLod', 'bundled', 'bm_setLodIndex'), ('SetLod', 'static', 'sm_setLod'),
    ('GetLod', 'bundled', 'bm_getLodIndex'), ('GetLodCount', 'bundled', 'bm_getLodCount'),
    ('SetUvTranslation', 'bundled', 'bm_uvTranslate'), ('SetUvTransformation', 'bundled', 'bm_uvTransform'),
    ('SetOwnerObject', 'bundled', 'bm_setOwnerObject'), ('SetOwnerObject', 'static', 'sm_setOwnerObject'),
    ('UpdateFlags', 'static', 'sm_updateFlags'), ('SetNextGeometry', 'bundled', 'bm_setNextGeometry'),
    ('GetNextGeometry', 'bundled', 'bm_getNextGeometry'), ('GetNextGeometry', 'static', 'sm_getNextGeometry'),
    ('SetFirstPerson', 'meshparticle', 'mpm_setFirstPerson'),
    ('SetLightSourceMask', 'static', 'sm_setDword28C'), ('GetLightSourceMask', 'static', 'sm_getDword28C'),
    ('GetShadowRecord', 'static', 'sm_getDword34C'), ('GetShadowRecord', 'skinned', 'skm_getDword33C'),
    ('GetShadowRecord', 'bundled', 'bm_getShadowProjection'),
    ('GetBoundingRadius', 'static', 'sm_getBoundingRadius'), ('GetBoundingRadius', 'skinned', 'skm_getBoundingRadius'),
    ('GetGeometryTransform', 'static', 'sm_getGeometryTransformation'),
    ('GetGeometryTransform', 'skinned', 'skm_getGeometryTransformation'),
    ('GetGeometryTransform', 'bundled', 'bm_getGeometryTransformation'),
]


def check_geometry_selectors(compiler):
    tables = {key: raw_entries(source, name) for key, (source, name) in GEOMETRY_TABLES.items()}
    code = '#include "Code/BF2/Geom/NativeGeometrySlots.h"\n'
    errors = []
    for role, table, function in GEOMETRY_ROLES:
        if function not in tables[table]:
            errors.append(dict(role=role, table=table, missing=function))
            continue
        index = tables[table].index(function)
        code += f'static_assert(dice::hfe::geom::native_geometry::{role} == {index}, "native geometry {role} ({table} {function})");\n'
    compiler.compile('geometry-selectors', code, 'host')
    return dict(pair='geometry-selectors', view=dict(header='Code/BF2/Geom/NativeGeometrySlots.h', cls='native_geometry'), host=dict(header=GEOMETRY_TABLES['bundled'][0], cls='geometry tables'), slots=len(GEOMETRY_ROLES), provider_slots=len(tables['bundled']), errors=errors, reserved=[])


def check_raw_slots(compiler):
    # Raw vtable-slot dispatch in non-MSVC code must be a reviewed baseline row.
    sys.path.insert(0, str(Path(__file__).parent))
    import raw_slot_scan
    sites, errors = raw_slot_scan.check()
    return dict(pair='raw-slot-dispatch', view=dict(header='tools/macos/raw_slot_scan.py', cls='non-MSVC raw table loads'), host=dict(header='tools/macos/raw_slot_baseline.tsv', cls='reviewed baseline'), slots=len(sites), provider_slots=len(sites), errors=errors, reserved=[])


def check_swiff_events(compiler):
    slots = compiler.inspect('swiff-events-provider', side('Code/BF2/Core/EventManager.h', 'EventManager'))
    register = next(i for i, s in enumerate(slots) if ' registerEventHandler(' in s)
    unregister = next(i for i, s in enumerate(slots) if ' unregisterEventHandler(' in s)
    code = '''
// The image-private global has a different storage type. Rename only the host
// header's extern while inspecting its real EventManager declaration.
#define eventManager auditHostEventManager
#include "Code/BF2/Core/EventManager.h"
#undef eventManager
#include "Code/BF2/External/gameswf/SwiffPlayer/SwiffPlayerModule.cpp"
#include <type_traits>
template<class> struct ExplicitSelf;
template<class R,class C,class... A> struct ExplicitSelf<R(C::*)(A...)> { using type=R(*)(void*,A...); };
namespace dice { namespace hfe { namespace ui {
static_assert(std::is_same<EmRegisterFn, ExplicitSelf<decltype(&EventManager::registerEventHandler)>::type>::value, "event registration signature");
static_assert(std::is_same<EmUnregisterFn, ExplicitSelf<decltype(&EventManager::unregisterEventHandler)>::type>::value, "event unregistration signature");
'''
    code += f'static_assert(nativeEventRegisterSlot == {register}, "event register slot");\n'
    code += f'static_assert(nativeEventUnregisterSlot == {unregister}, "event unregister slot");\n}}}}}}\n'
    compiler.compile('swiff-events-consumer', code, 'swiff')
    return dict(pair='swiff-event-registration', view=dict(header='Code/BF2/External/gameswf/SwiffPlayer/SwiffPlayerModule.cpp', cls='eventManagerRegister/Unregister'), host=dict(header='Code/BF2/Core/EventManager.h', cls='EventManager'), slots=2, provider_slots=len(slots), errors=[], reserved=[])


def check_postproduction(compiler):
    source = 'Code/BF2/RendDX9/PostProductionFrameWork.cpp'
    functions = raw_functions(source, 'kPostProdVtable')
    roles = {1: ('setFrequency', 'pp_set160'), 3: ('setRoll', 'pp_set164'),
             5: ('setNoise', 'pp_set168'), 7: ('setFilter', 'pp_set172'),
             9: ('setFog', 'pp_setStateVec4'), 13: ('setBlend', 'pp_setBlend'),
             17: ('setGlow', 'pp_set248'), 18: ('setBlur', 'pp_set252'),
             19: ('setEnabled', 'pp_setByte242'), 20: ('setCapture', 'pp_setByte241')}
    consumer = 'Code/BF2/Game/Objects/Fx/ExplosionEffectFlashBang.cpp'
    if 'dx9rend::native_post::slot(peOrdinal)' not in (ROOT / consumer).read_text():
        raise RuntimeError('flashbang consumer no longer uses the audited native selector')
    code = (f'#include "{source}"\n#include "Code/BF2/RendMisc/NativePostProductionSlots.h"\n' + RAW_TRAITS +
            'namespace dice { namespace hfe { namespace dx9rend {\n')
    errors = []
    for index, (method, function) in roles.items():
        if functions[index][0] != function or functions[index][2] != method:
            errors.append(dict(slot=index, view=method, host=functions[index][0]))
        code += f'static_assert(native_post::slot({index}) == {index}, "post-production slot");\n'
        code += f'static_assert(std::is_same<AuditMember<decltype(&native_post::Calls::{method})>::type,AuditFree<decltype({functions[index][1]})>::type>::value,"post-production signature");\n'
    compiler.compile('post-production-native-calls', code + '}}}\n', 'renderer', source)
    return dict(pair='post-production-native-calls', view=dict(header='Code/BF2/RendMisc/NativePostProductionSlots.h', cls='native_post::Calls'), host=dict(header=source, cls='kPostProdVtable'), slots=len(roles), provider_slots=len(functions), errors=errors, reserved=[])


# Evidence-derived native signatures of the five former argument-mapping stops.
# Each row: expression, exact native type, PE evidence.
SWIFF_TRANSPORT_CONTRACTS = [
    ('&ui::ISwiffRender::slot8', 'int (ui::ISwiffRender::*)(void*)',
     'SwiffPlayer 0x10005B56 push image::rgb*; RendDX9 0x101529D0 or eax,-1; ret 4'),
    ('&native_render::rcCreateRgb', 'int (*)(void*, void*)', 'SwiffPlayer 0x10005AE0'),
    ('&ui::ISwiffRender::fontForward32', 'char (ui::ISwiffRender::*)(void*, int*, float, float)',
     'SwiffPlayer 0x10005E30 lea edx,[ebp+0x14]; RendDX9 0x10153004 fld/fmul x,y; font 0x100A4910 mov [ecx],eax'),
    ('&native_render::rcTextHit', 'char (*)(void*, void*, int*, float, float)', 'SwiffPlayer 0x10005E20'),
    ('&ui::SwiffRenderHandler::text_hit', 'bool (ui::SwiffRenderHandler::*)(void*, int*, float, float)',
     'SwiffPlayer 0x100887A5..0x100887D7 fstp floats pushed unconverted'),
    ('&ui::ISwiffRender::fontForward40', 'char (ui::ISwiffRender::*)(void*, int*, int*)',
     'SwiffPlayer 0x10005E59/0x10005E5D lea; font 0x10152080 stores [edx]'),
    ('&native_render::rcTextSelection', 'int (*)(void*, void*, int*, int*)', 'SwiffPlayer 0x10005E50'),
    ('&ui::ISwiffRender::fontForward44', 'char (ui::ISwiffRender::*)(void*, float, float, float, float)',
     'RendDX9 0x10153074..0x101530A0 fld/fmul four args'),
    ('&native_render::rcTextSetSelection', 'char (*)(void*, void*, float, float, float, float)',
     'SwiffPlayer 0x10005E90 jmp [eax+0x88]'),
    ('&ui::SwiffRenderHandler::text_set_selection',
     'bool (ui::SwiffRenderHandler::*)(void*, float, float, float, float)',
     'SwiffPlayer 0x10088823..0x1008883B float dwords pushed'),
    ('&ui::ISwiffRender::setTextureTechnique', 'void* (ui::ISwiffRender::*)(const std::string&, int)',
     'RendDX9 0x10154E60 std::map<std::string,int>@+0x3D8; MSVCP71 operator< / copy-ctor'),
    ('&native_render::rcSetPairState', 'void (*)(void*, const std::string&, int)', 'SwiffPlayer 0x10005F5F'),
    ('&ui::ISwiffPlayer::renderSetPairState', 'void (ui::ISwiffPlayer::*)(const std::string&, int)',
     'SwiffPlayer 0x10005F50 jmp [eax+0xD4] (pass-through, ret 8)'),
]


def check_swiff_transport(compiler):
    header = 'Code/SwiffPlayerAbi/NativeRenderCalls.h'
    source = 'Code/BF2/External/gameswf/SwiffPlayer/SwiffPlayer.cpp'
    text = (ROOT / header).read_text()
    caller = (ROOT / source).read_text()
    if 'using namespace ::dice::hfe::ui::native_render;' not in caller or 'SwiffPlayerAbi/NativeRenderCalls.h' not in caller:
        raise RuntimeError('SwiffPlayer no longer selects its audited native renderer transport')
    if 'unresolved(' in text:
        raise RuntimeError('native SwiffRender transport regained an unresolved argument-mapping stop')
    # The real wrapper declarations type-check every named virtual call; the
    # contracts pin the evidence-derived types so a declaration cannot drift
    # back to a cast-compatible but wrong argument meaning.
    code = (f'#include "{header}"\n#include "Code/BF2/External/gameswf/SwiffPlayer/SwiffPlayer.h"\n'
            '#include <type_traits>\nnamespace dice { namespace hfe {\nusing namespace ::dice::hfe::ui;\n')
    for index, (expression, expected, evidence) in enumerate(SWIFF_TRANSPORT_CONTRACTS):
        code += (f'static_assert(std::is_same<decltype({expression}), {expected}>::value, '
                 f'"swiff transport contract {index}: {expression} ({evidence})");\n')
    compiler.compile('swiff-native-render-transport', code + '} }\n', 'swiff', source)
    contracts = [dict(expression=e, type=t, evidence=v) for e, t, v in SWIFF_TRANSPORT_CONTRACTS]
    return dict(pair='swiff-render-transport', view=dict(header=source, cls='native_render calls'), host=dict(header=header, cls='ISwiffRender typed calls'), slots=len(re.findall(r'inline (?:int|char|void\*?|void) rc\w+\(', text)), provider_slots=67, errors=[], reserved=[], contracts=contracts)

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument("--fresh", action="store_true", help="ignore successful probe cache")
    parser.add_argument("--jobs", type=int, default=4)
    parser.add_argument("--pair", help="only inspect a named pair")
    args = parser.parse_args()
    start = time.monotonic()
    compiler = Compiler(args.build_dir.resolve(), args.fresh)
    self_test(compiler)
    pairs = [p for p in PAIRS if not args.pair or p[0] == args.pair]
    raw = [p for p in RAW_PAIRS if not args.pair or p[0] == args.pair]
    opacity = [p for p in OPACITY_PAIRS if not args.pair or p[0] == args.pair]
    seams = [('geometry-selectors', check_geometry_selectors), ('swiff-event-registration', check_swiff_events),
             ('post-production-native-calls', check_postproduction), ('swiff-render-transport', check_swiff_transport),
             ('raw-slot-dispatch', check_raw_slots)]
    seams = [p for p in seams if not args.pair or p[0] == args.pair]
    manifest_file = Path(__file__).with_name("vtable_pairs.json")
    if not pairs and not raw and not opacity and not seams:
        parser.error("no matching pair")
    manifest = json.loads(manifest_file.read_text()) if raw else {}
    results, failures = [], []
    with ThreadPoolExecutor(max_workers=args.jobs) as pool:
        futures = [(p[0], pool.submit(check_pair, compiler, p)) for p in pairs]
        futures += [(p[0], pool.submit(check_raw, compiler, p, manifest)) for p in raw]
        futures += [(p[0], pool.submit(check_opacity, compiler, p)) for p in opacity]
        futures += [(name, pool.submit(check, compiler)) for name, check in seams]
        for name, future in futures:
            try:
                result = future.result()
                results.append(result)
                for error in result["errors"]:
                    failures.append(f"{name}: {error}")
                print(f"{'FAIL' if result['errors'] else 'PASS'} {name}: {result['slots']} slots, {len(result['reserved'])} positional slots")
            except Exception as error:
                failures.append(str(error))
    report = compiler.cache / "report.json"
    report.write_text(json.dumps(results, indent=2) + "\n")
    for failure in failures:
        print(failure, file=sys.stderr)
    print(f"{len(results)} pairs checked in {time.monotonic()-start:.2f}s; {len(failures)} failures; report: {report}")
    return bool(failures)


if __name__ == "__main__":
    sys.exit(main())
