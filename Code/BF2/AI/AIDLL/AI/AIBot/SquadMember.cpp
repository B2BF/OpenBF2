// [INFERRED] Source placement.
// [RETIRE-WHEN: aidll-02-skeleton] Complete and validate the remaining SquadMember operations.

#include "SquadMember.h"
#include <cmath>
#include <iostream>
// static init: ELF 0x1B56E0, 0x1B5740, 0x1B5770: iostream Init only.
#include "../AIStrategicEntity/AIStrategicArea.h"

#include "../Main/AIDllFrontier.h"
#include "../Main/IBotManager.h"
#include "BotMain.h"
#include "SquadCoordinator.h"
#include "SquadFormation.h"
#include "../AIKnowledge/Information.h"
#include "../Main/InformationManager.h"
#include "../AIKnowledge/IPIPhysical.h"
#include "../AIKnowledge/IPIArmament.h"
#include "../AIKnowledge/WeaponInfo.h"
#include "../AIUtils/WeaponInfoManager.h"
#include "SquadManager.h"
#include "WaypointList.h"
#include "../../../AIInterface/BFKnowledge/BFSquad.h"
#include "../../../AIInterface/BFKnowledge/BFTerrain.h"
#include "../Main/CommonSingletonServer.h"
#include "../AIStrategicEntity/AIStrategicObjectManager.h"
#include "../AIStrategic/SAI.h"
#include "../Main/AIMain.h"
#include "CommunicationAgent.h"
#include "WPAltitudeMoveTo.h"
#include "WPMoveTo.h"
#include "../Main/AIRandom.h"
#include "../../../../IO/Input/PlayerInput.h"
#include "../AIKnowledge/IPIMobile.h"
#include "../../../AIInterface/BFKnowledge/IAIObject.h"
#include "../../../AIInterface/BFKnowledge/AIPlayer.h"

