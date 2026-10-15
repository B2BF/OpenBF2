// Blend-function effect descriptors and drawing operations.
#include "BlendFuncEffect.h"
#if !DEDICATED
#include "Quad.h"
#include "Stream.h"
#include "TypeDescriptor.h"
namespace dice { namespace meme {

const char* __fastcall blendFuncName(int value)
{
    // A twelve-entry jump table; everything outside0..11 answers null.
    switch (value) {
    case 0: return "ERROR";
    case 1: return "Zero";
    case 2: return "One";
    case 3: return "Source color";
    case 4: return "One minus source color";
    case 5: return "Dest color";
    case 6: return "One minus dest color";
    case 7: return "Source alpha";
    case 8: return "One minus source alpha";
    case 9: return "Dest alpha";
    case 10: return "One minus dest alpha";
    case 11: return "Source alpha saturate";
    default: return nullptr;
    }
}
int __fastcall blendFuncNext(int value)
{
    // Zero terminates the walk, so a caller that starts at0 and steps forward
    // stops on the first unnamed value rather than on a count.
    if (!blendFuncName(value)) return 0;
    return value + 1;
}

BlendFuncEffect::BlendFuncEffect(int source, int destination)
    : m_source(source), m_destination(destination) {}
BlendFuncEffect::~BlendFuncEffect() {}                     // 835660 -> Object destructor

TypeDescriptor* BlendFuncEffect::getClassType()            // 8396E0 / factory839720
{
    static ClassTypeDescriptor<BlendFuncEffect, Effect> type("dice::meme::BlendFuncEffect");
    return &type;
}
TypeDescriptor* BlendFuncEffect::getType() { return getClassType(); }
unsigned int BlendFuncEffect::getObjectSize() { return sizeof(BlendFuncEffect); }

bool BlendFuncEffect::streamProperties(System* system, Stream* stream)
{
    if (!Effect::streamProperties(system, stream)) return false;
    if (!stream->streamEnum(&m_source, blendFuncNext, blendFuncName, "Source blend func"))
        return false;
    return stream->streamEnum(&m_destination, blendFuncNext, blendFuncName,
                              "Destination blend func");
}

bool BlendFuncEffect::drawCharacter(System* system, int character, FontHandle font,
                                    int elementIndex, Quad* quad, RenderContext* context)
{
    // Both words are stamped before the chain continues; retail inlines
    // Effect's one-line body into a direct 82D730 call on context->parent.
    quad->state08 = m_source;
    quad->state0c = m_destination;
    return Effect::drawCharacter(system, character, font, elementIndex, quad, context);
}
bool BlendFuncEffect::drawTexture(System* system, TextureHandle texture, Quad* quad,
                                  RenderContext* context)
{
    quad->state08 = m_source;
    quad->state0c = m_destination;
    return Effect::drawTexture(system, texture, quad, context);
}

#if defined(_M_IX86)
static_assert(sizeof(BlendFuncEffect) == 12, "PE BlendFuncEffect allocation");
#endif
} }
#endif
