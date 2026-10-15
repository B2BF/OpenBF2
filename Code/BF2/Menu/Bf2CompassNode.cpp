// Menu compass node using Bf2BarNode updates. [INFERRED] Source placement.
#include "Bf2CompassNode.h"
#if !DEDICATED
#include "../External/BFMeme/Quad.h"
#include "../External/BFMeme/System.h"
#include "../External/BFMeme/Effect.h"
#include "../External/BFMeme/TypeDescriptor.h"
#include <cmath>
#include <new>
namespace dice { namespace meme {
namespace {
const float radiansToDegrees = 57.295776f;
const float oneOverFullTurn = 0.0027777778f;
}

Bf2CompassNode::Bf2CompassNode(const char* name, int direction, bool snap, bool wrap)
    : Bf2BarNode(nullptr, 3), m_snapTexturePrimary(0), m_snapTextureSecondary(0),
      m_offset(0), m_snapEnabled(snap), m_wrapEnabled(wrap)
{
    m_textureRect.x = 0.0f;
    m_textureRect.y = 0.0f;
    m_textureRect.width = 0.0f;
    m_textureRect.height = 0.0f;
    m_textureRect.z = 0.0f;
    m_snapOffset[0] = m_snapOffset[1] = m_snapOffset[2] = m_snapOffset[3] = 0;
    m_direction = direction; // overwrites the 3 the base constructor was given
    setName(name);
}

Bf2CompassNode::~Bf2CompassNode()
{
    // The shipped body releases the node name here as well, before the base
    // destructor does it again. The release helper nulls the field, so the
    // repeat is harmless; keep it rather than "tidying" one of the two away.
    setName(nullptr);
}

TypeDescriptor* Bf2CompassNode::getClassType()
{
    // Complete eight-slot metadata938C28; independent counter A23830.
    static ClassTypeDescriptor<Bf2CompassNode, Bf2BarNode> type("dice::meme::Bf2CompassNode");
    return &type;
}
TypeDescriptor* Bf2CompassNode::getType() { return getClassType(); }
unsigned int Bf2CompassNode::getObjectSize() { return sizeof(Bf2CompassNode); }
namespace {
TypeDescriptor* const registeredBf2CompassNodeType = Bf2CompassNode::getClassType();
}

// native +108..+114 accessors
bool Bf2CompassNode::setTextureSizeForHud(float width, float height)
{
    m_textureRect.x = 0.0f; // the origin is cleared, the z word is not touched
    m_textureRect.y = 0.0f;
    m_textureRect.width = width;
    m_textureRect.height = height;
    return true;
}
bool Bf2CompassNode::setOffsetForHud(int offset) { m_offset = offset; return true; }
bool Bf2CompassNode::setSnapOffsetForHud(int a, int b, int c, int d)
{
    m_snapOffset[0] = a;
    m_snapOffset[1] = b;
    m_snapOffset[2] = c;
    m_snapOffset[3] = d;
    return true;
}
bool Bf2CompassNode::setSnapTextureForHud(bool primary, TextureHandle texture)
{
    if (primary) m_snapTexturePrimary = texture;
    else m_snapTextureSecondary = texture;
    return true;
}

// The compass draws its texture-sized strip, scrolled by the bound
// value and clipped to the node bounds, plus at most one wrapped copy.
// Only directions 0 (vertical) and 3 (horizontal) scroll; any other value
// leaves the strip where it is. The wrap arm is horizontal in both directions -
// that is the shipped behaviour, not an oversight of this reconstruction.
void Bf2CompassNode::paintSelf(System* system, const NodeState* state,
                              const Rect* bounds, const Rect* clip,
                              RenderContext* context)
{
    Rect strip;
    strip.x = bounds->x;
    strip.y = bounds->y;
    strip.width = m_textureRect.width;
    strip.height = m_textureRect.height;
    strip.z = 0.0f;
    int shift = 0;
    TextureHandle texture = m_texture;

    if (m_direction == 0) {
        const float travel = (m_textureRect.height - static_cast<float>(m_fillBottom))
                           * (*m_value) * radiansToDegrees * oneOverFullTurn
                           + static_cast<float>(m_offset);
        shift = static_cast<int>(std::floor(travel));
        if (m_snapEnabled) {
            const float leading = bounds->height - static_cast<float>(m_snapOffset[1]);
            if (static_cast<float>(shift - m_offset) > leading) {
                shift = static_cast<int>(std::floor(leading + static_cast<float>(m_offset)));
                if (m_snapTexturePrimary != 0) texture = m_snapTexturePrimary;
            } else {
                const float trailing = bounds->height - static_cast<float>(m_snapOffset[0]);
                if (static_cast<float>(m_offset - shift) > trailing) {
                    shift = static_cast<int>(std::floor(static_cast<float>(m_offset) - trailing));
                    if (m_snapTextureSecondary != 0) texture = m_snapTextureSecondary;
                }
            }
        }
        strip.y -= static_cast<float>(shift);
    } else if (m_direction == 3) {
        const float travel = (m_textureRect.width - static_cast<float>(m_fillRight))
                           * (*m_value) * radiansToDegrees * oneOverFullTurn
                           + static_cast<float>(m_offset);
        shift = static_cast<int>(std::floor(travel));
        if (m_snapEnabled) {
            const float leading = bounds->width * 0.5f - static_cast<float>(m_snapOffset[2]);
            if (static_cast<float>(shift - m_offset) > leading) {
                shift = static_cast<int>(std::floor(static_cast<float>(m_offset) + leading));
                if (m_snapTexturePrimary != 0) texture = m_snapTexturePrimary;
            } else {
                const float trailing = bounds->width * 0.5f - static_cast<float>(m_snapOffset[3]);
                if (static_cast<float>(m_offset - shift) > trailing) {
                    shift = static_cast<int>(std::floor(static_cast<float>(m_offset) - trailing));
                    if (m_snapTextureSecondary != 0) texture = m_snapTextureSecondary;
                }
            }
        }
        strip.x -= static_cast<float>(shift);
    }

    {
        Quad quad(&strip, bounds); // the node bounds are the clip
        drawContextTexture(context, system, texture, &quad);
    }
    if (!m_wrapEnabled) return;

    const float border = static_cast<float>(m_fillRight);
    if (shift < 0) {
        if (static_cast<float>(m_offset) - (m_textureRect.width - border) * 0.5f
            <= static_cast<float>(shift)) {
            strip.x = strip.x - m_textureRect.width + border;
            Quad wrapped(&strip, bounds);
            drawContextTexture(context, system, texture, &wrapped);
        }
        return;
    }
    if (static_cast<float>(shift) > m_textureRect.width - border - bounds->width &&
        static_cast<float>(shift)
            < (m_textureRect.width - border) * 0.5f + static_cast<float>(m_offset)) {
        strip.x = strip.x + m_textureRect.width - border;
        Quad wrapped(&strip, bounds);
        drawContextTexture(context, system, texture, &wrapped);
    }
}

#if defined(_M_IX86)
static_assert(sizeof(Bf2CompassNode) == 196, "native Bf2CompassNode storage");
static_assert(sizeof(ClassTypeDescriptor<Bf2CompassNode, Bf2BarNode>) == 16,
    "Bf2CompassNode metadata");
#endif
} }
#endif
