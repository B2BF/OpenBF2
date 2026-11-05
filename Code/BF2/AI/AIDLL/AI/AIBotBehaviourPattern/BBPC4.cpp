// C4 pattern: places or detonates the bot's C4 (AI/AIDLL/AI/AIBotBehaviourPattern/BBPC4.cpp in the
// image). Recovered from libaidll.so (Linux 1.5.3153, ia-32, symbols); every body carries its ELF
// address.
//
// static init: ELF 0x235010 (__tcf_0 0x235070, _GLOBAL__I 0x2350A0): the <iostream> guard object only.

#include "BBPC4.h"

#include "../AIBot/BotMain.h"
#include "../AIBotActionPlan/BAPAIdle.h"
#include "../AIBotActionPlan/BAPAPlaceC4.h"
#include "../AIBotActionPlan/BAPAResetControls.h"
#include "../AIBotActionPlan/BAPATrigger.h"
#include "../AIBotActionPlan/BAPCombinerSerial.h"
#include "../AIBotActionPlan/BAPConFalse.h"
#include "../AIBotActionPlan/BAPConNot.h"
#include "../AIBotActionPlan/BAPConTime.h"
#include "../AIBotActionPlan/BAPConTrue.h"
#include "../AIBotActionPlan/BAPConWeaponDetonatorIsActive.h"
#include "../AIBotActionPlan/BAPFlowCIf.h"
#include "../AIBotActionPlan/BAPIWFire.h"
#include "../AIBotActionPlan/BAPVariable.h"
#include "../AIKnowledge/Information.h"
#include "../Main/InformationManager.h"
#include "../AIKnowledge/IPIArmament.h"
#include "../AIKnowledge/WeaponInfo.h"
#include "../AIUtils/WeaponInfoManager.h"
#include "../AIPlanInterpreter/BAPContext.h"
#include "../AIPlanInterpreter/BotScriptDebugger.h"
#include "../AIUtils/checkPointer.h"
#include "../Main/AISettingsInterfaces.h"
#include "../Main/CommonSingletonServer.h"

#include <iostream>

