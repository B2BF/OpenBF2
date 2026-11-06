// Medium-LOD vehicle movement towards an object.

#ifndef AI_AIDLL_AI_AIPLANINTERPRETER_ENTRYMOVETOOBJECTMEDIUM_H
#define AI_AIDLL_AI_AIPLANINTERPRETER_ENTRYMOVETOOBJECTMEDIUM_H

#include "BAPEntry.h"

namespace dice {
namespace hfe {
namespace ai {

// EntryMoveToObjectMedium - statement type 9.
class EntryMoveToObjectMedium : public BAPEntry
{
public:
    EntryMoveToObjectMedium();
    virtual ~EntryMoveToObjectMedium();

    virtual bool execute(BotScript *, Bot *, bool&, bool&);
    virtual int getTargetId();
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIPLANINTERPRETER_ENTRYMOVETOOBJECTMEDIUM_H
