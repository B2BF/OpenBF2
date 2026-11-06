// Tests an object's distance from a fixed point; missing information satisfies the condition.

#ifndef AI_AIDLL_AI_AIBOTACTIONPLAN_BAPCONOBJECTDISTANCETOPOINT_H
#define AI_AIDLL_AI_AIBOTACTIONPLAN_BAPCONOBJECTDISTANCETOPOINT_H

#include <iosfwd>
#include "../../../../Math/Vec3.h"
#include "BAPConObject.h"
#include "BAPTypes.h"

namespace dice {
namespace hfe {

namespace ai {

class Bot;

// ELF: vtable 0x4B9648 (5 words), package H-deferred.
class BAPConObjectDistanceToPoint : public BAPConObject
{
public:
    BAPConObjectDistanceToPoint(const Vec3&, unsigned int, float, bool, bool);   // 0x1ED980
    BAPConObjectDistanceToPoint(const Vec3&, unsigned int, float, float, bool, bool);   // 0x1EDAA0
    virtual ~BAPConObjectDistanceToPoint();   // 0x1EDB50

    virtual bool evaluate(Bot*);   // 0x1EDBB0, slot +0x00; from the base declaration
    virtual BAPConditionType getConditionType() const;   // 0x1EDF00, slot +0x04; from the base declaration
    virtual void streamSelf(std::ostream&) const;   // 0x1EDD80, slot +0x08; from the base declaration
private:
    /* +0x0C */ float m_minDistanceSqr;
    /* +0x10 */ float m_maxDistanceSqr;
    /* +0x14 */ bool m_estimate;
    /* +0x15 */ bool m_flat;
    /* +0x18 */ Vec3 m_point;
    /* +0x24 */ float m_lastDistanceSqr;
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTACTIONPLAN_BAPCONOBJECTDISTANCETOPOINT_H
