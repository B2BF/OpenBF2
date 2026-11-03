// [FRONTIER] Remaining methods stop by name until implemented.
// [RETIRE-WHEN: aidll-02-skeleton] Complete and validate the remaining BotNavigation operations.

#include "BotNavigation.h"

// <iostream>'s ios_base::Init object is this TU's static initialisation and teardown (ELF
// 0x195420, __tcf_0 0x195480, _GLOBAL__I 0x1954B0).
#include <iostream>

#include "BotMain.h"                       // getBotId / getCurrentEquipment - vtable +0xE8, +0xD8
#include "../Main/CommonSingletonServer.h"
#include "../../../AIInterface/BFKnowledge/BFTimer.h"   // IBFTimer::getGameTime
#include "../AIKnowledge/InformationManager.h"
#include "../AIKnowledge/Information.h"
#include "../AIKnowledge/AIInformationGrid.h"
#include "../AIKnowledge/BFInformationEnvironment.h"
#include "../AIUtils/checkPointer.h"
#include "../Main/AIDllFrontier.h"
#include "../Main/IAIPathfinding.h"
#include "../AIKnowledge/IPIMobile.h"
#include "../AIKnowledge/IPIUnit.h"
#include "../AIKnowledge/IPIPhysical.h"
#include "../AIPathFinding/NavMesh/QuadTree.h"
#include "../../../AIInterface/BFKnowledge/AIPlayer.h"   // IAIPlayer::isParachuting
#include "../../../../IO/Input/PlayerInput.h"
#include "../../../../Core/Debug.h"
#include <cmath>

