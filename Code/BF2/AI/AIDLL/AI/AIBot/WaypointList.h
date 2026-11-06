// Abstract waypoint target a bot follows (WPMoveTo, WPEmpty, WPPatrol, ... derive from it).
// [INFERRED] Source placement; the class is inline in the ELF (weak symbols only).

#ifndef AI_AIDLL_AI_AIBOT_WAYPOINTLIST_H       // [INFERRED] guard style
#define AI_AIDLL_AI_AIBOT_WAYPOINTLIST_H

#include "../../../../Math/Vec3.h"

namespace dice {
namespace hfe {
namespace ai {

class Bot;

// vtable ELF 0x4b6f20: D1 0x1B6180 (vptr store only), D0 0x1B61A0, three pure slots (WPMoveTo
// 0x4b7360 names them getUrgency / getGoalPoint / getId), then the three accessors below.
// The base constructor is inlined into every subclass (WPEmpty::WPEmpty 0x1B9050 stores
// +4 = 1, +5 = 0, +6 = 0 before its own fields).
class WaypointList
{
public:
    WaypointList()
        : m_flag4(true)
        , m_flag5(false)
        , m_wayPointError(false)
        , m_float8(-1.0f)
    {
    }
    virtual ~WaypointList() {}

    virtual float getUrgency(const Vec3& position, Bot* bot) = 0;   // +0x08 [INFERRED] return type
    virtual bool  getGoalPoint(Vec3& out) = 0;                      // +0x0C bool: WPMoveTo 0x1B9F70 returns "not empty"
    virtual int   getId() = 0;                                      // +0x10 WPMoveTo 1, WPEmpty 8
    // ELF 0x1B61E0 / 0x1B61F0: the byte at +6.
    virtual void  setWayPointError(bool error) { m_wayPointError = error; }   // +0x14
    virtual bool  getWayPointError() const { return m_wayPointError; }        // +0x18
    // ELF 0x1B6200: the base radius is 0.
    virtual float getWayPointRadius() const { return 0.0f; }                  // +0x1C

    // [UNRESOLVED] meaning of +4 and +5; SAI::updateBotPositions reads +5 inline.
    /* 4 */ bool m_flag4;              // base ctor: true
    /* 5 */ bool m_flag5;              // base ctor: false
    /* 6 */ bool m_wayPointError;      // base ctor: false
    // Every subclass constructor stores -1.0f here before its own vptr store (the inlined base
    // constructor); PatrolPath::botIsAlive stores the patrol speed. BBPGotoWaypoint3d::createPlan
    // (0x27F7F6) reads it through the base pointer as the speed, the vehicle's maximum when < 0.
    /* 8 */ float m_float8;            // base ctor: -1.0f [INFERRED] meaning (speed override)
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOT_WAYPOINTLIST_H
