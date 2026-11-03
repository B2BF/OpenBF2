// C4 behaviour (infantry "c4" slot of aibehaviours.ai): a bot carrying a C4 kit with charges left
// either blows its placed charge (urgency 100) or places one at an owned control point (500).
// Recovered from libaidll.so (Linux 1.5.3153, ia-32); F-urgency package, see
// docs/macos-ai-behaviour-plan.md.

// static init: ELF 0x1FF760 (__tcf_0 0x1FF7C0, _GLOBAL__I 0x1FF7F0): <iostream>'s Init object only.
#include <iostream>

#include "BBC4.h"

#include "../AIBot/BotMain.h"
#include "../AIBot/SquadManager.h"
#include "../AIBotBehaviourPattern/BBPC4.h"
#include "../AIKnowledge/IPIArmament.h"
#include "../AIKnowledge/Information.h"
#include "../AIKnowledge/InformationManager.h"
#include "../AIKnowledge/WeaponInfo.h"
#include "../AIKnowledge/WeaponInfoManager.h"
#include "../AIStrategic/SAI.h"
#include "../AIStrategicEntity/AIStrategicObject.h"
#include "../AIUtils/checkPointer.h"
#include "../Main/AIDllFrontier.h"
#include "../Main/AIMain.h"
#include "../Main/AISettingsInterfaces.h"
#include "../Main/CommonSingletonServer.h"
#include "../../../AIInterface/BFKnowledge/AIPlayer.h"

#include <vector>

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x1FEDC0 (C2) / 0x1FEFB0 (C1): base with getMaxNBots, index/queue/pattern, then
// +0x08 = checkPointer(new[getMaxNBots]) and every base urgency = 0.0f (rodata 0x49CB34).
BBC4::BBC4(unsigned char index, unsigned char queue, PlanGenerator* pattern, UrgeCurve* curve, float* modifiers)
    : BotBehaviour(modifiers, curve, static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getMaxNBots())
{
    m_index = index;
    m_queue = queue;
    m_pattern = pattern;
    const int count = static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getMaxNBots();
    m_targets = checkPointer(new unsigned int[count]);
    for (int i = 0; i < count; ++i)
        m_urgency[i] = 0.0f;
}

// ELF 0x1FF1A0 (D2) / 0x1FF1F0 (D1) / 0x1FF240 (D0)
BBC4::~BBC4()
{
    if (m_targets)
        delete[] m_targets;
}

// ELF 0x1FF290 (249 insns).
float BBC4::calculateUrgency(Bot* bot, bool active, float)
{
    if (!active)
    {
        // 0x1FF2B9: `cmpb $0,0x10(%ebp); jne` - an inactive call only clears the slot.
        m_urgency[bot->getBotId()] = 0.0f;
        return m_urgency[bot->getBotId()];
    }

    // 0x1FF2F5: the bot's current equipment through the information pool (inlined lookup).
    Information* info = informationManager->getInformation(bot->getCurrentEquipment());
    float urgency = 0.0f;                         // -0x168
    bool placed = false;                          // -0x169
    AIStrategicObject* object = 0;                // -0x160
    SAI* sai = IAIMain::m_instance->getSAI(bot->getSide());   // IAIMain +0x3C

    // 0x1FF36E: `testb $0x20,0x6(%esi)` - type flag 0x200000 of the equipment, then the
    // behaviour's own isApplicable (slot +0x34).
    if (info && (info->getObjectTypeFlags() & 0x200000) && isApplicable(info, bot))
    {
        // 0x1FF41E: the first weapon of type 4 (C4); w keeps the last lookup when none matches.
        std::vector<unsigned int>& weapons = info->getArmament()->getWeapons();
        WeaponInfo* weapon = 0;
        for (unsigned int i = 0; i < weapons.size(); ++i)
        {
            weapon = WeaponInfoManager::m_instance->getInformation(weapons[i]);
            if (weapon && weapon->getWeaponType() == 4)
                break;
        }

        // 0x1FF4A4: Bot::getC4 (+0x158) by value (sret, BotMain 0x188880).
        const Bot::C4State c4 = bot->getC4();
        const Vec3 position = info->getPosition();   // slot +0x30, copied

        if (c4.placed)
        {
            object = c4.object;
            if (c4.placementType == 0)
            {
                // 0x1FF505: a hostile takeover of the area the charge lies in, then no friendly
                // within 15 m (rodata 0x49CDC4) of the charge: blow it.
                AIStrategicObject* takeover = 0;
                Vec2 takeoverPosition = { 0.0f, 0.0f };
                if (sai->hostileTakeoveInProgress(takeover, takeoverPosition) && c4.object
                    && c4.object->getIndex() == takeover->getIndex())
                {
                    if (!SquadManager::m_instance->friendliesCloseTo(c4.position, 15.0f, bot->getSide()))
                    {
                        urgency = 100.0f;                 // rodata 0x49CB5C
                        placed = true;
                    }
                }
            }
        }
        else
        {
            // 0x1FF5F0: charges left (WeaponInfo slot +0x14) and inside an owned control point
            // (10 m, rodata 0x49CC88): place one, urgency 500 (rodata 0x49F500, cmovne).
            placed = false;
            if (weapon->getCurrentAmmo() > 0)
            {
                if (sai->isPosInsideOwnedControlPoint(object, position, 10.0f))
                    urgency = 500.0f;
            }
        }
    }

    // 0x1FF378: `flds 0.001; flds urgency; fxch; fucompi; jbe init` - init unless urgency < 0.001.
    if (urgency < 0.001f)
        m_pattern->unInit(bot);
    else
        static_cast<BBPC4*>(m_pattern)->init(bot, placed, object);

    m_urgency[bot->getBotId()] = urgency;
    return m_urgency[bot->getBotId()];
}

// ELF 0x1FF690: alive, not man down, and an armament holding a type-4 weapon with ammo.
bool BBC4::isApplicable(Information* info, Bot* bot)
{
    IPIArmament* armament = info->getArmament();   // +0x2C, read first
    if (!bot->getPlayer()->getIsAlive())
        return false;
    if (bot->getManDown())
        return false;
    if (!armament)
        return false;

    std::vector<unsigned int>& weapons = armament->getWeapons();
    for (unsigned int i = 0; i < weapons.size(); ++i)
    {
        WeaponInfo* weapon = WeaponInfoManager::m_instance->getInformation(weapons[i]);
        if (weapon && weapon->getWeaponType() == 4 && weapon->getCurrentAmmo() > 0)
            return true;
    }
    return false;
}

} // namespace ai
} // namespace hfe
} // namespace dice
