// A waypoint list of (position, strategic area id) goals a bot walks through in order.
// Evidence: libaidll.so 1.5.3153 (ia-32). Attested file AIBot/WPMoveTo.cpp (FILE symbol beside
// 0x1BA040); vtable 0x4B7360; constructors 0x1B92F0 / 0x1B9530 (Vec3 list + last area) and
// 0x1B96C0 / 0x1B97E0 (pair list); destructors 0x1B9900 / 0x1B9960 / 0x1B99C0; getUrgency
// 0x1B9A30; getGoalPoint 0x1B9F70; getCoords 0x1B9FB0; getWayPointRadius 0x1BA030; getId 0x1BA140.
// Size 0x24 (`operator new(0x24)` in AIStrategicArea::orderNormalBot).

#ifndef AI_AIDLL_AI_AIBOT_WPMOVETO_H       // [INFERRED] guard style
#define AI_AIDLL_AI_AIBOT_WPMOVETO_H

#include <list>
#include <utility>

#include "WaypointList.h"

namespace dice {
namespace hfe {
namespace ai {

class WPMoveTo : public WaypointList
{
public:
    typedef std::list<std::pair<Vec3, int> > GoalList;

    // ELF 0x1B96C0: the goals as given; the second distance is at least 1.
    WPMoveTo(float radius, float stepRadius, unsigned int flags, const GoalList& goals);
    // ELF 0x1B92F0: each position with area -1, the last one with `lastArea`; the second distance is
    // at least 2.
    WPMoveTo(float radius, float stepRadius, unsigned int flags, const std::list<Vec3>& positions, int lastArea);
    virtual ~WPMoveTo();

    virtual float getUrgency(const Vec3& position, Bot* bot);   // ELF 0x1B9A30
    virtual bool  getGoalPoint(Vec3& out);                      // ELF 0x1B9F70: the first goal
    virtual int   getId() { return 1; }                         // ELF 0x1BA140 (weak)
    virtual float getWayPointRadius() const;                    // ELF 0x1BA030: +0x10

    void getCoords(std::list<Vec3>& out) const;                 // ELF 0x1B9FB0: appends the positions

    // [INFERRED] names; the stores are attested.
    // +0x08: WaypointList::m_float8 (base constructor: -1.0f)
    /* +0x0C */ int      m_int0C;         // ctor 0 (WPAltitudeMoveTo writes 4)
    /* +0x10 */ float    m_radius;        // the goal radius; getWayPointRadius
    /* +0x14 */ float    m_stepRadius;    // the radius for intermediate goals
    /* +0x18 */ unsigned int m_flags;     // the third argument ([UNRESOLVED] meaning)
    /* +0x1C */ GoalList m_goals;
    /* +0x20 */ bool     m_flag20;        // ctor false
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOT_WPMOVETO_H
