// Recovered from libaidll.so (Linux 1.5.3153, ia-32, symbols); every body carries its ELF address.
// Virtual functions are in ELF vtable slot order (tools/macos/ai_behaviour_vtables.py checks them).

#ifndef AI_AIDLL_AI_AIBOTACTIONPLAN_BAPAMOVEAWAYFROM3DOBJECT_H
#define AI_AIDLL_AI_AIBOTACTIONPLAN_BAPAMOVEAWAYFROM3DOBJECT_H

#include "../../../../Math/Vec3.h"
#include "BAPAction.h"
#include "BAPTypes.h"

namespace dice {
namespace hfe {

namespace ai {

class BAPCondition;
class Bot;

// ELF: vtable 0x4B7F00 (9 words), package G-vehicles.
// Flies away from an information object until the condition holds; the vehicle move entries
// execute it (on its own it fails) and read the object's point from getFirstPoint.
class BAPAMoveAwayFrom3dObject : public BAPAction
{
public:
    BAPAMoveAwayFrom3dObject(unsigned int, float, BAPCondition*, unsigned long long, int);   // 0x1D4D50
    virtual ~BAPAMoveAwayFrom3dObject();   // 0x1D4EA0

    virtual bool execute(Bot*, bool&, bool&);   // 0x1D4F70, slot +0x00
    virtual unsigned long long getControls();   // 0x1D50E0, slot +0x0C
    virtual BAPStatementType getId() const;   // 0x1D50F0, slot +0x10
    virtual bool hasDefault();   // 0x1D5100, slot +0x14
    virtual bool isFinished(Bot*);   // 0x1D5110, slot +0x20: the condition, false without one

    Vec3* getFirstPoint();   // 0x1D4F90; refreshes and returns this + 0x1C (was [INFERRED] int)

private:
    // [INFERRED] friendship: EntryHeliMoveAwayFromObject reads +0x14/+0x2C/+0x30 inline (ELF 0x2B6580).
    friend class EntryHeliMoveAwayFromObject;
    /* +0x14 */ unsigned int  m_object;
    /* +0x18 */ BAPCondition* m_condition;   // deleted by the destructor
    /* +0x1C */ Vec3          m_point;       // getFirstPoint: the object's position or position estimate
    /* +0x28 */ bool          m_mobile;      // the object had an IPIMobile at construction (else not set)
    /* +0x2C */ float         m_distance;    // [INFERRED] name: callers pass 150
    /* +0x30 */ Vec3          m_vec30;       // [INFERRED] previous velocity sample; EntryHeliMoveAwayFromObject
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTACTIONPLAN_BAPAMOVEAWAYFROM3DOBJECT_H
