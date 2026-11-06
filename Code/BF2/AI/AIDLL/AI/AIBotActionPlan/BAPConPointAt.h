// Compares the bot's forward direction with a normalized object-position-plus-offset vector.

#ifndef AI_AIDLL_AI_AIBOTACTIONPLAN_BAPCONPOINTAT_H
#define AI_AIDLL_AI_AIBOTACTIONPLAN_BAPCONPOINTAT_H

#include "../../../../Math/Vec3.h"
#include "BAPCondition.h"
#include "BAPTypes.h"

namespace dice {
namespace hfe {

namespace ai {

class Bot;

// ELF: vtable 0x4B9340 (5 words), package H-deferred.
class BAPConPointAt : public BAPCondition
{
public:
    BAPConPointAt(unsigned int, Vec3&, float);   // 0x1E8AA0
    virtual ~BAPConPointAt();   // 0x1E8B30

    virtual bool evaluate(Bot*);   // 0x1E8B90, slot +0x00; from the base declaration
    virtual BAPConditionType getConditionType() const;   // 0x1E8D90, slot +0x04; from the base declaration
private:
    /* +0x08 */ unsigned int m_object;
    /* +0x0C */ float m_cosAngle;
    /* +0x10 */ Vec3 m_offset;
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTACTIONPLAN_BAPCONPOINTAT_H
