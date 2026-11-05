// Revive behaviour (infantry "Revive" slot of aibehaviours.ai): a bot carrying a revive weapon
// (WeaponInfo type 3) looks for man-down friendly soldiers within 40 m and picks the one it can
// reach soonest relative to the man-down time left; ordered targets and human players weigh more.
// Recovered from libaidll.so (Linux 1.5.3153, ia-32); F-urgency package, see
// docs/macos-ai-behaviour-plan.md.

// static init: ELF 0x21C8A0 (__tcf_0 0x21C900, _GLOBAL__I 0x21C930): <iostream>'s Init object only.
#include <iostream>

#include "../AIBot/SquadManager.h"   // requestResponseModifier (ai-bot)
#include "BBRevive.h"

#include "../AIUtils/CustomCurve.h"
#include "../AIBot/BotMain.h"
#include "../AIBot/BotMemory.h"
#include "../AIBotBehaviourPattern/BBPRevive.h"
#include "../AIKnowledge/BFInformationEnvironment.h"
#include "../AIKnowledge/IPIArmament.h"
#include "../AIKnowledge/IPIMobile.h"
#include "../AIKnowledge/IPIPhysical.h"
#include "../AIKnowledge/IPIUnit.h"
#include "../AIKnowledge/Information.h"
#include "../Main/InformationManager.h"
#include "../AIKnowledge/WeaponInfo.h"
#include "../AIUtils/WeaponInfoManager.h"
#include "../AIUtils/checkPointer.h"
#include "../Main/AIDllFrontier.h"
#include "../Main/AISettingsInterfaces.h"
#include "../Main/CommonSingletonServer.h"
#include "../../../AIInterface/BFKnowledge/AIPlayer.h"
#include "../../../AIInterface/BFKnowledge/AIPlayerFactory.h"
#include "../../../AIInterface/BFKnowledge/BFTerrain.h"
#include "../../../../Math/Vec3.h"

#include <algorithm>
#include <cmath>
#include <list>
#include <vector>

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x21AC90 (C2) / 0x21B220 (C1). Base with getMaxNBots; the vector is empty; then +0x30, +0x34,
// +0x38 = checkPointer(new[getNBattleStrengths]) (IAISettings +0x34, called once per array); the
// index, queue and pattern; +0x08 = checkPointer(new unsigned int[getMaxNBots]) (not initialised);
// every bot starts with urgency 0.0f (rodata 0x49CB34) and changed flag false.
BBRevive::BBRevive(unsigned char index, unsigned char queue, BBPRevive* pattern, UrgeCurve* curve, float* modifiers)
    : BotBehaviour(modifiers, curve, static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getMaxNBots()),
      m_friendlyUnits()
{
    m_bestWeapon = checkPointer(new int[static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getNBattleStrengths()]);
    m_rangeSq = checkPointer(new float[static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getNBattleStrengths()]);
    m_bestStrength = checkPointer(new int[static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getNBattleStrengths()]);
    m_index = index;
    m_queue = queue;
    m_pattern = pattern;
    const int count = static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getMaxNBots();
    m_targets = checkPointer(new unsigned int[count]);
    for (int i = 0; i < count; ++i)
    {
        m_urgency[i] = 0.0f;
        m_targetChanged[i] = false;
    }
}

// ELF 0x21B7B0 (D2) / 0x21B860 (D1) / 0x21B910 (D0): delete[] +0x08, +0x30, +0x34, +0x38 when set,
// the vector, then the base.
BBRevive::~BBRevive()
{
    if (m_targets)
        delete[] m_targets;
    if (m_bestWeapon)
        delete[] m_bestWeapon;
    if (m_rangeSq)
        delete[] m_rangeSq;
    if (m_bestStrength)
        delete[] m_bestStrength;
}

