// Recovered by package A-core (docs/macos-ai-behaviour-plan.md) from libaidll.so; the virtual order
// is checked by tools/macos/ai_behaviour_vtables.py. Every body carries its ELF address.

#ifndef AI_AIDLL_AI_AIBOTACTIONPLAN_BAPCONNOT_H
#define AI_AIDLL_AI_AIBOTACTIONPLAN_BAPCONNOT_H

#include <iosfwd>
#include <list>
#include "BAPBoleanOp.h"
#include "BAPTypes.h"

namespace dice {
namespace hfe {

namespace ai {

class BAPCondition;
class Bot;

// ELF: vtable 0x4B9EA0 (6 words), operator new 0xC/0x20/0x2C, package A-core.
// Negates the first condition of the list (evaluate reads front() without an empty check, as
// the image does). addCondition only accepts the first one; the list constructor takes only the
// list's front. Owns its condition. sizeof 0xC.
class BAPConNot : public BAPBoleanOp
{
public:
    explicit BAPConNot(BAPCondition*);   // 0x1F46E0
    explicit BAPConNot(std::list<BAPCondition*>&);   // 0x1F4850
    BAPConNot();   // 0x1F4990
    virtual ~BAPConNot();   // 0x1F4AB0

    virtual bool evaluate(Bot*);   // 0x1F4C20, slot +0x00; from the base declaration
    virtual BAPConditionType getConditionType() const;   // 0x1F4CC0, slot +0x04; from the base declaration
    virtual void streamSelf(std::ostream&) const;   // 0x1F4CD0, slot +0x08; from the base declaration
    virtual void addCondition(BAPCondition*);   // 0x1F4C50, slot +0x14; from the base declaration
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTACTIONPLAN_BAPCONNOT_H
