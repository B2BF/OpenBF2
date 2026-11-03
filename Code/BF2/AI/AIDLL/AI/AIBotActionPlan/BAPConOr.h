// Recovered by package A-core (docs/macos-ai-behaviour-plan.md) from libaidll.so; the virtual order
// is checked by tools/macos/ai_behaviour_vtables.py. Every body carries its ELF address.

#ifndef AI_AIDLL_AI_AIBOTACTIONPLAN_BAPCONOR_H
#define AI_AIDLL_AI_AIBOTACTIONPLAN_BAPCONOR_H

#include <iosfwd>
#include <list>
#include "BAPBoleanOp.h"
#include "BAPTypes.h"

namespace dice {
namespace hfe {

namespace ai {

class BAPCondition;
class Bot;

// ELF: vtable 0x4B9EE0 (6 words), operator new 0xC, package A-core.
// True when any condition is (an empty Or is false); stops at the first true. Owns its
// conditions (BAPBoleanOp::m_conditions). sizeof 0xC.
class BAPConOr : public BAPBoleanOp
{
public:
    explicit BAPConOr(std::list<BAPCondition*>&);   // 0x1F4EA0
    BAPConOr();   // 0x1F4FC0
    virtual ~BAPConOr();   // 0x1F50E0

    virtual bool evaluate(Bot*);   // 0x1F5250, slot +0x00; from the base declaration
    virtual BAPConditionType getConditionType() const;   // 0x1F5330, slot +0x04; from the base declaration
    virtual void streamSelf(std::ostream&) const;   // 0x1F5340, slot +0x08; from the base declaration
    virtual void addCondition(BAPCondition*);   // 0x1F52C0, slot +0x14; from the base declaration
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTACTIONPLAN_BAPCONOR_H
