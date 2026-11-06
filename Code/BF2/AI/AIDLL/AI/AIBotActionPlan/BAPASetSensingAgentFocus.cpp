// Package C (ai-w1-c-actions): recovered from libaidll.so 1.5.3153 (ia-32); ELF addresses on every body.

// static init: ELF 0x1E05A0 (__tcf_0 0x1E0600, _GLOBAL__I 0x1E0630): <iostream>'s Init object only.
#include <iostream>

#include "BAPASetSensingAgentFocus.h"

#include "../AIBot/BotMain.h"
#include "../AIBot/SensingAgent.h"

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x1E0280 (C1; C2 0x1E01F0 region): target form.
BAPASetSensingAgentFocus::BAPASetSensingAgentFocus(float weight, unsigned int target, int tag, bool flag)
{
    m_controls = 0;
    m_direction.x = 0.0f;
    m_direction.y = 0.0f;
    m_direction.z = 0.0f;
    m_patternId = tag;
    m_weight = weight;
    m_target = target;
    m_isTarget = true;
    m_flag = flag;
}

// ELF 0x1E0390: direction form (+0x1C is left unwritten).
BAPASetSensingAgentFocus::BAPASetSensingAgentFocus(float weight, const Vec3& direction, int tag, bool flag)
{
    m_controls = 0;
    m_patternId = tag;
    m_weight = weight;
    m_direction = direction;
    m_isTarget = false;
    m_flag = flag;
}

// ELF 0x1E0460 (D1; D2 0x1E0420 region, D0 0x1E04A0)
BAPASetSensingAgentFocus::~BAPASetSensingAgentFocus()
{
}

// ELF 0x1E04F0
bool BAPASetSensingAgentFocus::execute(Bot* bot, bool& ok, bool& failed)
{
    if (m_isTarget)
        bot->getSensingAgent()->insertPlanTargetInformation(m_weight, m_target, m_flag);
    else
        bot->getSensingAgent()->insertPlanDirectionInformation(m_weight, m_direction, m_flag);
    ok = true;
    failed = true;
    return true;
}

// ELF 0x1E0650
BAPStatementType BAPASetSensingAgentFocus::getId() const
{
    return BAPSTSetSensingAgentFocus;   // 35
}

// ELF 0x1E0590
bool BAPASetSensingAgentFocus::hasDefault()
{
    return true;
}

} // namespace ai
} // namespace hfe
} // namespace dice
