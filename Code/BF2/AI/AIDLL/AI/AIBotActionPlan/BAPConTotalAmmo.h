// Condition: the product of the object's ammunition fractions is at most a threshold. Recovered from libaidll.so (Linux 1.5.3153, ia-32, symbols).

#ifndef AI_AIDLL_AI_AIBOTACTIONPLAN_BAPCONTOTALAMMO_H
#define AI_AIDLL_AI_AIBOTACTIONPLAN_BAPCONTOTALAMMO_H

#include "BAPConObject.h"
#include "BAPTypes.h"

namespace dice {
namespace hfe {

namespace ai {

class Bot;

// ELF: vtable 0x4B9AA8 (5 words), operator new 0x10, package F-infantry.
class BAPConTotalAmmo : public BAPConObject
{
public:
    BAPConTotalAmmo(unsigned int, float);   // 0x1F1C30
    virtual ~BAPConTotalAmmo();   // 0x1F1E50

    virtual bool evaluate(Bot*);   // 0x1F1C70, slot +0x00; from the base declaration
    virtual BAPConditionType getConditionType() const;   // 0x1F1EB0, slot +0x04; from the base declaration

private:
    /* +0x0C */ float m_threshold;   // the constructor's float
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTACTIONPLAN_BAPCONTOTALAMMO_H
