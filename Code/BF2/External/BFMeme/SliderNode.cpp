// dice::meme::SliderNode, read in full from BF2.exe 0x833660..0x833DD2 (BFMeme library slider; the
// HUD builds Bf2FixedSliderNode). x87 intermediates are carried in double and stored as float
// where the shipped code stores them; the owner dropped the bit-exact goal (2026-11-05).
#include "SliderNode.h"
#if !DEDICATED
#include "TypeDescriptor.h"
#include "Style.h"   // dice::meme::Rect is defined here
#include "Data.h"
#include "SliderNodeMath.h"
#include "NodeState.h"
#include "Stream.h"
#include "System.h"
namespace dice { namespace meme {

// Node() first, then nine own words in the shipped order: the vptr
// store sits between the zeroed group and the two 1.0 pairs, which is why the
// member initializers below are not in declaration order in the binary.
SliderNode::SliderNode()
    : Node(), m_grabOffset(0.0f), m_thumbOffset(0.0f), m_minimum(0.0f),
      m_maximum(1.0f), m_thumbSpan(1.0f), m_data(nullptr), m_child1(nullptr)
{
    m_state.target = 1.0f;   // +24
    m_state.current = 1.0f;  // +28
}

TypeDescriptor* SliderNode::getClassType()                // 833AB0, singletonA39E34
{
    static ClassTypeDescriptor<SliderNode, Node> type("dice::meme::SliderNode");
    return &type;
}
TypeDescriptor* SliderNode::getType() { return getClassType(); }
unsigned int SliderNode::getObjectSize() { return sizeof(SliderNode); }
int SliderNode::getChildCount() { return 2; }             // 828A70, shared with SplitNode

// not an array and not a range check: index 1 is the thumb and
// everything else - zero, negative, out of range - is Node's own +04 slot.
Node*& SliderNode::getChild(int index)
{
    if (index == 1) return m_child1;
    return m_next;
}

// The setter's fallback differs from the getter's: only 0 and 1 are
// accepted here, and any other index writes nothing and still returns.
void SliderNode::setChild(int index, Node* child)
{
    if (index == 0) { m_next = child; return; }
    if (index == 1) m_child1 = child;
}

// The thumb is visited first, through the visitor's +08 hook, with a
// rect shifted along x by the thumb offset and narrowed to the thumb's share of
// the range; the tracked child then gets the caller's own bounds and state
// through the +04 hook.
// The span is the ordered difference, chosen by a single `fcomp`: only a
// maximum strictly below the minimum takes the reversed subtraction, so an
// unordered (NaN) compare falls with the normal case exactly as `<` does here.
void SliderNode::visitChildren(System* system, NodeState* state, const Rect* bounds,
                               NodeVisitor* visitor)
{
    Rect thumb;
    thumb.x = bounds->x + m_thumbOffset;
    thumb.y = bounds->y;
    thumb.width = bounds->width;
    thumb.height = bounds->height;
    thumb.z = bounds->z;
    const float span = m_maximum < m_minimum ? m_minimum - m_maximum : m_maximum - m_minimum;
    thumb.width = m_thumbSpan / span * thumb.width;
    visitor->traverse0x08(system, m_child1, &thumb, &m_state);
    visitor->visit(system, m_next, bounds, state);
}


using namespace slider;

// 833700: the thumb's width on a track of the given width.
float SliderNode::thumbExtent(const float* width) const
{
    return static_cast<float>(m_thumbSpan / orderedSpan(m_minimum, m_maximum) * *width);
}

// 833740: places the thumb from the bound value; nothing without a Data. The value
// scale uses the plain max - min (not the ordered span) as shipped.
void SliderNode::syncFromData(System* system, const float* width)
{
    if (!m_data) return;
    const double thumb = m_thumbSpan / orderedSpan(m_minimum, m_maximum) * *width;
    const float scale = static_cast<float>((*width - thumb) /
        (static_cast<double>(m_maximum) - m_minimum));
    m_thumbOffset = static_cast<float>(
        (static_cast<double>(m_data->getFloat(system)) - m_minimum) * scale);
}

// 833660: Node's properties, then the cursor (thumb) node, the bound Data and the three
// range floats, each stream answer gating the next.
bool SliderNode::streamProperties(System* system, Stream* stream)
{
    if (!Node::streamProperties(system, stream)) return false;
    if (!stream->streamNode0x84(system, m_child1, "Cursor node", nullptr)) return false;
    if (!stream->streamData(system, m_data, "Data", nullptr)) return false;
    if (!stream->streamFloat(&m_minimum, "Minimum value")) return false;
    if (!stream->streamFloat(&m_maximum, "Maximum value")) return false;
    return stream->streamFloat(&m_thumbSpan, "Number visible");
}

// 833B80: the thumb rectangle, painted with the slider's own state; under an effect it
// gets a child context (the parent, no effect, the decoded state and its rate) as a
// TransformNode builds one. Then Node's own paint.
void SliderNode::paint(System* system, const NodeState* state, const Rect* bounds,
                       const Rect* clip, RenderContext* context)
{
    if (m_child1) {
        Rect thumb = *bounds;
        thumb.x = bounds->x + m_thumbOffset;
        thumb.width = static_cast<float>(m_thumbSpan / orderedSpan(m_minimum, m_maximum) * thumb.width);
        if (context->effect) {
            RenderContext child = {context, nullptr, context->field08, context->field0c,
                                   context->field10, decodedState(m_state.current), 1.0f};
            child.field10 = stateRate(m_state);
            m_child1->paint(system, &m_state, &thumb, clip, &child);
        } else {
            m_child1->paint(system, &m_state, &thumb, clip, context);
        }
    }
    Node::paint(system, state, bounds, clip, context);
}

// 833C90: the slider's state follows the pointer over the thumb (only while the parent
// is pressed, edges included, NaN inside) and the parent's decoded state; then the thumb
// updates, and a refusal stops the walk before Node's own update.
bool SliderNode::update(System* system, const NodeState* state, const Rect* bounds, float delta)
{
    Rect thumb = *bounds;
    thumb.x = bounds->x + m_thumbOffset;
    thumb.width = static_cast<float>(m_thumbSpan / orderedSpan(m_minimum, m_maximum) * thumb.width);
    bool inside = false;
    if (state->target < 0.0f) inside = pointerInside(system->getPointerPosition(), thumb);
    const bool enabled = !(decodedState(state->target) < 1.0f);
    updateNodeState(&m_state, system, this, m_child1, &thumb, delta, inside, enabled);
    if (m_child1 && !m_child1->update(system, &m_state, &thumb, delta)) return false;
    return Node::update(system, state, bounds, delta);
}

// 833810: while a press is held the thumb follows the pointer (clamped into the track,
// less the grab offset, kept inside [0, width - thumb]) and writes the value back to the
// Data; otherwise it is placed from the Data. Always true.
bool SliderNode::updateSelf(System* system, const NodeState* state, const Rect* rect, float)
{
    if (!m_data) return true;
    if (!pressHeld(state, system)) {
        syncFromData(system, &rect->width);
        return true;
    }
    const float* pointer = system->getPointerPosition();
    float position[2] = {pointer[0], pointer[1]};
    clampToRect(position, rect);
    position[0] = position[0] - rect->x;
    const double thumb = m_thumbSpan / orderedSpan(m_minimum, m_maximum) * rect->width;
    const double offset = static_cast<double>(position[0]) - m_grabOffset;
    m_thumbOffset = static_cast<float>(offset);
    if (offset < 0.0) m_thumbOffset = 0.0f;
    const double limit = rect->width - thumb;
    if (!(m_thumbOffset < limit) && !(limit != limit) && !(m_thumbOffset != m_thumbOffset))
        m_thumbOffset = static_cast<float>(limit);
    const double value = m_thumbOffset /
        ((rect->width - thumb) / (static_cast<double>(m_maximum) - m_minimum)) + m_minimum;
    m_data->setFloat(system, static_cast<float>(value));
    return true;
}

// 833900: a primary press (first 1, second 1) that is not already held locks the pointer
// and records where on the thumb it landed. A press beside the thumb pages it by whole
// thumb widths towards the pointer (clamped at both track ends), then the slider updates
// once with a zero delta. A held or other press is refused.
bool SliderNode::onEvent1(System* system, const Rect* rect, const NodeState* state,
                          int first, int, int second)
{
    if (pressHeld(state, system)) return false;
    if (first != 1 || second != 1) return false;
    system->lockPointer();
    const float* pointer = system->getPointerPosition();
    const float local = static_cast<float>(static_cast<double>(pointer[0]) - rect->x);
    m_grabOffset = static_cast<float>(static_cast<double>(local) - m_thumbOffset);
    const float grab = m_grabOffset;
    const float page = thumbExtent(&rect->width);
    if (grab < 0.0f) {
        do {
            m_thumbOffset = static_cast<float>(static_cast<double>(m_thumbOffset) - page);
            m_grabOffset = static_cast<float>(static_cast<double>(page) + m_grabOffset);
            if (m_thumbOffset < 0.0f) {
                m_grabOffset = local;
                m_thumbOffset = 0.0f;
            }
        } while (m_grabOffset < 0.0f);
    }
    while (m_grabOffset > page) {
        m_thumbOffset = static_cast<float>(static_cast<double>(page) + m_thumbOffset);
        m_grabOffset = static_cast<float>(static_cast<double>(m_grabOffset) - page);
        if (static_cast<double>(page) + m_thumbOffset >= rect->width) {   // `test ah,1`: NaN skips
            m_thumbOffset = static_cast<float>(static_cast<double>(rect->width) - page);
            m_grabOffset = static_cast<float>(static_cast<double>(local) - m_thumbOffset);
        }
    }
    updateSelf(system, state, rect, 0.0f);
    return true;
}

// 833A30, both slots +78 and +7C: the release of the primary press unlocks the pointer and
// re-places the thumb from the Data. Never consumes the event.
bool SliderNode::onEvent2(System* system, const Rect* rect, const NodeState*, int first, int)
{
    if (first == 1) {
        system->unlockPointer();
        syncFromData(system, &rect->width);
    }
    return false;
}

bool SliderNode::onEvent3(System* system, const Rect* rect, const NodeState* state, int first, int index)
{
    return SliderNode::onEvent2(system, rect, state, first, index);
}

} }
#endif
