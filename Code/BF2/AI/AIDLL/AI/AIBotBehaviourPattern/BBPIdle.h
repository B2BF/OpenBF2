// Idle pattern base: the pattern id and the pickup-seat queries the idle patterns share.
// Recovered from libaidll.so (Linux 1.5.3153); every body carries its ELF address. Package D.
// Virtual functions are in ELF vtable slot order (tools/macos/ai_behaviour_vtables.py checks them).

#ifndef AI_AIDLL_AI_AIBOTBEHAVIOURPATTERN_BBPIDLE_H
#define AI_AIDLL_AI_AIBOTBEHAVIOURPATTERN_BBPIDLE_H

#include "BBPPattern.h"

namespace dice {
namespace hfe {

namespace ai {

// ELF: vtable 0x4BC1C0 (7 words), package D-behaviours.
class BBPIdle : public BBPPattern
{
public:
    explicit BBPIdle(int id);   // 0x28C1E0
    virtual ~BBPIdle();   // 0x28C260

    // 0 for the base; BBPIdle2d 1, BBPIdle3d 2, BBPIdleInfantery 3 (BBIdle::calculateUrgency switches on it).
    virtual int getType();   // 0x28C6D0, slot +0x18

    // True when the equipment's primary object, or one of its secondary objects (seats), has an
    // unoccupied unit.
    bool freePosition(unsigned int equipment, int side);   // 0x28C2E0
    // True when the primary object or one of its secondary objects is booked.
    bool anyBookedPositions(unsigned int equipment, int side);   // 0x28C480

protected:
    /* +0x08 */ int m_patternId;   // the constructor argument (the BBCreateFunction's pattern id)
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTBEHAVIOURPATTERN_BBPIDLE_H
