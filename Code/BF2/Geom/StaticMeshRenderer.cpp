// Static-mesh renderer singleton: vertex declarations, shadow effects, and lightmap atlases.
// [CHOSEN] Internal layout is local; the two interface tables preserve the compatibility ABI.

#ifdef BF2_IMAGE_RENDDX9

#include "../../RendDX9Abi/ThunkAbi.h"
#include "../../RendDX9Abi/HfeAbi.h"
#include "../../RendDX9Abi/ModuleCommon.h"
#include "../../RendDX9Abi/BfMemory.h"
#include "../../RendDX9Abi/Frontier.h"
#include <string>
#include "../Core/EventManager.h"          // the ECCore registration
#include "../RendDX9/RendDX9Base.h"
#include "../RendDX9/ShaderParameter.h"
#include "HemiMapManager/HemiMapDependencies.h"
#include "StaticMeshRenderer.h"
#include "StaticMeshTemplate.h"
#include "BundledMeshStateManager.h"
#include "XpLightManager.h"

#include "../../RendDX9Abi/PluginEntry.h"   // fileManager - the atlas builders' listing
#include "../RendDX9/TextureAtlas/TextureAtlas.h"

#include <cstddef>
#include <cstdio>
#include <list>
#include <new>
#include <string>
#include <string.h>

