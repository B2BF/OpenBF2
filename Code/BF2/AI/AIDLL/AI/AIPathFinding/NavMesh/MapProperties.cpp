
#include "MapProperties.h"

// <iostream>'s ios_base::Init object is this TU's static initialisation and teardown (ELF
// 0x2FE200, __tcf_0 0x2FE260, _GLOBAL__I 0x2FE290).
#include <iostream>
#include <cmath>

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x2FDE80 (C2; C1 0x2FDFA0). Initializes map traversal properties and their default limits.
MapProperties::MapProperties()
    : m_maxSlopeCos(0.0f)
    , m_radius(0.0f)
    , m_headClearance(2.0f)
    , m_allowedHeightDiff(2.5f)
{
}

// ELF 0x2FE020 (D2; D1 0x2FE090). The whole body is the set's own teardown, which a C++ destructor does on its own. Empty here.
MapProperties::~MapProperties()
{
}

// ELF 0x2FE100. Tests only the Y component against the slope cosine. Requires a unit surface normal
// for slope semantics; the material argument is ignored.
bool MapProperties::isValidGroundMesh(const Vec3& normal, unsigned char /*material*/) const
{
    return m_maxSlopeCos <= normal.y;
}

// ELF 0x2FE130 / 0x2FE160. Stores maximum slope as a cosine. The getter clamps it to [-1, 1] before acosf
// and returns the resulting angle in radians.
void MapProperties::setMaxSlope(float radians)
{
    m_maxSlopeCos = cosf(radians);
}

float MapProperties::getMaxSlope() const
{
    float cosine = m_maxSlopeCos;
    if (cosine < -1.0f)
        cosine = -1.0f;
    else if (cosine > 1.0f)
        cosine = 1.0f;

    return acosf(cosine);
}

// ELF 0x2FE1B0.
void MapProperties::setHeadClearance(float clearance)
{
    m_headClearance = clearance;
}

// ELF 0x2FE1C0.
float MapProperties::getHeadClearance() const
{
    return m_headClearance;
}

// ELF 0x2FE1D0.
void MapProperties::setRadius(float radius)
{
    m_radius = radius;
}

// ELF 0x2FE1E0.
float MapProperties::getRadius() const
{
    return m_radius;
}

// ELF 0x2FE1F0. No matching getter ships - see the header.
void MapProperties::setAllowedHeightDiff(float diff)
{
    m_allowedHeightDiff = diff;
}

} // namespace ai
} // namespace hfe
} // namespace dice
