// Look statement along the facing of the bot's vehicle.
// Recovered from libaidll.so (Linux 1.5.3153, ia-32, symbols); every body carries its ELF address.
// Virtual functions are in ELF vtable slot order (tools/macos/ai_behaviour_vtables.py checks them).

#ifndef AI_AIDLL_AI_AIBOTACTIONPLAN_BAPALOOKAHEAD_H
#define AI_AIDLL_AI_AIBOTACTIONPLAN_BAPALOOKAHEAD_H

#include "../../../../Math/Vec3.h"
#include "BAPALook.h"

namespace dice {
namespace hfe {

namespace ai {

class BAPCondition;
class Bot;

// ELF: vtable 0x4B7D40 (9 words), package G-vehicles. Looks along the facing of the bot's equipment
// at construction time.
class BAPALookAhead : public BAPALook
{
public:
    BAPALookAhead(Bot*, float, BAPCondition*, unsigned long long, int);   // 0x1D2E60
    virtual ~BAPALookAhead();   // 0x1D2F30

    virtual Vec3 getLookDirection(bool&);   // 0x1D2FE0, slot +0x20; from the base declaration

    /* +0x30 */ unsigned int m_equipment;   // the bot's current equipment handle at construction
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTACTIONPLAN_BAPALOOKAHEAD_H
