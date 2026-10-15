// Menu progress-bar node. [INFERRED] Source placement.
#include "Bf2BarNode.h"
#if !DEDICATED
#include "../External/BFMeme/Quad.h"
#include "../External/BFMeme/System.h"
#include "../External/BFMeme/Effect.h"
#include "../External/BFMeme/TypeDescriptor.h"
#include <cmath>
#include <cstring>
#include <new>
namespace dice { namespace meme {
namespace {
// A23880/A23884, two adjacent loader-zeroed statics. The first is the value the
// constructor points every unbound bar at, so paintSelf can dereference it; the
// second is the "no fill texture requested" marker, compared by address and
// dereferenced as an empty string.
float defaultBarValue;
char unsetFillTextureName[1];

char* cloneBarName(const char* name)
{
    if (!name) return nullptr;
    const unsigned int bytes = static_cast<unsigned int>(std::strlen(name)) + 1;
    char* copy = static_cast<char*>(::operator new(bytes));
    std::memcpy(copy, name, bytes);
    return copy;
}
void releaseBarName(char*& owned)
{
    ::operator delete(owned);
    owned = nullptr;
}

// 7D1505/7D15A9/7D1620/7D1694 share this quantiser: the unfilled span is
// divided by the snap, rounded, and multiplied back. Only direction 0 can pick
// ceil; the other three always floor. A snap of zero divides by zero on the x87
// stack exactly as retail does - no guard is added.
int snapLength(float span, float value, int snap, bool towardsEnd)
{
    const float unfilled = (span - span * value) / static_cast<float>(snap);
    const float rounded = towardsEnd ? std::ceil(unfilled) : std::floor(unfilled);
    return static_cast<int>(rounded) * snap;
}
}

Bf2BarNode::Bf2BarNode(const char* name, int direction)
    : Bf2PictureNode(nullptr), m_fillTexture(0), m_fillTop(0), m_fillBottom(0),
      m_fillLeft(0), m_fillRight(0), m_snap(1), m_snapTowardsEnd(false),
      m_fillTextureBinding(&m_textureName),
      m_fillRequestedTextureName(unsetFillTextureName),
      m_fillPreviousTextureName(unsetFillTextureName),
      m_value(&defaultBarValue), m_direction(direction)
{
    setName(name); // the base already cloned a null name; retail repeats it
}

Bf2BarNode::~Bf2BarNode()
{
    // Only the requested fill name is released. The previously loaded one at
    // +84 is leaked by the shipped destructor; keep that.
    if (m_fillRequestedTextureName != unsetFillTextureName)
        ::operator delete(m_fillRequestedTextureName);
}

TypeDescriptor* Bf2BarNode::getClassType()
{
    // Complete eight-slot metadata939078; independent counter A23C70.
    static ClassTypeDescriptor<Bf2BarNode, Bf2PictureNode> type("dice::meme::Bf2BarNode");
    return &type;
}
TypeDescriptor* Bf2BarNode::getType() { return getClassType(); }
unsigned int Bf2BarNode::getObjectSize() { return sizeof(Bf2BarNode); }
namespace {
TypeDescriptor* const registeredBf2BarNodeType = Bf2BarNode::getClassType();
}

// native +EC..+104 accessors
bool Bf2BarNode::setIndexedTextureNameForHud(int index, const char* name)
{
    // Index 1 is the picture's own name, anything else the fill name. The
    // refusal is a pointer comparison against the shared empty literal.
    if (name == emptyHudTextureName) return false;
    char** target = index == 1 ? &m_requestedTextureName : &m_fillRequestedTextureName;
    if (**target) releaseBarName(*target);
    *target = cloneBarName(name);
    return true;
}
bool Bf2BarNode::setIndexedTextureBindingForHud(int index, const char** binding)
{
    if (!*binding) return false;
    if (index == 1) m_textureNameBinding = binding;
    else m_fillTextureBinding = binding;
    return true;
}
bool Bf2BarNode::setValueBindingForHud(float* value)
{
    // The binding is stored first and only then range-checked; an out-of-range
    // or unordered initial value is zeroed in place and the row reports false.
    m_value = value;
    if (*value >= 0.0f && *value <= 1.0f) return true;
    *value = 0.0f;
    return false;
}
void Bf2BarNode::setDirectionForHud(int direction)          // 4B3A40, returns nothing
{ m_direction = direction; }
bool Bf2BarNode::setFillBorderForHud(int top, int bottom, int left, int right)
{
    m_fillTop = top;
    m_fillBottom = bottom;
    m_fillLeft = left;
    m_fillRight = right;
    return true;
}
bool Bf2BarNode::setSnapForHud(int snap) { m_snap = snap; return true; }
bool Bf2BarNode::setSnapDirectionForHud(bool towardsEnd)
{ m_snapTowardsEnd = towardsEnd; return true; }

// Two independent texture pairs. The first pair short-circuits the
// whole body: when its name has not changed the call returns true without
// looking at the fill pair and without reaching Node::updateSelf. Also unlike
// Bf2PictureNode, the first pair's load has no empty-name or marker guard.
bool Bf2BarNode::updateSelf(System* system, const NodeState* state,
                           const Rect* bounds, float delta)
{
    const bool bound = m_textureNameBinding != &m_textureName;
    const char* const wanted = bound ? *m_textureNameBinding : m_requestedTextureName;
    if (std::strcmp(wanted, m_previousTextureName) == 0) return true;
    if (*m_previousTextureName) releaseBarName(m_previousTextureName);
    m_previousTextureName = cloneBarName(wanted);
    m_texture = system->loadTexture(m_previousTextureName, true);

    const bool fillBound = m_fillTextureBinding != &m_textureName;
    const char* const fillWanted =
        fillBound ? *m_fillTextureBinding : m_fillRequestedTextureName;
    if (std::strcmp(fillWanted, m_fillPreviousTextureName) != 0) {
        if (fillBound) {
            if (*m_fillPreviousTextureName) releaseBarName(m_fillPreviousTextureName);
            m_fillPreviousTextureName = cloneBarName(*m_fillTextureBinding);
        } else if (m_fillRequestedTextureName != unsetFillTextureName) {
            if (*m_fillPreviousTextureName) releaseBarName(m_fillPreviousTextureName);
            m_fillPreviousTextureName = cloneBarName(m_fillRequestedTextureName);
        }
        if (m_fillPreviousTextureName != unsetFillTextureName)
            m_fillTexture = system->loadTexture(m_fillPreviousTextureName, true);
    }
    return Node::updateSelf(system, state, bounds, delta);
}

// The background is the picture painted with the fill texture (or with
// a fully transparent effect colour when there is none), and the bar itself is
// one quad over the whole bounds clipped to the filled sub-rectangle.
void Bf2BarNode::paintSelf(System* system, const NodeState* state,
                          const Rect* bounds, const Rect* clip,
                          RenderContext* context)
{
    float value = *m_value;
    if (value > 1.0f) value = 1.0f;
    else if (value < 0.0f) value = 0.0f; // an unordered value survives both arms

    const Rect background = *bounds;
    Rect fill;
    fill.x = bounds->x + static_cast<float>(m_fillLeft);
    fill.y = bounds->y + static_cast<float>(m_fillTop);
    fill.width = bounds->width - static_cast<float>(m_fillRight)
               - static_cast<float>(m_fillLeft);
    fill.height = bounds->height - static_cast<float>(m_fillBottom)
                - static_cast<float>(m_fillTop);
    fill.z = 0.0f;

    // An unsigned compare: any direction above 3 leaves the fill rect alone.
    switch (static_cast<unsigned int>(m_direction)) {
    case 0: {
        const float length = value > 0.0f
            ? static_cast<float>(snapLength(fill.height, value, m_snap, m_snapTowardsEnd))
            : static_cast<float>(static_cast<int>(fill.height));
        fill.y += length;
        fill.height -= length;
        break;
    }
    case 1: {
        const float length = value > 0.0f
            ? static_cast<float>(snapLength(fill.height, value, m_snap, false))
            : static_cast<float>(static_cast<int>(fill.height));
        fill.height -= length;
        break;
    }
    case 2: {
        const float length = value > 0.0f
            ? static_cast<float>(snapLength(fill.width, value, m_snap, false))
            : static_cast<float>(static_cast<int>(fill.width));
        fill.x += length;
        fill.width -= length;
        break;
    }
    case 3: {
        const float length = value > 0.0f
            ? static_cast<float>(snapLength(fill.width, value, m_snap, false))
            : static_cast<float>(static_cast<int>(fill.width));
        fill.width -= length;
        break;
    }
    default:
        break;
    }

    const TextureHandle barTexture = m_texture;
    m_texture = m_fillTexture; // swapped for the background paint, restored below
    if (m_fillTexture != 0) {
        Bf2PictureNode::paintSelf(system, state, &background, &background, context);
    } else {
        Color saved = {1.0f, 1.0f, 1.0f, 1.0f};
        if (context->effect) {
            Color* const current = context->effect->getColor();
            if (current) saved = *current;
        }
        if (context->effect) context->effect->setColor(0.0f, 0.0f, 0.0f, 0.0f);
        Bf2PictureNode::paintSelf(system, state, &background, &background, context);
        if (context->effect)
            context->effect->setColor(saved.r, saved.g, saved.b, saved.a);
    }
    m_texture = barTexture;
    Quad quad(&background, &fill); // the fill rect is the clip, not the bounds
    drawContextTexture(context, system, m_texture, &quad);
}

#if defined(_M_IX86)
static_assert(sizeof(Bf2BarNode) == 144, "native Bf2BarNode storage");
static_assert(sizeof(ClassTypeDescriptor<Bf2BarNode, Bf2PictureNode>) == 16,
    "Bf2BarNode metadata");
#endif
} }
#endif
