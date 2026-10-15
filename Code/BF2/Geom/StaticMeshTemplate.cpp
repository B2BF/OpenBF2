// Path: attested by RendDX9.dll __FILE__ at 0x1021C2B8.
// Static mesh loading, properties and lifetime, recovered from RendDX9.dll 1.50 (PE only).
// Material merging and vertex-buffer construction live in StaticMeshMaterial.cpp;
// base readers live in MeshTemplate.cpp. Named [FRONTIER] bodies remain explicit.

#ifdef BF2_IMAGE_RENDDX9

#include "../../RendDX9Abi/HfeAbi.h"
#include "../../RendDX9Abi/BfMemory.h"
#include "../../RendDX9Abi/Frontier.h"
#include "../../RendDX9Abi/PluginEntry.h"
#include "../../RendDX9Abi/PluginAbi.h"
#include <string>
#include "../RendDX9/RendDX9Base.h"
#include "../RendDX9/Ra/RaShader.h"
#include "../RendDX9/Ra/RaShaderPermutator.h"
#include "../Game/Main/IVideoOptions.h"
#include "StaticMeshTemplate.h"
#include "StaticMesh.h"

#include <map>
#include <new>
#include <string.h>
#include <math.h>                        // loadOccluders' normalize (sub_100777A0)
#include <stdlib.h>                      // loadOccluders' numeric scans - see OccScanner

