#include "InputNodeVisitors.h"
#if !DEDICATED
namespace dice { namespace meme {
bool dispatchNodeEvent(System* system, Node* node, const Rect* rect,
                       const NodeState* state, const NodeInputEvent* event)
{
    return node->dispatchEvent(system, rect, state, event);
}

Event1NodeVisitor::Event1NodeVisitor(int first, int index, int second)
    : NodeVisitor(0), m_first(first), m_index(index), m_second(second), m_handled(false) {}
Event2NodeVisitor::Event2NodeVisitor(int first, int index)
    : NodeVisitor(0), m_first(first), m_index(index) {}
Event4NodeVisitor::Event4NodeVisitor(int first, int index, int second)
    : NodeVisitor(0), m_first(first), m_index(index), m_second(second) {}
Event5NodeVisitor::Event5NodeVisitor(int first)
    : NodeVisitor(0), m_first(first) {} // m_index deliberately untouched by83BBBA.
PressStateNodeVisitor::PressStateNodeVisitor() : NodeVisitor(0) {}
ReleaseStateNodeVisitor::ReleaseStateNodeVisitor() : NodeVisitor(0) {}

void Event1NodeVisitor::visit(System* system, Node*& node, const Rect* rect, NodeState* state)
{
    if (node && state->current < 0.0f) {
        node->visitChildren(system, state, rect, this);
        if (!m_handled) {
            NodeInputEvent event = {1, m_index, static_cast<float>(m_first),
                                     static_cast<float>(m_second), 0};
            m_handled = dispatchNodeEvent(system, node, rect, state, &event);
        }
    }
}
void Event2NodeVisitor::visit(System* system, Node*& node, const Rect* rect, NodeState* state)
{
    if (node && state->current < 0.0f) {
        node->visitChildren(system, state, rect, this);
        const bool negative = state->target < 0.0f;
        NodeInputEvent event = {negative ? 2 : 3, m_index, static_cast<float>(m_first), 0.0f, 0};
        dispatchNodeEvent(system, node, rect, state, &event);
    }
}
void Event4NodeVisitor::visit(System* system, Node*& node, const Rect* rect, NodeState* state)
{
    if (node && state->current < 0.0f) {
        node->visitChildren(system, state, rect, this);
        NodeInputEvent event = {4, m_index, static_cast<float>(m_first),
                                 static_cast<float>(m_second), 0};
        dispatchNodeEvent(system, node, rect, state, &event);
    }
}
void Event5NodeVisitor::visit(System* system, Node*& node, const Rect* rect, NodeState* state)
{
    if (node && state->current < 0.0f) {
        node->visitChildren(system, state, rect, this);
        const bool negative = state->target < 0.0f;
        NodeInputEvent event = {negative ? 5 : 6, m_index, static_cast<float>(m_first), 0.0f, 0};
        dispatchNodeEvent(system, node, rect, state, &event);
    }
}
void PressStateNodeVisitor::visit(System* system, Node*& node, const Rect* rect, NodeState* state)
{
    if (node) {
        float decoded = state->target;
        if (decoded < 0.0f) decoded = -(decoded + 1.0f);
        if (decoded != 2.0f) {
            decoded = state->target;
            if (decoded < 0.0f) decoded = -(decoded + 1.0f);
            if (decoded != 3.0f) return;
        }
        state->target = state->target < 0.0f ? -4.0f : 3.0f;
        node->visitChildren(system, state, rect, this);
    }
}
void ReleaseStateNodeVisitor::visit(System* system, Node*& node, const Rect* rect, NodeState* state)
{
    if (node) {
        float decoded = state->target;
        if (decoded < 0.0f) decoded = -(decoded + 1.0f);
        if (decoded == 3.0f)
            state->target = state->target < 0.0f ? -3.0f : 2.0f;
        node->visitChildren(system, state, rect, this);
    }
}
#if defined(_M_IX86)
static_assert(sizeof(Event1NodeVisitor) == 24, "client event1 visitor");
static_assert(sizeof(Event2NodeVisitor) == 16, "client event2 visitor");
static_assert(sizeof(Event4NodeVisitor) == 20, "client event4 visitor");
static_assert(sizeof(Event5NodeVisitor) == 16, "client event5 visitor, including unwritten index");
static_assert(sizeof(PressStateNodeVisitor) == 8, "client press state visitor");
static_assert(sizeof(ReleaseStateNodeVisitor) == 8, "client release state visitor");
#endif
} }
#endif
