// Timed world-space debug text. Evidence: libaidll.so (ia-32) 0x37C040..0x37C8DF.
// [INFERRED] Source placement.

#include "DPStatic3dText.h"

// <iostream>'s ios_base::Init object is this TU's static initialisation and teardown (ELF
// 0x37C830, __tcf_0 0x37C890, _GLOBAL__I 0x37C8C0).
#include <iostream>

#include "../../../AIInterface/BFKnowledge/DebugRenderer.h"

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x37C040 (C2; C1 0x37C1C0). The line vector is built with the fill constructor, one copy of `text`.
DPStatic3dText::DPStatic3dText(float lifeTime, unsigned int colour, const Vec3& position,
                               const std::string& text, int size, AITextAlignmentX alignX,
                               AITextAlignmentY alignY)
    : DrawPrimitive(lifeTime, colour)
    , m_position(position)
    , m_lines(1, text)
    , m_size(size)
    , m_alignX(alignX)
    , m_alignY(alignY)
{
}

// ELF 0x37C2A0 (C2; C1 0x37C3A0).
DPStatic3dText::DPStatic3dText(float lifeTime, unsigned int colour, const Vec3& position,
                               const std::vector<std::string>& lines, int size,
                               AITextAlignmentX alignX, AITextAlignmentY alignY)
    : DrawPrimitive(lifeTime, colour)
    , m_position(position)
    , m_lines(lines)
    , m_size(size)
    , m_alignX(alignX)
    , m_alignY(alignY)
{
}

// ELF 0x37C4A0 (D2; D1 0x37C560, D0 0x37C620).
DPStatic3dText::~DPStatic3dText()
{
}

// ELF 0x37C7E0. IDebugRenderer slot 7 (draw3dText).
bool DPStatic3dText::draw(IDebugRenderer* renderer)
{
    renderer->draw3dText(m_position, m_lines, m_size, m_colour, m_alignX, m_alignY);
    return true;
}

// ELF 0x37C6E0.
void DPStatic3dText::getAttributes(float& lifeTime, unsigned int& colour, Vec3& position,
                                   std::vector<std::string>& lines, int& size,
                                   AITextAlignmentX& alignX, AITextAlignmentY& alignY) const
{
    lifeTime = m_lifeTime;
    colour = m_colour;
    position = m_position;
    lines = m_lines;
    size = m_size;
    alignX = m_alignX;
    alignY = m_alignY;
}

// ELF 0x37C760.
void DPStatic3dText::setAttributes(float lifeTime, unsigned int colour, const Vec3& position,
                                   const std::vector<std::string>& lines, int size,
                                   AITextAlignmentX alignX, AITextAlignmentY alignY)
{
    m_lifeTime = lifeTime;
    m_colour = colour;
    m_position = position;
    m_lines = lines;
    m_size = size;
    m_alignX = alignX;
    m_alignY = alignY;
}

} // namespace ai
} // namespace hfe
} // namespace dice
