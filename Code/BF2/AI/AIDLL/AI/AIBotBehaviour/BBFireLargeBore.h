// Large-bore fire behaviour, including driver-controlled and stationary guns.
// Recovered from the symbolized Linux 1.50 libaidll.so; offsets refer to the ia-32 image.

#ifndef AI_AIDLL_AI_AIBOTBEHAVIOUR_BBFIRELARGEBORE_H
#define AI_AIDLL_AI_AIBOTBEHAVIOUR_BBFIRELARGEBORE_H

#include "BotBehaviour.h"

#include <map>

namespace dice {
namespace hfe {

namespace ai {

class BBPFire;
class Bot;
class BotMain;
class IPIMobile;
class Information;
class UrgeCurve;

// ELF: vtable 0x4BAD40 (14 words), operator new 0x40, package G-vehicles.
class BBFireLargeBore : public BotBehaviour
{
public:
    struct BBPFeedback
    {
        double m_time; // ELF 0x212CD0: sole member, initialized to -10000.0
    };
    BBFireLargeBore(unsigned char, unsigned char, BBPFire*, UrgeCurve*, float*, bool);   // 0x20F7E0
    virtual ~BBFireLargeBore();   // 0x20FC40

    virtual float calculateUrgency(Bot*, bool, float);   // 0x20FDC0, slot +0x28; from the base declaration
    virtual void initializeGenerator(BotMain*, int, bool, bool, bool);   // 0x212BE0, slot +0x34; [INFERRED] ELF void

    float calculateUrgency(Bot*, bool, float, Information*, IPIMobile*);   // 0x20FE50; [INFERRED] ELF float, callers {'float': 1}
    BBPFeedback* getBBPFeedback(unsigned int);   // 0x212CD0

    // Fields from the constructor 0x20F7E0 and destructor 0x20FC40 (operator new 0x40).
    /* +0x24 */ unsigned int* m_unknown24;   // per-bot weapon slot; initialized to -1
    /* +0x28 */ double m_double28;           // last feedback cleanup time; initialized to 0.0
    /* +0x30 */ std::map<unsigned int, BBPFeedback*> m_feedback;   // the destructor only frees the nodes [INFERRED] name
    /* +0x3C */ bool m_flag3C;               // use the first secondary object's armament
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTBEHAVIOUR_BBFIRELARGEBORE_H
