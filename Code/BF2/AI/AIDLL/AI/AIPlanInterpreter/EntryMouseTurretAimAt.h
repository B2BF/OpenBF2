// EntryMouseTurretAimAt: action-plan entry (split out of BAPEntries.h by S0, bodies unchanged).
// [RETIRE-WHEN: aidll-02-skeleton] Complete and validate execute.

#ifndef AI_AIDLL_AI_AIPLANINTERPRETER_ENTRYMOUSETURRETAIMAT_H
#define AI_AIDLL_AI_AIPLANINTERPRETER_ENTRYMOUSETURRETAIMAT_H

#include "BAPEntry.h"

namespace dice {
namespace hfe {
namespace ai {

// EntryMouseTurretAimAt - statement type 6.
class EntryMouseTurretAimAt : public BAPEntry
{
public:
    EntryMouseTurretAimAt();
    virtual ~EntryMouseTurretAimAt();

    virtual bool execute(BotScript *, Bot *, bool&, bool&);
    virtual int getTargetId();
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIPLANINTERPRETER_ENTRYMOUSETURRETAIMAT_H
