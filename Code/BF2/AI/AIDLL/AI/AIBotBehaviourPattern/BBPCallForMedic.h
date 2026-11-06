// Medic-request radio pattern, recovered from libaidll.so (Linux 1.5, ia-32).

#ifndef AI_AIDLL_AI_AIBOTBEHAVIOURPATTERN_BBPCALLFORMEDIC_H
#define AI_AIDLL_AI_AIBOTBEHAVIOURPATTERN_BBPCALLFORMEDIC_H

#include "BBPPattern.h"

namespace dice {
namespace hfe {

namespace ai {

class Bot;
class BotScript;

// ELF: vtable 0x4BB980 (6 words), operator new 0xC, package H-deferred.
class BBPCallForMedic : public BBPPattern
{
public:
    explicit BBPCallForMedic(int);   // 0x235320
    virtual ~BBPCallForMedic();   // 0x2353A0

    virtual BotScript* createPlan(Bot*);   // 0x235450, slot +0x08; from the base declaration

    void init(Bot*);   // 0x235420; [INFERRED] ELF void

private:
    /* +0x08 */ int m_patternId;   // [INFERRED] name: id passed to the generated radio statement
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTBEHAVIOURPATTERN_BBPCALLFORMEDIC_H