namespace dice {
namespace hfe {

namespace dx9rend {

// BF2Render.cpp - 0x1023D098. readLods and readMaterials read three ints out of it directly, the
// way the PE does; the Renderer class itself is not laid out in this translation unit.
extern IBase* g_Renderer;

// TextureManager.cpp - sub_10017DF0 (TM+0x98). readMaterials saves and restores it around the
// material pass. `textureManagerSwapDefaultMipClass` (sub_10017E00) is already in RendDX9Base.h.
int textureManagerDefaultMipClass();

// BufferManager.cpp - the shipped inline `if (!--wrapper[1]) { sub_10014510(wrapper);
// engineFree(wrapper); }` dance the destructor performs seven times.
void vertexBufferRelease(void* wrapper);

// RenderQueues.cpp - dword_1023E408, the 184-byte render-queue table whose tail is the Ra shader
// table. Every `getShader` call in initTechniques loads it straight into ecx (0x10116F53 and the
// eleven rows after it, plus 0x1019243D inside the mask builder) with no null test; neither does
// this declaration's users. RaShader.h models the fields the lookups touch.
extern void* g_renderQueues;

// BF2Render.cpp - the shared typed seam for Renderer vtable slot +0xAC (0x10032740,
// `return *(BYTE*)(this + 1892)`). initTechniques picks "Water" over "WaterBase" with it at
// The shipped call site loads g_Renderer (0x1023D098) rather than g_rendererBase
// (0x1023B970); BF2Render.cpp records that both globals hold the same Renderer `this`.
bool rendererIsExpansionPack();

} // namespace dx9rend

namespace geom {


// Everything this translation unit shares with StaticMeshStateManager.cpp - the singleton, the
// by-singleton createMaterial form (PE 0x101A9270, reached as
// `sub_101A9270(this+0x30, stream, this[42], this)` with ecx = dword_102651C0), the shader-combo
// flag table dword_1023A9D0 and the per-quality mask read - is declared in StaticMeshTemplate.h.


namespace {


// Renderer integer fields, named by their retail byte offset through BF2Render.cpp's tail
// accessor; the Renderer's full layout is not recovered.

int rendererField(unsigned int byteOffset)
{
    if (!::dice::hfe::dx9rend::g_Renderer)
        return 0;
    return ::dice::hfe::dx9rend::rendererTailInt(byteOffset);
}

// readLods: g_Renderer + 2020 (0x7E4) and + 2028 (0x7EC) - the renderer-level lod-skip settings.
// [INFERRED] roles: the first is a flat number of lods to drop, the second is an offset added to
// the template's own +0x2B8 before the result is clamped at zero.
int rendererLodSkipBase()   { return rendererField(2020); }
int rendererLodSkipBias()   { return rendererField(2028); }

// readMaterials / initTechniques: g_Renderer + 1740 (0x6CC) - the shader quality index.
unsigned int rendererShaderQuality()
{
    return static_cast<unsigned int>(rendererField(1740));
}

// sub_1000D950 on dword_1023B988 - the ResourceManager byte at +0x04. TextureManager.cpp has the
// same one-line body behind an anonymous namespace, so the shared typed accessor is used here.
bool resourceLoadingDeferred()
{
    return ::dice::hfe::dx9rend::resourceManagerIsReady();
}


// sub_10075110 - case-insensitive substring search. Read in full: it copies both strings, lowers
// each with sub_10074D10 (RendDX9Base.h's pathToLower - tolower plus the five Swedish maps), and
// then runs std::string::find on the lowered pair. Reproduced with the same two lowered copies.

const unsigned int kNpos = 0xFFFFFFFFu;

// sub_1007B220 - the shared unit scale the constructor publishes into both +0x138 and +0x144.
// MeshTemplate.cpp carries the same file-local helper with the full evidence note (a VC7.1
// magic-static Vec3(1,1,1) at dword_10247BFC); both copies are file-scope, as the shipped one is.
const Vec3& unitScale()
{
    static Vec3 value = { 1.0f, 1.0f, 1.0f };
    return value;
}

unsigned int findCaseInsensitive(const std::string& haystack, const std::string& needle)
{
    std::string lowerHaystack(haystack.c_str());
    std::string lowerNeedle(needle.c_str());
    ::dice::hfe::dx9rend::pathToLower(lowerHaystack);
    ::dice::hfe::dx9rend::pathToLower(lowerNeedle);

    const char* found = ::strstr(lowerHaystack.c_str(), lowerNeedle.c_str());
    if (!found)
        return kNpos;
    return static_cast<unsigned int>(found - lowerHaystack.c_str());
}

// std::string::find(const char*, 0, len) - the case-sensitive one load() uses for "vegitation".
unsigned int findCaseSensitive(const std::string& haystack, const char* needle)
{
    const char* found = ::strstr(haystack.c_str(), needle);
    if (!found)
        return kNpos;
    return static_cast<unsigned int>(found - haystack.c_str());
}


// sub_1011D380's fill element - vector<StaticMeshLod>::resize(count, StaticMeshLod()). The by-value
// element on the stack is 104 bytes with +0x04..+0x23 zeroed and the MeshBounds `dirty` byte at
// +0x24 set to 1; the shipped +0x00 (nodeCount) is left uninitialised, which readLods memsets away
// before any read, so it is zeroed here rather than copied indeterminate.
StaticMeshLod staticMeshLodFill()
{
    StaticMeshLod lod;
    ::memset(&lod, 0, sizeof(lod));
    lod.bounds.dirty = 1;
    return lod;
}

// The D3D "preload" seam behind slot +0xAC. sub_10014610 (vertex buffer), sub_10014400 (index
// buffer) and sub_10014920 (texture) are comdat-identical: `if (wrapper[2]) wrapper[2]->vtbl[9]()`,
// and vtable slot 9 of every IDirect3DResource9 is PreLoad.

// Index-buffer access requires a constructed buffer; the wrapper is not null-checked.

struct D3DResourceWrapper
{
    void*                vptr;      // +0x00
    int                  refCount;  // +0x04
    IDirect3DResource9*  resource;  // +0x08
};

void resourcePreload(void* wrapper)
{
    D3DResourceWrapper* self = static_cast<D3DResourceWrapper*>(wrapper);
    if (self->resource)
        self->resource->PreLoad();
}


// Technique tables initialized before material loading.

// byte_1023982C is the once-gate: it ships as 1 and the initialiser clears it on success.


unsigned char g_staticMeshTechniquesPending = 1;        // byte_1023982C, initial value 1


// Private technique registry keyed by unsigned mask. Its nodes are not shared across
// image boundaries and must not use the 48-byte string-key compatibility helpers.

typedef std::map<unsigned int, ::dice::hfe::dx9rend::Ra::RaShader*> StaticMeshTechniqueMap;

StaticMeshTechniqueMap g_staticMeshTechniques;

// The shader-quality mask vector operations the shared builder below performs (StaticMesh,
// BundledMesh and SkinnedMesh all hand in a std::vector).

void maskVectorPushBack(std::vector<unsigned int>& v, unsigned int value)
{
    v.push_back(value);
}

void maskVectorResize(std::vector<unsigned int>& v, unsigned int count, unsigned int value)
{
    v.resize(count, value);
}

unsigned int maskVectorSize(const std::vector<unsigned int>& v)
{
    return static_cast<unsigned int>(v.size());
}

unsigned int* maskVectorData(std::vector<unsigned int>& v)
{
    return v.data();
}

// Shader feature names in bit order: each entry encodes 1 << index.

const char* const kStaticMeshShaderBitNames[] =
{
    "lightmap", "base", "detail", "dirt", "crack", "parallaxdetail",
    "nbase", "ndetail", "ncrack", "shadow", "pointlight", "alphatest",
    0
};


// Returns 1 << index for a name in the null-terminated table, or zero without logging.
// names must be nonnull. The shift counter retains its byte-width wrap behavior.

unsigned int shaderBitFromName(const char* const* names, const std::string& name)
{
    unsigned char shift = 0;

    if (*names == 0)
        return 0;

    const char* const* it = names;
    for (;;)
    {
        // The shipped body copy-constructs a std::string from the table entry for every probe and
        // destroys it again; sub_10075000 is _stricmp on the two c_str()s.
        const std::string candidate(*it);
        const bool match = ::_stricmp(name.c_str(), candidate.c_str()) == 0;
        if (match)
            break;

        ++it;
        ++shift;
        if (*it == 0)
            return 0;
    }

    return 1u << shift;
}


// Builds per-quality masks and registered techniques from a MetaFx.
// bitNames is a NUL-terminated feature-name table.

bool staticMeshBuildTechniqueMasks(const std::string& mfxPath,
                                   const std::string& techniquePrefix,
                                   const char* const* bitNames,
                                   StaticMeshTechniqueMap& techniqueMap,
                                   std::vector<unsigned int>& maskVector)
{
    // sub_100EB1E0(head->parent), the recursive _Erase of the whole tree,
    // then the four header stores that relink it empty. That is map::clear() on 24-byte nodes; see
    // the note on g_staticMeshTechniques above for why this is no longer the 48-byte helper.
    techniqueMap.clear();

    // 0x10192204 - `mov ecx, dword_1023D954; push edx; call sub_10033D40`.
    ::dice::hfe::dx9rend::Ra::MetaFx* const fx =
        ::dice::hfe::dx9rend::effectManagerGetMetaFx(mfxPath);
    if (!fx)
        return false;                       // pop esi / xor al, al / retn 14h

    // 0x10192223 - `lea esi, [ebx+0A8h]; test esi, esi; jz loc_10192316`.

    // Keep the null branch around the deprecation-list accessor.
    const std::vector< ::dice::hfe::dx9rend::Ra::MetaFxStringArray>* const deprecation =
        &fx->getDeprecationList();

    if (deprecation)
    {
        // sub_100AEFF0(0, 0) on the mask vector: resize(0), i.e. clear.
        maskVectorResize(maskVector, 0u, 0u);

        unsigned int accumulated = 0;                       // var_8
        maskVectorPushBack(maskVector, 0xFFFFFFFFu);        // 0x10192254, sub_100E22D0(&-1)
        unsigned int passCount = 1;                         // arg_10, reused as the counter

        // Each row adds features to drop; lower quality levels inherit all previous exclusions.
        for (std::vector< ::dice::hfe::dx9rend::Ra::MetaFxStringArray>::const_iterator row =
                 deprecation->begin();
             row != deprecation->end(); ++row)
        {
            unsigned int groupFlags = 0;                    // arg_0, reused
            for (::dice::hfe::dx9rend::Ra::MetaFxStringArray::const_iterator name = row->begin();
                 name != row->end(); ++name)
            {
                groupFlags |= shaderBitFromName(bitNames, *name);
            }

            accumulated |= groupFlags;
            maskVectorPushBack(maskVector, ~accumulated);
            ++passCount;
        }

        // 0x101922FC - `if (passCount < 4) passCount = 4; resize(passCount, -1)`. An empty
        // deprecation list reaches this with passCount == 1 by falling through 0x10192304.
        maskVectorResize(maskVector, passCount < 4u ? 4u : passCount, 0xFFFFFFFFu);
    }

    // 0x10192316 - `quality = g_Renderer[0x6CC]; if (maskVector.size() < quality) quality = size`.

    // THE clamp IS `<`, NOT `<=`, SO IT clamps TO size() AND NOT size()-1. A renderer quality
    // index at or past the number of mask entries indexes one element past the end, twice, just
    // below. Preserved: it is the shipped comparison (`cmp eax, esi; jnb short`).
    unsigned int quality = rendererShaderQuality();
    const unsigned int maskCount = maskVectorSize(maskVector);
    if (maskCount < quality)
        quality = maskCount;

    // The lookup uses "hasshadow", but the table names it "shadow"; clearing the
    // resulting zero mask intentionally leaves the shadow bit set.
    if (!::dice::hfe::dx9rend::rendererGetVideoOptions()->getQuality(4))
    {
        const std::string hasShadow("hasshadow");

        // The shipped body computes the element address before the call (0x10192361) and applies
        // the AND after it. Kept in that order because the element address is taken from the
        // vector's _First as it stands at that instant.
        unsigned int* const slot = maskVectorData(maskVector) + quality;
        *slot &= ~shaderBitFromName(bitNames, hasShadow);
    }

    const unsigned int mask = maskVectorData(maskVector)[quality];

    // Numeric bit masks key permutations, collapsing duplicates before ordered registration.
    std::map<unsigned int, std::string> techniqueList;

    if (fx->buildPermutationBits(techniqueList, bitNames))
    {
        // the ordinary in-order walk from _Myhead->_Left to _Myhead.
        for (std::map<unsigned int, std::string>::const_iterator it = techniqueList.begin();
             it != techniqueList.end(); ++it)
        {
            const unsigned int bits = it->first;

            // 0x101923E0 - `find(bits & mask) == end()` AND `(mask & bits) == bits`. The second
            // test makes the first a plain `find(bits)`: a permutation is registered only when
            // every bit it needs survives this quality level and nothing has claimed it yet.
            if (techniqueMap.find(bits & mask) == techniqueMap.end() &&
                (mask & bits) == bits)
            {
                // 0x1019240E - `string(prefix) += name`, then a copy for the call argument. The
                // shipped pair of temporaries is the inlining of `techniquePrefix + name`.
                std::string techniqueName(techniquePrefix);
                techniqueName += it->second;

                // 0x1019243D - `mov ecx, dword_1023E408; call sub_10041F20` with skipFileCheck
                // true here and false at every row of initTechniques: a generated permutation has
                // no shaders/rashader<name>.fx on disk, so it must be built rather than looked up.
                ::dice::hfe::dx9rend::Ra::RaShader* const shader =
                    ::dice::hfe::dx9rend::Ra::getShader(
                        static_cast< ::dice::hfe::dx9rend::Ra::RaShaderTable*>(
                            ::dice::hfe::dx9rend::g_renderQueues),
                        techniqueName, true);

                // 0x1019245B, sub_100EBA10 - insert(pair), which keeps an existing key. The guard
                // above already proved there is none.
                techniqueMap.insert(std::make_pair(bits, shader));
            }
        }

        // sub_1003C960 with ecx = fx. Drops every generated .fx from the effect
        // manager's in-memory file store now that the shaders that wanted them are built.
        fx->dropGeneratedFiles();
    }

    return true;                            // mov al, 1
}


// Registers a file-backed fixed technique without replacing an existing mask entry.
// Generated permutations use a separate path that skips file-existence checks.

void staticMeshRegisterTechnique(const char* name, unsigned int flags)
{
    const std::string techniqueName(name);
    ::dice::hfe::dx9rend::Ra::RaShader* const shader =
        ::dice::hfe::dx9rend::Ra::getShader(
            static_cast< ::dice::hfe::dx9rend::Ra::RaShaderTable*>(
                ::dice::hfe::dx9rend::g_renderQueues),
            techniqueName, false);

    g_staticMeshTechniques.insert(std::make_pair(flags, shader));
}


// Initializes shared static-mesh techniques and quality masks while their gate is pending.
// Preserve shader-feature masks and repeated VideoOptions queries for each quality test.

void staticMeshInitTechniques()
{
    // Report build failure only when this call actually attempted initialization.
    bool built = false;
    bool attempted = false;
    if (g_staticMeshTechniquesPending)                  // byte_1023982C, 0x10116EAC
    {
        attempted = true;
        std::string prefix("STM");
        std::string mfx("shaders/RaShaderSTM.mfx");
        built = staticMeshBuildTechniqueMasks(mfx, prefix, kStaticMeshShaderBitNames,
                                              g_staticMeshTechniques,
                                              g_staticMeshQualityMasks);
    }

    if (!built)
    {
        // Diagnose an empty quality-mask table once. The following compatibility access
        // remains unchecked, so this report identifies the cause without suppressing the fault.
        static bool reported = false;
        if (attempted && !reported)
        {
            reported = true;
            ::renddx9abi::logf(
                "StaticMeshTemplate: the STM technique init FAILED, so the quality-mask vector is "
                "empty and the next material read will fault at staticMeshQualityMask() reading "
                "address 0. That fault is a SYMPTOM - look above for the metaShader load failure. "
                "Retail indexes this vector unguarded too; no guard is added here.");
        }
        return;                                         // 0x10116F27, jz loc_10117325
    }

    // the point-light quality setting.
    if (::dice::hfe::dx9rend::rendererGetVideoOptions()->getQuality(3))
        staticMeshRegisterTechnique("Leafpointlight", 0xF0000400u);

    // the shadow quality setting.
    if (::dice::hfe::dx9rend::rendererGetVideoOptions()->getQuality(4))
    {
        staticMeshRegisterTechnique("TrunkSTMDetailShadowed", 0xF9000200u);
        staticMeshRegisterTechnique("TrunkSTMBaseShadowed",   0xFA000200u);
        staticMeshRegisterTechnique("LeafShadowed",           0xF0000200u);
    }

    // a second, separately re-fetched shadow test, with the point-light test nested
    // inside it rather than ANDed with it. Written the way the image branches.
    if (::dice::hfe::dx9rend::rendererGetVideoOptions()->getQuality(4))
    {
        if (::dice::hfe::dx9rend::rendererGetVideoOptions()->getQuality(3))
            staticMeshRegisterTechnique("LeafpointlightShadowed", 0xF0000600u);
    }

    staticMeshRegisterTechnique("TrunkSTMDetail",    0xF9000000u);
    staticMeshRegisterTechnique("TrunkSTMBase",      0xFA000000u);
    staticMeshRegisterTechnique("Leaf",              0xF0000000u);
    staticMeshRegisterTechnique("Road",              0xF1000000u);
    staticMeshRegisterTechnique("RoadDetailNoBlend", 0xF4000000u);
    staticMeshRegisterTechnique("RoadDetail",        0xF2000000u);

    // Renderer vslot +0xAC, `return *(BYTE*)(this + 1892)`: the expansion-pack flag
    // picks the full water shader over the base one. The name is chosen before the string is
    // constructed, which is why this is one row and not two.
    staticMeshRegisterTechnique(
        ::dice::hfe::dx9rend::rendererIsExpansionPack() ? "Water" : "WaterBase", 0xF8000000u);

    staticMeshRegisterTechnique("Default",           0x01000000u);

    g_staticMeshTechniquesPending = 0;
}

} // anonymous namespace


// The factory registered under "dice.hfe.geom.GeometryTemplate.StaticMesh".
// PE 0x100B064C (RefFactory_geom_GeometryTemplate_StaticMesh), retail allocation 0x2CC.

IBase* __fastcall createStaticMeshTemplate(unsigned int iid, IBase* outer)
{
    if (outer)
        return 0;

    void* storage = ::renddx9abi::engineAlloc(sizeof(StaticMeshTemplate));
    StaticMeshTemplate* self = 0;
    if (storage)
        self = new (storage) StaticMeshTemplate();

    // The shipped body dispatches through the object it just built without testing it: a failed
    // allocation faults on the queryInterface call. Preserved.
    IBase* result = static_cast<IBase*>(self->queryInterface(iid));
    if (!result)
        self->release();
    return result;
}


// Constructor - PE 0x10119E00. The base MeshTemplate constructor (sub_101924F0) runs first, then
// the two vtable slots, then the members in shipped order. The last store is `m_keepData = 0`,
// which reaches back into the base.

StaticMeshTemplate::StaticMeshTemplate()
    : MeshTemplate()
{
    m_refCount = 1;                       // +0xC8

    // m_materials (+0xCC), m_geoms (+0xDC) and m_occluderGroups (+0x2BC) are default-constructed
    // empty by the compiler.

    // m_instances (+0xEC, its head node from sub_1011B6F0) is default-constructed empty.

    // +0xFC MeshBounds - eleven zero dwords with ONE byte store in the middle: the `dirty` flag at
    // +0x11C (this + 284) is set to 1, exactly as meshBoundsExpand would leave it. The three pad
    // bytes after it are NOT touched, so they stay whatever the allocator handed over.
    m_bounds.min.x = 0.0f; m_bounds.min.y = 0.0f; m_bounds.min.z = 0.0f;
    m_bounds.max.x = 0.0f; m_bounds.max.y = 0.0f; m_bounds.max.z = 0.0f;
    m_bounds.originRadius = 0.0f;
    m_bounds.radius = 0.0f;
    m_bounds.dirty = 1;
    m_bounds.extent.x = 0.0f; m_bounds.extent.y = 0.0f; m_bounds.extent.z = 0.0f;
    m_bounds.center.x = 0.0f; m_bounds.center.y = 0.0f; m_bounds.center.z = 0.0f;

    // sub_1007B220 - the function-local `static const Vec3(1,1,1)` MeshTemplate.cpp names
    // unitScale(). The PE calls it twice, once per scale, and copies z, y, x in that order; both
    // calls are kept. (MeshTemplate.cpp's copy is file-local, so this one is too.)
    m_decompressScale = unitScale();                 // +0x138
    m_compressScale   = unitScale();                 // +0x144
    // +0x150 m_uvDecompress is NOT written by the constructor.

    m_vertexStream = 0;                   // +0x154
    m_tangentStream = 0;                  // +0x16C
    // +0x184 m_hasTwoTangentSets is NOT written by the constructor - it starts as whatever the
    // 0x2CC-byte allocation held, and only buildVertexBuffers ever sets it. Left uninitialised.
    m_finished = 0;                       // +0x185
    m_hasAlphaMaterial = 0;               // +0x186
    m_shaderFlagsAll = 0;                 // +0x188
    m_shaderFlagsUsed = 0;                // +0x18C
    m_byte190 = 0;                        // +0x190
    m_skippingLods = 0;                   // +0x191
    m_lightmapOverride = 0;               // +0x192
    m_doNotGenerateLightmaps = 0;         // +0x193
    m_compressVertexData = 1;             // +0x294
    m_maxTextureRepeat = 16;              // +0x298
    m_isRoad = 0;                         // +0x29C
    m_roadNoBlend = 0;                    // +0x29D
    m_isVegetation = 0;                   // +0x29E
    m_dword2A0 = -1;                      // +0x2A0
    m_isWater = 0;                        // +0x2A4
    m_byte2A5 = 0;                        // +0x2A5
    m_float2A8 = 10.0f;                   // +0x2A8
    m_float2AC = 25.0f;                   // +0x2AC
    m_byte2B0 = 0;                        // +0x2B0
    m_keepVegetationNormals = 0;          // +0x2B1
    m_dword2B4 = 0;                       // +0x2B4
    m_dword2B8 = 0;                       // +0x2B8

    m_uvStreams[0] = 0;                  // +0x158 .. +0x168 - stored after the tail above
    m_uvStreams[1] = 0;
    m_uvStreams[2] = 0;
    m_uvStreams[3] = 0;
    m_uvStreams[4] = 0;

    ::memset(m_lightmapSizes, 0xFF, sizeof(m_lightmapSizes));   // +0x194, 0x100 bytes

    m_keepData = 0;                       // +0x04, the very last store
}


// sub_10115FA0 - the first half of the destructor: release every material's shader record, then
// drop the six vertex streams and the tangent stream.

// The material walk is driven by the base's +0x50 lod-count vector, not by the material containers
// themselves: geoms = m_lodCounts.size(), and for each geom it visits
// m_lodCounts[geom] lods. A shader record is released through IBase slot +0x08 (see the file
// header for the corrected vtable reading).

namespace {

void staticMeshReleaseShadersAndStreams(StaticMeshTemplate* self)
{
    const unsigned int geomCount = static_cast<unsigned int>(self->m_lodCounts.size());

    for (unsigned int geom = 0; geom < geomCount; ++geom)
    {
        const int lodCount = self->m_lodCounts[geom];
        if (!lodCount)
            continue;

        StaticMeshMaterialList* lodVectors = self->m_materials.data()[geom].data();

        for (int lod = 0; lod < lodCount; ++lod)
        {
            StaticMeshMaterialList& materials = lodVectors[lod];
            if (!materials.data())
                continue;
            const unsigned int count = static_cast<unsigned int>(materials.size());
            for (unsigned int i = 0; i < count; ++i)
            {
                StaticMeshMaterial* material = materials.data() + i;
                if (material->m_shaderMaterial)
                    reinterpret_cast<IBase*>(material->m_shaderMaterial)->release();
            }
        }
    }

    // `if (p && !--p[1]) { vertexBufferDtor(p); engineFree(p); }` - the shipped inline wrapper
    // release, with sub_10014510 as the destructor. BufferManager.cpp carries the same dance as
    // vertexBufferRelease(); it is repeated inline here because the PE repeats it inline, seven
    // times, and because the wrapper destructor's resource-tracker row is that file's frontier.
    if (self->m_vertexStream)
        ::dice::hfe::dx9rend::vertexBufferRelease(self->m_vertexStream);
    for (int i = 0; i < 5; ++i)
        if (self->m_uvStreams[i])
            ::dice::hfe::dx9rend::vertexBufferRelease(self->m_uvStreams[i]);
    if (self->m_tangentStream)
        ::dice::hfe::dx9rend::vertexBufferRelease(self->m_tangentStream);
}

} // anonymous namespace


// Destructor body - PE 0x10119FF0. Both vtable slots are restored first (MSVC's usual
// destructor preamble), then the members go down in shipped order: shader records and streams,
// the occluder groups, the instance set, the geom vector, the material vector, base. The storage
// is freed afterwards without a C++ destructor, so the std members are destroyed explicitly.

void StaticMeshTemplate::destroyMembers()
{
    staticMeshReleaseShadersAndStreams(this);
    m_occluderGroups.~OccluderGroupVector();                      // sub_10120D40 on +0x2BC

    m_instances.~InstanceSet();                                   // sub_101006E0(begin, end)

    m_geoms.~GeomVector();                                        // sub_1011E640 per geom

    m_materials.~StaticMeshMaterialTree();     // every material through its destructor, then the storage

    MeshTemplate::destroyMembers();
}

// the deleting destructor at slot +0xB0.
void* StaticMeshTemplate::destroy(unsigned char flags)
{
    destroyMembers();
    if ((flags & 1) != 0)
        ::renddx9abi::engineFree(this);
    return this;
}


// The overridden IBase quartet. StaticMesh counts at +0xC8 rather than the base's +0x10; the
// queryInterface at 0x100EE940 is shared with MeshTemplate (one body, two vtables).

void StaticMeshTemplate::addRef()
{
    ++m_refCount;
}

int StaticMeshTemplate::getRef()
{
    return m_refCount;
}

int StaticMeshTemplate::release()
{
    const int remaining = --m_refCount;
    if (!remaining)
        destroy(1);                                               // through the +0xB0 slot
    return remaining;
}

void* StaticMeshTemplate::queryInterface(unsigned int iid) const
{
    if (iid != 1 && iid != 0x17701FFFu)
    {
        // The `&& this` test is the shipped one. It cannot fail in a member function, but MSVC
        // emitted it because the returned pointer is `this + 12` and a null base must stay null.
        if (iid == 0x3709A4FFu && this)
            return const_cast<void*>(static_cast<const void*>(
                static_cast<const ::dice::hfe::io::IConsoleSaveable*>(this)));
        return 0;
    }
    return const_cast<StaticMeshTemplate*>(this);
}

// a plain load of GeometryTemplate_StaticMesh_reflectionTypeId.
unsigned int StaticMeshTemplate::getClassId() const
{
    return 0x870CA653u;
}


// createInstance - PE 0x10116E00.

void* StaticMeshTemplate::createInstance()
{
    void* storage = ::renddx9abi::engineAlloc(sizeof(StaticMesh));    // retail 0x37C
    void* instance = 0;
    if (storage)
    {
        // sub_100FDC20 with ecx = the allocation and the template pushed; see Geom/StaticMesh.cpp.
        instance = staticMeshConstruct(storage, this);
    }

    m_instances.insert(static_cast<StaticMesh*>(instance));       // sub_1011BEC0 into +0xEC

    if (!m_loaded && !resourceLoadingDeferred())
        load();                                                   // through the +0x2C slot

    m_keepData = 1;
    return instance;
}


// Loads <path>Meshes/<name>.staticMesh; returns false when already loaded.
// Both candidate paths intentionally use the same expression and are tested separately.
// The occluder path omits the Meshes/ component.

bool StaticMeshTemplate::load()
{
    if (m_loaded)
        return false;

    static const char* const kMeshesFolder = "Meshes/";           // unk_10263DD4

    std::string candidateA(m_path.c_str());
    candidateA += kMeshesFolder;
    candidateA += m_name;
    candidateA += ".staticMesh";

    std::string candidateB(m_path.c_str());
    candidateB += kMeshesFolder;
    candidateB += m_name;
    candidateB += ".staticMesh";

    ::renddx9abi::HostFileManager* files = ::dice::hfe::fileManager;

    if (!files->fileExists(::renddx9abi::makeHostString(candidateA)))     // +0x1C
    {
        // StaticMeshTemplate.cpp:455, Debug type 6.
        ::renddx9abi::logf("StaticMeshTemplate.cpp:455: Cannot load StaticMesh %s "
                           "Please make sure it exists!", candidateA.c_str());
        return false;
    }

    m_isVegetation =
        static_cast<unsigned char>(findCaseSensitive(candidateA, "vegitation") != kNpos);
    m_isWater =
        static_cast<unsigned char>(findCaseInsensitive(candidateA,
                                                       std::string("objects/water/")) != kNpos);
    m_isRoad =
        static_cast<unsigned char>(findCaseInsensitive(candidateA,
                                                       std::string("objects/roads/")) != kNpos);
    if (m_isRoad)
        m_roadNoBlend =
            static_cast<unsigned char>(findCaseInsensitive(candidateA,
                                                           std::string("noBlend")) != kNpos);

    m_dword8C = 0;

    // The second existence test picks which of the two identical paths is opened. Since they are
    // the same string, candidate B is always the one taken when the file is there at all.
    const bool haveB = files->fileExists(::renddx9abi::makeHostString(candidateB));
    ::renddx9abi::HostStream* stream = files->open(
        ::renddx9abi::makeHostString(haveB ? candidateB : candidateA),
        static_cast<::renddx9abi::HostFileMode>(2),
        static_cast<::renddx9abi::HostFileAccess>(0), 0);           // +0x18

    if (stream)
    {
        const bool ok = loadFromStream(static_cast<::renddx9abi::HostNonlinearStream*>(stream));
        stream->release();                                          // +0x08
        if (!ok)
            return false;
    }
    // note THE missing ARM: when open() returns null the shipped body does NOT return - it falls
    // straight through and marks the template loaded. Preserved.

    m_loaded = 1;

    std::string occPath(m_path.c_str());                           // sub_10001920: path + name
    occPath += m_name;
    occPath += ".occ";

    ::renddx9abi::HostStream* occ = files->open(
        ::renddx9abi::makeHostString(occPath),
        static_cast<::renddx9abi::HostFileMode>(2),
        static_cast<::renddx9abi::HostFileAccess>(0), 0);
    if (occ)
    {
        loadOccluders(occ);
        occ->release();                                             // +0x08
    }

    // Walk the live instances and hand each the freshly loaded template. The set is empty until
    // createInstance has run, and the callback belongs to the unrecovered instance class.
    for (InstanceSet::const_iterator node = m_instances.begin(); node != m_instances.end(); ++node)
    {
        StaticMesh* const instance = *node;
        if (instance)
            instance->postLoad();
    }

    return true;
}


// setLodDistance - PE 0x10119110. Two arms: with a loaded geom vector it writes into the geom's
// own lodDistances (shifted down by the number of lods readLods skipped); without one it grows the
// base's +0x60 vector<vector<float>> and pre-fills the defaults.

void StaticMeshTemplate::setLodDistance(int geom, int lod, float distance)
{
    if (geom < 0 || lod < 0 || distance < 0.0f)
    {
        ::renddx9abi::logf("StaticMeshTemplate.cpp:555: Negative subgeometry lodswitch or "
                           "distance %s", m_name.c_str());
        return;
    }

    if (!m_geoms.empty())
    {
        const int geomCount = static_cast<int>(m_geoms.size());
        if (geom >= geomCount)
        {
            ::renddx9abi::logf("StaticMeshTemplate.cpp:563:  Invalid Subgeometry %s",
                               m_name.c_str());
            return;
        }

        const int skipped = m_geomInts[geom];
        StaticMeshGeom& g = m_geoms[geom];
        const int stored  = static_cast<int>(g.lodDistances.size());
        const int limit   = skipped + stored;                       // sub_1004E300 == size()

        if (lod >= limit)
        {
            // The decompiler shuffles the three inserted values across phantom stack slots; the
            // literal text and its operands are recovered, the exact interleave is [INFERRED].
            ::renddx9abi::logf("StaticMeshTemplate.cpp:571:  Invalid lodSwitch %d must be smaller "
                               "than %d %s", lod, limit, m_name.c_str());
            return;
        }

        if (lod >= skipped)
            g.lodDistances[lod - skipped] = distance;
        return;
    }

    // the not-yet-loaded arm
    bool grewOuter = false;
    if (m_lodDistances.size() < static_cast<unsigned int>(geom + 1))
    {
        m_lodDistances.resize(static_cast<unsigned int>(geom + 1));        // sub_100EE010
        grewOuter = true;
    }

    std::vector<float>& inner = m_lodDistances[geom];
    if (inner.size() < static_cast<unsigned int>(lod + 1))
    {
        inner.resize(static_cast<unsigned int>(lod + 1), 0.0f);           // sub_100EC600
        if (grewOuter)
        {
            // The shipped fill is unrolled four wide (25/50/75/100, then +100 per group) with a
            // scalar tail whose seed is `25 * (i + 1)`. Both forms compute the same sequence.
            float* values = inner.data();
            for (int i = 0; i < lod; ++i)
                values[i] = static_cast<float>((i + 1) * 25);
        }
    }

    m_lodDistances[geom][lod] = distance;
}

// Unlike the setter, the getter does NOT shift by the skipped-lod count.
float StaticMeshTemplate::getLodDistance(int geom, int lod)
{
    if (!m_geoms.empty())
        return m_geoms[geom].lodDistances[lod];
    return -1.0f;
}


// The base's +0x80 map<unsigned, float>, keyed (geom << 16) | (lod << 8) | material. Only the LOW
// byte of geom and lod reaches the key - the shipped body loads each through a byte register.
// find (sub_10049650), then insert (sub_100EB890) on a miss or operator[] (sub_100EC4A0) on a hit.

namespace {

unsigned int materialValueKey(int geom, int lod, unsigned char material)
{
    const unsigned short pair =
        static_cast<unsigned short>((static_cast<unsigned char>(geom) << 8) |
                                     static_cast<unsigned char>(lod));
    return static_cast<unsigned int>(material) | (static_cast<unsigned int>(pair) << 8);
}

} // anonymous namespace


void StaticMeshTemplate::setMaterialValue(int geom, int lod, unsigned char material, float value)
{
    const unsigned int key = materialValueKey(geom, lod, material);

    if (m_materialValues.find(key) == m_materialValues.end())
        m_materialValues.insert(MaterialValueMap::value_type(key, value));
    else
        m_materialValues[key] = value;
}


float StaticMeshTemplate::getMaterialValue(int geom, int lod, unsigned char material)
{
    const unsigned int key = materialValueKey(geom, lod, material);

    if (m_materialValues.find(key) == m_materialValues.end())
        return 15.0f;
    return m_materialValues[key];
}


// The flag accessors. Each is one or two instructions in the image; the addresses are on the line.

bool StaticMeshTemplate::isWater() const                       { return m_isWater != 0; }
void StaticMeshTemplate::setDoNotGenerateLightmaps(bool value) { m_doNotGenerateLightmaps = value ? 1 : 0; }
bool StaticMeshTemplate::getDoNotGenerateLightmaps() const     { return m_doNotGenerateLightmaps != 0; }
void StaticMeshTemplate::setLightmapOverride(bool value)       { m_lightmapOverride = value ? 1 : 0; }
bool StaticMeshTemplate::getLightmapOverride() const           { return m_lightmapOverride != 0; }
void StaticMeshTemplate::setCompressVertexData(bool value)     { m_compressVertexData = value ? 1 : 0; }
bool StaticMeshTemplate::getCompressVertexData() const         { return m_compressVertexData != 0; }
void StaticMeshTemplate::setMaxTextureRepeat(int value)        { m_maxTextureRepeat = value; }
int  StaticMeshTemplate::getMaxTextureRepeat() const           { return m_maxTextureRepeat; }

// The write only happens with the lightmap override set, and when it is clear the
// shipped body returns an uninitialised eax - harmless here because the slot is void.
void StaticMeshTemplate::setLightmapSize(int a, int b, int size)
{
    if (m_lightmapOverride)
        m_lightmapSizes[b + 8 * a] = size;
}

// no range check, no override test.
int StaticMeshTemplate::getLightmapSize(int a, int b)
{
    return m_lightmapSizes[8 * a + b];
}


// getMaterialCount - PE 0x10115DE0. Sums the material count of `lod` across every geom.

int StaticMeshTemplate::getMaterialCount(unsigned int lod)
{
    int total = 0;
    unsigned int geom = 0;
    for (;;)
    {
        if (geom >= m_geoms.size())
            return total;

        if (lod < m_geoms[geom].lods.size())
        {
            StaticMeshMaterialList& materials = m_materials.data()[geom].data()[lod];
            if (materials.data())
                total += static_cast<int>(materials.size());
        }
        ++geom;
    }
}


// getLodPrimitiveCount - PE 0x10115EB0. The same walk, summing each material's own
// getPrimitiveCount (StaticMeshMaterial vtable +0x10).

int StaticMeshTemplate::getLodPrimitiveCount(unsigned int lod)
{
    int total = 0;
    unsigned int geom = 0;
    for (;;)
    {
        if (geom >= m_geoms.size())
            return total;

        if (lod < m_geoms[geom].lods.size())
        {
            StaticMeshMaterialList& materials = m_materials.data()[geom].data()[lod];
            unsigned int index = 0;
            for (;;)
            {
                if (!materials.data())
                    break;
                if (index >= static_cast<unsigned int>(materials.size()))
                    break;
                StaticMeshMaterial* material = materials.data() + index;
                total += material->getPrimitiveCount();
                ++index;
            }
        }
        ++geom;
    }
}


// decompressPosition - PE 0x10115A70. Multiply by the compress scale, truncate TO INT, multiply by
// the decompress scale. The y and z products are computed before x is stored; that order is kept
// because the (int) truncation makes it observable.

Vec3* StaticMeshTemplate::decompressPosition(Vec3* out, const Vec3* in)
{
    const float y = static_cast<float>(static_cast<int>(m_compressScale.y * in->y));
    const float z = static_cast<float>(static_cast<int>(m_compressScale.z * in->z));
    const float yOut = y * m_decompressScale.y;
    const float zOut = z * m_decompressScale.z;
    out->x = static_cast<float>(static_cast<int>(m_compressScale.x * in->x)) * m_decompressScale.x;
    out->y = yOut;
    out->z = zOut;
    return out;
}


// Slot +0xAC - PE 0x10118170. See the file header: this is a preload pass, not a release.

// The once-initialised static vector at unk_1025B8B8 collects the distinct textures of the shader
// records of geom 0, lod 0 only (the shipped walk never advances past the first lod of the first
// geom) and preloads each. The template's +0x08 is then stamped with the renderer's frame ordinal.

void StaticMeshTemplate::releaseDeviceResources()
{
    // unk_1025B8B8, constructed on first use behind dword_1025B8C8 bit 0 and torn down at exit
    // (atexit sub_101D95D0) - a function-local static.
    static std::vector<void*> s_preloadTextures;

    s_preloadTextures.clear();                                 // sub_100EBAD0(0, 0) - resize(0)

    if (m_vertexStream)
        resourcePreload(m_vertexStream);
    for (int i = 0; i < 5; ++i)
        if (m_uvStreams[i])
            resourcePreload(m_uvStreams[i]);
    if (m_tangentStream)
        resourcePreload(m_tangentStream);

    // No null test, exactly as shipped - see resourcePreload's note.
    resourcePreload(m_indexBuffer);

    if (!m_materials.empty())
    {
        StaticMeshMaterialList* lodVectors = m_materials.data()[0].data();
        StaticMeshMaterial* const first = lodVectors[0].data();
        StaticMeshMaterial* const last  = lodVectors[0].data() + lodVectors[0].size();
        for (StaticMeshMaterial* material = first; material != last; ++material)
        {
            void** textures = material->m_shaderMaterial->textures;      // +0x28, eight slots
            for (int slot = 0; slot < 8; ++slot)
            {
                void* texture = textures[slot];
                if (!texture)
                    continue;

                bool seen = false;
                for (std::vector<void*>::const_iterator q = s_preloadTextures.begin();
                     q != s_preloadTextures.end(); ++q)
                    if (*q == texture)
                    {
                        seen = true;
                        break;
                    }
                if (seen)
                    continue;

                s_preloadTextures.push_back(texture);
            }
        }
    }

    for (std::vector<void*>::const_iterator q = s_preloadTextures.begin();
         q != s_preloadTextures.end(); ++q)
        resourcePreload(*q);

    m_dword08 = static_cast<int>(::dice::hfe::dx9rend::rendererFrameOrdinal());   // +0x08
}

// the occluder-group accessors.
int StaticMeshTemplate::getOccluderGroupCount()
{
    return static_cast<int>(m_occluderGroups.size());
}

void* StaticMeshTemplate::getOccluderGroup(unsigned int index)
{
    if (!m_occluderGroups.empty() && index < m_occluderGroups.size())
        return &m_occluderGroups[index];
    return 0;
}

// load stock .occ groups into m_occluderGroups.
// The header token is ignored; corner indices are read as int then truncated to byte,
// and vertex x is negated. Record vectors transfer ownership; scratch storage is freed.
// Preserve the missing allocation/range guards and VC7.1 vector allocator words.

namespace {

// [CHOSEN, not recovered] Scanner matching the compatibility stream's extraction rules.
// EOF is set even when the final token was extracted; failed numeric reads leave the
// destination unchanged. The backing buffer includes a private NUL terminator.
class OccScanner
{
public:
    OccScanner(const char* buffer, unsigned int size)
        : m_p(buffer), m_end(buffer + size), m_eof(false) {}

