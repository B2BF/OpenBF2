// Fixed-slider node, read in full from BF2.exe 0x7D3D50..0x7D4B75 (the HUD's Bf2FixedSliderNode
// inherits every frame and event body from here).
#include "BfFixedSliderNode.h"
#if !DEDICATED
#include "../External/BFMeme/TypeDescriptor.h"
#include "../External/BFMeme/Style.h"   // dice::meme::Rect is defined here
#include "../External/BFMeme/Data.h"
#include "../External/BFMeme/NodeState.h"
#include "../External/BFMeme/SliderNodeMath.h"
#include "../External/BFMeme/Stream.h"
#include "../External/BFMeme/System.h"
#include "../IO/Input/Input.h"
namespace dice { namespace hfe { namespace io { extern IBase* inputDeviceManager; } } }   // Core/Globals.cpp
namespace dice { namespace meme {

// SliderNode() first - the base's own nine words are constructed and
// then never read again - followed by eleven own words. The vptr store sits
// between the zeroed group and the 1.0 group exactly as in the base.
BfFixedSliderNode::BfFixedSliderNode()
    : SliderNode(), m_grabOffset(0.0f), m_thumbOffset(0.0f), m_minimum(0.0f),
      m_maximum(1.0f), m_thumbSpan(1.0f), m_step(1.0f), m_data(nullptr),
      m_child1(nullptr), m_releaseTimer(0.0f)
{
    m_state.target = 1.0f;   // +4C
    m_state.current = 1.0f;  // +50
}

TypeDescriptor* BfFixedSliderNode::getClassType()         // 7D4850, singletonA23D84
{
    static ClassTypeDescriptor<BfFixedSliderNode, SliderNode>
        type("dice::meme::BfFixedSliderNode");
    return &type;
}
TypeDescriptor* BfFixedSliderNode::getType() { return getClassType(); }
unsigned int BfFixedSliderNode::getObjectSize() { return sizeof(BfFixedSliderNode); }

// Byte-for-byte the base's bodies against +48 instead of +20,
// including the same asymmetry: the getter sends every index but 1 to Node's
// +04 slot, while the setter accepts only 0 and 1 and silently drops the rest.
Node*& BfFixedSliderNode::getChild(int index)
{
    if (index == 1) return m_child1;
    return m_next;
}

void BfFixedSliderNode::setChild(int index, Node* child)
{
    if (index == 0) { m_next = child; return; }
    if (index == 1) m_child1 = child;
}

// The base's body at this class's own offsets; see SliderNode.cpp for
// the ordered-difference reading of the single `fcomp`.
void BfFixedSliderNode::visitChildren(System* system, NodeState* state, const Rect* bounds,
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

// 7D3E00: the thumb's width on a track of the given width (SliderNode's 833700 one level down).
float BfFixedSliderNode::thumbExtent(const float* width) const
{
    return static_cast<float>(m_thumbSpan / orderedSpan(m_minimum, m_maximum) * *width);
}

// 7D3E40: places the thumb from the bound value (SliderNode's 833740 one level down).
void BfFixedSliderNode::syncFromData(System* system, const float* width)
{
    if (!m_data) return;
    const double thumb = m_thumbSpan / orderedSpan(m_minimum, m_maximum) * *width;
    const float scale = static_cast<float>((*width - thumb) /
        (static_cast<double>(m_maximum) - m_minimum));
    m_thumbOffset = static_cast<float>(
        (static_cast<double>(m_data->getFloat(system)) - m_minimum) * scale);
}

// 7D3D50: the base's stream rows at this class's own offsets, then the step as "Interval".
bool BfFixedSliderNode::streamProperties(System* system, Stream* stream)
{
    if (!Node::streamProperties(system, stream)) return false;
    if (!stream->streamNode0x84(system, m_child1, "Cursor node", nullptr)) return false;
    if (!stream->streamData(system, m_data, "Data", nullptr)) return false;
    if (!stream->streamFloat(&m_minimum, "Minimum value")) return false;
    if (!stream->streamFloat(&m_maximum, "Maximum value")) return false;
    if (!stream->streamFloat(&m_thumbSpan, "Number visible")) return false;
    return stream->streamFloat(&m_step, "Interval");
}

// 7D4920: SliderNode::paint (833B80) at this class's offsets.
void BfFixedSliderNode::paint(System* system, const NodeState* state, const Rect* bounds,
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

// 7D4A30: SliderNode::update (833C90) at this class's offsets.
bool BfFixedSliderNode::update(System* system, const NodeState* state, const Rect* bounds, float delta)
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

// 7D4250: SliderNode's drag (833810), then the fixed step:
// - a drag writes the dragged value, re-reads it and snaps it up to the next multiple of the step
//   (counted by repeated subtraction; a value <= 0 snaps to 0);
// - without a held press the thumb is placed from the Data;
// - either way the value is clamped to [minimum, maximum] (an unordered value stays) and written;
// - while a press is held, a timer runs; once it passes 0.25 s with the left mouse button up and
//   the pointer still locked, the release is synthesised through onEvent3 (+7C) - a missed
//   button-up never leaves the pointer locked. The timer restarts after every check past 0.25 s.
// Always true; nothing at all without a Data.
bool BfFixedSliderNode::updateSelf(System* system, const NodeState* state, const Rect* rect, float delta)
{
    if (!m_data) return true;
    if (pressHeld(state, system)) {
        const float* pointer = system->getPointerPosition();
        float position[2] = {pointer[0], pointer[1]};
        clampToRect(position, rect);
        position[0] = position[0] - rect->x;
        const double thumb = m_thumbSpan / orderedSpan(m_minimum, m_maximum) * rect->width;
        const double offset = static_cast<double>(position[0]) - m_grabOffset;
        m_thumbOffset = static_cast<float>(offset);
        if (offset < 0.0) m_thumbOffset = 0.0f;
        const double limit = rect->width - thumb;
        if (!(m_thumbOffset < limit) && limit == limit && m_thumbOffset == m_thumbOffset)
            m_thumbOffset = static_cast<float>(limit);
        m_data->setFloat(system, static_cast<float>(m_thumbOffset /
            ((rect->width - thumb) / (static_cast<double>(m_maximum) - m_minimum)) + m_minimum));
        double value = m_data->getFloat(system);
        int steps = 0;
        if (value > 0.0) {
            do {
                value -= m_step;
                ++steps;
            } while (value > 0.0);
        }
        m_data->setFloat(system, static_cast<float>(steps * static_cast<double>(m_step)));
    } else {
        syncFromData(system, &rect->width);
    }
    float value = m_data->getFloat(system);
    if (value > m_maximum) value = m_maximum;
    if (value < m_minimum) value = m_minimum;
    m_data->setFloat(system, value);
    if (pressHeld(state, system)) {
        m_releaseTimer = static_cast<float>(static_cast<double>(delta) + m_releaseTimer);
        if (m_releaseTimer > 0.25f) {
            hfe::io::IInputDevice* const mouse = static_cast<hfe::io::IInputDeviceManager*>(
                hfe::io::inputDeviceManager)->getInputDevice(hfe::io::IDFMouse);
            if (mouse && !static_cast<hfe::io::IAdvancedInputDevice*>(mouse)->buttonDown(hfe::io::IDButton_0) && system->getLockCount() > 0)
                onEvent3(system, rect, state, 1, 1);
            m_releaseTimer = 0.0f;
        }
    }
    return true;
}

// 7D40A0: SliderNode::onEvent1 (833900) at this class's offsets.
bool BfFixedSliderNode::onEvent1(System* system, const Rect* rect, const NodeState* state,
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

// 7D4430, both slots +78 and +7C: unlike the base (833A30) the release only unlocks and re-places
// the thumb while the pointer is actually locked (6CFB80 > 0). Never consumes the event.
bool BfFixedSliderNode::onEvent2(System* system, const Rect* rect, const NodeState*, int first, int)
{
    if (first == 1 && system->getLockCount() > 0) {
        system->unlockPointer();
        syncFromData(system, &rect->width);
    }
    return false;
}

bool BfFixedSliderNode::onEvent3(System* system, const Rect* rect, const NodeState* state,
                                 int first, int index)
{
    return BfFixedSliderNode::onEvent2(system, rect, state, first, index);
}

} }
#endif
