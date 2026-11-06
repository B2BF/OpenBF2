// A boat waypoint list that first runs through a list of (position, strategic area) waypoints and
// then approaches a landing zone and its beach, as WPBeachLanding does.
// Evidence: libaidll.so 1.5.3153 (ia-32). File WPMoveToBeachLanding.cpp (FILE symbol; the AIBot
// directory is [INFERRED] from its neighbours); vtable 0x4B73A0 (D1, D0, getUrgency,
// getGoalPoint, getId, WaypointList's setWayPointError / getWayPointError, getWayPointRadius).
// Constructors 0x1BA150 / 0x1BA320 (zone index) and 0x1BA450 / 0x1BA560 (zone); destructors
// 0x1BA670 / 0x1BA6D0 / 0x1BA730. No call site in the image constructs it, so the size is not
// attested; getId answers 4.

#ifndef AI_AIDLL_AI_AIBOT_WPMOVETOBEACHLANDING_H       // [INFERRED] guard style
#define AI_AIDLL_AI_AIBOT_WPMOVETOBEACHLANDING_H

#include <list>
#include <utility>

#include "WaypointList.h"

namespace dice {
namespace hfe {
namespace ai {

class AILandingZone;

class WPMoveToBeachLanding : public WaypointList
{
public:
    // `waypoints` is copied: each a position and the index of the strategic area it lies in. The
    // radius is `radius`, at least 5. Both forms also store the zone into the bot (BotMain +0xE8).
    WPMoveToBeachLanding(int zoneIndex, float radius, int map, int approachType, int value24,
                         std::list<std::pair<Vec3, int> >& waypoints, Bot* bot);
    WPMoveToBeachLanding(AILandingZone* zone, float radius, int map, int approachType, int value24,
                         std::list<std::pair<Vec3, int> >& waypoints, Bot* bot);
    virtual ~WPMoveToBeachLanding();

    virtual float getUrgency(const Vec3& position, Bot* bot);   // ELF 0x1BA7A0
    virtual bool  getGoalPoint(Vec3& out);                      // ELF 0x1BAA20
    virtual int   getId() { return 4; }                         // ELF 0x1BAC30 (weak)
    virtual float getWayPointRadius() const;                    // ELF 0x1BAB70

    bool insideZone();                                          // ELF 0x1BAAC0
    bool closeToZone(float distance);                           // ELF 0x1BAAD0
    // Appends the remaining waypoint positions to `out`.
    void getCoords(std::list<Vec3>& out) const;                 // ELF 0x1BAAF0

    // [INFERRED] names; the stores are attested.
    /* +0x0C */ int            m_int0C;          // ctor 7 [UNRESOLVED] meaning (WPBeachLanding: 6)
    /* +0x10 */ int            m_map;            // getBeachPosition / getApproachPosition first argument
    /* +0x14 */ int            m_approachType;   // getApproachPosition second argument
    /* +0x18 */ bool           m_inside;         // getUrgency: squared zone distance below 10
    /* +0x1C */ float          m_radius;         // waypoint reach radius (ceil'd) and getWayPointRadius
    /* +0x20 */ float          m_distanceSqr;    // getUrgency; not initialised by the constructors
    /* +0x24 */ int            m_int24;          // ctor argument [UNRESOLVED]; no reader in this TU
    /* +0x28 */ AILandingZone* m_zone;
    /* +0x2C */ std::list<std::pair<Vec3, int> > m_waypoints;  // (position, strategic area index)
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOT_WPMOVETOBEACHLANDING_H
