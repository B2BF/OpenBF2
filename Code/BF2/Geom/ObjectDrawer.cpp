// Path:     attested - "C:\dice\Projects\BF2Branches\Patch_1_50\Code\BF2\Geom\ObjectDrawer.cpp",
//           read as a literal at RendDX9 0x1021AF98.
// Contents: recovered from RendDX9.dll (PE, symbols stripped).
// Occlusion evidence and validation: docs/recovery-notes/Geom/object-occlusion.md.

#ifdef BF2_IMAGE_RENDDX9

#include "../../RendDX9Abi/ThunkAbi.h"
#include "ObjectDrawer.h"
#include "MeshTemplate.h"                     // meshBoundsFinalize (sub_101C5EB0) - slot +0x3C
#include "StaticMesh.h"                       // StaticMeshOccluderRecord - slot +0xEC's records
#include "StaticMeshTemplate.h"               // StaticMeshOccluderEdge

#include "../../RendDX9Abi/BfMemory.h"
#include "../../RendDX9Abi/Frontier.h"
#include "../Core/EventManager.h"
#include "../Core/Ids.h"
#include "../Game/Objects/IObject.h"
#include "../Game/Objects/ObjectLayoutAccess.h"
#include "../Math/LocalBoundingBox.h"
#include "../Math/Mat4.h"
#include "../Math/Plane3.h"
#include "../Math/Vec3.h"
#include "../RendMisc/IRenderView.h"
#include "../Scene/Object/ObjectPredicator.h"

#include <algorithm>
#include <math.h>

#include <cstddef>
#include <cstdio>
#include <cstring>
#include <new>

