// A waypoint list that closes in on one object (by information handle). Evidence: libaidll.so
// 1.5.3153 (ia-32): vtable 0x4B72E0 (D1, D0, getUrgency 0x1B8BA0, getGoalPoint 0x1B8F00, getId
// 0x1B9040 (weak, 0), WaypointList's error pair, getWayPointRadius 0x1B8F80); constructors
// 0x1B8780 / 0x1B8950; destructors 0x1B8B20 / 0x1B8B40 / 0x1B8B60; modifyMaxSpeed 0x1B8E30.

#ifndef AI_AIDLL_AI_AIBOT_WPCLOSETO_H       // [INFERRED] guard style
#define AI_AIDLL_AI_AIBOT_WPCLOSETO_H

#include "WaypointList.h"

namespace dice {
namespace hfe {
namespace ai {

class WPCloseTo : public WaypointList
{
public:
    WPCloseTo(unsigned int target, float radius);               // ELF 0x1B8780
    virtual ~WPCloseTo();                                        // ELF 0x1B8B20

    virtual float getUrgency(const Vec3& position, Bot* bot);    // ELF 0x1B8BA0
    virtual bool  getGoalPoint(Vec3& out);                       // ELF 0x1B8F00
    virtual int   getId() { return 0; }                          // ELF 0x1B9040 (weak)
    virtual float getWayPointRadius() const { return m_radius; } // ELF 0x1B8F80: flds +0x14

    float modifyMaxSpeed(float speed);                           // ELF 0x1B8E30
    unsigned int getTarget() const { return m_target; }          // [INFERRED] reader of +0x10

    // [INFERRED] names; the stores are attested (constructor 0x1B8780).
    // +0x08: WaypointList::m_float8 (base constructor: -1.0f)
    /* +0x0C */ unsigned int m_int12;      // 2 [UNRESOLVED] (WPEmpty 1)
    /* +0x10 */ unsigned int m_target;     // the object's information handle
    /* +0x14 */ float        m_radius;     // at least 5, at least twice the target's smallest radius
    /* +0x18 */ float        m_distanceSq; // 25.0f, then getUrgency's squared distance to the goal
    /* +0x1C */ Vec3         m_goal;       // the target's (sphere centre) position when last moved
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOT_WPCLOSETO_H
