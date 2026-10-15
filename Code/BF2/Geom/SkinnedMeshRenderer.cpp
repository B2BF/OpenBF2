// Skinned-mesh render-path state. Registration and queryInterface both use IID_IBase.
// The primary and secondary vtables, with the listener at +0x04, are ABI boundaries.

#ifdef BF2_IMAGE_RENDDX9

#include "../../RendDX9Abi/ThunkAbi.h"
#include "../../RendDX9Abi/HfeAbi.h"
#include "../../RendDX9Abi/ModuleCommon.h"
#include "../../RendDX9Abi/BfMemory.h"
#include "../../RendDX9Abi/Frontier.h"
#include <string>
#include "SkinnedMesh.h"                       // the submission seams this file defines

#include <map>
#include <new>
#include <string.h>

namespace dice {
namespace hfe {
namespace geom {

extern IBase* g_SkinnedMeshRenderer;   // ModuleGeom.cpp, dword_1024B8F8

// getClassList: sub_100518A0(821015, "dice.hfe.geom.SkinnedMeshRenderer", factory). getSingletonList
// { "SkinnedMeshRenderer", &g_SkinnedMeshRenderer, iid 1, classId 821015, flags 2 }.
extern const unsigned int CID_SkinnedMeshRenderer  = 821015u;
extern const unsigned int IID_ISkinnedMeshRenderer = 1u;        // IID_IBase - the record really uses 1

// The retail object is 0x44 bytes (factory alloc, 0x100B06D0); the native one holds std
// containers and is allocated at sizeof.

// A distance-ordered submission queue: camera distance -> mesh, equal distances after the
// existing ones (multimap insert at the upper bound). The shipped nodes are 0x18 bytes.
typedef std::multimap<float, SkinnedMesh*> SkinnedMeshQueue;

// [CHOSEN] header over the 68-byte object; every offset is named because the class is fully
// recovered. The `+0xNN` comments are the retail offsets.
struct SkinnedMeshRenderer
{
    const void**  m_vptr;          // +0x00  primary vtable (ABI)
    const void**  m_listenerVptr;  // +0x04  RenderPathListener subobject vtable (ABI)
    void*         m_renderPath;    // +0x08  set/read by the listener slots
    int           m_refCount;      // +0x0C
    SkinnedMeshQueue m_map1;       // +0x10  the draw's sub_10123A50 queue
    SkinnedMeshQueue m_map2;       // +0x1C  [UNRESOLVED] use; the same 0x18-byte node
    SkinnedMeshQueue m_map3;       // +0x28  the draw's sub_10123A90 queue
    unsigned char m_bool;          // +0x34  primary slots 4/5
    unsigned char m_pad35[3];
    int           m_dword38;       // +0x38
    int           m_dword3C;       // +0x3C  [UNRESOLVED] - ctor does not write it
    int           m_dword40;       // +0x40
};

namespace {

// primary vtable (0x1021C928, 6 slots) - refcount at +0x0C
int RD9_THUNK skm_addRef(void* self RD9_THUNK_EDX) { return ++static_cast<SkinnedMeshRenderer*>(self)->m_refCount; }
int RD9_THUNK skm_getRc (void* self RD9_THUNK_EDX) { return   static_cast<SkinnedMeshRenderer*>(self)->m_refCount; }
int RD9_THUNK skm_release(void* self RD9_THUNK_EDX);
// queryInterface (0x10025930): iid 1 only.
void* RD9_THUNK skm_qi(void* self RD9_THUNK_EDX, unsigned int iid) { return (iid == RD9_IID_IBASE) ? self : 0; }
// +0x10 setBool([+0x34]), +0x14 getBool.
void RD9_THUNK skm_setBool(void* self RD9_THUNK_EDX, int v) { static_cast<SkinnedMeshRenderer*>(self)->m_bool = static_cast<unsigned char>(v); }
int  RD9_THUNK skm_getBool(void* self RD9_THUNK_EDX)        { return static_cast<SkinnedMeshRenderer*>(self)->m_bool; }

// secondary vtable (0x1021C910, RenderPathListener, 6 slots). `self` is the subobject pointer at
// object+0x04, so the object is `self - 4` and the render-path back-pointer is object+0x08.
inline SkinnedMeshRenderer* objOfListener(void* self)
{
    return reinterpret_cast<SkinnedMeshRenderer*>(
        reinterpret_cast<char*>(self) - offsetof(SkinnedMeshRenderer, m_listenerVptr));
}
void  RD9_THUNK skm_setRenderPath(void* self RD9_THUNK_EDX, void* path) { objOfListener(self)->m_renderPath = path; }
void* RD9_THUNK skm_getRenderPath(void* self RD9_THUNK_EDX)             { return objOfListener(self)->m_renderPath; }
void  RD9_THUNK skm_lisNull1(void* RD9_THUNK_EDX)      { }   // +0x04 nullsub_15
void  RD9_THUNK skm_lisNull2(void* RD9_THUNK_EDX, int) { }   // +0x08 nullsub_53
// +0x0C: the shipped "This path is invalid" error emitter (SkinnedMeshRenderer.cpp:29). Reproduced as
// a one-line diagnostic rather than the full dice error machinery.
void RD9_THUNK skm_lisPathInvalid(void* RD9_THUNK_EDX, void*)
{
    ::renddx9abi::logf("SkinnedMeshRenderer.cpp:29: This path is invalid");
}
// +0x14 getRenderPathListenerName -> returns "SkinnedMeshRenderer" by value (hidden out-pointer).
void* RD9_THUNK skm_lisGetName(void* RD9_THUNK_EDX, void* out)
{
    new (out) std::string("SkinnedMeshRenderer");
    return out;
}

const void* const kPrimaryVtable[6] =
{
    reinterpret_cast<void*>(&skm_addRef),         // +0x00
    reinterpret_cast<void*>(&skm_getRc),          // +0x04
    reinterpret_cast<void*>(&skm_release),        // +0x08
    reinterpret_cast<void*>(&skm_qi),             // +0x0C
    reinterpret_cast<void*>(&skm_setBool),        // +0x10
    reinterpret_cast<void*>(&skm_getBool),        // +0x14
};

const void* const kListenerVtable[6] =
{
    reinterpret_cast<void*>(&skm_setRenderPath),  // +0x00
    reinterpret_cast<void*>(&skm_lisNull1),       // +0x04  nullsub
    reinterpret_cast<void*>(&skm_lisNull2),       // +0x08  nullsub
    reinterpret_cast<void*>(&skm_lisPathInvalid), // +0x0C  "This path is invalid"
    reinterpret_cast<void*>(&skm_getRenderPath),  // +0x10  getRenderPath (inherited shape)
    reinterpret_cast<void*>(&skm_lisGetName),     // +0x14  getName
};

int RD9_THUNK skm_release(void* self RD9_THUNK_EDX)
{
    SkinnedMeshRenderer* p = static_cast<SkinnedMeshRenderer*>(self);
    const int remaining = --p->m_refCount;
    if (remaining == 0)
    {
        // The shipped dtor's teardown: the three queues (m_map3, m_map2, m_map1 - reverse member
        // order), then the object.
        p->~SkinnedMeshRenderer();
        ::renddx9abi::engineFree(self);
    }
    return remaining;
}

} // anonymous namespace

// geom_SkinnedMeshRenderer_ctor, 0x10123B80. Faithful except the render-path registration.
// [FRONTIER] dice::hfe::dx9rend::RendDX9_registerRenderPathListener - the listener-list layout is
// unread (same frontier RoadCompiledRenderer defers). The object is fully built; it is simply not
// yet spliced into the render-path list, so its listener slots are not driven by that list.
namespace {
void constructSkinnedMeshRenderer(SkinnedMeshRenderer* self)
{
    self->m_vptr         = const_cast<const void**>(kPrimaryVtable);
    self->m_listenerVptr = const_cast<const void**>(kListenerVtable);
    self->m_renderPath   = 0;
    self->m_refCount     = 1;
    // The three queues (sub_10123C60, 0x18-byte head nodes) are constructed with the object.
    self->m_bool = 1;
    self->m_pad35[0] = 0; self->m_pad35[1] = 0; self->m_pad35[2] = 0;
    self->m_dword38 = 0;
    self->m_dword3C = 0;
    self->m_dword40 = 0;
}
} // anonymous namespace

// RefFactory_geom_SkinnedMeshRenderer, 0x100B06D0.
IBase* __fastcall createSkinnedMeshRenderer(unsigned int iid, IBase* outer)
{
    if (outer)
        return 0;
    void* storage = ::renddx9abi::engineAlloc(sizeof(SkinnedMeshRenderer));
    if (!storage)
        return 0;
    // Value-initialised: the zero fill the factory gave the raw object, plus the three queues.
    SkinnedMeshRenderer* self = ::new (storage) SkinnedMeshRenderer();
    constructSkinnedMeshRenderer(self);

    void* itf = skm_qi(self RD9_THUNK_ARG, iid);
    if (!itf)
        skm_release(self RD9_THUNK_ARG);
    return static_cast<IBase*>(itf);
}

// sub_10123A40, `mov eax, [ecx+40h]; retn`, on the published renderer. [INFERRED] name.
int skinnedMeshRendererMode()
{
    return reinterpret_cast<const SkinnedMeshRenderer*>(g_SkinnedMeshRenderer)->m_dword40;   // +0x40
}

// sub_10123A50 / sub_10123A90 - insert the mesh by its camera distance into the renderer's +0x10 /
// +0x28 queue.
void skinnedMeshRendererSubmitA(SkinnedMesh* mesh)
{
    if (!g_SkinnedMeshRenderer || !mesh)
        return;
    reinterpret_cast<SkinnedMeshRenderer*>(g_SkinnedMeshRenderer)->m_map1.insert(
        SkinnedMeshQueue::value_type(mesh->m_distanceToCamera, mesh));
}

void skinnedMeshRendererSubmitB(SkinnedMesh* mesh)
{
    if (!g_SkinnedMeshRenderer || !mesh)
        return;
    reinterpret_cast<SkinnedMeshRenderer*>(g_SkinnedMeshRenderer)->m_map3.insert(
        SkinnedMeshQueue::value_type(mesh->m_distanceToCamera, mesh));
}

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