namespace dice {
namespace hfe {
namespace ai {

namespace {

// [CHOSEN, not recovered] the image's inlined sqrt of a squared length (with its sqrtf retry).
inline float length3(float x, float y, float z)
{
    return std::sqrt(x * x + y * y + z * z);
}

} // namespace

// Creates navigation paths and initializes movement state for the supplied bot.
BotNavigation::BotNavigation(BotMain* owner)
    : m_bot(owner)
    , m_arriveDistance(2.0f)
    , m_float8(2.0f)
    , m_island(-1)
    , m_startPolygon(-1)
    , m_goalPolygon(-1)
    , m_goalMaterial(0)
    // the two paths, in the order the binary builds them: the fine one first.
    , m_polygonPath(checkPointer(new QTPath()))
    , m_int76(-1)
    , m_currentPoint(0)
    , m_clusterPath(checkPointer(new ClusterNodePath()))
    , m_int128(-1)
    , m_double144(0.0)
    , m_double152(0.0)
    , m_double160(0.0)
    , m_double168(0.0)
    // 9,999,999.0f - a distance sentinel, not a time and not a magnitude. See above.
    , m_sentinel188(9999999.0f)
    , m_flag192(false)
    , m_flag193(false)
    , m_flag195(false)
    , m_vehiclePolygon(-1)
    , m_vehicleIsland(-1)
    , m_infantryPolygon(-1)
    , m_infantryIsland(-1)
    , m_int240(0)
    , m_grenadeTime(0.0)
    , m_grenadeFlag(false)
    // Independent sentinel, initialized to -1.0.
    , m_double300(-1.0)
{
    for (int i = 0; i < 3; ++i)
    {
        m_zero176[i] = 0;
    }
    m_tracePosition.x = 0.0f;
    m_tracePosition.y = 0.0f;
    m_tracePosition.z = 0.0f;
    m_grenadePosition.x = 0.0f; m_grenadePosition.y = 0.0f; m_grenadePosition.z = 0.0f;
    m_startPosition.x = 0.0f; m_startPosition.y = 0.0f; m_startPosition.z = 0.0f;
    m_goal.x = 0.0f; m_goal.y = 0.0f; m_goal.z = 0.0f;
    m_pathfinding = 0;                 // +60
    m_quadTree = 0;
    m_largestAngleDiff = 0.0f;
    m_pathLength = 0.0f;
    for (int i = 0; i < 3; ++i)
        m_zero280[i] = 0;
    m_spaceLeft = 0;
    m_spaceRight = 0;
    m_searchStart.x = 0.0f;                // +320..+340
    m_searchStart.y = 0.0f;
    m_searchStart.z = 0.0f;
    m_searchGoal.x = 0.0f;
    m_searchGoal.y = 0.0f;
    m_searchGoal.z = 0.0f;

    // the dead resize - see the block above. Written as what it is rather than dropped.
    m_points.resize(0);
}

// [RETIRE-WHEN: aidll-02-skeleton] Remove when the corresponding implementation is complete and validated.

// ELF 0x18FF30: reset (releases the search resources), then the two owned paths; the set and the
// two vectors are members.
BotNavigation::~BotNavigation()
{
    reset();
    if (m_polygonPath)
        delete m_polygonPath;
    if (m_clusterPath)
        delete m_clusterPath;
}

// Resets navigation, movement flags and progress timing.
void BotNavigation::resetAll()
{
    reset();

    m_int252 = 0;
    m_grenadeFlag = false;
    // Not the constructor's 0.0 - see the block above.
    m_grenadeTime = -9999.0;
    m_largestAngleDiff = 0.0f;
    m_double300 = -1.0;

    clearProgress();
}

// ELF 0x190A70. Releases both search slots, empties the paths and points, and re-reads the
// equipment's last valid positions.
void BotNavigation::reset()
{
    m_flag316 = false;

    if (m_int76 != 0xFFFFFFFFu)
    {
        m_pathfinding->releaseResource(m_int76, m_bot->getBotId());          // slot +0x9C
        m_int76 = 0xFFFFFFFFu;
    }
    if (m_int128 != 0xFFFFFFFFu)
    {
        m_pathfinding->releaseClusterResource(m_int128, m_bot->getBotId());  // slot +0xA4
        m_int128 = 0xFFFFFFFFu;
    }

    // Reaching through both path objects - see the block above.
    m_polygonPath->m_nodes.resize(0);
    m_polygonPath->m_i0 = 0;
    m_polygonPath->m_i20 = 0;

    m_clusterPath->m_nodes.resize(0);
    m_clusterPath->m_i0 = 0;

    m_int56 = 0;

    m_points.resize(0);

    m_int240 = 0;
    m_currentPoint = 0;
    m_double160 = 0.0;
    m_flag195 = false;
    m_flag193 = false;
    m_flag194 = false;
    m_double244 = 0.0;

    // the equipment's last valid positions (no mobile plug-in: unchanged)
    Information* const info = informationManager->getPool()->get(m_bot->getCurrentEquipment());
    if (info)
    {
        IPIMobile* const mobile = info->getMobile();
        if (mobile)
        {
            m_mapIndex = mobile->getVehicleNumber();
            const IPIMobile::ValidPos& vehicle = mobile->getLastValidPosition(m_mapIndex);
            m_vehicleIsland = static_cast<int>(vehicle.quadTree);
            m_vehiclePolygon = static_cast<int>(vehicle.polygon);
            m_infantryIsland = static_cast<int>(mobile->getLastValidInfantryPos().quadTree);
            m_infantryPolygon = static_cast<int>(mobile->getLastValidInfantryPos().polygon);
        }
    }

    // The sentinel is the interesting one - see the block above.
    m_sentinel188 = 9999999.0f;
    m_spaceLeft = 0;
    m_spaceRight = 0;
}

// Clears progress counters and restarts the progress timer at the current game time.
// Paused game time does not accumulate lack of progress.
void BotNavigation::clearProgress()
{
    m_zero176[0] = 0;                      // +176

    // the one non-zero store. See the block above.
    m_double144 =
        static_cast<const IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();

    m_double168 = 0.0;
    m_zero176[1] = 0;                      // +180
    m_zero176[2] = 0;                      // +184
    m_double152 = 0.0;
    m_flag192 = false;
    m_zero280[0] = 0;                      // +280
    m_zero280[1] = 0;                      // +284
    m_zero280[2] = 0;                      // +288
}

// [FRONTIER] Obstacle updates for living, standing bots are not implemented.
// [TEMP] [CHOSEN, not recovered] [RETIRE-WHEN: aidll-02-skeleton]
// Replace the stop when the corresponding navigation behavior is recovered.
namespace {

// ELF 0x197560 (vtable 0x4b6c38): a primary object with a mobile plug-in (+0x38), or any
// information whose type flags carry bit 25 (`testb $0x2,0x7(info)`).
class MobileOrObstaclePredicate : public IAIInformationGridPredicate
{
public:
    virtual bool includeInformation(Information* information) const
    {
        return (information->isPrimary() && information->hasMobile())
            || (information->getObjectTypeFlags() & 0x02000000) != 0;
    }
};

} // namespace

// ELF 0x194B60 (399 bytes). The obstacle list is emptied, then filled from both grids (the
// bool argument 1, then 0) within 50 m of the equipment's position for the bot's side.
// No null guard on the equipment information.
void BotNavigation::updateObstacles()
{
    m_obstacles.resize(0);
    Information* const equipment = informationManager->getInformation(m_bot->getCurrentEquipment());
    const Vec3& position = equipment->getPosition();
    MobileOrObstaclePredicate predicate;
    const IAIInformationEnvironment* const environment =
        static_cast<const IAIInformationEnvironment*>(AISingletonServer::m_iaiInformationEnvironment);
    environment->getObjectsWithinRadius(m_obstacles, position, 50.0f, m_bot->getSide(), true, &predicate);
    environment->getObjectsWithinRadius(m_obstacles, position, 50.0f, m_bot->getSide(), false, &predicate);
    m_int252 = static_cast<int>(m_obstacles.size());
}

// ELF 0x194E40. Empty in the shipped dedicated image.
void BotNavigation::drawPath()
{
}

// ELF 0x192160: active for three seconds of game time after +244.
bool BotNavigation::isActive()
{
    const double now = static_cast<const IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
    return now - m_double244 < 3.0;
}

// ELF 0x192830 (123 bytes).
void BotNavigation::updateOccationally()
{
    updateNavPoints();
    updateClosestDistance();
    if (m_int56 == 2)
    {
        if (hasIllegalPath())
            reset();
        else if (!checkProgress())
            m_flag192 = true;
    }
    if (m_updateSpaceLeftAndRight)
        updateSpaceLeftAndRight();
}

// ELF 0x191320 (372 bytes). Without a path owner (+60), or in state 4, there is nothing to
// search. A pending point reset (+195) empties the points and requests a polygon search.
bool BotNavigation::update(bool& a, bool* b)
{
    if (m_pathfinding == 0)
        return false;
    m_flag316 = false;
    if (m_int56 == 4)
        return false;
    a = false;
    if (b)
        *b = false;
    if (m_flag195)
    {
        m_points.resize(0);
        m_currentPoint = 0;
        m_polygonSearchPending = true;
        m_flag195 = false;
    }
    if (m_useClusterSearch)
        return doClusterSearch(a, b);
    if (!m_polygonSearchPending)
        return true;
    return doPolygonSearch(a, b);
}

// ELF 0x1901B0 (256 bytes). Finds the goal's polygon on the equipment's vehicle map, then starts
// the search; without equipment or a polygon nothing is started.
bool BotNavigation::initPathfinding(const Vec3& to, float arriveDistance, float b)
{
    Information* const info = informationManager->getInformation(m_bot->getCurrentEquipment());
    if (info)
    {
        const int equipmentType = info->getUnit()->getEquipmentType();
        const int vehicle = info->getMobile()->getVehicleNumber();
        unsigned int polygon;
        unsigned int island;
        if (IAIPathfinding::instance->getPolygon(to, vehicle, polygon, island, equipmentType))   // slot +0x44
            return initPathfinding(island, polygon, to, arriveDistance, b);
    }
    m_flag193 = false;
    return false;
}

// ELF 0x1902B0 (1744 bytes). The same goal on the same map and equipment keeps the current path
// (one point; the bot's polygon is the current point's; still on the path; or retrace). A new
// search needs the bot within 10 m of its last valid position and on the goal's island.
bool BotNavigation::initPathfinding(unsigned int island, unsigned int goalPolygon, const Vec3& to,
                                    float arriveDistance, float b)
{
    Information* const info = informationManager->getInformation(m_bot->getCurrentEquipment());
    if (!info)
    {
        m_flag193 = false;
        return false;
    }
    m_updateSpaceLeftAndRight = (info->getObjectTypeFlags() & 0x00200000) != 0;   // byte +6 & 0x20
    if (m_pathfinding == 0)
        m_pathfinding = IAIPathfinding::instance;
    IPIMobile* const mobile = info->getMobile();
    const IPIMobile::ValidPos& valid = mobile->getLastValidPosition();

    const bool sameGoal = std::fabs(to.x - m_goal.x) <= 1.1920929e-07f
                       && std::fabs(to.y - m_goal.y) <= 1.1920929e-07f
                       && std::fabs(to.z - m_goal.z) <= 1.1920929e-07f;
    if (!sameGoal)
        clearProgress();
    if (m_flag193
        && std::fabs(m_goal.x - to.x) <= 1.1920929e-07f
        && std::fabs(m_goal.y - to.y) <= 1.1920929e-07f
        && std::fabs(m_goal.z - to.z) <= 1.1920929e-07f
        && m_mapIndex == mobile->getVehicleNumber()
        && m_equipmentType == info->getUnit()->getEquipmentType())
    {
        if (static_cast<unsigned int>(m_int56) < 2)
            return true;
        if (!m_points.empty() && m_points[m_currentPoint].m_polygon == static_cast<int>(valid.polygon))
            return true;
        if (!m_points.empty() && stillOnPath())
            return true;
        if (m_points.size() > 1)
        {
            retrace();
            return true;
        }
    }

    if (island != 0xFFFFFFFFu && goalPolygon != 0xFFFFFFFFu)
    {
        m_goalPolygon = static_cast<int>(goalPolygon);
        m_goal = to;
        if (valid.quadTree != 0xFFFFFFFFu && valid.polygon != 0xFFFFFFFFu)
        {
            const Vec3& position = info->getPosition();                   // Information slot +0x30
            const float dx = position.x - valid.position.x;
            const float dy = position.y - valid.position.y;
            const float dz = position.z - valid.position.z;
            if (dx * dx + dy * dy + dz * dz <= 100.0f)
            {
                if (island == valid.quadTree)
                {
                    m_polygonSearchPending = true;
                    m_useClusterSearch = true;
                    m_flag192 = false;
                    m_island = static_cast<int>(valid.quadTree);
                    m_pathIsland = static_cast<int>(valid.quadTree);
                    m_startPolygon = static_cast<int>(valid.polygon);
                    m_startPosition = valid.position;
                    m_arriveDistance = arriveDistance;
                    m_float8 = b;
                    m_float8Squared = b * b;
                    m_mapIndex = mobile->getVehicleNumber();
                    const IPIMobile::ValidPos& vehicle = mobile->getLastValidPosition(m_mapIndex);
                    m_vehicleIsland = static_cast<int>(vehicle.quadTree);
                    m_vehiclePolygon = static_cast<int>(vehicle.polygon);
                    m_infantryIsland = static_cast<int>(mobile->getLastValidInfantryPos().quadTree);
                    m_infantryPolygon = static_cast<int>(mobile->getLastValidInfantryPos().polygon);
                    m_equipmentType = info->getUnit()->getEquipmentType();
                    m_flag195 = false;
                    m_int240 = 0;
                    reset();
                    m_flag193 = true;
                    m_radius = info->getPhysical()->getSmallestRadius();       // IPIPhysical slot +0x34
                    m_pathfinding->getMaterial(m_mapIndex, island, static_cast<unsigned int>(m_goalPolygon),
                                               m_goalMaterial);                // slot +0x5C
                    testDirectPath();
                    void* const quadTree = m_pathfinding->getQuadTree(m_mapIndex, static_cast<unsigned int>(m_island));
                    m_flag316 = true;
                    m_quadTree = static_cast<QuadTree*>(quadTree);             // slot +0x64
                    return true;
                }
                Debug(DebugTypeError, "AI/AIDLL/AI/AIBot/BotNavigation.cpp", 0xC4, "")
                    << "Bot " << m_bot->getName() << " is ordered to go to an island that it is not on ("
                    << island << " != " << valid.quadTree << ")";
                m_flag193 = false;
                return false;
            }
        }
    }
    m_flag193 = false;
    m_flag192 = true;
    return false;
}

// ELF 0x1920A0 (192 bytes). Once the cluster path is used up and no search is pending, the last
// point follows the bot's last valid position on the path island.
void BotNavigation::updateLastPoint(Information* info)
{
    const ClusterNodePath* const cluster = m_clusterPath;
    if (cluster->m_nodes.size() <= static_cast<unsigned int>(cluster->m_i0) && !m_useClusterSearch
        && !m_polygonSearchPending && static_cast<int>(m_points.size()) > 0)
    {
        const IPIMobile::ValidPos& valid = info->getMobile()->getLastValidPosition(m_mapIndex);
        if (static_cast<unsigned int>(m_pathIsland) == valid.quadTree)
        {
            NavigationPoint& last = m_points[m_points.size() - 1];
            last.m_position = valid.position;
            last.m_polygon = static_cast<int>(valid.polygon);
        }
    }
}

// ELF 0x1921B0. A parachuting bot is never stuck.
void BotNavigation::setIsStuck(bool stuck)
{
    if (m_bot->getPlayer()->isParachuting())          // Bot +0xBC, IAIPlayer +0x34
        m_flag192 = false;
    else
        m_flag192 = stuck;
}

// ELF 0x192200 (1440 bytes).
bool BotNavigation::updatePath(Vec2& point, bool& arrived, bool& direct, float distance)
{
    static bool useSkipTrace = false;                 // ELF 0x4CE00E (.bss)
    if (m_flag192)
        return false;
    const double now = static_cast<const IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
    m_double244 = now;
    if (!m_flag193)
        return false;
    arrived = false;
    direct = false;
    if (static_cast<unsigned int>(m_int56) < 2)
        m_double244 = 0.0;
    if (m_useClusterSearch || m_flag195 || (m_polygonSearchPending && m_points.size() <= m_currentPoint))
    {
        direct = true;
        return true;
    }

    // a soldier firing at an enemy does not walk onto material 4 polygons
    Information* const target = informationManager->getInformation(m_bot->getFiringTarget());
    if (!m_points.empty() && m_updateSpaceLeftAndRight && m_bot->isFiring() && target)
    {
        if (target->getSide() != m_bot->getSide())
        {
            const unsigned int polygon = m_points.size() < 2
                ? static_cast<unsigned int>(m_goalPolygon)
                : static_cast<unsigned int>(m_points[m_currentPoint].m_polygon);
            unsigned char material;
            if (m_pathfinding->getMaterial(m_mapIndex, static_cast<unsigned int>(m_pathIsland), polygon, material)
                && material == 4)
            {
                direct = true;
                m_double244 = 0.0;
                return true;
            }
        }
    }

    if (m_double168 == -666.0)
    {
        m_bot->getInput()->setInput(static_cast<io::PlayerInputMap>(9), 1.0f);   // PIAction
        m_double168 = now;
    }

    if (checkArrivedAtPoint(distance))
    {
        if (m_currentPoint == m_points.size() - 1)
        {
            m_double244 = 0.0;
            m_sentinel188 = 100000000.0f;
            clearProgress();
            arrived = true;
        }
        else
        {
            ++m_currentPoint;
            if (m_int240 > 0)
                --m_int240;
        }
    }

    ClusterNodePath* const cluster = m_clusterPath;
    bool trim = true;
    if (m_pathLength >= 30.0f && m_currentPoint + 10 < m_points.size())
    {
        const Polygon* const polygon = m_quadTree->getPolygon(static_cast<unsigned int>(m_points[m_currentPoint].m_polygon));
        if (static_cast<unsigned int>(polygon->m_cluster) != static_cast<unsigned int>(cluster->m_i0))
        {
            const Polygon* const again = m_quadTree->getPolygon(static_cast<unsigned int>(m_points[m_currentPoint].m_polygon));
            unsigned short current = 0xFFFF;
            if (cluster->m_i0 < static_cast<int>(cluster->m_nodes.size()))
                current = cluster->m_nodes[cluster->m_i0].first;
            if (again->m_cluster != current)
                trim = false;
        }
    }
    if (trim && static_cast<unsigned int>(cluster->m_i0) + 1 <= cluster->m_nodes.size() && !m_polygonSearchPending)
    {
        // keep at most ten points ahead and search the rest again
        m_polygonSearchPending = true;
        unsigned int keep = static_cast<unsigned int>(m_points.size());
        if (m_currentPoint + 10 < keep)
            keep = m_currentPoint + 10;
        m_points.resize(keep, NavigationPoint());
    }

    if (!m_flag195)
    {
        if (m_int56 != 2 && (!m_polygonSearchPending || static_cast<unsigned int>(m_int56) > 1))
            return false;
        stillOnPath();
        if (!m_flag194)
        {
            m_double160 = now;
        }
        else
        {
            const float delay = m_updateSpaceLeftAndRight ? 2.0f : 0.5f;
            if (static_cast<double>(delay) < now - m_double160)
            {
                m_flag194 = false;
                m_int240 = (useSkipTrace && m_updateSpaceLeftAndRight) ? 1 : 0;
                retrace();
            }
        }
    }
    vec3ToVec2XZ(m_points[m_currentPoint].m_position, point);
    return true;
}

// ELF 0x193EC0 (512 bytes). On the path when the bot's last valid polygon is the previous or the
// current point's, or one behind; or within the next 20 points (which becomes the current one).
bool BotNavigation::stillOnPath()
{
    if (m_points.size() < 2)
        return false;
    Information* const info = informationManager->getInformation(m_bot->getCurrentEquipment());
    const int polygon = static_cast<int>(info->getMobile()->getLastValidPosition().polygon);
    if (polygon == -1)
        return false;
    const unsigned int current = m_currentPoint;
    if (current >= m_points.size())
        return false;
    const int previous = current != 0 ? m_points[current - 1].m_polygon : -1;
    if (polygon == previous || polygon == m_points[current].m_polygon)
        return true;
    for (int i = static_cast<int>(current); i >= 0; --i)
        if (m_points[i].m_polygon == polygon)
            return true;
    unsigned int ahead = static_cast<unsigned int>(m_points.size()) - current;
    if (ahead > 0x14)
        ahead = 0x14;
    const unsigned int end = ahead + current;
    for (unsigned int i = current; i < end; ++i)
    {
        if (m_points[i].m_polygon == polygon)
        {
            m_currentPoint = i;
            const int left = m_int240 - static_cast<int>(i - current);
            m_int240 = left > 0 ? left : 0;
            return true;
        }
    }
    return false;
}

// ELF 0x1910C0 (608 bytes). Restarts from the bot's last valid position on the cluster path:
// at the bot's cluster, or at the first path cluster neighbouring it; otherwise a full reset.
void BotNavigation::retrace()
{
    ClusterNodePath* const cluster = m_clusterPath;
    const unsigned int count = static_cast<unsigned int>(cluster->m_nodes.size());
    if (count < 2)
    {
        m_flag195 = false;
        reset();
        return;
    }
    cluster->m_i0 = 0;
    m_points.resize(0, NavigationPoint());
    Information* const info = informationManager->getInformation(m_bot->getCurrentEquipment());
    const IPIMobile::ValidPos& valid = info->getMobile()->getLastValidPosition();
    m_island = static_cast<int>(valid.quadTree);
    m_startPolygon = static_cast<int>(valid.polygon);
    m_startPosition = valid.position;
    m_flag195 = true;
    const short own = static_cast<short>(m_pathfinding->getClusterOfPolygon(
        m_mapIndex, static_cast<unsigned int>(m_island), valid.polygon));              // slot +0x7C
    for (unsigned int i = 0; i < count - 1; ++i)
    {
        short node = -1;
        if (m_clusterPath->m_i0 < static_cast<int>(m_clusterPath->m_nodes.size()))
            node = static_cast<short>(m_clusterPath->m_nodes[m_clusterPath->m_i0].first);
        if (own == node)
            return;
        ++m_clusterPath->m_i0;
    }
    m_clusterPath->m_i0 = 0;
    for (unsigned int i = 0; i < count - 1; ++i)
    {
        unsigned short node = 0xFFFF;
        if (m_clusterPath->m_i0 < static_cast<int>(m_clusterPath->m_nodes.size()))
            node = m_clusterPath->m_nodes[m_clusterPath->m_i0].first;
        if (m_pathfinding->areNeighbouringClusters(m_mapIndex, static_cast<unsigned int>(m_island),
                                                   static_cast<unsigned short>(own), node))   // slot +0x80
            return;
        ++m_clusterPath->m_i0;
    }
    m_flag195 = false;
    reset();
}

// ELF 0x190DA0 (800 bytes). A goal within 200 m (minDistSqr 40000) that a straight trace reaches
// becomes a one-point path.
void BotNavigation::testDirectPath()
{
    static float minDistSqr = 40000.0f;                // ELF 0x4B6C24 (.data)
    const float dx = m_goal.x - m_startPosition.x;
    const float dy = m_goal.y - m_startPosition.y;
    const float dz = m_goal.z - m_startPosition.z;
    if (!(dx * dx + dy * dy + dz * dz < minDistSqr))
        return;
    Information* const info = informationManager->getInformation(m_bot->getCurrentEquipment());
    if (!info || !info->getMobile())
        return;
    Vec3 hit;
    hit.x = 0.0f; hit.y = 0.0f; hit.z = 0.0f;
    unsigned int polygon;
    unsigned char material;
    if (!m_pathfinding->trace(static_cast<unsigned int>(m_startPolygon), static_cast<unsigned int>(m_pathIsland),
                              m_mapIndex, m_startPosition, m_goal, hit, polygon, m_equipmentType, -1, 0.0f, 5,
                              &material))                                           // slot +0x8C
        return;
    const unsigned int index = static_cast<unsigned int>(m_points.size());
    m_points.resize(index + 1, NavigationPoint());
    NavigationPoint& last = m_points[index];
    last.m_position = m_goal;
    last.m_polygon = static_cast<int>(polygon);
    last.m_point0 = m_goal;
    last.m_pointC = m_goal;
    m_goalPolygon = static_cast<int>(polygon);
    m_goalMaterial = material;
    m_polygonSearchPending = false;
    m_useClusterSearch = false;
    m_currentPoint = 0;
    m_int56 = 2;
    updateClosestDistance();
}

// ELF 0x1949E0 (384 bytes). Within 3 m in height and the arrive distance in the plane: a soldier
// uses soldierMinDist; a vehicle max(+4, 1), at the last point max(radius - 1, that).
bool BotNavigation::checkArrivedAtPoint(float)
{
    static float soldierMinDist = 0.5f;               // ELF 0x4B6C30 (.data)
    // The image reads BotMain +0x8C (m_currentEquipment) inline; getCurrentEquipment returns it.
    Information* const info = informationManager->getInformation(m_bot->getCurrentEquipment());
    const Vec3& position = info->getPosition();
    if (m_points.empty())
        return false;
    const NavigationPoint& p = m_points[m_currentPoint];
    if (!(std::fabs(position.y - p.m_position.y) <= 3.0f))
        return false;
    float distance = soldierMinDist;
    if (!m_updateSpaceLeftAndRight)
    {
        distance = m_arriveDistance;
        if (m_arriveDistance <= 1.0f)
            distance = 1.0f;
    }
    float limit = distance;
    if (!m_updateSpaceLeftAndRight && m_currentPoint == m_points.size() - 1)
    {
        limit = m_radius - 1.0f;
        if (limit <= distance)
            limit = distance;
    }
    const float x = position.x - p.m_position.x;
    const float z = position.z - p.m_position.z;
    return z * z + x * x + 0.0f < limit * limit;
}

// ELF 0x1940C0.
bool BotNavigation::getSecondPoint(Vec2& out)
{
    const unsigned int next = m_currentPoint + 1;
    const bool exists = next < m_points.size();
    if (exists)
        vec3ToVec2XZ(m_points[next].m_position, out);
    return exists;
}

// ELF 0x194120 (1408 bytes). Every 0.5 s: the largest absolute heading change between the bot's
// facing and the path legs within max(speed * timeStep, 3) metres ahead; cached in +308.
bool BotNavigation::getLargestAngleDiff(float& out)
{
    static float timeStep = 2.0f;                     // ELF 0x4B6C2C (.data)
    const double now = static_cast<const IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
    if (!(now - m_double300 >= 0.5))
    {
        out = m_largestAngleDiff;
        return true;
    }
    m_double300 = now;
    // The image reads BotMain +0x8C inline (no null guard).
    Information* const info = informationManager->getInformation(m_bot->getCurrentEquipment());
    const Vec3& position = info->getPosition();
    unsigned int index = m_currentPoint;
    const Vec3& first = m_points[index].m_position;
    float covered = length3(position.x - first.x, position.y - first.y, position.z - first.z);
    Vec2 facing;
    facing.x = 0.0f; facing.y = 0.0f;
    const Vec3 faceing = info->getFaceing();                     // Information slot +0x40
    vec3ToVec2XZ(faceing, facing);
    out = 0.0f;
    float range = info->getMobile()->getSpeedMagnitude() * timeStep;   // IPIMobile slot +0x24
    if (range <= 3.0f)
        range = 3.0f;
    for (; covered < range && index < m_points.size(); ++index)
    {
        Vec3 from;
        if (index == 0)
            from = info->getPosition();
        else
            from = m_points[index - 1].m_position;
        const Vec3& to = m_points[index].m_position;
        covered = length3(from.x - to.x, from.y - to.y, from.z - to.z) + covered;
        Vec3 leg;
        leg.x = to.x - from.x; leg.y = to.y - from.y; leg.z = to.z - from.z;
        Vec2 dir;
        dir.x = 0.0f; dir.y = 0.0f;
        vec3ToVec2XZ(leg, dir);
        const float lengthSq = dir.x * dir.x + dir.y * dir.y;
        if (std::fabs(lengthSq - 1.0f) >= 1.1920929e-07f)
        {
            if (std::fabs(lengthSq) < 1.4210855e-14f)
            {
                dir.x = 0.0f; dir.y = 0.0f;
            }
            else
            {
                const float inv = 1.0f / std::sqrt(lengthSq);
                dir.x = dir.x * inv; dir.y = inv * dir.y;
            }
        }
        const float diff = std::fabs(getSignedDiffAngleToDir(facing, dir));
        if (out < diff)
            out = diff;
    }
    m_largestAngleDiff = out;
    return true;
}

// ELF 0x194E50 (192 bytes). The first point from the current one farther than `distance` from
// `from` in the plane; `out` keeps the last point looked at.
void BotNavigation::getFirstPointBeyondDist(const Vec2& from, float distance, Vec2& out)
{
    for (unsigned int i = m_currentPoint; i < m_points.size(); ++i)
    {
        const Vec3& p = m_points[i].m_position;
        out.x = p.x;
        out.y = p.z;
        const float dx = from.x - p.x;
        const float dz = from.y - p.z;
        if (distance * distance < dz * dz + dx * dx)
            return;
    }
}

// ELF 0x195280. The game-time query's result is discarded, as in the image.
void BotNavigation::setGrenadeWarning(const Vec3& position, double time, bool b)
{
    static_cast<const IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
    m_grenadeTime = time;
    m_grenadePosition = position;
    m_grenadeFlag = b;
}

// ELF 0x195310.
bool BotNavigation::getGrenadeWarning(Vec3& position, double& timeLeft)
{
    const double now = static_cast<const IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
    position = m_grenadePosition;
    timeLeft = 2.8 - (now - m_grenadeTime);
    return timeLeft > 0.0;
}

// ELF 0x1914B0 (1535 bytes). A local (polygon) search for the next path segment: the first call
// acquires a search resource (none free: *blocked), turns on topology smoothing off the trace map,
// and allows the start's cluster, the next three clusters of the cluster path and the segment
// goal's cluster; the segment runs from the last point (or the start) to the cluster portal two
// steps ahead, or to the goal. Each call runs the search (IAIPathfinding +0x24): 1 is in progress
// (*finished = true, true); 2 found - the points are rebuilt and optimised, the goal appended when
// the segment ended there (and the cluster path emptied), the cluster step advanced; 3 / 4 failed.
bool BotNavigation::doPolygonSearch(bool& finished, bool* blocked)
{
    if (m_int76 == 0xFFFFFFFFu)
    {
        if (!m_pathfinding->acquireResource(m_int76, m_bot->getBotId()))
        {
            if (blocked)
                *blocked = true;
            return false;
        }
        if (m_mapIndex != m_pathfinding->getTraceMapIndex())
            m_pathfinding->setUsePathTopologySmoothing(m_int76, true);
        if (m_points.empty())
        {
            m_searchStart = m_startPosition;
            m_searchStartPolygon = static_cast<unsigned int>(m_startPolygon);
        }
        else
        {
            const NavigationPoint& last = m_points.back();
            m_searchStart = last.m_position;
            m_searchStartPolygon = static_cast<unsigned int>(last.m_polygon);
        }
        m_set132.clear();
        m_set132.insert(m_quadTree->getPolygon(m_searchStartPolygon)->m_cluster);
        m_searchGoalIsFinal = false;
        if (!m_clusterPath->getPoint(m_searchGoal, m_searchGoalPolygon, static_cast<unsigned int>(m_clusterPath->m_i0 + 2)))
        {
            m_searchGoal = m_goal;
            m_searchGoalIsFinal = true;
            m_searchGoalPolygon = static_cast<unsigned int>(m_goalPolygon);
        }
        for (int k = 0; k < 3; ++k)
        {
            const unsigned int index = static_cast<unsigned int>(k + m_clusterPath->m_i0);
            const unsigned int nodes = static_cast<unsigned int>(m_clusterPath->m_nodes.size());
            if (nodes < index)
                break;
            const unsigned short cluster =
                static_cast<int>(index) < static_cast<int>(nodes) ? m_clusterPath->m_nodes[index].first : 0xFFFF;
            m_set132.insert(cluster);
        }
        m_set132.insert(m_quadTree->getPolygon(m_searchGoalPolygon)->m_cluster);
        m_pathfinding->setAllowedClusters(m_int76, &m_set132);
    }
    m_int56 = m_pathfinding->localSearch(static_cast<int>(m_int76), m_mapIndex, static_cast<unsigned int>(m_island),
                                         m_searchStartPolygon, m_searchStart, m_searchGoalPolygon, m_searchGoal,
                                         m_equipmentType, m_polygonPath);
    if (m_int56 != 1)
    {
        m_pathfinding->releaseResource(m_int76, m_bot->getBotId());
        m_int76 = 0xFFFFFFFFu;
        if (m_int56 == 2)
        {
            calculatePoints();
            if (m_searchGoalIsFinal)
            {
                const unsigned int n = static_cast<unsigned int>(m_points.size());
                m_points.resize(n + 1, NavigationPoint());
                m_points[n].m_position = m_goal;
                m_points[n].m_polygon = m_goalPolygon;
                m_clusterPath->m_nodes.resize(0);
                m_clusterPath->m_i0 = 0;
            }
            m_flag192 = false;
            m_quadTree = static_cast<QuadTree*>(
                m_pathfinding->getQuadTree(m_mapIndex, static_cast<unsigned int>(m_polygonPath->m_island)));
            optimizePoints();
            updateNavPoints();
            updateNavPoints();
            ++m_clusterPath->m_i0;
            m_polygonSearchPending = false;
            return true;
        }
        if (static_cast<unsigned int>(m_int56 - 3) < 2)
            return false;
    }
    finished = true;
    return true;
}

// ELF 0x191AB0 (315 bytes). The coarse (cluster) search: on success the polygon search takes
// over (+0x44 set, state 0); a failure (3 / 4) resets the navigation.
bool BotNavigation::doClusterSearch(bool& finished, bool* blocked)
{
    if (m_int128 == 0xFFFFFFFFu)
    {
        if (!m_pathfinding->acquireClusterResource(m_int128, m_bot->getBotId()))
        {
            if (blocked)
                *blocked = true;
            return false;
        }
    }
    m_int56 = m_pathfinding->clusterSearch(static_cast<int>(m_int128), m_mapIndex, static_cast<unsigned int>(m_island),
                                           static_cast<unsigned int>(m_startPolygon), m_startPosition,
                                           static_cast<unsigned int>(m_goalPolygon), m_goal, m_equipmentType,
                                           m_clusterPath);
    if (m_int56 != 1)
    {
        m_pathfinding->releaseClusterResource(m_int128, m_bot->getBotId());
        m_int128 = 0xFFFFFFFFu;
        if (m_int56 == 2)
        {
            m_useClusterSearch = false;
            m_polygonSearchPending = true;
            m_int56 = 0;
            return true;
        }
        if (static_cast<unsigned int>(m_int56 - 3) < 2)
        {
            reset();
            return false;
        }
    }
    finished = true;
    return true;
}

// ELF 0x194CF0: within the smallest radius of a non-soldier obstacle (+0x58) with a physical.
bool BotNavigation::pointWithinObstacleRadius(const Vec3& point)
{
    for (unsigned int i = 0; i < m_obstacles.size(); ++i)
    {
        Information* const info = informationManager->getInformation(m_obstacles[i]);
        if (!info || (info->getObjectTypeFlags() & 0x00200000u) || !info->getPhysical())
            continue;
        const Vec3& p = info->getPosition();
        const float radius = info->getPhysical()->getSmallestRadius();
        if ((point.x - p.x) * (point.x - p.x) + (point.y - p.y) * (point.y - p.y) + (point.z - p.z) * (point.z - p.z)
            < radius * radius)
            return true;
    }
    return false;
}

// ELF 0x1932A0. The local path's steps become navigation points, appended after the last point
// (or replacing all when the current point is past the end): each point's position and polygon
// from the step, and its portal edge (+0 / +0xC) from IAIPathfinding +0x70 getEdge(map, previous
// entered-from polygon, polygon, island), or the position twice for the first step. The path is
// emptied afterwards.
void BotNavigation::calculatePoints()
{
    m_polygonPath->m_i0 = 0;
    QTPath* path = m_polygonPath;
    const unsigned int island = static_cast<unsigned int>(path->m_island);
    const unsigned int steps = static_cast<unsigned int>(path->m_nodes.size());
    const unsigned int count = static_cast<unsigned int>(m_points.size());
    unsigned int base = count - 1;
    if (count <= m_currentPoint)
        base = 0;
    unsigned int size = 0;
    if (static_cast<unsigned int>(path->m_i0) <= steps + base)
        size = steps + base - static_cast<unsigned int>(path->m_i0);
    int from = -1;
    if (path->m_i0 < static_cast<int>(steps))
        from = static_cast<int>(path->m_nodes[path->m_i0].second);
    int polygon = -1;
    if (path->m_i0 < static_cast<int>(path->m_nodes.size()))
        polygon = static_cast<int>(path->m_nodes[path->m_i0].first);
    m_points.resize(size, NavigationPoint());
    path = m_polygonPath;
    unsigned int k = base;
    while (static_cast<unsigned int>(path->m_i0) < steps)
    {
        int entered = polygon;
        NavigationPoint& point = m_points[k];
        point.m_position = *path->getPosition(static_cast<unsigned int>(path->m_i0));
        point.m_polygon = static_cast<int>(m_polygonPath->getPolygon(static_cast<unsigned int>(m_polygonPath->m_i0)));
        if (from == -1)
        {
            point.m_point0 = *m_polygonPath->getPosition(static_cast<unsigned int>(m_polygonPath->m_i0));
            point.m_pointC = *m_polygonPath->getPosition(static_cast<unsigned int>(m_polygonPath->m_i0));
        }
        else
        {
            m_pathfinding->getEdge(point.m_point0, point.m_pointC, m_mapIndex, static_cast<unsigned int>(from),
                                   static_cast<unsigned int>(entered), island);
        }
        ++m_polygonPath->m_i0;
        path = m_polygonPath;
        int next = -1;
        if (path->m_i0 < static_cast<int>(path->m_nodes.size()))
            next = static_cast<int>(path->m_nodes[path->m_i0].second);
        if (next != -1)
            entered = next;
        polygon = -1;
        if (path->m_i0 < static_cast<int>(path->m_nodes.size()))
            polygon = static_cast<int>(path->m_nodes[path->m_i0].first);
        ++k;
        from = entered;
    }
    path->m_nodes.resize(0);
    path->m_i0 = 0;
    path->m_i20 = 0.0f;
}

// ELF 0x1937D0. Two passes pull each point (5 m or more from the bot) onto its portal edge where
// the line from the previous point (the bot first) to the next point crosses it, kept a margin
// (max(0.8 radius, 1), at most half the edge) from the edge's ends; a final pass places every
// point where that line crosses its edge, when it does within the edge.
void BotNavigation::optimizePoints()
{
    if (m_points.empty())
        return;
    Vec2 from;
    from.x = 0.0f;
    from.y = 0.0f;
    // BotMain +0x8C read inline (here through the accessor).
    Information* const info = informationManager->getInformation(m_bot->getCurrentEquipment());
    vec3ToVec2XZ(info->getPosition(), from);
    for (int pass = 0; pass < 2; ++pass)
    {
        for (unsigned int i = 0; i < m_points.size() - 1; ++i)
        {
            const Vec3& p = info->getPosition();
            const Vec3& at = m_points[i].m_position;
            const float dx = p.x - at.x;
            const float dy = p.y - at.y;
            const float dz = p.z - at.z;
            if (25.0f <= dx * dx + dy * dy + dz * dz)
            {
                Vec2 next, a, b;
                next.x = 0.0f; next.y = 0.0f;
                a.x = 0.0f; a.y = 0.0f;
                b.x = 0.0f; b.y = 0.0f;
                vec3ToVec2XZ(m_points[i + 1].m_position, next);
                vec3ToVec2XZ(m_points[i].m_point0, a);
                vec3ToVec2XZ(m_points[i].m_pointC, b);
                float s;
                float t;
                if (lineIntersect(from, next, a, b, s, t))
                {
                    const float ex = b.x - a.x;
                    const float ey = b.y - a.y;
                    const float length = std::sqrt(ey * ey + ex * ex);
                    const float along = length * t;
                    const float half = length * 0.5f;
                    float margin = m_radius * 0.8f;
                    if (margin <= 1.0f)
                        margin = 1.0f;
                    if (half <= margin)
                        margin = half;
                    if (half <= along)
                    {
                        if (length - margin < along)
                            t = 1.0f - margin / length;
                    }
                    else if (along < margin)
                    {
                        t = margin / length;
                    }
                    NavigationPoint& point = m_points[i];
                    const float u = t <= 1.0f ? t : 1.0f;
                    point.m_position.x = point.m_point0.x + (point.m_pointC.x - point.m_point0.x) * u;
                    point.m_position.y = (point.m_pointC.y - point.m_point0.y) * u + point.m_point0.y;
                    point.m_position.z = (point.m_pointC.z - point.m_point0.z) * u + point.m_point0.z;
                }
            }
            vec3ToVec2XZ(m_points[i].m_position, from);
        }
    }
    vec3ToVec2XZ(info->getPosition(), from);
    for (unsigned int i = 0; i < m_points.size() - 1; ++i)
    {
        Vec2 next, a, b;
        next.x = 0.0f; next.y = 0.0f;
        a.x = 0.0f; a.y = 0.0f;
        b.x = 0.0f; b.y = 0.0f;
        vec3ToVec2XZ(m_points[i + 1].m_position, next);
        vec3ToVec2XZ(m_points[i].m_point0, a);
        vec3ToVec2XZ(m_points[i].m_pointC, b);
        float s;
        float t;
        if (lineIntersect(from, next, a, b, s, t) && 0.0f <= t && t <= 1.0f)
        {
            NavigationPoint& point = m_points[i];
            point.m_position.x = (point.m_pointC.x - point.m_point0.x) * t + point.m_point0.x;
            point.m_position.y = (point.m_pointC.y - point.m_point0.y) * t + point.m_point0.y;
            point.m_position.z = (point.m_pointC.z - point.m_point0.z) * t + point.m_point0.z;
        }
        vec3ToVec2XZ(m_points[i].m_position, from);
    }
}

// ELF 0x192C00 (670 bytes). Following a path (state 2) from a valid position: unless +240 is set,
// when the current point is within 10 m (soldier) / 20 m, skip ahead while the next point is
// directly reachable (trace with clearance 0.5 / 0, 8 / 30, at most 1 / 4 points); otherwise a
// failed trace to the current point sets +194. minClearanceVehicle is a zero-initialised static
// (.bss 0x4CE010, no guard); minClearanceSoldier is 0.5 (0x4B6C28).
void BotNavigation::updateNavPoints()
{
    static float minClearanceSoldier = 0.5f;
    static float minClearanceVehicle;

    Information* const info = informationManager->getInformation(m_bot->getCurrentEquipment());
    if (!info || !info->getMobile() || m_int56 != 2)
        return;
    if (info->getMobile()->getLastValidPosition().polygon == 0xFFFFFFFFu)
        return;
    Vec3 hit;
    hit.x = 0.0f;
    hit.y = 0.0f;
    hit.z = 0.0f;
    const Vec3& position = info->getPosition();
    const bool soldier = m_updateSpaceLeftAndRight;
    const int steps = soldier ? 8 : 30;
    const float range = soldier ? 10.0f : 20.0f;
    bool advanced = false;
    const float clearance = soldier ? minClearanceSoldier : minClearanceVehicle;
    unsigned int polygon;
    if (m_int240 == 0)
    {
        const Vec3& current = m_points[m_currentPoint].m_position;
        const float dx = position.x - current.x;
        const float dy = position.y - current.y;
        const float dz = position.z - current.z;
        if (dz * dz + dx * dx + dy * dy < range * range)
        {
            for (int i = 0; i < (soldier ? 1 : 4); ++i)
            {
                if (m_points.size() - 1 <= m_currentPoint
                    || !m_pathfinding->trace(this, false, m_points[m_currentPoint + 1].m_position, hit, polygon,
                                             m_equipmentType, steps, clearance, 5, 0))
                    break;
                ++m_currentPoint;
                advanced = true;
            }
            if (advanced)
                return;
        }
    }
    if (!m_pathfinding->trace(this, false, m_points[m_currentPoint].m_position, hit, polygon, m_equipmentType, -1,
                              0.0f, 0, 0))
        m_flag194 = true;
}

// ELF 0x1946A0 (824 bytes). +0xBC: the distance from the bot along the remaining points, plus
// the remaining cluster path (its points 1.. from the last navigation point); +0x138 the points
// part. No points: 0. ClusterNodePath::getPoint is indexed from 1, not from the current step,
// as in the image.
void BotNavigation::updateClosestDistance()
{
    if (m_points.empty())
    {
        m_sentinel188 = 0.0f;
        return;
    }
    // The image reads BotMain +0x8C inline (no null guard).
    Information* const info = informationManager->getInformation(m_bot->getCurrentEquipment());
    const Vec3& position = info->getPosition();
    const Vec3& current = m_points[m_currentPoint].m_position;
    float distance = length3(position.x - current.x, position.y - current.y, position.z - current.z);
    unsigned int i = m_currentPoint + 1;
    for (; i < m_points.size(); ++i)
    {
        const Vec3& a = m_points[i - 1].m_position;
        const Vec3& b = m_points[i].m_position;
        distance = length3(a.x - b.x, a.y - b.y, a.z - b.z) + distance;
    }
    ClusterNodePath* const cluster = m_clusterPath;
    m_pathLength = distance;
    const unsigned int nodes = static_cast<unsigned int>(cluster->m_nodes.size());
    if (static_cast<unsigned int>(cluster->m_i0) < nodes)
    {
        const unsigned int remaining = nodes - static_cast<unsigned int>(cluster->m_i0);
        Vec3 previous = m_points[i - 1].m_position;
        for (unsigned int k = 1; k < remaining; ++k)
        {
            Vec3 point;
            point.x = 0.0f; point.y = 0.0f; point.z = 0.0f;
            unsigned int polygon;
            m_clusterPath->getPoint(point, polygon, k);
            distance = length3(previous.x - point.x, previous.y - point.y, previous.z - point.z) + distance;
            previous = point;
        }
    }
    m_sentinel188 = distance;
}

// ELF 0x1927A0 (130 bytes): a point on a missing or non-walkable (material 0) polygon.
bool BotNavigation::hasIllegalPath()
{
    for (unsigned int i = 0; i < m_points.size(); ++i)
    {
        const Polygon* const polygon = m_quadTree->getPolygon(static_cast<unsigned int>(m_points[i].m_polygon));
        if (!polygon || polygon->m_material == 0)
            return true;
    }
    return false;
}

namespace {

// Bot +4 inputs read inline: an unmapped input reads 0 (`(mappingFlags >> i) & 1`).
float mappedInput(const io::PlayerInput* input, io::PlayerInputMap index)
{
    return ((input->mappingFlags >> static_cast<unsigned int>(index)) & 1ULL) ? input->inputs[index] : 0.0f;
}

float distanceSq(const float* a, const Vec3& b)
{
    return (a[0] - b.x) * (a[0] - b.x) + (a[1] - b.y) * (a[1] - b.y) + (a[2] - b.z) * (a[2] - b.z);
}

} // namespace

// ELF 0x191BF0 (1192 bytes). The action input is released one second after +168. While a path
// is followed (state 2): no move input (max of throttle, sprint, yaw within 0.1) for 0.1 s
// (soldier) / 0.5 s restamps +144 and is progress; otherwise the bot progresses when it got
// 2 m (soldier) / 4 m from +280 (restamped then) or when it stayed within that for less than
// 10 s / 20 s since +144. A soldier standing still (0.001) for 2 s, 3 s after +168, sets +168
// to -666 (BotNavigation +0x1D0's action press, see updatePath).
bool BotNavigation::checkProgress()
{
    io::PlayerInput* const input = m_bot->getInput();
    const double now = static_cast<const IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
    if (1.0 < now - m_double168 && ((input->mappingFlags >> io::PIAction) & 1ULL))
        input->inputs[io::PIAction] = 0.0f;
    if (m_points.empty() || m_int56 != 2)
        return true;
    float move = mappedInput(input, io::PIThrottle);
    const float sprint = mappedInput(input, io::PISprint);
    if (move <= sprint)
        move = sprint;
    const float yaw = mappedInput(input, io::PIYaw);
    if (move < yaw)
        move = yaw;
    const double still = m_updateSpaceLeftAndRight ? 0.1 : 0.5;
    if (std::fabs(move) <= 0.1f)
    {
        if (still < now - m_double152)
        {
            m_double144 = now;
            m_flag192 = false;
            return true;
        }
    }
    else
    {
        m_double152 = now;
    }
    const float radius = m_updateSpaceLeftAndRight ? 2.0f : 4.0f;
    Information* const info = informationManager->getInformation(m_bot->getCurrentEquipment());
    const Vec3& position = info->getPosition();
    if (distanceSq(m_zero280, position) <= radius * radius)
    {
        const float limit = m_updateSpaceLeftAndRight ? 10.0f : 20.0f;
        const double since = m_double144;
        if (m_updateSpaceLeftAndRight && 2.0f < static_cast<float>(now - since) && 3.0 < now - m_double168)
        {
            if (distanceSq(m_zero176, info->getPosition()) < 0.001f)
                m_double168 = -666.0;
            const Vec3& p = info->getPosition();
            m_zero176[0] = p.x;
            m_zero176[1] = p.y;
            m_zero176[2] = p.z;
        }
        return now - since < static_cast<double>(limit);
    }
    m_zero280[0] = position.x;
    m_zero280[1] = position.y;
    m_zero280[2] = position.z;
    m_double144 = now;
    m_flag192 = false;
    return true;
}

// ELF 0x1928B0 (840 bytes). From the last valid position, a trace 2 m along the transformation's
// right axis (row 0) and one 2 m the other way (IAIPathfinding +0x90, trace(BotNavigation*, ...),
// map index 0, the equipment type, -1, 0, 5): +296 / +292 get the free distance, 2 when the trace
// reports true.
void BotNavigation::updateSpaceLeftAndRight()
{
    Information* const info = informationManager->getInformation(m_bot->getCurrentEquipment());
    if (!info || !info->getMobile() || m_int56 != 2)
        return;
    const IPIMobile::ValidPos& valid = info->getMobile()->getLastValidPosition();
    if (valid.polygon == 0xFFFFFFFFu)
        return;
    Vec3 hit;
    hit.x = 0.0f;
    hit.y = 0.0f;
    hit.z = 0.0f;
    const Mat4& transformation = info->getTransformation();
    unsigned int polygon;
    Vec3 probe;
    probe.x = transformation.m[0] * 2.0f + valid.position.x;
    probe.y = transformation.m[1] * 2.0f + valid.position.y;
    probe.z = transformation.m[2] * 2.0f + valid.position.z;
    float space = 2.0f;
    if (!m_pathfinding->trace(this, false, probe, hit, polygon, m_equipmentType, -1, 0.0f, 5, 0))
        space = length3(hit.x - valid.position.x, hit.y - valid.position.y, hit.z - valid.position.z);
    m_spaceRight = space;
    probe.x = probe.x - transformation.m[0] * 2.0f * 2.0f;
    probe.y = probe.y - transformation.m[1] * 2.0f * 2.0f;
    probe.z = probe.z - transformation.m[2] * 2.0f * 2.0f;
    space = 2.0f;
    if (!m_pathfinding->trace(this, false, probe, hit, polygon, m_equipmentType, -1, 0.0f, 5, 0))
        space = length3(hit.x - valid.position.x, hit.y - valid.position.y, hit.z - valid.position.z);
    m_spaceLeft = space;
}

// Always returns true, leaving BotManager's final pathfinding sweep gated by isSearchActive.
bool BotNavigation::isRunning()
{
    return true;
}

// ELF 0x194F10. The bot's equipment's last valid position for `mapIndex` (IPIMobile, unchecked
// lookups as in the image). A new map or a moved position (any coordinate more than
// 1.1920929e-07f away) is kept (+0xD8, +0xE4) and returned with its polygon / island; it also
// refreshes the vehicle pair (+0xC8, when it is the navigation's own map +0x50) and the infantry
// pair (+0xD0, for IAIPathfinding::getTraceMapIndex). Otherwise the kept position is returned
// with the stored pair of that map, or Debug line 0x630 "Should not be here!" (pair untouched).
void BotNavigation::getTraceData(int mapIndex, unsigned int& polygon, unsigned int& island, Vec3& position)
{
    Information* const info = informationManager->getInformation(m_bot->getCurrentEquipment());
    const IPIMobile::ValidPos& valid = info->getMobile()->getLastValidPosition(mapIndex);
    if (m_traceMapIndex != mapIndex
        || std::fabs(valid.position.x - m_tracePosition.x) > 1.1920929e-07f
        || std::fabs(valid.position.y - m_tracePosition.y) > 1.1920929e-07f
        || std::fabs(valid.position.z - m_tracePosition.z) > 1.1920929e-07f)
    {
        m_traceMapIndex = mapIndex;
        m_tracePosition = valid.position;
        position = valid.position;
        polygon = valid.polygon;
        island = valid.quadTree;
        if (m_mapIndex == mapIndex)
        {
            m_vehiclePolygon = static_cast<int>(valid.polygon);
            m_vehicleIsland = static_cast<int>(valid.quadTree);
        }
        if (mapIndex == IAIPathfinding::instance->getTraceMapIndex())
        {
            m_infantryPolygon = static_cast<int>(valid.polygon);
            m_infantryIsland = static_cast<int>(valid.quadTree);
        }
        return;
    }
    position = m_tracePosition;
    if (mapIndex == IAIPathfinding::instance->getTraceMapIndex())
    {
        polygon = static_cast<unsigned int>(m_infantryPolygon);
        island = static_cast<unsigned int>(m_infantryIsland);
    }
    else if (mapIndex == m_mapIndex)
    {
        polygon = static_cast<unsigned int>(m_vehiclePolygon);
        island = static_cast<unsigned int>(m_vehicleIsland);
    }
    else
    {
        Debug(DebugTypeError, "AI/AIDLL/AI/AIBot/BotNavigation.cpp", 0x630, "") << "Should not be here!";
    }
}

// ELF 0x192EA0: append the old route's remaining interior points after recalculating the prefix.
void BotNavigation::mergeNavigationPoints()
{
    const std::vector<NavigationPoint> previous(m_points);
    unsigned int index = m_currentPoint;
    m_polygonPath->m_i0 = 0;
    calculatePoints();
    // Shipped defect: an empty previous route underflows this unsigned bound.
    for (; index < previous.size() - 1; ++index)
        m_points.push_back(previous[index]);
    NavigationPoint last;
    last.m_point0 = m_goal;
    last.m_pointC = m_goal;
    last.m_position = m_goal;
    last.m_polygon = m_goalPolygon;
    m_points.push_back(last);
}

// ELF 0x195390: appends to the caller's vector without clearing it.
void BotNavigation::getNavigationPoints(std::vector<Vec3>& points) const
{
    for (std::vector<NavigationPoint>::const_iterator it = m_points.begin(); it != m_points.end(); ++it)
        points.push_back(it->m_position);
}

} // namespace ai
} // namespace hfe
} // namespace dice
