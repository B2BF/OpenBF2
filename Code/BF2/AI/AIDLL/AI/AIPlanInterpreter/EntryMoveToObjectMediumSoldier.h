// EntryMoveToObjectMediumSoldier: action-plan entry (split out of BAPEntries.h by S0, bodies unchanged).
// [RETIRE-WHEN: aidll-02-skeleton] Complete and validate execute.

#ifndef AI_AIDLL_AI_AIPLANINTERPRETER_ENTRYMOVETOOBJECTMEDIUMSOLDIER_H
#define AI_AIDLL_AI_AIPLANINTERPRETER_ENTRYMOVETOOBJECTMEDIUMSOLDIER_H

#include "BAPEntry.h"

namespace dice {
namespace hfe {
namespace ai {

// EntryMoveToObjectMediumSoldier - statement type 9.
class EntryMoveToObjectMediumSoldier : public BAPEntry
{
public:
    EntryMoveToObjectMediumSoldier();
    virtual ~EntryMoveToObjectMediumSoldier();

    virtual bool execute(BotScript *, Bot *, bool&, bool&);
    virtual int getTargetId();
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIPLANINTERPRETER_ENTRYMOVETOOBJECTMEDIUMSOLDIER_H
