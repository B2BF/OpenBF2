// Condition: the object is man down. Recovered from libaidll.so (Linux 1.5.3153, ia-32, symbols).

#ifndef AI_AIDLL_AI_AIBOTACTIONPLAN_BAPCONOBJECTMANDOWN_H
#define AI_AIDLL_AI_AIBOTACTIONPLAN_BAPCONOBJECTMANDOWN_H

#include "BAPConObject.h"
#include "BAPTypes.h"

namespace dice {
namespace hfe {

namespace ai {

class Bot;

// ELF: vtable 0x4B97B8 (5 words), operator new 0xC, package F-infantry.
class BAPConObjectManDown : public BAPConObject
{
public:
    explicit BAPConObjectManDown(unsigned int);   // 0x1EFA20
    virtual ~BAPConObjectManDown();   // 0x1EFA70

    virtual bool evaluate(Bot*);   // 0x1EFAD0, slot +0x00; from the base declaration
    virtual BAPConditionType getConditionType() const;   // 0x1EFB40, slot +0x04; from the base declaration
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTACTIONPLAN_BAPCONOBJECTMANDOWN_H
