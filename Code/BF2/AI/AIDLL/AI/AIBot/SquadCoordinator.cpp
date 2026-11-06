// [INFERRED] Source placement.
// [RETIRE-WHEN: aidll-02-skeleton] Complete and validate the remaining SquadCoordinator operations.

#include "SquadCoordinator.h"

// <iostream>'s ios_base::Init object is this TU's static initialisation and teardown (ELF
// 0x1A8DE0, __tcf_0 0x1A8E40, _GLOBAL__I 0x1A8E70).
#include <iostream>

#include <cmath>
#include <cfloat>
#include <cstdint>

#include "SquadFormation.h"
#include "SquadMember.h"
#include "BotMain.h"
#include "BotSenses.h"
#include "SensingAgent.h"
#include "CommunicationAgent.h"
#include "../AIKnowledge/Information.h"
#include "../Main/InformationManager.h"
#include "../Main/AIDllFrontier.h"
#include "../Main/AIMain.h"
#include "SquadManager.h"
#include "../../../AIInterface/BFKnowledge/BFTerrain.h"
#include "../Main/AISettingsInterfaces.h"
#include "../Main/AIRandom.h"
#include "../Main/IAIPathfinding.h"
#include "../AIStrategic/SAI.h"
#include "../AIStrategicEntity/AIStrategicArea.h"
#include "../AIStrategicEntity/AIStrategicObjectManager.h"
#include "../AIKnowledge/IPIUnit.h"
#include "../AIKnowledge/IPIPhysical.h"
#include "../Main/IBotManager.h"
#include "../AIKnowledge/IPIMobile.h"
#include "../../../AIInterface/BFKnowledge/IAIObject.h"
#include "../Main/CommonSingletonServer.h"
#include "../../../AIInterface/BFKnowledge/BFTimer.h"      // IBFTimer::getGameTime
#include "../../../AIInterface/BFKnowledge/BFSquad.h"
#include "../../../AIInterface/BFKnowledge/AIPlayer.h"
#include "../../../AIInterface/BFKnowledge/BFObjectEnvironment.h"
#include "../../../../Math/Vec3.h"                            // rotateXZ, vec3ToVec2XZ

