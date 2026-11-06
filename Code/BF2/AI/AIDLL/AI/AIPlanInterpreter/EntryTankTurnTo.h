// Tank turning entry for BAPATurn.

#ifndef AI_AIDLL_AI_AIPLANINTERPRETER_ENTRYTANKTURNTO_H
#define AI_AIDLL_AI_AIPLANINTERPRETER_ENTRYTANKTURNTO_H

#include "BAPEntry.h"
#include "../../../../Math/Vec3.h"

namespace dice {
namespace hfe {
namespace ai {

// EntryTankTurnTo - statement type 24.
class EntryTankTurnTo : public BAPEntry
{
public:
    EntryTankTurnTo();
    virtual ~EntryTankTurnTo();

    virtual bool execute(BotScript *, Bot *, bool&, bool&);
    virtual int getTargetId();
    // Returns a midpoint relative to the first argument, using the shipped corner scores.
    void getCentreOfFacingSide(const Vec2&, const Vec2&, const Vec2&, const Vec2&, Vec2&);
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIPLANINTERPRETER_ENTRYTANKTURNTO_H