namespace dice {
namespace hfe {
namespace ai {

// [INFERRED] local helper names. The update pair compares the x87 sum directly;
// spilling previous+9 to double first would change the boundary at large times.
static bool memberPathUpdateDue(double previous, double now)
{
#if defined(_MSC_VER) && defined(_M_IX86)
    const double interval = 9.0;
    bool due = false;
    __asm {
        fld previous
        fadd interval
        fcomp now
        fnstsw ax
        test ah, 5
        jp notDue
        mov due, 1
    notDue:
    }
    return due;
#else
    return static_cast<long double>(now) > static_cast<long double>(previous) + 9.0L;
#endif
}

// [PLATFORM] PE spills only dx, then sums (dz*dz+dx*dx)+dy*dy. ELF retains
// all three deltas wide and sums (dx*dx+dy*dy)+dz*dz. Compare before narrowing.
// MSVC x86 is the validated target; the portable long-double arm is not an ELF
// runtime or x64-equivalence claim.
static bool memberPathDisplaced(const Vec3& previous, const Vec3& current)
{
#if defined(_MSC_VER) && defined(_M_IX86)
    const Vec3* const oldPosition = &previous;
    const Vec3* const newPosition = &current;
    const float threshold = 0.01f;
    float deltaX;
    bool displaced = false;
    __asm {
        mov eax, oldPosition
        mov edx, newPosition
        fld dword ptr [eax]
        fsub dword ptr [edx]
        fstp deltaX
        fld dword ptr [eax+4]
        fsub dword ptr [edx+4]
        fld dword ptr [eax+8]
        fsub dword ptr [edx+8]
        fmul st(0), st(0)
        fld deltaX
        fmul st(0), st(0)
        faddp st(1), st(0)
        fxch st(1)
        fmul st(0), st(0)
        faddp st(1), st(0)
        fcomp threshold
        fnstsw ax
        test ah, 41h
        jnz notDisplaced
        mov displaced, 1
    notDisplaced:
    }
    return displaced;
#else
#if defined(_WIN32)
    const long double dx = static_cast<float>(static_cast<long double>(previous.x) - current.x);
#else
    const long double dx = static_cast<long double>(previous.x) - current.x;
#endif
    const long double dy = static_cast<long double>(previous.y) - current.y;
    const long double dz = static_cast<long double>(previous.z) - current.z;
#if defined(_WIN32)
    return (dz * dz + dx * dx) + dy * dy > static_cast<long double>(0.01f);
#else
    return (dx * dx + dy * dy) + dz * dz > static_cast<long double>(0.01f);
#endif
#endif
}

// Preserve the stores around the two virtual calls: publishing
// this member to its bot happens before request/position flags are initialized. The Vec3 members
// are cleared before the body and again later. There is no null player/manager guard.
SquadMember::SquadMember(SquadCoordinator* squad, IAIPlayer* player,
                         int squadIndex, int side, int memberIndex)
    : m_position{0.0f, 0.0f, 0.0f}
    , m_vector54{0.0f, 0.0f, 0.0f}
    , m_vector60{0.0f, 0.0f, 0.0f}
{
    m_player = player;
    m_playerId = m_player->getId();
    m_squadIndex = squadIndex;
    m_isSquadLeader = false;
    m_memberIndex = memberIndex;
    m_squad = squad;
    m_maxSpeed = 0.0f;
    m_side = side;
    m_estimatedArrival = 1.0f;

    Bot* const bot = IBotManager::instance->getBotFromPlayerId(m_playerId);
    if (bot)
    {
        m_bot = bot;
        m_bot->setSquadMember(this);
    }
    else
    {
        m_bot = 0;
    }

    m_flag30 = false;
    m_havePath = false;
    m_request = new RequestState();
    m_request->m_action = 0;
    m_assistMember = 0;
    m_position = {0.0f, 0.0f, 0.0f};
    m_positionValid = false;
    m_flag49 = false;
    m_flag4A = false;
    m_flag4B = false;
    m_formationDistanceSq = 900.0f;
    m_moved = false;
    m_time24 = 0.0;
    m_vector54 = {0.0f, 0.0f, 0.0f};
    m_vector60 = {0.0f, 0.0f, 0.0f};
}

// only these two stores. Retail does not free m_request or
// clear the bot's back-pointer here. Preserve both omissions rather than inventing ownership.
SquadMember::~SquadMember()
{
    m_player = 0;
    m_bot = 0;
}

IAIPlayer* SquadMember::getPlayer() { return m_player; }

// return the stored squad pointer.
// Non-const in the mangled signature; no lookup, ownership transfer or null guard.
SquadCoordinator* SquadMember::getSquad()
{
    return m_squad;
}

int SquadMember::getPlayerId() { return m_playerId; }
bool SquadMember::isBot() { return m_bot != 0; }
// stored pointer, no lookup or guard.
Bot* SquadMember::getBot()
{
    return m_bot;
}

int SquadMember::getMemberIndex() { return m_memberIndex; }
void SquadMember::setIsSquadLeader(bool leader) { m_isSquadLeader = leader; }
bool SquadMember::isSquadLeader() { return m_isSquadLeader; }

// the second flag receives the inverse, not a constant clear.
void SquadMember::setHavePath(bool havePath)
{
    m_havePath = havePath;
    m_flag4A = !havePath;
}

// [INFERRED] void return: coordinator discards the result and exits have no common
// return materialization. Each missing dependency stops at its real call site;
// paths which retail skips continue through mission selection and movement.
void SquadMember::update(double time)
{
    updateMemberPos();
    SquadFormation* const formation = m_squad->getSquadFormation();

    if (memberPathUpdateDue(m_time24, time))
    {
        Vec3 position = {};
        if (memberPathDisplaced(m_vector54, m_vector60))
        {
            setHavePath(false);
            m_vector54 = m_vector60;
        }
        else if (m_bot && getPosition(position))
        {
            // ELF 0x1B1ED0: WaypointList::getUrgency (vtable +8) at this position, no null
            // guard. Below 0.5 the path is dropped; so it is when an unmoved member is more than
            // 4 m from its path goal (+0x60).
            WaypointList* const waypoints = m_bot->getWaypoints();
            if (waypoints->getUrgency(position, m_bot) < 0.5f)
            {
                setHavePath(false);
            }
            else if (!m_moved)
            {
                const float dx = m_vector60.x - position.x;
                const float dy = m_vector60.y - position.y;
                const float dz = m_vector60.z - position.z;
                if (dx * dx + dy * dy + dz * dz > 16.0f)
                    setHavePath(false);
            }
        }
        m_time24 = time;
    }

    if (m_bot && m_player && m_player->getIsAlive())
    {
        Vec3 position = {};
        if (getPosition(position))
        {
            double sinceArtillery = 0.0;
            int artillerySide = 0;
            if (isMobile()
                && SquadManager::m_instance->isInArtilleryDangerZone(position, m_side + 1, sinceArtillery, artillerySide))
            {
                // A member that runs from artillery does nothing else this tick.
                if (avoidArtillery(position, sinceArtillery, artillerySide))
                    return;
            }
            else
            {
                m_flag4B = false;
            }
        }
    }

    if (m_bot && !m_bot->isFiring())
    {
        // [PLATFORM] PE reads the manager after getCurrentEquipment; ELF captures
        // it first. Both reload the bot after isFiring and preserve pool semantics.
#if defined(_WIN32)
        const unsigned int handle = m_bot->getCurrentEquipment();
        Information* const information = informationManager->getInformation(handle);
#else
        InformationManager* const manager = informationManager;
        const unsigned int handle = m_bot->getCurrentEquipment();
        Information* const information = manager->getInformation(handle);
#endif
        IPIArmament* const armament = information ? information->getArmament() : 0;
        if (information && information->isOfType(ITSoldier) && armament
            && armament->getActiveWeapon() != -1)
        {
            // ELF 0x1B1ED0: a soldier who is not firing switches to weapon slot 2 (activation
            // input, inlined Input::setInput(float) = 1) unless he should keep the active one:
            // a medic (kit 5) with weapon type 1 at no more than 95 % health, an engineer
            // (kit 3) with type 2 while his other ammunition is at most 90 % (product of
            // current/max over the remaining weapons), or a non-type-1/2 weapon while firing.
            WeaponInfo* const active =
                WeaponInfoManager::m_instance->getInformation(armament->getWeapon(armament->getActiveWeapon()));
            if (active)
            {
                IPIPhysical* const physical = information->getPhysical();
                bool keep = false;
                if (m_player->getSpawnKit() == 5 && active->getWeaponType() == 1
                    && !(physical->getHealth() > 0.95f))
                {
                    keep = true;
                }
                else
                {
                    if (m_player->getSpawnKit() == 3 && active->getWeaponType() == 2)
                    {
                        const std::vector<unsigned int> weapons = armament->getWeapons();
                        float ratio = 1.0f;
                        for (unsigned int i = 0; i < weapons.size(); ++i)
                        {
                            WeaponInfo* const weapon = WeaponInfoManager::m_instance->getInformation(weapons[i]);
                            if (weapon && weapon->getWeaponType() != 1 && weapon->getWeaponType() != 2
                                && weapon->getAmmo() != -1 && weapon->getMaxAmmo() != 0)
                            {
                                ratio = (static_cast<float>(weapon->getCurrentAmmo())
                                         / static_cast<float>(weapon->getMaxAmmo())) * ratio;
                            }
                        }
                        keep = ratio <= 0.9f;
                    }
                    if (!keep && active->getWeaponType() != 1 && active->getWeaponType() != 2
                        && m_bot->isFiring())
                        keep = true;
                }
                WeaponInfo* const primary = WeaponInfoManager::m_instance->getInformation(armament->getWeapon(2));
                if (primary && !keep && active != primary)
                    m_bot->m_playerInput->setInput(
                        static_cast<io::PlayerInputMap>(primary->getWeaponActivate()), 1.0f);
            }
        }
    }

    if (m_squad->getMissionStatus() == 1)
    {
        SquadMember* member = m_squad->getSquadLeader();
        if (!member || !member->getPlayer()->getIsAlive() || member->getPlayer()->isManDown())
        {
            member = this;
            if (!m_player->getIsAlive() || m_player->isManDown())
                member = nullptr;
        }
        if (member)
        {
            // ELF 0x1B20AB: the selected member goes to acceptMission, then this member's player
            // accepts (IBFSquad +0x2C) or declines (+0x30) the squad order.
            IBFSquad* const engine = static_cast<IBFSquad*>(CommonSingletonServer::m_iBFSquad);
            if (m_squad->acceptMission(member))
                engine->acceptSquadOrder(m_squadIndex + 1, m_player);
            else
                engine->declineSquadOrder(m_squadIndex + 1, m_player);
        }
    }

    // Formation remains the entry capture; member index is fresh after callbacks.
    const bool inFormation = formation->isInFormation(m_memberIndex);
    m_assistMember = 0;
    if (inFormation)
        moveWithFormation();
    else
        moveWithoutFormation();
}

// No result
// is materialized. Capture the formation and allowance once, but query membership twice
// and reload the member index after every callback. A join/leave clears only havePath:
// setHavePath(false) would incorrectly change m_flag4A as well.
void SquadMember::updateFormationStatus()
{
    SquadFormation* const formation = m_squad->getSquadFormation();
    const bool allowed = isAllowedInFormation();
    if (formation->isInFormation(m_memberIndex) && !allowed)
    {
        formation->leaveFormation(m_memberIndex);
        m_havePath = false;
    }
    if (!formation->isInFormation(m_memberIndex) && allowed)
    {
        formation->joinFormation(m_memberIndex);
        m_havePath = false;
    }
}

// Copy all coordinates before
// returning the stored validity flag; false does not preserve the caller's old output.
bool SquadMember::getPosition(Vec3& position)
{
    position = m_position;
    return m_positionValid;
}

// one byte load, ELF +0x50 / PE +0x54.
bool SquadMember::isMoving()
{
    return m_moved;
}

// ELF 0x1B5380 (432 bytes). A live, not man-down member whose object is a soldier
// (type flag 0x200000) with unit and mobile information; on a mission to an assigned area it
// leaves the formation once within m_formationDistanceSq of the valid area position.
bool SquadMember::isAllowedInFormation()
{
    bool allowed = false;
    if (!m_player->getIsAlive() || m_player->isManDown())
        return false;
    IAIObject* const vehicle = m_player->getCurrentVehicle();
    if (!vehicle)
        return false;
    Information* const info = informationManager->getInformation(vehicle->getInformation(m_side + 1));
    if (info && info->getUnit() && info->isOfType(ITSoldier) && info->getMobile())
    {
        allowed = true;
        Vec3 goal = m_squad->getSquadGotoPos();
        Vec3 position = { 0.0f, 0.0f, 0.0f };
        if (m_squad->getMissionStatus() == 2 && m_squad->isSquadAssignedToAnyArea())
        {
            m_squad->getValidSaPos(this, goal);
            if (getPosition(position))
            {
                const float dz = goal.z - position.z;
                const float dx = goal.x - position.x;
                const float dy = goal.y - position.y;
                if (dz * dz + dx * dx + dy * dy < m_formationDistanceSq)
                    allowed = false;
            }
        }
    }
    return allowed;
}

// ELF 0x1B1A70 (371 bytes). The position is the primary object's (with physical information)
// for the member's side; it counts as moved beyond 0.1 m.
void SquadMember::updateMemberPos()
{
    IAIObject* const vehicle = m_player->getCurrentVehicle();
    m_positionValid = false;
    m_moved = false;
    if (!vehicle)
        return;
    Information* const info = informationManager->getInformation(vehicle->getInformation(m_side + 1));
    Information* const primary = informationManager->getInformation(info->getPrimaryObject(m_side + 1));
    if (primary && primary->getUnit())
    {
        const float oldY = m_position.y;
        const float oldX = m_position.x;
        const float oldZ = m_position.z;
        m_position = primary->getPosition();
        m_positionValid = true;
        const float dz = oldZ - m_position.z;
        const float dx = oldX - m_position.x;
        const float dy = oldY - m_position.y;
        if (dz * dz + dx * dx + dy * dy > 0.01f)
            m_moved = true;
    }
}

// ELF 0x1B5560 (188 bytes): a live member in an object with unit and mobile information.
bool SquadMember::isMobile()
{
    IAIObject* const vehicle = m_player->getCurrentVehicle();
    if (!vehicle)
        return false;
    if (!m_player->getIsAlive())
        return false;
    if (m_player->isManDown())
        return false;
    Information* const info = informationManager->getInformation(vehicle->getInformation(m_side + 1));
    return info && info->getUnit() && info->getMobile();
}

// ELF 0x1B3AC0 (864 bytes). The member's slot position (SquadFormation::getSlotWorldPos); with it,
// the maximum speed and the estimated arrival (squared distance / speed, as in the image) are
// refreshed from the vehicle's mobile information (no null checks). A bot member: within
// sqrt(10) of the slot it counts as arrived (+0x30; arriving clears the path flag). The slot-0
// member leads: on a mission once the formation moves out, at the formation's speed, towards the
// goal (snapped to the area order position) unless it has a path or is within sqrt(8); otherwise
// only with a player commander and no path. Everyone else moves to the slot while +0x49 / +0x4A
// ask for it (+0x4A is consumed).
void SquadMember::moveWithFormation()
{
    SquadFormation* const formation = m_squad->getSquadFormation();
    Vec3 position = {};
    getPosition(position);
    Vec3 slotPosition = {};
    if (!formation->getSlotWorldPos(m_memberIndex, slotPosition))
        return;
    const float dx = slotPosition.x - position.x;
    const float dy = slotPosition.y - position.y;
    const float dz = slotPosition.z - position.z;
    const float distanceSq = dy * dy + dx * dx + dz * dz;
    Information* const info =
        informationManager->getInformation(m_player->getCurrentVehicle()->getInformation(m_side + 1));
    const float maxSpeed = info->getMobile()->getMaxSpeed();
    m_maxSpeed = maxSpeed;
    m_estimatedArrival = static_cast<float>(static_cast<double>(distanceSq) / maxSpeed);
    if (!m_bot)
        return;
    if (distanceSq <= 10.0f)
    {
        if (!m_flag30)
            m_havePath = false;
        m_flag30 = true;
    }
    else
    {
        m_flag30 = false;
    }
    if (formation->getSquadMemberIndexAtSlot(0) == m_memberIndex)
    {
        if (m_squad->getMissionStatus() == 2 && formation->shouldSquadMoveOut())
        {
            m_maxSpeed = formation->getFormationMaxSpeed();
            Vec3 goal = m_squad->getSquadGotoPos();
            if (m_squad->isSquadAssignedToAnyArea())
                m_squad->getValidSaPos(this, goal);
            if (m_havePath)
                return;
            const float gx = goal.x - position.x;
            const float gy = goal.y - position.y;
            const float gz = goal.z - position.z;
            if (gz * gz + gx * gx + gy * gy <= 8.0f)
                return;
        }
        else
        {
            if (m_havePath)
                return;
            if (!IAIMain::m_instance->getSAI(m_side + 1)->isPlayerCommander())
                return;
        }
        moveToGoalPos(info);
    }
    else if (m_flag49 || m_flag4A)
    {
        m_flag4A = false;
        moveToSlotPos(slotPosition, info);
    }
}

// ELF 0x1B3FF0 (592 bytes). A path to the slot: a one-goal WPMoveTo(0, 60, 2) on the ground, or a
// WPAltitudeMoveTo for aircraft as in moveToGoalPos - except that here the image samples the
// terrain at (x, y) of the slot, not (x, z).
void SquadMember::moveToSlotPos(const Vec3& position, Information* information)
{
    m_havePath = true;
    m_vector60 = position;
    if (!information->isOfType(ITAir))
    {
        WPMoveTo::GoalList goals;
        goals.push_back(std::make_pair(position, -1));
        m_bot->setWaypoints(new WPMoveTo(0.0f, 60.0f, 2, goals));
        return;
    }
    float altitude = 70.0f;
    float radius = 30.0f;
    if (information->isOfType(ITHelicopter))
    {
        radius = 5.0f;
        altitude = 20.0f;
    }
    std::list<Vec3> positions;
    Vec3 raised = position;
    raised.y = static_cast<IAITerrain*>(CommonSingletonServer::m_iaiTerrain)->getTerrainAltitude(position.x, position.y)
             + altitude;
    positions.push_back(raised);
    m_bot->setWaypoints(new WPAltitudeMoveTo(radius, radius, altitude, 2, positions));
}

// ELF 0x1B3E20 (464 bytes). A live bot member refreshes its maximum speed from its vehicle's
// mobile information (no null check on the information) and moves to the goal while it has no
// path: on a mission (status 2) when farther than sqrt(8) from the squad's goto position;
// otherwise only while its side's SAI has a player commander.
void SquadMember::moveWithoutFormation()
{
    if (!m_bot)
        return;
    if (!m_player->getIsAlive())
        return;

    Vec3 position = {};
    getPosition(position);
    const Vec3 goal = m_squad->getSquadGotoPos();
    Information* const info =
        informationManager->getInformation(m_player->getCurrentVehicle()->getInformation(m_side + 1));
    if (info->getMobile())
        m_maxSpeed = info->getMobile()->getMaxSpeed();
    if (m_squad->getMissionStatus() == 2)
    {
        if (m_havePath)
            return;
        const float dz = goal.z - position.z;
        const float dx = goal.x - position.x;
        const float dy = goal.y - position.y;
        if (8.0f < dz * dz + dx * dx + dy * dy)
        {
            moveToGoalPos(info);
            return;
        }
    }
    else if (m_havePath)
    {
        return;
    }
    if (!IAIMain::m_instance->getSAI(m_side + 1)->isPlayerCommander())
        return;
    moveToGoalPos(info);
}

namespace {

// The acknowledgement after a move order: a bot member tells the squad (radio message 6 when
// it moves, 7 when the position is unreachable; channel 3, priority 3, delay 2 s) once per
// order, as the coordinator's roger-that flag allows.
void acknowledgePosition(SquadMember* member, SquadCoordinator* squad, int message)
{
    if (!squad->getRogerThatPositionMessage() || !member->isBot())
        return;
    member->getBot()->getCommunicationAgent()->enqueueRadioMessage(
        static_cast<AIRadioMessageType>(message), static_cast<AIRadioChannelType>(3), 3, 2.0f, 0);
    squad->setRogerThatPositionMessage(false);
}

} // namespace

// ELF 0x1B4240 (3802 bytes). Only for a mobile member of a squad on a mission (status 2).
//   * Ground units (no type flag 0x10) follow the squad leader while the formation says so and the
//     squad is not close to a non-owned target: to the leader's goto position, unless it lies in
//     the assigned area; without an area directly, otherwise snapped to the order position of the
//     strategic area containing it. A reachable goal becomes a one-goal WPMoveTo(0, 60, 2);
//     an unreachable one is acknowledged with message 7 and falls through to the squad goal.
//   * The squad goal: the goto position, snapped to the assigned area's order position. Aircraft
//     (flag 0x10; helicopters 0x20 too) get a WPAltitudeMoveTo at terrain height + 70 (20) with
//     radius 30 (5). Otherwise an unreachable goal is acknowledged with 7; a reachable one without
//     an area becomes a WPMoveTo (set only when it differs from the current waypoints); with an
//     area, the strategic route from the bot's current object to the area: no route gives a
//     WPMoveTo as before (no acknowledgement), a route is handed to orderNormalBot, dropping its
//     first object when the bot is closer to the goal than that object is.
// The ordered goal is kept at +0x60 (for a route: the area's height marker m_f148 as y).
void SquadMember::moveToGoalPos(Information* information)
{
    if (!information->getMobile())
        return;
    if (m_squad->getMissionStatus() != 2)
        return;
    Vec3 goal = m_squad->getSquadGotoPos();
    AIStrategicArea* area = m_squad->getSquadStrategicAreaAssignment();
    const int side = m_side + 1;

    if (!information->isOfType(ITAir)
        && m_squad->getSquadFormation()->shouldLeaveSquadLeader()
        && !m_squad->isSquadCloseToNonOwnedTarget())
    {
        goal = m_squad->getSquadLeaderGotoPos();
        bool follow = false;
        if (!area)
        {
            follow = true;
        }
        else if (!area->isInside(goal))
        {
            AIStrategicArea* const closest = static_cast<AIStrategicArea*>(
                AIStrategicObjectManager::getInstance()->getClosestObject(goal, side));
            if (closest && closest->isInside(goal))
                m_squad->getValidSaPos(this, closest, goal);
            follow = true;
        }
        if (follow)
        {
            if (m_squad->isValidPosition(this, goal))
            {
                m_havePath = true;
                WPMoveTo::GoalList goals;
                goals.push_back(std::make_pair(goal, -1));
                m_bot->setWaypoints(new WPMoveTo(0.0f, 60.0f, 2, goals));
                m_vector60 = goal;
                acknowledgePosition(this, m_squad, 6);
                return;
            }
            acknowledgePosition(this, m_squad, 7);
        }
    }

    goal = m_squad->getSquadGotoPos();
    if (m_squad->isSquadAssignedToAnyArea())
        m_squad->getValidSaPos(this, goal);
    if (information->isOfType(ITAir))
    {
        float altitude = 70.0f;
        float radius = 30.0f;
        if (information->isOfType(ITHelicopter))
        {
            radius = 5.0f;
            altitude = 20.0f;
        }
        std::list<Vec3> positions;
        Vec3 raised = goal;
        raised.y = static_cast<IAITerrain*>(CommonSingletonServer::m_iaiTerrain)->getTerrainAltitude(goal.x, goal.z)
                 + altitude;
        positions.push_back(raised);
        m_bot->setWaypoints(new WPAltitudeMoveTo(radius, radius, altitude, 2, positions));
        m_vector60 = goal;
        acknowledgePosition(this, m_squad, 6);
        return;
    }
    if (!m_squad->isValidPosition(this, goal))
    {
        acknowledgePosition(this, m_squad, 7);
        return;
    }
    m_havePath = true;
    WPMoveTo::GoalList goals;
    if (!m_squad->isSquadAssignedToAnyArea())
    {
        goals.push_back(std::make_pair(goal, -1));
        WPMoveTo* const waypoints = new WPMoveTo(0.0f, 60.0f, 2, goals);
        if (!isSameWaypointList(m_bot->getWaypoints(), waypoints))
        {
            m_bot->setWaypoints(waypoints);
            m_vector60 = goal;
            acknowledgePosition(this, m_squad, 6);
        }
        else
        {
            delete waypoints;
        }
        return;
    }

    std::list<AIStrategicObject*> route;
    const int botId = static_cast<int>(m_bot->getBotId());
    AIStrategicArea* const from = static_cast<AIStrategicArea*>(AIStrategicObjectManager::getInstance()->getStrategicObject(
        IAIMain::m_instance->getSAI(side)->getBotCurrentPos(botId)));
    if (!m_squad->getSquadStrategicAreaAssignment()->getRouteList(side, from, route))
    {
        goals.push_back(std::make_pair(goal, -1));
        WPMoveTo* const waypoints = new WPMoveTo(0.0f, 60.0f, 2, goals);
        if (!isSameWaypointList(m_bot->getWaypoints(), waypoints))
        {
            m_bot->setWaypoints(waypoints);
            m_vector60 = goal;
        }
        else
        {
            delete waypoints;
        }
        return;
    }
    if (1 < route.size())
    {
        Vec3 position = {};
        getPosition(position);
        const float dx = goal.x - position.x;
        const float dz = goal.z - position.z;
        const Vec2& first = route.front()->getPosition(side);
        const float fx = goal.x - first.x;
        const float fz = goal.z - first.y;
        if (dx * dx + dz * dz < fz * fz + fx * fx)
            route.erase(route.begin());
    }
    goal.y = m_squad->getSquadStrategicAreaAssignment()->m_f148;
    m_vector60 = goal;
    ISAI* const sai = IAIMain::m_instance->getSAI(side);
    m_squad->getSquadStrategicAreaAssignment()->orderNormalBot(side, route, static_cast<int>(m_bot->getBotId()), sai);
    acknowledgePosition(this, m_squad, 6);
}

// ELF 0x1B5120 (539 bytes). Only a current WPMoveTo (type word +0xC == 0) can match: both goal
// lists must have the same length and each pair of goals lie within 10 m (squared <= 100).
bool SquadMember::isSameWaypointList(WaypointList* current, WaypointList* candidate)
{
    if (static_cast<WPMoveTo*>(current)->m_int0C != 0)
        return false;
    std::list<Vec3> a;
    std::list<Vec3> b;
    static_cast<WPMoveTo*>(current)->getCoords(a);
    static_cast<WPMoveTo*>(candidate)->getCoords(b);
    if (a.empty() || b.empty())
        return false;
    std::list<Vec3>::const_iterator i = a.begin();
    std::list<Vec3>::const_iterator j = b.begin();
    while (i != a.end())
    {
        const float dx = j->x - i->x;
        const float dy = j->y - i->y;
        const float dz = j->z - i->z;
        if (dx * dx + dy * dy + dz * dz > 100.0f)
            return false;
        ++i;
        ++j;
        if (i == a.end())
            return j == b.end();
        if (j == b.end())
            return false;
    }
    return false;
}

// ELF 0x1B3A10: IPIPhysical::getRadius of the member's object information; no null guards.
float SquadMember::getVehicleRadius()
{
    IAIObject* const vehicle = m_player->getCurrentVehicle();
    Information* const info = informationManager->getInformation(vehicle->getInformation(m_side + 1));
    return info->getPhysical()->getRadius();
}

// ELF 0x1B1930: the per-round state - the position flags (+0x48..+0x4B: invalid, 49/4A set,
// 4B clear), the path reconsideration time (+0x24) and the two Vec3s at +0x54/+0x60 zeroed.
void SquadMember::reset()
{
    m_positionValid = false;
    m_flag49 = true;
    m_flag4A = true;
    m_flag4B = false;
    m_time24 = 0.0;
    m_vector54.x = m_vector54.y = m_vector54.z = 0.0f;
    m_vector60.x = m_vector60.y = m_vector60.z = 0.0f;
}

void SquadMember::setMemberIndex(int index)
{
    m_memberIndex = index;
}

// ELF 0x1B1BF0: only with a mission, an area assignment whose status for this member's side is
// non-zero and a valid position: true when the squad's goto position lies inside the formation
// distance (squared distance below +0x4C).
bool SquadMember::isCloseToNonOwnedTarget() const
{
    if (!m_squad->squadHasMission())
        return false;
    if (!m_squad->isSquadAssignedToAnyArea())
        return false;
    if (!m_squad->getSquadStrategicAreaAssignment()->m_status[m_side])
        return false;
    if (!m_positionValid)
        return false;
    const Vec3& target = m_squad->getSquadGotoPos();
    const double dx = static_cast<double>(target.x) - m_position.x;
    const double dy = static_cast<double>(target.y) - m_position.y;
    const double dz = static_cast<double>(target.z) - m_position.z;
    return m_formationDistanceSq > dx * dx + dy * dy + dz * dz;
}

// ELF 0x1B1D10: status 1 uses the second threshold; other nonzero statuses use the first.
bool SquadMember::isCloseToNonOwnedTarget(float otherDistanceSq, float statusOneDistanceSq) const
{
    if (!m_squad->squadHasMission())
        return false;
    if (!m_squad->isSquadAssignedToAnyArea())
        return false;
    const int status = m_squad->getSquadStrategicAreaAssignment()->m_status[m_side];
    if (!status)
        return false;
    const float distanceSq = status == 1 ? statusOneDistanceSq : otherDistanceSq;
    if (!m_positionValid)
        return false;
    Vec2 target = {0.0f, 0.0f};
    Vec2 position = {0.0f, 0.0f};
    vec3ToVec2XZ(m_squad->getSquadGotoPos(), target);
    vec3ToVec2XZ(m_position, position);
    const float dx = position.x - target.x;
    const float dy = position.y - target.y;
    return distanceSq > dx * dx + dy * dy;
}

// ELF 0x1B2840 (1609 bytes). Already avoiding: true. Otherwise up to five candidates at the
// safe distance + 5 m from the artillery target, starting away from it through `position` and
// swinging round (the angle goes 0 -> +x, -(a) - x, 3a + x, ..., x in [0.6, 0.8)); a valid one, or
// else the side's remembered flee position, becomes a one-goal WPMoveTo (0, 60, flags 2) and the
// member avoids artillery (+0x31, +0x4B). A found candidate is remembered for the side.
bool SquadMember::avoidArtillery(Vec3 position, double /*sinceStart*/, int side)
{
    if (m_flag4B)
        return true;
    SquadManager* const manager = SquadManager::m_instance;
    const Vec3 target = manager->getArtilleryTargetPos(side);
    Vec3 away;
    away.x = position.x - target.x;
    away.y = position.y - target.y;
    away.z = position.z - target.z;
    const float safe = manager->getArtillerySafeDistance();
    const float lengthSq = away.z * away.z + away.x * away.x + away.y * away.y;
    if (std::fabs(lengthSq - 1.0f) >= 1.1920929e-07f)
    {
        if (std::fabs(lengthSq) >= 1.1920929e-07f)
        {
            const float inverse = 1.0f / std::sqrt(lengthSq);
            away.x = away.x * inverse;
            away.y = away.y * inverse;
            away.z = inverse * away.z;
        }
        else
        {
            away.x = 0.0f;
            away.y = 0.0f;
            away.z = 0.0f;
        }
    }
    const float radius = safe + 5.0f;
    Vec3 direction = away;
    Vec3 candidate;
    candidate.x = 0.0f;
    candidate.y = 0.0f;
    candidate.z = 0.0f;
    float angle = 0.0f;
    bool found = false;
    for (int tries = 0; tries <= 4;)
    {
        candidate.x = radius * direction.x + target.x;
        candidate.y = radius * direction.y + target.y;
        candidate.z = radius * direction.z + target.z;
        if (m_squad->isValidPosition(this, candidate))
        {
            found = true;
            break;
        }
        ++tries;
        if (0.0f < angle)
            angle = (angle - (angle + angle)) - (AIRandom::m_instance->nextFloat() * 0.2f + 0.6f);
        else
            angle = (angle + angle + angle) + (AIRandom::m_instance->nextFloat() * 0.2f + 0.6f);
        rotateXZ(away, direction, angle);
    }
    if (!found && !manager->haveValidArtilleryFleePos(this, candidate))
        return false;
    m_havePath = true;
    m_flag4B = true;
    std::list<std::pair<Vec3, int> > goals;
    goals.push_back(std::make_pair(candidate, -1));
    m_bot->setWaypoints(new WPMoveTo(0.0f, 60.0f, 2, goals));
    if (found)
        manager->setValidArtilleryFleePos(this, candidate);
    return true;
}

// ELF 0x1B1E60: a killed member leaves its formation slot.
void SquadMember::squadMemberKilled()
{
    SquadFormation* const formation = m_squad->getSquadFormation();
    if (formation->isInFormation(m_memberIndex))
        formation->leaveFormation(m_memberIndex);
}

// ELF 0x1B1990: frees the request record (operator delete); the pointer is left as is.
void SquadMember::removeRequest()
{
    ::operator delete(m_request);
}

// ELF 0x1B3810: the member stands on its squad's assigned area's control point: it has a
// position, the area exists and its status for the member's side (+0x70 + 4 * (side + 1)) is
// non-zero, the area has a control point, the player is alive (IAIPlayer +0x24) and not man-down
// (+0x2C), and the squared distance is below radius^2 + 5 (0x49CDAC).
bool SquadMember::isTakingFlag()
{
    bool taking = false;
    AIStrategicArea* const area = m_squad->getSquadStrategicAreaAssignment();
    Vec3 position = {};
    if (getPosition(position) && area && static_cast<int>(area->getStatus(m_side + 1)) != 0)
    {
        Vec3 point = {};
        float radius;
        if (area->getControlPointPosAndRadius(point, radius))
        {
            radius = radius * radius;
            if (m_player->getIsAlive() && !m_player->isManDown())
            {
                const float dx = position.x - point.x;
                const float dy = position.y - point.y;
                const float dz = position.z - point.z;
                if (dx * dx + dy * dy + dz * dz < radius + 5.0f)
                    taking = true;
            }
        }
    }
    return taking;
}

// ELF 0x1B1E50.
void SquadMember::setForcePathUpdate(bool force) { m_flag4A = force; }

// ELF 0x1B2EA0: the assisted member's equipment supplies the movement dimensions.
void SquadMember::moveToAssistSquadMember(SquadMember* member, const Vec3& position)
{
    if (m_havePath)
        return;
    IAIObject* const vehicle = member->getPlayer()->getCurrentVehicle();
    InformationManager* const manager = informationManager;
    Information* const information = manager->getInformation(vehicle->getInformation(m_side + 1));
    moveToSlotPos(position, information);
}

// ELF 0x1B2F40: ten attempts; failure returns the zero vector.
Vec3 SquadMember::getRandomAssistPos(SquadMember* member, const Vec3& position, const Vec3& direction)
{
    Vec3 result = {};
    const Vec2 normal = {direction.z, -direction.x};
    const float radius = member->getVehicleRadius();
    const float diameter = radius + radius;
    const float selfRadius = getVehicleRadius();
    const float range = selfRadius + selfRadius + diameter;
    Vec3 selfPosition = {};
    if (!getPosition(selfPosition))
        return result;
    for (int attempt = 0; attempt < 10; ++attempt)
    {
        float angle = AIRandom::m_instance->nextFloat() * 0.785398006f + 0.436329991f;
        Vec2 toward = {selfPosition.x - position.x, selfPosition.z - position.z};
        const float squared = toward.x * toward.x + toward.y * toward.y;
        if (!(std::fabs(squared - 1.0f) < 1.1920928955078125e-7f))
        {
            if (!(std::fabs(squared) < 1.4210854715202004e-14f))
            {
                const float inverse = 1.0f / std::sqrt(squared);
                toward.x *= inverse;
                toward.y *= inverse;
            }
            else
            {
                toward.x = 0.0f;
                toward.y = 0.0f;
            }
        }
        if (toward.x * normal.x + toward.y * normal.y > 0.0f)
            angle = -angle;
        Vec3 rotated = {};
        rotateXZ(direction, rotated, angle);
        const Vec3 base = {radius * rotated.x * 4.0f + position.x,
                           radius * rotated.y * 4.0f + position.y,
                           radius * rotated.z * 4.0f + position.z};
        const float random = AIRandom::m_instance->nextFloat();
        const Vec3 candidate = {rotated.x * random * range + base.x,
                                rotated.y * random * range + base.y,
                                random * rotated.z * range + base.z};
        if (m_squad->isValidPosition(this, candidate))
            return candidate;
    }
    return result;
}

// ELF 0x1B3450: only a different shooter within 75 metres can be assisted.
SquadMember* SquadMember::getAssistFiringSquadMember(Vec3& position)
{
    Vec3 memberPosition = {};
    if (!m_squad->anyFiring() || m_squad->isFiring(m_playerId))
        return 0;
    float distanceSquared;
    SquadMember* const member = m_squad->getClosestFiringMember(this, distanceSquared);
    if (!member || !(distanceSquared < 5625.0f))
        return 0;
    if (m_assistMember != member)
        m_havePath = false;
    m_assistMember = member;
    if (member->getPosition(memberPosition))
    {
        const Vec3 direction = m_squad->getFiringDir(member);
        position = getRandomAssistPos(member, memberPosition, direction);
        if (!(position.x * position.x + position.y * position.y + position.z * position.z < 0.001f))
            return member;
    }
    m_assistMember = 0;
    return 0;
}

// ELF 0x1B35B0: missing data normally permits damage; man-down and insufficient strength refuse it.
bool SquadMember::canDamageTarget(unsigned int target)
{
    if (!m_bot || target == 0xFFFFFFFFu)
        return true;
    if (m_bot->getManDown())
        return false;
    Information* const targetInformation = informationManager->getInformation(target);
    if (!targetInformation)
        return true;
    Vec3 position = {};
    IPIPhysical* const targetPhysical = targetInformation->getPhysical();
    if (!targetPhysical || !getPosition(position))
        return true;
    InformationManager* const manager = informationManager;
    Information* const equipment = manager->getInformation(m_bot->getCurrentEquipment());
    if (!equipment)
        return true;
    IPIArmament* const armament = equipment->getArmament();
    if (!armament)
        return true;
    const std::vector<unsigned int>& weapons = armament->getWeapons();
    const Vec3& targetPosition = targetInformation->getPosition();
    const float dx = m_position.x - targetPosition.x;
    const float dy = m_position.y - targetPosition.y;
    const float dz = m_position.z - targetPosition.z;
    (void)std::sqrt(dx * dx + dy * dy + dz * dz);
    float strength = 0.0f;
    if (armament->isAntiAircraft() || !targetInformation->isOfType(ITAir))
    {
        for (unsigned int i = 0; i < weapons.size(); ++i)
        {
            WeaponInfo* const weapon = WeaponInfoManager::m_instance->getInformation(weapons[i]);
            if (weapon && weapon->getWeaponType() == 0)
            {
                const float candidate = static_cast<float>(weapon->getStrength(targetPhysical->getStrengthType()));
                if (candidate > strength)
                    strength = candidate;
            }
        }
    }
    return strength > 0.1f;
}

// ELF 0x1B39A0: action is written before the handle is re-read from the request.
void SquadMember::getCurrentRequestAction(RequestAction& action, unsigned int& handle)
{
    action = static_cast<RequestAction>(m_request->m_action);
    handle = m_request->m_handle;
}

} // namespace ai
} // namespace hfe
} // namespace dice
