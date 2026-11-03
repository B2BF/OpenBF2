// Collision-avoidance pattern of aircraft (3d navigation): steers 45 degrees off the own course,
// away from the obstacle, and climbs or dives by twice the time to collision.
// Recovered from libaidll.so (Linux 1.5.3153, ia-32, symbols); every body carries its ELF address.
// Virtual functions are in ELF vtable slot order (tools/macos/ai_behaviour_vtables.py checks them).

#ifndef AI_AIDLL_AI_AIBOTBEHAVIOURPATTERN_BBPAVOIDCOLLISION3D_H
#define AI_AIDLL_AI_AIBOTBEHAVIOURPATTERN_BBPAVOIDCOLLISION3D_H

#include "../../../../Math/Vec3.h"
#include "BBPAvoidCollision.h"

namespace dice {
namespace hfe {

namespace ai {

class Bot;
class BotScript;

// ELF: vtable 0x4BB7A0 (7 words), operator new 0x20, package H-deferred.
class BBPAvoidCollision3d : public BBPAvoidCollision
{
public:
    explicit BBPAvoidCollision3d(int);   // 0x22B220
    virtual ~BBPAvoidCollision3d();   // 0x22B410

    virtual BotScript* createPlan(Bot*);   // 0x22B570, slot +0x08; from the base declaration
    virtual void init(Bot*, Vec3&, unsigned int, Vec3&, float);   // 0x22B4B0, slot +0x18; from the base declaration

    // Same layout as BBPAvoidCollision2d, with a Vec3 per bot at +0x1C.
    /* +0x18 */ unsigned char m_unwritten18[4];   // not written by the constructor [UNRESOLVED]
    /* +0x1C */ Vec3* m_collisionPoint;           // per bot, zeroed: init's first Vec3 [INFERRED] name
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTBEHAVIOURPATTERN_BBPAVOIDCOLLISION3D_H
