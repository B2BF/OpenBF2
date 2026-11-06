// Recovered from libaidll.so 1.5.3153 (ia-32); ELF addresses on every declaration.
// Drives a car at an object's predicted position (BBPRunOverSoldier's plan).

#ifndef AI_AIDLL_AI_AIBOTACTIONPLAN_BAPARUNOVEROBJECT_H
#define AI_AIDLL_AI_AIBOTACTIONPLAN_BAPARUNOVEROBJECT_H

#include "BAPAction.h"
#include "BAPTypes.h"

namespace dice {
namespace hfe {

namespace ai {

class Bot;

// ELF: vtable 0x4B8A00 (8 words), package G-vehicles.
// [UNRESOLVED] data members: the owning package names them from the constructors.
class BAPARunOverObject : public BAPAction
{
public:
    BAPARunOverObject(unsigned int object, unsigned long long controls, int patternId);   // 0x1DF470 (C2 0x1DF410)
    virtual ~BAPARunOverObject();   // 0x1DFBF0

    virtual bool execute(Bot*, bool&, bool&);   // 0x1DF4D0, slot +0x00; from the base declaration
    virtual BAPStatementType getId() const;   // 0x1DFB30, slot +0x10; from the base declaration
    virtual bool hasDefault();   // 0x1DFB20, slot +0x14; from the base declaration

private:
    /* +0x14 */ unsigned int m_object;           // information handle of the target
    /* +0x18 */ unsigned long long m_controls18; // the constructor's controls; no reader. The base
                                                 // m_controls (+0x08) stays 0, so getControls
                                                 // (BAPStatement's, slot +0x0C) answers 0 [INFERRED name]
};   // operator new size 0x20 (BBPRunOverSoldier::createPlan)

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTACTIONPLAN_BAPARUNOVEROBJECT_H