namespace dice {
namespace hfe {
namespace ai {

// [INFERRED] local helper name. PE's Vec3 subtraction helper spills all three
// deltas to float, then its length-squared helper keeps (dx*dx+dy*dy)+dz*dz wide
// through the ordered comparison. ELF keeps the subtractions wide and adds
// (dy*dy+dx*dx)+dz*dz. Only the MSVC x86 branch is runtime-validated here.
static bool maneuverMemberWithinRange(const Vec3& position, const Vec3& squadPosition)
{
#if defined(_MSC_VER) && defined(_M_IX86)
    const Vec3* const member = &position;
    const Vec3* const squad = &squadPosition;
    const float threshold = 8100.0f;
    float deltaX, deltaY, deltaZ;
    bool within = false;
    __asm {
        mov eax, squad
        mov edx, member
        fld dword ptr [eax]
        fsub dword ptr [edx]
        fstp deltaX
        fld dword ptr [eax+4]
        fsub dword ptr [edx+4]
        fstp deltaY
        fld dword ptr [eax+8]
        fsub dword ptr [edx+8]
        fstp deltaZ
        fld deltaX
        fmul st(0), st(0)
        fld deltaY
        fmul st(0), st(0)
        faddp st(1), st(0)
        fld deltaZ
        fmul st(0), st(0)
        faddp st(1), st(0)
        fcomp threshold
        fnstsw ax
        test ah, 5
        jp notWithin
        mov within, 1
    notWithin:
    }
    return within;
#else
#if defined(_WIN32)
    const long double dx = static_cast<float>(static_cast<long double>(squadPosition.x) - position.x);
    const long double dy = static_cast<float>(static_cast<long double>(squadPosition.y) - position.y);
    const long double dz = static_cast<float>(static_cast<long double>(squadPosition.z) - position.z);
#else
    const long double dx = static_cast<long double>(squadPosition.x) - position.x;
    const long double dy = static_cast<long double>(squadPosition.y) - position.y;
    const long double dz = static_cast<long double>(squadPosition.z) - position.z;
#endif
#if defined(_WIN32)
    return (dx * dx + dy * dy) + dz * dz < 8100.0L;
#else
    return (dy * dy + dx * dx) + dz * dz < 8100.0L;
#endif
#endif
}

// [INFERRED] local helper name. A shooter's squared distance from where it first fired, ordered
// strictly above 25 (five metres). PE spills only dx to float and sums (dz*dz+dy*dy)+dx*dx;
// ELF keeps all three differences wide and sums (dy*dy+dx*dx)+dz*dz. Equality/NaN keep the record.
static bool shooterMovedAway(const Vec3& firePosition, const Vec3& position)
{
#if defined(_MSC_VER) && defined(_M_IX86)
    const Vec3* const fire = &firePosition;
    const Vec3* const member = &position;
    const float threshold = 25.0f;
    float deltaX;
    bool moved = false;
    __asm {
        mov eax, fire
        mov edx, member
        fld dword ptr [eax]
        fsub dword ptr [edx]
        fstp deltaX
        fld dword ptr [eax+4]
        fsub dword ptr [edx+4]
        fld dword ptr [eax+8]
        fsub dword ptr [edx+8]
        fmul st(0), st(0)
        fxch st(1)
        fmul st(0), st(0)
        faddp st(1), st(0)
        fld deltaX
        fmul st(0), st(0)
        faddp st(1), st(0)
        fcomp threshold
        fnstsw ax
        test ah, 41h
        jnz notMoved
        mov moved, 1
    notMoved:
    }
    return moved;
#else
#if defined(_WIN32)
    const long double dx = static_cast<float>(static_cast<long double>(firePosition.x) - position.x);
#else
    const long double dx = static_cast<long double>(firePosition.x) - position.x;
#endif
    const long double dy = static_cast<long double>(firePosition.y) - position.y;
    const long double dz = static_cast<long double>(firePosition.z) - position.z;
#if defined(_WIN32)
    return (dz * dz + dy * dy) + dx * dx > 25.0L;
#else
    return (dy * dy + dx * dx) + dz * dz > 25.0L;
#endif
#endif
}

// [INFERRED] local helper name. The leader's squared distance to the goto position, narrowed to
// float. PE subtracts through its float-returning Vec3 operator (position - goto); ELF keeps
// goto - position wide. Both sum (dx*dx+dy*dy)+dz*dz.
static float leaderGotoDistanceSq(const Vec3& position, const Vec3& gotoPosition)
{
#if defined(_MSC_VER) && defined(_M_IX86)
    const Vec3* const p = &position;
    const Vec3* const g = &gotoPosition;
    float deltaX, deltaY, deltaZ, distance;
    __asm {
        mov eax, p
        mov edx, g
        fld dword ptr [eax]
        fsub dword ptr [edx]
        fstp deltaX
        fld dword ptr [eax+4]
        fsub dword ptr [edx+4]
        fstp deltaY
        fld dword ptr [eax+8]
        fsub dword ptr [edx+8]
        fstp deltaZ
        fld deltaX
        fmul st(0), st(0)
        fld deltaY
        fmul st(0), st(0)
        faddp st(1), st(0)
        fld deltaZ
        fmul st(0), st(0)
        faddp st(1), st(0)
        fstp distance
    }
    return distance;
#else
#if defined(_WIN32)
    const long double dx = static_cast<float>(static_cast<long double>(position.x) - gotoPosition.x);
    const long double dy = static_cast<float>(static_cast<long double>(position.y) - gotoPosition.y);
    const long double dz = static_cast<float>(static_cast<long double>(position.z) - gotoPosition.z);
#else
    const long double dx = static_cast<long double>(gotoPosition.x) - position.x;
    const long double dy = static_cast<long double>(gotoPosition.y) - position.y;
    const long double dz = static_cast<long double>(gotoPosition.z) - position.z;
#endif
    return static_cast<float>((dx * dx + dy * dy) + dz * dz);
#endif
}

// [INFERRED] local helper name. Squared member distance ordered strictly below the (float)
// squared radius. PE: control point - position through its float Vec3 operator, then
// (dx*dx+dy*dy)+dz*dz; ELF: position - control point wide, then (dy*dy+dx*dx)+dz*dz.
static bool insideControlPointRadius(const Vec3& controlPoint, const Vec3& position, float radiusSq)
{
#if defined(_MSC_VER) && defined(_M_IX86)
    const Vec3* const c = &controlPoint;
    const Vec3* const p = &position;
    float deltaX, deltaY, deltaZ;
    bool inside = false;
    __asm {
        mov eax, c
        mov edx, p
        fld dword ptr [eax]
        fsub dword ptr [edx]
        fstp deltaX
        fld dword ptr [eax+4]
        fsub dword ptr [edx+4]
        fstp deltaY
        fld dword ptr [eax+8]
        fsub dword ptr [edx+8]
        fstp deltaZ
        fld deltaX
        fmul st(0), st(0)
        fld deltaY
        fmul st(0), st(0)
        faddp st(1), st(0)
        fld deltaZ
        fmul st(0), st(0)
        faddp st(1), st(0)
        fcomp radiusSq
        fnstsw ax
        test ah, 5
        jp outside
        mov inside, 1
    outside:
    }
    return inside;
#else
#if defined(_WIN32)
    const long double dx = static_cast<float>(static_cast<long double>(controlPoint.x) - position.x);
    const long double dy = static_cast<float>(static_cast<long double>(controlPoint.y) - position.y);
    const long double dz = static_cast<float>(static_cast<long double>(controlPoint.z) - position.z);
    return (dx * dx + dy * dy) + dz * dz < static_cast<long double>(radiusSq);
#else
    const long double dx = static_cast<long double>(position.x) - controlPoint.x;
    const long double dy = static_cast<long double>(position.y) - controlPoint.y;
    const long double dz = static_cast<long double>(position.z) - controlPoint.z;
    return (dy * dy + dx * dx) + dz * dz < static_cast<long double>(radiusSq);
#endif
#endif
}

// [INFERRED]
// void return: the manager discards it and exits do not materialize a common result.
// Timestamp and mission-2 assignment checks precede even the empty-member return.
void SquadCoordinator::update(double time)
{
    m_d124 = time;
    if (m_missionStatus == 2 && !IAIMain::m_instance->getSAI(m_side + 1)->isPlayerCommander())
    {
        if (m_i40 == 0)
        {
            if (m_strategicAreaAssignment &&
                m_strategicAreaAssignment->getStatus(m_side + 1) != 0)
            {
                if (!m_strategicAreaAssignment->isAreaTakeable())
                {
                    // Both the main singleton and side are fresh after the area callback;
                    // the second side read is later still, after getSAI has returned.
                    SAI* const sai = IAIMain::m_instance->getSAI(m_side + 1);
                    if (!sai->ownAllTakeableCps(m_side + 1))
                        unAssignSquad();
                }
            }
            else if (m_strategicAreaAssignment &&
                     m_strategicAreaAssignment->getStatus(m_side + 1) == 0)
            {
                unAssignSquad();
            }
        }
        else if (m_i40 == 1 && m_strategicAreaAssignment &&
                 m_strategicAreaAssignment->getStatus(m_side + 1) == 0)
        {
            // Opposite one-based side is captured before getSAI; it is not reloaded
            // after that callback, unlike the ownAllTakeableCps argument above.
            const int otherSide = (m_side == 0) + 1;
            if (IAIMain::m_instance->getSAI(m_side + 1)->haveAnyOwnedAndTakeableCps(otherSide))
                unAssignSquad();
        }
    }

    if (m_squadMembers.empty())
        return;

    // Both images reload the member vector after each call, without a null-member guard.
    for (unsigned int index = 0; index < m_squadMembers.size(); ++index)
        m_squadMembers[index]->updateFormationStatus();
    m_formation->updateClosestSlots();

    // The second live loop forwards the original double, not the mutable stored time.
    // It is skipped if the preceding callbacks emptied the vector; maneuver still follows.
    for (unsigned int index = 0; index < m_squadMembers.size(); ++index)
        m_squadMembers[index]->update(time);

    updateSquadManeuver();

    m_formation->update(time);
    updateSquadPosition();
    updateShotsFired();
    // Each timed tail receives the original double argument, not the stored m_d124.
    updateRepairRequest(time);
    updateNeedOrdersMessage(time);
    updateIncomingMessages();
}

// [INFERRED] Updates the squad position from its living anchor, otherwise its first
// living member. Retains the old position when nobody is alive. Anchor/leader indices
// must be valid; lookups are unchecked and position-validity results are ignored.
void SquadCoordinator::updateSquadPosition()
{
    Vec3 position = {};
    const int anchorIndex = m_formation->getSquadMemberIndexAtSlot(0);
    SquadMember* member = anchorIndex == -1 ? getSquadLeader() : m_squadMembers[anchorIndex];
    if (!member->getPlayer()->getIsAlive())
    {
        member = 0;
        for (unsigned int index = 0; index < m_squadMembers.size(); ++index)
        {
            if (m_squadMembers[index]->getPlayer()->getIsAlive())
            {
                member = m_squadMembers[index];
                break;
            }
        }
    }
    if (member)
        member->getPosition(position);
    else
        position = m_v108;
    m_v108 = position;
}

// [INFERRED] Removes fire records with invalid shooter positions. Expired records or
// shooters more than five metres away also clear member paths and the movement flag.
// Reuses the position local, deletes records before nodes, and re-reads the list end.
void SquadCoordinator::updateShotsFired()
{
    Vec3 position = {};
    for (std::list<SquadMemberFire*>::iterator it = m_list12.begin(); it != m_list12.end(); )
    {
        SquadMemberFire* const fire = *it;
        if (!fire->m_member->getPosition(position))
        {
            delete *it;
            m_list12.erase(it++);
        }
        else if (m_d124 > fire->m_time || shooterMovedAway(fire->m_position, position))
        {
            delete *it;
            m_list12.erase(it++);
            // Not updateSquadPaths: this inline walk has no null-member test.
            for (unsigned int index = 0; index < m_squadMembers.size(); ++index)
                m_squadMembers[index]->setHavePath(false);
            m_formation->updateMovementFlag(true);
        }
        else
            ++it;
    }
}

// Strict ordered expiry clears the request and
// its confirmation together; the position and deadline are left as they were.
void SquadCoordinator::updateRepairRequest(double time)
{
    if (m_b172 && time > m_d188)
    {
        m_b172 = false;
        m_b196 = false;
    }
}

// Requests orders for an idle bot squad leader after twenty seconds without orders.
// [RETIRE-WHEN: aidll-02-skeleton] Complete and validate the corresponding SquadCoordinator operation.
void SquadCoordinator::updateNeedOrdersMessage(double time)
{
    if (IAIMain::m_instance->getSAI(m_side + 1)->isPlayerCommander() && !isSquadTakingFlag())
    {
        const int orderState = m_i40;
        if (orderState == 0 || orderState == 2 || orderState == 4)
        {
            SquadMember* const leader = getSquadLeader();
            float distanceSq = 1000.0f;
            Vec3 position = {};
            if (leader)
            {
                if (leader->getPosition(position))
                    distanceSq = leaderGotoDistanceSq(position, m_v28);
                if (leader->isBot() && !leader->isMoving() &&
                    leader->getPlayer()->getIsAlive() && !leader->getPlayer()->isManDown() &&
                    (distanceSq < 225.0f ||
                     (m_strategicAreaAssignment &&
                      m_strategicAreaAssignment->getStatus(m_side + 1) == 0)))
                {
                    // The one path that keeps the running timer.
                    if (!(time > m_needOrderMessageTimer + 20.0))
                        return;
                    static_cast<IBFSquad*>(CommonSingletonServer::m_iBFSquad)
                        ->requestNewCommanderOrder(m_squadIndex + 1, leader->getPlayer());
                    // ELF 0x1A3CB6: radio message 14 on channel 4, priority 3, delay 1 s.
                    leader->getBot()->getCommunicationAgent()->enqueueRadioMessage(
                        static_cast<AIRadioMessageType>(14), static_cast<AIRadioChannelType>(4), 3, 1.0f, 0);
                }
            }
        }
    }
    resetNeedOrderMessageTimer(time);
}

// Routes a human leader's call to one bot when the formation has left the leader.
// [RETIRE-WHEN: aidll-02-skeleton] Complete and validate the corresponding SquadCoordinator operation.
void SquadCoordinator::updateIncomingMessages()
{
    SquadMember* const leader = getSquadLeader();
    if (!leader || leader->isBot())
        return;
    if (!m_formation->shouldLeaveSquadLeader())
        return;

    bool found = false;
    SquadMember* member = 0;
    for (unsigned int index = 0; index < m_squadMembers.size(); ++index)
    {
        member = m_squadMembers[index];
        if (member->isBot() && member->getPlayer()->getIsAlive())
        {
            found = true;
            break;
        }
    }
    if (!found)
        return;

    CommunicationAgent* const radio = member->getBot()->getCommunicationAgent();
    const std::list<AIRadioMessageInfo>& messages =
        radio->getReceivedMessages(static_cast<AIRadioMessageType>(12));
    if (messages.empty())
        return;
    if (messages.front().m_senderPlayerId != leader->getPlayerId())
        return;

    m_formation->joinSquadLeader();
    // ELF 0x1A3760: the bot answers with radio message 6 (channel 3, priority 3, delay 2 s), and
    // every member's path is invalidated.
    radio->enqueueRadioMessage(static_cast<AIRadioMessageType>(6), static_cast<AIRadioChannelType>(3), 3, 2.0f, 0);
    updateSquadPaths();
}

// Returns whether a living, standing squad member is inside the assigned control point
// for an area this side does not own. Requires a valid member position and a successful
// control-point query; squares the radius in float.
bool SquadCoordinator::isSquadTakingFlag()
{
    bool taking = false;
    if (m_strategicAreaAssignment && m_strategicAreaAssignment->getStatus(m_side + 1) != 0)
    {
        Vec3 controlPoint = {};
        Vec3 position = {};
        float radius;
        if (m_strategicAreaAssignment->getControlPointPosAndRadius(controlPoint, radius))
        {
            radius = radius * radius;
            for (unsigned int index = 0; index < m_squadMembers.size(); ++index)
            {
                SquadMember* const member = m_squadMembers[index];
                if (member->getPlayer()->getIsAlive() && !member->getPlayer()->isManDown() &&
                    member->getPosition(position) &&
                    insideControlPointRadius(controlPoint, position, radius))
                {
                    taking = true;
                    break;
                }
            }
        }
    }
    return taking;
}

// [INFERRED] void result: discarded by update, no common return materialization.
void SquadCoordinator::updateSquadManeuver()
{
    m_i148 = chooseSquadManeuver();
    switch (m_i148)
    {
    case 0:
        for (unsigned int index = 0; index < m_squadMembers.size(); ++index)
        {
            SquadMember* const member = m_squadMembers[index];
            if (member->isBot())
                member->getBot()->setAllowFire(true);
        }
        break;
    case 1:
        squadManeuverAlternateFire();
        break;
    case 2:
        squadManeuverStaticFlank();
        break;
    }
}

// Return carries the full integer selector; the source enum name remains unknown.
int SquadCoordinator::chooseSquadManeuver()
{
    int firingMembers = 0;
    Vec3 position = {};
    const unsigned int count = static_cast<unsigned int>(m_squadMembers.size());
    for (unsigned int index = 0; index < count; ++index)
    {
        SquadMember* const member = m_squadMembers[index];
        if (member->isBot() && member->getPlayer()->getIsAlive())
        {
            Bot* const bot = member->getBot();
            // [PLATFORM] ELF captures the manager before the equipment callback.
            // PE loads it afterward, and only when the low handle word is nonzero.
#if defined(_WIN32)
            const unsigned int handle = bot->getCurrentEquipment();
            Information* const information = (handle & 0xffffu)
                ? informationManager->getInformation(handle) : nullptr;
#else
            InformationManager* const manager = informationManager;
            const unsigned int handle = bot->getCurrentEquipment();
            Information* const information = manager->getInformation(handle);
#endif
            if (bot->isFiring())
            {
                // No information-null guard exists in either image.
                if (!information->isOfType(ITAir) && !information->isOfType(ITHelicopter) &&
                    !information->isOfType(ITNaval) &&
                    member->getPosition(position) &&
                    maneuverMemberWithinRange(position, m_v108))
                    ++firingMembers;
            }
        }
    }
    if (firingMembers > 1)
        return 1;
    if (m_missionStatus != 2)
        return 0;

    // ELF 0x1A5741: the mission target (+28) at terrain height (IAITerrain +0x14), 50..173 m from the
    // squad, a commander mission (+56) on an area this side holds (status 1): flank. A new flank
    // (previous maneuver != 2) clears its two flags.
    const float x = m_v28.x;
    const float z = m_v28.z;
    const float height = static_cast<const IAITerrain*>(CommonSingletonServer::m_iaiTerrain)->getTerrainAltitude(x, z);
    const float dx = x - m_v108.x;
    const float dy = height - m_v108.y;
    const float dz = z - m_v108.z;
    const float distanceSq = dx * dx + dy * dy + dz * dz;
    if (distanceSq < 30000.0f && 2500.0f < distanceSq && m_b56 && m_strategicAreaAssignment
        && static_cast<int>(m_strategicAreaAssignment->getStatus(m_side + 1)) == 1)
    {
        if (m_i148 != 2)
        {
            m_b156 = false;
            m_b157 = false;
        }
        return 2;
    }
    return 0;
}

// ELF 0x1A4E90. Every 4 s (+124 the update time, +140 the next time): all bots may fire; the
// living bots firing on foot from a mobile primary within 90 m of the squad take turns in two
// halves (phase +152: 0 the first half holds fire 1.5 s, 1 a 1-3 s pause, 2 the second half
// 1.5 s, 3 a pause). A held bot only stops when it faces (dot > 0.85) its target more than 40 m away.
void SquadCoordinator::squadManeuverAlternateFire()
{
    if (!(m_d140 < m_d124))
        return;
    m_d140 = m_d124 + 4.0f;
    std::vector<SquadMember*> firing;
    Vec3 position;
    position.x = 0.0f;
    position.y = 0.0f;
    position.z = 0.0f;
    for (unsigned int i = 0; i < m_squadMembers.size(); ++i)
    {
        if (m_squadMembers[i]->isBot())
            m_squadMembers[i]->getBot()->setAllowFire(true);
    }
    for (unsigned int i = 0; i < m_squadMembers.size(); ++i)
    {
        SquadMember* const member = m_squadMembers[i];
        if (!member->isBot() || !member->getPlayer()->getIsAlive())
            continue;
        Bot* const bot = member->getBot();
        Information* const equipment = informationManager->getInformation(bot->getCurrentEquipment());
        if (!bot->isFiring() || !equipment->isPrimary() || !equipment->hasMobile())
            continue;
        if (equipment->isOfType(ITAir) || equipment->isOfType(ITHelicopter) || equipment->isOfType(ITNaval) ||
            !member->getPosition(position))
            continue;
        const float dx = m_v108.x - position.x;
        const float dy = m_v108.y - position.y;
        const float dz = m_v108.z - position.z;
        if (dx * dx + dy * dy + dz * dz < 8100.0f)
            firing.push_back(member);
    }
    const unsigned int count = static_cast<unsigned int>(firing.size());
    if (count < 2)
        return;
    unsigned int start = 0;
    unsigned int end = 0;
    switch (m_i152)
    {
    case 0:
        end = count >> 1;
        m_i152 = 1;
        m_d140 = 1.5f + m_d124;
        break;
    case 1:
        m_d140 = (1.0f + m_d124) + static_cast<double>(AIRandom::m_instance->next()) * 2.3283064e-10f * 2.0;
        ++m_i152;
        break;
    case 2:
        end = count;
        start = count >> 1;
        m_i152 = 3;
        m_d140 = 1.5f + m_d124;
        break;
    case 3:
        m_d140 = (1.0f + m_d124) + static_cast<double>(AIRandom::m_instance->next()) * 2.3283064e-10f * 2.0;
        m_i152 = 0;
        break;
    default:
        break;
    }
    for (unsigned int i = start; i < end; ++i)
    {
        SquadMember* const member = firing[i];
        Information* const target = informationManager->getInformation(member->getBot()->getFiringTarget());
        if (!target || !member->getPosition(position))
            continue;
        const Vec3& t = target->getPosition();
        Vec3 direction;
        direction.x = t.x - position.x;
        direction.y = t.y - position.y;
        direction.z = t.z - position.z;
        const float lengthSq = direction.x * direction.x + direction.y * direction.y + direction.z * direction.z;
        if (std::fabs(lengthSq - 1.0f) >= 1.1920929e-07f)
        {
            if (std::fabs(lengthSq) >= 1.1920929e-07f)
            {
                const float inverse = 1.0f / std::sqrt(lengthSq);
                direction.x = direction.x * inverse;
                direction.y = direction.y * inverse;
                direction.z = inverse * direction.z;
            }
            else
            {
                direction.x = 0.0f;
                direction.y = 0.0f;
                direction.z = 0.0f;
            }
        }
        const Vec3 facing =
            informationManager->getInformation(member->getBot()->getCurrentEquipment())->getFaceing();
        if (0.85f < facing.z * direction.z + facing.x * direction.x + facing.y * direction.y)
        {
            const Vec3& again = target->getPosition();
            const float ex = again.x - position.x;
            const float ey = again.y - position.y;
            const float ez = again.z - position.z;
            if (1600.0f < ex * ex + ey * ey + ez * ez)
                firing[i]->getBot()->setAllowFire(false);
        }
    }
}

// ELF 0x1A4060. All bots may fire. Once per mission (+157) on an area mission (+120 == 2) with a
// formed squad and an area, at 65 %: the flanking pair closest to the squad (into +160) when both
// are valid for the leader (slot 0) marks the flank (+156) and re-paths the leader.
void SquadCoordinator::squadManeuverStaticFlank()
{
    for (unsigned int i = 0; i < m_squadMembers.size(); ++i)
    {
        if (m_squadMembers[i]->isBot())
            m_squadMembers[i]->getBot()->setAllowFire(true);
    }
    if (m_b157 || m_missionStatus != 2)
        return;
    if (!m_formation->hasSquadMembers() || !m_strategicAreaAssignment)
        return;
    m_b157 = true;
    if (!(AIRandom::m_instance->nextFloat() < 0.65f))
        return;
    SquadMember* const leader = m_squadMembers[m_formation->getSquadMemberIndexAtSlot(0)];
    if (m_strategicAreaAssignment->getClosestFlankingPositions(m_v108, m_positions[0], m_positions[1])
        && isValidPosition(leader, m_positions[0]) && isValidPosition(leader, m_positions[1]))
    {
        m_b156 = true;
        leader->setHavePath(false);
    }
}

// [INFERRED] void return, as above.
// Null entries are skipped; a live unsigned index walk invalidates each real member.
void SquadCoordinator::updateSquadPaths()
{
    for (std::size_t index = 0; index < m_squadMembers.size(); ++index)
    {
        SquadMember* const member = m_squadMembers[index];
        if (member)
            member->setHavePath(false);
    }
}

// Borrowed formation pointer, unchanged.
SquadFormation* SquadCoordinator::getSquadFormation()
{
    return m_formation;
}

// Return the address of the stored
// destination. [INFERRED] const-reference spelling; callers read the borrowed Vec3.
const Vec3& SquadCoordinator::getSquadGotoPos()
{
    return m_v28;
}

// Full-width field read; the original
// return enum spelling is unresolved, so int records the observed payload without new labels.
int SquadCoordinator::getMissionStatus()
{
    return m_missionStatus;
}

// The assignment is borrowed and nullable.
bool SquadCoordinator::isSquadAssignedToArea(int areaIndex)
{
    return m_strategicAreaAssignment && m_strategicAreaAssignment->getIndex() == areaIndex;
}

// ELF 0x1A7FF0 (277 bytes): the sum of the members' unit strategic strengths of the type
// (IPIUnit +0xC), rounded by +0.5 and truncation.
int SquadCoordinator::getStrategicStrength(StrategicStrengthType type)
{
    float total = 0.0f;
    for (std::size_t index = 0; index < m_squadMembers.size(); ++index)
    {
        IAIPlayer* const player = m_squadMembers[index]->getPlayer();
        if (!player->getIsAlive() || player->isManDown())
            continue;
        IAIObject* const vehicle = player->getCurrentVehicle();
        if (!vehicle)
            continue;
        Information* const info = informationManager->getInformation(vehicle->getInformation(m_side + 1));
        if (info && info->getUnit())
            total = total + info->getUnit()->getStrategicStrengths()[type];
    }
    return static_cast<int>(total + 0.5f);
}

// ELF 0x1A7BC0. The order position of the strategic object closest to the squad position
// (+28), for the member's vehicle number, when the member has mobile unit information.
bool SquadCoordinator::getValidSaPos(SquadMember* member, Vec3& position)
{
    AIStrategicObject* const object =
        AIStrategicObjectManager::getInstance()->getClosestObject(m_v28, m_side + 1);
    bool valid = false;
    if (member && object)
    {
        IAIObject* const vehicle = member->getPlayer()->getCurrentVehicle();
        if (vehicle)
        {
            Information* const info = informationManager->getInformation(vehicle->getInformation(m_side + 1));
            if (info && info->getUnit() && info->getMobile())
            {
                position = object->getOrderPos(info->getMobile()->getVehicleNumber(), m_side + 1);
                valid = true;
            }
        }
    }
    return valid;
}

// ELF 0x1A7D30 (320 bytes). As the overload above, but the order position of the given area.
bool SquadCoordinator::getValidSaPos(SquadMember* member, AIStrategicArea* area, Vec3& position)
{
    bool valid = false;
    if (member && area)
    {
        IAIObject* const vehicle = member->getPlayer()->getCurrentVehicle();
        if (vehicle)
        {
            Information* const info = informationManager->getInformation(vehicle->getInformation(m_side + 1));
            if (info && info->getUnit() && info->getMobile())
            {
                position = area->getOrderPos(info->getMobile()->getVehicleNumber(), m_side + 1);
                valid = true;
            }
        }
    }
    return valid;
}

// ELF 0x1A6B60 (856 bytes). Whether `position` lies on the member's navigation mesh: the
// polygon under it (IAIPathfinding +0x44 getPolygon(position, vehicle number, polygon, island,
// equipment type)) must be valid for the vehicle (+0x30 isValidPosition(vehicle number,
// equipment type, position, island, true)). A member whose vehicle information has no mobile
// is tested with its soldier (IAIPlayer +0x4C) instead, and then the island must also be the
// one of the soldier's last valid position. As in the image, neither information is null-checked.
bool SquadCoordinator::isValidPosition(SquadMember* member, const Vec3& position)
{
    Information* const info = informationManager->getInformation(
        member->getPlayer()->getCurrentVehicle()->getInformation(m_side + 1));
    IPIMobile* mobile = info->getMobile();
    unsigned int polygon;
    unsigned int island;
    if (mobile)
    {
        mobile->getLastValidPosition();
        const int equipment = info->getUnit()->getEquipmentType();
        if (!IAIPathfinding::instance->getPolygon(position, mobile->getVehicleNumber(), polygon, island, equipment))
            return false;
        return IAIPathfinding::instance->isValidPosition(
            mobile->getVehicleNumber(), info->getUnit()->getEquipmentType(), position, island, true);
    }
    Information* const soldier = informationManager->getInformation(
        member->getPlayer()->getSoldier()->getInformation(m_side + 1));
    mobile = soldier->getMobile();
    const IPIMobile::ValidPos last = mobile->getLastValidPosition();
    const int equipment = soldier->getUnit()->getEquipmentType();
    if (!IAIPathfinding::instance->getPolygon(position, mobile->getVehicleNumber(), polygon, island, equipment))
        return false;
    if (last.island == static_cast<unsigned int>(-1) || island != last.island)
        return false;
    return IAIPathfinding::instance->isValidPosition(
        mobile->getVehicleNumber(), soldier->getUnit()->getEquipmentType(), position, last.island, true);
}

// ELF 0x1A3A40: the requested-mission record (+60..+95) by value.
SquadCoordinator::MissionRequest SquadCoordinator::getMissionRequest()
{
    MissionRequest request;
    request.m_area = m_requestedArea;
    request.m_position = m_v64;
    request.m_type = m_i76;
    request.m_object = m_requestedObject;
    request.m_time = m_d84;
    request.m_commander = m_b92;
    return request;
}

// ELF 0x1A4950: a repair request at `position`, unconfirmed, valid for 60 s of game time.
void SquadCoordinator::requestRepair(const Vec3& position)
{
    m_b172 = true;
    m_v176 = position;
    const double now = static_cast<const IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
    m_b196 = false;
    m_d188 = now + 60.0f;
}

// ELF 0x1A6F30 (560 bytes). A squad order from a human squad leader (not `commander`) only moves
// the squad leader's goto position (+200) when it changed (componentwise beyond FLT_EPSILON) and
// the leader is not a bot: the roger-that flag is set, the paths invalidated and the formation
// leaves the leader. A commander order is taken when the side has a player commander, or when
// the squad has no mission and is not close to a non-owned target: the mission state (+120)
// becomes 1 (requested) until the update time + 10 s (+132), and the request record is filled.
void SquadCoordinator::requestMission(BotSquad::OrderType type, const Vec3& position, IAIObject* object, bool commander)
{
    SquadMember* const leader = getSquadLeader();
    if (!commander)
    {
        if (m_squadMembers.size() == 0)
            return;
        bool same = false;
        if (std::fabs(position.x - m_v200.x) <= 1.1920929e-07f && std::fabs(position.y - m_v200.y) <= 1.1920929e-07f)
            same = std::fabs(position.z - m_v200.z) <= 1.1920929e-07f;
        if (same || leader->isBot())
            return;
        m_b220 = true;
        m_v200 = position;
        updateSquadPaths();
        m_formation->leaveSquadLeader();
        return;
    }
    if ((!IAIMain::m_instance->getSAI(m_side + 1)->isPlayerCommander() && isSquadCloseToNonOwnedTarget())
        || m_missionStatus != 0)
    {
        if (!IAIMain::m_instance->getSAI(m_side + 1)->isPlayerCommander())
            return;
    }
    m_missionStatus = 1;
    m_d132 = m_d124 + 10.0f;
    m_v64 = position;
    m_i76 = type;
    m_requestedArea = 0;
    m_requestedObject = object;
    m_b92 = commander;
}

// ELF 0x1A79C0 (480 bytes). `position` itself when valid, else up to eight random points in the
// square of half-size `radius` around it, on the terrain; the first valid one replaces it.
bool SquadCoordinator::getRandomValidPosition(SquadMember* member, Vec3& position, float radius)
{
    if (isValidPosition(member, position))
        return true;
    IAITerrain* const terrain = static_cast<IAITerrain*>(CommonSingletonServer::m_iaiTerrain);
    AIRandom* const random = AIRandom::m_instance;
    Vec3 candidate;
    candidate.x = 0.0f;
    candidate.y = 0.0f;
    candidate.z = 0.0f;
    for (int attempt = 0; attempt <= 7; ++attempt)
    {
        // The image keeps each draw in x87 precision: u * 2^-32, doubled, times the radius.
        const float lowX = position.x - radius;
        candidate.x = static_cast<float>(static_cast<double>(random->next()) * 2.3283064e-10f * 2.0 * radius + lowX);
        const float lowZ = position.z - radius;
        candidate.z = static_cast<float>(static_cast<double>(random->next()) * 2.3283064e-10f * 2.0 * radius + lowZ);
        candidate.y = terrain->getTerrainAltitude(candidate.x, candidate.z);
        if (isValidPosition(member, candidate))
        {
            position = candidate;
            return true;
        }
    }
    return false;
}

// ELF 0x1A7160 (2016 bytes). Turns the requested mission (+60..) into the accepted one (+24..),
// for `member` (the squad leader, or the member itself when the leader is down).
//   * The area assignment is cleared first. Without a player commander, an attack request whose
//     closest strategic area has status 0 for this side unassigns the squad (false); the same order
//     on the same area is already accepted (true - unreachable after the clear unless no area).
//   * The closest area is taken when the goal lies inside it, or within the ignore-commander
//     distance (IAISettings +0x08) plus the area's size of its centre on the terrain and the area's
//     status is non-zero; the goal becomes the area's centre.
//   * Without an area, a valid position within 30 m of the goal is taken instead; failing that a
//     bot member declines (radio 7; as squad leader the engine's decline for the order type), and
//     the squad is unassigned.
//   * On acceptance: a human leader gets a commander message (attack 0 / defend 1, 3 s) when there
//     is no player commander; a bot leader acknowledges (radio 6 with a player commander), issues the
//     squad order and accepts the commander order for the member (attack 0x10, defend 0xF, move
//     0x11 on the radio), and every member's path is invalidated.
bool SquadCoordinator::acceptMission(SquadMember* member)
{
    const double now = static_cast<const IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
    bool accepted = false;
    m_strategicAreaAssignment = 0;
    AIStrategicArea* const closest = static_cast<AIStrategicArea*>(
        AIStrategicObjectManager::getInstance()->getClosestObject(m_v64, m_side + 1));
    if (!IAIMain::m_instance->getSAI(m_side + 1)->isPlayerCommander() && closest && m_i76 == 0
        && closest->m_status[m_side] == 0)
    {
        unAssignSquad();
        return false;
    }
    if (!IAIMain::m_instance->getSAI(m_side + 1)->isPlayerCommander() && m_i40 == m_i76
        && m_strategicAreaAssignment == closest)
        return true;

    if (closest)
    {
        const Vec2& centre2 = closest->getPosition(m_side + 1);
        const float cx = centre2.x;
        const float cz = centre2.y;
        Vec3 centre;
        centre.x = cx;
        centre.y = static_cast<IAITerrain*>(CommonSingletonServer::m_iaiTerrain)->getTerrainAltitude(cx, cz);
        centre.z = cz;
        const float slack = static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getIgnoreCommanderDistance();
        const float size = closest->m_size[m_side];
        bool take = closest->isInside(m_v64);
        if (!take)
        {
            const float dx = m_v64.x - centre.x;
            const float dy = m_v64.y - centre.y;
            const float dz = m_v64.z - centre.z;
            const float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
            take = !(static_cast<float>(slack + size) <= distance) && closest->m_status[m_side] != 0;
        }
        if (take)
        {
            m_v64 = centre;
            m_v28 = centre;
            m_missionStatus = 2;
            m_i40 = m_i76;
            m_strategicAreaAssignment = closest;
            m_i44 = m_requestedObject;
            m_d48 = now;
            m_b56 = m_b92;
            accepted = true;
        }
    }

    if (!m_strategicAreaAssignment)
    {
        Vec3 position = m_v64;
        if (getRandomValidPosition(member, position, 30.0f))
        {
            m_v64 = position;
            m_missionStatus = 2;
            m_v28 = m_v64;
            m_i40 = m_i76;
            m_i44 = m_requestedObject;
            m_b56 = m_b92;
            m_d48 = now;
            accepted = true;
        }
        else if (member->isBot())
        {
            member->getBot()->getCommunicationAgent()->enqueueRadioMessage(
                static_cast<AIRadioMessageType>(7), static_cast<AIRadioChannelType>(4), 3, 2.0f, 0);
            if (member->isSquadLeader())
            {
                IBFSquad* const engine = static_cast<IBFSquad*>(CommonSingletonServer::m_iBFSquad);
                if (m_i76 == 0)
                    engine->declineCommanderAttackOrder(m_squadIndex + 1, member->getPlayer());
                else if (m_i76 == 1)
                    engine->declineCommanderDefendOrder(m_squadIndex + 1, member->getPlayer());
                else if (m_i76 == 2)
                    engine->declineCommanderMoveOrder(m_squadIndex + 1, member->getPlayer());
            }
            unAssignSquad();
            updateSquadPaths();
        }
    }
    if (!accepted)
        return false;

    SquadMember* const leader = getSquadLeader();
    if (!leader->isBot())
    {
        if (!IAIMain::m_instance->getSAI(m_side + 1)->isPlayerCommander() && (m_i40 == 0 || m_i40 == 1))
            SquadManager::m_instance->enqueueCommanderMessage(
                m_side, m_squadIndex, static_cast<SquadManager::CommanderMessageType>(m_i40 == 0 ? 0 : 1), 3.0);
    }
    else
    {
        CommunicationAgent* const radio = leader->getBot()->getCommunicationAgent();
        if (IAIMain::m_instance->getSAI(m_side + 1)->isPlayerCommander())
            radio->enqueueRadioMessage(static_cast<AIRadioMessageType>(6), static_cast<AIRadioChannelType>(4), 3, 2.5f, 0);
        IBFSquad* const engine = static_cast<IBFSquad*>(CommonSingletonServer::m_iBFSquad);
        int message = -1;
        if (m_i40 == 0)
        {
            engine->issueSquadAttackOrder(m_v28, m_squadIndex + 1, m_side + 1, 0);
            engine->acceptCommanderAttackOrder(m_squadIndex + 1, member->getPlayer());
            message = 0x10;
        }
        else if (m_i40 == 1)
        {
            engine->issueSquadDefendOrder(m_v28, m_squadIndex + 1, m_side + 1, 0);
            engine->acceptCommanderDefendOrder(m_squadIndex + 1, member->getPlayer());
            message = 0xF;
        }
        else if (m_i40 == 2)
        {
            engine->issueSquadMoveOrder(m_v28, m_squadIndex + 1, m_side + 1, 0);
            engine->acceptCommanderMoveOrder(m_squadIndex + 1, member->getPlayer());
            message = 0x11;
        }
        if (message != -1)
            radio->enqueueRadioMessage(static_cast<AIRadioMessageType>(message), static_cast<AIRadioChannelType>(3), 3, 2.0f, 0);
    }
    for (unsigned int i = 0; i < m_squadMembers.size(); ++i)
        m_squadMembers[i]->setHavePath(false);
    return true;
}

// ELF 0x1A4DC0 (208 bytes). Whether the navigation material under `position` for the member's
// vehicle (IAIPathfinding +0x58 getMaterial) is water (4); true without a vehicle. The vehicle
// information is not null-checked, and getMaterial's result is ignored.
bool SquadCoordinator::isPosOnWater(SquadMember* member, Vec3& position)
{
    IAIObject* const vehicle = member->getPlayer()->getCurrentVehicle();
    if (!vehicle)
        return true;
    Information* const info = informationManager->getInformation(vehicle->getInformation(m_side + 1));
    const int equipment = info->getUnit()->getEquipmentType();
    unsigned char material;
    IAIPathfinding::instance->getMaterial(position, info->getMobile()->getVehicleNumber(), equipment, material);
    return material == 4;
}

// ELF 0x1A6B20: the accepted mission's goal (+28), only for a mission without a target object.
bool SquadCoordinator::getSquadMissionTargetPos(Vec3& position)
{
    if (m_i44)
        return false;
    position = m_v28;
    return true;
}

// Stores side and squad identity before constructing the formation, which queries this coordinator.

SquadCoordinator::SquadCoordinator(int side, int squadIndex)
    : m_squadMembers()
    , m_list12()
    , m_squadIndex(squadIndex)
    , m_side(side)
    , m_strategicAreaAssignment(0)
    , m_i40(4)
    , m_i44(0)
    , m_d48(-20.0)
    , m_requestedArea(0)
    , m_i76(4)
    , m_requestedObject(0)
    , m_squadLeaderIndex(-1)
    , m_list104()
    , m_missionStatus(0)
    , m_d124(0.0)
    , m_d132(0.0)
    , m_d140(0.0)
    , m_i148(0)
    , m_i152(0)
    , m_b156(false)
    , m_b157(false)
    , m_positions()
    , m_b172(false)
    , m_b196(false)
    , m_b220(false)
{
    m_v28.x = 0.0f;
    m_v28.y = 0.0f;
    m_v28.z = 0.0f;
    m_v64.x = 0.0f;
    m_v64.y = 0.0f;
    m_v64.z = 0.0f;
    m_v108.x = 0.0f;
    m_v108.y = 0.0f;
    m_v108.z = 0.0f;
    m_v176.x = 0.0f;
    m_v176.y = 0.0f;
    m_v176.z = 0.0f;
    m_v200.x = 0.0f;
    m_v200.y = 0.0f;
    m_v200.z = 0.0f;

    m_formation = new SquadFormation(this);

    m_v108.x = 0.0f;
    m_v108.y = 0.0f;
    m_v108.z = 0.0f;

    // two of them, pushed one after the other. See the header.
    Vec3 zero;
    zero.x = 0.0f;
    zero.y = 0.0f;
    zero.z = 0.0f;
    m_positions.push_back(zero);
    m_positions.push_back(zero);

    m_v200.x = 0.0f;
    m_v200.y = 0.0f;
    m_v200.z = 0.0f;

    resetNeedOrderMessageTimer(
        static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime());

    m_b220 = false;
}

int SquadCoordinator::getSide()
{
    return m_side;
}

// Copy the stored squad position
// (retail ELF +0x6C / PE +0x8C), without testing squadHasPosition. [INFERRED]
// void return: both callers discard it; EAX holds different scratch values per port.
void SquadCoordinator::getSquadPosition(Vec3& position)
{
    position = m_v108;
}

// The stored leader index is
// used directly, including the absence of an empty-vector or negative-index guard.
SquadMember* SquadCoordinator::getSquadLeader()
{
    return m_squadMembers[m_squadLeaderIndex];
}

// Signed count consumed by the manager's comparisons.
int SquadCoordinator::getNumberOfSquadMembers()
{
    return static_cast<int>(m_squadMembers.size());
}

// non-const in the mangled signature.
int SquadCoordinator::getSquadId()
{
    return m_squadIndex;
}

// Zero-distance test with 0.001f, including true for NaN.
// PE sums z squared, y squared, then x squared. No square root or component-wise epsilon.
bool SquadCoordinator::squadHasPosition()
{
    const float x = -m_v108.x;
    const float y = -m_v108.y;
    const float z = -m_v108.z;
    return !(z * z + y * y + x * x < 0.001f);
}

// PE inlined at : four stores in this order.
void SquadCoordinator::unAssignSquad()
{
    m_strategicAreaAssignment = 0;
    m_i40 = 4;
    m_i44 = 0;
    m_missionStatus = 0;
}

// Allocation is not checkPointer-wrapped. The human arm
// queries the one-based side's commander before publishing the member in the vector.
void SquadCoordinator::addSquadMember(IAIPlayer* player)
{
    SquadMember* member = new SquadMember(this, player, m_squadIndex, m_side,
                                         static_cast<int>(m_squadMembers.size()));
    if (!member->isBot())
    {
        if (!IAIMain::m_instance->getSAI(m_side + 1)->isPlayerCommander())
            unAssignSquad();
    }
    m_squadMembers.push_back(member);
    if (member->isAllowedInFormation())
    {
        m_formation->joinFormation(member->getMemberIndex());
        m_formation->updateMovementFlag(true);
    }
    member->updateMemberPos();
}

// Stop on the first engine-recognized leader; later members
// are not visited. No found leader leaves the old index intact. A first leader skips the
// change arm, but the single-member trailing-position reset still runs.
void SquadCoordinator::updateSquadLeader()
{
    const int oldLeader = m_squadLeaderIndex;
    bool found = false;
    for (unsigned int i = 0; i < m_squadMembers.size(); ++i)
    {
        m_squadMembers[i]->setIsSquadLeader(
            static_cast<IBFSquad*>(CommonSingletonServer::m_iBFSquad)->isSquadLeader(
                m_squadIndex + 1, m_squadMembers[i]->getPlayer()));
        if (m_squadMembers[i]->isSquadLeader())
        {
            m_squadLeaderIndex = static_cast<int>(i);
            found = true;
            break;
        }
    }
    if (!found)
        return;

    if (oldLeader != m_squadLeaderIndex && oldLeader != -1 && !m_squadMembers.empty())
    {
        // ELF 0x1A5C66: a new leader - the formation rejoins it and the squad position and paths
        // are refreshed; a human leader of a squad with more members leaves the AI commander's
        // assignment (unless the side has a player commander).
        m_squadMembers[m_squadLeaderIndex]->updateMemberPos();
        m_formation->joinSquadLeader();
        updateSquadPosition();
        updateSquadPaths();
        if (getSquadLeader() && !getSquadLeader()->isBot() && m_squadMembers.size() > 1
            && !IAIMain::m_instance->getSAI(m_side + 1)->isPlayerCommander())
            unAssignSquad();
    }
    if (m_squadMembers.size() == 1)
        m_formation->resetTrailingPositions();
}

// - one indexed load through the vector's data pointer.
// no bounds check. Same shape as `BotManager::getBot`, and the same reason to leave it alone.
SquadMember* SquadCoordinator::getSquadMember(int index)
{
    return m_squadMembers[index];
}

// First matching player wins. The output index is written
// only on success, and the vector/member is fetched again after the player-id call.
SquadMember* SquadCoordinator::getSquadMember(int playerId, int& index)
{
    for (unsigned int i = 0; i < m_squadMembers.size(); ++i)
    {
        if (m_squadMembers[i]->getPlayerId() == playerId)
        {
            index = static_cast<int>(i);
            return m_squadMembers[i];
        }
    }
    return 0;
}

// Skip null members, but do not guard their player.
// Compare the complete returned integer; no alive/bot/leader filter exists. Both images
// refresh the member vector after a nonmatching virtual getSpawnKit callback.
bool SquadCoordinator::hasMemberWithKit(KitType type)
{
    for (unsigned int i = 0; i < m_squadMembers.size(); ++i)
    {
        if (m_squadMembers[i] && m_squadMembers[i]->getPlayer()->getSpawnKit() == type)
            return true;
    }
    return false;
}

// Only pointer presence is tested;
// there is no area validity, membership, side or mission-state query.
bool SquadCoordinator::isSquadAssignedToAnyArea()
{
    return m_strategicAreaAssignment != 0;
}

// Return the stored pointer unchanged.
// ELF callers use it directly as AIStrategicArea::getRouteList/orderNormalBot's this;
// the paired kit callers read its area index at +0xF0. No ownership is transferred.
AIStrategicArea* SquadCoordinator::getSquadStrategicAreaAssignment()
{
    return m_strategicAreaAssignment;
}

// - one store of a double.
void SquadCoordinator::resetNeedOrderMessageTimer(double now)
{
    m_needOrderMessageTimer = now;
}

// ELF 0x1A3790: the per-round reset, store by store. The fire records are freed, the formation
// rejoins its leader and reassigns its slots, the need-orders timer restarts at the game time and
// every member resets its own state.
void SquadCoordinator::reset()
{
    m_strategicAreaAssignment = 0;
    m_i40 = 4;
    m_d48 = -20.0;
    m_i44 = 0;
    m_i76 = 4;
    m_requestedArea = 0;
    m_requestedObject = 0;
    m_missionStatus = 0;
    m_formation->joinSquadLeader();
    for (std::list<SquadMemberFire*>::iterator it = m_list12.begin(); it != m_list12.end();)
    {
        ::operator delete(*it);
        it = m_list12.erase(it);
    }
    m_formation->reassignSlots();
    m_v108.x = m_v108.y = m_v108.z = 0.0f;
    m_i148 = 0;
    m_i152 = 0;
    m_d132 = 0.0;
    m_d140 = 0.0;
    m_b156 = false;
    m_b157 = false;
    m_b172 = false;
    m_b196 = false;
    m_v200.x = m_v200.y = m_v200.z = 0.0f;
    resetNeedOrderMessageTimer(static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime());
    m_b220 = false;
    for (unsigned int i = 0; i < m_squadMembers.size(); ++i)
        m_squadMembers[i]->reset();
}

// ELF 0x1A64B0: frees and unlinks every fire record of the member's player (IAIPlayer +0x1C).
void SquadCoordinator::removeSquadMemberFire(SquadMember* member)
{
    const int playerId = member->getPlayer()->getId();
    for (std::list<SquadMemberFire*>::iterator it = m_list12.begin(); it != m_list12.end();)
    {
        if ((*it)->m_playerId == playerId)
        {
            ::operator delete(*it);
            it = m_list12.erase(it);
        }
        else
            ++it;
    }
}

// ELF 0x1A82E0: the member leaves the fire records and, if it holds a slot, the formation; it is
// unlinked (not deleted - the caller owns it now), the remaining members are renumbered, the
// leader is re-elected and the slots are reassigned.
void SquadCoordinator::removeSquadMember(int index)
{
    SquadMember* const member = m_squadMembers[index];
    removeSquadMemberFire(member);
    if (m_formation->isInFormation(member->getMemberIndex()))
        m_formation->leaveFormation(member->getMemberIndex());
    m_squadMembers.erase(m_squadMembers.begin() + index);
    for (unsigned int i = 0; i < m_squadMembers.size(); ++i)
        m_squadMembers[i]->setMemberIndex(static_cast<int>(i));
    updateSquadLeader();
    m_formation->reassignSlots();
}

// ELF 0x1A84F0: the first member with the same player id is removed by index; none, nothing.
void SquadCoordinator::removeSquadMember(SquadMember* member)
{
    for (unsigned int i = 0; i < m_squadMembers.size(); ++i)
    {
        if (member->getPlayerId() == m_squadMembers[i]->getPlayerId())
        {
            removeSquadMember(static_cast<int>(i));
            return;
        }
    }
}

// ELF 0x1A6EC0: with an assigned area whose status for this squad's side is non-zero, true as
// soon as one member is close to that non-owned target.
bool SquadCoordinator::isSquadCloseToNonOwnedTarget()
{
    if (!m_strategicAreaAssignment || !m_strategicAreaAssignment->m_status[m_side])
        return false;
    for (unsigned int i = 0; i < m_squadMembers.size(); ++i)
        if (m_squadMembers[i]->isCloseToNonOwnedTarget())
            return true;
    return false;
}

// ELF 0x1A6B00: the mission state at +0x78 (m_missionStatus) equals 2.
bool SquadCoordinator::squadHasMission()
{
    return m_missionStatus == 2;
}

// ELF 0x1A7FA0.
bool SquadCoordinator::isSquadAvailable()
{
    return m_missionStatus == 0;
}

// ELF 0x1A7F60: the requested area's index (+0xF0).
bool SquadCoordinator::isSquadRequestedToArea(int areaIndex)
{
    bool requested = false;
    if (m_requestedArea)
        requested = areaIndex == m_requestedArea->getIndex();
    return requested;
}

// ELF 0x1A3930: every member's request and record are freed, the list emptied, slots reassigned.
void SquadCoordinator::removeAll()
{
    for (unsigned int i = 0; i < m_squadMembers.size(); ++i)
    {
        SquadMember* const member = m_squadMembers[i];
        member->removeRequest();
        delete member;
    }
    m_squadMembers.resize(0);
    m_formation->reassignSlots();
}

// ELF 0x1A83C0 (290 bytes). Like removeSquadMember, but the member record is destroyed and a
// bot's back-pointer cleared (Bot vtable +0x13C setSquadMember(0)).
void SquadCoordinator::destroySquadMember(int index)
{
    SquadMember* const member = m_squadMembers[index];
    removeSquadMemberFire(member);
    member->removeRequest();
    if (m_formation->isInFormation(member->getMemberIndex()))
        m_formation->leaveFormation(member->getMemberIndex());
    m_squadMembers.erase(m_squadMembers.begin() + index);
    if (member->isBot())
        member->getBot()->setSquadMember(0);
    delete member;
    for (unsigned int i = 0; i < m_squadMembers.size(); ++i)
        m_squadMembers[i]->setMemberIndex(i);
    updateSquadLeader();
    m_formation->reassignSlots();
}

} // namespace ai
} // namespace hfe
} // namespace dice

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x1A5D90, added by ai-w1-e-fire: a squad mate fired at `target`; every other bot of the
// squad sees the target's primary object (side + 1 = the image's `m_side + 1`) at the origin,
// part -1, neither direct nor flag 0x40.
void SquadCoordinator::shotsFired(int playerId, unsigned int target)
{
    const Vec3 origin = { 0.0f, 0.0f, 0.0f };
    Information* const information = informationManager->getInformation(target);
    if (!information)
        return;
    const unsigned int primary = information->getPrimaryObject(m_side + 1);
    Information* const primaryInformation = informationManager->getInformation(primary);
    if (!primaryInformation)
        return;
    for (unsigned int i = 0; i < m_squadMembers.size(); ++i)
    {
        SquadMember* const member = m_squadMembers[i];
        if (!member || !member->isBot() || member->getPlayerId() == playerId)
            continue;
        member->getBot()->getSensingAgent()->getSenses()->event_seenObject(
            primary, origin, static_cast<unsigned int>(-1), primaryInformation, false, false);
    }
}

