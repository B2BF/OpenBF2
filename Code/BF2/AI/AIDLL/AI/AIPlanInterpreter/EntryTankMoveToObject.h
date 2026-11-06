// EntryTankMoveToObject: action-plan entry (split out of BAPEntries.h by S0, bodies unchanged).
// [RETIRE-WHEN: aidll-02-skeleton] Complete and validate execute.

#ifndef AI_AIDLL_AI_AIPLANINTERPRETER_ENTRYTANKMOVETOOBJECT_H
#define AI_AIDLL_AI_AIPLANINTERPRETER_ENTRYTANKMOVETOOBJECT_H

#include "BAPEntry.h"

namespace dice {
namespace hfe {
namespace ai {

// EntryTankMoveToObject - statement type 9.
class EntryTankMoveToObject : public BAPEntry
{
public:
    EntryTankMoveToObject();
    virtual ~EntryTankMoveToObject();

    virtual bool execute(BotScript *, Bot *, bool&, bool&);
    virtual int getTargetId();
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIPLANINTERPRETER_ENTRYTANKMOVETOOBJECT_H
