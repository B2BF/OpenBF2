// BF2 menu text styles with left, right and centered alignment. [INFERRED] Source placement.
#include "Bf2Style.h"
#if !DEDICATED
#include "../External/BFMeme/Data.h"
#include "../External/BFMeme/Effect.h"
#include "../External/BFMeme/Node.h"
#include "../External/BFMeme/System.h"
#include "../External/BFMeme/TypeDescriptor.h"
#include <cmath>
#include <new>
namespace dice { namespace meme {
namespace {
// the parchment tint every body installs while
// the section-sign toggle is on.
const float highlightRed = 0.82744142f;
const float highlightGreen = 0.81960791f;
const float highlightBlue = 0.70196080f;
const int sectionSign = 0xA7;

// Every body reads the effect's colour field at +4 directly instead of
// calling getColor(); an effect that is not colour-shaped yields whatever sits
// at that offset. Keep the raw read rather than substituting the virtual.
// +4 is the first word after the vtable pointer (ColorEffect's colour); the
// same word sits at +8 on x64, so the offset is the vtable pointer's size.
Color readEffectColorField(Effect* effect)
{
    return *reinterpret_cast<const Color*>(
        reinterpret_cast<const char*>(effect) + sizeof(void*));
}
// style unsigned conversion: the four escape digits are widened as
// unsigned, so a character below '0' becomes a very large float. The values are
// then stored into a local array none of the three bodies ever reads again.
float escapeDigit(int value)
{ return static_cast<float>(static_cast<unsigned int>(value)); }
}

Bf2Style::Bf2Style(const char* name, FontHandle font)
    : Style(name, font), m_wordWrap(true), m_shadowFont(0), m_texture(0),
      m_shadowOffsetX(0.0f), m_shadowOffsetY(0.0f)
{
    m_padding19[0] = m_padding19[1] = m_padding19[2] = 0;
}
Bf2Style::~Bf2Style() { setName(nullptr); }                  // 7C15B0, then ~Style

TypeDescriptor* Bf2Style::getClassType()
{
    // Complete eight-slot metadata935E90; independent counter A22184.
    static ClassTypeDescriptor<Bf2Style, Style> type("dice::meme::Bf2Style");
    return &type;
}
TypeDescriptor* Bf2Style::getType() { return getClassType(); }
unsigned int Bf2Style::getObjectSize() { return sizeof(Bf2Style); }
namespace {
TypeDescriptor* const registeredBf2StyleType = Bf2Style::getClassType();
}

bool Bf2Style::setShadowFont(FontHandle font)
{
    if (!font) return false;
    m_shadowFont = font;
    return true;
}
bool Bf2Style::setShadowOffset(float x, float y)
{ m_shadowOffsetX = x; m_shadowOffsetY = y; return true; }
bool Bf2Style::setTexture(TextureHandle texture)
{ m_texture = texture; return true; }

// The shadow pass then the glyph pass, exactly as all three bodies inline them.
// Returns false when either draw refuses, which makes the caller stop at once.
bool Bf2Style::paintGlyph(System* system, int character, int position,
                         Quad& glyphQuad, RenderContext* context, bool highlight)
{
    if (m_texture) glyphQuad.texture2 = m_texture;
    if (m_shadowFont || m_shadowOffsetX != 0.0f || m_shadowOffsetY != 0.0f) {
        Effect* const effect = context->effect;
        Color saved = {1.0f, 1.0f, 1.0f, 1.0f};
        if (effect) saved = readEffectColorField(effect);
        RenderContext shadowContext = *context; // a full seven-word copy
        Quad shadowQuad(glyphQuad);
        if (shadowContext.effect)
            shadowContext.effect->setColor(0.0f, 0.0f, 0.0f, 1.0f);
        const Color black = {0.0f, 0.0f, 0.0f, 1.0f};
        colorQuad(&shadowQuad, &black);
        translateQuad(&shadowQuad, m_shadowOffsetX, m_shadowOffsetY, 0.0f);
        const FontHandle font = m_shadowFont ? m_shadowFont : m_font;
        if (!drawContextCharacter(&shadowContext, system, character, font,
                                  position, &shadowQuad))
            return false;
        // The restore is gated on the effect the pass started with, then reads
        // the context's current one again.
        if (effect && context->effect)
            context->effect->setColor(saved.r, saved.g, saved.b, saved.a);
    }
    Effect* const effect = context->effect;
    Color saved = {1.0f, 1.0f, 1.0f, 1.0f};
    if (highlight && effect) {
        saved = readEffectColorField(effect);
        effect->setColor(highlightRed, highlightGreen, highlightBlue, 1.0f);
    }
    if (!drawContextCharacter(context, system, character, m_font, position, &glyphQuad))
        return false;
    if (highlight && effect && context->effect)
        context->effect->setColor(saved.r, saved.g, saved.b, saved.a);
    return true;
}

// Left-aligned layout: one forward pass with optional word wrap, a
// carriage-return/newline/section-sign dispatch and a per-glyph advance.
void Bf2Style::drawText(System* system, Data* data, int elementIndex,
                       const Rect* bounds, const Rect* clip, RenderContext* context)
{
    float penX = bounds->x;
    const float right = bounds->x + bounds->width;
    float penY = bounds->y;
    const float bottom = bounds->y + bounds->height;
    const float lineHeight = system->getFontHeight(m_font);
    Rect glyphRect = {0.0f, 0.0f, 0.0f, 0.0f, bounds->z};
    float escapeChannels[4] = {0.0f, 0.0f, 0.0f, 0.0f}; // written, never read
    int position = 0;
    bool highlight = false;
    prepareFont();
    int character = data->getCharacterElement(system, elementIndex, position);
    if (!character) return;
    for (;;) {
        bool drawThisCharacter = false;
        bool advance = true; // the wrap arm is the only path that does not step
        if (m_wordWrap && character == ' ') {
            // Measure the next word; wrap before it if it would cross the edge.
            const int probeStart = position + 1;
            int probe = probeStart;
            float probeX = penX;
            bool wrapped = false;
            int probeChar = data->getCharacterElement(system, elementIndex, probe);
            while (probeChar) {
                if (probeChar == sectionSign) {
                    const int code = data->getCharacterElement(system, elementIndex, probe + 1);
                    if (code == 'C' || code == 'c') {
                        probe += 6;
                        probeChar = data->getCharacterElement(system, elementIndex, probe);
                        continue;
                    }
                }
                GlyphMetrics probeMetrics;
                system->getGlyphMetrics(probeChar, m_font, &probeMetrics);
                probeX += probeMetrics.leftBearing + probeMetrics.rightBearing
                        + probeMetrics.width;
                if (!(probeX < right)) {
                    penY += lineHeight;
                    penX = bounds->x;
                    if (!(penY < bottom)) return;
                    position = probeStart;
                    wrapped = true;
                    break;
                }
                if (probeChar == '\n' || probeChar == '\r' || probeChar == ' ') break;
                probe += 1;
                probeChar = data->getCharacterElement(system, elementIndex, probe);
            }
            if (wrapped) advance = false;
            else drawThisCharacter = true;
        } else if (character == '\r') {
            penX = bounds->x;
        } else if (character == '\n') {
            penY += lineHeight;
            if (!(penY < bottom)) return;
        } else if (character == sectionSign) {
            position += 1;
            const int code = data->getCharacterElement(system, elementIndex, position);
            if (code == 'C' || code == 'c') {
                highlight = !highlight;
                for (int i = 0; i != 4; ++i) {
                    position += 1;
                    escapeChannels[i] = escapeDigit(
                        data->getCharacterElement(system, elementIndex, position) - '0');
                }
            } else if (code == 'L' || code == 'l') {
                penX = bounds->x;
                penY += lineHeight;
                if (!(penY < bottom)) return;
            }
        } else {
            drawThisCharacter = true;
        }

        if (drawThisCharacter) {
            GlyphMetrics metrics;
            system->getGlyphMetrics(character, m_font, &metrics);
            penX += metrics.leftBearing;
            glyphRect.height = metrics.height;
            glyphRect.width = metrics.width;
            positionGlyph(&glyphRect, &metrics, &penX, &penY);
            Quad glyphQuad(&glyphRect, clip);
            if (!paintGlyph(system, character, position, glyphQuad, context, highlight))
                return;
            penX += metrics.rightBearing + metrics.width;
            if (!(penX < right)) {
                penX = bounds->x;
                penY += lineHeight;
                if (!(penY < bottom)) return;
            }
        }
        if (advance) position += 1;
        character = data->getCharacterElement(system, elementIndex, position);
        if (!character) return;
    }
}

// right aligned
Bf2RightAlignedStyle::Bf2RightAlignedStyle(const char* name, FontHandle font)
    : Bf2Style(name, font) {}
Bf2RightAlignedStyle::~Bf2RightAlignedStyle() {}              // 7C26A0, shared body
TypeDescriptor* Bf2RightAlignedStyle::getClassType()
{
    static ClassTypeDescriptor<Bf2RightAlignedStyle, Bf2Style>
        type("dice::meme::Bf2RightAlignedStyle");
    return &type;
}
TypeDescriptor* Bf2RightAlignedStyle::getType() { return getClassType(); }
namespace {
TypeDescriptor* const registeredBf2RightAlignedStyleType =
    Bf2RightAlignedStyle::getClassType();
}

// The string is walked backwards from its last character and the pen
// moves left, so the text ends flush with the right edge. There is no word
// wrap; a line breaks only when the pen crosses the left edge. The escape
// parser still reads forward, so a section-sign escape consumes characters in
// the opposite direction from the glyph walk. That is the shipped body.
void Bf2RightAlignedStyle::drawText(System* system, Data* data, int elementIndex,
                                   const Rect* bounds, const Rect* clip,
                                   RenderContext* context)
{
    const float right = bounds->x + bounds->width;
    float penX = right;
    float penY = bounds->y;
    const float bottom = bounds->y + bounds->height;
    const float left = bounds->x;
    const float lineHeight = system->getFontHeight(m_font);
    const int length = data->getStringLength(system);
    Rect glyphRect = {0.0f, 0.0f, 0.0f, 0.0f, bounds->z};
    float escapeChannels[4] = {0.0f, 0.0f, 0.0f, 0.0f}; // written, never read
    bool highlight = false;
    prepareFont();
    int index = length - 1;
    if (index < 0) return;
    do {
        const int character = data->getCharacterElement(system, elementIndex, index);
        if (character == '\r') {
            penX = right;
        } else if (character == '\n') {
            penY += lineHeight;
            if (!(penY < bottom)) return;
        } else if (character == sectionSign) {
            index += 1; // the escape payload is read forward
            const int code = data->getCharacterElement(system, elementIndex, index);
            if (code == 'C' || code == 'c') {
                highlight = !highlight;
                for (int i = 0; i != 4; ++i) {
                    index += 1;
                    escapeChannels[i] = escapeDigit(
                        data->getCharacterElement(system, elementIndex, index) - '0');
                }
            } else if (code == 'L' || code == 'l') {
                penX = right;
                penY += lineHeight;
                if (!(penY < bottom)) return;
            }
        } else {
            GlyphMetrics metrics;
            system->getGlyphMetrics(character, m_font, &metrics);
            glyphRect.height = metrics.height;
            glyphRect.width = metrics.width;
            penX -= metrics.rightBearing + metrics.width;
            positionGlyph(&glyphRect, &metrics, &penX, &penY);
            Quad glyphQuad(&glyphRect, clip);
            if (!paintGlyph(system, character, index, glyphQuad, context, highlight))
                return;
            penX -= metrics.leftBearing;
            // The edge test truncates the pen to an integer first.
            if (static_cast<float>(static_cast<int>(penX)) < left) {
                penX = right;
                penY += lineHeight;
                if (!(penY < bottom)) return;
            }
        }
        index -= 1;
    } while (index >= 0);
}

// centre aligned
Bf2CenterAlignedStyle::Bf2CenterAlignedStyle(const char* name, FontHandle font)
    : Bf2Style(name, font) {}
Bf2CenterAlignedStyle::~Bf2CenterAlignedStyle() {}            // 7C26A0, shared body
TypeDescriptor* Bf2CenterAlignedStyle::getClassType()
{
    static ClassTypeDescriptor<Bf2CenterAlignedStyle, Bf2Style>
        type("dice::meme::Bf2CenterAlignedStyle");
    return &type;
}
TypeDescriptor* Bf2CenterAlignedStyle::getType() { return getClassType(); }
namespace {
TypeDescriptor* const registeredBf2CenterAlignedStyleType =
    Bf2CenterAlignedStyle::getClassType();
}

// Two passes per line: measure its width, then draw it starting at the
// ceil of the centred origin. Three shipped quirks: the section-sign toggle
// is applied during the measure pass only, so it colours the whole following
// line; the draw pass has no escape handling at all and paints the section sign
// and its payload as ordinary glyphs; and nothing tests the pen against the
// bottom of the bounds, so the text is not clipped vertically.
void Bf2CenterAlignedStyle::drawText(System* system, Data* data, int elementIndex,
                                    const Rect* bounds, const Rect* clip,
                                    RenderContext* context)
{
    const float lineHeight = system->getFontHeight(m_font);
    float penY = bounds->y;
    Rect glyphRect = {0.0f, 0.0f, 0.0f, 0.0f, bounds->z};
    float escapeChannels[4] = {0.0f, 0.0f, 0.0f, 0.0f}; // written, never read
    bool highlight = false;
    prepareFont();
    float lineWidth = 0.0f;
    int lineStart = 0;
    int scan = 0;
    for (;;) {
        int character = 0;
        for (;;) {
            character = data->getCharacterElement(system, elementIndex, scan);
            if (!character || character == '\n') break;
            if (character == '\r') { scan += 1; continue; }
            if (character == sectionSign) {
                scan += 1;
                const int code = data->getCharacterElement(system, elementIndex, scan);
                if (code == 'C' || code == 'c') {
                    highlight = !highlight;
                    for (int i = 0; i != 4; ++i) {
                        scan += 1;
                        escapeChannels[i] = escapeDigit(
                            data->getCharacterElement(system, elementIndex, scan) - '0');
                    }
                }
                scan += 1;
                continue;
            }
            GlyphMetrics metrics;
            system->getGlyphMetrics(character, m_font, &metrics);
            scan += 1;
            lineWidth += metrics.rightBearing + metrics.width + metrics.leftBearing;
        }
        float penX = std::ceil((bounds->width - lineWidth) * 0.5f + bounds->x);
        for (int index = lineStart; index < scan; ++index) {
            const int glyph = data->getCharacterElement(system, elementIndex, index);
            if (glyph == '\r' || glyph == '\n') continue;
            GlyphMetrics metrics;
            system->getGlyphMetrics(glyph, m_font, &metrics);
            penX += metrics.leftBearing;
            glyphRect.height = metrics.height;
            glyphRect.width = metrics.width;
            positionGlyph(&glyphRect, &metrics, &penX, &penY);
            Quad glyphQuad(&glyphRect, clip);
            if (!paintGlyph(system, glyph, index, glyphQuad, context, highlight))
                return;
            penX += metrics.rightBearing + metrics.width;
        }
        if (!character) return;
        penY += lineHeight;
        lineStart = scan;
        lineWidth = 0.0f;
        scan += 1; // step over the newline that ended the line
    }
}

#if defined(_M_IX86)
static_assert(sizeof(Bf2Style) == 44, "native Bf2Style storage");
#endif
static_assert(sizeof(Bf2RightAlignedStyle) == sizeof(Bf2Style), "right-aligned adds no storage");
static_assert(sizeof(Bf2CenterAlignedStyle) == sizeof(Bf2Style), "centre-aligned adds no storage");
#if defined(_M_IX86)
static_assert(sizeof(ClassTypeDescriptor<Bf2Style, Style>) == 16, "Bf2Style metadata");
#endif
} }
#endif
