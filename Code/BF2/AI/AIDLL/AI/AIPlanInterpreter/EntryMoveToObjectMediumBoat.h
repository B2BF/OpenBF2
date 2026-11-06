// Medium-LOD boat movement towards an object.

#ifndef AI_AIDLL_AI_AIPLANINTERPRETER_ENTRYMOVETOOBJECTMEDIUMBOAT_H
#define AI_AIDLL_AI_AIPLANINTERPRETER_ENTRYMOVETOOBJECTMEDIUMBOAT_H

#include "BAPEntry.h"

namespace dice {
namespace hfe {
namespace ai {

// EntryMoveToObjectMediumBoat - statement type 9.
class EntryMoveToObjectMediumBoat : public BAPEntry
{
public:
    EntryMoveToObjectMediumBoat();
    virtual ~EntryMoveToObjectMediumBoat();

    virtual bool execute(BotScript *, Bot *, bool&, bool&);
    virtual int getTargetId();
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIPLANINTERPRETER_ENTRYMOVETOOBJECTMEDIUMBOAT_H
