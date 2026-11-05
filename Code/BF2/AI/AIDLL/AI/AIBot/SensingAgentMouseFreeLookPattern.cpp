// Source placement: AIDLL_r.dll __FILE__ "Code\BF2\AI\AIDLL\AI\AIBot\SensingAgentMouseFreeLookPattern.cpp"
// (asserts in 0x10136CD0 and 0x10137020, e.g. "lookDir vector empty!"). Split out of the former
// SensingAgentPatterns.cpp; the functions are unchanged.
#include "SensingAgentPatterns.h"
#include <iostream>
// static init: ELF 0x1C1780, 0x1C17E0, 0x1C1810 (MouseFreeLook)

#include "SensingAgentPatternPrologue.h"
#include "../AIBotActionPlan/BAPALookAtObject.h"
#include "../AIBotActionPlan/BAPALookInDir.h"
#include "../AIBotActionPlan/BAPCombinerParallel.h"

namespace dice {
namespace hfe {
namespace ai {

// The base owns the shared id field used by every pattern's getId/reset pair.
// Constructors and destructors perform no additional member initialization or cleanup.
// [INFERRED] placement: weak in the ELF, emitted right after this TU's static init (0x1C1830).
ISensingAgentPattern::~ISensingAgentPattern()
{
}

SensingAgentMouseFreeLookPattern::SensingAgentMouseFreeLookPattern()
{
}

SensingAgentMouseFreeLookPattern::~SensingAgentMouseFreeLookPattern()
{
}

int SensingAgentMouseFreeLookPattern::getId() const
{
    return m_id;
}

void SensingAgentMouseFreeLookPattern::reset(int id)
{
    m_id = id;
}

// ELF 0x1C0560: hold the mouse-look button and look in the direction, in parallel.
BotScript* SensingAgentMouseFreeLookPattern::createPlan(SensingAgent* agent, Bot* bot, const Vec3& direction)
{
    SENSING_PATTERN_KEEP_OR_DROP(agent);
    BotScript* plan = 0;
    if (IPIControlInfo* const control = currentControlInfo(bot))
    {
        BAPATriggerContinously* const trigger = holdMouseLook(getId());
        const int id = getId();
        const unsigned long long controls = control->getLookControls();
        BAPALookInDir* const look = checkPointer(
            new BAPALookInDir(direction, 0.08726646f, checkPointer(new BAPConFalse()), controls, id));
        BAPCombinerParallel* const parallel = checkPointer(new BAPCombinerParallel(getId(), -1, false));
        parallel->addStatement(trigger);
        parallel->addStatement(look);
        parallel->mpOwner = this;
        plan = parallel;
    }
    return plan;
}

// ELF 0x1C0E60: as above, looking at the object.
BotScript* SensingAgentMouseFreeLookPattern::createPlan(SensingAgent* agent, Bot* bot, unsigned int target)
{
    SENSING_PATTERN_KEEP_OR_DROP(agent);
    BotScript* plan = 0;
    if (IPIControlInfo* const control = currentControlInfo(bot))
    {
        BAPATriggerContinously* const trigger = holdMouseLook(getId());
        const int id = getId();
        const unsigned long long controls = control->getLookControls();
        BAPALookAtObject* const look = checkPointer(new BAPALookAtObject(
            bot, target, 0.08726646f, checkPointer(new BAPConFalse()), controls, id, 0, 0.0f));
        BAPCombinerParallel* const parallel = checkPointer(new BAPCombinerParallel(getId(), -1, false));
        parallel->addStatement(trigger);
        parallel->addStatement(look);
        parallel->mpOwner = this;
        plan = parallel;
    }
    return plan;
}

} // namespace ai
} // namespace hfe
} // namespace dice
