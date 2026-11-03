// Random pattern (BBRandom: the grenade-warning reaction).
// Recovered from libaidll.so (Linux 1.5.3153, ia-32).

// static init: ELF 0x2989B0 (__tcf_0 0x298A10, _GLOBAL__I 0x298A40): <iostream>'s Init object only.
#include <iostream>

#include "BBPRandom.h"

#include "../AIBot/BotMain.h"
#include "../AIBot/SquadMember.h"
#include "../AIBotActionPlan/BAPASoldierPose.h"
#include "../AIBotActionPlan/BAPATrigger.h"
#include "../AIBotActionPlan/BAPCombinerParallel.h"
#include "../AIBotActionPlan/BAPConFalse.h"
#include "../AIBotActionPlan/BAPConTrue.h"
#include "../AIBotActionPlan/BAPFlowCWhile.h"
#include "../AIBotActionPlan/BAPIWIdle.h"
#include "../AIKnowledge/Information.h"
#include "../AIKnowledge/InformationManager.h"
#include "../AIKnowledge/IPIArmament.h"
#include "../AIKnowledge/WeaponInfo.h"
#include "../AIKnowledge/WeaponInfoManager.h"
#include "../AIPlanInterpreter/BAPContext.h"
#include "../AIPlanInterpreter/BotScriptDebugger.h"
#include "../AIUtils/checkPointer.h"
#include "../Main/AISettingsInterfaces.h"
#include "../Main/CommonSingletonServer.h"
#include "../../../AIInterface/BFKnowledge/BFTimer.h"
#include "../../../../Core/Random.h"
#include <cstdint>

namespace dice {
namespace hfe {
namespace ai {

namespace {
// [CHOSEN, not recovered] Local packaging of Random's inlined arithmetic at ELF 0x2979A4..0x297B64.
std::uint32_t randomStage(std::uint32_t seed, int factor)
{
    const std::int32_t low = static_cast<std::int32_t>((seed & 0xffffu) * factor);
    const std::int32_t high = (static_cast<std::int32_t>(seed) >> 16) * factor + (low >> 16);
    const std::uint32_t combined = (static_cast<std::uint32_t>(low) & 0xffffu)
        + ((static_cast<std::uint32_t>(high) & 0x7fffu) << 16)
        + static_cast<std::uint32_t>(high >> 15);
    const std::uint32_t reduced = combined - 0x7fffffffu;
    return static_cast<std::int32_t>(reduced) < 0 ? combined : reduced;
}

float randomDraw()
{
    Rand.m_seed = randomStage(randomStage(Rand.m_seed, 24112), 26143);
    const std::int32_t raw = (static_cast<std::int32_t>(Rand.m_seed) >> 7) | 1;
    return static_cast<float>(raw + 1) * 5.9604644775390625e-8f;
}
}

// ELF 0x297400 (C2) / 0x297580 (C1): BBPPattern(), +0x08 = id, +0x0C = checkPointer(new[getMaxNBots]).
BBPRandom::BBPRandom(int id)
    : BBPPattern()
{
    m_id = id;
    m_perBot = checkPointer(new unsigned int[static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getMaxNBots()]);
}

// ELF 0x297700 (D2) / 0x297750 (D1) / 0x2977A0 (D0): delete[] +0x0C when set, then ~BBPPattern.
BBPRandom::~BBPRandom()
{
    if (m_perBot)
        delete[] m_perBot;
}

// ELF 0x297840
BotScript* BBPRandom::createPlan(Bot* bot)
{
    if (!m_initiated[bot->getBotId()])
    {
        if (bot->getDebugger())
            static_cast<BotScriptDebugger*>(bot->getDebugger())->clear();
        bot->getScriptContext()->clear();
        if (bot->getPlan())
        {
            BotScript* previous = bot->getPlan();
            if (previous)
                delete previous;
            bot->setPlan(0);
        }
        return 0;
    }
    bot->getSquadMember()->clearRequestAction();
    Information* self = informationManager->getInformation(bot->getCurrentEquipment());
    if (!bot->hasPlan())
    {
        if (bot->getPlan())
        {
            BotScript* previous = bot->getPlan();
            if (previous)
                delete previous;
            bot->setPlan(0);
        }
    }
    else if (static_cast<BAPStatement*>(bot->getPlan())->getPatternId() == m_id)
        return bot->getPlan();

    if (bot->getDebugger())
        static_cast<BotScriptDebugger*>(bot->getDebugger())->clear();
    bot->getSquadMember()->clearRequestAction();
    // shipped defect: an active plan with a different pattern id is not deleted here.
    bot->getScriptContext()->clear();
    BAPCombinerParallel* parallel = checkPointer(new BAPCombinerParallel(m_id, -1, false));
    const double now = static_cast<const IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
    const float first = randomDraw();
    const float second = randomDraw();
    const float third = randomDraw();
    m_initTime = now;
    const SoldierPose pose = first > second ? SPCrouching : SPLying;
    parallel->addStatement(checkPointer(new BAPASoldierPose(pose, checkPointer(new BAPConTrue()), m_id, false)));
    Information* weaponOwner = informationManager->getInformation(bot->getCurrentEquipment());
    WeaponInfo* weapon = WeaponInfoManager::m_instance->getInformation(weaponOwner->getArmament()->getWeapon(1));
    if (weapon && third > 0.7f)
    {
        const unsigned int mask = 1u << (static_cast<unsigned int>(weapon->getWeaponFire()) & 31u);
        const unsigned long long controls = static_cast<unsigned long long>(static_cast<long long>(static_cast<std::int32_t>(mask)));
        BAPConFalse* stop = checkPointer(new BAPConFalse());
        BAPConTrue* start = checkPointer(new BAPConTrue());
        const io::PlayerInputMap input = static_cast<io::PlayerInputMap>(weapon->getWeaponFire());
        parallel->addStatement(checkPointer(new BAPATrigger(input, start, stop, controls, m_id, 0.0, true)));
    }
    BAPFlowCWhile* loop = checkPointer(new BAPFlowCWhile(checkPointer(new BAPConTrue()), parallel, true, m_id));
    BAPIWIdle* wrapper = checkPointer(new BAPIWIdle(self->getPosition(), loop, m_id, bot));
    wrapper->m_owner = this;
    return wrapper;
}

// ELF 0x2977F0: m_initiated[bot->getBotId()] = true; +0x14 = IBFTimer slot +0x08 (getGameTime).
void BBPRandom::init(Bot* bot)
{
    m_initiated[bot->getBotId()] = true;
    m_initTime = static_cast<const IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
}

} // namespace ai
} // namespace hfe
} // namespace dice
