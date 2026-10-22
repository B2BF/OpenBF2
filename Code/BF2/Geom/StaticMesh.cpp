// Static-mesh instance rendering, visibility and shadow state.


#include "StaticMesh.h"

#ifdef BF2_IMAGE_RENDDX9

#include "../../RendDX9Abi/BfMemory.h"        // engineAlloc - the map sentinel
#include "../../RendDX9Abi/D3dxEffect.h"      // D3dxCommitChangesFn - the shadow draw's commit
#include "../../RendDX9Abi/EngineFile.h"      // engineFileExists / readEngineFile
#include "../../RendDX9Abi/Frontier.h"
#include "../../RendDX9Abi/PluginEntry.h"     // classManager - the WindManager lookup
#include "../../RendDX9Abi/Vc71Map.h"         // the int-keyed lightmap-cache node
#include "../Core/Ids.h"                      // IID_IGeometry / IRenderable / ILightSampleable
#include "../Game/Main/IVideoOptions.h"
#include "../Game/Objects/IObject.h"          // the owner's getIsOvergrowth
#include "../Game/Objects/ObjectLayoutAccess.h"   // staticMeshRootParent's IObject prefix fields
#include "../Game/Objects/Visual/LightSource.h"
#include "../Math/Frustum.h"
#include "../Math/Plane3.h"
#include "../Math/Vec3.h"
#include "../RendDX9/Ra/RaShader.h"           // raObjectParametersConstruct, RenderQueueEntry
#include "../RendDX9/RendDX9Base.h"
#include "../RendDX9/Texture.h"               // textureReleaseRef - the lightmap cache
#include "../RendMisc/IRenderView.h"
#include "../RendMisc/IRenderer.h"            // getRenderStats - the shadow draws' +0x25C counter
#include "../Scene/Object/WindManager.h"      // the vegetation arm's wind speed
#include "LightmapGeneration/ILightSampleable.h"  // the +0x30 table's interface (ILightSampling)
#include "LightmapGeneration/MeshLightSampling.h" // the sampler createLightSampler makes
#include "MeshTemplate.h"                     // MeshBounds / meshBoundsFinalize
#include "StaticMeshRenderer.h"               // the singleton's flags, counters and atlases
#include "StaticMeshTemplate.h"               // the tail gate reads MeshTemplate::m_loaded
#include "XpLightManager.h"                   // the light gather and the five colour getters

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

