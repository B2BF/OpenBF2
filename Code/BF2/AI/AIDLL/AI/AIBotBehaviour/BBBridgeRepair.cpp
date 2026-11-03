// Bridge-repair behaviour (infantry "BridgeRepair" slot of aibehaviours.ai): an engineer (spawn kit 4)
// picks the closest destroyed bridge segment and a valid standing point at one of its two ends.
// Recovered from libaidll.so (Linux 1.5.3153, ia-32); F-urgency package, see
// docs/macos-ai-behaviour-plan.md.

// static init: ELF 0x1FED10 (__tcf_0 0x1FED70, _GLOBAL__I 0x1FEDA0): <iostream>'s Init object only.
#include <iostream>

#include "../AIBot/SquadManager.h"   // requestResponseModifier (ai-bot)
#include "BBBridgeRepair.h"

#include "../AIBot/BotMain.h"
#include "../AIBot/SquadCoordinator.h"
#include "../AIBot/SquadMember.h"
#include "../AIBotBehaviourPattern/BBPBridgeRepair.h"
#include "../AIBotBehaviourPattern/PlanGenerator.h"
#include "../AIKnowledge/IPIArmament.h"
#include "../AIKnowledge/IPIPhysical.h"
#include "../AIKnowledge/Information.h"
#include "../AIKnowledge/InformationManager.h"
#include "../AIKnowledge/WeaponInfo.h"
#include "../AIKnowledge/WeaponInfoManager.h"
#include "../AIStrategicEntity/AIStrategicObjectManager.h"
#include "../AIUtils/checkPointer.h"
#include "../Main/AIDllFrontier.h"
#include "../Main/AISettingsInterfaces.h"
#include "../Main/CommonSingletonServer.h"
#include "../../../AIInterface/BFKnowledge/AIPlayer.h"
#include "../../../AIInterface/BFKnowledge/IAIObject.h"
#include "../../../AIInterface/BFKnowledge/SoldierPose.h"
#include "../../../../Math/Vec3.h"

#include <vector>

