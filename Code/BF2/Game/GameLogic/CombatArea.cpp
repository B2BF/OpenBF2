// [INFERRED] CombatArea and CombatAreaManager use separate translation units.
#include "CombatArea.h"
#include "../Objects/IPlayer.h"
#include "../Objects/IObject.h"
#include "../Objects/ObjectTemplate.h"
#include "../Objects/IPlayerControlObjectTemplate.h"
#include "../../Core/Ids.h"

namespace dice {
namespace hfe {

// Return whether the area applies to the player's team and vehicle category.
// Null players and players without a controllable vehicle do not match. Team 0 on either
// side matches any team; category 4 matches all vehicles, and category 5 matches 2 or 3.
bool CombatArea::checkIfActiveToPlayer(world::IPlayer* player)
{
    if (!player)
        return false;

    world::IObject* vehicle = player->getVehicle();                        // IPlayer +0x48
    if (!vehicle)
        return false;
    if (!vehicle->queryInterface(world::IID_IPlayerControlObject))         // +0x0C
        return false;

    const int playerTeam = player->getTeam();                              // IPlayer +0xE8

    // Preserve the second vehicle query before reading its template.
    int vehicleCategory = 0;
    world::IObject* again = player->getVehicle();
    world::IPlayerControlObjectTemplate* pcoTemplate =
        static_cast<world::IPlayerControlObjectTemplate*>(
            again->getTemplate()->queryInterface(world::IID_IPlayerControlObjectTemplate));
    if (pcoTemplate)
        vehicleCategory = pcoTemplate->getVehicleCategory();               // slot +0x24

    if (playerTeam && m_team && playerTeam != m_team)
        return false;

    if (m_vehicleCategory == 4)
        return true;
    if (m_vehicleCategory == vehicleCategory)
        return true;
    // Category 5 groups categories 2 and 3.
    if (m_vehicleCategory == 5 && static_cast<unsigned int>(vehicleCategory - 2) <= 1u)
        return true;
    return false;
}

// Test the position against the inclusive rectangle in the XZ plane.
bool CombatArea::checkPositionRect(const Vec3& position) const
{
    if (m_minX > position.x) return false;      // +0x04
    if (position.x > m_maxX) return false;      // +0x0C
    if (m_minZ > position.z) return false;      // +0x08
    if (position.z > m_maxZ) return false;      // +0x10
    return true;
}

// Test the position against the closed polygon in the XZ plane using crossing parity.
// An empty polygon returns false; horizontal edges are skipped before dividing by their height.
bool CombatArea::checkPositionPolygon(const Vec3& position) const
{
    bool inside = false;
    const int count = static_cast<int>(m_points.size());
    if (count <= 0)
        return false;

    for (int i = 0, next = 0; i < count; ++i)
    {
        const Vec2& cur = m_points[i];
        if (++next == count)
            next = 0;
        const Vec2& nxt = m_points[next];

        const bool curAbove = position.z > cur.y;          // Vec2::y stores the Z coordinate.
        const bool nxtAbove = position.z > nxt.y;
        if (curAbove == nxtAbove)
            continue;                                      // the edge does not straddle the ray

        const float crossX = (position.z - cur.y) / (nxt.y - cur.y) * (nxt.x - cur.x) + cur.x;
        if (position.x > crossX)
            inside = !inside;
    }
    return inside;
}

// ELF 0x81245E0. The first edge's left-hand normal offsets its midpoint.
bool CombatArea::isClockwise() const
{
    const float dx = m_points[1].x - m_points[0].x;
    const float dz = m_points[1].y - m_points[0].y;
    Vec3 position;
    position.x = -dz + (m_points[1].x + m_points[0].x) * 0.5f;
    position.y = 0.0f;
    position.z = dx + (m_points[1].y + m_points[0].y) * 0.5f;
    return !checkPositionPolygon(position);
}

} // namespace hfe
} // namespace dice
