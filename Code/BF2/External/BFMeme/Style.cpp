// Attested source: BF2.exe0x94EA50, External/BFMeme/Style.cpp.
// Every written body was read instruction-by-instruction.
#include "Style.h"
#if !DEDICATED
#include "Data.h"
#include "System.h"
#include "Quad.h"
#include "Stream.h"
#include "Effect.h"
#include "TypeDescriptor.h"
#include "../../Menu/Font.h"
#include "../../Menu/DifFontMetadata.h"
#include <cmath>
#include <cstring>
#include <new>

namespace dice { namespace meme {
namespace {
char* cloneStyleName(const char* name)               // shared helper0x737DC0
{
    if (!name) return 0;
    const unsigned int bytes = static_cast<unsigned int>(std::strlen(name)) + 1;
    char* copy = static_cast<char*>(::operator new(bytes));
    std::memcpy(copy, name, bytes);
    return copy;
}
}

Style::Style(const char* name, FontHandle font)
    : m_font(font), m_name(cloneStyleName(name))
{
    // Retail deliberately leaves m_shadowFont and both scale fields untouched.
}
Style::~Style()
{
    ::operator delete(m_name);
    m_name = 0;
}
unsigned int Style::getObjectSize() { return sizeof(Style); }
TypeDescriptor* Style::getClassType()               // 838C90, factory838D40
{
    static ClassTypeDescriptor<Style, Object> type("dice::meme::Style");
    return &type;
}
TypeDescriptor* Style::getType() { return getClassType(); }
const char* Style::getName() { return m_name; }
bool Style::setName(const char* name)
{
    ::operator delete(m_name);
    m_name = 0;
    m_name = cloneStyleName(name); // Preserve free-before-copy, including aliasing defect.
    return true;
}
bool Style::streamProperties(System* system, Stream* stream)
{
    if (!Object::streamProperties(system, stream)) return false;
    return stream->streamFont(system, &m_font, "Font handle") != false;
}
void Style::prepareFont()
{
    hfe::menu::Font* font = reinterpret_cast<hfe::menu::Font*>(m_font);
    std::string name(font->getName());               // no null-font guard in retail
    if (name.find(".dif", 0, 4) != std::string::npos)
        m_scale = static_cast<hfe::menu::DifFont*>(font)->getMetadata()->getScale();
    else
        m_scale = 1.0f;
    m_reciprocalScale = static_cast<float>(1.0 / m_scale);
}

float Style::getCharacterWidth(System* system, int character)
{
    GlyphMetrics metrics; // Failure leaves the retail output uninitialized.
    system->getGlyphMetrics(character, this ? m_font : 0, &metrics);
    return static_cast<float>((static_cast<double>(metrics.leftBearing) +
                               metrics.rightBearing) + metrics.width);
}
float Style::getStringWidth(System* system, const char* text)
{
    if (!this) return 0.0f;
    float width = 0.0f;
    for (const char* at = text; *at; ++at)
    {
        GlyphMetrics metrics;
        system->getGlyphMetrics(static_cast<signed char>(*at), m_font, &metrics);
        width = static_cast<float>(((static_cast<double>(metrics.rightBearing) +
                    metrics.width) + metrics.leftBearing) + width);
    }
    return width;
}
float Style::getDataWidth(System* system, Data* data, int elementIndex)
{
    if (!this) return 0.0f;
    if (!data) return 0.0f;
    float width = 0.0f;
    int index = 0;
    int character = data->getCharacterElement(system, elementIndex, index);
    while (character)
    {
        GlyphMetrics metrics;
        system->getGlyphMetrics(character, m_font, &metrics);
        width = static_cast<float>(((static_cast<double>(metrics.rightBearing) +
                    metrics.width) + metrics.leftBearing) + width);
        character = data->getCharacterElement(system, elementIndex, ++index);
    }
    return width;
}

void Style::drawText(System* system, Data* data, int elementIndex,
                     const Rect* bounds, const Rect* clip, RenderContext* context)
{
    float penX = bounds->x;
    const float right = static_cast<float>(static_cast<double>(penX) + bounds->width);
    float penY = bounds->y;
    const float bottom = static_cast<float>(static_cast<double>(penY) + bounds->height);
    const float lineHeight = system->getFontHeight(m_font);
    penY = static_cast<float>(static_cast<double>(lineHeight) + penY);
    Rect glyphBounds{0.0f, 0.0f, 0.0f, 0.0f, bounds->z};
    prepareFont();
    int index = 0;
    int character = data->getCharacterElement(system, elementIndex, index);
    while (character)
    {
        if (character == 13)
        {
            penX = bounds->x;
        }
        else
        {
            bool nextLine = character == 10;
            if (!nextLine)
            {
                GlyphMetrics metrics;
                system->getGlyphMetrics(character, m_font, &metrics);
                glyphBounds.height = metrics.height;
                penX = static_cast<float>(static_cast<double>(metrics.leftBearing) + penX);
                glyphBounds.width = metrics.width;
                positionGlyph(&glyphBounds, &metrics, &penX, &penY);
                Quad glyph(&glyphBounds, clip);
                if (m_shadowFont)
                {
                    Color black{0.0f, 0.0f, 0.0f, 1.0f};
                    Quad shadow(&glyphBounds, clip, &black);
                    if (!drawContextCharacter(context, system, character, m_shadowFont, index, &shadow))
                        return;
                }
                if (!drawContextCharacter(context, system, character, m_font, index, &glyph))
                    return;
                const double nextX = (static_cast<double>(metrics.rightBearing) + metrics.width) + penX;
                penX = static_cast<float>(nextX);
                if (nextX >= right)
                {
                    penX = bounds->x;
                    nextLine = true;
                }
            }
            if (nextLine)
            {
                const double nextY = static_cast<double>(lineHeight) + penY;
                penY = static_cast<float>(nextY);
                if (nextY >= bottom) return;
            }
        }
        character = data->getCharacterElement(system, elementIndex, ++index);
    }
}

void Style::positionGlyph(Rect* output, const GlyphMetrics* metrics,
                          float* x, float* y)
{
    if (m_scale == 1.0f)
    {
        output->x = *x;
        output->y = static_cast<float>(static_cast<double>(metrics->verticalOffset) + *y);
        return;
    }
    const float scaledX = static_cast<float>(static_cast<double>(*x) * m_scale);
    const float scaledY = static_cast<float>(static_cast<double>(*y) * m_scale);
    // x87 tests C0 alone: unordered follows the same arm as remainder<0.5.
    const double roundedX = std::floor(!(std::fmod(scaledX, 1.0) >= 0.5) ?
                                        scaledX : static_cast<double>(scaledX) + 1.0);
    const double positionX = roundedX * m_reciprocalScale;
    *x = static_cast<float>(positionX);
    output->x = static_cast<float>(positionX);
    const double roundedY = std::floor(!(std::fmod(scaledY, 1.0) >= 0.5) ?
                                        scaledY : static_cast<double>(scaledY) + 1.0);
    const float verticalOffset = metrics->verticalOffset;
    output->y = static_cast<float>(roundedY * m_reciprocalScale - verticalOffset);
    // Retail updates *x but leaves *y unchanged in the scaled arm. The unscaled
    // arm adds the vertical offset, whereas this arm subtracts it.
}

void Style::positionGlyphInt(Rect* output, const GlyphMetrics* metrics,
                             int* x, int* y)
{
    float floatX = static_cast<float>(*x);
    float floatY = static_cast<float>(*y);
    positionGlyph(output, metrics, &floatX, &floatY); // preserve virtual dispatch
    *x = static_cast<int>(floatX);
    *y = static_cast<int>(floatY);
}

#if defined(_M_IX86)
static_assert(sizeof(Style) == 24, "PE Style allocation");
#endif
static_assert(sizeof(GlyphMetrics) == 20, "five scalar glyph metrics");
static_assert(sizeof(Rect) == 20, "five scalar drawing bounds");
} }
#endif