namespace dice {
namespace hfe {
namespace ai {

namespace {

// Narrow stops for callees that the tree does not declare (or declares with a different
// signature). Each is a one-line swap once the owner declares the method.

// ai-bot: _ZN4dice3hfe2ai16SquadCoordinator17isRepairRequestedEv (ELF 0x1A49F0, PLT 0x17BB30);
// call: squad->isRepairRequested(). SquadCoordinator.h names the member (+172 m_b172) only.
bool squadIsRepairRequested(SquadCoordinator* squad)
{
    return squad->isRepairRequested();
}

// ai-bot: _ZN4dice3hfe2ai16SquadCoordinator19getRepairRequestPosEv (ELF 0x1A4A00, PLT 0x17DB10);
// call: squad->getRepairRequestPos(); the caller reads three floats through the returned address (+176).
const Vec3& squadGetRepairRequestPos(SquadCoordinator* squad)
{
    return squad->getRepairRequestPos();
}

// ai-bot: _ZN4dice3hfe2ai12SquadManager23requestResponseModifierEPNS1_11SquadMemberEjNS3_13RequestActionEi
// (ELF 0x1B0390, PLT 0x17A880); call: SquadManager::m_instance->requestResponseModifier(
// bot->getSquadMember(), segmentHandle, SquadMember::RequestAction(2), bot->getSide()); float in ST0.
float squadManagerRequestResponseModifier(SquadMember* member, unsigned int handle, int action, int side)
{
    return SquadManager::m_instance->requestResponseModifier(member, handle, action, side);
}

// ai-strategic: _ZN4dice3hfe2ai24AIStrategicObjectManager32getClosestDestroyedBridgeSegmentENS0_4Vec3EbiRS3_Rf
// (ELF 0x349ED0, PLT 0x185180): the image returns the segment's IAIObject pointer, and this caller
// calls slot +0xD0 (IAIObject::getHandle) on it.
IAIObject* strategicClosestDestroyedBridgeSegment(AIStrategicObjectManager* manager, Vec3 position,
                                                 bool repairRequested, int side, Vec3& direction,
                                                 float& distance)
{
    return manager->getClosestDestroyedBridgeSegment(position, repairRequested, side, direction, distance);
}

inline float lengthSquared(float x, float y, float z)
{
    return x * x + y * y + z * z;
}

} // namespace

// ELF 0x1FD180 (C2) / 0x1FD6C0 (C1). The base gets getMaxNBots (IAISettings slot 0); the empty
// vector at +0x24 is zeroed; three getNBattleStrengths-sized (slot +0x34) arrays go to +0x30/+0x34/
// +0x38 (checkPointer'd, not initialised) before the index/queue/pattern stores; a getMaxNBots
// array goes to +0x08 and every base urgency (+0x10) is set to 0.0f (rodata 0x49CB34).
BBBridgeRepair::BBBridgeRepair(unsigned char index, unsigned char queue, PlanGenerator* pattern, UrgeCurve* curve,
                               float* modifiers)
    : BotBehaviour(modifiers, curve, static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getMaxNBots())
{
    m_unknown30 = checkPointer(new int[static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getNBattleStrengths()]);
    m_unknown34 = checkPointer(new int[static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getNBattleStrengths()]);
    m_unknown38 = checkPointer(new int[static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getNBattleStrengths()]);
    m_index = index;
    m_queue = queue;
    m_pattern = pattern;
    const int count = static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getMaxNBots();
    m_targets = checkPointer(new unsigned int[count]);
    for (int i = 0; i < count; ++i)
        m_urgency[i] = 0.0f;
}

// ELF 0x1FDC00 (D2) / 0x1FDCB0 (D1) / 0x1FDD60 (D0): delete[] +0x08, +0x30, +0x34, +0x38 when set,
// then the vector at +0x24 (default_alloc deallocate) and the base.
BBBridgeRepair::~BBBridgeRepair()
{
    if (m_targets)
        delete[] m_targets;
    if (m_unknown30)
        delete[] m_unknown30;
    if (m_unknown34)
        delete[] m_unknown34;
    if (m_unknown38)
        delete[] m_unknown38;
}

// ELF 0x1FE2E0.
float BBBridgeRepair::calculateUrgency(Bot* bot, bool update, float urgencyScale)
{
    unsigned int* const segmentHandles = m_targets;
    PlanGenerator* const pattern = m_pattern;

    if (!update)
    {
        // 0x1FE304..0x1FE460: keep the current choice while its segment Information exists.
        if (informationManager->getInformation(segmentHandles[bot->getBotId()]))
        {
            m_targetChanged[bot->getBotId()] = false;
            if (!pattern->isInitiated(bot))   // PlanGenerator slot +0x0C
                m_urgency[bot->getBotId()] = 0.0f;
        }
        else
        {
            // 0x1FE3C7: fucompi against 0.0f, setp|setne (unordered counts as changed).
            m_targetChanged[bot->getBotId()] = m_urgency[bot->getBotId()] != 0.0f;
            m_urgency[bot->getBotId()] = 0.0f;
            pattern->unInit(bot);   // PlanGenerator slot +0x10
        }
        return m_urgency[bot->getBotId()];
    }

    Information* const info = informationManager->getInformation(bot->getCurrentEquipment());   // Bot +0xD8
    const unsigned int botId = bot->getBotId();
    const Vec3 botPosition = info->getPosition();   // Information slot +0x30
    int weaponIndex = -1;
    // 0x1FE4EF: retail quirk, getNBattleStrengths (IAISettings +0x34) is called and its result dropped.
    static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getNBattleStrengths();

    // 0x1FE508: virtual isApplicable (slot +0x34); then the soldier (0x200000) or 0x40 type flag.
    if (isApplicable(info, bot) &&
        ((info->getObjectTypeFlags() & 0x200000) || (info->getObjectTypeFlags() & 0x40)))
    {
        if ((info->getObjectTypeFlags() & 0x200000) && info->getSoldierPose() == SPSwimming)
        {
            // 0x1FEBF0
            m_targetChanged[botId] = true;
            pattern->unInit(bot);
            m_urgency[botId] = 0.0f;
            return 0.0f;
        }

        // 0x1FE54B: the soldier's own Information (side from Bot +0xE0 first), its weapon list
        // copied, and the index of the first weapon of type 1 (-1 when none).
        const int side = bot->getSide();
        Information* const soldierInfo =
            informationManager->getInformation(bot->getPlayer()->getSoldier()->getInformation(side));
        std::vector<unsigned int> weapons = soldierInfo->getArmament()->getWeapons();
        for (unsigned int i = 0; i < weapons.size(); ++i)
        {
            WeaponInfo* const weapon = WeaponInfoManager::m_instance->getInformation(weapons[i]);
            if (weapon && weapon->getWeaponType() == 1)
            {
                weaponIndex = i;
                break;
            }
        }

        SquadMember* const member = bot->getSquadMember();   // Bot +0x140
        SquadCoordinator* const squad = member->getSquad();
        const bool repairRequested = squadIsRepairRequested(squad);
        Vec3 direction = {0.0f, 0.0f, 0.0f};
        bool nearRequest = false;
        Vec3 standPosition = {0.0f, 0.0f, 0.0f};
        Vec3 segmentEnd = {0.0f, 0.0f, 0.0f};
        float distance;
        const int segmentSide = bot->getSide();
        IAIObject* const segment = strategicClosestDestroyedBridgeSegment(
            AIStrategicObjectManager::getInstance(), info->getPosition(), repairRequested, segmentSide, direction,
            distance);

        // 0x1FE784: continue only when |direction|^2 > 0.01f (rodata 0x49CB30; ja).
        if (segment && lengthSquared(direction.x, direction.y, direction.z) > 0.01f)
        {
            const unsigned int segmentHandle = segment->getHandle(1);   // IAIObject +0xD0
            // 0x1FE85F: retail quirk, a stale handle dereferences a null Information.
            const Vec3 segmentPosition = informationManager->getInformation(segmentHandle)->getPosition();
            if (repairRequested)
            {
                const Vec3& requestPosition = squadGetRepairRequestPos(squad);
                // 0x1FEBAE: rodata 0x49CF6C 2500.0f.
                if (lengthSquared(requestPosition.x - segmentPosition.x, requestPosition.y - segmentPosition.y,
                                  requestPosition.z - segmentPosition.z) < 2500.0f)
                    nearRequest = true;
            }
            // 0x1FE889: fcmove picks 10000.0f (rodata 0x49F4C0) near a request, else 2500.0f;
            // jbe fails when limit <= distance (and when unordered).
            const float limit = nearRequest ? 10000.0f : 2500.0f;
            if (distance < limit && segmentHandle != 0xFFFFFFFFu)
            {
                standPosition = segmentPosition;
                if (getBridgeCoordinate(segmentHandle, direction, bot, botPosition, standPosition, segmentEnd))
                {
                    // 0x1FE985: fmuls rodata 0x49CC8C 20.0f.
                    m_urgency[botId] = urgencyScale * 20.0f;
                    m_targetChanged[botId] = segmentHandles[botId] != segmentHandle;
                    segmentHandles[botId] = segmentHandle;
                    static_cast<BBPBridgeRepair*>(pattern)->init(segmentHandles[botId], weaponIndex, direction, bot,
                                                                 nearRequest, standPosition, segmentEnd);
                    float* const urgency = m_urgency;
                    const int responseSide = bot->getSide();
                    urgency[botId] = squadManagerRequestResponseModifier(bot->getSquadMember(), segmentHandle, 2,
                                                                         responseSide) *
                                     urgency[botId];
                    if (nearRequest)
                        m_urgency[botId] *= 2.0f;   // 0x1FEAF0: fadd %st(0),%st
                    return m_urgency[bot->getBotId()];
                }
            }
        }

        // 0x1FE78E (the copied weapon list is freed after this)
        m_urgency[botId] = 0.0f;
        m_targetChanged[botId] = segmentHandles[botId] != 0xFFFFFFFFu;
        segmentHandles[botId] = 0xFFFFFFFFu;
        pattern->unInit(bot);
        return 0.0f;
    }

    // 0x1FEC3B
    m_urgency[botId] = 0.0f;
    m_targetChanged[botId] = segmentHandles[botId] != 0xFFFFFFFFu;
    segmentHandles[botId] = 0xFFFFFFFFu;
    pattern->unInit(bot);
    return 0.0f;
}

// ELF 0x1FECB0: bot->getPlayer() (+0xBC) alive (+0x24), not getManDown (+0x1B0), and the
// player's getSpawnKit (+0x44) == 4.
bool BBBridgeRepair::isApplicable(Information*, Bot* bot)
{
    if (!bot->getPlayer()->getIsAlive())
        return false;
    if (bot->getManDown())
        return false;
    return bot->getPlayer()->getSpawnKit() == 4;
}

// ELF 0x1FDE20: the two candidate ends of the segment are bridgePosition +/- direction * radius
// (segment Information's IPIPhysical::getRadius, slot +0x2C; a stale handle dereferences null, retail
// quirk). Each end is raised by 1.5f (rodata 0x49CDC0) and stepped along its direction up to ten
// times until the squad accepts it; the end closer to botPosition wins. bridgePosition receives the
// stand point, segmentEnd the unraised end.
bool BBBridgeRepair::getBridgeCoordinate(unsigned int segmentHandle, Vec3 direction, Bot* bot, Vec3 botPosition,
                                         Vec3& bridgePosition, Vec3& segmentEnd)
{
    bot->getBotId();   // 0x1FDE5E: result unused (retail quirk)
    const Vec3 forward = direction;
    const Vec3 backward = {-forward.x, -forward.y, -forward.z};   // xorl $0x80000000 per component
    const float radius = informationManager->getInformation(segmentHandle)->getPhysical()->getRadius();

    Vec3 forwardEnd = {forward.x * radius + bridgePosition.x, forward.y * radius + bridgePosition.y,
                       forward.z * radius + bridgePosition.z};
    Vec3 backwardEnd = {backward.x * radius + bridgePosition.x, backward.y * radius + bridgePosition.y,
                        backward.z * radius + bridgePosition.z};
    const Vec3 forwardStart = forwardEnd;
    const Vec3 backwardStart = backwardEnd;

    SquadMember* const member = bot->getSquadMember();   // Bot +0x140
    SquadCoordinator* const squad = member->getSquad();
    forwardEnd.y += 1.5f;
    backwardEnd.y += 1.5f;

    bool forwardValid = false;
    int tries = 0;
    do
    {
        if (squad->isValidPosition(member, forwardEnd))
            forwardValid = true;
        else
        {
            ++tries;
            forwardEnd.x = forward.x + forwardEnd.x;
            forwardEnd.y = forward.y + forwardEnd.y;
            forwardEnd.z = forward.z + forwardEnd.z;
        }
    }
    while (tries < 10 && !forwardValid);   // 0x1FDFFB: cmpl $9; jg

    bool backwardValid = false;
    tries = 0;
    do
    {
        if (squad->isValidPosition(member, backwardEnd))
            backwardValid = true;
        else
        {
            ++tries;
            backwardEnd.x = backward.x + backwardEnd.x;
            backwardEnd.y = backward.y + backwardEnd.y;
            backwardEnd.z = backward.z + backwardEnd.z;
        }
    }
    while (tries < 10 && !backwardValid);

    if (forwardValid)
    {
        // 0x1FE073: with both valid, the backward end wins when its squared distance to
        // botPosition is <= the forward one (fucompi; jbe at 0x1FE154) [INFERRED operand order,
        // read from the Ghidra decompile].
        if (backwardValid &&
            lengthSquared(backwardEnd.x - botPosition.x, backwardEnd.y - botPosition.y, backwardEnd.z - botPosition.z) <=
                lengthSquared(forwardEnd.x - botPosition.x, forwardEnd.y - botPosition.y,
                              forwardEnd.z - botPosition.z))
        {
            bridgePosition = backwardEnd;
            segmentEnd = backwardStart;
            return true;
        }
        bridgePosition = forwardEnd;
        segmentEnd = forwardStart;
        return true;
    }
    if (!backwardValid)
        return false;
    bridgePosition = backwardEnd;
    segmentEnd = backwardStart;
    return true;
}

} // namespace ai
} // namespace hfe
} // namespace dice
