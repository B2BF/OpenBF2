// End-of-path condition. The retail evaluator is an unconditional false.

#ifndef AI_AIDLL_AI_AIBOTACTIONPLAN_BAPCONENDOFPATH_H
#define AI_AIDLL_AI_AIBOTACTIONPLAN_BAPCONENDOFPATH_H

#include "BAPCondition.h"
#include "BAPTypes.h"

namespace dice {
namespace hfe {

namespace ai {

class Bot;

// ELF: vtable 0x4B9190 (5 words), package H-deferred.
class BAPConEndOfPath : public BAPCondition
{
public:
    virtual ~BAPConEndOfPath();   // 0x1E7230

    virtual bool evaluate(Bot*);   // 0x1E7170, slot +0x00; from the base declaration
    virtual BAPConditionType getConditionType() const;   // 0x1E7290, slot +0x04; from the base declaration
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTACTIONPLAN_BAPCONENDOFPATH_H
