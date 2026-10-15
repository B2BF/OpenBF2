// [INFERRED] path/names. Bounded real BfEditNode storage/properties/update;
// no complete Node descriptor or edit-node factory is published.
#include "BfEditNode.h"
#if !DEDICATED
#include "BfEditData.h"
#include "../External/BFMeme/Stream.h"
#include "../External/BFMeme/System.h"
#include "../External/BFMeme/TypeDescriptor.h"
#include <cstdio>
#include <cstdlib>
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
        const FontHandle font = m_font;
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

// [FRONTIER] [TEMP] [CHOSEN, not recovered] Edit-node painting and pointer-event handling stop on invocation.
// [RETIRE-WHEN: hud-edit-node-frame] Replace with complete, validated frame and pointer-event implementations.
void BfEditNode::paintSelf(System*, const NodeState*, const Rect*, const Rect*, RenderContext*)
{
    std::fprintf(stderr, "[FRONTIER] dice::meme::BfEditNode::paintSelf reached\n");
    std::fflush(stderr);
    std::abort();
}
bool BfEditNode::onEvent1(System*, const Rect*, const NodeState*, int, int, int)
{
    std::fprintf(stderr, "[FRONTIER] dice::meme::BfEditNode::onEvent1 reached\n");
    std::fflush(stderr);
    std::abort();
}
#if defined(_M_IX86)
static_assert(sizeof(BfEditNode) == 80, "native BfEditNode storage");
#endif
} }
#endif
