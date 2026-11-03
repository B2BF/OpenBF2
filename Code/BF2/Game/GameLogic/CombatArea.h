// [INFERRED] Header beside ServerGameLogic.
#ifndef GAME_GAMELOGIC_COMBATAREA_H
#define GAME_GAMELOGIC_COMBATAREA_H

#include <vector>
#include <string>
#include "../../Math/Vec3.h"

namespace dice {
namespace hfe {

namespace world { class IPlayer; }

// A rectangular or polygonal battlefield boundary, optionally restricted by team and vehicle.
// CombatAreaManager applies exclusions after inclusions, so an exclusion clears an inside result.
class CombatArea
{
public:
    // Return false for null players or a missing controllable vehicle; otherwise apply team/category filters.
    // Team 0 on either side is unrestricted. Category 4 accepts all vehicles; category 5 accepts 2 or 3.
    bool checkIfActiveToPlayer(world::IPlayer* player);

    // Test the XZ position against a closed polygon; an empty polygon returns false.
    bool checkPositionPolygon(const Vec3& position) const;

    // Test the XZ position against the inclusive rectangle.
    bool checkPositionRect(const Vec3& position) const;

    // Test the left side of the first polygon edge; requires at least two vertices.
    bool isClockwise() const;

    // Offsets describe the original 32-bit layout; STL-bearing layouts depend on the compiler.
    /* +0x00 */ int   m_isPolygon;                 // 0 = rectangle, non-0 = polygon
    /* +0x04 */ float m_minX;
    /* +0x08 */ float m_minZ;
    /* +0x0C */ float m_maxX;
    /* +0x10 */ float m_maxZ;
    /* +0x14 */ int   m_vehicleCategory;           // 4 = all, 5 = categories 2 and 3
    // Names are compared case-insensitively by the manager.
    /* +0x18 */ std::string m_name;
    /* +0x1C */ int   m_team;                      // 0 = any team
    // These flags occupy separate bytes; widening either would overlap the other.
    /* +0x20 */ bool  m_isExclusion;               // An exclusion clears the inside result.
    // Opt in to AI pathfinding bounds with CombatArea.usedByPathFinding.
    /* +0x21 */ bool  m_usedByPathFinding;

    // Closed polygon vertices in (x, z) coordinates.
    /* +0x24 */ std::vector<Vec2> m_points;        // begin +0x24 / end +0x28 / cap +0x2C

    // AI layer, default 1. BFObjectEnvironment uses the first AI combat area's layer.
    /* +0x30 */ int   m_layer;                     // ctor 1
};

} // namespace hfe
} // namespace dice

#endif // GAME_GAMELOGIC_COMBATAREA_H
