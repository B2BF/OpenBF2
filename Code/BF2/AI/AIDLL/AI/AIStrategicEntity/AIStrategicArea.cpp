#include "AIStrategicArea.h"

// <iostream>'s ios_base::Init object is this TU's static initialisation and teardown (ELF
// 0x33F260, __tcf_0 0x33F2C0, _GLOBAL__I 0x33F2F0).
#include <iostream>
#include <algorithm>
#include <cmath>                                          // the two centre-to-corner distances
#include <sstream>                                        // listLandingZones

#include "../AIUtils/checkPointer.h"
#include "../Main/AIDllFrontier.h"
#include "../Main/AISettingsInterfaces.h"                  // IAISettings::getNumberOfVehicleGroups
#include "../Main/CommonSingletonServer.h"
#include "../Main/IAIPathfinding.h"
#include "../AIBot/SquadManager.h"
#include "../AIBot/SquadCoordinator.h"
#include "../AIBotBehaviour/CustomCurve.h"
#include "../AIStrategic/SAI.h"
#include "../AIStrategic/Strategy.h"
#include "AIStrategicObjectManager.h"
#include "../AIBot/BotMain.h"
#include "../Main/IBotManager.h"
#include "../Main/AIMain.h"                             // IAIMain::m_instance->getSAI
#include "../AIKnowledge/BFInformationEnvironment.h"
#include "../AIKnowledge/InformationManager.h"
#include "../AIKnowledge/Information.h"
#include "../AIKnowledge/IPIPhysical.h"
#include "../AIKnowledge/IPIUnit.h"
#include "../AIKnowledge/IPIMobile.h"
#include "../AIBot/SquadMember.h"                    // isSameWaypointList
#include "../AIBot/WPMoveTo.h"
#include "../AIBot/WPAltitudeMoveTo.h"
#include "../Main/AIRandom.h"
#include "SAWayPoint.h"
#include "AILandingZone.h"
#include "AILandingZoneManager.h"
#include "../AIKnowledge/AIInformationGrid.h"
#include "../../../AIInterface/BFKnowledge/BFGame.h"       // IAIGame - the control-point queries
#include "../../../AIInterface/BFKnowledge/BFTerrain.h"    // IAITerrain::getTerrainAltitude
#include "../../../../Math/Vec3.h"                         // vec3ToVec2XZ / vec2ToVec3XZ

namespace dice {
namespace hfe {
namespace ai {

namespace {

// libaidll.so: std::list<(anonymous namespace)::ObjectSearchInfo> in AIStrategicArea.cpp, node
// 0x14 = twelve payload bytes, written as {object, predecessor, cost} by getDistances.
// [INFERRED] field names.
struct ObjectSearchInfo
{
    ObjectSearchInfo(AIStrategicObject* o, AIStrategicObject* p, float c)
        : object(o), previous(p), cost(c) {}
    AIStrategicObject* object;
    AIStrategicObject* previous;
    float cost;
};

// ELF vtable 0x4BF5F0: this TU's own predicate (mangled under AIStrategicArea.cpp's anonymous
// namespace; BotSpawner.cpp has a separate class of the same name). One slot, no destructor.
// Layout: vptr, side (+4).
class FreeUnitPred : public IAIInformationGridPredicate
{
public:
    explicit FreeUnitPred(int side) : m_side(side) {}

    // ELF 0x340B90. A known mobile unit (not types 0x80 / 0x200000) passes when it or one of its
    // secondary objects is unoccupied. Only the root record's type and mobility are checked.
    bool includeInformation(Information* information) const
    {
        if (!information->getUnit() || !information->hasMobile())
            return false;
        const unsigned int flags = information->getObjectTypeFlags();
        if ((flags & 0x80u) || (flags & 0x200000u))
            return false;

        int iterator = information->getFirstSecondaryObjectIterator();
        Information* current = information;
        while (current)
        {
            if (!current->getUnit()->isOccupied())
                return true;
            // The ELF captures the manager before the secondary-object call.
            InformationManager* const manager = informationManager;
            const unsigned int handle = information->getSecondaryObject(iterator, m_side);
            current = manager->getInformation(handle);
        }
        return false;
    }

