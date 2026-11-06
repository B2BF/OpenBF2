// Timed debug circle. Evidence: libaidll.so (ia-32) 0x37C8E0..0x37CCDF. The translation unit's
// file symbol is DPStaticCirlce.cpp (sic); the directory is [INFERRED].

#include "DPStaticCircle.h"

// <iostream>'s ios_base::Init object is this TU's static initialisation and teardown (ELF
// 0x37CC30, __tcf_0 0x37CC90, _GLOBAL__I 0x37CCC0).
#include <iostream>

#include "../../../AIInterface/BFKnowledge/DebugRenderer.h"

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x37C8E0 (C2; C1 0x37C990).
DPStaticCircle::DPStaticCircle(float lifeTime, unsigned int colour, const Vec3& position,
                               float radius, const Vec3& normal, bool filled, int segments)
    : DrawPrimitive(lifeTime, colour)
    , m_position(position)
    , m_radius(radius)
    , m_normal(normal)
    , m_filled(filled)
    , m_segments(segments)
{
}

// ELF 0x37CA40 (D2; D1 0x37CA80, D0 0x37CAC0).
DPStaticCircle::~DPStaticCircle()
{
}

// ELF 0x37CB00. IDebugRenderer slot 5 (drawCircle) takes the normal first.
bool DPStaticCircle::draw(IDebugRenderer* renderer)
{
    renderer->drawCircle(m_normal, m_position, m_radius, m_colour, m_filled, m_segments);
    return true;
}

// ELF 0x37CB50.
void DPStaticCircle::getAttributes(float& lifeTime, unsigned int& colour, Vec3& position,
                                   float& radius, Vec3& normal, bool& filled, int& segments) const
{
    lifeTime = m_lifeTime;
    colour = m_colour;
    position = m_position;
    radius = m_radius;
    normal = m_normal;
    filled = m_filled;
    segments = m_segments;
}

// ELF 0x37CBC0.
void DPStaticCircle::setAttributes(float lifeTime, unsigned int colour, Vec3 position,
                                   float radius, const Vec3& normal, bool filled, int segments)
{
    m_lifeTime = lifeTime;
    m_colour = colour;
    m_position = position;
    m_radius = radius;
    m_normal = normal;
    m_filled = filled;
    m_segments = segments;
}

} // namespace ai
} // namespace hfe
} // namespace dice
