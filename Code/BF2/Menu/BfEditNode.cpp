// Editable HUD text, focus, cursor movement and drawing. Source placement [INFERRED].
#include "BfEditNode.h"
#if !DEDICATED
#include "BfEditData.h"
#include "Font.h"
#include "DifFontMetadata.h"
#include "../External/BFMeme/Action.h"
#include "../External/BFMeme/Effect.h"
#include "../External/BFMeme/Pointer.h"
#include "../External/BFMeme/Stream.h"
#include "../External/BFMeme/System.h"
#include "../External/BFMeme/TypeDescriptor.h"
#include <cmath>
#include <cstring>
#include <new>
namespace dice { namespace meme {
BfEditNode::BfEditNode()
    : m_editData(nullptr), m_string(nullptr), m_selectAction(nullptr), m_font(0),
      m_maxCharacters(-1), m_firstVisible(0), m_cursor(0), m_blinkTime(-0.4f),
      m_focused(false), m_pendingInput(0), m_ignoreClick(false), m_flag39(false),
      m_slowBlink(false), m_skipCharacter(false), m_limitWidth(false), m_outline(false),
      m_color{0.0f, 0.0f, 1.0f, 1.0f} {}
unsigned int BfEditNode::getObjectSize() { return sizeof(BfEditNode); }
bool BfEditNode::streamProperties(System* system, Stream* stream)
{
    if (!Node::streamProperties(system, stream)) return false;
    if (!stream->streamFont(system, &m_font, "Font")) return false;
    if (!stream->streamData(system, m_string, "String", nullptr)) return false;
    if (!stream->streamInt(&m_maxCharacters, "Max characters(-1 = no limit)")) return false;
    if (!stream->streamAction(system, m_selectAction, "Select action", nullptr)) return false;
    // Stream the pointer itself, retaining modifications made before a failure.
    if (!stream->streamData(system, reinterpret_cast<Data*&>(m_editData),
                            "Editbox data", BfEditData::getClassType())) return false;
    if (m_editData) {
        m_editData->m_activeNode = this;
        m_editData->registerNode(this); // reload the live member; duplicates retained
    }
    return stream->streamBool(&m_focused, "Focus") != false;
}
bool BfEditNode::updateSelf(System* system, const NodeState* state, const Rect* rect,
                          float delta)
{
    if (!Node::updateSelf(system, state, rect, delta)) return false;
    // Original stores the float but compares the still-live x87 sum. Double
    // holds the exact sum of these two float operands for this comparison.
    const double time = static_cast<double>(delta) + m_blinkTime;
    m_blinkTime = static_cast<float>(time);
    if (!m_slowBlink) {
        if (time >= static_cast<double>(0.4f)) m_blinkTime = -0.4f;
    } else if (time >= static_cast<double>(0.8f)) m_blinkTime = -0.8f;
    if (m_pendingInput == 1) {
        onEvent1(system, rect, state, m_pendingKey, 1, 1);
        m_pendingInput = 0; // callback may mutate it; the shipped clear follows
        return true;
    }
    if (m_pendingInput == 2) {
        onEvent4(system, rect, state, m_pendingCharacter, 1, 1);
        m_pendingInput = 0;
    }
    return true;
}
void BfEditNode::updateVisibleStart(System* system, const Rect* rect)
{
    if (!m_string) return;
    if (m_cursor <= m_firstVisible) {
        m_firstVisible = m_cursor;
        return;
    }
    float remaining = rect->width;
    for (int index = m_cursor - 1; index >= m_firstVisible; --index) {
        GlyphMetrics metrics;
        const int font = m_font;
        const int character = m_string->getCharacter(system, index);
        system->getGlyphMetrics(character, font, &metrics); // result deliberately discarded
        // Right + width + left, preserving the actual evaluation order.
        const double width = (static_cast<double>(metrics.rightBearing) + metrics.width) + metrics.leftBearing;
        const double next = static_cast<double>(remaining) - width;
        remaining = static_cast<float>(next);
        if (next <= 0.0) {
            m_firstVisible = index + 1;
            return;
        }
    }
}
bool BfEditNode::onEvent4(System* system, const Rect* rect, const NodeState*,
                         int character, int, int) // 7AB9B0; final two integers unused
{
    if (!m_focused || !m_string || character == 13 || character == 27 ||
        character > 256 || character == 22) return false;
    if (m_slowBlink && m_skipCharacter) {
        m_skipCharacter = false;
        return false;
    }
    m_skipCharacter = !m_skipCharacter;
    const int length = m_string->getStringLength(system);
    if (m_maxCharacters > 0 && length + 1 > m_maxCharacters) return false;
    const int capacity = length + 2;
    char* buffer = static_cast<char*>(::operator new(capacity)); // 447EF0; no null guard
    m_string->getString(system, buffer, capacity);
    std::memmove(buffer + m_cursor + 1, buffer + m_cursor, length - m_cursor + 1);
    buffer[m_cursor] = static_cast<char>(character); // negative and 256 accepted; low byte retained
    if (m_limitWidth) {
        float remaining = rect->width;
        for (int index = 0; index < length + 1; ++index) {
            GlyphMetrics metrics;
            system->getGlyphMetrics(static_cast<signed char>(buffer[index]), m_font, &metrics);
            const double width = (static_cast<double>(metrics.rightBearing) + metrics.width) + metrics.leftBearing;
            const double next = static_cast<double>(remaining) - width;
            remaining = static_cast<float>(next);
            if (next <= 0.0) return false; // Shipped buffer leak at7ABAB1 ->7ABAFE; preserve it.
        }
    }
    m_string->setString(system, buffer); // callback result is ignored; member reloaded below
    ::operator delete(buffer);
    buffer = nullptr;
    if (length != m_string->getStringLength(system)) {
        ++m_cursor;
        updateVisibleStart(system, rect);
    }
    return true;
}
TypeDescriptor* BfEditNode::getClassType()                 // 7AC340, singletonA20EFC
{
    static ClassTypeDescriptor<BfEditNode, Node> type("dice::meme::BfEditNode");
    return &type;
}
TypeDescriptor* BfEditNode::getType() { return getClassType(); }

void BfEditNode::paintSelf(System* system, const NodeState*, const Rect* bounds,
                         const Rect*, RenderContext* context)
{
    if (!m_string) return;
    float x = bounds->x;
    const float y = bounds->y;
    const float endX = x + bounds->width;
    const float fontHeight = system->getFontHeight(m_font);
    Rect glyph = *bounds;
    int stringIndex = m_firstVisible;
    int elementIndex = 0;
    bool drewCursor = false;
    float scale = 1.0f;
    hfe::menu::Font* const font = reinterpret_cast<hfe::menu::Font*>(m_font);
    const std::string fontName(font->getName());
    if (fontName.find(".dif", 0, 4) != std::string::npos)
        scale = static_cast<hfe::menu::DifFont*>(font)->getMetadata()->getScale();
    const float reciprocalScale = static_cast<float>(1.0 / scale);
    int character = m_string->getCharacter(system, stringIndex);
    while (character) {
        if (character == 13) {
            if (!drewCursor) stringIndex = m_cursor;
            break;
        }
        GlyphMetrics metrics;
        system->getGlyphMetrics(character, m_font, &metrics);
        glyph.width = metrics.width;
        glyph.height = metrics.height;
        if (scale == 1.0f) {
            glyph.x = x;
            glyph.y = metrics.verticalOffset + y;
        } else {
            x = scale * x;
            const float scaledY = scale * y;
            const double roundedX = std::floor(!(std::fmod(x, 1.0) >= 0.5) ?
                                               static_cast<double>(x) : x + 1.0);
            x = static_cast<float>(roundedX * reciprocalScale);
            glyph.x = x;
            const double roundedY = std::floor(!(std::fmod(scaledY, 1.0) >= 0.5) ?
                                               static_cast<double>(scaledY) : scaledY + 1.0);
            glyph.y = static_cast<float>((roundedY + metrics.verticalOffset) * reciprocalScale);
        }
        if (x >= endX) break;
        if (!m_editData || !m_editData->m_hideText) {
            if (m_outline) {
                const Color outlineColor{0.15f, 0.15f, 0.15f, 1.0f};
                Quad outline(&glyph, bounds, &outlineColor);
                translateQuad(&outline, 1.0f, 1.0f, -0.001f);
                drawContextCharacter(context, system, character, m_font, elementIndex, &outline);
                const float offsets[7][2] = {
                    {1.0f, 0.0f}, {1.0f, -1.0f}, {0.0f, -1.0f},
                    {-1.0f, -1.0f}, {-1.0f, 0.0f}, {-1.0f, 1.0f}, {0.0f, 1.0f}
                };
                for (const auto& offset : offsets) {
                    setQuadBounds(&outline, x + offset[0], glyph.y + offset[1],
                                  glyph.width, glyph.height);
                    drawContextCharacter(context, system, character, m_font, elementIndex, &outline);
                }
            }
            Quad foreground(&glyph, bounds, &m_color);
            if (!drawContextCharacter(context, system, character, m_font, elementIndex, &foreground))
                return;
        }
        if (stringIndex == m_cursor && m_blinkTime >= 0.0f && m_focused) {
            glyph.height = fontHeight;
            glyph.y = y;
            glyph.width = 2.0f;
            Quad cursor(&glyph, nullptr);
            drawContextTexture(context, system, 0, &cursor);
            drewCursor = true;
        }
        ++elementIndex;
        ++stringIndex;
        const double advance = (static_cast<double>(metrics.rightBearing) + metrics.width) + metrics.leftBearing;
        x = static_cast<float>(advance + x);
        character = m_string->getCharacter(system, stringIndex);
    }
    if (stringIndex == m_cursor && m_blinkTime >= 0.0f && m_focused) {
        glyph.x = x;
        glyph.y = y;
        glyph.height = fontHeight;
        glyph.width = 2.0f;
        Quad cursor(&glyph, nullptr);
        drawContextTexture(context, system, 0, &cursor);
    }
}
bool BfEditNode::onEvent1(System* system, const Rect* rect, const NodeState* state,
                         int first, int index, int second)
{
    if (index != 1) return false;
    if (first == 18) {
        if (m_selectAction) {
            const NodeInputEvent event{1, 0, 0.0f, 0.0f, 0};
            m_selectAction->execute(system, rect, state, &event);
            if (!m_focused) return false;
            m_focused = false;
            system->unlockPointer();
        }
    } else if (first == 1 && !m_ignoreClick) {
        const float* position = system->getPointerPosition();
        const float right = rect->x + rect->width;
        const float bottom = rect->y + rect->height;
        const bool inside = position[0] > rect->x && position[0] < right &&
                            position[1] > rect->y && position[1] < bottom;
        if (!inside) {
            m_focused = false;
            system->unlockPointer(); // also decrements when the node was already unfocused
            pointerEvent1(system, static_cast<Pointer*>(system->getPointerObject()), 1, 1, second);
            return false;
        }
        if (!m_focused) {
            m_focused = true;
            system->lockPointer();
        }
    }
    if (!m_focused || !m_string) return false;
    const int length = m_string->getStringLength(system);
    switch (first) {
    case 1: {
        if (m_ignoreClick) {
            m_ignoreClick = false;
            break;
        }
        float distance = rect->x - system->getPointerPosition()[0];
        if (distance < 0.0f) distance *= -1.0f;
        float remaining = rect->width;
        int characterIndex = 0;
        for (; characterIndex < length; ++characterIndex) {
            GlyphMetrics metrics;
            const int font = m_font;
            const int character = m_string->getCharacter(system, characterIndex);
            system->getGlyphMetrics(character, font, &metrics);
            const double width = (static_cast<double>(metrics.rightBearing) + metrics.width) + metrics.leftBearing;
            const double next = static_cast<double>(remaining) - width;
            remaining = static_cast<float>(next);
            if (next <= static_cast<double>(rect->width) - distance) {
                m_cursor = characterIndex;
                updateVisibleStart(system, rect);
                break;
            }
        }
        if (characterIndex == length) {
            m_cursor = characterIndex;
            updateVisibleStart(system, rect);
        }
        break;
    }
    case 5:
        if (m_cursor > 0) --m_cursor;
        break;
    case 7:
        if (m_cursor < length) ++m_cursor;
        break;
    case 9:
        m_cursor = 0;
        updateVisibleStart(system, rect);
        break;
        m_cursor = length;
        updateVisibleStart(system, rect);
        break;
    case 14:
        if (m_cursor < length) {
            char* buffer = static_cast<char*>(::operator new(length + 1));
            m_string->getString(system, buffer, length + 1);
            std::memmove(buffer + m_cursor, buffer + m_cursor + 1, length - m_cursor);
            m_string->setString(system, buffer);
            ::operator delete(buffer);
        }
        break;
    case 15:
        if (m_cursor > 0) {
            char* buffer = static_cast<char*>(::operator new(length + 1));
            m_string->getString(system, buffer, length + 1);
            std::memmove(buffer + m_cursor - 1, buffer + m_cursor, length - m_cursor + 1);
            m_string->setString(system, buffer);
            ::operator delete(buffer);
            --m_cursor;
        }
        break;
    case 16:
        return true;
    case 17:
        if (m_ignoreClick) m_ignoreClick = false;
        break;
    default:
        break;
    }
    updateVisibleStart(system, rect);
    return true;
}
static_assert(sizeof(BfEditNode) == 80, "native BfEditNode storage");
} }
#endif
