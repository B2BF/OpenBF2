
#include "SquadFormation.h"

// <iostream>'s ios_base::Init object is this TU's static initialisation and teardown (ELF
// 0x1ACBB0, __tcf_0 0x1ACC10, _GLOBAL__I 0x1ACC40).
#include <iostream>

#include <cmath>

#include <list>

#include "SquadCoordinator.h"
#include "SquadMember.h"
#include "../Main/AIDllFrontier.h"
#include "../Main/CommonSingletonServer.h"
#include "../Main/AIRandom.h"
#include "../Main/InformationManager.h"
#include "../AIKnowledge/Information.h"
#include "../AIKnowledge/IPIMobile.h"
#include "../../../AIInterface/BFKnowledge/IAIObject.h"
#include "../../../AIInterface/BFKnowledge/AIPlayer.h"   // IAIPlayer::getCurrentVehicle
#include "../../../AIInterface/BFKnowledge/BFGame.h"     // IAIGame::getMaxNumberOfSquadMembers
#include "../../../AIInterface/BFKnowledge/BFTimer.h"
#include "../../../AIInterface/BFKnowledge/BFObjectEnvironment.h"

namespace dice {
namespace hfe {
namespace ai {

namespace {

// attested type name: `std::list<dice::hfe::ai::(anonymous namespace)::SquadMemberDistanceObj>`
// is instantiated from this translation unit (the anonymous namespace carries its path). Eight
// bytes, from the list's 16-byte header node. Member names are [INFERRED].
struct SquadMemberDistanceObj
{
    // Both images' merge instantiations order by the float at +4, ascending, strict; an
    // unordered pair compares false and keeps the first list's element in front.
    bool operator<(const SquadMemberDistanceObj& other) const
    {
        return m_distanceSq < other.m_distanceSq;
    }