// ELF 0x1A86D0.
bool SquadCoordinator::isAnySquadMemberRespondingToRequest(unsigned int handle, int action)
{
    for (unsigned int i = 0; i < m_squadMembers.size(); ++i)
    {
        if (m_squadMembers[i]->isSameRequest(handle, action))
            return true;
    }
    return false;
}

// ELF 0x1A8730 (1.7 KB). `target` is the squad member occupying the request's object (a hurt or
// downed player, a vehicle needing repair). Request actions as the callers pass them: 1 revive
// (BBRevive), 2 repair (BBBridgeRepair), 4 heal / supply (BBMedicAssist), 5 (any number may answer).
// * No occupant: 0 when someone of the side already answers; else 1, 0 beyond sqrt(20000) m of the
//   squad position (+108), doubled when a human occupies the object.
// * Occupant in this squad: 0 for a revive of a living (health > 0.001) medic (kit 5) other than
//   `member`; 0 for request 4 to a living engineer (kit 3); otherwise 0 when another member
//   already has the request, x0.75 per such member for action 5.
// * Occupant in another squad: actions 1 / 2 / 4 within sqrt(1000) m of the squad position and with
//   nobody answering: 0.5, or 0.1 when the occupant's squad has its own medic (5) / kit 4 /
//   engineer (3); 0 otherwise; other actions 1.
// Finally doubled when the occupant is human.
float SquadCoordinator::requestResponseModifier(SquadMember* member, unsigned int handle, int action)
{
    SquadManager* const manager = SquadManager::m_instance;
    float modifier = 1.0f;
    bool skipRequests = false;
    float distanceSq = 0.0f;
    Vec3 position;
    position.x = 0.0f;
    position.y = 0.0f;
    position.z = 0.0f;
    SquadMember* const target = manager->getSquadMember(handle, m_side + 1);
    bool zero = false;
    if (!target)
    {
        if (manager->isAnySquadMemberRespondingToRequest(handle, action, m_side + 1))
        {
            zero = true;
        }
        else
        {
            Information* const info = informationManager->getInformation(handle);
            modifier = 1.0f;
            if (info)
            {
                const Vec3& p = info->getPosition();
                if (20000.0f < (p.x - m_v108.x) * (p.x - m_v108.x) + (p.y - m_v108.y) * (p.y - m_v108.y)
                                   + (p.z - m_v108.z) * (p.z - m_v108.z))
                    modifier = 0.0f;
                if (info->getUnit())
                {
                    const int occupant = info->getUnit()->getOccupyingPlayerId(m_side + 1);
                    if (occupant != -1 && IBotManager::instance->getBotFromPlayerId(occupant) == 0)
                        modifier = modifier + modifier;
                }
            }
        }
    }
    else if (target->getSquadId() == member->getSquadId())
    {
        if (target->getMemberIndex() != member->getMemberIndex() && action == 1)
        {
            // No null guard on the information (0x1A87F8).
            IPIPhysical* const physical = informationManager->getInformation(handle)->getPhysical();
            if (physical && 0.001f < physical->getHealth() && target->getPlayer()->getSpawnKit() == 5)
                zero = true;
        }
        if (!zero && action == 4 && target->getMemberIndex() != member->getMemberIndex())
        {
            IPIPhysical* const physical = informationManager->getInformation(handle)->getPhysical();
            if (physical && 0.001f < physical->getHealth() && target->getPlayer()->getSpawnKit() == 3)
            {
                modifier = 0.0f;
                skipRequests = true;
            }
        }
        if (!zero && !skipRequests)
        {
            for (unsigned int i = 0; i < m_squadMembers.size(); ++i)
            {
                SquadMember* const other = m_squadMembers[i];
                if (member->getMemberIndex() != other->getMemberIndex() && other->isSameRequest(handle, action))
                {
                    if (action != 5)
                    {
                        zero = true;
                        break;
                    }
                    modifier = modifier * 0.75f;
                }
            }
        }
    }
    else
    {
        const bool responding = manager->isAnySquadMemberRespondingToRequest(handle, action, m_side + 1);
        if (target->getPosition(position))
            distanceSq = (position.z - m_v108.z) * (position.z - m_v108.z)
                         + (position.x - m_v108.x) * (position.x - m_v108.x)
                         + (position.y - m_v108.y) * (position.y - m_v108.y);
        int kit = -1;
        switch (action)
        {
        case 1: kit = 5; break;
        case 2: kit = 4; break;
        case 4: kit = 3; break;
        default: break;
        }
        if (kit >= 0)
        {
            if (!responding && distanceSq < 1000.0f)
            {
                // No null guard on the squad (0x1A8958).
                SquadCoordinator* const squad = manager->getSquadFromPlayer(target->getPlayerId(), m_side + 1);
                modifier = squad->hasMemberWithKit(static_cast<KitType>(kit)) ? 0.1f : 0.5f;
            }
            else
            {
                zero = true;
            }
        }
    }
    if (zero)
        modifier = 0.0f;
    if (target && !target->isBot())
        modifier = modifier + modifier;
    return modifier;
}

