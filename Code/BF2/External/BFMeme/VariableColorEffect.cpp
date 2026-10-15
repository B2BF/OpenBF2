// [INFERRED] path beside attested BFMeme System/Style. Full original21-slot
// primary, eight-slot metadata, construction and draw helpers read before code.
#include "VariableColorEffect.h"
#if !DEDICATED
#include "Data.h"
#include "Quad.h"
#include "Stream.h"
#include "TypeDescriptor.h"
namespace dice { namespace meme {

VariableColorEffect::VariableColorEffect()                // 7CADF0 inlined construction
    : m_red(nullptr), m_green(nullptr), m_blue(nullptr), m_alpha(nullptr) {}
VariableColorEffect::~VariableColorEffect() {}

TypeDescriptor* VariableColorEffect::getClassType()        // 7CADB0, metadata9375F0
{
    static ClassTypeDescriptor<VariableColorEffect, Effect> type("dice::meme::VariableColorEffect");
    return &type;
}
TypeDescriptor* VariableColorEffect::getType() { return getClassType(); }
unsigned int VariableColorEffect::getObjectSize() { return sizeof(VariableColorEffect); }
namespace {
// CRT96DA60 ->86B820 unconditionally sets the guard before constructing the
// same descriptor and storesA227C4; source-native static lifetime owns teardown.
TypeDescriptor* const registeredVariableColorEffectType = VariableColorEffect::getClassType();
}

bool VariableColorEffect::streamProperties(System* system, Stream* stream)
{
    if (!Effect::streamProperties(system, stream)) return false;
    return stream->streamData(system, m_red, "Red", nullptr)
        && stream->streamData(system, m_green, "Green", nullptr)
        && stream->streamData(system, m_blue, "Blue", nullptr)
        && stream->streamData(system, m_alpha, "Alpha", nullptr);
}

bool VariableColorEffect::drawCharacter(System* system, int character, FontHandle font,
                                        int elementIndex, Quad* quad, RenderContext* context)
{
    // read each current field only after the preceding callback. Each
    // returned value is stored to float32 before the next component is sampled.
    Color color;
    color.r = m_red ? m_red->getFloat(system) : 1.0f;
    color.g = m_green ? m_green->getFloat(system) : 1.0f;
    color.b = m_blue ? m_blue->getFloat(system) : 1.0f;
    color.a = m_alpha ? m_alpha->getFloat(system) : 1.0f;
    for (auto& corner : quad->corners) corner.color = color; // 771FA0, overwrite
    return Effect::drawCharacter(system, character, font, elementIndex, quad, context);
}

bool VariableColorEffect::drawTexture(System* system, TextureHandle texture, Quad* quad,
                                      RenderContext* context)
{
    // 7CAC40 has the same ordered sampling, including independent null defaults.
    Color color;
    color.r = m_red ? m_red->getFloat(system) : 1.0f;
    color.g = m_green ? m_green->getFloat(system) : 1.0f;
    color.b = m_blue ? m_blue->getFloat(system) : 1.0f;
    color.a = m_alpha ? m_alpha->getFloat(system) : 1.0f;
    for (auto& corner : quad->corners) corner.color = color;
    return Effect::drawTexture(system, texture, quad, context);
}

#if defined(_M_IX86)
static_assert(sizeof(ClassTypeDescriptor<VariableColorEffect, Effect>) == 16,
              "VariableColorEffect metadata storage");
#endif
} }
#endif
