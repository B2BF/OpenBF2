// Defuse-mines behaviour (infantry "DefuseMines" slot of aibehaviours.ai): an engineer (spawn kit 4)
// without a mine target looks for the closest hostile mine whose pathfinding position is reachable
// within 35 m. Recovered from libaidll.so (Linux 1.5.3153, ia-32); F-urgency package, see
// docs/macos-ai-behaviour-plan.md.

// static init: ELF 0x207370 (__tcf_0 0x2073D0, _GLOBAL__I 0x207400): <iostream>'s Init object only.
#include <iostream>

#include "BBDefuseMines.h"

#include "../AIBot/BotMain.h"
#include "../AIBot/SquadMember.h"
#include "../AIBotBehaviourPattern/BBPDefuseMines.h"
#include "../AIBotBehaviourPattern/PlanGenerator.h"
#include "../AIKnowledge/IPIArmament.h"
#include "../AIKnowledge/IPIMobile.h"
#include "../AIKnowledge/IPIUnit.h"
#include "../AIKnowledge/Information.h"
#include "../AIKnowledge/InformationManager.h"
#include "../AIKnowledge/WeaponInfo.h"
#include "../AIKnowledge/WeaponInfoManager.h"
#include "../AIUtils/checkPointer.h"
#include "../Main/AIMineManager.h"
#include "../Main/AISettingsInterfaces.h"
#include "../Main/CommonSingletonServer.h"
#include "../Main/IAIPathfinding.h"
#include "../../../AIInterface/BFKnowledge/AIPlayer.h"
#include "../../../AIInterface/BFKnowledge/BFGame.h"
#include "../../../AIInterface/BFKnowledge/IAIObject.h"
#include "../../../AIInterface/BFKnowledge/SoldierPose.h"
#include "../../../../Math/Vec3.h"

#include <cmath>
#include <vector>

