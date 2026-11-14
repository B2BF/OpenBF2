// Spawn-group state and replication. [INFERRED] source path.

#include "SpawnGroup.h"

#include <cstdio>     // [TEMP] BF2_SPAWNGROUP_TRACE
#include <cstdlib>

#include "../../../Core/Ids.h"
#include "../../../Core/SettingsRepostitory.h"      // settings - GLSWorldSizeX/Z
#include "../../../Core/System.h"                   // WorldPref::getTickCount
#include "../../IGame.h"                            // game->queryInterface(IID_IGameServer)
#include "../../Objects/ISpawnPoint.h"
#include "../../Objects/IObject.h"
#include "../../../IO/BitStream.h"                  // the wire - getNetUpdate
#include "../../../IO/Network/NetworkStats.h"       // the TU's RegistredNetworkStats (0x8AAC75C)
#include "../../../Math/Mat4.h"
#include <cstdio>                                   // the setNetUpdate [FRONTIER] stop


#include <cstdlib>                                  // rand - getSpawnPoint's pick
#include "../../../Platform/MsvcRand.h"  // [CHOSEN, not recovered] the client CRT's rand (MSVCR71)

namespace dice {
namespace hfe {

// Starts active and team-settable, with no team, squad, control point, group ID,
// or baselines. Position starts dirty; AI/human restrictions start disabled.
SpawnGroup::SpawnGroup()
{
    m_position.x = m_position.y = m_position.z = 0.0f;   // +0x4C
    m_refCount = 1;                                  // +0x40
    m_team = -1;                                     // +0x44
    m_squad = -1;                                    // +0x48
    // m_string58 (+0x58): the shared empty _Rep - default construction
    m_active = true;                                 // +0x5C
    m_teamSettable = true;                           // +0x5D
    // m_spawnPoints (+0x60..+0x68): empty
    m_positionDirty = true;                          // +0x6C
    m_positionLocked = false;                        // +0x6D
    m_onlyForAI = false;                             // +0x6E
    m_onlyForHuman = false;                          // +0x6F
    m_controlPointId = static_cast<unsigned int>(-1);   // +0x70
    m_hasSpawnPoints = false;                        // +0x74
    m_spawnGroupId = -1;                             // +0x78
    m_tickCount = 0;                                 // +0x7C
    m_baselines = 0;                                 // +0x80
}

//  (D1) / 0x81112B0 (D0), 144 / 155 B: vptr restores, delete[] the baselines, free the
// vector and the string, NetworkableBase's and ISpawnGroup's dtors (D0 adds operator delete).
SpawnGroup::~SpawnGroup()
{
    if (m_baselines)
        delete[] m_baselines;
}

// The quartet - refcount at +0x40.
void SpawnGroup::addRef()  { ++m_refCount; }
int  SpawnGroup::getRef()  { return m_refCount; }
int  SpawnGroup::release()
{
    const int r = --m_refCount;
    if (r == 0) delete this;
    return r;
}

// 45 B: IBase / ISpawnGroup -> this; INetworkable -> the +4 sub-object; else 0.
void* SpawnGroup::queryInterface(unsigned int iid) const
{
    if (iid == IID_IBase || iid == IID_ISpawnGroup)
        return static_cast<ISpawnGroup*>(const_cast<SpawnGroup*>(this));
    if (iid == io::IID_INetworkable)
        return static_cast<io::INetworkable*>(const_cast<SpawnGroup*>(this));   // this + 4
    return 0;
}

// 24 B: the store is gated on the +0x5D flag (true from the ctor; nothing recovered clears it).
void SpawnGroup::setTeam(int team)
{
    if (m_teamSettable)
        m_team = team;
}
int  SpawnGroup::getTeam() const            { return m_team; }
void SpawnGroup::setSquad(int squad)        { m_squad = squad; }
int  SpawnGroup::getSquad() const           { return m_squad; }
void SpawnGroup::setActive(bool active)     { m_active = active; }

// getActive(isAI) - 0x8111350, 161 B: inactive -> false. A client trusts the flag (true). The
// server asks the points - unless the group is closed to this asker (the ELF's test is
// `isAI ? !m_onlyForHuman : !m_onlyForAI`: an AI may use the group unless it is human-only, a
// human unless it is AI-only) - and answers true when ANY point answers getActive(isAI, m_team).
bool SpawnGroup::getActive(bool isAI) const
{
    if (!m_active)
        return false;
    if (!game->queryInterface(IID_IGameServer))
        return true;
    if (m_active)                                    // re-tested, as shipped
    {
        const bool open = isAI ? !m_onlyForHuman : !m_onlyForAI;
        if (open)
        {
            for (std::vector<ISpawnPoint*>::const_iterator it = m_spawnPoints.begin();
                 it != m_spawnPoints.end(); ++it)
            {
                if ((*it)->getActive(isAI, m_team))  // ISpawnPoint slot 7 (+0x1C)
                    return true;
            }
        }
    }
    return false;
}

void SpawnGroup::setPos(Vec3 position)     { m_position = position; }

// getPos - 0x8111630, 131 B: on the server, a dynamic group (id > 191 - the squad-leader groups)
// re-averages once per tick (the tick stamp forces the dirty flag); then, dirty and not locked,
// calcNewPos. Returns the position's address (a reference here). A client returns whatever it was
// told through setPos.
const Vec3& SpawnGroup::getPos()
{
    if (game->queryInterface(IID_IGameServer))
    {
        if (game->queryInterface(IID_IGameServer) && m_spawnGroupId > 191
            && m_tickCount != WorldPref::getTickCount())
        {
            m_tickCount = WorldPref::getTickCount();
            m_positionDirty = true;
        }
        if (m_positionDirty && !m_positionLocked)
            calcNewPos();
    }
    return m_position;
}

void SpawnGroup::setOnlyForHuman(bool only) { m_onlyForHuman = only; }
bool SpawnGroup::getOnlyForHuman() const    { return m_onlyForHuman; }
std::vector<ISpawnPoint*>& SpawnGroup::getSpawnPoints() { return m_spawnPoints; }

// getSpawnPoint(isAI) - 0x81116C0, 218 B: gather every point whose getActive(isAI, m_team) == 1
// into a local vector and return one at rand() % n; null when none. (The team is passed - the
// decompiler dropped it; the local it spills at [ebp-50h] is m_team.)
ISpawnPoint* SpawnGroup::getSpawnPoint(bool isAI) const
{
    std::vector<ISpawnPoint*> candidates;
    for (std::vector<ISpawnPoint*>::const_iterator it = m_spawnPoints.begin();
         it != m_spawnPoints.end(); ++it)
    {
        if ((*it)->getActive(isAI, m_team))
            candidates.push_back(*it);
    }
    ISpawnPoint* picked = 0;
    if (!candidates.empty())
        picked = candidates[static_cast<unsigned int>(bf2crt::rand()) % candidates.size()];
    return picked;
}

void SpawnGroup::setControlPointId(unsigned int id) { m_controlPointId = id; }
unsigned int SpawnGroup::getControlPointId()        { return m_controlPointId; }

// Allocates four baseline records once and seeds the initial state. Ignores object
// because spawn groups replicate themselves. Calls getPos twice for its averaging
// side effect. Returns false only on allocation failure.
bool SpawnGroup::init(void* /*object*/)
{
    if (!m_baselines)
    {
        m_baselines = new BaseLine[4];
        if (!m_baselines)                            //   the same 64 bytes; see the struct in the header
            return false;
    }
    m_next = 0;                                      // NetworkableBase +0x38 (SpawnGroup +0x3C)
    // THE RAW +6 poke IS into record zero. The ELF writes `m_baselines[6]` before it computes the
    // record pointer, and with m_next just set to 0 the two coincide - record 0's hasSpawnPoints.
    // Written as the field it is now that the record has a type.
    m_baselines[0].hasSpawnPoints = m_hasSpawnPoints ? 1 : 0;
    BaseLine& record = m_baselines[m_next];
    record.team   = getTeam();
    record.active = static_cast<unsigned char>(getActive(false) ? 1 : 0);
    getPos();
    getPos();
    return true;
}

unsigned int SpawnGroup::getMinimumBitSize() const { return 40; }

// addSpawnPoint - 0x8111400, 149 B: the AI-only latch - the first point decides (an AI-only first
// point makes the group AI-only), a later human-usable point clears it; push_back; dirty the
// average; hasSpawnPoints = true. removeSpawnPoint - 0x81114A0, 149 B: find/erase (first match),
// dirty, hasSpawnPoints = !empty.
void SpawnGroup::addSpawnPoint(ISpawnPoint* point)
{
    if (m_spawnPoints.empty() && point->getOnlyForAI())        // ISpawnPoint slot 19 (+0x4C)
        m_onlyForAI = true;
    else if (m_onlyForAI && !point->getOnlyForAI())
        m_onlyForAI = false;
    m_spawnPoints.push_back(point);
    m_positionDirty = true;
    m_hasSpawnPoints = true;
}

void SpawnGroup::removeSpawnPoint(ISpawnPoint* point)
{
    for (std::vector<ISpawnPoint*>::iterator it = m_spawnPoints.begin(); it != m_spawnPoints.end(); ++it)
    {
        if (*it == point)
        {
            m_spawnPoints.erase(it);
            m_positionDirty = true;
            m_hasSpawnPoints = !m_spawnPoints.empty();
            return;
        }
    }
}

// calcNewPos - 0x8111540, 236 B: the position = the sum of every point's object position (the
// ISpawnPoint -> IObject through queryInterface, then getAbsoluteTransformation()'s translation
// row m[12..14]) divided by the count - divided only when the count is > 1 (a one-point group
// keeps the sum, which is the same number; a zero-point group keeps (0,0,0)); dirty cleared.
void SpawnGroup::calcNewPos()
{
    m_position.x = m_position.y = m_position.z = 0.0f;
    const unsigned int count = static_cast<unsigned int>(m_spawnPoints.size());
    if (count)
    {
        for (std::vector<ISpawnPoint*>::iterator it = m_spawnPoints.begin(); it != m_spawnPoints.end(); ++it)
        {
            world::IObject* object = *it
                ? static_cast<world::IObject*>((*it)->queryInterface(world::IID_IObject)) : 0;
            const Mat4& m = object->getAbsoluteTransformation();   // null object -> null vcall, as shipped
            m_position.x += m.m[12];
            m_position.y += m.m[13];
            m_position.z += m.m[14];
        }
        if (count > 1)
        {
            const float inv = 1.0f / static_cast<float>(count);
            m_position.x *= inv;
            m_position.y *= inv;
            m_position.z *= inv;
        }
    }
    m_positionDirty = false;
}

// getUnsignedWorldPosition - 0x8111840, 517 B: the 8-bit map coordinates the CreateSpawnGroupEvent
// carries. half = GLSWorldSizeX / 2 (setting, default 1024): x > half -> 255; x < -half -> 0;
// else (x + half) / (2 * half) * 255 truncated. The same with GLSWorldSizeZ for z. Both read
// getPos() through the vtable.
void SpawnGroup::getUnsignedWorldPosition(unsigned char* outX, unsigned char* outZ)
{
    int worldSizeX = 1024;
    worldSizeX = settings->intGet("GLSWorldSizeX", worldSizeX);
    const int halfX = worldSizeX / 2;
    int worldSizeZ = 1024;
    worldSizeZ = settings->intGet("GLSWorldSizeZ", worldSizeZ);
    const int halfZ = worldSizeZ / 2;

    const float x = getPos().x;
    if (x > static_cast<float>(halfX))
        *outX = 255;
    else if (static_cast<float>(worldSizeX / -2) <= x)
        *outX = static_cast<unsigned char>((x + static_cast<float>(halfX))
                                            / static_cast<float>(2 * halfX) * 255.0f);
    else
        *outX = 0;

    const float z = getPos().z;
    if (z > static_cast<float>(halfZ))
        *outZ = 255;
    else if (static_cast<float>(worldSizeZ / -2) <= z)
        *outZ = static_cast<unsigned char>((z + static_cast<float>(halfZ))
                                            / static_cast<float>(2 * halfZ) * 255.0f);
    else
        *outZ = 0;
}

// Snapshots spawn-group state into the next baseline and sets six dirty bits.
// Quantized X/Z share one bit; active state is sampled for humans even on AI-only groups.
void SpawnGroup::updateStateMask(unsigned int* mask)
{
    m_prev = m_head;                                 // +0x38 (NetworkableBase +0x34, this base is +4)
    m_head = (m_head + 1) & 3;                       // +0x2C
    m_next = m_head;                                 // +0x3C

    BaseLine& next = m_baselines[m_next];
    BaseLine& prev = m_baselines[m_prev];

    next.hasSpawnPoints = m_hasSpawnPoints;          // written first, tested fourth
    next.team           = getTeam();
    next.active         = static_cast<unsigned char>(getActive(false));   // the human variant
    next.onlyForAI      = m_onlyForAI;
    next.squad          = getSquad();
    getUnsignedWorldPosition(&next.posX, &next.posZ);

    if (prev.team != next.team)                     *mask |= 0x02;
    if (next.active != prev.active)                 *mask |= 0x04;
    if (next.onlyForAI != prev.onlyForAI)           *mask |= 0x08;
    if (next.hasSpawnPoints != prev.hasSpawnPoints) *mask |= 0x10;
    if (prev.posX != next.posX || prev.posZ != next.posZ) *mask |= 0x01;   // one bit for the pair
    if (prev.squad != next.squad)                   *mask |= 0x20;
}

// The TU's own network-stats registration (static-init _0ii_109: the object at ds:0x8AAC75C,
// "SpawnGroup", colour (250, 100, 250); getNetUpdate's accumulator add lands in its +4).
namespace {
Vec3 spawnGroupStatsColour()
{
    Vec3 c; c.x = 250.0f; c.y = 100.0f; c.z = 250.0f;
    return c;
}
io::RegistredNetworkStats g_spawnGroupStats("SpawnGroup", spawnGroupStatsColour());
} // namespace

// Writes a six-bit mask, then has-points, team, active, AI-only, quantized position,
// and squad+1 in that order. Baseline and precision arguments are unused.
bool SpawnGroup::getNetUpdate(io::BitStream& stream, io::BaseLineData* /*baseline*/,
                              unsigned int stateMask, int /*precision*/)
{
    m_updateStartBitPos = stream.getBitPosition();   // NetworkableBase +0x30 (this base is +4)
    m_next = m_head;                                 // pin the send slot

    stream.writeBits(&stateMask, 6);

    const BaseLine& record = m_baselines[m_next];

    if (stateMask & 0x10)
    {
        unsigned int bit = record.hasSpawnPoints ? 1u : 0u;
        stream.writeBits(&bit, 1);
    }
    if (stateMask & 0x02)
    {
        unsigned int team = static_cast<unsigned int>(record.team);
        stream.writeBits(&team, 4);
    }
    if (stateMask & 0x04)
    {
        unsigned int bit = record.active ? 1u : 0u;
        stream.writeBits(&bit, 1);
    }
    if (stateMask & 0x08)
    {
        unsigned int bit = record.onlyForAI ? 1u : 0u;
        stream.writeBits(&bit, 1);
    }
    if (stateMask & 0x01)
    {
        unsigned int posX = record.posX;
        stream.writeBits(&posX, 8);
        unsigned int posZ = record.posZ;
        stream.writeBits(&posZ, 8);
    }
    if (stateMask & 0x20)
    {
        unsigned int squad = static_cast<unsigned int>(record.squad + 1);
        stream.writeBits(&squad, 8);
    }

    if (stream.hasError())
    {
        stream.clearError();
        return false;
    }
    g_spawnGroupStats.m_writeBits +=
        static_cast<int>(stream.getBitPosition() - m_updateStartBitPos);
    return true;
}

// Reads changed spawn-group fields into the next baseline, carrying forward unsent values.
// Position and squad are both decoded and both applied (position first).
// Decoded positions have zero Y. World-size settings are reread for each update;
// odd world sizes retain the writer/reader quantization difference.
void SpawnGroup::setNetUpdate(io::BitStream& stream, float /*time*/, bool ghostActive,
                              io::BaseLineData* /*baseline*/, int /*precision*/)
{
    // Save the cursor for the stream's current mode; there is no error-based early return.
    m_updateStartBitPos = stream.getBitPosition();      // NetworkableBase +0x30 (this base is +4)
    makeRoomForNextUpdate(ghostActive);                  // on the +4 base

    if (m_head != m_next)                               // carry the previous slot forward
        m_baselines[m_next] = m_baselines[m_head];      // sixteen bytes

    unsigned int stateMask = 0;
    stream.readBits(&stateMask, 6);

    BaseLine& record = m_baselines[m_next];

    if (stateMask & 0x10)
    {
        unsigned int bit = 0;
        stream.readBits(&bit, 1);
        record.hasSpawnPoints = static_cast<unsigned char>(bit);
    }
    if (stateMask & 0x02)
    {
        unsigned int team = 0;
        stream.readBits(&team, 4);
        record.team = static_cast<int>(team);
    }
    if (stateMask & 0x04)
    {
        unsigned int bit = 0;
        stream.readBits(&bit, 1);
        record.active = static_cast<unsigned char>(bit);
    }
    if (stateMask & 0x08)
    {
        unsigned int bit = 0;
        stream.readBits(&bit, 1);
        record.onlyForAI = static_cast<unsigned char>(bit);
    }
    if (stateMask & 0x01)
    {
        unsigned int posX = 0;
        stream.readBits(&posX, 8);
        record.posX = static_cast<unsigned char>(posX);
        unsigned int posZ = 0;
        stream.readBits(&posZ, 8);
        record.posZ = static_cast<unsigned char>(posZ);
    }
    if (stateMask & 0x20)
    {
        unsigned int squad = 0;
        stream.readBits(&squad, 8);
        record.squad = static_cast<int>(squad) - 1;     // the writer sent squad + 1
    }

    // this one is the real error flag: 0x8111F9C reads the byte at +0x1C, which is `m_error`.
    if (stream.hasError())
        stream.clearError();

    // Measure the consumed bits for statistics.
    g_spawnGroupStats.m_readBits +=
        static_cast<int>(stream.getBitPosition() - m_updateStartBitPos);

    if (stateMask == 0)                                 // nothing changed - no apply
        return;

    // 6: the apply. A second pass, AND NOT IN THE read'S order.
    const int worldSizeX = settings->intGet(std::string("GLSWorldSizeX"), 0);
    const int worldSizeZ = settings->intGet(std::string("GLSWorldSizeZ"), 0);

    if (stateMask & 0x10)
        m_hasSpawnPoints = record.hasSpawnPoints != 0;                    // +0x74, a direct store
    if (stateMask & 0x02)
        setTeam(record.team);                                             // ISpawnGroup +0x18
    if (stateMask & 0x04)
        setActive(record.active != 0);                                    // +0x28
    if (stateMask & 0x08)
        m_onlyForAI = record.onlyForAI != 0;                              // +0x6E, a direct store

    if (stateMask & 0x01)
    {
        Vec3 position;
        position.x = static_cast<float>(worldSizeX) * (static_cast<float>(record.posX) / 255.0f)
                     - static_cast<float>(worldSizeX / 2);
        position.y = 0.0f;                                                // never on the wire
        position.z = static_cast<float>(worldSizeZ) * (static_cast<float>(record.posZ) / 255.0f)
                     - static_cast<float>(worldSizeZ / 2);
        setPos(position);                                                 // +0x30, BY value
    }
    // A second if, not an else: retail lays the setPos block out of line and returns from it to
    // the squad test (0x811222D call +0x30, 0x8112230 jmp 0x811211C `and edi, 0x20`). An else
    // dropped the squad of every group whose first update also carried its position, leaving the
    // constructor's -1 ("no squad", offered to the whole team): squad-leader spawn points appeared
    // for players outside the squad.
    if (stateMask & 0x20)
    {
        setSquad(record.squad);                                           // +0x20
    }
#if defined(__APPLE__)
    // [TEMP] [CHOSEN, not recovered] [RETIRE-WHEN: squad-spawn-filter-validated]
    // BF2_SPAWNGROUP_TRACE=1: each applied client spawn-group update.
    static const bool trace = std::getenv("BF2_SPAWNGROUP_TRACE") != 0;
    if (trace)
        std::fprintf(stderr, "[SPAWNGROUP] %p mask=0x%02x team=%d squad=%d active=%d points=%d aiOnly=%d\n",
                     (void*) this, stateMask, getTeam(), getSquad(), m_active ? 1 : 0, m_hasSpawnPoints ? 1 : 0,
                     m_onlyForAI ? 1 : 0);
#endif
}

} // namespace hfe
} // namespace dice
