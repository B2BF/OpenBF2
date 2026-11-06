// Tests the sensed object's visible flag while excluding indirect sensing.

#ifndef AI_AIDLL_AI_AIBOTACTIONPLAN_BAPCONOBJECTVISIBLE_H
#define AI_AIDLL_AI_AIBOTACTIONPLAN_BAPCONOBJECTVISIBLE_H

#include <iosfwd>
#include "BAPConObject.h"
#include "BAPTypes.h"

namespace dice {
namespace hfe {

namespace ai {

class Bot;

// ELF: vtable 0x4B9A40 (6 words), operator new 0xC, package H-deferred.
class BAPConObjectVisible : public BAPConObject
{
public:
    explicit BAPConObjectVisible(unsigned int);   // 0x1F19B0
    virtual ~BAPConObjectVisible();   // 0x1F1A00

    virtual bool evaluate(Bot*);   // 0x1F1A60, slot +0x00; from the base declaration
    virtual BAPConditionType getConditionType() const;   // 0x1F1AA0, slot +0x04; from the base declaration
    virtual void operator<<(std::ostream&);   // 0x1F1AB0, slot +0x14; [INFERRED] ELF void
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTACTIONPLAN_BAPCONOBJECTVISIBLE_H
