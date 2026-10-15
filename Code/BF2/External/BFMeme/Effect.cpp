// Path [INFERRED]. All 21 table entries, descriptor/factory, and dispatch helper
// instructions were read from BF2.exe.
#include "Effect.h"
#if !DEDICATED
#include "Node.h"
#include "System.h"
#include "TypeDescriptor.h"

namespace dice { namespace meme {
Effect::~Effect() {}                                  // 835660 -> Object destructor
TypeDescriptor* Effect::getClassType()
{
    static ClassTypeDescriptor<Effect, Object> type("dice::meme::Effect");
    return &type;
}
TypeDescriptor* Effect::getType() { return getClassType(); }
unsigned int Effect::getObjectSize() { return sizeof(Effect); }
bool Effect::setColor(float, float, float, float) { return false; }
Color* Effect::getColor() { return nullptr; }
bool Effect::update(System*, float) { return true; }    // 4553F0, real default hook

bool updateEffect(System* system, Effect* effect, float delta)
{
    if (!effect) return true;
    return effect->update(system, delta);
}

bool drawContextCharacter(RenderContext* context, System* system, int character,
                          FontHandle font, int elementIndex, Quad* quad)
{
    while (context && !context->effect) context = context->parent;
    if (!context) return system->drawCharacter(character, font, quad);
    return context->effect->drawCharacter(system, character, font, elementIndex, quad, context);
}
bool drawContextTexture(RenderContext* context, System* system, TextureHandle texture,
                        Quad* quad)
{
    while (context && !context->effect) context = context->parent;
    if (!context) return system->drawTexture(texture, quad);
    return context->effect->drawTexture(system, texture, quad, context);
}
bool Effect::drawCharacter(System* system, int character, FontHandle font, int elementIndex,
                           Quad* quad, RenderContext* context)
{
    // Retail requires a context and dereferences it even when its parent is null.
    return drawContextCharacter(context->parent, system, character, font, elementIndex, quad);
}
bool Effect::drawTexture(System* system, TextureHandle texture, Quad* quad,
                         RenderContext* context)
{
    return drawContextTexture(context->parent, system, texture, quad);
}
TypeDescriptor* getEffectClassType() { return Effect::getClassType(); }

#if defined(_M_IX86)
static_assert(sizeof(Effect) == 4, "PE Effect allocation");
static_assert(sizeof(RenderContext) == 28, "seven-word Effect context");
#endif
} }
#endif
