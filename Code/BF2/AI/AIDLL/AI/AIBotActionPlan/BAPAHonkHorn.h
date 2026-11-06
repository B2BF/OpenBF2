// Requests a horn notification for one information handle; execution returns false as shipped.

#ifndef AI_AIDLL_AI_AIBOTACTIONPLAN_BAPAHONKHORN_H
#define AI_AIDLL_AI_AIBOTACTIONPLAN_BAPAHONKHORN_H

#include "BAPAction.h"
#include "BAPTypes.h"

namespace dice {
namespace hfe {

namespace ai {

class Bot;

// ELF: vtable 0x4B7C80 (8 words), operator new 0x18, package G-vehicles.
class BAPAHonkHorn : public BAPAction
{
public:
    BAPAHonkHorn(unsigned int, int);   // 0x1D25D0
    virtual ~BAPAHonkHorn();   // 0x1D2670

    virtual bool execute(Bot*, bool&, bool&);   // 0x1D2700, slot +0x00; from the base declaration
    virtual BAPStatementType getId() const;   // 0x1D2830, slot +0x10; from the base declaration
    virtual bool hasDefault();   // 0x1D2840, slot +0x14; from the base declaration
private:
    /* +0x14 */ unsigned int m_object;
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTACTIONPLAN_BAPAHONKHORN_H
