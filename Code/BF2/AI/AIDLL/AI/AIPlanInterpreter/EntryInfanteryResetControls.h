// EntryInfanteryResetControls: action-plan entry (split out of BAPEntries.h by S0, bodies unchanged).
// [RETIRE-WHEN: aidll-02-skeleton] Complete and validate execute.

#ifndef AI_AIDLL_AI_AIPLANINTERPRETER_ENTRYINFANTERYRESETCONTROLS_H
#define AI_AIDLL_AI_AIPLANINTERPRETER_ENTRYINFANTERYRESETCONTROLS_H

#include "BAPEntry.h"

namespace dice {
namespace hfe {
namespace ai {

// EntryInfanteryResetControls - statement type 14.
class EntryInfanteryResetControls : public BAPEntry
{
public:
    EntryInfanteryResetControls();
    virtual ~EntryInfanteryResetControls();

    virtual bool execute(BotScript *, Bot *, bool&, bool&);
    virtual int getTargetId();
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIPLANINTERPRETER_ENTRYINFANTERYRESETCONTROLS_H
