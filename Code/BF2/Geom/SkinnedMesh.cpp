// Skinned geometry instances own bone palettes and per-material draw state.
// The flat compatibility object publishes only its final vtable during construction
// and destruction; there are no C++ base constructors requiring intermediate tables.


#ifdef BF2_IMAGE_RENDDX9

#include "SkinnedMesh.h"

#include "../../RendDX9Abi/BfMemory.h"        // engineAlloc - the 104 bone matrices
#include "../../RendDX9Abi/Frontier.h"
#include "../../RendDX9Abi/Vc71Vector.h"
#include "../../RendDX9Abi/Vc71Map.h"     // the renderer queues' multimap insert
#include "../Core/Ids.h"                      // IID_IBase / IID_IGeometry / IID_IRenderable
#include "../Game/Objects/Visual/LightSource.h"  // the gathered lights
#include "../Math/Vec3.h"
#include "../RendDX9/Ra/RaShader.h"           // raObjectParametersConstruct
#include "../RendDX9/RendDX9Base.h"           // rendererFrameOrdinal, the render view
#include "../../RendDX9Abi/D3dx9Sdk.h"        // ID3DXEffect - the shadow draw's commit
#include "XpLightManager.h"                   // g_LightManager - the shadow lod
#include "MeshTemplate.h"                     // MeshBounds / meshBoundsFinalize
#include "SkinnedMeshTemplate.h"
#include "StaticMesh.h"                       // staticMeshRootParent - sub_10103CE0, shared
// Host interfaces preserve virtual order. Field and container access selects the host ABI;
// only the common IObject layout prefix is shared directly.
#if !BF2_PLUGIN_ABI_NATIVE
#define CORE_CLASSMANAGER_H
#endif
#include "../Game/Objects/Visual/LightSourceTemplate.h"   // getIsDynamic - the lit path's +0xF0
#include "../Game/Objects/IPlayer.h"                      // getVehicle / getCamera / hasFirstPersonView
#include "../Game/Objects/ICameraObject.h"                // getAllowDynamicLights
#include "../Game/Objects/IObjectTemplate.h"              // getClassID / getFlags
#include "../Game/Objects/ObjectLayoutAccess.h"           // IObject's retail-aligned prefix
#include "../Scene/Object/PlayerManager.h"                // getLocalHumanPlayer
#include "../Scene/Object/ObjectTypes/Game/IPlayerControlObject.h"   // the first-person pair
#if !BF2_PLUGIN_ABI_NATIVE
#undef CORE_CLASSMANAGER_H
#endif

#include <cmath>
#include <map>
#include <cstring>

