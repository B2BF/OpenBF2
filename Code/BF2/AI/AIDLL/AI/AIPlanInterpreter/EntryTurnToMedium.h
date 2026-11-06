// Medium-LOD turning entry for BAPATurn.

#ifndef AI_AIDLL_AI_AIPLANINTERPRETER_ENTRYTURNTOMEDIUM_H
#define AI_AIDLL_AI_AIPLANINTERPRETER_ENTRYTURNTOMEDIUM_H

#include "BAPEntry.h"

namespace dice {
namespace hfe {

namespace ai {

class Bot;
class BotScript;

// ELF: vtable 0x4BDB68; fieldless BAPEntry subclass.
class EntryTurnToMedium : public BAPEntry
{
public:
    EntryTurnToMedium();   // 0x2C7DB0
    virtual ~EntryTurnToMedium();   // 0x2C7DF0

    virtual bool execute(BotScript*, Bot*, bool&, bool&);   // 0x2C7E50, slot +0x08; from the base declaration
    virtual int getTargetId();   // 0x2C8A70, slot +0x0C; from the base declaration
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIPLANINTERPRETER_ENTRYTURNTOMEDIUM_H
