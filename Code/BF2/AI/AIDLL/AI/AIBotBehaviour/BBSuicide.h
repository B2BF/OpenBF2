// Man-down suicide urgency and pattern initialization.

#ifndef AI_AIDLL_AI_AIBOTBEHAVIOUR_BBSUICIDE_H
#define AI_AIDLL_AI_AIBOTBEHAVIOUR_BBSUICIDE_H

#include "BotBehaviour.h"

namespace dice {
namespace hfe {

namespace ai {

class BBPSuicide;
class Bot;
class UrgeCurve;

// ELF: vtable 0x4BB220 (13 words), operator new 0x24, package H-deferred.
class BBSuicide : public BotBehaviour
{
public:
    BBSuicide(unsigned char, unsigned char, BBPSuicide*, UrgeCurve*, float*);   // 0x21D510
    virtual ~BBSuicide();   // 0x21D600

    virtual float calculateUrgency(Bot*, bool, float);   // 0x21D680, slot +0x28; from the base declaration
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTBEHAVIOUR_BBSUICIDE_H