    SquadMember* m_member;
    float m_distanceSq;
};

} // anonymous namespace

// [INFERRED] local name. Both images retain every delta/product through the
// ordered comparison. PE adds (z*z+y*y)+x*x; ELF adds (y*y+x*x)+z*z.
// Native MSVC x86 is the validated target; portable arms are not ELF/x64 proof.
static bool formationLeaderWithinRange(const Vec3& leader, const Vec3& squad)
{
#if defined(_MSC_VER) && defined(_M_IX86)
    const Vec3* const leaderPosition = &leader;
    const Vec3* const squadPosition = &squad;
    const float threshold = 2500.0f;
    bool within = false;
    __asm {
        mov eax, leaderPosition
        mov edx, squadPosition
        fld dword ptr [eax+8]
        fsub dword ptr [edx+8]
        fmul st(0), st(0)
        fld dword ptr [eax+4]
        fsub dword ptr [edx+4]
        fmul st(0), st(0)
        faddp st(1), st(0)
        fld dword ptr [eax]
        fsub dword ptr [edx]
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
    const long double deltaX = static_cast<long double>(leader.x) - squad.x;
    const long double deltaY = static_cast<long double>(leader.y) - squad.y;
    const long double deltaZ = static_cast<long double>(leader.z) - squad.z;
#if defined(_WIN32)
    return (deltaZ * deltaZ + deltaY * deltaY) + deltaX * deltaX < 2500.0L;
#else
    return (deltaY * deltaY + deltaX * deltaX) + deltaZ * deltaZ < 2500.0L;
#endif
#endif
}

// [INFERRED] local name. Squared length ordered strictly above 0.001f; equality and NaN refuse.
// PE squares the stored components and sums (z*z+y*y)+x*x. ELF first subtracts a zero vector
// (exact) and sums (y*y+x*x)+z*z. Both keep the sum wide through the comparison.
// Native MSVC x86 is the validated target; portable arms are not ELF/x64 proof.
static bool trailPointIsSet(const Vec3& point)
{
#if defined(_MSC_VER) && defined(_M_IX86)
    const Vec3* const p = &point;
    const float threshold = 0.001f;
    bool set = false;
    __asm {
        mov eax, p
        fld dword ptr [eax+8]
        fmul st(0), st(0)
        fld dword ptr [eax+4]
        fmul st(0), st(0)
        faddp st(1), st(0)
        fld dword ptr [eax]
        fmul st(0), st(0)
        faddp st(1), st(0)
        fcomp threshold
        fnstsw ax
        test ah, 41h
        jnz notSet
        mov set, 1
    notSet:
    }
    return set;
#else
    const long double x = point.x;
    const long double y = point.y;
    const long double z = point.z;
#if defined(_WIN32)
    return (z * z + y * y) + x * x > static_cast<long double>(0.001f);
#else
    return (y * y + x * x) + z * z > static_cast<long double>(0.001f);
#endif
#endif
}

// [INFERRED] local name. Squared distance between the anchor and the newest crumb, narrowed
// to float before the caller's strict 36 test. PE subtracts through its float-returning Vec3
// operator (anchor - crumb) and sums (dx*dx+dy*dy)+dz*dz; ELF keeps crumb - anchor wide and
// sums (dy*dy+dx*dx)+dz*dz. The sign of each difference cannot matter once squared.
static float trailCrumbDistanceSq(const Vec3& anchor, const Vec3& crumb)
{
#if defined(_MSC_VER) && defined(_M_IX86)
    const Vec3* const a = &anchor;
    const Vec3* const c = &crumb;
    float deltaX, deltaY, deltaZ, distance;
    __asm {
        mov eax, a
        mov edx, c
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
    const long double dx = static_cast<float>(static_cast<long double>(anchor.x) - crumb.x);
    const long double dy = static_cast<float>(static_cast<long double>(anchor.y) - crumb.y);
    const long double dz = static_cast<float>(static_cast<long double>(anchor.z) - crumb.z);
    return static_cast<float>((dx * dx + dy * dy) + dz * dz);
#else
    const long double dx = static_cast<long double>(crumb.x) - anchor.x;
    const long double dy = static_cast<long double>(crumb.y) - anchor.y;
    const long double dz = static_cast<long double>(crumb.z) - anchor.z;
    return static_cast<float>((dy * dy + dx * dx) + dz * dz);
#endif
#endif
}

// [INFERRED] local name. A claimed member's squared distance to the anchor, stored as the sort
// key. PE spills only dx to float and sums (dz*dz+dy*dy)+dx*dx; ELF keeps all three
// differences wide and sums (dy*dy+dx*dx)+dz*dz. Both narrow the sum to float for the key.
static float trailMemberDistanceSq(const Vec3& position, const Vec3& anchor)
{
#if defined(_MSC_VER) && defined(_M_IX86)
    const Vec3* const p = &position;
    const Vec3* const a = &anchor;
    float deltaX, distance;
    __asm {
        mov eax, p
        mov edx, a
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
        fstp distance
    }
    return distance;
#else
#if defined(_WIN32)
    const long double dx = static_cast<float>(static_cast<long double>(position.x) - anchor.x);
#else
    const long double dx = static_cast<long double>(position.x) - anchor.x;
#endif
    const long double dy = static_cast<long double>(position.y) - anchor.y;
    const long double dz = static_cast<long double>(position.z) - anchor.z;
#if defined(_WIN32)
    return static_cast<float>((dz * dz + dy * dy) + dx * dx);
#else
    return static_cast<float>((dy * dy + dx * dx) + dz * dz);
#endif
#endif
}

// [INFERRED] Runs the formation helpers, then advances a signed six-tick cycle: tick 3
// selects a formation and tick 6 resets the counter and applies it. [PLATFORM] The PE
// inlines selection; ELF calls it. Both use the same integer policy.
void SquadFormation::update(double time)
{
    updateSlotZeroPos();
    updateMovementFlag(false);
    updateFormationMaxSpeed();
    updateFormation();
    updateWaitForSquadMembers(time);
    updateFormationForwardVector();
    updateTrailingPositions();
    updateTrailingSlots();

    if (++m_i88 > 5)
    {
        m_i88 = 0;
        setFormation();
    }
    else if (m_i88 == 3)
        selectFormation();
}

// Selects the line formation unconditionally. The next cycle wrap applies it, replacing
// any shape selected by changeFormation.
void SquadFormation::selectFormation()
{
    m_formation = FormationLine;
}

// ELF 0x1AB470: the anchor's (slot 0) vehicle speed (IAIPlayer +0x54 getCurrentVehicle, its
// information for side m_side + 1, Information +0x38 IPIMobile +0x1C getSpeed, copied) sets the
// forward vector when its squared length exceeds 0.25f (0x49CE84): vec3ToVec2XZ into m_forward,
// then the inlined Vec2 normalisation (left alone within 1.1920929e-07f (0x49CC7C) of unit length,
// zeroed below 1.4210855e-14f (0x49CCA4), else scaled by 1 / sqrt). An empty anchor returns.
void SquadFormation::updateFormationForwardVector()
{
    Slot* const anchor = m_slots[0];
    if (!anchor->hasSquadMemberAssigned)
        return;

    IAIPlayer* const player = m_coordinator->getSquadMember(anchor->m_memberIndex)->getPlayer();
    IAIObject* const vehicle = player->getCurrentVehicle();
    Information* const info = informationManager->getInformation(vehicle->getInformation(m_side + 1));
    const Vec3 speed = info->getMobile()->getSpeed();
    if (!(speed.x * speed.x + speed.y * speed.y + speed.z * speed.z > 0.25f))
        return;

    vec3ToVec2XZ(speed, m_forward);
    const float lengthSq = m_forward.x * m_forward.x + m_forward.y * m_forward.y;
    if (std::fabs(lengthSq - 1.0f) < 1.1920929e-07f)
        return;
    if (std::fabs(lengthSq) < 1.4210855e-14f)
    {
        m_forward.x = 0.0f;
        m_forward.y = 0.0f;
        return;
    }
    const float inverse = 1.0f / std::sqrt(lengthSq);
    m_forward.y = m_forward.y * inverse;
    m_forward.x = m_forward.x * inverse;
}

// Adds a trail position when the anchor moves more than six metres from the last position.
void SquadFormation::updateTrailingPositions()
{
    Slot* const anchor = m_slots[0];
    if (!trailPointIsSet(m_v72))
        return;
    if (!trailPointIsSet(m_trailingPosition))
    {
        m_trailingPosition = m_v72;
        return;
    }
    if (!anchor->hasSquadMemberAssigned)
        return;

    const float distanceSq = trailCrumbDistanceSq(m_v72, m_trailingPosition);
    SquadMember* const member = m_coordinator->getSquadMember(anchor->m_memberIndex);
    (void)member;
    if (!(distanceSq > 36.0f))
        return;

    for (unsigned int index = m_numberOfSlots - 1; index != 0; --index)
    {
        // Two statements: both images reload the vector's storage after the byte store.
        m_trailingPositions[index].first = m_trailingPositions[index - 1].first;
        m_trailingPositions[index].second = m_trailingPositions[index - 1].second;
    }
    m_trailingPositions[0].first = true;
    m_trailingPositions[0].second = m_trailingPosition;
    m_trailingPosition = m_v72;
}

// Reassigns occupied trailing slots according to distance.
void SquadFormation::updateTrailingSlots()
{
    std::list<SquadMemberDistanceObj> members;
    Vec3 position = {};

    for (unsigned int index = 0; index < m_numberOfSlots; ++index)
    {
        Slot* const slot = m_slots[index];
        if (slot->m_i20 != -1 && slot->hasSquadMemberAssigned)
        {
            SquadMember* const member = m_coordinator->getSquadMember(slot->m_memberIndex);
            member->getPosition(position);
            const SquadMemberDistanceObj entry = { member, trailMemberDistanceSq(position, m_v72) };
            members.push_back(entry);
        }
    }

    if (members.empty())
        return;

    members.sort();

    Slot* slot = 0;
    for (unsigned int index = 0; index < m_numberOfSlots; ++index)
    {
        slot = m_slots[index];
        slot->m_i20 = -1;
    }

    int trailIndex = 0;
    for (std::list<SquadMemberDistanceObj>::iterator it = members.begin(); it != members.end(); ++it)
    {
        SquadMember* const member = it->m_member;
        for (unsigned int index = 1; index < m_numberOfSlots; ++index)
        {
            slot = m_slots[index];
            if (slot->m_memberIndex == member->getMemberIndex())
                break;
        }
        slot->m_i20 = trailIndex++;
    }
}

// No bounds, null or occupied test: an empty
// slot answers its stored index, -1 after construction or reset.
int SquadFormation::getSquadMemberIndexAtSlot(int slotIndex)
{
    return m_slots[slotIndex]->m_memberIndex;
}

bool SquadFormation::shouldLeaveSquadLeader()
{
    return m_b84;
}

// (, updateMovementFlag(true) inlined).
// The joined flag is set before the leave flag is cleared, in both images.
void SquadFormation::joinSquadLeader()
{
    m_b85 = true;
    m_b84 = false;
    updateMovementFlag(true);
}

// ELF 0x1AC550 (1248 bytes). The world position of `memberIndex`'s slot: the squad position plus
// the slot offset rotated by the forward vector (normalised when not unit length; the angle is
// acos(y), negated for x > 0). Slot 0 (the leader's) is always taken; another slot only when the
// position is valid for its member and not on water (its trailing fallback is then released),
// otherwise its trailing position - claiming the first free one, and failing that the mission
// target (only for a mission without an object) or the slot-0 member's position. Without a squad
// position the leader's position is used.
bool SquadFormation::getSlotWorldPos(int memberIndex, Vec3& position)
{
    bool found = true;
    Vec3 squad = {};
    m_coordinator->getSquadPosition(squad);
    if ((0.0f - squad.x) * (0.0f - squad.x) + (0.0f - squad.y) * (0.0f - squad.y)
        + (0.0f - squad.z) * (0.0f - squad.z) < 0.001f)
    {
        found = false;
        if (m_coordinator->getSquadLeader()->getPosition(position))
            found = true;
        return found;
    }
    float fx = m_forward.x;
    float fy = m_forward.y;
    for (unsigned int i = 0; i < m_numberOfSlots; ++i)
    {
        Slot* const slot = m_slots[i];
        if (slot->m_memberIndex != memberIndex)
            continue;
        const Vec3 offset = slot->m_offset;
        const float lengthSq = fx * fx + fy * fy;
        if (1.1920929e-07f <= std::fabs(lengthSq - 1.0f))
        {
            if (1.4210855e-14f <= std::fabs(lengthSq))
            {
                const float inverse = 1.0f / std::sqrt(lengthSq);
                fx = fx * inverse;
                fy = inverse * fy;
            }
            else
            {
                fx = 0.0f;
                fy = 0.0f;
            }
        }
        float clamped = -1.0f;
        if (-1.0f <= fy)
        {
            clamped = 1.0f;
            if (fy <= 1.0f)
                clamped = fy;
        }
        float angle = std::acos(clamped);
        if (0.0f < fx)
            angle = -angle;
        const float s = std::sin(angle);
        position.x = squad.x + (offset.x * fy - offset.z * s);
        position.y = offset.y + squad.y;
        position.z = squad.z + offset.x * s + offset.z * fy;
        if (i == 0)
            return true;
        SquadMember* member = m_coordinator->getSquadMember(slot->m_memberIndex);
        if (m_coordinator->isValidPosition(member, position) && !m_coordinator->isPosOnWater(member, position))
        {
            slot->m_i20 = -1;
            return true;
        }
        int trail = slot->m_i20;
        if (trail == -1)
        {
            trail = getFirstFreeTrailingSlot(member);
            if (trail == -1)
            {
                if (m_coordinator->squadHasMission())
                    return m_coordinator->getSquadMissionTargetPos(position);
                member = m_coordinator->getSquadMember(getSquadMemberIndexAtSlot(0));
                if (!member)
                    return false;
                return member->getPosition(position);
            }
            slot->m_i20 = trail;
        }
        position = m_trailingPositions[trail].second;
        return true;
    }
    return found;
}

// ELF 0x1ACA30 (320 bytes). The first trailing position that is set, unclaimed by any slot and
// valid for `member`; -1 when none. (The image also computes, and drops, the distance to it.)
int SquadFormation::getFirstFreeTrailingSlot(SquadMember* member)
{
    for (unsigned int i = 0; i < m_numberOfSlots; ++i)
    {
        const bool assigned = isTrailPosAssigned(static_cast<int>(i));
        const bool valid = m_coordinator->isValidPosition(member, m_trailingPositions[i].second);
        if (m_trailingPositions[i].first && !assigned && valid)
            return static_cast<int>(i);
    }
    return -1;
}

// ELF 0x1ACB70: whether any slot falls back to trailing position `trailIndex`.
bool SquadFormation::isTrailPosAssigned(int trailIndex)
{
    for (unsigned int i = 0; i < m_numberOfSlots; ++i)
    {
        if (m_slots[i]->m_i20 == trailIndex)
            return true;
    }
    return false;
}

// ELF 0x1AB640: the formation leaves the squad leader (+84 set, +85 cleared).
void SquadFormation::leaveSquadLeader()
{
    m_b85 = false;
    m_b84 = true;
    updateMovementFlag(true);
}

// [INFERRED] void result. No slot-vector, leader or member null guards exist.
void SquadFormation::updateFormation()
{
    Slot* slot = m_slots[0];
    SquadMember* const leader = m_coordinator->getSquadLeader();
    const int leaderIndex = leader->getMemberIndex();
    const bool leaderIsBot = leader->isBot();
    bool leaderPresent = false;
    Vec3 squadPosition = {};
    m_coordinator->getSquadPosition(squadPosition);

    unsigned int leaderSlotIndex = 0;
    for (; leaderSlotIndex < m_numberOfSlots; ++leaderSlotIndex)
    {
        slot = m_slots[leaderSlotIndex];
        if (slot->hasSquadMemberAssigned && slot->m_memberIndex == leaderIndex)
        {
            leaderPresent = true;
            break;
        }
    }

    if (leaderPresent)
    {
        if (m_b84)
        {
            if (leaderSlotIndex != 0)
                return;
            for (unsigned int index = m_numberOfSlots - 1; index > 0; --index)
            {
                Slot* const other = m_slots[index];
                if (!other->hasSquadMemberAssigned)
                    continue;
                const int otherIndex = other->m_memberIndex;
                const int oldIndex = slot->m_memberIndex;
                slot->m_b24 = false;
                slot->m_i28 = 0.0f;
                slot->m_memberIndex = otherIndex;
                other->m_b24 = false;
                other->m_i28 = 0.0f;
                other->m_memberIndex = oldIndex;

                // This arm resets trailing positions before either path clear.
                resetTrailingPositions();
                m_coordinator->getSquadMember(slot->m_memberIndex)->setHavePath(false);
                m_coordinator->getSquadMember(other->m_memberIndex)->setHavePath(false);
                return;
            }
            return;
        }

        if (leaderSlotIndex == 0)
            return;
        Vec3 leaderPosition = {};
        leader->getPosition(leaderPosition); // Retail deliberately ignores validity.
        if (!formationLeaderWithinRange(leaderPosition, squadPosition) && !m_b85)
            return;

        m_b85 = false;
        Slot* const zero = m_slots[0];
        const bool oldOccupied = zero->hasSquadMemberAssigned;
        const int oldIndex = zero->m_memberIndex;
        zero->hasSquadMemberAssigned = true;
        zero->m_memberIndex = leaderIndex;
        zero->m_b24 = false;
        zero->m_i28 = 0.0f;
        m_coordinator->getSquadMember(leaderIndex)->setHavePath(false);

        // The old leader-slot pointer is reloaded after that callback.
        slot = m_slots[leaderSlotIndex];
        slot->hasSquadMemberAssigned = oldOccupied;
        slot->m_memberIndex = oldIndex;
        slot->m_b24 = false;
        slot->m_i28 = 0.0f;
        // Even an unoccupied former anchor gets this unchecked member lookup.
        m_coordinator->getSquadMember(oldIndex)->setHavePath(false);
        resetTrailingPositions();
        return;
    }

    Slot* const zero = m_slots[0];
    if (!zero->hasSquadMemberAssigned)
        return;
    if (m_coordinator->getSquadMember(zero->m_memberIndex)->isBot() == leaderIsBot)
        return;

    // Find the first occupied slot whose bot/human kind matches the captured leader.
    // The bound, vector and coordinator are fresh after each member callback.
    for (unsigned int index = 1; index < m_numberOfSlots; ++index)
    {
        Slot* const other = m_slots[index];
        if (!other->hasSquadMemberAssigned ||
            m_coordinator->getSquadMember(other->m_memberIndex)->isBot() != leaderIsBot)
            continue;
        const int oldIndex = zero->m_memberIndex;
        const int otherIndex = other->m_memberIndex;
        other->m_memberIndex = oldIndex;
        other->m_i28 = 0.0f;
        other->m_b24 = false;
        zero->m_memberIndex = otherIndex;
        zero->m_b24 = false;
        zero->m_i28 = 0.0f;
        m_coordinator->getSquadMember(otherIndex)->setHavePath(false);
        resetTrailingPositions();
        return;
    }
}

// ELF 0x1AAFC0. m_b36 (shouldSquadMoveOut) starts true; every occupied slot but the first is
// checked. A slot stops waiting (+0x18 = false, +0x1C = 0.0f) for the squad leader while the
// formation leaves it (m_b84), for a bot, for a member without a position, outside an estimated
// arrival of (150, 1500) (0x49CF9C / 0x49CF98), or when the member is no farther from the squad's
// goto position than the squad itself (both squared distances). Otherwise the squad waits
// (m_b36 = false): a waiting slot whose deadline (float)time has passed stops waiting (and the
// squad moves out unless another slot holds it); a slot without a deadline (< 0.01f, 0x49CB30)
// starts waiting until (float)time + 10 + 10 * random (0x49CC88; AIRandom inlined, 2^-32).
void SquadFormation::updateWaitForSquadMembers(double time)
{
    m_b36 = true;
    Vec3 memberPosition = {};
    Vec3 squadPosition = {};
    for (unsigned int index = 1; index < m_numberOfSlots; ++index)
    {
        Slot* const slot = m_slots[index];
        if (!slot->hasSquadMemberAssigned)
            continue;

        SquadMember* const member = m_coordinator->getSquadMember(slot->m_memberIndex);
        bool wait = false;
        if (!(m_b84 && slot->m_memberIndex == m_coordinator->getSquadLeaderIndex()) && !member->isBot())
        {
            const float arrival = m_coordinator->getSquadMember(slot->m_memberIndex)->getEstimatedArrival();
            const bool hasPosition = member->getPosition(memberPosition);
            m_coordinator->getSquadPosition(squadPosition);
            if (hasPosition && 1500.0f > arrival && arrival > 150.0f)
            {
                const Vec3& goal = m_coordinator->getSquadGotoPos();
                const float mx = memberPosition.x - goal.x;
                const float my = memberPosition.y - goal.y;
                const float mz = memberPosition.z - goal.z;
                const float memberDistanceSq = mx * mx + my * my + mz * mz;
                const Vec3& goal2 = m_coordinator->getSquadGotoPos();
                const float sx = squadPosition.x - goal2.x;
                const float sy = squadPosition.y - goal2.y;
                const float sz = squadPosition.z - goal2.z;
                wait = memberDistanceSq > sx * sx + sy * sy + sz * sz;
            }
        }

        if (!wait)
        {
            slot->m_b24 = false;
            slot->m_i28 = 0.0f;
            continue;
        }

        if (slot->m_b24)
        {
            if (static_cast<float>(time) > slot->m_i28)
                slot->m_b24 = false;
            else
                m_b36 = false;
        }
        else if (0.01f > slot->m_i28)
        {
            m_b36 = false;
            slot->m_b24 = true;
            const double draw = static_cast<double>(AIRandom::m_instance->next()) * 2.3283064e-10;
            slot->m_i28 = static_cast<float>(static_cast<double>(static_cast<float>(time)) + (draw * 10.0 + 10.0));
        }
    }
}

// [INFERRED] local helper name. Preserve leaveFormation's x87 comparison before its
// conditional float store. [PLATFORM] PE spills only dx to float; ELF keeps all three differences
// in x87 registers. The MSVC x86 block avoids modern SSE narrowing of dy/dz and the sum.
static bool nearerFormationMember(const Vec3& position, const Vec3& destination, float& minimum)
{
#if defined(_MSC_VER) && defined(_M_IX86)
    const Vec3* const positionPointer = &position;
    const Vec3* const destinationPointer = &destination;
    float* const minimumPointer = &minimum;
    float deltaX;
    bool nearer = false;
    __asm {
        mov eax, positionPointer
        mov edx, destinationPointer
        fld dword ptr [eax]
        fsub dword ptr [edx]
        fstp deltaX
        fld dword ptr [eax+4]
        fsub dword ptr [edx+4]
        fmul st(0), st(0)
        fld deltaX
        fmul st(0), st(0)
        faddp st(1), st(0)
        fld dword ptr [eax+8]
        fsub dword ptr [edx+8]
        fmul st(0), st(0)
        faddp st(1), st(0)
        mov ecx, minimumPointer
        fcom dword ptr [ecx]
        fnstsw ax
        test ah, 5
        jp notNearer
        fstp dword ptr [ecx]
        mov nearer, 1
        jmp compared
    notNearer:
        fstp st(0)
    compared:
    }
    return nearer;
#else
    // Native long double retains the ELF x87 intermediate width. The separate MSVC
    // x64 port has no x87 inline assembly; it is not validated as retail x86 here.
#if defined(_WIN32)
    const long double dx = static_cast<float>(static_cast<long double>(position.x) - destination.x);
#else
    const long double dx = static_cast<long double>(position.x) - destination.x;
#endif
    const long double dy = static_cast<long double>(position.y) - destination.y;
    const long double dz = static_cast<long double>(position.z) - destination.z;
    const long double distance = (dy * dy + dx * dx) + dz * dz;
    if (distance < minimum)
    {
        minimum = static_cast<float>(distance);
        return true;
    }
    return false;
#endif
}

// Captured unsigned slot count, full signed member
// comparison, no occupied/null/vector-size test. Even an empty slot's -1 can match.
bool SquadFormation::isInFormation(int memberIndex)
{
    const unsigned int count = m_numberOfSlots;
    for (unsigned int i = 0; i < count; ++i)
        if (m_slots[i]->m_memberIndex == memberIndex)
            return true;
    return false;
}

// Preserve offset
// reference-slot and target-vector state. [INFERRED] void return from discarded uses.
void SquadFormation::resetSlot(int slotIndex)
{
    Slot* const slot = m_slots[slotIndex];
    slot->hasSquadMemberAssigned = false;
    slot->m_memberIndex = -1;
    slot->m_i20 = -1;
    slot->m_b24 = false;
    slot->m_i28 = 0.0f;
}

// [INFERRED]
// void return from discarded calls and inconsistent exit registers. Zero slots, null slots
// and bad member indices retain the retail missing guards. A missing member still updates
// movement; duplicate member indices stop at their first slot.
void SquadFormation::leaveFormation(int memberIndex)
{
    unsigned int removed = 0;
    for (; removed < m_numberOfSlots; ++removed)
    {
        if (m_slots[removed]->m_memberIndex == memberIndex)
        {
            resetSlot(static_cast<int>(removed));
            break;
        }
    }

    if (removed < m_numberOfSlots - 1 && m_slots[removed + 1]->hasSquadMemberAssigned)
    {
        unsigned int last = m_numberOfSlots - 1;
        while (last > 0)
        {
            Slot* const lastSlot = m_slots[last];
            if (lastSlot->hasSquadMemberAssigned)
            {
                if (removed == 0 && last > 1)
                {
                    unsigned int selected = last;
                    float minimum = 3.402823466e+38F;
                    Vec3 position = {};
                    for (unsigned int candidate = last; candidate > 0; --candidate)
                    {
                        Slot* const slot = m_slots[candidate];
                        // Retail obtains the member even for an unoccupied candidate.
                        SquadMember* const member = m_coordinator->getSquadMember(slot->m_memberIndex);
                        if (slot->hasSquadMemberAssigned && !member->isSquadLeader() && member->isBot() &&
                            member->getPosition(position))
                        {
                            const Vec3& destination = m_coordinator->getSquadGotoPos();
                            if (nearerFormationMember(position, destination, minimum))
                                selected = candidate;
                        }
                    }

                    Slot* destination = m_slots[0];
                    Slot* source = m_slots[selected];
                    destination->hasSquadMemberAssigned = source->hasSquadMemberAssigned;
                    destination->m_b24 = source->m_b24;
                    destination->m_offset = source->m_offset;
                    destination->m_memberIndex = source->m_memberIndex;
                    destination->m_i20 = source->m_i20;
                    destination->m_i28 = source->m_i28;
                    m_coordinator->getSquadMember(destination->m_memberIndex)->setHavePath(false);
                    resetSlot(static_cast<int>(selected));

                    if (selected < m_numberOfSlots - 1 && m_slots[selected + 1]->hasSquadMemberAssigned)
                    {
                        unsigned int replacement = m_numberOfSlots - 1;
                        while (replacement > 0)
                        {
                            source = m_slots[replacement];
                            if (source->hasSquadMemberAssigned)
                            {
                                destination = m_slots[selected];
                                destination->hasSquadMemberAssigned = source->hasSquadMemberAssigned;
                                destination->m_b24 = source->m_b24;
                                destination->m_offset = source->m_offset;
                                destination->m_memberIndex = source->m_memberIndex;
                                destination->m_i20 = source->m_i20;
                                destination->m_i28 = source->m_i28;
                                m_coordinator->getSquadMember(destination->m_memberIndex)->setHavePath(false);
                                resetSlot(static_cast<int>(replacement));
                                break;
                            }
                            --replacement;
                        }
                    }
                }
                else
                {
                    const int movedMember = lastSlot->m_memberIndex;
                    const int oldIndex = lastSlot->m_i20;
                    const bool oldFlag = lastSlot->m_b24;
                    const float oldValue = lastSlot->m_i28;
                    resetSlot(static_cast<int>(last));
                    Slot* const slot = m_slots[removed];
                    slot->hasSquadMemberAssigned = true;
                    slot->m_memberIndex = movedMember;
                    slot->m_i20 = oldIndex;
                    slot->m_b24 = oldFlag;
                    slot->m_i28 = oldValue;
                    m_coordinator->getSquadMember(movedMember)->setHavePath(false);
                }
                break;
            }
            --last;
        }
    }
    if (removed == 0)
        resetTrailingPositions();
    updateMovementFlag(true);
}

// Slot zero is never considered. Fresh bounds after
// callbacks; occupied candidates only, with -1 and identical-slot results skipped.
void SquadFormation::updateClosestSlots()
{
    for (unsigned int slot = 1; slot < m_numberOfSlots; ++slot)
    {
        if (m_slots[slot]->hasSquadMemberAssigned)
        {
            const int closest = getClosestSlotIndex(static_cast<int>(slot));
            if (closest != -1 && closest != static_cast<int>(slot))
                swapSlots(static_cast<int>(slot), closest);
        }
    }
}

// ELF 0x1AAE70 (336 bytes). The occupied slot (1..) whose member stands closest to the world
// position of `slotIndex`'s slot; -1 when that position is unknown or no member qualifies.
int SquadFormation::getClosestSlotIndex(int slotIndex)
{
    int best = -1;
    float minimum = 3.402823466e+38F;
    Vec3 target = {};
    Vec3 position = {};
    if (!getSlotWorldPos(m_slots[slotIndex]->m_memberIndex, target))
        return best;
    for (unsigned int i = 1; i < m_numberOfSlots; ++i)
    {
        if (!m_slots[i]->hasSquadMemberAssigned)
            continue;
        if (!m_coordinator->getSquadMember(m_slots[i]->m_memberIndex)->getPosition(position))
            continue;
        const float dx = target.x - position.x;
        const float dy = target.y - position.y;
        const float dz = target.z - position.z;
        const float distanceSq = dx * dx + dy * dy + dz * dz;
        if (minimum > distanceSq)
        {
            minimum = distanceSq;
            best = static_cast<int>(i);
        }
    }
    return best;
}

// Keep the real allocation and captured pointers;
// only these three payload fields move, even for identical or aliased slots.
void SquadFormation::swapSlots(int first, int second)
{
    Slot* const a = m_slots[first];
    Slot* const b = m_slots[second];
    Slot* const temporary = new Slot(Slot::SwapTemporary());
    temporary->m_memberIndex = a->m_memberIndex;
    temporary->m_b24 = a->m_b24;
    temporary->m_i28 = a->m_i28;
    a->m_memberIndex = b->m_memberIndex;
    a->m_b24 = b->m_b24;
    a->m_i28 = b->m_i28;
    b->m_memberIndex = temporary->m_memberIndex;
    b->m_b24 = temporary->m_b24;
    b->m_i28 = temporary->m_i28;
    delete temporary;
}

// Allocates formation slots and parallel position state for the configured squad size.

SquadFormation::SquadFormation(SquadCoordinator* coordinator)
    : m_slots()
    , m_coordinator(coordinator)
    , m_formation(0)
    , m_forward()
    , m_i88(0)
    , m_d92(0.0)
    , m_trailingPositions()
    , m_trailingPosition()
{
    m_direction.x = 0.0f;
    m_direction.y = 0.0f;
    m_direction.z = 0.0f;

    m_v60.x = 0.0f;
    m_v60.y = 0.0f;
    m_v60.z = 0.0f;
    m_v72.x = 0.0f;
    m_v72.y = 0.0f;
    m_v72.z = 0.0f;

    m_side = coordinator->getSide();

    m_numberOfSlots = static_cast<unsigned int>(
        static_cast<IAIGame*>(CommonSingletonServer::m_iaiGame)->getMaxNumberOfSquadMembers());

    for (unsigned int i = 0; i < m_numberOfSlots; ++i)
    {
        m_slots.push_back(new Slot());

        Vec3 zero;
        zero.x = 0.0f;
        zero.y = 0.0f;
        zero.z = 0.0f;
        m_trailingPositions.push_back(std::pair<bool, Vec3>(false, zero));
    }

    // FLT_MAX, written as the bit pattern the binary stores.
    m_formationMaxSpeed = 3.402823466e+38F;
    updateFormationMaxSpeed();

    m_formation = FormationLine;
    setFormation();

    m_b36 = false;
    m_forward.x = 0.0f;
    m_forward.y = 1.0f;
    m_v60.x = 0.0f;
    m_v60.y = 0.0f;
    m_v60.z = 0.0f;
    m_b56 = false;
    m_b84 = false;
    m_b85 = false;
    m_i88 = 0;
    m_d92 = 0.0;
    m_v72.x = 0.0f;
    m_v72.y = 0.0f;
    m_v72.z = 0.0f;
}

float SquadFormation::getFormationMaxSpeed()
{
    return m_formationMaxSpeed;
}

// Use the first free slot; neither a duplicate-member
// check nor a full-formation refusal exists. The movement update and path reset run
// even when the scan finds no free slot. PE inlines updateMovementFlag(true).
void SquadFormation::joinFormation(int memberIndex)
{
    for (unsigned int i = 0; i < m_numberOfSlots; ++i)
    {
        Slot* const slot = m_slots[i];
        if (!slot->hasSquadMemberAssigned)
        {
            slot->hasSquadMemberAssigned = true;
            slot->m_memberIndex = memberIndex;
            slot->m_i20 = -1;
            slot->m_b24 = false;
            slot->m_i28 = 0;
            break;
        }
    }

    updateMovementFlag(true);
    m_coordinator->getSquadMember(memberIndex)->setHavePath(false);
}

// An unoccupied slot zero leaves the old movement
// flag untouched. Otherwise clear it before reading the clock, then set it only
// when forced or when both strict thresholds pass. No empty-slot-vector guard.
void SquadFormation::updateMovementFlag(bool force)
{
    if (!m_slots[0]->hasSquadMemberAssigned)
        return;

    m_b56 = false;
    const double now =
        static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();

    bool update = force;
    if (!update)
    {
        const float dx = m_v60.x - m_v72.x;
        const float dy = m_v60.y - m_v72.y;
        const float dz = m_v60.z - m_v72.z;
        // The PE sums z squared, y squared, then x squared.
        update = dz * dz + dy * dy + dx * dx > 4.0f && now > m_d92;
    }

    if (update)
    {
        m_b56 = true;
        m_d92 = now + 0.2;
        m_v60 = m_v72;
    }
}

// Preserve both guards (inlined in the image): the positioned squad's slot-zero world position.
void SquadFormation::updateSlotZeroPos()
{
    if (m_slots[0]->hasSquadMemberAssigned && m_coordinator->squadHasPosition())
        getSlotWorldPos(getSquadMemberIndexAtSlot(0), m_v72);
}

// Only the target-valid flags and slot indices are
// cleared; target coordinates and occupied/member-index fields remain unchanged.
void SquadFormation::resetTrailingPositions()
{
    updateSlotZeroPos();
    m_trailingPosition = m_v72;

    for (unsigned int i = 0; i < m_numberOfSlots; ++i)
    {
        m_trailingPositions[i].first = false;
        m_slots[i]->m_i20 = -1;
    }
}

// ELF 0x1AB310: the formation speed is the slowest occupied slot's vehicle speed, scaled by 0.9
// (0x49CFA0) when more than one slot is occupied, 5.0f (0x49CDAC) when none is. Per occupied slot:
// getSquadMember(slot +4)->getPlayer()->getCurrentVehicle() (IAIPlayer +0x54), its information
// handle for side m_side + 1 (IAIObject +0xB8), the informationManager lookup (inlined: generation
// check, null on mismatch) and Information +0x38 IPIMobile::getMaxSpeed; `flds 0x28; fucompi` keeps
// the smaller value. The image dereferences the looked-up information unchecked.
void SquadFormation::updateFormationMaxSpeed()
{
    int occupied = 0;

    m_formationMaxSpeed = 3.402823466e+38F;

    for (unsigned int i = 0; i < m_numberOfSlots; ++i)
    {
        Slot* const slot = m_slots[i];

        if (slot->hasSquadMemberAssigned)
        {
            ++occupied;
            IAIObject* const vehicle =
                m_coordinator->getSquadMember(slot->m_memberIndex)->getPlayer()->getCurrentVehicle();
            Information* const info = informationManager->getInformation(vehicle->getInformation(m_side + 1));
            const float speed = info->getMobile()->getMaxSpeed();
            if (m_formationMaxSpeed > speed)
                m_formationMaxSpeed = speed;
        }
    }

    if (occupied)
    {
        if (occupied > 1)
            m_formationMaxSpeed = 0.89999998f * m_formationMaxSpeed;
    }
    else
    {
        m_formationMaxSpeed = 5.0f;
    }
}

// Sets the formation-specific direction and dispatches layout. Echelons mirror the
// third direction component; unknown formation values leave the layout unchanged.
void SquadFormation::setFormation()
{
    if (m_formation == FormationWedge)
    {
        m_direction.x = 0.80000001f;
        m_direction.y = 0.0f;
        m_direction.z = -0.60000002f;
        setWedgeFormation();
        return;
    }

    if (m_formation > FormationWedge)
    {
        if (m_formation == FormationEchelonA)
        {
            m_direction.x = 0.60000002f;
            m_direction.y = 0.0f;
            m_direction.z = -0.80000001f;
        }
        else
        {
            if (m_formation != FormationEchelonB)
                return;

            m_direction.x = 0.60000002f;
            m_direction.y = 0.0f;
            m_direction.z = 0.80000001f;
        }

        setEchelonFormation();
        return;
    }

    if (m_formation == FormationLine)
    {
        m_direction.x = 1.0f;
        m_direction.y = 0.0f;
        m_direction.z = 0.0f;
        setLineFormation();
    }
}

// ELF 0x1AB830: slot zero at the origin, later slots alternate +/- direction.x on either side.
void SquadFormation::setLineFormation()
{
    const float spacing = 1.0f;

    bool  positiveSide  = true;
    float offset        = 0.0f;
    int   referenceSlot = 0;

    for (unsigned int i = 0; i < m_numberOfSlots; ++i)
    {
        Slot* const slot = m_slots[i];

        if (i)
        {
            if (positiveSide)
            {
                offset = spacing * m_direction.x;
                slot->m_referenceSlot = referenceSlot;
                positiveSide = false;

                if (i >= 3)
                    ++referenceSlot;
            }
            else
            {
                offset = -spacing * m_direction.x;
                slot->m_referenceSlot = referenceSlot++;
                positiveSide = true;
            }
        }

        slot->m_offset.x = offset;
        slot->m_offset.y = 0.0f;
        slot->m_offset.z = 0.0f;

        // ELF 0x1AB920: an occupied slot is spread by both vehicles' radii, measured from its
        // reference slot: offset = reference offset + offset * (3 * own radius + 3 * reference
        // radius) (0x49CC90 = 3.0f; the image rounds the first product to float, keeps the x
        // product's sum in x87 precision).
        if (slot->hasSquadMemberAssigned)
        {
            const float own = 3.0f * m_coordinator->getSquadMember(slot->m_memberIndex)->getVehicleRadius();
            Slot* const reference = m_slots[slot->m_referenceSlot];
            const float spread = 3.0f * m_coordinator->getSquadMember(reference->m_memberIndex)->getVehicleRadius() + own;
            slot->m_offset.x = slot->m_offset.x * spread + reference->m_offset.x;
            slot->m_offset.y = slot->m_offset.y * spread + reference->m_offset.y;
            slot->m_offset.z = slot->m_offset.z * spread + reference->m_offset.z;
        }
    }
}

// ELF 0x1ABA10: the line's alternating walk, but the offset also steps 1.0f * direction.z
// along z on both sides (+x / -x); the image's Vec3 temporary keeps the last (x, z) between
// slots (slot 0 is the origin). Occupied slots are spread as in setLineFormation.
void SquadFormation::setWedgeFormation()
{
    const float spacing = 1.0f;

    bool  positiveSide  = true;
    Vec3  offset        = {};
    int   referenceSlot = 0;

    for (unsigned int i = 0; i < m_numberOfSlots; ++i)
    {
        Slot* const slot = m_slots[i];

        if (i)
        {
            if (positiveSide)
            {
                offset.x = spacing * m_direction.x;
                offset.z = spacing * m_direction.z;
                slot->m_referenceSlot = referenceSlot;
                positiveSide = false;

                if (i >= 3)
                    ++referenceSlot;
            }
            else
            {
                offset.x = -spacing * m_direction.x;
                offset.z = spacing * m_direction.z;
                slot->m_referenceSlot = referenceSlot++;
                positiveSide = true;
            }
        }

        slot->m_offset = offset;

        if (slot->hasSquadMemberAssigned)
        {
            const float own = 3.0f * m_coordinator->getSquadMember(slot->m_memberIndex)->getVehicleRadius();
            Slot* const reference = m_slots[slot->m_referenceSlot];
            const float spread = 3.0f * m_coordinator->getSquadMember(reference->m_memberIndex)->getVehicleRadius() + own;
            slot->m_offset.x = slot->m_offset.x * spread + reference->m_offset.x;
            slot->m_offset.y = slot->m_offset.y * spread + reference->m_offset.y;
            slot->m_offset.z = slot->m_offset.z * spread + reference->m_offset.z;
        }
    }
}

// ELF 0x1ABBF0: as the wedge, with z mirrored per side: (+x, -z) then (-x, +z).
void SquadFormation::setEchelonFormation()
{
    const float spacing = 1.0f;

    bool  positiveSide  = true;
    Vec3  offset        = {};
    int   referenceSlot = 0;

    for (unsigned int i = 0; i < m_numberOfSlots; ++i)
    {
        Slot* const slot = m_slots[i];

        if (i)
        {
            if (positiveSide)
            {
                offset.x = spacing * m_direction.x;
                offset.z = -spacing * m_direction.z;
                slot->m_referenceSlot = referenceSlot;
                positiveSide = false;

                if (i >= 3)
                    ++referenceSlot;
            }
            else
            {
                offset.x = -spacing * m_direction.x;
                offset.z = spacing * m_direction.z;
                slot->m_referenceSlot = referenceSlot++;
                positiveSide = true;
            }
        }

        slot->m_offset = offset;

        if (slot->hasSquadMemberAssigned)
        {
            const float own = 3.0f * m_coordinator->getSquadMember(slot->m_memberIndex)->getVehicleRadius();
            Slot* const reference = m_slots[slot->m_referenceSlot];
            const float spread = 3.0f * m_coordinator->getSquadMember(reference->m_memberIndex)->getVehicleRadius() + own;
            slot->m_offset.x = slot->m_offset.x * spread + reference->m_offset.x;
            slot->m_offset.y = slot->m_offset.y * spread + reference->m_offset.y;
            slot->m_offset.z = slot->m_offset.z * spread + reference->m_offset.z;
        }
    }
}

// ELF 0x1AC370: every slot is reset, then the first min(members, slots - 1) members that are
// allowed in formation take consecutive slots (occupied, their member index). When there are at
// least as many members as slots, the last slot is left free.
void SquadFormation::reassignSlots()
{
    for (unsigned int i = 0; i < m_numberOfSlots; ++i)
        resetSlot(static_cast<int>(i));
    unsigned int count = static_cast<unsigned int>(m_coordinator->getNumberOfSquadMembers());
    if (count >= m_numberOfSlots)
        count = m_numberOfSlots - 1;
    unsigned int next = 0;
    for (unsigned int i = 0; i < count; ++i)
    {
        SquadMember* const member = m_coordinator->getSquadMember(static_cast<int>(i));
        if (!member->isAllowedInFormation())
            continue;
        Slot* const slot = m_slots[next++];
        slot->hasSquadMemberAssigned = true;
        slot->m_memberIndex = member->getMemberIndex();
    }
}

// ELF 0x1A9E10 (D1, 240 B; recovered on the client track, docs/macos-c-singleplayer.md). Clears
// the coordinator back-pointer, deletes the first m_numberOfSlots slots (the bound is the
// member count fixed at construction, not the vector's size), then empties both vectors.
SquadFormation::~SquadFormation()
{
    m_coordinator = 0;
    for (unsigned int i = 0; i < m_numberOfSlots; ++i)
        delete m_slots[i];
    m_slots.erase(m_slots.begin(), m_slots.end());
    m_trailingPositions.erase(m_trailingPositions.begin(), m_trailingPositions.end());
}

// ELF 0x1A9FB0. Shipped defect: the gap-checking loop has no checks left in this image.
void SquadFormation::debugCheckNoFormationSlotGaps()
{
    unsigned int last = m_numberOfSlots - 1;
    while (last && !m_slots[last]->hasSquadMemberAssigned)
        --last;
    if (last)
        for (unsigned int index = 0; index <= last; ++index) {}
}

// ELF 0x1AA7E0.
bool SquadFormation::hasFormationMoved() { return m_b56; }

// ELF 0x1ABDE0.
void SquadFormation::changeFormation(Formations formation)
{
    m_formation = formation;
    setFormation();
}

// ELF 0x1AC440: both ray endpoints are raised by one metre; the input vectors are untouched.
bool SquadFormation::haveLineOfSightBetween(Vec3& from, Vec3& to)
{
    std::vector<IAIObject*> ignore;
    m_coordinator->getSquadIgnoreList(ignore);
    IAIObjectEnvironment* const environment =
        static_cast<IAIObjectEnvironment*>(CommonSingletonServer::m_iaiObjectEnvironment);
    const Vec3 raisedFrom = {from.x, from.y + 1.0f, from.z};
    const Vec3 raisedTo = {to.x, to.y + 1.0f, to.z};
    bool hit;
    return !environment->collideLineWithWorld(raisedFrom, raisedTo, ignore, false, 0.0f, true, hit);
}

} // namespace ai
} // namespace hfe
} // namespace dice
