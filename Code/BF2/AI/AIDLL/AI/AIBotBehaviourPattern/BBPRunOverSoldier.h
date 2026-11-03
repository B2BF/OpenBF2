// Recovered from libaidll.so 1.5.3153 (ia-32); ELF addresses on every declaration.
// Vehicle pattern: run over a soldier with a car (BAPARunOverObject under a fire wrapper).

#ifndef AI_AIDLL_AI_AIBOTBEHAVIOURPATTERN_BBPRUNOVERSOLDIER_H
#define AI_AIDLL_AI_AIBOTBEHAVIOURPATTERN_BBPRUNOVERSOLDIER_H

#include "BBPPattern.h"

namespace dice {
namespace hfe {

namespace ai {

class Bot;
class BotScript;

// ELF: vtable 0x4BC6E0 (6 words), operator new 0x10, package G-vehicles.
// [UNRESOLVED] data members: the owning package names them from the constructors.
class BBPRunOverSoldier : public BBPPattern
{
public:
    explicit BBPRunOverSoldier(int);   // 0x29C2D0
    virtual ~BBPRunOverSoldier();   // 0x29C4A0

    virtual BotScript* createPlan(Bot*);   // 0x29C580, slot +0x08; from the base declaration

    void init(Bot*, unsigned int target);   // 0x29C540; ELF void (no eax write)

    /* +0x08 */ int m_patternId;        // the constructor argument
    /* +0x0C */ unsigned int* m_target; // per bot: init's handle (init also sets m_initiated) [INFERRED] name
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTBEHAVIOURPATTERN_BBPRUNOVERSOLDIER_H
