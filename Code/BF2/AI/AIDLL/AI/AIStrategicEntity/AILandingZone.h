// A rectangular landing zone on the XZ plane with a beach side, used by the amphibious waypoints.
// Not polymorphic: the image has no vtable and no deleting destructor for it. sizeof is 32 (0x20)
// in the ia-32 image with its libstdc++ string; the native size differs.
// Placement in AIStrategicEntity is [INFERRED]: the TU sits in the image between
// StrategyPrerequisite and AIStrategicArea.
#ifndef AI_AIDLL_AI_AISTRATEGICENTITY_AILANDINGZONE_H
#define AI_AIDLL_AI_AISTRATEGICENTITY_AILANDINGZONE_H

#include <string>

#include "../../../../Math/Vec3.h"

namespace dice {
namespace hfe {
namespace ai {

// The side of the zone a beach landing comes from. Names and values from the image's stream
// operators (ELF 0x3C89A0 / 0x3C8A20), which also accept them without the "LZ" prefix.
enum LandingZoneDirection : int
{
    LZXMin = 0,
    LZZMin = 1,
    LZXMax = 2,
    LZZMax = 3,
    LZUndefined = 4
};

class AILandingZone
{
public:
    // The corners are sorted per axis after the copy, so minCorner <= maxCorner.
    AILandingZone(const std::string& name, int id, const Vec2& minCorner, const Vec2& maxCorner,
                  float height, LandingZoneDirection beachDirection);
    ~AILandingZone();

    // A random position on the side opposite the beach, `distance` inside the zone, that the
    // pathfinder accepts (IAIPathfinding::isValidPosition(map, type, position, true)); up to 21
    // draws. False when none was accepted; `position` then holds the last draw.
    bool getApproachPosition(int map, int type, Vec3& position, float distance) const;
    // A random position on the beach side's edge. Always true.
    bool getBeachPosition(int map, Vec3& position) const;
    LandingZoneDirection getBeachDirection() const;
    void setBeachDirection(LandingZoneDirection direction);
    // Inclusive XZ box test.
    bool isInZone(const Vec2& position) const;
    bool isInZone(const Vec3& position) const;
    // 0 inside the zone; otherwise the squared distance to where the ray from the centre towards
    // `position` leaves the box.
    float getDistanceSqr(const Vec2& position) const;
    float getDistanceSqr(const Vec3& position) const;
    const std::string& getName() const;
    int getId() const;
    const Vec2& getMinCorner() const;
    const Vec2& getMaxCorner() const;
    void setMinCorner(const Vec2& corner);
    void setMaxCorner(const Vec2& corner);
    // A uniformly random point on the line `distance` inside the given side, at the zone's height.
    // An undefined side yields (0, height, 0); both random draws are still taken. `map` is unused.
    void getRandomPosOnSide(LandingZoneDirection side, int map, Vec3& position,
                            float distance) const;

private:
    // AIStrategicArea::attachLandingZone / ::listLandingZones (ELF 0x33B580, 0x33B7A0) and
    // AILandingZoneManager::activeZone / ::listZones read m_name at +0x14 directly instead of
    // calling getName. [INFERRED] friendship.
    friend class AIStrategicArea;
    friend class AILandingZoneManager;              // activeZone / listZones, same direct read

    /* +0x00 */ Vec2 m_minCorner;
    /* +0x08 */ Vec2 m_maxCorner;
    /* +0x10 */ LandingZoneDirection m_beachDirection;
    /* +0x14 */ std::string m_name;
    /* +0x18 */ int m_id;
    /* +0x1C */ float m_height;          // y of every position the zone hands out [INFERRED name]
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AISTRATEGICENTITY_AILANDINGZONE_H
