// Transform-node state updates and metadata; unsupported Node suffix calls retain diagnostic stops.
#include "TransformNode.h"
#if !DEDICATED
#include "NodeState.h"
#include "Stream.h"
#include "Style.h"
#include "System.h"
#include "TypeDescriptor.h"
namespace dice { namespace meme {
namespace {
// [INFERRED] descriptor class name. Table94AE90 is a complete eight-slot
// interface; its independent counterA398D4 is not adjusted by construction.
int transformNodeInstances;
class TransformNodeTypeDescriptor final : public TypeDescriptor
{
public:
    TransformNodeTypeDescriptor() : TypeDescriptor("dice::meme::TransformNode") {}
    TypeDescriptor* getBaseType() override { return Node::getClassType(); }
    RuntimeObject* create() override
    {
        // [RETIRE-WHEN: node-native-prefix-publication]
        // Real allocation and construction of the native33-slot prefix.
        void* allocation = ::operator new(sizeof(TransformNode));
        return allocation ? new(allocation) TransformNode : nullptr;
    }
    // Base destructor/description retain the actual no-unlink/null behavior.
    void addInstance() override { ++transformNodeInstances; }
    void removeInstance() override { --transformNodeInstances; }
    int getInstanceCount() override { return transformNodeInstances; }
    unsigned int getInstanceSize() override { return sizeof(TransformNode); }
};

bool streamPair(hfe::Vec2& value, Stream* stream, const char* first, const char* second)
{
    if (!stream->streamFloat(&value.x, first)) return false;
    return stream->streamFloat(&value.y, second);
}
float decodedState(float value) // float stores in838F40 and82D340
{
#if defined(_M_IX86)
    float result;
    __asm {
        fld value
        ftst
        fnstsw ax
        test ah, 5
        jp decoded
        fld1
        faddp st(1), st(0)
        fchs
    decoded:
        fstp result
    }
    return result;
#else
    // [PLATFORM] x64: SSE2 arithmetic (x87 keeps 80-bit intermediates).
    // Only an ordered negative value is decoded; zero, positive and NaN pass through the
    // double round trip, which quiets a signalling NaN as fld/fstp does.
    const double loaded = value;
    return static_cast<float>(value < 0.0f ? -(loaded + 1.0) : loaded);
#endif
}
float stateRate(const NodeState& state) // 838F40 ->82BD30; System/Node receivers unused
{
    const float current = decodedState(state.current);
    const float target = decodedState(state.target);
    if (target < current) return -3.0f;
    if (target > current) return 3.0f;
    return 0.0f;
}
bool sumNotLess(float first, float second, float limit) // 82D49E..4BB, unordered passes
{
#if defined(_M_IX86)
    bool result;
    __asm {
        fld first
        fadd second
        fcomp limit
        fnstsw ax
        test ah, 5
        setp result
    }
    return result;
#else
    // [PLATFORM] x64: SSE2 arithmetic (x87 keeps 80-bit intermediates).
    // setp after test ah,5: false only for an ordered sum below the limit.
    const double sum = static_cast<double>(first) + second;
    return !(sum < limit);
#endif
}
}

TransformNode::TransformNode() : m_child(nullptr), m_position{0.0f, 0.0f},
    m_size{20.0f, 20.0f}, m_state{1.0f, 1.0f} {} // 82D630; links remain borrowed
TypeDescriptor* TransformNode::getClassType()
{
    static TransformNodeTypeDescriptor type;
    return &type;
}
TypeDescriptor* TransformNode::getType() { return getClassType(); }
namespace {
// CRT96E0CC ->8702E0 registers this same descriptor before normal lookup.
// Native C++ supplies the static guard/destruction; source spelling inferred.
TypeDescriptor* const registeredTransformNodeType = TransformNode::getClassType();
}

unsigned int TransformNode::getObjectSize() { return sizeof(TransformNode); }
const char* TransformNode::getName() { return nullptr; } // 80D430, real default
int TransformNode::getChildCount() { return 2; }
Node*& TransformNode::getChild(int index) // 835860; every index except1 returns next
{ return index == 1 ? m_child : m_next; }
void TransformNode::setChild(int index, Node* child) // 835880; invalid indices ignored
{
    if (index == 0) m_next = child;
    else if (index == 1) m_child = child;
}
bool TransformNode::streamProperties(System* system, Stream* stream)
{
    return Node::streamProperties(system, stream)
        && streamPair(m_position, stream, "X", "Y")
        && streamPair(m_size, stream, "Width", "Height")
        && stream->streamNode0x84(system, m_child, "Transformed node", nullptr);
}
void TransformNode::visitChildren(System* system, NodeState* state,
                                 const Rect* bounds, NodeVisitor* visitor)
{
    const Rect transformed = {bounds->x + m_position.x, bounds->y + m_position.y,
        m_size.x, m_size.y, bounds->z};
    visitor->traverse0x08(system, m_child, &transformed, &m_state);
    visitor->visit(system, m_next, bounds, state);
}
void TransformNode::paint(System* system, const NodeState* state, const Rect* bounds,
                          const Rect* clip, RenderContext* context)
{
    const Rect transformed = {bounds->x + m_position.x, bounds->y + m_position.y,
        m_size.x, m_size.y, bounds->z};
    paintSelf(system, state, &transformed, clip, context);
    if (m_child) {
        if (context->effect) {
            RenderContext childContext = {context, nullptr, context->field08,
                context->field0c, context->field10, decodedState(m_state.current), 1.0f};
            childContext.field10 = stateRate(m_state);
            m_child->paint(system, &m_state, &transformed, clip, &childContext);
        } else m_child->paint(system, &m_state, &transformed, clip, context);
    }
    if (m_next) m_next->paint(system, state, bounds, clip, context);
}
bool TransformNode::update(System* system, const NodeState* state,
                           const Rect* bounds, float delta)
{
    const Rect transformed = {bounds->x + m_position.x, bounds->y + m_position.y,
        m_size.x, m_size.y, bounds->z};
    bool inside = false;
    if (state->target < 0.0f) {
        const float* position = system->getPointerPosition();
        inside = !(position[0] < transformed.x) && !(position[1] < transformed.y)
            && sumNotLess(transformed.width, transformed.x, position[0])
            && sumNotLess(transformed.height, transformed.y, position[1]);
    }
    // Both unordered bounds comparisons and an unordered decoded parent state
    // pass their retail x87 tests. Do not replace these with >= comparisons.
    const bool enabled = !(decodedState(state->target) < 1.0f);
    updateNodeState(&m_state, system, m_child, this, &transformed, delta, inside, enabled);
    if (m_child && !m_child->update(system, &m_state, &transformed, delta)) return false;
    return Node::update(system, state, bounds, delta);
}
void TransformNode::reset() // 82D560; Node's real+C4 dispatch remains a named frontier
{
    if (m_child) m_child->reset();
    Node::reset();
}
#if defined(_M_IX86)
static_assert(sizeof(TransformNode) == 36, "native x86 TransformNode storage");
static_assert(sizeof(TransformNodeTypeDescriptor) == 16, "native TransformNode metadata");
#endif
} }
#endif
