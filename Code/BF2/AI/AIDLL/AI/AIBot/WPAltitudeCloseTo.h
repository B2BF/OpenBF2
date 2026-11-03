// A flight waypoint list that closes in on an object at an altitude.
// Evidence: libaidll.so 1.5.3153 (ia-32). File WPAltitudeCloseTo.cpp (FILE symbol; the AIBot
// directory is [INFERRED] from its neighbours); vtable 0x4B6EE0; constructors 0x1B5DE0 / 0x1B5E40;
// destructors 0x1B5EA0 / 0x1B5EC0 / 0x1B5EE0; getUrgency 0x1B5F20; getGoalPoint 0x1B6040;
// getWayPointRadius 0x1B60C0; getId 0x1B6220. No call site in the image constructs it.

#ifndef AI_AIDLL_AI_AIBOT_WPALTITUDECLOSETO_H       // [INFERRED] guard style
#define AI_AIDLL_AI_AIBOT_WPALTITUDECLOSETO_H

#include "WaypointList.h"

namespace dice {
namespace hfe {
namespace ai {

class WPAltitudeCloseTo : public WaypointList
{
public:
    // ELF 0x1B5E40: base fields (+8 = -1.0f), the arguments at +0x10..+0x1C, +0xC = 3.
    WPAltitudeCloseTo(unsigned int target, float radius, float second, float third);
    virtual ~WPAltitudeCloseTo();

    virtual float getUrgency(const Vec3& position, Bot* bot);   // ELF 0x1B5F20
    virtual bool  getGoalPoint(Vec3& out);                      // ELF 0x1B6040: the target's position
    virtual int   getId() { return 5; }                         // ELF 0x1B6220 (weak)
    virtual float getWayPointRadius() const;                    // ELF 0x1B60C0: +0x14

    /* +0x0C */ int          m_int0C;    // ctor 3 [UNRESOLVED] meaning
    /* +0x10 */ unsigned int m_target;   // information handle (getGoalPoint)
    /* +0x14 */ float        m_radius;   // getWayPointRadius; the arrival radius in getUrgency
    /* +0x18 */ float        m_second;   // getUrgency: arrival needs position.y below it
    /* +0x1C */ float        m_third;    // BBPGotoWaypoint3d: BAPAMoveTo3dObject's distance argument
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOT_WPALTITUDECLOSETO_H
