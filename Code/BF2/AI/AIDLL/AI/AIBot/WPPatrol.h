// A closed patrol loop: the bot walks to the front point; once within the radius the point is moved
// to the back of the list, so the loop repeats forever.
// Evidence: libaidll.so 1.5.3153 (ia-32). Placement AIBot/WPPatrol.cpp [INFERRED] from the
// neighbouring WPMoveTo / WPMoveToBeachLanding TUs (the static-init symbol names the WPPatrol
// constructor); vtable 0x4B73E0 (D1, D0, getUrgency, getGoalPoint, getId, then WaypointList's
// setWayPointError / getWayPointError, then getWayPointRadius); constructors 0x1BAC40 / 0x1BADF0
// (list) and 0x1BAEF0 / 0x1BAFD0 (vector); destructors 0x1BB0B0 / 0x1BB110 / 0x1BB170.
// Size 0x1C (`operator new(0x1c)` in PatrolPath::botIsAlive 0x350BF0).

#ifndef AI_AIDLL_AI_AIBOT_WPPATROL_H       // [INFERRED] guard style
#define AI_AIDLL_AI_AIBOT_WPPATROL_H

#include <list>
#include <vector>

#include "WaypointList.h"

namespace dice {
namespace hfe {
namespace ai {

class WPPatrol : public WaypointList
{
public:
    // Both constructors clamp the radius to at least 5 and copy the points in order.
    WPPatrol(float radius, unsigned int flags, std::list<Vec3>& points);
    WPPatrol(float radius, unsigned int flags, std::vector<Vec3>& points);
    virtual ~WPPatrol();

    // 0 with the waypoint error set; otherwise 1, after cycling the front point to the back when
    // `position` is within the radius of it (XZ plane) and marking the step (+4).
    virtual float getUrgency(const Vec3& position, Bot* bot);
    // The front point; always true (an empty list is not checked).
    virtual bool  getGoalPoint(Vec3& out);
    virtual int   getId() { return 2; }                         // ELF 0x1BB420 (weak)
    virtual float getWayPointRadius() const;

    // Replaces `out` with the patrol points.
    void getCoords(std::list<Vec3>& out) const;

    // [INFERRED] names, as in WPMoveTo; the stores are attested.
    // +0x08: WaypointList::m_float8 (base constructor: -1.0f)
    /* +0x0C */ int   m_int0C;             // ctor 8 [UNRESOLVED] meaning
    /* +0x10 */ float m_radius;
    /* +0x14 */ unsigned int m_flags;      // the second argument (PatrolPath passes 5) [UNRESOLVED] meaning
    /* +0x18 */ std::list<Vec3> m_points;
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOT_WPPATROL_H
