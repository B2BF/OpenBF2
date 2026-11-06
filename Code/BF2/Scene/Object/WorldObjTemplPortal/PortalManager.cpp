// Portal registration and sector transitions. Directory is attested; filename [INFERRED].
// Sector transitions remain unimplemented and require the original 0.01 re-test offset.

#include "PortalManager.h"
#include <cstdio>
#include <cstdlib>

#include "../../../Core/Ids.h"       // IID_IBase, world::IID_IPortalManager - the two ids
                                     // queryInterface compares against, both loaded from globals in
                                     // the ELF (0x8328616 / 0x8328625), not from immediates.
#include "../../../Game/Objects/IObject.h"

#include <new>

namespace dice {
namespace hfe {
namespace world {

// Starts with one reference and an empty callback set for each of the two portal event types.
PortalManager::PortalManager()
    : m_refCount(1)                                 // +0x04
    , m_passingObjects()                            // +0x08  allocate(0x18) at 0x8325F8D
    , m_sectorObjects()                             // +0x14  allocate(0x14) at 0x8325FC9
    , m_globalCallbacks()                           // +0x20  0x8325FF4/FB/0x8326002
    , m_showStats(false)                            // +0x2C
    , m_debugLevel(0)                               // +0x30
{
    // One callback set per event type. `operator new(0xCu)` at 0x8326022 is the 12-byte
    // std::set<IPortalEventCallback*>; its own header node is the allocate(0x14) at 0x8326036.
    for (int type = 0; type < NUM_PORTAL_EVENT_TYPES; ++type)
        m_globalCallbacks.push_back(new std::set<IPortalEventCallback*>());
}

// Deletes PassingObjectData records and callback sets, but not PortalPassing objects or callbacks.
// Callers must unregister live passing objects before manager destruction to release them.
PortalManager::~PortalManager()
{
    // The per-object data blocks. Loop 0x8326280-0x832629E; the delete itself is the hoisted block
    // at 0x83263F0. The map is emptied by its own destructor afterwards (_M_erase at 0x8326350,
    // deallocate(header, 24) at 0x8326330).
    for (std::map<IObject*, PassingObjectData*>::iterator it = m_passingObjects.begin();
         it != m_passingObjects.end(); ++it)
    {
        delete it->second;                          // 0x83263F3, bare operator delete - POD
    }

    // The two callback sets. Loop 0x83262B0-0x83262E6.
    for (std::vector<std::set<IPortalEventCallback*>*>::iterator it = m_globalCallbacks.begin();
         it != m_globalCallbacks.end(); ++it)
    {
        delete *it;                                 // 0x83262D7, set dtor + operator delete
    }

    // m_sectorObjects is emptied by its own destructor: _M_erase at 0x8326381, deallocate(header,
    // 20) at 0x8326315. The vector's buffer goes at 0x83263B2. All three are compiler output.
}

// The IBase quartet. 0x83285B0 / 0x83285C0 / 0x83285D0 / 0x8328610.

// Byte for byte the same four functions as world::BasicPhysicsSystem's - same sizes (15/11/57/32),
// same shapes. release() calls vtable +0x14, the deleting destructor (`call dword ptr [eax+14h]` at
// 0x83285F0), and returns the new count (`mov eax, ebx` at 0x8328600).
void PortalManager::addRef()
{
    ++m_refCount;
}

int PortalManager::getRef()
{
    return m_refCount;
}

int PortalManager::release()
{
    int remaining = --m_refCount;
    if (remaining == 0)
        delete this;                                // 0x83285F0, call [vtable+0x14] = D0
    return remaining;
}

// Returns IBase or IPortalManager for the matching global id, otherwise null.
// Factory registration must request IID_IPortalManager, not zero.
void* PortalManager::queryInterface(unsigned int iid) const
{
    if (iid == IID_IBase)
        return const_cast<PortalManager*>(this);
    if (iid == IID_IPortalManager)
        return const_cast<PortalManager*>(this);
    return 0;
}

// Adds a global callback for event type 0 or 1. The caller must keep type within
// the two-entry vector; registration does not check bounds.
void PortalManager::subscribeGlobalEvents(PortalEventType type, IPortalEventCallback* callback)
{
    m_globalCallbacks[type]->insert(callback);      // 0x83271AC index, 0x83271C7 insert_unique
}

// unSubscribeGlobalEvents - 0x83271E0, 41 bytes. The mirror image; `erase(const key&)`, whose return
// (a size_type) is discarded.
void PortalManager::unSubscribeGlobalEvents(PortalEventType type, IPortalEventCallback* callback)
{
    m_globalCallbacks[type]->erase(callback);       // 0x83271EC index, 0x8327200 erase
}

// Intentionally empty statistics rendering in the dedicated image.
void PortalManager::renderStats()
{
}

// Reads or writes the statistics flag and integer debug level.
void PortalManager::setShowStats(bool on)
{
    m_showStats = on;
}

bool PortalManager::getShowStats() const
{
    return m_showStats;                             // 0x8327497, movzx
}

void PortalManager::setDebugLevel(int level)
{
    m_debugLevel = level;
}

int PortalManager::getDebugLevel() const
{
    return m_debugLevel;
}

// [CHOSEN, not recovered] Portal registration, event, and update operations remain
// incomplete. Attaching PortalPassing must preserve replacement without deleting the
// previous registration data.
// [FRONTIER] Registering a portal-passing object stops here instead of silently dropping the
// object: update() is recovered only for an empty passing-object map. No stock BF2 1.50 object
// template sets isPortalPassing (none of the 9007 files in Objects_server.zip mention portals).
void PortalManager::registerPortalPassingObject(IObject* object, const Vec3& portalPassingCoordinate)
{
    (void)object;
    (void)portalPassingCoordinate;
    std::fprintf(stderr, "[FRONTIER] dice::hfe::world::PortalManager::registerPortalPassingObject reached"
                         " (PortalPassing component and passing-object record unrecovered)\n");
    std::fflush(stderr);
    std::abort();
}

// Removes a portal registration by detaching the component before releasing it, then deletes data and erases the entry.
void PortalManager::unRegisterPortalPassingObject(IObject* object)
{
    std::map<IObject*, PassingObjectData*>::iterator it =
        m_passingObjects.find(object);
    if (it == m_passingObjects.end())
        return;

    object->setComponent(IID_IPortalPassing, 0);
    PassingObjectData* data = it->second;
    reinterpret_cast<IBase*>(data->portalPassing)->release();
    ::operator delete(data);
    m_passingObjects.erase(it);
}

// [CHOSEN, not recovered] sector-registration placeholder.
// Required behavior creates/attaches a Sector and inserts the object; repeated registration overwrites
// the prior component without releasing it. Sector construction remains to be implemented.
void PortalManager::registerSectorObject(IObject* object)
{
    (void)object;
    // [CHOSEN, not recovered] - see above.
}

// Queries the sector with IID_ISector in both arguments, detaches it, releases it and erases membership.
// The doubled-ID interpretation remains [UNRESOLVED]; preserve the observed dispatch.
void PortalManager::unRegisterSectorObject(IObject* object)
{
    IBase* sector = object->queryComponent(IID_ISector, IID_ISector);
    if (!sector)
        return;

    object->setComponent(IID_ISector, 0);
    sector->release();
    m_sectorObjects.erase(object);
}

// [CHOSEN, not recovered] subscription placeholders awaiting PortalPassing dispatch.
// Required behavior inserts a null entry for an unregistered object and returns false;
// registered objects forward subscribe/unsubscribe and return true.
bool PortalManager::subscribePortalPassingObjectEvents(PortalEventType type,
                                                       IObject* object,
                                                       IPortalEventCallback* callback)
{
    (void)type; (void)object; (void)callback;
    return false;   // [CHOSEN, not recovered]. The real one returns true when the object has a
                    // PortalPassing. false is the real function's own "no such object" answer, so
                    // callers see a defined, in-range result rather than a fabricated success.
}

bool PortalManager::unSubscribePortalPassingObjectEvents(PortalEventType type,
                                                         IObject* object,
                                                         IPortalEventCallback* callback)
{
    (void)type; (void)object; (void)callback;
    return false;   // [CHOSEN, not recovered] - as above.
}

// [CHOSEN, not recovered] sector-subscription placeholders awaiting Sector dispatch.
// Required behavior queries the sector directly, without requiring manager registration; null fails,
// otherwise forwards subscribe/unsubscribe and succeeds.
bool PortalManager::subscribeSectorEvents(PortalEventType type,
                                          IObject* object,
                                          IPortalEventCallback* callback)
{
    (void)type; (void)object; (void)callback;
    return false;   // [CHOSEN, not recovered]. false is the real function's "not a sector" answer.
}

bool PortalManager::unSubscribeSectorEvents(PortalEventType type,
                                            IObject* object,
                                            IPortalEventCallback* callback)
{
    (void)type; (void)object; (void)callback;
    return false;   // [CHOSEN, not recovered] - as above.
}

// [CHOSEN, not recovered] event-broadcast placeholder; currently delivers nothing.
// Required behavior selects the unchecked event-type callback set and calls notification slot zero.
// The callback interface has no IBase reference-count quartet.
void PortalManager::broadcastEvent(PortalEvent* event) const
{
    (void)event;
    // [CHOSEN, not recovered] - see above. Delivers nothing.
}

// Unimplemented portal-mesh intersection path, [CHOSEN, not recovered].
// The required body tests the mesh/collider, returns null for no hit and prepends a hit record.
// Its intersection outputs must remain function-static, making the operation non-reentrant.
// Implement the portal mesh, collider and sector dependencies before replacing this placeholder.
PortalManager::PortalCollision* PortalManager::checkAddPortalCollision(
        std::list<PortalCollision*>& collisions,
        IObject* object,
        const Vec3& from,
        const Vec3& to,
        bool twoSided) const
{
    (void)collisions; (void)object; (void)from; (void)to; (void)twoSided;
    return 0;   // [CHOSEN, not recovered]. 0 is the real function's "no portal crossed" answer, and
                // it is the value that terminates update()'s inner while-loop - so a caller written
                // against the real one still behaves sanely against this.
}

// ELF 0x8326A70. The body walks the passing-object map (+0x08); with no passing objects it only
// constructs and frees its temporary collision list and vector, so it has no effect (the branch
// at 0x8326AD5 to 0x8327092). deltaTime is not read. The per-object portal crossing, sector and
// event part is unrecovered and stops.
void PortalManager::update(float deltaTime)
{
    (void)deltaTime;
    if (m_passingObjects.empty())
        return;
    std::fprintf(stderr, "[FRONTIER] dice::hfe::world::PortalManager::update reached with portal-passing"
                         " objects (crossing, sector and event dispatch unrecovered)\n");
    std::fflush(stderr);
    std::abort();
}

} // namespace world
} // namespace hfe
} // namespace dice

// Portal interfaces have distinct virtual layouts: callbacks start with notification, while
// PortalPassing's destructor follows its three portal methods and Sector's follows IBase.
// Use each real interface declaration; a common guessed layout changes dispatch.