// ELF 0x1A69C0.
bool SquadCoordinator::isSameMission(BotSquad::OrderType type, const Vec3& position, IAIObject* object)
{
    if (m_i40 != static_cast<int>(type) || m_i44 != object)
        return false;
    const float epsilon = 1.1920929e-07f;
    if (epsilon < std::fabs(m_v28.x - position.x))
        return false;
    if (epsilon < std::fabs(m_v28.y - position.y))
        return false;
    return epsilon >= std::fabs(m_v28.z - position.z);
}

// ELF 0x1A3270 (D1, 470 B; recovered on the client track, docs/macos-c-singleplayer.md). Each
// member drops its request and leaves the engine squad (IBFSquad +0x10, exitSquad with the
// 1-based squad number) before it is deleted - the member pointer is used before the null test,
// as shipped. Then the formation goes, then every fire record (plain operator delete, the node
// erased behind the iterator). The containers follow as members.
SquadCoordinator::~SquadCoordinator()
{
    for (unsigned int i = 0; i < m_squadMembers.size(); ++i)
    {
        SquadMember* const member = m_squadMembers[i];
        member->removeRequest();
        static_cast<IBFSquad*>(CommonSingletonServer::m_iBFSquad)->exitSquad(m_squadIndex + 1, member->getPlayer());
        delete member;
    }
    delete m_formation;
    std::list<SquadMemberFire*>::iterator fire = m_list12.begin();
    while (fire != m_list12.end())
    {
        delete *fire;
        m_list12.erase(fire++);
    }
}

