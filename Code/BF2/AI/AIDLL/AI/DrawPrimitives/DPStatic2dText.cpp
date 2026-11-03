// Timed screen-space debug text. Evidence: libaidll.so (ia-32) 0x37B590..0x37BD6F.
// [INFERRED] Source placement.

#include "DPStatic2dText.h"

// <iostream>'s ios_base::Init object is this TU's static initialisation and teardown (ELF
// 0x37BCC0, __tcf_0 0x37BD20, _GLOBAL__I 0x37BD50).
#include <iostream>

#include "../../../AIInterface/BFKnowledge/DebugRenderer.h"

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x37B590 (C2; C1 0x37B700). The line vector is built with the fill constructor, one copy of `text`.
DPStatic2dText::DPStatic2dText(float lifeTime, unsigned int colour, const Vec2& position,
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

// ELF 0x37B7B0 (C2; C1 0x37B880).
DPStatic2dText::DPStatic2dText(float lifeTime, unsigned int colour, const Vec2& position,
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

// ELF 0x37B950 (D2; D1 0x37BA10, D0 0x37BAD0).
DPStatic2dText::~DPStatic2dText()
{
}

// ELF 0x37BC70. IDebugRenderer slot 6 (drawScreenText).
bool DPStatic2dText::draw(IDebugRenderer* renderer)
{
    renderer->drawScreenText(m_position, m_lines, m_size, m_colour, m_alignX, m_alignY);
    return true;
}

// ELF 0x37BB90.
void DPStatic2dText::getAttributes(float& lifeTime, unsigned int& colour, Vec2& position,
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

// ELF 0x37BC00.
void DPStatic2dText::setAttributes(float lifeTime, unsigned int colour, const Vec2& position,
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
