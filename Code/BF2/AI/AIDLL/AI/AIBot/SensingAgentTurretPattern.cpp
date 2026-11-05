// Source placement: AIDLL_r.dll __FILE__ "Code\BF2\AI\AIDLL\AI\AIBot\SensingAgentTurretPattern.cpp"
// (asserts "No info" / "No control info" in 0x10137390 and 0x10137570). Split out of the former
// SensingAgentPatterns.cpp; the functions are unchanged.
#include "SensingAgentPatterns.h"
#include <iostream>
// static init: ELF 0x1C31A0, 0x1C3200, 0x1C3230 (Turret)

#include "SensingAgentPatternPrologue.h"
#include "../AIBotActionPlan/BAPALookAtObject.h"
#include "../AIBotActionPlan/BAPALookInDir.h"

namespace dice {
namespace hfe {
namespace ai {

SensingAgentTurretPattern::SensingAgentTurretPattern()
{
}

SensingAgentTurretPattern::~SensingAgentTurretPattern()
{
}

int SensingAgentTurretPattern::getId() const
{
    return m_id;
}

void SensingAgentTurretPattern::reset(int id)
{
    m_id = id;
}

// ELF 0x1C2B10: look in the direction, 5 degrees (0.08726646 rad) tolerance, never ending.
BotScript* SensingAgentTurretPattern::createPlan(SensingAgent* agent, Bot* bot, const Vec3& direction)
{
    SENSING_PATTERN_KEEP_OR_DROP(agent);
    BotScript* plan = 0;
    if (IPIControlInfo* const control = currentControlInfo(bot))
    {
        const int id = getId();
        const unsigned long long controls = control->getLookControls();
        BAPALookInDir* const look = checkPointer(
            new BAPALookInDir(direction, 0.08726646f, checkPointer(new BAPConFalse()), controls, id));
        look->mpOwner = this;
        plan = look;
    }
    return plan;
}

// ELF 0x1C2E50: look at the object, as above.
BotScript* SensingAgentTurretPattern::createPlan(SensingAgent* agent, Bot* bot, unsigned int target)
{
    SENSING_PATTERN_KEEP_OR_DROP(agent);
    BotScript* plan = 0;
    if (IPIControlInfo* const control = currentControlInfo(bot))
    {
        const int id = getId();
        const unsigned long long controls = control->getLookControls();
        BAPALookAtObject* const look = checkPointer(new BAPALookAtObject(
            bot, target, 0.08726646f, checkPointer(new BAPConFalse()), controls, id, 0, 0.0f));
        look->mpOwner = this;
        plan = look;
    }
    return plan;
}

} // namespace ai
} // namespace hfe
} // namespace dice
