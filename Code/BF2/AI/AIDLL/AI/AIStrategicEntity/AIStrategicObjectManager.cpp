#include "AIStrategicObjectManager.h"

// <iostream>'s ios_base::Init object is this TU's static initialisation and teardown (ELF
// 0x34A400, __tcf_0 0x34A460, _GLOBAL__I 0x34A490).
#include <iostream>

#include "AIStrategicArea.h"                          // createFromControlPoint constructs one
#include <cmath>
#include <cstring>                                    // checkLayerStrList's strtok
#include "../AIKnowledge/Information.h"
#include "../../../AIInterface/BFKnowledge/AIPlayerFactory.h"
#include "../../../AIInterface/BFKnowledge/AIPlayer.h"
#include "../AIBot/SquadManager.h"                  // haveHumanPlayerOnOwnSide
#include "../AIKnowledge/InformationManager.h"
#include "../AIKnowledge/IPIPhysical.h"
#include "../../../../Math/Mat4.h"
#include "../../../AIInterface/BFKnowledge/IAIObject.h"
#include "SAWayPoint.h"                               // the destructor and postLoadInitAreas own them
#include "../AIUtils/checkPointer.h"
#include "../AIUtils/DebugRendering/AIRenderUtil.h"   // addStrategicObject's null-guarded tail
#include "../Main/AIDllFrontier.h"
#include "../Main/AISettingsInterfaces.h"
#include "../Main/CommonSingletonServer.h"
#include "../Main/IAIPathfinding.h"
#include "../Main/AIStreamOperators.h"                // StrategicObjectType names
#include "../../../../Core/StringUtil.h"              // formatString

