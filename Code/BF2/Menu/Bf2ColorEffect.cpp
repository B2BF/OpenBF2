// Menu color effect. [INFERRED] Source placement.
#include "Bf2ColorEffect.h"
#if !DEDICATED
#include "../External/BFMeme/Quad.h"
#include "../External/BFMeme/TypeDescriptor.h"
#include <new>
namespace dice { namespace meme {

Bf2ColorEffect::Bf2ColorEffect(float red, float green, float blue, float alpha)
    : ColorEffect(1.0f, 1.0f, 1.0f, 1.0f),
      m_alphaBinding(&m_disabledBinding), m_redBinding(&m_disabledBinding),
      m_greenBinding(&m_disabledBinding), m_blueBinding(&m_disabledBinding),
      m_field28(0), m_disabledBinding(-1.0f)
{
    // The base is constructed with white and the requested colour overwrites it
    // afterwards, exactly as the shipped body does. m_owner is left alone.
    setColor(red, green, blue, alpha);
}
Bf2ColorEffect::~Bf2ColorEffect() {}                       // 7C2410, nothing owned

TypeDescriptor* Bf2ColorEffect::getClassType()
{
    // Complete eight-slot metadata936080; independent counter A221D8.
    static ClassTypeDescriptor<Bf2ColorEffect, ColorEffect> type("dice::meme::Bf2ColorEffect");
    return &type;
}
TypeDescriptor* Bf2ColorEffect::getType() { return getClassType(); }
unsigned int Bf2ColorEffect::getObjectSize() { return sizeof(Bf2ColorEffect); }
namespace {
TypeDescriptor* const registeredBf2ColorEffectType = Bf2ColorEffect::getClassType();
}

bool Bf2ColorEffect::setColor(float r, float g, float b, float a)
{
    Color* const target = getColor();
    target->r = r;
    target->g = g;
    target->b = b;
    target->a = a;
    return true;
}
bool Bf2ColorEffect::setOwnerNode(void* owner) { m_owner = owner; return true; }
bool Bf2ColorEffect::setAlphaBinding(const float* alpha)
{ m_alphaBinding = alpha; return true; }
bool Bf2ColorEffect::setRgbBindings(const float* red, const float* green, const float* blue)
{ m_redBinding = red; m_greenBinding = green; m_blueBinding = blue; return true; }

namespace {
// The shared sampling both draw bodies perform before handing the quad on.
// A negative or unordered sample leaves that channel alone; only the alpha is
// multiplied, the three colour channels are replaced.
Color sampleBoundColor(const Color& base, const float* alpha, const float* red,
                       const float* green, const float* blue)
{
    Color color = base;
    if (alpha && *alpha >= 0.0f) color.a *= *alpha;
    if (red && *red >= 0.0f) color.r = *red;
    if (green && *green >= 0.0f) color.g = *green;
    if (blue && *blue >= 0.0f) color.b = *blue;
    return color;
}
}

bool Bf2ColorEffect::drawCharacter(System* system, int character, FontHandle font,
                                  int elementIndex, Quad* quad, RenderContext* context)
{
    const Color color = sampleBoundColor(*getColor(), m_alphaBinding, m_redBinding,
                                         m_greenBinding, m_blueBinding);
    colorQuad(quad, &color);
    return Effect::drawCharacter(system, character, font, elementIndex, quad, context);
}
bool Bf2ColorEffect::drawTexture(System* system, TextureHandle texture, Quad* quad,
                                RenderContext* context)
{
    const Color color = sampleBoundColor(*getColor(), m_alphaBinding, m_redBinding,
                                         m_greenBinding, m_blueBinding);
    colorQuad(quad, &color);
    return Effect::drawTexture(system, texture, quad, context);
}

#if defined(_M_IX86)
static_assert(sizeof(Bf2ColorEffect) == 48, "native Bf2ColorEffect storage");
static_assert(sizeof(ClassTypeDescriptor<Bf2ColorEffect, ColorEffect>) == 16,
    "Bf2ColorEffect metadata");
#endif
} }
#endif
