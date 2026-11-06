// Vehicle collision-avoidance plan generator.

#ifndef AI_AIDLL_AI_AIBOTBEHAVIOURPATTERN_BBPAVOIDCOLLISION2D_H
#define AI_AIDLL_AI_AIBOTBEHAVIOURPATTERN_BBPAVOIDCOLLISION2D_H

#include "../../../../Math/Vec3.h"
#include "BBPAvoidCollision.h"

namespace dice {
namespace hfe {

namespace ai {

class Bot;
class BotScript;

// ELF: vtable 0x4BB720 (7 words), operator new 0x20, package G-vehicles.
class BBPAvoidCollision2d : public BBPAvoidCollision
{
public:
    explicit BBPAvoidCollision2d(int);   // 0x229AD0
    virtual ~BBPAvoidCollision2d();   // 0x229CC0

    virtual BotScript* createPlan(Bot*);   // 0x229E20, slot +0x08; from the base declaration
    virtual void init(Bot*, Vec3&, unsigned int, Vec3&, float);   // 0x229D60, slot +0x18; from the base declaration

    // Same layout as BBPAvoidCollisionInfantery (operator new 0x20 in both create arms).
    /* +0x18 */ unsigned char m_unwritten18[4];   // not written by the constructor [UNRESOLVED]
    /* +0x1C */ Vec2* m_pointXZ;                  // per bot, zeroed [INFERRED] name, as in the infantry pattern
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTBEHAVIOURPATTERN_BBPAVOIDCOLLISION2D_H
