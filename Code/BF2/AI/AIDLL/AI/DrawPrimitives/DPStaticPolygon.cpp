// Timed debug polygon. Evidence: libaidll.so (ia-32) 0x37E620..0x37F4CF.
// [INFERRED] Source placement.

#include "DPStaticPolygon.h"

// <iostream>'s ios_base::Init object is this TU's static initialisation and teardown (ELF
// 0x37F420, __tcf_0 0x37F480, _GLOBAL__I 0x37F4B0).
#include <iostream>

#include "../../../AIInterface/BFKnowledge/DebugRenderer.h"

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x37E620 (C2; C1 0x37EAC0). Both outer vectors are resized to size - 2 (an unsigned count,
// so fewer than two vertices wraps), then each fan triangle receives v0, vi, vi+1 with its colours
// interleaved in that order.
DPStaticPolygon::DPStaticPolygon(float lifeTime, const std::vector<Vec3>& vertices,
                                 const std::vector<unsigned int>& colours, bool filled)
    : DrawPrimitive(lifeTime, colours[0])
    , m_filled(filled)
{
    const unsigned int count = vertices.size();
    m_triangles.resize(count - 2);
    m_colours.resize(count - 2);
    for (unsigned int i = 1; i < count - 1; ++i)
    {
        m_triangles[i - 1].push_back(vertices[0]);
        m_colours[i - 1].push_back(colours[0]);
        m_triangles[i - 1].push_back(vertices[i]);
        m_colours[i - 1].push_back(colours[i]);
        m_triangles[i - 1].push_back(vertices[i + 1]);
        m_colours[i - 1].push_back(colours[i + 1]);
    }
}

// ELF 0x37EF60 (D2; D1 0x37F0B0, D0 0x37F200).
DPStaticPolygon::~DPStaticPolygon()
{
}

// ELF 0x37F350. The triangle count is read once before the loop.
bool DPStaticPolygon::draw(IDebugRenderer* renderer)
{
    const unsigned int count = m_triangles.size();
    for (unsigned int i = 0; i < count; ++i)
        renderer->drawTriangle(m_triangles[i], m_colours[i], m_filled);
    return true;
}

// ELF 0x37F3E0.
void DPStaticPolygon::getAttributes(float& lifeTime, bool& filled) const
{
    lifeTime = m_lifeTime;
    filled = m_filled;
}

// ELF 0x37F400.
void DPStaticPolygon::setAttributes(float& lifeTime, bool& filled)
{
    m_lifeTime = lifeTime;
    m_filled = filled;
}

} // namespace ai
} // namespace hfe
} // namespace dice
