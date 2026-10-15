// Node-state decoding and input traversal. Negative values encode -(value+1); names are [INFERRED].
#include "NodeState.h"
#if !DEDICATED
#include "System.h"
#include "InputNodeVisitors.h"
namespace dice { namespace meme {
namespace {
double decoded(float value) // 7C1BB0 and inlined equivalents; unordered stays raw
{ return value < 0.0f ? -(static_cast<double>(value) + 1.0) : value; }
float transitionRate(Node*, float target, float current) // 82BD30, receiver unused
{
    if (target < current) return -3.0f;
    if (target > current) return 3.0f;
    return 0.0f;
}
void setTarget(NodeState* state, float value, bool inside)
{ state->target = inside ? static_cast<float>(-(static_cast<double>(value) + 1.0)) : value; }
void setTargetValue(NodeState* state, float value) // 838DA0, retain encoded flag
{ setTarget(state, value, state->target < 0.0f); }
bool setCurrentInside(NodeState* state, System* system, bool inside)
{
    if (!system->getLockCount()) {
        if (inside) {
            if (state->current >= 0.0f) {
                state->current = static_cast<float>(-(static_cast<double>(state->current) + 1.0));
                return true;
            }
        } else if (state->current < 0.0f) {
            state->current = static_cast<float>(-(static_cast<double>(state->current) + 1.0));
            return true;
        }
    }
    return false;
}
bool pointerUnchanged(System* system)
{
    const float* previous = system->getPreviousPointerPosition();
    const float* current = system->getPointerPosition();
    return previous[0] == current[0] && previous[1] == current[1];
}

// Complete three-slot table94EB2C. Source class spelling is [INFERRED].
// Event traversal crosses the first node even when it owns a state, but stops
// descending at subsequent state-owning nodes. Each emitted event gets a new visitor.
class StateEventVisitor : public NodeVisitor
{
public:
    explicit StateEventVisitor(const NodeInputEvent& event)
        : NodeVisitor(0), m_event(event), m_first(true) {}
    void visit(System* system, Node*& node, const Rect* rect, NodeState* state) override
    {
        if (node) {
            dispatchNodeEvent(system, node, rect, state, &m_event);
            if (m_first) m_first = false;
            else if (node->getState()) return;
            node->visitChildren(system, state, rect, this);
        }
    }
private:
    NodeInputEvent m_event;
    bool m_first;
};
void emitStateEvent(int type, System* system, Node*& eventRoot, const Rect* rect, NodeState* state)
{
    const NodeInputEvent event = {type, 0, 0.0f, 0.0f, 0};
    StateEventVisitor visitor(event);
    visitor.traverse(system, eventRoot, rect, state);
}
#if defined(_M_IX86)
static_assert(sizeof(StateEventVisitor) == 32, "client state event visitor");
#endif
}
void updateNodeState(NodeState* state, System* system, Node* transitionSource,
                     Node* eventRoot, const Rect* rect, float delta, bool pointerInside, bool enabled)
{
    const float oldValue = static_cast<float>(decoded(state->current));
    const float targetValue = static_cast<float>(decoded(state->target));
    const float rate = transitionRate(transitionSource, targetValue, oldValue);
    const float nextValue = static_cast<float>(static_cast<double>(rate) * delta + decoded(state->current));
    if ((rate < 0.0f && nextValue <= decoded(state->target))
        || (rate > 0.0f && nextValue >= decoded(state->target))) {
        double value = decoded(state->target);
        if (state->current < 0.0f) value = -(value + 1.0);
        state->current = static_cast<float>(value);
    } else {
        state->current = state->current < 0.0f
            ? static_cast<float>(-(static_cast<double>(nextValue) + 1.0)) : nextValue;
    }

    if (!enabled) {
        if (state->current < 0.0f) {
            if (!system->getLockCount() && state->current < 0.0f)
                state->current = static_cast<float>(-(static_cast<double>(state->current) + 1.0));
            emitStateEvent(8, system, eventRoot, rect, state);
        }
        if (decoded(state->target) != 0.0)
            emitStateEvent(20, system, eventRoot, rect, state);
        state->target = pointerInside ? -1.0f : 0.0f;
        return;
    }

    if (decoded(state->target) == 0.0) {
        emitStateEvent(21, system, eventRoot, rect, state);
        state->target = state->target < 0.0f ? -2.0f : 1.0f;
    }
    if (setCurrentInside(state, system, pointerInside)) {
        if (pointerInside) emitStateEvent(7, system, eventRoot, rect, state);
        else {
            emitStateEvent(8, system, eventRoot, rect, state);
            if (decoded(state->target) == 2.0) setTargetValue(state, 1.0f);
        }
    }

    if (pointerInside) {
        if (state->target < 0.0f) {
            const double value = -(static_cast<double>(state->target) + 1.0);
            if (value == 3.0) {
                // Retail repeats the flag test even though the outer branch
                // already established the opposite. Preserve that quirk.
                if (state->target >= 0.0f) state->target = static_cast<float>(value);
            } else setTarget(state, 2.0f, pointerInside);
            if (!pointerUnchanged(system)) emitStateEvent(11, system, eventRoot, rect, state);
        } else {
            if (state->target == 3.0f) {
                if (state->target >= 0.0f)
                    state->target = static_cast<float>(-(static_cast<double>(state->target) + 1.0));
            } else setTarget(state, 2.0f, pointerInside);
            emitStateEvent(9, system, eventRoot, rect, state);
        }
        return;
    }

    if (state->target < 0.0f) {
        const double value = -(static_cast<double>(state->target) + 1.0);
        if (value == 3.0) {
            if (state->target < 0.0f) state->target = static_cast<float>(value);
        } else state->target = 1.0f;
        emitStateEvent(10, system, eventRoot, rect, state);
    } else if (state->current < 0.0f) {
        if (decoded(state->target) == 3.0) {
            if (state->target < 0.0f)
                state->target = static_cast<float>(-(static_cast<double>(state->target) + 1.0));
        } else state->target = 1.0f;
        if (!pointerUnchanged(system)) emitStateEvent(12, system, eventRoot, rect, state);
    }
}
} }
#endif