    int m_side;
};

} // namespace

// ELF 0x335480 (C2; C1 0x335850). An area with explicit bounds: control point id -1, no
// control-point phase, the corner stored before the shared tail runs.
AIStrategicArea::AIStrategicArea(const std::string& name, Vec2 topLeft, Vec2 centre, float height,
                                 float radius)
    : AIStrategicObject(radius, height)
    , m_list268()
    , m_centre(topLeft)
    , m_mapData(checkPointer(new MapData[IAIPathfinding::instance->getNumberOfMaps()]))
    , m_neighbours()
    , m_neighbouringObjects()
    , m_name(name)
    , m_objectTypeFlags(0)
    , m_controlPointId(static_cast<unsigned int>(-1))
    , m_landingZones()
    , m_i340(0)
    , m_showNeighbours(false)
{
    m_i400 = -1;
    m_visible = true;
    m_statusChanged = false;
    m_v416.x = 0.0f;
    m_v416.y = 0.0f;
    m_v416.z = 0.0f;
    m_fromControlPoint      = false;
    m_orderPosesInitialised = false;
    internalConstructorCall(topLeft, centre);
}

// ELF 0x335B70 (C2; C1 0x335E70).
// Creates a control-point area with an unset-height sentinel and one record per pathfinding map.

AIStrategicArea::AIStrategicArea(const std::string& name, unsigned int controlPointId, float radius)
    : AIStrategicObject(radius, -666.0f)
    , m_list268()
    , m_centre()
    , m_mapData(checkPointer(new MapData[IAIPathfinding::instance->getNumberOfMaps()]))
    , m_neighbours()
    , m_neighbouringObjects()
    , m_name(name)
    , m_objectTypeFlags(0)
    , m_controlPointId(controlPointId)
    , m_landingZones()
    , m_i340(0)
    , m_showNeighbours(false)
{
    // The tail. Everything above this point lands before the three list members are constructed, so
    // it cannot be body code; everything here lands after them, so it cannot be an initialiser.
    m_i400 = -1;
    m_visible = true;
    m_statusChanged = false;
    m_v416.x = 0.0f;
    m_v416.y = 0.0f;
    m_v416.z = 0.0f;
    m_fromControlPoint      = true;    // the phase selector - see the header
    m_orderPosesInitialised = false;
}

// ELF 0x336F90 (D2; D1 0x337410, D0 0x337890; from AIStrategicObjectManager::reset/destructor):
// delete[] of the per-map data (+0x118), the two per-group flag arrays (+0x15C/+0x160) and the
// six per-group container arrays (+0x168..+0x17C, each element destroyed). The member lists,
// vectors and the name (+0x188 down to +0x10C) then go in reverse declaration order, then the base.
AIStrategicArea::~AIStrategicArea()
{
    delete[] m_mapData;
    delete[] m_flagsA;
    delete[] m_flagsB;
    delete[] m_listsA;
    delete[] m_listsB;
    delete[] m_mapsA;
    delete[] m_mapsB;
    delete[] m_mapsC;
    delete[] m_mapsD;
}

// Returns type code zero. [UNRESOLVED] Its meaning is not established as a StrategicObjectType.
// ELF 0x340B70.
int AIStrategicArea::getType() const
{
    return 0;
}

// - one load. The flag word an `.ai` script fills through
// `aiStrategicArea.addObjectTypeFlag`.
// ELF 0x340B80.
int AIStrategicArea::getObjectTypeFlags() const
{
    return m_objectTypeFlags;
}

// Returns twice the center-to-side-corner distance along X or Z.
// Side indices are one-based and must be valid.
// ELF 0x33B540.
float AIStrategicArea::getXExtent(int side) const
{
    const float half = m_position[side - 1].x - m_centre.x;
    return half + half;
}

// ELF 0x33B560.
float AIStrategicArea::getZExtent(int side) const
{
    const float half = m_position[side - 1].y - m_centre.y;
    return half + half;
}

// The query's own result and
// its fourth (active) output are discarded; the local is seeded true only to be passed along.
// Any assigned id answers true, whatever the engine wrote; the singleton is not null-checked.
// ELF 0x33A040.
bool AIStrategicArea::getControlPointPosAndRadius(Vec3& position, float& radius)
{
    bool active = true;
    if (m_controlPointId == static_cast<unsigned int>(-1))
        return false;
    static_cast<IAIGame*>(CommonSingletonServer::m_iaiGame)
        ->getControlPointInfo(m_controlPointId, position, radius, active);
    return true;
}

// Resolves control-point position, height, and ownership after world objects are available.
// ELF 0x337E20.
void AIStrategicArea::postLoadInit()
{
    if (m_fromControlPoint)
    {
        IAIGame* const game = static_cast<IAIGame*>(CommonSingletonServer::m_iaiGame);

        Vec3 controlPointPosition;
        controlPointPosition.x = 0.0f;
        controlPointPosition.y = 0.0f;
        controlPointPosition.z = 0.0f;

        float radius = 0.0f;
        bool  active = false;

        game->getControlPointInfo(m_controlPointId, controlPointPosition, radius, active);

        Vec2 flat;
        flat.x = 0.0f;
        flat.y = 0.0f;
        vec3ToVec2XZ(controlPointPosition, flat);

        m_centre = flat;
        m_centre.x -= radius;
        m_centre.y -= radius;

        m_f148 = controlPointPosition.y;

        internalConstructorCall(m_centre, flat);
        init();
    }

    initOrderPoses();

    if (hasObjectTypeFlag(ControlPoint))
    {
        if (m_controlPointId == 0xFFFFFFFFu)
        {
            IAIGame* const game = static_cast<IAIGame*>(CommonSingletonServer::m_iaiGame);

            const int found = game->findControlPoint(m_centre, m_position[0]);
            m_controlPointId = static_cast<unsigned int>(found);

            if (found != -1)
            {
                const int owner = game->getOwnerOfControlPoint(m_controlPointId);

                if (owner == 0)
                {
                    m_status[0] = StatusMap::Neutral;
                    m_status[1] = StatusMap::Neutral;
                }
                else if (owner == 1)
                {
                    m_status[0] = StatusMap::Owned;
                    m_status[1] = StatusMap::Hostile;
                }
                else
                {
                    m_status[0] = StatusMap::Hostile;
                    m_status[1] = StatusMap::Owned;
                }
            }
        }
    }
}

// ELF 0x336170. Initializes per-vehicle-group area arrays, with at least one element in each
// allocation (each new[] goes through checkPointer); the destructor releases them.
void AIStrategicArea::internalConstructorCall(const Vec2& topLeft, const Vec2& centre)
{
    // unused, and deliberately so - see the note above.
    (void)topLeft;

    m_landingZones.resize(0);

    m_position[0] = centre;
    m_position[1] = centre;

    m_i320 = 25;
    m_i324 = 25;
    m_sideEnabled[0] = true;
    m_sideEnabled[1] = true;

    const IAISettings* const settings =
        static_cast<const IAISettings*>(CommonSingletonServer::m_iaiSettings);

    const int vehicleGroups = settings->getNumberOfVehicleGroups();
    const int groups = vehicleGroups ? vehicleGroups : 1;

    m_flagsA = checkPointer(new bool[groups]);
    m_flagsB = checkPointer(new bool[groups]);

    m_listsA = checkPointer(new std::list<std::pair<float, AIStrategicObject*> >[groups]);
    m_listsB = checkPointer(new std::list<std::pair<float, AIStrategicObject*> >[groups]);

    m_mapsA = checkPointer(new std::map<AIStrategicObject*, std::list<AIStrategicObject*> >[groups]);
    m_mapsB = checkPointer(new std::map<AIStrategicObject*, std::list<AIStrategicObject*> >[groups]);
    m_mapsC = checkPointer(new std::map<AIStrategicObject*, float>[groups]);
    m_mapsD = checkPointer(new std::map<AIStrategicObject*, float>[groups]);

    m_distancesDirty[0] = true;
    m_distancesDirty[1] = true;

    for (int i = 0; i < groups; ++i)
    {
        m_flagsA[i] = true;
        m_flagsB[i] = true;
    }

    // Stores equal center-to-corner distances for the initial positions.
    // Each side retains its own value because setPosition can later move them independently.
    const float dx0 = m_centre.x - m_position[0].x;
    const float dy0 = m_centre.y - m_position[0].y;
    m_size[1] = std::sqrt(dx0 * dx0 + dy0 * dy0);

    const float dx1 = m_centre.x - m_position[1].x;
    const float dy1 = m_centre.y - m_position[1].y;
    m_size[2] = std::sqrt(dx1 * dx1 + dy1 * dy1);

    m_enemyTakeover[0] = false;
    m_enemyTakeover[1] = false;
    m_f408 = 1.0f;
    m_f412 = 1.0f;

    m_f396 = m_size[1] + m_size[1];
}

// Raises the stored area height to the terrain height when the terrain is higher.
// ELF 0x337D20.
void AIStrategicArea::init()
{
    const IAITerrain* const terrain =
        static_cast<const IAITerrain*>(CommonSingletonServer::m_iaiTerrain);

    const float altitude = terrain->getTerrainAltitude(m_position[0].x, m_position[0].y);

    if (altitude > m_f148)
        m_f148 = altitude;

    IAIPathfinding* const pathfinding = IAIPathfinding::instance;

    for (unsigned int i = 0; i < static_cast<unsigned int>(pathfinding->getNumberOfMaps()); ++i)
    {
        if (pathfinding->isMapUsed(static_cast<int>(i)))
        {
            const Vec3 orderPosition = getOrderPos(static_cast<int>(i), 1);

            unsigned int polygon = 0;
            unsigned int island  = 0;
            pathfinding->getPolygon(orderPosition, static_cast<int>(i), polygon, island, 1);
        }
    }
}

// Resolves order positions to the navmesh and converts them to area-relative offsets once.
// ELF 0x336E80.
void AIStrategicArea::initOrderPoses()
{
    Vec3 areaPosition;
    areaPosition.x = 0.0f;
    areaPosition.y = 0.0f;
    areaPosition.z = 0.0f;

    vec2ToVec3XZ(m_position[0], areaPosition);
    areaPosition.y = m_f148;

    IAIPathfinding* const pathfinding = IAIPathfinding::instance;

    for (int i = 0; i < pathfinding->getNumberOfMaps(); ++i)
    {
        MapData& data = m_mapData[i];

        unsigned int polygon = 0;
        unsigned int island  = 0;
        pathfinding->getPolygon(data.m_offset, i, polygon, island, 1);

        data.m_offset.x = data.m_offset.x - areaPosition.x;
        data.m_offset.y = data.m_offset.y - areaPosition.y;
        data.m_offset.z = data.m_offset.z - areaPosition.z;

        data.m_island = island;
    }

    m_orderPosesInitialised = true;
}

// Returns the area position plus the selected map's delta; map -1 omits the delta.
// The second parameter is unused; [UNRESOLVED] its original purpose is unknown.
// ELF 0x339830.
Vec3 AIStrategicArea::getOrderPos(int map, int unused) const
{
    (void)unused;

    Vec3 position;
    position.x = m_position[0].x;
    position.y = m_f148;
    position.z = m_position[0].y;

    if (map != -1)
    {
        const MapData& data = m_mapData[map];
        position.x += data.m_offset.x;
        position.y += data.m_offset.y;
        position.z += data.m_offset.z;
    }

    return position;
}

// Tests a StrategicObjectType bit position in the object flags.
// ELF 0x33A0B0.
bool AIStrategicArea::hasObjectTypeFlag(StrategicObjectType type) const
{
    return ((m_objectTypeFlags >> type) & 1) != 0;
}


// ELF 0x33BD30.
bool AIStrategicArea::isLandingZoneUser(int equipment)
{
    return (m_i340 >> equipment) & 1;
}

// ELF 0x33A0F0. Both temperature pairs of the side (+0xA0/+0xA8 + 4 * side, i.e. m_f164[side - 1]
// and m_f164[side + 1]) are scaled by the strategy's object modifiers for the area's status: one
// factor per object type flag the area has (types 0..20), then the status factor (Owned: Front
// when a hostile neighbour exists, else Safe; Hostile: Enemy; Neutral: Neutral), then the
// strategy's specific modifier for this object. Answers the scaled first temperature.
float AIStrategicArea::updateTemperature(const Strategy* strategy, int side)
{
    const float* const modifiers = strategy->getStrategicObjectsModifiers(getStatus(side));
    for (int type = 0; type <= 20; ++type)
    {
        if (hasObjectTypeFlag(static_cast<StrategicObjectType>(type)))
        {
            m_f164[side - 1] *= modifiers[type];
            m_f164[side + 1] *= modifiers[type];
        }
    }
    switch (getStatus(side))
    {
    case StatusMap::Owned:
        if (hasNeighbourWithStatus(StatusMap::Hostile, side))
        {
            m_f164[side - 1] *= modifiers[Front];
            m_f164[side + 1] *= modifiers[Front];
        }
        else
        {
            m_f164[side - 1] *= modifiers[Safe];
            m_f164[side + 1] *= modifiers[Safe];
        }
        break;
    case StatusMap::Hostile:
        m_f164[side - 1] *= modifiers[Enemy];
        m_f164[side + 1] *= modifiers[Enemy];
        break;
    case StatusMap::Neutral:
        m_f164[side - 1] *= modifiers[NeutralType];
        m_f164[side + 1] *= modifiers[NeutralType];
        break;
    default:
        break;
    }
    m_f164[side - 1] *= strategy->getSpecificStrategicObjectModifier(m_index);
    m_f164[side + 1] *= strategy->getSpecificStrategicObjectModifier(m_index);
    AIStrategicObjectManager::getInstance();   // result unused in the image
    return m_f164[side - 1];
}

// Parameter types
// are mangled; neither body materializes a return. Names of local aggregates are inferred.
// ELF 0x337FF0.
void AIStrategicArea::update(int side, ISAI* sai, bool& changed)
{
    IAIGame* const game = static_cast<IAIGame*>(CommonSingletonServer::m_iaiGame);
    m_assignedSquadStrength[side - 1] = 0;
    std::list<AIStrategicObject*> objects;
    m_assignedSquadStrength[side - 1] = SquadManager::m_instance->getSquadStrengths(
        side, m_index, static_cast<StrategicStrengthType>(m_attack[side - 1] ? 0 : 1));

    Vec2 upper;
    const double dx = static_cast<double>(m_position[side - 1].x) - m_centre.x;
#if defined(_MSC_VER)
    // [PLATFORM] PE spills only the Y difference before adding position again.
    const float dy = m_position[side - 1].y - m_centre.y;
#else
    const double dy = static_cast<double>(m_position[side - 1].y) - m_centre.y;
#endif
    upper.x = static_cast<float>(dx + m_position[side - 1].x);
    upper.y = static_cast<float>(static_cast<double>(dy) + m_position[side - 1].y);

    float total = 0.0f, withoutUnits = 0.0f;
    float friendly = 0.0f, enemy = 0.0f, ratio = 0.0f;
    int friendlyUnits = 0, enemyUnits = 0;
    SampleHelper battleSamples(6), strategicSamples(2);
    float battle[6], strategic[2];
    int battleCounts[6], strategicCounts[2];
    int secondary = -1;
    {
        std::vector<unsigned int> handles;
        battleSamples.clear(battle, battleCounts);
        strategicSamples.clear(strategic, strategicCounts);
        m_resourceNeed[side - 1] = 0;
        IAIInformationEnvironment* const environment =
            static_cast<IAIInformationEnvironment*>(AISingletonServer::m_iaiInformationEnvironment);
        environment->getObjectsWithinRectangle(handles, m_centre, upper, 0, true, nullptr);
        environment->getObjectsWithinRectangle(handles, m_centre, upper, 0, false, nullptr);
        environment->getObjectsWithinRectangle(handles, m_centre, upper, side, true, nullptr);
        environment->getObjectsWithinRectangle(handles, m_centre, upper, side, false, nullptr);

        // The two original record arms do the same sampling, except the root-only flag
        // suppression. Keep selected unit/physical/array receivers across their callbacks.
        const auto sampleInformation = [&](Information* information, bool root)
        {
            const double returnedSecurity = information->getSecurity();
            const float security = static_cast<float>(returnedSecurity);
#if defined(_MSC_VER)
            const double weighted = static_cast<double>(security) * information->getStrength();
#else
            const double weighted = returnedSecurity * information->getStrength();
#endif
            IPIUnit* const unit = information->getUnit();
            total = static_cast<float>(static_cast<double>(total) + weighted);
            if (!unit)
            {
                withoutUnits = static_cast<float>(weighted + withoutUnits);
                return;
            }

#if defined(_MSC_VER)
            // [PLATFORM] PE sums all six floats wide, then stores once. ELF stores each sum.
            const float* const values = unit->getBattleStrengths();
            const double sum = ((((static_cast<double>(values[0]) + values[1])
                + values[2]) + values[3]) + values[4]) + values[5];
            const float strength = static_cast<float>(sum);
#else
            float strength = 0.0f;
            for (int i = 0; i < 6; ++i)
                strength = unit->getBattleStrengths()[i] + strength;
#endif
            if (!unit->isOccupied())
                return;
            if (sai->isFriend(information->getSide()))
            {
                if (!root || (information->getObjectTypeFlags() & 0x10u) == 0)
                    ++friendlyUnits;
                friendly = static_cast<float>(static_cast<double>(security) * strength + friendly);
            }
            else if (sai->isEnemy(information->getSide()))
            {
                if (!root || (information->getObjectTypeFlags() & 0x10u) == 0)
                    ++enemyUnits;
                enemy = static_cast<float>(static_cast<double>(security) * strength + enemy);
                IPIPhysical* const physical = information->getPhysical();
#if defined(_MSC_VER)
                float* const input = unit->getBattleStrengths();
#endif
                const int type = physical->getStrengthType();
#if !defined(_MSC_VER)
                float* const input = unit->getBattleStrengths();
#endif
                battleSamples.addSample(input, type, battle, battleCounts);
                strategicSamples.addSample(unit->getStrategicStrengths(),
                    m_status[side - 1] != 0, strategic, strategicCounts);
                m_resourceNeed[side - 1] += static_cast<int>(
                    unit->getStrategicStrengths()[m_status[side - 1] == 0]);
            }
        };

#if defined(_MSC_VER)
        InformationManager* manager = handles.empty() ? nullptr : informationManager;
#endif
        auto end = handles.end();
        for (auto it = handles.begin(); it != end; ++it)
        {
#if !defined(_MSC_VER)
            InformationManager* manager = informationManager;
            Information* const root = manager->getInformation(*it);
#else
            // PE tests the handle's index before touching the captured manager's pool.
            Information* const root = (*it & 0xffffu) ? manager->getInformation(*it) : nullptr;
#endif
            if (root)
            {
                sampleInformation(root, true);
#if !defined(_MSC_VER)
                manager = informationManager;
#endif
                unsigned int handle = root->getFirstSecondaryObject(secondary, side);
                for (;;)
                {
#if defined(_MSC_VER)
                    // [PLATFORM] PE captures after secondary callbacks and carries this
                    // manager to the next root; ELF captures before each callback.
                    manager = informationManager;
                    Information* const current = (handle & 0xffffu)
                        ? manager->getInformation(handle) : nullptr;
#else
                    Information* const current = manager->getInformation(handle);
#endif
                    if (!current)
                        break;
                    sampleInformation(current, false);
#if !defined(_MSC_VER)
                    manager = informationManager;
#endif
                    handle = root->getSecondaryObject(secondary, side);
                }
                end = handles.end();
            }
#if defined(_MSC_VER)
            end = handles.end();
#endif
        }
        const double denominator = static_cast<double>(friendly) + enemy;
        if (denominator > 0.0)
            ratio = static_cast<float>(friendly / denominator);
    } // The query vector dies before the old-status capture and output stores.

    const StatusMap::Status oldStatus = m_status[side - 1];
    m_f164[side - 1] = total + m_size[0];
    m_f164[side + 1] = withoutUnits + m_size[0];
    m_friendlyRatio[side - 1] = ratio;
    m_enemyBattleStrength[side - 1] = enemy;
    m_friendlyBattleStrength[side - 1] = friendly;
    if (m_controlPointId != 0xffffffffu)
    {
        const int owner = game->getOwnerOfControlPoint(m_controlPointId);
        m_status[side - 1] = owner == side ? StatusMap::Owned
            : owner == 0 ? StatusMap::Neutral : StatusMap::Hostile;
    }
    else if (friendlyUnits > 0 || enemyUnits > 0)
    {
        if (m_sideEnabled[side - 1] && friendlyUnits > 0 && enemyUnits == 0)
            m_status[side - 1] = StatusMap::Owned;
        else if (m_sideEnabled[side == 1] && enemyUnits > 0 && friendlyUnits == 0)
            m_status[side - 1] = StatusMap::Hostile;
    }

    // Reread the ID/status after the owner callback. Owned->Neutral skips both the
    // takeover reset and getter; all other CP transitions query even when not owned.
    if (m_controlPointId != 0xffffffffu)
    {
        if (oldStatus == StatusMap::Owned && m_status[side - 1] == StatusMap::Neutral)
            changed = true;
        else
        {
            m_enemyTakeover[side - 1] = false;
            const double counter = getControlPointTakeOverCounter();
            if (m_status[side - 1] == StatusMap::Owned && counter < 0.9f)
            {
                m_enemyTakeover[side - 1] = true;
                m_status[side - 1] = StatusMap::Neutral;
            }
            changed = oldStatus == StatusMap::Owned
                ? m_status[side - 1] != StatusMap::Owned : m_status[side - 1] == StatusMap::Owned;
        }
    }
    else
    {
        changed = false;
        if ((oldStatus == StatusMap::Owned && m_status[side - 1] == StatusMap::Hostile)
            || (oldStatus == StatusMap::Hostile && m_status[side - 1] == StatusMap::Owned))
            changed = true;
    }
    m_statusChanged = changed;
    m_statistics[side - 1]->sample(battle, battleCounts);
    m_statistics[side + 1]->sample(strategic, strategicCounts);

    const float count = static_cast<float>(m_resourceNeed[side - 1]);
    const float floored = count > 1.0f ? count : 1.0f;
    const bool owned = m_status[side - 1] == StatusMap::Owned;
    const float input = owned ? m_needCurveInput[side - 1] : m_friendlyRatio[side - 1];
#if defined(_MSC_VER)
    double need = (2.0 - static_cast<double>(SCurve::calculate(input))) * floored;
    if (!owned)
        need *= 1.25;
    m_resourceNeed[side - 1] = static_cast<int>(need);
#else
    // [PLATFORM] ELF scales the base first and narrows the final product before cvttss2si.
    const float base = owned ? floored : floored * 1.25f;
    const float need = static_cast<float>((2.0 - static_cast<double>(SCurve::calculate(input))) * base);
    m_resourceNeed[side - 1] = static_cast<int>(need);
#endif
}

// ELF 0x337FC0: 1 when the last update changed the status and the side does not own the area.
int AIStrategicArea::getChangedAttackDefence(int side)
{
    int changed = 0;
    if (m_statusChanged && getStatus(side) != StatusMap::Owned)
        changed = 1;
    return changed;
}

// Full pair : fresh game/id after the flag callback.
// ELF 0x3399D0.
float AIStrategicArea::getControlPointTakeOverCounter()
{
    if (hasObjectTypeFlag(ControlPoint))
        return static_cast<IAIGame*>(CommonSingletonServer::m_iaiGame)->
            getControlPointTakeOverCounter(m_controlPointId);
    return 1.0f;
}

// ELF 0x338BF0.
bool AIStrategicArea::isEnemyTakingOverBase(int side)
{
    return m_enemyTakeover[side - 1];
}

// ELF 0x338C10 (slot 11 answers a float in st0). Only owned areas need defence:
// (2 - SCurve(need input)) * (enemy battle strength + temperature), times 1.5 with a hostile
// neighbour, else 1.25 with a neutral one.
float AIStrategicArea::calculateDefenseNeed(int side)
{
    if (getStatus(side) != StatusMap::Owned)
        return 0.0f;
    float need = (2.0f - SCurve::calculate(m_needCurveInput[side - 1]))
        * (m_enemyBattleStrength[side - 1] + m_f164[side - 1]);
    if (hasNeighbourWithStatus(StatusMap::Hostile, side))
        need = need * 1.5f;
    else if (hasNeighbourWithStatus(StatusMap::Neutral, side))
        need = need * 1.25f;
    return need;
}

// ELF 0x338CE0 (float). A not-owned area next to an owned one is worth
// (1 + need input) * temperature, times 1.5 next to a neutral area or 1.25 otherwise (no
// factor next to a hostile one), scaled by 100 / distance of the closest squad to the area's
// first position when that lies in (1, 100000), else 0; minus the attack cost.
float AIStrategicArea::calculateAttackValue(int side)
{
    float value = 0.0f;
    if (getStatus(side) != StatusMap::Owned && hasNeighbourWithStatus(StatusMap::Owned, side))
    {
        value = 1.0f;
        value = value + m_needCurveInput[side - 1];
        value = value * m_f164[side - 1];
        if (!hasNeighbourWithStatus(StatusMap::Hostile, side))
            value = (hasNeighbourWithStatus(StatusMap::Neutral, side) ? 1.5f : 1.25f) * value;
        const float x = m_position[0].x;
        Vec3 position;
        position.y = static_cast<IAITerrain*>(CommonSingletonServer::m_iaiTerrain)->getTerrainAltitude(x, m_position[0].y);
        position.z = m_position[0].y;
        position.x = x;
        const float distance = SquadManager::m_instance->getDistanceToClosestSquad(position, side);
        if (distance > 1.0f && 100000.0f > distance)
            value = 1.0f / distance * 100.0f * value;
        else
            value = 0.0f;
    }
    value = value - calculateAttackCost(side);
    return value;
}

// ELF 0x338E60 (float): the enemy's second battle strength (-1 with fewer than two buckets)
// times (1.5 - 0.5 * need input) * (1 + friendly ratio).
float AIStrategicArea::calculateAttackCost(int side)
{
    const EnemyStatistics* const statistics = m_statistics[side - 1];
    const float strength = statistics->m_size > 1 ? statistics->m_strength[1] : -1.0f;
    return strength * (1.5f - 0.5f * m_needCurveInput[side - 1]) * (1.0f + m_friendlyRatio[side - 1]);
}

// ELF 0x339130.
bool AIStrategicArea::isNeighbour(AIStrategicObject* object) const
{
    return std::find(m_neighbours.begin(), m_neighbours.end(), object) != m_neighbours.end();
}

// Native vector scan, captured end, no null/side guards; first matching status wins.
// ELF 0x339330.
bool AIStrategicArea::hasNeighbourWithStatus(StatusMap::Status status, int side) const
{
    const auto end = m_neighbours.end();
    for (auto it = m_neighbours.begin(); it != end; ++it)
    {
        if ((*it)->getStatus(side) == status)
            return true;
    }
    return false;
}

// ELF 0x339370.
bool AIStrategicArea::isNeighbouringObject(AIStrategicObject* object) const
{
    return std::find(m_neighbouringObjects.begin(), m_neighbouringObjects.end(), object)
        != m_neighbouringObjects.end();
}

// Applies the same scan to the second neighbor vector.
// ELF 0x3394C0.
bool AIStrategicArea::hasNeighbouringObjectWithStatus(StatusMap::Status status, int side) const
{
    const auto end = m_neighbouringObjects.end();
    for (auto it = m_neighbouringObjects.begin(); it != end; ++it)
    {
        if ((*it)->getStatus(side) == status)
            return true;
    }
    return false;
}

// ELF 0x33A0D0: all of the given flags.
bool AIStrategicArea::hasObjectTypeFlags(int flags) const
{
    return (m_objectTypeFlags & flags) == flags;
}

// ELF 0x33A970: the int at +316 + 4 * side, i.e. m_i320 / m_i324 for sides 1 / 2.
int AIStrategicArea::getTacticalStatus(int side) const
{
    return side == 1 ? m_i320 : (side == 2 ? m_i324 : static_cast<int>(m_controlPointId));
}

// ELF 0x339A30: the game reports the area's control point as active. The radius output is
// left uninitialised, as shipped.
bool AIStrategicArea::isAreaTakeable()
{
    bool active = false;
    const unsigned int cp = m_controlPointId;
    if (cp != static_cast<unsigned int>(-1))
    {
        Vec3 position;
        position.x = 0.0f;
        position.y = 0.0f;
        position.z = 0.0f;
        float radius;
        static_cast<IAIGame*>(CommonSingletonServer::m_iaiGame)->getControlPointInfo(cp, position, radius, active);
    }
    return active;
}

// ELF 0x33A380: answers whether the assigned squad strength covers the need (+0xE0 + 4 side vs
// +0xD8 + 4 side). Otherwise the closest available squads are ordered to the area (y = m_f148)
// like collectAdditionalSquadResources does, while the assigned strength stays at or below the
// need (`jle` back to the query), so an exact match still asks for one more squad.
bool AIStrategicArea::collectSquadResources(int side, ISAI* sai)
{
    if (m_assignedSquadStrength[side - 1] >= m_resourceNeed[side - 1])
        return true;
    int strength = 0;
    Vec3 position;
    position.x = 0.0f;
    position.y = 0.0f;
    position.z = 0.0f;
    vec2ToVec3XZ(getPosition(side), position);
    SquadManager* const squads = SquadManager::m_instance;
    position.y = m_f148;
    validateDistances(side);
    if (m_assignedSquadStrength[side - 1] > m_resourceNeed[side - 1])
        return true;
    do
    {
        SquadCoordinator* const squad =
            squads->getClosestAvailableSquad(this, position, side, m_attack[side - 1], strength);
        if (!squad)
            break;
        const StatusMap::Status status = getStatus(side);
        bool defend;
        if (status != StatusMap::Owned)
            defend = m_enemyTakeover[side - 1] && status == StatusMap::Neutral;
        else
            defend = true;
        if (defend)
            sai->getCommanderInterface()->giveDefendOrder(position, squad);
        else
            sai->getCommanderInterface()->giveAttackOrder(position, squad);
        m_assignedSquadStrength[side - 1] += strength;
    } while (m_assignedSquadStrength[side - 1] <= m_resourceNeed[side - 1]);
    return m_assignedSquadStrength[side - 1] >= m_resourceNeed[side - 1];
}

// ELF 0x33A530: up to `amount` of the closest available squads get an attack order to the
// area's position (y = m_f148), or a defend order when the side owns it or it is neutral and
// being taken over; their strength adds to m_assignedSquadStrength. Answers the squad count.
int AIStrategicArea::collectAdditionalSquadResources(int amount, int side, ISAI* sai)
{
    int count = 0;
    int strength = 0;
    Vec3 position;
    position.x = 0.0f;
    position.y = 0.0f;
    position.z = 0.0f;
    vec2ToVec3XZ(getPosition(side), position);
    SquadManager* const squads = SquadManager::m_instance;
    position.y = m_f148;
    validateDistances(side);
    for (; count < amount; ++count)
    {
        SquadCoordinator* const squad =
            squads->getClosestAvailableSquad(this, position, side, m_attack[side - 1], strength);
        if (!squad)
            break;
        const StatusMap::Status status = getStatus(side);
        bool defend;
        if (status != StatusMap::Owned)
            defend = m_enemyTakeover[side - 1] && status == StatusMap::Neutral;
        else
            defend = true;
        if (defend)
            sai->getCommanderInterface()->giveDefendOrder(position, squad);
        else
            sai->getCommanderInterface()->giveAttackOrder(position, squad);
        m_assignedSquadStrength[side - 1] += strength;
    }
    return count;
}

// ELF 0x33BD50.
void AIStrategicArea::validateDistances(int side)
{
    if (!m_distancesDirty[side - 1])
        return;
    m_distances[side - 1].clear();
    const int groups = static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getNumberOfVehicleGroups();
    if (!groups)
    {
        validateDistances(side, 0);
        insertUnitDistances(side, 0);
        return;
    }
    for (int group = 0; group < groups; ++group)
    {
        validateDistances(side, group);
        insertUnitDistances(side, group);
    }
}

// ELF 0x33BE30: a dirty group (m_flagsA/B[group]) is rebuilt: its distance list and both maps
// are cleared, then getDistances refills them for the group's bit.
void AIStrategicArea::validateDistances(int side, int group)
{
    bool* const flags = side == 1 ? m_flagsA : m_flagsB;
    if (!flags[group])
        return;
    flags[group] = false;
    std::list<std::pair<float, AIStrategicObject*> >* const lists = side == 1 ? m_listsA : m_listsB;
    std::map<AIStrategicObject*, std::list<AIStrategicObject*> >* const routes = side == 1 ? m_mapsA : m_mapsB;
    std::map<AIStrategicObject*, float>* const costs = side == 1 ? m_mapsC : m_mapsD;
    lists[group].clear();
    routes[group].clear();
    costs[group].clear();
    getDistances(side, lists[group], routes[group], costs[group], 1 << group);
}

// ELF 0x33AAC0: a breadth-ordered relaxation over the area graph from this area. Entries
// {object, predecessor, cost} are appended to a FIFO (std::list<(anonymous)::ObjectSearchInfo>,
// node 0x14); the front entry is expanded when it improves the object's best cost
// (`fucompi; jbe` = skip unless costs[object] > cost or no cost yet) and the object allows one of
// the groups in groupMask. Edge cost: the 2-D distance between the side's positions (slot 3),
// times 5.0f into a hostile area and 1.1f into a neutral one (rodata 0x49CDAC / 0x49DCA8).
// The object's route is its predecessor's route with the object pushed to the front. Finally
// every (cost, object) of costs is appended to distances, which is sorted.
void AIStrategicArea::getDistances(int side, std::list<std::pair<float, AIStrategicObject*> >& distances,
                                   std::map<AIStrategicObject*, std::list<AIStrategicObject*> >& routes,
                                   std::map<AIStrategicObject*, float>& costs, int groupMask)
{
    std::list<ObjectSearchInfo> open;
    std::list<AIStrategicObject*> route;
    route.clear();                                   // 0x33AB53, on the fresh list
    if (!allowsSomeVehicleGroups(groupMask))
        return;
    open.push_back(ObjectSearchInfo(this, this, 0.0f));
    routes.insert(std::make_pair(static_cast<AIStrategicObject*>(this), route));
    std::list<ObjectSearchInfo>::iterator it = open.begin();
    while (!open.empty())
    {
        std::map<AIStrategicObject*, float>::iterator found = costs.find(it->object);
        if ((found == costs.end() || found->second > it->cost)
            && it->object->allowsSomeVehicleGroups(groupMask))
        {
            // A copy of the area's neighbour vector (+0x11C), then one entry per neighbour.
            std::vector<AIStrategicObject*> neighbours =
                static_cast<AIStrategicArea*>(it->object)->m_neighbours;
            for (std::vector<AIStrategicObject*>::iterator n = neighbours.begin();
                 n != neighbours.end(); ++n)
            {
                const Vec2& from = it->object->getPosition(side);
                const Vec2& to = (*n)->getPosition(side);
                Vec2 delta;
                delta.x = to.x - from.x;
                delta.y = to.y - from.y;
                float distance = std::sqrt(delta.x * delta.x + delta.y * delta.y);
                const StatusMap::Status status = (*n)->getStatus(side);
                if (status == StatusMap::Neutral)
                    distance *= 1.1f;
                else if (status == StatusMap::Hostile)
                    distance *= 5.0f;
                open.push_back(ObjectSearchInfo(*n, it->object, it->cost + distance));
            }
            if (found == costs.end())
                costs.insert(std::make_pair(it->object, it->cost));
            else
                found->second = it->cost;
            std::map<AIStrategicObject*, std::list<AIStrategicObject*> >::iterator previous =
                routes.find(it->previous);
            std::map<AIStrategicObject*, std::list<AIStrategicObject*> >::iterator own =
                routes.find(it->object);
            if (own == routes.end())
            {
                routes.insert(std::make_pair(it->object, previous->second));
                own = routes.find(it->object);
            }
            else
            {
                own->second = previous->second;
            }
            own->second.push_front(it->object);
        }
        it = open.erase(it);
    }
    for (std::map<AIStrategicObject*, float>::iterator c = costs.begin(); c != costs.end(); ++c)
        distances.push_back(std::make_pair(c->second, c->first));
    distances.sort();
}

// ELF 0x33BF90: merges the group's sorted distance list into the side's AreaDistanceInfo list
// (sorted by distance). An entry of equal distance gains the group's bit (`orl %eax,0x8(%edx)`);
// otherwise a new {1 << group, area, distance} is inserted before the first larger entry, and
// whatever is left is appended.
void AIStrategicArea::insertUnitDistances(int side, int group)
{
    AreaDistanceInfo info;
    std::list<AreaDistanceInfo>& merged = m_distances[side - 1];
    std::list<std::pair<float, AIStrategicObject*> >& distances = (side == 1 ? m_listsA : m_listsB)[group];
    std::list<AreaDistanceInfo>::iterator at = merged.begin();
    std::list<std::pair<float, AIStrategicObject*> >::iterator it = distances.begin();
    for (; it != distances.end(); ++it)
    {
        // [CHOSEN] The image compares before the end test, reading the libstdc++ list header's
        // payload once at the end; the end test runs first here (same result, no sentinel read).
        while (at != merged.end() && it->first > at->m_distance)
            ++at;
        if (at == merged.end())
            break;
        if (at->m_distance == it->first)
        {
            at->m_vehicleGroups |= 1 << group;
        }
        else
        {
            info.m_vehicleGroups = 1 << group;
            info.m_area = static_cast<AIStrategicArea*>(it->second);
            info.m_distance = it->first;
            merged.insert(at, info);
        }
    }
    for (; it != distances.end(); ++it)
    {
        info.m_vehicleGroups = 1 << group;
        info.m_area = static_cast<AIStrategicArea*>(it->second);
        info.m_distance = it->first;
        merged.push_back(info);
    }
}

// ELF 0x33A6A0: with more strength assigned than needed, one assigned unit (+0x38 + 12 side,
// m_set68) is released. Units whose bot is gone are erased on the way. The pick prefers units
// not present in the area and, among those, the lowest strategic strength for the area's role
// (getStrategicStrengths()[attack ? 0 : 1], truncated to int). It is released through the side's
// SAI (slot 33) only if (assigned - strength) / need (integer division, `idivl` - a zero need
// traps as in the image) is still at least 1.1f; the assigned strength drops by that strength.
// The pick iterator starts value-initialised (null, not end()), as in the image: with no
// candidate the 1e9 start strength makes the ratio negative, so nothing is released.
bool AIStrategicArea::releaseSurplus(int side)
{
    std::set<int>::iterator best = std::set<int>::iterator();
    if (m_resourceNeed[side - 1] > m_assignedSquadStrength[side - 1])
        return false;
    bool bestIsPresent = true;
    int bestStrength = 1000000000;
    std::set<int>& units = m_set68[side - 1];
    for (std::set<int>::iterator it = units.begin(); it != units.end();)
    {
        Bot* const bot = IBotManager::instance->getBotFromId(*it);
        if (!bot)
        {
            units.erase(it++);
            continue;
        }
        Information* const information = informationManager->getInformation(bot->getCurrentEquipment());
        IPIUnit* const unit = information ? information->getUnit() : 0;
        if (unit)
        {
            const bool present = isPresentUnit(*it, side);
            if (!present || bestIsPresent)
            {
                const float* const strengths = unit->getStrategicStrengths();
                const int role = !m_attack[side - 1];
                if (static_cast<float>(bestStrength) > strengths[role] || (!present && bestIsPresent))
                {
                    bestStrength = static_cast<int>(strengths[role]);
                    bestIsPresent = present;
                    best = it;
                }
            }
        }
        ++it;
    }
    if (best == units.end())
        return true;
    const int remaining = m_assignedSquadStrength[side - 1] - bestStrength;
    const int ratio = remaining / m_resourceNeed[side - 1];
    if (static_cast<float>(ratio) >= 1.1f)
    {
        m_assignedSquadStrength[side - 1] = remaining;
        IAIMain::m_instance->getSAI(side)->releaseUnitFromObject(m_index, best, side);
    }
    return true;
}

// Both delegations stay virtual;
// MSVC groups the Vec2/Vec3 overload slots in the opposite order to the ELF.
// ELF 0x33A990.
float AIStrategicArea::gravity(const Vec3& position, int side)
{
    Vec2 flat;
    flat.x = 0.0f;
    flat.y = 0.0f;
    vec3ToVec2XZ(position, flat);
    return gravity(flat, side);
}

// The position is read directly
// using side-1. x87 keeps the arithmetic wide until the float call argument.
// ELF 0x33A9F0.
float AIStrategicArea::gravity(const Vec2& position, int side)
{
    const double dx = static_cast<double>(position.x) - m_position[side - 1].x;
    const double dz = static_cast<double>(position.y) - m_position[side - 1].y;
    const double squaredDistance = dx * dx + dz * dz;
    double distance = std::sqrt(squaredDistance);
#if !defined(_MSC_VER)
    // [PLATFORM] ELF retries an unordered fsqrt through sqrtf, with its input
    // narrowed to float. PE uses the hardware result without this CRT fallback.
    if (distance != distance)
        distance = std::sqrt(static_cast<float>(squaredDistance));
#endif
    return gravity(static_cast<float>(distance), side);
}

// ELF 0x33EFA0: true when the bot's current equipment is a known mobile unit; the
// present-set find and the local object list are unused in the image.
bool AIStrategicArea::retainBot(int side, int unit, ISAI*)
{
    m_set92[side - 1].find(unit);
    Bot* const bot = IBotManager::instance->getBotFromId(unit);
    Information* const information = informationManager->getInformation(bot->getCurrentEquipment());
    if (!information || !information->getMobile() || !information->getUnit())
        return false;
    std::list<AIStrategicObject*> objects;
    return true;
}

// ELF 0x33F0B0: the side's distance flag (+0x157 + side) and every vehicle-group flag of that
// side (+0x158 + 4 * side: m_flagsA for side 1, m_flagsB for side 2) are set. With zero vehicle
// groups the first flag is still written.
void AIStrategicArea::setAsDirty(int side)
{
    m_distancesDirty[side - 1] = true;
    const int groups = static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getNumberOfVehicleGroups();
    bool* const flags = side == 1 ? m_flagsA : m_flagsB;
    if (!groups)
    {
        flags[0] = true;
        return;
    }
    for (int i = 0; i < groups; ++i)
        flags[i] = true;
}

// ELF 0x33F180.
bool AIStrategicArea::isInside(const Vec2& point) const
{
    return insideCentreBox(point, m_centre, m_position[0]);
}

// ELF 0x3396E0: `lea eax, [this + 0x84]`.
const Vec2& AIStrategicArea::getCenterPosition() const
{
    return m_position[0];
}

// ELF 0x33F130.
bool AIStrategicArea::isInside(const Vec3& point) const
{
    Vec2 flat;
    flat.x = 0.0f;
    flat.y = 0.0f;
    vec3ToVec2XZ(point, flat);
    return isInside(flat);
}

// ELF 0x33EF80.
Vec3 AIStrategicArea::getBotWaypoint(Vec3, Vec3 to, const std::vector<SAWayPoint*>&, int)
{
    return to;
}

// ELF 0x339AB0: a position for a unit ordered to this area's control point. An active point
// (IAIGame +0x28) keeps `from` when it is within 0.8 radius and valid on the map (IAIPathfinding
// +0x30 with the area's island for the map); an inactive one uses twice the area's centre-to-
// position extent instead. Otherwise up to twenty-one randomized candidates around the point's
// centre (height m_f148; 0.8/radius resp. 1.0/twice-extent) are tried, then the object's order
// position (slot 5) is the answer.
void AIStrategicArea::getControlpointPos(AIStrategicObject* object, int side, int map, int unit,
                                         const Vec3& from, Vec3& position)
{
    IAIPathfinding* const pathfinding = IAIPathfinding::instance;
    IAIGame* const game = static_cast<IAIGame*>(CommonSingletonServer::m_iaiGame);
    bool active = true;
    int tries = 0;
    float radius = 1.0f;
    Vec3 centre;
    centre.x = 0.0f;
    centre.y = 0.0f;
    centre.z = 0.0f;
    game->getControlPointInfo(m_controlPointId, centre, radius, active);
    float scale;
    float spread;
    if (active)
    {
        const float dx = centre.x - from.x;
        const float dy = centre.y - from.y;
        const float dz = centre.z - from.z;
        const float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
        if (0.8f * radius > distance
            && pathfinding->isValidPosition(map, unit, from, m_mapData[map].m_island, true))
        {
            position = from;
            return;
        }
        scale = 0.8f;
        spread = radius;
    }
    else
    {
        const Vec2 corner = m_centre;
        const Vec2& anchor = getPosition(side);
        const float hx = anchor.x - corner.x;
        const float hy = anchor.y - corner.y;
        const float extent = std::sqrt(hx * hx + hy * hy);
        const float twice = extent + extent;
        const float dx = centre.x - from.x;
        const float dy = centre.y - from.y;
        const float dz = centre.z - from.z;
        const float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
        if (twice > distance
            && pathfinding->isValidPosition(map, unit, from, m_mapData[map].m_island, true))
        {
            position = from;
            return;
        }
        scale = 1.0f;
        spread = twice;
    }
    position.x = centre.x;
    position.y = m_f148;
    position.z = centre.z;
    object->randomizeControlpointPos(scale, spread, position);
    for (; tries <= 19; ++tries)
    {
        if (pathfinding->isValidPosition(map, unit, position, m_mapData[map].m_island, true))
            return;
        position.x = centre.x;
        position.y = m_f148;
        position.z = centre.z;
        object->randomizeControlpointPos(scale, spread, position);
    }
    // (sret, object, map, side): `movl 0x14(%ebp)` (map) before `0x10(%ebp)` (side) at 0x339CC8.
    position = object->getOrderPos(map, side);
}

// ELF 0x33C340: the unit's current equipment information decides the arm - object type bit
// 0x10 (`testb $0x10,0x4(%eax)`, the air bit the temperature samples also test) orders it as an
// air bot. The image does not check the information for null. eax is whatever the arm leaves.
void AIStrategicArea::orderBot(int side, const std::list<AIStrategicObject*>& objects, int unit, ISAI* sai)
{
    Bot* const bot = IBotManager::instance->getBotFromId(unit);
    Information* const information = informationManager->getInformation(bot->getCurrentEquipment());
    if (information->getObjectTypeFlags() & 0x10u)
    {
        orderAirBot(side, objects, unit, sai);
        return;
    }
    orderNormalBot(side, objects, unit, sai);
}

// ELF 0x33C430: an air path 75 above the terrain (IAITerrain::getHeight, +0x08) at this area's
// position (no objects), at the one object's, or at every object after the first. The cruise
// altitude is min(m_size[side], 4.0f for object type bit 0x20 else 40.0f) (`fcmovnbe`); the
// waypoint is WPAltitudeMoveTo(altitude, 120.0f, 50.0f, 2, path) through checkPointer (0x33C650),
// handed to the bot (+0x44, also when the allocation failed). The image does not check the
// information for null.
void AIStrategicArea::orderAirBot(int side, const std::list<AIStrategicObject*>& objects, int unit, ISAI*)
{
    std::list<Vec3> path;
    float altitude = 1.0f;
    Bot* const bot = IBotManager::instance->getBotFromId(unit);
    Information* const information = informationManager->getInformation(bot->getCurrentEquipment());
    const IAITerrain* const terrain = static_cast<const IAITerrain*>(CommonSingletonServer::m_iaiTerrain);
    const float clearance = (information->getObjectTypeFlags() & 0x20u) ? 4.0f : 40.0f;
    const std::size_t count = objects.size();
    if (count == 0 || count == 1)
    {
        const Vec2& anchor = count == 0 ? getPosition(side) : objects.front()->getPosition(side);
        const float x = anchor.x;
        Vec3 point;
        point.y = terrain->getHeight(anchor.x, anchor.y) + 75.0f;
        point.x = x;
        point.z = anchor.y;
        path.push_back(point);
        altitude = m_size[side] > clearance ? clearance : m_size[side];
    }
    else
    {
        std::list<AIStrategicObject*>::const_iterator it = objects.begin();
        for (++it; it != objects.end(); ++it)
        {
            const Vec2& anchor = (*it)->getPosition(side);
            const float x = anchor.x;
            Vec3 point;
            point.y = terrain->getHeight(anchor.x, anchor.y) + 75.0f;
            point.x = x;
            point.z = anchor.y;
            path.push_back(point);
            altitude = m_size[side] > clearance ? clearance : m_size[side];
        }
    }
    bot->setWaypoints(checkPointer(new WPAltitudeMoveTo(altitude, 120.0f, 50.0f, 2, path)));
}

// ELF 0x33A2F0 (caller SquadMember::moveToGoalPos 0x1B4240): the side's route map
// (`0x16c(%edi,%esi,4)`, i.e. m_mapsA / m_mapsB for side 1 / 2, first group element) is searched
// for `from`; when found the route is copied out (list operator=) and the answer is true.
bool AIStrategicArea::getRouteList(int side, AIStrategicArea* from, std::list<AIStrategicObject*>& route)
{
    bool found = false;
    std::map<AIStrategicObject*, std::list<AIStrategicObject*> >& routes = (side == 1 ? m_mapsA : m_mapsB)[0];
    std::map<AIStrategicObject*, std::list<AIStrategicObject*> >::iterator it = routes.find(from);
    if (it != routes.end())
    {
        found = true;
        route = it->second;
    }
    return found;
}

namespace {

float squaredDistance(const Vec3& a, const Vec3& b)
{
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    const float dz = a.z - b.z;
    return dx * dx + dy * dy + dz * dz;
}

// (a - origin) . (b - origin): positive when a lies on the side of origin facing b.
float dotFrom(const Vec3& origin, const Vec3& a, const Vec3& b)
{
    return (a.x - origin.x) * (b.x - origin.x) + (a.y - origin.y) * (b.y - origin.y)
         + (a.z - origin.z) * (b.z - origin.z);
}

// The inline AIRandom draw of orderNormalBot (0x33D390 / 0x33E764): low 0.0f, high the waypoint
// count (`fildll` of the zero-extended size, stored to float), (high - low) * (draw * 2^-32) + low
// stored to float, then `cvttss2si`.
std::size_t randomWaypointIndex(std::size_t count)
{
    const float low = 0.0f;
    const float high = static_cast<float>(static_cast<unsigned long long>(count));
    const double draw = static_cast<double>(AIRandom::m_instance->next()) * static_cast<double>(2.32830644e-10f);
    return static_cast<std::size_t>(static_cast<int>(static_cast<float>((high - low) * draw + low)));
}

// [CHOSEN] helper for a sequence orderNormalBot repeats in its three arms (0x33CB6E, 0x33E205,
// 0x33D6B2): randomizePos(side, 0.8f) around `object`, up to twenty candidates checked against
// the ordering area's island for the map (IAIPathfinding +0x30, always the ordering area's
// +0x118 data), then the object's order position (slot 5, (map, side)).
void findOrderPosition(const AIStrategicArea* area, AIStrategicObject* object, int side, int map,
                       int equipment, Vec3& position)
{
    IAIPathfinding* const pathfinding = IAIPathfinding::instance;
    object->randomizePos(side, 0.8f, position);
    for (int tries = 0; tries <= 19; ++tries)
    {
        if (pathfinding->isValidPosition(map, equipment, position, area->m_mapData[map].m_island, true))
            return;
        object->randomizePos(side, 0.8f, position);
    }
    position = object->getOrderPos(map, side);
}

// [CHOSEN] helper for the `hasSameAssignedAreas` arm (0x33DC66 / 0x33EB83): the bot keeps its
// current waypoint (through getBotWaypoint) when it still leads towards `goal` and is closer to it
// than the bot; it is then stored again (Bot +0x20C) and the stored one (+0x210) becomes a goal
// with area -1.
bool keepCurrentWaypoint(AIStrategicArea* area, Bot* bot, Information* information, int side, int map,
                         const Vec3& botPosition, const Vec3& goal, float distance,
                         const std::vector<SAWayPoint*>& waypoints, WPMoveTo::GoalList& goals)
{
    const Vec3 current = static_cast<BotMain*>(bot)->getCurrentWaypoint();
    const Vec3 from = information->getPosition();
    const Vec3 next = area->getBotWaypoint(from, current, waypoints, side);
    if (dotFrom(botPosition, next, goal) > 0.0f && squaredDistance(next, goal) < distance)
    {
        static_cast<BotMain*>(bot)->setCurrentWaypoint(next, map);
        const Vec3 stored = static_cast<BotMain*>(bot)->getCurrentWaypoint();
        goals.push_back(std::make_pair(stored, -1));
        return true;
    }
    return false;
}

bool isOrderableControlPoint(const AIStrategicArea* area, int side)
{
    return area->hasObjectTypeFlag(AIStrategicObject::ControlPoint)
        && area->m_controlPointId != 0xFFFFFFFFu && area->getStatus(side) != StatusMap::Owned;
}

} // namespace

// ELF 0x33C940 (0x33C940..0x33EF80). A ground unit's goal list (WPMoveTo goals, radius 60):
// * no objects: this area's control point (getControlpointPos, goal radius 0) when the area is a
//   control point (+0x13C set, side status not Owned); else the bot's own position when it is
//   inside the area and valid on the map, else a randomized order position (findOrderPosition);
//   radius m_size[side] / 4 + the bot's diameter (IPIPhysical +0x2C, `fadd %st,%st`).
// * one object: a route waypoint between the bot's current strategic position (ISAI +0xB4) and
//   the object first (AIStrategicObjectManager::getWaypoints; Bot +0x200..+0x214 track the
//   assigned pair): the kept current waypoint, or one of three random picks that lies towards
//   this area's position and closer to it than the bot, with a valid polygon. Then the object's
//   (or this control point's) position with this area's index.
// * several objects: the same per consecutive pair (only the first pair updates the bot's
//   assigned areas; only the first pick is direction-tested), each object's position with its own
//   index; the radius is the last object's.
// Bot +0x200..+0x214 are declared on BotMain only (the established static_cast<BotMain*> form).
// The waypoint list replaces the bot's unless SquadMember::isSameWaypointList says it is the same
// (then it is deleted). The image does not check the information or the squad member for null.
// ELF 0x33C940.
void AIStrategicArea::orderNormalBot(int side, const std::list<AIStrategicObject*>& objects, int unit, ISAI* sai)
{
    WPMoveTo::GoalList goals;
    float radius = 1.0f;
    Vec2 botPosition2;
    botPosition2.x = 0.0f;
    botPosition2.y = 0.0f;
    Vec3 controlPointPosition;
    controlPointPosition.x = 0.0f;
    controlPointPosition.y = 0.0f;
    controlPointPosition.z = 0.0f;
    Vec3 position = controlPointPosition;
    IAIPathfinding* const pathfinding = IAIPathfinding::instance;
    Bot* const bot = IBotManager::instance->getBotFromId(unit);
    Information* const information = informationManager->getInformation(bot->getCurrentEquipment());
    IPIMobile* const mobile = information->getMobile();
    const float radiusPhysical = information->getPhysical()->getRadius();
    const float diameter = radiusPhysical + radiusPhysical;
    const int map = mobile->getVehicleNumber();
    const int equipment = information->getUnit()->getEquipmentType();
    const Vec3& botPosition = information->getPosition();
    vec3ToVec2XZ(botPosition, botPosition2);
    WPMoveTo* waypoint = 0;

    if (objects.empty())
    {
        isLandingZoneUser(equipment);                       // 0x33CAB8: the answer is unused
        if (isOrderableControlPoint(this, side))
        {
            getControlpointPos(this, side, map, equipment, botPosition, controlPointPosition);
            goals.push_back(std::make_pair(controlPointPosition, m_index));
            waypoint = checkPointer(new WPMoveTo(0.0f, 60.0f, 2, goals));
        }
        else
        {
            bool found = false;
            if (isInside(botPosition2)
                && pathfinding->isValidPosition(map, equipment, information->getPosition(),
                                                m_mapData[map].m_island, true))
            {
                position = information->getPosition();
                found = true;
            }
            if (!found)
                findOrderPosition(this, this, side, map, equipment, position);
            goals.push_back(std::make_pair(position, m_index));
            waypoint = checkPointer(new WPMoveTo(m_size[side] * 0.25f + diameter, 60.0f, 2, goals));
        }
    }
    else if (objects.size() == 1)
    {
        AIStrategicObject* const object = objects.front();
        if (object->getType() == 0)
            static_cast<AIStrategicArea*>(object)->isLandingZoneUser(equipment);   // unused
        AIStrategicObjectManager* const manager = AIStrategicObjectManager::getInstance();
        const int current = sai->getBotCurrentPos(unit);
        const int target = object->getIndex();
        if (current != -1 && current != target)
        {
            // The goal of the route is this area's position (slot 3 on `this`, height m_f148).
            Vec3 goal;
            goal.x = 0.0f;
            goal.y = 0.0f;
            goal.z = 0.0f;
            vec2ToVec3XZ(getPosition(side), goal);
            goal.y = m_f148;
            static_cast<BotMain*>(bot)->setNewAssignedAreas(current, target);
            const std::vector<SAWayPoint*>& waypoints = manager->getWaypoints(map, current, target);
            const float distance = squaredDistance(goal, botPosition);
            if (static_cast<BotMain*>(bot)->hasSameAssignedAreas(map))
            {
                keepCurrentWaypoint(this, bot, information, side, map, botPosition, goal, distance,
                                    waypoints, goals);
            }
            else
            {
                bool added = false;
                if (!waypoints.empty())
                {
                    for (int tries = 0;;)
                    {
                        const SAWayPoint* const candidate = waypoints[randomWaypointIndex(waypoints.size())];
                        if (dotFrom(botPosition, candidate->getPosition(), goal) > 0.0f
                            && squaredDistance(candidate->getPosition(), goal) < distance
                            && candidate->getPolygon() != 0xFFFFFFFFu)
                        {
                            goals.push_back(std::make_pair(candidate->getPosition(), -1));
                            static_cast<BotMain*>(bot)->setCurrentWaypoint(candidate->getPosition(), map);
                            added = true;
                            break;
                        }
                        if (++tries > 2)
                            break;
                    }
                }
                if (!added)
                    static_cast<BotMain*>(bot)->resetAssignedAreas();
            }
            static_cast<BotMain*>(bot)->copyAssignedAreas();
        }
        if (object == this && isOrderableControlPoint(this, side))
        {
            getControlpointPos(this, side, map, equipment, botPosition, controlPointPosition);
            goals.push_back(std::make_pair(controlPointPosition, m_index));
            waypoint = checkPointer(new WPMoveTo(0.0f, 60.0f, 2, goals));
        }
        else
        {
            findOrderPosition(this, object, side, map, equipment, position);
            goals.push_back(std::make_pair(position, m_index));
            waypoint = checkPointer(new WPMoveTo(objects.front()->m_size[side] * 0.25f + diameter,
                                                 60.0f, 2, goals));
        }
    }
    else
    {
        int previous = sai->getBotCurrentPos(unit);
        bool first = true;
        bool assigned = false;
        AIStrategicObjectManager* const manager = AIStrategicObjectManager::getInstance();
        for (std::list<AIStrategicObject*>::const_iterator it = objects.begin(); it != objects.end(); ++it)
        {
            AIStrategicObject* const object = *it;
            const int target = object->getIndex();
            if (previous != -1 && previous != target)
            {
                Vec3 goal;
                goal.x = 0.0f;
                goal.y = 0.0f;
                goal.z = 0.0f;
                vec2ToVec3XZ(object->getPosition(side), goal);
                goal.y = object->m_f148;
                const std::vector<SAWayPoint*>& waypoints = manager->getWaypoints(map, previous, target);
                bool added = false;
                const float distance = squaredDistance(goal, botPosition);
                if (!assigned)
                {
                    static_cast<BotMain*>(bot)->setNewAssignedAreas(previous, target);
                    if (static_cast<BotMain*>(bot)->hasSameAssignedAreas(map))
                        added = keepCurrentWaypoint(this, bot, information, side, map, botPosition, goal,
                                                    distance, waypoints, goals);
                    static_cast<BotMain*>(bot)->copyAssignedAreas();
                }
                if (waypoints.empty())
                    static_cast<BotMain*>(bot)->resetAssignedAreas();              // 0x33DBEB, whatever `assigned` is
                if (!waypoints.empty() && !added)
                {
                    for (int tries = 0;;)
                    {
                        const SAWayPoint* const candidate = waypoints[randomWaypointIndex(waypoints.size())];
                        if ((!first || dotFrom(botPosition, candidate->getPosition(), goal) > 0.0f)
                            && candidate->getPolygon() != 0xFFFFFFFFu
                            && squaredDistance(candidate->getPosition(), goal) < distance)
                        {
                            goals.push_back(std::make_pair(candidate->getPosition(), -1));
                            if (!assigned)
                                static_cast<BotMain*>(bot)->setCurrentWaypoint(candidate->getPosition(), map);
                            added = true;
                            break;
                        }
                        if (++tries > 2)
                            break;
                    }
                    if (!assigned && !added)
                        static_cast<BotMain*>(bot)->resetAssignedAreas();
                }
                assigned = true;
            }
            first = false;
            if (object == this && isOrderableControlPoint(this, side))
            {
                getControlpointPos(this, side, map, equipment, botPosition, controlPointPosition);
                goals.push_back(std::make_pair(controlPointPosition, m_index));
                radius = 0.0f;
            }
            else
            {
                findOrderPosition(this, object, side, map, equipment, position);
                goals.push_back(std::make_pair(position, object->getIndex()));
                radius = object->m_size[side] * 0.25f + diameter;
            }
            previous = target;
        }
        waypoint = checkPointer(new WPMoveTo(radius, 60.0f, 2, goals));
    }

    // 0x33CDA0: the bot's list (+0x40) is read before its squad member (+0x140).
    WaypointList* const currentList = bot->getWaypoints();
    if (!bot->getSquadMember()->isSameWaypointList(currentList, waypoint))
        bot->setWaypoints(waypoint);
    else
        delete waypoint;                                    // slot 1 when non-null
}

// The size uses side, unlike the
// position's side-1 index above. Unordered distance takes the division arm.
// ELF 0x33AA80.
float AIStrategicArea::gravity(float distance, int side)
{
    if (distance <= 1.0f)
        return m_size[side];
    const double wideDistance = distance;
    return static_cast<float>(m_size[side] / (wideDistance * wideDistance));
}

// ELF 0x339890 (added by ai-bot). The flag test is vtable +0x4C with 0x10 (ControlPoint).
bool AIStrategicArea::isOrderPosOutsideCpRadius(int map, int side)
{
    bool outside = false;
    if (hasObjectTypeFlag(ControlPoint))
    {
        const Vec3 order = getOrderPos(map, side);
        float radius = 0.0f;
        Vec3 centre;
        centre.x = 0.0f;
        centre.y = 0.0f;
        centre.z = 0.0f;
        bool active;
        static_cast<const IAIGame*>(CommonSingletonServer::m_iaiGame)
            ->getControlPointInfo(m_controlPointId, centre, radius, active);
        const float dx = order.x - centre.x;
        const float dy = order.y - centre.y;
        const float dz = order.z - centre.z;
        if (std::sqrt(dx * dx + dy * dy + dz * dz) > radius)
            outside = true;
    }
    return outside;
}

// ELF 0x338FF0 (added by ai-bot). The records are read as two Vec3 (+0 / +0xC of the node value).
bool AIStrategicArea::getClosestFlankingPositions(Vec3 position, Vec3& first, Vec3& second)
{
    bool found = false;
    float best = 3.40282347e+38f;
    for (std::list<std::pair<Vec3, Vec3> >::const_iterator it = m_flankPositions.begin();
         it != m_flankPositions.end(); ++it)
    {
        const Vec3 a = it->first;
        const Vec3 b = it->second;
        const float dx = a.x - position.x;
        const float dy = a.y - position.y;
        const float dz = a.z - position.z;
        const float d = dx * dx + dy * dy + dz * dz;
        if (d < best)
        {
            best = d;
            first = a;
            second = b;
            found = true;
        }
    }
    return found;
}

// ELF 0x338EC0.
void AIStrategicArea::addFlankPosition(Vec3 first, Vec3 second)
{
    m_flankPositions.push_back(std::pair<Vec3, Vec3>(first, second));
}

// ELF 0x338F50: a copy of the flanking pairs.
std::list<std::pair<Vec3, Vec3> > AIStrategicArea::getFlankPositions()
{
    return m_flankPositions;
}

// ELF 0x3391A0: appended unless already present.
void AIStrategicArea::addNeighbour(AIStrategicObject* object)
{
    for (int i = 0; i < static_cast<int>(m_neighbours.size()); ++i)
    {
        if (m_neighbours[i] == object)
            return;
    }
    m_neighbours.push_back(object);
}

// ELF 0x339230. Shipped defect, preserved: the shift starts at the match, so it overwrites the
// element before the match (index -1 when the match is first) and keeps the match itself; the
// vector then shrinks by one whether or not the object was found.
void AIStrategicArea::removeNeighbour(AIStrategicObject* object)
{
    const int size = static_cast<int>(m_neighbours.size());
    for (int i = 0; i < size; ++i)
    {
        if (m_neighbours[i] == object)
        {
            for (int j = i; j < size; ++j)
                m_neighbours[j - 1] = m_neighbours[j];
            break;
        }
    }
    m_neighbours.resize(size - 1);
}

// ELF 0x3393E0: grown by one through resize, unless already present.
void AIStrategicArea::addNeighbouringObject(AIStrategicObject* object)
{
    const int size = static_cast<int>(m_neighbouringObjects.size());
    for (int i = 0; i < size; ++i)
    {
        if (m_neighbouringObjects[i] == object)
            return;
    }
    m_neighbouringObjects.resize(size + 1);
    m_neighbouringObjects[size] = object;
}

// ELF 0x339500: the neighbours, the neighbouring objects and the attached landing zones.
void AIStrategicArea::removeAllNeighbours()
{
    m_neighbours.resize(0);
    m_neighbouringObjects.resize(0);
    m_landingZones.resize(0);
}

// ELF 0x339680: both sides' positions.
void AIStrategicArea::setPosition(const Vec2& position)
{
    m_position[0] = position;
    m_position[1] = position;
}

// ELF 0x3396B0: the same stores as setPosition.
void AIStrategicArea::setCenterPosition(const Vec2& position)
{
    m_position[0] = position;
    m_position[1] = position;
}

// ELF 0x3396F0.
void AIStrategicArea::setTopLeftCorner(const Vec2& corner)
{
    m_centre = corner;
}

// ELF 0x339710. Before initOrderPoses the position is stored absolute; afterwards the map is
// queried with the stored (old) offset, as initOrderPoses does, and the new position is stored
// relative to the area position together with the island the query answered. Always true.
bool AIStrategicArea::setOrderPos(int map, const Vec3& position)
{
    if (!m_orderPosesInitialised)
    {
        m_mapData[map].m_offset = position;
        return true;
    }

    Vec3 areaPosition;
    areaPosition.x = 0.0f;
    areaPosition.y = 0.0f;
    areaPosition.z = 0.0f;
    vec2ToVec3XZ(m_position[0], areaPosition);
    areaPosition.y = m_f148;

    unsigned int polygon;
    unsigned int island;
    IAIPathfinding::instance->getPolygon(m_mapData[map].m_offset, map, polygon, island, 1);

    Vec3 delta;
    delta.x = position.x - areaPosition.x;
    delta.y = position.y - areaPosition.y;
    delta.z = position.z - areaPosition.z;
    m_mapData[map].m_offset = delta;
    m_mapData[map].m_island = island;
    return true;
}

// ELF 0x33A2B0.
int AIStrategicArea::setObjectTypeFlags(int flags)
{
    return m_objectTypeFlags = flags;
}

// ELF 0x33A2D0.
int AIStrategicArea::addObjectTypeFlag(StrategicObjectType type)
{
    return m_objectTypeFlags |= 1 << type;
}

// ELF 0x33B580: appends the named zone (grown through resize) and answers its name, or a
// message when the manager has no such zone. The zone name is read directly, not through
// AILandingZone::getName.
std::string AIStrategicArea::attachLandingZone(const std::string& name)
{
    AILandingZone* const zone = AILandingZoneManager::m_instance->getAILandingZone(name);
    if (!zone)
        return "No zone by that name found";

    const int size = static_cast<int>(m_landingZones.size());
    m_landingZones.resize(size + 1);
    m_landingZones[size] = zone;
    return zone->m_name;
}

// ELF 0x33B6E0: unchecked index.
AILandingZone* AIStrategicArea::getLandingZone(int index)
{
    return m_landingZones[index];
}

// ELF 0x33B700: the attached zone with the id, and its index; 0 and -1 when none matches.
AILandingZone* AIStrategicArea::getLandingZone(int id, int& index)
{
    index = 0;
    for (std::vector<AILandingZone*>::iterator it = m_landingZones.begin();
         it != m_landingZones.end(); ++it)
    {
        if ((*it)->getId() == id)
            return *it;
        ++index;
    }
    index = -1;
    return 0;
}

// ELF 0x33B780.
std::vector<AILandingZone*>& AIStrategicArea::getLandingZones()
{
    return m_landingZones;
}

// ELF 0x33B790.
const std::vector<AILandingZone*>& AIStrategicArea::getLandingZones() const
{
    return m_landingZones;
}

// ELF 0x33B7A0: one attached zone name per line.
std::string AIStrategicArea::listLandingZones()
{
    std::stringstream stream;
    for (int i = 0; i < static_cast<int>(m_landingZones.size()); ++i)
    {
        const std::string name = m_landingZones[i]->m_name;
        stream << name << std::endl;
    }
    return stream.str();
}

// ELF 0x33BC40: the attached zone with the smallest squared distance below 1e9; 0 when none.
AILandingZone* AIStrategicArea::getClosestLandingZone(const Vec2& position)
{
    float best = 1e9f;
    AILandingZone* closest = 0;
    for (std::vector<AILandingZone*>::iterator it = m_landingZones.begin();
         it != m_landingZones.end(); ++it)
    {
        const float distance = (*it)->getDistanceSqr(position);
        if (distance < best)
        {
            best = distance;
            closest = *it;
        }
    }
    return closest;
}

// ELF 0x33BCC0: the XZ projection through the Vec2 form.
AILandingZone* AIStrategicArea::getClosestLandingZone(const Vec3& position)
{
    Vec2 flat;
    flat.x = 0.0f;
    flat.y = 0.0f;
    vec3ToVec2XZ(position, flat);
    return getClosestLandingZone(flat);
}

// ELF 0x33BD10: the equipment bit isLandingZoneUser tests.
void AIStrategicArea::addToLandingZoneUsers(int equipment)
{
    m_i340 |= 1 << equipment;
}

// ELF 0x33C260: clears the group bit in every distance record of the side and erases the
// records left without groups.
void AIStrategicArea::cleanUnitDistances(int side, int group)
{
    std::list<AreaDistanceInfo>::iterator it = m_distances[side - 1].begin();
    while (it != m_distances[side - 1].end())
    {
        it->m_vehicleGroups &= ~(1 << group);
        if (it->m_vehicleGroups == 0)
            m_distances[side - 1].erase(it++);
        else
            ++it;
    }
}

// ELF 0x33F1C0: known vehicles with a free seat within m_f396 of the side's position.
// The ELF passes vec2ToVec3XZ's eax (the output address) as the position; the output local is the
// same object.
void AIStrategicArea::getKnownEmptyVehicles(std::vector<unsigned int>& vehicles, int side) const
{
    const IAIInformationEnvironment* const environment =
        static_cast<const IAIInformationEnvironment*>(AISingletonServer::m_iaiInformationEnvironment);
    Vec3 position;
    position.x = 0.0f;
    position.y = 0.0f;
    position.z = 0.0f;
    FreeUnitPred predicate(side);
    vec2ToVec3XZ(m_position[side - 1], position);
    environment->getObjectsWithinRadius(vehicles, position, m_f396, side, true, &predicate);
}

} // namespace ai
} // namespace hfe
} // namespace dice
