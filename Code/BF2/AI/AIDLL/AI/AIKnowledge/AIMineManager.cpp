// Source placement: AIDLL_r.dll __FILE__ "Code\BF2\AI\AIDLL\AI\AIKnowledge\AIMineManager.cpp" (asserts in 0x100B3B60 / 0x100B3C60).
// static init: ELF 0x2E17C0 (__tcf_0 0x2E1820, _GLOBAL__I 0x2E1850): <iostream>'s Init object only.
#include <iostream>

#include "AIMineManager.h"

#include <cmath>
#include <string>

#include "../../../../Core/ClassManager.h"
#include "../../../AIInterface/BFKnowledge/BFObjectEnvironment.h"   // IAIObjectEnvironment::isAsleep
#include "../../../AIInterface/BFKnowledge/BFTimer.h"               // IBFTimer::getGameTime
#include "../Main/AIRandom.h"
#include "../Main/CommonSingletonServer.h"
#include "../Main/IAIPathfinding.h"

namespace dice {
namespace hfe {

extern ClassManager* classManager;

namespace ai {

namespace {

// The Mat4's translation row, read as a Vec3. `m[12]`, `m[13]` and `m[14]` are three contiguous
// floats and the binary passes their address straight to `IAIPathfinding::getPolygon`, which takes a
// `const Vec3&`. [CHOSEN] packaging - the recovered `Mat4` has no translation accessor - and it is
// the same reinterpretation the compiler performed.
inline const Vec3& translationOf(const Mat4& m)
{
    return *reinterpret_cast<const Vec3*>(&m.m[12]);
}

} // anonymous namespace

// IAIMineManager
// ELF 0x2E9700 / 0x2E97A0
IAIMineManager::IAIMineManager()
{
    // the guard reads the roster static, not the registry. Reproduced.
    if (!CommonSingletonServer::m_iAIMineManager)
        classManager->registerSimpleSingleton("IAIMineManager", this);
}

// ELF 0x2E9840 / 0x2E98D0 / 0x2E9960
IAIMineManager::~IAIMineManager()
{
    if (CommonSingletonServer::m_iAIMineManager == this)
        classManager->unregisterSimpleSingleton("IAIMineManager");
}

// Polygon counter records default to zero and support memberwise copying.
// ELF 0x2DEDD0 / 0x2DEDF0
AIMineManager::PolygonData::PolygonData()
    : m_mines(0)
    , m_otherMines(0)
{
}

// ELF 0x2DEE10 / 0x2DEE30
AIMineManager::PolygonData::PolygonData(const PolygonData& other)
    : m_mines(other.m_mines)
    , m_otherMines(other.m_otherMines)
{
}

// Creates an empty mine record with invalid ID, quad-tree and polygon sentinels.
// ELF 0x2DE2C0 / 0x2DE420
AIMineManager::MineInfo::MineInfo()
    : m_id(-1)
    , m_mineType(0)
    , m_quadTree(0xFFFFFFFFu)
    , m_polygon(0xFFFFFFFFu)
    , m_radius(1.0f)
{
    for (int i = 0; i < 16; ++i)
        m_transform.m[i] = 0.0f;
    m_transform.m[0]  = 1.0f;
    m_transform.m[5]  = 1.0f;
    m_transform.m[10] = 1.0f;
    m_transform.m[15] = 1.0f;

    m_corner1.x = 0.0f;
    m_corner1.y = 0.0f;
    m_corner1.z = 0.0f;
    m_corner2.x = 0.0f;
    m_corner2.y = 0.0f;
    m_corner2.z = 0.0f;
}

// Creates a circular mine record. mineAtRest() must fill its quad-tree and polygon indices before use.
// ELF 0x2DE4D0 / 0x2DE610
AIMineManager::MineInfo::MineInfo(unsigned int id, MineType type, const Mat4& transform,
                                  float radius)
    : m_id(static_cast<int>(id))
    , m_mineType(type)
    , m_radius(radius)
{
    // m_quadTree and m_polygon deliberately not initialised. See above.
    for (int i = 0; i < 16; ++i)
        m_transform.m[i] = 0.0f;
    m_transform = transform;

    m_corner1.x = 0.0f;
    m_corner1.y = 0.0f;
    m_corner1.z = 0.0f;
    m_corner2.x = 0.0f;
    m_corner2.y = 0.0f;
    m_corner2.z = 0.0f;
}

// Builds an anti-personnel mine from its triangular footprint. Leaves the quad-tree id,
// polygon id, and radius uninitialized; the anti-personnel path does not read radius.
// ELF 0x2DE750 / 0x2DE8B0
AIMineManager::MineInfo::MineInfo(unsigned int id, MineType type, const Mat4& transform,
                                  const Vec3& corner1, const Vec3& corner2)
    : m_id(static_cast<int>(id))
    , m_mineType(type)
{
    // m_quadTree, m_polygon and m_radius deliberately not initialised. See above.
    for (int i = 0; i < 16; ++i)
        m_transform.m[i] = 0.0f;
    m_transform = transform;

    m_corner1 = corner1;
    m_corner2 = corner2;
}

// ELF 0x2DEA10 / 0x2DEBF0
AIMineManager::MineInfo::MineInfo(const MineInfo& other)
    : m_id(other.m_id)
    , m_mineType(other.m_mineType)
    , m_quadTree(other.m_quadTree)
    , m_polygon(other.m_polygon)
    , m_polygons(other.m_polygons)
    , m_radius(other.m_radius)
{
    for (int i = 0; i < 16; ++i)
        m_transform.m[i] = 0.0f;
    m_transform = other.m_transform;
    m_corner1 = other.m_corner1;
    m_corner2 = other.m_corner2;
}

// Initializes mine tracking with one outer-vector entry for each of the two sides.
// ELF 0x2DEE50 / 0x2DF140
AIMineManager::AIMineManager()
    : m_nextUpdateTime(-1000000000.0)
    , m_int60(-1)
    , m_missingMinePosition{0.0f, 0.0f, 0.0f}
    , m_flag84(false)
{
    // Leaves m_gap64 uninitialized. Creates entries for anti-personnel and anti-tank mines;
    // init later sizes each type's map dimension.
    m_minedPolygons.resize(2);
    m_minesInPolygon.resize(2);
}

// The binary tears down exactly the four containers, in reverse declaration order - the settling
// map, the resting map, then the two nested vectors - and touches none of the six trailing scalars.
// That is what proves +72..+83 are three plain ints rather than another vector, and it is all a C++
// destructor does on its own, so the body is empty.
// ELF 0x2DF430 / 0x2DF6D0 / 0x2DF970
AIMineManager::~AIMineManager()
{
}

// Sizes navigation-island tracking for the given mine type. The type must be valid.
// ELF 0x2DFC10
void AIMineManager::init(MineType type, unsigned int islandCount)
{
    m_minedPolygons[type].resize(islandCount);
    m_minesInPolygon[type].resize(islandCount);
}

// Resets scheduling and scalar state while retaining both mine maps and polygon indices.
// ELF 0x2DFDE0
void AIMineManager::reset()
{
    m_int60 = -1;
    m_flag84 = false;
    m_nextUpdateTime = -1000000000.0;
}

// Returns a reference to the indexed polygon set. Both indices must be valid; a later
// init that grows the inner vector can invalidate the reference.
// ELF 0x2E16F0
const std::set<unsigned int>& AIMineManager::getMinedPolygons(MineType type, unsigned int map) const
{
    return m_minedPolygons[type][map];
}

// Returns the latest recorded mine position and writes its quad tree and polygon; absent mines return the zero fallback.
// ELF 0x2E1720
const Vec3& AIMineManager::getPathfindingPositionOfMine(unsigned int mineId, MineType /*type*/,
                                                 unsigned int& quadTree, unsigned int& polygon) const
{
    const std::map<unsigned int, std::list<MineInfo> >::const_iterator i = m_mines.find(mineId);
    if (i == m_mines.end())
    {
        quadTree = 0xFFFFFFFFu;
        polygon  = 0xFFFFFFFFu;
        return m_missingMinePosition;
    }

    quadTree = i->second.back().m_quadTree;
    polygon  = i->second.back().m_polygon;
    return translationOf(i->second.back().m_transform);
}

// Processes pending mine state at two updates per second.
// The deadline test is strict: an update runs only when now is greater than m_nextUpdateTime.
// ELF 0x2E0390
void AIMineManager::update()
{
    IBFTimer* const timer = static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer);
    const double now = timer->getGameTime();
    if (now <= m_nextUpdateTime)
        return;

