// Teleport and same-vehicle seat-change pattern, recovered from libaidll.so (Linux 1.5, ia-32).

#ifndef AI_AIDLL_AI_AIBOTBEHAVIOURPATTERN_BBPCHANGETELEPORT_H
#define AI_AIDLL_AI_AIBOTBEHAVIOURPATTERN_BBPCHANGETELEPORT_H

#include "BBPPattern.h"

namespace dice {
namespace hfe {

namespace ai {

class Bot;
class BotScript;

// ELF: vtable 0x4BBB00 (6 words), operator new 0x14, package G-vehicles.
class BBPChangeTeleport : public BBPPattern
{
public:
    explicit BBPChangeTeleport(int);   // 0x23F960
    virtual ~BBPChangeTeleport();   // 0x23FC50

    virtual BotScript* createPlan(Bot*);   // 0x23FD90, slot +0x08; from the base declaration

    void init(unsigned int, bool, Bot*);   // 0x23FD20; [INFERRED] ELF void

    /* +0x08 */ int m_patternId;          // the constructor argument
    /* +0x0C */ unsigned int* m_target;   // per bot: init's handle [INFERRED] name
    /* +0x10 */ bool* m_sameVehicle;      // per bot: init's bool [INFERRED] name (as BBPChange's)
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTBEHAVIOURPATTERN_BBPCHANGETELEPORT_H
