// Runs the sensing statement until either owned condition completes it.

#ifndef AI_AIDLL_AI_AIPLANINTERPRETER_ENTRYSENSE_H
#define AI_AIDLL_AI_AIPLANINTERPRETER_ENTRYSENSE_H

#include "BAPEntry.h"

namespace dice {
namespace hfe {
namespace ai {

// EntrySense - statement type 20.
class EntrySense : public BAPEntry
{
public:
    EntrySense();
    virtual ~EntrySense();

    virtual bool execute(BotScript *, Bot *, bool&, bool&);
    virtual int getTargetId();
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIPLANINTERPRETER_ENTRYSENSE_H
