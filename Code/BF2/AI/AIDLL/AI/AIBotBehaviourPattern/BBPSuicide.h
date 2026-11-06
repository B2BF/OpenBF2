// Suicide input pattern, recovered from libaidll.so (Linux 1.5, ia-32).

#ifndef AI_AIDLL_AI_AIBOTBEHAVIOURPATTERN_BBPSUICIDE_H
#define AI_AIDLL_AI_AIBOTBEHAVIOURPATTERN_BBPSUICIDE_H

#include "BBPPattern.h"

namespace dice {
namespace hfe {

namespace ai {

class Bot;
class BotScript;

// ELF: vtable 0x4BC760 (6 words), operator new 0xC, package H-deferred.
class BBPSuicide : public BBPPattern
{
public:
    explicit BBPSuicide(int);   // 0x29CBD0
    virtual ~BBPSuicide();   // 0x29CC50

    virtual BotScript* createPlan(Bot*);   // 0x29CD00, slot +0x08; from the base declaration

    void init(Bot*);   // 0x29CCD0; [INFERRED] ELF void

private:
    // [INFERRED] member name from the constructor and generated trigger.
    /* +0x08 */ int m_patternId;   // constructor argument carried by the trigger
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTBEHAVIOURPATTERN_BBPSUICIDE_H
