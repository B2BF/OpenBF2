// Fire pattern base: the per-bot target state every fire pattern shares, set by BBFire's
// initializeGenerator through the two init overloads.
// Recovered from libaidll.so (Linux 1.5.3153); every body carries its ELF address. Package D.
// Virtual functions are in ELF vtable slot order (tools/macos/ai_behaviour_vtables.py checks them).

#ifndef AI_AIDLL_AI_AIBOTBEHAVIOURPATTERN_BBPFIRE_H
#define AI_AIDLL_AI_AIBOTBEHAVIOURPATTERN_BBPFIRE_H

#include "../../../../Math/Vec3.h"
#include "BBPPattern.h"

namespace dice {
namespace hfe {

namespace ai {

class Bot;
class BotBehaviour;

// ELF: vtable 0x4BBBC0 (9 words), package D-behaviours. Abstract; the concrete patterns
// (BBPFireInfantery 0x34 bytes) start their own members at +0x28.
class BBPFire : public BBPPattern
{
public:
    BBPFire();   // 0x244910
    virtual ~BBPFire();   // 0x2441C0

    // BBPFireInfantery::init (0x25C480) stores into the arrays below; see the member comments.
    virtual void init(Bot*, unsigned int target, int weapon, bool, bool) = 0;   // slot +0x18
    virtual void init(Bot*, unsigned int target, const Vec3& position, int weapon) = 0;   // slot +0x1C
    virtual int getImplementationType();   // 0x244FC0, slot +0x20: 0

    void setUrgencyGenerator(BotBehaviour* behaviour);   // 0x244F00

protected:
    // Per bot slot (IAISettings::getMaxNBots), allocated by the constructor and not initialised there.
    // Names as agreed with package E (coord/ai-requests.md).
    /* +0x08 */ int           m_patternId;       // written by the concrete constructors (their int argument);
                                                 // compared with BAPStatement +0x10 of the current plan
    /* +0x0C */ unsigned int* m_targets;         // the firing target handle
    /* +0x10 */ bool*         m_targetChanged;   // init: the handle differs from the previous one
    /* +0x14 */ int*          m_weapons;         // init's int argument (the chosen weapon)
    /* +0x18 */ unsigned int* m_positionArgs;    // init(Vec3) overload: its handle argument
    /* +0x1C */ Vec3*         m_targetPositions; // init(Vec3) overload: the position; constructed zero
    /* +0x20 */ bool          m_flag20;          // [UNRESOLVED] not written by BBPFire; E's createPlan reads it
    /* +0x21 */ bool          m_flag21;          // [UNRESOLVED] init(Bot*, unsigned, int, bool, bool)'s second bool
                                                 // (BBPFire3d::init 0x2513C0); not read by BBPFire3d
    /* +0x24 */ BotBehaviour* m_urgencyGenerator;// the owning BBFire (setUrgencyGenerator)
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTBEHAVIOURPATTERN_BBPFIRE_H