namespace {

// [CHOSEN, not recovered] packaging: the engine's inline Vec2 / Vec3 normalise as the fire and
// flank bodies below spell it out (0x1A6040.., 0x1A4360..): left alone within FLT_EPSILON of unit
// length, zeroed below the threshold (Vec2: FLT_EPSILON^2, Vec3: FLT_EPSILON), else scaled by
// 1/sqrt.
void normalizeInline(Vec2& v)
{
    const float lsq = v.x * v.x + v.y * v.y;
    if (std::fabs(lsq - 1.0f) < 1.1920929e-07f)
        return;
    if (std::fabs(lsq) < 1.4210855e-14f)
    {
        v.x = 0.0f;
        v.y = 0.0f;
        return;
    }
    const float inv = 1.0f / std::sqrt(lsq);
    v.x = v.x * inv;
    v.y = inv * v.y;
}

void normalizeInline(Vec3& v)
{
    const float lsq = v.z * v.z + v.x * v.x + v.y * v.y;
    if (std::fabs(lsq - 1.0f) < 1.1920929e-07f)
        return;
    if (std::fabs(lsq) < 1.1920929e-07f)
    {
        v.x = 0.0f;
        v.y = 0.0f;
        v.z = 0.0f;
        return;
    }
    const float inv = 1.0f / std::sqrt(lsq);
    v.x = v.x * inv;
    v.y = v.y * inv;
    v.z = inv * v.z;
}

} // anonymous namespace

