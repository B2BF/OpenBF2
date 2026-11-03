// A boat waypoint list: approach a landing zone, then run onto its beach.
// Evidence: libaidll.so 1.5.3153 (ia-32). File WPBeachLanding.cpp (FILE symbol; the AIBot
// directory is [INFERRED] from its neighbours); vtable 0x4B72A0 (D1, D0, getUrgency, getGoalPoint,
// getId, then WaypointList's setWayPointError / getWayPointError, then getWayPointRadius).
// Constructors 0x1B82E0 / 0x1B8370 (zone index) and 0x1B8400 / 0x1B8470 (zone); destructors
// 0x1B84E0 / 0x1B8500 / 0x1B8520. No call site in the image constructs it, so the size is not
// attested; BBPGotoWaypointBoat::createPlan reads it through getId 3.

#ifndef AI_AIDLL_AI_AIBOT_WPBEACHLANDING_H       // [INFERRED] guard style
#define AI_AIDLL_AI_AIBOT_WPBEACHLANDING_H

#include "WaypointList.h"

namespace dice {
namespace hfe {
namespace ai {

class AILandingZone;

class WPBeachLanding : public WaypointList
{
public:
    // Both forms also store the zone into the bot (BotMain +0xE8).
    WPBeachLanding(int zoneIndex, float value, int map, int approachType, Bot* bot);
    WPBeachLanding(AILandingZone* zone, float value, int map, int approachType, Bot* bot);
    virtual ~WPBeachLanding();

    virtual float getUrgency(const Vec3& position, Bot* bot);   // ELF 0x1B8560
    virtual bool  getGoalPoint(Vec3& out);                      // ELF 0x1B8600
    virtual int   getId() { return 3; }                         // ELF 0x1B8770 (weak)
    virtual float getWayPointRadius() const;                    // ELF 0x1B86A0: 10

    bool insideZone();                    // ELF 0x1B8670
    bool closeToZone(float distance);     // ELF 0x1B8680

    // [CHOSEN, not recovered] packaging: the BotMain +0xE8 store both beach-landing waypoint
    // lists' constructors make inline (WPMoveToBeachLanding 0x1BA42E too). It lives here because
    // WPBeachLanding is BotMain's friend; [RETIRE-WHEN: wpmovetobeach-friend] BotMain.h also
    // befriends WPMoveToBeachLanding, which then stores the zone itself.
    static void storeLandingZone(Bot* bot, AILandingZone* zone);

    // [INFERRED] names; the stores are attested.
    /* +0x0C */ int            m_int0C;          // ctor 6 [UNRESOLVED] meaning
    /* +0x10 */ int            m_map;            // getBeachPosition / getApproachPosition first argument
    /* +0x14 */ int            m_approachType;   // getApproachPosition second argument
    /* +0x18 */ bool           m_inside;         // getUrgency: distance 0 to the zone
    /* +0x1C */ float          m_value;          // ctor float argument; no reader in this TU
    /* +0x20 */ float          m_distanceSqr;    // getUrgency; not initialised by the constructors
    /* +0x24 */ int            m_unknown24;      // [UNRESOLVED] never stored or read in this TU
    /* +0x28 */ AILandingZone* m_zone;
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOT_WPBEACHLANDING_H
