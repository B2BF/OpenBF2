// Compiled road instances with per-chunk visibility and renderer submission.
// Distance arithmetic retains float association and unordered-comparison behavior.


#ifdef BF2_IMAGE_RENDDX9

#include "../../RendDX9Abi/ThunkAbi.h"
#include "RoadCompiled.h"
#include "RoadCompiledTemplate.h"
#include "QuadPatch.h"                    // TerrainCuller (Terrain::m_culler's type)
#include "Terrain.h"                      // terrain() - the "Terrain" singleton (dword_1024B930)

#include "../Math/Frustum.h"
#include "../RendMisc/IRenderView.h"
#include "../RendDX9/RendDX9Base.h"       // rendererGetRenderView, the ABI host stream typedefs
#include "../../RendDX9Abi/BfMemory.h"
#include "../../RendDX9Abi/Frontier.h"
#include "../../RendDX9Abi/HostFileAbi.h"
#include "../../RendDX9Abi/ModuleCommon.h"         // RD9_IID_IBASE
#include "../../RendDX9Abi/PluginAbi.h"
#include "../../RendDX9Abi/PluginEntry.h"
#include <string>

#include <cmath>
#include <cstdint>
#include <new>

namespace dice {
namespace hfe {

namespace dx9rend {

extern void*  g_bufferManager;   // dword_1023B978 - BufferManager.cpp
extern IBase* g_Renderer;        // dword_1023D098 - BF2Render.cpp (the Renderer; same object as
                                 // g_rendererBase, read through the global retail names)

// BufferManager.cpp / StateManager.cpp, declared per TU as every other client does.
void* createVertexBuffer(void* bufferManager, unsigned int count, unsigned int stride,
                         const std::string& name, unsigned int usage);
void* bufferManagerCreateIndexBuffer(unsigned int count, unsigned int usage,
                                     unsigned int format);
void* vertexBufferLock(void* wrapper, unsigned int offsetVerts, unsigned int countVerts,
                       unsigned int flags);
bool  vertexBufferUnlock(void* wrapper);
void* indexBufferLock(void* wrapper, unsigned int offsetIndices, unsigned int countIndices,
                      unsigned int flags);
bool  indexBufferUnlock(void* wrapper);
void  vertexBufferBindStream(void* wrapper, unsigned int stream);
void  indexBufferBind(void* wrapper);
void  vertexBufferRelease(void* wrapper);                                    // --ref, sub_10014510, free
void  indexBufferRelease(void* wrapper);                                     // --ref, sub_10014300, free
// Named after the D3D call, NOT the shipped push order - see StateManager.cpp.
void  rendererDrawIndexedPrimitive(unsigned int primCount, unsigned int type, int baseVertex,
                                   unsigned int minIndex, unsigned int numVertices,
                                   unsigned int startIndex);

} // namespace dx9rend

namespace geom {

namespace {

// dword_1021AE24 (820010, IRoad) and dword_1021AE58 (0x492FE0FE, IGeometry) - the two ids QI tests
// after IBase; both read as data.
const unsigned int kIID_IRoad     = 820010u;
const unsigned int kIID_IGeometry = 0x492FE0FEu;

// The 32-byte "RoadCompiled" vertex loadMesh writes: position (relative to the file's origin - the
// road compiler subtracts it), the two uv sets and the edge alpha.
struct RoadCompiledVertex
{
    float position[3];   // +0x00
    float uv0[2];        // +0x0C
    float uv1[2];        // +0x14
    float alpha;         // +0x1C
};

static_assert(sizeof(RoadCompiledVertex) == 32, "loadMesh creates the buffer with stride 0x20");

// Renderer +0x6A4, `drawRoads` - VideoOptions::apply's terrain-quality arm writes it (0 at quality 1,
// 1 at 2 and 3) and renderFrame's road block tests it. addToRenderQueue reads it through g_Renderer
// (`mov eax, g_Renderer; cmp byte ptr [eax+6A4h], 0`), unguarded. Read through BF2Render.cpp's
// render-switch accessor, keyed by that retail offset.
bool rendererDrawRoads()
{
    return dx9rend::rendererRenderSwitch(0x6A4) != 0;
}

RoadCompiled* fromGeometry(void* subobject)
{
    return reinterpret_cast<RoadCompiled*>(static_cast<unsigned char*>(subobject)
                                           - offsetof(RoadCompiled, m_geometry));   // retail +4
}


// THE IRoad table, seven slots. MSVC will not make a free function __thiscall; a
// __fastcall with an unused EDX is the same convention for these slots.

int RD9_THUNK rc_addRef(void* self RD9_THUNK_EDX)
{
    return static_cast<RoadCompiled*>(self)->addRef();
}

int RD9_THUNK rc_getRef(void* self RD9_THUNK_EDX)
{
    return static_cast<RoadCompiled*>(self)->getRef();
}

int RD9_THUNK rc_release(void* self RD9_THUNK_EDX)
{
    return static_cast<RoadCompiled*>(self)->release();
}

void* RD9_THUNK rc_queryInterface(void* self RD9_THUNK_EDX, unsigned int iid)
{
    return static_cast<RoadCompiled*>(self)->queryInterface(iid);
}

// +0x10, 0x1015AD30 - `ret`. [UNRESOLVED] role; nothing recovered calls it.
void RD9_THUNK rc_slot10(void* RD9_THUNK_EDX)
{
}

void RD9_THUNK rc_updateChunkVisibility(void* self RD9_THUNK_EDX, const Frustum* frustum)
{
    static_cast<RoadCompiled*>(self)->updateChunkVisibility(frustum);
}

bool RD9_THUNK rc_anyChunkVisible(void* self RD9_THUNK_EDX)
{
    return static_cast<RoadCompiled*>(self)->anyChunkVisible();
}

void* const kRoadVtable[7] =
{
    reinterpret_cast<void*>(&rc_addRef),                  // +0x00  (shared with StaticMesh)
    reinterpret_cast<void*>(&rc_getRef),                  // +0x04  (shared with StaticMesh)
    reinterpret_cast<void*>(&rc_release),                 // +0x08
    reinterpret_cast<void*>(&rc_queryInterface),          // +0x0C
    reinterpret_cast<void*>(&rc_slot10),                  // +0x10  0x1015AD30  `ret`
    reinterpret_cast<void*>(&rc_updateChunkVisibility),   // +0x14
    reinterpret_cast<void*>(&rc_anyChunkVisible)          // +0x18
};


// THE IGeometry table, 0x1021E8E8 - 62 slots, `this` = the subobject at +4. The four IBase entries
// are MSVC adjustor thunks (`sub ecx, 4; jmp <primary body>`); every other entry that is not this
// class's own body is one of the image-wide default bodies, each read: its `retn N` fixes the arity.

int RD9_THUNK rg_addRef(void* self RD9_THUNK_EDX)
{
    return fromGeometry(self)->addRef();
}

int RD9_THUNK rg_getRef(void* self RD9_THUNK_EDX)
{
    return fromGeometry(self)->getRef();
}

int RD9_THUNK rg_release(void* self RD9_THUNK_EDX)
{
    return fromGeometry(self)->release();
}

void* RD9_THUNK rg_queryInterface(void* self RD9_THUNK_EDX, unsigned int iid)
{
    return fromGeometry(self)->queryInterface(iid);
}

void RD9_THUNK rg_emptyNoArgs(void* RD9_THUNK_EDX) { }                                  // 0x10037D40 `ret`
void RD9_THUNK rg_emptyOneArg(void* RD9_THUNK_EDX, int) { }                             // 0x100E13B0 `retn 4`
void RD9_THUNK rg_emptyTwoArgs(void* RD9_THUNK_EDX, int, int) { }                       // 0x100E1480 `retn 8`
void RD9_THUNK rg_emptyThreeArgs(void* RD9_THUNK_EDX, int, int, int) { }                // 0x10083AB0 `retn 0Ch`
void RD9_THUNK rg_emptySixArgs(void* RD9_THUNK_EDX, int, int, int, int, int, int) { }   // 0x10082D20 `retn 18h`
// The two zero bodies also fill pointer slots (+0x28 the light record, +0xA4 the root object, +0xCC the
// next geometry), so the zero is pointer-sized: the same `xor eax, eax` on x86, all of RAX on x64.
std::uintptr_t RD9_THUNK rg_returnsZero(void* RD9_THUNK_EDX) { return 0; }
std::uintptr_t RD9_THUNK rg_returnsZeroOneArg(void* RD9_THUNK_EDX, int) { return 0; }   // 0x10088060 `retn 4`
bool RD9_THUNK rg_returnsFalse(void* RD9_THUNK_EDX) { return false; }
bool RD9_THUNK rg_returnsFalseOneArg(void* RD9_THUNK_EDX, int) { return false; }        // 0x1005DA00 `retn 4`
int  RD9_THUNK rg_returnsOne(void* RD9_THUNK_EDX) { return 1; }
int  RD9_THUNK rg_returnsMinusOne(void* RD9_THUNK_EDX) { return -1; }
int  RD9_THUNK rg_returnsSeven(void* RD9_THUNK_EDX) { return 7; }
float RD9_THUNK rg_returnsMinusOneF(void* RD9_THUNK_EDX) { return -1.0f; }              // 0x10082D10 `fld -1.0`

// +0x18, (`retn 0Ch`) - the ObjectDrawer's deferred pass passes (renderView, occluder, 1);
// the body reads none of the three.
void RD9_THUNK rg_addToRenderQueue(void* self RD9_THUNK_EDX, void*, void*, int)
{
    fromGeometry(self)->addToRenderQueue();
}

// +0x34, 0x1015BB00 - `sub ecx, 4; jmp 0x1015BB60`, the scalar deleting destructor.
void* RD9_THUNK rg_destroy(void* self RD9_THUNK_EDX, unsigned int flags)
{
    return fromGeometry(self)->destroy(flags);
}

// +0x38, the same body as IRoad +0x04 (`mov eax, [ecx+34h]`), which on the subobject
// reads object +0x38: the template. ICF folded "getRef" and "getTemplate" into one function.
void* RD9_THUNK rg_getTemplate(void* self RD9_THUNK_EDX)
{
    return fromGeometry(self)->m_template;
}

// +0x40, 0x1015AD50 - `lea eax, [ecx+94h]`: the local bounds (object +0x98).
const void* RD9_THUNK rg_getLocalBounds(void* self RD9_THUNK_EDX)
{
    return &fromGeometry(self)->m_localBounds;
}

// +0x44, 0x1015AD60 - `fld dword ptr [ecx+54h]`: the file's radius (object +0x58).
float RD9_THUNK rg_getRadius(void* self RD9_THUNK_EDX)
{
    return fromGeometry(self)->m_radius;
}

// +0x48, 0x1015AD70 - `lea eax, [ecx+0D0h]`: the transform (object +0xD4).
const Mat4* RD9_THUNK rg_getTransform(void* self RD9_THUNK_EDX)
{
    return &fromGeometry(self)->m_transform;
}

// +0x50, (`retn 4`).
void RD9_THUNK rg_setTransform(void* self RD9_THUNK_EDX, const Mat4* transform)
{
    fromGeometry(self)->setTransform(transform);
}

// +0x68, 0x10082D30 - `add ecx, 4; jmp std::string::operator=`: the subobject's own name string
// (object +0x08). The shared body StaticMesh's +0x68 is; it answers the string.
std::string* RD9_THUNK rg_setName(void* self RD9_THUNK_EDX, const std::string& value)
{
    std::string& name = static_cast<GeometryInstanceData*>(self)->m_name;
    name = value;
    return &name;
}

void* const kRoadGeometryVtable[62] =
{
    reinterpret_cast<void*>(&rg_addRef),             // +0x00  0x1015BB30  thunk -> 0x10103C00
    reinterpret_cast<void*>(&rg_getRef),             // +0x04  0x1015BB40  thunk -> 0x10103C10
    reinterpret_cast<void*>(&rg_release),            // +0x08  0x1015BB50  thunk -> 0x1015BB10
    reinterpret_cast<void*>(&rg_queryInterface),     // +0x0C  0x1015BAF0  thunk -> 0x1015BAB0
    reinterpret_cast<void*>(&rg_emptyOneArg),        // +0x10
    reinterpret_cast<void*>(&rg_returnsMinusOneF),   // +0x14  0x10082D10  -1.0f
    reinterpret_cast<void*>(&rg_addToRenderQueue),   // +0x18
    reinterpret_cast<void*>(&rg_emptyTwoArgs),       // +0x1C
    reinterpret_cast<void*>(&rg_emptySixArgs),       // +0x20
    reinterpret_cast<void*>(&rg_emptyOneArg),        // +0x24
    reinterpret_cast<void*>(&rg_returnsZero),        // +0x28
    reinterpret_cast<void*>(&rg_returnsZero),        // +0x2C
    reinterpret_cast<void*>(&rg_returnsFalseOneArg), // +0x30
    reinterpret_cast<void*>(&rg_destroy),            // +0x34  0x1015BB00  thunk -> 0x1015BB60
    reinterpret_cast<void*>(&rg_getTemplate),        // +0x38  (object +0x38)
    reinterpret_cast<void*>(&rg_returnsZeroOneArg),  // +0x3C
    reinterpret_cast<void*>(&rg_getLocalBounds),     // +0x40
    reinterpret_cast<void*>(&rg_getRadius),          // +0x44
    reinterpret_cast<void*>(&rg_getTransform),       // +0x48
    reinterpret_cast<void*>(&rg_emptyTwoArgs),       // +0x4C
    reinterpret_cast<void*>(&rg_setTransform),       // +0x50
    reinterpret_cast<void*>(&rg_emptyTwoArgs),       // +0x54
    reinterpret_cast<void*>(&rg_emptyOneArg),        // +0x58
    reinterpret_cast<void*>(&rg_returnsZero),        // +0x5C
    reinterpret_cast<void*>(&rg_emptyTwoArgs),       // +0x60
    reinterpret_cast<void*>(&rg_emptyOneArg),        // +0x64
    reinterpret_cast<void*>(&rg_setName),            // +0x68
    reinterpret_cast<void*>(&rg_emptyOneArg),        // +0x6C
    reinterpret_cast<void*>(&rg_returnsZero),        // +0x70
    reinterpret_cast<void*>(&rg_returnsOne),         // +0x74
    reinterpret_cast<void*>(&rg_emptyOneArg),        // +0x78
    reinterpret_cast<void*>(&rg_returnsZero),        // +0x7C
    reinterpret_cast<void*>(&rg_returnsOne),         // +0x80
    reinterpret_cast<void*>(&rg_emptyOneArg),        // +0x84
    reinterpret_cast<void*>(&rg_returnsMinusOne),    // +0x88
    reinterpret_cast<void*>(&rg_returnsZero),        // +0x8C
    reinterpret_cast<void*>(&rg_emptyThreeArgs),     // +0x90
    reinterpret_cast<void*>(&rg_emptyTwoArgs),       // +0x94
    reinterpret_cast<void*>(&rg_emptyTwoArgs),       // +0x98
    reinterpret_cast<void*>(&rg_emptyOneArg),        // +0x9C  (setOwner: nothing kept)
    reinterpret_cast<void*>(&rg_returnsZero),        // +0xA0
    reinterpret_cast<void*>(&rg_returnsZero),        // +0xA4  (root object: null)
    reinterpret_cast<void*>(&rg_returnsFalse),       // +0xA8
    reinterpret_cast<void*>(&rg_emptyOneArg),        // +0xAC
    reinterpret_cast<void*>(&rg_returnsZero),        // +0xB0
    reinterpret_cast<void*>(&rg_emptyOneArg),        // +0xB4
    reinterpret_cast<void*>(&rg_emptyOneArg),        // +0xB8
    reinterpret_cast<void*>(&rg_returnsFalse),       // +0xBC
    reinterpret_cast<void*>(&rg_emptyTwoArgs),       // +0xC0
    reinterpret_cast<void*>(&rg_returnsZero),        // +0xC4
    reinterpret_cast<void*>(&rg_returnsZeroOneArg),  // +0xC8
    reinterpret_cast<void*>(&rg_returnsZero),        // +0xCC  (next geometry: none)
    reinterpret_cast<void*>(&rg_returnsFalse),       // +0xD0
    reinterpret_cast<void*>(&rg_emptyOneArg),        // +0xD4
    reinterpret_cast<void*>(&rg_returnsFalse),       // +0xD8
    reinterpret_cast<void*>(&rg_emptyOneArg),        // +0xDC
    reinterpret_cast<void*>(&rg_emptyOneArg),        // +0xE0
    reinterpret_cast<void*>(&rg_returnsSeven),       // +0xE4
    reinterpret_cast<void*>(&rg_returnsZero),        // +0xE8
    reinterpret_cast<void*>(&rg_returnsZeroOneArg),  // +0xEC
    reinterpret_cast<void*>(&rg_emptyNoArgs),        // +0xF0
    reinterpret_cast<void*>(&rg_emptyOneArg)         // +0xF4  (setOpacity: nothing kept)
};

// The two helpers setTransform and loadMesh share: the local bounds are the world bounds minus the
// transform's translation row (+0x104..+0x10C), with the dirty byte raised. Both shipped copies
// compute min.x, min.y, min.z, max.x, max.y, max.z in that order and store min.z first; only the
// finished values are observable.
void refreshLocalBounds(RoadCompiled& road)
{
    const float* const translation = &road.m_transform.m[12];
    const float minX = road.m_worldBounds.min.x - translation[0];
    const float minY = road.m_worldBounds.min.y - translation[1];
    const float minZ = road.m_worldBounds.min.z - translation[2];
    const float maxX = road.m_worldBounds.max.x - translation[0];
    const float maxY = road.m_worldBounds.max.y - translation[1];
    const float maxZ = road.m_worldBounds.max.z - translation[2];
    road.m_localBounds.min.z = minZ;
    road.m_localBounds.min.x = minX;
    road.m_localBounds.min.y = minY;
    road.m_localBounds.max.z = maxZ;
    road.m_localBounds.max.x = maxX;
    road.m_localBounds.max.y = maxY;
    road.m_localBounds.dirty = 1;
}

void clearBounds(MeshBounds& bounds)
{
    bounds.min.x = 0.0f; bounds.min.y = 0.0f; bounds.min.z = 0.0f;
    bounds.max.x = 0.0f; bounds.max.y = 0.0f; bounds.max.z = 0.0f;
    bounds.dirty = 1;
    bounds.originRadius = 0.0f;
    bounds.radius = 0.0f;
    bounds.extent.x = 0.0f; bounds.extent.y = 0.0f; bounds.extent.z = 0.0f;
    bounds.center.x = 0.0f; bounds.center.y = 0.0f; bounds.center.z = 0.0f;
}

void setIdentity(Mat4& matrix)
{
    for (int i = 0; i < 16; ++i)
        matrix.m[i] = 0.0f;
    matrix.m[0]  = 1.0f;
    matrix.m[5]  = 1.0f;
    matrix.m[10] = 1.0f;
    matrix.m[15] = 1.0f;
}

} // anonymous namespace


// Initializes geometry state and owned containers. loadMesh must run before drawing:
// index/chunk counts, visibility, version words, and allocator words remain uninitialized.

RoadCompiled::RoadCompiled(RoadCompiledTemplate* owner)
    : m_roadVptr(kRoadVtable)
    , m_geometry(kRoadGeometryVtable)
    , m_refCount(1)
    , m_template(owner)
    , m_vertexCount(0)
    , m_vertexBuffer(0)
    , m_indexBuffer(0)
    , m_radius(0.0f)
    , m_order(0)
{
    m_origin.x = 0.0f;
    m_origin.y = 0.0f;
    m_origin.z = 0.0f;
    clearBounds(m_worldBounds);
    clearBounds(m_localBounds);
    setIdentity(m_transform);
}

// the vertex buffer and the index buffer released owner-side (each only when present),
// then the members in reverse order: the +0x170 string, the visible bits, the centres, radii, index
// counts and starts, and finally the IGeometry prefix's name string.
RoadCompiled::~RoadCompiled()
{
    if (m_vertexBuffer)
        dx9rend::vertexBufferRelease(m_vertexBuffer);
    if (m_indexBuffer)
        dx9rend::indexBufferRelease(m_indexBuffer);
}


// IRoad.


// 0x10103C00 - `add [ecx+34h], 1; mov eax, [ecx+34h]`: it answers the new count.
int RoadCompiled::addRef()
{
    return ++m_refCount;
}

int RoadCompiled::getRef() const
{
    return m_refCount;
}

// at zero the object is destroyed through ITS OWN IGeometry table (`mov eax,[ecx+4];
// add ecx, 4; push 1; call [eax+34h]`), not by a direct call.
int RoadCompiled::release()
{
    const int remaining = --m_refCount;
    if (!remaining)
    {
        typedef void* (__thiscall* DestroyFn)(void*, unsigned int);
        reinterpret_cast<DestroyFn>(m_geometry.m_vptr[0x34 / 4])(&m_geometry, 1);
    }
    return remaining;
}

// IBase and IRoad (820010) answer with the object; IGeometry (0x492FE0FE) with the +4
// subobject. The shipped body tests `this` before adjusting - the static_cast's null guard, which a
// member function cannot observe.
void* RoadCompiled::queryInterface(unsigned int iid)
{
    if (iid == RD9_IID_IBASE || iid == kIID_IRoad)
        return this;
    if (iid == kIID_IGeometry)
        return &m_geometry;
    return 0;
}

// the scalar deleting destructor. It answers `this` (the object, not the subobject).
void* RoadCompiled::destroy(unsigned int flags)
{
    this->~RoadCompiled();
    if (flags & 1)
        ::renddx9abi::engineFree(this);
    return this;
}


// Updates 1.4+ chunk visibility by distance then frustum; older meshes are always visible.
// Requires terrain and compiled-road renderer singletons. Unordered distances are rejected.

void RoadCompiled::updateChunkVisibility(const Frustum* frustum)
{
#if defined(BF2_IMAGE_EDITOR) && BF2_IMAGE_EDITOR
    // [CHOSEN, not recovered] editor-only region (agents.md, Editor): over the editor's TerrainEditable,
    // which never builds a culler (Geom/TerrainEditable.cpp, region 2), no compiled road is visible - the walk
    // below reads the culler's view distance unguarded, as shipped. The retail editor drew road editables there,
    // not compiled roads.
    if (!terrain()->m_culler)
    {
        m_anyChunkVisible = 0;
        return;
    }
#endif
    if (m_versionMajor >= 1 && m_versionMinor >= 4)
    {
        const float maxDistance = static_cast<float>(terrain()->m_culler->param18)
                                * roadCompiledRendererViewDistanceScale();
        m_anyChunkVisible = 0;
        for (unsigned int i = 0; i < m_chunkCount; ++i)
        {
            const Vec3& centre = m_chunkCentres[i];
            const float dx = frustum->m_transformation.m[12] - centre.x;
            const float dy = frustum->m_transformation.m[13] - centre.y;
            const float dz = frustum->m_transformation.m[14] - centre.z;
            const float* const radius = &m_chunkRadii[i];
            const float distance = std::sqrt((dz * dz + dy * dy) + dx * dx) - *radius;
            if (distance < maxDistance)
            {
                m_chunkVisible[i] = frustum->inside(centre, *radius);
                m_anyChunkVisible |= m_chunkVisible[i] ? 1 : 0;
            }
            else
            {
                m_chunkVisible[i] = false;
            }
        }
    }
    else
    {
        m_anyChunkVisible = 1;
    }
}


// IGeometry +0x18, addToRenderQueue. Only while the renderer draws roads: the chunk pass
// against the renderer's view frustum (g_rendererBase vslot +0x48, then the view's +0x74) - a virtual
// call through this object's own IRoad table - and, when any chunk survived, the queue insert on the
// RoadCompiledRenderer singleton (unguarded).

void RoadCompiled::addToRenderQueue()
{
    if (!rendererDrawRoads())
        return;

    const Frustum* const frustum = dx9rend::rendererGetRenderView()->getFrustumPtr();
    typedef void (__thiscall* VisibilityFn)(void*, const Frustum*);
    reinterpret_cast<VisibilityFn>(m_roadVptr[0x14 / 4])(this, frustum);

    if (m_anyChunkVisible)
        roadCompiledRendererAdd(this);
}

// IGeometry +0x50, the transform copied in (sub_10030E80), then the local bounds
// re-derived from it.
void RoadCompiled::setTransform(const Mat4* transform)
{
    m_transform = *transform;
    refreshLocalBounds(*this);
}


// Loads versioned road vertices, indices and chunks; stream reads are unchecked.
// Missing files trap. Failed buffer locks skip reads and desynchronize the stream.
// Version 1.4 requires a chunk table even though the editable-road writer omits it.

void RoadCompiled::loadMesh(const std::string& file)
{
    renddx9abi::HostStream* const stream = ::dice::hfe::fileManager->open(
        renddx9abi::makeHostString(file), static_cast<renddx9abi::HostFileMode>(2),
        static_cast<renddx9abi::HostFileAccess>(0), 0);                      // FileManager +0x18
    if (!stream)
    {
        __debugbreak();                                                      // retail INT3
        return;
    }

    stream->read(&m_versionMinor, 4);                                        // +0x18C and +0x18E
    if (m_versionMajor <= 1 && m_versionMinor <= 2)
    {
        // sub_10074300(6, __FILE__, 140, "") << ... ; sub_10074690 - dice's debug stream, rendered
        // as one log line with dice's file, line and text, as every recovered diagnostic here is.
        ::renddx9abi::logf("RoadCompiled.cpp:140: TBM: Old compiled road version on %s. "
                           "Needs to be recompiled", file.c_str());
    }

    stream->read(&m_origin, 12);                                             // +0x4C
    stream->read(&m_radius, 4);                                              // +0x58
    stream->read(&m_worldBounds.min, 12);                                    // +0x5C
    stream->read(&m_worldBounds.max, 12);                                    // +0x68
    refreshLocalBounds(*this);
    stream->read(&m_order, 4);                                               // +0x190
    stream->read(&m_vertexCount, 4);                                         // +0x3C

    m_vertexBuffer = dx9rend::createVertexBuffer(dx9rend::g_bufferManager, m_vertexCount, 32,
                                                 std::string("RoadCompiled"), 8);   // writeonly
    RoadCompiledVertex* const vertices = static_cast<RoadCompiledVertex*>(
        dx9rend::vertexBufferLock(m_vertexBuffer, 0, 0, 0));
    if (vertices)
    {
        RoadCompiledVertex* out = vertices;
        for (int i = 0; i < static_cast<int>(m_vertexCount); ++i, ++out)    // `jge` - signed
        {
            float position[3] = { 0.0f, 0.0f, 0.0f };
            float uv0[2]      = { 0.0f, 0.0f };
            float uv1[2]      = { 0.0f, 0.0f };
            float skipped12[3] = { 0.0f, 0.0f, 0.0f };
            float skipped16[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
            float alpha;

            stream->read(position, 12);
            if (m_versionMajor == 1 && m_versionMinor == 2)
                stream->read(skipped12, 12);
            stream->read(uv0, 8);
            stream->read(uv1, 8);
            if (m_versionMajor == 1 && m_versionMinor == 2)
                stream->read(skipped16, 16);
            if (m_versionMajor >= 1 && m_versionMinor >= 3)
                stream->read(&alpha, 4);
            else
                alpha = 1.0f;

            out->position[0] = position[0];
            out->position[1] = position[1];
            out->position[2] = position[2];
            out->uv0[0]      = uv0[0];
            out->uv0[1]      = uv0[1];
            out->uv1[0]      = uv1[0];
            out->uv1[1]      = uv1[1];
            out->alpha       = alpha;
        }
    }
    dx9rend::vertexBufferUnlock(m_vertexBuffer);

    stream->read(&m_indexCount, 4);                                          // +0x40
    if (m_indexCount)
    {
        m_indexBuffer = dx9rend::bufferManagerCreateIndexBuffer(
            static_cast<unsigned int>(m_indexCount), 8, 101);                // writeonly, INDEX16
        void* const indices = dx9rend::indexBufferLock(m_indexBuffer, 0, 0, 0);
        if (indices)
            stream->read(indices, 2u * static_cast<unsigned int>(m_indexCount));
        dx9rend::indexBufferUnlock(m_indexBuffer);
    }

    if (m_versionMajor >= 1 && m_versionMinor >= 4)
    {
        stream->read(&m_chunkCount, 4);                                      // +0x114
        for (unsigned int i = 0; i < m_chunkCount; ++i)
        {
            unsigned int start      = 0;     // the reader reuses its argument slot for this
            unsigned int indexCount = 0;
            float        radius     = 0.0f;
            Vec3         centre     = { 0.0f, 0.0f, 0.0f };
            stream->read(&start, 4);
            stream->read(&indexCount, 4);
            stream->read(&radius, 4);
            stream->read(&centre, 12);
            m_chunkStarts.push_back(start);                                  // sub_100AE870 on +0x118
            m_chunkIndexCounts.push_back(indexCount);                        // sub_100AE870 on +0x128
            m_chunkRadii.push_back(radius);                                  // sub_1004E420 on +0x138
            m_chunkCentres.push_back(centre);                                // sub_10095BB0 on +0x148
            m_chunkVisible.push_back(false);                                 // sub_1013AD50 on +0x158
        }
        m_anyChunkVisible = 0;
    }

    stream->release();                                                       // +0x08
}


// Draws visible chunk runs with the effect pass already active; older meshes draw whole.
// Binds vertices before checking indices. Chunk primitive counts use unsigned division;
// the pre-1.4 whole-buffer count uses signed division.

void RoadCompiled::draw()
{
    if (!m_vertexCount)
        return;

    dx9rend::vertexBufferBindStream(m_vertexBuffer, 0);
    if (!m_indexBuffer)
    {
        ::renddx9abi::logf("RoadCompiled.cpp:291: TBM: the new roads need a idx Buffer!");
        return;
    }
    dx9rend::indexBufferBind(m_indexBuffer);

    if (m_versionMajor >= 1 && m_versionMinor >= 4)
    {
        unsigned int start = 0;          // var_8 - only ever read after a visible chunk set it
        unsigned int count = 0;          // ebx
        unsigned int i     = 0;          // esi
        if (m_chunkCount)
        {
            do
            {
                if (m_chunkVisible[i])
                {
                    start = m_chunkStarts[i];
                    count = m_chunkIndexCounts[i];
                    ++i;
                    while (i < m_chunkCount)
                    {
                        if (!m_chunkVisible[i])
                        {
                            dx9rend::rendererDrawIndexedPrimitive(count / 3u, 4u, 0, 0u,
                                                                  m_vertexCount, start);
                            count = 0;
                            break;
                        }
                        count += m_chunkIndexCounts[i];
                        ++i;
                    }
                }
                ++i;
            }
            while (i < m_chunkCount);

            if (count)
                dx9rend::rendererDrawIndexedPrimitive(count / 3u, 4u, 0, 0u, m_vertexCount, start);
        }
    }
    else
    {
        dx9rend::rendererDrawIndexedPrimitive(static_cast<unsigned int>(m_indexCount / 3), 4u, 0, 0u,
                                              m_vertexCount, 0u);
    }
}

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