namespace dice {
namespace hfe {

namespace dx9rend {
// RendDX9Init.cpp, dword_1023B970.  Declared per-TU here exactly as DecalManager.cpp and
// HemiMapManager.cpp declare it; +0x54 is getFrameOrdinal, which MeshTemplate.cpp also documents.
extern void* g_rendererBase;

// BF2Render.cpp, dword_1023D098 - the Renderer object.  The draw reads eleven of its fields by
// byte offset, exactly as the shipped body does and exactly as StaticMeshTemplate.cpp's readLods
// reads its lod-skip settings; the Renderer's full layout is not recovered.
extern IBase* g_Renderer;

// 0x10014510 plus the refcount/free wrapper around it. Declared per-TU here exactly as
// MeshTemplate.cpp:67 and StaticMeshTemplate.cpp:89 declare it.
void vertexBufferRelease(void* wrapper);

// BF2Render.cpp - Renderer vtable slot +0xAC (`return *(BYTE*)(this + 1892)`), the draw
// dispatcher's arm test.  Declared per-TU exactly as StaticMeshTemplate.cpp:104 declares it.
bool rendererIsExpansionPack();

// BufferManager.cpp - declared here the way StaticMeshMaterial.cpp declares the same entry points.
extern void* g_bufferManager;                                              // dword_1023B978
void* createVertexBuffer(void* bufferManager, unsigned int count, unsigned int stride,
                         const vc71::string& name, unsigned int usage);
void* vertexBufferLock(void* wrapper, unsigned int offsetVerts, unsigned int countVerts,
                       unsigned int flags);
bool  vertexBufferUnlock(void* wrapper);

// The stream / draw seams the shadow-pass slots use, declared per-TU exactly as SkyDome.cpp and
// SwiffRender.cpp declare them.
void vertexBufferBindStream(void* wrapper, unsigned int stream);
void indexBufferBind(void* wrapper);
void vertexDeclarationBind(void* wrapper);
void rendererDrawIndexedPrimitive(unsigned int primCount, unsigned int type, int baseVertex,
                                  unsigned int minIndex, unsigned int numVertices,
                                  unsigned int startIndex);
void stateManagerPush();
void stateManagerPop();
}

namespace geom {

extern LightManager* g_LightManager;      // XpLightManager.cpp, dword_1024B914
extern IBase*        g_StaticMeshRenderer; // StaticMeshRenderer.cpp, dword_1024B8F8

namespace {


// The primary table has 63 slots; ILightSampleable has seven.
// Explicit tables preserve the ABI while remaining interface signatures are unknown.
void __fastcall sm_drawSilhouette(void* self, void*, void* unused, int legacyArm)
{
    static_cast<StaticMesh*>(self)->drawSilhouette(unused, legacyArm);
}

// +0x20, the shadow-map draw.  `retn 18h`.
void __fastcall sm_drawShadowMap(void* self, void*, void* caster, unsigned int a2,
                                 unsigned int a3, unsigned int a4, unsigned int a5,
                                 unsigned int a6)
{
    static_cast<StaticMesh*>(self)->drawShadowMap(caster, a2, a3, a4, a5, a6);
}

// The draw's expansion-pack arm, sub_100FE130 (0x117F B).  Only reached when
// Renderer::isExpansionPack(), which vanilla BF2 1.50 never answers true; the vanilla arm is
// written in full below.  [RETIRE-WHEN: RW-staticmesh-xpack-draw]
void staticMeshDrawExpansionStop(void* self)
{
    RD9_FRONTIER_AT("dice::hfe::geom::StaticMesh::drawExpansion (sub_100FE130, the expansion-pack "
                    "arm of slot +0x18; unreachable on vanilla 1.50)", self, 0x18);
}

// the written slots, as the thin adapters the tables need

// +0x34, the scalar deleting destructor. Bit 0 of the flag means "and
// free the storage", which is the MSVC convention release() relies on.
void* __fastcall sm_deletingDestructor(void* self, void*, int flags)
{
    static_cast<StaticMesh*>(self)->~StaticMesh();
    if ((flags & 1) != 0)
        ::renddx9abi::engineFree(self);
    return self;
}

int __fastcall sm_addRef(void* self, void*)
{
    return static_cast<StaticMesh*>(self)->addRef();
}

int __fastcall sm_getRef(void* self, void*)
{
    return static_cast<StaticMesh*>(self)->getRef();
}

int __fastcall sm_release(void* self, void*)
{
    return static_cast<StaticMesh*>(self)->release();
}

void* __fastcall sm_queryInterface(void* self, void*, unsigned int iid)
{
    return static_cast<StaticMesh*>(self)->queryInterface(iid);
}

// +0x18, the draw.  `retn 0Ch`: renderView, occluder, flag.
void __fastcall sm_draw(void* self, void*, void* renderView, void* occluder, int flag)
{
    static_cast<StaticMesh*>(self)->draw(renderView, occluder, flag);
}

// +0x14, 0x10082D10 - `fld -1.0; retn`.  Shared with the base family (fifteen constructors
// install the base table this slot comes from).
float __fastcall sm_slot14(void* self, void*)
{
    return static_cast<const StaticMesh*>(self)->slot14ReturnsMinusOne();
}

// +0x30, 0x1005DA00 - `xor al,al; retn 4`.
bool __fastcall sm_slot30(void* self, void*, int argument)
{
    return static_cast<const StaticMesh*>(self)->slot30ReturnsFalse(argument);
}

// +0x3C and +0xC8, 0x10088060 - `xor eax,eax; retn 4`.  One body, two slots, one arity.
int __fastcall sm_slot3C(void* self, void*, int argument)
{
    return static_cast<const StaticMesh*>(self)->slot3CReturnsZero(argument);
}

// +0x44, the bounding radius.
float __fastcall sm_getBoundingRadius(void* self, void*)
{
    return static_cast<StaticMesh*>(self)->getBoundingRadius();
}

// +0x48, the transform, or the shared identity.
const Mat4* __fastcall sm_getGeometryTransformation(void* self, void*)
{
    return static_cast<const StaticMesh*>(self)->getGeometryTransformation();
}

// +0x4C, 0x100FB320 - `retn 8`.
void __fastcall sm_setDynamicPartTransformation(void* self, void*, unsigned int part,
                                                const Mat4* transform)
{
    static_cast<StaticMesh*>(self)->setDynamicPartTransformation(part, transform);
}

void __fastcall sm_setGeometryTransformation(void* self, void*, const Mat4* transform)
{
    static_cast<StaticMesh*>(self)->setGeometryTransformation(transform);
}

// +0x54, +0x60, +0x94, +0x98 - 0x100E1480, `retn 8`: an empty virtual taking two dwords.  The
// one body sits in four slots; its arity is its own, so the four adapters are one function.
void __fastcall sm_emptyTwoArgs(void*, void*, unsigned int, unsigned int)
{
}

// +0x58, +0x64, +0xB4, +0xD4, +0xDC - 0x100E13B0, `retn 4`: an empty virtual taking one dword.
void __fastcall sm_emptyOneArg(void*, void*, unsigned int)
{
}

// +0x5C, +0xA0, +0xCC - 0x100ED3B0, `xor eax,eax; retn`: zero, no arguments.  +0xCC is the
// "next geometry in the chain" slot the object walkers follow; see getNextGeometry.
int __fastcall sm_returnsZero(void*, void*)
{
    return 0;
}

// +0x68, 0x10082D30 - `add ecx, 4; jmp basic_string::operator=(const basic_string&)`: the
// name at +0x04 assigned from the argument.  The argument is a VC7.1 std::string in the
// shipped image; whether the host ever calls this slot (with its own string layout) is not known -
// no recovered host body does - so it is typed as the image types it.
void __fastcall sm_setName(void* self, void*, const vc71::string* value)
{
    static_cast<StaticMesh*>(self)->setName(*value);
}

// +0x84, 0x100FC010 - `retn 4`.
void __fastcall sm_setMaterialOverride(void* self, void*, int index)
{
    static_cast<StaticMesh*>(self)->setMaterialOverride(index);
}

// +0x8C
int __fastcall sm_getMaterialCount(void* self, void*)
{
    return static_cast<const StaticMesh*>(self)->getMaterialCount();
}

// +0x90, 0x10083AB0 - `retn 0Ch`: an empty virtual taking three dwords.
void __fastcall sm_emptyThreeArgs(void*, void*, unsigned int, unsigned int, unsigned int)
{
}

// +0xA8, +0xD0, +0xD8 - 0x10083AF0, `xor al,al; retn`: false, no arguments.
bool __fastcall sm_returnsFalse(void*, void*)
{
    return false;
}

// +0xE8
int __fastcall sm_getOccluderRecordCount(void* self, void*)
{
    return static_cast<const StaticMesh*>(self)->getOccluderRecordCount();
}

// +0xEC, 0x100FC120 - `retn 4`.
const void* __fastcall sm_getOccluderRecord(void* self, void*, unsigned int index)
{
    return static_cast<const StaticMesh*>(self)->getOccluderRecord(index);
}

int __fastcall sm_getNextGeometry(void* self, void*)
{
    return static_cast<StaticMesh*>(self)->getNextGeometry();
}

void __fastcall sm_setDword28C(void* self, void*, void* value)
{
    static_cast<StaticMesh*>(self)->setDword28C(value);
}

void* __fastcall sm_getDword28C(void* self, void*)
{
    return static_cast<StaticMesh*>(self)->getDword28C();
}

void __fastcall sm_setGeom(void* self, void*, int geom)
{
    static_cast<StaticMesh*>(self)->setGeom(geom);
}

void __fastcall sm_setLod(void* self, void*, int lod)
{
    static_cast<StaticMesh*>(self)->setLod(lod);
}

int __fastcall sm_getGeomCount(void* self, void*)
{
    return static_cast<StaticMesh*>(self)->getGeomCount();
}

int __fastcall sm_getLodCount(void* self, void*)
{
    return static_cast<StaticMesh*>(self)->getLodCount();
}

int __fastcall sm_getGeomIndex(void* self, void*)
{
    return static_cast<StaticMesh*>(self)->getGeomIndex();
}

int __fastcall sm_getLodIndex(void* self, void*)
{
    return static_cast<StaticMesh*>(self)->getLodIndex();
}

int __fastcall sm_getDword31C(void* self, void*)
{
    return static_cast<StaticMesh*>(self)->getDword31C();
}

void __fastcall sm_updateFlags(void* self, void*, unsigned int set, unsigned int clear)
{
    static_cast<StaticMesh*>(self)->updateFlags(set, clear);
}

unsigned int __fastcall sm_getFlags(void* self, void*)
{
    return static_cast<StaticMesh*>(self)->getFlags();
}

void __fastcall sm_stampFrameOrdinal(void* self, void*)
{
    static_cast<StaticMesh*>(self)->stampFrameOrdinal();
}

void __fastcall sm_setDword350(void* self, void*, void* value)
{
    static_cast<StaticMesh*>(self)->setDword350(value);
}

void* __fastcall sm_getDword350(void* self, void*)
{
    return static_cast<StaticMesh*>(self)->getDword350();
}

void __fastcall sm_setOwnerObject(void* self, void*, void* object)
{
    static_cast<StaticMesh*>(self)->setOwnerObject(object);
}

void* __fastcall sm_refreshRootObject(void* self, void*)
{
    return static_cast<StaticMesh*>(self)->refreshRootObject();
}

void __fastcall sm_setFade(void* self, void*, float value)
{
    static_cast<StaticMesh*>(self)->setFade(value);
}

// the nine accessors
void* __fastcall sm_getTemplate(void* self, void*)
{
    return static_cast<StaticMesh*>(self)->getTemplate();
}

const void* __fastcall sm_getLocalBounds(void* self, void*)
{
    return static_cast<StaticMesh*>(self)->getLocalBounds();
}

void* __fastcall sm_getRootObject(void* self, void*)
{
    return static_cast<StaticMesh*>(self)->getRootObject();
}

void __fastcall sm_setFloat308(void* self, void*, float value)
{
    static_cast<StaticMesh*>(self)->setFloat308(value);
}

void __fastcall sm_setDword34C(void* self, void*, unsigned int value)
{
    static_cast<StaticMesh*>(self)->setDword34C(value);
}

unsigned int __fastcall sm_getDword34C(void* self, void*)
{
    return static_cast<StaticMesh*>(self)->getDword34C();
}

int __fastcall sm_getGeometryType(void* self, void*)
{
    return static_cast<StaticMesh*>(self)->getGeometryType();
}

// THE truncation IS THE point. The shipped body compares the low byte of the argument
// slot, so 0x100 is false to it and would be true to an `int != 0` test. Taking the raw dword
// here and narrowing it is what keeps that.
void __fastcall sm_setFlagBit1(void* self, void*, int raw)
{
    static_cast<StaticMesh*>(self)->setFlagBit1(static_cast<unsigned char>(raw));
}

bool __fastcall sm_getFlagBit1(void* self, void*)
{
    return static_cast<StaticMesh*>(self)->getFlagBit1();
}

void* const kStaticMeshVtable[63] =
{
    reinterpret_cast<void*>(&sm_addRef),                   // +0x00
    reinterpret_cast<void*>(&sm_getRef),                   // +0x04
    reinterpret_cast<void*>(&sm_release),                  // +0x08
    reinterpret_cast<void*>(&sm_queryInterface),           // +0x0C
    reinterpret_cast<void*>(&sm_setFloat308),              // +0x10
    reinterpret_cast<void*>(&sm_slot14),                   // +0x14  (-1.0f)
    reinterpret_cast<void*>(&sm_draw),                     // +0x18  (vanilla arm)
    reinterpret_cast<void*>(&sm_drawSilhouette),           // +0x1C
    reinterpret_cast<void*>(&sm_drawShadowMap),            // +0x20
    reinterpret_cast<void*>(&sm_setDword34C),              // +0x24
    reinterpret_cast<void*>(&sm_getDword34C),              // +0x28
    reinterpret_cast<void*>(&sm_getGeometryType),          // +0x2C
    reinterpret_cast<void*>(&sm_slot30),                   // +0x30  (false)
    reinterpret_cast<void*>(&sm_deletingDestructor),       // +0x34
    reinterpret_cast<void*>(&sm_getTemplate),              // +0x38
    reinterpret_cast<void*>(&sm_slot3C),                   // +0x3C  (0)
    reinterpret_cast<void*>(&sm_getLocalBounds),           // +0x40
    reinterpret_cast<void*>(&sm_getBoundingRadius),        // +0x44
    reinterpret_cast<void*>(&sm_getGeometryTransformation),// +0x48
    reinterpret_cast<void*>(&sm_setDynamicPartTransformation), // +0x4C
    reinterpret_cast<void*>(&sm_setGeometryTransformation),// +0x50
    reinterpret_cast<void*>(&sm_emptyTwoArgs),             // +0x54  (empty)
    reinterpret_cast<void*>(&sm_emptyOneArg),              // +0x58  (empty)
    reinterpret_cast<void*>(&sm_returnsZero),              // +0x5C  (0)
    reinterpret_cast<void*>(&sm_emptyTwoArgs),             // +0x60  (empty)
    reinterpret_cast<void*>(&sm_emptyOneArg),              // +0x64  (empty)
    reinterpret_cast<void*>(&sm_setName),                  // +0x68
    reinterpret_cast<void*>(&sm_setGeom),                  // +0x6C
    reinterpret_cast<void*>(&sm_getGeomIndex),             // +0x70
    reinterpret_cast<void*>(&sm_getGeomCount),             // +0x74
    reinterpret_cast<void*>(&sm_setLod),                   // +0x78
    reinterpret_cast<void*>(&sm_getLodIndex),              // +0x7C
    reinterpret_cast<void*>(&sm_getLodCount),              // +0x80
    reinterpret_cast<void*>(&sm_setMaterialOverride),      // +0x84
    reinterpret_cast<void*>(&sm_getDword31C),              // +0x88
    reinterpret_cast<void*>(&sm_getMaterialCount),         // +0x8C
    reinterpret_cast<void*>(&sm_emptyThreeArgs),           // +0x90  (empty)
    reinterpret_cast<void*>(&sm_emptyTwoArgs),             // +0x94  (empty)
    reinterpret_cast<void*>(&sm_emptyTwoArgs),             // +0x98  (empty)
    reinterpret_cast<void*>(&sm_setOwnerObject),           // +0x9C
    reinterpret_cast<void*>(&sm_returnsZero),              // +0xA0  (0)
    reinterpret_cast<void*>(&sm_getRootObject),            // +0xA4
    reinterpret_cast<void*>(&sm_returnsFalse),             // +0xA8  (false)
    reinterpret_cast<void*>(&sm_setDword350),              // +0xAC
    reinterpret_cast<void*>(&sm_getDword350),              // +0xB0
    reinterpret_cast<void*>(&sm_emptyOneArg),              // +0xB4  (empty)
    reinterpret_cast<void*>(&sm_setFlagBit1),              // +0xB8
    reinterpret_cast<void*>(&sm_getFlagBit1),              // +0xBC
    reinterpret_cast<void*>(&sm_updateFlags),              // +0xC0
    reinterpret_cast<void*>(&sm_getFlags),                 // +0xC4
    reinterpret_cast<void*>(&sm_slot3C),                   // +0xC8  (0)
    reinterpret_cast<void*>(&sm_getNextGeometry),          // +0xCC
    reinterpret_cast<void*>(&sm_returnsFalse),             // +0xD0  (false)
    reinterpret_cast<void*>(&sm_emptyOneArg),              // +0xD4  (empty)
    reinterpret_cast<void*>(&sm_returnsFalse),             // +0xD8  (false)
    reinterpret_cast<void*>(&sm_emptyOneArg),              // +0xDC  (empty)
    reinterpret_cast<void*>(&sm_setDword28C),              // +0xE0
    reinterpret_cast<void*>(&sm_getDword28C),              // +0xE4
    reinterpret_cast<void*>(&sm_getOccluderRecordCount),   // +0xE8
    reinterpret_cast<void*>(&sm_getOccluderRecord),        // +0xEC
    reinterpret_cast<void*>(&sm_stampFrameOrdinal),        // +0xF0
    reinterpret_cast<void*>(&sm_setFade),                  // +0xF4
    reinterpret_cast<void*>(&sm_refreshRootObject)         // +0xF8
};


// Converts the ILightSampleable face at +0x30 to its owning StaticMesh.

inline StaticMesh* fromLightSampleable(void* subobject)
{
    return reinterpret_cast<StaticMesh*>(static_cast<unsigned char*>(subobject) - 0x30);
}

int __fastcall sm_ls_addRef(void* self, void*)
{
    return fromLightSampleable(self)->addRef();
}

int __fastcall sm_ls_getRef(void* self, void*)
{
    return fromLightSampleable(self)->getRef();
}

int __fastcall sm_ls_release(void* self, void*)
{
    return fromLightSampleable(self)->release();
}

void* __fastcall sm_ls_queryInterface(void* self, void*, unsigned int iid)
{
    return fromLightSampleable(self)->queryInterface(iid);
}

bool __fastcall sm_ls_slot10(void* self, void*)
{
    return fromLightSampleable(self)->lightSampleableSlot10ReturnsTrue();
}

// Defined below with the other name builders.
void staticMeshLightmapName(const StaticMesh* self, vc71::string& out);

// ILightSampleable +0x14: a MeshLightSampler for the current (geom, lod), loaded from
// "<template path>/Meshes/<template name>.samples" (geom 0, lod 0) or ".samp_<geom><lod>" and
// writing the instance's lightmap name. A template with lightmapOverride caps the sample grid at
// its lightmap size for the pair; `resolutionModifier` is LightmapGeneration's. On success the
// pair's cached lightmap texture is released and the instance's current lightmap cleared. Null
// when the file cannot be opened. The allocation is used unchecked, as shipped.
// [INFERRED] placement: retail keeps this body with the rest of its run in
// StaticMeshLightmapGeneration.cpp; this tree keeps the run's other members here.
ILightSampling* __fastcall sm_ls_createLightSampler(void* self, void*, float resolutionModifier)
{
    StaticMesh* const mesh = fromLightSampleable(self);
    void* const storage = ::renddx9abi::engineAlloc(sizeof(MeshLightSampler));
    MeshLightSampler* const sampler = storage ? new (storage) MeshLightSampler() : 0;

    typedef int (__fastcall* IndexFn)(const void*, void*);
    const int lod  = reinterpret_cast<IndexFn>(mesh->m_vptr[0x7C / sizeof(void*)])(mesh, 0);
    const int geom = reinterpret_cast<IndexFn>(mesh->m_vptr[0x70 / sizeof(void*)])(mesh, 0);
    char suffix[12];
    if ((geom | lod) != 0)
        std::sprintf(suffix, ".samp_%d%d", geom, lod);
    else
        std::strcpy(suffix, ".samples");

    IGeometryTemplate* const tmpl = static_cast<StaticMeshTemplate*>(mesh->m_template);
    if (tmpl->getLightmapOverride())                                       // +0x6C
        sampler->m_sizeCap = tmpl->getLightmapSize(geom, lod);             // +0x74
    sampler->m_resolutionModifier = resolutionModifier;

    vc71::string meshes(tmpl->getPath());                                  // +0x20
    meshes += "/Meshes/";
    vc71::string named(meshes);
    named += tmpl->getName();                                              // +0x18
    vc71::string path(named);
    path += suffix;
    vc71::string outputName;
    staticMeshLightmapName(mesh, outputName);
    const bool failed = !sampler->load(path, outputName);
    if (failed)
    {
        sampler->~MeshLightSampler();
        ::renddx9abi::engineFree(sampler);
        return 0;
    }

    const int geomTimesFive =
        5 * reinterpret_cast<IndexFn>(mesh->m_vptr[0x70 / sizeof(void*)])(mesh, 0);
    const unsigned int key = static_cast<unsigned int>(
        reinterpret_cast<IndexFn>(mesh->m_vptr[0x7C / sizeof(void*)])(mesh, 0) + 2 * geomTimesFive);
    vc71::map_node_u32* const node = vc71::mapInsertU32(mesh->m_map260, key);
    dx9rend::EngineTexture*& slot = *reinterpret_cast<dx9rend::EngineTexture**>(node->value);
    dx9rend::EngineTexture* const cached = slot;
    mesh->m_dword26C = reinterpret_cast<unsigned int>(cached);
    if (cached)
    {
        if (--cached->refCount == 0)
        {
            dx9rend::textureDtor(cached);
            ::renddx9abi::engineFree(cached);
        }
        mesh->m_dword26C = 0;
        slot = 0;
    }
    return sampler;
}

// ILightSampleable +0x18: destroys and frees the sampler when one is passed (its destructor
// writes the lightmap file), then re-fetches this instance's lightmap texture from its (geom, lod)
// map entry, lowers the "no lightmap" flag and runs the lightmap loader again. Always true.
bool __fastcall sm_ls_destroyLightSampler(void* self, void*, ILightSampling* sampler)
{
    StaticMesh* const mesh = fromLightSampleable(self);
    if (sampler)
    {
        MeshLightSampler* const meshSampler = static_cast<MeshLightSampler*>(sampler);
        meshSampler->~MeshLightSampler();
        ::renddx9abi::engineFree(meshSampler);
    }

    const unsigned int key = static_cast<unsigned int>(mesh->getLodIndex())
                           + 2u * (5u * static_cast<unsigned int>(mesh->getGeomIndex()));
    vc71::map_node_u32* const node = vc71::mapInsertU32(mesh->m_map260, key);
    mesh->m_dword26C = node ? *reinterpret_cast<unsigned int*>(node->value) : 0u;
    mesh->m_byte35A  = 0;
    mesh->loadLightmap();
    return true;
}

void* const kStaticMeshLightSampleableVtable[7] =
{
    reinterpret_cast<void*>(&sm_ls_addRef),               // +0x00  0x10103C50  thunk -> addRef
    reinterpret_cast<void*>(&sm_ls_getRef),               // +0x04  0x10103C60  thunk -> getRef
    reinterpret_cast<void*>(&sm_ls_release),              // +0x08  0x10103C70  thunk -> release
    reinterpret_cast<void*>(&sm_ls_queryInterface),       // +0x0C  0x10103BF0  thunk -> queryInterface
    reinterpret_cast<void*>(&sm_ls_slot10),               // +0x10  0x10197EF0  true
    reinterpret_cast<void*>(&sm_ls_createLightSampler),   // +0x14  0x10198680
    reinterpret_cast<void*>(&sm_ls_destroyLightSampler)   // +0x18  0x10198D80
};

} // anonymous namespace - staticMeshRootParent is shared with XpLightManager.cpp's
// Returns and memoizes the object's root parent; root objects cache themselves.
// Returns null for a null object.
void* staticMeshRootParent(void* object)
{
    if (!object)
        return 0;

    typedef world::object_detail::LayoutAccess L;
    world::IObject* const current = static_cast<world::IObject*>(object);

    if ((L::flags(current) & 0x01000000u) != 0)    // m_flags & 0x01000000 - "I am the root"
    {
        L::cachedRootParent(current) = current;
        return object;
    }

    world::IObject* cached = L::cachedRootParent(current);
    if (!cached)
    {
        cached = static_cast<world::IObject*>(staticMeshRootParent(L::parent(current)));   // the parent
        L::cachedRootParent(current) = cached;
    }
    return cached;
}

namespace {


// Reports whether authored lightmaps are enabled by a force flag, quality setting or expansion mode.
bool staticMeshTemplateUsesLightmaps(const void* objectTemplate)
{
    if (!objectTemplate)
        return false;

    const unsigned char* const t = static_cast<const unsigned char*>(objectTemplate);
    if (!t[0x185])
        return false;
    if (t[0x2A4])
        return true;

    dx9rend::IRenderer* const base =
        static_cast<dx9rend::IRenderer*>(::dice::hfe::dx9rend::g_rendererBase);
    if (!base)
        return false;

    IVideoOptions* const options = base->getVideoOptions();

    if (options)
    {
        const int quality = options->getQuality(2);
        if (quality > 1)
            return true;
    }

    return base->isExpansionPack();
}


// Refreshes selected LOD/material pointers and bounds after a geometry or LOD change.
// Template tables use 32-byte geometry records for LODs and 16-byte companion records.

void staticMeshRefreshSelection(StaticMesh* self)
{
    unsigned char* const lodRecord = static_cast<unsigned char*>(self->m_dword30C);
    if (!lodRecord)
        return;

    MeshBounds* const bounds = reinterpret_cast<MeshBounds*>(lodRecord + 0x04);
    if (bounds->dirty)
        meshBoundsFinalize(*bounds);

    self->m_dword304 = *reinterpret_cast<const unsigned int*>(&bounds->radius);

    // The lightmap entry for this (geom, lod). The shipped call is the inserting
    // operator[], not a find - a miss leaves a zeroed entry behind and copies those zeros out, which
    // is why a fresh selection can clear the 1.0f UV scale the constructor seeded.
    if (self->m_byte35A || self->m_map260.size == 0)
        return;

    const unsigned char* const objectTemplate =
        static_cast<const unsigned char*>(self->m_template);
    if (!objectTemplate)
        return;

    const unsigned int geom = static_cast<unsigned int>(self->getGeomIndex());
    const unsigned int lod  = static_cast<unsigned int>(self->getLodIndex());

    const unsigned int* const perGeom =
        *reinterpret_cast<const unsigned int* const*>(objectTemplate + 0x74);
    if (!perGeom)
        return;

    const unsigned int key = perGeom[geom] + lod + 2u * (5u * geom);

    vc71::map_node_u32* const node = vc71::mapInsertU32(self->m_map260, key);
    if (!node)
        return;

    const unsigned char* const value = node->value;
    self->m_dword26C = *reinterpret_cast<const unsigned int*>(value + 0);
    self->m_float270 = *reinterpret_cast<const float*>(value + 4);
    self->m_float274 = *reinterpret_cast<const float*>(value + 8);
    self->m_dword278 = *reinterpret_cast<const unsigned int*>(value + 12);
    self->m_dword27C = *reinterpret_cast<const unsigned int*>(value + 16);
}

// Release every cached lightmap texture in the +0x260 map. Recurse right, iterate left -
// deliberately the same walk mapEraseTreeU32 performs, because this must visit exactly the
// nodes that walk is about to free, and it must happen first: the textures live in the nodes.
void staticMeshReleaseLightmaps(vc71::map_node_u32* node)
{
    while (node && !node->isNil)
    {
        staticMeshReleaseLightmaps(node->right);

        dx9rend::EngineTexture*& texture =
            *reinterpret_cast<dx9rend::EngineTexture**>(node->value);
        if (texture)
            dx9rend::textureReleaseRef(texture);

        node = node->left;
    }
}


// Transforms a plane by rotating its normal and transforming the point -normal * distance.
// Does not use inverse transpose or normalize the result; input normals must be normalized.

void staticMeshTransformPlane(float* out, const float* plane, const float* m)
{
    out[0] = plane[0] * m[0] + plane[1] * m[4] + plane[2] * m[8];
    out[1] = plane[0] * m[1] + plane[1] * m[5] + plane[2] * m[9];
    out[2] = plane[0] * m[2] + plane[1] * m[6] + plane[2] * m[10];

    // A point that lies on the plane, then that same point in the new frame.
    const float px = -plane[0] * plane[3];
    const float py = -plane[1] * plane[3];
    const float pz = -plane[2] * plane[3];

    const float tx = px * m[0] + py * m[4] + pz * m[8]  + m[12];
    const float ty = px * m[1] + py * m[5] + pz * m[9]  + m[13];
    const float tz = px * m[2] + py * m[6] + pz * m[10] + m[14];

    out[3] = -(tz * out[2] + ty * out[1] + tx * out[0]);
}

// sub_100777A0 - Vec3 normalize in place, with the shipped |v|^2 within-epsilon short circuits.
// The same body StaticMeshMaterial.cpp carries at its own file scope; three lines, repeated here
// rather than exported, because that one is file-local too.
void staticMeshNormalizeInPlace(float* v)
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

// sub_100FFB50 - allocate the map's sentinel node. forty bytes: left/parent/right, a 24-byte
// value, then the colour byte at +0x24 and the nil flag at +0x25. The shipped helper zeroes the
// three links and writes colour=1/nil=0; the caller then flips nil to 1 and self-links all three,
// which is what makes it a header rather than an element.
void* staticMeshAllocateMapNode()
{
    vc71::map_node_u32* const node = static_cast<vc71::map_node_u32*>(
        ::renddx9abi::engineAlloc(sizeof(vc71::map_node_u32)));
    if (!node)
        return 0;

    node->left   = 0;
    node->parent = 0;
    node->right  = 0;
    node->color  = 1;      // black
    node->isNil  = 0;      // the caller flips this to 1 - see the constructor
    return node;
}


// Accesses renderer configuration through its fixed compatibility layout.
unsigned char rendererByte(unsigned int byteOffset)
{
    return *(reinterpret_cast<const unsigned char*>(::dice::hfe::dx9rend::g_Renderer) + byteOffset);
}

unsigned int rendererDword(unsigned int byteOffset)
{
    return *reinterpret_cast<const unsigned int*>(
        reinterpret_cast<const unsigned char*>(::dice::hfe::dx9rend::g_Renderer) + byteOffset);
}

float rendererFloat(unsigned int byteOffset)
{
    return *reinterpret_cast<const float*>(
        reinterpret_cast<const unsigned char*>(::dice::hfe::dx9rend::g_Renderer) + byteOffset);
}

// The base object's +0x102 byte, the "editor / lightmap generation" gate BF2Render.cpp's step 14
// tests too.
unsigned char rendererBaseByte(unsigned int byteOffset)
{
    return *(static_cast<const unsigned char*>(::dice::hfe::dx9rend::g_rendererBase) + byteOffset);
}

} // anonymous namespace - the two helpers below are shared with
  // StaticMeshLightmapGeneration.cpp (declared in StaticMesh.h)

StaticMeshRenderer* staticMeshRenderer()
{
    return static_cast<StaticMeshRenderer*>(static_cast<void*>(g_StaticMeshRenderer));
}

// dword_1024B930 - the Terrain singleton.  BF2Render.cpp records that nothing recovered publishes
// it; the water arm of the draw tests it first and is dead until a Terrain class lands.
// [FRONTIER] dice::hfe::geom::Terrain - the class that publishes dword_1024B930
void* const g_terrain = 0;

// sub_1008DD80 - pack a Vec3 and a w into a static Vec4 and answer its address.  The shipped body
// guards a one-time zeroing of the static with a flag bit; a zero-initialised static is that.
const float* staticMeshPackVec4(const float v[3], float w)
{
    static float s_packed[4] = { 0.0f, 0.0f, 0.0f, 0.0f };   // unk_1024B100
    s_packed[0] = v[0];
    s_packed[1] = v[1];
    s_packed[2] = v[2];
    s_packed[3] = w;
    return s_packed;
}

namespace {

// sub_10031180 - a counted texture-holder assignment on a {vptr, value} holder: release the old
// (textureDtor + engineFree at zero), take the new, addRef it through its vtable slot +0x08.  The
// same body StaticMeshMaterial.cpp's raAssignTextureValue reproduces for the material block's own
// slots; repeated here because that one is file-local.
void staticMeshAssignTexture(void*& slot, dx9rend::EngineTexture* incoming)
{
    dx9rend::EngineTexture* const old = static_cast<dx9rend::EngineTexture*>(slot);
    if (old == incoming)
        return;
    if (old && --old->refCount == 0)
    {
        dx9rend::textureDtor(old);
        ::renddx9abi::engineFree(old);
    }
    slot = incoming;
    if (incoming)
        dx9rend::textureAddRef(incoming);
}

// Cached technique pointer at material +0x178, refreshed for each draw.
// This shares storage with normalUnpack.value[2]; the shader uses only its first two components.
dx9rend::Ra::RaShader*& staticMeshMaterialShader(unsigned char* material)
{
    return *reinterpret_cast<dx9rend::Ra::RaShader**>(material + 0x178);
}

// The IDirect3DBaseTexture9 the lightmap holder of a render entry keeps, with COM reference
// counting - the shipped swap: `if (old != new) { old->Release(); slot = new; new->AddRef(); }`.
void staticMeshSwapD3DTexture(void*& slot, IDirect3DBaseTexture9* incoming)
{
    IDirect3DBaseTexture9* const current = static_cast<IDirect3DBaseTexture9*>(slot);
    if (current == incoming)
        return;
    if (current)
        current->Release();
    slot = incoming;
    if (incoming)
        incoming->AddRef();
}

// The four lightmap-offset words this instance caches (+0x270..+0x27C) into an entry's
// LightMapOffset holder, in the shipped store order.
void staticMeshStoreLightMapOffset(dx9rend::RenderQueueEntry* entry, const StaticMesh* self)
{
    float* const out = entry->lighting.lightMapOffset.value;
    out[0] = self->m_float270;
    out[1] = self->m_float274;
    *reinterpret_cast<unsigned int*>(&out[2]) = self->m_dword278;
    *reinterpret_cast<unsigned int*>(&out[3]) = self->m_dword27C;
}


// sub_100CE120 / sub_101C6750 / sub_101C67F0 / sub_101C6A90 / sub_101C6C50 - the eight-corner
// box test the draw culls each material with.  A 96-byte object: the eight corners of a MeshBounds'
// min/max box (sub_101C6750, in the fixed order below), pushed through the world matrix
// (sub_101C67F0), then tested against the six frustum planes (sub_101C6A90): a plane with ALL
// eight corners on its positive side rejects the box.  sub_101C6C50 is the `!` on that.

struct StaticMeshBoundsCorners
{
    float corner[8][3];

