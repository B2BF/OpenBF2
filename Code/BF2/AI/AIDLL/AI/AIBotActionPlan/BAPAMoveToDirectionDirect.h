// Direct direction move with two owned conditions.

#ifndef AI_AIDLL_AI_AIBOTACTIONPLAN_BAPAMOVETODIRECTIONDIRECT_H
#define AI_AIDLL_AI_AIBOTACTIONPLAN_BAPAMOVETODIRECTIONDIRECT_H

#include "../../../../Math/Vec3.h"
#include "BAPAMoveToDirection.h"

namespace dice {
namespace hfe {

namespace ai {

class BAPCondition;
class Bot;

// ELF: vtable 0x4B82C0 (30 words), package H-deferred.
class BAPAMoveToDirectionDirect : public BAPAMoveToDirection
{
public:
    BAPAMoveToDirectionDirect(Bot*, Vec3&, Vec3&, float, float, BAPCondition*, BAPCondition*, unsigned long long, int);   // 0x1D8A30
    virtual ~BAPAMoveToDirectionDirect();   // 0x1D8CA0

    virtual bool execute(Bot*, bool&, bool&);   // 0x1D8DE0, slot +0x00; from the base declaration

    Vec2* getFirstPoint();   // ELF 0x1D8E00; [INFERRED] pointer spelling; returns the member address
    Vec2* getSecondPoint(bool&);   // ELF 0x1D8E10; [INFERRED] pointer spelling

    /* +0x78 */ Vec2 m_firstPoint; // [INFERRED] name
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTACTIONPLAN_BAPAMOVETODIRECTIONDIRECT_H
