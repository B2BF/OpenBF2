// Move statement that steers straight at its goal.
// Recovered from libaidll.so (Linux 1.5.3153, ia-32, symbols); every body carries its ELF address.
// Virtual functions are in ELF vtable slot order (tools/macos/ai_behaviour_vtables.py checks them).

#ifndef AI_AIDLL_AI_AIBOTACTIONPLAN_BAPAMOVETODIRECT_H
#define AI_AIDLL_AI_AIBOTACTIONPLAN_BAPAMOVETODIRECT_H

#include "../../../../Math/Vec3.h"
#include "BAPAMoveTo.h"

namespace dice {
namespace hfe {

namespace ai {

class BAPCondition;
class Bot;

// ELF: vtable 0x4B8140 (40 words), package F-infantry. Steers straight at the goal without
// pathfinding; size 0xC0 (`operator new(0xc0)` in BBPGotoWaypointBoat::createPlan).
class BAPAMoveToDirect : public BAPAMoveTo
{
public:
    BAPAMoveToDirect(Bot*, const Vec3&, float, float, BAPCondition*, unsigned long long, int);   // 0x1D8110
    virtual ~BAPAMoveToDirect();   // 0x1D8250

    virtual bool execute(Bot*, bool&, bool&);   // 0x1D82E0, slot +0x00; from the base declaration
    virtual int update(Vec2&, bool&, bool&, float);   // 0x1D8300, slot +0x2C; from the base declaration
    virtual int usesPathfinding() { return 0; }   // 0x1D8420 (weak), slot +0x4C
    virtual int withinDistance() { return 0; }    // 0x1D8430 (weak), slot +0x50

    /* +0xB4 */ Vec2 m_goal2d;   // the goal in XZ; update hands it out
    /* +0xBC */ Bot* m_bot;
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTACTIONPLAN_BAPAMOVETODIRECT_H
