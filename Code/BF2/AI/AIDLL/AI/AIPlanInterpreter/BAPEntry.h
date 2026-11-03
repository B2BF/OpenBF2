// Action-plan primitive interface. Preserve virtual declaration order.

#ifndef AI_AIDLL_AI_AIPLANINTERPRETER_BAPENTRY_H
#define AI_AIDLL_AI_AIPLANINTERPRETER_BAPENTRY_H

#include "../Main/AIDllTypes.h"

namespace dice {
namespace hfe {
namespace ai {

class Bot;
class BotScript;

class BAPEntry
{
public:
    BAPEntry() {}
    virtual ~BAPEntry() {}   // weak in the image (emitted in EntryBoatMoveTo.cpp)

    // execute reports success as bool; getTargetId returns the entry's integer target id.
    virtual bool execute(BotScript *, Bot *, bool&, bool&) = 0;
    virtual int getTargetId() = 0;
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIPLANINTERPRETER_BAPENTRY_H
