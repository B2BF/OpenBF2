// dice::meme::Bf2TileNode, BF2.exe 0x7CB990..0x7CBBD0 (see Bf2TileNode.h).
#include "Bf2TileNode.h"
#if !DEDICATED
#include "../External/BFMeme/Effect.h"       // drawContextTexture (82D790)
#include "../External/BFMeme/Quad.h"
#include "../External/BFMeme/Style.h"        // Rect
#include "../External/BFMeme/TypeDescriptor.h"
#include <cstring>
namespace dice { namespace meme {
namespace {
char* cloneTileName(const char* name)                       // 737DC0
{
    if (!name) return nullptr;
    const unsigned int bytes = static_cast<unsigned int>(std::strlen(name)) + 1;
    char* result = static_cast<char*>(::operator new(bytes));
    std::memcpy(result, name, bytes);
    return result;
}
}

Bf2TileNode::Bf2TileNode(const char* name, int count)
    : m_name(nullptr), m_count(count), m_thickness(0), m_horizontal(0)
{
    // 7CB9E0 clears the name, then clones it; +10/+14 are not written (the factory's zeroed
    // allocation is the only source of their start value, so they start at zero here).
    m_name = cloneTileName(name);
}

Bf2TileNode::~Bf2TileNode() { ::operator delete(m_name); m_name = nullptr; }

TypeDescriptor* Bf2TileNode::getClassType()
{
    static ClassTypeDescriptor<Bf2TileNode, Node> type("dice::meme::Bf2TileNode");
    return &type;
}
TypeDescriptor* Bf2TileNode::getType() { return getClassType(); }
unsigned int Bf2TileNode::getObjectSize() { return sizeof(Bf2TileNode); }
const char* Bf2TileNode::getName() { return m_name; }
bool Bf2TileNode::setName(const char* name)
{
    ::operator delete(m_name);
    m_name = nullptr;
    m_name = cloneTileName(name);       // freed before the copy, as shipped
    return true;
}
bool Bf2TileNode::slot_0x24() { return true; }

bool Bf2TileNode::setOptions(int count, int thickness, int horizontal)
{
    m_count = count;
    m_horizontal = horizontal;
    m_thickness = thickness;
    return true;
}

// 7CBA60. The bounds are split into `count` steps along the run (the x87 quotient truncated to an
// integer); each tile is `thickness` across, the bounds' extent along the run, and draws an
// untextured quad with no clip through the context.
void Bf2TileNode::paintSelf(System* system, const NodeState*, const Rect* bounds, const Rect*,
                            RenderContext* context)
{
    const int count = m_count;
    if (count == 0) return;
    float width = bounds->width;
    float height = bounds->height;
    float dirX, dirY;
    int step;
    if (m_horizontal == 0) {
        step = static_cast<int>(static_cast<double>(bounds->height) / count);
        dirX = 0.0f; dirY = 1.0f;
        height = static_cast<float>(m_thickness);
    } else {
        step = static_cast<int>(static_cast<double>(bounds->width) / count);
        dirX = 1.0f; dirY = 0.0f;
        width = static_cast<float>(m_thickness);
    }
    if (count <= 0) return;
    int offset = 0;
    for (int i = 0; i < m_count; ++i) {
        Rect tile;
        tile.x = dirX * static_cast<float>(offset) + bounds->x;
        tile.y = static_cast<float>(offset) * dirY + bounds->y;
        tile.width = width;
        tile.height = height;
        tile.z = bounds->z;
        Quad quad(&tile, nullptr);
        drawContextTexture(context, system, 0, &quad);
        offset += step;
    }
}

#if defined(_M_IX86)
static_assert(sizeof(Bf2TileNode) == 0x18, "Bf2TileNode layout");
#endif
} }
#endif
