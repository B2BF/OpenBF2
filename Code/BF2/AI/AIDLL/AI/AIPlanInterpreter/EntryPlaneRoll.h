// Plane roll action-plan entry.

#ifndef AI_AIDLL_AI_AIPLANINTERPRETER_ENTRYPLANEROLL_H
#define AI_AIDLL_AI_AIPLANINTERPRETER_ENTRYPLANEROLL_H

#include "BAPEntry.h"

namespace dice {
namespace hfe {

namespace ai {

class Bot;
class BotScript;

// ELF: vtable 0x4BD7A8 (4 words), package H-deferred.
class EntryPlaneRoll : public BAPEntry
{
public:
    EntryPlaneRoll();   // 0x2C3410
    virtual ~EntryPlaneRoll();   // 0x2C3450

    virtual bool execute(BotScript*, Bot*, bool&, bool&);   // 0x2C34B0, slot +0x08; from the base declaration
    virtual int getTargetId();   // 0x2C3960, slot +0x0C; from the base declaration
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIPLANINTERPRETER_ENTRYPLANEROLL_H
