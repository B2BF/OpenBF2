// A flight waypoint list that patrols positions at an altitude.
// Evidence: libaidll.so 1.5.3153 (ia-32). File WPAltitudePatrol.cpp (FILE symbol; the AIBot
// directory is [INFERRED] from its neighbours); vtable 0x4B7260; constructors 0x1B7CB0 / 0x1B7E60;
// destructors 0x1B7F60 / 0x1B7FC0 / 0x1B8020; getUrgency 0x1B8090; getGoalPoint 0x1B81A0;
// getCoords 0x1B81E0; getWayPointRadius 0x1B8210; getId 0x1B82D0. Constructed only by
// AIConsole::botSetWaypoints_Patrol3d (0x3921D0, call at 0x3922B9).

#ifndef AI_AIDLL_AI_AIBOT_WPALTITUDEPATROL_H       // [INFERRED] guard style
#define AI_AIDLL_AI_AIBOT_WPALTITUDEPATROL_H

#include <list>

#include "WaypointList.h"

namespace dice {
namespace hfe {
namespace ai {

class WPAltitudePatrol : public WaypointList
{
public:
    // ELF 0x1B7E60: base fields (+8 = -1.0f), the arguments at +0x10..+0x1C, the empty list,
    // +0xC = 5, a clear and the copy. The list is taken by non-const reference (mangled `RSt4list`).
    WPAltitudePatrol(float first, float second, float third, unsigned int flags, std::list<Vec3>& positions);
    virtual ~WPAltitudePatrol();

    virtual float getUrgency(const Vec3& position, Bot* bot);   // ELF 0x1B8090
    virtual bool  getGoalPoint(Vec3& out);                      // ELF 0x1B81A0: the first position
    virtual int   getId() { return 7; }                         // ELF 0x1B82D0 (weak)
    virtual float getWayPointRadius() const;                    // ELF 0x1B8210

    void getCoords(std::list<Vec3>& out) const;                 // ELF 0x1B81E0

    /* +0x0C */ int             m_int0C;      // ctor 5 [UNRESOLVED] meaning
    /* +0x10 */ float           m_first;      // getWayPointRadius; the XZ arrival radius
    /* +0x14 */ float           m_second;     // getUrgency: the |dy| arrival tolerance
    /* +0x18 */ float           m_third;      // BBPGotoWaypoint3d: BAPAMoveTo3d's distance argument
    /* +0x1C */ unsigned int    m_flags;
    /* +0x20 */ std::list<Vec3> m_positions;
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOT_WPALTITUDEPATROL_H