namespace dice {
namespace hfe {

// PluginEntry.cpp - the host ObjectManager singleton (dword_10244AD8).  Declared here rather
// than pulled through a header, exactly as BF2Render.cpp declares the same four singletons.
extern IBase* objectManager;

namespace geom {

namespace {


// Explicit primary and event-handler tables preserve the 16-slot and one-slot interfaces
// while the remaining method signatures are unknown.


#define OD_FRONTIER(fn, slotOffset, addr, what)                                       \
    void RD9_THUNK fn(void* self RD9_THUNK_EDX)                                             \
    {                                                                                 \
        RD9_FRONTIER_AT("dice::hfe::geom::ObjectDrawer::slot_" #slotOffset             \
                        " (" #addr " - " what ")", self, slotOffset);                  \
    }

OD_FRONTIER(od_slot1C, 0x1C, 0x100F8DE0, "324 B, unread")
// +0x3C, the box occlusion query; the body is ObjectDrawer::isBoundsOccluded below.
// `retn 4`, the answer in AL, like +0x38.
bool RD9_THUNK od_slot3C(void* self RD9_THUNK_EDX, void* bounds);
#undef OD_FRONTIER

// +0x38, the occlusion query; the body is ObjectDrawer::isGeometryOccluded below.
// `retn 4`, the answer in AL: __fastcall with the dummy edx gives the same shape.
bool RD9_THUNK od_slot38(void* self RD9_THUNK_EDX, void* geometry);


#if defined(_M_IX86)
static_assert(offsetof(ObjectDrawer, m_eventVptr) == 0x04, "the ECCore handler subobject at +0x04");
#endif

// Handles the drawer's event subobject at +0x04 and updates the outer frame gate.
// [UNRESOLVED] Event data at +0x2C must point to a word compared with 4; neither pointer is guarded.

char RD9_THUNK od_event00(void* self RD9_THUNK_EDX, int category, int id, void* data, void* arg4)
{
    (void)category;
    (void)arg4;

    if (id != 1)
        return static_cast<char>(id - 1);

    // &m_eventVptr sits at +0x04, so the object starts that far back.
    ObjectDrawer* const drawer = reinterpret_cast<ObjectDrawer*>(
        static_cast<unsigned char*>(self) - offsetof(ObjectDrawer, m_eventVptr));

    // `data` is the notification node; PE +0x2C is its payload pointer, m_data (asserted below).
#if defined(_M_IX86)
    static_assert(offsetof(NotificationEventNode, m_data) == 0x2C, "retail PE notification payload");
#endif
    const unsigned int* const probe = static_cast<const unsigned int*>(
        static_cast<NotificationEventNode*>(static_cast<EventNode*>(data))->m_data);

    const char result = (*probe == 4u) ? 1 : 0;
    drawer->m_byte0C = static_cast<unsigned char>(result);
    return result;
}

// The two that take an argument, so they cannot use the macro above.
void RD9_THUNK od_draw(void* self RD9_THUNK_EDX, float time)
{
    static_cast<ObjectDrawer*>(self)->draw(time);
}

void RD9_THUNK od_updateVisible(void* self RD9_THUNK_EDX, float time)
{
    static_cast<ObjectDrawer*>(self)->updateVisible(time);
}

void RD9_THUNK od_slot18(void* self RD9_THUNK_EDX)
{
    static_cast<ObjectDrawer*>(self)->slot_0x18();
}

void RD9_THUNK od_slot20(void* self RD9_THUNK_EDX, void* object)
{
    (void)object;
    RD9_FRONTIER_AT("dice::hfe::geom::ObjectDrawer::slot_0x20 (0x100F8F30, 51 B - takes an object, "
                    "clears a flag through its +0xA4, then two helpers)", self, 0x20);
}

// the written leaf slots, as the thin __fastcall adapters the tables need

void RD9_THUNK od_addRef(void* self RD9_THUNK_EDX)
{
    static_cast<ObjectDrawer*>(self)->addRef();
}

int RD9_THUNK od_getRef(void* self RD9_THUNK_EDX)
{
    return static_cast<ObjectDrawer*>(self)->getRef();
}

int RD9_THUNK od_release(void* self RD9_THUNK_EDX)
{
    return static_cast<ObjectDrawer*>(self)->release();
}

void* RD9_THUNK od_queryInterface(void* self RD9_THUNK_EDX, unsigned int iid)
{
    return static_cast<const ObjectDrawer*>(self)->queryInterface(iid);
}

void* RD9_THUNK od_getObjectVector(void* self RD9_THUNK_EDX, int which)
{
    return static_cast<ObjectDrawer*>(self)->getObjectVector(which);
}

void RD9_THUNK od_setRenderView(void* self RD9_THUNK_EDX, void* view)
{
    static_cast<ObjectDrawer*>(self)->setRenderView(view);
}

void* RD9_THUNK od_getRenderView(void* self RD9_THUNK_EDX)
{
    return static_cast<const ObjectDrawer*>(self)->getRenderView();
}

void RD9_THUNK od_setOccluder(void* self RD9_THUNK_EDX, IBase* occluder)
{
    static_cast<ObjectDrawer*>(self)->setOccluder(occluder);
}

IBase* RD9_THUNK od_getOccluder(void* self RD9_THUNK_EDX)
{
    return static_cast<const ObjectDrawer*>(self)->getOccluder();
}


// Flag-based object-grid predicate with a compatibility-safe standalone virtual interface.
// mask also supplies query flags. Keep predicate/getFlags/performCulling/predicateSurface order,
// getFlags constness, and the absence of a virtual destructor.
class ObjectDrawerFlagPredicator
{
public:
    explicit ObjectDrawerFlagPredicator(unsigned int mask) : m_mask(mask) {}

    // every bit of the mask must be present, not merely one of them.
    virtual bool predicate(world::IObject* object)
    {
        return (m_mask & object->getFlags()) == m_mask;
    }
    virtual unsigned int getFlags() const       { return m_mask; }
    virtual bool performCulling(world::IObject*) { return true; }
    virtual bool predicateSurface(int)           { return true; }

private:
    unsigned int m_mask;
};


// [TEMP] [CHOSEN, not recovered] [RETIRE-WHEN: RW-objectdrawer-census]
// Counts rejection and missing geometry on the first drawn frame. updateGeometry only
// updates transforms; submission also requires a geometry instance.
// Remove this census and its two call sites once the first-frame result is established.

struct ObjectDrawerCensus
{
    unsigned int seen;
    unsigned int rejectedFlags;
    unsigned int withGeometry;
    unsigned int withGeometryPart;
    unsigned int nothingToDraw;
};

ObjectDrawerCensus g_objectDrawerCensus = { 0, 0, 0, 0, 0 };
bool               g_objectDrawerCensusReported = false;

void objectDrawerCensusReport()
{
    if (g_objectDrawerCensusReported)
        return;
    g_objectDrawerCensusReported = true;

    const ObjectDrawerCensus& c = g_objectDrawerCensus;
    std::fprintf(stderr,
                 "[CENSUS] ObjectDrawer first frame: %u seen, %u rejected by flags, "
                 "%u WITH geometry, %u with getGeometryPart>0, %u with NOTHING to draw\n",
                 c.seen, c.rejectedFlags, c.withGeometry, c.withGeometryPart, c.nothingToDraw);
    std::fflush(stderr);
}

// Updates visual transforms and decals for an object; geometry submission runs in the deferred pass.
// Null objects are ignored. Typed calls preserve platform-specific virtual ordering.
void objectDrawerDrawObject(world::IObject* object)
{
    if (!object)
        return;

    ++g_objectDrawerCensus.seen;                               // [TEMP] census

    const unsigned int flags = object->getFlags();
    if ((flags & world::IObject::OFIsDisabled) != 0 || (flags & world::IObject::OFIsVisible) == 0)
    {
        ++g_objectDrawerCensus.rejectedFlags;                  // [TEMP] census
        return;
    }

    if (void* const decal = object->getDecalObject())            // PE +0xD8
    {
        typedef void (__thiscall* DecalFn)(void*, int);
        void** const vtbl = *reinterpret_cast<void***>(decal);
        reinterpret_cast<DecalFn>(vtbl[0x0C / 4])(decal, 1);
    }

    if (world::object_detail::LayoutAccess::geometry(object))
    {
        ++g_objectDrawerCensus.withGeometry;                      // [TEMP] census
        object->updateGeometry();                                 // PE +0xB4
    }
    else if (object->getGeometryPart() > 0)                       // PE +0xC4
    {
        ++g_objectDrawerCensus.withGeometryPart;                  // [TEMP] census
        // THE result IS discarded.  0x100F7C1E calls it and drops the matrix; the only
        // effect that can matter is whatever recomputing it does.  Shipped, reproduced.
        object->getAbsoluteTransformation();                      // PE +0x80
        object->updateGeometry();
    }
    else
    {
        ++g_objectDrawerCensus.nothingToDraw;                     // [TEMP] census
    }
}

// byte_10239638 - the alternating gate below.  [INFERRED] name.
unsigned char g_objectDrawerVisualTick = 0;

// sub_100F7E80 - the per-object visual update: hand each object the frame time and its distance
// from the camera.

// The +0x0C flag limits visual updates to alternating frames using the shared toggle.
class ObjectDrawerFlagPredicator;

// Defined below beside the other set accessors; declared here because the visual update is
// the first body that walks a set.
world::IObject** objectDrawerSetBegin(ObjectDrawerSet& set);
world::IObject** objectDrawerSetEnd(ObjectDrawerSet& set);

void objectDrawerVisualDistanceUpdate(ObjectDrawer* self, ObjectDrawerSet& set, float time)
{
    if (self->m_byte0C && g_objectDrawerVisualTick != 1)
        return;

    dx9rend::IRenderView* const view = static_cast<dx9rend::IRenderView*>(self->m_renderView);

    for (world::IObject** cursor = objectDrawerSetBegin(set);
         cursor != objectDrawerSetEnd(set); ++cursor)
    {
        world::IObject* const object = *cursor;

        // *(object + 5) & 0x10 - bit 12 of m_flags, read as a byte.  The enum does not name it.
        if ((object->getFlags() & 0x00001000u) == 0)
            continue;

        // The camera is the view matrix translation row; the object's is its visual transform.
        const Mat4* const viewMatrix = view->getViewMatrixPtr();          // +0x64
        const Mat4& objectMatrix = object->getVisualAbsoluteTransformation();  // PE +0x8C

        const float dx = viewMatrix->m[12] - objectMatrix.m[12];
        const float dy = viewMatrix->m[13] - objectMatrix.m[13];
        const float dz = viewMatrix->m[14] - objectMatrix.m[14];
        const float distance = static_cast<float>(::sqrt(static_cast<double>(dx * dx + dy * dy + dz * dz)));

        object->handleVisualUpdate(time, distance);                        // PE +0xA4
    }

    g_objectDrawerVisualTick = self->m_byte0C ? 0u : 1u;
}

// Compares geometry sort keys; missing geometry compares false against every object.
// [PLATFORM] This breaks strict weak ordering. Iterator debug checks must remain disabled.
struct ObjectDrawerGeometrySortKey
{
    bool operator()(world::IObject* lhs, world::IObject* rhs) const
    {
        void* const lg = world::object_detail::LayoutAccess::geometry(lhs);
        void* const rg = world::object_detail::LayoutAccess::geometry(rhs);
        if (!lg || !rg)
            return false;

        typedef unsigned int (__thiscall* KeyFn)(void*);
        void** const lv = *reinterpret_cast<void***>(lg);
        void** const rv = *reinterpret_cast<void***>(rg);
        return reinterpret_cast<KeyFn>(lv[0x38 / 4])(lg)
             < reinterpret_cast<KeyFn>(rv[0x38 / 4])(rg);
    }
};

// the four set accessors, the only places that know which flavour is being built
// See the ABI note in the header: `native` uses the host's own std::vector, `compat` mirrors retail
// VC7.1's sixteen-byte shape.  Everything else below works in terms of IObject, so the two
// flavours share one body.

world::IObject** objectDrawerSetBegin(ObjectDrawerSet& set)
{
    return set.empty() ? 0 : &set[0];
}

world::IObject** objectDrawerSetEnd(ObjectDrawerSet& set)
{
    return set.empty() ? 0 : &set[0] + set.size();
}

void objectDrawerSort(ObjectDrawerSet& set)
{
    world::IObject** const first = objectDrawerSetBegin(set);
    world::IObject** const last  = objectDrawerSetEnd(set);
    if (first && first != last)
        std::sort(first, last, ObjectDrawerGeometrySortKey());
}

// sub_10032330(v, 0, 0) - resize(0) on a vector of pointers: the elements are trivially
// destructible, so retail's resize(0) is nothing but last = first, and clear() is the same thing
// on the native side.  Capacity is retained either way, which is what makes the per-frame reuse
// allocation-free.
void objectDrawerClear(ObjectDrawerSet& set)
{
    set.clear();
}

unsigned int objectDrawerSetSize(const ObjectDrawerSet& set)
{
    return static_cast<unsigned int>(set.size());
}

// Queries host collision/noncollision object grids through the selected compatibility slot.
void objectDrawerQueryGrid(unsigned int slot, const void* frustum, ObjectDrawerSet& out,
                           ObjectDrawerFlagPredicator* predicator, float time)
{
    if (!::dice::hfe::objectManager)
        return;

    // In the native build, out is the host's std::vector; the plugin ABI uses its own layout.
    typedef void (__thiscall* QueryFn)(void*, const void*, void*,
                                       void*, int, float, float);
    void* const manager = ::dice::hfe::objectManager;
    void** const vtbl = *reinterpret_cast<void***>(manager);
    reinterpret_cast<QueryFn>(vtbl[slot / 4])(
        manager, frustum, &out, predicator, 1, 0.0f, time);
}


void objectDrawerBuildOccluders(ObjectDrawer* self, ObjectDrawerSet& set);
void objectDrawerCullObjects(ObjectDrawer* self, ObjectDrawerSet& set);
void objectDrawerDestroyMembers(ObjectDrawer* self);

// Submits each geometry in the object's chain using the drawer's view and occluder.
// The unmodeled interface uses +0x18 for submission and +0xCC for the next geometry.
void objectDrawerDeferredGeometryPass(ObjectDrawer* self, world::IObject* object)
{
    if (!object)
        return;

    const unsigned int flags = object->getFlags();
    if ((flags & world::IObject::OFIsDisabled) != 0 ||
        (flags & world::IObject::OFIsVisible) == 0)
        return;

    void* geometry = world::object_detail::LayoutAccess::geometry(object);
    while (geometry)
    {
        typedef void  (__thiscall* PassFn)(void*, void*, void*, int);
        typedef void* (__thiscall* NextFn)(void*);
        void** const vtbl = *reinterpret_cast<void***>(geometry);
        reinterpret_cast<PassFn>(vtbl[0x18 / 4])(
            geometry, self->m_renderView, self->m_occluder, 1);
        geometry = reinterpret_cast<NextFn>(vtbl[0xCC / 4])(geometry);
    }
}

void* const kObjectDrawerPrimaryVtable[16] =
{
    reinterpret_cast<void*>(&od_addRef),           // +0x00
    reinterpret_cast<void*>(&od_getRef),           // +0x04
    reinterpret_cast<void*>(&od_release),          // +0x08
    reinterpret_cast<void*>(&od_queryInterface),   // +0x0C
    reinterpret_cast<void*>(&od_draw),             // +0x10
    reinterpret_cast<void*>(&od_updateVisible),    // +0x14
    reinterpret_cast<void*>(&od_slot18),           // +0x18
    reinterpret_cast<void*>(&od_slot1C),           // +0x1C
    reinterpret_cast<void*>(&od_slot20),           // +0x20
    reinterpret_cast<void*>(&od_getObjectVector),  // +0x24
    reinterpret_cast<void*>(&od_setRenderView),    // +0x28
    reinterpret_cast<void*>(&od_getRenderView),    // +0x2C
    reinterpret_cast<void*>(&od_setOccluder),      // +0x30
    reinterpret_cast<void*>(&od_getOccluder),      // +0x34
    reinterpret_cast<void*>(&od_slot38),           // +0x38
    reinterpret_cast<void*>(&od_slot3C)            // +0x3C
};

void* const kObjectDrawerEventVtable[1] =
{
    reinterpret_cast<void*>(&od_event00)           // +0x00
};

} // anonymous namespace


// Initializes drawer state and registers its ECCore subobject at priority zero.

ObjectDrawer::ObjectDrawer()
    : m_vptr(kObjectDrawerPrimaryVtable)
    , m_eventVptr(kObjectDrawerEventVtable)
    , m_refCount(1)
    , m_byte0C(0)
    , m_updateVisibleHasRun(0)
    , m_byte0E(0)
    , m_renderView(0)
    , m_occluder(0)
    , m_byte60(1)
    , m_float64(60.0f)
    , m_byte68(1)
    , m_dword6C(0)
    , m_byte70(0)
    , m_culledCount(0)
    , m_dword78(0)
    , m_dword7C(0)
    , m_dword80(0)
    , m_dword84(0)
    , m_byte88(0)
{
    // The two trees (+0x10, +0x1C; sub_100F9A80 builds each sentinel), the two object sets
    // (+0x30, +0x40) and the occluder list (+0x50) are default-constructed empty.

    // EventManager vslot +0x2C, registerEventHandler(category 0, &subobject, 0).
    if (::dice::hfe::eventManager)
    {
        ::dice::hfe::eventManager->registerEventHandler(
            static_cast<EventCategory>(0),
            reinterpret_cast<IEventListener*>(&m_eventVptr),
            0);
    }
}

ObjectDrawer::~ObjectDrawer()
{
    objectDrawerDestroyMembers(this);
}

// the IBase quartet

void ObjectDrawer::addRef()                                  // 0x1001A010, the shared body
{
    ++m_refCount;
}

int ObjectDrawer::getRef()                                   // 0x10051870, the shared body
{
    return m_refCount;
}

// Decrement, and on zero run the destructor and free through the engine allocator.
int ObjectDrawer::release()
{
    const int remaining = --m_refCount;
    if (remaining == 0)
    {
        this->~ObjectDrawer();
        ::renddx9abi::engineFree(this);
    }
    return remaining;
}

// Two ids answer, everything else is null.  IT IS NOT const-correct in the image -
// the slot takes the object non-const - but every recovered reader treats it as a query.
void* ObjectDrawer::queryInterface(unsigned int iid) const
{
    if (iid == 1u)
        return const_cast<ObjectDrawer*>(this);
    if (iid == IID_IObjectDrawer)
        return const_cast<ObjectDrawer*>(this);
    return 0;
}

// the leaf slots

// 0x100FAFD0, 19 B.  note THE sense: zero selects the first vector (+0x30), anything else the
// second (+0x40).  The shipped body computes `this + 64` first and only then branches.
ObjectDrawerSet* ObjectDrawer::getObjectVector(int which)
{
    return which ? &m_drawSet : &m_visibleSet;
}

void ObjectDrawer::setRenderView(void* view)
{
    m_renderView = view;
}

void* ObjectDrawer::getRenderView() const
{
    return m_renderView;
}

// Releases the previous occluder before retaining the replacement, even when they are identical.
void ObjectDrawer::setOccluder(IBase* occluder)
{
    if (m_occluder)
    {
        m_occluder->release();
        m_occluder = 0;
    }

    m_occluder = occluder;

    if (occluder)
        occluder->addRef();
}

IBase* ObjectDrawer::getOccluder() const
{
    return m_occluder;
}

// Builds and sorts visible candidates, applies occlusion, then stores camera distances.
// Requires setRenderView before the call.

void ObjectDrawer::updateVisible(float time)
{
    objectDrawerClear(m_visibleSet);                       // sub_10032330(this+0x30, 0, 0)

    ObjectDrawerFlagPredicator predicator(world::IObject::OFIsVisible);   // sub_101C7E90(2)

    dx9rend::IRenderView* const view = static_cast<dx9rend::IRenderView*>(m_renderView);
    objectDrawerQueryGrid(0x40, view->getFrustumPtr(), m_visibleSet, &predicator, time);

    objectDrawerSort(m_visibleSet);

    if (m_byte88)
    {
        RD9_FRONTIER_AT("dice::hfe::geom::ObjectDrawer::updateVisible (sub_100F8750, the +0x88 "
                        "pass - unread; the constructor leaves +0x88 clear so nothing has reached "
                        "it yet)", this, 0x14);
    }

    m_dword7C = 0;
    m_dword80 = 0;
    m_dword84 = 0;

    m_culledCount = static_cast<int>(objectDrawerSetSize(m_visibleSet));

    if (m_byte60)
    {
        objectDrawerBuildOccluders(this, m_visibleSet);
        objectDrawerCullObjects(this, m_visibleSet);
    }

    m_culledCount -= static_cast<int>(objectDrawerSetSize(m_visibleSet));

    objectDrawerVisualDistanceUpdate(this, m_visibleSet, time);

    m_updateVisibleHasRun = 1;                             // +0x0D
}


// Draws the visible collision set, then separately queries and draws noncollision objects.

void ObjectDrawer::draw(float time)
{
    if (!m_updateVisibleHasRun)
    {
        // The shipped diagnostic is sub_10074300(6, __FILE__, 598, "") streamed into - dice's own
        // file and line.  This image logs instead of asserting; the text is theirs.
        ::renddx9abi::logf("ObjectDrawer.cpp:598 updateVisible() must be drawn before this!");
        return;
    }

    for (world::IObject** cursor = objectDrawerSetBegin(m_visibleSet);
         cursor != objectDrawerSetEnd(m_visibleSet); ++cursor)
    {
        objectDrawerDrawObject(*cursor);
    }

    objectDrawerClear(m_drawSet);                          // sub_10032330(this+0x40, 0, 0)

    ObjectDrawerFlagPredicator predicator(world::IObject::OFIsVisible);

    dx9rend::IRenderView* const view = static_cast<dx9rend::IRenderView*>(m_renderView);
    objectDrawerQueryGrid(0x4C, view->getFrustumPtr(), m_drawSet, &predicator, time);

    objectDrawerSort(m_drawSet);

    m_dword78 = objectDrawerSetSize(m_drawSet);

    if (m_byte60)
    {
        objectDrawerCullObjects(this, m_drawSet);
    }

    m_dword78 -= objectDrawerSetSize(m_drawSet);

    objectDrawerVisualDistanceUpdate(this, m_drawSet, time);

    // The same per-object draw again, inlined in the shipped body.
    for (world::IObject** cursor = objectDrawerSetBegin(m_drawSet);
         cursor != objectDrawerSetEnd(m_drawSet); ++cursor)
    {
        objectDrawerDrawObject(*cursor);
    }

    m_byte0E = 1;                                          // +0x0E

    objectDrawerCensusReport();                            // [TEMP] census, once
}


// Submits the deferred geometry pass once per updateVisible/draw pair.
// Consumes both completion flags before callbacks, so reentrant calls see them cleared.

void ObjectDrawer::slot_0x18()
{
    ObjectDrawerFlagPredicator predicator(world::IObject::OFIsVisible);   // built, never read
    (void)predicator;

    if (!m_updateVisibleHasRun || !m_byte0E)
    {
        // sub_10074300(6, __FILE__, 638, "") streamed into - dice's own file and line.  This image
        // logs where retail asserts; the text is theirs.
        ::renddx9abi::logf("ObjectDrawer.cpp:638 updateVisible() must be drawn before this!");
        return;
    }

    m_updateVisibleHasRun = 0;
    m_byte0E = 0;

    for (world::IObject** cursor = objectDrawerSetBegin(m_visibleSet);
         cursor != objectDrawerSetEnd(m_visibleSet); ++cursor)
    {
        objectDrawerDeferredGeometryPass(this, *cursor);
    }

    // The same body again over the draw set, inlined in the shipped code.
    for (world::IObject** cursor = objectDrawerSetBegin(m_drawSet);
         cursor != objectDrawerSetEnd(m_drawSet); ++cursor)
    {
        objectDrawerDeferredGeometryPass(this, *cursor);
    }
}
void ObjectDrawer::slot_0x1C()           { od_slot1C(this RD9_THUNK_ARG); }
void ObjectDrawer::slot_0x20(void* o)    { od_slot20(this RD9_THUNK_ARG, o); }


// Tests geometry against the occluder set and counts hidden objects.
// Precise mode falls back from the sphere to box corners when a plane intersects it.
// An occluder with no planes hides every tested object; an empty set hides none.

namespace {

// The geometry instance slots, reached by offset through the instance's retail-ordered table as the
// deferred pass above does (StaticMesh / BundledMesh / SkinnedMesh install the same arity at each).
inline void* occlusionGeometrySlot(void* geometry, unsigned int offset)
{
    return (*reinterpret_cast<void***>(geometry))[offset / 4];
}

inline const LocalBoundingBox& occlusionGeometryLocalBounds(void* geometry)          // +0x40
{
    typedef const void* (__thiscall* BoundsFn)(void*);
    return *static_cast<const LocalBoundingBox*>(
        reinterpret_cast<BoundsFn>(occlusionGeometrySlot(geometry, 0x40))(geometry));
}

inline float occlusionGeometryBoundingRadius(void* geometry)                         // +0x44
{
    typedef float (__thiscall* RadiusFn)(void*);
    return reinterpret_cast<RadiusFn>(occlusionGeometrySlot(geometry, 0x44))(geometry);
}

inline const Mat4* occlusionGeometryTransformation(void* geometry)                  // +0x48
{
    typedef const Mat4* (__thiscall* TransformFn)(void*);
    return reinterpret_cast<TransformFn>(occlusionGeometrySlot(geometry, 0x48))(geometry);
}


// The eight-corner object box - 0x100AE110 (the 24 floats zeroed), 0x101C6750 (corner i takes x from
// max when bit 2, y when bit 1, z when bit 0) and 0x101C67F0 (all three rows formed from the untouched
// corner; corner 0's y and x rows associate differently from corners 1..7). The same bodies as
// XpLightManager.cpp's ShadowBoxCorners, re-read for this slot.
struct OcclusionBoxCorners
{
    Vec3 corner[8];

