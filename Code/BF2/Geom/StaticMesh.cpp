// Static-mesh instance rendering, visibility and shadow state.


#include "../../RendDX9Abi/ThunkAbi.h"
#include "StaticMesh.h"
#include "../Core/StringUtil.h"         // formatString (sub_10074AB0)

#ifdef BF2_IMAGE_RENDDX9
#if !defined(_MSC_VER)
// [CHOSEN, not recovered] Compiler-typed native table entries.
#include "../../RendDX9Abi/NativeVtable.h"
#include "LightmapGeneration/ILightSampleable.h"
#endif

#include "../../RendDX9Abi/BfMemory.h"        // engineAlloc - the map sentinel
#include "../../RendDX9Abi/D3dx9Sdk.h"        // ID3DXEffect - the shadow draw's commit
#include "../../RendDX9Abi/EngineFile.h"      // engineFileExists / readEngineFile
#include "../../RendDX9Abi/Frontier.h"
#include "../../RendDX9Abi/PluginEntry.h"     // classManager - the WindManager lookup
#include "../Core/Ids.h"                      // IID_IGeometry / IRenderable / ILightSampleable
#include "../Game/Main/IVideoOptions.h"
#include "../Game/Objects/IObject.h"          // the owner's getIsOvergrowth
#include "../Game/Objects/ObjectLayoutAccess.h"   // staticMeshRootParent's IObject prefix fields
#include "../Game/Objects/Visual/LightSource.h"
#include "../Math/Frustum.h"
#include "../RendDX9/RendDX9Math.h"       // the RendDX9.dll Math copies
#include "../Math/Plane3.h"
#include "../Math/Vec3.h"
#include "../Platform/X87Compat.h"
#include "../RendDX9/Ra/RaShader.h"           // raObjectParametersConstruct, RenderQueueEntry
#include "../RendDX9/RendDX9Base.h"
#include "../RendDX9/Texture.h"               // textureReleaseRef - the lightmap cache
#include "../RendMisc/IRenderView.h"
#include "../RendMisc/IRenderer.h"            // getRenderStats - the shadow draws' +0x25C counter
#include "../Scene/Object/WindManager.h"      // the vegetation arm's wind speed
#include "LightmapGeneration/ILightSampleable.h"  // the +0x30 table's interface (ILightSampling)
#include "LightmapGeneration/MeshLightSampling.h" // the sampler createLightSampler makes
#include "BoxCorners.h"                       // the eight-corner material box test
#include "MeshTemplate.h"                     // MeshBounds / meshBoundsFinalize
#include "StaticMeshRenderer.h"               // the singleton's flags, counters and atlases
#include "StaticMeshTemplate.h"               // the tail gate reads MeshTemplate::m_loaded
#include "Terrain.h"                          // the water arm's techniques and envmap
#include "XpLightManager.h"                   // the light gather and the five colour getters

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

namespace dice {
namespace hfe {

namespace dx9rend {
namespace Ra { unsigned char effectManagerFlag40(); }
// BufferManager.cpp.
void* indexBufferLock(void* wrapper, unsigned int offsetIndices, unsigned int countIndices,
                      unsigned int flags);
bool  indexBufferUnlock(void* wrapper);
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
                         const std::string& name, unsigned int usage);
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
void* effectD3dxHandle(void* effect);   // Effect.cpp - the wrapper's +0x08 ID3DXEffect*
}

namespace geom {

extern LightManager* g_LightManager;      // XpLightManager.cpp, dword_1024B914
extern IBase*        g_StaticMeshRenderer; // StaticMeshRenderer.cpp, dword_1024B8F8
extern IBase*        g_Terrain;            // ModuleGeom.cpp, dword_1024B930

namespace {


// The primary table has 63 slots; ILightSampleable has seven.
// Explicit tables preserve the ABI while remaining interface signatures are unknown.
void RD9_THUNK sm_drawSilhouette(void* self RD9_THUNK_EDX, void* unused, int legacyArm)
{
    static_cast<StaticMesh*>(self)->drawSilhouette(unused, legacyArm);
}

// +0x20, the shadow-map draw.  `retn 18h`.
void RD9_THUNK sm_drawShadowMap(void* self RD9_THUNK_EDX, void* caster, unsigned int a2,
                                 unsigned int a3, unsigned int a4, unsigned int a5,
                                 unsigned int a6)
{
    static_cast<StaticMesh*>(self)->drawShadowMap(caster, a2, a3, a4, a5, a6);
}

void staticMeshDrawExpansion(void* self);

// the written slots, as the thin adapters the tables need

// +0x34, the scalar deleting destructor. Bit 0 of the flag means "and
// free the storage", which is the MSVC convention release() relies on.
void* RD9_THUNK sm_deletingDestructor(void* self RD9_THUNK_EDX, int flags)
{
    static_cast<StaticMesh*>(self)->~StaticMesh();
    if ((flags & 1) != 0)
        ::renddx9abi::engineFree(self);
    return self;
}

int RD9_THUNK sm_addRef(void* self RD9_THUNK_EDX)
{
    return static_cast<StaticMesh*>(self)->addRef();
}

int RD9_THUNK sm_getRef(void* self RD9_THUNK_EDX)
{
    return static_cast<StaticMesh*>(self)->getRef();
}

int RD9_THUNK sm_release(void* self RD9_THUNK_EDX)
{
    return static_cast<StaticMesh*>(self)->release();
}

void* RD9_THUNK sm_queryInterface(void* self RD9_THUNK_EDX, unsigned int iid)
{
    return static_cast<StaticMesh*>(self)->queryInterface(iid);
}

// +0x18, the draw.  `retn 0Ch`: renderView, occluder, flag.
void RD9_THUNK sm_draw(void* self RD9_THUNK_EDX, void* renderView, void* occluder, int flag)
{
    static_cast<StaticMesh*>(self)->draw(renderView, occluder, flag);
}

// +0x14, 0x10082D10 - `fld -1.0; retn`.  Shared with the base family (fifteen constructors
// install the base table this slot comes from).
float RD9_THUNK sm_slot14(void* self RD9_THUNK_EDX)
{
    return static_cast<const StaticMesh*>(self)->slot14ReturnsMinusOne();
}

// +0x30, 0x1005DA00 - `xor al,al; retn 4`.
bool RD9_THUNK sm_slot30(void* self RD9_THUNK_EDX, int argument)
{
    return static_cast<const StaticMesh*>(self)->slot30ReturnsFalse(argument);
}

// +0x3C and +0xC8, 0x10088060 - `xor eax,eax; retn 4`.  One body, two slots, one arity.
int RD9_THUNK sm_slot3C(void* self RD9_THUNK_EDX, int argument)
{
    return static_cast<const StaticMesh*>(self)->slot3CReturnsZero(argument);
}

// +0x44, the bounding radius.
float RD9_THUNK sm_getBoundingRadius(void* self RD9_THUNK_EDX)
{
    return static_cast<StaticMesh*>(self)->getBoundingRadius();
}

// +0x48, the transform, or the shared identity.
const Mat4* RD9_THUNK sm_getGeometryTransformation(void* self RD9_THUNK_EDX)
{
    return static_cast<const StaticMesh*>(self)->getGeometryTransformation();
}

// +0x4C, 0x100FB320 - `retn 8`.
void RD9_THUNK sm_setDynamicPartTransformation(void* self RD9_THUNK_EDX, unsigned int part,
                                                const Mat4* transform)
{
    static_cast<StaticMesh*>(self)->setDynamicPartTransformation(part, transform);
}

void RD9_THUNK sm_setGeometryTransformation(void* self RD9_THUNK_EDX, const Mat4* transform)
{
    static_cast<StaticMesh*>(self)->setGeometryTransformation(transform);
}

// +0x54, +0x60, +0x94, +0x98 - 0x100E1480, `retn 8`: an empty virtual taking two dwords.  The
// one body sits in four slots; its arity is its own, so the four adapters are one function.
void RD9_THUNK sm_emptyTwoArgs(void* RD9_THUNK_EDX, unsigned int, unsigned int)
{
}

// +0x58, +0x64, +0xB4, +0xD4, +0xDC - 0x100E13B0, `retn 4`: an empty virtual taking one dword.
void RD9_THUNK sm_emptyOneArg(void* RD9_THUNK_EDX, unsigned int)
{
}

// +0x5C, +0xA0, +0xCC - 0x100ED3B0, `xor eax,eax; retn`: zero, no arguments.  +0xCC is the
// "next geometry in the chain" slot the object walkers follow; see getNextGeometry.
int RD9_THUNK sm_returnsZero(void* RD9_THUNK_EDX)
{
    return 0;
}

// +0x68, 0x10082D30 - `add ecx, 4; jmp basic_string::operator=(const basic_string&)`: the
// name at +0x04 assigned from the argument.  The argument is a VC7.1 std::string in the
// shipped image; whether the host ever calls this slot (with its own string layout) is not known -
// no recovered host body does - so it is typed as the image types it.
void RD9_THUNK sm_setName(void* self RD9_THUNK_EDX, const std::string* value)
{
    static_cast<StaticMesh*>(self)->setName(*value);
}

// +0x84, 0x100FC010 - `retn 4`.
void RD9_THUNK sm_setMaterialOverride(void* self RD9_THUNK_EDX, int index)
{
    static_cast<StaticMesh*>(self)->setMaterialOverride(index);
}

// +0x8C
int RD9_THUNK sm_getMaterialCount(void* self RD9_THUNK_EDX)
{
    return static_cast<const StaticMesh*>(self)->getMaterialCount();
}

// +0x90, 0x10083AB0 - `retn 0Ch`: an empty virtual taking three dwords.
void RD9_THUNK sm_emptyThreeArgs(void* RD9_THUNK_EDX, unsigned int, unsigned int, unsigned int)
{
}

// +0xA8, +0xD0, +0xD8 - 0x10083AF0, `xor al,al; retn`: false, no arguments.
bool RD9_THUNK sm_returnsFalse(void* RD9_THUNK_EDX)
{
    return false;
}

// +0xE8
int RD9_THUNK sm_getOccluderRecordCount(void* self RD9_THUNK_EDX)
{
    return static_cast<const StaticMesh*>(self)->getOccluderRecordCount();
}

// +0xEC, 0x100FC120 - `retn 4`.
const void* RD9_THUNK sm_getOccluderRecord(void* self RD9_THUNK_EDX, unsigned int index)
{
    return static_cast<const StaticMesh*>(self)->getOccluderRecord(index);
}

int RD9_THUNK sm_getNextGeometry(void* self RD9_THUNK_EDX)
{
    return static_cast<StaticMesh*>(self)->getNextGeometry();
}

void RD9_THUNK sm_setDword28C(void* self RD9_THUNK_EDX, void* value)
{
    static_cast<StaticMesh*>(self)->setDword28C(value);
}

void* RD9_THUNK sm_getDword28C(void* self RD9_THUNK_EDX)
{
    return static_cast<StaticMesh*>(self)->getDword28C();
}

void RD9_THUNK sm_setGeom(void* self RD9_THUNK_EDX, int geom)
{
    static_cast<StaticMesh*>(self)->setGeom(geom);
}

void RD9_THUNK sm_setLod(void* self RD9_THUNK_EDX, int lod)
{
    static_cast<StaticMesh*>(self)->setLod(lod);
}

int RD9_THUNK sm_getGeomCount(void* self RD9_THUNK_EDX)
{
    return static_cast<StaticMesh*>(self)->getGeomCount();
}

int RD9_THUNK sm_getLodCount(void* self RD9_THUNK_EDX)
{
    return static_cast<StaticMesh*>(self)->getLodCount();
}

int RD9_THUNK sm_getGeomIndex(void* self RD9_THUNK_EDX)
{
    return static_cast<StaticMesh*>(self)->getGeomIndex();
}

int RD9_THUNK sm_getLodIndex(void* self RD9_THUNK_EDX)
{
    return static_cast<StaticMesh*>(self)->getLodIndex();
}

int RD9_THUNK sm_getDword31C(void* self RD9_THUNK_EDX)
{
    return static_cast<StaticMesh*>(self)->getDword31C();
}

void RD9_THUNK sm_updateFlags(void* self RD9_THUNK_EDX, unsigned int set, unsigned int clear)
{
    static_cast<StaticMesh*>(self)->updateFlags(set, clear);
}

unsigned int RD9_THUNK sm_getFlags(void* self RD9_THUNK_EDX)
{
    return static_cast<StaticMesh*>(self)->getFlags();
}

void RD9_THUNK sm_stampFrameOrdinal(void* self RD9_THUNK_EDX)
{
    static_cast<StaticMesh*>(self)->stampFrameOrdinal();
}

void RD9_THUNK sm_setDword350(void* self RD9_THUNK_EDX, void* value)
{
    static_cast<StaticMesh*>(self)->setDword350(value);
}

void* RD9_THUNK sm_getDword350(void* self RD9_THUNK_EDX)
{
    return static_cast<StaticMesh*>(self)->getDword350();
}

void RD9_THUNK sm_setOwnerObject(void* self RD9_THUNK_EDX, void* object)
{
    static_cast<StaticMesh*>(self)->setOwnerObject(object);
}

void* RD9_THUNK sm_refreshRootObject(void* self RD9_THUNK_EDX)
{
    return static_cast<StaticMesh*>(self)->refreshRootObject();
}

void RD9_THUNK sm_setFade(void* self RD9_THUNK_EDX, float value)
{
    static_cast<StaticMesh*>(self)->setFade(value);
}

// the nine accessors
void* RD9_THUNK sm_getTemplate(void* self RD9_THUNK_EDX)
{
    return static_cast<StaticMesh*>(self)->getTemplate();
}

const void* RD9_THUNK sm_getLocalBounds(void* self RD9_THUNK_EDX)
{
    return static_cast<StaticMesh*>(self)->getLocalBounds();
}

void* RD9_THUNK sm_getRootObject(void* self RD9_THUNK_EDX)
{
    return static_cast<StaticMesh*>(self)->getRootObject();
}

void RD9_THUNK sm_setFloat308(void* self RD9_THUNK_EDX, float value)
{
    static_cast<StaticMesh*>(self)->setFloat308(value);
}

// The +0x24 caller (XpLightManager) passes a LightShadowRecord pointer; pointer-sized on x64.
void RD9_THUNK sm_setDword34C(void* self RD9_THUNK_EDX, uintptr_t value)
{
    static_cast<StaticMesh*>(self)->setDword34C(value);
}

uintptr_t RD9_THUNK sm_getDword34C(void* self RD9_THUNK_EDX)
{
    return static_cast<StaticMesh*>(self)->getDword34C();
}

int RD9_THUNK sm_getGeometryType(void* self RD9_THUNK_EDX)
{
    return static_cast<StaticMesh*>(self)->getGeometryType();
}

// THE truncation IS THE point. The shipped body compares the low byte of the argument
// slot, so 0x100 is false to it and would be true to an `int != 0` test. Taking the raw dword
// here and narrowing it is what keeps that.
void RD9_THUNK sm_setFlagBit1(void* self RD9_THUNK_EDX, int raw)
{
    static_cast<StaticMesh*>(self)->setFlagBit1(static_cast<unsigned char>(raw));
}

bool RD9_THUNK sm_getFlagBit1(void* self RD9_THUNK_EDX)
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


// Converts the ILightSampleable face (retail +0x30) to its owning StaticMesh.

inline StaticMesh* fromLightSampleable(void* subobject)
{
    return reinterpret_cast<StaticMesh*>(static_cast<unsigned char*>(subobject)
                                         - offsetof(StaticMesh, m_lightSampleableVptr));
}

int RD9_THUNK sm_ls_addRef(void* self RD9_THUNK_EDX)
{
    return fromLightSampleable(self)->addRef();
}

int RD9_THUNK sm_ls_getRef(void* self RD9_THUNK_EDX)
{
    return fromLightSampleable(self)->getRef();
}

int RD9_THUNK sm_ls_release(void* self RD9_THUNK_EDX)
{
    return fromLightSampleable(self)->release();
}

void* RD9_THUNK sm_ls_queryInterface(void* self RD9_THUNK_EDX, unsigned int iid)
{
    return fromLightSampleable(self)->queryInterface(iid);
}

bool RD9_THUNK sm_ls_slot10(void* self RD9_THUNK_EDX)
{
    return fromLightSampleable(self)->lightSampleableSlot10ReturnsTrue();
}

// Defined below with the other name builders.
void staticMeshLightmapName(const StaticMesh* self, std::string& out);

// ILightSampleable +0x14: a MeshLightSampler for the current (geom, lod), loaded from
// "<template path>/Meshes/<template name>.samples" (geom 0, lod 0) or ".samp_<geom><lod>" and
// writing the instance's lightmap name. A template with lightmapOverride caps the sample grid at
// its lightmap size for the pair; `resolutionModifier` is LightmapGeneration's. On success the
// pair's cached lightmap texture is released and the instance's current lightmap cleared. Null
// when the file cannot be opened. The allocation is used unchecked, as shipped.
// [INFERRED] placement: retail keeps this body with the rest of its run in
// StaticMeshLightmapGeneration.cpp; this tree keeps the run's other members here.
// PE 0x10198680
ILightSampling* RD9_THUNK sm_ls_createLightSampler(void* self RD9_THUNK_EDX, float resolutionModifier)
{
    StaticMesh* const mesh = fromLightSampleable(self);
    void* const storage = ::renddx9abi::engineAlloc(sizeof(MeshLightSampler));
    MeshLightSampler* const sampler = storage ? new (storage) MeshLightSampler() : 0;

    typedef int (__thiscall* IndexFn)(const void*);
    const int lod  = reinterpret_cast<IndexFn>(mesh->m_vptr[0x7C / 4])(mesh);
    const int geom = reinterpret_cast<IndexFn>(mesh->m_vptr[0x70 / 4])(mesh);
    char suffix[12];
    if ((geom | lod) != 0)
        std::sprintf(suffix, ".samp_%d%d", geom, lod);
    else
        std::strcpy(suffix, ".samples");

    IGeometryTemplate* const tmpl = static_cast<StaticMeshTemplate*>(mesh->m_template);
    if (tmpl->getLightmapOverride())                                       // +0x6C
        sampler->m_sizeCap = tmpl->getLightmapSize(geom, lod);             // +0x74
    sampler->m_resolutionModifier = resolutionModifier;

    bool failed;
    {
        std::string meshes(tmpl->getPath());                              // +0x20
        meshes += "/Meshes/";
        std::string named(meshes);
        named += tmpl->getName();                                        // +0x18
        std::string path(named);
        path += suffix;
        std::string outputName;
        staticMeshLightmapName(mesh, outputName);
        failed = !sampler->load(path, outputName);
    }
    if (failed)
    {
        sampler->~MeshLightSampler();
        ::renddx9abi::engineFree(sampler);
        return 0;
    }

    const int geomTimesFive =
        5 * reinterpret_cast<IndexFn>(mesh->m_vptr[0x70 / 4])(mesh);
    const unsigned int key = static_cast<unsigned int>(
        reinterpret_cast<IndexFn>(mesh->m_vptr[0x7C / 4])(mesh) + 2 * geomTimesFive);
    dx9rend::EngineTexture*& slot = mesh->m_map260[key].texture;
    dx9rend::EngineTexture* const cached = slot;
    mesh->m_dword26C = reinterpret_cast<uintptr_t>(cached);
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
// PE 0x10198D80
bool RD9_THUNK sm_ls_destroyLightSampler(void* self RD9_THUNK_EDX, ILightSampling* sampler)
{
    StaticMesh* const mesh = fromLightSampleable(self);
    if (sampler)
    {
        MeshLightSampler* const meshSampler = static_cast<MeshLightSampler*>(sampler);
        meshSampler->~MeshLightSampler();
        ::renddx9abi::engineFree(meshSampler);
    }

    typedef int (__thiscall* IndexFn)(const void*);
    const unsigned int geom = static_cast<unsigned int>(
        reinterpret_cast<IndexFn>(mesh->m_vptr[0x70 / 4])(mesh));
    const unsigned int lod = static_cast<unsigned int>(
        reinterpret_cast<IndexFn>(mesh->m_vptr[0x7C / 4])(mesh));
    const unsigned int key = lod + 2u * (5u * geom);
    mesh->m_dword26C = reinterpret_cast<uintptr_t>(mesh->m_map260[key].texture);
    mesh->m_byte35A  = 0;
    mesh->loadLightmap();
    return true;
}

#if !defined(_MSC_VER)
// [CHOSEN, not recovered] Match the shared consumer interface under the native ABI.
void* const kStaticMeshLightSampleableVtable[] =
{
    BF2_NATIVE_SLOT(::dice::hfe::geom::ILightSampleable, addRef, sm_ls_addRef),
    BF2_NATIVE_SLOT(::dice::hfe::geom::ILightSampleable, getRef, sm_ls_getRef),
    BF2_NATIVE_SLOT(::dice::hfe::geom::ILightSampleable, release, sm_ls_release),
    BF2_NATIVE_SLOT(::dice::hfe::geom::ILightSampleable, queryInterface, sm_ls_queryInterface),
    BF2_NATIVE_SLOT(::dice::hfe::geom::ILightSampleable, isLightSampleable, sm_ls_slot10),
    BF2_NATIVE_SLOT(::dice::hfe::geom::ILightSampleable, createLightSampler, sm_ls_createLightSampler),
    BF2_NATIVE_SLOT(::dice::hfe::geom::ILightSampleable, destroyLightSampler, sm_ls_destroyLightSampler),
};
#else
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
#endif

} // anonymous namespace - staticMeshRootParent is shared with XpLightManager.cpp's

// sub_10103CB0 - walk UP the parent chain from the object itself (+0x28, the same link
// staticMeshRootParent follows) asking each for IID_IPlayerControlObject (ds:10208CF0 = 50373),
// and answer the first interface found, not the object; null when none answers:
//     test esi,esi; je 0 / loop: call [eax+0Ch](50373); jnz ret; mov esi,[esi+28h]; test; jnz loop
// queryInterface is IBase slot 3 (+0x0C) and the parent is IObject's retail-aligned +0x28 prefix
// field (ObjectLayoutAccess), identical in both flavours.
void* staticMeshControlObject(void* object)
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

