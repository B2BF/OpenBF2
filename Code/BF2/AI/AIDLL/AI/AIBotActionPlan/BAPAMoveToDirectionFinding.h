// Direction move retaining the shipped zero-valued path points.

#ifndef AI_AIDLL_AI_AIBOTACTIONPLAN_BAPAMOVETODIRECTIONFINDING_H
#define AI_AIDLL_AI_AIBOTACTIONPLAN_BAPAMOVETODIRECTIONFINDING_H

#include "../../../../Math/Vec3.h"
#include "BAPAMoveToDirection.h"

namespace dice {
namespace hfe {

namespace ai {

class BAPCondition;
class Bot;

// ELF: vtable 0x4B8360 (30 words), package H-deferred.
class BAPAMoveToDirectionFinding : public BAPAMoveToDirection
{
public:
    BAPAMoveToDirectionFinding(Bot*, Vec3&, Vec3&, float, float, BAPCondition*, BAPCondition*, unsigned long long, int);   // 0x1D9170
    virtual ~BAPAMoveToDirectionFinding();   // 0x1D9460

    virtual bool execute(Bot*, bool&, bool&);   // 0x1D95A0, slot +0x00; from the base declaration

    Vec2* getFirstPoint();   // ELF 0x1D95C0; [INFERRED] pointer spelling
    Vec2* getSecondPoint(bool&);   // ELF 0x1D95D0; [INFERRED] pointer spelling

    /* +0x78 */ Vec2 m_firstPoint; // [INFERRED] names
    /* +0x80 */ Vec2 m_secondPoint;
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTACTIONPLAN_BAPAMOVETODIRECTIONFINDING_H
