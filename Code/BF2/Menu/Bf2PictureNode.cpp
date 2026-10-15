// Menu picture node. [INFERRED] Source placement.
// Unimplemented inherited Node operations retain their diagnostic stops.
#include "Bf2PictureNode.h"
#if !DEDICATED
#include "../External/BFMeme/Quad.h"
#include "../External/BFMeme/System.h"
#include "../External/BFMeme/Effect.h"
#include "../External/BFMeme/TypeDescriptor.h"
#include <cstring>
#include <new>
namespace dice { namespace meme {
namespace {
// [INFERRED] name. A20F44 is a loader-zeroed static byte the constructor uses
// as the "no texture requested yet" marker. It is compared by address and
// dereferenced as an empty string, so it must be a real zero byte.
char unsetTextureName[1];

char* clonePictureName(const char* name)
{
    if (!name) return nullptr;
    const unsigned int bytes = static_cast<unsigned int>(std::strlen(name)) + 1;
    char* copy = static_cast<char*>(::operator new(bytes));
    std::memcpy(copy, name, bytes); // no allocation-null guard in the original
    return copy;
}
void releasePictureName(char*& owned)
{
    ::operator delete(owned);
    owned = nullptr;
}

const float degreesToRadians = 0.017453292f;
const float halfTurn = 3.1415927f;
const float fullTurn = 6.2831855f;
}

// The empty-string constant every constructor stores at +28.
const char* const emptyHudTextureName = "";

Bf2PictureNode::Bf2PictureNode(const char* name)
    : PictureNode(0), m_centerSpecified(false), m_centerX(0.0f), m_centerY(0.0f),
      m_name(nullptr), m_rotationBinding(&m_rotation), m_rotation(0.0f),
      m_additionalRotation(0.0f), m_textureName(emptyHudTextureName),
      m_textureNameBinding(&m_textureName),
      m_requestedTextureName(unsetTextureName),
      m_previousTextureName(unsetTextureName), m_secondaryTexture(0),
      m_borderEnabled(false), m_borderLeft(0), m_borderRight(0),
      m_borderTop(0), m_borderBottom(0), m_borderRed(0.0f), m_borderGreen(0.0f),
      m_borderBlue(0.0f), m_borderAlpha(0.0f)
{
    // The tail is setName inlined: release the just-cleared field, then clone.
    releasePictureName(m_name);
    m_name = clonePictureName(name);
}

Bf2PictureNode::~Bf2PictureNode()
{
    releasePictureName(m_name);
    // Only clones are released; the shared unset marker is left alone. A clone
    // of an empty name is never released by the accessors, so retail can leak
    // one here as well - do not add a length test.
    if (m_requestedTextureName != unsetTextureName)
        ::operator delete(m_requestedTextureName);
    if (m_previousTextureName != unsetTextureName)
        ::operator delete(m_previousTextureName);
}

TypeDescriptor* Bf2PictureNode::getClassType()
{
    // Complete eight-slot metadata935108; independent counter A20F4C. Eager
    // 869D60 registers the same descriptor before any HUD script runs.
    static ClassTypeDescriptor<Bf2PictureNode, PictureNode> type("dice::meme::Bf2PictureNode");
    return &type;
}
TypeDescriptor* Bf2PictureNode::getType() { return getClassType(); }
unsigned int Bf2PictureNode::getObjectSize() { return sizeof(Bf2PictureNode); }
namespace {
TypeDescriptor* const registeredBf2PictureNodeType = Bf2PictureNode::getClassType();
}

const char* Bf2PictureNode::getName() { return m_name; }
bool Bf2PictureNode::setName(const char* name)
{
    releasePictureName(m_name); // retains the shipped free-before-copy aliasing defect
    m_name = clonePictureName(name);
    return true;
}
bool Bf2PictureNode::slot_0x24() { return true; }

// native +C8..+E8 accessors
bool Bf2PictureNode::setTextureNameForHud(const char* name)
{
    // Pointer identity against the bound name, not a comparison of the text.
    if (name == m_textureName) return false;
    if (*m_requestedTextureName) releasePictureName(m_requestedTextureName);
    m_requestedTextureName = clonePictureName(name);
    return true;
}
bool Bf2PictureNode::setTextureForHud(TextureHandle texture)
{ m_texture = texture; return true; }
bool Bf2PictureNode::setSecondaryTextureForHud(TextureHandle texture)
{ m_secondaryTexture = texture; return true; }
bool Bf2PictureNode::setTextureNameBindingForHud(const char** binding)
{
    if (!*binding) return false; // the pointed-to text, not the binding, is checked
    m_textureNameBinding = binding;
    return true;
}
bool Bf2PictureNode::setRotationBindingForHud(const float* binding)
{ m_rotationBinding = binding; return true; }
bool Bf2PictureNode::setAdditionalRotationForHud(float degrees)
{ m_additionalRotation = degrees; return true; }
bool Bf2PictureNode::setCenterPointForHud(int x, int y)
{
    m_centerX = static_cast<float>(x);
    m_centerSpecified = true; // written between the two coordinate stores
    m_centerY = static_cast<float>(y);
    return true;
}
bool Bf2PictureNode::setBorderForHud(int left, int right, int top, int bottom)
{
    m_borderLeft = left;
    m_borderRight = right;
    m_borderTop = top;
    m_borderEnabled = true; // set before the last store, as in retail
    m_borderBottom = bottom;
    return true;
}
bool Bf2PictureNode::setBorderColorForHud(float r, float g, float b, float a)
{
    m_borderRed = r;
    m_borderGreen = g;
    m_borderBlue = b;
    m_borderAlpha = a;
    return true;
}
bool dispatchPictureTexture(Bf2PictureNode* node, TextureHandle texture) // 7511C0's callee
{ return node->setTextureForHud(texture); }

// The texture is (re)loaded only when the requested and previously
// loaded names differ, and the comparison source depends on whether a variable
// texture is bound. Both string compares are plain strcmp on possibly empty
// text; neither pointer is null-checked.
bool Bf2PictureNode::updateSelf(System* system, const NodeState* state,
                               const Rect* bounds, float delta)
{
    const bool bound = m_textureNameBinding != &m_textureName;
    bool reload;
    if (!bound)
        reload = std::strcmp(m_requestedTextureName, m_previousTextureName) != 0;
    else
        reload = std::strcmp(*m_textureNameBinding, m_previousTextureName) != 0;
    if (reload) {
        if (bound) {
            if (m_previousTextureName != *m_textureNameBinding) {
                if (*m_previousTextureName) releasePictureName(m_previousTextureName);
                m_previousTextureName = clonePictureName(*m_textureNameBinding);
            }
        } else if (m_requestedTextureName != unsetTextureName) {
            if (*m_previousTextureName) releasePictureName(m_previousTextureName);
            m_previousTextureName = clonePictureName(m_requestedTextureName);
        }
        if (m_previousTextureName != unsetTextureName &&
            std::strlen(m_previousTextureName) > 0)
            m_texture = system->loadTexture(m_previousTextureName, true);
        else
            m_texture = 0; // an empty or unset name clears the handle
    }
    return Node::updateSelf(system, state, bounds, delta);
}

// Two quads are built from the same bounds/clip: the picture itself and
// a scratch quad reused for the four border strips. Rotation is applied to the
// already-built quads, so the border strips rotate about the same centre.
void Bf2PictureNode::paintSelf(System* system, const NodeState* state,
                              const Rect* bounds, const Rect* clip,
                              RenderContext* context)
{
    Quad picture(bounds, clip);
    Quad border(bounds, clip);
    float center[2];
    center[0] = bounds->width * 0.5f + bounds->x;
    center[1] = bounds->height * 0.5f + bounds->y;
    float radians = 0.0f;
    if (m_centerSpecified) {
        center[0] = m_centerX;
        center[1] = m_centerY;
    }
    if (m_additionalRotation != 0.0f) {
        radians = m_additionalRotation * degreesToRadians;
        // Only the negative half-turn is wrapped; a large positive angle is not.
        if (radians + halfTurn < 0.0f) radians += fullTurn;
    } else if (m_rotationBinding != &m_rotation) {
        radians = *m_rotationBinding;
    }
    if (m_secondaryTexture != 0) {
        picture.texture2 = m_secondaryTexture;
        border.texture2 = m_secondaryTexture;
    }
    if (radians != 0.0f) {
        rotateQuad(&picture, center, radians);
        rotateQuad(&border, center, radians);
    }
    drawContextTexture(context, system, m_texture, &picture);
    if (!m_borderEnabled) return;

    const float left = bounds->x;
    const float top = bounds->y;
    const float right = left + bounds->width;
    const float bottom = top + bounds->height;
    // White is the value restored when the effect has no color of its own.
    Color saved = {1.0f, 1.0f, 1.0f, 1.0f};
    if (context->effect) {
        Color* const current = context->effect->getColor();
        if (current) {
            saved = *current;
            if (context->effect) // the second read is in the shipped body
                context->effect->setColor(m_borderRed, m_borderGreen,
                                          m_borderBlue, m_borderAlpha);
        }
    }
    const Color borderColor = {m_borderRed, m_borderGreen, m_borderBlue, m_borderAlpha};
    colorQuad(&border, &borderColor);

    const float insetTop = top + static_cast<float>(m_borderTop);
    const float insetRight = right - static_cast<float>(m_borderRight);
    const float insetBottom = bottom - static_cast<float>(m_borderBottom);
    const float insetLeft = left + static_cast<float>(m_borderLeft);
    const float strips[4][8] = {
        {left, top,         right,      top,         right,      insetTop,    left,       insetTop},
        {insetRight, insetTop, right,    insetTop,    right,      bottom,      insetRight, bottom},
        {left, insetBottom, insetRight, insetBottom, insetRight, bottom,      left,       bottom},
        {left, insetTop,    insetLeft,  insetTop,    insetLeft,  bottom,      left,       bottom},
    };
    for (unsigned int strip = 0; strip != 4; ++strip) {
        for (unsigned int corner = 0; corner != 4; ++corner) {
            border.corners[corner].x = strips[strip][corner * 2];
            border.corners[corner].y = strips[strip][corner * 2 + 1];
            border.corners[corner].z = 0.0f; // the bounds z is not carried here
        }
        if (radians != 0.0f) rotateQuad(&border, center, radians);
        drawContextTexture(context, system, 0, &border);
    }
    if (context->effect)
        context->effect->setColor(saved.r, saved.g, saved.b, saved.a);
}

#if defined(_M_IX86)
static_assert(sizeof(Bf2PictureNode) == 96, "native Bf2PictureNode storage");
static_assert(sizeof(ClassTypeDescriptor<Bf2PictureNode, PictureNode>) == 16,
    "Bf2PictureNode metadata");
#endif
} }
#endif