    const StaticMeshTemplate* const t = static_cast<const StaticMeshTemplate*>(objectTemplate);
    if (!t->m_finished)                                    // +0x185
        return false;
    if (t->m_isWater)                                      // +0x2A4
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

void staticMeshRefreshSelection(StaticMesh* self)
{
    StaticMeshLod* const lodRecord = self->m_dword30C;
    if (!lodRecord)
        return;

    MeshBounds* const bounds = &lodRecord->bounds;
    if (bounds->dirty)
        meshBoundsFinalize(*bounds);

    self->m_dword304 = *reinterpret_cast<const unsigned int*>(&bounds->radius);

    // The lightmap entry for this (geom, lod). The shipped call is the inserting
    // operator[], not a find - a miss leaves a zeroed entry behind and copies those zeros out, which
    // is why a fresh selection can clear the 1.0f UV scale the constructor seeded.
    if (self->m_byte35A || self->m_map260.empty())
        return;

    const StaticMeshTemplate* const objectTemplate =
        static_cast<const StaticMeshTemplate*>(self->m_template);
    if (!objectTemplate)
        return;

    const unsigned int geom = static_cast<unsigned int>(self->getGeomIndex());
    const unsigned int lod  = static_cast<unsigned int>(self->getLodIndex());

    // The base's +0x70 per-geom skipped-lod counts (null until the vector first allocates).
    const unsigned int* const perGeom =
        reinterpret_cast<const unsigned int*>(objectTemplate->m_geomInts.data());
    if (!perGeom)
        return;

    const unsigned int key = perGeom[geom] + lod + 2u * (5u * geom);

    const StaticMeshLightmapEntry& entry = self->m_map260[key];
    self->m_dword26C = reinterpret_cast<uintptr_t>(entry.texture);
    self->m_float270 = entry.offset[0];
    self->m_float274 = entry.offset[1];
    self->m_dword278 = *reinterpret_cast<const unsigned int*>(&entry.offset[2]);
    self->m_dword27C = *reinterpret_cast<const unsigned int*>(&entry.offset[3]);
}

// Release cached lightmaps in ascending key order without clearing their map values.
void staticMeshReleaseLightmaps(StaticMesh::LightmapCache& cache)
{
    for (StaticMesh::LightmapCache::iterator it = cache.begin(); it != cache.end(); ++it)
    {
        dx9rend::EngineTexture* const texture = it->second.texture;
        if (texture && --texture->refCount == 0)
        {
            dx9rend::textureDtor(texture);
            ::renddx9abi::engineFree(texture);
        }
    }
}


// Renderer configuration, named by its retail offset through BF2Render.cpp's tail accessors.
unsigned char rendererByte(unsigned int byteOffset)
{
    return ::dice::hfe::dx9rend::rendererTailByte(byteOffset);
}

unsigned int rendererDword(unsigned int byteOffset)
{
    return static_cast<unsigned int>(::dice::hfe::dx9rend::rendererTailInt(byteOffset));
}

float rendererFloat(unsigned int byteOffset)
{
    return ::dice::hfe::dx9rend::rendererTailFloat(byteOffset);
}

// The base object's +0x102 byte, the "editor / lightmap generation" gate BF2Render.cpp's step 14
// tests too.
unsigned char rendererBaseFlag102()
{
    return static_cast<const ::dice::hfe::dx9rend::RendDX9Base*>(
        ::dice::hfe::dx9rend::g_rendererBase)->flag102;
}

} // anonymous namespace - the two helpers below are shared with
  // StaticMeshLightmapGeneration.cpp (declared in StaticMesh.h)

StaticMeshRenderer* staticMeshRenderer()
{
    return static_cast<StaticMeshRenderer*>(static_cast<void*>(g_StaticMeshRenderer));
}

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

// Cached technique pointer at material +0x178 (parameter block +0x14C), refreshed for each draw.
// On x86 this shares storage with normalUnpack.value[2]; the shader uses only its first two components.
dx9rend::Ra::RaShader*& staticMeshMaterialShader(StaticMeshMaterial* material)
{
    return reinterpret_cast<dx9rend::Ra::RaShader*&>(
        raMaterialShaderSlot(staticMeshMaterialParameters(material)));
}

// The material parameter block's CubeMap holder value: material +0x14C, block +0x120.
void*& staticMeshMaterialCubeMap(StaticMeshMaterial* material)
{
    return staticMeshMaterialParameters(material).cubeMap.value;
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


// The eight-corner box test the draw culls each material with: geom::BoxCorners (BoxCorners.h,
// the one body per retail function of the 0x100CE120 family) over the material's MeshBounds, which
// is the 60-byte LocalBoundingBox record (min +0x00, max +0x0C; MeshTemplate.cpp asserts the
// layout). The old local copy here summed every corner in one association and every plane distance
// as x + y + z; the listings (0x101C6845.., 0x101C6ACD..) associate corners 1..7 differently.
struct StaticMeshBoundsCorners : BoxCorners
{
    StaticMeshBoundsCorners(const MeshBounds& bounds, const Mat4& world)
        : BoxCorners(reinterpret_cast<const LocalBoundingBox&>(bounds), world)
    {
    }
};


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
void staticMeshLightmapName(const StaticMesh* self, std::string& out)
{
    const float* const position = &self->m_transform->m[12];
    StaticMeshRenderer* const smr = staticMeshRendererByName();
    const int key = self->getLodIndex() + 2 * (5 * self->getGeomIndex());

    std::string savePath("");
    dx9rend::readStringSettingDefault("Editor_LevelSavePath", "", savePath);
    std::string root(savePath);
    root += "/";
    std::string levelDirectory("");
    dx9rend::readStringSettingDefault("GSLevelDirectory", "", levelDirectory);
    std::string withDirectory(root);
    withDirectory += levelDirectory;
    std::string level("");
    dx9rend::readStringSettingDefault("GSLoadLevel", "lightsPlease", level);
    std::string levelPath(withDirectory);
    levelPath += level;

    const MeshTemplate* const t = static_cast<const MeshTemplate*>(
        static_cast<const StaticMeshTemplate*>(self->m_template));
    std::string objects(levelPath);
    objects += "/Lightmaps/Objects/";
    std::string named(objects);
    named += t->getName();                                             // MeshTemplate +0x18

    std::string suffix("");
    if (smr->m_flags60[2])
        suffix = formatString("=%02d=%d=%d=%d", key, static_cast<int>(position[0]),
                              static_cast<int>(position[1]), static_cast<int>(position[2]));
    else
        suffix = formatString("_%d-%d-%d", static_cast<int>(position[0]),
                              static_cast<int>(position[1]), static_cast<int>(position[2]));

    std::string result(named);
    result += suffix;
    out = result;
}

// sub_10198410 - the object part alone: <template name> plus the same suffix.
void staticMeshLightmapObjectName(const StaticMesh* self, std::string& out)
{
    const float* const position = &self->m_transform->m[12];
    StaticMeshRenderer* const smr = staticMeshRendererByName();
    const int key = self->getLodIndex() + 2 * (5 * self->getGeomIndex());
    const MeshTemplate* const t = static_cast<const MeshTemplate*>(
        static_cast<const StaticMeshTemplate*>(self->m_template));

    std::string suffix("");
    if (smr->m_flags60[2])
        suffix = formatString("=%02d=%d=%d=%d", key, static_cast<int>(position[0]),
                              static_cast<int>(position[1]), static_cast<int>(position[2]));
    else
        suffix = formatString("_%d-%d-%d", static_cast<int>(position[0]),
                              static_cast<int>(position[1]), static_cast<int>(position[2]));

    std::string result(t->getName());
    result += suffix;
    out = result;
}

// sub_10197F00 - the directory part: <GSLevelDirectory><GSLoadLevel>/Lightmaps/Objects/ (no save
// path, unlike the full name).
void staticMeshLightmapDirectory(std::string& out)
{
    std::string level("");
    dx9rend::readStringSettingDefault("GSLoadLevel", "lightsPlease", level);
    std::string levelDirectory("");
    dx9rend::readStringSettingDefault("GSLevelDirectory", "", levelDirectory);
    std::string path(levelDirectory);
    path += level;
    std::string result(path);
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

    // +0x260 - the lightmap cache (its sentinel from sub_100FFB50), the std::vectors at +0x2D0,
    // +0x2E0, +0x2F0, +0x338, +0x35C and +0x36C: default-constructed empty by the compiler.

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

// PE 0x100FDE50
StaticMesh::~StaticMesh()
{
    m_vptr = kStaticMeshVtable;
    m_lightSampleableVptr = kStaticMeshLightSampleableVtable;
    // 1. Release every cached lightmap texture; the nodes go with the map (step 7).
    staticMeshReleaseLightmaps(m_map260);

    // 2. Deregister from the template's instance set (+0xEC), the one createInstance inserted
    //    into.
    static_cast<StaticMeshTemplate*>(m_template)->m_instances.erase(this);

    // 3. The refcounted vertex-buffer wrapper at +0x300.
    if (m_dword300)
    {
        dx9rend::vertexBufferRelease(reinterpret_cast<void*>(m_dword300));
    }

    // The template flag controls release of each selected material's +0x14C texture.
    if (static_cast<const StaticMeshTemplate*>(m_template)->m_isWater)
    {
        const unsigned int count = static_cast<unsigned int>(m_dword310->size());
        for (unsigned int i = 0; i < count; ++i)
        {
            dx9rend::EngineTexture*& slot = reinterpret_cast<dx9rend::EngineTexture*&>(
                staticMeshMaterialCubeMap(m_dword310->data() + i));
            if (slot)
                dx9rend::textureReleaseRef(slot);
        }
    }

    // Release member storage before the raw parameter block, in reverse declaration order.
    std::vector<unsigned int>().swap(m_geomLodDistanceCounts);
    std::vector<float>().swap(m_geomRadii);
    std::vector<StaticMeshOccluderRecord>().swap(m_vector338);
    std::vector<VertexLightRecord>().swap(m_vector2F0);
    std::vector<dx9rend::Ra::RaLightRecord>().swap(m_vector2E0);
    std::vector<world::LightSource*>().swap(m_vector2D0);
    m_map260.clear();

    // 8. The Ra per-object parameter block, the exact counterpart of the constructor's
    //    raObjectParametersConstruct.
    ::dice::hfe::dx9rend::Ra::raObjectParametersDestroy(m_objectParameters);

    // 9. The member destructors (the containers, then m_name) run after this body.
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
        typedef void (__thiscall* DeletingDtorFn)(void*, int);
        reinterpret_cast<DeletingDtorFn>(m_vptr[0x34 / 4])(this, 1);
    }
    return m_refCount;
}

// PE 0x10103A70, +0x0C.
// All four ids are already named in Core/Ids.h; see the header's identity note.
// The ILightSampleable arm null-tests `this`, which cannot be null on a member call. Shipped,
// reproduced.
void* StaticMesh::queryInterface(unsigned int iid)
{
    if (iid == 1u || iid == IID_IGeometry || iid == IID_IRenderable)
        return this;

    if (iid == IID_ILightSampleable && this)
        return &m_lightSampleableVptr;

    return 0;
}


// Borrows transform and lazily builds each part's world-space occluder geometry.
// A template with no parts keeps initialization pending. Requires a valid template.
// Occluder faces retain their template plane's edge references, records the group's edges.

void StaticMesh::setGeometryTransformation(const Mat4* transform)
{
    m_transform = transform;
    m_byte358   = 1;

    if (transform && m_byte348)
    {
        // +0xF0 is the template's part count, +0xF4 fetches one part record by index.
#if defined(_MSC_VER)
        typedef int (__thiscall* PartCountFn)(void*);
        typedef StaticMeshOccluderGroup* (__thiscall* GetPartFn)(void*, int);

        void* const objectTemplate = m_template;
        void** const tvtbl = *reinterpret_cast<void***>(objectTemplate);
        const int partCount = reinterpret_cast<PartCountFn>(
            tvtbl[0xF0 / 4])(objectTemplate);
#else
        // [CHOSEN, not recovered] Typed calls on the native template. The x86 byte offsets
        // index the Itanium table elsewhere: +0xF0 reached makeScript (c-join-stall).
        StaticMeshTemplate* const objectTemplate = static_cast<StaticMeshTemplate*>(m_template);
        const int partCount = objectTemplate->getOccluderGroupCount();          // PE +0xF0
#endif

        // reached AS sixteen RAW floats, exactly as the shipped body reaches it - every
        // access below is one of its documented byte offsets, not a named Mat4 member.
        const float* const m = reinterpret_cast<const float*>(transform);

        for (int part = 0; part < partCount; ++part)
        {
            m_byte348 = 0;

#if defined(_MSC_VER)
            StaticMeshOccluderGroup* const source = reinterpret_cast<GetPartFn>(
                tvtbl[0xF4 / 4])(objectTemplate, part);
#else
            StaticMeshOccluderGroup* const source = static_cast<StaticMeshOccluderGroup*>(
                objectTemplate->getOccluderGroup(static_cast<unsigned int>(part)));   // PE +0xF4
#endif

            // The record, built on the stack exactly as the shipped body builds it.
            StaticMeshOccluderRecord record;
            std::vector<StaticMeshOccluderFace>& planes = record.faces;
            std::vector<Vec3>& points = record.points;
            record.edges = &source->edges;

            // the points, transformed
            {
                const unsigned int count = static_cast<unsigned int>(source->points.size());
                for (unsigned int i = 0; i < count; ++i)
                {
                    const Vec3& v = source->points[i];
                    Vec3 world;
                    world.x = v.x * m[0] + v.y * m[4] + v.z * m[8]  + m[12];
                    world.y = v.x * m[1] + v.y * m[5] + v.z * m[9]  + m[13];
                    world.z = v.x * m[2] + v.y * m[6] + v.z * m[10] + m[14];
                    points.push_back(world);
                }
            }

            // the planes
            {
                const unsigned int count = static_cast<unsigned int>(source->planes.size());
                for (unsigned int i = 0; i < count; ++i)
                {
                    const StaticMeshOccluderPlane& src = source->planes[i];
                    const float* const plane = src.plane;

                    StaticMeshOccluderFace element;
                    // The back-pointer first, as the shipped body writes it first.
                    element.edgeReferences = &src.edgeReferences;

                    // Normalize the input normal only (PE 0x100777A0), then RendDX9's
                    // Plane3::transform copy (PE 0x1007C530) into the element.
                    Plane3 input;
                    input.normal.x = plane[0]; input.normal.y = plane[1]; input.normal.z = plane[2];
                    rendmath::normalize(input.normal);
                    input.d = plane[3];

                    rendmath::planeTransform(element.plane, input, *transform);
                    planes.push_back(element);
                }
            }

            // Transfers nested points/planes storage to the vector record.
            m_vector338.push_back(std::move(record));
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

    typedef void* (__thiscall* RefreshFn)(void*);
    reinterpret_cast<RefreshFn>(m_vptr[0xF8 / 4])(this);
}

// PE 0x100FB2C0, +0xF8: memoise the owner's root parent at +0x334, AND return IT.
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

    typedef unsigned int (__thiscall* FrameOrdinalFn)(void*);
    m_dword24 = reinterpret_cast<FrameOrdinalFn>(
        (*reinterpret_cast<void***>(base))[0x54 / 4])(base);
}

// the nine accessors

void* StaticMesh::getTemplate() const
{
    return m_template;
}

// Requires a selected LOD; returns its bounds (record +4) without a null check.
const void* StaticMesh::getLocalBounds() const
{
    return &m_dword30C->bounds;
}

void* StaticMesh::getRootObject() const
{
    return m_dword334;
}

// PE 0x10103BB0, +0x10: stores the argument at +0x308 (`retn 4`).
void StaticMesh::setFloat308(float value)
{
    m_float308 = value;
}

// PE 0x10103B90, +0x24: stores the argument at +0x34C (`retn 4`).
void StaticMesh::setDword34C(uintptr_t value)
{
    m_dword34C = value;
}

uintptr_t StaticMesh::getDword34C() const
{
    return m_dword34C;
}

int StaticMesh::getGeometryType() const
{
    return 3;
}

// PE 0x10103B40: updates flag bit 1 through the virtual updateFlags slot, honoring overrides.
void StaticMesh::setFlagBit1(unsigned char enable)
{
    typedef void (__thiscall* UpdateFlagsFn)(void*, unsigned int, unsigned int);
    const UpdateFlagsFn update =
        reinterpret_cast<UpdateFlagsFn>(m_vptr[0xC0 / 4]);

    if (enable)
        update(this, 2u, 0u);
    else
        update(this, 0u, 2u);
}

// 0x10103B70 - `call [eax+0C4h]; shr eax, 1; and al, 1`. The 8-bit AND is the bool return
// convention, not a mask over the whole register: only AL is meaningful and only AL is read.
bool StaticMesh::getFlagBit1() const
{
    typedef unsigned int (__thiscall* GetFlagsFn)(const void*);
    const unsigned int flags = reinterpret_cast<GetFlagsFn>(
        m_vptr[0xC4 / 4])(this);
    return ((flags >> 1) & 1u) != 0u;
}

// PE 0x100FB2E0, +0xAC: stores the argument at +0x350 (`retn 4`).
void StaticMesh::setDword350(void* value)
{
    m_dword350 = value;
}

void* StaticMesh::getDword350() const
{
    return m_dword350;
}


// PE 0x101985D0 (`retn 14h`). Unreachable in 1.50: no code or data references. Locks the template's
// index buffer whole (offset 0, count 0, flags 0), picks the first index by the template's primitive
// type (4 = list: material index start + 3 * triangle; 5 = strip: start + triangle; otherwise the
// triangle number alone), stores the three indices plus the material's base vertex through the
// out pointers, and unlocks.
void StaticMesh::getTriangleIndices(int material, int triangle, unsigned int* a, unsigned int* b,
                                    unsigned int* c)
{
    StaticMeshMaterial* const entry = m_dword310->data() + material;
    StaticMeshTemplate* const tmpl = static_cast<StaticMeshTemplate*>(m_template);
    const unsigned short* const indices =
        static_cast<const unsigned short*>(dx9rend::indexBufferLock(tmpl->m_indexBuffer, 0, 0, 0));
    unsigned int first;
    if (tmpl->m_primitiveType == 4)
        first = static_cast<unsigned int>(triangle + 2 * triangle) + entry->m_indexStart;
    else if (tmpl->m_primitiveType == 5)
        first = entry->m_indexStart + static_cast<unsigned int>(triangle);
    else
        first = static_cast<unsigned int>(triangle);
    *a = indices[first] + entry->m_vertexStart;
    *b = indices[first + 1] + entry->m_vertexStart;
    *c = indices[first + 2] + entry->m_vertexStart;
    dx9rend::indexBufferUnlock(tmpl->m_indexBuffer);
}

// PE 0x100FC570, +0x6C: setGeom. Selects geometry with an unsigned LOD clamp (an empty geometry
// clamps to 0xFFFFFFFF, so the stored lod survives); the bounds/lightmap tail is
// staticMeshRefreshSelection.
void StaticMesh::setGeom(int geom)
{
    m_dword314 = static_cast<unsigned int>(geom);

    StaticMeshTemplate* const objectTemplate = static_cast<StaticMeshTemplate*>(m_template);
    if (!objectTemplate)
        return;

    // The template's material tree, geom level: one vector of lod material vectors.
    StaticMeshLodMaterialLists* const table16 = objectTemplate->m_materials.data();
    StaticMeshMaterialList* const entry = table16[geom].data();

    unsigned int count = 0;
    if (entry)
        count = static_cast<unsigned int>(table16[geom].size());

    unsigned int lod = count - 1u;              // see the unsigned note above
    if (m_dword318 < lod)
        lod = m_dword318;
    m_dword318 = lod;

    m_dword30C = objectTemplate->m_geoms[geom].lods.data() + lod;
    m_dword310 = entry + lod;

    staticMeshRefreshSelection(this);
}


// PE 0x100FC690, +0x78: setLod.
// -1 IS A reset, NOT AN index: it selects lod 0 AND clears +0x357, the byte that marks the
// lod as explicitly chosen. The draw reads that byte to decide whether to run its own LOD
// selection, so the two arms differ in meaning, not just in arithmetic.
void StaticMesh::setLod(int lod)
{
    StaticMeshTemplate* const objectTemplate = static_cast<StaticMeshTemplate*>(m_template);
    if (!objectTemplate)
        return;

    const unsigned int geom = m_dword314;

    StaticMeshLod* const lodBase = objectTemplate->m_geoms[geom].lods.data();
    StaticMeshMaterialList* const recBase = objectTemplate->m_materials.data()[geom].data();

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
        m_dword30C = lodBase + lod;
        m_dword310 = recBase + lod;
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

    StaticMeshTemplate* const objectTemplate = static_cast<StaticMeshTemplate*>(m_template);
    if (!objectTemplate)
        return;

    StaticMeshLodMaterialLists* const table16 = objectTemplate->m_materials.data();
    if (objectTemplate->m_geoms.empty() || !table16)
        return;

    // Re-select the current (geom, lod) from the freshly loaded tables.
    {
        const unsigned int geom = m_dword314;
        const unsigned int lod  = m_dword318;
        m_dword30C = objectTemplate->m_geoms[geom].lods.data() + lod;
        m_dword310 = table16[geom].data() + lod;

        MeshBounds* const bounds = &m_dword30C->bounds;
        if (bounds->dirty)
            meshBoundsFinalize(*bounds);
        m_dword304 = *reinterpret_cast<const unsigned int*>(&bounds->radius);
    }

    if (!staticMeshTemplateUsesLightmaps(objectTemplate))
        m_byte35A = 1;

    // The geom count, from the template's +0xDC vector.
    const unsigned int geomCount = static_cast<unsigned int>(objectTemplate->m_geoms.size());

    // Keep the material-count walk even though its result is unused.
    {
        unsigned int unusedMaxMaterials = 0;
        for (unsigned int g = 0; g < geomCount; ++g)
        {
            const unsigned int lodCount =
                static_cast<unsigned int>(objectTemplate->m_geoms[g].lods.size());
            if (!lodCount)
                continue;

            StaticMeshMaterialList* const recBase = table16[g].data();
            for (unsigned int l = 0; l < lodCount; ++l)
            {
                const StaticMeshMaterialList& rec = recBase[l];
                unsigned int materials = 0;
                if (rec.data())
                    materials = static_cast<unsigned int>(rec.size());
                if (materials >= unusedMaxMaterials)
                    unusedMaxMaterials = materials;
            }
        }
        (void)unusedMaxMaterials;
    }

    const int savedGeom = getGeomIndex();
    const int savedLod  = getLodIndex();

    m_geomRadii.resize(geomCount, 0.0f);                       // sub_100EC600
    m_geomLodDistanceCounts.resize(geomCount, 0u);

    for (unsigned int g = 0; g < geomCount; ++g)
    {
        m_dword314 = g;

        StaticMeshGeom& geomRecord = objectTemplate->m_geoms[g];
        StaticMeshLod* const lodBase = geomRecord.lods.data();
        MeshBounds* const bounds = &lodBase->bounds;
        if (bounds->dirty)
            meshBoundsFinalize(*bounds);

        if (!m_geomRadii.empty())
            m_geomRadii[g] = bounds->radius;

        // Count this geometry's LOD switch distances.
        const unsigned int distanceCount =
            static_cast<unsigned int>(geomRecord.lodDistances.size());
        if (!m_geomLodDistanceCounts.empty())
            m_geomLodDistanceCounts[g] = distanceCount;

        if (m_byte35A || !staticMeshTemplateUsesLightmaps(objectTemplate))
            continue;

        // Pre-populate this geom's lightmap entries with the default (1,1) UV scale - which is
        // exactly what the constructor seeds +0x270/+0x274 with.
        const unsigned int lodCount = static_cast<unsigned int>(geomRecord.lods.size());

        // The base's +0x70 per-geom skipped-lod counts (null until the vector first allocates).
        const unsigned int* const perGeom =
            reinterpret_cast<const unsigned int*>(objectTemplate->m_geomInts.data());

        for (unsigned int l = 0; l < lodCount; ++l)
        {
            m_dword318 = l;

            if (!perGeom)
                break;

            const unsigned int geomIndex = static_cast<unsigned int>(getGeomIndex());
            const unsigned int key = perGeom[geomIndex]
                                   + static_cast<unsigned int>(getLodIndex())
                                   + 2u * (5u * geomIndex);

            // find, then insert a zeroed entry on a miss, then operator[] for the stores.
            if (m_map260.find(key) == m_map260.end())
                m_map260.insert(LightmapCache::value_type(key, StaticMeshLightmapEntry()));

            StaticMeshLightmapEntry& entry = m_map260[key];
            entry.texture   = 0;
            entry.offset[0] = 1.0f;
            entry.offset[1] = 1.0f;
            *reinterpret_cast<unsigned int*>(&entry.offset[2]) = 0;
            *reinterpret_cast<unsigned int*>(&entry.offset[3]) = 0;

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

// PE 0x10103BD0, +0xE0 (the getter at +0xE4 is PE 0x10103BE0): `retn 4` and `retn`, so the arity is certain.
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
    const StaticMeshTemplate* const objectTemplate =
        static_cast<const StaticMeshTemplate*>(m_template);
    return static_cast<int>(objectTemplate->m_geoms.size());
}


// Returns the current geometry's LOD count. Requires a valid template and geometry table.

int StaticMesh::getLodCount() const
{
    const StaticMeshTemplate* const objectTemplate =
        static_cast<const StaticMeshTemplate*>(m_template);
    return static_cast<int>(objectTemplate->m_geoms[m_dword314].lods.size());
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

    MeshBounds* const bounds = &m_dword30C->bounds;
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
void StaticMesh::setName(const std::string& value)
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

// +0x8C, the selected lod's material records, counted; 0 with no lod or no materials.
int StaticMesh::getMaterialCount() const
{
    if (!m_dword310)
        return 0;
    const StaticMeshMaterial* const first =
        m_dword310->data();
    if (!first)
        return 0;
    return static_cast<int>((m_dword310->data() + m_dword310->size()) - first);
}

// +0xE8, the +0x338 records, counted.
int StaticMesh::getOccluderRecordCount() const
{
    return static_cast<int>(m_vector338.size());
}

// +0xEC, the i-th +0x338 record, or null when there are none or i is past the end.
const void* StaticMesh::getOccluderRecord(unsigned int index) const
{
    const unsigned int count = static_cast<unsigned int>(m_vector338.size());
    if (!count || index >= count)
        return 0;
    return &m_vector338[index];
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
    const StaticMeshMaterial* const mat = static_cast<const StaticMeshMaterial*>(material);
    const ShaderMaterial* const shaderMaterial = mat->m_shaderMaterial;     // +0x28
    unsigned int bits = g_shaderComboFlags[shaderMaterial->shaderCombo];    // dword_1023A9D0

    typedef unsigned int (__thiscall* GetFlagsFn)(const void*);
    if ((reinterpret_cast<GetFlagsFn>(m_vptr[0xC4 / 4])(this) & 1u) != 0)
        bits &= ~8u;

    if (pointLightPass)
    {
        bits |= 0x400u;
    }
    else
    {
        if (!m_byte35A && staticMeshTemplateUsesLightmaps(m_template) && m_dword26C)
            bits |= 1u;
        typedef int (__thiscall* GetLodFn)(const void*);
        const int lod = reinterpret_cast<GetLodFn>(m_vptr[0x7C / 4])(this);
        if (static_cast<int>(staticMeshRenderer()->m_dword98) < lod)
            bits &= ~1u;
        if (m_byte355)
            bits |= 0x200u;
    }

    if (mat->m_alphaMode == 2u)                                           // +0x17C
        bits |= 0x800u;
    return bits;
}


// PE 0x100FB090: the pending world-inverse refresh. Retail's two silhouette bodies test +0x358 and
// call it; the draw prelude below carries the same test and body inline. [INFERRED] name.
void StaticMesh::refreshInverseTransform()
{
    fullInvert(*m_transform, m_matrix290);
    m_byte358 = 0;
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
        typedef void* (__thiscall* GetDword350Fn)(const void*);
        const void* const stamped =
            reinterpret_cast<GetDword350Fn>(m_vptr[0xB0 / 4])(this);
        // The pointer-typed slot carries a 32-bit frame ordinal.
        if (static_cast<unsigned int>(reinterpret_cast<uintptr_t>(stamped))
            == dx9rend::rendererFrameOrdinal())                                  // +0x54
            casterThisFrame = true;
    }
    m_byte355 = casterThisFrame ? 1 : 0;

    if (casterThisFrame)
    {
        const LightShadowRecord* const caster = reinterpret_cast<const LightShadowRecord*>(m_dword34C);
        Mat4 projection = Mat4::getIdentity();
        projection.fullMult(*m_transform, caster->matrices[4]);                 // +0x2F8
        std::memcpy(op.shadowProjMat.value, projection.m, sizeof(op.shadowProjMat.value));
        Mat4 trapezoid = Mat4::getIdentity();
        trapezoid.fullMult(*m_transform, caster->matrices[7]);                  // +0x3B8
        std::memcpy(op.shadowTrapMat.value, trapezoid.m, sizeof(op.shadowTrapMat.value));
    }
}


// Loads the selected geometry/LOD lightmap from loose files or the level atlas.
// Failed atlas lookups retain a null cache entry for retry; editor failures mark no lightmap.

void StaticMesh::loadLightmap()
{
    if (m_byte35A && !rendererBaseFlag102())
        return;

    typedef int (__thiscall* IndexFn)(const void*);
    const int geom = reinterpret_cast<IndexFn>(m_vptr[0x70 / 4])(this);
    const int lod  = reinterpret_cast<IndexFn>(m_vptr[0x7C / 4])(this);
    const unsigned int key = static_cast<unsigned int>(lod + 2 * (5 * geom));

    StaticMeshLightmapEntry& entry = m_map260[key];
    dx9rend::EngineTexture*& slot = entry.texture;
    if (slot)
        return;

    StaticMeshRenderer* const smr = staticMeshRenderer();
    if (rendererBaseFlag102())
    {
        std::string name("");
        staticMeshLightmapName(this, name);
        std::string path(name);
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
        m_dword26C = reinterpret_cast<uintptr_t>(texture);
    }
    else if (smr->m_flags60[4])
    {
        const int savedMipClass = dx9rend::textureManagerSwapDefaultMipClass(13);
        std::string objectName("");
        staticMeshLightmapObjectName(this, objectName);
        std::string directory("");
        staticMeshLightmapDirectory(directory);
        staticMeshRendererTryLoadLightmapTexture(smr, directory, objectName, entry.offset, &slot);
        dx9rend::textureManagerSwapDefaultMipClass(savedMipClass);
        if (!slot)
        {
            m_dword26C = 0;
            m_byte35A  = 1;
            return;
        }
        m_dword26C = reinterpret_cast<uintptr_t>(slot);
        // sub_1000BDD0(this + 0x270, entry + 4): the four offset floats.
        const unsigned int* const offset = reinterpret_cast<const unsigned int*>(entry.offset);
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
        std::string name("StaticMesh : ");
        name += t->getName();
        m_dword300 = reinterpret_cast<uintptr_t>(
            dx9rend::createVertexBuffer(dx9rend::g_bufferManager, vertexCount, 4, name, 8));
    }
    void* const buffer = reinterpret_cast<void*>(m_dword300);
    if (!buffer)
        return;

    unsigned short* const words = static_cast<unsigned short*>(
        dx9rend::vertexBufferLock(buffer, 0, vertexCount, 0));
    std::memset(words, 0, 4u * vertexCount);

    typedef int (__thiscall* IndexFn)(const void*);
    const int savedGeom = reinterpret_cast<IndexFn>(m_vptr[0x70 / 4])(this);
    const int savedLod  = reinterpret_cast<IndexFn>(m_vptr[0x7C / 4])(this);

    const StaticMeshTemplate* const objectTemplate =
        static_cast<const StaticMeshTemplate*>(m_template);
    const unsigned int geomCount = static_cast<unsigned int>(objectTemplate->m_geoms.size());

    for (unsigned int g = 0; g < geomCount; ++g)
    {
        m_dword314 = g;
        const unsigned int lodCount =
            static_cast<unsigned int>(objectTemplate->m_geoms[g].lods.size());

        for (unsigned int l = 0; l < lodCount; ++l)
        {
            m_dword318 = l;
            std::string name("");
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
                m_vector2F0.resize(count);                                   // sub_10199AE0
                for (std::vector<VertexLightRecord>::iterator record = m_vector2F0.begin();
                     record != m_vector2F0.end(); ++record)
                {
                    if (cursor + 28 <= end)
                        std::memcpy(record->bytes, cursor, 28);              // 12 + 12 + 4
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

    typedef void (__thiscall* SetIndexFn)(void*, int);
    reinterpret_cast<SetIndexFn>(m_vptr[0x6C / 4])(this, savedGeom);
    reinterpret_cast<SetIndexFn>(m_vptr[0x78 / 4])(this, savedLod);
    reinterpret_cast<SetIndexFn>(m_vptr[0x78 / 4])(this, -1);
    dx9rend::vertexBufferUnlock(buffer);
}


// draw - +0x18, 0x100FF510.  The dispatcher.

void StaticMesh::draw(void* renderView, void* occluder, int flag)
{
    (void)renderView;
    (void)occluder;
    (void)flag;
    if (dx9rend::rendererIsExpansionPack())                                   // +0xAC
        staticMeshDrawExpansion(this);
    else
        drawVanilla();
}


// PE 0x100FC7D0. Selects LOD, lighting and material techniques, then queues visible materials.
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
        // sub_1007AFE0: the camera's translation minus the transform's.
        const Vec3 difference = rendmath::subtract(
            *reinterpret_cast<const Vec3*>(&camera->m[12]),
            *reinterpret_cast<const Vec3*>(&m_transform->m[12]));
        const float dx = difference.x;
        const float dy = difference.y;
        const float dz = difference.z;
        distance = static_cast<float>(std::sqrt(static_cast<double>(dz * dz + dy * dy + dx * dx)));
    }
    *reinterpret_cast<float*>(&m_dword288) = distance;
    *reinterpret_cast<float*>(&op.tail220)  = distance;

    // 3. lod selection
    if (!m_byte357)
    {
        const float radius = m_geomRadii.data()[m_dword314];
        const float lodDistance = mainView->getInverseZoom() * distance - radius;   // +0x2C

        int selected = 0;
        int i = static_cast<int>(m_geomLodDistanceCounts.data()[m_dword314]) - 1;
        if (i >= 0)
        {
            const float* const switchDistances = t->m_geoms[m_dword314].lodDistances.data();
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
            typedef void (__thiscall* SetLodFn)(void*, int);
            reinterpret_cast<SetLodFn>(m_vptr[0x78 / 4])(this, selected);
            m_byte357 = 0;
        }
    }
    if (smr->m_byte94 && !m_byte357)
    {
        typedef void (__thiscall* SetLodFn)(void*, int);
        reinterpret_cast<SetLodFn>(m_vptr[0x78 / 4])(this, 0);
        m_byte357 = 0;
    }

    // 4. the lightmap, the view, the parameters
    if (!m_dword26C && !m_byte35A)
        loadLightmap();
    prepareView();
    updateObjectParameters();

    const bool transparent = op.transparency.value < 1.0f;

    // 5. the point-light gather
    m_vector2D0.clear();                                                  // sub_100E1C20(0, 0)
    if (!transparent)
    {
        const int maxLights = g_LightManager->getMaxLightsPerStaticMesh();          // +0x68
        const unsigned int required = m_dword26C ? 0x11B09u : 0x1B09u;
        g_LightManager->gatherLightsForGeometry(this, m_vector2D0,
                                                static_cast<unsigned int>(maxLights),
                                                required, 0u, true);                // +0x20
    }

    const unsigned int lightCount = static_cast<unsigned int>(m_vector2D0.size());
    const bool hasLights = lightCount != 0 && !transparent;
    m_byte354 = hasLights ? 1 : 0;

    if (hasLights)
    {
        // The fill record, built on the stack exactly as sub_10046540 builds one: fourteen zero
        // words, then 1.0f at +0x3C and +0x40, +0x38 untouched.
        dx9rend::Ra::RaLightRecord fill;
        std::memset(&fill, 0, 0x38);
        fill.float3C = 1.0f;
        fill.float40 = 1.0f;
        m_vector2E0.resize(lightCount, fill);                            // sub_10102690

        world::LightSource* const* const lights = m_vector2D0.data();
        dx9rend::Ra::RaLightRecord* const records = m_vector2E0.data();
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
            const unsigned int maskCount = static_cast<unsigned int>(g_staticMeshQualityMasks.size());
            // `count` when the lod is past the end - one element past the vector.  Shipped.
            maskIndex = (maskCount < m_dword318) ? maskCount : m_dword318;
        }
        else if (rendererByte(0x708) && m_dword320 > maskIndex)
        {
            maskIndex = m_dword320;
        }
        const unsigned int qualityMask = g_staticMeshQualityMasks.data()[maskIndex];

        StaticMeshMaterial* const materialsFirst =
            m_dword310->data();
        const unsigned int materialCount = materialsFirst
            ? static_cast<unsigned int>(
                  (m_dword310->data() + m_dword310->size()) - materialsFirst)
            : 0u;

        for (unsigned int i = 0; i < materialCount; ++i)
        {
            m_dword31C = i;
            StaticMeshMaterial* const material =
                m_dword310->data() + i;

            if (smr->m_flags60[0])
            {
                StaticMeshBoundsCorners corners(material->m_bounds, *m_transform);   // +0x198
                const Frustum* const frustum =
                    dx9rend::rendererGetRenderViewDup()->getFrustumPtr();        // +0x50, +0x74
                if (corners.outside(*frustum))
                {
                    ++smr->m_culledMaterials;
                    continue;
                }
            }

            dx9rend::RenderQueueEntry* const entry = dx9rend::renderEntryPoolAcquire();
            entry->materialParameters = material->m_parameters;              // +0x2C
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

            if (t->m_isWater && g_Terrain && m_dword26C)
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

                const Terrain* const terrain = static_cast<const Terrain*>(g_Terrain);
                dx9rend::Ra::RaShader* const nearShader = terrain->m_waterDistantTechnique;   // +0x718
                dx9rend::Ra::RaShader* const farShader  = terrain->m_waterDistantTechnique2;  // +0x71C
                const float threshold = terrain->m_waterTechniqueMargin;                      // +0x70C
                MeshBounds* const bounds = &m_dword30C->bounds;
                if (bounds->dirty)
                    meshBoundsFinalize(*bounds);
                float waterDistance;
                std::memcpy(&waterDistance, &m_dword288, sizeof(waterDistance));
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
                // [CHOSEN, not recovered] Retain the FSub intermediate through the comparison.
                const bool farWater = bf2x87::greater(bf2x87::sub(bf2x87::wide(waterDistance),
                    bf2x87::wide(bounds->radius)), bf2x87::wide(threshold));
#else
                const bool farWater = waterDistance - bounds->radius > threshold;
#endif
                // The shipped FCOM branch selects the near shader for unordered values too.
                staticMeshMaterialShader(material) =
                    farWater ? farShader : nearShader;

                // The material block's CubeMap holder (material + 0x148 / +0x14C): the terrain's
                // +0x6C0 object's texture, else the LightManager's cube at its +0x248.
                if (!staticMeshMaterialCubeMap(material))
                {
                    const TerrainEnvMapRecord* const terrainCube =
                        static_cast<const Terrain*>(g_Terrain)->m_waterEnvMap; // +0x6C0, fresh owner
                    dx9rend::EngineTexture* const cube = terrainCube
                        ? static_cast<dx9rend::EngineTexture*>(terrainCube->texture)    // +0x10
                        : lightManagerBlackCubeTexture(g_LightManager);                  // +0x248
                    holderSetTexture(staticMeshMaterialParameters(material).cubeMap, cube);
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
                dx9rend::EngineTexture* const scratch =
                    dx9rend::rendererGetHemiScratchTexture();                    // Renderer +0x774
                if (scratch)
                    staticMeshSwapD3DTexture(entry->lighting.lightMap.value, scratch->baseTexture);

                dx9rend::Ra::RaShader* shader;
                // DetailMap.value: material +0xF0, parameter block +0xC4.
                if (staticMeshMaterialParameters(material).detailMap.value == 0)
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
                else if (material->m_vegetationNonTiled)                       // +0x1EC
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
                    if (material->m_vegetationNonTiled)                        // +0x1EC
                    {
                        pointShader = staticMeshFindShaderBits(
                            m_byte355 ? 0xF0000600u : 0xF0000400u);
                    }
                    else
                    {
                        const ShaderMaterial* const shaderMaterial = material->m_shaderMaterial;
                        unsigned int pointBits = g_shaderComboFlags[shaderMaterial->shaderCombo];
                        typedef unsigned int (__thiscall* GetFlagsFn)(const void*);
                        if ((reinterpret_cast<GetFlagsFn>(m_vptr[0xC4 / 4])(this)
                             & 1u) != 0)
                            pointBits &= ~8u;
                        pointBits |= 0x400u;
                        if (material->m_alphaMode == 2u)                       // +0x17C
                            pointBits |= 0x800u;
                        pointShader = staticMeshFindShaderBits(qualityMask & pointBits);
                    }
                    staticMeshMaterialShader(material) = pointShader;

                    for (unsigned int k = 0; ; ++k)
                    {
                        const unsigned int recordCount =
                            static_cast<unsigned int>(m_vector2E0.size());
                        if (k >= recordCount)
                            break;

                        dx9rend::RenderQueueEntry* const lightEntry =
                            dx9rend::renderEntryPoolAcquire();
                        dx9rend::Ra::RaLightRecord* const destination =
                            static_cast<dx9rend::Ra::RaLightRecord*>(
                                lightEntry->lightArray.lights.value);
                        lightEntry->material           = material;
                        lightEntry->materialParameters = material->m_parameters;
                        lightEntry->objectParameters   = &op;
                        lightEntry->shader             = staticMeshMaterialShader(material);
                        *destination = m_vector2E0[k];
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

// [INFERRED] names for the expansion draw's file-scope hashes. PE CRT initializers
// 0x101D2D20/0x101D2D50/0x101D2D80/0x101D2DB0/0x101D2DE0.
const unsigned int kBeforeLight = dx9rend::raHashString(std::string("BeforeLight"));
const unsigned int kStaticLit = dx9rend::raHashString(std::string("StaticLit"));
const unsigned int kDynamicPointLit = dx9rend::raHashString(std::string("DynamicPointLit"));
const unsigned int kDynamicSpotLit = dx9rend::raHashString(std::string("DynamicSpotLit"));
const unsigned int kAfterLight = dx9rend::raHashString(std::string("AfterLight"));

#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
// [CHOSEN, not recovered] Carry the PE's x87 arithmetic between explicit float stores.
typedef bf2x87::Real ExpansionReal;
#else
typedef float ExpansionReal;
#endif

// PE 0x100FB160. The first flag is passed in edx; the second consumes one stack word.
void expansionEnqueue(dx9rend::RenderQueueEntry* entry, bool transparent, unsigned int flag)
{
    const unsigned int index = (flag * 2u) | static_cast<unsigned int>(transparent);
    if (entry->techniqueKey == 0 || entry->techniqueKey == kAfterLight)
        dx9rend::renderQueueEnqueue(entry, transparent, flag != 0, false);
    else if (entry->techniqueKey == kBeforeLight)
        dx9rend::renderQueueAddToSlot(16u + index, entry);
    else if (entry->techniqueKey == kStaticLit)
        dx9rend::renderQueueAddToSlot(20u + index, entry);
    else if (entry->techniqueKey == kDynamicPointLit || entry->techniqueKey == kDynamicSpotLit)
        dx9rend::renderQueueAddToSlot(24u + index, entry);
}

void expansionLightmap(StaticMesh& mesh, dx9rend::RenderQueueEntry* entry)
{
    dx9rend::EngineTexture* const lightmap =
        reinterpret_cast<dx9rend::EngineTexture*>(mesh.m_dword26C);
    staticMeshSwapD3DTexture(entry->lighting.lightMap.value, lightmap->baseTexture);
    staticMeshStoreLightMapOffset(entry, &mesh);
}

// The same, as staticMeshDrawExpansion's own body emits it (0x100FEA3F..0x100FEA57 and
// 0x100FEE23..0x100FEE3B): the four words at +0x270..+0x27C go to the LightMapOffset holder through
// the Vec4 holder setter (Ra::raParamVec4Set) instead of four inline stores.
void expansionLightmapThroughSetter(StaticMesh& mesh, dx9rend::RenderQueueEntry* entry)
{
    dx9rend::EngineTexture* const lightmap =
        reinterpret_cast<dx9rend::EngineTexture*>(mesh.m_dword26C);
    staticMeshSwapD3DTexture(entry->lighting.lightMap.value, lightmap->baseTexture);
    Vec4 offset;
    std::memcpy(&offset, &mesh.m_float270, sizeof(offset));                // +0x270..+0x27C
    dx9rend::Ra::raParamVec4Set(entry->lighting.lightMapOffset, offset);
}

// PE 0x10077A30. [INFERRED] local spelling for the shared inline matrix operation.
// Keep the shipped addition order and sequential writes, including aliased inputs.
void expansionProduct(Mat4& destination, const Mat4& lhs, const Mat4& rhs)
{
    static const unsigned char order[16][4] = {
        {0,2,1,3}, {3,1,0,2}, {3,1,0,2}, {0,3,2,1},
        {1,2,3,0}, {1,0,3,2}, {1,0,3,2}, {2,1,0,3},
        {3,0,2,1}, {1,0,3,2}, {1,0,3,2}, {0,1,3,2},
        {2,1,3,0}, {0,3,2,1}, {0,3,2,1}, {2,3,1,0}
    };
    for (unsigned int column = 0; column < 4; ++column)
        for (unsigned int row = 0; row < 4; ++row)
        {
            const unsigned int index = row * 4 + column;
            const unsigned char* const sequence = order[index];
            ExpansionReal value = ExpansionReal(lhs.m[row * 4 + sequence[0]])
                                * ExpansionReal(rhs.m[sequence[0] * 4 + column]);
            for (unsigned int i = 1; i < 4; ++i)
                value = value + ExpansionReal(lhs.m[row * 4 + sequence[i]])
                              * ExpansionReal(rhs.m[sequence[i] * 4 + column]);
            destination.m[index] = static_cast<float>(value);
        }
}

// PE 0x10102C00. Zero-count resizes retain storage; matrix growth fills identity values.
void expansionShadowArrays(dx9rend::RenderQueueEntry* entry, const Mat4* matrices,
                           const Vec4* positions, const Vec4* sizes, int count)
{
    dx9rend::Ra::RaLightingParameters& lighting = entry->lighting;
    lighting.shadowPosArray.value.resize(static_cast<size_t>(count) * sizeof(Vec4), 0);
    lighting.shadowSizeArray.value.resize(static_cast<size_t>(count) * sizeof(Vec4), 0);
    std::vector<unsigned char>* const arrays[] = {
        &lighting.shadowViewProjArray.value, &lighting.clampViewProjArray.value
    };
    for (unsigned int a = 0; a < 2; ++a)
    {
        const size_t oldCount = arrays[a]->size() / sizeof(Mat4);
        arrays[a]->resize(static_cast<size_t>(count) * sizeof(Mat4));
        for (size_t i = oldCount; i < static_cast<size_t>(count); ++i)
            std::memcpy(arrays[a]->data() + i * sizeof(Mat4), &Mat4::getIdentity(), sizeof(Mat4));
    }
    for (int i = 0; i < count; ++i)
    {
        const Vec4 packed = { positions[i].x, positions[i].y, sizes[i].x, sizes[i].y };
        std::memcpy(lighting.shadowPosArray.value.data() + i * sizeof(Vec4), &packed, sizeof(packed));
        if (dx9rend::rendererPerformanceFlag(30))
        {
            Mat4 atlas = Mat4::getZero();
            atlas.m[0] = sizes[i].x;
            atlas.m[5] = sizes[i].y;
            atlas.m[10] = atlas.m[15] = 1.0f;
            atlas.m[12] = positions[i].x;
            atlas.m[13] = positions[i].y;
            expansionProduct(*reinterpret_cast<Mat4*>(
                lighting.shadowViewProjArray.value.data() + i * sizeof(Mat4)), matrices[i], atlas);
            static const Mat4 clamp = {{
                0.5f,0,0,0, 0,0.5f,0,0, 0,0,1,0, 0.25f,0.25f,0,1
            }};
            expansionProduct(*reinterpret_cast<Mat4*>(
                lighting.clampViewProjArray.value.data() + i * sizeof(Mat4)), matrices[i], clamp);
        }
        else
            std::memcpy(lighting.shadowViewProjArray.value.data() + i * sizeof(Mat4),
                        &matrices[i], sizeof(Mat4));
    }
    lighting.shadowCount.value = count;
}

// PE 0x100E2550/0x10102430/0x100E29D0/0x10102F00; names/placement [INFERRED].
struct ExpansionShadowBatch
{
    Mat4* matrices;
    Vec4* positions;
    Vec4* sizes;
    int capacity;
    int count;
    std::vector<TerrainShadowRange> ranges;
    explicit ExpansionShadowBatch(int limit) : capacity(limit), count(0)
    {
        matrices = static_cast<Mat4*>(::renddx9abi::engineAlloc(limit * sizeof(Mat4)));
        if (matrices)
            for (int i = 0; i < limit; ++i) matrices[i] = Mat4::getIdentity();
        positions = static_cast<Vec4*>(::renddx9abi::engineAlloc(limit * sizeof(Vec4)));
        if (positions) std::memset(positions, 0, limit * sizeof(Vec4));
        sizes = static_cast<Vec4*>(::renddx9abi::engineAlloc(limit * sizeof(Vec4)));
        if (sizes) std::memset(sizes, 0, limit * sizeof(Vec4));
        ranges.reserve(limit);
    }
    ~ExpansionShadowBatch()
    {
        // PE 0x101D9370 frees the three arrays before the range-vector storage.
        ::renddx9abi::engineFree(matrices);
        ::renddx9abi::engineFree(positions);
        ::renddx9abi::engineFree(sizes);
    }
    // PE 0x10102430
    void clear()
    {
        ranges.clear();
        count = 0;
    }
    bool append(const TerrainShadowRange& range, const Mat4& matrix,
                const Vec4& position, const Vec4& size)
    {
        if (count >= capacity) return false;
        matrices[count] = matrix;
        positions[count] = position;
        sizes[count] = size;
        ++count;
        ranges.push_back(range);
        return true;
    }
    // PE 0x10102F00
    void apply(dx9rend::RenderQueueEntry* entry) const
    {
        if (count) expansionShadowArrays(entry, matrices, positions, sizes, count);
    }
};

// PE 0x100FBA80. Retains both materialShaderBits calls in the static-light arm.
void expansionProjectedEntry(StaticMesh& mesh, StaticMeshMaterial* material,
                             unsigned int mask, XpLight* light, dx9rend::RenderQueueEntry* entry)
{
    mesh.materialShaderBits(material, false);
    const bool transparent = mesh.objectParameters().transparency.value < 1.0f;
    unsigned int bits;
    dx9rend::Ra::RaShader* shader;
    if (xpLightCategory(light) == 0)
    {
        const int type = xpLightType(light);
        if (type == 1)
            entry->techniqueKey = kDynamicPointLit;
        else if (type == 2)
            entry->techniqueKey = kDynamicSpotLit;
        bits = g_shaderComboFlags[material->m_shaderMaterial->shaderCombo];
        typedef unsigned int (__thiscall* FlagsFn)(const void*);
        if (reinterpret_cast<FlagsFn>(mesh.m_vptr[0xC4 / 4])(&mesh) & 1u)
            bits &= ~8u;
        bits |= 0x400u;
        if (material->m_alphaMode == 2)
            bits |= 0x800u;
        shader = staticMeshFindShaderBits(bits & mask);
    }
    else
    {
        entry->techniqueKey = kStaticLit;
        bits = mesh.materialShaderBits(material, false);
        shader = staticMeshFindShaderBits(bits & mask);
    }
    if (mesh.m_dword26C && (bits & 1u))
        expansionLightmap(mesh, entry);
    entry->material = material;
    entry->materialParameters = material->m_parameters;
    entry->objectParameters = &mesh.objectParameters();
    xpLightFillRecord(light, mesh.m_matrix290,
        *static_cast<dx9rend::Ra::RaLightRecord*>(entry->lightArray.lights.value));
    entry->shader = shader;
    // ds:1025A6AC is loader-zero and has no writer in the image.
    if (!dx9rend::rendererPerformanceFlag(18))
        expansionEnqueue(entry, transparent, 0);
}

// PE 0x100FE130 (also 0x100FB200, 0x100FB610, 0x100FB760, 0x100FB830, 0x100FB980, 0x100FBA30): the
// dispatcher's six arm helpers, which retail emits out of line, are inlined below at the PE address
// each arm carries. The dispatcher supplies three unused stack arguments (retn 0x0C).
void staticMeshDrawExpansion(void* self)
{
    StaticMesh& mesh = *static_cast<StaticMesh*>(self);
    dx9rend::Ra::RaObjectParameters& op = mesh.objectParameters();

    // 1. the early outs
    StaticMeshTemplate* const t = static_cast<StaticMeshTemplate*>(mesh.m_template);
    if (!t->m_vertexStream)                                               // template +0x154
        return;
    StaticMeshRenderer* const smr = staticMeshRenderer();
    if (!smr->m_enabled && !t->m_isRoad)
        return;
    if (t->m_isRoad && !rendererByte(0x6A4))
        return;
    if (!rendererByte(0x6A7))
    {
        world::IObject* const owner = static_cast<world::IObject*>(mesh.m_dword330);
        if (owner && owner->getIsOvergrowth())                            // IObject PE +0x12C
            return;
    }

    // 2. the camera distance
    dx9rend::IRenderView* const mainView = dx9rend::rendererGetRenderView();    // +0x48
    typedef const Mat4* (__thiscall* TransformFn)(const void*);
    const Mat4* const transform = reinterpret_cast<TransformFn>(mesh.m_vptr[0x48 / 4])(&mesh);
    const Mat4* const camera = mainView->getViewMatrixPtr();
    const float dx = camera->m[12] - transform->m[12];
    const ExpansionReal dy = ExpansionReal(camera->m[13]) - ExpansionReal(transform->m[13]);
    const ExpansionReal dz = ExpansionReal(camera->m[14]) - ExpansionReal(transform->m[14]);
    const ExpansionReal squared = (ExpansionReal(dx) * ExpansionReal(dx) + dz * dz) + dy * dy;
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
    const float distance = bf2x87::store(bf2x87::squareRoot(squared));
#else
    const float distance = std::sqrt(squared);
#endif
    std::memcpy(&mesh.m_dword288, &distance, sizeof(distance));
    std::memcpy(&op.tail220, &distance, sizeof(distance));

    // 3. lod selection
    if (!mesh.m_byte357)
    {
        const float radius = mesh.m_geomRadii.data()[mesh.m_dword314];
        const float lodDistance = static_cast<float>(ExpansionReal(mainView->getInverseZoom()) * ExpansionReal(distance) - ExpansionReal(radius));   // +0x2C

        int selected = 0;
        int i = static_cast<int>(mesh.m_geomLodDistanceCounts.data()[mesh.m_dword314]) - 1;
        if (i >= 0)
        {
            const float* const switchDistances = t->m_geoms[mesh.m_dword314].lodDistances.data();
            const float scale = static_cast<float>(ExpansionReal(rendererFloat(0x8B0)) * ExpansionReal(rendererFloat(0x84C)));
            for (;;)
            {
                if (ExpansionReal(scale) * ExpansionReal(switchDistances[i]) < ExpansionReal(lodDistance))
                {
                    selected = i + 1;
                    break;
                }
                if (--i < 0)
                    break;
            }
        }

        mesh.m_dword320 = 0;
        if (rendererByte(0x708))
        {
            for (int bucket = 3; bucket >= 0; --bucket)
            {
                const float threshold = (bucket < 0 || bucket > 3)
                    ? 0.0f
                    : rendererFloat(0x70C + 4u * static_cast<unsigned int>(bucket));
                if (lodDistance > threshold)
                {
                    mesh.m_dword320 = static_cast<unsigned int>(bucket + 1);
                    break;
                }
            }
        }

        if (selected != static_cast<int>(mesh.m_dword318))
        {
            typedef void (__thiscall* SetLodFn)(void*, int);
            reinterpret_cast<SetLodFn>(mesh.m_vptr[0x78 / 4])(&mesh, selected);
            mesh.m_byte357 = 0;
        }
    }
    if (smr->m_byte94 && !mesh.m_byte357)
    {
        typedef void (__thiscall* SetLodFn)(void*, int);
        reinterpret_cast<SetLodFn>(mesh.m_vptr[0x78 / 4])(&mesh, 0);
        mesh.m_byte357 = 0;
    }

    // 4. the lightmap, the view, the parameters
    if (!mesh.m_dword26C && !mesh.m_byte35A)
        mesh.loadLightmap();
    mesh.prepareView();
    mesh.updateObjectParameters();

    const bool transparent = op.transparency.value < 1.0f;

    static std::vector<XpLight*> projectedLights;          // ds:1025A9C8
    projectedLights.clear();
    static std::vector<world::LightSource*> lights;        // ds:1025A9B8
    lights.clear();
    mesh.m_byte354 = 0;
    typedef const MeshBounds* (__thiscall* BoundsFn)(const void*);
    const MeshBounds* const bounds = reinterpret_cast<BoundsFn>(mesh.m_vptr[0x40 / 4])(&mesh);
    StaticMeshBoundsCorners worldBounds(*bounds, *mesh.m_transform);
    const bool cone = t->m_byte2B0 != 0;
    if (!cone && !transparent)
    {
        if (dx9rend::rendererIsExpansionPack())
            appendXpLightsForGeometry(&mesh, projectedLights);
        else
        {
            const unsigned int required = mesh.m_dword26C ? 0x11B09u : 0x1B09u;
            const int limit = g_LightManager->getMaxLightsPerStaticMesh();
            g_LightManager->gatherLightsForGeometry(&mesh, lights, limit, required, 0u, true);
        }
        for (size_t i = 0; !mesh.m_byte354 && i < projectedLights.size(); ++i)
        {
            XpLight* const light = projectedLights[i];
            if (xpLightCategory(light) == 0)
                mesh.m_byte354 = 1;
            else
            {
                const std::vector<LightShadowRecord>& records = xpLightShadowRecords(light);
                for (size_t j = 0; j < records.size(); ++j)
                    if (!worldBounds.outside(records[j].frustum))
                    {
                        mesh.m_byte354 = 1;
                        break;
                    }
            }
        }
    }
    static std::vector<unsigned int> unused;              // ds:1025A9A8; [UNRESOLVED] word elements
    static std::vector<LightShadowRecord> unusedRecords;   // ds:1025A998
    unused.clear();
    { LightShadowRecord fill; unusedRecords.resize(0, fill); }

    // 6. the sun direction in object space
    float sunDirection[3] = { 0.0f, 0.0f, 0.0f };
    {
        world::LightSource* const sky = g_LightManager->getSkyLight();         // +0x28
        if (sky)
        {
            const Vec3& d = sky->getDirection();                               // PE +0x17C
            const float* const m = mesh.m_matrix290.m;
            sunDirection[0] = static_cast<float>((ExpansionReal(m[8]) * ExpansionReal(d.z) + ExpansionReal(m[4]) * ExpansionReal(d.y)) + ExpansionReal(d.x) * ExpansionReal(m[0]));
            sunDirection[1] = static_cast<float>((ExpansionReal(m[9]) * ExpansionReal(d.z) + ExpansionReal(m[1]) * ExpansionReal(d.x)) + ExpansionReal(m[5]) * ExpansionReal(d.y));
            sunDirection[2] = static_cast<float>((ExpansionReal(m[10]) * ExpansionReal(d.z) + ExpansionReal(m[2]) * ExpansionReal(d.x)) + ExpansionReal(m[6]) * ExpansionReal(d.y));
        }
    }

    // 7. the materials
    if (!mesh.m_byte356)
    {
        unsigned int maskIndex = rendererDword(0x6CC);
        if (rendererByte(0x6D0))
        {
            const unsigned int maskCount = static_cast<unsigned int>(g_staticMeshQualityMasks.size());
            // `count` when the lod is past the end - one element past the vector.  Shipped.
            maskIndex = (maskCount < mesh.m_dword318) ? maskCount : mesh.m_dword318;
        }
        else if (rendererByte(0x708) && mesh.m_dword320 > maskIndex)
        {
            maskIndex = mesh.m_dword320;
        }
        const unsigned int qualityMask = g_staticMeshQualityMasks.data()[maskIndex];

        StaticMeshMaterial* const materialsFirst =
            mesh.m_dword310->data();
        const unsigned int materialCount = materialsFirst
            ? static_cast<unsigned int>(
                  (mesh.m_dword310->data() + mesh.m_dword310->size()) - materialsFirst)
            : 0u;

        for (unsigned int i = 0; i < materialCount; ++i)
        {
            mesh.m_dword31C = i;
            StaticMeshMaterial* const material =
                mesh.m_dword310->data() + i;

            if (smr->m_flags60[0])
            {
                StaticMeshBoundsCorners corners(material->m_bounds, *mesh.m_transform);   // +0x198
                const Frustum* const frustum =
                    dx9rend::rendererGetRenderViewDup()->getFrustumPtr();        // +0x50, +0x74
                if (corners.outside(*frustum))
                {
                    ++smr->m_culledMaterials;
                    continue;
                }
            }

            dx9rend::RenderQueueEntry* const entry = dx9rend::renderEntryPoolAcquire();
            entry->materialParameters = material->m_parameters;              // +0x2C
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

            if (rendererByte(0x69C))
            {
                const Mat4* const view = dx9rend::rendererGetRenderView()->getViewMatrixPtr();
                const float* const m = mesh.m_matrix290.m;
                sun->position[0] = static_cast<float>(((ExpansionReal(m[8]) * ExpansionReal(view->m[14])
                    + ExpansionReal(m[4]) * ExpansionReal(view->m[13]))
                    + ExpansionReal(view->m[12]) * ExpansionReal(m[0])) + ExpansionReal(m[12]));
                sun->position[1] = static_cast<float>(((ExpansionReal(m[9]) * ExpansionReal(view->m[14])
                    + ExpansionReal(m[1]) * ExpansionReal(view->m[12]))
                    + ExpansionReal(m[5]) * ExpansionReal(view->m[13])) + ExpansionReal(m[13]));
                sun->position[2] = static_cast<float>(((ExpansionReal(m[10]) * ExpansionReal(view->m[14])
                    + ExpansionReal(m[2]) * ExpansionReal(view->m[12]))
                    + ExpansionReal(m[6]) * ExpansionReal(view->m[13])) + ExpansionReal(m[14]));
                staticMeshMaterialParameters(material).pointSun.value = 1;
            }
            else
                staticMeshMaterialParameters(material).pointSun.value = 0;
            static const Mat4* identity = &Mat4::getIdentity(); // ds:1025A994
            dx9rend::EngineTexture* const shadowMap = lightManagerPointShadowMapTexture(g_LightManager);
            if (shadowMap)
            {
                std::memset(entry->lighting.unnamed008.value, 0, sizeof(entry->lighting.unnamed008.value));
                staticMeshSwapD3DTexture(entry->lighting.terrainLightMap.value, shadowMap->baseTexture);
                entry->lighting.matBones.a = reinterpret_cast<uintptr_t>(&identity);
                entry->lighting.matBones.b = 0;
            }

            if (t->m_isWater && g_Terrain && mesh.m_dword26C)
            {
                // PE 0x100FB610, inlined water arm.
                std::memcpy(op.world.value, mesh.m_transform->m, sizeof(op.world.value));
                std::memcpy(op.viewProjection.value,
                            dx9rend::rendererGetRenderViewDup()->getViewProjectionMatrixPtr()->m,
                            sizeof(op.viewProjection.value));
                dx9rend::EngineTexture* const lightmap =
                    reinterpret_cast<dx9rend::EngineTexture*>(mesh.m_dword26C);
                if (lightmap)
                {
                    staticMeshSwapD3DTexture(entry->lighting.lightMap.value, lightmap->baseTexture);
                    staticMeshStoreLightMapOffset(entry, &mesh);
                }

                const Terrain* const terrain = static_cast<const Terrain*>(g_Terrain);
                dx9rend::Ra::RaShader* const nearShader = terrain->m_waterDistantTechnique;   // +0x718
                dx9rend::Ra::RaShader* const farShader  = terrain->m_waterDistantTechnique2;  // +0x71C
                const float threshold = terrain->m_waterTechniqueMargin;                      // +0x70C
                MeshBounds* const bounds = &mesh.m_dword30C->bounds;
                if (bounds->dirty)
                    meshBoundsFinalize(*bounds);
                float waterDistance;
                std::memcpy(&waterDistance, &mesh.m_dword288, sizeof(waterDistance));
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
                // [CHOSEN, not recovered] PE 0x100FB6D7 retains the subtraction through FCOM.
                const bool farWater = bf2x87::greater(bf2x87::sub(bf2x87::wide(waterDistance),
                    bf2x87::wide(bounds->radius)), bf2x87::wide(threshold));
#else
                const bool farWater = waterDistance - bounds->radius > threshold;
#endif
                staticMeshMaterialShader(material) =
                    farWater ? farShader : nearShader;

                // The material block's CubeMap holder (material + 0x148 / +0x14C): the terrain's
                // +0x6C0 object's texture, else the LightManager's cube at its +0x248.
                if (!staticMeshMaterialCubeMap(material))
                {
                    const TerrainEnvMapRecord* const terrainCube =
                        static_cast<const Terrain*>(g_Terrain)->m_waterEnvMap; // +0x6C0, fresh owner
                    dx9rend::EngineTexture* const cube = terrainCube
                        ? static_cast<dx9rend::EngineTexture*>(terrainCube->texture)    // +0x10
                        : lightManagerBlackCubeTexture(g_LightManager);                  // +0x248
                    holderSetTexture(staticMeshMaterialParameters(material).cubeMap, cube);
                }
                entry->shader = staticMeshMaterialShader(material);
                dx9rend::renderQueueAddToSlot(9, entry);                         // RSMTWater, +0x28
            }
            else if (t->m_isRoad)
            {
                // PE 0x100FB760, inlined road arm.
                std::memcpy(op.world.value, mesh.m_transform->m, sizeof(op.world.value));
                std::memcpy(op.viewProjection.value,
                            dx9rend::rendererGetRenderViewDup()->getViewProjectionMatrixPtr()->m,
                            sizeof(op.viewProjection.value));
                dx9rend::EngineTexture* const scratch =
                    dx9rend::rendererGetHemiScratchTexture();                    // Renderer +0x774
                if (scratch)
                    staticMeshSwapD3DTexture(entry->lighting.lightMap.value, scratch->baseTexture);

                dx9rend::Ra::RaShader* shader;
                // DetailMap.value: material +0xF0, parameter block +0xC4.
                if (staticMeshMaterialParameters(material).detailMap.value == 0)
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
                unsigned int bits = mesh.materialShaderBits(material, false);
                if ((t->m_dword2B4 & 2) && dx9rend::rendererGetVideoOptions()->getQuality(4) < 3)
                    bits &= ~0x200u;
                const bool lowShader = dx9rend::deviceCapsPixelShaderMajor() < 2
                                    || dx9rend::Ra::effectManagerFlag40() != 0;
                if (!t->m_isVegetation && !transparent && dx9rend::rendererIsExpansionPack()
                    && !dx9rend::rendererPerformanceFlag(0) && !lowShader
                    && dx9rend::rendererGetVideoOptions()->getQuality(4) >= 3)
                {
                    // ds:1025A990 has no writer and is loader-zero.
                    const int limit = dx9rend::rendererPerformanceFlag(30) ? 4 : 5;
                    static ExpansionShadowBatch batch(limit);
                    staticMeshMaterialShader(material) = staticMeshFindShaderBits(bits & qualityMask);
                    if (mesh.m_dword26C && (bits & 1u))
                        expansionLightmapThroughSetter(mesh, entry);
                    if (t->m_byte2A5)
                    {
                        // PE 0x100FB980; the clamp argument is stored before the call.
                        if (distance < t->m_float2AC)
                        {
                            float fade = static_cast<float>(ExpansionReal(1.0f)
                                - (ExpansionReal(distance) - ExpansionReal(t->m_float2A8))
                                / (ExpansionReal(t->m_float2AC) - ExpansionReal(t->m_float2A8)));
                            // FCMOVNB followed by FCMOVB: unordered values select the lower bound.
                            fade = fade >= 1.0f ? 1.0f : fade;
                            fade = !(fade >= 0.0f) ? 0.0f : fade;
                            op.transparency.value = fade;
                            entry->shader = staticMeshMaterialShader(material);
                            entry->techniqueKey = dx9rend::raHashString(std::string("Glowing"));
                            dx9rend::renderQueueAddToSlot(31, entry);
                        }
                        continue;
                    }
                    if (cone)
                    {
                        // PE 0x100FB200.
                        entry->shader = staticMeshMaterialShader(material);
                        entry->techniqueKey = dx9rend::raHashString(std::string("LightCone"));
                        dx9rend::renderQueueAddToSlot(10, entry);
                        continue;
                    }
                    if (!mesh.m_byte354)
                    {
                        // PE 0x100FBA30.
                        entry->shader = staticMeshMaterialShader(material);
                        entry->techniqueKey = 0;
                        expansionEnqueue(entry, op.transparency.value < 1.0f, 0);
                        continue;
                    }
                    entry->shader = staticMeshMaterialShader(material);
                    entry->techniqueKey = kBeforeLight;
                    // ds:1025A968 has no writer and is loader-zero.
                    if (!dx9rend::rendererPerformanceFlag(17))
                        expansionEnqueue(entry, false, 0);
                    if (material->m_vegetationNonTiled)
                    {
                        unsigned int pointBits = 0xF0000400u;
                        if (mesh.m_byte355 && dx9rend::rendererGetVideoOptions()->getQuality(4))
                            pointBits = 0xF0000600u;
                        staticMeshMaterialShader(material) = staticMeshFindShaderBits(pointBits);
                    }
                    else
                    {
                        bits = mesh.materialShaderBits(material, true);
                        staticMeshMaterialShader(material) = staticMeshFindShaderBits(bits & qualityMask);
                    }
                    bool projected = false;
                    for (size_t k = 0; k < projectedLights.size(); ++k)
                    {
                        XpLight* const light = projectedLights[k];
                        batch.clear();
                        const std::vector<LightShadowRecord>& records = xpLightShadowRecords(light);
                        if (xpLightCategory(light) == 0
                            && (!g_LightManager->getDynamicShadowsEnabled() || records.empty()))
                        {
                            dx9rend::RenderQueueEntry* const out = dx9rend::renderEntryPoolAcquire();
                            expansionShadowArrays(out, 0, 0, 0, 0);
                            expansionProjectedEntry(mesh, material, qualityMask, light, out);
                        }
                        else
                        {
                            for (size_t j = 0; j < records.size(); ++j)
                            {
                                const LightShadowRecord& record = records[j];
                                if (worldBounds.outside(record.frustum)
                                    || !mesh.materialVisible(material, &record.frustum))
                                    continue;
                                Mat4 projection;
                                rendmath::constructIdentity(projection);    // 0x100FEC1E
                                expansionProduct(projection, record.matrices[1], record.matrices[2]);
                                expansionProduct(projection, projection, Mat4::getTexScaleBias());
                                const Vec4 position = {
                                    record.atlasScaleOffset[0], record.atlasScaleOffset[1],0,0
                                };
                                const Vec4 size = {
                                    record.atlasScaleOffset[2], record.atlasScaleOffset[3],0,0
                                };
                                const TerrainShadowRange empty = {};
                                batch.append(empty, projection, position, size);
                                if (batch.count >= batch.capacity)
                                {
                                    dx9rend::RenderQueueEntry* const out = dx9rend::renderEntryPoolAcquire();
                                    batch.apply(out);
                                    batch.clear();
                                    expansionProjectedEntry(mesh, material, qualityMask, light, out);
                                }
                                projected = true;
                            }
                            if (batch.count)
                            {
                                dx9rend::RenderQueueEntry* const out = dx9rend::renderEntryPoolAcquire();
                                batch.apply(out);
                                batch.clear();
                                expansionProjectedEntry(mesh, material, qualityMask, light, out);
                            }
                        }
                    }
                    // ds:1025A964 is 0 and ds:1023969C is 1; neither has a writer.
                    const unsigned int afterBits = mesh.materialShaderBits(material, false);
                    bits = afterBits;
                    dx9rend::Ra::RaShader* const after = staticMeshFindShaderBits(
                        (projected ? afterBits | 0x200u : afterBits) & qualityMask);
                    dx9rend::RenderQueueEntry* const out = dx9rend::renderEntryPoolAcquire();
                    if (mesh.m_dword26C && (bits & 1u))
                        expansionLightmapThroughSetter(mesh, out);
                    out->material = entry->material;
                    out->materialParameters = entry->materialParameters;
                    out->objectParameters = entry->objectParameters;
                    // PE 0x100FF680, including the otherwise-unwritten +0x38 word.
                    std::memcpy(out->lightArray.lights.value, entry->lightArray.lights.value,
                                sizeof(dx9rend::Ra::RaLightRecord));
                    out->shader = after;
                    out->techniqueKey = kAfterLight;
                    expansionEnqueue(out, false, 0);
                    continue;
                }
                if (!t->m_isVegetation)
                    staticMeshMaterialShader(material) = staticMeshFindShaderBits(bits & qualityMask);
                else
                {
                    if (material->m_vegetationNonTiled)
                    {
                        const float f = mesh.m_float25C;
                        const Vec3& color = g_LightManager->getTreeSunColor();
                        const ExpansionReal red = ExpansionReal(f) * ExpansionReal(color.x);
                        const float green = static_cast<float>(ExpansionReal(f) * ExpansionReal(color.y));
                        const float blue = static_cast<float>(ExpansionReal(f) * ExpansionReal(color.z));
                        const float second = mesh.m_float25C;
                        sun->color[2] = static_cast<float>(ExpansionReal(second) * ExpansionReal(blue));
                        sun->color[0] = static_cast<float>(red * ExpansionReal(second));
                        sun->color[1] = static_cast<float>(ExpansionReal(second) * ExpansionReal(green));
                        unsigned int treeBits = 0xF0000000u;
                        if (mesh.m_byte355 && dx9rend::rendererGetVideoOptions()->getQuality(4))
                            treeBits = 0xF0000200u;
                        staticMeshMaterialShader(material) = staticMeshFindShaderBits(treeBits & qualityMask);
                        const float ambientScale = mesh.m_float25C;
                        const Vec3& ambient = g_LightManager->getTreeAmbientColor();
                        const float scaled[] = {
                            static_cast<float>(ExpansionReal(ambientScale) * ExpansionReal(ambient.x)),
                            static_cast<float>(ExpansionReal(ambientScale) * ExpansionReal(ambient.y)),
                            static_cast<float>(ExpansionReal(ambientScale) * ExpansionReal(ambient.z))
                        };
                        std::memcpy(entry->lighting.overGrowthAmbient.value, staticMeshPackVec4(scaled,1.0f),
                                    sizeof(entry->lighting.overGrowthAmbient.value));
                        IBase* const found = classManager->getSingleton(renddx9abi::HostString("WindManager"));
                        op.windSpeed.value = static_cast<world::WindManager*>(static_cast<void*>(found))
                            ->getWindSpeed(*reinterpret_cast<const Vec3*>(&mesh.m_transform->m[12]));
                        std::memcpy(op.world.value, mesh.m_transform->m, sizeof(op.world.value));
                    }
                    else
                    {
                        unsigned int shadow = 0;
                        if (mesh.m_byte355 && dx9rend::rendererGetVideoOptions()->getQuality(4))
                            shadow = 0x200u;
                        staticMeshMaterialShader(material) = staticMeshFindShaderBits(
                            ((bits & 4u ? 0xF9000000u : 0xFA000000u) | shadow) & qualityMask);
                        if (dx9rend::rendererIsExpansionPack())
                        {
                            const float f = mesh.m_float25C;
                            const Vec3& color = g_LightManager->getTreeSkyColor();
                            const ExpansionReal red = ExpansionReal(f) * ExpansionReal(color.x);
                            const float green = static_cast<float>(ExpansionReal(f) * ExpansionReal(color.y));
                            const float blue = static_cast<float>(ExpansionReal(f) * ExpansionReal(color.z));
                            const float second = mesh.m_float25C;
                            sun->color[2] = static_cast<float>(ExpansionReal(second) * ExpansionReal(blue));
                            sun->color[0] = static_cast<float>(red * ExpansionReal(second));
                            sun->color[1] = static_cast<float>(ExpansionReal(second) * ExpansionReal(green));
                        }
                        else
                        {
                            const float f = mesh.m_float25C;
                            const ExpansionReal square = ExpansionReal(f) * ExpansionReal(f);
                            for (unsigned int c = 0; c < 3; ++c)
                                sun->color[c] = static_cast<float>(square * ExpansionReal(sun->color[c]));
                            sun->scaled24 = static_cast<float>(square * ExpansionReal(sun->scaled24));
                        }
                        const float ambientScale = mesh.m_float25C;
                        const Vec3& ambient = g_LightManager->getStaticSkyColor();
                        const float scaled[] = {
                            static_cast<float>(ExpansionReal(ambientScale) * ExpansionReal(ambient.x)),
                            static_cast<float>(ExpansionReal(ambientScale) * ExpansionReal(ambient.y)),
                            static_cast<float>(ExpansionReal(ambientScale) * ExpansionReal(ambient.z))
                        };
                        std::memcpy(entry->lighting.overGrowthAmbient.value, staticMeshPackVec4(scaled,1.0f),
                                    sizeof(entry->lighting.overGrowthAmbient.value));
                    }
                }
                // PE 0x100FB830.
                const bool enqueueTransparent = op.transparency.value < 1.0f;
                if (mesh.m_dword26C && (bits & 1u))
                    expansionLightmap(mesh, entry);
                entry->shader = staticMeshMaterialShader(material);
                entry->techniqueKey = 0;
                dx9rend::renderQueueEnqueue(entry, enqueueTransparent, false, false);
                if (!lights.empty())
                {
                    unsigned int pointBits;
                    if (material->m_vegetationNonTiled)
                    {
                        pointBits = 0xF0000000u;
                        if (dx9rend::rendererGetVideoOptions()->getQuality(3))
                            pointBits = 0xF0000400u;
                        if (mesh.m_byte355 && dx9rend::rendererGetVideoOptions()->getQuality(4))
                            pointBits |= 0x200u;
                    }
                    else
                    {
                        pointBits = g_shaderComboFlags[material->m_shaderMaterial->shaderCombo];
                        typedef unsigned int (__thiscall* FlagsFn)(const void*);
                        if (reinterpret_cast<FlagsFn>(mesh.m_vptr[0xC4 / 4])(&mesh) & 1u)
                            pointBits &= ~8u;
                        pointBits |= 0x400u;
                        if (material->m_alphaMode == 2)
                            pointBits |= 0x800u;
                        bits = pointBits;
                        pointBits &= qualityMask;
                    }
                    staticMeshMaterialShader(material) = staticMeshFindShaderBits(pointBits);
                    for (size_t k = 0; k < lights.size(); ++k)
                    {
                        // PE 0x100FB8F0; the bits stack word is unused.
                        dx9rend::RenderQueueEntry* const out = dx9rend::renderEntryPoolAcquire();
                        const bool blend = op.transparency.value < 1.0f;
                        out->material = material;
                        out->materialParameters = material->m_parameters;
                        out->objectParameters = &op;
                        out->shader = staticMeshMaterialShader(material);
                        submitSourceLightRecord(lights[k], mesh.m_matrix290, out, blend);
                    }
                }
            }
        }
    }

    // 8. the frame stamp
    mesh.m_dword20 = dx9rend::rendererFrameOrdinal();                              // +0x54
}


// Shadow-map and silhouette drawing helpers. Profiling-only brackets are omitted.

// materialVisible's two frustum sources and the renderer's cull counter, shared with the draw.
bool staticMeshCornersOutside(const void* material, const Mat4& world, const Frustum& frustum)
{
    StaticMeshBoundsCorners corners(
        static_cast<const StaticMeshMaterial*>(material)->m_bounds, world);   // +0x198
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
// effect.  The wrapper's +0x08 is the interface pointer. The only caller, drawShadowMap, has
// already dereferenced the effect in its parameter lookups, so it is non-null here.
void staticMeshEffectCommitChanges(dx9rend::Effect* effect)
{
    ID3DXEffect* const d3dx = static_cast<ID3DXEffect*>(dx9rend::effectD3dxHandle(effect));
    d3dx->CommitChanges();
}

dx9rend::ShaderParameter* staticMeshShadowParameter(dx9rend::Effect* effect, const char* name)
{
    return dx9rend::effectGetShaderParameterTyped(effect, std::string(name));
}

// dword_10239698 - the shadow draw's mode word, 2 in .data and never written by the image:
//     0  merge the lod's index ranges and draw the whole lod in one call
//     1  draw the last lod's first material only
//     2  draw the selected lod material by material, alpha-tested ones through pass 1
// [INFERRED] name; kept as the mutable word it is.
unsigned int g_staticMeshShadowDrawMode = 2;

} // anonymous namespace

// [CHOSEN, not recovered] Shared source entry for the PE 0x10077A30 matrix operation.
void geometryRendererMatrixProduct(Mat4& output, const Mat4& left, const Mat4& right)
{
    expansionProduct(output, left, right);
}

// PE 0x100FB250: unreachable in 1.50: no code or data references
// [INFERRED] name. True when the template has an alpha material, the material's alpha mode is 2 and
// the instance's current lod is 0 (`retn 4`: the material is the one argument).
bool staticMeshUsesAlphaPass(const StaticMesh& mesh, const StaticMeshMaterial* material)
{
    const StaticMeshTemplate* const t = static_cast<const StaticMeshTemplate*>(mesh.m_template);
    return t->m_hasAlphaMaterial && material->m_alphaMode == 2u && mesh.m_dword318 == 0;
}

// PE 0x100FC0E0: unreachable in 1.50: no code or data references
// [INFERRED] name. The combined (geom, lod) key into the lightmap cache: the template's skipped-lod
// count for the geom (the base vector at +0x70), plus the lod index, plus ten per geom. Retail asks
// the instance for its geom index twice and its lod index once through the virtual table.
int staticMeshGeomLodKey(StaticMesh& mesh)
{
    typedef int (__thiscall* IndexFn)(void*);
    const int geom = reinterpret_cast<IndexFn>(mesh.m_vptr[0x70 / 4])(&mesh);
    const int skipped = static_cast<const StaticMeshTemplate*>(mesh.m_template)->m_geomInts.data()[geom];
    const int geomAgain = reinterpret_cast<IndexFn>(mesh.m_vptr[0x70 / 4])(&mesh);
    const int lod = reinterpret_cast<IndexFn>(mesh.m_vptr[0x7C / 4])(&mesh);
    return lod + skipped + 2 * (5 * geomAgain);
}

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
    if (m_dword30C->hasAlphaMaterial)                                    // +0x54
        return;

    bool fromOwnCaster = false;
    const LightShadowRecord* casterRecord = static_cast<const LightShadowRecord*>(caster);
    const Mat4* lightMatrix;
    if (!casterRecord)
    {
        typedef void* (__thiscall* GetDword350Fn)(const void*);
        const void* const stamped =
            reinterpret_cast<GetDword350Fn>(m_vptr[0xB0 / 4])(this);
        // The pointer-typed slot carries a 32-bit frame ordinal.
        if (static_cast<unsigned int>(reinterpret_cast<uintptr_t>(stamped))
            != dx9rend::rendererFrameOrdinal())                                  // +0x54
            return;
        casterRecord  = reinterpret_cast<const LightShadowRecord*>(m_dword34C);
        lightMatrix   = &casterRecord->matrices[1];                           // +0x238
        fromOwnCaster = true;
    }
    else
    {
        lightMatrix = &casterRecord->matrices[0];                             // +0x1F8
    }

    const int shadowLod  = g_LightManager->getStaticMeshShadowLod();     // +0x280
    typedef int (__thiscall* GetIndexFn)(const void*);
    const int currentLod = reinterpret_cast<GetIndexFn>(m_vptr[0x7C / 4])(this);
    const int lodCount   = reinterpret_cast<GetIndexFn>(m_vptr[0x80 / 4])(this);
    if (currentLod != shadowLod)
    {
        int lod = lodCount - 1;
        if (lod >= shadowLod)
            lod = shadowLod;
        typedef void (__thiscall* SetLodFn)(void*, int);
        reinterpret_cast<SetLodFn>(m_vptr[0x78 / 4])(this, lod);
    }
    // setLod rewrites +0x30C; every read below is of the lod now selected (the shipped body
    // reloads the word at each use).
    StaticMeshLod* const lodRecord = m_dword30C;

    StaticMeshRenderer* const smr = staticMeshRenderer();
    dx9rend::Effect* const effect = smr->m_shadowEffect;                  // +0x44

    staticMeshShadowParameter(effect, "World")->setValue(m_transform, 0);
    {
        const float* const packed = staticMeshPackVec4(
            reinterpret_cast<const float*>(&t->m_decompressScale), 1.0f);    // template +0x138
        staticMeshShadowParameter(effect, "POSUNPACK")->setValue(packed, 0);
    }
    staticMeshShadowParameter(effect, "TEXUNPACK")->setValue(&t->m_uvDecompress, 0);  // +0x150

    if (casterRecord && casterRecord->tail494[1])                             // +0x495
    {
        Mat4 light;
        rendmath::constructIdentity(light);                                   // 0x100FD76F
        expansionProduct(light, casterRecord->matrices[1], casterRecord->matrices[2]);  // +0x238, +0x278 (0x100FD785)
        staticMeshShadowParameter(effect, "vpLightMat")->setValue(&light, 0);
        staticMeshShadowParameter(effect, "vpLightTrapezMat")->setValue(&light, 0);
    }
    else
    {
        staticMeshShadowParameter(effect, "vpLightMat")->setValue(lightMatrix, 0);
        staticMeshShadowParameter(effect, "vpLightTrapezMat")->setValue(&casterRecord->matrices[2], 0);
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
                StaticMeshMaterial* const material =
                    m_dword310->data() + i;
                const ShaderMaterial* const shaderMaterial = material->m_shaderMaterial;  // +0x28
                const unsigned int alphaMode = material->m_alphaMode;                    // +0x17C
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

                StaticMeshMaterial* const typed = material;
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
        const int lastLod = reinterpret_cast<GetIndexFn>(m_vptr[0x80 / 4])(this) - 1;
        const StaticMeshGeom& geom = t->m_geoms[m_dword314];
        const StaticMeshLod* const lastRecord = geom.lods.data() + lastLod;
        const StaticMeshMaterialList* const lodRecords = t->m_materials.data()[m_dword314].data();
        const StaticMeshMaterial* const material = lodRecords[lastLod].data();
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
        typedef void (__thiscall* SetLodFn)(void*, int);
        reinterpret_cast<SetLodFn>(m_vptr[0x78 / 4])(this, -1);
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
