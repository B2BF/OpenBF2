// Recovered by package A-core (docs/macos-ai-behaviour-plan.md) from libaidll.so; the virtual order
// is checked by tools/macos/ai_behaviour_vtables.py. Every body carries its ELF address.

#ifndef AI_AIDLL_AI_AIBOTACTIONPLAN_BAPCONAND_H
#define AI_AIDLL_AI_AIBOTACTIONPLAN_BAPCONAND_H

#include <iosfwd>
#include <list>
#include "BAPBoleanOp.h"
#include "BAPTypes.h"

namespace dice {
namespace hfe {

namespace ai {

class BAPCondition;
class Bot;

// ELF: vtable 0x4B9E40 (6 words), operator new 0x8/0xC, package A-core.
// True when every condition is (an empty And is true); stops at the first false. Owns its
// conditions (BAPBoleanOp::m_conditions). sizeof 0xC.
class BAPConAnd : public BAPBoleanOp
{
public:
    explicit BAPConAnd(std::list<BAPCondition*>&);   // 0x1F3C80
    BAPConAnd();   // 0x1F3DA0
    virtual ~BAPConAnd();   // 0x1F3EC0

    virtual bool evaluate(Bot*);   // 0x1F4030, slot +0x00; from the base declaration
    virtual BAPConditionType getConditionType() const;   // 0x1F4110, slot +0x04; from the base declaration
    virtual void streamSelf(std::ostream&) const;   // 0x1F4120, slot +0x08; from the base declaration
    virtual void addCondition(BAPCondition*);   // 0x1F40A0, slot +0x14; from the base declaration
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTACTIONPLAN_BAPCONAND_H
