// A flight waypoint list: positions an air bot flies through at an altitude.
// Evidence: libaidll.so 1.5.3153 (ia-32). Attested file AIBot/WPAltitudeMoveTo.cpp (FILE symbol
// beside 0x1B6CC0); vtable 0x4B70A0; constructors 0x1B6230 / 0x1B6420; destructors 0x1B6560 /
// 0x1B65C0 / 0x1B6620; getUrgency 0x1B6690; getGoalPoint 0x1B6C40; getCoords 0x1B6C80;
// getWayPointRadius 0x1B6CB0; getId 0x1B6F90. Size 0x28 (`operator new(0x28)` in
// AIStrategicArea::orderAirBot, which passes (altitude, 120.0f, 50.0f, 2, path)).

#ifndef AI_AIDLL_AI_AIBOT_WPALTITUDEMOVETO_H       // [INFERRED] guard style
#define AI_AIDLL_AI_AIBOT_WPALTITUDEMOVETO_H

#include <list>

#include "WaypointList.h"

namespace dice {
namespace hfe {
namespace ai {

class WPAltitudeMoveTo : public WaypointList
{
public:
    // ELF 0x1B6230. Note the list is taken by non-const reference (mangled `RSt4list`).
    WPAltitudeMoveTo(float first, float second, float third, unsigned int flags, std::list<Vec3>& positions);
    virtual ~WPAltitudeMoveTo();

    virtual float getUrgency(const Vec3& position, Bot* bot);   // ELF 0x1B6690
    virtual bool  getGoalPoint(Vec3& out);                      // ELF 0x1B6C40: the first position
    virtual int   getId() { return 6; }                         // ELF 0x1B6F90 (weak)
    virtual float getWayPointRadius() const;                    // ELF 0x1B6CB0: +0x10

    void getCoords(std::list<Vec3>& out) const;                 // ELF 0x1B6C80: list assignment

    // [INFERRED] names; the stores are attested.
    // +0x08: WaypointList::m_float8 (base constructor: -1.0f)
    /* +0x0C */ int   m_int0C;           // ctor 4
    /* +0x10 */ float m_first;           // the first argument; getWayPointRadius
    /* +0x14 */ float m_second;
    /* +0x18 */ float m_float18;         // ctor 1e9f; getUrgency's last front distance
    /* +0x1C */ float m_third;
    /* +0x20 */ unsigned int m_flags;
    /* +0x24 */ std::list<Vec3> m_positions;
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOT_WPALTITUDEMOVETO_H