    OcclusionBoxCorners()
    {
        for (unsigned int i = 0; i < 8; ++i)
            corner[i].x = corner[i].y = corner[i].z = 0.0f;
    }

    void setBounds(const LocalBoundingBox& bounds)
    {
        for (unsigned int i = 0; i < 8; ++i)
        {
            corner[i].x = (i & 4u) ? bounds.m_max.x : bounds.m_min.x;
            corner[i].y = (i & 2u) ? bounds.m_max.y : bounds.m_min.y;
            corner[i].z = (i & 1u) ? bounds.m_max.z : bounds.m_min.z;
        }
    }

    void transform(const Mat4& world)
    {
        const float* const m = world.m;
        for (unsigned int i = 0; i < 8; ++i)
        {
            const float x = corner[i].x;
            const float y = corner[i].y;
            const float z = corner[i].z;
            const float tz = z * m[10] + x * m[2] + y * m[6] + m[14];
            const float ty = i == 0 ? x * m[1] + y * m[5] + z * m[9] + m[13]
                                    : x * m[1] + z * m[9] + y * m[5] + m[13];
            const float tx = i == 0 ? z * m[8] + x * m[0] + y * m[4] + m[12]
                                    : y * m[4] + z * m[8] + x * m[0] + m[12];
            corner[i].x = tx;
            corner[i].y = ty;
            corner[i].z = tz;
        }
    }
};


// Frees each owned plane list while retaining capacity in the outer occluder list.
void objectDrawerClearOccluders(ObjectDrawer* self)
{
    self->m_vector50.clear();
}

void objectDrawerNormalizeOccluderNormal(Vec3& normal)
{
    const float lengthSquared = static_cast<float>(
        static_cast<double>(normal.x) * normal.x
        + static_cast<double>(normal.y) * normal.y
        + static_cast<double>(normal.z) * normal.z);
    const double epsilon = 1.1920928955078125e-7;
    if (::fabs(static_cast<double>(lengthSquared) - 1.0) < epsilon)
        return;
    if (::fabs(static_cast<double>(lengthSquared)) < epsilon)
    {
        normal.x = normal.y = normal.z = 0.0f;
        return;
    }
    const double inverse = 1.0 / ::sqrt(static_cast<double>(lengthSquared));
    normal.x = static_cast<float>(inverse * normal.x);
    normal.y = static_cast<float>(inverse * normal.y);
    normal.z = static_cast<float>(inverse * normal.z);
}

// Builds camera-facing source and silhouette planes from nearby world-space occluders.
// Requires a render view and the StaticMesh face/edge/point records to remain valid for the call.
void objectDrawerBuildOccluders(ObjectDrawer* self, ObjectDrawerSet& objects)
{
    dx9rend::IRenderView* const view = static_cast<dx9rend::IRenderView*>(self->m_renderView);
    const float* const camera = view->getViewMatrixPtr()->m + 12;
    const float* const forward = view->getViewMatrixPtr()->m + 8;
    objectDrawerClearOccluders(self);

    for (world::IObject** cursor = objectDrawerSetBegin(objects);
         cursor != objectDrawerSetEnd(objects); ++cursor)
    {
        world::IObject* const object = *cursor;
        void* const geometry = world::object_detail::LayoutAccess::geometry(object);
        if (!geometry)
            continue;

        typedef int (__thiscall* RecordCountFn)(void*);
        const int recordCount = reinterpret_cast<RecordCountFn>(
            occlusionGeometrySlot(geometry, 0xE8))(geometry);
        if (!recordCount)
            continue;

        const Vec3& position = object->getAbsolutePosition();
        const double dx = static_cast<double>(position.x) - camera[0];
        const double dy = static_cast<double>(position.y) - camera[1];
        const double dz = static_cast<double>(position.z) - camera[2];
        if (static_cast<double>(self->m_float64) * self->m_float64 < dx * dx + dz * dz + dy * dy)
            continue;

        // The records are StaticMesh's world-space occluders; their edges are the template's.
        typedef const StaticMeshOccluderRecord* (__thiscall* RecordFn)(void*, int);
        for (int recordIndex = 0; recordIndex < recordCount; ++recordIndex)
        {
            const StaticMeshOccluderRecord* const record = reinterpret_cast<RecordFn>(
                occlusionGeometrySlot(geometry, 0xEC))(geometry, recordIndex);
            std::vector<StaticMeshOccluderEdge>& edges = *record->edges;
            const unsigned int edgeCount = static_cast<unsigned int>(edges.size());
            const unsigned int faceCount = static_cast<unsigned int>(record->faces.size());
            if (!faceCount)
                continue;

            // The local plane list; a separate copy is appended to the drawer's outer list.
            std::vector<Plane3> planes;
            const StaticMeshOccluderFace* const faces = record->faces.data();
            for (unsigned int faceIndex = 0; faceIndex < faceCount; ++faceIndex)
            {
                const StaticMeshOccluderFace& face = faces[faceIndex];
                const Plane3& plane = face.plane;
                // The shipped loop takes every iteration's reference count from the first face.
                const unsigned int referenceCount =
                    static_cast<unsigned int>(faces[0].edgeReferences->size());
                const double distance = static_cast<double>(plane.normal.y) * camera[1]
                    + static_cast<double>(plane.normal.x) * camera[0]
                    + static_cast<double>(plane.normal.z) * camera[2] + plane.d;
                unsigned char selected = 0;
                // The original first comparison admits unordered values; the angle comparison does not.
                if (!(distance <= 0.0))
                {
                    const double facing = -static_cast<double>(forward[2]) * plane.normal.z
                        + -static_cast<double>(forward[1]) * plane.normal.y
                        + -static_cast<double>(forward[0]) * plane.normal.x;
                    float angleThreshold;
                    ::memcpy(&angleThreshold, &self->m_dword6C, sizeof(angleThreshold));
                    if (facing > angleThreshold)
                    {
                        planes.push_back(plane);
                        selected = 1;
                    }
                }
                for (unsigned int referenceIndex = 0; referenceIndex < referenceCount; ++referenceIndex)
                {
                    // The edge index in the low byte, the created-here flag in the high byte.
                    const unsigned short reference = (*face.edgeReferences)[referenceIndex];
                    StaticMeshOccluderEdge& edge = edges[reference & 0xFFu];
                    edge.marks[(reference >> 8) ? 0 : 1] = selected;
                }
            }
            if (planes.empty())
                continue;

            const Vec3* const points = record->points.data();
            for (unsigned int edgeIndex = 0; edgeIndex < edgeCount; ++edgeIndex)
            {
                const StaticMeshOccluderEdge& edge = edges[edgeIndex];
                if (static_cast<unsigned int>(edge.marks[0]) + edge.marks[1] != 1u)
                    continue;
                const Vec3& a = points[edge.points[0]];
                const Vec3& b = points[edge.points[1]];
                const double ax = static_cast<double>(a.x) - camera[0];
                const double ay = static_cast<double>(a.y) - camera[1];
                const float az = static_cast<float>(static_cast<double>(a.z) - camera[2]);
                const double ex = static_cast<double>(b.x) - a.x;
                const double ey = static_cast<double>(b.y) - a.y;
                const double ez = static_cast<double>(b.z) - a.z;
                const float storedEz = static_cast<float>(ez);
                Plane3 silhouette;
                silhouette.normal.x = static_cast<float>(ez * ay - ey * az);
                silhouette.normal.y = static_cast<float>(az * ex - storedEz * ax);
                silhouette.normal.z = static_cast<float>(ey * ax - ay * ex);
                objectDrawerNormalizeOccluderNormal(silhouette.normal);
                if (edge.marks[0])
                {
                    silhouette.normal.x = -silhouette.normal.x;
                    silhouette.normal.y = -silhouette.normal.y;
                    silhouette.normal.z = -silhouette.normal.z;
                }
                silhouette.d = static_cast<float>(-(static_cast<double>(silhouette.normal.z) * b.z
                    + static_cast<double>(silhouette.normal.y) * b.y
                    + static_cast<double>(silhouette.normal.x) * b.x));
                planes.push_back(silhouette);
            }
            self->m_vector50.push_back(planes);
        }
    }
}

// Removes objects wholly behind any occluder's planes, retaining the order of survivors.
// Precise mode tests transformed geometry bounds only when the sphere intersects a plane.
void objectDrawerCullObjects(ObjectDrawer* self, ObjectDrawerSet& set)
{
    const bool precise = self->m_byte68 != 0;
    OcclusionBoxCorners box;
    unsigned int index = 0;
    while (index < objectDrawerSetSize(set))
    {
        world::IObject* const object = objectDrawerSetBegin(set)[index];
        const Vec3& centre = object->getAbsolutePosition();
        const float radius = object->getVisualBoundingRadius();
        void* const geometry = precise
            ? world::object_detail::LayoutAccess::geometry(object) : 0;
        bool boxBuilt = false;
        bool hidden = false;

        for (unsigned int occluder = 0; occluder < self->m_vector50.size(); ++occluder)
        {
            const std::vector<Plane3>& planes = self->m_vector50[occluder];
            hidden = true;
            for (std::vector<Plane3>::const_iterator cursor = planes.begin(); cursor != planes.end();
                 ++cursor)
            {
                const Plane3& plane = *cursor;
                const double distance = precise
                    ? static_cast<double>(plane.normal.z) * centre.z
                      + static_cast<double>(plane.normal.y) * centre.y
                      + static_cast<double>(plane.normal.x) * centre.x + plane.d
                    : static_cast<double>(plane.normal.z) * centre.z
                      + static_cast<double>(plane.normal.x) * centre.x
                      + static_cast<double>(centre.y) * plane.normal.y + plane.d;
                if (precise && distance > 0.0f)
                {
                    hidden = false;
                    break;
                }
                if (distance > -radius)
                {
                    if (!precise || !geometry)
                    {
                        hidden = false;
                        break;
                    }
                    if (!boxBuilt)
                    {
                        box.setBounds(occlusionGeometryLocalBounds(geometry));
                        box.transform(object->getAbsoluteTransformation());
                        boxBuilt = true;
                    }
                    for (unsigned int corner = 0; corner < 8; ++corner)
                    {
                        const Vec3& point = box.corner[corner];
                        if (static_cast<double>(point.z) * plane.normal.z
                            + static_cast<double>(point.x) * plane.normal.x
                            + static_cast<double>(plane.normal.y) * point.y + plane.d > 0.0)
                        {
                            hidden = false;
                            break;
                        }
                    }
                    if (!hidden)
                        break;
                }
            }
            if (hidden)
                break;
        }

        if (hidden)
        {
            if (precise ? geometry : world::object_detail::LayoutAccess::geometry(object))
                ++self->m_dword7C;
            set.erase(set.begin() + index);
        }
        else
        {
            ++index;
        }
    }
}

// Detaches the event listener and releases the drawer's owned storage and occluder.
// The event manager must outlive the drawer. Render view and scene objects are borrowed.
void objectDrawerDestroyMembers(ObjectDrawer* self)
{
    self->m_vptr = kObjectDrawerPrimaryVtable;
    self->m_eventVptr = kObjectDrawerEventVtable;
    ::dice::hfe::eventManager->unregisterEventHandler(
        static_cast<EventCategory>(0), reinterpret_cast<IEventListener*>(&self->m_eventVptr));
    objectDrawerClearOccluders(self);
    std::vector<std::vector<Plane3> >().swap(self->m_vector50);
    ObjectDrawerSet().swap(self->m_drawSet);
    ObjectDrawerSet().swap(self->m_visibleSet);
    if (self->m_occluder)
        self->m_occluder->release();
    // The +0x1C tree, then the +0x10 tree, are destroyed with the drawer after this body.
}

} // anonymous namespace

namespace {

// Table thunks use __fastcall with an unused EDX to match the __thiscall slots.
// Keep definitions in the anonymous namespace containing their declarations.
bool RD9_THUNK od_slot38(void* self RD9_THUNK_EDX, void* geometry)
{
    return static_cast<ObjectDrawer*>(self)->isGeometryOccluded(geometry);
}

bool RD9_THUNK od_slot3C(void* self RD9_THUNK_EDX, void* bounds)
{
    return static_cast<ObjectDrawer*>(self)->isBoundsOccluded(static_cast<LocalBoundingBox*>(bounds));
}

} // anonymous namespace

bool ObjectDrawer::isGeometryOccluded(void* geometry)
{
    if (!m_byte60)
        return false;
    const Mat4* const transform = occlusionGeometryTransformation(geometry);  // +0x48
    const Vec3& centre = *reinterpret_cast<const Vec3*>(&transform->m[12]);
    const float radius = occlusionGeometryBoundingRadius(geometry);         // +0x44

    if (m_byte68)
    {
        OcclusionBoxCorners box;
        bool boxBuilt = false;                                               // [ebp-1]
        for (unsigned int occluder = 0; ; ++occluder)
        {
            if (occluder >= m_vector50.size())
                break;
            const std::vector<Plane3>& record = m_vector50[occluder];
            bool hidden = true;                                              // [ebp-2]
            for (unsigned int index = 0; ; ++index)
            {
                if (index >= record.size())
                    break;
                const Plane3& plane = record[index];
                const float distance = plane.normal.z * centre.z + plane.normal.y * centre.y
                                     + plane.normal.x * centre.x + plane.d;
                if (distance > 0.0f)
                {
                    hidden = false;                                          // straight to the
                    break;                                                   // next occluder
                }
                if (distance > -radius)
                {
                    if (!boxBuilt)
                    {
                        box.setBounds(occlusionGeometryLocalBounds(geometry));       // +0x40
                        box.transform(*occlusionGeometryTransformation(geometry));   // +0x48
                        boxBuilt = true;
                    }
                    for (unsigned int c = 0; c < 8; ++c)
                    {
                        const Vec3& k = box.corner[c];
                        if (k.x * plane.normal.x + k.z * plane.normal.z + plane.normal.y * k.y + plane.d > 0.0f)
                        {
                            hidden = false;                                  // 100F8373, next plane
                            break;
                        }
                    }
                }
            }
            if (hidden)
            {
                ++m_dword80;
                return true;
            }
        }
        return false;
    }

    for (unsigned int occluder = 0; ; ++occluder)
    {
        if (occluder >= m_vector50.size())
            break;
        const std::vector<Plane3>& planes = m_vector50[occluder];
        bool hidden = true;
        for (unsigned int index = 0; index < planes.size(); ++index)
        {
            const Plane3& plane = planes[index];
            const float distance = plane.normal.z * centre.z + plane.normal.x * centre.x
                                 + centre.y * plane.normal.y + plane.d;
            if (-radius < distance)                                          // 100F8409 `jnp`
            {
                hidden = false;
                break;
            }
        }
        if (hidden)
        {
            ++m_dword80;
            return true;
        }
    }
    return false;
}

// Tests whether an axis-aligned world-space box is hidden by an occluder.
// bounds must be valid; enabled queries finalize its derived data in place.
// Uses the origin-based radius around the box centre. An occluder with no planes hides it.
bool ObjectDrawer::isBoundsOccluded(LocalBoundingBox* bounds)
{
    if (!m_byte60)
        return false;
    if (bounds->m_derivedDataDirty)
        meshBoundsFinalize(*reinterpret_cast<MeshBounds*>(bounds));
    if (bounds->m_derivedDataDirty)
        meshBoundsFinalize(*reinterpret_cast<MeshBounds*>(bounds));
    const float radius = bounds->m_radiusFromOrigin;                         // +0x18
    const Vec3& centre = bounds->m_centre;                                   // +0x30

    if (m_byte68)
    {
        for (unsigned int occluder = 0; ; ++occluder)
        {
            if (occluder >= m_vector50.size())
                break;
            const std::vector<Plane3>& record = m_vector50[occluder];
            bool hidden = true;                                              // [ebp-1]
            for (unsigned int index = 0; ; ++index)
            {
                if (index >= record.size())
                    break;
                const Plane3& plane = record[index];
                const float distance = centre.y * plane.normal.y + plane.normal.x * centre.x
                                     + centre.z * plane.normal.z + plane.d;
                if (distance > 0.0f)
                {
                    hidden = false;                                          // straight to the
                    break;                                                   // next occluder
                }
                if (distance > -radius)
                {
                    for (unsigned int c = 0; c < 8; ++c)
                    {
                        const float kx = (c & 4u) ? bounds->m_max.x : bounds->m_min.x;
                        const float ky = (c & 2u) ? bounds->m_max.y : bounds->m_min.y;
                        const float kz = (c & 1u) ? bounds->m_max.z : bounds->m_min.z;
                        if (ky * plane.normal.y + kz * plane.normal.z + kx * plane.normal.x + plane.d > 0.0f)
                        {
                            hidden = false;                                  // 100F8138, next plane
                            break;
                        }
                    }
                }
            }
            if (hidden)
            {
                ++m_dword84;
                return true;
            }
        }
        return false;
    }

    for (unsigned int occluder = 0; ; ++occluder)
    {
        if (occluder >= m_vector50.size())
            break;
        const std::vector<Plane3>& planes = m_vector50[occluder];
        bool hidden = true;
        for (unsigned int index = 0; index < planes.size(); ++index)
        {
            const Plane3& plane = planes[index];
            const float distance = plane.normal.z * centre.z + plane.normal.x * centre.x
                                 + centre.y * plane.normal.y + plane.d;
            if (-radius < distance)                                          // 100F81DA `jnp`
            {
                hidden = false;
                break;
            }
        }
        if (hidden)
        {
            ++m_dword84;
            return true;
        }
    }
    return false;
}


// RefFactory_geom_ObjectDrawer - 0x100B0ADE.  operator new(0x8C), the constructor, then
// queryInterface; the same shape as every other factory in this image.

IBase* __fastcall createObjectDrawer(unsigned int iid, IBase* outer)
{
    if (outer)
        return 0;

    void* const storage = ::renddx9abi::engineAlloc(sizeof(ObjectDrawer));
    if (!storage)
        return 0;

    ObjectDrawer* const self = new (storage) ObjectDrawer();

    void* const itf = self->queryInterface(iid);
    if (!itf)
        self->release();

    return static_cast<IBase*>(itf);
}

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
