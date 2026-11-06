// AI landing zones. Evidence: libaidll.so (ia-32) 0x334BB0..0x335480.

#include "AILandingZone.h"

#include <cmath>
// <iostream>'s ios_base::Init object is this TU's static initialisation and teardown (ELF
// 0x3353D0, __tcf_0 0x335430, _GLOBAL__I 0x335460).
#include <iostream>

#include "../Main/AIRandom.h"
#include "../Main/IAIPathfinding.h"

namespace dice {
namespace hfe {
namespace ai {

namespace {

// The image's inline uniform draw: lo + next() * 2^-32 * (hi - lo), unrounded until the store.
float randomBetween(float lo, float hi)
{
    const float range = hi - lo;
    return lo + AIRandom::m_instance->next() * 2.3283064e-10f * range;
}

} // namespace

// ELF 0x334BB0 (C2; C1 0x334C40).
AILandingZone::AILandingZone(const std::string& name, int id, const Vec2& minCorner,
                             const Vec2& maxCorner, float height,
                             LandingZoneDirection beachDirection)
    : m_minCorner(minCorner)
    , m_maxCorner(maxCorner)
    , m_beachDirection(beachDirection)
    , m_name(name)
    , m_id(id)
    , m_height(height)
{
    if (m_minCorner.x > m_maxCorner.x)
    {
        const float x = m_minCorner.x;
        m_minCorner.x = m_maxCorner.x;
        m_maxCorner.x = x;
    }
    if (m_minCorner.y > m_maxCorner.y)
    {
        const float y = m_minCorner.y;
        m_minCorner.y = m_maxCorner.y;
        m_maxCorner.y = y;
    }
}

// ELF 0x334CD0 (D2; D1 0x334D20): the name is the only member with a destructor.
AILandingZone::~AILandingZone()
{
}

// ELF 0x334D70. The approach comes from the side opposite the beach; the modulo keeps the shipped
// sign test on the sum, not on the remainder. The pathfinder singleton is read once.
bool AILandingZone::getApproachPosition(int map, int type, Vec3& position, float distance) const
{
    IAIPathfinding* const pathfinding = IAIPathfinding::instance;
    int attempts = 0;
    const int opposite = m_beachDirection + 2;
    const LandingZoneDirection side =
        static_cast<LandingZoneDirection>(opposite < 0 ? opposite % 4 + 4 : opposite % 4);
    getRandomPosOnSide(side, map, position, distance);
    do
    {
        if (pathfinding->isValidPosition(map, type, position, true))
            return true;
        ++attempts;
        getRandomPosOnSide(side, map, position, distance);
    } while (attempts < 20);
    return false;
}

// ELF 0x334E60.
bool AILandingZone::getBeachPosition(int map, Vec3& position) const
{
    getRandomPosOnSide(m_beachDirection, map, position, 0.0f);
    return true;
}

// ELF 0x334EB0.
LandingZoneDirection AILandingZone::getBeachDirection() const
{
    return m_beachDirection;
}

// ELF 0x334EC0.
void AILandingZone::setBeachDirection(LandingZoneDirection direction)
{
    m_beachDirection = direction;
}

// ELF 0x334ED0.
bool AILandingZone::isInZone(const Vec2& position) const
{
    return insideCornerBox(position, m_minCorner, m_maxCorner);
}

// ELF 0x334F10.
bool AILandingZone::isInZone(const Vec3& position) const
{
    Vec2 xz;
    xz.x = 0.0f;
    xz.y = 0.0f;
    vec3ToVec2XZ(position, xz);
    return isInZone(xz);
}

// ELF 0x334F70. The direction from the centre is normalised inline as the image does elsewhere:
// left alone within FLT_EPSILON of unit length, zeroed below FLT_EPSILON squared, otherwise scaled
// by 1 / sqrt. Both tests are written so that NaN takes the scaling arm, as the shipped `ja` /
// `jbe` pair does.
float AILandingZone::getDistanceSqr(const Vec2& position) const
{
    if (isInZone(position))
        return 0.0f;

    Vec2 centre;
    centre.x = 0.0f;
    centre.y = 0.0f;
    Vec2 direction;
    direction.x = 0.0f;
    direction.y = 0.0f;
    Vec2 exit;
    exit.x = 0.0f;
    exit.y = 0.0f;
    getBoxCentre(m_minCorner, centre, m_maxCorner);
    direction.x = position.x - centre.x;
    direction.y = position.y - centre.y;
    const float lengthSq = direction.x * direction.x + direction.y * direction.y;
    if (!(1.1920929e-07f > std::fabs(lengthSq - 1.0f)))
    {
        if (!(1.4210855e-14f > std::fabs(lengthSq)))
        {
            const float inverse = 1.0f / std::sqrt(lengthSq);
            direction.x = direction.x * inverse;
            direction.y = inverse * direction.y;
        }
        else
        {
            direction.x = 0.0f;
            direction.y = 0.0f;
        }
    }
    hfe::getIntersection(m_minCorner, m_maxCorner, direction, centre, exit);
    const float dx = position.x - exit.x;
    const float dy = position.y - exit.y;
    return dx * dx + dy * dy;
}

// ELF 0x335120.
float AILandingZone::getDistanceSqr(const Vec3& position) const
{
    Vec2 xz;
    xz.x = 0.0f;
    xz.y = 0.0f;
    vec3ToVec2XZ(position, xz);
    return getDistanceSqr(xz);
}

// ELF 0x335170.
const std::string& AILandingZone::getName() const
{
    return m_name;
}

// ELF 0x335180.
int AILandingZone::getId() const
{
    return m_id;
}

// ELF 0x335190.
const Vec2& AILandingZone::getMinCorner() const
{
    return m_minCorner;
}

// ELF 0x3351A0.
const Vec2& AILandingZone::getMaxCorner() const
{
    return m_maxCorner;
}

// ELF 0x3351B0.
void AILandingZone::setMinCorner(const Vec2& corner)
{
    m_minCorner = corner;
}

// ELF 0x3351D0.
void AILandingZone::setMaxCorner(const Vec2& corner)
{
    m_maxCorner = corner;
}

// ELF 0x3351F0. Both ranges are taken before the first draw; x is drawn before z.
void AILandingZone::getRandomPosOnSide(LandingZoneDirection side, int, Vec3& position,
                                       float distance) const
{
    float fromX;
    float toX;
    float fromZ;
    float toZ;
    switch (side)
    {
    case LZXMin:
        fromX = toX = m_minCorner.x + distance;
        fromZ = m_minCorner.y;
        toZ = m_maxCorner.y;
        break;
    case LZZMin:
        fromX = m_minCorner.x;
        toX = m_maxCorner.x;
        fromZ = toZ = m_minCorner.y + distance;
        break;
    case LZXMax:
        fromX = toX = m_maxCorner.x - distance;
        fromZ = m_minCorner.y;
        toZ = m_maxCorner.y;
        break;
    case LZZMax:
        fromX = m_minCorner.x;
        toX = m_maxCorner.x;
        fromZ = toZ = m_maxCorner.y - distance;
        break;
    default:
        fromX = toX = fromZ = toZ = 0.0f;
        break;
    }
    position.x = randomBetween(fromX, toX);
    position.y = m_height;
    position.z = randomBetween(fromZ, toZ);
}

} // namespace ai
} // namespace hfe
} // namespace dice
