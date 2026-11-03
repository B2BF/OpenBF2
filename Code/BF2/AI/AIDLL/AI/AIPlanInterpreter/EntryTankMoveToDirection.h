// EntryTankMoveToDirection: action-plan entry (split out of BAPEntries.h by S0, bodies unchanged).

#ifndef AI_AIDLL_AI_AIPLANINTERPRETER_ENTRYTANKMOVETODIRECTION_H
#define AI_AIDLL_AI_AIPLANINTERPRETER_ENTRYTANKMOVETODIRECTION_H

#include "BAPEntry.h"

namespace dice {
namespace hfe {
namespace ai {

// EntryTankMoveToDirection - statement type 8.
class EntryTankMoveToDirection : public BAPEntry
{
public:
    EntryTankMoveToDirection();
    virtual ~EntryTankMoveToDirection();

    virtual bool execute(BotScript *, Bot *, bool&, bool&);
    virtual int getTargetId();
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIPLANINTERPRETER_ENTRYTANKMOVETODIRECTION_H