// ELF 0x1A4250. No caller in libaidll.so. Every bot may fire. Once per mission (m_b157), with a
// mission (+0x78 == 2) and a formation leader slot: from the squad position (m_v108) towards the
// accepted mission position (m_v28, put on the terrain), with probability 0.3 (0.6 when the squad
// cannot see the target), the leader (slot 0) is given two flank points: the half-distance point
// of the direction turned by 0.781 rad (sign at random), then half the distance further along the
// direction; each is randomised within a quarter of the distance. When either fails the other
// side is tried; on success m_b156 is set and the leader re-paths. The leader's liveness is
// tested only after the null test.
void SquadCoordinator::squadManeuverDynamicFlank()
{
    for (unsigned int index = 0; index < m_squadMembers.size(); ++index)
    {
        SquadMember* const member = m_squadMembers[index];
        if (member->isBot())
            member->getBot()->setAllowFire(true);                              // +0x134
    }
    if (m_b157 || m_missionStatus != 2 || !m_formation->hasSquadMembers())
        return;
    m_b157 = true;
    SquadMember* const leader = m_squadMembers[m_formation->getSquadMemberIndexAtSlot(0)];

    Vec3 direction;
    direction.x = m_v28.x - m_v108.x;
    direction.y = m_v28.y - m_v108.y;
    direction.z = m_v28.z - m_v108.z;
    float distance =
        std::sqrt(direction.x * direction.x + direction.y * direction.y + direction.z * direction.z);
    normalizeInline(direction);

    Vec3 target = m_v28;
    Vec3 turned;
    turned.x = 0.0f;
    turned.y = 0.0f;
    turned.z = 0.0f;
    target.y = static_cast<IAITerrain*>(CommonSingletonServer::m_iaiTerrain)
                   ->getTerrainAltitude(target.x, target.z);                    // +0x14
    float probability = 0.3f;
    float angle = 0.781f;
    if (!canSeePos(m_v108, target))
        probability = probability + probability;
    if (!(AIRandom::m_instance->nextFloat() < probability) || leader == 0)
        return;
    if (!leader->getPlayer()->getIsAlive())
        return;
    const float half = 0.5f;
    if (half < AIRandom::m_instance->nextFloat())
        angle = -angle;
    rotateXZ(direction, turned, angle);
    m_positions[0].x = distance * turned.x * half + m_v108.x;
    m_positions[0].y = distance * turned.y * half + m_v108.y;
    m_positions[0].z = distance * turned.z * half + m_v108.z;
    m_positions[1].x = distance * direction.x * half + m_positions[0].x;
    m_positions[1].y = distance * direction.y * half + m_positions[0].y;
    m_positions[1].z = half * distance * direction.z + m_positions[0].z;
    const float radius = distance * 0.25f;
    if (!getRandomValidFlankPos(leader, radius, m_positions[0])
        || !getRandomValidFlankPos(leader, radius, m_positions[1]))
    {
        angle = -angle;
        rotateXZ(direction, turned, angle);
        m_positions[0].x = distance * turned.x * half + m_v108.x;
        m_positions[0].y = distance * turned.y * half + m_v108.y;
        m_positions[0].z = distance * turned.z * half + m_v108.z;
        m_positions[1].x = distance * direction.x * half + m_positions[0].x;
        m_positions[1].y = distance * direction.y * half + m_positions[0].y;
        m_positions[1].z = distance * direction.z * half + m_positions[0].z;
        distance = distance * 0.25f;
        if (!getRandomValidFlankPos(leader, distance, m_positions[0]))
            return;
        if (!getRandomValidFlankPos(leader, distance, m_positions[1]))
            return;
    }
    m_b156 = true;
    leader->setHavePath(false);
}

