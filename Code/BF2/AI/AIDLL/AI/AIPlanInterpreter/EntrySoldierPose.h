// EntrySoldierPose: action-plan entry (split out of BAPEntries.h by S0, bodies unchanged).
// [RETIRE-WHEN: aidll-02-skeleton] Complete and validate execute.

#ifndef AI_AIDLL_AI_AIPLANINTERPRETER_ENTRYSOLDIERPOSE_H
#define AI_AIDLL_AI_AIPLANINTERPRETER_ENTRYSOLDIERPOSE_H

#include "BAPEntry.h"

namespace dice {
namespace hfe {
namespace ai {

// EntrySoldierPose - statement type 26.
class EntrySoldierPose : public BAPEntry
{
public:
    EntrySoldierPose();
    virtual ~EntrySoldierPose();

    virtual bool execute(BotScript *, Bot *, bool&, bool&);
    virtual int getTargetId();
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIPLANINTERPRETER_ENTRYSOLDIERPOSE_H
