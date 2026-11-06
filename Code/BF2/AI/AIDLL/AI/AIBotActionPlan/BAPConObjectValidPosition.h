// Tests the object's position against the selected vehicle and equipment navigation constraints.

#ifndef AI_AIDLL_AI_AIBOTACTIONPLAN_BAPCONOBJECTVALIDPOSITION_H
#define AI_AIDLL_AI_AIBOTACTIONPLAN_BAPCONOBJECTVALIDPOSITION_H

#include "BAPConObject.h"
#include "BAPTypes.h"

namespace dice {
namespace hfe {

namespace ai {

class Bot;

// ELF: vtable 0x4B99A8 (5 words), package H-deferred.
class BAPConObjectValidPosition : public BAPConObject
{
public:
    explicit BAPConObjectValidPosition(unsigned int);   // 0x1F12F0
    BAPConObjectValidPosition(unsigned int, int, int);   // 0x1F13C0
    virtual ~BAPConObjectValidPosition();   // 0x1F1420

    virtual bool evaluate(Bot*);   // 0x1F1480, slot +0x00; from the base declaration
    virtual BAPConditionType getConditionType() const;   // 0x1F15D0, slot +0x04; from the base declaration
private:
    /* +0x0C */ int m_vehicle;
    /* +0x10 */ int m_equipment;
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTACTIONPLAN_BAPCONOBJECTVALIDPOSITION_H