// ELF 0x1A4A10. A line one metre above both points, ignoring the squad's vehicles
// (IAIObjectEnvironment +0x40, collideLineWithWorld(from, to, ignore, false, 0, true, hit)):
// visible when nothing is hit.
bool SquadCoordinator::canSeePos(const Vec3& from, const Vec3& to)
{
    Vec3 start = from;
    start.y = from.y + 1.0f;
    Vec3 end = to;
    end.y = 1.0f + to.y;
    std::vector<IAIObject*> ignore;
    getSquadIgnoreList(ignore);
    bool hit;
    const bool collided = static_cast<IAIObjectEnvironment*>(CommonSingletonServer::m_iaiObjectEnvironment)
                              ->collideLineWithWorld(start, end, ignore, false, 0.0f, true, hit);
    return !collided;
}

// ELF 0x1A4B10: the two flank points.
void SquadCoordinator::getFlankingCoords(std::vector<Vec3>& coords)
{
    coords = m_positions;
}

// ELF 0x1A4B60. `position` put on the terrain is kept when it is valid for `member` and not on
// water - then `position` itself is NOT updated (its height stays as given). Otherwise up to ten
// random points in the square of half-side `radius` around it; the first good one is stored.
bool SquadCoordinator::getRandomValidFlankPos(SquadMember* member, float radius, Vec3& position)
{
    IAITerrain* const terrain = static_cast<IAITerrain*>(CommonSingletonServer::m_iaiTerrain);
    bool found = true;
    Vec3 candidate = position;
    candidate.y = terrain->getTerrainAltitude(candidate.x, candidate.z);       // +0x14
    if (!isValidPosition(member, candidate) || isPosOnWater(member, candidate))
    {
        found = false;
        for (int attempt = 0; attempt < 10; ++attempt)
        {
            const float lowX = position.x - radius;
            const float drawX = AIRandom::m_instance->nextFloat() * radius;
            candidate.x = drawX + drawX + lowX;
            const float lowZ = position.z - radius;
            const float drawZ = AIRandom::m_instance->nextFloat() * radius;
            candidate.z = drawZ + drawZ + lowZ;
            candidate.y = terrain->getTerrainAltitude(candidate.x, candidate.z);
            if (isValidPosition(member, candidate) && !isPosOnWater(member, candidate))
            {
                position = candidate;
                return true;
            }
        }
    }
    return found;
}

