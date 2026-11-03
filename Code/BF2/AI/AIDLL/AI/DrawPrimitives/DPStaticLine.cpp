// Timed debug polyline. Evidence: libaidll.so (ia-32) 0x37CCE0..0x37D4BF.
// [INFERRED] Source placement.

#include "DPStaticLine.h"

// <iostream>'s ios_base::Init object is this TU's static initialisation and teardown (ELF
// 0x37D410, __tcf_0 0x37D470, _GLOBAL__I 0x37D4A0).
#include <iostream>

#include "../../../AIInterface/BFKnowledge/DebugRenderer.h"

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x37CCE0 (C2; C1 0x37CDF0). Three push_backs: the start point, then the end point twice.
// shipped defect: the end point is stored twice, so the renderer receives a degenerate last segment.
DPStaticLine::DPStaticLine(float lifeTime, unsigned int colour, const Vec3& start, const Vec3& end,
                           float width)
    : DrawPrimitive(lifeTime, colour)
    , m_width(width)
{
    m_points.push_back(start);
    m_points.push_back(end);
    m_points.push_back(end);
}

// ELF 0x37CF00 (C2; C1 0x37CFE0).
DPStaticLine::DPStaticLine(float lifeTime, unsigned int colour, const std::vector<Vec3>& points,
                           float width)
    : DrawPrimitive(lifeTime, colour)
    , m_points(points)
    , m_width(width)
{
}

// ELF 0x37D0C0 (D2; D1 0x37D140, D0 0x37D1C0).
DPStaticLine::~DPStaticLine()
{
}

// ELF 0x37D240. IDebugRenderer slot 4 (drawLine).
bool DPStaticLine::draw(IDebugRenderer* renderer)
{
    renderer->drawLine(m_points, m_colour, m_width);
    return true;
}

// ELF 0x37D280. resize(0) then a range insert at the end, as the listing's erase/_M_fill_insert
// pair followed by _M_range_insert shows.
void DPStaticLine::getAttributes(float& lifeTime, unsigned int& colour, std::vector<Vec3>& points,
                                 float& width) const
{
    lifeTime = m_lifeTime;
    colour = m_colour;
    width = m_width;
    points.resize(0);
    points.insert(points.end(), m_points.begin(), m_points.end());
}

// ELF 0x37D340.
void DPStaticLine::setAttributes(float& lifeTime, unsigned int& colour,
                                 const std::vector<Vec3>& points, float& width)
{
    m_lifeTime = lifeTime;
    m_colour = colour;
    m_width = width;
    m_points.resize(0);
    m_points.insert(m_points.end(), points.begin(), points.end());
}

} // namespace ai
} // namespace hfe
} // namespace dice
