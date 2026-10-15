// Movement effect drawing and property serialization.
#include "MoveEffect.h"
#if !DEDICATED
#include "Data.h"
#include "Node.h"
#include "Quad.h"
#include "Stream.h"
#include "TypeDescriptor.h"
#include <cmath>
namespace dice { namespace meme {

MoveEffect::MoveEffect(Data* direction, Data* length)
    : m_length(length), m_direction(direction) {}
MoveEffect::~MoveEffect() {}                               // 835660, owns neither

TypeDescriptor* MoveEffect::getClassType()                 // 832700 / factory832740
{
    static ClassTypeDescriptor<MoveEffect, Effect> type("dice::meme::MoveEffect");
    return &type;
}
TypeDescriptor* MoveEffect::getType() { return getClassType(); }
unsigned int MoveEffect::getObjectSize() { return sizeof(MoveEffect); }

bool MoveEffect::streamProperties(System* system, Stream* stream)
{
    if (!Effect::streamProperties(system, stream)) return false;
    if (!stream->streamData(system, reinterpret_cast<Data*&>(m_length), "Move length", nullptr))
        return false;
    return stream->streamData(system, reinterpret_cast<Data*&>(m_direction),
                              "Move direction", nullptr);
}

namespace {
// The shared offset both draw bodies compute. Direction and length are sampled
// from the two borrowed expression nodes; the distance still to travel is
// (1 - context->field18) scaled by the length, and the pen moves against the
// direction on x and with it on y.
// When m_length is null the length stays the shipped 0 (93C850) - the effect
// then contributes nothing, and the direction was still sampled first.
void computeMoveOffset(MoveEffect* self, System* system, Data* direction, Data* length,
                       const RenderContext* context, float& dx, float& dy)
{
    (void)self;
    float radians = 0.0f;
    if (direction) radians = direction->getFloat(system);
    float distance = 0.0f;
    if (length) distance = length->getFloat(system);
    const float remaining = (1.0f - context->field18) * distance; // 87FE78 is 1.0f
    dx = -(std::cos(radians) * remaining);
    dy = std::sin(radians) * remaining;
}
}

bool MoveEffect::drawCharacter(System* system, int character, FontHandle font,
                               int elementIndex, Quad* quad, RenderContext* context)
{
    float dx = 0.0f, dy = 0.0f;
    computeMoveOffset(this, system, m_direction, m_length, context, dx, dy);
    translateQuad(quad, dx, dy, 0.0f);
    const Rect* const clip = quad->clip;
    if (!clip)
        return Effect::drawCharacter(system, character, font, elementIndex, quad, context);
    // The clip travels with the quad, on a stack copy that is swapped back out
    // again the moment the chain returns.
    Rect shifted;
    shifted.x = clip->x + dx;
    shifted.y = clip->y + dy;
    shifted.width = clip->width;
    shifted.height = clip->height;
    shifted.z = clip->z;
    quad->clip = &shifted;
    const bool drawn =
        Effect::drawCharacter(system, character, font, elementIndex, quad, context);
    quad->clip = clip;
    return drawn;
}

bool MoveEffect::drawTexture(System* system, TextureHandle texture, Quad* quad,
                             RenderContext* context)
{
    float dx = 0.0f, dy = 0.0f;
    computeMoveOffset(this, system, m_direction, m_length, context, dx, dy);
    translateQuad(quad, dx, dy, 0.0f);
    const Rect* const clip = quad->clip;
    if (!clip) return Effect::drawTexture(system, texture, quad, context);
    Rect shifted;
    shifted.x = clip->x + dx;
    shifted.y = clip->y + dy;
    shifted.width = clip->width;
    shifted.height = clip->height;
    shifted.z = clip->z;
    quad->clip = &shifted;
    const bool drawn = Effect::drawTexture(system, texture, quad, context);
    quad->clip = clip;
    return drawn;
}

#if defined(_M_IX86)
static_assert(sizeof(MoveEffect) == 12, "PE MoveEffect allocation");
#endif
} }
#endif