    // sub_100CE120 - zero the 24 floats, then the two setters.
    StaticMeshBoundsCorners(const MeshBounds& bounds, const Mat4& world)
    {
        for (unsigned int i = 0; i < 8; ++i)
            for (unsigned int j = 0; j < 3; ++j)
                corner[i][j] = 0.0f;
        setBounds(bounds);
        transform(world);
    }

    // sub_101C6750 - min/max to corners, in this exact order.
    void setBounds(const MeshBounds& b)
    {
        corner[0][0] = b.min.x; corner[0][1] = b.min.y; corner[0][2] = b.min.z;
        corner[1][0] = b.min.x; corner[1][1] = b.min.y; corner[1][2] = b.max.z;
        corner[2][0] = b.min.x; corner[2][1] = b.max.y; corner[2][2] = b.min.z;
        corner[3][0] = b.min.x; corner[3][1] = b.max.y; corner[3][2] = b.max.z;
        corner[4][0] = b.max.x; corner[4][1] = b.min.y; corner[4][2] = b.min.z;
        corner[5][0] = b.max.x; corner[5][1] = b.min.y; corner[5][2] = b.max.z;
        corner[6][0] = b.max.x; corner[6][1] = b.max.y; corner[6][2] = b.min.z;
        corner[7][0] = b.max.x; corner[7][1] = b.max.y; corner[7][2] = b.max.z;
    }

    // sub_101C67F0 - every corner through the full matrix, translation included.
    void transform(const Mat4& w)
    {
        const float* const m = w.m;
        for (unsigned int i = 0; i < 8; ++i)
        {
            const float x = corner[i][0];
            const float y = corner[i][1];
            const float z = corner[i][2];
            const float tz = z * m[10] + x * m[2] + y * m[6] + m[14];
            const float ty = x * m[1] + y * m[5] + z * m[9] + m[13];
            corner[i][0] = m[8] * z + x * m[0] + y * m[4] + m[12];
            corner[i][1] = ty;
            corner[i][2] = tz;
        }
    }