namespace dice {
namespace hfe {
namespace ai {

namespace {

void dropPlan(Bot* bot)
{
    delete bot->getPlan();
    bot->setPlan(0);
}

// One weapon key as the image builds it inline every time: a trigger of the key that starts at
// once (BAPConTrue) and never stops by condition (BAPConFalse), with the key's control bit
// (1 << key, sign-extended from int) and no repeat time. The key is read twice, for the controls
// and for the input map (right-to-left argument order: the controls' read comes first).
BAPATrigger* newKeyTrigger(const WeaponInfo* weapon, int (WeaponInfo::*key)() const, int patternId, bool flag)
{
    return checkPointer(new BAPATrigger(static_cast<io::PlayerInputMap>((weapon->*key)()),
                                        checkPointer(new BAPConTrue()), checkPointer(new BAPConFalse()),
                                        1 << (weapon->*key)(), patternId, 0.0, flag));
}

} // namespace

// ELF 0x2321F0 (C2) / 0x232370 (C1): BBPPattern(), +0x08 = id, +0x0C = checkPointer(new[getMaxNBots]).
BBPC4::BBPC4(int id)
    : BBPPattern()
{
    m_id = id;
    m_perBot = checkPointer(new unsigned int[static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getMaxNBots()]);
}

// ELF 0x2324F0 (D2) / 0x232540 (D1) / 0x232590 (D0): delete[] +0x0C when set, then ~BBPPattern.
BBPC4::~BBPC4()
{
    if (m_perBot)
        delete[] m_perBot;
}

// ELF 0x232620 (listing). Builds, under a BAPIWFire(bot, -1, true, false, 4, -1, ..) wrapper,
//   serial { reset controls, trigger(activate key of the C4 slot 4 weapon),
//            if (detonator [not] active) trigger(alt-fire key), idle 0.7 s, trigger(fire key),
//            [trigger(activate key of the slot 2 weapon)], place C4 (type 0, target) }
// m_placed (init's bool) selects the variant: placed: the alt-fire key when the detonator is NOT
// active and BAPAPlaceC4(.., false); not placed: when it IS active and BAPAPlaceC4(.., true).
// The script context is cleared first, also when the current plan of this pattern is kept, and the
// equipment handle is read once without use; as compiled. Returns 0 without equipment, unit, mobile
// or a slot 4 weapon. The bot's controls are reset after the plan is built.
BotScript* BBPC4::createPlan(Bot* bot)
{
    if (!m_initiated[bot->getBotId()])
    {
        // 0x234F98
        if (bot->getDebugger())
            static_cast<BotScriptDebugger*>(bot->getDebugger())->clear();
        bot->getScriptContext()->clear();
        if (bot->getPlan())
            dropPlan(bot);
        return 0;
    }

    if (bot->getDebugger())
        static_cast<BotScriptDebugger*>(bot->getDebugger())->clear();
    BAPContext* const context = bot->getScriptContext();
    context->clear();
    bot->getCurrentEquipment();   // 0x232692, result unused

    if (bot->hasPlan() && static_cast<BAPStatement*>(bot->getPlan())->getPatternId() == m_id)
        return bot->getPlan();   // 0x234F73
    if (bot->getPlan())
        dropPlan(bot);           // 0x234F48

    Information* const equipment = informationManager->getInformation(bot->getCurrentEquipment());
    if (!equipment || !equipment->getUnit() || !equipment->getMobile())
        return 0;
    IPIArmament* const armament = equipment->getArmament();
    WeaponInfo* const c4 = WeaponInfoManager::m_instance->getInformation(armament->getWeapon(4));   // 0x23274B
    if (!c4)
        return 0;

    BAPATrigger* const activate = newKeyTrigger(c4, &WeaponInfo::getWeaponActivate, m_id, false);   // 0x232880
    BAPCombinerSerial* const serial = checkPointer(new BAPCombinerSerial(m_id, -1));
    serial->addStatement(checkPointer(new BAPAResetControls(true, true, true, m_id)));
    serial->addStatement(checkPointer(activate));   // checkPointer'd a second time (0x232945)

    // 0x23296B: the plan variable of the C4 weapon (inline BAPVariable, then list insert at begin).
    BAPVariable<WeaponInfo*>* const weaponVariable = checkPointer(new BAPVariable<WeaponInfo*>(context, c4));
    context->addVariable(weaponVariable);

    BAPFlowCIf* detonate;
    BAPAPlaceC4* place;
    if (m_placed)
    {
        // 0x232A02
        detonate = checkPointer(new BAPFlowCIf(
            checkPointer(new BAPConNot(checkPointer(new BAPConWeaponDetonatorIsActive(weaponVariable)))),
            newKeyTrigger(c4, &WeaponInfo::getWeaponAltFire, m_id, true), 0, m_id, true, false));
        place = checkPointer(new BAPAPlaceC4(0, m_target, false));
    }
    else
    {
        // 0x2340D0
        detonate = checkPointer(new BAPFlowCIf(checkPointer(new BAPConWeaponDetonatorIsActive(weaponVariable)),
                                               newKeyTrigger(c4, &WeaponInfo::getWeaponAltFire, m_id, true), 0, m_id, true,
                                               false));
        place = checkPointer(new BAPAPlaceC4(0, m_target, true));
    }
    // 0x232C55
    serial->addStatement(detonate);
    serial->addStatement(checkPointer(new BAPAIdle(checkPointer(new BAPConTime(0.7f, false, false, false)), 0ULL, m_id)));
    serial->addStatement(newKeyTrigger(c4, &WeaponInfo::getWeaponFire, m_id, true));
    serial->addStatement(place);

    WeaponInfo* const other = WeaponInfoManager::m_instance->getInformation(armament->getWeapon(2));   // 0x232E9E
    if (other)
        serial->addStatement(newKeyTrigger(other, &WeaponInfo::getWeaponActivate, m_id, false));

    // 0x232FE2
    BAPIWFire* const root = checkPointer(new BAPIWFire(bot, 0xFFFFFFFFu, true, false, 4, -1, serial, m_id));
    root->setParent(this);   // +0x04 (BotScript::mpOwner)
    static_cast<BotMain*>(bot)->resetAllControls(true, true, true);
    return root;
}

// ELF 0x2325E0: m_initiated[bot->getBotId()] = true (BBPPattern +0x04), then +0x10/+0x14.
void BBPC4::init(Bot* bot, bool placed, AIStrategicObject* target)
{
    m_initiated[bot->getBotId()] = true;
    m_placed = placed;
    m_target = target;
}

} // namespace ai
} // namespace hfe
} // namespace dice
