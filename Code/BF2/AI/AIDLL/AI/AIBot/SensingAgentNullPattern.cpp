// [INFERRED] Source placement and file name: the ELF gives the null pattern its own TU (own static
// init below, between the MouseFreeLook and Soldier TUs); AIDLL_r.dll has no assert in it, so no
// __FILE__ names it. Its three siblings are SensingAgentMouseFreeLookPattern.cpp,
// SensingAgentSoldierPattern.cpp and SensingAgentTurretPattern.cpp (AIDLL_r __FILE__). Split out of
// the former SensingAgentPatterns.cpp; the functions are unchanged.
#include "SensingAgentPatterns.h"
#include <iostream>
// static init: ELF 0x1C1DD0, 0x1C1E30, 0x1C1E60 (Null)

#include "SensingAgentPatternPrologue.h"
#include "../AIBotActionPlan/BAPAResetControls.h"

namespace dice {
namespace hfe {
namespace ai {

SensingAgentNullPattern::SensingAgentNullPattern()
{
}

SensingAgentNullPattern::~SensingAgentNullPattern()
{
}

int SensingAgentNullPattern::getId() const
{
    return m_id;
}

void SensingAgentNullPattern::reset(int id)
{
    m_id = id;
}

// ELF 0x1C1A70: BAPAResetControls(false, true, false, id) (0x1C bytes), owned by the pattern.
BotScript* SensingAgentNullPattern::createPlan(SensingAgent* agent, Bot* /*bot*/, const Vec3& /*direction*/)
{
    SENSING_PATTERN_KEEP_OR_DROP(agent);
    BAPAResetControls* const reset = checkPointer(new BAPAResetControls(false, true, false, getId()));
    reset->mpOwner = this;
    return reset;
}

// ELF 0x1C1C20: identical to the direction overload.
BotScript* SensingAgentNullPattern::createPlan(SensingAgent* agent, Bot* /*bot*/, unsigned int /*target*/)
{
    SENSING_PATTERN_KEEP_OR_DROP(agent);
    BAPAResetControls* const reset = checkPointer(new BAPAResetControls(false, true, false, getId()));
    reset->mpOwner = this;
    return reset;
}

} // namespace ai
} // namespace hfe
} // namespace dice
