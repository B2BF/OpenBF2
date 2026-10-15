// [INFERRED] path. Primary939A70, metadata939B48, constructor7D3AC0,
// destructor7D3B10, factory7D3B50, streamProperties7D3A20 and paintSelf7D39B0
// were read in full. The class adds no virtual slot to Node.
#include "TextNode.h"
#if !DEDICATED
#include "ColorEffect.h"
#include "Data.h"
#include "Style.h"
#include "Stream.h"
#include "System.h"
#include "TypeDescriptor.h"
#include <new>
namespace dice { namespace meme {

BfTextNode::BfTextNode()
    : m_style(nullptr), m_string(nullptr), m_type(nullptr), m_shadow(nullptr)
{
    // The shadow effect is the node's own: opaque black, allocated eagerly and
    // released by the destructor through its virtual teardown.
    void* allocation = ::operator new(sizeof(ColorEffect));
    m_shadow = allocation
        ? new(allocation) ColorEffect(0.0f, 0.0f, 0.0f, 1.0f) : nullptr;
}
BfTextNode::~BfTextNode()
{
    // Retail invokes the effect's deleting destructor (slot 0 with flag 1).
    delete m_shadow;
}
TypeDescriptor* BfTextNode::getClassType()
{
    // Complete eight-slot metadata939B48; independent counter.
    static ClassTypeDescriptor<BfTextNode, Node> type("dice::meme::BfTextNode");
    return &type;
}
TypeDescriptor* BfTextNode::getType() { return getClassType(); }
unsigned int BfTextNode::getObjectSize() { return sizeof(BfTextNode); }
namespace {
TypeDescriptor* const registeredBfTextNodeType = BfTextNode::getClassType();
}

bool BfTextNode::streamProperties(System* system, Stream* stream)
{
    if (!Node::streamProperties(system, stream)) return false;
    if (!stream->streamData(system, m_string, "String", nullptr)) return false;
    if (!stream->streamStyle(system, m_style, "Style", nullptr)) return false;
    return stream->streamData(system, m_type, "Type", nullptr) != false;
}

// The node paints nothing at all without both a string and a type, and
// the type's float value gates it: only a truncated zero draws. The bounds
// rectangle is mutated through its const pointer - retail shifts the baseline
// down by one before handing it to the style. Keep that.
void BfTextNode::paintSelf(System* system, const NodeState* state,
                          const Rect* bounds, const Rect* clip,
                          RenderContext* context)
{
    if (!m_string) return;
    if (!m_type) return;
    if (static_cast<int>(m_type->getFloat(system)) != 0) return;
    Rect* const writable = const_cast<Rect*>(bounds);
    writable->y += 1.0f;
    drawStyledText(system, m_style, writable, clip, context, m_string, 0);
}

// 838460, __fastcall with the System and Style in registers. Name [INFERRED].
void drawStyledText(System* system, Style* style, const Rect* bounds,
                    const Rect* clip, RenderContext* context,
                    Data* data, int elementIndex)
{
    if (!data) return;
    // A node with no style of its own falls back to the System's default.
    if (!style) style = static_cast<Style*>(system->getDefaultStyleObject());
    style->drawText(system, data, elementIndex, bounds, clip, context);
}

#if defined(_M_IX86)
static_assert(sizeof(BfTextNode) == 24, "native BfTextNode storage");
static_assert(sizeof(ClassTypeDescriptor<BfTextNode, Node>) == 16, "BfTextNode metadata");
#endif
} }
#endif