namespace dice {
namespace hfe {
namespace ai {

AIStrategicObjectManager* AIStrategicObjectManager::m_instance = 0;

// ELF 0x344F30: the object with the strongest gravity at the position among those allowing the
// equipment's vehicle group and not expelling it; areas that use the vehicle as a landing-zone
// user are skipped when a vehicle is given. Objects are not null-checked; ties keep the first.
AIStrategicObject* AIStrategicObjectManager::getClosestObject(const Vec3& position, int equipment,
                                                              int vehicle, int side) const
{
    AIStrategicObject* best = 0;
    float bestGravity = 0.0f;
    const int group = static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getVehicleGroup(equipment);
    for (std::vector<AIStrategicObject*>::const_iterator it = m_strategicObjects.begin();
         it != m_strategicObjects.end(); ++it)
    {
        if (!(*it)->isAllowedVehicleGroup(group) || (*it)->isExpelledUnit(equipment))
            continue;
        if ((*it)->getType() == 0 && vehicle != -1
            && static_cast<AIStrategicArea*>(*it)->isLandingZoneUser(vehicle))
            continue;
        const float gravity = (*it)->gravity(position, side);
        if (gravity > bestGravity)
        {
            bestGravity = gravity;
            best = *it;
        }
    }
    return best;
}

// ELF 0x344E80, added by ai-w1-e-fire for BBPFireInfantery::createPlanInternal (0x265DA6;
// coord/ai-requests.md 2026-11-01): as getClosestObject(position, side), but only objects whose
// status for the side is 0 (`status == 0` when the flag is set, `status != 0` otherwise). The
// gravity is evaluated before the status test, as in the image.
AIStrategicObject* AIStrategicObjectManager::getClosestObject(const Vec3& position, int side,
                                                              bool statusZero) const
{
    AIStrategicObject* closest = 0;
    float best = -1.0f;
    for (std::vector<AIStrategicObject*>::const_iterator it = m_strategicObjects.begin();
         it != m_strategicObjects.end(); ++it)
    {
        const double gravity = (*it)->gravity(position, side);
        const bool zero = (*it)->getStatus(side) == 0;
        if (zero != statusZero)
            continue;
        if (gravity > best)
        {
            best = static_cast<float>(gravity);
            closest = *it;
        }
    }
    return closest;
}

// ELF 0x346D40: the active-area cursor is the lookup result, end() when not found.
bool AIStrategicObjectManager::setActive(std::string name)
{
    m_activeArea = m_strategicAreas.find(name);
    return m_activeArea != m_strategicAreas.end();
}

// "Closest" is the greatest virtual gravity
// starting at -1, not a distance calculation here. Strict comparison preserves
// first-wins ties and ignores unordered results. Both ports reload the selected
// pointer after gravity and the vector end on each iteration; neither guards nulls.
// ELF 0x344DF0.
AIStrategicObject* AIStrategicObjectManager::getClosestObject(
    const Vec3& position, int side) const
{
    AIStrategicObject* closest = 0;
    float best = -1.0f;
    for (std::vector<AIStrategicObject*>::const_iterator it = m_strategicObjects.begin();
         it != m_strategicObjects.end(); ++it)
    {
        const double gravity = (*it)->gravity(position, side);
        if (gravity > best)
        {
            best = static_cast<float>(gravity);
            closest = *it;
        }
    }
    return closest;
}

// WPContainer - , each.
// mov eax, this
// test a2, a2
// jz ret ; -> this
// lea eax, [this+12] ; -> this+12
// so any non-zero selects the second list. Written literally.

// ELF 0x343340 (C2; C1 0x343380): both lists empty.
AIStrategicObjectManager::WPContainer::WPContainer()
    : m_wayPointsA()
    , m_wayPointsB()
{
}

// ELF 0x3433C0 (C2; C1 0x3434F0): member-wise vector copies.
AIStrategicObjectManager::WPContainer::WPContainer(const WPContainer& other)
    : m_wayPointsA(other.m_wayPointsA)
    , m_wayPointsB(other.m_wayPointsB)
{
}

// ELF 0x343660 (D2; D1 0x3436D0): the vectors go as members; the waypoints are not deleted here.
AIStrategicObjectManager::WPContainer::~WPContainer()
{
}

// ELF 0x343640.
std::vector<SAWayPoint*>& AIStrategicObjectManager::WPContainer::getWayPoints(int type)
{
    if (type)
        return m_wayPointsB;

    return m_wayPointsA;
}

// ELF 0x343620.
const std::vector<SAWayPoint*>& AIStrategicObjectManager::WPContainer::getWayPoints(int type) const
{
    if (type)
        return m_wayPointsB;

    return m_wayPointsA;
}

// ELF 0x348E20: the container of the unordered area pair (min, max) - found unchecked, as in the
// image - and its list of the given type.
const std::vector<SAWayPoint*>& AIStrategicObjectManager::getWaypoints(int type, int a, int b) const
{
    const std::pair<int, int> key(a < b ? a : b, a > b ? a : b);
    return m_wayPoints.find(key)->second.getWayPoints(type);
}

// ELF 0x348EB0: by the two areas' indices (+0xF0).
const std::vector<SAWayPoint*>& AIStrategicObjectManager::getWaypoints(int type, const AIStrategicArea* a,
                                                                      const AIStrategicArea* b) const
{
    return getWaypoints(type, a->getIndex(), b->getIndex());
}

// Initializes strategic-area collections and the active selection.

AIStrategicObjectManager::AIStrategicObjectManager()
    : m_debugEnabled(false)
    , m_drawVehicleSearchRadii(false)
    , m_wayPointsToDrawFirst(-1)
    , m_wayPointsToDrawSecond(-1)
{
    m_strategicObjects.resize(0);
    m_objectsByCategory.resize(0);

    if (!m_instance)
        m_instance = this;

    m_activeArea = m_strategicAreas.end();

    m_categoryNames.resize(0);
}

// Deletes owned areas, waypoints and bridges, and clears the singleton if it refers to this object.

AIStrategicObjectManager::~AIStrategicObjectManager()
{
    if (m_instance == this)
        m_instance = 0;

    for (std::map<std::string, AIStrategicArea*>::iterator area = m_strategicAreas.begin();
         area != m_strategicAreas.end(); )
    {
        if (area->second)
            delete area->second;

        m_strategicAreas.erase(area++);
    }

    for (std::map<std::pair<int, int>, WPContainer>::iterator entry = m_wayPoints.begin();
         entry != m_wayPoints.end(); ++entry)
    {
        std::vector<SAWayPoint*>& first = entry->second.getWayPoints(0);

        for (std::vector<SAWayPoint*>::iterator point = first.begin();
             point != first.end(); ++point)
        {
            if (*point)
                delete *point;
        }

        std::vector<SAWayPoint*>& second = entry->second.getWayPoints(1);

        for (std::vector<SAWayPoint*>::iterator point = second.begin();
             point != second.end(); ++point)
        {
            if (*point)
                delete *point;
        }
    }

    deleteBridges();
}

// Initializes areas and waypoints after level loading, before pathfinding-map initialization.
// Waypoint containers use an unordered area pair as their key.

// ELF 0x347CF0.
void AIStrategicObjectManager::postLoadInitAreas()
{
    WPContainer prototype;

    for (std::map<std::string, AIStrategicArea*>::iterator entry = m_strategicAreas.begin();
         entry != m_strategicAreas.end(); ++entry)
    {
        AIStrategicArea* const area = entry->second;

        area->postLoadInit();

        for (std::vector<AIStrategicObject*>::iterator neighbour = area->m_neighbours.begin();
             neighbour != area->m_neighbours.end(); ++neighbour)
        {
            const int mine  = area->m_index;
            const int other = (*neighbour)->m_index;

            const int low  = (mine < other)  ? mine : other;
            const int high = (mine >= other) ? mine : other;

            m_wayPoints.insert(
                std::pair<std::pair<int, int>, WPContainer>(std::pair<int, int>(low, high),
                                                            prototype));
        }
    }

    for (std::map<std::pair<int, int>, WPContainer>::iterator entry = m_wayPoints.begin();
         entry != m_wayPoints.end(); ++entry)
    {
        std::vector<SAWayPoint*>& first = entry->second.getWayPoints(0);

        for (std::vector<SAWayPoint*>::iterator point = first.begin();
             point != first.end(); ++point)
        {
            (*point)->init();
        }

        std::vector<SAWayPoint*>& second = entry->second.getWayPoints(1);

        for (std::vector<SAWayPoint*>::iterator point = second.begin();
             point != second.end(); ++point)
        {
            (*point)->init();
        }
    }
}

// The emitted loop walks the raw node chain and frees each `BridgeSegment*` with a bare
// `operator delete` - no destructor call, which is what `delete` compiles to for a trivially
// destructible type and is how `BridgeSegment` was pinned as a plain record. It then calls
// `_List_base::__clear`, which in this libstdc++ both frees the nodes and re-links the header, so
// the source is `clear` and calling it twice (here and again from `~list`) is safe.

// ELF 0x349A10.
void AIStrategicObjectManager::deleteBridges()
{
    for (std::list<BridgeSegment*>::iterator i = m_bridgeSegments.begin();
         i != m_bridgeSegments.end(); ++i)
    {
        delete *i;
    }

    m_bridgeSegments.clear();
}

// Tracks each announced AI-enabled bridge segment by object pointer. Lookup stops at
// the first match; unknown objects are ignored by update and removal operations.
// ELF 0x3498E0.
void AIStrategicObjectManager::createBridgeSegment(IAIObject* object)
{
    BridgeSegment* const segment = new BridgeSegment;   // zeroes the position only - +4 stays unset
    segment->m_object = object;
    segment->m_destroyed = false;
    // The position is zeroed again, as its own statement: the PE stores it a second time outside
    // the new-expression's null-checked constructor block, the ELF copies a zeroed 12-byte temporary.
    const Vec3 origin = { 0.0f, 0.0f, 0.0f };
    segment->m_position = origin;
    m_bridgeSegments.push_back(segment);
}

// The record is freed with a bare delete (a plain struct), then its node is erased.
// ELF 0x349970.
void AIStrategicObjectManager::removeBridgeSegment(IAIObject* object)
{
    for (std::list<BridgeSegment*>::iterator i = m_bridgeSegments.begin();
         i != m_bridgeSegments.end(); ++i)
    {
        if ((*i)->m_object == object)
        {
            delete *i;
            m_bridgeSegments.erase(i);
            return;
        }
    }
}

// ELF 0x349E00.
void AIStrategicObjectManager::bridgeSegmentDestroyed(IAIObject* object, int value)
{
    for (std::list<BridgeSegment*>::iterator i = m_bridgeSegments.begin();
         i != m_bridgeSegments.end(); ++i)
    {
        if ((*i)->m_object == object)
        {
            (*i)->m_destroyed = true;
            (*i)->m_i4 = value;
            return;
        }
    }
}

// ELF 0x349ED0 (listing). Every segment's information position is read first (the object's
// information for side 1, IAIObject +0xB8, through the information manager; Information +0x30),
// including for intact segments, and neither lookup is null-tested. A destroyed segment counts
// unless it was destroyed by a human of `side` (the destroying player's id is +4;
// IAIPlayerFactory +0x20, getSide +0x6C, getIsAIPlayer +0x88) or, with no such player, a human of
// `side` is in the game (SquadManager::haveHumanPlayerOnOwnSide) - both only when no repair was
// requested. The nearest by squared distance from `from` to the information position wins: its
// record position and that squared distance are written out and its object returned; 0 for none.
IAIObject* AIStrategicObjectManager::getClosestDestroyedBridgeSegment(Vec3 from, bool repairRequested, int side,
                                                                      Vec3& position, float& distance)
{
    IAIObject* closest = 0;
    float best = 3.4028235e38f;
    Vec3 segmentPosition = { 0.0f, 0.0f, 0.0f };
    IAIPlayerFactory* const players = static_cast<IAIPlayerFactory*>(CommonSingletonServer::m_iaiPlayerFactory);
    for (std::list<BridgeSegment*>::iterator i = m_bridgeSegments.begin(); i != m_bridgeSegments.end(); ++i)
    {
        BridgeSegment* const segment = *i;
        segmentPosition = informationManager->getInformation(segment->m_object->getInformation(1))->getPosition();
        if (!segment->m_destroyed)
            continue;
        bool ownHuman = false;
        IAIPlayer* const destroyer = players->getPlayerFromId(segment->m_i4);              // +0x20
        if (destroyer)
        {
            if (destroyer->getSide() == side)                                            // +0x6C
                ownHuman = !destroyer->getIsAIPlayer();                                  // +0x88
        }
        else if (SquadManager::m_instance->haveHumanPlayerOnOwnSide(side))
        {
            ownHuman = true;
        }
        if (!repairRequested && ownHuman)
            continue;
        const float dx = segmentPosition.x - from.x;
        const float dy = segmentPosition.y - from.y;
        const float dz = segmentPosition.z - from.z;
        const float d = dx * dx + dy * dy + dz * dz;
        if (best > d)
        {
            best = d;
            closest = segment->m_object;
            distance = d;
            position = segment->m_position;
        }
    }
    return closest;
}

// Only the flag is cleared; +4 keeps the value the destruction wrote.
// ELF 0x349E70.
void AIStrategicObjectManager::bridgeSegmentRepaired(IAIObject* object)
{
    for (std::list<BridgeSegment*>::iterator i = m_bridgeSegments.begin();
         i != m_bridgeSegments.end(); ++i)
    {
        if ((*i)->m_object == object)
        {
            (*i)->m_destroyed = false;
            return;
        }
    }
}

// ELF 0x345150 (from BF2AI::clear): clears the initialised flag, deletes every named area (slot 1)
// while erasing it node by node, clears the area names, points the active area at end(), empties
// the object list, the category lists and the category names (each through resize(0), the
// `size == 0 ? _M_fill_insert : erase` pair), then erases the category map node by node. The way
// points and bridge segments are left alone. Always true.
bool AIStrategicObjectManager::reset()
{
    m_debugEnabled = false;
    for (std::map<std::string, AIStrategicArea*>::iterator area = m_strategicAreas.begin();
         area != m_strategicAreas.end();)
    {
        if (area->second)
            delete area->second;
        m_strategicAreas.erase(area++);
    }
    if (!m_areaNames.empty())
        m_areaNames.clear();
    m_activeArea = m_strategicAreas.end();
    m_strategicObjects.resize(0, 0);
    m_objectsByCategory.resize(0, std::vector<AIStrategicObject*>());
    m_categoryNames.resize(0, std::string());
    for (std::map<std::string, int>::iterator category = m_categories.begin(); category != m_categories.end();)
        m_categories.erase(category++);
    return true;
}

// ELF 0x345520: clears the initialised flag and resets every area's status; always true.
bool AIStrategicObjectManager::resetAreas()
{
    m_debugEnabled = false;
    for (std::map<std::string, AIStrategicArea*>::iterator it = m_strategicAreas.begin();
         it != m_strategicAreas.end(); ++it)
        it->second->resetStatus();
    return true;
}

// ELF 0x349A80. Every segment is marked intact again and gets its direction (+0x0C) back: the
// unit vector to the first other segment whose squared distance d lies in (0.01, (2r + 5)^2),
// r being this segment's physical radius (the bound is stored as a float before the compare);
// without such a neighbour, the object's transformation row 2 (+0x20). A neighbour distance is
// kept as is when |d - 1| < FLT_EPSILON and becomes zero when |d| < FLT_EPSILON.
// [CHOSEN, not recovered] x87 extended intermediates are carried in double here.
void AIStrategicObjectManager::resetBridges()
{
    for (std::list<BridgeSegment*>::iterator it = m_bridgeSegments.begin(); it != m_bridgeSegments.end(); ++it)
    {
        BridgeSegment* const segment = *it;
        segment->m_destroyed = false;
        Information* const information =
            informationManager->getInformation(segment->m_object->getHandle(1));
        const Vec3 position = information->getPosition();
        const double radius = information->getPhysical()->getRadius();
        const float bound = static_cast<float>((radius + radius + 5.0f) * (radius + radius + 5.0f));
        bool found = false;
        for (std::list<BridgeSegment*>::iterator other = m_bridgeSegments.begin();
             other != m_bridgeSegments.end(); ++other)
        {
            Information* const neighbour =
                informationManager->getInformation((*other)->m_object->getHandle(1));
            const Vec3& q = neighbour->getPosition();
            const double dx = static_cast<double>(q.x) - position.x;
            const double dy = static_cast<double>(q.y) - position.y;
            const double dz = static_cast<double>(q.z) - position.z;
            Vec3 diff;
            diff.x = static_cast<float>(dx);
            diff.y = static_cast<float>(dy);
            diff.z = static_cast<float>(dz);
            const double distanceSq = dx * dx + dy * dy + dz * dz;
            if (!(distanceSq > 0.01f) || !(bound > distanceSq))
                continue;
            found = true;
            if (!(std::fabs(distanceSq - 1.0f) < 1.1920928955078125e-07f))
            {
                if (!(std::fabs(distanceSq) < 1.1920928955078125e-07f))
                {
                    const double scale = 1.0f / std::sqrt(distanceSq);
                    diff.x = static_cast<float>(diff.x * scale);
                    diff.y = static_cast<float>(diff.y * scale);
                    diff.z = static_cast<float>(diff.z * scale);
                }
                else
                    diff.x = diff.y = diff.z = 0.0f;
            }
            segment->m_position = diff;
            break;
        }
        if (!found)
        {
            const Mat4& transformation = segment->m_object->getTransformation();
            segment->m_position.x = transformation.m[8];
            segment->m_position.y = transformation.m[9];
            segment->m_position.z = transformation.m[10];
        }
    }
}

// The accessors that are two or three instructions. Each is transcribed from its own body; none of
// them needs a type this file does not have.
// `BF2AI::clear` destroys the object through this rather than through the
// static, which is the only reason it exists.
// ELF 0x3448F0.
AIStrategicObjectManager* AIStrategicObjectManager::getInstance()
{
    return m_instance;
}

// - `return (char *)this + 4;`
// ELF 0x344AD0.
const std::vector<AIStrategicObject*>& AIStrategicObjectManager::getStrategicObjects() const
{
    return m_strategicObjects;
}

// - the byte difference shifted right by two, i.e. the element count.
// ELF 0x344AE0.
int AIStrategicObjectManager::getNumberOfStrategicObjects() const
{
    return (int)m_strategicObjects.size();
}

// Returns the category's object vector. The caller must supply a valid category index.
const std::vector<AIStrategicObject*>&
AIStrategicObjectManager::getStrategicObjectsOfCategory(int category) const
{
    return m_objectsByCategory[category];
}

// - same shape, four bytes per `std::string`, again unchecked.
// ELF 0x347910.
const std::string& AIStrategicObjectManager::getCategoryName(int category) const
{
    return m_categoryNames[category];
}

// ELF 0x345610.
int AIStrategicObjectManager::getNumberOfCategories()
{
    return (int)m_categoryNames.size();
}

// - `return (char *)this + 32;`
// ELF 0x3476A0.
const std::map<std::string, AIStrategicArea*>& AIStrategicObjectManager::getStrategicAreas() const
{
    return m_strategicAreas;
}

// Returns null while no area is active - `m_activeArea` starts at `end` and
// the constructor is the only thing that puts it there.
// ELF 0x345630.
AIStrategicArea* AIStrategicObjectManager::getActiveArea()
{
    if (m_activeArea != m_strategicAreas.end())
        return m_activeArea->second;

    return 0;
}

// This one checks both ends, and returns null rather than clamping.
// ELF 0x344910.
AIStrategicObject* AIStrategicObjectManager::getStrategicObject(int index) const
{
    if (index < 0)
        return 0;

    if (index >= (int)m_strategicObjects.size())
        return 0;

    return m_strategicObjects[index];
}

// ELF 0x344940 (added by air/s2 for GuardPosition::update): the named area, null when unknown.
AIStrategicObject* AIStrategicObjectManager::getStrategicObject(const std::string& name) const
{
    const std::map<std::string, AIStrategicArea*>::const_iterator it = m_strategicAreas.find(name);
    if (it != m_strategicAreas.end())
        return it->second;
    return 0;
}

// ELF 0x3449E0 (added by ai-bot for WPMoveTo::getUrgency): the object at `index` when its type
// (vtable +8, getType) is 0 - an area - else null; out-of-range indices give null.
AIStrategicArea* AIStrategicObjectManager::getStrategicArea(int index) const
{
    if (index < 0 || index >= (int)m_strategicObjects.size())
        return 0;
    if (m_strategicObjects[index]->getType() != 0)
        return 0;
    return static_cast<AIStrategicArea*>(m_strategicObjects[index]);
}

// Disables waypoint drawing by clearing the first key component; leaves the second unchanged.
// ELF 0x349390.
void AIStrategicObjectManager::clearWaypointToDraw()
{
    m_wayPointsToDrawFirst = -1;
}

// ELF 0x349040: every waypoint container (+0x58), type 0 then type 1, undraws each waypoint.
void AIStrategicObjectManager::undrawWayPoints() const
{
    for (std::map<std::pair<int, int>, WPContainer>::const_iterator it = m_wayPoints.begin();
         it != m_wayPoints.end(); ++it)
    {
        for (int type = 0; type < 2; ++type)
        {
            const std::vector<SAWayPoint*>& points = it->second.getWayPoints(type);
            for (std::vector<SAWayPoint*>::const_iterator wp = points.begin(); wp != points.end(); ++wp)
                (*wp)->undraw();
        }
    }
}

// Returns the category id for an existing name. The caller must ensure that exact
// name exists; merely having a nonempty category map is insufficient.
// ELF 0x347930.
int AIStrategicObjectManager::getCategoryFromName(const std::string& name) const
{
    return m_categories.find(name)->second;
}

// Adds an object to the category table and assigns its index. The object and category must be valid.
// Other category rows retain null entries at that index.
// ELF 0x344B20.
void AIStrategicObjectManager::addStrategicObject(AIStrategicObject* object)
{
    const int index = (int)m_strategicObjects.size();

    m_strategicObjects.resize(index + 1);
    m_strategicObjects[index] = object;
    object->m_index = index;

    for (int category = 0; category < (int)m_objectsByCategory.size(); ++category)
    {
        m_objectsByCategory[category].resize(index + 1);

        if (category == object->m_category)
            m_objectsByCategory[category][index] = object;
        else
            m_objectsByCategory[category][index] = 0;
    }

    if (AIRenderUtil::m_instance)
        AIRenderUtil::m_instance->updateNumberOfStrategicAreas();
}

// Initializes areas with explicit bounds. Control-point areas wait for postLoadInit().
// ELF 0x345590.
void AIStrategicObjectManager::initAreas()
{
    for (std::map<std::string, AIStrategicArea*>::iterator i = m_strategicAreas.begin();
         i != m_strategicAreas.end(); ++i)
    {
        if (!i->second->m_fromControlPoint)
            i->second->init();
    }
}

// Creates a strategic area from a named control point and selects it.
// Returns false and clears the active selection when creation is refused.
// ELF 0x345C40.
bool AIStrategicObjectManager::createFromControlPoint(const std::string& name,
                                                      unsigned controlPointId,
                                                      float radius,
                                                      const std::string& category)
{
    if (m_strategicAreas.find(name) != m_strategicAreas.end())
    {
        m_activeArea = m_strategicAreas.end();
        return false;
    }

    AIStrategicArea* const area = checkPointer(new AIStrategicArea(name, controlPointId, radius));
    if (!area)
    {
        m_activeArea = m_strategicAreas.end();
        return false;
    }

    if (getNumberOfCategories() <= 0)
        area->m_category = 0;
    else
        area->m_category = getCategoryFromName(category);

    AIStrategicObjectManager::getInstance()->addStrategicObject(area);

    m_strategicAreas[name] = area;
    m_areaNames[area] = name;
    m_activeArea = m_strategicAreas.find(name);
    return true;
}

// ELF 0x344990: bounded by the object count, not by the category row. Unchecked category.
AIStrategicObject* AIStrategicObjectManager::getStrategicObjectOfCategory(int index, int category) const
{
    if (index < 0 || index >= static_cast<int>(m_strategicObjects.size()))
        return 0;
    return m_objectsByCategory[category][index];
}

// ELF 0x344A40: as getStrategicObjectOfCategory, but only an area (getType() == 0).
AIStrategicArea* AIStrategicObjectManager::getStrategicAreaOfCategory(int index, int category) const
{
    if (index < 0 || index >= static_cast<int>(m_strategicObjects.size()))
        return 0;
    if (!m_objectsByCategory[category][index] || m_objectsByCategory[category][index]->getType() != 0)
        return 0;
    return static_cast<AIStrategicArea*>(m_objectsByCategory[category][index]);
}

// ELF 0x344D00: clears the object's category slot (when its category is in range) and erases the
// first matching entry of the object list. Later objects keep their stored indices.
AIStrategicObject* AIStrategicObjectManager::removeStrategicObject(int index)
{
    if (index < 0 || index >= static_cast<int>(m_strategicObjects.size()))
        return 0;

    AIStrategicObject* const object = m_strategicObjects[index];
    const int category = object->m_category;
    if (category >= 0 && category < static_cast<int>(m_objectsByCategory.size()))
        m_objectsByCategory[category][index] = 0;

    for (std::vector<AIStrategicObject*>::iterator it = m_strategicObjects.begin();
         it != m_strategicObjects.end(); ++it)
    {
        if (*it == object)
        {
            m_strategicObjects.erase(it);
            break;
        }
    }
    return object;
}

// ELF 0x345020: getClosestObject's vehicle-group form over one category row (0 for a category
// out of range). The vehicle group is queried before the range test.
AIStrategicObject* AIStrategicObjectManager::getClosestObjectinCategory(const Vec3& position, int side,
                                                                        int category, int equipment,
                                                                        int vehicle) const
{
    AIStrategicObject* best = 0;
    float bestGravity = 0.0f;
    const int group =
        static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getVehicleGroup(equipment);
    if (category < 0 || category >= static_cast<int>(m_objectsByCategory.size()))
        return best;

    const std::vector<AIStrategicObject*>& objects = m_objectsByCategory[category];
    for (std::vector<AIStrategicObject*>::const_iterator it = objects.begin(); it != objects.end(); ++it)
    {
        if (!(*it)->isAllowedVehicleGroup(group) || (*it)->isExpelledUnit(equipment))
            continue;
        if ((*it)->getType() == 0 && vehicle != -1
            && static_cast<AIStrategicArea*>(*it)->isLandingZoneUser(vehicle))
            continue;
        const float gravity = (*it)->gravity(position, side);
        if (gravity > bestGravity)
        {
            bestGravity = gravity;
            best = *it;
        }
    }
    return best;
}

// ELF 0x345650: unchecked active area.
void AIStrategicObjectManager::setFlankPosition(Vec3 first, Vec3 second)
{
    m_activeArea->second->addFlankPosition(first, second);
}

// ELF 0x3456D0: createFromControlPoint's sequence around the bounded area constructor.
bool AIStrategicObjectManager::create(std::string name, Vec2 topLeft, Vec2 centre, float height,
                                      float radius, const std::string& category)
{
    if (m_strategicAreas.find(name) != m_strategicAreas.end())
    {
        m_activeArea = m_strategicAreas.end();
        return false;
    }

    AIStrategicArea* const area =
        checkPointer(new AIStrategicArea(name, topLeft, centre, height, radius));
    if (!area)
    {
        m_activeArea = m_strategicAreas.end();
        return false;
    }

    if (getNumberOfCategories() <= 0)
        area->m_category = 0;
    else
        area->m_category = getCategoryFromName(category);

    AIStrategicObjectManager::getInstance()->addStrategicObject(area);

    m_strategicAreas[name] = area;
    m_areaNames[area] = name;
    m_activeArea = m_strategicAreas.find(name);
    return true;
}

// ELF 0x3461A0: unlinks the active area from its neighbours, drops both name entries, its debug
// rendering and its object slot, then deletes it. AIRenderUtil::m_instance is not null-checked.
bool AIStrategicObjectManager::removeActive()
{
    if (m_activeArea == m_strategicAreas.end())
        return false;
    AIStrategicArea* area = m_activeArea->second;
    if (!area)
        return false;

    for (std::vector<AIStrategicObject*>::iterator it = area->m_neighbours.begin();
         it != area->m_neighbours.end(); ++it)
        static_cast<AIStrategicArea*>(*it)->removeNeighbour(area);

    std::map<AIStrategicArea*, std::string>::iterator name = m_areaNames.find(area);
    if (name != m_areaNames.end())
        m_areaNames.erase(name);
    m_strategicAreas.erase(m_activeArea);
    m_activeArea = m_strategicAreas.end();

    AIRenderUtil::m_instance->unrenderStrategicArea(area);
    AIRenderUtil::m_instance->updateNumberOfStrategicAreas();
    removeStrategicObject(area->m_index);
    delete area;
    return true;
}

// ELF 0x3463B0: the active area (unchecked) gains the named area as a neighbour and the named
// area gains the active one as a neighbouring object.
bool AIStrategicObjectManager::addNeighbour(std::string name)
{
    std::map<std::string, AIStrategicArea*>::iterator it = m_strategicAreas.find(name);
    if (it == m_strategicAreas.end())
        return false;
    if (m_activeArea->second->m_i400 != it->second->m_i400)
        return false;
    m_activeArea->second->addNeighbour(it->second);
    it->second->addNeighbouringObject(m_activeArea->second);
    return true;
}

// ELF 0x346460: both directions, in the image's call order.
bool AIStrategicObjectManager::addTwoWayNeighbour(std::string first, std::string second)
{
    std::map<std::string, AIStrategicArea*>::iterator a = m_strategicAreas.find(first);
    if (a == m_strategicAreas.end())
        return false;
    std::map<std::string, AIStrategicArea*>::iterator b = m_strategicAreas.find(second);
    if (b == m_strategicAreas.end())
        return false;
    if (a->second->m_i400 != b->second->m_i400)
        return false;
    a->second->addNeighbour(b->second);
    b->second->addNeighbouringObject(a->second);
    b->second->addNeighbour(a->second);
    a->second->addNeighbouringObject(b->second);
    return true;
}

// ELF 0x346590.
void AIStrategicObjectManager::removeAllNeighbours()
{
    if (m_activeArea != m_strategicAreas.end())
        m_activeArea->second->removeAllNeighbours();
}

// ELF 0x3465D0: the active area's neighbour names, each followed by a newline ("" without an
// active area). The neighbour list is copied first; the name lookups are unchecked.
std::string AIStrategicObjectManager::getNeighbours()
{
    if (m_activeArea == m_strategicAreas.end())
        return "";

    const std::vector<AIStrategicObject*> neighbours = m_activeArea->second->m_neighbours;
    std::string names;
    for (std::vector<AIStrategicObject*>::const_iterator it = neighbours.begin();
         it != neighbours.end(); ++it)
    {
        names.append(m_areaNames.find(static_cast<AIStrategicArea*>(*it))->second);
        names.append("\n");
    }
    return names;
}

// ELF 0x3467B0.
int AIStrategicObjectManager::addObjectTypeFlag(AIStrategicObject::StrategicObjectType type)
{
    if (m_activeArea == m_strategicAreas.end() || type == 27)
        return -1;
    return m_activeArea->second->addObjectTypeFlag(type);
}

// ELF 0x346800. Shipped defect, preserved: the bit is toggled (xor), not cleared.
int AIStrategicObjectManager::removeObjectTypeFlag(AIStrategicObject::StrategicObjectType type)
{
    if (m_activeArea == m_strategicAreas.end() || type == 27)
        return -1;
    return m_activeArea->second->m_objectTypeFlags ^= 1 << type;
}

// ELF 0x346850.
int AIStrategicObjectManager::setObjectTypeFlags(int flags)
{
    if (m_activeArea == m_strategicAreas.end())
        return -1;
    return m_activeArea->second->setObjectTypeFlags(flags);
}

// ELF 0x3468A0: through the virtual getter (slot 18).
int AIStrategicObjectManager::getObjectTypeFlags()
{
    if (m_activeArea == m_strategicAreas.end())
        return -1;
    return m_activeArea->second->getObjectTypeFlags();
}

// ELF 0x3468D0.
void AIStrategicObjectManager::removeAllObjectTypeFlags()
{
    if (m_activeArea != m_strategicAreas.end())
        m_activeArea->second->m_objectTypeFlags = 0;
}

// ELF 0x346900: "<type> <name>" per line for the types 1..20.
std::string AIStrategicObjectManager::getObjectTypeFlagMapping() const
{
    std::stringstream stream;
    for (unsigned int type = 1; type < 21; ++type)
        stream << type << " " << static_cast<AIStrategicObject::StrategicObjectType>(type) << std::endl;
    return stream.str();
}

// ELF 0x346DB0: the active area's name, "" without one.
std::string AIStrategicObjectManager::getActive() const
{
    if (m_activeArea == m_strategicAreas.end())
        return "";
    return m_activeArea->first;
}

// ELF 0x346E20: every area name, one per line.
std::string AIStrategicObjectManager::list() const
{
    std::stringstream stream;
    for (std::map<std::string, AIStrategicArea*>::const_iterator it = m_strategicAreas.begin();
         it != m_strategicAreas.end(); ++it)
    {
        const std::string name = it->first;
        stream << name << std::endl;
    }
    return stream.str();
}

// ELF 0x3471A0: false without an active area or pathfinding, or for an unknown map name.
bool AIStrategicObjectManager::setOrderPosition(std::string map, const Vec3& position)
{
    if (m_activeArea == m_strategicAreas.end() || !IAIPathfinding::instance)
        return false;
    const int id = IAIPathfinding::instance->getMapIdFromName(map);
    if (id == -1)
        return false;
    return m_activeArea->second->setOrderPos(id, position);
}

// ELF 0x347240: the active area's order position on the named map; zero otherwise.
Vec3 AIStrategicObjectManager::getOrderPosition(std::string map)
{
    Vec3 position;
    position.x = 0.0f;
    position.y = 0.0f;
    position.z = 0.0f;
    if (m_activeArea == m_strategicAreas.end() || !IAIPathfinding::instance)
        return position;
    const int id = IAIPathfinding::instance->getMapIdFromName(map);
    if (id == -1)
        return position;
    return m_activeArea->second->getOrderPos(id, 1);
}

// ELF 0x347300: "<map> <name> x/y/z" per pathfinding map for the active area; "" without an
// active area or pathfinding. The image inlines the x/y/z stream operator.
std::string AIStrategicObjectManager::listOrderPositions()
{
    if (m_activeArea == m_strategicAreas.end() || !IAIPathfinding::instance)
        return "";

    std::ostringstream stream;
    for (unsigned int map = 0; map < static_cast<unsigned int>(IAIPathfinding::instance->getNumberOfMaps());
         ++map)
    {
        const std::string& name = IAIPathfinding::instance->getMapNameFromId(map);
        const Vec3 position = m_activeArea->second->getOrderPos(map, 1);
        stream << map << " " << name << " " << position.x << '/' << position.y << '/' << position.z
               << "\n";
    }
    return stream.str();
}

// ELF 0x3476B0: the area names joined by newlines.
std::string AIStrategicObjectManager::getStrategicAreaNames()
{
    std::string names;
    for (std::map<std::string, AIStrategicArea*>::const_iterator it = m_strategicAreas.begin();
         it != m_strategicAreas.end(); ++it)
        names = names.empty() ? it->first : names + "\n" + it->first;
    return names;
}

// ELF 0x3478D0: the active area's side-1 position (slot 3); unchecked active area.
Vec2 AIStrategicObjectManager::getPosition() const
{
    return m_activeArea->second->getPosition(1);
}

// ELF 0x347980: a new last category, its name index and an object row as long as the object list.
void AIStrategicObjectManager::createNewCategory(const std::string& name)
{
    const int index = static_cast<int>(m_categoryNames.size());
    m_categoryNames.resize(index + 1);
    m_categoryNames[index] = name;
    m_categories.insert(std::make_pair(name, index));
    m_objectsByCategory.resize(index + 1);
    m_objectsByCategory[index].resize(m_strategicObjects.size());
}

// ELF 0x347C80: false for an unknown area name.
bool AIStrategicObjectManager::isInside(const std::string& name, const Vec2& position)
{
    std::map<std::string, AIStrategicArea*>::iterator it = m_strategicAreas.find(name);
    if (it == m_strategicAreas.end())
        return false;
    return it->second->isInside(position);
}

// ELF 0x348020: a waypoint on the named map between two different areas, keyed by their
// (lower, higher) indices; the container is created on first use.
void AIStrategicObjectManager::addWayPoint(const Vec3& position, const std::string& map,
                                           const std::string& first, const std::string& second)
{
    std::map<std::string, AIStrategicArea*>::iterator a = m_strategicAreas.find(first);
    if (a == m_strategicAreas.end())
    {
        Debug(DebugTypeError, "AI/AIDLL/AI/AIStrategicEntity/AIStrategicObjectManager.cpp", 1144, "")
            << first << " is not the name of a valid strategic area.";
        return;
    }
    std::map<std::string, AIStrategicArea*>::iterator b = m_strategicAreas.find(second);
    if (b == m_strategicAreas.end())
    {
        Debug(DebugTypeError, "AI/AIDLL/AI/AIStrategicEntity/AIStrategicObjectManager.cpp", 1151, "")
            << second << " is not the name of a valid strategic area.";
        return;
    }

    AIStrategicArea* low = b->second;
    AIStrategicArea* high = a->second;
    if (a->second->m_index < b->second->m_index)
    {
        low = a->second;
        high = b->second;
    }
    if (low == high)
    {
        Debug(DebugTypeError, "AI/AIDLL/AI/AIStrategicEntity/AIStrategicObjectManager.cpp", 1171, "")
            << "Must have two different strategic areas when adding waypoints.";
        return;
    }

    WPContainer container;
    std::map<std::pair<int, int>, WPContainer>::iterator it =
        m_wayPoints.insert(std::make_pair(std::make_pair(low->m_index, high->m_index), container)).first;
    const int id = IAIPathfinding::instance->getMapIdFromName(map);
    std::vector<SAWayPoint*>& points = it->second.getWayPoints(id);
    points.push_back(checkPointer(new SAWayPoint(position, id)));
}

// ELF 0x348630: deletes the waypoint of the pair's list closest to `position` within a squared
// distance of 10; false when none is that close or a name is unknown. Same-area pairs are not
// refused here, and the container lookup is unchecked.
bool AIStrategicObjectManager::removeWayPoint(const Vec3& position, const std::string& map,
                                              const std::string& first, const std::string& second)
{
    std::map<std::string, AIStrategicArea*>::iterator a = m_strategicAreas.find(first);
    if (a == m_strategicAreas.end())
    {
        Debug(DebugTypeError, "AI/AIDLL/AI/AIStrategicEntity/AIStrategicObjectManager.cpp", 1191, "")
            << first << " is not the name of a valid strategic area.";
        return false;
    }
    std::map<std::string, AIStrategicArea*>::iterator b = m_strategicAreas.find(second);
    if (b == m_strategicAreas.end())
    {
        Debug(DebugTypeError, "AI/AIDLL/AI/AIStrategicEntity/AIStrategicObjectManager.cpp", 1198, "")
            << second << " is not the name of a valid strategic area.";
        return false;
    }

    AIStrategicArea* low = b->second;
    AIStrategicArea* high = a->second;
    if (a->second->m_index < b->second->m_index)
    {
        low = a->second;
        high = b->second;
    }

    const std::pair<int, int> key(low->m_index, high->m_index);
    std::map<std::pair<int, int>, WPContainer>::iterator it = m_wayPoints.find(key);
    const int id = IAIPathfinding::instance->getMapIdFromName(map);
    std::vector<SAWayPoint*>& points = it->second.getWayPoints(id);

    float best = 10.0f;
    std::vector<SAWayPoint*>::iterator closest = points.end();
    for (std::vector<SAWayPoint*>::iterator wp = points.begin(); wp != points.end(); ++wp)
    {
        const Vec3& at = (*wp)->getPosition();
        const float dx = at.x - position.x;
        const float dy = at.y - position.y;
        const float dz = at.z - position.z;
        const float distance = dx * dx + dy * dy + dz * dz;
        if (distance < best)
        {
            closest = wp;
            best = distance;
        }
    }
    if (closest == points.end())
        return false;

    delete *closest;
    points.erase(closest);
    return true;
}

// ELF 0x348A90: deletes every waypoint of the pair's list on the named map and empties it
// (resize(0)). The container lookup is unchecked.
void AIStrategicObjectManager::clearWayPoints(const std::string& map, const std::string& first,
                                              const std::string& second)
{
    std::map<std::string, AIStrategicArea*>::iterator a = m_strategicAreas.find(first);
    if (a == m_strategicAreas.end())
    {
        Debug(DebugTypeError, "AI/AIDLL/AI/AIStrategicEntity/AIStrategicObjectManager.cpp", 1253, "")
            << first << " is not the name of a valid strategic area.";
        return;
    }
    std::map<std::string, AIStrategicArea*>::iterator b = m_strategicAreas.find(second);
    if (b == m_strategicAreas.end())
    {
        Debug(DebugTypeError, "AI/AIDLL/AI/AIStrategicEntity/AIStrategicObjectManager.cpp", 1260, "")
            << second << " is not the name of a valid strategic area.";
        return;
    }

    AIStrategicArea* low = b->second;
    AIStrategicArea* high = a->second;
    if (a->second->m_index < b->second->m_index)
    {
        low = a->second;
        high = b->second;
    }

    const std::pair<int, int> key(low->m_index, high->m_index);
    std::map<std::pair<int, int>, WPContainer>::iterator it = m_wayPoints.find(key);
    const int id = IAIPathfinding::instance->getMapIdFromName(map);
    std::vector<SAWayPoint*>& points = it->second.getWayPoints(id);
    for (std::vector<SAWayPoint*>::iterator wp = points.begin(); wp != points.end(); ++wp)
        delete *wp;
    points.resize(0);
}

// ELF 0x348F00: the selected pair's waypoints (lookup unchecked), or every container's when either
// half of the selection is -1; type 0 then type 1.
void AIStrategicObjectManager::drawWayPoints() const
{
    if (m_wayPointsToDrawFirst != -1 && m_wayPointsToDrawSecond != -1)
    {
        const std::pair<int, int> key(m_wayPointsToDrawFirst, m_wayPointsToDrawSecond);
        const WPContainer& container = m_wayPoints.find(key)->second;
        const std::vector<SAWayPoint*>& infantry = container.getWayPoints(0);
        for (std::vector<SAWayPoint*>::const_iterator wp = infantry.begin(); wp != infantry.end(); ++wp)
            (*wp)->draw();
        const std::vector<SAWayPoint*>& vehicles = container.getWayPoints(1);
        for (std::vector<SAWayPoint*>::const_iterator wp = vehicles.begin(); wp != vehicles.end(); ++wp)
            (*wp)->draw();
        return;
    }

    for (std::map<std::pair<int, int>, WPContainer>::const_iterator it = m_wayPoints.begin();
         it != m_wayPoints.end(); ++it)
    {
        const std::vector<SAWayPoint*>& infantry = it->second.getWayPoints(0);
        for (std::vector<SAWayPoint*>::const_iterator wp = infantry.begin(); wp != infantry.end(); ++wp)
            (*wp)->draw();
        const std::vector<SAWayPoint*>& vehicles = it->second.getWayPoints(1);
        for (std::vector<SAWayPoint*>::const_iterator wp = vehicles.begin(); wp != vehicles.end(); ++wp)
            (*wp)->draw();
    }
}

// ELF 0x349100: selects the pair (lower, higher index) whose waypoints drawWayPoints shows.
void AIStrategicObjectManager::setWayPointsToDraw(const std::string& first, const std::string& second)
{
    std::map<std::string, AIStrategicArea*>::iterator a = m_strategicAreas.find(first);
    if (a == m_strategicAreas.end())
    {
        Debug(DebugTypeError, "AI/AIDLL/AI/AIStrategicEntity/AIStrategicObjectManager.cpp", 1371, "")
            << first << " is not the name of a valid strategic area.";
        return;
    }
    std::map<std::string, AIStrategicArea*>::iterator b = m_strategicAreas.find(second);
    if (b == m_strategicAreas.end())
    {
        Debug(DebugTypeError, "AI/AIDLL/AI/AIStrategicEntity/AIStrategicObjectManager.cpp", 1378, "")
            << second << " is not the name of a valid strategic area.";
        return;
    }

    AIStrategicArea* low = b->second;
    AIStrategicArea* high = a->second;
    if (a->second->m_index < b->second->m_index)
    {
        low = a->second;
        high = b->second;
    }
    m_wayPointsToDrawFirst = low->m_index;
    m_wayPointsToDrawSecond = high->m_index;
}

namespace {

// ELF 0x3493B0: whether the decimal layer is one of the "|"-separated entries. strtok runs on
// the copy's buffer.
bool checkLayerStrList(const std::string& list, int layer)
{
    std::string copy = list;
    const std::string wanted = formatString("%d", layer);
    char* token = std::strtok(const_cast<char*>(copy.c_str()), "|");
    while (token)
    {
        if (std::string(token) == wanted)
            return true;
        token = std::strtok(0, "|");
    }
    return false;
}

} // namespace

// ELF 0x349610. The image inlines the x/y/z stream operator.
void AIStrategicObjectManager::streamWaypoints(std::ostringstream& out,
                                               const std::string& excludedLayers) const
{
    for (std::map<std::pair<int, int>, WPContainer>::const_iterator it = m_wayPoints.begin();
         it != m_wayPoints.end(); ++it)
    {
        AIStrategicArea* const first = getStrategicArea(it->first.first);
        if (!excludedLayers.empty() && checkLayerStrList(excludedLayers, first->m_i400))
            continue;
        AIStrategicArea* const second = getStrategicArea(it->first.second);

        const std::vector<SAWayPoint*>& infantry = it->second.getWayPoints(0);
        for (std::vector<SAWayPoint*>::const_iterator wp = infantry.begin(); wp != infantry.end(); ++wp)
        {
            const Vec3& at = (*wp)->getPosition();
            out << "aiStrategicArea.addWayPoint " << at.x << '/' << at.y << '/' << at.z
                << " Infantry " << first->m_name << " " << second->m_name << std::endl;
        }
        const std::vector<SAWayPoint*>& vehicles = it->second.getWayPoints(1);
        for (std::vector<SAWayPoint*>::const_iterator wp = vehicles.begin(); wp != vehicles.end(); ++wp)
        {
            const Vec3& at = (*wp)->getPosition();
            out << "aiStrategicArea.addWayPoint " << at.x << '/' << at.y << '/' << at.z
                << " Vehicle " << first->m_name << " " << second->m_name << std::endl;
        }
    }
}

// ELF 0x34A150: queries every area's order position on every map and discards them (the copy of
// the area map and the loop survive in the image; the checks they fed are compiled out).
void AIStrategicObjectManager::checkAllOrderPositions() const
{
    IAIPathfinding* const pathfinding = IAIPathfinding::instance;
    const std::map<std::string, AIStrategicArea*> areas = getStrategicAreas();
    for (std::map<std::string, AIStrategicArea*>::const_iterator it = areas.begin(); it != areas.end();
         ++it)
    {
        AIStrategicArea* const area = it->second;
        const int maps = pathfinding->getNumberOfMaps();
        for (int map = 0; map < maps; ++map)
            area->getOrderPos(map, 1);
    }
}

// ELF 0x34A350: walks every container's two lists without effect (the image keeps the empty
// loops; whatever they checked is compiled out).
void AIStrategicObjectManager::checkAllWayPoints() const
{
    for (std::map<std::pair<int, int>, WPContainer>::const_iterator it = m_wayPoints.begin();
         it != m_wayPoints.end(); ++it)
    {
        for (int type = 0; type < 2; ++type)
        {
            const std::vector<SAWayPoint*>& points = it->second.getWayPoints(type);
            for (std::vector<SAWayPoint*>::const_iterator wp = points.begin(); wp != points.end(); ++wp)
            {
            }
        }
    }
}

} // namespace ai
} // namespace hfe
} // namespace dice
