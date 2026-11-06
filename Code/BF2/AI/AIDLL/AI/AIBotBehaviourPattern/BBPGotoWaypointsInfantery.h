// Waypoint-following pattern of infantry (class BBPGotoWaypointSoldier in BBPGotoWaypointsInfantery.cpp;
// aibehaviours.ai names it BBPGotoWaypointInfantery).
// Recovered from libaidll.so (Linux 1.5.3153); every body carries its ELF address. Package D.
// Virtual functions are in ELF vtable slot order (tools/macos/ai_behaviour_vtables.py checks them).

#ifndef AI_AIDLL_AI_AIBOTBEHAVIOURPATTERN_BBPGOTOWAYPOINTSINFANTERY_H
#define AI_AIDLL_AI_AIBOTBEHAVIOURPATTERN_BBPGOTOWAYPOINTSINFANTERY_H

#include "../../../../Math/Vec3.h"
#include "BBPGotoWaypoint.h"

namespace dice {
namespace hfe {

namespace ai {

class Bot;
class BotScript;
class Information;
class WaypointList;

// ELF: vtable 0x4BC0C0 (7 words), operator new 0xC, package D-behaviours.
class BBPGotoWaypointSoldier : public BBPGotoWaypoint
{
public:
    explicit BBPGotoWaypointSoldier(int id);   // 0x284600
    virtual ~BBPGotoWaypointSoldier();   // 0x284680

    virtual BotScript* createPlan(Bot*);   // 0x284730, slot +0x08
    virtual void init(Bot*);   // 0x284700, slot +0x18

private:
    // [CHOSEN, not recovered] helpers for the plan the image builds inline in four arms.
    BotScript* buildMoveTo(Bot*, Information* equipment, const Vec3& goal, float maxSpeed,
                           unsigned long long controls, float wayPointRadius);
    BotScript* buildMoveToObject(Bot*, Information* equipment, WaypointList*, unsigned int target, const Vec3& goal,
                                 float range, float maxSpeed, unsigned long long controls);

    /* +0x08 */ int m_patternId;   // the constructor argument
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTBEHAVIOURPATTERN_BBPGOTOWAYPOINTSINFANTERY_H
