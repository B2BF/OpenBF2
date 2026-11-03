// Medium-LOD vehicle movement entry.

#ifndef AI_AIDLL_AI_AIPLANINTERPRETER_ENTRYMOVETOMEDIUM_H
#define AI_AIDLL_AI_AIPLANINTERPRETER_ENTRYMOVETOMEDIUM_H

#include "BAPEntry.h"

namespace dice {
namespace hfe {
namespace ai {

// EntryMoveToMedium - statement type 7.
class EntryMoveToMedium : public BAPEntry
{
public:
    EntryMoveToMedium();
    virtual ~EntryMoveToMedium();

    virtual bool execute(BotScript *, Bot *, bool&, bool&);
    virtual int getTargetId();
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIPLANINTERPRETER_ENTRYMOVETOMEDIUM_H
