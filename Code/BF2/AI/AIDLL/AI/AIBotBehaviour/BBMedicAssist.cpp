// Medic/support assist behaviour (infantry "MedicAssist" slot of aibehaviours.ai). A medic
// (IAIPlayer::getSpawnKit 5) heals and an engineer (kit 4) repairs the friendly unit with the best
// strength / (curve(1 - health) * distance) score; a support soldier (kit 3) hands out ammunition
// (calculateGiveAmmoUrgency). The chosen target goes to the pattern through BBPMedicAssist::init.
// Recovered from libaidll.so (Linux 1.5.3153, ia-32); F-urgency package, see
// docs/macos-ai-behaviour-plan.md.
//
// Float constants (rodata): 0x49CB34 0.0f, 0x49CB2C 1.0f, 0x49F818 75.0f, 0x49CB54 0.75f,
// 0x49F81C 0.99f, 0x49F580 0.7071f, 0x49CB5C 100.0f, 0x49CCA8 50.0f, 0x49CC88 10.0f,
// 0x49CDC0 1.5f, 0x49CC90 3.0f, 0x49CC84 4.0f, 0x49F820 0.49f, 0x49F824 0.74f, 0x49CF6C 2500.0f,
// 0x49F828 0.9 (double, fldl).

// static init: ELF 0x219240 (__tcf_0 0x2192A0, _GLOBAL__I 0x2192D0): <iostream>'s Init object only.
#include <iostream>

#include "../AIBot/SquadManager.h"   // requestResponseModifier (ai-bot)
#include "BBMedicAssist.h"

#include "CustomCurve.h"
#include "../AIBot/BotMain.h"
#include "../AIBot/BotMemory.h"
#include "../AIBot/CommunicationAgent.h"
#include "../AIBot/SquadCoordinator.h"
#include "../AIBot/SquadMember.h"
#include "../AIBotBehaviourPattern/BBPMedicAssist.h"
#include "../AIBotBehaviourPattern/PlanGenerator.h"
#include "../AIKnowledge/BFInformationEnvironment.h"
#include "../AIKnowledge/IPIArmament.h"
#include "../AIKnowledge/IPIMobile.h"
#include "../AIKnowledge/IPIPhysical.h"
#include "../AIKnowledge/IPIUnit.h"
#include "../AIKnowledge/Information.h"
#include "../AIKnowledge/InformationManager.h"
#include "../AIKnowledge/WeaponInfo.h"
#include "../AIKnowledge/WeaponInfoManager.h"
#include "../AIUtils/checkPointer.h"
#include "../Main/AIDllFrontier.h"
#include "../Main/AISettingsInterfaces.h"
#include "../Main/CommonSingletonServer.h"
#include "../Main/IBotManager.h"
#include "../../../AIInterface/BFKnowledge/AIPlayer.h"
#include "../../../AIInterface/BFKnowledge/AIPlayerFactory.h"
#include "../../../AIInterface/BFKnowledge/BFTerrain.h"
#include "../../../../Math/Mat4.h"
#include "../../../../Math/Vec3.h"

#include <algorithm>
#include <cmath>
#include <list>

