// Fire behaviour of infantry: BBFire with the infantry fire pattern (BBPFireInfantery, package E).
// Recovered from libaidll.so (Linux 1.5.3153); every body carries its ELF address. Package D.
// Virtual functions are in ELF vtable slot order (tools/macos/ai_behaviour_vtables.py checks them).

#ifndef AI_AIDLL_AI_AIBOTBEHAVIOUR_BBFIREINFANTERY_H
#define AI_AIDLL_AI_AIBOTBEHAVIOUR_BBFIREINFANTERY_H

#include "BBFire.h"

namespace dice {
namespace hfe {

namespace ai {

class BBPFireInfantery;
class BotMain;
class UrgeCurve;

// ELF: vtable 0x4BACA0 (15 words), operator new 0x44, package D-behaviours.
class BBFireInfantery : public BBFire
{
public:
    BBFireInfantery(unsigned char index, unsigned char queue, BBPFireInfantery* pattern, UrgeCurve* curve,
                    float* modifiers);   // 0x20F190
    virtual ~BBFireInfantery();   // 0x20F240

    virtual void initializeGenerator(BotMain*, int, bool, bool);   // 0x20F2C0, slot +0x34

private:
    // The constructor's pattern argument again, typed (BBFire keeps it as the generic pattern at +0x0C).
    /* +0x40 */ BBPFireInfantery* m_infantryPattern;
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTBEHAVIOUR_BBFIREINFANTERY_H
