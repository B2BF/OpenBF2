#include "ColorEffect.h"
#if !DEDICATED
#include "Stream.h"
#include "TypeDescriptor.h"
namespace dice { namespace meme {
ColorEffect::ColorEffect(float red, float green, float blue, float alpha)
    : m_color{red, green, blue, alpha} {}
ColorEffect::~ColorEffect() {} // 835660 -> RuntimeObject destructor
TypeDescriptor* ColorEffect::getClassType()
{
    static ClassTypeDescriptor<ColorEffect, Effect> type("dice::meme::ColorEffect");
    return &type;
}
TypeDescriptor* ColorEffect::getType() { return getClassType(); }
unsigned int ColorEffect::getObjectSize() { return sizeof(ColorEffect); }
Color* ColorEffect::getColor() { return &m_color; }
bool ColorEffect::streamProperties(System* system, Stream* stream)
{
    if (!Effect::streamProperties(system, stream)) return false;
    return stream->streamFloat(&m_color.r, "Red")
        && stream->streamFloat(&m_color.g, "Green")
        && stream->streamFloat(&m_color.b, "Blue")
        && stream->streamFloat(&m_color.a, "Alpha");
}
bool ColorEffect::drawCharacter(System* system, int character, FontHandle font,
                                int elementIndex, Quad* quad, RenderContext* context)
{
    for (auto& corner : quad->corners) corner.color = m_color;
    return Effect::drawCharacter(system, character, font, elementIndex, quad, context);
}
bool ColorEffect::drawTexture(System* system, TextureHandle texture, Quad* quad, RenderContext* context)
{
    for (auto& corner : quad->corners) corner.color = m_color;
    return Effect::drawTexture(system, texture, quad, context);
}
#if defined(_M_IX86)
static_assert(sizeof(ColorEffect) == 20, "Native x86 ColorEffect layout");
#endif
} }
#endif

