// Package F (ai-w1-f-urgency): recovered from libaidll.so 1.5.3153 (ia-32); ELF addresses on every body.
// Virtual functions are in ELF vtable slot order (tools/macos/ai_behaviour_vtables.py checks them).

#ifndef AI_AIDLL_AI_AIBOTACTIONPLAN_BAPAUPDATEVEHICLE_H
#define AI_AIDLL_AI_AIBOTACTIONPLAN_BAPAUPDATEVEHICLE_H

#include "BAPAction.h"
#include "BAPTypes.h"

namespace dice {
namespace hfe {

namespace ai {

class Bot;

// ELF: vtable 0x4B8E00 (8 words), operator new 0x14, package F-infantry.
// Asks the bot to re-read its current equipment (BotMain::setUpdateCurrentEquipment) after a change.
// The constructor argument is the creating pattern's id (BAPStatement +0x10).
class BAPAUpdateVehicle : public BAPAction
{
public:
    explicit BAPAUpdateVehicle(int);   // 0x1E2C30
    virtual ~BAPAUpdateVehicle();   // 0x1E2CC0

    virtual bool execute(Bot*, bool&, bool&);   // 0x1E2D50, slot +0x00; from the base declaration
    virtual unsigned long long getControls();   // 0x1E2E50, slot +0x0C; from the base declaration
    virtual BAPStatementType getId() const;   // 0x1E2E60, slot +0x10; from the base declaration
    virtual bool hasDefault();   // 0x1E2D90, slot +0x14; from the base declaration
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTACTIONPLAN_BAPAUPDATEVEHICLE_H