namespace dice {
namespace hfe {
namespace ai {

namespace {

// Narrow stops for callees the tree does not declare yet (owner ai-bot). Each is a one-line swap
// to the real call once the owner declares it.

// ELF 0x1C47F0 _ZNK4dice3hfe2ai18CommunicationAgent18getReceivedMessageERNS1_18AIRadioMessageInfoENS1_18AIRadioMessageTypeEi
// (PLT 0x185930). Swap to: return agent->getReceivedMessage(info, type, playerId);
bool communicationAgentGetReceivedMessage(const CommunicationAgent* agent, AIRadioMessageInfo& info,
                                          AIRadioMessageType type, int playerId)
{
    return agent->getReceivedMessage(info, type, playerId);
}

// ELF 0x1C4A00 _ZN4dice3hfe2ai18CommunicationAgent21receivedMessageExistsENS1_18AIRadioMessageTypeEi
// (PLT 0x17E1F0). Swap to: return agent->receivedMessageExists(type, playerId);
bool communicationAgentReceivedMessageExists(CommunicationAgent* agent, AIRadioMessageType type,
                                             int playerId)
{
    return agent->receivedMessageExists(type, playerId);
}

// ELF _ZN4dice3hfe2ai12SquadManager23requestResponseModifierEPNS1_11SquadMemberEjNS3_13RequestActionEi
// (PLT 0x17A880): float SquadManager::requestResponseModifier(SquadMember*, unsigned int target,
// SquadManager::RequestAction, int side), called on SquadManager::m_instance; the x87 result is
// multiplied into the urgency. Swap to:
//     return SquadManager::m_instance->requestResponseModifier(member, target,
//                static_cast<SquadManager::RequestAction>(action), side);
float squadManagerRequestResponseModifier(SquadMember* member, unsigned int target, int action, int side)
{
    return SquadManager::m_instance->requestResponseModifier(member, target, action, side);
}

const unsigned int kNoTarget = 0xFFFFFFFFu;

inline IAIPlayer* playerFromId(int id)
{
    return static_cast<IAIPlayerFactory*>(CommonSingletonServer::m_iaiPlayerFactory)->getPlayerFromId(id);
}

// The inlined Vec3 subtraction followed by the x87 length (fsqrt, sqrtf on NaN).
inline float distance(const Vec3& a, const Vec3& b)
{
    const float dx = b.x - a.x;
    const float dy = b.y - a.y;
    const float dz = b.z - a.z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

inline float distanceSqr(const Vec3& a, const Vec3& b)
{
    const float dx = b.x - a.x;
    const float dy = b.y - a.y;
    const float dz = b.z - a.z;
    return dx * dx + dy * dy + dz * dz;
}

} // namespace

// ELF 0x215CE0 (C2) / 0x216220 (C1). The base gets getMaxNBots (IAISettings slot 0); the
// vector at +0x24 is zeroed (three words); +0x30/+0x34/+0x38 are new[getNBattleStrengths]
// (slot +0x34, called once per array, each checkPointer'd); then index/queue/pattern; +0x08 is
// new[getMaxNBots] (not initialised) and every base urgency is 0.0f (rodata 0x49CB34).
BBMedicAssist::BBMedicAssist(unsigned char index, unsigned char queue, PlanGenerator* pattern, UrgeCurve* curve,
                             float* modifiers)
    : BotBehaviour(modifiers, curve, static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getMaxNBots())
{
    IAISettings* settings = static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings);
    m_bestWeaponIndex = checkPointer(new int[settings->getNBattleStrengths()]);
    m_bestWeaponRange = checkPointer(new float[settings->getNBattleStrengths()]);
    m_bestWeaponStrength = checkPointer(new int[settings->getNBattleStrengths()]);
    m_index = index;
    m_queue = queue;
    m_pattern = pattern;
    const int count = settings->getMaxNBots();
    m_targets = checkPointer(new unsigned int[count]);   // per-bot chosen target handle
    for (int i = 0; i < count; ++i)
        m_urgency[i] = 0.0f;
}

// ELF 0x216760 (D2) / 0x216810 (D1) / 0x2168C0 (D0): delete[] +0x08, +0x30, +0x34, +0x38 when
// set, then the vector's storage, then the base.
BBMedicAssist::~BBMedicAssist()
{
    if (m_targets)
        delete[] m_targets;
    if (m_bestWeaponIndex)
        delete[] m_bestWeaponIndex;
    if (m_bestWeaponRange)
        delete[] m_bestWeaponRange;
    if (m_bestWeaponStrength)
        delete[] m_bestWeaponStrength;
}

// ELF 0x216980. Primary object of `info` for `side`, then its secondary objects: the first unit
// that is occupied by a player whom IAIPlayerFactory::getPlayerFromId (+0x20) knows and whose
// getIsAIPlayer (+0x88) is false answers true.
bool BBMedicAssist::isHumanInTarget(Information* info, int side)
{
    bool human = false;
    if (info)
    {
        Information* primary = informationManager->getInformation(info->getPrimaryObject(side));
        if (primary)
        {
            IPIUnit* unit = primary->getUnit();
            if (unit && unit->isOccupied())
            {
                IAIPlayer* player = playerFromId(unit->getOccupyingPlayerId(side));
                if (player && !player->getIsAIPlayer())
                    return true;
            }
            int iterator;
            for (Information* secondary =
                     informationManager->getInformation(primary->getFirstSecondaryObject(iterator, side));
                 secondary;
                 secondary = informationManager->getInformation(primary->getSecondaryObject(iterator, side)))
            {
                unit = secondary->getUnit();
                if (unit && unit->isOccupied())
                {
                    IAIPlayer* player = playerFromId(unit->getOccupyingPlayerId(side));
                    if (player && !player->getIsAIPlayer())
                    {
                        human = true;
                        break;
                    }
                }
            }
        }
    }
    return human;
}

// ELF 0x216B40.
float BBMedicAssist::calculateUrgency(Bot* bot, bool recalculate, float urgencyScale)
{
    unsigned int* targets = m_targets;
    PlanGenerator* pattern = m_pattern;

    if (!recalculate)
    {
        // Keep the current target while it is still on the bot's side (Information slot +0x44
        // getSide vs Bot +0xE0); a pattern that is no longer initiated (+0x0C) drops the urgency.
        Information* target = informationManager->getInformation(targets[bot->getBotId()]);
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

    // ELF 0x216CE9: cmpl $0x3 on IAIPlayer::getSpawnKit (+0x44) - support soldiers hand out ammo.
    if (bot->getPlayer()->getSpawnKit() == 3)
        return calculateGiveAmmoUrgency(bot, urgencyScale);

    const int nStrengths = static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getNBattleStrengths();
    int bestPlayerId = -1;      // -0x198
    int bestWeaponIndex = -1;   // -0x194
    Vec3 normal = {0.0f, 0.0f, 0.0f};
    const int kit = bot->getPlayer()->getSpawnKit();
    Information* self = informationManager->getInformation(bot->getCurrentEquipment());
    const Vec3& selfPosition = self->getPosition();   // retail: no null check

    for (int i = 0; i < nStrengths; ++i)
    {
        m_bestWeaponIndex[i] = -1;
        m_bestWeaponStrength[i] = 0;
    }

    IPIArmament* armament = self->getArmament();
    IPIMobile* selfMobile = self->getMobile();
    int vehicleNumber = -2;
    if (selfMobile)
    {
        vehicleNumber = selfMobile->getVehicleNumber();
        self->getUnit()->getEquipmentType();   // ELF 0x216E1A: result unused
    }

    // The bot's own healing/repair weapons (WeaponInfo::getWeaponType 1): per strength type the
    // strongest one, its index and its max range.
    std::vector<unsigned int> weapons = armament->getWeapons();
    for (unsigned int w = 0; w < weapons.size(); ++w)
    {
        WeaponInfo* weapon = WeaponInfoManager::m_instance->getInformation(weapons[w]);
        if (!weapon || weapon->getWeaponType() != 1)
            continue;
        for (int i = 0; i < nStrengths; ++i)
        {
            if (weapon->getStrength(i) > m_bestWeaponStrength[i])
            {
                m_bestWeaponIndex[i] = static_cast<int>(w);
                m_bestWeaponStrength[i] = weapon->getStrength(i);
                m_bestWeaponRange[i] = weapon->getMaxRange();
            }
        }
    }

    float bestScore = 0.0f;
    // ELF -0x188: the same stack slot is the new size of the resize below and the counter of
    // ordered targets met in the loop. [INFERRED] one source variable.
    unsigned int orderedCount = 0;
    // ELF 0x216FA5..0x216FC9 / 0x217E18: size > 0 -> erase(begin, end), else
    // _M_fill_insert(end, 0, 0): the inlined vector::resize(0).
    m_friendlies.resize(orderedCount);
    // IAIInformationEnvironment slot +0x0C: getFriendlyAndNeutralUnits(out, position, bot, 75.0f).
    static_cast<IAIInformationEnvironment*>(AISingletonServer::m_iaiInformationEnvironment)
        ->getFriendlyAndNeutralUnits(m_friendlies, selfPosition, bot, 75.0f);

    unsigned int bestTarget = kNoTarget;   // -0x1BC
    CommunicationAgent* agent = bot->getCommunicationAgent();
    const std::list<AIRadioMessageInfo>& medicCalls = agent->getReceivedMessages(static_cast<AIRadioMessageType>(3));
    const std::list<AIRadioMessageInfo>& repairCalls = agent->getReceivedMessages(static_cast<AIRadioMessageType>(4));
    bool canAnswerMedicCall = !medicCalls.empty();
    bool canAnswerRepairCall = !repairCalls.empty();
    if (bot->isAirBorn())
    {
        canAnswerMedicCall = false;
        canAnswerRepairCall = false;
    }
    // ELF 0x21708D: a 1.0f local that multiplies every score (fmuls -0x1A0).
    const float scoreModifier = 1.0f;

    // Squad queries whose results are not used (ELF 0x2170C5..0x21710C).
    Vec3 squadPosition = {0.0f, 0.0f, 0.0f};
    SquadCoordinator* squad = bot->getSquadMember()->getSquad();
    squad->getSquadPosition(squadPosition);
    Vec3 squadGotoPosition = squad->getSquadGotoPos();
    (void)squadGotoPosition;
    squad->getSquadLeader();

    float bestDistance = 100.0f;
    bool bestIsHuman = false;       // -0x1DA
    bool bestHasNoBot = false;      // -0x1DB: set by any 3x candidate, not only the best (retail)

    for (std::vector<unsigned int>::iterator it = m_friendlies.begin(); it != m_friendlies.end(); ++it)
    {
        float healthThreshold = 0.75f;
        bool isHuman = false;
        Information* info = informationManager->getInformation(*it);
        if (!info || !info->getPhysical())
            continue;
        // A firing medic does not treat itself.
        if (info == self && bot->getPlayer()->getSpawnKit() == 5 && bot->isFiring())
            continue;
        // Object-type flag 0x80 (testb $-0x80, +0x04): only when a human is in it.
        if ((info->getObjectTypeFlags() & 0x80) && !isHumanInTarget(info, bot->getSide()))
            continue;
        if (info == self && bot->getPlayer()->getSpawnKit() == 5 && armament->getActiveWeapon() != -1)
        {
            // A medic holding its healing weapon (type 1) skips itself.
            WeaponInfo* active =
                WeaponInfoManager::m_instance->getInformation(armament->getWeapon(armament->getActiveWeapon()));
            if (active && active->getWeaponType() == 1)
                continue;
        }

        IPIPhysical* physical = info->getPhysical();
        const int strengthType = physical->getStrengthType();
        if (physical->getManDown())
            continue;
        if (isHumanInTarget(info, bot->getSide()))
        {
            healthThreshold = 0.99f;
            isHuman = true;
        }
        if (m_bestWeaponIndex[strengthType] == -1)
            continue;

        const float health = physical->getHealth();
        int occupant = -1;
        if (info->getUnit())
            occupant = info->getUnit()->getOccupyingPlayerId(info->getSide());

        AIRadioMessageInfo medicCall;
        bool medicCalled = false;
        if (canAnswerMedicCall)
            medicCalled = communicationAgentGetReceivedMessage(agent, medicCall, static_cast<AIRadioMessageType>(3),
                                                               occupant);
        // Retail: repairCall is never filled, so its sender stays -1 (the constructor's value).
        AIRadioMessageInfo repairCall;
        bool repairCalled = false;
        if (canAnswerRepairCall)
            repairCalled = vehicleRequestsEngineer(info, info->getSide(), agent);
        if (!repairCalled && bot->getPlayer()->getSpawnKit() == 4 &&
            bot->getSquadMember()->isCloseToNonOwnedTarget())
            continue;

        // ucomiss: needs treatment (0 < health < threshold) or a call for it.
        if (!(healthThreshold > health && health > 0.0f) && !medicCalled && !repairCalled)
            continue;
        if (health > healthThreshold)
            continue;

        // Only units standing on ground flatter than 45 degrees (terrain normal . object up row).
        const Vec3& position = info->getPosition();
        static_cast<IAITerrain*>(CommonSingletonServer::m_iaiTerrain)->getNormal(position.x, position.z, normal);
        const Mat4& transformation = info->getTransformation();
        if (normal.x * transformation.m[4] + normal.y * transformation.m[5] + normal.z * transformation.m[6] <
            0.7071f)
            continue;

        IPIUnit* unit = info->getUnit();
        if (unit->getAlwaysTargetSoldier())
            continue;
        if (distance(selfPosition, position) > m_bestWeaponRange[strengthType])
        {
            if (!selfMobile)
                continue;
            if (!unit->useNoPathfindingToGetToObject() && info->getMobile())
            {
                // Retail: skips the target when the bot's last valid position is within range.
                const Vec3& lastValid = selfMobile->getLastValidPosition(vehicleNumber).position;
                const float range = m_bestWeaponRange[strengthType];
                if (distanceSqr(lastValid, position) < range * range)
                    continue;
            }
        }
        const float targetDistance = info == self ? 1.0f : distance(selfPosition, position);

        float strength;
        if (bot->isOrderedBehaviour(m_index) && bot->isTakingOrders() &&
            orderedCount < bot->getOrderList(m_index)->size() &&
            std::find(bot->getOrderList(m_index)->begin(), bot->getOrderList(m_index)->end(), *it) !=
                bot->getOrderList(m_index)->end())
        {
            ++orderedCount;
            strength = info->getStrength() * bot->getMemory()->m_float8;
        }
        else
        {
            strength = info->getStrength();
        }

        // fdivp (AT&T: st(1) = st(0) / st(1)): strength / (curve(1 - health) * distance).
        float score = strength / (DecleiningSlopeCurve::calculate(1.0f - health) * targetDistance);
        if ((medicCalled && medicCall.m_senderPlayerId == occupant && kit == 5) ||
            (repairCalled && repairCall.m_senderPlayerId == occupant && kit == 4))
        {
            // IBotManager slot +0x1C: a caller without a bot (a human) triples the score.
            if (IBotManager::instance->getBotFromPlayerId(occupant))
            {
                score = score * 1.5f;
            }
            else
            {
                score = score * 3.0f;
                bestHasNoBot = true;
            }
        }
        if (score * scoreModifier > bestScore)
        {
            bestIsHuman = isHuman;
            bestDistance = targetDistance;
            bestWeaponIndex = m_bestWeaponIndex[strengthType];
            bestScore = score * scoreModifier;
            bestTarget = *it;
            bestPlayerId = occupant;
        }
    }

    const unsigned int id = bot->getBotId();
    if (bestTarget == kNoTarget)
    {
        m_urgency[id] = 0.0f;
        m_targetChanged[id] = targets[id] != kNoTarget;
        targets[id] = kNoTarget;
        pattern->unInit(bot);
        return 0.0f;
    }

    // An ordered behaviour with an empty order list scales the score by BotMemory +0x08.
    if (bot->isOrderedBehaviour(m_index) && bot->isTakingOrders() && bot->getOrderList(m_index)->empty())
        bestScore = bestScore * bot->getMemory()->m_float8;

    if (bestIsHuman && bestHasNoBot && bestDistance < 50.0f)
    {
        m_urgency[id] = 100.0f;
    }
    else if (bestIsHuman)
    {
        m_urgency[id] = DecleiningSlopeCurve::calculate(bestScore) * 10.0f * urgencyScale;
    }
    else
    {
        // fadd %st(0), %st: curve + curve.
        const float curve = DecleiningSlopeCurve::calculate(bestScore);
        m_urgency[id] = (curve + curve) * urgencyScale;
    }

    m_targetChanged[id] = targets[id] != bestTarget;
    targets[id] = bestTarget;
    static_cast<BBPMedicAssist*>(m_pattern)->init(targets[id], bestWeaponIndex, bestPlayerId, bot);

    if (kit == 5)
    {
        // RequestAction 1 (ELF 0x217452 movl $0x1).
        m_urgency[id] = squadManagerRequestResponseModifier(bot->getSquadMember(), bestTarget, 1, bot->getSide()) *
                        m_urgency[id];
    }
    else if (kit == 4)
    {
        // RequestAction 2 (ELF 0x2173A2 movl $0x2); Bot +0x1C8 setWantedRepairTarget.
        m_urgency[id] = squadManagerRequestResponseModifier(bot->getSquadMember(), bestTarget, 2, bot->getSide()) *
                        m_urgency[id];
        bot->setWantedRepairTarget(m_urgency[id] == 0.0f ? kNoTarget : bestTarget);
    }
    return m_urgency[bot->getBotId()];
}

// ELF 0x217E70. A bot that is man down never applies; otherwise the information's armament must
// hold a weapon of type 1 (heal/repair) or 2 (ammo): `decl; cmpl $0x1; jbe`.
bool BBMedicAssist::isAplicable(Information* info, Bot* bot)
{
    IPIArmament* armament = info->getArmament();
    if (bot->getManDown() || !armament)
        return false;
    std::vector<unsigned int>& weapons = armament->getWeapons();
    for (unsigned int i = 0; i < weapons.size(); ++i)
    {
        WeaponInfo* weapon = WeaponInfoManager::m_instance->getInformation(weapons[i]);
        if (weapon && static_cast<unsigned int>(weapon->getWeaponType() - 1) <= 1)
            return true;
    }
    return false;
}

// ELF 0x217F10. A repair request (message type 4) from the information's player, or from the
// player of any of its secondary objects for `side`.
bool BBMedicAssist::vehicleRequestsEngineer(Information* info, int side, CommunicationAgent* agent) const
{
    if (info && communicationAgentReceivedMessageExists(agent, static_cast<AIRadioMessageType>(4), info->getPlayerId()))
        return true;
    // Retail: also reached with a null info (getFirstSecondaryObject is called on it).
    int iterator;
    for (Information* secondary = informationManager->getInformation(info->getFirstSecondaryObject(iterator, side));
         secondary; secondary = informationManager->getInformation(info->getSecondaryObject(iterator, side)))
    {
        if (communicationAgentReceivedMessageExists(agent, static_cast<AIRadioMessageType>(4),
                                                    secondary->getPlayerId()))
            return true;
    }
    return false;
}

// ELF 0x218020. The support soldier's version of calculateUrgency: weapon type 2 (ammo), friendly
// units low on ammunition, message type 10, SCurve of the ammo ratio, RequestAction 4.
float BBMedicAssist::calculateGiveAmmoUrgency(Bot* bot, float urgencyScale)
{
    unsigned int* targets = m_targets;
    PlanGenerator* pattern = m_pattern;

    const int nStrengths = static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getNBattleStrengths();
    IAITerrain* terrain = static_cast<IAITerrain*>(CommonSingletonServer::m_iaiTerrain);
    int bestPlayerId = -1;      // -0x190
    int bestWeaponIndex = -1;   // -0x18C
    Vec3 normal = {0.0f, 0.0f, 0.0f};
    Information* self = informationManager->getInformation(bot->getCurrentEquipment());
    const Vec3& selfPosition = self->getPosition();   // retail: no null check

    for (int i = 0; i < nStrengths; ++i)
    {
        m_bestWeaponIndex[i] = -1;
        m_bestWeaponStrength[i] = 0;
    }

    IPIArmament* armament = self->getArmament();
    IPIMobile* selfMobile = self->getMobile();
    int vehicleNumber = -2;
    if (selfMobile)
    {
        vehicleNumber = selfMobile->getVehicleNumber();
        self->getUnit()->getEquipmentType();   // ELF 0x21815A: result unused
    }

    // The bot's own ammo weapons (type 2).
    std::vector<unsigned int> weapons = armament->getWeapons();
    for (unsigned int w = 0; w < weapons.size(); ++w)
    {
        WeaponInfo* weapon = WeaponInfoManager::m_instance->getInformation(weapons[w]);
        if (!weapon || weapon->getWeaponType() != 2)
            continue;
        for (int i = 0; i < nStrengths; ++i)
        {
            if (weapon->getStrength(i) > m_bestWeaponStrength[i])
            {
                m_bestWeaponIndex[i] = static_cast<int>(w);
                m_bestWeaponStrength[i] = weapon->getStrength(i);
                m_bestWeaponRange[i] = weapon->getMaxRange();
            }
        }
    }

    float bestScore = 0.0f;
    unsigned int orderedCount = 0;   // same slot as the resize size, see calculateUrgency
    m_friendlies.resize(orderedCount);
    // IAIInformationEnvironment slot +0x08: getFriendlyUnits(out, position, bot, 75.0f).
    static_cast<IAIInformationEnvironment*>(AISingletonServer::m_iaiInformationEnvironment)
        ->getFriendlyUnits(m_friendlies, selfPosition, bot, 75.0f);

    const float scoreModifier = 1.0f;   // ELF 0x218340 (-0x198)
    unsigned int bestTarget = kNoTarget;   // -0x1B4
    CommunicationAgent* agent = bot->getCommunicationAgent();
    bool canAnswerAmmoCall = !agent->getReceivedMessages(static_cast<AIRadioMessageType>(10)).empty();
    if (canAnswerAmmoCall && bot->isAirBorn())
        canAnswerAmmoCall = false;

    std::vector<unsigned int> targetWeapons;
    Vec3 squadPosition = {0.0f, 0.0f, 0.0f};
    SquadMember* member = bot->getSquadMember();
    member->getSquad()->getSquadPosition(squadPosition);
    Vec3 squadGotoPosition = member->getSquad()->getSquadGotoPos();
    (void)squadGotoPosition;
    Vec3 memberPosition = {0.0f, 0.0f, 0.0f};

    for (std::vector<unsigned int>::iterator it = m_friendlies.begin(); it != m_friendlies.end(); ++it)
    {
        float ammoThreshold = 0.49f;
        Information* info = informationManager->getInformation(*it);
        if (!info || !info->getPhysical())
            continue;

        IPIUnit* occupiedUnit = info->getUnit();
        if (occupiedUnit && occupiedUnit->isOccupied())
        {
            // An occupied unit: skipped when an AI player is in it.
            IAIPlayer* player = playerFromId(occupiedUnit->getOccupyingPlayerId(info->getSide()));
            if (player && player->getIsAIPlayer())
                continue;
        }
        else if (member->getPosition(memberPosition) &&
                 distanceSqr(memberPosition, info->getPosition()) > 2500.0f)
        {
            // An empty unit more than 50 m from the bot's own squad-member position.
            continue;
        }

        if (info == self && bot->getPlayer()->getSpawnKit() == 3 && armament->getActiveWeapon() != -1)
        {
            // A support soldier holding its ammo weapon (type 2) skips itself.
            WeaponInfo* active =
                WeaponInfoManager::m_instance->getInformation(armament->getWeapon(armament->getActiveWeapon()));
            if (active && active->getWeaponType() == 2)
                continue;
        }

        IPIPhysical* physical = info->getPhysical();
        if (physical->getManDown())
            continue;
        const int strengthType = physical->getStrengthType();
        if (m_bestWeaponIndex[strengthType] == -1)
            continue;

        // The ammo ratio over the target's weapons except heal/repair (1) and ammo (2) ones, with
        // getAmmo (+0x10) != -1 and getMaxAmmo != 0. Needs ammo when a weapon has more magazines
        // (+0x18 getNumberOfMags) than int(getCurrentAmmo (+0x14) / getMaxAmmoInMag) + 1.
        targetWeapons = info->getArmament()->getWeapons();
        float ammoRatio = 1.0f;
        bool needsAmmo = false;
        // Retail: the bound is the bot's own weapon count (-0x168/-0x164), the index goes into
        // the target's copy (-0x158).
        for (unsigned int w = 0; w < weapons.size(); ++w)
        {
            WeaponInfo* weapon = WeaponInfoManager::m_instance->getInformation(targetWeapons[w]);
            if (!weapon || weapon->getWeaponType() == 1 || weapon->getWeaponType() == 2 || weapon->getAmmo() == -1 ||
                weapon->getMaxAmmo() == 0)
                continue;
            const float currentAmmo = static_cast<float>(weapon->getCurrentAmmo());
            ammoRatio = currentAmmo / static_cast<float>(weapon->getMaxAmmo()) * ammoRatio;
            const int mags = weapon->getNumberOfMags();
            const float fullMags = currentAmmo / static_cast<float>(weapon->getMaxAmmoInMag());   // fstps, cvttss2si
            if (static_cast<int>(fullMags) + 1 < mags)
                needsAmmo = true;
        }
        if (!needsAmmo)
            continue;

        int occupant = -1;
        if (info->getUnit())
            occupant = info->getUnit()->getOccupyingPlayerId(info->getSide());
        AIRadioMessageInfo ammoCall;
        bool ammoCalled = false;
        if (canAnswerAmmoCall)
            ammoCalled = communicationAgentGetReceivedMessage(agent, ammoCall, static_cast<AIRadioMessageType>(10),
                                                              occupant);
        if (isHumanInTarget(info, bot->getSide()))
            ammoThreshold = 0.74f;   // cmovnel of the bit pattern

        if (!(ammoThreshold > ammoRatio && ammoRatio > 0.0f))
        {
            if (!ammoCalled)
                continue;
            if (!(ammoRatio < 0.9))   // fldl 0x49F828: double compare
                continue;
        }

        const Vec3& position = info->getPosition();
        terrain->getNormal(position.x, position.z, normal);
        const Mat4& transformation = info->getTransformation();
        if (normal.x * transformation.m[4] + normal.y * transformation.m[5] + normal.z * transformation.m[6] <
            0.7071f)
            continue;

        IPIUnit* unit = info->getUnit();
        if (unit->getAlwaysTargetSoldier())
            continue;
        if (distance(selfPosition, position) > m_bestWeaponRange[strengthType])
        {
            if (!selfMobile)
                continue;
            if (!unit->useNoPathfindingToGetToObject() && info->getMobile())
            {
                const Vec3& lastValid = selfMobile->getLastValidPosition(vehicleNumber).position;
                const float range = m_bestWeaponRange[strengthType];
                if (distanceSqr(lastValid, position) < range * range)
                    continue;
            }
        }
        const float targetDistance = info == self ? 1.0f : distance(selfPosition, position);

        float strength;
        if (bot->isOrderedBehaviour(m_index) && bot->isTakingOrders() &&
            orderedCount < bot->getOrderList(m_index)->size() &&
            std::find(bot->getOrderList(m_index)->begin(), bot->getOrderList(m_index)->end(), *it) !=
                bot->getOrderList(m_index)->end())
        {
            ++orderedCount;
            strength = info->getStrength() * bot->getMemory()->m_float8;
        }
        else
        {
            strength = info->getStrength();
        }

        float score = strength / (SCurve::calculate(ammoRatio) * targetDistance);
        if (ammoCalled)
        {
            // Looked up by the message's sender (-0x114), not the occupant.
            if (IBotManager::instance->getBotFromPlayerId(ammoCall.m_senderPlayerId))
                score = score * 1.5f;
            else
                score = score * 3.0f;
        }
        if (score * scoreModifier > bestScore)
        {
            bestWeaponIndex = m_bestWeaponIndex[strengthType];
            bestScore = score * scoreModifier;
            bestTarget = *it;
            bestPlayerId = occupant;
        }
    }

    if (bestTarget == kNoTarget)
    {
        m_urgency[bot->getBotId()] = 0.0f;
        m_targetChanged[bot->getBotId()] = targets[bot->getBotId()] != kNoTarget;
        targets[bot->getBotId()] = kNoTarget;
        pattern->unInit(bot);
        return 0.0f;
    }

    if (bot->isOrderedBehaviour(m_index) && bot->isTakingOrders() && bot->getOrderList(m_index)->empty())
        bestScore = bestScore * bot->getMemory()->m_float8;

    m_urgency[bot->getBotId()] = DecleiningSlopeCurve::calculate(bestScore) * 4.0f * urgencyScale;
    m_targetChanged[bot->getBotId()] = targets[bot->getBotId()] != bestTarget;
    targets[bot->getBotId()] = bestTarget;
    static_cast<BBPMedicAssist*>(m_pattern)->init(targets[bot->getBotId()], bestWeaponIndex, bestPlayerId, bot);
    // RequestAction 4 (ELF 0x218680 movl $0x4).
    const unsigned int id = bot->getBotId();
    m_urgency[id] = squadManagerRequestResponseModifier(bot->getSquadMember(), bestTarget, 4, bot->getSide()) *
                    m_urgency[id];
    return m_urgency[bot->getBotId()];
}

} // namespace ai
} // namespace hfe
} // namespace dice
