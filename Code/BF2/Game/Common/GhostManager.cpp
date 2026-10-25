// Per-connection object replication with priority scheduling, delta compression
// and packet acknowledgements. Saved updates own snapshots for resend; do not
// replace them with pointers into live object state. The controlled object uses
// a separate prediction/reconciliation path.

#include "GhostManager.h"

#include "../../Core/Ids.h"                      // IID_IBase, IID_IStreamManager
#include "../../Core/Debug.h"                    // the attested Debug sites
#include "../../IO/BitStream.h"                  // the 42-bit header
#include "GhostObject.h"                         // GhostLessComp - the send order
#include "ClientConnection.h"                    // PacketStatus
#include "../Objects/IObject.h"                  // the flags bit-5 skip
#include "../Objects/Projectiles/TVMissileTrace.h"
#include "../../Scene/Object/PlayerManager.h"    // readData's action-3 arm
#include "../../IO/Network/INetworkable.h"       // m_ghostActive, m_basePriority
#include "../../IO/Network/NetworkManager.h"     // networkManager->getObject

#include <algorithm>                             // pop_heap - the send order
#include <cstdio>                                // [TEMP] control-state wire probe (RW-45)
#include "../../IO/Network/NetworkManager.h"     // NetworkableDescriptor
#include "../../IO/Network/INetworkable.h"       // the representative networkable
#include "../../IO/Network/INetworkableObject.h" // m_object - getGhostPrioVec's chain
#include "../../IO/Network/NetworkableInfo.h"    // getForceNetworkableId, vt +0x3C
#include "GhostObject.h"                     // the 84-byte per-ghost record
#include "../Objects/IPlayer.h"
#include "../Objects/ISoldier.h"              // getKit - writeControlObjectState's kit id
#include "../../Scene/Object/ObjectTypes/Game/IPlayerControlObject.h"   // getSoldier's walk
#include "../IGame.h"                         // game->queryInterface(IID_IGameClient) - the reader's tail
#include "../IGameClient.h"
#include "../../Core/System.h"                // WorldPref::getTickCount - the reader's profile residue
#include "../GameLogic/ActionBuffer.h"        // m_currentInputIndex - updatePlayerControlObject

#include "../Objects/IObjectTemplate.h"       // getId +0x44 / getName +0x2C / getClassID +0x10
#include "../Objects/ObjectLayoutAccess.h"    // the +0x14 template read
#include "../../Scene/Object/ObjectTypes/Base/ObjectTemplateManager.h"   // getTemplate(id) +0x60
#include "../../Core/ClassManager.h"          // getClassName +0x28 - debugShowActiveGhosts
#include "../../Core/SettingsRepostitory.h"   // GSDebugGhostManager, boolGet +0x50

#include <cstring>                            // memset - the 4 KB ghost table
#include <string>                             // debugShowActiveGhosts' two locals