    IAIObjectEnvironment* const environment =
        static_cast<IAIObjectEnvironment*>(CommonSingletonServer::m_iaiObjectEnvironment);

    std::map<unsigned int, MineInfo>::iterator i = m_settlingMines.begin();
    while (i != m_settlingMines.end())
    {
        if (environment->isAsleep(i->first))
        {
            mineAtRest(i->second);
            m_settlingMines.erase(i++);
        }
        else
        {
            ++i;
        }
    }

    m_nextUpdateTime = now + 0.5;
}

// Registers a resting mine as a pathfinding hazard and updates affected polygon counts.
// ELF 0x2E08B0
void AIMineManager::mineAtRest(const MineInfo& info)
{
    MineInfo local(info);

    unsigned int seed = 0xFFFFFFFFu;
    IAIPathfinding::instance->getPolygon(translationOf(local.m_transform), local.m_mineType,
                                         seed, local.m_quadTree, 1);

    bool direct = false;
    if (local.m_quadTree == 0xFFFFFFFFu || seed == 0xFFFFFFFFu)
    {
        // the branch is on the mine type, which is also the map index. Non-zero means the vehicle
        // map, i.e. an at mine.
        seed = local.m_mineType ? findSeedPolygonForATMine(local)
                                : findSeedPolygonForAPMine(local);
    }
    else
    {
        direct = true;
    }

    if (seed == 0xFFFFFFFFu)
        return;

    const std::map<unsigned int, std::list<MineInfo> >::iterator entry =
        m_mines.insert(std::make_pair(static_cast<unsigned int>(local.m_id),
                                      std::list<MineInfo>())).first;

    entry->second.push_back(local);

    // front, not back - see the warning above.
    MineInfo& stored = entry->second.front();

    if (direct)
        stored.m_polygon = seed;

    if (stored.m_mineType == 1)
        IAIPathfinding::instance->getPolygonsInRadius(stored.m_polygons, stored.m_mineType,
                                                      stored.m_quadTree, seed,
                                                      translationOf(stored.m_transform),
                                                      stored.m_radius);
    else
        IAIPathfinding::instance->getPolygonsInTriangle(stored.m_polygons, stored.m_mineType,
                                                        stored.m_quadTree, seed,
                                                        translationOf(stored.m_transform),
                                                        stored.m_corner1, stored.m_corner2);

    for (std::list<unsigned int>::const_iterator p = stored.m_polygons.begin();
         p != stored.m_polygons.end(); ++p)
    {
        addNewMineToPolygon(static_cast<MineType>(stored.m_mineType), stored.m_quadTree, *p);
        IAIPathfinding::instance->drawPolygon(stored.m_mineType, stored.m_quadTree, *p, 0, 5.0f);
    }

    if (local.m_mineType != 1)
        return;

    // the projection onto map 0, so infantry pathfinding sees the at mine too
    entry->second.push_back(local);
    MineInfo& other = entry->second.back();
    other.m_mineType = 0;

    IAIPathfinding::instance->getPolygon(translationOf(other.m_transform), other.m_mineType,
                                         seed, other.m_quadTree, 1);

    // assigned before the test, and from a value that may still be -1. The binary's order.
    const bool noQuadTree = (other.m_quadTree == 0xFFFFFFFFu);
    other.m_polygon = seed;
    if (noQuadTree || seed == 0xFFFFFFFFu)
    {
        // the at search, even though m_mineType has just been set to 0. The projected entry
        // keeps the at mine's circular shape; only its map changes. Reproduced.
        seed = findSeedPolygonForATMine(other);
    }

    if (seed == 0xFFFFFFFFu)
        return;

    IAIPathfinding::instance->getPolygonsInRadius(other.m_polygons, other.m_mineType,
                                                  other.m_quadTree, seed,
                                                  translationOf(other.m_transform), other.m_radius);

    // no drawPolygon in this loop, unlike the first. Also the binary's.
    for (std::list<unsigned int>::const_iterator p = other.m_polygons.begin();
         p != other.m_polygons.end(); ++p)
        addNewOtherMineToPolygon(static_cast<MineType>(other.m_mineType), other.m_quadTree, *p);
}

// Probes seven positions along the mine's facing axis, from zero through six metres.
// Stops when both polygon and quad-tree ids are valid; returns the final seed otherwise,
// which may still be the invalid handle.
// ELF 0x2E04D0
unsigned int AIMineManager::findSeedPolygonForAPMine(MineInfo& info)
{
    unsigned int seed = 0xFFFFFFFFu;

    for (int step = 0; ; ++step)
    {
        const float distance = static_cast<float>(step);

        Vec3 probe;
        probe.x = info.m_transform.m[8]  * distance + info.m_transform.m[12];
        probe.y = info.m_transform.m[9]  * distance + info.m_transform.m[13];
        probe.z = info.m_transform.m[10] * distance + info.m_transform.m[14];

        IAIPathfinding::instance->getPolygon(probe, info.m_mineType, seed, info.m_quadTree, 1);

        if (info.m_quadTree != 0xFFFFFFFFu && seed != 0xFFFFFFFFu)
            return seed;

        if (step >= 6)
            return seed;
    }
}

// Tries up to fifteen random positions around the mine to find a seed polygon.
// ELF 0x2E05C0
unsigned int AIMineManager::findSeedPolygonForATMine(MineInfo& info)
{
    AIRandom* const random = AIRandom::m_instance;

    unsigned int seed = 0xFFFFFFFFu;
    int attempts = 0;

    do
    {
        Vec3 probe;
        probe.x = info.m_transform.m[12];
        probe.y = info.m_transform.m[13];
        probe.z = info.m_transform.m[14];

        const double angle = static_cast<double>(random->nextFloat()) * 2.0 * 3.1415927;
        const float distance = (info.m_radius - 2.0f) * random->nextFloat() + 2.0f;

        probe.x += static_cast<float>(sinf(static_cast<float>(angle))) * distance;
        probe.z += static_cast<float>(cosf(static_cast<float>(angle))) * distance;

        IAIPathfinding::instance->getPolygon(probe, info.m_mineType, seed, info.m_quadTree, 1);
        ++attempts;
    }
    while (seed == 0xFFFFFFFFu && attempts <= 14);

    return seed;
}

// Adds a mine to a polygon's counters. Material changes occur on an existing counter's zero-to-one transition.
// ELF 0x2E1090
void AIMineManager::addNewMineToPolygon(MineType type, unsigned int quadTree, unsigned int polygon)
{
    const std::pair<std::map<unsigned int, PolygonData>::iterator, bool> result =
        m_minesInPolygon[type][quadTree].insert(std::make_pair(polygon, PolygonData()));

    if (result.second)
    {
        result.first->second.m_mines = 1;
        m_minedPolygons[type][quadTree].insert(polygon);
        return;
    }

    if (result.first->second.m_mines == 0)
        IAIPathfinding::instance->setMaterial(type, quadTree, polygon, 7);
    ++result.first->second.m_mines;
}

// ELF 0x2E11D0
void AIMineManager::addNewOtherMineToPolygon(MineType type, unsigned int quadTree,
                                             unsigned int polygon)
{
    const std::pair<std::map<unsigned int, PolygonData>::iterator, bool> result =
        m_minesInPolygon[type][quadTree].insert(std::make_pair(polygon, PolygonData()));

    if (result.second)
    {
        result.first->second.m_otherMines = 1;
        m_minedPolygons[type][quadTree].insert(polygon);
        return;
    }

    if (result.first->second.m_otherMines == 0)
        IAIPathfinding::instance->setMaterial(type, quadTree, polygon, 7);
    ++result.first->second.m_otherMines;
}

// Removes a mine from an existing polygon entry. The caller must ensure the entry exists.
// ELF 0x2E14B0
void AIMineManager::removeMineFromPolygon(MineType type, unsigned int island,
                                          unsigned int polygon)
{
    std::map<unsigned int, PolygonData>& polygons = m_minesInPolygon[type][island];

    // No end check - see above.
    const std::map<unsigned int, PolygonData>::iterator i = polygons.find(polygon);
    --i->second.m_mines;

    if (i->second.m_mines == 0 && i->second.m_otherMines == 0)
    {
        polygons.erase(i);
        m_minedPolygons[type][island].erase(polygon);
    }
}

// ELF 0x2E15D0
void AIMineManager::removeOtherMineFromPolygon(MineType type, unsigned int island,
                                               unsigned int polygon)
{
    std::map<unsigned int, PolygonData>& polygons = m_minesInPolygon[type][island];

    const std::map<unsigned int, PolygonData>::iterator i = polygons.find(polygon);
    --i->second.m_otherMines;

    if (i->second.m_otherMines == 0 && i->second.m_mines == 0)
    {
        polygons.erase(i);
        m_minedPolygons[type][island].erase(polygon);
    }
}

// Queues a newly placed mine for later processing; it does not update navigation polygons immediately.
// ELF 0x2DFF00
void AIMineManager::minePlaced(MineType type, unsigned int id, const Mat4& transform, float radius)
{
    if (type == 1)
    {
        // insert, not operator[] - a repeated id is dropped rather than overwriting the entry
        // already settling. Reproduced.
        m_settlingMines.insert(std::make_pair(id, MineInfo(id, type, transform, radius)));
    }
    else
    {
        static float angle = 22.5f;

        const float radians = 0.017453292f * angle;
        const float s = sinf(radians);
        const float c = cosf(radians);

        Vec3 corner1;
        corner1.x = (transform.m[8]  * c + transform.m[0] * s) * radius + transform.m[12];
        corner1.y = (transform.m[9]  * c + transform.m[1] * s) * radius + transform.m[13];
        corner1.z = (transform.m[10] * c + transform.m[2] * s) * radius + transform.m[14];

        Vec3 corner2;
        corner2.x = (transform.m[8]  * c - transform.m[0] * s) * radius + transform.m[12];
        corner2.y = (transform.m[9]  * c - transform.m[1] * s) * radius + transform.m[13];
        corner2.z = (transform.m[10] * c - transform.m[2] * s) * radius + transform.m[14];

        m_settlingMines.insert(std::make_pair(id, MineInfo(id, type, transform, corner1, corner2)));
    }
}

// Removes settling mines first. If absent there, removes a resting mine and releases
// its polygon counters. An id must not exist in both maps.
// ELF 0x2DFE10
void AIMineManager::mineRemoved(unsigned int id)
{
    const std::map<unsigned int, MineInfo>::iterator settling = m_settlingMines.find(id);
    if (settling == m_settlingMines.end())
    {
        std::map<unsigned int, std::list<MineInfo> >::iterator resting = m_mines.find(id);
        if (resting != m_mines.end())
            removeMine(resting);
    }
    else
    {
        m_settlingMines.erase(settling);
    }
}

// Releases all polygon registrations for a mine and erases its entry.
// ELF 0x2E1310
void AIMineManager::removeMine(std::map<unsigned int, std::list<MineInfo> >::iterator& entry)
{
    int index = 0;
    for (std::list<MineInfo>::const_iterator i = entry->second.begin();
         i != entry->second.end(); ++i, ++index)
    {
        const MineInfo& info = *i;

        if (index == 0)
        {
            for (std::list<unsigned int>::const_iterator p = info.m_polygons.begin();
                 p != info.m_polygons.end(); ++p)
                removeMineFromPolygon(static_cast<MineType>(info.m_mineType), info.m_quadTree, *p);
        }
        else
        {
            for (std::list<unsigned int>::const_iterator p = info.m_polygons.begin();
                 p != info.m_polygons.end(); ++p)
                removeOtherMineFromPolygon(static_cast<MineType>(info.m_mineType),
                                           info.m_quadTree, *p);
        }
    }

    m_mines.erase(entry);
}

} // namespace ai
} // namespace hfe
} // namespace dice