// ELF 0x21B9D0 (958 insns).
// Not updating: a target still on the bot's side keeps the urgency while the pattern runs;
// otherwise the changed flag records a non-zero urgency, which drops to 0, and the pattern unInits.
// Updating: candidates are man-down soldiers (type flag 0x200000) with physical and mobile
// plug-ins, not swimming and not under more than 0.75 of water, whose strength type the bot has a
// revive weapon for and whose man-down time left is at least 0.001. Retail: the bot's own
// Information and its armament are used without a null check.
float BBRevive::calculateUrgency(Bot* bot, bool update, float modifier)
{
    BBPRevive* const pattern = static_cast<BBPRevive*>(m_pattern);
    unsigned int* const targets = m_targets;

    if (!update)
    {
        // 0x21B9FA: Information slot +0x44 (getSide) against Bot +0xE0.
        Information* const target = informationManager->getInformation(targets[bot->getBotId()]);
        if (target && target->getSide() == bot->getSide())
        {
            m_targetChanged[bot->getBotId()] = false;
            if (!pattern->isInitiated(bot))
                m_urgency[bot->getBotId()] = 0.0f;
        }
        else
        {
            m_targetChanged[bot->getBotId()] = m_urgency[bot->getBotId()] != 0.0f;
            m_urgency[bot->getBotId()] = 0.0f;
            pattern->unInit(bot);
        }
        return m_urgency[bot->getBotId()];
    }

    const int strengthCount = static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getNBattleStrengths();
    for (int i = 0; i < strengthCount; ++i)
    {
        m_bestWeapon[i] = -1;
        m_bestStrength[i] = 0;
    }
    Information* const self = informationManager->getInformation(bot->getCurrentEquipment());
    IPIArmament* const armament = self->getArmament();
    const Vec3& position = self->getPosition();
    // 0x21BC15: the inlined resize(0) (erase when non-empty, _M_fill_insert of 0 elements else).
    m_friendlyUnits.resize(0);
    static_cast<IAIInformationEnvironment*>(AISingletonServer::m_iaiInformationEnvironment)
        ->getFriendlyUnits(m_friendlyUnits, position, bot, 40.0f);          // slot +0x08, rodata 0x49CFD4
    if (m_friendlyUnits.empty())
    {
        m_targetChanged[bot->getBotId()] = m_urgency[bot->getBotId()] != 0.0f;
        m_urgency[bot->getBotId()] = 0.0f;
        pattern->unInit(bot);
        return 0.0f;
    }

    // 0x21BC7F: a copy of the weapon list (allocate + memmove), freed on every exit.
    const std::vector<unsigned int> weapons = armament->getWeapons();
    const int kit = bot->getPlayer()->getSpawnKit();                        // IAIPlayer +0x44
    for (unsigned int i = 0; i < weapons.size(); ++i)
    {
        WeaponInfo* const weapon = WeaponInfoManager::m_instance->getInformation(weapons[i]);
        if (!weapon || weapon->getWeaponType() != 3)
            continue;
        for (int s = 0; s < strengthCount; ++s)
        {
            if (weapon->getStrength(s) > m_bestStrength[s])
            {
                m_bestWeapon[s] = static_cast<int>(i);
                m_bestStrength[s] = weapon->getStrength(s);
                const float range = weapon->getMaxRange();
                m_rangeSq[s] = range * range;
            }
        }
    }

    float bestScore = 0.0f;                 // -0x1A0
    unsigned int orderedCount = 0;          // -0x1A4
    int bestWeapon = -1;                    // -0x1A8
    unsigned int bestTarget = 0xFFFFFFFFu;  // -0x1AC
    const float maxSpeed = self->getMobile()->getMaxSpeed();
    float bestDistance = 100.0f;            // -0x1B4, rodata 0x49CB5C

    for (std::vector<unsigned int>::iterator it = m_friendlyUnits.begin(); it != m_friendlyUnits.end(); ++it)
    {
        Information* const unit = informationManager->getInformation(*it);
        if (!unit)
            continue;
        IPIPhysical* const physical = unit->getPhysical();
        if (!physical || unit == self || !unit->hasMobile() || !unit->isOfType(ITSoldier))
            continue;
        if (!physical->getManDown())                                        // IPIPhysical +0x5C
            continue;
        if (unit->getSoldierPose() == SPSwimming)
            continue;
        // 0x21C30D: IAITerrain +0x30 getWaterLevel and +0x08 getHeight at the unit's x, z.
        const Vec3& unitPosition = unit->getPosition();
        IAITerrain* const terrain = static_cast<IAITerrain*>(CommonSingletonServer::m_iaiTerrain);
        const float waterLevel = terrain->getWaterLevel(unitPosition.x, unitPosition.z);
        const float height = terrain->getHeight(unitPosition.x, unitPosition.z);
        if (waterLevel + 0.5f > unitPosition.y && waterLevel - height > 0.75f)   // rodata 0x49CB44, 0x49CB54
            continue;
        const int strengthType = physical->getStrengthType();               // IPIPhysical +0x44
        if (m_bestWeapon[strengthType] == -1)
            continue;
        const float timeLeft = physical->getManDownTimeLeft();              // IPIPhysical +0x54
        if (0.001f > timeLeft)                                              // rodata 0x49CC94
            continue;

        Vec3 offset;
        offset.x = unitPosition.x - position.x;
        offset.y = unitPosition.y - position.y;
        offset.z = unitPosition.z - position.z;
        const float distance = std::sqrt(offset.x * offset.x + offset.y * offset.y + offset.z * offset.z);
        float score = distance / maxSpeed / timeLeft;
        if (score > 1.0f)
            score = 1.0f;
        score = 1.0f - score;

        if (bot->isOrderedBehaviour(m_index) && bot->isTakingOrders())
        {
            // 0x21C5D0: the old std::list::size walk, then std::find over the order list; each
            // further one-time order match multiplies by BotMemory +0x08.
            if (bot->getOrderList(m_index)->size() > orderedCount)
            {
                const std::list<unsigned int>::iterator end = bot->getOrderList(m_index)->end();
                if (std::find(bot->getOrderList(m_index)->begin(), bot->getOrderList(m_index)->end(), *it) != end)
                {
                    ++orderedCount;
                    score *= bot->getMemory()->m_float8;
                }
            }
        }

        IPIUnit* const unitPlugIn = unit->getUnit();
        if (unitPlugIn && unitPlugIn->isOccupied())
        {
            // IAIPlayerFactory +0x20 getPlayerFromId, IAIPlayer +0x88 getIsAIPlayer: humans weigh x10.
            IAIPlayer* const occupant = static_cast<IAIPlayerFactory*>(CommonSingletonServer::m_iaiPlayerFactory)
                                            ->getPlayerFromId(unitPlugIn->getOccupyingPlayerId(unit->getSide()));
            if (occupant && !occupant->getIsAIPlayer())
                score *= 10.0f;                                             // rodata 0x49CC88
        }
        if (score > bestScore)
        {
            bestDistance = distance;
            bestScore = score;
            bestWeapon = m_bestWeapon[strengthType];
            bestTarget = *it;
        }
    }

    if (bestTarget == 0xFFFFFFFFu)
    {
        m_urgency[bot->getBotId()] = 0.0f;
        m_targetChanged[bot->getBotId()] = targets[bot->getBotId()] != 0xFFFFFFFFu;
        targets[bot->getBotId()] = 0xFFFFFFFFu;
        pattern->unInit(bot);
        return 0.0f;
    }

    Information* const target = informationManager->getInformation(bestTarget);
    bool humanTarget = false;
    if (target && target->getUnit() && target->getUnit()->isOccupied())
    {
        IAIPlayer* const occupant = static_cast<IAIPlayerFactory*>(CommonSingletonServer::m_iaiPlayerFactory)
                                        ->getPlayerFromId(target->getUnit()->getOccupyingPlayerId(target->getSide()));
        if (occupant && !occupant->getIsAIPlayer())
            humanTarget = true;
    }
    if (bot->isOrderedBehaviour(m_index) && bot->isTakingOrders() && bot->getOrderList(m_index)->empty())
        bestScore *= bot->getMemory()->m_float8;

    // 0x21BF7C: a human closer than 50 m gets 100.0f flat (no modifier); otherwise the declining
    // slope of the best score, x5 for a human and x2 (fadd %st(0), %st) for a bot, times the modifier.
    float urgency;
    if (humanTarget)
    {
        if (50.0f > bestDistance)                                           // rodata 0x49CCA8
            urgency = 100.0f;
        else
            urgency = DecleiningSlopeCurve::calculate(bestScore) * 5.0f * modifier;
    }
    else
    {
        const float slope = DecleiningSlopeCurve::calculate(bestScore);
        urgency = (slope + slope) * modifier;
    }
    m_urgency[bot->getBotId()] = urgency;
    m_targetChanged[bot->getBotId()] = targets[bot->getBotId()] != bestTarget;
    targets[bot->getBotId()] = bestTarget;
    pattern->init(targets[bot->getBotId()], bestWeapon, bot);

    if (kit == 5)
    {
        // 0x21C0BC: m_urgency[id] = SquadManager::requestResponseModifier(SquadManager::m_instance,
        // bot->getSquadMember() (Bot +0x140), bestTarget, RequestAction 1, bot->getSide()) * m_urgency[id]
        // (x87 result, fmuls of the stored urgency).
        m_urgency[bot->getBotId()] = SquadManager::m_instance->requestResponseModifier(
                                         bot->getSquadMember(), bestTarget, 1, bot->getSide())
                                     * m_urgency[bot->getBotId()];
    }
    return m_urgency[bot->getBotId()];
}

// ELF 0x21C810: true when the information's armament (+0x2C) lists a weapon of type 3; the list
// (IPIArmament +0x20 getWeapons) is re-measured on every pass.
bool BBRevive::isAplicable(Information* information, Bot*)
{
    IPIArmament* const armament = information->getArmament();
    if (armament)
    {
        std::vector<unsigned int>& weapons = armament->getWeapons();
        for (unsigned int i = 0; i < weapons.size(); ++i)
        {
            WeaponInfo* const weapon = WeaponInfoManager::m_instance->getInformation(weapons[i]);
            if (weapon && weapon->getWeaponType() == 3)
                return true;
        }
    }
    return false;
}

} // namespace ai
} // namespace hfe
} // namespace dice