    // sub_101C6C50 -> !sub_101C6A90: true when some plane has all eight corners outside.
    bool outside(const Frustum& frustum) const
    {
        for (unsigned int p = 0; p < 6; ++p)
        {
            const Plane3& plane = frustum.m_planes[p];
            unsigned int positive = 0;
            for (unsigned int i = 0; i < 8; ++i)
            {
                const float distance = plane.normal.x * corner[i][0]
                                     + plane.normal.y * corner[i][1]
                                     + plane.normal.z * corner[i][2]
                                     + plane.d;
                if (distance > 0.0f)
                    ++positive;
            }
            if (positive == 8)
                return true;
        }
        return false;
    }
};

// sub_100E1C20 - vector<T*>::resize(n, fill): shrink moves _Last down (the shipped memmove of
// zero bytes), grow inserts copies of `fill` at the end.
void staticMeshPointerVectorResize(vc71::vector_layout& v, unsigned int count, void* fill)
{
    const unsigned int size = v.first
        ? static_cast<unsigned int>(v.last - v.first) / sizeof(void*)
        : 0u;
    if (size >= count)
    {
        if (v.first && count < size)
            v.last = v.first + count * sizeof(void*);
        return;
    }
    for (unsigned int i = size; i < count; ++i)
        vc71::vectorPushBackIn(v, sizeof(void*), &fill);
}

// sub_10102690 - vector<RaLightRecord>::resize(n, fill) on the 68-byte elements; the same two arms.
void staticMeshLightRecordVectorResize(vc71::vector_layout& v, unsigned int count,
                                       const dx9rend::Ra::RaLightRecord& fill)
{
    const unsigned int stride = sizeof(dx9rend::Ra::RaLightRecord);
    const unsigned int size = v.first ? static_cast<unsigned int>(v.last - v.first) / stride : 0u;
    if (size >= count)
    {
        if (v.first && count < size)
            v.last = v.first + count * stride;
        return;
    }
    for (unsigned int i = size; i < count; ++i)
        vc71::vectorPushBackIn(v, stride, &fill);
}

// sub_10199AE0 - the same on the 28-byte ".lights" records.
void staticMeshVertexLightVectorResize(vc71::vector_layout& v, unsigned int count)
{
    const unsigned int stride = 28u;
    const unsigned int size = v.first ? static_cast<unsigned int>(v.last - v.first) / stride : 0u;
    if (size >= count)
    {
        if (v.first && count < size)
            v.last = v.first + count * stride;
        return;
    }
    unsigned char zero[28];
    for (unsigned int b = 0; b < sizeof(zero); ++b)
        zero[b] = 0;
    for (unsigned int i = size; i < count; ++i)
        vc71::vectorPushBackIn(v, stride, zero);
}

// sub_10074AB0 - the image's printf-into-a-std::string helper: vsnprintf into a 0xFFF-byte
// buffer, or the "WARNING: The original message was too long for formatString()." text when it
// does not fit.
void staticMeshFormatString(vc71::string& out, const char* format, int a, int b, int c, int d,
                            bool four)
{
    char buffer[0x1000];
    const int written = four ? std::snprintf(buffer, 0xFFF, format, a, b, c, d)
                             : std::snprintf(buffer, 0xFFF, format, a, b, c);
    if (written < 0)
        std::snprintf(buffer, sizeof(buffer), "%s",
                      "WARNING: The original message was too long for formatString(). \n");
    else
        buffer[written] = 0;
    out = buffer;
}

// The StaticMeshRenderer the three name builders look up BY name on every call, as shipped.
StaticMeshRenderer* staticMeshRendererByName()
{
    IBase* const found = classManager->getSingleton(renddx9abi::HostString("StaticMeshRenderer"));
    return static_cast<StaticMeshRenderer*>(static_cast<void*>(found));
}

// sub_10198010 - the full lightmap file name of this instance's current (geom, lod):
//     <Editor_LevelSavePath>/<GSLevelDirectory><GSLoadLevel>/Lightmaps/Objects/<template name>
//     followed by "=<key:02>=<x>=<y>=<z>" when the renderer's +0x62 flag is set, else "_<x>-<y>-<z>"
// where key = lod + 10 * geom and x/y/z are the transform's translation truncated to int.
// The three settings come with the shipped defaults: "", "" and "lightsPlease".
void staticMeshLightmapName(const StaticMesh* self, vc71::string& out)
{
    const float* const position = &self->m_transform->m[12];
    StaticMeshRenderer* const smr = staticMeshRendererByName();
    const int key = self->getLodIndex() + 2 * (5 * self->getGeomIndex());

    vc71::string savePath("");
    dx9rend::readStringSettingDefault("Editor_LevelSavePath", "", savePath);
    vc71::string root(savePath);
    root += "/";
    vc71::string levelDirectory("");
    dx9rend::readStringSettingDefault("GSLevelDirectory", "", levelDirectory);
    vc71::string withDirectory(root);
    withDirectory += levelDirectory;
    vc71::string level("");
    dx9rend::readStringSettingDefault("GSLoadLevel", "lightsPlease", level);
    vc71::string levelPath(withDirectory);
    levelPath += level;

    const MeshTemplate* const t = static_cast<const MeshTemplate*>(
        static_cast<const StaticMeshTemplate*>(self->m_template));
    vc71::string objects(levelPath);
    objects += "/Lightmaps/Objects/";
    vc71::string named(objects);
    named += t->getName();                                             // MeshTemplate +0x18

    vc71::string suffix("");
    if (smr->m_flags60[2])
        staticMeshFormatString(suffix, "=%02d=%d=%d=%d", key, static_cast<int>(position[0]),
                               static_cast<int>(position[1]), static_cast<int>(position[2]), true);
    else
        staticMeshFormatString(suffix, "_%d-%d-%d", static_cast<int>(position[0]),
                               static_cast<int>(position[1]), static_cast<int>(position[2]), 0,
                               false);

    vc71::string result(named);
    result += suffix;
    out = result;
}

// sub_10198410 - the object part alone: <template name> plus the same suffix.
void staticMeshLightmapObjectName(const StaticMesh* self, vc71::string& out)
{
    const float* const position = &self->m_transform->m[12];
    StaticMeshRenderer* const smr = staticMeshRendererByName();
    const int key = self->getLodIndex() + 2 * (5 * self->getGeomIndex());
    const MeshTemplate* const t = static_cast<const MeshTemplate*>(
        static_cast<const StaticMeshTemplate*>(self->m_template));

    vc71::string suffix("");
    if (smr->m_flags60[2])
        staticMeshFormatString(suffix, "=%02d=%d=%d=%d", key, static_cast<int>(position[0]),
                               static_cast<int>(position[1]), static_cast<int>(position[2]), true);
    else
        staticMeshFormatString(suffix, "_%d-%d-%d", static_cast<int>(position[0]),
                               static_cast<int>(position[1]), static_cast<int>(position[2]), 0,
                               false);

    vc71::string result(t->getName());
    result += suffix;
    out = result;
}

// sub_10197F00 - the directory part: <GSLevelDirectory><GSLoadLevel>/Lightmaps/Objects/ (no save
// path, unlike the full name).
void staticMeshLightmapDirectory(vc71::string& out)
{
    vc71::string level("");
    dx9rend::readStringSettingDefault("GSLoadLevel", "lightsPlease", level);
    vc71::string levelDirectory("");
    dx9rend::readStringSettingDefault("GSLevelDirectory", "", levelDirectory);
    vc71::string path(levelDirectory);
    path += level;
    vc71::string result(path);
    result += "/Lightmaps/Objects/";
    out = result;
}

} // anonymous namespace


// Initializes the geometry instance and installs its final interface tables.

StaticMesh::StaticMesh(void* objectTemplate)
    : GeometryInstanceData(kStaticMeshVtable)
    , m_lightSampleableVptr(kStaticMeshLightSampleableVtable)
    , m_refCount(1)
    , m_float25C(1.0f)
    , m_dword26C(0)
    , m_float270(1.0f)
    , m_float274(1.0f)
    , m_dword278(0)
    , m_dword27C(0)
    , m_template(objectTemplate)
    , m_transform(0)
    , m_dword288(0)
    , m_dword300(0), m_dword304(0)
    , m_float308(-1.0f)
    , m_dword30C(0), m_dword310(0), m_dword314(0), m_dword318(0), m_dword31C(0)
    , m_dword324(0), m_dword328(0), m_dword32C(0), m_dword330(0), m_dword334(0)
    , m_byte348(1)
    , m_dword34C(0)
    , m_dword350(0)
    , m_byte354(0)
    , m_byte356(0)
    , m_byte357(0)
    , m_byte358(0)
    , m_byte359(0)
    , m_byte35A(0)
{
    // +0x38 .. +0x25B - the Ra per-object parameter block. Already recovered; see the header.
    ::dice::hfe::dx9rend::Ra::raObjectParametersConstruct(m_objectParameters);

    // +0x260 - the VC7.1 map header. The node comes back with its links null; the constructor
    // then makes it the self-linked nil sentinel.
    m_map260.comp = 0;
    m_map260.head  = reinterpret_cast<vc71::map_node*>(staticMeshAllocateMapNode());
    m_map260.size  = 0;
    if (m_map260.head)
    {
        vc71::map_node_u32* const head = reinterpret_cast<vc71::map_node_u32*>(m_map260.head);
        head->isNil  = 1;
        head->parent = head;
        head->left   = head;
        head->right  = head;
    }

    // The four vectors the destructor (0x100FDE50) frees. Their allocator words are
    // deliberately left alone, exactly as the shipped constructor leaves them.
    m_vector2D0.first = 0; m_vector2D0.last = 0; m_vector2D0.end = 0;
    m_vector2E0.first = 0; m_vector2E0.last = 0; m_vector2E0.end = 0;
    m_vector2F0.first = 0; m_vector2F0.last = 0; m_vector2F0.end = 0;
    m_vector338.first = 0; m_vector338.last = 0; m_vector338.end = 0;

    // +0x35C and +0x36C - the two per-geom vectors.  The allocator word of each is
    // deliberately NOT written, exactly as the constructor leaves it and exactly as ObjectDrawer's
    // three vectors are handled.
    m_geomRadii.first = 0;             m_geomRadii.last = 0;             m_geomRadii.end = 0;
    m_geomLodDistanceCounts.first = 0; m_geomLodDistanceCounts.last = 0; m_geomLodDistanceCounts.end = 0;

    // +0x290 - identity.
    for (int i = 0; i < 16; ++i)
        m_matrix290.m[i] = 0.0f;
    m_matrix290.m[0] = 1.0f;
    m_matrix290.m[5] = 1.0f;
    m_matrix290.m[10] = 1.0f;
    m_matrix290.m[15] = 1.0f;

    // Loaded templates need immediate per-instance post-load initialization.
    const MeshTemplate* const meshTemplate = static_cast<const MeshTemplate*>(
        static_cast<const StaticMeshTemplate*>(objectTemplate));
    if (meshTemplate && meshTemplate->m_loaded)
        postLoad();
}


// Releases cached lightmap textures before destroying the map nodes that hold them.

StaticMesh::~StaticMesh()
{
    // 1. Release every cached lightmap texture, then drop the nodes.
    if (m_map260.head)
        staticMeshReleaseLightmaps(
            reinterpret_cast<vc71::map_node_u32*>(m_map260.head)->parent);

    // 2. Deregister from the template's instance set (+0xEC), the one createInstance inserted
    //    into. Reached by documented raw offset, as everywhere else this file touches the
    //    template.
    if (m_template)
    {
        vc71::set_layout* const instances = reinterpret_cast<vc71::set_layout*>(
            static_cast<unsigned char*>(m_template) + 0xEC);
        vc71::setEraseValue(*instances, this);
    }

    // 3. The refcounted vertex-buffer wrapper at +0x300.
    if (m_dword300)
    {
        dx9rend::vertexBufferRelease(reinterpret_cast<void*>(m_dword300));
        m_dword300 = 0;
    }

    // The template flag controls release of each selected material's +0x14C texture.
    if (m_template && *(static_cast<const unsigned char*>(m_template) + 0x2A4) && m_dword310)
    {
        unsigned char* const first = *reinterpret_cast<unsigned char**>(m_dword310 + 4);
        if (first)
        {
            unsigned char* const last = *reinterpret_cast<unsigned char**>(m_dword310 + 8);
            const unsigned int count = static_cast<unsigned int>(last - first) / 516u;
            for (unsigned int i = 0; i < count; ++i)
            {
                unsigned char* const material = first + 516u * i;
                dx9rend::EngineTexture*& slot =
                    *reinterpret_cast<dx9rend::EngineTexture**>(material + 332);
                if (slot)
                    dx9rend::textureReleaseRef(slot);   // nulls the slot itself
            }
        }
    }

    // Destroy the two nested vectors in each 36-byte element before freeing m_vector338.
    if (m_vector338.first)
    {
        for (unsigned char* p = reinterpret_cast<unsigned char*>(m_vector338.first);
             p != reinterpret_cast<unsigned char*>(m_vector338.last); p += 36)
        {
            for (unsigned int inner = 0; inner < 2; ++inner)
            {
                vc71::vector_layout* const v =
                    reinterpret_cast<vc71::vector_layout*>(p + 16 * inner);
                if (v->first)
                    ::renddx9abi::engineFree(v->first);
                v->first = 0; v->last = 0; v->end = 0;
            }
        }
    }

    // 6. The six vectors themselves.
    {
        vc71::vector_layout* const owned[] =
        {
            &m_geomLodDistanceCounts, &m_geomRadii, &m_vector338,
            &m_vector2F0, &m_vector2E0, &m_vector2D0
        };
        for (unsigned int i = 0; i < sizeof(owned) / sizeof(owned[0]); ++i)
        {
            if (owned[i]->first)
                ::renddx9abi::engineFree(owned[i]->first);
            owned[i]->first = 0; owned[i]->last = 0; owned[i]->end = 0;
        }
    }

    // 7. Clear the map, then free its sentinel - the one node mapClearU32 deliberately keeps.
    if (m_map260.head)
    {
        vc71::mapClearU32(m_map260);
        ::renddx9abi::engineFree(m_map260.head);
        m_map260.head = 0;
        m_map260.size = 0;
    }

    // 8. The Ra per-object parameter block, the exact counterpart of the constructor's
    //    raObjectParametersConstruct.
    ::dice::hfe::dx9rend::Ra::raObjectParametersDestroy(m_objectParameters);

    // 9. m_name's destructor runs after this body - see the header note.
}

// the IBase quartet

int StaticMesh::addRef()
{
    return ++m_refCount;
}

int StaticMesh::getRef() const
{
    return m_refCount;
}

// The zero test uses the decremented value, and the deleting destructor is reached
// through this object's OWN vtable slot +0x34 with a literal 1 - the MSVC scalar-deleting-destructor
// convention, where 1 means "and free the storage".
int StaticMesh::release()
{
    --m_refCount;
    if (m_refCount == 0)
    {
        typedef void (__fastcall* DeletingDtorFn)(void*, void*, int);
        reinterpret_cast<DeletingDtorFn>(m_vptr[0x34 / sizeof(void*)])(this, 0, 1);
    }
    return m_refCount;
}

// All four ids are already named in Core/Ids.h; see the header's identity note.
// The ILightSampleable arm null-tests `this`, which cannot be null on a member call. Shipped,
// reproduced.
void* StaticMesh::queryInterface(unsigned int iid)
{
    if (iid == 1u || iid == IID_IGeometry || iid == IID_IRenderable)
        return this;

    if (iid == IID_ILightSampleable && this)
        return reinterpret_cast<unsigned char*>(this) + 0x30;

    return 0;
}


// Borrows transform and lazily builds each part's world-space occluder geometry.
// A template with no parts keeps initialization pending. Requires a valid template.
// Occluder planes retain their template back-pointer and occupy 20 bytes.

void StaticMesh::setGeometryTransformation(const Mat4* transform)
{
    m_transform = transform;
    m_byte358   = 1;

    if (transform && m_byte348)
    {
        // +0xF0 is the template's part count, +0xF4 fetches one part record by index.
        typedef int (__fastcall* PartCountFn)(void*, void*);
        typedef unsigned char* (__fastcall* GetPartFn)(void*, void*, int);

        void* const objectTemplate = m_template;
        void** const tvtbl = *reinterpret_cast<void***>(objectTemplate);
        const int partCount = reinterpret_cast<PartCountFn>(
            tvtbl[0xF0 / sizeof(void*)])(objectTemplate, 0);

        // reached AS sixteen RAW floats, exactly as the shipped body reaches it - every
        // access below is one of its documented byte offsets, not a named Mat4 member.
        const float* const m = reinterpret_cast<const float*>(transform);

        for (int part = 0; part < partCount; ++part)
        {
            m_byte348 = 0;

            unsigned char* const source = reinterpret_cast<GetPartFn>(
                tvtbl[0xF4 / sizeof(void*)])(objectTemplate, 0, part);

            // The 36-byte element, built on the stack exactly as the shipped body builds it.
            // The shipped code leaves both allocator words unwritten (that is how this
            // class's vectors were identified in the first place); zeroing all 36 bytes here
            // avoids copying indeterminate bytes into the vector and is otherwise identical.
            unsigned char record[36];
            for (unsigned int b = 0; b < sizeof(record); ++b)
                record[b] = 0;

            vc71::vector_layout& planes = *reinterpret_cast<vc71::vector_layout*>(record);
            vc71::vector_layout& points = *reinterpret_cast<vc71::vector_layout*>(record + 16);
            *reinterpret_cast<unsigned char**>(record + 32) = source + 0x20;

            // the points, source +0x14/+0x18 on a twelve-byte stride
            unsigned char* const pointFirst = *reinterpret_cast<unsigned char**>(source + 0x14);
            if (pointFirst)
            {
                unsigned char* const pointLast =
                    *reinterpret_cast<unsigned char**>(source + 0x18);
                const unsigned int count =
                    static_cast<unsigned int>(pointLast - pointFirst) / 12u;
                for (unsigned int i = 0; i < count; ++i)
                {
                    const float* const v =
                        reinterpret_cast<const float*>(pointFirst + 12u * i);
                    float world[3];
                    world[0] = v[0] * m[0] + v[1] * m[4] + v[2] * m[8]  + m[12];
                    world[1] = v[0] * m[1] + v[1] * m[5] + v[2] * m[9]  + m[13];
                    world[2] = v[0] * m[2] + v[1] * m[6] + v[2] * m[10] + m[14];
                    vc71::vectorPushBackIn(points, 12u, world);
                }
            }

            // the planes, source +0x04/+0x08 on a thirty-TWO byte stride
            unsigned char* const planeFirst = *reinterpret_cast<unsigned char**>(source + 0x04);
            if (planeFirst)
            {
                unsigned char* const planeLast =
                    *reinterpret_cast<unsigned char**>(source + 0x08);
                const unsigned int count =
                    static_cast<unsigned int>(planeLast - planeFirst) >> 5;
                for (unsigned int i = 0; i < count; ++i)
                {
                    unsigned char* const src = planeFirst + 32u * i;
                    const float* const plane = reinterpret_cast<const float*>(src);

                    unsigned char element[20];
                    // The back-pointer first, as the shipped body writes it first.
                    *reinterpret_cast<unsigned char**>(element + 16) = src + 0x10;

                    // Normalize the input normal only.
                    float input[4];
                    input[0] = plane[0]; input[1] = plane[1]; input[2] = plane[2];
                    staticMeshNormalizeInPlace(input);
                    input[3] = plane[3];

                    staticMeshTransformPlane(reinterpret_cast<float*>(element), input, m);
                    vc71::vectorPushBackIn(planes, 20u, element);
                }
            }

            // Transfers nested points/planes storage to the vector record.
            // Do not free either local buffer; the record destructor now owns them.
            vc71::vectorPushBackIn(m_vector338, 36u, record);
        }
    }
}

// +0xCC, the shared const-0 stub. A StaticMesh never chains, so every walker that
// follows this slot stops after one step.
int StaticMesh::getNextGeometry()
{
    return 0;
}


// Two lines: store the object, then refresh the cached root through this object's OWN +0xF8.
// The store is NOT null-tested, and the +0xF8 call happens unconditionally. Shipped.

void StaticMesh::setOwnerObject(void* object)
{
    m_dword330 = object;

    typedef void* (__fastcall* RefreshFn)(void*, void*);
    reinterpret_cast<RefreshFn>(m_vptr[0xF8 / sizeof(void*)])(this, 0);
}

// +0xF8, memoise the owner's root parent at +0x334, AND return IT.
// The return is not decoration: the shipped body leaves the root in eax. Our caller
// discards it exactly as the shipped setOwnerObject does.
void* StaticMesh::refreshRootObject()
{
    m_dword334 = staticMeshRootParent(m_dword330);
    return m_dword334;
}


// The eight read accessors.  Each one's arity is taken from its own instruction stream, not
// from a sibling class - `retn` for the getters, `retn 4` for setDword350, `retn 8` for updateFlags
// - which is what distinguishes these from the folded stubs still left as stops above.


int StaticMesh::getGeomIndex() const
{
    return static_cast<int>(m_dword314);
}

int StaticMesh::getLodIndex() const
{
    return static_cast<int>(m_dword318);
}

int StaticMesh::getDword31C() const
{
    return static_cast<int>(m_dword31C);
}

// 0x10103B10 - `flags = (flags & ~clear) | set`, the same shape Object::updateFlags has.
void StaticMesh::updateFlags(unsigned int set, unsigned int clear)
{
    m_dword32C = (m_dword32C & ~clear) | set;
}

unsigned int StaticMesh::getFlags() const
{
    return m_dword32C;
}

// stamp this instance with the renderer's current frame ordinal.  g_rendererBase's
// +0x54 is getFrameOrdinal, which MeshTemplate.cpp already documents by that name; reached by
// documented raw slot because RendDX9Base is not modelled as a typed interface here.
void StaticMesh::stampFrameOrdinal()
{
    void* const base = ::dice::hfe::dx9rend::g_rendererBase;
    if (!base)
        return;

    typedef unsigned int (__fastcall* FrameOrdinalFn)(void*, void*);
    m_dword24 = reinterpret_cast<FrameOrdinalFn>(
        (*reinterpret_cast<void***>(base))[0x54 / sizeof(void*)])(base, 0);
}

// the nine accessors

void* StaticMesh::getTemplate() const
{
    return m_template;
}

// Requires a selected LOD; returns its bounds at offset +4 without a null check.
const void* StaticMesh::getLocalBounds() const
{
    return m_dword30C + 4;
}

void* StaticMesh::getRootObject() const
{
    return m_dword334;
}

void StaticMesh::setFloat308(float value)
{
    m_float308 = value;
}

void StaticMesh::setDword34C(unsigned int value)
{
    m_dword34C = value;
}

unsigned int StaticMesh::getDword34C() const
{
    return m_dword34C;
}

int StaticMesh::getGeometryType() const
{
    return 3;
}

// Updates flag bit 1 through the virtual updateFlags slot, honoring overrides.
void StaticMesh::setFlagBit1(unsigned char enable)
{
    typedef void (__fastcall* UpdateFlagsFn)(void*, void*, unsigned int, unsigned int);
    const UpdateFlagsFn update =
        reinterpret_cast<UpdateFlagsFn>(m_vptr[0xC0 / sizeof(void*)]);

    if (enable)
        update(this, 0, 2u, 0u);
    else
        update(this, 0, 0u, 2u);
}

// 0x10103B70 - `call [eax+0C4h]; shr eax, 1; and al, 1`. The 8-bit AND is the bool return
// convention, not a mask over the whole register: only AL is meaningful and only AL is read.
bool StaticMesh::getFlagBit1() const
{
    typedef unsigned int (__fastcall* GetFlagsFn)(const void*, void*);
    const unsigned int flags = reinterpret_cast<GetFlagsFn>(
        m_vptr[0xC4 / sizeof(void*)])(this, 0);
    return ((flags >> 1) & 1u) != 0u;
}

void StaticMesh::setDword350(void* value)
{
    m_dword350 = value;
}

void* StaticMesh::getDword350() const
{
    return m_dword350;
}


// setGeom - +0x6C, 0x100FC570.

// Selects geometry with an unsigned LOD clamp; an empty geometry does not clamp.

void StaticMesh::setGeom(int geom)
{
    m_dword314 = static_cast<unsigned int>(geom);

    unsigned char* const objectTemplate = static_cast<unsigned char*>(m_template);
    if (!objectTemplate)
        return;

    unsigned char* const table16 =
        *reinterpret_cast<unsigned char**>(objectTemplate + 0xD0);
    unsigned char* const entry =
        *reinterpret_cast<unsigned char**>(table16 + 16 * geom + 4);

    unsigned int count = 0;
    if (entry)
    {
        unsigned char* const end =
            *reinterpret_cast<unsigned char**>(table16 + 16 * geom + 8);
        count = static_cast<unsigned int>(end - entry) >> 4;
    }

    unsigned int lod = count - 1u;              // see the unsigned note above
    if (m_dword318 < lod)
        lod = m_dword318;
    m_dword318 = lod;

    unsigned char* const table32 =
        *reinterpret_cast<unsigned char**>(objectTemplate + 0xE0);
    m_dword30C = *reinterpret_cast<unsigned char**>(table32 + 32 * geom + 4) + 104 * lod;
    m_dword310 = *reinterpret_cast<unsigned char**>(table16 + 16 * geom + 4) + 16 * lod;

    staticMeshRefreshSelection(this);
}


// setLod - +0x78, 0x100FC690.

// -1 IS A reset, NOT AN index: it selects lod 0 AND clears +0x357, the byte that marks the
// lod as explicitly chosen. The draw reads that byte to decide whether to run its own LOD
// selection, so the two arms differ in meaning, not just in arithmetic.

void StaticMesh::setLod(int lod)
{
    unsigned char* const objectTemplate = static_cast<unsigned char*>(m_template);
    if (!objectTemplate)
        return;

    const unsigned int geom = m_dword314;

    unsigned char* const table32 =
        *reinterpret_cast<unsigned char**>(objectTemplate + 0xE0);
    unsigned char* const table16 =
        *reinterpret_cast<unsigned char**>(objectTemplate + 0xD0);

    unsigned char* const lodBase =
        *reinterpret_cast<unsigned char**>(table32 + 32 * geom + 4);
    unsigned char* const recBase =
        *reinterpret_cast<unsigned char**>(table16 + 16 * geom + 4);

    if (lod == -1)
    {
        m_dword318 = 0;
        m_byte357  = 0;
        m_dword30C = lodBase;
        m_dword310 = recBase;
    }
    else
    {
        m_dword318 = static_cast<unsigned int>(lod);
        m_byte357  = 1;
        m_dword30C = lodBase + 104 * lod;
        m_dword310 = recBase + 16 * lod;
    }

    staticMeshRefreshSelection(this);
}


// Initializes instance state once its template is loaded, guarded against repeated calls.
// Lightmap loading requires a transform, which is normally assigned after construction.

void StaticMesh::postLoad()
{
    if (m_byte359)
        return;
    m_byte359 = 1;

    unsigned char* const objectTemplate = static_cast<unsigned char*>(m_template);
    if (!objectTemplate)
        return;

    unsigned char* const table32 = *reinterpret_cast<unsigned char**>(objectTemplate + 0xE0);
    unsigned char* const table16 = *reinterpret_cast<unsigned char**>(objectTemplate + 0xD0);
    if (!table32 || !table16)
        return;

    // Re-select the current (geom, lod) from the freshly loaded tables.
    {
        const unsigned int geom = m_dword314;
        const unsigned int lod  = m_dword318;
        m_dword30C = *reinterpret_cast<unsigned char**>(table32 + 32 * geom + 4) + 104 * lod;
        m_dword310 = *reinterpret_cast<unsigned char**>(table16 + 16 * geom + 4) + 16 * lod;

        MeshBounds* const bounds = reinterpret_cast<MeshBounds*>(m_dword30C + 4);
        if (bounds->dirty)
            meshBoundsFinalize(*bounds);
        m_dword304 = *reinterpret_cast<const unsigned int*>(&bounds->radius);
    }

    if (!staticMeshTemplateUsesLightmaps(objectTemplate))
        m_byte35A = 1;

    // The geom count, from the template's +0xE0 vector.
    unsigned char* const geomsFirst = *reinterpret_cast<unsigned char**>(objectTemplate + 0xE0);
    unsigned char* const geomsLast  = *reinterpret_cast<unsigned char**>(objectTemplate + 0xE4);
    const unsigned int geomCount =
        geomsFirst ? static_cast<unsigned int>(geomsLast - geomsFirst) >> 5 : 0;

    // Keep the material-count walk even though its result is unused.
    {
        unsigned int unusedMaxMaterials = 0;
        for (unsigned int g = 0; g < geomCount; ++g)
        {
            unsigned char* const lodFirst =
                *reinterpret_cast<unsigned char**>(table32 + 32 * g + 4);
            if (!lodFirst)
                continue;
            unsigned char* const lodLast =
                *reinterpret_cast<unsigned char**>(table32 + 32 * g + 8);
            const unsigned int lodCount =
                static_cast<unsigned int>(lodLast - lodFirst) / 104u;
            if (!lodCount)
                continue;

            unsigned char* const recBase =
                *reinterpret_cast<unsigned char**>(table16 + 16 * g + 4);
            for (unsigned int l = 0; l < lodCount; ++l)
            {
                unsigned char* const rec = recBase + 16 * l;
                unsigned char* const matFirst = *reinterpret_cast<unsigned char**>(rec + 4);
                unsigned int materials = 0;
                if (matFirst)
                {
                    unsigned char* const matLast =
                        *reinterpret_cast<unsigned char**>(rec + 8);
                    materials = static_cast<unsigned int>(matLast - matFirst) / 516u;
                }
                if (materials >= unusedMaxMaterials)
                    unusedMaxMaterials = materials;
            }
        }
        (void)unusedMaxMaterials;
    }

    const int savedGeom = getGeomIndex();
    const int savedLod  = getLodIndex();

    staticMeshFloatVectorResize(m_geomRadii, geomCount, 0.0f);
    staticMeshUnsignedVectorResize(m_geomLodDistanceCounts, geomCount, 0u);

    for (unsigned int g = 0; g < geomCount; ++g)
    {
        m_dword314 = g;

        unsigned char* const lodBase = *reinterpret_cast<unsigned char**>(table32 + 32 * g + 4);
        MeshBounds* const bounds = reinterpret_cast<MeshBounds*>(lodBase + 4);
        if (bounds->dirty)
            meshBoundsFinalize(*bounds);

        if (m_geomRadii.first)
        {
            reinterpret_cast<unsigned int*>(m_geomRadii.first)[g] =
                *reinterpret_cast<const unsigned int*>(&bounds->radius);
        }

        // Count this geometry's LOD switch distances.
        unsigned char* const distancesFirst =
            *reinterpret_cast<unsigned char**>(table32 + 32 * g + 20);
        unsigned int distanceCount = 0;
        if (distancesFirst)
        {
            unsigned char* const distancesLast =
                *reinterpret_cast<unsigned char**>(table32 + 32 * g + 24);
            distanceCount = static_cast<unsigned int>(distancesLast - distancesFirst) >> 2;
        }
        if (m_geomLodDistanceCounts.first)
            reinterpret_cast<unsigned int*>(m_geomLodDistanceCounts.first)[g] = distanceCount;

        if (m_byte35A || !staticMeshTemplateUsesLightmaps(objectTemplate))
            continue;

        // Pre-populate this geom's lightmap entries with the default (1,1) UV scale - which is
        // exactly what the constructor seeds +0x270/+0x274 with.
        unsigned char* const lodFirst = lodBase;
        unsigned char* const lodLast  = *reinterpret_cast<unsigned char**>(table32 + 32 * g + 8);
        const unsigned int lodCount =
            lodFirst ? static_cast<unsigned int>(lodLast - lodFirst) / 104u : 0;

        const unsigned int* const perGeom =
            *reinterpret_cast<const unsigned int* const*>(objectTemplate + 0x74);

        for (unsigned int l = 0; l < lodCount; ++l)
        {
            m_dword318 = l;

            if (!perGeom)
                break;

            const unsigned int geomIndex = static_cast<unsigned int>(getGeomIndex());
            const unsigned int key = perGeom[geomIndex]
                                   + static_cast<unsigned int>(getLodIndex())
                                   + 2u * (5u * geomIndex);

            // mapFindU32 answers NULL on a miss where the shipped find returns the head
            // sentinel; the shipped test compares against the head, so this compares against null.
            if (!vc71::mapFindU32(m_map260, key))
            {
                vc71::map_node_u32* const fresh = vc71::mapInsertU32(m_map260, key);
                if (fresh)
                {
                    for (unsigned int b = 0; b < sizeof(fresh->value); ++b)
                        fresh->value[b] = 0;
                }
            }

            vc71::map_node_u32* const entry = vc71::mapInsertU32(m_map260, key);
            if (entry)
            {
                *reinterpret_cast<unsigned int*>(entry->value + 0)  = 0;
                *reinterpret_cast<float*>(entry->value + 4)         = 1.0f;
                *reinterpret_cast<float*>(entry->value + 8)         = 1.0f;
                *reinterpret_cast<unsigned int*>(entry->value + 12) = 0;
                *reinterpret_cast<unsigned int*>(entry->value + 16) = 0;
            }

            if (m_transform)
                loadLightmap();
        }
    }

    // Restore the selection the caller had, then reset it - both, in this order, as shipped.
    setGeom(savedGeom);
    setLod(savedLod);
    setLod(-1);

    if (staticMeshTemplateUsesLightmaps(objectTemplate))
    {
        if (m_transform && !m_dword26C && !m_byte35A)
            loadLightmap();
    }
    else
    {
        m_byte35A = 1;
    }

    if (m_transform)
        setGeometryTransformation(m_transform);
}

// +0xE0 / +0xE4, 0x10103BD0 / 0x10103BE0 - `retn 4` and `retn`, so the arity is certain.

// Stores the field populated during template loading.
void StaticMesh::setDword28C(void* value)
{
    m_dword28C = value;
}

void* StaticMesh::getDword28C() const
{
    return m_dword28C;
}


// Returns the template's geometry count. Requires a valid template; an empty vector yields zero.

int StaticMesh::getGeomCount() const
{
    const unsigned char* const objectTemplate =
        static_cast<const unsigned char*>(m_template);

    const unsigned char* const first =
        *reinterpret_cast<const unsigned char* const*>(objectTemplate + 0xE0);
    if (!first)
        return 0;

    const unsigned char* const last =
        *reinterpret_cast<const unsigned char* const*>(objectTemplate + 0xE4);
    return static_cast<int>(static_cast<unsigned int>(last - first) >> 5);
}


// Returns the current geometry's LOD count. Requires a valid template and geometry table.

int StaticMesh::getLodCount() const
{
    const unsigned char* const objectTemplate =
        static_cast<const unsigned char*>(m_template);

    const unsigned char* const geomRecord =
        *reinterpret_cast<const unsigned char* const*>(objectTemplate + 0xE0)
        + 32 * m_dword314;

    const unsigned char* const first =
        *reinterpret_cast<const unsigned char* const*>(geomRecord + 4);
    if (!first)
        return 0;

    const unsigned char* const last =
        *reinterpret_cast<const unsigned char* const*>(geomRecord + 8);
    return static_cast<int>((last - first) / 104);
}

// +0xF4, one store, into RaObjectParameters::transparency.value.
void StaticMesh::setFade(float value)
{
    objectParameters().transparency.value = value;
}


// +0x14, 0x10082D10 - `fld ds:-1.0; retn`.
float StaticMesh::slot14ReturnsMinusOne() const
{
    return -1.0f;
}

// +0x30, 0x1005DA00 - `xor al, al; retn 4`.
bool StaticMesh::slot30ReturnsFalse(int) const
{
    return false;
}

// +0x3C / +0xC8, 0x10088060 - `xor eax, eax; retn 4`.
int StaticMesh::slot3CReturnsZero(int) const
{
    return 0;
}

// +0x44, the selected lod's bounding radius.  0.01f with no lod selected; the
// bounds are finalised first when dirty, and the radius is also cached into +0x304 on the way out
// (`fst`, not `fstp`) - the same cache staticMeshRefreshSelection fills.
float StaticMesh::getBoundingRadius()
{
    if (!m_dword30C)
        return 0.00999999978f;                       // ds:1020CE4C

    MeshBounds* const bounds = reinterpret_cast<MeshBounds*>(m_dword30C + 4);
    if (bounds->dirty)
        meshBoundsFinalize(*bounds);
    m_dword304 = *reinterpret_cast<const unsigned int*>(&bounds->radius);
    return bounds->radius;
}

// +0x48, 0x100FB310 - `mov eax, [ecx+284h]; test; jnz ret; jmp sub_100791E0`: the stored transform,
// or the image-wide shared identity (sub_100791E0 builds it once behind a flag) when none has been
// set yet.  The LightManager's gatherLightsForGeometry reads the translation off whichever comes
// back.
const Mat4* StaticMesh::getGeometryTransformation() const
{
    if (m_transform)
        return m_transform;
    return &Mat4::getIdentity();
}

// +0x4C, 0x100FB320 - `retn 8`, both arguments ignored.  dice's own diagnostic, verbatim: level 6,
// StaticMesh.cpp line 2238 (0x8BE).  This is the string that attests this file's path.
void StaticMesh::setDynamicPartTransformation(unsigned int part, const Mat4* transform)
{
    (void)part;
    (void)transform;
    ::renddx9abi::logf("StaticMesh.cpp:2238 A Static Mesh should not have any dynamic parts...");
}

// +0x68, 0x10082D30 - `add ecx, 4; jmp basic_string::operator=(const basic_string&)`.
void StaticMesh::setName(const vc71::string& value)
{
    m_name = value;
}

// +0x84, 0x100FC010.  -1 clears the override; anything else selects that one material and
// raises the flag that makes the draw submit nothing.
void StaticMesh::setMaterialOverride(int index)
{
    if (index == -1)
    {
        m_byte356  = 0;
        m_dword31C = 0;
        return;
    }
    m_byte356  = 1;
    m_dword31C = static_cast<unsigned int>(index);
}

// +0x8C, the selected lod's 516-byte material records, counted; 0 with no lod or
// no materials.
int StaticMesh::getMaterialCount() const
{
    if (!m_dword310)
        return 0;
    const unsigned char* const first = *reinterpret_cast<const unsigned char* const*>(m_dword310 + 4);
    if (!first)
        return 0;
    const unsigned char* const last = *reinterpret_cast<const unsigned char* const*>(m_dword310 + 8);
    return static_cast<int>((last - first) / 516);
}

// +0xE8, the +0x338 records, counted at their 36-byte stride.
int StaticMesh::getOccluderRecordCount() const
{
    if (!m_vector338.first)
        return 0;
    return static_cast<int>((m_vector338.last - m_vector338.first) / 36);
}

// +0xEC, the i-th +0x338 record, or null when there are none or i is past the end.
const void* StaticMesh::getOccluderRecord(unsigned int index) const
{
    if (!m_vector338.first)
        return 0;
    const unsigned int count = static_cast<unsigned int>(m_vector338.last - m_vector338.first) / 36u;
    if (!count || index >= count)
        return 0;
    return m_vector338.first + 36u * index;
}

// ILightSampleable +0x10, 0x10197EF0 - `mov al, 1; retn`.
bool StaticMesh::lightSampleableSlot10ReturnsTrue() const
{
    return true;
}


// Builds material feature bits before quality masking: texture/shadow state, point-light
// pass, and alpha mode. LOD distance can disable lightmaps.

unsigned int StaticMesh::materialShaderBits(void* material, bool pointLightPass)
{
    unsigned char* const mat = static_cast<unsigned char*>(material);
    const ShaderMaterial* const shaderMaterial =
        *reinterpret_cast<const ShaderMaterial* const*>(mat + 0x28);
    unsigned int bits = g_shaderComboFlags[shaderMaterial->shaderCombo];    // dword_1023A9D0

    typedef unsigned int (__fastcall* GetFlagsFn)(const void*, void*);
    if ((reinterpret_cast<GetFlagsFn>(m_vptr[0xC4 / sizeof(void*)])(this, 0) & 1u) != 0)
        bits &= ~8u;

    if (pointLightPass)
    {
        bits |= 0x400u;
    }
    else
    {
        if (!m_byte35A && staticMeshTemplateUsesLightmaps(m_template) && m_dword26C)
            bits |= 1u;
        typedef int (__fastcall* GetLodFn)(const void*, void*);
        const int lod = reinterpret_cast<GetLodFn>(m_vptr[0x7C / sizeof(void*)])(this, 0);
        if (static_cast<int>(staticMeshRenderer()->m_dword98) < lod)
            bits &= ~1u;
        if (m_byte355)
            bits |= 0x200u;
    }

    if (*reinterpret_cast<const unsigned int*>(mat + 0x17C) == 2u)       // m_alphaMode
        bits |= 0x800u;
    return bits;
}


// prepareView - sub_100FB3B0.

// On the current render view (Renderer +0x50, getRenderViewDup): when this instance carries a fov
// override (+0x308 != -1), read the view's fov, push the override through setAdjustedFov, and after
// the inverse refresh put the old fov back through setFov.  Two different setters for the push
// and the restore - +0x18 and +0x1C - exactly as shipped; IRenderView.h names them.

void StaticMesh::prepareView()
{
    dx9rend::IRenderView* const view = dx9rend::rendererGetRenderViewDup();    // +0x50
    float savedFov = -1.0f;
    if (*reinterpret_cast<const unsigned int*>(&m_float308) != 0xBF800000u)
    {
        savedFov = view->getFov();                                            // +0x20
        view->setAdjustedFov(m_float308);                                     // +0x18
    }

    if (m_byte358)
    {
        fullInvert(*m_transform, m_matrix290);
        m_byte358 = 0;
    }

    if (savedFov != -1.0f)
        view->setFov(savedFov);                                               // +0x1C
}


// Refreshes object transforms, camera/up vectors and this frame's shadow matrices.
// Preserve zero-multiplied terms because nonfinite inputs affect their results.

void StaticMesh::updateObjectParameters()
{
    dx9rend::Ra::RaObjectParameters& op = objectParameters();

    if (m_byte358)
    {
        fullInvert(*m_transform, m_matrix290);
        m_byte358 = 0;
    }

    dx9rend::IRenderView* const view = dx9rend::rendererGetRenderViewDup();    // +0x50
    const Mat4* const viewProjection = view->getViewProjectionMatrixPtr();    // +0x84
    reinterpret_cast<Mat4*>(op.worldViewProjection.value)->fullMult(*m_transform, *viewProjection);
    std::memcpy(op.world.value, m_transform->m, sizeof(op.world.value));

    const Mat4* const camera = view->getViewMatrixPtr();                      // +0x64
    const float* const c = &camera->m[12];
    const float* const m = m_matrix290.m;
    op.objectSpaceCamPos.value[3] = 1.0f;
    const float x = ((m[8] * c[2] + m[4] * c[1]) + c[0] * m[0]) + m[12];
    const float y = ((m[1] * c[0] + m[9] * c[2]) + m[5] * c[1]) + m[13];
    const float z = ((m[2] * c[0] + m[10] * c[2]) + m[6] * c[1]) + m[14];
    op.objectSpaceCamPos.value[2] = z;
    op.objectSpaceCamPos.value[0] = x;
    op.objectSpaceCamPos.value[1] = y;

    op.objectSpaceWorldUp.value[2] = (m[10] + m[2]) * 0.0f + m[6];
    op.objectSpaceWorldUp.value[0] = (m[8]  + m[0]) * 0.0f + m[4];
    op.objectSpaceWorldUp.value[1] = (m[9]  + m[1]) * 0.0f + m[5];

    bool casterThisFrame = false;
    if (m_dword34C)
    {
        typedef void* (__fastcall* GetDword350Fn)(const void*, void*);
        const void* const stamped =
            reinterpret_cast<GetDword350Fn>(m_vptr[0xB0 / sizeof(void*)])(this, 0);
        if (reinterpret_cast<unsigned int>(stamped) == dx9rend::rendererFrameOrdinal())   // +0x54
            casterThisFrame = true;
    }
    m_byte355 = casterThisFrame ? 1 : 0;

    if (casterThisFrame)
    {
        const unsigned char* const caster = reinterpret_cast<const unsigned char*>(m_dword34C);
        Mat4 projection = Mat4::getIdentity();
        projection.fullMult(*m_transform, *reinterpret_cast<const Mat4*>(caster + 0x2F8));
        std::memcpy(op.shadowProjMat.value, projection.m, sizeof(op.shadowProjMat.value));
        Mat4 trapezoid = Mat4::getIdentity();
        trapezoid.fullMult(*m_transform, *reinterpret_cast<const Mat4*>(caster + 0x3B8));
        std::memcpy(op.shadowTrapMat.value, trapezoid.m, sizeof(op.shadowTrapMat.value));
    }
}


// Loads the selected geometry/LOD lightmap from loose files or the level atlas.
// Failed atlas lookups retain a null cache entry for retry; editor failures mark no lightmap.

void StaticMesh::loadLightmap()
{
    if (m_byte35A && !rendererBaseByte(0x102))
        return;

    typedef int (__fastcall* IndexFn)(const void*, void*);
    const int geom = reinterpret_cast<IndexFn>(m_vptr[0x70 / sizeof(void*)])(this, 0);
    const int lod  = reinterpret_cast<IndexFn>(m_vptr[0x7C / sizeof(void*)])(this, 0);
    const unsigned int key = static_cast<unsigned int>(lod + 2 * (5 * geom));

    vc71::map_node_u32* const node = vc71::mapInsertU32(m_map260, key);
    dx9rend::EngineTexture*& slot = *reinterpret_cast<dx9rend::EngineTexture**>(node->value);
    if (slot)
        return;

    StaticMeshRenderer* const smr = staticMeshRenderer();
    if (rendererBaseByte(0x102))
    {
        vc71::string name("");
        staticMeshLightmapName(this, name);
        vc71::string path(name);
        path += ".dds";
        if (!::renddx9abi::engineFileExists(path))                           // FileManager +0x1C
        {
            m_dword26C = 0;
            m_byte35A  = 1;
            return;
        }
        dx9rend::EngineTexture* const texture = static_cast<dx9rend::EngineTexture*>(
            dx9rend::textureManagerGetTexture(name));
        slot = texture;
        if (!texture)
        {
            m_dword26C = 0;
            m_byte35A  = 1;
            return;
        }
        m_dword26C = reinterpret_cast<unsigned int>(texture);
    }
    else if (smr->m_flags60[4])
    {
        const int savedMipClass = dx9rend::textureManagerSwapDefaultMipClass(13);
        vc71::string objectName("");
        staticMeshLightmapObjectName(this, objectName);
        vc71::string directory("");
        staticMeshLightmapDirectory(directory);
        staticMeshRendererTryLoadLightmapTexture(smr, directory, objectName,
                                                 reinterpret_cast<float*>(node->value + 4),
                                                 &slot);
        dx9rend::textureManagerSwapDefaultMipClass(savedMipClass);
        if (!slot)
        {
            m_dword26C = 0;
            m_byte35A  = 1;
            return;
        }
        m_dword26C = reinterpret_cast<unsigned int>(slot);
        // sub_1000BDD0(this + 0x270, entry + 4): the four offset floats.
        const unsigned int* const offset = reinterpret_cast<const unsigned int*>(node->value + 4);
        m_float270 = *reinterpret_cast<const float*>(&offset[0]);
        m_float274 = *reinterpret_cast<const float*>(&offset[1]);
        m_dword278 = offset[2];
        m_dword27C = offset[3];
    }

    if (smr->m_flags60[1])
        loadVertexLightLists();
}


// Loads optional per-vertex .lights records into a four-byte-per-vertex buffer.
// Restores geometry/LOD selection afterward; input vertex counts must match the template.

void StaticMesh::loadVertexLightLists()
{
    const MeshTemplate* const t = static_cast<const MeshTemplate*>(
        static_cast<const StaticMeshTemplate*>(m_template));
    const unsigned int vertexCount = t->m_vertexCount;                        // template +0xB4

    if (!m_dword300)
    {
        vc71::string name("StaticMesh : ");
        name += t->getName();
        m_dword300 = reinterpret_cast<unsigned int>(
            dx9rend::createVertexBuffer(dx9rend::g_bufferManager, vertexCount, 4, name, 8));
    }
    void* const buffer = reinterpret_cast<void*>(m_dword300);
    if (!buffer)
        return;

    unsigned short* const words = static_cast<unsigned short*>(
        dx9rend::vertexBufferLock(buffer, 0, vertexCount, 0));
    std::memset(words, 0, 4u * vertexCount);

    typedef int (__fastcall* IndexFn)(const void*, void*);
    const int savedGeom = reinterpret_cast<IndexFn>(m_vptr[0x70 / sizeof(void*)])(this, 0);
    const int savedLod  = reinterpret_cast<IndexFn>(m_vptr[0x7C / sizeof(void*)])(this, 0);

    const unsigned char* const objectTemplate = static_cast<const unsigned char*>(m_template);
    const unsigned char* const geomsFirst =
        *reinterpret_cast<const unsigned char* const*>(objectTemplate + 0xE0);
    const unsigned char* const geomsLast =
        *reinterpret_cast<const unsigned char* const*>(objectTemplate + 0xE4);
    const unsigned int geomCount =
        geomsFirst ? static_cast<unsigned int>(geomsLast - geomsFirst) >> 5 : 0u;

    for (unsigned int g = 0; g < geomCount; ++g)
    {
        m_dword314 = g;
        const unsigned char* const geomRecord = geomsFirst + 32u * g;
        const unsigned char* const lodFirst =
            *reinterpret_cast<const unsigned char* const*>(geomRecord + 4);
        const unsigned char* const lodLast =
            *reinterpret_cast<const unsigned char* const*>(geomRecord + 8);
        const unsigned int lodCount =
            lodFirst ? static_cast<unsigned int>(lodLast - lodFirst) / 104u : 0u;

        for (unsigned int l = 0; l < lodCount; ++l)
        {
            m_dword318 = l;
            vc71::string name("");
            staticMeshLightmapName(this, name);
            name += ".lights";

            ::renddx9abi::EngineFileData file = ::renddx9abi::readEngineFile(name.c_str());
            if (file.ok())
            {
                const unsigned char* cursor = file.data;
                const unsigned char* const end = file.data + file.size;

                unsigned int count = 0;
                if (cursor + 4 <= end)
                    std::memcpy(&count, cursor, 4);
                cursor += 4;
                staticMeshVertexLightVectorResize(m_vector2F0, count);
                for (unsigned char* record = reinterpret_cast<unsigned char*>(m_vector2F0.first);
                     record != reinterpret_cast<unsigned char*>(m_vector2F0.last); record += 28)
                {
                    if (cursor + 28 <= end)
                        std::memcpy(record, cursor, 28);                     // 12 + 12 + 4
                    cursor += 28;
                }

                unsigned int listed = 0;
                if (cursor + 4 <= end)
                    std::memcpy(&listed, cursor, 4);
                cursor += 4;
                if (listed == vertexCount)
                {
                    unsigned short* out = words;
                    for (unsigned int v = 0; v < listed; ++v)
                    {
                        unsigned short index = 0;
                        if (cursor + 2 <= end)
                            std::memcpy(&index, cursor, 2);
                        cursor += 2;
                        out[0] |= index;
                        out[1]  = 0;
                        out += 2;
                    }
                }
                ::renddx9abi::freeEngineFile(file);
            }
        }
    }

    typedef void (__fastcall* SetIndexFn)(void*, void*, int);
    reinterpret_cast<SetIndexFn>(m_vptr[0x6C / sizeof(void*)])(this, 0, savedGeom);
    reinterpret_cast<SetIndexFn>(m_vptr[0x78 / sizeof(void*)])(this, 0, savedLod);
    reinterpret_cast<SetIndexFn>(m_vptr[0x78 / sizeof(void*)])(this, 0, -1);
    dx9rend::vertexBufferUnlock(buffer);
}


// draw - +0x18, 0x100FF510.  The dispatcher.

void StaticMesh::draw(void* renderView, void* occluder, int flag)
{
    (void)renderView;
    (void)occluder;
    (void)flag;
    if (dx9rend::rendererIsExpansionPack())                                   // +0xAC
        staticMeshDrawExpansionStop(this);
    else
        drawVanilla();
}


// Selects LOD, lighting and material techniques, then queues the mesh's visible materials.
// Requires initialized template, renderer, lighting and material tables.
// Initial visibility/enable checks skip the frame stamp; later exits retain it.

void StaticMesh::drawVanilla()
{
    dx9rend::Ra::RaObjectParameters& op = objectParameters();

    // 1. the early outs
    if (op.transparency.value < 0.0000099999997f)                         // ds:1021B0E8
        return;
    StaticMeshTemplate* const t = static_cast<StaticMeshTemplate*>(m_template);
    if (!t->m_vertexStream)                                               // template +0x154
        return;
    StaticMeshRenderer* const smr = staticMeshRenderer();
    if (!smr->m_enabled && !t->m_isRoad)
        return;
    if (t->m_isRoad && !rendererByte(0x6A4))
        return;
    if (!rendererByte(0x6A7))
    {
        world::IObject* const owner = static_cast<world::IObject*>(m_dword330);
        if (owner && owner->getIsOvergrowth())                            // IObject PE +0x12C
            return;
    }

    // 2. the camera distance
    dx9rend::IRenderView* const mainView = dx9rend::rendererGetRenderView();    // +0x48
    const Mat4* const camera = mainView->getViewMatrixPtr();                    // +0x64
    float distance;
    {
        // sub_1007AFE0: the transform's translation minus the camera's.
        const float dx = m_transform->m[12] - camera->m[12];
        const float dy = m_transform->m[13] - camera->m[13];
        const float dz = m_transform->m[14] - camera->m[14];
        distance = static_cast<float>(std::sqrt(static_cast<double>(dz * dz + dy * dy + dx * dx)));
    }
    *reinterpret_cast<float*>(&m_dword288) = distance;
    *reinterpret_cast<float*>(&op.tail220)  = distance;

    // 3. lod selection
    if (!m_byte357)
    {
        const float radius =
            reinterpret_cast<const float*>(m_geomRadii.first)[m_dword314];
        const float lodDistance = mainView->getInverseZoom() * distance - radius;   // +0x2C

        int selected = 0;
        int i = static_cast<int>(
            reinterpret_cast<const unsigned int*>(m_geomLodDistanceCounts.first)[m_dword314]) - 1;
        if (i >= 0)
        {
            const StaticMeshGeom* const geoms =
                reinterpret_cast<const StaticMeshGeom*>(t->m_geoms.first);
            const float* const switchDistances =
                reinterpret_cast<const float*>(geoms[m_dword314].lodDistances.first);
            const float scale = rendererFloat(0x8B0) * rendererFloat(0x84C);
            for (;;)
            {
                if (scale * switchDistances[i] < lodDistance)
                {
                    selected = i + 1;
                    break;
                }
                if (--i < 0)
                    break;
            }
        }

        m_dword320 = 0;
        if (rendererByte(0x708))
        {
            for (int bucket = 3; bucket >= 0; --bucket)
            {
                const float threshold = (bucket < 0 || bucket > 3)
                    ? 0.0f
                    : rendererFloat(0x70C + 4u * static_cast<unsigned int>(bucket));
                if (lodDistance > threshold)
                {
                    m_dword320 = static_cast<unsigned int>(bucket + 1);
                    break;
                }
            }
        }

        if (selected != static_cast<int>(m_dword318))
        {
            typedef void (__fastcall* SetLodFn)(void*, void*, int);
            reinterpret_cast<SetLodFn>(m_vptr[0x78 / sizeof(void*)])(this, 0, selected);
            m_byte357 = 0;
        }
    }
    if (smr->m_byte94 && !m_byte357)
    {
        typedef void (__fastcall* SetLodFn)(void*, void*, int);
        reinterpret_cast<SetLodFn>(m_vptr[0x78 / sizeof(void*)])(this, 0, 0);
        m_byte357 = 0;
    }

    // 4. the lightmap, the view, the parameters
    if (!m_dword26C && !m_byte35A)
        loadLightmap();
    prepareView();
    updateObjectParameters();

    const bool transparent = op.transparency.value < 1.0f;

    // 5. the point-light gather
    staticMeshPointerVectorResize(m_vector2D0, 0, 0);                     // sub_100E1C20(0, 0)
    if (!transparent)
    {
        const int maxLights = g_LightManager->getMaxLightsPerStaticMesh();          // +0x68
        const unsigned int required = m_dword26C ? 0x11B09u : 0x1B09u;
        g_LightManager->gatherLightsForGeometry(this, m_vector2D0,
                                                static_cast<unsigned int>(maxLights),
                                                required, 0u, true);                // +0x20
    }

    const unsigned int lightCount = m_vector2D0.first
        ? static_cast<unsigned int>(m_vector2D0.last - m_vector2D0.first) / sizeof(void*)
        : 0u;
    const bool hasLights = m_vector2D0.first && lightCount != 0 && !transparent;
    m_byte354 = hasLights ? 1 : 0;

    if (hasLights)
    {
        // The fill record, built on the stack exactly as sub_10046540 builds one: fourteen zero
        // words, then 1.0f at +0x3C and +0x40, +0x38 untouched.
        dx9rend::Ra::RaLightRecord fill;
        std::memset(&fill, 0, 0x38);
        fill.float3C = 1.0f;
        fill.float40 = 1.0f;
        staticMeshLightRecordVectorResize(m_vector2E0, lightCount, fill);

        world::LightSource** const lights =
            reinterpret_cast<world::LightSource**>(m_vector2D0.first);
        dx9rend::Ra::RaLightRecord* const records =
            reinterpret_cast<dx9rend::Ra::RaLightRecord*>(m_vector2E0.first);
        const float* const m = m_matrix290.m;
        for (unsigned int k = 0; k < lightCount; ++k)
        {
            world::LightSource* const light = lights[k];
            dx9rend::Ra::RaLightRecord& record = records[k];

            const float* const p = &light->getAbsoluteTransformation().m[12];   // PE +0x80
            const float z = ((p[2] * m[10] + m[6] * p[1]) + m[2] * p[0]) + m[14];
            const float y = ((m[1] * p[0] + p[2] * m[9]) + p[1] * m[5]) + m[13];
            const float x = ((m[4] * p[1] + m[0] * p[0]) + p[2] * m[8]) + m[12];
            record.position[0] = x;
            record.position[1] = y;
            record.position[2] = z;

            const float range = light->getAttenuationRange2();                   // PE +0x190
            record.invRangeSq = 1.0f / (range * range);

            const Vec3& color = light->getColor();                               // PE +0x198
            record.color[0] = color.x;
            record.color[1] = color.y;
            record.color[2] = color.z;
        }
    }

    // 6. the sun direction in object space
    float sunDirection[3] = { 0.0f, 0.0f, 0.0f };
    {
        world::LightSource* const sky = g_LightManager->getSkyLight();         // +0x28
        if (sky)
        {
            const Vec3& d = sky->getDirection();                               // PE +0x17C
            const float* const m = m_matrix290.m;
            sunDirection[0] = (m[4] * d.y + m[8] * d.z) + d.x * m[0];
            sunDirection[1] = (m[1] * d.x + m[5] * d.y) + m[9] * d.z;
            sunDirection[2] = (m[2] * d.x + m[6] * d.y) + m[10] * d.z;
        }
    }

    // 7. the materials
    if (!m_byte356)
    {
        unsigned int maskIndex = rendererDword(0x6CC);
        if (rendererByte(0x6D0))
        {
            const unsigned int maskCount = g_staticMeshQualityMasks.first
                ? static_cast<unsigned int>(g_staticMeshQualityMasks.last
                                            - g_staticMeshQualityMasks.first) >> 2
                : 0u;
            // `count` when the lod is past the end - one element past the vector.  Shipped.
            maskIndex = (maskCount < m_dword318) ? maskCount : m_dword318;
        }
        else if (rendererByte(0x708) && m_dword320 > maskIndex)
        {
            maskIndex = m_dword320;
        }
        const unsigned int qualityMask =
            reinterpret_cast<const unsigned int*>(g_staticMeshQualityMasks.first)[maskIndex];

        unsigned char* const materialsFirst =
            *reinterpret_cast<unsigned char**>(m_dword310 + 4);
        const unsigned int materialCount = materialsFirst
            ? static_cast<unsigned int>(*reinterpret_cast<unsigned char**>(m_dword310 + 8)
                                        - materialsFirst) / 516u
            : 0u;

        for (unsigned int i = 0; i < materialCount; ++i)
        {
            m_dword31C = i;
            unsigned char* const material =
                *reinterpret_cast<unsigned char**>(m_dword310 + 4) + 516u * i;

            if (smr->m_flags60[0])
            {
                StaticMeshBoundsCorners corners(
                    *reinterpret_cast<const MeshBounds*>(material + 0x198), *m_transform);
                const Frustum* const frustum =
                    dx9rend::rendererGetRenderViewDup()->getFrustumPtr();        // +0x50, +0x74
                if (corners.outside(*frustum))
                {
                    ++smr->m_culledMaterials;
                    continue;
                }
            }

            dx9rend::RenderQueueEntry* const entry = dx9rend::renderEntryPoolAcquire();
            entry->materialParameters = material + 0x2C;
            entry->material           = material;
            entry->objectParameters   = &op;

            dx9rend::Ra::RaLightRecord* const sun =
                static_cast<dx9rend::Ra::RaLightRecord*>(entry->lightArray.lights.value);
            {
                const Vec3& sunColor = g_LightManager->getStaticSunColor();      // +0x1D8
                sun->color[0] = sunColor.x;
                sun->color[1] = sunColor.y;
                sun->color[2] = sunColor.z;
                sun->direction[0] = sunDirection[0];
                sun->direction[1] = sunDirection[1];
                sun->direction[2] = sunDirection[2];
            }

            if (t->m_isWater && g_terrain && m_dword26C)
            {
                // the water arm
                std::memcpy(op.world.value, m_transform->m, sizeof(op.world.value));
                std::memcpy(op.viewProjection.value,
                            dx9rend::rendererGetRenderViewDup()->getViewProjectionMatrixPtr()->m,
                            sizeof(op.viewProjection.value));
                dx9rend::EngineTexture* const lightmap =
                    reinterpret_cast<dx9rend::EngineTexture*>(m_dword26C);
                if (lightmap)
                {
                    staticMeshSwapD3DTexture(entry->lighting.lightMap.value, lightmap->baseTexture);
                    staticMeshStoreLightMapOffset(entry, this);
                }

                const unsigned char* const terrain = static_cast<const unsigned char*>(g_terrain);
                dx9rend::Ra::RaShader* const nearShader =
                    *reinterpret_cast<dx9rend::Ra::RaShader* const*>(terrain + 0x718);
                dx9rend::Ra::RaShader* const farShader =
                    *reinterpret_cast<dx9rend::Ra::RaShader* const*>(terrain + 0x71C);
                const float threshold = *reinterpret_cast<const float*>(terrain + 0x70C);
                MeshBounds* const bounds = reinterpret_cast<MeshBounds*>(m_dword30C + 4);
                if (bounds->dirty)
                    meshBoundsFinalize(*bounds);
                staticMeshMaterialShader(material) =
                    (distance - bounds->radius <= threshold) ? nearShader : farShader;

                // The material block's CubeMap holder (material + 0x148 / +0x14C): the terrain's
                // +0x6C0 object's texture, else the LightManager's cube at its +0x248.
                if (!*reinterpret_cast<void**>(material + 0x14C))
                {
                    const unsigned char* const terrainCube =
                        *reinterpret_cast<const unsigned char* const*>(terrain + 0x6C0);
                    dx9rend::EngineTexture* const cube = terrainCube
                        ? *reinterpret_cast<dx9rend::EngineTexture* const*>(terrainCube + 0x10)
                        : *reinterpret_cast<dx9rend::EngineTexture* const*>(
                              reinterpret_cast<const unsigned char*>(g_LightManager) + 0x248);
                    staticMeshAssignTexture(*reinterpret_cast<void**>(material + 0x14C), cube);
                }
                entry->shader = staticMeshMaterialShader(material);
                dx9rend::renderQueueAddToSlot(9, entry);                         // RSMTWater, +0x28
            }
            else if (t->m_isRoad)
            {
                // the road arm
                std::memcpy(op.world.value, m_transform->m, sizeof(op.world.value));
                std::memcpy(op.viewProjection.value,
                            dx9rend::rendererGetRenderViewDup()->getViewProjectionMatrixPtr()->m,
                            sizeof(op.viewProjection.value));
                dx9rend::EngineTexture* const scratch = *reinterpret_cast<dx9rend::EngineTexture**>(
                    reinterpret_cast<unsigned char*>(dx9rend::g_Renderer) + 0x774);
                if (scratch)
                    staticMeshSwapD3DTexture(entry->lighting.lightMap.value, scratch->baseTexture);

                dx9rend::Ra::RaShader* shader;
                if (*reinterpret_cast<const void* const*>(material + 0xF0) == 0)   // DetailMap.value
                    shader = staticMeshFindShaderBits(0xF1000000u);
                else if (t->m_roadNoBlend)
                    shader = staticMeshFindShaderBits(0xF4000000u);
                else
                    shader = staticMeshFindShaderBits(0xF2000000u);
                staticMeshMaterialShader(material) = shader;
                entry->shader = shader;
                dx9rend::renderQueueAddToSlot(8, entry);                         // RSMTRoad, +0x24
            }
            else
            {
                // the ordinary arm
                const unsigned int bits = materialShaderBits(material, false);
                if (!t->m_isVegetation)
                {
                    staticMeshMaterialShader(material) = staticMeshFindShaderBits(qualityMask & bits);
                }
                else if (material[0x1EC])                                      // m_vegetationNonTiled
                {
                    const float f = m_float25C;
                    const Vec3& treeSun = g_LightManager->getTreeSunColor();     // +0x1C8
                    const float sx = f * treeSun.x;
                    const float sy = f * treeSun.y;
                    const float sz = f * treeSun.z;
                    sun->color[2] = sz * f;
                    sun->color[0] = sx * f;
                    sun->color[1] = sy * f;
                    staticMeshMaterialShader(material) = staticMeshFindShaderBits(
                        qualityMask & (m_byte355 ? 0xF0000200u : 0xF0000000u));

                    const Vec3& ambient = g_LightManager->getTreeAmbientColor();  // +0x40
                    const float scaled[3] = { f * ambient.x, f * ambient.y, f * ambient.z };
                    const float* const packed = staticMeshPackVec4(scaled, 1.0f);
                    entry->lighting.overGrowthAmbient.value[0] = packed[0];
                    entry->lighting.overGrowthAmbient.value[1] = packed[1];
                    entry->lighting.overGrowthAmbient.value[2] = packed[2];
                    entry->lighting.overGrowthAmbient.value[3] = packed[3];

                    IBase* const found =
                        classManager->getSingleton(renddx9abi::HostString("WindManager"));
                    world::WindManager* const wind =
                        static_cast<world::WindManager*>(static_cast<void*>(found));
                    op.windSpeed.value = wind->getWindSpeed(
                        *reinterpret_cast<const Vec3*>(&m_transform->m[12]));     // +0x30
                    std::memcpy(op.world.value, m_transform->m, sizeof(op.world.value));
                }
                else
                {
                    unsigned int key = m_byte355 ? 0x200u : 0u;
                    key |= (bits & 4u) ? 0xF9000000u : 0xFA000000u;
                    staticMeshMaterialShader(material) = staticMeshFindShaderBits(qualityMask & key);

                    const float f2 = m_float25C * m_float25C;
                    sun->color[0] *= f2;
                    sun->color[1] *= f2;
                    sun->color[2] *= f2;
                    sun->scaled24 *= f2;

                    const Vec3& skyColor = g_LightManager->getStaticSkyColor();   // +0x1E8
                    const float f = m_float25C;
                    const float scaled[3] = { f * skyColor.x, f * skyColor.y, f * skyColor.z };
                    const float* const packed = staticMeshPackVec4(scaled, 1.0f);
                    entry->lighting.overGrowthAmbient.value[0] = packed[0];
                    entry->lighting.overGrowthAmbient.value[1] = packed[1];
                    entry->lighting.overGrowthAmbient.value[2] = packed[2];
                    entry->lighting.overGrowthAmbient.value[3] = packed[3];
                }

                dx9rend::EngineTexture* const lightmap =
                    reinterpret_cast<dx9rend::EngineTexture*>(m_dword26C);
                if (lightmap && (bits & 1u) != 0)
                {
                    staticMeshSwapD3DTexture(entry->lighting.lightMap.value, lightmap->baseTexture);
                    staticMeshStoreLightMapOffset(entry, this);
                }

                entry->shader = staticMeshMaterialShader(material);
                dx9rend::renderQueueEnqueue(entry, transparent, false, false);

                if (m_byte354)
                {
                    // the point-light pass: one entry per gathered light
                    dx9rend::Ra::RaShader* pointShader;
                    if (material[0x1EC])
                    {
                        pointShader = staticMeshFindShaderBits(
                            m_byte355 ? 0xF0000600u : 0xF0000400u);
                    }
                    else
                    {
                        const ShaderMaterial* const shaderMaterial =
                            *reinterpret_cast<const ShaderMaterial* const*>(material + 0x28);
                        unsigned int pointBits = g_shaderComboFlags[shaderMaterial->shaderCombo];
                        typedef unsigned int (__fastcall* GetFlagsFn)(const void*, void*);
                        if ((reinterpret_cast<GetFlagsFn>(m_vptr[0xC4 / sizeof(void*)])(this, 0)
                             & 1u) != 0)
                            pointBits &= ~8u;
                        pointBits |= 0x400u;
                        if (*reinterpret_cast<const unsigned int*>(material + 0x17C) == 2u)
                            pointBits |= 0x800u;
                        pointShader = staticMeshFindShaderBits(qualityMask & pointBits);
                    }
                    staticMeshMaterialShader(material) = pointShader;

                    for (unsigned int k = 0; ; ++k)
                    {
                        if (!m_vector2E0.first)
                            break;
                        const unsigned int recordCount =
                            static_cast<unsigned int>(m_vector2E0.last - m_vector2E0.first)
                            / sizeof(dx9rend::Ra::RaLightRecord);
                        if (k >= recordCount)
                            break;

                        dx9rend::RenderQueueEntry* const lightEntry =
                            dx9rend::renderEntryPoolAcquire();
                        dx9rend::Ra::RaLightRecord* const destination =
                            static_cast<dx9rend::Ra::RaLightRecord*>(
                                lightEntry->lightArray.lights.value);
                        lightEntry->material           = material;
                        lightEntry->materialParameters = material + 0x2C;
                        lightEntry->objectParameters   = &op;
                        lightEntry->shader             = staticMeshMaterialShader(material);
                        *destination = reinterpret_cast<const dx9rend::Ra::RaLightRecord*>(
                            m_vector2E0.first)[k];
                        dx9rend::renderQueueEnqueue(lightEntry, transparent, false, true);
                    }
                }
            }
        }
    }

    // 8. the frame stamp
    m_dword20 = dx9rend::rendererFrameOrdinal();                              // +0x54
}


// Shadow-map and silhouette drawing helpers. Profiling-only brackets are omitted.

namespace {

// sub_100FB440's two frustum sources and the renderer's cull counter, shared with the draw.
bool staticMeshCornersOutside(const void* material, const Mat4& world, const Frustum& frustum)
{
    StaticMeshBoundsCorners corners(
        *reinterpret_cast<const MeshBounds*>(static_cast<const unsigned char*>(material) + 0x198),
        world);
    return corners.outside(frustum);
}

// The renderer's +0x6C statistics block, its +0x25C word: every shadow draw adds the LOD's
// primitive count to it, whatever it actually submitted.
void staticMeshCountShadowPrimitives(unsigned int primitives)
{
    dx9rend::IRenderer* const renderer =
        static_cast<dx9rend::IRenderer*>(dx9rend::g_rendererBase);
    unsigned char* const stats = static_cast<unsigned char*>(renderer->getRenderStats());
    *reinterpret_cast<unsigned int*>(stats + 0x25C) += primitives;
}

// `mov eax, [effect+8]; call [[eax]+104h]` - ID3DXEffect::CommitChanges on the wrapper's D3DX
// effect.  D3dxEffect.h names the slot; the wrapper's +0x08 is the interface pointer.
void staticMeshEffectCommitChanges(dx9rend::Effect* effect)
{
    void* const d3dx = *reinterpret_cast<void**>(reinterpret_cast<unsigned char*>(effect) + 8);
    reinterpret_cast<::renddx9abi::D3dxCommitChangesFn>(
        (*reinterpret_cast<void***>(d3dx))[0x104 / sizeof(void*)])(d3dx);
}

dx9rend::ShaderParameter* staticMeshShadowParameter(dx9rend::Effect* effect, const char* name)
{
    return dx9rend::effectGetShaderParameterTyped(effect, vc71::string(name));
}

// dword_10239698 - the shadow draw's mode word, 2 in .data and never written by the image:
//     0  merge the lod's index ranges and draw the whole lod in one call
//     1  draw the last lod's first material only
//     2  draw the selected lod material by material, alpha-tested ones through pass 1
// [INFERRED] name; kept as the mutable word it is.
unsigned int g_staticMeshShadowDrawMode = 2;

} // anonymous namespace

// sub_100FB440.  `retn 8`.
bool StaticMesh::materialVisible(void* material, const Frustum* frustum)
{
    StaticMeshRenderer* const smr = staticMeshRenderer();
    if (!smr->m_flags60[0])
        return true;
    const Frustum* const test =
        frustum ? frustum : dx9rend::rendererGetRenderViewDup()->getFrustumPtr();   // +0x50, +0x74
    if (!staticMeshCornersOutside(material, *m_transform, *test))
        return true;
    ++smr->m_culledMaterials;
    return false;
}


// Draws shadow geometry with the supplied caster or this frame's cached caster.
// Temporarily applies the shadow LOD and restores -1 afterward. Alpha texture selection
// and primitive statistics preserve the compatibility path's per-LOD behavior.

void StaticMesh::drawShadowMap(void* caster, unsigned int, unsigned int, unsigned int,
                               unsigned int, unsigned int)
{
    StaticMeshTemplate* const t = static_cast<StaticMeshTemplate*>(m_template);
    if (!t->m_vertexStream)                                               // +0x154
        return;
    if (!m_transform)
        return;
    if (!t->m_indexBuffer)                                                // +0x9C
        return;
    if (reinterpret_cast<const StaticMeshLod*>(m_dword30C)->hasAlphaMaterial)   // +0x54
        return;

    bool fromOwnCaster = false;
    const unsigned char* casterRecord = static_cast<const unsigned char*>(caster);
    const Mat4* lightMatrix;
    if (!casterRecord)
    {
        typedef void* (__fastcall* GetDword350Fn)(const void*, void*);
        const void* const stamped =
            reinterpret_cast<GetDword350Fn>(m_vptr[0xB0 / sizeof(void*)])(this, 0);
        if (reinterpret_cast<unsigned int>(stamped) != dx9rend::rendererFrameOrdinal())   // +0x54
            return;
        casterRecord  = reinterpret_cast<const unsigned char*>(m_dword34C);
        lightMatrix   = reinterpret_cast<const Mat4*>(casterRecord + 0x238);
        fromOwnCaster = true;
    }
    else
    {
        lightMatrix = reinterpret_cast<const Mat4*>(casterRecord + 0x1F8);
    }

    const int shadowLod  = g_LightManager->getStaticMeshShadowLod();     // +0x280
    typedef int (__fastcall* GetIndexFn)(const void*, void*);
    const int currentLod = reinterpret_cast<GetIndexFn>(m_vptr[0x7C / sizeof(void*)])(this, 0);
    const int lodCount   = reinterpret_cast<GetIndexFn>(m_vptr[0x80 / sizeof(void*)])(this, 0);
    if (currentLod != shadowLod)
    {
        int lod = lodCount - 1;
        if (lod >= shadowLod)
            lod = shadowLod;
        typedef void (__fastcall* SetLodFn)(void*, void*, int);
        reinterpret_cast<SetLodFn>(m_vptr[0x78 / sizeof(void*)])(this, 0, lod);
    }
    // setLod rewrites +0x30C; every read below is of the lod now selected (the shipped body
    // reloads the word at each use).
    StaticMeshLod* const lodRecord = reinterpret_cast<StaticMeshLod*>(m_dword30C);

    StaticMeshRenderer* const smr = staticMeshRenderer();
    dx9rend::Effect* const effect = smr->m_shadowEffect;                  // +0x44

    staticMeshShadowParameter(effect, "World")->setValue(m_transform, 0);
    {
        const float* const packed = staticMeshPackVec4(
            reinterpret_cast<const float*>(&t->m_decompressScale), 1.0f);    // template +0x138
        staticMeshShadowParameter(effect, "POSUNPACK")->setValue(packed, 0);
    }
    staticMeshShadowParameter(effect, "TEXUNPACK")->setValue(&t->m_uvDecompress, 0);  // +0x150

    if (casterRecord && casterRecord[0x495])
    {
        Mat4 light = Mat4::getIdentity();
        light.fullMult(*reinterpret_cast<const Mat4*>(casterRecord + 0x238),
                       *reinterpret_cast<const Mat4*>(casterRecord + 0x278));
        staticMeshShadowParameter(effect, "vpLightMat")->setValue(&light, 0);
        staticMeshShadowParameter(effect, "vpLightTrapezMat")->setValue(&light, 0);
    }
    else
    {
        staticMeshShadowParameter(effect, "vpLightMat")->setValue(lightMatrix, 0);
        staticMeshShadowParameter(effect, "vpLightTrapezMat")->setValue(casterRecord + 0x278, 0);
    }
    staticMeshEffectCommitChanges(effect);                                    // ID3DXEffect +0x104

    dx9rend::vertexBufferBindStream(t->m_vertexStream, 0);
    dx9rend::indexBufferBind(t->m_indexBuffer);
    dx9rend::vertexDeclarationBind(t->m_compressVertexData ? smr->m_decl34 : smr->m_decl2C);

    const int mergeLimit = g_LightManager->getShadowMergeLodLimit();       // +0x2D8
    bool drawLastLodMaterial = false;
    bool done = false;

    if (shadowLod <= mergeLimit && g_staticMeshShadowDrawMode == 0u)
    {
        t->mergeShadowIndexRanges();
        if (g_staticMeshShadowDrawMode == 0u)
        {
            if (lodRecord->byte5C)
            {
                dx9rend::vertexBufferBindStream(t->m_vertexStream, 0);
                dx9rend::indexBufferBind(t->m_indexBuffer);
                dx9rend::vertexDeclarationBind(
                    t->m_compressVertexData ? smr->m_decl34 : smr->m_decl2C);
                dx9rend::rendererDrawIndexedPrimitive(
                    lodRecord->mergedPrimitiveCount, t->m_primitiveType,
                    static_cast<int>(lodRecord->vertexStart), 0,
                    lodRecord->mergedVertexCount, 0);
                staticMeshCountShadowPrimitives(lodRecord->mergedPrimitiveCount);   // +0x60 here
                done = true;
            }
            else
            {
                drawLastLodMaterial = true;
            }
        }
    }

    if (!done && !drawLastLodMaterial)
    {
        const unsigned int mode = g_staticMeshShadowDrawMode;
        if (mode == 1u || !lodRecord->byte5C)
        {
            drawLastLodMaterial = true;
        }
        else if (mode == 2u)
        {
            const unsigned int materialCount = static_cast<unsigned int>(getMaterialCount());
            int  currentPass  = -1;
            bool streamsBound = false;
            for (unsigned int i = 0; i < materialCount; ++i)
            {
                unsigned char* const material =
                    *reinterpret_cast<unsigned char**>(m_dword310 + 4) + 516u * i;
                const ShaderMaterial* const shaderMaterial =
                    *reinterpret_cast<const ShaderMaterial* const*>(material + 0x28);
                const unsigned int alphaMode =
                    *reinterpret_cast<const unsigned int*>(material + 0x17C);
                int pass;
                bool skipPassSwitch = false;
                if (!fromOwnCaster && alphaMode == 2u)
                {
                    pass = 1;
                    const bool detailCombo = !t->m_isVegetation
                        && shaderMaterial->shaderCombo != 10u && shaderMaterial->shaderCombo != 0u;
                    const void* const tested =
                        detailCombo ? shaderMaterial->textures[2] : shaderMaterial->textures[1];
                    if (tested)
                    {
                        staticMeshShadowParameter(effect, "TEXLAYER0")->setValue(
                            shaderMaterial->textures[1], 0);                   // [1], as shipped
                    }
                    if (!streamsBound)
                    {
                        if (detailCombo)
                        {
                            dx9rend::vertexDeclarationBind(smr->m_decl30);       // +0x30
                            dx9rend::vertexBufferBindStream(
                                t->m_uvStreams[t->m_streamIndexA], 1);           // +0x158[+0x170]
                        }
                        streamsBound = true;
                    }
                }
                else
                {
                    pass = 0;
                    skipPassSwitch = fromOwnCaster;
                }

                if (!skipPassSwitch && pass != currentPass)
                {
                    currentPass = pass;
                    dx9rend::effectBeginPass(effect, static_cast<unsigned int>(pass));
                }

                StaticMeshMaterial* const typed = reinterpret_cast<StaticMeshMaterial*>(material);
                dx9rend::rendererDrawIndexedPrimitive(
                    static_cast<unsigned int>(typed->getPrimitiveCount()),     // +0x10
                    static_cast<unsigned int>(typed->getDword1F0()),           // +0x14
                    typed->getVertexStart(),                                   // +0x18
                    static_cast<unsigned int>(typed->slot_0x1C()),             // +0x1C
                    static_cast<unsigned int>(typed->getVertexCount()),        // +0x08
                    static_cast<unsigned int>(typed->getIndexStart()));        // +0x20
                staticMeshCountShadowPrimitives(lodRecord->primitiveCount);
            }
        }
    }

    if (drawLastLodMaterial)
    {
        const int lastLod = reinterpret_cast<GetIndexFn>(m_vptr[0x80 / sizeof(void*)])(this, 0) - 1;
        const StaticMeshGeom& geom =
            reinterpret_cast<const StaticMeshGeom*>(t->m_geoms.first)[m_dword314];
        const StaticMeshLod* const lastRecord =
            reinterpret_cast<const StaticMeshLod*>(geom.lods.first) + lastLod;
        const vc71::vector_layout* const lodRecords = reinterpret_cast<const vc71::vector_layout*>(
            reinterpret_cast<const vc71::vector_layout*>(t->m_materials.first)[m_dword314].first);
        const StaticMeshMaterial* const material =
            reinterpret_cast<const StaticMeshMaterial*>(lodRecords[lastLod].first);
        const unsigned int alphaMode = material->m_alphaMode;
        if (alphaMode != 2u && alphaMode != 1u)
        {
            dx9rend::rendererDrawIndexedPrimitive(
                material->m_primitiveCount, t->m_primitiveType,
                static_cast<int>(material->m_vertexStart), 0,
                material->m_vertexCount, material->m_indexStart);
            staticMeshCountShadowPrimitives(lastRecord->primitiveCount);
        }
    }

    if (currentLod != shadowLod)
    {
        typedef void (__fastcall* SetLodFn)(void*, void*, int);
        reinterpret_cast<SetLodFn>(m_vptr[0x78 / sizeof(void*)])(this, 0, -1);
    }
}

// 0x100FDC20 as the template's createInstance reaches it.
StaticMesh* staticMeshConstruct(void* storage, void* objectTemplate)
{
    if (!storage)
        return 0;
    return new (storage) StaticMesh(objectTemplate);
}

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
