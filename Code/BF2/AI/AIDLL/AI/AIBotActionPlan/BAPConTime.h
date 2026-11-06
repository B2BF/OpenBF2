// Recovered by package A-core (docs/macos-ai-behaviour-plan.md) from libaidll.so; the virtual order
// is checked by tools/macos/ai_behaviour_vtables.py. Every body carries its ELF address.

#ifndef AI_AIDLL_AI_AIBOTACTIONPLAN_BAPCONTIME_H
#define AI_AIDLL_AI_AIBOTACTIONPLAN_BAPCONTIME_H

#include "BAPCondition.h"
#include "BAPTypes.h"

namespace dice {
namespace hfe {

namespace ai {

class Bot;

// ELF: vtable 0x4B9498 (5 words), operator new 0x18/0x1C, package A-core.
// True once the game time (IBFTimer::getGameTime, CommonSingletonServer::m_iBFTimer) reaches the
// deadline; then it re-arms: the next evaluate sets deadline = now + duration. sizeof 0x1C
// (`operator new(0x1C)`; 0x18 at other sites rounds the 0x1A of fields).
class BAPConTime : public BAPCondition
{
public:
    // fireFirst: the first evaluate is already true (deadline -1). armed (or fireFirst): the deadline
    // counts from construction; otherwise from the first evaluate.
    BAPConTime(double duration, bool flag19, bool armed, bool fireFirst);   // 0x1EAD50
    virtual ~BAPConTime();   // 0x1EAE10

    virtual bool evaluate(Bot*);   // 0x1EAE70, slot +0x00; from the base declaration
    virtual BAPConditionType getConditionType() const;   // 0x1EAEE0, slot +0x04; from the base declaration

private:
    /* +0x08 */ double m_deadline;
    /* +0x10 */ double m_duration;
    /* +0x18 */ bool m_armed;     // [INFERRED] name
    /* +0x19 */ bool m_flag19;    // first bool argument; no recovered function reads it [UNRESOLVED] meaning
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTACTIONPLAN_BAPCONTIME_H