namespace dice {
namespace hfe {

extern ClassManager* classManager;            // Core/Globals.cpp - the per-TU extern convention

// Initialize per-connection ghost queues and baseline storage. Ghost ids are seeded
// lazily on first assignment. State masks and the control-object id are deliberately
// left uninitialized until their owning paths set them.
// [UNRESOLVED] Demo updates use a 13-bit length field instead of 11; retain that
// wire width even though both modes have the same ghost-id capacity.
GhostManager::GhostManager(GhostManagerMode mode, int capacity)
    : m_refCount(1)                                      // +0x0010
    , m_error(0)                                         // +0x0018
    , m_ghostHash(4096)                                  // +0x001C  a power of two
    , m_ghostables(3011)                                 // +0x0028  the engine standard
    , m_updateActiveGhostVec(true)                       // +0x1038    seeded true
    , m_ghostIdPoolSeeded(false)                         // +0x1080
    , m_debugGhostManager(false)                         // +0x1081  overwritten from settings below
    , m_timeSeconds(0.0)                                  // +0x1084  `fldz` / `fstp qword`
    , m_tickCount(0)                                     // +0x108C
    , m_streamManagerId(-1)                              // +0x1090  the write-once latch
    , m_updateControlObject(false)                       // +0x1094
    , m_priorityFloor(-10000.0f)                         // +0x2114  symmetric with the ceiling
    , m_priorityCeil(10000.0f)                           // +0x2118
    , m_frameCount(-1)                                   // +0x2120
    , m_actionBufferEntries(-1)                          // +0x2124
    , m_lastSentActionBufferEntries(-1)                  // +0x2128
    , m_baseLineSlotThisPacket(-1)                       // +0x212C
    , m_baseLineIndex(-1)                                // +0x2130
    , m_baseLineData(0)                                  // +0x2134
    , m_currentBaseLineSlot(0)                           // +0x2138
    , m_deltasUntilBaseLine(0)                           // +0x213C
    , m_player(0)                                        // +0x2140
    // +0x2144 is a std::vector<unsigned short>, not three ints - the destructor's `>> 1` and
    //   `2 * n` deallocate settle it. It default-constructs empty, which is the ELF's three zero
    //   stores. And it is dead: nothing in the binary ever fills it. See GhostManager.h.
    , m_mode(static_cast<int>(mode))                     // +0x2160  stored and never read again
    , m_byte2164(false)                                  // +0x2164
    , m_capacity(capacity)                               // +0x2174
    , m_updateLengthBits(11)                                  // +0x2178  see below
{
    // NOT initialised, as shipped: m_dword0034, m_networkableStateMasks[] and
    //   m_controlObjectNetworkableId. See the header note - this is a finding, not an omission.

    // 1. the id-indexed ghost table. the ELF's alignment branch is the compiler's - see above.
    std::memset(m_ghostObjects, 0, sizeof(m_ghostObjects));   //  `rep stosd`, 4,096 B

    // Initialize both empty priority heaps with their distinct comparators.
    std::make_heap(m_ghostPrioHeap.begin(), m_ghostPrioHeap.end(), GhostLessComp());
    std::make_heap(m_ghostPrioVec.begin(),  m_ghostPrioVec.end(),  GhostPrioLessComp());
    //    The two `resize(0)` calls the ELF makes here ARE emitted (the erase-or-_M_fill_insert
    //    pair at 0x8093921 and just after). They are no-ops on a freshly built vector, but the
    //    destructor makes the same call on m_ghostPrioHeap and it is written there, so writing it
    //    here too keeps one idiom reading the same way in both places.
    m_activeGhostVec.resize(0);
    m_ghostPrioHeap.resize(0);

    // 3. THE ghost-ID width. Eleven bits normally, thirteen for a demo - see the note above.
    if (mode)
        m_updateLengthBits = 13;

    // 4. the debug switch, re-read from settings on every construction. boolGet is vtable +0x50,
    //    which is exactly the `call dword ptr [ecx+50h]` at 0x8093899.
    const bool debugDefault = false;                     // [ebp+var_19F], zeroed at 0x809388A
    m_debugGhostManager = settings->boolGet(std::string("GSDebugGhostManager"), debugDefault);

    // 5. the base-line buffer. 0x2010 = 8,208 bytes - larger than the object that owns it, which
    //    is the shape of a per-frame snapshot arena rather than a struct.
    m_baseLineData = new unsigned char[0x2010];
    // `if (!m_baseLineData)` + Debug(6, "Game/Common/GhostManager.cpp", 174)
    //   << "Failed to allocate baseLine date"   [sic] - unreachable; operator new[] throws.
}

// Removes ghosts for reset or destruction and marks the active vector dirty.
// Drains resend entries one at a time, rechecking the queue after each pop.
void GhostManager::reset()
{
    m_updateActiveGhostVec = true;                       // the vector is stale again

    while (!m_resendQueue.empty())                       // drained, not cleared - see above
        m_resendQueue.pop_front();

    m_ghostPrioHeap.resize(0);                           // the erase-or-_M_fill_insert pair, n = 0

    resetGhostStates(true);                              // the mark phase, full
    removeOldGhosts(true);                               // the sweep, forced
}

// Resets ghosts and frees every buffer through the owning all-save-buffers list.
// The free-buffer pool contains borrowed duplicates and must not delete its entries.
GhostManager::~GhostManager()
{
    reset();                                             // reaches two unwritten frontiers

    delete[] m_baseLineData;                             // operator delete[]; the ELF's null test
                                                         //   is delete's own, not a source `if`

    m_ghostPrioHeap.resize(0);                           // redundant - reset() just did this

    // the list owns every buffer; the deque only borrows. See the note above.
    while (!m_allSaveBuffers.empty())
    {
        delete m_allSaveBuffers.front();                 // ~vector + operator delete
        m_allSaveBuffers.erase(m_allSaveBuffers.begin());
    }
}

// The stream-manager ID is write-once; repeated registration retains the first ID.
int GhostManager::getStreamManagerId() const { return m_streamManagerId; }   // +0x1090

void GhostManager::setStreamManagerId(int id)
{
    if (m_streamManagerId == -1)                         // write-once, 0x809BED6
        m_streamManagerId = id;
}

// Lazily rebuilds the active list by scanning all 1024 slots when dirty.
// resize(0) retains vector capacity between rebuilds.
void GhostManager::updateActiveGhostVec()
{
    if (!m_updateActiveGhostVec)
        return;

    m_activeGhostVec.resize(0);                          // keeps the capacity - see above

    for (int i = 0; i <= 1023; ++i)                      // the literal bound, not a member
    {
        GhostObject* const ghost = m_ghostObjects[i];
        if (ghost)
            m_activeGhostVec.push_back(ghost);
    }

    m_updateActiveGhostVec = false;
}

// Removes only the hash node. removeOldGhosts owns deletion of the GhostObject.
void GhostManager::removeGhostObjectFromHash(unsigned short netId)
{
    m_ghostHash.remove(netId);                           // frees the node only - see above
}

// Rebuilds the active vector, marks vanished ghostables for removal at maximum priority,
// and reprices surviving ghosts. A full reset also fills their bookkeeping masks with all ones.
void GhostManager::resetGhostStates(bool full)
{
    updateActiveGhostVec();                              // before anything else

    for (std::vector<GhostObject*>::iterator it = m_activeGhostVec.begin();
         it != m_activeGhostVec.end(); ++it)
    {
        GhostObject* const ghost = *it;

        if (!ghost->m_isStaticGhost)                     // byte [edx+40h]
        {
            ghost->m_ghostableState = 3;                 // gone - and see the ceiling below
            ghost->m_priority = 10000.0f;                // flt_888BBE0, the priority ceiling
        }
        else
        {
            ghost->m_priority = 255.0f;                  // flt_888BBE4

            if (full)
            {
                // the ELF unrolls this into twelve stores, 0x809520B..0x809525B.
                for (int i = 0; i < 12; ++i)
                    ghost->m_stateMask[i] = -1;
            }
        }
    }
}

// Removes state-3 ghosts when forced or their countdown expires; NaN does not expire.
// Returns IDs and removes lookups before deletion, invalidating the active-vector cache.
// Deletes the current table entry rather than trusting the possibly stale vector pointer.
void GhostManager::removeOldGhosts(bool force)
{
    for (std::vector<GhostObject*>::size_type i = 0; i < m_activeGhostVec.size(); ++i)
    {
        GhostObject* const ghost = m_activeGhostVec[i];

        if (ghost->m_ghostableState != 3)                // only the gone ones
            continue;
        if (!force && !(ghost->m_accumulatedPriority <= 0.0f))  // NaN never removes
            continue;

        // the table's entry, not the loop variable - see the note above.
        GhostObject* const stored = m_ghostObjects[ghost->m_ghostId];
        if (!stored)
            continue;

        m_ghostIdPool.push_back(stored->m_ghostId);      // 1. the id can be reissued
        removeGhostObjectFromHash(stored->m_networkableId);  // 2. unhook the engine-id lookup
        m_updateActiveGhostVec = true;                   // 3. the active vector is stale now
        m_ghostObjects[stored->m_ghostId] = 0;           // 4. the slot is free
        delete stored;                                   // 5. and only now the memory
    }

    updateActiveGhostVec();                              // rebuild once, at the end
}

// Takes an id from the lazily populated pool of 1,023 ids. Exhaustion refreshes the
// active ghost vector and emits a fatal diagnostic; it cannot return an invalid id.
unsigned short GhostManager::assignGhostId()
{
    if (!m_ghostIdPoolSeeded)
    {
        m_ghostIdPoolSeeded = true;
        for (unsigned short id = 0; id <= 0x3FE; ++id)   // 0..1022 - one thousand and twenty-three
            m_ghostIdPool.push_back(id);
    }

    if (m_ghostIdPool.empty())
    {
        updateActiveGhostVec();
        Debug(DEBUGTYPE_7, "Game/Common/GhostManager.cpp", 245)
            << "Out of ghost ids, max:" << 1023;
        // unreachable past here - type 7 folds to 6 and aborts. See above.
    }

    const unsigned short id = m_ghostIdPool.front();
    m_ghostIdPool.pop_front();
    return id;
}

// Returns the ghost for a registered network ID. ID zero is never probed.
GhostObject* GhostManager::findGhostObjectInHash(unsigned short netId)
{
    GhostObject* found = 0;
    if (netId != 0 && m_ghostHash.get(netId, found))     // zero is never a valid key
        return found;
    return 0;
}

// Merges fresh descriptor state into ghosts already scoped to this connection.
// Looks up each ghost through the descriptor's first networkable id, not its
// descriptor id. Unscoped objects are skipped.
void GhostManager::updateStateMasks(
    const std::vector<std::pair<const io::NetworkableDescriptor*, io::NetworkableStateMask> >& updated)
{
    typedef std::vector<std::pair<const io::NetworkableDescriptor*, io::NetworkableStateMask> > Vec;
    for (Vec::const_iterator it = updated.begin(); it != updated.end(); ++it)
    {
        const io::NetworkableDescriptor* const desc = it->first;
        // desc +0x0C properties -> [0] -> networkables -> [0] -> INetworkable +0x04 m_networkableId
        const unsigned short netId =
            desc->properties[0].networkables[0]->m_networkableId;

        GhostObject* const ghost = findGhostObjectInHash(netId);
        if (ghost)
        {
            // ghost->m_stateMask[0..11] IS an io::NetworkableStateMask by value (48 B) at +0x0C; the
            // pair's second is the fresh mask; operation 1 folds it in.
            reinterpret_cast<io::NetworkableStateMask*>(&ghost->m_stateMask[0])
                ->doOperation(it->second, static_cast<io::StateMaskOp>(1));   // op 1 = OR
        }
    }
}

// Inserts only absent IDs, computing the bucket once for both lookup and insertion.
void GhostManager::addGhostObjectToHash(unsigned short netId, GhostObject* ghost)
{
    const unsigned int bucket = netId % m_ghostHash.m_maxSize;   // hashed once - see above
    if (!m_ghostHash.exists(netId, bucket))
        m_ghostHash.insert(netId, ghost, bucket);
}

// Create or refresh a scoped ghost using the first networkable of the first
// descriptor property; both vectors must be nonempty. Negative priority makes
// a static ghost. keepHigher merges priorities except when the old value is
// at the ceiling, which accepts a lower replacement. NaN remains unclamped.
void GhostManager::ghostObject(io::NetworkableDescriptor* descriptor, float priority, bool keepHigher)
{
    // the representative networkable's id, not descriptor->descId - see above.
    // no emptiness check on either vector, exactly as shipped.
    io::INetworkable* const networkable = descriptor->properties[0].networkables[0];
    const unsigned short netId = networkable->m_networkableId;

    if (priority >= 10000.0f)                            // the `>=` spelling is deliberate - NaN
        priority = 10000.0f;                             //   passes through, as it does in the ELF

    GhostObject* const existing = findGhostObjectInHash(netId);
    if (existing)
    {
        if (descriptor != existing->m_descriptor)
        {
            Debug(DEBUGTYPE_8, "Game/Common/GhostManager.cpp", 426)
                << "Replacing descriptor for ghost (Removal of a ghost and adding a new with the "
                   "same id during the same update may trigger this): " << netId;
            existing->m_descriptor = descriptor;
            for (int i = 0; i < 12; ++i)
                existing->m_stateMask[i] = -1;
        }

        existing->m_ghostableState = 1;                  // THE revive - see the header note

        if (priority < 0.0f)                             // a static re-ghost blanks the block too
        {
            for (int i = 0; i < 12; ++i)
                existing->m_stateMask[i] = -1;
        }

        // the merge hole - see above. At the ceiling, keepHigher does nothing.
        if (keepHigher && existing->m_priority < 10000.0f)
        {
            if (existing->m_priority <= priority)
                existing->m_priority = priority;
        }
        else
        {
            existing->m_priority = priority;
        }
        return;
    }

    GhostObject* const ghost = new GhostObject;          // operator new(0x54) - NO parens: the ELF
                                                         //   leaves +0x02/+0x0A/+0x4C uninitialised
    ghost->m_byte51         = false;                     // before anything else
    ghost->m_ghostId        = assignGhostId();           // `mov [edi], ax` - sixteen bits
    ghost->m_ghostableState = 1;
    ghost->m_priority       = 10000.0f;                  // born at the ceiling
    ghost->m_networkableId  = netId;
    ghost->m_accumulatedPriority = 0.0f;                 // starts unstarved
    for (int i = 0; i < 12; ++i)
        ghost->m_stateMask[i] = -1;
    ghost->m_descriptor     = descriptor;
    ghost->m_ghostablePending = true;
    ghost->m_isStaticGhost  = (priority < 0.0f);         // the sweep exemption - see above

    // Replicates attached networkables that are scoped or explicitly forced.
    if (networkable->m_networkableObject)                                 // INetworkable +0x18
    {
        if (!m_ghostables.exists(netId))                                  // the +0x28 set
        {
            io::INetworkableInfo* const info = networkable->m_networkableInfo;   // +0x08
            if (!info || !info->getForceNetworkableId())                  // vtable +0x3C
                ghost->m_ghostablePending = false;
        }
    }

    if (m_ghostObjects[ghost->m_ghostId])                // a real invariant, and type 6 aborts
    {
        Debug(DEBUGTYPE_6, "Game/Common/GhostManager.cpp", 477)
            << "GhostId already assigned " << ghost->m_ghostId;
    }

    m_ghostObjects[ghost->m_ghostId] = ghost;            // unreachable if the check above fired
    m_updateActiveGhostVec = true;
    addGhostObjectToHash(netId, ghost);
}

// Marks every field dirty for an existing ghost. The descriptor must contain its
// representative networkable; an unscoped ID is ignored.
void GhostManager::resetGhost(io::NetworkableDescriptor* descriptor)
{
    const unsigned short netId =
        descriptor->properties[0].networkables[0]->m_networkableId;   // unchecked, as shipped

    GhostObject* const ghost = findGhostObjectInHash(netId);
    if (ghost)
    {
        for (int i = 0; i < 12; ++i)                     // twelve stores in the ELF
            ghost->m_stateMask[i] = -1;
    }
}

// Marks the ghost for removal with maximum priority; the later sweep owns deletion.
// The descriptor overload ignores unscoped IDs.
void GhostManager::removeGhostObject(io::NetworkableDescriptor* descriptor)
{
    const unsigned short netId =
        descriptor->properties[0].networkables[0]->m_networkableId;   // unchecked, as shipped

    GhostObject* const ghost = findGhostObjectInHash(netId);
    if (ghost)
        removeGhostObject(ghost);
}

void GhostManager::removeGhostObject(GhostObject* ghost)
{
    ghost->m_ghostableState = 3;                         // marked, not freed - see above
    ghost->m_priority       = 10000.0f;                  // dying at the ceiling
}

// Builds the send heap from dying or dirty ghosts, accumulating priority until each
// successful send. Captures the tick used by subsequent packets and their timebase.
void GhostManager::update(unsigned int tickCount)
{
    m_ghostPrioHeap.resize(0);                           // the erase-or-_M_fill_insert pair, n = 0

    m_tickCount   = tickCount;                           // +0x108C - the 42-bit header's field
    m_timeSeconds = static_cast<double>(tickCount) / 30.0;   // +0x1084 - the same instant in seconds

    updateActiveGhostVec();

    for (std::vector<GhostObject*>::iterator it = m_activeGhostVec.begin();
         it != m_activeGhostVec.end(); ++it)
    {
        GhostObject* const ghost = *it;

        const int* const m = ghost->m_stateMask;
        const bool hasSomethingToSay =
            ghost->m_ghostableState != 1                 // dying counts as news
            || (m[0] | m[1] | m[2] | m[3] | m[4] | m[5]
              | m[6] | m[7] | m[8] | m[9] | m[10] | m[11]) != 0;

        if (hasSomethingToSay && ghost->m_ghostablePending)   // +0x50 - the announce gate
        {
            ghost->m_accumulatedPriority += ghost->m_priority;   // the starvation integral
            m_ghostPrioHeap.push_back(ghost);
            std::push_heap(m_ghostPrioHeap.begin(), m_ghostPrioHeap.end(), GhostLessComp());
        }
    }
}

// Captures this frame's control-object IDs and input-buffer depth for transmission.
// Requires a vehicle and networkable; a changed control-object ID invalidates its baseline.
// Parachutes include the underlying soldier ID. Previously sent input depth is suppressed.
// Returns true after updating the state.
bool GhostManager::updatePlayerControlObject(world::IPlayer* player, int frameCount)
{
    m_updateControlObject = true;                        // +0x1094 - transmit's gate
    m_frameCount          = frameCount;                  // +0x2120
    m_actionBufferEntries = player->getActionBuffer()->m_currentInputIndex;   // +0x2124

    const unsigned short previousControlNetId = m_controlObjectNetworkableId;

    // the baseline dies with the old control object - see above.
    if (m_controlObjectNetworkableId
            != player->getVehicle()->getNetworkable()->m_networkableId)   // unchecked derefs
        m_baseLineIndex = -1;

    // fetched again - the second getVehicle()/getNetworkable() pair, as shipped.
    m_parachuteSoldierNetId = 0;                         // +0x211E - zeroed every frame
    m_controlObjectNetworkableId =
        player->getVehicle()->getNetworkable()->m_networkableId;          // +0x211C
    const bool controlObjectChanged =
        previousControlNetId != m_controlObjectNetworkableId;

    // the PCO walk, on a third fetch - and this one does null-check.
    world::IPlayerControlObject* pco = 0;
    for (world::IObject* o = player->getVehicle(); o; o = o->getParent())
    {
        pco = static_cast<world::IPlayerControlObject*>(
                  o->queryInterface(world::IID_IPlayerControlObject));
        if (pco)
            break;
    }

    if (pco)
    {
        // the parachute arm - the fourth fetch, unchecked again.
        world::IObject* const vehicle = player->getVehicle();
        if (world::object_detail::LayoutAccess::objectTemplate(vehicle)->getClassID()
                == world::CID_ParachuteTemplate)
        {
            world::IObject* const soldier = player->getDefaultVehicle();
            m_parachuteSoldierNetId = soldier->getNetworkable()->m_networkableId;
        }
    }

    // suppress an action-buffer depth the last packet already carried.
    if (m_lastSentActionBufferEntries != -1
        && m_actionBufferEntries == m_lastSentActionBufferEntries)
        m_actionBufferEntries = -1;

    return true;
}

// setCompressionVector - ELF 0x80961D0, 56 bytes: three dword stores into +0x2168. The reference
// point every compressed ghost position is expressed against; ServerConnection re-seats it on the
// player's camera each update.
void GhostManager::setCompressionVector(const Vec3& reference)
{
    m_compressionVector = reference;                     // +0x2168
}

// .data 0x8986344, compile-time 5. Five deltas ride each baseline before
// writeControlObjectState snapshots a fresh one into the eight-slot ring.
int GhostManager::m_numDeltasPerBaseLine = 5;

// Returns the object itself if it is a soldier, otherwise the active soldier
// occupying a control object without its own cameras. Null input returns null.
// Soldier interface calls use an adjusted typed pointer in this port.
world::IObject* getSoldier(world::IObject* object)
{
    if (!object)
        return 0;

    if (world::object_detail::LayoutAccess::objectTemplate(object)->getClassID()
            == world::CID_SoldierTemplate)
        return object;                                   // already the soldier

    world::IPlayerControlObject* pco = 0;
    for (world::IObject* o = object; o; o = o->getParent())
    {
        pco = static_cast<world::IPlayerControlObject*>(
                  o->queryInterface(world::IID_IPlayerControlObject));
        if (pco)
            break;
    }

    if (pco
        && pco->getOwnCameras()->empty()                 // the [+4]-as-empty() idiom, 5th site
        && pco->isChildSoldierActive())                  // +0x140
    {
        return pco->getOccupingObject();                 // +0x14
    }
    return 0;
}

// Write the controlled object's state and player synchronization block. Requires
// new action state and a registered ghostable control object. Delta updates use
// only acknowledged baselines; fresh baselines enter an eight-slot ring.
// Stream failures restore the whole saved position/length state and return false.
// Missing descriptors, failed baseline capture and oversize control blocks are fatal.
bool GhostManager::writeControlObjectState(io::BitStream* stream)
{
    if (m_actionBufferEntries == -1)
        return false;                                    // nothing new to say

    // the registered-ghostable gate - the two-step hash idiom, hashed once.
    {
        const unsigned int bucket = m_controlObjectNetworkableId % m_ghostables.m_maxSize;
        if (!m_ghostables.exists(m_controlObjectNetworkableId, bucket))
        {
            return false;
        }
    }

    // the full-restore save - the byte38 mechanism, as in transmit/sendData
    stream->m_byte38 = true;
    if (stream->m_readWriteSwitch == 1)
        stream->m_positionCache = stream->m_readBitPos;
    else
        stream->m_positionCache = stream->m_bitPos;
    stream->m_rwSwitchCache = stream->m_readWriteSwitch;
    stream->m_lengthCache   = stream->m_highWaterBits;

    // the outer 12-bit length reserve
    io::BSPosition outerMark;
    if (stream->m_readWriteSwitch == 1)
    {
        outerMark.m_readWriteSwitch = 1;
        outerMark.m_position        = stream->m_readBitPos;
    }
    else
    {
        outerMark.m_readWriteSwitch = stream->m_readWriteSwitch;
        outerMark.m_position        = stream->m_bitPos;
    }
    stream->skipBits(12);
    const unsigned int payloadStart =
        (stream->m_readWriteSwitch == 1) ? stream->m_readBitPos : stream->m_bitPos;

    // actionBufferEntries: sign bit + 31-bit magnitude
    if (m_actionBufferEntries < 0)
    {
        const unsigned int one = 1;
        stream->writeBits(&one, 1);
        const unsigned int magnitude = static_cast<unsigned int>(-m_actionBufferEntries);
        stream->writeBits(&magnitude, 31);
    }
    else
    {
        const unsigned int zero = 0;
        stream->writeBits(&zero, 1);
        const unsigned int magnitude = static_cast<unsigned int>(m_actionBufferEntries);
        stream->writeBits(&magnitude, 31);
    }

    io::NetworkableDescriptor* const controlDescriptor =
        io::networkManager->getDescriptor(m_controlObjectNetworkableId);
    if (!controlDescriptor)
    {
        Debug(DEBUGTYPE_6, "Game/Common/GhostManager.cpp", 979)
            << "Control object not found";
        // type 6 aborts; the dereference below is unreachable on this arm, as shipped.
    }

    // the compression vector, armed for everything that follows
    stream->resetCompressionVector();
    if (controlDescriptor->properties[0].networkables[0]->m_networkableObject)
    {
        stream->writeBits(&m_compressionVector.x, 32);   // three RAW floats
        stream->writeBits(&m_compressionVector.y, 32);
        stream->writeBits(&m_compressionVector.z, 32);
        stream->setCompressionVector(m_compressionVector);
    }

    const unsigned short controlNetId = m_controlObjectNetworkableId;
    stream->writeBits(&controlNetId, 16);

    // the parachute soldier, when updatePlayerControlObject latched one
    const bool hasParachuteSoldier = m_parachuteSoldierNetId != 0;
    {
        const unsigned int flag = hasParachuteSoldier ? 1u : 0u;
        stream->writeBits(&flag, 1);
    }
    if (hasParachuteSoldier)
    {
        const unsigned short soldierNetId = m_parachuteSoldierNetId;
        stream->writeBits(&soldierNetId, 16);
    }

    // the soldier and its kit
    world::IObject* object = io::networkManager->getObject(m_controlObjectNetworkableId);
    world::IObject* const soldier = getSoldier(object);
    {
        const unsigned int flag = soldier ? 1u : 0u;
        stream->writeBits(&flag, 1);
    }
    if (soldier)
    {
        object = soldier;                                // getStateSync targets the soldier

        // the ELF reads the kit through the soldier's primary vtable (+0x2A0); this port takes
        // the IID_ISoldier seam - see getSoldier's header note.
        world::ISoldier* const soldierInterface = static_cast<world::ISoldier*>(
            soldier->queryInterface(world::IID_ISoldier));
        world::IObject* const kit =
            reinterpret_cast<world::IObject*>(soldierInterface->getKit());
        if (kit)
        {
            const unsigned int flag = 1;
            stream->writeBits(&flag, 1);
            const unsigned short kitNetId = kit->getNetworkable()->m_networkableId;
            stream->writeBits(&kitNetId, 16);
        }
        else
        {
            const unsigned int flag = 0;
            stream->writeBits(&flag, 1);
        }
    }

    // delta against the acknowledged baseline, or a fresh one
    if (m_baseLineIndex != -1 && m_deltasUntilBaseLine > 0)
    {
        const unsigned int flag = 1;
        stream->writeBits(&flag, 1);
        const unsigned int ackedSlot = static_cast<unsigned int>(m_baseLineIndex);
        stream->writeBits(&ackedSlot, 3);

        io::BaseLineData& baseLine = *reinterpret_cast<io::BaseLineData*>(
            m_baseLineData + 1026 * m_baseLineIndex);

        bool failed = !io::networkManager->getNetUpdateDelta(
            *stream, io::networkManager->getDescriptor(m_controlObjectNetworkableId),
            baseLine, 0, io::StatePrecisionFull, false, true);   // cursor rewound
        if (!failed && hasParachuteSoldier)
        {
            failed = !io::networkManager->getNetUpdateDelta(
                *stream, io::networkManager->getDescriptor(m_parachuteSoldierNetId),
                baseLine, 0, io::StatePrecisionFull, false, false);   // cursor continues
        }
        if (failed)
        {
            // the full restore - see the header note.
            stream->setPosition(*reinterpret_cast<io::BSPosition*>(&stream->m_dword3C));
            stream->m_byte38        = false;
            stream->m_errorCode     = 0;
            stream->m_error         = false;
            stream->m_highWaterBits = stream->m_lengthCache;
            stream->resetCompressionVector();
            return false;
        }

        --m_deltasUntilBaseLine;
    }
    else
    {
        const unsigned int flag = 0;
        stream->writeBits(&flag, 1);

        const int slot = (m_currentBaseLineSlot + 1) % 8;   // the eight-slot ring
        io::BaseLineData& baseLine = *reinterpret_cast<io::BaseLineData*>(
            m_baseLineData + 1026 * slot);

        // snapshot first - a refusal here is fatal, not a rollback.
        if (!io::networkManager->getBaseLine(
                baseLine, io::networkManager->getDescriptor(m_controlObjectNetworkableId), true))
        {
            stream->setPosition(*reinterpret_cast<io::BSPosition*>(&stream->m_dword3C));
            stream->m_byte38        = false;
            stream->m_errorCode     = 0;
            stream->m_error         = false;
            stream->m_highWaterBits = stream->m_lengthCache;
            Debug(DEBUGTYPE_6, "Game/Common/GhostManager.cpp", 1045)
                << "Failed to get baseLine";
            // type 6 aborts; unreachable past here, as shipped.
            stream->resetCompressionVector();
            return false;
        }
        if (hasParachuteSoldier
            && !io::networkManager->getBaseLine(
                   baseLine, io::networkManager->getDescriptor(m_parachuteSoldierNetId), false))
        {
            stream->setPosition(*reinterpret_cast<io::BSPosition*>(&stream->m_dword3C));
            stream->m_byte38        = false;
            stream->m_errorCode     = 0;
            stream->m_error         = false;
            stream->m_highWaterBits = stream->m_lengthCache;
            Debug(DEBUGTYPE_6, "Game/Common/GhostManager.cpp", 1055)
                << "Failed to get baseLine";
            stream->resetCompressionVector();
            return false;
        }

        const unsigned int newSlot = static_cast<unsigned int>(slot);
        stream->writeBits(&newSlot, 3);

        bool failed = !io::networkManager->getNetUpdate(
            *stream, io::networkManager->getDescriptor(m_controlObjectNetworkableId),
            0, io::StatePrecisionFull, false);           // full state, no mask
        if (!failed && hasParachuteSoldier)
        {
            failed = !io::networkManager->getNetUpdate(
                *stream, io::networkManager->getDescriptor(m_parachuteSoldierNetId),
                0, io::StatePrecisionFull, false);
        }
        if (failed)
        {
            stream->setPosition(*reinterpret_cast<io::BSPosition*>(&stream->m_dword3C));
            stream->m_byte38        = false;
            stream->m_errorCode     = 0;
            stream->m_error         = false;
            stream->m_highWaterBits = stream->m_lengthCache;
            stream->resetCompressionVector();
            return false;
        }

        // the ledger advances - see the header note.
        m_currentBaseLineSlot    = slot;
        m_baseLineSlotThisPacket = slot;
        m_deltasUntilBaseLine    = m_numDeltasPerBaseLine;
    }

    // the 10-bit-wrapped Player::getStateSync block
    io::BSPosition syncMark;
    if (stream->m_readWriteSwitch == 1)
    {
        syncMark.m_readWriteSwitch = 1;
        syncMark.m_position        = stream->m_readBitPos;
    }
    else
    {
        syncMark.m_readWriteSwitch = stream->m_readWriteSwitch;
        syncMark.m_position        = stream->m_bitPos;
    }
    stream->skipBits(10);

    const unsigned int syncStart =
        (stream->m_readWriteSwitch == 1) ? stream->m_readBitPos : stream->m_bitPos;
    m_player->getStateSync(stream, object);              // result discarded, as shipped
    const unsigned int syncEnd =
        (stream->m_readWriteSwitch == 1) ? stream->m_readBitPos : stream->m_bitPos;

    const unsigned int syncBits = syncEnd - syncStart;
    {
        stream->setWritePosition(syncMark);
        stream->writeBits(&syncBits, 10);
    }
    if (stream->m_canResetWritePosition)
    {
        stream->m_canResetWritePosition = false;
        stream->setPosition(*reinterpret_cast<io::BSPosition*>(&stream->m_dword24));
    }
    else
    {
        Debug(DEBUGTYPE_6, "IO/BitStream.inl", 341)
            << "Call to resetWritePosition without prior call to setPosition";
    }

    // the outer 12-bit length backfill
    const unsigned int payloadEnd =
        (stream->m_readWriteSwitch == 1) ? stream->m_readBitPos : stream->m_bitPos;
    const unsigned int totalBits = payloadEnd - payloadStart;
    stream->setWritePosition(outerMark);
    if (totalBits > 0xFFF)
    {
        Debug(DEBUGTYPE_6, "Game/Common/GhostManager.cpp", 1152)
            << "control object size is to big!!! " << totalBits;   // [sic]
    }
    stream->writeBits(&totalBits, 12);
    if (stream->m_canResetWritePosition)
    {
        stream->m_canResetWritePosition = false;
        stream->setPosition(*reinterpret_cast<io::BSPosition*>(&stream->m_dword24));
    }
    else
    {
        Debug(DEBUGTYPE_6, "IO/BitStream.inl", 341)
            << "Call to resetWritePosition without prior call to setPosition";
    }

    // overrun: restore everything and report no control block
    if (stream->m_error)
    {
        stream->m_errorCode = 0;
        stream->m_error     = false;
        stream->setPosition(*reinterpret_cast<io::BSPosition*>(&stream->m_dword3C));
        stream->m_byte38        = false;
        stream->m_errorCode     = 0;
        stream->m_error         = false;
        stream->m_highWaterBits = stream->m_lengthCache;
        stream->resetCompressionVector();
        return false;
    }

    stream->m_byte38 = false;
    return true;
}

// Reads length-delimited control state, applying a baseline or delta and optional state sync.
// Unknown objects skip the outer block; kit mismatches skip only the incompatible state-sync data.
// Known objects must consume the declared length exactly. Client calls also replay prediction.
// A failed Player::setStateSync can clear the stream error before this method returns false;
// processReceivedPacket checks the latch rather than this return value, preserving that behavior.
bool GhostManager::readControlObjectState(io::BitStream* stream)
{
    unsigned int size = 0;
    stream->readBits(&size, 12);                         // the outer wrapper

    unsigned int startPos;
    int startMode;
    if (stream->m_readWriteSwitch == 1)
    {
        startPos  = stream->m_readBitPos;
        startMode = 1;
    }
    else
    {
        startPos  = stream->m_bitPos;
        startMode = stream->m_readWriteSwitch;
    }

    // the signed action-buffer count
    int actionBufferEntries;
    {
        unsigned int negative = 0;
        stream->readBits(&negative, 1);
        unsigned int magnitude = 0;
        stream->readBits(&magnitude, 31);
        actionBufferEntries = (negative == 1) ? -static_cast<int>(magnitude)
                                              : static_cast<int>(magnitude);
    }

    // the compression vector - three raw floats, then armed
    stream->resetCompressionVector();
    Vec3 compression;
    std::memset(&compression, 0, sizeof(compression));
    stream->readBits(&compression.x, 32);
    stream->readBits(&compression.y, 32);
    stream->readBits(&compression.z, 32);
    stream->setCompressionVector(compression);

    unsigned short controlNetId = 0;
    stream->readBits(&controlNetId, 16);

    unsigned int hasParachuteSoldier = 0;
    stream->readBits(&hasParachuteSoldier, 1);
    unsigned short parachuteNetId = 0;
    if (hasParachuteSoldier == 1)
        stream->readBits(&parachuteNetId, 16);

    world::IObject* const object = io::networkManager->getObject(controlNetId);

    unsigned int isSoldier = 0;
    stream->readBits(&isSoldier, 1);

    unsigned short kitNetId = 0;
    world::IObject* const soldier = getSoldier(object);
    world::IObject* target = object;                     // what setStateSync will be aimed at
    if (isSoldier == 1)
    {
        target = soldier;                                // may be null if we disagree - gated below
        unsigned int hasKit = 0;
        stream->readBits(&hasKit, 1);
        if (hasKit == 1)
            stream->readBits(&kitNetId, 16);
    }

    if (stream->m_error)
    {
        stream->m_errorCode = 0;
        stream->m_error     = false;
        return false;
    }

    world::IObject* parachuteObject = 0;
    if (hasParachuteSoldier == 1)
        parachuteObject = io::networkManager->getObject(parachuteNetId);

    // the kit consistency gate - see the header note
    bool stateSyncReadable = true;
    if (isSoldier == 1)
    {
        world::IObject* kit = 0;
        if (soldier)
        {
            world::ISoldier* const soldierInterface = static_cast<world::ISoldier*>(
                soldier->queryInterface(world::IID_ISoldier));   // the +0x2A0 seam - see getSoldier
            kit = reinterpret_cast<world::IObject*>(soldierInterface->getKit());
        }
        if (!soldier || !kit || !kitNetId
            || kit->getNetworkable()->m_networkableId != kitNetId)
        {
            stateSyncReadable = false;
        }
    }

    if (object)
    {
        // delta against a shared slot, or a fresh baseline to apply-and-snapshot
        unsigned int isDelta = 0;
        stream->readBits(&isDelta, 1);
        if (isDelta != 1)
        {
            // fresh baseline: apply full state, then snapshot our own copy into the named slot.
            unsigned int slot = 0;
            stream->readBits(&slot, 3);
            if (stream->m_error)
            {
                stream->m_errorCode = 0;
                stream->m_error     = false;
                return false;
            }
            if (slot > 7)                                // dead - 3 bits cannot exceed 7
            {
                Debug(DEBUGTYPE_6, "Game/Common/GhostManager.cpp", 1324)
                    << "Invalid server baseLineIndex";
            }

            io::networkManager->setNetUpdate(
                *stream, io::networkManager->getDescriptor(controlNetId),
                static_cast<float>(1000.0 * m_timeSeconds), true, io::StatePrecisionFull);
            if (parachuteObject)
            {
                io::networkManager->setNetUpdate(
                    *stream, io::networkManager->getDescriptor(parachuteNetId),
                    static_cast<float>(1000.0 * m_timeSeconds), true, io::StatePrecisionFull);
            }

            io::BaseLineData& baseLine = *reinterpret_cast<io::BaseLineData*>(
                m_baseLineData + 1026 * slot);
            if (!io::networkManager->getBaseLine(
                    baseLine, io::networkManager->getDescriptor(controlNetId), true))
            {
                Debug(DEBUGTYPE_6, "Game/Common/GhostManager.cpp", 1335)
                    << "Failed to get baseLine";
                return false;                            // unreachable past the abort, as shipped
            }
            if (parachuteObject
                && !io::networkManager->getBaseLine(
                       baseLine, io::networkManager->getDescriptor(parachuteNetId), false))
            {
                Debug(DEBUGTYPE_6, "Game/Common/GhostManager.cpp", 1343)
                    << "Failed to get baseLine";
                return false;
            }
        }
        else
        {
            // delta: decompress against the slot both ends hold.
            unsigned int slot = 0;
            stream->readBits(&slot, 3);
            if (stream->m_error)
            {
                stream->m_errorCode = 0;
                stream->m_error     = false;
                return false;
            }

            io::BaseLineData& baseLine = *reinterpret_cast<io::BaseLineData*>(
                m_baseLineData + 1026 * slot);
            io::networkManager->setNetUpdateDelta(
                *stream, io::networkManager->getDescriptor(controlNetId), baseLine,
                static_cast<float>(1000.0 * m_timeSeconds), true, io::StatePrecisionFull, true);
            if (parachuteObject)
            {
                io::networkManager->setNetUpdateDelta(
                    *stream, io::networkManager->getDescriptor(parachuteNetId), baseLine,
                    static_cast<float>(1000.0 * m_timeSeconds), true, io::StatePrecisionFull,
                    false);
            }
        }

        // the 10-bit-wrapped state sync - read it or step over it
        unsigned int syncBits = 0;
        stream->readBits(&syncBits, 10);
        const unsigned int syncStart =
            (stream->m_readWriteSwitch == 1) ? stream->m_readBitPos : stream->m_bitPos;

        if (!stateSyncReadable)
        {
            stream->skipBits(syncBits);                  // the resilience the length buys
        }
        else
        {
            if (!m_player->setStateSync(stream, target, 1000.0 * m_timeSeconds))
                return false;                            // callee already cleared the latch - see above

            const unsigned int syncEnd =
                (stream->m_readWriteSwitch == 1) ? stream->m_readBitPos : stream->m_bitPos;
            if (syncEnd - syncStart != syncBits)
            {
                Debug(DEBUGTYPE_6, "Game/Common/GhostManager.cpp", 1363)
                    << "state bits differs read:" << (syncEnd - syncStart)
                    << " expected:" << syncBits;
            }
        }

        if (stream->m_error)
        {
            stream->m_errorCode = 0;
            stream->m_error     = false;
            return false;
        }
    }

    // the tail: the client's prediction replay, then the size-verified reposition
    IGameClient* const gameClient = static_cast<IGameClient*>(game->queryInterface(IID_IGameClient));
    if (object && gameClient)
    {
        // client-only - a dedicated server answers null above. The two discarded tick reads
        // bracket the replay in the ELF; profile residue, kept because they are real calls.
        (void)WorldPref::getTickCount();
        if (!gameClient->handleControlObjectState(stream, controlNetId,
                                                  actionBufferEntries, m_tickCount, 0.033333335f))
            return false;
        (void)WorldPref::getTickCount();
    }

    const unsigned int endPos =
        (stream->m_readWriteSwitch == 1) ? stream->m_readBitPos : stream->m_bitPos;
    const unsigned int bitsRead = endPos - startPos;

    if (object && bitsRead != size)
    {
        Debug(DEBUGTYPE_6, "Game/Common/GhostManager.cpp", 1406)
            << "Size differs bits read " << bitsRead << " indicated size " << size;
        return false;                                    // unreachable past the abort, as shipped
    }

    // the cursor is forced to start + size either way - see the header note.
    io::BSPosition endMark;
    endMark.m_readWriteSwitch = startMode;
    endMark.m_position        = startPos + size;
    stream->setPosition(endMark);
    return true;
}

// Rebuilds and returns the member max-heap of live, non-static ghosts, ordered
// by raw priority. Missing links in the object/template chain produce template ID 0.
std::vector<PrioInfo>* GhostManager::getGhostPrioVec()
{
    m_ghostPrioVec.resize(0);                            // the erase-or-_M_fill_insert pair, n = 0

    updateActiveGhostVec();

    for (std::vector<GhostObject*>::iterator it = m_activeGhostVec.begin();
         it != m_activeGhostVec.end(); ++it)
    {
        GhostObject* const ghost = *it;
        if (ghost->m_isStaticGhost || ghost->m_ghostableState != 1)
            continue;                                    // live, non-static only

        // the defensive chain - see above.
        int templateId = 0;
        io::INetworkableObject* const objEntry =
            ghost->m_descriptor->properties[0].networkables[0]->m_networkableObject;
        world::IObject* const object = objEntry ? objEntry->m_object : 0;
        if (object)
        {
            const world::IObjectTemplate* const templ =
                world::object_detail::LayoutAccess::objectTemplate(object);
            if (templ)
                templateId = static_cast<int>(templ->getId());   // vtable +0x44
        }

        PrioInfo info;
        info.m_templateId = templateId;
        info.m_priority   = ghost->m_priority;           // +0x44 raw - NOT the accumulator
        m_ghostPrioVec.push_back(info);
        std::push_heap(m_ghostPrioVec.begin(), m_ghostPrioVec.end(), GhostPrioLessComp());
    }

    return &m_ghostPrioVec;
}

// renderPriorities - ELF 0x8091100, five bytes, empty. The on-screen half of the priority debug
// view, compiled out of the dedicated server. Reproduced as the empty body it is.
void GhostManager::renderPriorities()
{
}

// Resolves template and leaf class names for active ghosts. Dedicated rendering
// is absent, but these lookups and string operations still run.
void GhostManager::debugShowActiveGhosts()
{
    std::vector<PrioInfo>* const prio = getGhostPrioVec();

    for (std::vector<PrioInfo>::iterator it = prio->begin(); it != prio->end(); ++it)
    {
        world::IObjectTemplate* const templ =
            world::objectTemplateManager->getTemplate(
                static_cast<unsigned int>(it->m_templateId));    // vtable +0x60

        std::string name("(invalid template id)");
        std::string className("(unknown)");
        if (templ)
        {
            name = templ->getName();
            classManager->getClassName(templ->getClassID(), className);   // vtable +0x28

            const std::string::size_type dot = className.rfind('.');
            if (dot != std::string::npos)
                className.erase(0, dot + 1);             // strip to the leaf - see above
        }
        // the render that consumed `name`/`className` is compiled out - see above.
    }
}

// Register a network id and mark its ghost announcement pending. Removing
// membership does not clear that flag; senders consume it. Both operations
// invalidate the active-ghost vector even if membership did not change.
void GhostManager::addGhostable(unsigned short netId)
{
    const unsigned int bucket = netId % m_ghostables.m_maxSize;   // hashed once - the two-step form
    if (!m_ghostables.exists(netId, bucket))                      // out of line here, 0x8098F43
        m_ghostables.insert(netId, true, bucket);                 // the value is always true

    GhostObject* const ghost = findGhostObjectInHash(netId);
    if (ghost)
        ghost->m_ghostablePending = true;

    m_updateActiveGhostVec = true;

}

void GhostManager::removeGhostable(unsigned short netId)
{
    m_ghostables.remove(netId);          // inlined in the ELF; frees the node only
    // m_ghostablePending is deliberately NOT cleared here - see the note above.
    m_updateActiveGhostVec = true;       // set even when the key was not present
}

// Allocates an empty save buffer with requested capacity using resize(n), then
// resize(0). Freed buffers return to the pool.
std::vector<GhostManager::GhostSaveBufferEntry>*
GhostManager::allocGhostSaveBuffer(int entries)
{
    if (m_saveBufferPool.empty())                        // nothing pooled - build a new one
    {
        std::vector<GhostSaveBufferEntry>* const buffer = new std::vector<GhostSaveBufferEntry>();
        buffer->resize(entries);                         // allocate(56 * n) ...
        buffer->resize(0);                               // ... then empty it. See above.
        m_allSaveBuffers.push_back(buffer);              // the ownership registry
        return buffer;
    }

    std::vector<GhostSaveBufferEntry>* const buffer = m_saveBufferPool.back();
    m_saveBufferPool.pop_back();
    return buffer;
}

void GhostManager::freeGhostSaveBuffer(std::vector<GhostManager::GhostSaveBufferEntry>* buffer)
{
    buffer->resize(0);                                   // emptied, NOT deallocated
    m_saveBufferPool.push_back(buffer);                  // back to the free list, still owned
}

// Write queued resends followed by highest-priority ghosts within the packet and
// ghost-count budgets. Failed large records may yield space to smaller ones.
// Return bits added, measured by the stream's high-water mark. An empty update
// uses one zero bit; a populated update has a 42-bit header and at most 255 ghosts.
// Save transmitted state for acknowledgement and reset each sent ghost's priority.
unsigned int GhostManager::transmit(io::BitStream* stream, PacketStatus& status, unsigned int budget)
{
    const unsigned int startLength    = stream->m_highWaterBits;   // the high-water mark
    const unsigned int savedBitLimit  = stream->m_bitLimit;

    if (budget)                                          // a budget only ever narrows the limit
    {
        unsigned int limit = budget + startLength;
        if (stream->m_bitLimit <= limit)
            limit = stream->m_bitLimit;
        stream->m_bitLimit = limit;
    }

    // the position marker the header patch-back seeks to - the read cursor in read mode, the write
    // cursor otherwise, carried with the mode it was taken in.
    io::BSPosition mark;
    if (stream->m_readWriteSwitch == 1)
    {
        mark.m_readWriteSwitch = 1;
        mark.m_position        = stream->m_readBitPos;
        stream->m_positionCache = stream->m_readBitPos;
    }
    else
    {
        mark.m_readWriteSwitch = stream->m_readWriteSwitch;
        mark.m_position        = stream->m_bitPos;
        stream->m_positionCache = stream->m_bitPos;
    }
    stream->m_byte38       = true;
    stream->m_rwSwitchCache = stream->m_readWriteSwitch;
    stream->m_lengthCache   = stream->m_highWaterBits;

    if (!stream->skipBits(42))                           // the 42-bit header - see above
    {
        // not even room for the header. Restore everything and emit the one-bit "no ghosts".
        stream->setPosition(*reinterpret_cast<io::BSPosition*>(&stream->m_dword3C));
        stream->m_byte38            = false;
        stream->m_errorCode         = 0;
        stream->m_highWaterBits     = stream->m_lengthCache;
        stream->m_error             = false;
        if (budget)
            stream->m_bitLimit = savedBitLimit;
        const unsigned int zero = 0;
        stream->writeBits(&zero, 1);
        return 1;                                        // ONE, not zero - see above
    }
    stream->m_byte38 = false;

    int remaining = m_capacity;                          // the per-transmit ghost budget
    m_ghostsThisPacket = 0;                              // the 8-bit header field

    // ONE local, not four: sendData's fifth parameter is `SaveListEntry&` by its mangled name and
    //   transmit passes the address of this - so the buffer really is its first member.
    //   The other three fields stay at their construction values unless a control state is
    //   written, and 0xFFFF is what the ELF seeds the net id with.
    SaveListEntry entry;
    entry.m_buffer              = allocGhostSaveBuffer(m_capacity);
    entry.m_controlObjNetId     = 0xFFFF;
    entry.m_actionBufferEntries = 0;
    entry.m_baseLineIndex       = 0;

    bool hasControlState = false;
    m_baseLineSlotThisPacket = -1;

    if (m_updateControlObject && m_actionBufferEntries != -1)
    {
        stream->resetCompressionVector();
        hasControlState = writeControlObjectState(stream);
        if (hasControlState)
        {
            entry.m_controlObjNetId     = m_controlObjectNetworkableId;
            entry.m_actionBufferEntries = m_actionBufferEntries;
            entry.m_baseLineIndex       = m_baseLineSlotThisPacket;   // the baseline this packet is against
        }
        m_lastSentActionBufferEntries = m_actionBufferEntries;
        m_actionBufferEntries         = -1;
    }

    // what WAS lost goes first. Each entry is replayed as action 3 through a stack GhostObject
    //    with only two fields set - see the note above about its other 82 bytes.
    while (!m_resendQueue.empty())
    {
        const ResendQueueEntry lost = m_resendQueue.front();   // NOT the SaveListEntry
        m_resendQueue.pop_front();

        GhostObject replayed;                            // deliberately uninitialised, as shipped
        replayed.m_ghostableState = lost.m_ghostableState;
        replayed.m_networkableId  = lost.m_networkableId;

        if (!sendData(*stream, static_cast<GhostAction>(3), &replayed, false, entry))
            break;                                       // the stream is full - stop draining
    }

    // Send live ghosts in descending accumulated priority.
    while (!m_ghostPrioHeap.empty() && remaining > 0)
    {
        GhostObject* const ghost = m_ghostPrioHeap.front();        // the heap root
        std::pop_heap(m_ghostPrioHeap.begin(), m_ghostPrioHeap.end(), GhostLessComp());
        m_ghostPrioHeap.pop_back();

        if (sendData(*stream, static_cast<GhostAction>(ghost->m_ghostableState), ghost,
                     false, entry))
        {
            ++ghost->m_byte51;
            ghost->m_accumulatedPriority = 0.0f;                   // the starvation reset
        }
        else if ((stream->m_bitLimit - stream->m_highWaterBits) <= 0x17)
        {
            break;                                                 // 23 bits or fewer left
        }
        --remaining;
    }

    if (hasControlState || m_ghostsThisPacket)
    {
        // tell the packet which stream managers contributed - the mask the ack path reads back.
        reinterpret_cast<unsigned int*>(&status)[7] |= 1u << getStreamManagerId();   // +0x1C

        m_saveList.push_back(entry);                     // the four dwords, as one 16-byte record

        // back to the marker, and the 42 bits go in - see the header note.
        stream->setWritePosition(mark);
        const unsigned int one = 1;
        stream->writeBits(&one, 1);                                // present
        stream->writeBits(&m_tickCount, 32);                       // the server tick
        const unsigned int ghosts = static_cast<unsigned int>(m_ghostsThisPacket);
        stream->writeBits(&ghosts, 8);                             // 255 ghosts max per packet
        const unsigned int control = hasControlState ? 1u : 0u;
        stream->writeBits(&control, 1);

        if (stream->m_canResetWritePosition)
        {
            stream->m_canResetWritePosition = false;
            stream->setPosition(*reinterpret_cast<io::BSPosition*>(&stream->m_dword24));
        }
        else
        {
            // attested, and from another file - "IO/BitStream.inl" line 341. Type 6 aborts.
            Debug(DEBUGTYPE_6, "IO/BitStream.inl", 341)
                << "Call to resetWritePosition without prior call to setPosition";
        }
    }
    else
    {
        // nothing to say: rewind, put the length back, and spend ONE bit saying so.
        stream->setPosition(mark);
        stream->setBitSize(startLength);
        const unsigned int zero = 0;
        stream->writeBits(&zero, 1);
        freeGhostSaveBuffer(entry.m_buffer);
    }

    reinterpret_cast<unsigned int*>(&status)[6] = m_ghostsThisPacket;   // PacketStatus +0x18

    removeOldGhosts(false);                              // the sweep, unforced
    stream->resetCompressionVector();
    if (budget)
        stream->m_bitLimit = savedBitLimit;

    const unsigned int written = stream->m_highWaterBits - startLength;
    m_writeBits = written;                        // +0x08
    return written;                                      // the caller does `>> 3` on this
}

// Attempts one ghost write, rolling back stream position, length and errors on failure.
// Successful writes save the dirty mask before clearing it for acknowledgement recovery.
// Objects with flag 0x20 are skipped with success and consume neither bits nor packet count.
bool GhostManager::sendData(io::BitStream& stream, GhostAction action, GhostObject* ghost,
                            bool flag, SaveListEntry& entry)
{
    // bit 5 means "skip me", and the answer is true - see above.
    world::IObject* const object = io::networkManager->getObject(ghost->m_networkableId);
    if (object && (object->getFlags() & 0x20))
        return true;

    // cache the position and length so a failed write can be undone exactly.
    stream.m_byte38 = true;
    if (stream.m_readWriteSwitch == 1)
    {
        stream.m_positionCache  = stream.m_readBitPos;
        stream.m_rwSwitchCache  = 1;                     // the literal, as in transmit
    }
    else
    {
        stream.m_positionCache  = stream.m_bitPos;
        stream.m_rwSwitchCache  = stream.m_readWriteSwitch;
    }
    stream.m_lengthCache = stream.m_highWaterBits;

    if (!writeData(stream, action, ghost, flag))
    {
        if (world::tv_missile_trace::armed() && action == 1)
            world::tv_missile_trace::log(object, "ghost_rejected",
                "connection=%d stream=%p packet_tick=%u priority=%.9g accumulated=%.9g "
                "bit_start=%u bit_limit=%u",
                m_connectionId, static_cast<void*>(&stream), m_tickCount,
                ghost->m_priority, ghost->m_accumulatedPriority,
                stream.m_positionCache, stream.m_bitLimit);
        // the roll-back: put everything back and report failure.
        stream.setPosition(*reinterpret_cast<io::BSPosition*>(&stream.m_dword3C));
        stream.m_byte38        = false;
        stream.m_errorCode     = 0;
        stream.m_error         = false;
        stream.m_highWaterBits = stream.m_lengthCache;
        return false;
    }
    stream.m_byte38 = false;

    // the undo record - what was sent, for whom, and the dirty block before the send.
    GhostSaveBufferEntry saved;
    saved.m_action        = static_cast<int>(action);
    saved.m_ghostId       = ghost->m_ghostId;
    saved.m_networkableId = ghost->m_networkableId;
    for (int i = 0; i < 12; ++i)
        saved.m_state[i] = ghost->m_stateMask[i];
    entry.m_buffer->push_back(saved);

    // and only now is the ghost clean.
    for (int i = 0; i < 12; ++i)
        ghost->m_stateMask[i] = 0;

    ++m_ghostsThisPacket;                                // the 8-bit header field, 0x809CC75
    if (world::tv_missile_trace::armed() && action == 1)
        world::tv_missile_trace::log(object, "ghost_selected",
            "connection=%d stream=%p packet_tick=%u priority=%.9g accumulated=%.9g "
            "bit_start=%u bit_end=%u bit_limit=%u",
            m_connectionId, static_cast<void*>(&stream), m_tickCount,
            ghost->m_priority, ghost->m_accumulatedPriority,
            stream.m_positionCache, stream.getBitPosition(), stream.m_bitLimit);
    return true;
}

// Writes a ghost action and networkable ID; only updates carry a flag and length-prefixed payload.
// Uses ghost precision and consumes the supplied state mask.
// Returns false for a null ghost or stream failure, clearing the error latch so the caller can roll back.
bool GhostManager::writeData(io::BitStream& stream, GhostAction action, GhostObject* ghost, bool flag)
{
    if (!ghost)
        return false;                                    // defensive; sendData never passes null

    const unsigned int actionBits = static_cast<unsigned int>(action);
    stream.writeBits(&actionBits, 2);                    // 2 bits - four actions
    const unsigned short netId = ghost->m_networkableId;
    stream.writeBits(&netId, 16);                        // the sixteen-BIT id, not the ghost id

    if (action == 1)                                     // only action 1 carries a payload
    {
        const unsigned int flagBit = flag ? 1u : 0u;
        stream.writeBits(&flagBit, 1);

        // reserve the length field, remember where it went.
        io::BSPosition mark;
        if (stream.m_readWriteSwitch == 1)
        {
            mark.m_readWriteSwitch = 1;
            mark.m_position        = stream.m_readBitPos;
        }
        else
        {
            mark.m_readWriteSwitch = stream.m_readWriteSwitch;
            mark.m_position        = stream.m_bitPos;
        }
        stream.skipBits(m_updateLengthBits);

        const unsigned int payloadStart =
            (stream.m_readWriteSwitch == 1) ? stream.m_readBitPos : stream.m_bitPos;

        // the state mask goes in by address and comes back consumed; the precision is the
        //    ghost tier, not Full - see the note above.
        if (!io::networkManager->getNetUpdate(stream, ghost->m_descriptor,
                                             reinterpret_cast<io::NetworkableStateMask*>(ghost->m_stateMask),
                                             io::StatePrecisionGhost, true))
            return false;                                // the latch is NOT cleared on this arm

        const unsigned int payloadEnd =
            (stream.m_readWriteSwitch == 1) ? stream.m_readBitPos : stream.m_bitPos;
        const unsigned int payloadBits = payloadEnd - payloadStart;

        // back to the reserved field and write how long the update turned out to be.
        stream.setWritePosition(mark);
        stream.writeBits(&payloadBits, m_updateLengthBits);

        if (stream.m_canResetWritePosition)
        {
            stream.m_canResetWritePosition = false;
            stream.setPosition(*reinterpret_cast<io::BSPosition*>(&stream.m_dword24));
        }
        else
        {
            // the second attested site for this line. Type 6 aborts.
            Debug(DEBUGTYPE_6, "IO/BitStream.inl", 341)
                << "Call to resetWritePosition without prior call to setPosition";
        }
    }

    if (stream.m_error)                                  // tested and cleared - see above
    {
        stream.m_errorCode = 0;
        stream.m_error     = false;
        return false;
    }
    return true;
}

// Retires one packet's saved state. Lost removals are requeued; lost updates restore
// only mask bits not covered by later packets and raise the ghost's priority.
// Delivered control-state baselines are acknowledged only while the control object still matches.
// Calls must remain aligned with packet order; scratch-mask pointers expire with the saved buffer.
void GhostManager::handlePacketStatus(PacketStatus& status, bool& delivered)
{
    (void)status;                                        // the ELF reads neither field of it here

    if (m_saveList.empty())
        return;

    SaveListEntry& front = m_saveList.front();

    if (delivered)
    {
        // the packet arrived - credit its baseline, if the control object still matches.
        if (front.m_baseLineIndex != -1 &&
            front.m_controlObjNetId == m_controlObjectNetworkableId)
        {
            m_baseLineIndex = front.m_baseLineIndex;
        }
    }
    else
    {
        // the packet was lost. Steps 1-4 of the note above.

        // 1. the scratch table. the ELF's alignment branch is the compiler's - see above.
        std::memset(m_networkableStateMasks, 0, sizeof(m_networkableStateMasks));

        // 2. what this packet carried.
        {
            std::vector<GhostSaveBufferEntry>& buffer = *front.m_buffer;
            for (std::vector<GhostSaveBufferEntry>::iterator it = buffer.begin();
                 it != buffer.end(); ++it)
            {
                if (it->m_action == 1)
                {
                    // point at the saved mask - the dirt as it was when the packet went out
                    m_networkableStateMasks[it->m_ghostId] =
                        reinterpret_cast<io::NetworkableStateMask*>(it->m_state);
                }
                else if (it->m_action == 3)
                {
                    // a lost removal is re-queued verbatim - see the note above.
                    ResendQueueEntry lost;
                    lost.m_ghostableState = 3;
                    lost.m_networkableId  = it->m_networkableId;
                    m_resendQueue.push_back(lost);
                }
            }
        }

        // 3. discount everything a later, still-unacknowledged packet already carried.
        for (std::deque<SaveListEntry>::iterator later = m_saveList.begin() + 1;
             later != m_saveList.end(); ++later)
        {
            std::vector<GhostSaveBufferEntry>& buffer = *later->m_buffer;
            for (std::vector<GhostSaveBufferEntry>::iterator it = buffer.begin();
                 it != buffer.end(); ++it)
            {
                if (it->m_action != 1)
                    continue;
                io::NetworkableStateMask* const scratch = m_networkableStateMasks[it->m_ghostId];
                if (scratch)
                {
                    scratch->doOperation(*reinterpret_cast<io::NetworkableStateMask*>(it->m_state),
                                         static_cast<io::StateMaskOp>(2));     // op 2 = AND NOT
                }
            }
        }

        // 4. fold what survives back into the live ghosts and make them urgent.
        for (int ghostId = 0; ghostId <= 1023; ++ghostId)
        {
            io::NetworkableStateMask* const scratch = m_networkableStateMasks[ghostId];
            if (!scratch)
                continue;
            GhostObject* const ghost = m_ghostObjects[ghostId];
            if (!ghost)
                continue;
            reinterpret_cast<io::NetworkableStateMask*>(ghost->m_stateMask)
                ->doOperation(*scratch, static_cast<io::StateMaskOp>(1));      // op 1 = OR
            ghost->m_priority = 10000.0f;                // the ceiling - resend it now
        }
    }

    // exactly one entry is retired per call, whatever the outcome.
    freeGhostSaveBuffer(front.m_buffer);
    m_saveList.pop_front();
}

// An absent ghost block succeeds without changing state. Dedicated builds reject a present block.
// Client builds read its control state and records, storing the tick only after a valid header.
// Client decode failures set m_error = 2; the record-failure diagnostic is fatal.
// The received tick is not validated. A false result makes the server disconnect the peer.
bool GhostManager::processReceivedPacket(io::BitStream* stream)
{
#if !DEDICATED
    const unsigned int startPos = (stream->m_readWriteSwitch == 1)
                                ? stream->m_readBitPos : stream->m_bitPos;
#endif

    unsigned int present = 0;
    stream->readBits(&present, 1);
    if (present != 1)
        return true;                                     // the one-bit "no ghosts" case

#if DEDICATED
    // [CHOSEN, not recovered] Clients must not send ghost updates to a dedicated server.
    // Reject before decoding or applying any state: https://github.com/B2BF/OpenBF2/issues/1.
    return false;
#else
    unsigned int tick = 0;
    stream->readBits(&tick, 32);
    unsigned int count = 0;
    stream->readBits(&count, 8);                         // 0..255, and that is the whole bound
    unsigned int control = 0;
    stream->readBits(&control, 1);
    const bool hasControlState = (control == 1);

    bool failed = stream->m_error;                       // the header itself may have run out
    if (!failed)
    {
        m_tickCount   = tick;                            // only reached on a clean header
        m_timeSeconds = tick / 30.0;
        stream->resetCompressionVector();
        if (hasControlState)
        {
            readControlObjectState(stream);              // bool result discarded, as shipped
            failed = stream->m_error;
        }
    }

    if (!failed)
    {
        while (count)
        {
            --count;
            if (!readData(*stream))
            {
                // type 7 folds to 6 and aborts - the tail below is unreachable from here.
                Debug(DEBUGTYPE_7, "Game/Common/GhostManager.cpp", 1658)
                    << "Failed to receive ghostmanager";
                m_error = 2;
                return false;
            }
        }

        stream->resetCompressionVector();
        const unsigned int endPos = (stream->m_readWriteSwitch == 1)
                                  ? stream->m_readBitPos : stream->m_bitPos;
        m_readBits = endPos - startPos;          // +0x04 - transmit's mirror image
        return true;
    }

    // fail closed. The latch is cleared so the rest of the packet can still be walked, and the
    //     false is what disconnects the peer - see the note above.
    stream->m_errorCode = 0;
    stream->m_error     = false;
    m_error = 2;
    return false;
#endif
}

// Read one ghost action: no-op, update or removal; action 2 is a protocol error.
// Unknown/skipped descriptors consume the length-delimited payload. Known updates
// must consume exactly that length or fail. Timestamps are milliseconds. Removal
// deactivates the descriptor and conditionally disables the object.
// [UNRESOLVED] The relationship between object and network-info bit 0x20 is unknown.
// [RETIRE-WHEN: ghostmanager-real]
bool GhostManager::readData(io::BitStream& stream)
{
    unsigned int action = 0;
    stream.readBits(&action, 2);                         // two bits, four actions
    unsigned short netId = 0;
    stream.readBits(&netId, 16);

    if (action != 1)
    {
        if (action <= 1)
            return true;                                 // action 0 - a no-op, by design

        if (action != 2)
        {
            if (action != 3)
                return true;                             // unreachable - 2 bits, 4 handled

            // action 3: remove. client code; dead on a dedicated server - see above.
            io::NetworkableDescriptor* const desc = io::networkManager->getDescriptor(netId);
            if (!desc)
                return true;                             // an id we do not know: ignore it

            io::INetworkable* networkable = desc->properties[0].networkables[0];
            world::IObject* const object = io::networkManager->getObject(desc->descId);

            // the disable decision. Everything below still ends at the same deactivation.
            if (object && !(networkable->m_basePriority < 0.0f))   // 1.0f = skip the disable
            {
                io::INetworkableObject* const objEntry = networkable->m_networkableObject;
                bool disable = true;

                if (objEntry && *reinterpret_cast<const short*>(
                        reinterpret_cast<const char*>(objEntry) + 4) >= 0)
                {
                    // the object belongs to a player - only disable it if that player is gone
                    //   or local, and only if it is not parented to something else.
                    const short playerId = *reinterpret_cast<const short*>(
                        reinterpret_cast<const char*>(objEntry) + 4);
                    world::IPlayer* const player = world::playerManager->getPlayerFromId(playerId);
                    if (!player || !player->getIsRemote() || object->getParent())
                        disable = false;
                }
                else if (object->getParent())
                {
                    disable = false;
                }
                else
                {
                    // never disable the local player's own vehicle's root.
                    world::IPlayer* const local = world::playerManager->getLocalHumanPlayer();
                    world::IObject* const vehicle = local ? local->getVehicle() : 0;
                    if (!vehicle || object == world::getRootParent(vehicle))
                        disable = false;
                }

                if (disable)
                    disableObject(object);
            }

            networkable = desc->properties[0].networkables[0];    // re-read, as the ELF does
            networkable->setGhostActive(false);
            io::networkManager->removeActiveDescriptor(desc);
            return true;
        }

        // action 2: never legal. Disconnect.
        m_error = 2;
        return false;
    }

    // action 1: update
    unsigned int flag = 0;
    stream.readBits(&flag, 1);
    bool ghostActive = (flag == 1);

    unsigned int bits = 0;
    stream.readBits(&bits, m_updateLengthBits);          // the payload length - see above
    if (stream.m_error)
    {
        stream.m_errorCode = 0;
        stream.m_error     = false;
        return false;                                    // no m_error here - the caller aborts
    }

    io::NetworkableDescriptor* desc = io::networkManager->getDescriptor(netId);

    // the 0x20 filter, on INetworkableInfo this time - see the note above.
    {
        io::INetworkable* const n = desc ? desc->properties[0].networkables[0] : 0;
        if (n && n->m_networkableObject)
        {
            const unsigned char* const info =
                *reinterpret_cast<unsigned char* const*>(
                    reinterpret_cast<const char*>(n->m_networkableObject) + 8);
            if (info && (info[4] & 0x20))
                desc = 0;                                // treat it as unknown: skip the payload
        }
    }

    if (!desc)
    {
        // Skip the declared payload when its record cannot be applied.
        if (stream.skipBits(bits))
            return true;
        m_error = 2;                                     // a length past the end: disconnect
        return false;
    }

    if (!desc->properties[0].networkables[0]->m_ghostActive)
    {
        // first update for this object: bring it back to life before applying the state.
        world::IObject* const object = io::networkManager->getObject(netId);
        if (object)
            enableObject(object);
        desc->properties[0].networkables[0]->setGhostActive(true);
        io::networkManager->addActiveDescriptor(desc);
        ghostActive = true;                              // forced, whatever the wire said
    }

    const unsigned int before =
        (stream.m_readWriteSwitch == 1) ? stream.m_readBitPos : stream.m_bitPos;

    // the timestamp is milliseconds - see above.
    const float timeMs = 1000.0f * static_cast<float>(m_timeSeconds);
    if (!io::networkManager->setNetUpdate(stream, desc, timeMs, ghostActive,
                                          io::StatePrecisionGhost))
        return false;                                    // [FRONTIER]; no m_error on this arm

    const unsigned int after =
        (stream.m_readWriteSwitch == 1) ? stream.m_readBitPos : stream.m_bitPos;

    if (after - before == bits)                          // the length is verified
        return true;

    m_error = 2;                                         // a payload that lied about its size
    return false;
}

// Applies assign, OR, AND-NOT, or AND across all twelve mask words. Unknown operation
// values leave the destination unchanged.
void io::NetworkableStateMask::doOperation(const io::NetworkableStateMask& other, io::StateMaskOp op)
{
    const int operation = static_cast<int>(op);

    if (operation == 1)                                  // OR - the fold
    {
        for (int i = 0; i < 12; ++i)
            m_opaque[i] |= other.m_opaque[i];
        return;
    }

    if (operation > 1)
    {
        if (operation == 2)                              // AND NOT - the subtraction
        {
            for (int i = 0; i < 12; ++i)
                m_opaque[i] &= ~other.m_opaque[i];
            return;
        }
        if (operation != 3)                              // op > 3: silent no-op
            return;
        for (int i = 0; i < 12; ++i)                     // AND
            m_opaque[i] &= other.m_opaque[i];
        return;
    }

    if (operation == 0)                                  // assign
    {
        for (int i = 0; i < 12; ++i)
            m_opaque[i] = other.m_opaque[i];
    }
}

// Serializes each property's representative networkable when the minimum bit budget fits.
// Ghost updates intersect the supplied mask with ghost masks at the requested precision;
// other updates use full precision. Empty property lists fail; null representatives are skipped.
// Processes at most twelve properties and supplies no baseline.

bool io::NetworkManager::getNetUpdate(io::BitStream& stream, io::NetworkableDescriptor* descriptor,
                                      io::NetworkableStateMask* mask, int precision, bool ghostTier)
{
    if (!descriptor)
        return false;

    // the budget gate - see above
    unsigned int limit;
    unsigned int cursor;
    if (stream.m_readWriteSwitch == 1)
    {
        limit  = stream.m_highWaterBits;                 // +0x18 - the read-side bound
        cursor = stream.m_readBitPos;
    }
    else
    {
        limit = stream.m_bitLimit;                       // +0x08 - transmit's choke, 0 = none
        if (!limit)
            limit = 8 * stream.getSizeBytes();
        cursor = stream.m_bitPos;
    }
    if (static_cast<unsigned int>(descriptor->minimumBitSize) > limit - cursor)
    {
        ++m_dword6C;                                     // the "did not fit" counter
        return false;
    }

    typedef std::vector<UniqueNetProperty>::iterator PropIter;
    const PropIter end = descriptor->properties.end();

    if (mask)
    {
        if (ghostTier)
        {
            // mask & getGhostStateMask(), caller's precision
            PropIter it = descriptor->properties.begin();
            int slot = 0;
            if (it != end)
            {
                for (;;)
                {
                    UniqueNetProperty& property = *it;
                    if (property.networkables.empty())   // an empty property aborts everything
                        return false;
                    INetworkable* const networkable = property.networkables[0];
                    if (networkable)                     // a null one is skipped
                    {
                        const unsigned int ghostMask = networkable->getGhostStateMask();
                        const bool walkOk = networkable->getNetUpdate(stream, 0,
                                mask->m_opaque[slot] & ghostMask, precision);
                        if (!walkOk)
                        {
                            ++m_ghostNetworkableUpdatesMissing;
                            return false;
                        }
                    }
                    ++it;
                    ++slot;
                    if (it == end || slot > 11)          // the twelve-slot cap - see above
                        return true;
                }
            }
        }
        else
        {
            // mask alone, full precision
            PropIter it = descriptor->properties.begin();
            int slot = 0;
            if (it != end)
            {
                for (;;)
                {
                    UniqueNetProperty& property = *it;
                    if (property.networkables.empty())
                        return false;
                    INetworkable* const networkable = property.networkables[0];
                    if (networkable)
                    {
                        const bool walkOk = networkable->getNetUpdate(stream, 0,
                               mask->m_opaque[slot], io::StatePrecisionFull);
                        if (!walkOk)
                        {
                            ++m_ghostNetworkableUpdatesMissing;
                            return false;
                        }
                    }
                    ++it;
                    ++slot;
                    if (it == end || slot > 11)
                        return true;
                }
            }
        }
    }
    else if (ghostTier)
    {
        // getGhostStateMask() alone, caller's precision
        PropIter it = descriptor->properties.begin();
        if (it != end)
        {
            for (;;)
            {
                UniqueNetProperty& property = *it;
                if (property.networkables.empty())
                    return false;
                INetworkable* const networkable = property.networkables[0];
                if (networkable)
                {
                    const unsigned int ghostMask = networkable->getGhostStateMask();
                    const bool walkOk = networkable->getNetUpdate(stream, 0, ghostMask, precision);
                    if (!walkOk)
                    {
                        ++m_ghostNetworkableUpdatesMissing;
                        return false;
                    }
                }
                ++it;
                if (it == end)                           // no slot cap on the maskless arms
                    return true;
            }
        }
    }
    else
    {
        // everything, full precision
        PropIter it = descriptor->properties.begin();
        if (it != end)
        {
            for (;;)
            {
                UniqueNetProperty& property = *it;
                if (property.networkables.empty())
                    return false;
                INetworkable* const networkable = property.networkables[0];
                if (networkable)
                {
                    const bool walkOk = networkable->getNetUpdate(stream, 0,
                           0xFFFFFFFFu, io::StatePrecisionFull);
                    if (!walkOk)
                    {
                        ++m_ghostNetworkableUpdatesMissing;
                        return false;
                    }
                }
                ++it;
                if (it == end)
                    return true;
            }
        }
    }

    return true;                                         // an empty property vector succeeds
}

// Applies each property's payload to every associated networkable by rewinding
// to the saved cursor before each call. Elements must be non-null; the last call
// leaves the cursor at the next property.
bool io::NetworkManager::setNetUpdate(io::BitStream& stream, io::NetworkableDescriptor* descriptor,
                                      float timeMs, bool ghostActive, int precision)
{
    if (!descriptor)
        return false;

    typedef std::vector<UniqueNetProperty>::iterator PropIter;
    for (PropIter it = descriptor->properties.begin();
         it != descriptor->properties.end(); ++it)
    {
        io::BSPosition mark;
        if (stream.m_readWriteSwitch == 1)
        {
            mark.m_readWriteSwitch = 1;
            mark.m_position        = stream.m_readBitPos;
        }
        else
        {
            mark.m_readWriteSwitch = stream.m_readWriteSwitch;
            mark.m_position        = stream.m_bitPos;
        }

        for (std::vector<INetworkable*>::iterator n = it->networkables.begin();
             n != it->networkables.end(); ++n)
        {
            stream.setPosition(mark);                    // every sharer re-reads the same bits
            (*n)->setNetUpdate(stream, timeMs, ghostActive, 0, precision);   // unchecked
        }
    }
    return true;
}

// Snapshots each property's representative networkable, optionally rewinding first.
// Empty properties or refused writes fail the snapshot; null representatives are skipped.
bool io::NetworkManager::getBaseLine(io::BaseLineData& baseLine,
                                     io::NetworkableDescriptor* descriptor,
                                     bool resetBaseLineCursor)
{
    if (!descriptor)
        return false;

    if (resetBaseLineCursor)
        baseLine.m_cursor = 0;

    typedef std::vector<UniqueNetProperty>::iterator PropIter;
    const PropIter end = descriptor->properties.end();
    PropIter it = descriptor->properties.begin();
    if (it == end)
        return true;

    for (;;)
    {
        UniqueNetProperty& property = *it;
        if (property.networkables.empty())
            return false;
        INetworkable* const networkable = property.networkables[0];
        if (networkable && !networkable->getBaseLine(baseLine))   // +0x38
            return false;
        ++it;
        if (it == end)
            return true;
    }
}

// Writes updates against the supplied baseline after rewinding its cursor. Unlike
// the non-delta path, masked properties have no twelve-slot cap and budget refusal
// does not increment the refusal counter.
bool io::NetworkManager::getNetUpdateDelta(io::BitStream& stream,
                                           io::NetworkableDescriptor* descriptor,
                                           io::BaseLineData& baseLine,
                                           io::NetworkableStateMask* mask, int precision,
                                           bool ghostTier, bool resetBaseLineCursor)
{
    if (!descriptor)
        return false;

    // the budget gate - the delta variant's own shape, see above
    unsigned int remaining;
    if (stream.m_readWriteSwitch == 1)
    {
        remaining = stream.m_highWaterBits - stream.m_readBitPos;
    }
    else if (stream.m_bitLimit)
    {
        remaining = stream.m_bitLimit - stream.m_bitPos; // one subtraction, as shipped
    }
    else
    {
        remaining = 8 * stream.getSizeBytes() - stream.m_bitPos;
    }
    if (static_cast<unsigned int>(descriptor->minimumBitSize) > remaining)
        return false;                                    // NO counter here - see above

    if (resetBaseLineCursor)
        baseLine.m_cursor = 0;                           // the +0x400 word - now a named field

    typedef std::vector<UniqueNetProperty>::iterator PropIter;
    const PropIter end = descriptor->properties.end();

    if (mask)
    {
        if (ghostTier)
        {
            PropIter it = descriptor->properties.begin();
            int slot = 0;
            if (it != end)
            {
                for (;;)
                {
                    UniqueNetProperty& property = *it;
                    if (property.networkables.empty())
                        return false;
                    INetworkable* const networkable = property.networkables[0];
                    if (networkable)
                    {
                        const unsigned int ghostMask = networkable->getGhostStateMask();
                        const bool walkOk = networkable->getNetUpdate(stream, &baseLine,
                                mask->m_opaque[slot] & ghostMask, precision);
                        if (!walkOk)
                        {
                            ++m_ghostNetworkableUpdatesMissing;
                            return false;
                        }
                    }
                    ++it;
                    ++slot;                              // no > 11 cap in the delta variant
                    if (it == end)
                        return true;
                }
            }
        }
        else
        {
            PropIter it = descriptor->properties.begin();
            int slot = 0;
            if (it != end)
            {
                for (;;)
                {
                    UniqueNetProperty& property = *it;
                    if (property.networkables.empty())
                        return false;
                    INetworkable* const networkable = property.networkables[0];
                    if (networkable)
                    {
                        const bool walkOk = networkable->getNetUpdate(stream, &baseLine,
                               mask->m_opaque[slot], io::StatePrecisionFull);
                        if (!walkOk)
                        {
                            ++m_ghostNetworkableUpdatesMissing;
                            return false;
                        }
                    }
                    ++it;
                    ++slot;
                    if (it == end)
                        return true;
                }
            }
        }
    }
    else if (ghostTier)
    {
        PropIter it = descriptor->properties.begin();
        if (it != end)
        {
            for (;;)
            {
                UniqueNetProperty& property = *it;
                if (property.networkables.empty())
                    return false;
                INetworkable* const networkable = property.networkables[0];
                if (networkable)
                {
                    const unsigned int ghostMask = networkable->getGhostStateMask();
                    const bool walkOk = networkable->getNetUpdate(stream, &baseLine,
                                                                  ghostMask, precision);
                    if (!walkOk)
                    {
                        ++m_ghostNetworkableUpdatesMissing;
                        return false;
                    }
                }
                ++it;
                if (it == end)
                    return true;
            }
        }
    }
    else
    {
        PropIter it = descriptor->properties.begin();
        if (it != end)
        {
            for (;;)
            {
                UniqueNetProperty& property = *it;
                if (property.networkables.empty())
                    return false;
                INetworkable* const networkable = property.networkables[0];
                if (networkable)
                {
                    const bool walkOk = networkable->getNetUpdate(stream, &baseLine,
                           0xFFFFFFFFu, io::StatePrecisionFull);
                    if (!walkOk)
                    {
                        ++m_ghostNetworkableUpdatesMissing;
                        return false;
                    }
                }
                ++it;
                if (it == end)
                    return true;
            }
        }
    }

    return true;
}

// io::NetworkManager::setNetUpdateDelta - ELF 0x809C500, 220 bytes. Same
// TU. The delta twin of setNetUpdate above - identical rewind fan-out, identical missing null
// check - with the BaseLineData threaded through so each networkable's baseline-delta arm can
// decompress against the snapshot, and the same cursor rewind on demand.
bool io::NetworkManager::setNetUpdateDelta(io::BitStream& stream,
                                           io::NetworkableDescriptor* descriptor,
                                           io::BaseLineData& baseLine, float timeMs,
                                           bool ghostActive, int precision,
                                           bool resetBaseLineCursor)
{
    if (!descriptor)
        return false;

    if (resetBaseLineCursor)
        baseLine.m_cursor = 0;

    typedef std::vector<UniqueNetProperty>::iterator PropIter;
    for (PropIter it = descriptor->properties.begin();
         it != descriptor->properties.end(); ++it)
    {
        io::BSPosition mark;
        if (stream.m_readWriteSwitch == 1)
        {
            mark.m_readWriteSwitch = 1;
            mark.m_position        = stream.m_readBitPos;
        }
        else
        {
            mark.m_readWriteSwitch = stream.m_readWriteSwitch;
            mark.m_position        = stream.m_bitPos;
        }

        for (std::vector<INetworkable*>::iterator n = it->networkables.begin();
             n != it->networkables.end(); ++n)
        {
            stream.setPosition(mark);                    // the same bits for every sharer
            (*n)->setNetUpdate(stream, timeMs, ghostActive, &baseLine, precision);   // unchecked
        }
    }
    return true;
}

// The IBase quartet - 0x809BE10 / 0x809BE20 / 0x809BE30 / 0x809BE70. The third copy of these four
// bodies in as many stream managers, at the third address, answering the same two iids.
// queryInterface does NOT answer an IEventListener iid even though the class carries that base -
// the second sub-object is reachable only by static upcast.
void GhostManager::addRef() { ++m_refCount; }                    // +0x010
int  GhostManager::getRef() { return m_refCount; }

int GhostManager::release()
{
    if (--m_refCount == 0)
    {
        delete this;
        return 0;
    }
    return m_refCount;
}

void* GhostManager::queryInterface(unsigned int iid) const
{
    if (iid == IID_IBase)
        return const_cast<GhostManager*>(this);
    if (iid == IID_IStreamManager)
        return const_cast<GhostManager*>(this);
    return 0;
}

// The IStreamManager accessors - 0x809BE90..0x809BED0. getStreamManagerId and setStreamManagerId
// reach a field the constructor sets and this header does not model, so both are left to RW-25
// rather than written against an undeclared member.
void GhostManager::setConnectionId(int id) { m_connectionId = id; }      //   +0x014
int  GhostManager::getConnectionId()       { return m_connectionId; }    //   +0x014
int  GhostManager::getError()              { return m_error; }           //   +0x018

// setPlayer - 0x8099010, 17 bytes. One store, and it is the deepest offset this tree has pinned in
// any class: +0x2140 of an 0x217C-byte object. ClientConnection::setPlayer tail-jumps here.
void GhostManager::setPlayer(world::IPlayer* player)
{
    m_player = player;                                   //   +0x2140
}

// handleEvent - 0x80993F0, five bytes, empty. The class carries the IEventListener base, its own
// second vptr and a this-adjusting thunk, and answers nothing at all. The inheritance is real and
// the behaviour is not; reproduced as the empty body it is.
void GhostManager::handleEvent(EventCategory, unsigned int, EventNode*, void*)
{
}

} // namespace hfe
} // namespace dice
