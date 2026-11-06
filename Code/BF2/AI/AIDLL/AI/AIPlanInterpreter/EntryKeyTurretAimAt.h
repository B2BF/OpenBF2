// Key-driven turret aiming entry for BAPAAimAt.

#ifndef AI_AIDLL_AI_AIPLANINTERPRETER_ENTRYKEYTURRETAIMAT_H
#define AI_AIDLL_AI_AIPLANINTERPRETER_ENTRYKEYTURRETAIMAT_H

#include "BAPEntry.h"

namespace dice {
namespace hfe {
namespace ai {

// EntryKeyTurretAimAt - statement type 6.
class EntryKeyTurretAimAt : public BAPEntry
{
public:
    EntryKeyTurretAimAt();
    virtual ~EntryKeyTurretAimAt();

    virtual bool execute(BotScript *, Bot *, bool&, bool&);
    virtual int getTargetId();
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIPLANINTERPRETER_ENTRYKEYTURRETAIMAT_H
