// BotBehaviour: recovered by package A-core from libaidll.so (TU BotBehaviour.cpp, 0x225250..0x2260D0;
// the two weak members sit at 0x1FD160). Every body carries its ELF address.

// static init: ELF 0x226040 (__tcf_0 0x2260A0, _GLOBAL__I 0x2260D0): <iostream>'s Init object only.
#include <iostream>

#include "BotBehaviour.h"

#include "../AIBot/BotMain.h"
#include "../AIBot/SensingAgent.h"
#include "../AIBotBehaviourPattern/PlanGenerator.h"
#include "../AIPlanInterpreter/BotScript.h"
#include "../AIUtils/checkPointer.h"
#include "../Main/AIDllFrontier.h"

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x225250 (C2; C1 0x225610). Three checkPointer'd arrays, then the init loop. The urgency array
// is not initialised here; reset() and the derived calculateUrgency write it.
BotBehaviour::BotBehaviour(float* modifiers, UrgeCurve* urgeCurve, int count)
    : m_targets(0)
    , m_pattern(0)
{
    m_urgency = checkPointer(new float[count]);
    m_modifiers = modifiers;
    m_targetChanged = checkPointer(new bool[count]);
    m_activeUrgency = checkPointer(new float[count]);
    m_urgeCurve = urgeCurve;
    for (int i = 0; i < count; ++i)
    {
        m_activeUrgency[i] = 1e-5f;     // rodata 0x49D588
        m_targetChanged[i] = true;
    }
}

// ELF 0x2259D0 (C2; C1 0x225A20). No arrays.
BotBehaviour::BotBehaviour(float* modifiers, UrgeCurve* urgeCurve)
    : m_targets(0)
    , m_pattern(0)
    , m_urgency(0)
    , m_modifiers(modifiers)
    , m_targetChanged(0)
    , m_activeUrgency(0)
    , m_urgeCurve(urgeCurve)
{
}

// [TEMP] S0 skeleton default constructor (see the header).
BotBehaviour::BotBehaviour()
{
    BF2_AI_FRONTIER("dice::hfe::ai::BotBehaviour::BotBehaviour (S0 skeleton default constructor: a derived constructor is not recovered)");
}

// ELF 0x225A70 (D2; D1 0x225B00, D0 0x225B90). The pattern is owned (deleting destructor, slot +4);
// the modifier row, the curve and m_targets are not freed here.
BotBehaviour::~BotBehaviour()
{
    delete[] m_urgency;
    delete[] m_targetChanged;
    delete[] m_activeUrgency;
    delete m_pattern;
}

// ELF 0x225D70.
float BotBehaviour::getUrgency(Bot* bot)
{
    return m_urgency[bot->getBotId()];
}

// ELF 0x225DA0.
float BotBehaviour::getActiveUrgency(Bot* bot)
{
    return m_activeUrgency[bot->getBotId()];
}

// ELF 0x225DD0: the current urgency becomes the active one and is returned (fsts).
float BotBehaviour::updateActiveUrgency(Bot* bot)
{
    return m_activeUrgency[bot->getBotId()] = m_urgency[bot->getBotId()];
}

// ELF 0x225E20.
bool BotBehaviour::hasChangedTarget(Bot* bot)
{
    return m_targetChanged[bot->getBotId()];
}

// ELF 0x225E50. With no active urgency: changed when the new one is positive. With a zero new
// urgency: changed. Otherwise the ratio new/active (`fdivp %st,%st(1)` = de f1, st(1) = st(0)/st(1)
// with st(0) = urgency) must lie inside [low, high]. The comparisons are the unordered-false
// forms of the image (ja / jbe after fucomip).
bool BotBehaviour::quotientChanged(Bot* bot, float low, float high)
{
    if (m_activeUrgency[bot->getBotId()] == 0.0f)
        return m_urgency[bot->getBotId()] > 0.0f;

    if (m_urgency[bot->getBotId()] == 0.0f)
        return true;

    const float quotient = m_urgency[bot->getBotId()] / m_activeUrgency[bot->getBotId()];
    return low > quotient || quotient > high;
}

// ELF 0x225F30: both through the vtable (+0x14, +0x18).
bool BotBehaviour::hasChanged(Bot* bot, float low, float high)
{
    return hasChangedTarget(bot) && quotientChanged(bot, low, high);
}

// ELF 0x1FD160 (weak).
UrgencyGenerator* BotBehaviour::getChosenGenerator()
{
    return this;
}

// ELF 0x225F90. Note the 1e-4f here against the constructor's 1e-5f (rodata 0x49D578 / 0x49D588).
void BotBehaviour::reset(Bot* bot)
{
    if (m_pattern)
        m_pattern->unInit(bot);
    m_urgency[bot->getBotId()] = 0.0f;
    m_activeUrgency[bot->getBotId()] = 1e-4f;
    m_targetChanged[bot->getBotId()] = true;
    if (m_targets)
        m_targets[bot->getBotId()] = 0xFFFFFFFFu;
}

// ELF 0x225C30 (105 instructions).
// * A locked plan (Bot +0x68) is kept: the current plan (+0x60) is returned.
// * An initiated pattern builds a new plan. The old plan is first re-owned by the pattern, the new
//   one by this behaviour; when a different old plan exists it is re-owned by this behaviour too
//   and the sensing agent is told something new was sensed (SensingAgent +0x78 = 1).
//   The new plan is returned (the caller replaces and frees the old one).
// * A pattern that is not initiated: the current plan is deleted (slot +8), the plan is cleared
//   (+0x64) and 0 is returned.
BotScript* BotBehaviour::generatePlan(Bot* bot)
{
    if (bot->getPlanLock())
        return bot->getPlan();

    if (!m_pattern->isInitiated(bot))
    {
        if (bot->getPlan() != 0)
        {
            delete bot->getPlan();
            bot->setPlan(0);
        }
        return 0;
    }

    if (bot->getPlan() != 0)
        bot->getPlan()->m_owner = m_pattern;

    BotScript* plan = m_pattern->createPlan(bot);
    if (plan != 0)
    {
        plan->m_owner = this;
        if (bot->getPlan() != 0 && bot->getPlan() != plan)
        {
            bot->getPlan()->m_owner = this;
            bot->getSensingAgent()->setSensedNewObject();
        }
    }
    return plan;
}

// ELF 0x1FD170 (weak).
bool BotBehaviour::isAplicable(Information*, Bot*)
{
    return true;
}

} // namespace ai
} // namespace hfe
} // namespace dice