    bool eof() const { return m_eof; }

    // std::operator>>(istream&, string&). The token is returned as a range into the buffer: both
    // uses here either compare it or throw it away, so nothing is copied and there is no length
    // limit to get wrong.
    bool token(const char** begin, const char** end)
    {
        skipSpace();
        if (m_p >= m_end)
        {
            m_eof = true;                  // eofbit, and failbit - nothing was extracted
            return false;
        }
        *begin = m_p;
        while (m_p < m_end && !isSpace(*m_p))
            ++m_p;
        *end = m_p;
        if (m_p >= m_end)
            m_eof = true;                  // eofbit even though a token was extracted
        return true;
    }

    bool tokenIs(const char* literal)
    {
        const char* begin = 0;
        const char* end   = 0;
        if (!token(&begin, &end))
            return false;
        const unsigned int n = static_cast<unsigned int>(end - begin);
        return ::strlen(literal) == n && ::memcmp(begin, literal, n) == 0;
    }

    // operator>>(int&) / operator>>(float&): on failure the destination is left alone.
    bool readInt(int* out)
    {
        skipSpace();
        if (m_p >= m_end)
        {
            m_eof = true;
            return false;
        }
        char* stop = 0;
        const long value = ::strtol(m_p, &stop, 10);
        if (stop == m_p)
            return false;
        m_p = stop;
        *out = static_cast<int>(value);
        if (m_p >= m_end)
            m_eof = true;
        return true;
    }