namespace dice {
namespace hfe {

namespace dx9rend {
extern void* g_shaderManager;       // ShaderManager.cpp, PE 0x1023B97C
extern void* g_rendererBase;        // RendDX9Init.cpp, the lightmap loader's +0x102 gate
}

namespace geom {

extern LightManager* g_LightManager;
extern IBase* g_StaticMeshRenderer;

// getClassList: sub_100518A0(821014, "dice.hfe.geom.StaticMeshRenderer", factory). getSingletonList
// { "StaticMeshRenderer", &g_StaticMeshRenderer, iid 820013, classId 821014, flags 2 }.
// queryInterface (0x10145610, shared MeshRendererCommon body) accepts iid 1 and 820013.
extern const unsigned int CID_StaticMeshRenderer  = 821014u;
extern const unsigned int IID_IStaticMeshRenderer = 820013u;

// The factory allocation, 0x100B068E (retail 0xA0).
static const unsigned int kStaticMeshRendererSize = sizeof(StaticMeshRenderer);

// Shared bundled-mesh shadow handles owned by StaticMeshRenderer.
dx9rend::Effect* g_bundledShadowEffect = 0;
dx9rend::ShaderParameter* g_bundledShadowAlphaThreshold = 0;
dx9rend::ShaderParameter* g_bundledTexLayer0 = 0;
dx9rend::ShaderParameter* g_bundledOneBoneSkinning = 0;
dx9rend::ShaderParameter* g_bundledViewI = 0;

namespace {

void setShadowTechnique(dx9rend::Effect* effect)
{
    dx9rend::effectSetTechniqueByName(
        effect, std::string(dx9rend::deviceCapsIsNvidia()
                                ? "DrawShadowMapNV"
                                : "DrawShadowMap"));
}

dx9rend::ShaderParameter* getEffectParameter(dx9rend::Effect* effect, const char* semantic)
{
    return dx9rend::effectGetShaderParameterTyped(effect, std::string(semantic));
}

void destroyStaticMeshRenderer(StaticMeshRenderer* self)
{
    dx9rend::vertexDeclarationRelease(self->m_decl2C);
    dx9rend::vertexDeclarationRelease(self->m_decl30);
    dx9rend::vertexDeclarationRelease(self->m_decl34);
    dx9rend::vertexDeclarationRelease(self->m_decl38);
    dx9rend::vertexDeclarationRelease(self->m_decl3C);

    if (self->m_worldViewProj) self->m_worldViewProj->release();
    if (self->m_viewI) self->m_viewI->release();
    if (self->m_paraboloid) self->m_paraboloid->release();
    if (self->m_paraboloidZ) self->m_paraboloidZ->release();
    if (self->m_parallax) self->m_parallax->release();
    if (self->m_shadowEffect) dx9rend::effectRelease(self->m_shadowEffect);

    // this non-intuitive order is shipped ordering.
    if (g_bundledTexLayer0) g_bundledTexLayer0->release();
    if (g_bundledOneBoneSkinning) g_bundledOneBoneSkinning->release();
    if (g_bundledShadowAlphaThreshold) g_bundledShadowAlphaThreshold->release();
    if (g_bundledViewI) g_bundledViewI->release();
    if (g_bundledShadowEffect) dx9rend::effectRelease(g_bundledShadowEffect);

    // PE 0x10121BD8: `mov ecx, dword_102651C0 / test ecx,ecx / call sub_101A9150 / push / engineFree`.
    // The StaticMeshStateManager is torn down directly - its member teardown followed by the raw
    // free - not through its release slot, and the singleton cell is left pointing at freed memory
    // exactly as shipped. This sits after the bundled-effect releases and before the event-manager
    // unregister and the ten cache releases.
    if (g_staticMeshStateManager)
    {
        staticMeshStateManagerDestroyMembers(g_staticMeshStateManager);
        ::renddx9abi::engineFree(g_staticMeshStateManager);
    }

    // Remove the core-event listener before releasing owned renderer resources.
    eventManager->unregisterEventHandler(
        ECCore, reinterpret_cast<IEventListener*>(&self->m_ehVptr));

    // each of the ten lightmap atlases is released
    // through its own vtable +0x08 (TextureAtlas release, 0x10017410), in order and with NO null
    // test, exactly as shipped. Shutdown only - the map-change path never destroys this singleton.
    for (int i = 0; i < 10; ++i)
    {
        dx9rend::TextureAtlas* const atlas = self->m_lightmapAtlases[i];
        typedef int (__thiscall* AtlasReleaseFn)(void*);
        reinterpret_cast<AtlasReleaseFn>(
            static_cast<void**>(atlas->vptr)[0x08 / 4])(atlas);
    }
    self->m_name.~basic_string();
}

#define RD9_SMR_FRONTIER(name, qualified, slotOffset)                               \
    void RD9_THUNK name(void* self RD9_THUNK_EDX)                                         \
    {                                                                               \
        RD9_FRONTIER_AT("dice::hfe::geom::StaticMeshRenderer::" qualified,           \
                        self, slotOffset);                                          \
    }

// Primary spine - refcount at +0x28.
int RD9_THUNK smr_addRef(void* self RD9_THUNK_EDX) { return ++static_cast<StaticMeshRenderer*>(self)->m_refCount; }
int RD9_THUNK smr_getRc (void* self RD9_THUNK_EDX) { return   static_cast<StaticMeshRenderer*>(self)->m_refCount; }
int RD9_THUNK smr_release(void* self RD9_THUNK_EDX)
{
    StaticMeshRenderer* p = static_cast<StaticMeshRenderer*>(self);
    const int remaining = --p->m_refCount;
    if (remaining == 0)
    {
        destroyStaticMeshRenderer(p);
        ::renddx9abi::engineFree(self);
    }
    return remaining;
}
// accepts iid 1 and 820013, returns this.
void* RD9_THUNK smr_qi(void* self RD9_THUNK_EDX, unsigned int iid)
{
    if (iid == RD9_IID_IBASE || iid == IID_IStaticMeshRenderer)
        return self;
    return 0;
}
// +0x10 nullsub_53 (empty), +0x14 `return 0`.
void RD9_THUNK smr_null10(void* RD9_THUNK_EDX) { }
int  RD9_THUNK smr_ret0  (void* RD9_THUNK_EDX) { return 0; }
// +0x18 setEnabled (byte store at +0x04), +0x1C getEnabled.
void RD9_THUNK smr_setEnabled(void* self RD9_THUNK_EDX, int v)
{ static_cast<StaticMeshRenderer*>(self)->setEnabled(static_cast<unsigned char>(v)); }
int  RD9_THUNK smr_getEnabled(void* self RD9_THUNK_EDX)
{ return static_cast<StaticMeshRenderer*>(self)->getEnabled(); }
// +0x24 getName - return the address of the embedded name string (this+0x08).
void* RD9_THUNK smr_getName(void* self RD9_THUNK_EDX) { return &static_cast<StaticMeshRenderer*>(self)->m_name; }

// +0x20 setName (string::operator= at this+8). The argument's type (std::string* vs char*) is not
// pinned and nothing recovered calls it on the singleton, so it is a named frontier rather than a
// guess at the ABI.
RD9_SMR_FRONTIER(smr_setName, "setName (0x10121A10)", 0x20)

#undef RD9_SMR_FRONTIER


// Releases loaded lightmap-atlas pages on CEUnloadLevel so the next level reloads them.
// Requires all ten atlas objects; ignores other event categories and ids.

void RD9_THUNK smr_handleEvent(void* self RD9_THUNK_EDX, int category, unsigned int id, void*, void*)
{
    if (category != 5 || id != 13)                                     // ECCore / CEUnloadLevel
        return;

    StaticMeshRenderer* const renderer = reinterpret_cast<StaticMeshRenderer*>(
        static_cast<unsigned char*>(self) - offsetof(StaticMeshRenderer, m_ehVptr));   // +0x24
    for (unsigned int i = 0; i < 10u; ++i)
    {
        dx9rend::TextureAtlas* const atlas = renderer->m_lightmapAtlases[i];   // [esi]
        if (dx9rend::textureAtlasEntryCount(atlas) > 0u)                       // [ecx+10h], jbe
            dx9rend::textureAtlasReleaseAll(atlas);
    }
}

const void* const kPrimaryVtable[10] =
{
    reinterpret_cast<void*>(&smr_addRef),        // +0x00
    reinterpret_cast<void*>(&smr_getRc),         // +0x04
    reinterpret_cast<void*>(&smr_release),       // +0x08
    reinterpret_cast<void*>(&smr_qi),            // +0x0C
    reinterpret_cast<void*>(&smr_null10),        // +0x10  nullsub_53
    reinterpret_cast<void*>(&smr_ret0),          // +0x14  return 0
    reinterpret_cast<void*>(&smr_setEnabled),    // +0x18  setEnabled([+4])
    reinterpret_cast<void*>(&smr_getEnabled),    // +0x1C  getEnabled
    reinterpret_cast<void*>(&smr_setName),       // +0x20  setName            [FRONTIER]
    reinterpret_cast<void*>(&smr_getName),       // +0x24  getName -> this+8
};

const void* const kEventHandlerVtable[1] =
{
    reinterpret_cast<void*>(&smr_handleEvent),   // +0x00  handleECCoreEvent  [FRONTIER]
};

} // anonymous namespace

void bundledMeshSetHemiEnabled(unsigned char enabled)
{
    // PE 0x1012B170/0x1012B460 dereference dword_102597F4 without a guard and call +0x10. The
    // singleton is the template-owned BundledMeshStateManager, not a separate Hemi object.
    g_bundledMeshStateManager->setEnabled(enabled != 0);
}

void staticMeshRendererSetHemiEnabled(unsigned char enabled)
{
    // Intentionally unguarded: PE calls singleton slot +0x18 without checking it.
    static_cast<StaticMeshRenderer*>(static_cast<void*>(g_StaticMeshRenderer))->setEnabled(enabled);
}

// geom_StaticMeshRenderer_ctor, 0x101220F0.  This recovers the shadow-load-bearing slice exactly:
// five declarations; StaticMesh and first-use BundledMesh effects; LightManager registration;
// technique choice; and retained parameters.  Cache/event/lightmap construction remains deferred.
namespace {
void constructStaticMeshRenderer(StaticMeshRenderer* self)
{
    self->m_vptr = const_cast<const void**>(kPrimaryVtable);
    self->m_enabled = 1;
    self->m_pad05[0] = 0; self->m_pad05[1] = 0; self->m_pad05[2] = 0;
    new (&self->m_name) std::string();
    self->m_ehVptr = const_cast<const void**>(kEventHandlerVtable);
    self->m_refCount = 1;

    static const D3DVERTEXELEMENT9 kStaticDecl[] =
    {
        {0, 0,  D3DDECLTYPE_SHORT4,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
        {0, 8,  D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL,   0},
        {0, 12, D3DDECLTYPE_SHORT2,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0},
        D3DDECL_END()
    };
    static const D3DVERTEXELEMENT9 kLightMapDecl[] =
    {
        {0, 0,  D3DDECLTYPE_SHORT4,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
        {0, 8,  D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL,   0},
        {0, 12, D3DDECLTYPE_SHORT2,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 1},
        {1, 0,  D3DDECLTYPE_SHORT2,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0},
        D3DDECL_END()
    };
    static const D3DVERTEXELEMENT9 kFloatStreamDecl[] =
    {
        {0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
        {1, 0, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0},
        D3DDECL_END()
    };
    static const D3DVERTEXELEMENT9 kShortStreamDecl[] =
    {
        {0, 0, D3DDECLTYPE_SHORT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
        {1, 0, D3DDECLTYPE_SHORT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0},
        D3DDECL_END()
    };

    self->m_decl2C = dx9rend::vertexDeclarationCreate(kStaticDecl);
    self->m_decl30 = dx9rend::vertexDeclarationCreate(kLightMapDecl);
    self->m_decl34 = dx9rend::vertexDeclarationCreate(kStaticDecl);
    self->m_decl38 = dx9rend::vertexDeclarationCreate(kFloatStreamDecl);
    self->m_decl3C = dx9rend::vertexDeclarationCreate(kShortStreamDecl);

    // Register the core-event listener before acquiring shadow effects.
    eventManager->registerEventHandler(
        ECCore, reinterpret_cast<IEventListener*>(&self->m_ehVptr), 0);

    self->m_shadowEffect = static_cast<dx9rend::Effect*>(dx9rend::shaderManagerGetEffect(
        dx9rend::g_shaderManager, "mods/xpack/Shaders/StaticMesh"));
    LightManager* const lightManager = g_LightManager;
    lightManager->registerShadowEffect(
        static_cast<int>(0x870CA653u),
        self->m_shadowEffect);
    if (lightManager->getDynamicShadowsEnabled())
        setShadowTechnique(self->m_shadowEffect);
    self->m_worldViewProj = getEffectParameter(self->m_shadowEffect, "WorldViewProjection");
    self->m_viewI         = getEffectParameter(self->m_shadowEffect, "ViewI");
    self->m_paraboloid    = getEffectParameter(self->m_shadowEffect, "ParaboloidValues");
    self->m_paraboloidZ   = getEffectParameter(self->m_shadowEffect, "ParaboloidZValues");
    self->m_parallax      = getEffectParameter(self->m_shadowEffect, "PARALLAXSCALEBIAS");

    if (!g_bundledShadowEffect)
    {
        g_bundledShadowEffect = static_cast<dx9rend::Effect*>(dx9rend::shaderManagerGetEffect(
            dx9rend::g_shaderManager, "mods/xpack/Shaders/BundledMesh"));
        lightManager->registerShadowEffect(
            static_cast<int>(0x870CA651u),
            g_bundledShadowEffect);
        if (lightManager->getDynamicShadowsEnabled() && g_bundledShadowEffect)
            setShadowTechnique(g_bundledShadowEffect);
        g_bundledShadowAlphaThreshold =
            getEffectParameter(g_bundledShadowEffect, "SHADOWALPHATHRESHOLD");
        g_bundledTexLayer0 = getEffectParameter(g_bundledShadowEffect, "TEXLAYER0");
        g_bundledOneBoneSkinning =
            getEffectParameter(g_bundledShadowEffect, "matONEBONESKINNING");
        g_bundledViewI = getEffectParameter(g_bundledShadowEffect, "ViewI");
    }

    self->m_flags60[0] = 1;
    self->m_flags60[1] = 0;
    self->m_flags60[2] = 1;
    self->m_flags60[3] = 0;
    self->m_flags60[4] = 1;
    self->m_byte94  = 0;
    self->m_dword98 = 9;
    self->m_dword9C = 0;

    // PE 0x10122159..0x1012218C - `10 times { engineAlloc(0x14); sub_10015E60 }`: the ten
    // lightmap TextureAtlas objects at +0x68..+0x8C, constructed in order.  An allocation failure
    // stores a null, as shipped.  ; see the header for the identification.
    for (unsigned int i = 0; i < 10u; ++i)
    {
        void* const storage = ::renddx9abi::engineAlloc(sizeof(dx9rend::TextureAtlas));
        self->m_lightmapAtlases[i] = storage ? dx9rend::textureAtlasCtor(storage) : 0;
    }

    // PE 0x1012218C..0x101221B5, immediately after the ten engineAlloc(0x14) cache objects and
    // before the vertex declarations: the StaticMeshStateManager singleton (retail 0x38 bytes) is
    // created lazily, and an allocation failure publishes a NULL singleton rather than leaving the
    // cell alone.
    if (!g_staticMeshStateManager)
    {
        void* const storage = ::renddx9abi::engineAlloc(sizeof(StaticMeshStateManager));
        g_staticMeshStateManager = storage ? staticMeshStateManagerConstruct(storage) : 0;
    }
}
} // anonymous namespace


// StaticMeshRenderer_tryLoadLightmapTexture, PE 0x10121F30.  Attested to
// this file's line 355 by its own diagnostic.  See the header for the contract.

// The base byte at +0x102 (g_rendererBase) gates only the diagnostic - BF2Render.cpp's step
// 14 tests the same byte - the flag clear happens either way.  The page texture is addRef'd
// through its vtable slot +0x08, which for the shared three-slot texture table IS addRef
// (Texture.h: "{getRef-at+4, nullsub, addRef-at+4}" - slot +0x08 bumps the count at +0x04).

bool staticMeshRendererTryLoadLightmapTexture(StaticMeshRenderer* self,
                                              const std::string& directory,
                                              const std::string& objectName,
                                              float outOffset[4],
                                              dx9rend::EngineTexture** outTexture)
{
    dx9rend::TextureAtlas* const atlas = self->m_lightmapAtlases[0];
    if (dx9rend::textureAtlasEntryCount(atlas) == 0)
    {
        std::string taiPath(directory);
        taiPath += "LightmapAtlas.tai";
        if (!dx9rend::textureAtlasLoadTAI(atlas, taiPath))              // TextureAtlas_loadTAI
        {
            self->m_flags60[4] = 0;
            const dx9rend::RendDX9Base* const base =
                static_cast<const dx9rend::RendDX9Base*>(dx9rend::g_rendererBase);
            if (!base->flag102)
            {
                ::renddx9abi::logf("StaticMeshRenderer.cpp:355 No lightmapatlas found, no "
                                   "lightmaps will be used.");
            }
            return false;
        }
        ++self->m_atlasesLoaded;
    }

    std::string name(directory);
    name += objectName;
    name += ".dds";
    dx9rend::pathToLower(name);

    dx9rend::TextureAtlasEntry* const entry = dx9rend::textureAtlasFind(atlas, name);
    if (!entry)
        return false;

    outOffset[0] = entry->width;
    outOffset[1] = entry->height;
    outOffset[2] = entry->uOffset;
    outOffset[3] = entry->vOffset;
    *outTexture  = entry->page;
    dx9rend::textureAddRef(entry->page);                                 // page vslot +0x08
    return true;
}


namespace {


// The part after the last '\\' or '/', or the whole path when neither occurs.
std::string lightmapBaseName(const std::string& path)
{
    int cut = static_cast<int>(path.rfind('\\'));
    const int forward = static_cast<int>(path.rfind('/'));
    if (forward > cut)
        cut = forward;
    if (cut == -1)
        return path;
    return path.substr(static_cast<std::string::size_type>(cut) + 1);
}

// The base name without its last '.' and what follows it.
std::string lightmapStem(const std::string& path)
{
    const std::string base = lightmapBaseName(path);
    const std::string::size_type dot = base.rfind('.');
    if (dot == std::string::npos)
        return base;
    return base.substr(0, dot);
}

// Appends directory + name when the listed file `name` is one of the lightmaps both atlas builders
// pack (see StaticMeshRenderer.h).
void collectLightmapFile(const std::string& name, unsigned int size, const std::string& directory,
                         const std::string& extension, std::list<std::string>& files)
{
    if (size == 0)
        return;
    if (name.find(extension.c_str(), 0, extension.size()) == std::string::npos)
        return;
    const std::string stem = lightmapStem(name);
    const std::string base = lightmapBaseName(name);
    if (stem.find("LightmapAtlas", 0, 13) != std::string::npos)
        return;
    std::string expected(stem);
    expected += extension;
    if (base.compare(expected) != 0)
        return;
    std::string path(directory);
    path += name.c_str();
    files.push_back(path);
}

// The builders' shared prologue: `directory` with a trailing '/' unless it already ends in a
// separator, and the lightmap files listed under it (recursively, in the FileManager's order).
void collectLightmapFiles(const std::string& directory, const std::string& extension,
                          std::string& normalised, std::list<std::string>& files)
{
    normalised = directory;
    if (normalised.size() != 0)
    {
        const char last = normalised.c_str()[normalised.size() - 1];
        if (last != '\\' && last != '/')
            normalised += std::string("/");
    }
    const std::string ext(extension.c_str(), extension.size());
    std::list<io::FileInfo> listed;
    fileManager->findFiles(std::string(normalised.c_str(), normalised.size()), true, listed);   // +0x34
    for (std::list<io::FileInfo>::const_iterator it = listed.begin(); it != listed.end(); ++it)
        collectLightmapFile(it->name, it->size, normalised, ext, files);
}

// The batch limit: a batch's command line stays below this many characters.
const unsigned int kAtlasCommandLineLimit = 32300u;

} // anonymous namespace

bool staticMeshRendererLoadAllLightMapFilesDll(StaticMeshRenderer* self,
                                               const std::string& directory,
                                               const std::string& extension)
{
    std::string normalised;
    std::list<std::string> files;
    collectLightmapFiles(directory, extension, normalised, files);

    std::string savePath;
    dx9rend::readStringSettingDefault("Editor_LevelSavePath", " ERROR ", savePath);
    std::string atlasName(normalised);
    atlasName += "LightmapAtlas";
    return dx9rend::textureAtlasBuild(self->m_lightmapAtlases[0], savePath, atlasName, savePath,
                                      files, false);
}

bool staticMeshRendererLoadAllLightMapFiles(StaticMeshRenderer* self,
                                            const std::string& directory,
                                            const std::string& extension)
{
    std::string normalised;
    std::list<std::string> files;
    collectLightmapFiles(directory, extension, normalised, files);

    std::string savePath;
    dx9rend::readStringSettingDefault("Editor_LevelSavePath", " ERROR ", savePath);
    std::list<std::string>::const_iterator next = files.begin();
    for (int batch = 0; ; ++batch)
    {
        std::list<std::string> batchFiles;
        char number[256];
        std::sprintf(number, "%d", batch);
        std::string command("dice_TextureAtlasTool -maxres 2048 -o ");
        command += normalised.c_str();
        command += "LightmapAtlas";
        command += number;
        command += " -output_path ";
        command += savePath.c_str();
        command += " -input_path ";
        command += savePath.c_str();
        command += " ";
        while (next != files.end() && command.size() < kAtlasCommandLineLimit)
        {
            command += next->c_str();
            command += " ";
            batchFiles.push_back(*next);
            ++next;
        }

        std::string atlasName(normalised);
        atlasName += "LightmapAtlas";
        std::string batchName(atlasName);
        batchName += number;
        if (!dx9rend::textureAtlasGenerate(self->m_lightmapAtlases[0], savePath, batchName, savePath,
                                           batchFiles, false))
            return false;
        if (next == files.end())
            return true;
    }
}

// RefFactory_geom_StaticMeshRenderer, 0x100B068E.
IBase* __fastcall createStaticMeshRenderer(unsigned int iid, IBase* outer)
{
    if (outer)
        return 0;
    void* storage = ::renddx9abi::engineAlloc(kStaticMeshRendererSize);
    if (!storage)
        return 0;
    ::memset(storage, 0, kStaticMeshRendererSize);
    StaticMeshRenderer* self = static_cast<StaticMeshRenderer*>(storage);
    constructStaticMeshRenderer(self);

    void* itf = smr_qi(self RD9_THUNK_ARG, iid);
    if (!itf)
        smr_release(self RD9_THUNK_ARG);
    return static_cast<IBase*>(itf);
}

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
