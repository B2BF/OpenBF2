// Random grenade-warning reaction, recovered from libaidll.so.

#ifndef AI_AIDLL_AI_AIBOTBEHAVIOURPATTERN_BBPRANDOM_H
#define AI_AIDLL_AI_AIBOTBEHAVIOURPATTERN_BBPRANDOM_H

#include "BBPPattern.h"

namespace dice {
namespace hfe {

namespace ai {

class Bot;
class BotScript;

// ELF: vtable 0x4BC4A0 (6 words), operator new 0x1C, package F-infantry.
class BBPRandom : public BBPPattern
{
public:
    explicit BBPRandom(int);   // 0x297580
    virtual ~BBPRandom();   // 0x297750

    virtual BotScript* createPlan(Bot*);   // 0x297840, slot +0x08; from the base declaration

    void init(Bot*);   // 0x2977F0; void (fstpl is the last instruction before the epilogue)

private:
    /* +0x08 */ int m_id;                          // constructor argument [INFERRED] name
    /* +0x0C */ unsigned int* m_perBot;            // new[getMaxNBots], checkPointer'd [UNRESOLVED] use
    /* +0x10 */ unsigned char m_unwritten10[4];    // not written by the constructor or init
    /* +0x14 */ double m_initTime;                 // init: IBFTimer::getGameTime [INFERRED] name
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTBEHAVIOURPATTERN_BBPRANDOM_H
