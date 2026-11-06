// Timed debug triangles. Evidence: libaidll.so (ia-32) 0x380730..0x380F8F.
// [INFERRED] Source placement.

#include "DPStaticTriangle.h"

// <iostream>'s ios_base::Init object is this TU's static initialisation and teardown (ELF
// 0x380EE0, __tcf_0 0x380F40, _GLOBAL__I 0x380F70).
#include <iostream>

#include "../../../AIInterface/BFKnowledge/DebugRenderer.h"

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x380730 (C2; C1 0x3808A0). Reads colours[0] unchecked.
DPStaticTriangle::DPStaticTriangle(float lifeTime, const std::vector<Vec3>& vertices,
                                   const std::vector<unsigned int>& colours, bool filled)
    : DrawPrimitive(lifeTime, colours[0])
    , m_vertices(vertices)
    , m_colours(colours)
    , m_filled(filled)
{
}

// ELF 0x380A10 (D2; D1 0x380AB0, D0 0x380B50).
DPStaticTriangle::~DPStaticTriangle()
{
}

// ELF 0x380BF0. IDebugRenderer slot 8 (drawTriangle).
bool DPStaticTriangle::draw(IDebugRenderer* renderer)
{
    renderer->drawTriangle(m_vertices, m_colours, m_filled);
    return true;
}

// ELF 0x380C30.
void DPStaticTriangle::getAttributes(float& lifeTime, std::vector<Vec3>& vertices,
                                     std::vector<unsigned int>& colours, bool& filled) const
{
    lifeTime = m_lifeTime;
    vertices.resize(0);
    vertices.insert(vertices.end(), m_vertices.begin(), m_vertices.end());
    colours.resize(0);
    colours.insert(colours.end(), m_colours.begin(), m_colours.end());
    filled = m_filled;
}

// ELF 0x380D70.
void DPStaticTriangle::setAttributes(float& lifeTime, const std::vector<Vec3>& vertices,
                                     const std::vector<unsigned int>& colours, bool& filled)
{
    m_lifeTime = lifeTime;
    m_vertices.resize(0);
    m_vertices.insert(m_vertices.end(), vertices.begin(), vertices.end());
    m_colours.resize(0);
    m_colours.insert(m_colours.end(), colours.begin(), colours.end());
    m_filled = filled;
}

} // namespace ai
} // namespace hfe
} // namespace dice