    bool readFloat(float* out)
    {
        skipSpace();
        if (m_p >= m_end)
        {
            m_eof = true;
            return false;
        }
        char* stop = 0;
        const double value = ::strtod(m_p, &stop);
        if (stop == m_p)
            return false;
        m_p = stop;
        *out = static_cast<float>(value);
        if (m_p >= m_end)
            m_eof = true;
        return true;
    }

private:
    // The "C" locale's space|blank set, written out so no locale or signed-char question arises.
    static bool isSpace(char c)
    {
        return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
    }

    void skipSpace()
    {
        while (m_p < m_end && isSpace(*m_p))
            ++m_p;
    }

    const char* m_p;
    const char* m_end;
    bool        m_eof;
};

// sub_100777A0 - Vec3 normalize in place, with the shipped |v|^2 within-epsilon short circuits.
// The same three-line body StaticMesh.cpp and StaticMeshMaterial.cpp each carry at their own file
// scope; repeated here for the same reason - all of them are file-local in the image.
void occNormalizeInPlace(float* v)
{
    const float lengthSquared = v[0] * v[0] + v[1] * v[1] + v[2] * v[2];
    if (::fabs(static_cast<double>(lengthSquared) - 1.0) < 0.00000011920929)
        return;
    if (::fabs(static_cast<double>(lengthSquared)) < 0.00000011920929)
    {
        v[0] = 0.0f; v[1] = 0.0f; v[2] = 0.0f;
        return;
    }
    const float inverse = static_cast<float>(1.0 / ::sqrt(static_cast<double>(lengthSquared)));
    v[0] *= inverse; v[1] *= inverse; v[2] *= inverse;
}


// Finds or creates an edge and appends its reference to the plane.
// Reversed orientation matches; same orientation logs an error and keeps searching.
// The stored index is narrowed to a byte, with a separate new-edge flag.

void addOccluderEdge(StaticMeshOccluderGroup& group, StaticMeshOccluderPlane& plane,
                     unsigned char a, unsigned char b)
{
    std::vector<StaticMeshOccluderEdge>& edges = group.edges;
    std::vector<unsigned short>& refs = plane.edgeReferences;

    const unsigned int count = static_cast<unsigned int>(edges.size());

    for (unsigned int i = 0; i < count; ++i)
    {
        const StaticMeshOccluderEdge& edge = edges[i];

        if (edge.points[0] == a && edge.points[1] == b)
        {
            // StaticMeshTemplate.cpp:310, Debug type 6 - and it does NOT stop.
            ::renddx9abi::logf("StaticMeshTemplate.cpp:310: EDGE ERROR");
        }

        if (edge.points[0] == b && edge.points[1] == a)
        {
            refs.push_back(static_cast<unsigned short>(static_cast<unsigned char>(i)));
            return;
        }
    }

    // Not there. The index is taken before the push, so it is the slot the new edge lands in.
    const unsigned char newIndex = static_cast<unsigned char>(count);

    StaticMeshOccluderEdge edge;
    edge.points[0] = a;
    edge.points[1] = b;
    // warning: the shipped element's upper two bytes are the caller's register leftovers - the
    // record is pushed straight out of the argument dword before BYTE2/BYTE3 are rewritten.
    // The loader never reads them and ObjectDrawer rewrites them before reading, so they are
    // zeroed here rather than copied indeterminate.
    edge.marks[0] = 0;
    edge.marks[1] = 0;
    edges.push_back(edge);

    refs.push_back(static_cast<unsigned short>(static_cast<unsigned short>(newIndex) | 0x0100u));
}

} // namespace

void StaticMeshTemplate::loadOccluders(::renddx9abi::HostStream* stream)
{
    // 0x1011A0DB -> the stream's own +0x1C, operator new, then the whole-file read.
    const unsigned int size = static_cast<unsigned int>(stream->getAvailableBytes());   // +0x1C
    char* const buffer = static_cast<char*>(::renddx9abi::engineAlloc(size + 1u));
    stream->read(buffer, size);                                                         // +0x10
    buffer[size] = '\0';

    OccScanner scanner(buffer, size);

    // The version line, read and dropped. warning: its state is NOT tested (0x1011A179 is
    // followed straight by the second string's constructor) - only the marker read below is.
    const char* tokenBegin = 0;
    const char* tokenEnd   = 0;
    scanner.token(&tokenBegin, &tokenEnd);

    // EOF is checked before matching, so a final token without trailing whitespace is skipped.
    bool isGroup = scanner.tokenIs("GROUP");

    int corner = 0;        // var_68: NOT pre-zeroed per read by the shipped body; it persists

    while (!scanner.eof() && isGroup)
    {
        // The group record, built on the stack exactly as the shipped body builds it
        // (0x1011A1EB..0x1011A203), then moved into the template below.
        StaticMeshOccluderGroup group;
        std::vector<StaticMeshOccluderPlane>& planes = group.planes;
        std::vector<Vec3>&                    points = group.points;
        std::vector<StaticMeshOccluderEdge>&  edges  = group.edges;

        // the quad corner indices, into a scratch byte vector (0x1011A230..0x1011A28E)
        // This fourth vector is not part of the record; it is freed at the end of the group.
        std::vector<unsigned char> cornerBytes;

        int quadCount = 0;
        scanner.readInt(&quadCount);

        for (int k = 0; k < 4 * quadCount; ++k)
        {
            scanner.readInt(&corner);
            cornerBytes.push_back(static_cast<unsigned char>(corner));
        }

        // the vertices (0x1011A290..0x1011A2FE)
        int vertexCount = 0;
        scanner.readInt(&vertexCount);

        for (int v = 0; v < vertexCount; ++v)
        {
            float position[3];
            position[0] = 0.0f;                 // pre-zeroed every iteration, as shipped
            position[1] = 0.0f;
            position[2] = 0.0f;
            scanner.readFloat(&position[0]);
            scanner.readFloat(&position[1]);
            scanner.readFloat(&position[2]);
            position[0] = -position[0];         // X, and only X
            const Vec3 point = { position[0], position[1], position[2] };
            points.push_back(point);
        }

        // one plane per quad, four edges per plane (0x1011A314..0x1011A3D1)
        for (int quad = 0; quad < quadCount; ++quad)
        {
            // The shipped template element zeroes the normal and the nested vector and leaves `d`
            // alone (0x1011A314..0x1011A332). `d` is overwritten by the plane pass below for every
            // element, so zeroing it too changes nothing observable.
            StaticMeshOccluderPlane blank;
            blank.plane[0] = 0.0f;
            blank.plane[1] = 0.0f;
            blank.plane[2] = 0.0f;
            blank.plane[3] = 0.0f;
            planes.push_back(blank);

            for (int c = 0; c < 4; ++c)
            {
                const int next = (c == 3) ? 0 : c + 1;
                const unsigned char* const quadCorners = cornerBytes.data() + 4 * quad;

                // Recomputed per corner, as shipped: the element just added.
                StaticMeshOccluderPlane& plane = planes.back();

                addOccluderEdge(group, plane, quadCorners[c], quadCorners[next]);
            }
        }

        // Edge 0 and edge 1 intentionally use opposite direction-flag conventions.
        // The plane anchor always uses edge 0's v0, independently of that flag. Each reference is
        // the edge index in its low byte and the created-here flag in its high byte.
        if (!planes.empty())
        {
            const unsigned int planeCount = static_cast<unsigned int>(planes.size());

            for (unsigned int p = 0; p < planeCount; ++p)
            {
                StaticMeshOccluderPlane& plane = planes[p];
                const unsigned short* const ref = plane.edgeReferences.data();

                // edge 0 -> the first tangent, and the anchor
                const StaticMeshOccluderEdge& edge0 = edges[ref[0] & 0xFFu];
                const Vec3* firstA;
                const Vec3* secondA;
                if ((ref[0] >> 8) != 0)
                {
                    firstA  = &points[edge0.points[0]];
                    secondA = &points[edge0.points[1]];
                }
                else
                {
                    firstA  = &points[edge0.points[1]];
                    secondA = &points[edge0.points[0]];
                }
                const float ax = firstA->x - secondA->x;
                const float ay = firstA->y - secondA->y;
                const float az = firstA->z - secondA->z;
                const Vec3& anchorPoint = points[edge0.points[0]];
                const float anchor[3] = { anchorPoint.x, anchorPoint.y, anchorPoint.z };

                // edge 1 -> the second tangent
                const StaticMeshOccluderEdge& edge1 = edges[ref[1] & 0xFFu];
                const Vec3* firstB;
                const Vec3* secondB;
                if ((ref[1] >> 8) == 0)
                {
                    firstB  = &points[edge1.points[0]];
                    secondB = &points[edge1.points[1]];
                }
                else
                {
                    firstB  = &points[edge1.points[1]];
                    secondB = &points[edge1.points[0]];
                }
                const float bx = firstB->x - secondB->x;
                const float by = firstB->y - secondB->y;
                const float bz = firstB->z - secondB->z;

                // cross(a, b), written in the shipped operand order so the rounding matches:
                // 0x1011A50x emits y first (v51), then x, then z.
                float normal[3];
                normal[1] = az * bx - bz * ax;
                normal[0] = bz * ay - by * az;
                normal[2] = by * ax - ay * bx;

                occNormalizeInPlace(normal);

                float* const out = plane.plane;
                out[0] = normal[0];
                out[1] = normal[1];
                out[2] = normal[2];
                // The shipped grouping, z and y first: -((nz*p.z + ny*p.y) + nx*p.x).
                out[3] = -((normal[2] * anchor[2] + normal[1] * anchor[1]) + normal[0] * anchor[0]);
            }
        }

        // the record goes in, and only then do the buffers get released
        // Ownership of the three record vectors moves here. The scratch corner bytes are not part
        // of the record; they are freed with the iteration's locals.
        m_occluderGroups.push_back(std::move(group));

        // the next marker (0x1011A5FE..0x1011A63D)
        isGroup = scanner.tokenIs("GROUP");
    }

    // the file buffer, and then the stream objects unwind.
    ::renddx9abi::engineFree(buffer);
}


// Reads each geometry's LODs, overwriting skipped records before retaining later LODs.
// Header zero with version <10 uses legacy bounds; other files use min/max bounds.

void StaticMeshTemplate::readLods(::renddx9abi::HostNonlinearStream* stream)
{
    const unsigned int geomCount = static_cast<unsigned int>(m_lodCounts.size());

    m_geoms.resize(geomCount);                                     // sub_1011F870
    if (!geomCount)
        return;

    for (unsigned int geom = 0; geom < geomCount; ++geom)
    {
        // The renderer's skip budget: (rendererLodSkipBias + this->+0x2B8) clamped at zero, plus
        // the flat base, then capped by the template's own per-role maximum.
        int budget = rendererLodSkipBias() + m_dword2B8;
        if (budget <= 0)
            budget = 0;
        budget += rendererLodSkipBase();

        int cap = 0;
        if (geom == 0)
            cap = m_maxSkip3pLods;                                // +0xC0
        else if (geom == 1)
            cap = m_maxSkipWreckLods;                             // +0xC4

        const int lodCount = m_lodCounts[geom];
        if (cap < budget)
            budget = cap;

        int skip = lodCount - 1;
        if (skip >= budget)
            skip = budget;

        m_geomInts[geom] = skip;                                   // +0x70 remembers the skip
        const int kept = lodCount - skip;
        m_lodCounts[geom] = kept;

        StaticMeshGeom& g = m_geoms[geom];
        g.lods.resize(static_cast<unsigned int>(kept), staticMeshLodFill());       // sub_1011D380
        g.lodDistances.resize(static_cast<unsigned int>(kept - 1), 0.0f);         // sub_100EC600

        int remainingSkips = skip;
        int lodIndex = 0;
        if (kept > 0)
        {
            do
            {
                StaticMeshLod* lod = g.lods.data() + lodIndex;
                ::memset(lod, 0, sizeof(StaticMeshLod));

                if (m_head || m_version >= 10u)
                    meshBoundsReadMinMax(lod->bounds, stream);
                else
                    meshBoundsReadLegacy(lod->bounds, stream);

                meshBoundsExpand(m_bounds, lod->bounds.min);       // sub_100796A0, twice
                meshBoundsExpand(m_bounds, lod->bounds.max);

                stream->read(&lod->nodeCount, 4);                  // +0x10
                stream->seek(static_cast<::renddx9abi::HostSeekOrigin>(1),
                             static_cast<int>(lod->nodeCount << 6));   // +0x30

                if (remainingSkips > 0)
                {
                    --remainingSkips;
                    --lodIndex;
                }
                ++lodIndex;
            }
            while (lodIndex < kept);
        }

        // the lod switch distances
        // The accumulator starts at (skip + 1) * 25. Where the base's +0x60 vector has an entry it
        // is copied across THE skip (source index lod + skip) - but the accumulator is advanced by
        // the entry at index lod. That asymmetry is in the instructions (0x101198ED stores from
        // [ebx+eax] with ebx = 4*(lod+skip), 0x10119900 adds [edx+ecx*4] with ecx = lod); it is
        // preserved rather than tidied.
        const int skipped = m_geomInts[geom];
        const int distanceCount = kept - 1;
        float accumulator = static_cast<float>((skipped + 1) * 25);

        for (int lod = 0; lod < distanceCount; ++lod)
        {
            float* destination = g.lodDistances.data();

            bool haveStored = false;
            if (m_lodDistances.size() > geom)
            {
                const std::vector<float>& inner = m_lodDistances[geom];
                if (static_cast<int>(inner.size()) > lod)
                {
                    const float* stored = inner.data();
                    destination[lod] = stored[lod + skipped];
                    accumulator += stored[lod];
                    haveStored = true;
                }
            }

            if (!haveStored)
            {
                destination[lod] = accumulator;
                accumulator += 25.0f;
            }
        }
    }
}


// Builds techniques and reads materials with mip class 10, compacting kept LOD data.
// Versions below 11 compute bounds from vertices; newer versions read min/max bounds.

void StaticMeshTemplate::readMaterials(::renddx9abi::HostNonlinearStream* stream)
{
    const int savedMipClass = ::dice::hfe::dx9rend::textureManagerDefaultMipClass();
    ::dice::hfe::dx9rend::textureManagerSwapDefaultMipClass(10);

    staticMeshInitTechniques();

    const unsigned int geomCount = static_cast<unsigned int>(m_lodCounts.size());

    m_materials.resize(geomCount);

    unsigned int droppedVertices = 0;
    unsigned int droppedIndices  = 0;

    for (unsigned int geom = 0; geom < geomCount; ++geom)
    {
        const int lodCount = m_lodCounts[geom];
        int remainingSkips = m_geomInts[geom];

        StaticMeshLodMaterialLists& geomMaterials = m_materials.data()[geom];
        geomMaterials.resize(static_cast<unsigned int>(lodCount));
        if (lodCount <= 0)
            continue;

        StaticMeshGeom& g = m_geoms[geom];

        int lodIndex = 0;               // the destination lod

        for (;;)
        {
            StaticMeshLod* lod = g.lods.data() + lodIndex;
            StaticMeshMaterialList& lodMaterials = geomMaterials.data()[lodIndex];

            unsigned int materialCount = 0;
            stream->read(&materialCount, 4);                       // +0x10

            // The shipped call builds a default StaticMeshMaterial on the stack and passes it by
            // value; resize default-constructs each new material.
            lodMaterials.resize(materialCount);

            m_skippingLods = static_cast<unsigned char>(remainingSkips > 0);
            lod->primitiveCount = 0;
            lod->vertexCount    = 0;
            lod->hasAlphaMaterial = 0;
            lod->byte5C = 0;
            m_dword0F8 = 3;

            unsigned int materialIndex = 0;
            while (materialCount)
            {
                StaticMeshMaterial* material =
                    lodMaterials.data() + materialIndex;

                stream->read(&material->m_alphaMode, 4);           // +0x17C
                if (material->m_alphaMode == 1)
                    m_dword0F8 = 2;
                else if (material->m_alphaMode == 2)
                    m_hasAlphaMaterial = 1;

                unsigned int nameLength = 0;
                stream->read(&nameLength, 4);
                stream->skip(nameLength);                          // +0x18 - the fx name is skipped

                ShaderMaterial* shader =
                    staticMeshStateManagerCreateMaterial(m_path, stream, m_version, this);
                material->m_shaderMaterial = shader;

                if (!m_skippingLods)
                {
                    const unsigned int comboFlags = g_shaderComboFlags[shader->shaderCombo];
                    const unsigned int qualityMask = staticMeshQualityMask();
                    m_shaderFlagsAll |= comboFlags;
                    const unsigned int used = qualityMask & comboFlags;
                    lod->needsAltStreams =
                        static_cast<unsigned char>((used & 0xC0u) != 0 || (used & 0x100u) != 0);
                    m_shaderFlagsUsed |= used;
                }

                stream->read(&material->m_vertexStart, 4);         // +0x180
                stream->read(&material->m_indexStart, 4);          // +0x184

                unsigned int indexCount = 0;
                stream->read(&indexCount, 4);
                if (m_primitiveType == 4)
                    material->m_primitiveCount = indexCount / 3;
                else if (m_primitiveType == 5)
                    material->m_primitiveCount = indexCount - 2;
                // any other primitive type leaves +0x188 as the resize left it

                stream->read(&material->m_vertexCount, 4);         // +0x18C
                stream->read(&material->m_u4, 4);                  // +0x190
                stream->read(&material->m_u5u6, 4);                // +0x194

                if (materialIndex == 0)
                {
                    lod->primitiveType   = m_primitiveType;
                    lod->primitiveCount += material->m_primitiveCount;
                    lod->vertexCount    += material->m_vertexCount;
                    lod->vertexStart     = material->m_vertexStart;
                    lod->indexStart      = material->m_indexStart;
                }

                if (material->m_alphaMode == 1)
                    lod->hasAlphaMaterial = 1;

                if (m_version < 11u)
                {
                    // 10000 / -10000 seeds, dirty set, then every index of this material expands
                    // the box. The vertex is selected by the LOW word of (vertexStart + index).
                    material->m_bounds.min.x = 10000.0f;
                    material->m_bounds.min.y = 10000.0f;
                    material->m_bounds.min.z = 10000.0f;
                    material->m_bounds.max.x = -10000.0f;
                    material->m_bounds.max.y = -10000.0f;
                    material->m_bounds.max.z = -10000.0f;
                    material->m_bounds.dirty = 1;

                    for (unsigned int i = 0; i < indexCount; ++i)
                    {
                        const unsigned short vertex = static_cast<unsigned short>(
                            static_cast<unsigned short>(material->m_vertexStart) +
                            m_indexData[i + material->m_indexStart]);
                        const Vec3* position = reinterpret_cast<const Vec3*>(
                            static_cast<char*>(m_vertexData) + m_vertexStride * vertex);
                        meshBoundsExpand(material->m_bounds, *position);
                    }
                }
                else
                {
                    meshBoundsReadMinMax(material->m_bounds, stream);
                }

                if (remainingSkips <= 0)
                {
                    // A kept lod: slide its data down over whatever the skipped lods left behind.
                    if (droppedIndices)
                    {
                        const unsigned int oldStart = material->m_indexStart;
                        material->m_indexStart = oldStart - droppedIndices;
                        ::memmove(m_indexData + material->m_indexStart,
                                  m_indexData + oldStart,
                                  2u * indexCount);
                    }
                    if (droppedVertices)
                    {
                        const unsigned int oldStart = material->m_vertexStart;
                        const unsigned int count    = material->m_vertexCount;
                        material->m_vertexStart = oldStart - droppedVertices;
                        ::memmove(static_cast<char*>(m_vertexData) +
                                      m_vertexStride * material->m_vertexStart,
                                  static_cast<char*>(m_vertexData) + m_vertexStride * oldStart,
                                  m_vertexStride * count);
                    }
                }
                else
                {
                    // A skipped lod: drop the shader reference and count the data away.
                    if (material->m_shaderMaterial)
                        reinterpret_cast<IBase*>(material->m_shaderMaterial)->release();
                    droppedVertices += material->m_vertexCount;
                    droppedIndices  += indexCount;
                }

                if (++materialIndex >= materialCount)
                    break;
            }

            if (remainingSkips > 0)
            {
                --remainingSkips;
                --lodIndex;
            }
            if (++lodIndex >= lodCount)
                break;
        }
    }

    m_indexCount  -= droppedIndices;
    m_vertexCount -= droppedVertices;
    m_finished = 1;

    ::dice::hfe::dx9rend::textureManagerSwapDefaultMipClass(savedMipClass);
}


// [FRONTIER] Serializes mesh-template overrides as console commands.
// Remains a stopping boundary until output ordering is implemented and validated.

bool StaticMeshTemplate::makeScript(::dice::hfe::io::IStream* stream)
{
    (void)stream;
    RD9_FRONTIER("dice::hfe::geom::StaticMeshTemplate::makeScript (0x10116140, the editor .con "
                 "writer; its ostream insertion order is not legibly recovered)", 0x00);
    return false;
}


// sub_10191CE0, the feature names of every set bit, concatenated in bit order.
// The shipped loop tests each of the 32 bit positions and, for a set bit,
// recomputes its index by scanning up from zero - which is that same position - before reading the
// name table.  Reproduced as written.  A bit above the twelve named ones reads past the table's
// terminator in the shipped body too; no recovered caller passes one.

void staticMeshShaderBitsToName(std::string& out, unsigned int bits)
{
    std::string names("");
    for (unsigned int i = 0; i < 32u; ++i)
    {
        const unsigned int bit = bits & (1u << i);
        if (!bit)
            continue;
        unsigned int index = 0;
        if ((bit & 1u) == 0)
        {
            do
                ++index;
            while (((bit >> index) & 1u) == 0);
        }
        names += kStaticMeshShaderBitNames[index];
    }
    out = names;
}


// Returns the shader for bits, falling back to the default mask after logging a miss.
// Returns null and logs if the default is absent.

::dice::hfe::dx9rend::Ra::RaShader* staticMeshFindShaderBits(unsigned int bits)
{
    StaticMeshTechniqueMap::const_iterator found = g_staticMeshTechniques.find(bits);
    if (found != g_staticMeshTechniques.end())
        return found->second;

    {
        std::string names("");
        staticMeshShaderBitsToName(names, bits);
        ::renddx9abi::logf("StaticMeshTemplate.cpp:2076 Couldn't find shader bits: %u looked up "
                           "name: %s", bits, names.c_str());
    }

    found = g_staticMeshTechniques.find(0x1000000u);
    if (found != g_staticMeshTechniques.end())
        return found->second;

    ::renddx9abi::logf("StaticMeshTemplate.cpp:2084 Couldn't find shader default");
    return 0;
}


// Shared technique-mask builder used by the bundled and skinned mesh templates.
// [CHOSEN, not recovered] - plumbing only.


bool meshBuildTechniqueMasks(const std::string& mfxPath,
                             const std::string& techniquePrefix,
                             const char* const* bitNames,
                             std::map<unsigned int, ::dice::hfe::dx9rend::Ra::RaShader*>& techniqueMap,
                             std::vector<unsigned int>& maskVector)
{
    return staticMeshBuildTechniqueMasks(mfxPath, techniquePrefix, bitNames,
                                         techniqueMap, maskVector);
}

} // namespace geom

namespace dx9rend {
// BufferManager.cpp - declared here the way MeshTemplate.cpp declares the same pair.
void* indexBufferLock(void* wrapper, unsigned int offsetIndices, unsigned int countIndices,
                      unsigned int flags);
bool  indexBufferUnlock(void* wrapper);
}

namespace geom {


// Merges triangle-list index ranges once per template when each LOD fits 16-bit indices.
// Rebases onto the first material for a single shadow draw. Whole-buffer unlock still runs
// after a failed lock; index arithmetic intentionally remains 16-bit.

void StaticMeshTemplate::mergeShadowIndexRanges()
{
    if (m_byte190)
        return;

    if (m_primitiveType == 4u)
    {
        const unsigned int geomCount = static_cast<unsigned int>(m_lodCounts.size());
        for (unsigned int g = 0; g < geomCount; ++g)
        {
            const int lodCount = m_lodCounts[g];
            if (!lodCount)
                continue;

            const StaticMeshMaterialList* const lodRecords = m_materials.data()[g].data();
            StaticMeshLod* const lods = m_geoms[g].lods.data();

            for (int l = 0; l < lodCount; ++l)
            {
                const StaticMeshMaterialList& record = lodRecords[l];
                StaticMeshLod& lod = lods[l];
                if (lod.byte5C)
                    continue;

                StaticMeshMaterial* const first = const_cast<StaticMeshMaterial*>(record.data());
                if (!first)
                    continue;
                const unsigned int materialCount = static_cast<unsigned int>(record.size());
                if (!materialCount)
                    continue;

                bool fits = true;
                for (unsigned int m = 0; m < materialCount; ++m)
                {
                    const StaticMeshMaterial& material = first[m];
                    // `jle` - a signed compare of the 32-bit sum, as shipped.
                    if (static_cast<int>(material.m_vertexCount - first->m_vertexStart
                                         + material.m_vertexStart) > 0xFFFF)
                    {
                        fits = false;
                        break;
                    }
                }
                if (!fits)
                    continue;

                lod.mergedPrimitiveCount = 0;
                lod.mergedVertexCount    = 0;
                lod.vertexStart          = first->m_vertexStart;

                unsigned short* const indices = static_cast<unsigned short*>(
                    dx9rend::indexBufferLock(m_indexBuffer, 0, 0, 0));
                if (indices)
                {
                    for (unsigned int m = 0; m < materialCount; ++m)
                    {
                        StaticMeshMaterial& material = first[m];
                        lod.mergedPrimitiveCount += material.m_primitiveCount;
                        lod.mergedVertexCount    += material.m_vertexCount;
                        const int indexCount = 3 * static_cast<int>(material.m_primitiveCount);
                        const unsigned short delta = static_cast<unsigned short>(
                            static_cast<unsigned short>(material.m_vertexStart)
                            - static_cast<unsigned short>(lod.vertexStart));
                        for (int k = 0; k < indexCount; ++k)
                            indices[material.m_indexStart + k] =
                                static_cast<unsigned short>(indices[material.m_indexStart + k] + delta);
                        material.m_vertexStart = lod.vertexStart;
                    }
                }
                lod.byte5C = 1;
                dx9rend::indexBufferUnlock(m_indexBuffer);
            }
        }
    }

    m_byte190 = 1;
}

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