namespace dice {
namespace hfe {
namespace ai {

namespace {

inline float lengthSquared(float x, float y, float z)
{
    return x * x + y * y + z * z;
}

} // namespace

// ELF 0x205E20 (C2) / 0x206280 (C1). The base gets getMaxNBots (IAISettings slot 0); the empty
// vector at +0x24 is zeroed; +0x30 is new [getNBattleStrengths] (slot +0x34) and +0x34 new
// [getMaxNBots], both checkPointer'd, before the index/queue/pattern stores; +0x08 is new
// [getMaxNBots]; then every base urgency (+0x10) is 0.0f (rodata 0x49CB34) and every mine target -1.
BBDefuseMines::BBDefuseMines(unsigned char index, unsigned char queue, PlanGenerator* pattern, UrgeCurve* curve,
                             float* modifiers)
    : BotBehaviour(modifiers, curve, static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getMaxNBots())
{
    m_unknown30 = checkPointer(new int[static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getNBattleStrengths()]);
    m_mineTarget = checkPointer(new unsigned int[static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getMaxNBots()]);
    m_index = index;
    m_queue = queue;
    m_pattern = pattern;
    const int count = static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getMaxNBots();
    m_targets = checkPointer(new unsigned int[count]);
    for (int i = 0; i < count; ++i)
    {
        m_urgency[i] = 0.0f;
        m_mineTarget[i] = 0xFFFFFFFFu;
    }
}

// ELF 0x2066E0 (D2) / 0x206780 (D1) / 0x206820 (D0): delete[] +0x08, +0x30, +0x34 when set, then the
// vector at +0x24 (default_alloc deallocate) and the base.
BBDefuseMines::~BBDefuseMines()
{
    if (m_targets)
        delete[] m_targets;
    if (m_unknown30)
        delete[] m_unknown30;
    if (m_mineTarget)
        delete[] m_mineTarget;
}

// ELF 0x2068C0. Bot slots +0x1F8 setMineTarget / +0x1FC getMineTarget are virtual calls in the image,
// but the Bot interface in AIBot/BotMain.h ends at +0x1DC; [CHOSEN, not recovered] the calls go
// through BotMain (the only Bot implementation, same slots) until ai-bot extends Bot.
float BBDefuseMines::calculateUrgency(Bot* bot, bool update, float urgencyScale)
{
    PlanGenerator* const pattern = m_pattern;

    if (!update)
    {
        // 0x2068E4..0x206A11: keep the current choice while a mine target is set.
        if (m_mineTarget[bot->getBotId()] != 0xFFFFFFFFu)
        {
            m_targetChanged[bot->getBotId()] = false;
            if (!pattern->isInitiated(bot))   // PlanGenerator slot +0x0C
                m_urgency[bot->getBotId()] = 0.0f;
        }
        else
        {
            // 0x206980: fucompi against 0.0f, setp|setne (unordered counts as changed).
            m_targetChanged[bot->getBotId()] = m_urgency[bot->getBotId()] != 0.0f;
            m_urgency[bot->getBotId()] = 0.0f;
            pattern->unInit(bot);   // PlanGenerator slot +0x10
        }
        return m_urgency[bot->getBotId()];
    }

    float baseUrgency = 0.0f;
    Information* const info = informationManager->getInformation(bot->getCurrentEquipment());   // Bot +0xD8
    unsigned int mineTarget = 0xFFFFFFFFu;
    const unsigned int botId = bot->getBotId();
    const Vec3 botPosition = info->getPosition();   // Information slot +0x30
    int weaponIndex = -1;
    // 0x206AAB: retail quirk, getNBattleStrengths (IAISettings +0x34) is called and its result dropped.
    static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getNBattleStrengths();

    // 0x206AC4: virtual isApplicable (slot +0x34); then the soldier (0x200000) or 0x40 type flag.
    if (isApplicable(info, bot) &&
        ((info->getObjectTypeFlags() & 0x200000) || (info->getObjectTypeFlags() & 0x40)))
    {
        if ((info->getObjectTypeFlags() & 0x200000) && info->getSoldierPose() == SPSwimming)
        {
            // 0x207255
            m_targetChanged[botId] = true;
            pattern->unInit(bot);
            m_urgency[botId] = 0.0f;
            return 0.0f;
        }

        // 0x206B01: a bot that already has a mine target (Bot +0x1FC) keeps its urgency.
        if (static_cast<BotMain*>(bot)->getMineTarget() != 0xFFFFFFFFu)
            return m_urgency[botId];

        // 0x206B29: the soldier's own Information (side from Bot +0xE0 first), its weapon list
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

        IPIMobile* const mobile = soldierInfo->getMobile();   // +0x38, read once before the loop
        SquadMember* const member = bot->getSquadMember();    // Bot +0x140
        std::vector<unsigned int> ignoredMines;
        Vec3 minePosition = {0.0f, 0.0f, 0.0f};
        Vec3 mineNormal = {0.0f, 0.0f, 0.0f};
        Vec3 targetPosition = {0.0f, 0.0f, 0.0f};
        bool done = false;
        bool found = false;
        int tries = 0;
        while (!done)
        {
            // 0x206CF6: cmpl $0x64; jg -- at most 100 queries.
            if (++tries > 100)
                break;

            bool mineFlag;   // [UNRESOLVED] meaning; its negation is passed as the MineType
            unsigned int mineId;
            // IAIGame slot +0x18 (declared int; the caller tests %al only, 0x206D91).
            if (!static_cast<unsigned char>(static_cast<IAIGame*>(CommonSingletonServer::m_iaiGame)
                                                ->getClosestHostileMine(member->getPlayer(), botPosition, mineFlag,
                                                                        minePosition, mineNormal, mineId,
                                                                        ignoredMines)))
            {
                done = true;
                continue;
            }

            found = false;
            unsigned int quadTree;
            unsigned int polygon;
            // ELF 0x206DDB calls the const-reference getter (0x2E1720), then copies its Vec3.
            minePosition = static_cast<IAIMineManager*>(CommonSingletonServer::m_iAIMineManager)
                ->getPathfindingPositionOfMine(mineId, static_cast<IAIMineManager::MineType>(!mineFlag),
                                               quadTree, polygon);
            if (quadTree != 0xFFFFFFFFu && polygon != 0xFFFFFFFFu)
            {
                targetPosition = minePosition;
                // 0x206E2A: fucompi |normal.y| against 0.5f (rodata 0x49CB44); jbe pulls the target
                // back 0.8f (rodata 0x49CB64) along the normal, also when unordered.
                if (!(std::fabs(mineNormal.y) > 0.5f))
                {
                    targetPosition.x = minePosition.x - mineNormal.x * 0.8f;
                    targetPosition.y = minePosition.y - mineNormal.y * 0.8f;
                    targetPosition.z = minePosition.z - mineNormal.z * 0.8f;
                }
                // IAIPathfinding slot +0x2C; getEquipmentType (Information +0x40 unit, reread each
                // pass) is evaluated before getVehicleNumber.
                const int equipmentType = soldierInfo->getUnit()->getEquipmentType();
                const int vehicleNumber = mobile->getVehicleNumber();
                if (IAIPathfinding::instance->isValidPosition(vehicleNumber, equipmentType, targetPosition, false))
                {
                    // 0x206F0B: rodata 0x49F6A4 1225.0f (35 m squared); 42.0f is rodata 0x49F6A8.
                    if (lengthSquared(targetPosition.x - botPosition.x, targetPosition.y - botPosition.y,
                                      targetPosition.z - botPosition.z) < 1225.0f)
                    {
                        baseUrgency = 42.0f;
                        found = true;
                        mineTarget = mineId;
                        done = true;
                        continue;
                    }
                }
            }
            ignoredMines.push_back(mineId);
        }

        if (found)
        {
            // 0x206F81: flds base; fmuls urgencyScale.
            m_urgency[botId] = baseUrgency * urgencyScale;
            m_targetChanged[botId] = m_mineTarget[botId] != mineTarget;
            m_mineTarget[botId] = mineTarget;
            static_cast<BBPDefuseMines*>(pattern)->init(mineTarget, weaponIndex, bot, minePosition, targetPosition);
            if (m_urgency[botId] == 0.0f)
                static_cast<BotMain*>(bot)->setMineTarget(0xFFFFFFFFu);   // Bot +0x1F8
            return m_urgency[bot->getBotId()];
        }

        // 0x207105
        m_urgency[botId] = 0.0f;
        m_targetChanged[botId] = m_mineTarget[botId] != mineTarget;
        pattern->unInit(bot);
        m_mineTarget[botId] = mineTarget;
        static_cast<BotMain*>(bot)->setMineTarget(0xFFFFFFFFu);
        return 0.0f;
    }

    // 0x207294
    m_urgency[botId] = 0.0f;
    m_targetChanged[botId] = m_mineTarget[botId] != 0xFFFFFFFFu;
    m_mineTarget[botId] = 0xFFFFFFFFu;
    pattern->unInit(bot);
    static_cast<BotMain*>(bot)->setMineTarget(0xFFFFFFFFu);
    return 0.0f;
}

// ELF 0x207310: bot->getPlayer() (+0xBC) alive (+0x24), not getManDown (+0x1B0), and the
// player's getSpawnKit (+0x44) == 4.
bool BBDefuseMines::isApplicable(Information*, Bot* bot)
{
    if (!bot->getPlayer()->getIsAlive())
        return false;
    if (bot->getManDown())
        return false;
    return bot->getPlayer()->getSpawnKit() == 4;
}

} // namespace ai
} // namespace hfe
} // namespace dice
