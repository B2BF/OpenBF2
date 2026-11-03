// Strategic-area waypoint and its resolved navigation position.
// [INFERRED] Source placement.

#ifndef AI_AIDLL_AI_AISTRATEGICENTITY_SAWAYPOINT_H      // [INFERRED] guard style
#define AI_AIDLL_AI_AISTRATEGICENTITY_SAWAYPOINT_H

#include "../../../../Math/Vec3.h"

namespace dice {
namespace hfe {
namespace ai {

class DrawPrimitive;

class SAWayPoint
{
public:
    SAWayPoint(const Vec3& position, int map);
    ~SAWayPoint();

    // [INFERRED] Initializes the waypoint polygon; no result is returned to its caller.
    void init();

    void draw() const;
    void undraw() const;

    // [INFERRED] accessor forms; AIStrategicArea::orderNormalBot (ELF 0x33C940) reads +0 (the
    // position) and tests +0x10 against -1 inline.
    const Vec3& getPosition() const { return m_position; }
    unsigned int getPolygon() const { return m_polygon; }

private:
    // layout, on the ELF, read store by store out of the constructor
    /*  +0 */ Vec3 m_position;               // the constructor's first argument
    // [INFERRED] Island and polygon ids, both initialized to the invalid handle.
    // MapData caches the first field; debug color tests the second.
    /* +12 */ unsigned int m_island;         // = -1; the half a MapData keeps
    /* +16 */ unsigned int m_polygon;        // = -1; the half `draw` tests
    /* +20 */ int m_map;                     // the constructor's second argument
    // mutable, and recovered as such - see the header note.
    /* +24 */ mutable DrawPrimitive* m_debugPrimitive;   // = 0
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AISTRATEGICENTITY_SAWAYPOINT_H
