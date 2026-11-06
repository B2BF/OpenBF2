// Recovered from libaidll.so (Linux 1.5.3153, ia-32, symbols); every body carries its ELF address.
// Virtual functions are in ELF vtable slot order (tools/macos/ai_behaviour_vtables.py checks them).

#ifndef AI_AIDLL_AI_AIBOTACTIONPLAN_BAPCONOBJECTEXISTS_H
#define AI_AIDLL_AI_AIBOTACTIONPLAN_BAPCONOBJECTEXISTS_H

#include "BAPConObject.h"
#include "BAPTypes.h"

namespace dice {
namespace hfe {

namespace ai {

class Bot;

// ELF: vtable 0x4B9698 (5 words), operator new 0xC, package F-infantry.
// True while the object has information.
class BAPConObjectExists : public BAPConObject
{
public:
    explicit BAPConObjectExists(unsigned int);   // 0x1EE240
    virtual ~BAPConObjectExists();   // 0x1EE2E0

    virtual bool evaluate(Bot*);   // 0x1EE270, slot +0x00; from the base declaration
    virtual BAPConditionType getConditionType() const;   // 0x1EE340, slot +0x04; from the base declaration
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTACTIONPLAN_BAPCONOBJECTEXISTS_H