namespace dice {
namespace hfe {

// BF2Render.cpp declares these engine singletons at dice::hfe scope; matched verbatim.
extern IBase* playerManager;    // getLocalHumanPlayer at its +0x30
extern IBase* objectManager;    // the height probe's interface

namespace dx9rend {

// dword_1023B970. Declared per-TU exactly as StaticMesh.cpp, SkyDome.cpp and SwiffRender.cpp
// declare it, because RendDX9Base.h publishes the typed seams but not the base pointer.
extern void* g_rendererBase;
extern void* g_renderQueues;    // RenderQueues.cpp - dword_1023E408, the Ra shader table
extern void* g_shaderManager;   // ShaderManager.cpp  - dword_1023B97C

// declared per-TU, as QuadDrawer.cpp does; Effect.cpp publishes no header for it.
bool effectSetTechniqueByName(void* effect, const vc71::string& name);

// The Renderer console object - a different global from g_rendererBase, and declared in this same
// namespace by StaticMesh.cpp, StaticMeshStateManager.cpp and StaticMeshTemplate.cpp.
extern IBase* g_Renderer;

// The stream / draw seams both draws use.
void vertexBufferBindStream(void* wrapper, unsigned int stream);
void indexBufferBind(void* wrapper);
void vertexDeclarationBind(void* wrapper);
void rendererDrawIndexedPrimitive(unsigned int primCount, unsigned int type, int baseVertex,
                                  unsigned int minIndex, unsigned int numVertices,
                                  unsigned int startIndex);

} // namespace dx9rend

namespace geom {

extern LightManager* g_LightManager;      // XpLightManager.cpp, dword_1024B914
extern IBase*        g_SkinnedMeshRenderer;   // ModuleGeom.cpp, dword_1024B8F8

namespace {

// Per-geometry records contain paired vectors of 132-byte LODs and distance floats.
// The parallel companion table contains vectors of 16-byte records.
const vc71::vector_layout* geomRecord(const SkinnedMeshTemplate* t, unsigned int geom)
{
    return reinterpret_cast<const vc71::vector_layout*>(t->m_vector0DC.first + 32 * geom);
}

const vc71::vector_layout* companionRecord(const SkinnedMeshTemplate* t, unsigned int geom)
{
    return reinterpret_cast<const vc71::vector_layout*>(t->m_vector0CC.first + 16 * geom);
}

enum
{
    kLodRecordStride       = 132,   // the records inside a geom's nested vector
    kCompanionStride       =  16,   // the records inside the companion's nested vector
    kMaterialStride        = 456,   // what +0x58 counts out of m_lodMaterials
    kPartRecordStride      =  68,   // the records +0x58 gathers ids from
    kLodBoundsOffset       =  28,   // MeshBounds inside the 132-byte lod record: the radius
    kLodBoundsDirtyOffset  =  32,   // and its "needs finalising" byte
    kLodPartVectorOffset   =  68,   // first/last of the part vector, at +68 / +72
    kLightRecordStride     =  68    // the records the draw builds at +0x584
};


// Shared selection and matrix helpers.


// LightManager vtable +0x290 - the skinned-mesh shadow lod, the exact counterpart of the attested
// +0x280 (`getStaticMeshShadowLod`) that StaticMesh::drawShadowMap uses in the same position, the
// same way. PE 1015BE40 reads the manager's +15C field; the recovered typed method retains that
// dispatch through the primary table in both host ABI modes.
int skinnedMeshShadowLod()
{
    return g_LightManager->getSkinnedMeshShadowLod();
}

// Returns the shared zero matrix used to collapse suppressed body parts.
// The identity matrix is reserved for instances without a skeleton.
const Mat4& skinnedMeshZeroMatrix()
{
    static Mat4 zero;
    static bool built = false;
    if (!built)
    {
        std::memset(&zero, 0, sizeof(zero));   // four zeroed rows, as sub_100795B0 builds them
        built = true;
    }
    return zero;
}


// Raw draw-time vtable adapters. Names are [INFERRED]; slot offsets are fixed ABI.

template <class R>
R viewSlot(void* view, unsigned int slot)
{
    typedef R(__fastcall * Fn)(void*, void*);
    return reinterpret_cast<Fn>(
        (*reinterpret_cast<void***>(view))[slot / sizeof(void*)])(view, 0);
}

// The Renderer's two gate bytes, read at +0x6A3 and +0x6AB off the console object.
bool rendererFlagAt(unsigned int byteOffset)
{
    return *(reinterpret_cast<const unsigned char*>(dx9rend::g_Renderer) + byteOffset) != 0;
}

// SkinnedMeshRenderer +0x40 - a mode word the draw compares against 2 in three places.
// sub_10123A40, `mov eax, [ecx+40h]; retn`. [INFERRED] name.
int skinnedMeshRendererMode()
{
    return *reinterpret_cast<const int*>(
        reinterpret_cast<const unsigned char*>(g_SkinnedMeshRenderer) + 0x40);
}


// Submits by camera distance to the selected renderer map; the two draw paths use
// different maps. Compatibility nodes are 24 bytes.

const unsigned int kSkinnedMeshRendererNodeSize = 0x18u;   // SkinnedMeshRenderer.cpp's construct()

void skinnedMeshRendererSubmit(SkinnedMesh* mesh, unsigned int mapOffset)
{
    if (!g_SkinnedMeshRenderer || !mesh)
        return;
    vc71::map_layout* const queue = reinterpret_cast<vc71::map_layout*>(
        reinterpret_cast<unsigned char*>(g_SkinnedMeshRenderer) + mapOffset);
    queue->insertMultiFloat(mesh->m_distanceToCamera, mesh, kSkinnedMeshRendererNodeSize);
}

void skinnedMeshRendererSubmitA(SkinnedMesh* mesh)   // sub_10123A50, the renderer's +0x10 map
{
    skinnedMeshRendererSubmit(mesh, 0x10);
}

void skinnedMeshRendererSubmitB(SkinnedMesh* mesh)   // sub_10123A90, the renderer's +0x28 map
{
    skinnedMeshRendererSubmit(mesh, 0x28);
}

// the lit path's LightSource seams
// The light's own virtuals are typed (RW-266): getAbsolutePosition +0x68, getDirection +0x17C,
// getAttenuationRange2 +0x190 and getColor +0x198 - the retail PE slots, and the tree's slots 26,
// 95, 100 and 102 in both flavours (RW-264's native vtable dump).
world::LightSource* lightSourceOf(void* light)
{
    return static_cast<world::LightSource*>(light);
}

// Gets the light template through the native object or compatibility offset +0x144.
// Native LightSource has a different layout; never apply that offset to it.
const world::LightSourceTemplate* lightSourceTemplate(void* light)
{
#if BF2_PLUGIN_ABI_NATIVE
    return static_cast<const world::LightSourceTemplate*>(
        static_cast<world::LightSource*>(light)->getTemplate());
#else
    return *reinterpret_cast<const world::LightSourceTemplate* const*>(
        static_cast<unsigned char*>(light) + 0x144);
#endif
}

// A virtual, so typed in both flavours: the tree's slot 60 (+0xF0) is retail's getIsDynamic.
bool lightTemplateIsDynamic(const world::LightSourceTemplate* lightTemplate)
{
    return lightTemplate->getIsDynamic();
}

// Returns the local human player used to identify first-person geometry.
world::IPlayer* skinnedMeshLightOwner()
{
    if (!::dice::hfe::playerManager)
        return 0;
    return reinterpret_cast<world::PlayerManager*>(::dice::hfe::playerManager)
        ->getLocalHumanPlayer();                                            // +0x30
}

// Debug switch with no normal writer. A debugger can enable the diagnostic branch.
unsigned int g_skinnedMeshLightDebugMode = 0;

int skinnedMeshLightDebugMode()
{
    return static_cast<int>(g_skinnedMeshLightDebugMode);
}

// Unreachable while the switch above stays zero, which in the shipped image is always.
void skinnedMeshDebugLightList(SkinnedMesh* mesh, vc71::vector_layout* out)
{
    (void)out;
    RD9_FRONTIER_AT("dice::hfe::geom::skinnedMeshDebugLightList (PE sub_100EEED0, a multimap "
                    "equal_range walk keyed by the instance pointer; reachable only if something "
                    "sets ds:10264C48, which nothing in the shipped image does)", mesh, 0);
}


// Returns the owner's borrowed template. A live owner must have a template.
// Typed template calls preserve the platform-specific destructor-slot layout.

void* objectTemplateOf(void* object)
{
    return const_cast<world::IObjectTemplate*>(static_cast<world::IObject*>(object)->getTemplate());
}

unsigned int objectTemplateGetClassID(void* objectTemplate)          // PE +0x10 / ELF +0x10
{
    return static_cast<world::IObjectTemplate*>(objectTemplate)->getClassID();
}

unsigned int objectTemplateGetFlags(void* objectTemplate)            // PE +0x44 / ELF +0x48
{
    return static_cast<world::IObjectTemplate*>(objectTemplate)->getFlags();
}

// ObjectTemplate flag controlling dynamic shadow casting.
const unsigned int kTemplateCastsDynamicShadow = 0x0800u;


// Empties the light pointer vector while retaining capacity.

void lightVectorResizeToEmpty(vc71::vector_layout& v)
{
    if (v.first && v.first != v.last)
        v.last = v.first;
}


// Resizes the light-record vector without freeing capacity on shrink.
// New records leave attenuation at +0x38 untouched; the draw loop fills it before use.

void skinnedMeshReserveLightRecords(SkinnedMesh* mesh, unsigned int count)
{
    vc71::vector_layout& v = mesh->m_lightRecords;

    const unsigned int size =
        v.first ? static_cast<unsigned int>((v.last - v.first) / kLightRecordStride) : 0u;

    if (count <= size)
    {
        // Shrink (or no change): the shipped path walks a destroy range and republishes `last`.
        v.last = v.first + kLightRecordStride * count;
        return;
    }

    const unsigned int capacity =
        v.first ? static_cast<unsigned int>((v.end - v.first) / kLightRecordStride) : 0u;
    if (count > capacity)
    {
        char* const grown =
            static_cast<char*>(::renddx9abi::engineAlloc(kLightRecordStride * count));
        if (!grown)
            return;
        if (v.first)
        {
            std::memcpy(grown, v.first, kLightRecordStride * size);
            ::renddx9abi::engineFree(v.first);
        }
        v.first = grown;
        v.end   = grown + kLightRecordStride * count;
    }
    v.last = v.first + kLightRecordStride * count;

    for (unsigned int i = size; i < count; ++i)
    {
        unsigned char* const record =
            reinterpret_cast<unsigned char*>(v.first) + kLightRecordStride * i;
        std::memset(record, 0, 0x38);                       // the fourteen zeroed dwords
        // +0x38 is deliberately NOT written - see the header note.
        *reinterpret_cast<float*>(record + 0x3C) = 1.0f;
        *reinterpret_cast<float*>(record + 0x40) = 1.0f;
    }
}

// sub_100286E0 - the Renderer's time, off its +0x5DC. sub_1015C5E0 - the LightManager's fade
// duration, a double at its +0x838. Both return doubles and both are kept as doubles.
// sub_100286E0 -> sub_100776A0 - The Renderer holds a timer pointer at
// its +0x5DC, and the time is that object's +0x20 read as a double. Two indirections, not one:
// reading +0x5DC itself as a double would give a pointer bit pattern.
double rendererTimeSeconds()
{
    void* const timer = *reinterpret_cast<void* const*>(
        reinterpret_cast<const unsigned char*>(dx9rend::g_Renderer) + 0x5DC);
    return *reinterpret_cast<const double*>(static_cast<const unsigned char*>(timer) + 0x20);
}

} // anonymous namespace


// Shader-quality masks populated by technique initialization before any mesh draw.
// [CHOSEN, not recovered] Storage lives with its accessor.
// [RETIRE-WHEN: the SkinnedMesh technique init fills it]

vc71::vector_layout g_skinnedMeshQualityMasks = { 0, 0, 0, 0 };

// The SkinnedMesh technique map behind sub_1010EFF0's two lookups - header at 0x1025B574. Mirrors
// `g_staticMeshTechniques` in StaticMeshTemplate.cpp, and like it this is a file-scope static that
// nothing indexes by offset, so it is declared as the ordinary container the shipped source used.
typedef std::map<unsigned int, ::dice::hfe::dx9rend::Ra::RaShader*> SkinnedMeshTechniqueMap;
SkinnedMeshTechniqueMap g_skinnedMeshTechniques;

// Technique initialization remains pending until successful; the manager can rearm it.
extern unsigned char g_skinnedMeshTechniquesPending;

// StaticMeshTemplate.cpp publishes this: it is sub_101921D0, which the shipped image calls from
// both classes' technique inits. See the note beside the forwarder there.
bool meshBuildTechniqueMasks(const vc71::string& mfxPath,
                             const vc71::string& techniquePrefix,
                             const char* const* bitNames,
                             std::map<unsigned int, ::dice::hfe::dx9rend::Ra::RaShader*>& techniqueMap,
                             vc71::vector_layout& maskVector);


// Shader feature names in bit order: each entry encodes 1 << index.
// Keep this table independent of StaticMesh's feature list.

const char* const kSkinnedMeshShaderBitNames[] =
{
    "nothing",            // bit 0   0x10220C28
    "hasnormalmap",       // bit 1   0x10220C08
    "objspacenormalmap",  // bit 2   0x10221524
    "hasenvmap",          // bit 3   0x10220BF0
    "usehemimap",         // bit 4   0x10220BD8
    "hascolormapgloss",   // bit 5   0x10220BC4
    "hasshadow",          // bit 6   0x10220A68
    "pointlight",         // bit 7   0x101E1BDC
    "hasshadowocclusion", // bit 8   0x10220B8C
    "hashemiocclusion",   // bit 9   0x10220B78
    0
};


// Initializes skinned technique masks and the Default fallback at key 0xF8000000.
// Failed loads leave the gate armed for retry and do not emit a diagnostic.

void skinnedMeshInitTechniques()
{
    bool built = false;
    if (g_skinnedMeshTechniquesPending)                       // byte_102397D4, 0x1010F19C
    {
        vc71::string prefix("SM");
        vc71::string mfx("shaders/RaShaderSM.mfx");
        built = meshBuildTechniqueMasks(mfx, prefix, kSkinnedMeshShaderBitNames,
                                        g_skinnedMeshTechniques,
                                        g_skinnedMeshQualityMasks);
    }

    if (!built)
        return;                                               // 0x1010F217, and nothing is logged

    vc71::string name("Default");
    ::dice::hfe::dx9rend::Ra::RaShader* const shader =
        ::dice::hfe::dx9rend::Ra::getShader(
            static_cast< ::dice::hfe::dx9rend::Ra::RaShaderTable*>(
                ::dice::hfe::dx9rend::g_renderQueues),
            name, false);                                     // sub_10041F20, skipFileCheck false

    // sub_100EBA10 - insert(pair), which keeps an existing key rather than overwriting.
    g_skinnedMeshTechniques.insert(SkinnedMeshTechniqueMap::value_type(0xF8000000u, shader));

    g_skinnedMeshTechniquesPending = 0;                       // 0x1010F25D, success only
}

namespace {

unsigned int shaderMaskTableSize()
{
    return g_skinnedMeshQualityMasks.first
        ? static_cast<unsigned int>(g_skinnedMeshQualityMasks.last
                                    - g_skinnedMeshQualityMasks.first) >> 2
        : 0u;
}

const unsigned int* shaderMaskTable()
{
    // [CHOSEN, not recovered] Report an unloaded technique table before indexed access.
    // Normal material loading fills it before drawing.
    if (!g_skinnedMeshQualityMasks.first)
    {
        RD9_FRONTIER_AT("dice::hfe::geom::shaderMaskTable - the per-quality mask vector "
                        "(ds:1025B584) is EMPTY. Its filler is the technique init inside "
                        "SkinnedMeshTemplate::readMaterials (slot +0xDC, sub_1010FD00), which IS "
                        "written - so reaching this means a draw ran before that template loaded",
                        0, 0);
        return 0;
    }
    return reinterpret_cast<const unsigned int*>(g_skinnedMeshQualityMasks.first);
}


// Finds the shader for bits, then tries the default key 0xF8000000.
// Logs each miss and returns null if neither key exists.

void* findShaderForBits(unsigned int bits)
{
    SkinnedMeshTechniqueMap::const_iterator found = g_skinnedMeshTechniques.find(bits);
    if (found != g_skinnedMeshTechniques.end())
        return found->second;

    ::renddx9abi::logf("SkinnedMeshTemplate.cpp:918 Couldn't find shader bits: %u", bits);

    found = g_skinnedMeshTechniques.find(0xF8000000u);
    if (found != g_skinnedMeshTechniques.end())
        return found->second;

    ::renddx9abi::logf("SkinnedMeshTemplate.cpp:926 Couldn't find shader default");
    return 0;
}

// sub_10103CB0 - Walk UP the parent chain (+0x28, the same link
// staticMeshRootParent follows) asking each ancestor for IID_IPlayerControlObject (50373), and
// return the first that answers. So it means "the nearest ancestor the local player CONTROLS" -
// which is exactly what the draw's first-person test needs, and it is why the test compares this
// against the light owner's own +0x44 rather than comparing raw roots.
// queryInterface is IBase slot 3 (+0x0C) and the parent is IObject's
// retail-aligned +0x28 prefix field (ObjectLayoutAccess), identical in both flavours.
void* skinnedMeshObjectOf(void* object)
{
    world::IObject* current = static_cast<world::IObject*>(object);
    while (current)
    {
        void* const found = current->queryInterface(::dice::hfe::world::IID_IPlayerControlObject);
        if (found)
            return found;
        current = world::object_detail::LayoutAccess::parent(current);                 // +0x28
    }
    return 0;
}

double lightManagerFadeSeconds()
{
    return *reinterpret_cast<const double*>(
        reinterpret_cast<const unsigned char*>(g_LightManager) + 0x838);
}

// The draw tail's remaining seams. [UNRESOLVED] owners; offsets are read.
// ds:1025B584/588 - a vector of per-quality shader-define masks the draw ANDs into every material's
// mask. ds:10264C48's absence of a bound check is shipped.
const unsigned int* shaderMaskTable();
unsigned int        shaderMaskTableSize();
// sub_1010EFF0 - define-bits -> shader, falling back to the 0xF8000000 default entry. Its
// diagnostics carry the SkinnedMeshTemplate.cpp __FILE__ at lines 918 and 926.
void* findShaderForBits(unsigned int bits);
// sub_10103CB0 - the companion of staticMeshRootParent used by the first-person test.
void* skinnedMeshObjectOf(void* object);

// Clamps high before low; NaN passes through unchanged.
float clampFloat(float value, float low, float high)
{
    double v = value;
    if (value >= static_cast<double>(high))
        v = high;
    if (v <= static_cast<double>(low))
        v = low;
    return static_cast<float>(v);
}

// and recorded rather than named: sub_10079610 is `transformPoint`
// (row 3 added), sub_1007C440 is `transformVector` (row 3 NOT added) and sub_100FB020 is
// transformPoint packed into a Vec4 with an explicit w. Three near-identical bodies whose only
// difference is the translation row and the trailing store; do not substitute one for another.
// sub_10079610, transcribed. The shipped body computes z and y into x87 registers before
// storing x, because out[] may alias in[]; the ordering is load-bearing and is preserved.
void mat4TransformPoint(const Mat4& m, const float in[3], float out[3])
{
    const float z = m.m[2] * in[0] + m.m[10] * in[2] + m.m[6] * in[1] + m.m[14];
    const float y = m.m[1] * in[0] + m.m[9] * in[2] + m.m[5] * in[1] + m.m[13];
    out[0] = m.m[8] * in[2] + m.m[4] * in[1] + in[0] * m.m[0] + m.m[12];
    out[1] = y;
    out[2] = z;
}

// sub_1007C440 - the same body without the translation row: a direction, not a point.
void mat4TransformVector(const Mat4& m, const float in[3], float out[3])
{
    const float z = m.m[2] * in[0] + m.m[10] * in[2] + m.m[6] * in[1];
    const float y = m.m[1] * in[0] + m.m[9] * in[2] + m.m[5] * in[1];
    out[0] = m.m[8] * in[2] + m.m[4] * in[1] + in[0] * m.m[0];
    out[1] = y;
    out[2] = z;
}

// Transforms a point and writes w before x/y/z; preserve the store order when out aliases input.
void mat4TransformPointVec4(const Mat4& m, const float in[3], void* out, unsigned int w)
{
    const float z = m.m[6] * in[1] + m.m[10] * in[2] + m.m[2] * in[0] + m.m[14];
    const float y = m.m[5] * in[1] + m.m[9] * in[2] + m.m[1] * in[0] + m.m[13];
    const float x = m.m[4] * in[1] + m.m[8] * in[2] + in[0] * m.m[0] + m.m[12];
    unsigned char* const d = static_cast<unsigned char*>(out);
    *reinterpret_cast<unsigned int*>(d + 12) = w;
    *reinterpret_cast<float*>(d + 0) = x;
    *reinterpret_cast<float*>(d + 4) = y;
    *reinterpret_cast<float*>(d + 8) = z;
}

// sub_100777A0 - normalise in place, but only when the squared length is not already 1 within
// FLT_EPSILON; a degenerate vector is zeroed and -20001 returned. SkyDome.cpp carries this same
// body in its anonymous namespace, so it has internal linkage and cannot be shared - this is that
// transcription repeated, not a second independent reading. The draw discards the return, as every
// caller in the image does.
void mat4Normalize(float v[3])
{
    const float lengthSq = v[0] * v[0] + v[1] * v[1] + v[2] * v[2];
    if (std::fabs(static_cast<double>(lengthSq) - 1.0) >= 1.1920929e-7)
    {
        if (std::fabs(static_cast<double>(lengthSq)) < 1.1920929e-7)
        {
            v[0] = 0.0f;
            v[1] = 0.0f;
            v[2] = 0.0f;
            return;
        }
        const double scale = 1.0 / std::sqrt(static_cast<double>(lengthSq));
        v[0] = static_cast<float>(v[0] * scale);
        v[1] = static_cast<float>(v[1] * scale);
        v[2] = static_cast<float>(v[2] * scale);
    }
}


// Assigns POD elements, reusing capacity when possible and clearing an empty source.
// [CHOSEN, not recovered] Allocation helpers reproduce contents and ownership without
// the original allocator internals. [RETIRE-WHEN: RW-74]

void vectorAssignRaw(vc71::vector_layout& dst, const vc71::vector_layout& src, unsigned elemSize)
{
    if (&dst == &src)
        return;

    const unsigned int srcCount =
        src.first ? static_cast<unsigned int>((src.last - src.first) / elemSize) : 0u;
    if (!srcCount)
    {
        if (dst.first)
            ::renddx9abi::engineFree(dst.first);
        dst.first = 0;
        dst.last  = 0;
        dst.end   = 0;
        return;
    }

    const unsigned int capacity =
        dst.first ? static_cast<unsigned int>((dst.end - dst.first) / elemSize) : 0u;
    if (srcCount > capacity)
    {
        if (dst.first)
            ::renddx9abi::engineFree(dst.first);
        char* const grown = static_cast<char*>(::renddx9abi::engineAlloc(elemSize * srcCount));
        dst.first = grown;
        dst.end   = grown ? grown + elemSize * srcCount : 0;
        if (!grown)
        {
            dst.last = 0;
            return;
        }
    }
    std::memcpy(dst.first, src.first, elemSize * srcCount);
    dst.last = dst.first + elemSize * srcCount;
}


// Copies the base queue payload, including nested 64-byte and 16-byte record vectors.
// Ref-counted pointers release before addRef; keep the old != new guards for self-assignment.
// The two references are COM interfaces: Release (+0x08) and AddRef (+0x04) are __stdcall with the
// interface pushed, as the shipped copy calls them - not __fastcall with it in ECX, which hands the
// callee a stack word as its `this` (a skinned mesh lit by a muzzle flash crashed in d3d9.dll so).

void renderQueueEntryCopyPayload(void* destEntry, void* sourceEntry)
{
    unsigned char* const d = static_cast<unsigned char*>(destEntry);
    const unsigned char* const a = static_cast<const unsigned char*>(sourceEntry);
    if (d == a)
        return;

    typedef unsigned long(__stdcall * RefFn)(void*);
    struct Local
    {
        static void swapRef(unsigned char* dst, const unsigned char* src, unsigned int at)
        {
            void* const  oldRef = *reinterpret_cast<void* const*>(dst + at);
            void* const  newRef = *reinterpret_cast<void* const*>(src + at);
            if (oldRef == newRef)
                return;
            if (oldRef)   // release first - shipped, and safe only behind the test above
                reinterpret_cast<RefFn>((*reinterpret_cast<void***>(oldRef))[0x08 / 4])(oldRef);
            *reinterpret_cast<void**>(dst + at) = newRef;
            if (newRef)
                reinterpret_cast<RefFn>((*reinterpret_cast<void***>(newRef))[0x04 / 4])(newRef);
        }
    };

    *reinterpret_cast<unsigned int*>(d + 0x04) = *reinterpret_cast<const unsigned int*>(a + 0x04);

    std::memcpy(d + 0x0C, a + 0x0C, 0x1C - 0x0C);      // +0x0C .. +0x18
    Local::swapRef(d, a, 0x20);
    Local::swapRef(d, a, 0x28);
    std::memcpy(d + 0x30, a + 0x30, 0x40 - 0x30);      // +0x30 .. +0x3C
    *reinterpret_cast<unsigned int*>(d + 0x44) = *reinterpret_cast<const unsigned int*>(a + 0x44);
    *reinterpret_cast<unsigned int*>(d + 0x48) = *reinterpret_cast<const unsigned int*>(a + 0x48);
    std::memcpy(d + 0x50, a + 0x50, 0x60 - 0x50);      // +0x50 .. +0x5C
    std::memcpy(d + 0x64, a + 0x64, sizeof(Mat4));

    vectorAssignRaw(*reinterpret_cast<vc71::vector_layout*>(d + 168),
                    *reinterpret_cast<const vc71::vector_layout*>(a + 168), 64);
    vectorAssignRaw(*reinterpret_cast<vc71::vector_layout*>(d + 188),
                    *reinterpret_cast<const vc71::vector_layout*>(a + 188), 64);
    vectorAssignRaw(*reinterpret_cast<vc71::vector_layout*>(d + 208),
                    *reinterpret_cast<const vc71::vector_layout*>(a + 208), 16);
    vectorAssignRaw(*reinterpret_cast<vc71::vector_layout*>(d + 228),
                    *reinterpret_cast<const vc71::vector_layout*>(a + 228), 16);
    *reinterpret_cast<unsigned int*>(d + 248) = *reinterpret_cast<const unsigned int*>(a + 248);
    vectorAssignRaw(*reinterpret_cast<vc71::vector_layout*>(d + 252),
                    *reinterpret_cast<const vc71::vector_layout*>(a + 252), 16);
    *reinterpret_cast<unsigned int*>(d + 268) = *reinterpret_cast<const unsigned int*>(a + 268);
}

// The SkinnedMeshRenderer's cached shadow Effect (ds:1025B230) and its four cached ShaderParameters
// (ds:1025B234/238/23C/240). All five are filled by SkinnedMeshRenderer, which is NOT recovered
// (sub_10123Axx); the shadow draw below is written in full against them, which is the same position
// StaticMesh.cpp's shadow pass is in - transcribed, compiled, and not yet drivable.
struct SkinnedMeshShadowState
{
    void* effect;           // ds:1025B230; its +0x08 is the D3DX interface, CommitChanges at 0x104
    void* lightMat;         // ds:1025B234
    void* lightTrapezMat;   // ds:1025B238
    void* texLayer0;        // ds:1025B23C
    void* geomBones;        // ds:1025B240; set through its vslot +0x28 as (Mat4 palette, count)
};


// Shared shadow effect and parameters initialized by SkinnedMeshTemplate::readMaterials.
// Later templates retain the existing references. The [CHOSEN] guard reports missing
// initialization before a draw can dereference this state.

SkinnedMeshShadowState g_skinnedMeshShadowState = { 0, 0, 0, 0, 0 };

} // anonymous namespace


// Creates shared shadow parameters once, otherwise retains each existing reference.
// Effect loading must succeed before the unguarded parameter lookups.

void skinnedMeshInitShadowState()
{
    SkinnedMeshShadowState& state = g_skinnedMeshShadowState;

    if (state.effect)
    {
        typedef void(__fastcall * RefFn)(void*, void*);
        reinterpret_cast<RefFn>(
            (*reinterpret_cast<void***>(state.effect))[0x00 / sizeof(void*)])(state.effect, 0);
        void* const params[4] =
            { state.lightMat, state.lightTrapezMat, state.texLayer0, state.geomBones };
        for (int i = 0; i < 4; ++i)
            reinterpret_cast<RefFn>(
                (*reinterpret_cast<void***>(params[i]))[0x04 / sizeof(void*)])(params[i], 0);
        return;
    }

    // dice.hfe.geom.GeometryTemplate.SkinnedMesh, from ModuleGeom.cpp's class-id census.
    const unsigned int kSkinnedMeshTemplateClassId = 0x870CA652u;

    vc71::string effectName("Shaders/");          // *unk_10263DF0, static init at 0x101D5740
    effectName += "SkinnedMesh";
    state.effect = ::dice::hfe::dx9rend::shaderManagerGetEffect(
        ::dice::hfe::dx9rend::g_shaderManager, effectName.c_str());

    {
        void* const lm = g_LightManager;
        typedef void(__fastcall * RegisterFn)(void*, void*, unsigned int, void*);
        reinterpret_cast<RegisterFn>(
            (*reinterpret_cast<void***>(lm))[0x2F0 / sizeof(void*)])(
                lm, 0, kSkinnedMeshTemplateClassId, state.effect);

        typedef unsigned char(__fastcall * PredFn)(void*, void*);
        if (reinterpret_cast<PredFn>(
                (*reinterpret_cast<void***>(lm))[0xF0 / sizeof(void*)])(lm, 0))
        {
            const vc71::string technique(
                ::dice::hfe::dx9rend::deviceCapsIsNvidia() ? "DrawShadowMapNV" : "DrawShadowMap");
            ::dice::hfe::dx9rend::effectSetTechniqueByName(state.effect, technique);
        }
    }

    ::dice::hfe::dx9rend::Effect* const effect =
        static_cast< ::dice::hfe::dx9rend::Effect*>(state.effect);
    state.lightMat =
        ::dice::hfe::dx9rend::effectGetShaderParameterTyped(effect, vc71::string("vpLightMat"));
    state.lightTrapezMat =
        ::dice::hfe::dx9rend::effectGetShaderParameterTyped(effect, vc71::string("vpLightTrapezMat"));
    state.texLayer0 =
        ::dice::hfe::dx9rend::effectGetShaderParameterTyped(effect, vc71::string("TEXLAYER0"));
    state.geomBones =
        ::dice::hfe::dx9rend::effectGetShaderParameterTyped(effect, vc71::string("BoneArray"));
}

namespace {

SkinnedMeshShadowState* skinnedMeshShadowState()
{
    // Require shadow-technique initialization before this draw.
    if (!g_skinnedMeshShadowState.effect)
    {
        RD9_FRONTIER_AT("dice::hfe::geom::skinnedMeshShadowState - the five globals at "
                        "ds:1025B230..1025B240 are NULL. Their one-shot initialiser IS written - "
                        "SkinnedMeshTemplate::readMaterials (slot +0xDC, sub_1010FD00) - so "
                        "reaching this means a shadow draw ran before that template loaded; the "
                        "parameters are vpLightMat / vpLightTrapezMat / TEXLAYER0 / BoneArray",
                        0, 0);
        return 0;
    }
    return &g_skinnedMeshShadowState;
}

// The four seams the shadow draw drives those five globals through. Each is a single raw vtable
// dispatch at a measured offset, written here rather than inline so the draw body reads as the
// shipped sequence it is.

// ShaderParameter vslot +0x18 - `setValue(value, index)`, the same slot StaticMesh's shadow pass
// reaches through its typed dx9rend::ShaderParameter::setValue.
void shaderParameterSetValue(void* parameter, const void* value, int index)
{
    typedef void(__fastcall * Fn)(void*, void*, const void*, int);
    reinterpret_cast<Fn>(
        (*reinterpret_cast<void***>(parameter))[0x18 / sizeof(void*)])(parameter, 0, value, index);
}

// ShaderParameter vslot +0x28 - the matrix-array upload, `(Mat4 palette, unsigned count)`. This
// is the GeomBones set: the skinning palette reaching the shader.
void shaderParameterSetMatrixArray(void* parameter, Mat4* const* palette, unsigned int count)
{
    typedef void(__fastcall * Fn)(void*, void*, Mat4* const*, unsigned int);
    reinterpret_cast<Fn>(
        (*reinterpret_cast<void***>(parameter))[0x28 / sizeof(void*)])(parameter, 0, palette, count);
}

// `mov eax, [effect+8]; call [[eax]+104h]` - ID3DXEffect::CommitChanges on the wrapper's D3DX
// effect. Byte-for-byte the shape StaticMesh.cpp's staticMeshEffectCommitChanges records.
void skinnedMeshEffectCommitChanges(void* effect)
{
    ID3DXEffect* const d3dx =
        *reinterpret_cast<ID3DXEffect**>(static_cast<unsigned char*>(effect) + 8);
    d3dx->CommitChanges();
}

// The renderer's statistics block, its +0x23C word. Every shadow draw adds the material's primitive
// count to it. note +0x23C - StaticMesh's shadow draw uses +0x25C, a different counter, and adds
// the LOD's count rather than the material's. Two classes, two statistics; not unified.
void skinnedMeshCountShadowPrimitives(unsigned int primitives)
{
    void* const base = dx9rend::g_rendererBase;
    typedef void*(__fastcall * Fn)(void*, void*);
    void* const stats = reinterpret_cast<Fn>(
        (*reinterpret_cast<void***>(base))[0x6C / sizeof(void*)])(base, 0);
    *reinterpret_cast<unsigned int*>(static_cast<unsigned char*>(stats) + 0x23C) += primitives;
}

// The two Renderer scale factors updateLodSelection multiplies every lod distance by, read off the
// Renderer console object at +0x84C and +0x8B8. g_Renderer is NOT g_rendererBase: the shipped
// body reads these two through the console object while taking the frame ordinal and the render
// views through the base. Two different globals, kept apart. [UNRESOLVED] names; offsets are read.
float rendererLodScaleAt(unsigned int byteOffset)
{
    return *reinterpret_cast<const float*>(
        reinterpret_cast<const unsigned char*>(dx9rend::g_Renderer) + byteOffset);
}


} // anonymous namespace


// Initializes mesh state, selects loaded template data, then allocates the palette.
// Vector allocator words and fields +0x160, +0x348, and +0x594 remain untouched.

SkinnedMesh::SkinnedMesh(void* objectTemplate)
    : GeometryInstanceData(g_skinnedMeshVtable)
{
    // The shipped constructor writes off_1020A660 (the base table) and then off_10221410. Only
    // the final store is reproduced: the transient base vptr is observable solely from inside the
    // base constructor, which this class does not have as a separate body.
    // The common prefix is initialized by GeometryInstanceData.
    m_refCount = 1;                  // +0x030, and the trio at +0x00..+0x08 counts here

    m_lodRecord    = 0;
    m_lodMaterials = 0;
    m_geomIndex    = 0;
    m_lodIndex     = 0;
    m_lodExplicit  = 0;
    m_dword48      = 0;
    m_dword4C      = 0;
    m_byte50       = 0;

    m_template  = static_cast<SkinnedMeshTemplate*>(objectTemplate);   // +0x058
    m_transform = 0;                                                    // +0x05C

    m_matrix60  = Mat4::getIdentity();
    m_matrixA0  = Mat4::getIdentity();
    m_matrixE0  = Mat4::getIdentity();
    m_matrix120 = Mat4::getIdentity();

    // +0x164: first/last/end only. The allocator word is NOT written - shipped.
    m_lights.first = 0;
    m_lights.last  = 0;
    m_lights.end   = 0;

    m_boneTransforms     = 0;        // +0x174 / +0x178, the caller's skeleton palette
    m_boneTransformCount = 0;
    m_excludedPart = -1;             // +0x17C, the "suppress nothing" sentinel

    m_ownerObject = 0;
    m_byte334     = 0;
    m_shadowCasterFrame = 0;
    m_dword33C    = 0;
    m_distanceToCamera = 0.0f;
    m_paletteFrameStamp = 0;         // +0x34C
    m_lodFrameStamp     = 0;         // +0x350
    m_float354    = 1.0f;
    m_double358   = 0.0;

    // +0x360 .. +0x583, the 0x224-byte block Terrain.cpp already drives.
    ::dice::hfe::dx9rend::Ra::raObjectParametersConstruct(m_objectParameters);

    // +0x584: first/last/end only, allocator word untouched - the same shape again.
    m_lightRecords.first = 0;
    m_lightRecords.last  = 0;
    m_lightRecords.end   = 0;

    // the same arithmetic as refreshSelection, inlined here in retail.
    // No template/null/vector guards are shipped beyond this loaded-byte test. Instances made
    // before loading retain null selections until load() refreshes them; later instances select
    // immediately in this constructor, before createInstance takes its already-loaded return.
    if (m_template->m_loaded)
        refreshSelection();

    // Four palettes of twenty-six matrices, after selection (PE 0x101A7909..0x101A79C1).
    // Each count is cleared before allocating its palette; each matrix is set to identity twice,
    // once inside the allocation guard and once after storing it. Both are shipped operations.
    for (int palette = 0; palette < kSkinnedMeshPalettes; ++palette)
    {
        m_boneCounts[palette] = 0;
        for (int bone = 0; bone < kSkinnedMeshBonesPerPalette; ++bone)
        {
            Mat4* const matrix =
                static_cast<Mat4*>(::renddx9abi::engineAlloc(sizeof(Mat4)));
            if (matrix)
                *matrix = Mat4::getIdentity();
            m_bonePalettes[palette][bone] = matrix;
            *m_bonePalettes[palette][bone] = Mat4::getIdentity();
        }
    }
    m_fov = -1.0f;                  // PE 0x101A79CF, after the palette loop

    // [UNRESOLVED] Separate existing constructor gap: retail 0x101A79EB..0x101A79F1 looks up
    // ClassManager.getSingleton("HeightmapCluster") here. The current zero keeps the named
    // updateHeightOverTerrain frontier unreachable; this radius repair does not recover it.
    m_heightmapCluster = 0;
}

SkinnedMesh* skinnedMeshConstruct(void* storage, void* objectTemplate)
{
    return new (storage) SkinnedMesh(objectTemplate);
}


// The IBase trio, +0x00..+0x08 - 0x101071D0 / 0x101071E0 / 0x101071F0, all read.

// these ARE NOT StaticMesh'S bodies. StaticMesh counts at +0x34 through 0x10103C00/10/20; these
// count at +0x30. Two classes, two offsets, two sets of addresses - which is the cross-check that
// says the layouts really do diverge at +0x30, and not a transcription slip.
// addRef returns THE NEW count (`add [ecx+30h],1` then `mov eax,[ecx+30h]`), exactly as on the
// sibling. It is not void.

int SkinnedMesh::addRef()
{
    ++m_refCount;
    return m_refCount;
}

int SkinnedMesh::getRef() const
{
    return m_refCount;
}

// decrement, and on zero call vtable slot +0x34 - the scalar deleting destructor - with
// flag 1. The remaining count is returned either way.

// Returns the decremented count, captured before deletion to avoid reading freed memory.
int SkinnedMesh::release()
{
    --m_refCount;
    const int remaining = m_refCount;
    if (!remaining)
    {
        // Dispatched through the vtable (`mov eax,[ecx]; push 1; call [eax+34h]`), not called
        // directly. Slot +0x34 is the scalar deleting destructor, and flag bit 0 tells it to free
        // the block after running ~SkinnedMesh.
        typedef void*(__fastcall * DeleteFn)(void*, void*, unsigned int);
        reinterpret_cast<DeleteFn>(m_vptr[0x34 / sizeof(void*)])(this, 0, 1u);
    }
    return remaining;
}


// Returns the primary face for IBase, IGeometry, or IRenderable; otherwise null.

void* SkinnedMesh::queryInterface(unsigned int iid)
{
    if (iid == IID_IBase || iid == IID_IGeometry || iid == IID_IRenderable)
        return this;
    return 0;
}


// +0x10 / +0x14, THE FOV override.

// This is the slot Soldier::hackSetFov calls. 0x101A8A90 stores the argument and returns without
// materializing an x87 result; PE 0x61C69E and ELF 0x81B4B40 reload the input FOV. The constructor seeds
// -1.0f, and the draw's first branch tests `m_fov == -1.0f` to pick between two projections on the
// render view. First-person weapon FOV.

void SkinnedMesh::setFov(float fov)
{
    m_fov = fov;
}

float SkinnedMesh::getFov() const
{
    return m_fov;
}

// +0x24 / +0x28, 0x101A8A10 / 0x101A8A20. The setter returns the stored pointer in EAX.
// [UNRESOLVED] role; the draw chases the stored pointer's +0x2F8, +0x338 and +0x3B8 as Mat4s.
void* SkinnedMesh::setDword33C(void* value)
{
    m_dword33C = value;
    return m_dword33C;
}

void* SkinnedMesh::getDword33C() const
{
    return m_dword33C;
}

// +0x38, 0x101A8A30 - `mov eax, [ecx+58h]; retn`. Two instructions.
void* SkinnedMesh::getTemplate() const
{
    return m_template;
}


// Returns borrowed LOD bounds or a non-null shared empty bound.
// The shared fallback starts dirty; finalizing it changes the fallback for all callers.

const void* SkinnedMesh::getLocalBounds() const
{
    if (m_lodRecord)
        return m_lodRecord;

    // Built once, the same way skinnedMeshZeroMatrix builds its static: sixty zero bytes with the
    // one non-zero byte set. static_assert keeps the shape honest against the 0x3C the image shows.
    static MeshBounds emptyBounds;
    static bool       emptyBoundsBuilt = false;
    if (!emptyBoundsBuilt)
    {
        std::memset(&emptyBounds, 0, sizeof(emptyBounds));
        emptyBounds.dirty = 1;
        emptyBoundsBuilt  = true;
    }
    static_assert(sizeof(MeshBounds) == 0x3C, "ds:1023AC88 is 0x3C bytes wide");
    return &emptyBounds;                                          // ds:1023AC88
}


// Returns zero without an LOD, a fixed radius of one for soldiers, or finalized LOD bounds.

float SkinnedMesh::getBoundingRadius() const
{
    if (!m_lodRecord)
        return 0.0f;                                          // ds:101DDE64 == 0.0f

    // The owner is guarded; its template is not. See objectTemplateOf's note.
    if (m_ownerObject)
    {
        if (objectTemplateGetClassID(objectTemplateOf(m_ownerObject))
            == ::dice::hfe::world::CID_SoldierTemplate)       // ds:10208B20 == 38035
            return 1.0f;                                      // ds:101DDDE4 == 1.0f
    }

    MeshBounds* const bounds =
        reinterpret_cast<MeshBounds*>(m_lodRecord + kLodBoundsOffset - 28);
    if (m_lodRecord[kLodBoundsDirtyOffset])
        meshBoundsFinalize(*bounds);

    return *reinterpret_cast<const float*>(m_lodRecord + kLodBoundsOffset);
}


// +0x48, 0x101A67C0 - `mov eax,[ecx+5Ch]; test; jnz ret; jmp sub_100791E0`.

// cross-check: byte-for-byte the shape StaticMesh.h records for its own +0x48 ("the transform
// slot +0x50 stored, or the shared identity when none"), with 0x5C here where the sibling has
// 0x284. Same shared identity builder, sub_100791E0.

const Mat4* SkinnedMesh::getGeometryTransformation() const
{
    if (m_transform)
        return m_transform;
    return &Mat4::getIdentity();
}

// +0x50, the store, and nothing else. Unlike StaticMesh's +0x50, which also runs a
// one-shot occluder build behind its +0x348, this body is the bare assignment.
void SkinnedMesh::setGeometryTransformation(const Mat4* transform)
{
    m_transform = transform;
}


// +0x54, the skeleton palette. Two stores and nothing else.

// updateBonePalettes proves THE type: it indexes +0x174 with `partRecord[0] << 6`, a stride of
// 64 = sizeof(Mat4), to fetch the bone each part is multiplied by. And that identifies Soldier.cpp's
// `geometrySetBoneTransforms(geometry, m_collisionSkeleton->m_transforms, boneCount)` - a pointer
// and a count - as this slot, one below the "+0x58" its comment carries. See the header.

void SkinnedMesh::setBoneTransforms(const Mat4* bones, unsigned int count)
{
    m_boneTransforms     = bones;
    m_boneTransformCount = count;
}


// Refreshes selection and appends all part IDs except the excluded part.
// Material count bounds the parallel part vectors; the LOD pointer is reread per element.

void SkinnedMesh::gatherVisibleParts(HostIntVector* out)
{
    updateLodSelection();            // sub_101A7180, the same refresh both draws run first

    const unsigned int geomCount =
        m_lodMaterials && m_lodMaterials->first
            ? static_cast<unsigned int>((m_lodMaterials->last - m_lodMaterials->first)
                                        / kMaterialStride)
            : 0u;

    // The ONE place this class touches a container the host owns. See the HostIntVector note in
    // the header for why the type differs by flavour; the push below is the only line that cares.
#if BF2_PLUGIN_ABI_NATIVE
    HostIntVector& ids = *out;       // our own BF2.exe: same STL, same /MD heap, same container
#else
    vc71::vectorRef<int> ids(out);   // the retail host's VC7.1 vector, driven through the shim
#endif

    for (unsigned int i = 0; i < geomCount; ++i)
    {
        const unsigned int byteOffset = 16 * i;
        const int* part = *reinterpret_cast<int* const*>(
            m_lodRecord + byteOffset + kLodPartVectorOffset);

        while (part != *reinterpret_cast<int* const*>(
                   m_lodRecord + byteOffset + kLodPartVectorOffset + 4))
        {
            if (m_excludedPart != *part)
                ids.push_back(*part);

            part = reinterpret_cast<const int*>(
                reinterpret_cast<const unsigned char*>(part) + kPartRecordStride);
        }
    }
}

// +0x60, set the excluded part, or clear it. The second argument is the enable: false
// stores the -1 sentinel and the first argument is discarded.
void SkinnedMesh::setExcludedPart(int part, bool enable)
{
    if (enable)
        m_excludedPart = part;
    else
        m_excludedPart = -1;
}


// Selects geometry and resets its LOD to zero. Selection methods do nothing until the
// template is loaded; unloaded geometry/LOD counts are zero.
void SkinnedMesh::setGeomIndex(int geom)
{
    if (!m_template->m_loaded)
        return;

    if (m_geomIndex == static_cast<unsigned int>(geom))
        return;

    m_geomIndex = geom;
    m_lodIndex  = 0;
    m_lodRecord = reinterpret_cast<unsigned char*>(geomRecord(m_template, m_geomIndex)->first);
    m_lodMaterials = reinterpret_cast<vc71::vector_layout*>(
        companionRecord(m_template, m_geomIndex)->first);
}

// +0x70, 0x101A6810 - `mov eax, [ecx+3Ch]; retn`.
int SkinnedMesh::getGeomIndex() const
{
    return static_cast<int>(m_geomIndex);
}

// +0x74, how many geoms the template has, out of its 32-byte-strided vector.
int SkinnedMesh::getGeomCount() const
{
    if (m_template->m_loaded && m_template->m_vector0DC.first)
        return static_cast<int>((m_template->m_vector0DC.last - m_template->m_vector0DC.first) / 32);
    return 0;
}


// Selects a LOD; -1 restores LOD zero and clears explicit selection.
// Out-of-range requests retry through the virtual slot with -1, preserving overrides.
// Returns the last record pointer computed by the selected branch.

void* SkinnedMesh::setLodIndex(int lod)
{
    if (!m_template->m_loaded)
        return 0;

    if (lod == -1)
    {
        m_lodIndex    = 0;
        m_lodExplicit = 0;
        m_lodRecord = reinterpret_cast<unsigned char*>(geomRecord(m_template, m_geomIndex)->first);
        m_lodMaterials = reinterpret_cast<vc71::vector_layout*>(
            companionRecord(m_template, m_geomIndex)->first);
        return m_lodMaterials;
    }

    const vc71::vector_layout* const geom = geomRecord(m_template, m_geomIndex);
    const unsigned int lodCount =
        geom->first ? static_cast<unsigned int>((geom->last - geom->first) / kLodRecordStride) : 0u;

    if (!geom->first || lod < 0 || static_cast<unsigned int>(lod) >= lodCount)
    {
        // The shipped retry really is a virtual self-dispatch, and now that the table exists it
        // is written as one. A subclass overriding +0x78 sees the retry, exactly as in retail.
        typedef void*(__fastcall * SetLodFn)(void*, void*, int);
        return reinterpret_cast<SetLodFn>(m_vptr[0x78 / sizeof(void*)])(this, 0, -1);
    }

    m_lodIndex    = lod;
    m_lodExplicit = 1;
    m_lodRecord   = reinterpret_cast<unsigned char*>(geom->first) + kLodRecordStride * lod;
    m_lodMaterials = reinterpret_cast<vc71::vector_layout*>(
        reinterpret_cast<unsigned char*>(companionRecord(m_template, m_geomIndex)->first)
        + kCompanionStride * lod);
    return m_lodRecord;
}

// +0x7C, 0x101A8A70 - `mov eax, [ecx+40h]; retn`.
int SkinnedMesh::getLodIndex() const
{
    return static_cast<int>(m_lodIndex);
}

// +0x80, the number of LODs the current geom has, 132 bytes apiece.
int SkinnedMesh::getLodCount() const
{
    if (!m_template->m_loaded)
        return 0;

    const vc71::vector_layout* const geom = geomRecord(m_template, m_geomIndex);
    if (!geom->first)
        return 0;
    return static_cast<int>((geom->last - geom->first) / kLodRecordStride);
}


// +0x9C .. +0xB0 - the owning object.


// +0x9C, 0x101A6930. world::setGeometry calls this right after setComponent succeeds - the same
// sequence StaticMesh.h records for its own +0x9C.
void SkinnedMesh::setOwnerObject(void* object)
{
    m_ownerObject = object;
}

// +0xA0, 0x101A8A80 - `mov eax, [ecx+330h]; retn`.
void* SkinnedMesh::getOwnerObject() const
{
    return m_ownerObject;
}


// Returns the owner's root and refreshes dynamic-shadow eligibility from its template.
// Null roots clear eligibility and remain null in the return value.

void* SkinnedMesh::refreshRootObject()
{
    void* const root = staticMeshRootParent(m_ownerObject);

    unsigned char castsShadow = 0;
    if (root)
    {
        // No guard on the template pointer, exactly as shipped.
        if (objectTemplateGetFlags(objectTemplateOf(root)) & kTemplateCastsDynamicShadow)
            castsShadow = 1;
    }
    m_byte334 = castsShadow;
    return root;
}

// +0xA8, refresh, then report the memoised byte. Two instructions plus the call.
unsigned char SkinnedMesh::refreshAndGetByte334()
{
    refreshRootObject();
    return m_byte334;
}


// Shadow-frame state belongs to the root object's geometry: setters affect root owners
// only, and getters delegate when this mesh is parented under another object.

void SkinnedMesh::setShadowCasterFrame(void* frame)
{
    if (!m_ownerObject)
        return;
    if (m_ownerObject == refreshRootObject())
        m_shadowCasterFrame = frame;
}

void* SkinnedMesh::getShadowCasterFrame()
{
    void* const root = refreshRootObject();
    if (root && root != m_ownerObject)
    {
        // +0x38 is IObject's retail-aligned geometry field (ObjectLayoutAccess, RW-266). The +0xB0
        // below is the renderer geometry's own table - this DLL's, not the host's.
        void* const rootGeometry =
            world::object_detail::LayoutAccess::geometry(static_cast<world::IObject*>(root));
        // Delegate the result directly to the root geometry.
        if (rootGeometry)
            return viewSlot<void*>(rootGeometry, 0xB0);
    }
    return m_shadowCasterFrame;
}

// +0xF4, 0x101A8AB0 - `*(this + 0x510) = value`. +0x510 lands inside the Ra block, on the
// `Transparency` holder; the draw reads it as a float, refuses the whole body below 0.00001f and
// flags "is translucent" below 1.0f.
void SkinnedMesh::setTransparency(float alpha)
{
    *reinterpret_cast<float*>(m_objectParameters + (0x510 - 0x360)) = alpha;
}


// Refreshes automatic LOD once per frame, selecting the last qualifying distance.
// Explicit LODs remain selected until setLodIndex(-1); their frame stamp still advances.

void SkinnedMesh::updateLodSelection()
{
    const unsigned int frame = dx9rend::rendererFrameOrdinal();          // base vslot +0x54
    if (m_lodFrameStamp == frame)
        return;

    const bool explicitLod = m_lodExplicit != 0;
    m_lodFrameStamp = frame;
    if (explicitLod)
        return;

    // The view the distance is measured against depends on the FOV override, the same test the
    // main draw opens with: base vslot +0x50 normally, +0xD4 when a FOV was set.
    void* view;
    if (m_fov == -1.0f)
    {
        view = dx9rend::rendererGetRenderViewDup();                      // base vslot +0x50
    }
    else
    {
        void* const base = dx9rend::g_rendererBase;
        typedef void*(__fastcall * ViewFn)(void*, void*);
        view = reinterpret_cast<ViewFn>(
            (*reinterpret_cast<void***>(base))[0xD4 / sizeof(void*)])(base, 0);
    }

    // The current geom's first lod record supplies the radius the distance is biased by.
    const vc71::vector_layout* const geom = geomRecord(m_template, m_geomIndex);
    unsigned char* const firstLod = reinterpret_cast<unsigned char*>(geom->first);
    if (firstLod[kLodBoundsDirtyOffset])
        meshBoundsFinalize(*reinterpret_cast<MeshBounds*>(firstLod));
    const float radius = *reinterpret_cast<const float*>(firstLod + kLodBoundsOffset);

    typedef float(__fastcall * LodScaleFn)(void*, void*);
    const float viewLodScale = reinterpret_cast<LodScaleFn>(
        (*reinterpret_cast<void***>(view))[0x2C / sizeof(void*)])(view, 0);

    const float threshold = viewLodScale * m_distanceToCamera - radius;
    const float rendererScale = rendererLodScaleAt(0x84C) * rendererLodScaleAt(0x8B8);

    // The per-lod distances live in the geom record's second vector.
    const vc71::vector_layout& distances = geom[1];
    const unsigned int count =
        distances.first
            ? static_cast<unsigned int>((distances.last - distances.first) / sizeof(float))
            : 0u;
    const float* const values = reinterpret_cast<const float*>(distances.first);

    unsigned int chosen = 0;
    for (unsigned int i = 0; i < count; ++i)
        if (values[i] * rendererScale < threshold)
            chosen = i + 1;

    if (chosen != m_lodIndex)
    {
        m_lodIndex     = chosen;
        m_lodRecord    = reinterpret_cast<unsigned char*>(geom->first) + kLodRecordStride * chosen;
        m_lodMaterials = reinterpret_cast<vc71::vector_layout*>(
            reinterpret_cast<unsigned char*>(companionRecord(m_template, m_geomIndex)->first)
            + kCompanionStride * chosen);
    }
}


// Builds per-material palettes once per frame using part transforms and skeleton matrices.
// Missing skeletons use identity; excluded parts use a separate shared transform.
// Only geometry zero receives an identity fallback when its palette is empty.

void SkinnedMesh::updateBonePalettes()
{
    const unsigned int frame = dx9rend::rendererFrameOrdinal();
    if (m_paletteFrameStamp == frame)
        return;
    m_paletteFrameStamp = frame;

    const unsigned int count =
        m_lodMaterials && m_lodMaterials->first
            ? static_cast<unsigned int>((m_lodMaterials->last - m_lodMaterials->first)
                                        / kMaterialStride)
            : 0u;
    if (!count)
        return;

    for (unsigned int geom = 0; geom < count; ++geom)
    {
        m_boneCounts[geom] = 0;

        const unsigned int vectorOffset = 16 * geom + kLodPartVectorOffset;
        unsigned char* part = *reinterpret_cast<unsigned char* const*>(m_lodRecord + vectorOffset);
        Mat4** palette = &m_bonePalettes[geom][0];

        if (part != *reinterpret_cast<unsigned char* const*>(m_lodRecord + vectorOffset + 4))
        {
            do
            {
                const int partId = *reinterpret_cast<const int*>(part);
                if (m_boneTransforms)
                {
                    if (m_excludedPart != partId)
                    {
                        (*palette)->fullMult(*reinterpret_cast<const Mat4*>(part + 4),
                                             m_boneTransforms[partId]);   // partId << 6
                    }
                    else
                    {
                        **palette = skinnedMeshZeroMatrix();
                    }
                }
                else
                {
                    **palette = Mat4::getIdentity();
                }

                ++m_boneCounts[geom];
                ++palette;
                part += kPartRecordStride;
            }
            // The end pointer is RE-read from the object each iteration, not cached. Preserved.
            while (part != *reinterpret_cast<unsigned char* const*>(m_lodRecord + vectorOffset + 4));

            if (m_boneCounts[geom])
                continue;
        }

        if (geom == 0)
        {
            *m_bonePalettes[0][0] = Mat4::getIdentity();
            m_boneCounts[0] = 1;
        }
    }
}


// refreshSelection - 0x101A6FD0. Seven instructions, no guards.

// This is the other half of the load protocol: an instance created before its template was read
// has null m_lodRecord / m_lodMaterials, and SkinnedMeshTemplate::load calls
// this on every live instance once the file is in. It recomputes both from the current geom and lod
// rather than resetting them, so a selection made earlier survives the load.

void* SkinnedMesh::refreshSelection()
{
    const vc71::vector_layout* const geom = geomRecord(m_template, m_geomIndex);
    m_lodRecord = reinterpret_cast<unsigned char*>(geom->first) + kLodRecordStride * m_lodIndex;
    m_lodMaterials = reinterpret_cast<vc71::vector_layout*>(
        reinterpret_cast<unsigned char*>(companionRecord(m_template, m_geomIndex)->first)
        + kCompanionStride * m_lodIndex);
    return m_lodMaterials;
}


// Draws matching alpha-pass materials with the caster's shadow transform.
// Temporarily applies the shadow LOD, then restores it and invalidates the palette stamp.
// Both shadow parameters receive the same matrix; only the caster-specific arm commits it.

void SkinnedMesh::drawShadowMap(void* caster, unsigned int, unsigned int, unsigned int,
                                bool alphaPass, unsigned int)
{
    SkinnedMeshTemplate* const t = m_template;
    if (!t->m_vertexBuffer)                                              // +0x94
        return;
    if (!m_transform)
        return;
    if (alphaPass && !m_lodRecord[129])
        return;

    updateLodSelection();

    const int shadowLod  = skinnedMeshShadowLod();                       // LightManager +0x290
    const int currentLod = getLodIndex();
    const int lodCount   = getLodCount();
    int selected = shadowLod;
    if (currentLod > shadowLod)
    {
        selected = currentLod;              // already coarser than the shadow lod: leave it alone
    }
    else if (currentLod != shadowLod)
    {
        int lod = lodCount - 1;
        if (lod >= shadowLod)
            lod = shadowLod;
        setLodIndex(lod);
    }

    updateBonePalettes();

    SkinnedMeshShadowState* const state = skinnedMeshShadowState();

    dx9rend::vertexDeclarationBind(t->m_vertexDeclaration);              // +0xA0
    dx9rend::vertexBufferBindStream(t->m_vertexBuffer, 0);               // +0x94
    dx9rend::indexBufferBind(t->m_indexBuffer);                          // +0x9C

    const unsigned char* const casterBytes = static_cast<const unsigned char*>(caster);
    if (casterBytes && casterBytes[0x495])
    {
        Mat4 light = Mat4::getIdentity();
        light.fullMult(*m_transform, *reinterpret_cast<const Mat4*>(casterBytes + 0x238));
        Mat4 scratch = Mat4::getIdentity();
        scratch.fullMult(*m_transform, *reinterpret_cast<const Mat4*>(casterBytes + 0x238));
        // `light` is recomputed here; the value above is discarded. Shipped.
        light.fullMult(scratch, *reinterpret_cast<const Mat4*>(casterBytes + 0x278));
        shaderParameterSetValue(state->lightMat, &light, 0);
        shaderParameterSetValue(state->lightTrapezMat, &light, 0);       // the same matrix
        skinnedMeshEffectCommitChanges(state->effect);
    }
    else
    {
        const unsigned char* const own = static_cast<const unsigned char*>(m_dword33C);
        Mat4 light = Mat4::getIdentity();
        light.fullMult(*m_transform, *reinterpret_cast<const Mat4*>(own + 0x238));
        Mat4 trapez = Mat4::getIdentity();
        trapez.fullMult(*m_transform, *reinterpret_cast<const Mat4*>(own + 0x278));
        shaderParameterSetValue(state->lightMat, &light, 0);
        shaderParameterSetValue(state->lightTrapezMat, &trapez, 0);
        // No CommitChanges on this arm.
    }

    const unsigned int materials =
        m_lodMaterials && m_lodMaterials->first
            ? static_cast<unsigned int>((m_lodMaterials->last - m_lodMaterials->first)
                                        / kMaterialStride)
            : 0u;

    for (unsigned int i = 0; i < materials; ++i)
    {
        const unsigned char* const material =
            reinterpret_cast<const unsigned char*>(m_lodMaterials->first) + kMaterialStride * i;
        const bool materialIsAlpha = material[0x18A] != 0;
        if (materialIsAlpha != alphaPass)
            continue;

        shaderParameterSetMatrixArray(state->geomBones, m_bonePalettes[i], m_boneCounts[i]);
        if (materialIsAlpha)
        {
            shaderParameterSetValue(state->texLayer0,
                                    **reinterpret_cast<void* const* const*>(material + 8), 0);
        }

        const unsigned int primitives = *reinterpret_cast<const unsigned int*>(material + 0x20);
        dx9rend::rendererDrawIndexedPrimitive(
            primitives,
            t->m_primitiveType,                                          // template +0xAC
            *reinterpret_cast<const int*>(material + 0x18),
            0,
            *reinterpret_cast<const unsigned int*>(material + 0x24),
            *reinterpret_cast<const unsigned int*>(material + 0x1C));

        skinnedMeshCountShadowPrimitives(primitives);                    // renderer stats +0x23C
    }

    if (currentLod != selected)
    {
        setLodIndex(-1);
        --m_paletteFrameStamp;   // forces the next updateBonePalettes to rebuild. Shipped.
    }
}


// Builds material shader-permutation flags, including this frame's shadow-caster state.

unsigned int SkinnedMesh::buildShaderDefineMask(const unsigned char* material, bool alphaPass,
                                                unsigned int quality)
{
    unsigned int mask = 0;
    if (material[0x188])
        mask = 2;
    if (material[0x189])
        mask |= 4u;
    if (material[0x18B])
        mask |= 8u;
    mask |= 0x10u;

    if (!alphaPass)
    {
        if (refreshAndGetByte334())                                       // +0xA8
        {
            if (reinterpret_cast<unsigned int>(getShadowCasterFrame())    // +0xB0
                == dx9rend::rendererFrameOrdinal())
                mask |= 0x40u;
        }
    }

    // The quality refinement is gated on the 0x40 bit just set, so it can only ever apply to a
    // caster; and on the Renderer's +0xAC predicate being false.
    typedef unsigned char(__fastcall * PredFn)(void*, void*);
    void* const renderer = dx9rend::g_Renderer;
    if (quality >= 3
        && !reinterpret_cast<PredFn>(
               (*reinterpret_cast<void***>(renderer))[0xAC / sizeof(void*)])(renderer, 0)
        && (mask & 0x40u) != 0)
        mask |= 0x100u;

    if (alphaPass)
        mask |= 0x80u;

    return mask;
}


// [FRONTIER] Terrain-height probes require the shared scratch bank, probe interface,
// and LightManager inputs. This entry stops until those dependencies are implemented.

void SkinnedMesh::updateHeightOverTerrain()
{
    // These three guards are shipped. The first currently cannot pass because the constructor's
    // HeightmapCluster singleton lookup remains omitted, not because retail always stores zero.
    // Recovering that separate lookup may expose the named probe frontier below (RW-204).
    if (!m_heightmapCluster)                                  // +0x54
        return;
    if (!m_ownerObject)                                       // +0x330
        return;
    if (!m_template->m_loaded)                                // the template's +0x4C
        return;

    RD9_FRONTIER("dice::hfe::geom::SkinnedMesh::updateHeightOverTerrain body (PE 0x101A6A20, READ - "
                 "three ray probes through objectManager (ds:10244AD8) vslot +0x68 "
                 "against a lazily-zeroed scratch bank behind five bits of ds:10264C44)", 0);
}


// Instance rendering; remaining dependency boundaries retain named stops.
// [FRONTIER] [TEMP] [CHOSEN, not recovered] [RETIRE-WHEN: skinnedmesh-instance]
void SkinnedMesh::draw(void*, void*, void*)
{
    // -- 1. the entry gate
    // both halves are shipped and both matter: a renderer flag AND the Ra block's Transparency
    // holder. Below 0.00001f the whole body is skipped, so a fully transparent soldier costs
    // nothing; below 1.0f he is merely flagged translucent and takes a different queue later.
    if (!rendererFlagAt(0x6A3))
        return;
    const float transparency = *reinterpret_cast<const float*>(m_objectParameters + (0x510 - 0x360));
    if (transparency < 0.0000099999997f)
        return;

    updateHeightOverTerrain();

    // -- 2. the view, chosen by the FOV override
    // The same test updateLodSelection makes, and the reason +0x340's -1.0f sentinel exists.
    void* const base = dx9rend::g_rendererBase;
    void* view;
    if (m_fov == -1.0f)
        view = dx9rend::rendererGetRenderViewDup();                   // base vslot +0x50
    else
        view = viewSlot<void*>(base, 0xD4);                           // the FOV-override view

    // Keep the virtual lookup even though its result is discarded.
    (void)getGeometryTransformation();

    // -- 3. distance to the camera
    const Mat4* const camera = viewSlot<const Mat4*>(view, 0x64);
    const float dx = camera->m[12] - m_transform->m[12];
    const float dy = camera->m[13] - m_transform->m[13];
    const float dz = camera->m[14] - m_transform->m[14];
    const float distance = static_cast<float>(std::sqrt(dx * dx + dy * dy + dz * dz));
    m_distanceToCamera = distance;                                    // +0x344
    // The same value is also written into the Ra block at +0x580, where the shader reads it.
    *reinterpret_cast<float*>(m_objectParameters + (0x580 - 0x360)) = distance;

    updateLodSelection();

    // The "cheap lighting" flag: past the light manager's threshold, or the renderer is in mode 2.
    *reinterpret_cast<unsigned char*>(&m_dword348) =
        (g_LightManager->getSkinnedMeshShaderLodDistance() <= m_distanceToCamera
         || skinnedMeshRendererMode() == 2) ? 1u : 0u;

    // -- 4. the matrices
    // fullMult, NOT operator* and NOT Mat4::mult - the affine shortcut would destroy the
    // projection, which this tree has already been bitten by once (see the SkyDome note).
    m_matrixA0.fullMult(*m_transform, *viewSlot<const Mat4*>(view, 0x68));
    m_matrix120.fullMult(m_matrixA0, *viewSlot<const Mat4*>(view, 0x6C));
    // sub_10078270 is fullINVERT, not a transpose. StaticMesh.cpp names it; an earlier reading
    // of this body guessed transpose and was wrong.
    fullInvert(m_matrixA0, m_matrixE0);
    fullInvert(*m_transform, m_matrix60);

    updateBonePalettes();

    // -- 5. the no-shadow branch, which returns through the common tail
    if (!rendererFlagAt(0x6AB))
    {
        // Clear retained lights before gathering this frame's list.
        lightVectorResizeToEmpty(m_lights);                           // sub_100E1C20(0, 0)

        // The maximum-light query takes no arguments; 0x1B09 is the gather's required mask.
        const int maxLights = g_LightManager->getMaxLightsPerSkinnedMesh();    // +0x70
        g_LightManager->gatherLightsForGeometry(this, m_lights,
                                                static_cast<unsigned int>(maxLights),
                                                0x1B09u, 0u, true);           // +0x20
        const unsigned int gathered =
            m_lights.first
                ? static_cast<unsigned int>((m_lights.last - m_lights.first) >> 2) : 0u;

        if (gathered || skinnedMeshRendererMode() == 2)
        {
            if (m_lodRecord[128] && skinnedMeshRendererMode() != 2)
                skinnedMeshRendererSubmitA(this);
            skinnedMeshRendererSubmitB(this);
        }
        else
        {
            skinnedMeshRendererSubmitA(this);
        }

        m_dword20 = dx9rend::rendererFrameOrdinal();                  // the common tail
        return;
    }

    // -- 6. the lit path: the hemi / ambient solve
    const bool translucent = transparency < 1.0f;
    float hemiFactor   = 1.0f;
    float ambientScale = 1.0f;
    float easeTarget   = 1.0f;

    if (viewSlot<unsigned char>(base, 0xAC))                      // IRenderer::isExpansionPack
    {
        // The expansion arm skips the pow below entirely - hemiFactor stays 1.0f. That is a
        // `goto` past LABEL_20 in the shipped body, not an oversight here.
        easeTarget = g_LightManager->sampleSimpleShadowMapIO(m_transform->m[12], m_transform->m[14]);
        if (m_byte50)
            ambientScale = 0.0f;
    }
    else
    {
        const float x = m_transform->m[12];
        const float z = m_transform->m[14];
        const float hemiA = g_LightManager->sampleSimpleShadowMap(x, z);
        const bool  below = g_LightManager->sampleSimpleShadowMapIO(x, z) < 0.5f;

        float factor;
        if (m_byte50)
        {
            factor = below ? hemiA : 1.0f;
        }
        else if (!below)
        {
            const Vec2& range = g_LightManager->getShadowSunHeightOverFade();
            const float height = *reinterpret_cast<const float*>(&m_dword4C);
            float u = (height - range.x) / (range.y - range.x);
            // THE clamp'S result IS discarded. clampFloat returns the clamped value and the
            // shipped body ignores it, using the unclamped `u` on the next line. Reproduced: this
            // is a real defect in retail, and agents.md says preserve it rather than fix it.
            (void)clampFloat(u, 0.0f, 1.0f);
            factor = u + (1.0f - u) * hemiA;
        }
        else
        {
            factor = 1.0f;
        }
        hemiFactor = static_cast<float>(std::pow(static_cast<double>(factor),
                                                 static_cast<double>(g_LightManager->getShadowSunScalePower())));
    }

    // -- 7. ease +0x354 toward the target, over real time
    // The elapsed time is kept as a double at +0x358 and the divisor is a double at the light
    // manager's +0x838. Narrowing either to float changes the easing on long sessions.
    const double now     = rendererTimeSeconds();
    const double elapsed = now - m_double358;
    m_double358 = now;
    float t = static_cast<float>(elapsed / lightManagerFadeSeconds());
    if (t > 1.0f)
        t = 1.0f;
    m_float354 += t * (easeTarget - m_float354);

    // Clear gathered lights while retaining capacity for the next frame.
    lightVectorResizeToEmpty(m_lights);                               // sub_100E1C20(0, 0)

    // For the local player's vehicle root, the camera decides whether inside dynamic
    // lighting is allowed; other roots accept lights.
    bool acceptLights = true;
    {
        world::IPlayer* const owner = skinnedMeshLightOwner();    // ds:10244AE0 +0x30
        if (owner)
        {
            world::IObject* const candidate = owner->getVehicle();                  // PE +0x44
            if (candidate && staticMeshRootParent(candidate) == refreshRootObject())
            {
                world::ICameraObject* const gate = owner->getCamera();              // PE +0x17C
                if (gate)
                    acceptLights = gate->getAllowDynamicLights();                   // +0x5C
            }
        }
    }

    // -- 9. gather this frame's lights
    if (skinnedMeshLightDebugMode())                              // ds:10264C48
    {
        if (skinnedMeshLightDebugMode() == 1)
            skinnedMeshDebugLightList(this, &m_lights);
    }
    else
    {
        // The same argument-gluing trap as the no-shadow branch: +0x70 takes nothing, and the
        // constant below is gatherLights' `required` mask.
        unsigned int required = 0x8B01u;
        if (m_float354 == 0.0f || !viewSlot<unsigned char>(base, 0xAC))
            required = 0x8B09u;

        if (acceptLights && !translucent)
        {
            const int maxLights = g_LightManager->getMaxLightsPerSkinnedMesh();     // +0x70
            g_LightManager->gatherLightsForGeometry(this, m_lights,
                                                    static_cast<unsigned int>(maxLights),
                                                    required, 0u, true);            // +0x20
        }
    }

    // -- 10. build one 68-byte record per gathered light
    const unsigned int lightCount =
        m_lights.first ? static_cast<unsigned int>((m_lights.last - m_lights.first) >> 2) : 0u;
    skinnedMeshReserveLightRecords(this, lightCount);

    for (unsigned int i = 0; i < lightCount; ++i)
    {
        void* const light = reinterpret_cast<void**>(m_lights.first)[i];
        unsigned char* const record = reinterpret_cast<unsigned char*>(m_lightRecords.first)
                                      + kLightRecordStride * i;

        float range = lightSourceOf(light)->getAttenuationRange2();                // +0x190
        const world::LightSourceTemplate* const lightTemplate = lightSourceTemplate(light);  // +0x144
        const bool isDynamic = lightTemplate && lightTemplateIsDynamic(lightTemplate);      // +0xF0
        if (isDynamic)
            range = g_LightManager->getDynamicLightRangeScale() * range;           // +0x2E0

        // THE receiver IS m_matrix60, which IS fullInvert(*m_transform) - so the light's world
        // position lands in object space. Settled from the instruction stream (`lea ecx, [esi+60h]`
        // at 0x101A7F3D), not from the decompiler, which dropped the receiver entirely.
        mat4TransformPoint(m_matrix60, &lightSourceOf(light)->getAbsolutePosition().x,   // +0x68
                           reinterpret_cast<float*>(record));
        *reinterpret_cast<float*>(record + 0x38) = 1.0f / (range * range);

        const float* const color = &lightSourceOf(light)->getColor().x;             // +0x198
        float r = color[0], g = color[1], b = color[2];
        // The template's isDynamic is asked a second time here, the field re-read from the light
        // (`mov edi,[edi+144h]` at 0x101A7F6B) and not null-tested, rather than reusing the answer
        // above; the scaling is skipped when it holds. Shipped; not hoisted.
        if (!lightTemplateIsDynamic(lightSourceTemplate(light)))
        {
            r *= m_float354;
            g *= m_float354;
            b *= m_float354;
        }
        *reinterpret_cast<float*>(record + 0x18) = r;
        *reinterpret_cast<float*>(record + 0x1C) = g;
        *reinterpret_cast<float*>(record + 0x20) = b;
    }

    // Fill the per-object shader parameters.
    unsigned char* const ra = m_objectParameters;

    // +0x3F4 = world x viewProjection -> WorldViewProjection.
    reinterpret_cast<Mat4*>(ra + (0x3F4 - 0x360))
        ->fullMult(*m_transform, *viewSlot<const Mat4*>(view, 0x84));
    // +0x36C = the transform verbatim -> World. The same store StaticMesh.cpp makes.
    std::memcpy(ra + (0x36C - 0x360), m_transform->m, sizeof(Mat4));
    // +0x518 = the camera's translation through m_matrix60 (= fullInvert(*m_transform)), w = 1
    // -> ObjectSpaceCamPos. Settled from `lea ecx, [esi+60h]` at 0x101A801B, not from the
    // decompiler, which rendered the receiver as an operand.
    mat4TransformPointVec4(m_matrix60, &viewSlot<const Mat4*>(view, 0x64)->m[12],
                           ra + (0x518 - 0x360), 0x3F800000u);
    // ObjectSpaceWorldUp retains multiplication by zero to preserve NaN/infinity behavior.
    const float* const inv = m_matrix60.m;
    *reinterpret_cast<float*>(ra + (0x574 - 0x360)) = (inv[8] + inv[0]) * 0.0f + inv[4];
    *reinterpret_cast<float*>(ra + (0x578 - 0x360)) = (inv[9] + inv[1]) * 0.0f + inv[5];
    *reinterpret_cast<float*>(ra + (0x57C - 0x360)) = (inv[10] + inv[2]) * 0.0f + inv[6];

    const unsigned int materials =
        m_lodMaterials && m_lodMaterials->first
            ? static_cast<unsigned int>((m_lodMaterials->last - m_lodMaterials->first)
                                        / kMaterialStride)
            : 0u;

    // A FOV override marks first-person geometry; otherwise compare roots with the local player.
    bool firstPerson = true;
    if (m_fov == -1.0f)
    {
        firstPerson = false;
        // ; getVehicle is asked twice, as shipped. The pair on the
        // player-control object: +0x144 getNeverDrawAs1p, +0x140 isChildSoldierActive.
        world::IPlayer* const owner = skinnedMeshLightOwner();        // ds:10244AE0 +0x30
        if (owner && owner->hasFirstPersonView())                     // PE +0x184
        {
            world::IPlayerControlObject* const local = static_cast<world::IPlayerControlObject*>(
                skinnedMeshObjectOf(owner->getVehicle()));            // PE +0x44
            if (local && !local->getNeverDrawAs1p()                   // +0x144
                && !local->isChildSoldierActive())                    // +0x140
            {
                world::IPlayerControlObject* const mine =
                    static_cast<world::IPlayerControlObject*>(skinnedMeshObjectOf(m_ownerObject));
                if (m_ownerObject && mine && !mine->getNeverDrawAs1p())
                {
                    firstPerson = staticMeshRootParent(owner->getVehicle())
                                  == staticMeshRootParent(m_ownerObject);
                }
            }
        }
    }

    // +0x558 = the height probe's first output -> HeightOverTerrain.
    *reinterpret_cast<unsigned int*>(ra + (0x558 - 0x360)) = m_dword48;

    // The shader quality, from IRenderer::getVideoOptions()'s +0x04 with 4.
    void* const videoOptions = viewSlot<void*>(base, 0x74);
    typedef unsigned int(__fastcall * OptionFn)(void*, void*, int);
    const unsigned int quality = reinterpret_cast<OptionFn>(
        (*reinterpret_cast<void***>(videoOptions))[0x04 / sizeof(void*)])(videoOptions, 0, 4);

    // The sky light's direction, carried into object space.
    float skyDirection[3] = { 0.0f, 0.0f, 0.0f };
    if (world::LightSource* const sky = g_LightManager->getSkyLight())         // +0x28
    {
        const float* const d = &sky->getDirection().x;                          // +0x17C
        float v[3] = { d[0], d[1], d[2] };
        if (rendererFlagAt(0x69C))
        {
            // The camera comes from the renderer's own view here (+0x48 then +0x64), not from
            // the `view` this draw picked, and the object position comes through the vtable
            // (+0x48) rather than m_transform directly. Both are shipped; neither is shortcut.
            const Mat4* const cam = viewSlot<const Mat4*>(viewSlot<void*>(base, 0x48), 0x64);
            const Mat4* const here = getGeometryTransformation();
            v[0] = here->m[12] - cam->m[12];
            v[1] = here->m[13] - cam->m[13];
            v[2] = here->m[14] - cam->m[14];
            mat4Normalize(v);
        }
        mat4TransformVector(m_matrix60, v, skyDirection);
    }

    // -- 13. one queue entry per material, plus one more per gathered light
    for (unsigned int i = 0; i < materials; ++i)
    {
        unsigned char* const material =
            reinterpret_cast<unsigned char*>(m_lodMaterials->first) + kMaterialStride * i;

        const unsigned int mask = buildShaderDefineMask(material, false, quality);

        // Only the alternate path clamps this index, and it clamps to the LOD index.
        unsigned int index = *reinterpret_cast<const unsigned int*>(
            reinterpret_cast<const unsigned char*>(dx9rend::g_Renderer) + 0x6CC);
        if (rendererFlagAt(0x6D0))
        {
            const unsigned int available = shaderMaskTableSize();
            index = m_lodIndex;
            if (available < index)
                index = available;
        }
        unsigned int bits = shaderMaskTable()[index] & mask;
        if ((bits & 0x1Bu) == 0)
            bits |= 1u;
        *reinterpret_cast<void**>(material + 0x184) = findShaderForBits(bits);

        // The shadow matrices, copied out of the +0x33C object - but only while this instance is a
        // shadow caster on this frame, the same +0xA8/+0xB0 pair buildShaderDefineMask consults.
        if (refreshAndGetByte334()
            && reinterpret_cast<unsigned int>(getShadowCasterFrame())
                   == dx9rend::rendererFrameOrdinal())
        {
            if (m_dword33C)
            {
                const unsigned char* const caster = static_cast<const unsigned char*>(m_dword33C);
                std::memcpy(ra + (0x438 - 0x360), caster + 0x2F8, sizeof(Mat4));
                std::memcpy(ra + (0x4C0 - 0x360), caster + 0x3B8, sizeof(Mat4));
                std::memcpy(ra + (0x47C - 0x360), caster + 0x338, sizeof(Mat4));
            }
        }

        // +0x560 = the sky colour -> HemiMapSkyColor. The ambient scale is applied only on the
        // expansion-pack path; the other arm copies the colour through untouched.
        const Vec3& skyColor = g_LightManager->getSkyColor();                  // +0x30
        if (viewSlot<unsigned char>(base, 0xAC))
        {
            *reinterpret_cast<float*>(ra + (0x560 - 0x360)) = ambientScale * skyColor.x;
            *reinterpret_cast<float*>(ra + (0x564 - 0x360)) = ambientScale * skyColor.y;
            *reinterpret_cast<float*>(ra + (0x568 - 0x360)) = ambientScale * skyColor.z;
        }
        else
        {
            *reinterpret_cast<float*>(ra + (0x560 - 0x360)) = skyColor.x;
            *reinterpret_cast<float*>(ra + (0x564 - 0x360)) = skyColor.y;
            *reinterpret_cast<float*>(ra + (0x568 - 0x360)) = skyColor.z;
        }

        dx9rend::RenderQueueEntry* const entry = dx9rend::renderEntryPoolAcquire();
        entry->material           = material;
        entry->materialParameters = material + 0x38;
        entry->objectParameters   = ra;
        *reinterpret_cast<void**>(reinterpret_cast<unsigned char*>(entry) + 0x138) =
            *reinterpret_cast<void**>(material + 0x184);
        // +0x4C / +0x50 - this material's bone palette and its count. This is where the skinning
        // reaches the shader, and it is why the palettes are per-material rather than per-instance.
        *reinterpret_cast<void**>(reinterpret_cast<unsigned char*>(entry) + 0x4C) =
            &m_bonePalettes[i][0];
        *reinterpret_cast<unsigned int*>(reinterpret_cast<unsigned char*>(entry) + 0x50) =
            m_boneCounts[i];

        unsigned char* const params =
            *reinterpret_cast<unsigned char**>(reinterpret_cast<unsigned char*>(entry) + 0x128);
        {
            const Vec3& sun = g_LightManager->getSunColor();                   // +0x1AC
            float* const dst = reinterpret_cast<float*>(params + 24);
            dst[0] = hemiFactor * sun.x; dst[1] = hemiFactor * sun.y; dst[2] = hemiFactor * sun.z;
        }
        {
            const Vec3& spec = g_LightManager->getSunSpecColor();              // +0x1B4
            float* const dst = reinterpret_cast<float*>(params + 40);
            dst[0] = hemiFactor * spec.x; dst[1] = hemiFactor * spec.y;
            dst[2] = hemiFactor * spec.z;
        }
        if (world::LightSource* const sky = g_LightManager->getSkyLight())     // +0x28
        {
            float* const dir = reinterpret_cast<float*>(params + 12);
            dir[0] = skyDirection[0]; dir[1] = skyDirection[1]; dir[2] = skyDirection[2];
            // A sky light overrides the previously stored sun color.
            const float* const c = &sky->getColor().x;                          // +0x198
            float* const dst = reinterpret_cast<float*>(params + 24);
            dst[0] = hemiFactor * c[0]; dst[1] = hemiFactor * c[1]; dst[2] = hemiFactor * c[2];
        }

        dx9rend::renderQueueEnqueue(entry, translucent, firstPerson, false);

        // One extra entry per light record, sharing the material but carrying that light's data
        // and a re-based define mask.
        const unsigned int records =
            m_lightRecords.first
                ? static_cast<unsigned int>((m_lightRecords.last - m_lightRecords.first)
                                            / kLightRecordStride)
                : 0u;
        for (unsigned int l = 0; l < records; ++l)
        {
            dx9rend::RenderQueueEntry* const lit = dx9rend::renderEntryPoolAcquire();
            lit->material           = material;
            lit->materialParameters = material + 0x38;
            lit->objectParameters   = ra;

            // THE RE-base IS TWO steps AND THE first ONE IS dead: `bits & 0xFFFFFCBF` is computed
            // and then its low byte is overwritten by `(bits & 0x3F) | 0x80`. Reproduced as the
            // shipped pair rather than folded to the value it actually yields.
            unsigned int rebased = bits & 0xFFFFFCBFu;
            rebased = (rebased & 0xFFFFFF00u) | ((bits & 0x3Fu) | 0x80u);
            *reinterpret_cast<void**>(reinterpret_cast<unsigned char*>(lit) + 0x138) =
                findShaderForBits(rebased);

            unsigned char* const litParams =
                *reinterpret_cast<unsigned char**>(reinterpret_cast<unsigned char*>(lit) + 0x128);
            std::memcpy(litParams, reinterpret_cast<unsigned char*>(m_lightRecords.first)
                                       + kLightRecordStride * l, kLightRecordStride);

            // both operands are the entry + 8: `lea ecx,[edi+8]` / `lea ecx,[ebx+8]` at
            // The payload starts one dword past the entry's own head.
            renderQueueEntryCopyPayload(reinterpret_cast<unsigned char*>(lit) + 8,
                                        reinterpret_cast<unsigned char*>(entry) + 8);
            dx9rend::renderQueueEnqueue(lit, translucent, m_fov != -1.0f, true);
        }
    }

    m_dword20 = dx9rend::rendererFrameOrdinal();
}


// Unregisters the instance, frees bone palettes and owned vectors, then destroys the name.
// The template must still exist. Palette frees are unconditional and do not clear the slots.
// This flat representation omits the original base-vtable restoration.

SkinnedMesh::~SkinnedMesh()
{
    // m_template is dereferenced without a null test, exactly as shipped.
    instanceSetErase(m_template->m_instances, this);

    for (int palette = 0; palette < kSkinnedMeshPalettes; ++palette)
        for (int bone = 0; bone < kSkinnedMeshBonesPerPalette; ++bone)
            ::renddx9abi::engineFree(m_bonePalettes[palette][bone]);   // sub_1005F360, unguarded

    if (m_lightRecords.first)
        ::renddx9abi::engineFree(m_lightRecords.first);
    m_lightRecords.first = 0;
    m_lightRecords.last  = 0;
    m_lightRecords.end   = 0;

    ::dice::hfe::dx9rend::Ra::raObjectParametersDestroy(m_objectParameters);

    if (m_lights.first)
        ::renddx9abi::engineFree(m_lights.first);
    m_lights.first = 0;
    m_lights.last  = 0;
    m_lights.end   = 0;
}


// Primary-table thunks use callee cleanup; argument counts are part of the ABI.
// Shared field-writing slots require the same name and frame-stamp offsets.

namespace {

int   __fastcall skm_addRef(void* self, void*)  { return static_cast<SkinnedMesh*>(self)->addRef(); }
int   __fastcall skm_getRef(void* self, void*)  { return static_cast<SkinnedMesh*>(self)->getRef(); }
int   __fastcall skm_release(void* self, void*) { return static_cast<SkinnedMesh*>(self)->release(); }
void* __fastcall skm_queryInterface(void* self, void*, unsigned int iid)
{ return static_cast<SkinnedMesh*>(self)->queryInterface(iid); }

void __fastcall skm_setFov(void* self, void*, float fov)
{ return static_cast<SkinnedMesh*>(self)->setFov(fov); }
float __fastcall skm_getFov(void* self, void*)
{ return static_cast<const SkinnedMesh*>(self)->getFov(); }

void __fastcall skm_draw(void* self, void*, void* a, void* b, void* c)
{ static_cast<SkinnedMesh*>(self)->draw(a, b, c); }
void __fastcall skm_drawShadowMap(void* self, void*, void* caster, unsigned int a,
                                  unsigned int b, unsigned int c, bool alphaPass, unsigned int d)
{ static_cast<SkinnedMesh*>(self)->drawShadowMap(caster, a, b, c, alphaPass, d); }

void* __fastcall skm_setDword33C(void* self, void*, void* v)
{ return static_cast<SkinnedMesh*>(self)->setDword33C(v); }
void* __fastcall skm_getDword33C(void* self, void*)
{ return static_cast<const SkinnedMesh*>(self)->getDword33C(); }

// +0x34 - the MSVC scalar deleting destructor: run the real one, then free when bit 0 is set.
void* __fastcall skm_scalarDeletingDtor(void* self, void*, unsigned int flags)
{
    SkinnedMesh* const mesh = static_cast<SkinnedMesh*>(self);
    mesh->~SkinnedMesh();
    if (flags & 1u)
        ::renddx9abi::engineFree(mesh);
    return self;
}

void* __fastcall skm_getTemplate(void* self, void*)
{ return static_cast<const SkinnedMesh*>(self)->getTemplate(); }
const void* __fastcall skm_getLocalBounds(void* self, void*)
{ return static_cast<const SkinnedMesh*>(self)->getLocalBounds(); }
float __fastcall skm_getBoundingRadius(void* self, void*)
{ return static_cast<const SkinnedMesh*>(self)->getBoundingRadius(); }
const Mat4* __fastcall skm_getGeometryTransformation(void* self, void*)
{ return static_cast<const SkinnedMesh*>(self)->getGeometryTransformation(); }
void __fastcall skm_setGeometryTransformation(void* self, void*, const Mat4* m)
{ static_cast<SkinnedMesh*>(self)->setGeometryTransformation(m); }

void __fastcall skm_setBoneTransforms(void* self, void*, const Mat4* bones, unsigned int count)
{ static_cast<SkinnedMesh*>(self)->setBoneTransforms(bones, count); }
void __fastcall skm_gatherVisibleParts(void* self, void*, HostIntVector* out)
{ static_cast<SkinnedMesh*>(self)->gatherVisibleParts(out); }
void __fastcall skm_setExcludedPart(void* self, void*, int part, bool enable)
{ static_cast<SkinnedMesh*>(self)->setExcludedPart(part, enable); }

void  __fastcall skm_setGeomIndex(void* self, void*, int g)
{ static_cast<SkinnedMesh*>(self)->setGeomIndex(g); }
int   __fastcall skm_getGeomIndex(void* self, void*)
{ return static_cast<const SkinnedMesh*>(self)->getGeomIndex(); }
int   __fastcall skm_getGeomCount(void* self, void*)
{ return static_cast<const SkinnedMesh*>(self)->getGeomCount(); }
void* __fastcall skm_setLodIndex(void* self, void*, int lod)
{ return static_cast<SkinnedMesh*>(self)->setLodIndex(lod); }
int   __fastcall skm_getLodIndex(void* self, void*)
{ return static_cast<const SkinnedMesh*>(self)->getLodIndex(); }
int   __fastcall skm_getLodCount(void* self, void*)
{ return static_cast<const SkinnedMesh*>(self)->getLodCount(); }

void  __fastcall skm_setOwnerObject(void* self, void*, void* o)
{ static_cast<SkinnedMesh*>(self)->setOwnerObject(o); }
void* __fastcall skm_getOwnerObject(void* self, void*)
{ return static_cast<const SkinnedMesh*>(self)->getOwnerObject(); }
void* __fastcall skm_refreshRootObject(void* self, void*)
{ return static_cast<SkinnedMesh*>(self)->refreshRootObject(); }
unsigned char __fastcall skm_refreshAndGetByte334(void* self, void*)
{ return static_cast<SkinnedMesh*>(self)->refreshAndGetByte334(); }
void  __fastcall skm_setShadowCasterFrame(void* self, void*, void* f)
{ static_cast<SkinnedMesh*>(self)->setShadowCasterFrame(f); }
void* __fastcall skm_getShadowCasterFrame(void* self, void*)
{ return static_cast<SkinnedMesh*>(self)->getShadowCasterFrame(); }
void  __fastcall skm_setTransparency(void* self, void*, float a)
{ static_cast<SkinnedMesh*>(self)->setTransparency(a); }

// the shared bodies, each read
void __fastcall skm_empty1(void*, void*, unsigned int) {}
void __fastcall skm_empty2(void*, void*, unsigned int, unsigned int) {}
void __fastcall skm_empty3(void*, void*, unsigned int, unsigned int,
                           unsigned int) {}
unsigned int __fastcall skm_zero0(void*, void*) { return 0; }
unsigned int __fastcall skm_zero1(void*, void*, unsigned int) { return 0; }
bool __fastcall skm_false0(void*, void*) { return false; }
bool __fastcall skm_false1(void*, void*, unsigned int) { return false; }
int  __fastcall skm_minusOne(void*, void*) { return -1; }
int  __fastcall skm_seven(void*, void*) { return 7; }
int  __fastcall skm_three(void*, void*) { return 3; }

// 0x10082D30 - `m_name = value` through MSVCP71's string operator=. Writes +0x04.
void __fastcall skm_setName(void* self, void*, const vc71::string& name)
{ static_cast<SkinnedMesh*>(self)->m_name = name; }

// 0x10103AD0 - `[ecx+24h] = rendererFrameOrdinal()`. Writes +0x24, valid in both classes.
void __fastcall skm_stampFrameOrdinal(void* self, void*)
{ static_cast<SkinnedMesh*>(self)->m_dword24 = dx9rend::rendererFrameOrdinal(); }

} // anonymous namespace

void* const g_skinnedMeshVtable[62] =
{
    reinterpret_cast<void*>(&skm_addRef),                      // +0x00
    reinterpret_cast<void*>(&skm_getRef),                      // +0x04
    reinterpret_cast<void*>(&skm_release),                     // +0x08
    reinterpret_cast<void*>(&skm_queryInterface),              // +0x0C  0x101A89E0  OWN
    reinterpret_cast<void*>(&skm_setFov),                      // +0x10  0x101A8A90  OWN
    reinterpret_cast<void*>(&skm_getFov),                      // +0x14  0x101A8AA0  OWN
    reinterpret_cast<void*>(&skm_draw),                        // +0x18  0x101A7AF0  OWN
    reinterpret_cast<void*>(&skm_empty2),                      // +0x1C
    reinterpret_cast<void*>(&skm_drawShadowMap),               // +0x20  0x101A86B0  OWN
    reinterpret_cast<void*>(&skm_setDword33C),                 // +0x24  0x101A8A10  OWN
    reinterpret_cast<void*>(&skm_getDword33C),                 // +0x28  0x101A8A20  OWN
    reinterpret_cast<void*>(&skm_three),                       // +0x2C
    reinterpret_cast<void*>(&skm_false1),                      // +0x30
    reinterpret_cast<void*>(&skm_scalarDeletingDtor),          // +0x34  0x101A8AC0  OWN
    reinterpret_cast<void*>(&skm_getTemplate),                 // +0x38  0x101A8A30  OWN
    reinterpret_cast<void*>(&skm_zero1),                       // +0x3C
    reinterpret_cast<void*>(&skm_getLocalBounds),              // +0x40  0x101A8A40  OWN
    reinterpret_cast<void*>(&skm_getBoundingRadius),           // +0x44  0x101A68E0  OWN
    reinterpret_cast<void*>(&skm_getGeometryTransformation),   // +0x48  0x101A67C0  OWN
    reinterpret_cast<void*>(&skm_empty2),                      // +0x4C
    reinterpret_cast<void*>(&skm_setGeometryTransformation),   // +0x50  0x101A67D0  OWN
    reinterpret_cast<void*>(&skm_setBoneTransforms),           // +0x54  0x101A8A50  OWN
    reinterpret_cast<void*>(&skm_gatherVisibleParts),          // +0x58  0x101A7570  OWN
    reinterpret_cast<void*>(&skm_zero0),                       // +0x5C
    reinterpret_cast<void*>(&skm_setExcludedPart),             // +0x60  0x101A67E0  OWN
    reinterpret_cast<void*>(&skm_empty1),                      // +0x64
    reinterpret_cast<void*>(&skm_setName),                     // +0x68
    reinterpret_cast<void*>(&skm_setGeomIndex),                // +0x6C  0x101A7010  OWN
    reinterpret_cast<void*>(&skm_getGeomIndex),                // +0x70  0x101A6810  OWN
    reinterpret_cast<void*>(&skm_getGeomCount),                // +0x74  0x101A6CE0  OWN
    reinterpret_cast<void*>(&skm_setLodIndex),                 // +0x78  0x101A7060  OWN
    reinterpret_cast<void*>(&skm_getLodIndex),                 // +0x7C  0x101A8A70  OWN
    reinterpret_cast<void*>(&skm_getLodCount),                 // +0x80  0x101A7140  OWN
    reinterpret_cast<void*>(&skm_empty1),                      // +0x84
    reinterpret_cast<void*>(&skm_minusOne),                    // +0x88
    reinterpret_cast<void*>(&skm_zero0),                       // +0x8C
    reinterpret_cast<void*>(&skm_empty3),                      // +0x90
    reinterpret_cast<void*>(&skm_empty2),                      // +0x94
    reinterpret_cast<void*>(&skm_empty2),                      // +0x98
    reinterpret_cast<void*>(&skm_setOwnerObject),              // +0x9C  0x101A6930  OWN
    reinterpret_cast<void*>(&skm_getOwnerObject),              // +0xA0  0x101A8A80  OWN
    reinterpret_cast<void*>(&skm_refreshRootObject),           // +0xA4  0x101A6940  OWN
    reinterpret_cast<void*>(&skm_refreshAndGetByte334),        // +0xA8  0x101A6980  OWN
    reinterpret_cast<void*>(&skm_setShadowCasterFrame),        // +0xAC  0x101A69A0  OWN
    reinterpret_cast<void*>(&skm_getShadowCasterFrame),        // +0xB0  0x101A69D0  OWN
    reinterpret_cast<void*>(&skm_empty1),                      // +0xB4
    reinterpret_cast<void*>(&skm_empty1),                      // +0xB8
    reinterpret_cast<void*>(&skm_false0),                      // +0xBC
    reinterpret_cast<void*>(&skm_empty2),                      // +0xC0
    reinterpret_cast<void*>(&skm_zero0),                       // +0xC4
    reinterpret_cast<void*>(&skm_zero1),                       // +0xC8
    reinterpret_cast<void*>(&skm_zero0),                       // +0xCC
    reinterpret_cast<void*>(&skm_false0),                      // +0xD0
    reinterpret_cast<void*>(&skm_empty1),                      // +0xD4
    reinterpret_cast<void*>(&skm_false0),                      // +0xD8
    reinterpret_cast<void*>(&skm_empty1),                      // +0xDC
    reinterpret_cast<void*>(&skm_empty1),                      // +0xE0
    reinterpret_cast<void*>(&skm_seven),                       // +0xE4
    reinterpret_cast<void*>(&skm_zero0),                       // +0xE8
    reinterpret_cast<void*>(&skm_zero1),                       // +0xEC
    reinterpret_cast<void*>(&skm_stampFrameOrdinal),           // +0xF0
    reinterpret_cast<void*>(&skm_setTransparency),             // +0xF4  0x101A8AB0  OWN
};

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