// ELF 0x1A5F00 (SquadManager::shotsFired). Nothing unless the shooter has a position. An existing
// record of the shooter is refreshed: when the shot's XZ direction turns away from the record's
// re-path direction (normalised dot below 0.1) that direction is replaced and the whole squad
// re-paths; then the latest direction and position, the expiry (now + 10 s) and the firing target
// (Bot +0x80 for a bot, else -1) are stored. Without a record a new one is appended with both
// directions and both positions from this shot, and the squad re-paths.
void SquadCoordinator::shotsFired(int playerId, const Vec3& direction)
{
    Vec3 position;
    position.x = 0.0f;
    position.y = 0.0f;
    position.z = 0.0f;
    const double now = static_cast<const IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
    Vec2 shot;
    shot.x = direction.x;
    shot.y = direction.z;
    int target = -1;
    int index = 0;
    Vec2 previous;
    previous.x = 0.0f;
    previous.y = 0.0f;
    SquadMember* const shooter = getSquadMember(playerId, index);
    if (!shooter->getPosition(position))
        return;

    std::list<SquadMemberFire*>::iterator it = m_list12.begin();
    for (; it != m_list12.end(); ++it)
    {
        SquadMemberFire* const fire = *it;
        if (playerId != fire->m_playerId)
            continue;
        vec3ToVec2XZ(fire->m_v20, previous);
        normalizeInline(previous);
        normalizeInline(shot);
        if (shot.x * previous.x + shot.y * previous.y < 0.1f)
        {
            fire->m_v20 = direction;
            for (unsigned int i = 0; i < m_squadMembers.size(); ++i)
                m_squadMembers[i]->setHavePath(false);
            m_formation->updateMovementFlag(true);
        }
        fire->m_v8 = direction;
        fire->m_v40 = position;
        fire->m_time = now + 10.0f;
        if (shooter->isBot())
            target = static_cast<int>(shooter->getBot()->getFiringTarget());   // +0x80
        fire->m_i64 = target;
        break;
    }
    if (it != m_list12.end())
        return;

    if (shooter->isBot())
        target = static_cast<int>(shooter->getBot()->getFiringTarget());
    SquadMemberFire* const fire = new SquadMemberFire;
    fire->m_v8.x = fire->m_v8.y = fire->m_v8.z = 0.0f;
    fire->m_v20.x = fire->m_v20.y = fire->m_v20.z = 0.0f;
    fire->m_v40.x = fire->m_v40.y = fire->m_v40.z = 0.0f;
    fire->m_position.x = fire->m_position.y = fire->m_position.z = 0.0f;
    fire->m_v20 = direction;
    fire->m_v8 = direction;
    fire->m_playerId = playerId;
    fire->m_member = getSquadMember(playerId, index);
    fire->m_v40 = position;
    fire->m_position = position;
    fire->m_i64 = target;
    fire->m_time = now + 10.0f;
    m_list12.push_back(fire);
    for (unsigned int i = 0; i < m_squadMembers.size(); ++i)
        m_squadMembers[i]->setHavePath(false);
    m_formation->updateMovementFlag(true);
}

// ELF 0x1A6570: any fire record (the list is counted).
bool SquadCoordinator::anyFiring()
{
    return m_list12.size() != 0;
}

// ELF 0x1A65B0: a fire record of `playerId`.
bool SquadCoordinator::isFiring(int playerId)
{
    for (std::list<SquadMemberFire*>::iterator it = m_list12.begin(); it != m_list12.end(); ++it)
        if (playerId == (*it)->m_playerId)
            return true;
    return false;
}

// ELF 0x1A6610: distance output is written only when a nearer eligible shooter is found.
SquadMember* SquadCoordinator::getClosestFiringMember(SquadMember* member, float& distanceSquared)
{
    SquadMember* closest = 0;
    Vec3 position = {};
    Vec3 candidatePosition = {};
    const int playerId = member->getPlayerId();
    float nearest = FLT_MAX;
    if (!member->getPosition(position))
        return closest;
    for (std::list<SquadMemberFire*>::iterator it = m_list12.begin(); it != m_list12.end(); ++it)
    {
        SquadMemberFire* const fire = *it;
        SquadMember* const candidate = fire->m_member;
        if (candidate->getPlayerId() == playerId || !candidate->getPosition(candidatePosition)
            || !candidate->getPlayer()->getIsAlive()
            || !m_formation->isInFormation(candidate->getMemberIndex())
            || !member->canDamageTarget(static_cast<unsigned int>(fire->m_i64)))
            continue;
        if (candidate->isSquadLeader() && m_formation->shouldLeaveSquadLeader())
            continue;
        const float dx = position.x - candidatePosition.x;
        const float dy = position.y - candidatePosition.y;
        const float dz = position.z - candidatePosition.z;
        const float squared = dx * dx + dy * dy + dz * dz;
        if (nearest > squared)
        {
            nearest = squared;
            closest = candidate;
            distanceSquared = squared;
        }
    }
    return closest;
}

// ELF 0x1A6820: the member's latest shot direction, (1, 0, 0) without a record.
Vec3 SquadCoordinator::getFiringDir(SquadMember* member)
{
    Vec3 direction;
    direction.y = 0.0f;
    direction.z = 0.0f;
    direction.x = 1.0f;
    for (std::list<SquadMemberFire*>::iterator it = m_list12.begin(); it != m_list12.end(); ++it)
    {
        SquadMemberFire* const fire = *it;
        if (fire->m_playerId == member->getPlayerId())
        {
            direction = fire->m_v8;
            return direction;
        }
    }
    return direction;
}

// ELF 0x1A6A60. Compares the requested mission (+0x4C type, +0x50 object, +0x40 position), not
// the accepted one isSameMission reads.
bool SquadCoordinator::isSameRequest(BotSquad::OrderType type, const Vec3& position, IAIObject* object)
{
    bool same = false;
    if (m_i76 == static_cast<int>(type) && m_requestedObject == object)
    {
        const float epsilon = 1.1920929e-07f;
        if (std::fabs(m_v64.x - position.x) <= epsilon && std::fabs(m_v64.y - position.y) <= epsilon)
            same = std::fabs(m_v64.z - position.z) <= epsilon;
    }
    return same;
}

// ELF 0x1A7940.
SquadMember* SquadCoordinator::getBotInSquad()
{
    for (unsigned int index = 0; index < m_squadMembers.size(); ++index)
        if (m_squadMembers[index]->isBot())
            return m_squadMembers[index];
    return 0;
}

// ELF 0x1A7E70: by value.
std::list<AIStrategicObject*> SquadCoordinator::getListOfAreaNavPoints()
{
    return m_list104;
}

// ELF 0x1A8580.
void SquadCoordinator::getSquadIgnoreList(std::vector<IAIObject*>& ignore)
{
    for (unsigned int index = 0; index < m_squadMembers.size(); ++index)
    {
        IAIObject* const vehicle = m_squadMembers[index]->getPlayer()->getCurrentVehicle();   // +0x54
        if (vehicle != 0)
            ignore.push_back(vehicle);
    }
}

// ELF 0x1A8680.
bool SquadCoordinator::squadHasHumanPlayer()
{
    for (unsigned int index = 0; index < m_squadMembers.size(); ++index)
        if (!m_squadMembers[index]->isBot())
            return true;
    return false;
}

} // namespace ai
} // namespace hfe
} // namespace dice
