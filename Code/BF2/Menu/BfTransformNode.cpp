// Menu transform node. [INFERRED] Source placement.
// Inherited unimplemented Node operations retain their diagnostic stops.
#include "BfTransformNode.h"
#if !DEDICATED
#include "../External/BFMeme/Data.h"
#include "../External/BFMeme/NodeState.h"
#include "../External/BFMeme/Stream.h"
#include "../External/BFMeme/Style.h"
#include "../External/BFMeme/System.h"
#include "../External/BFMeme/TypeDescriptor.h"

namespace dice { namespace meme {
namespace {
// [INFERRED] descriptor spelling. CounterA23D14 is in the PE's unloaded .data
// tail; ordinary image loading supplies its initial zero.
int bfTransformNodeInstances;
class BfTransformNodeTypeDescriptor final : public TypeDescriptor
{
public:
    BfTransformNodeTypeDescriptor() : TypeDescriptor("dice::meme::BfTransformNode") {}
    TypeDescriptor* getBaseType() override { return TransformNode::getClassType(); }
    RuntimeObject* create() override
    {
        // [RETIRE-WHEN: node-native-prefix-publication]
        void* allocation = ::operator new(sizeof(BfTransformNode));
        return allocation ? new(allocation) BfTransformNode : nullptr;
    }
    // Inherited descriptor destruction does not unlink; description is null.
    void addInstance() override { ++bfTransformNodeInstances; }
    void removeInstance() override { --bfTransformNodeInstances; }
    int getInstanceCount() override { return bfTransformNodeInstances; }
    unsigned int getInstanceSize() override { return sizeof(BfTransformNode); }
};

float decodedState(float value) // 7D2FBD..7D2FFC /838F40, binary32 store
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
float stateRate(const NodeState& state) // 838F40 ->82BD30; unused receivers
{
    const float current = decodedState(state.current);
    const float target = decodedState(state.target);
    if (target < current) return -3.0f;
    if (target > current) return 3.0f;
    return 0.0f;
}
bool sumNotLess(float first, float second, float limit)
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
bool stateEnabled(float value) // 7D3230..7D325E; no float spill before comparison
{
#if defined(_M_IX86)
    const float one = 1.0f;
    bool result;
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
        fcomp one
        fnstsw ax
        test ah, 5
        setp result
    }
    return result;
#else
    // [PLATFORM] x64: SSE2 arithmetic (x87 keeps 80-bit intermediates).
    // The decoded value is compared unrounded; unordered compares pass.
    const double decoded = value < 0.0f ? -(static_cast<double>(value) + 1.0) : value;
    return !(decoded < 1.0);
#endif
}
}

BfTransformNode::BfTransformNode() // initialization inline in7D34E0
    : m_transformedChild(nullptr), m_transformedPosition{0.0f, 0.0f},
      m_transformedSize{20.0f, 20.0f}, m_transformedState{1.0f, 1.0f},
      m_xData(nullptr), m_yData(nullptr), m_widthData(nullptr), m_heightData(nullptr) {}

TypeDescriptor* BfTransformNode::getClassType()
{
    static BfTransformNodeTypeDescriptor type;
    return &type;
}
TypeDescriptor* BfTransformNode::getType() { return getClassType(); }
namespace {
// CRT96DB1C ->86D170 unconditionally sets guardA23D2C and constructs the same
// descriptor. Native static/eager spelling supplies its once-only lifetime.
TypeDescriptor* const registeredBfTransformNodeType = BfTransformNode::getClassType();
}

unsigned int BfTransformNode::getObjectSize() { return sizeof(BfTransformNode); }
NodeState* BfTransformNode::getState() { return &m_transformedState; }
Node*& BfTransformNode::getChild(int index) // 7D3180; all other indices alias next
{ return index == 1 ? m_transformedChild : m_next; }
void BfTransformNode::setChild(int index, Node* child)
{
    if (index == 0) m_next = child;
    else if (index == 1) m_transformedChild = child;
}
void BfTransformNode::sampleData(System* system) // inline7D30D1 and7D3126
{
    if (m_xData) m_transformedPosition.x = m_xData->getFloat(system);
    if (m_yData) m_transformedPosition.y = m_yData->getFloat(system);
    if (m_widthData) m_transformedSize.x = m_widthData->getFloat(system);
    if (m_heightData) m_transformedSize.y = m_heightData->getFloat(system);
}
bool BfTransformNode::streamProperties(System* system, Stream* stream)
{
    // Width/Height stream the local floats; their optional Data links are only sampled.
    if (!Node::streamProperties(system, stream)
        || !stream->streamData(system, m_xData, "X", nullptr)
        || !stream->streamData(system, m_yData, "Y", nullptr)
        || !stream->streamFloat(&m_transformedSize.x, "Width")
        || !stream->streamFloat(&m_transformedSize.y, "Height")
        || !stream->streamNode0x84(system, m_transformedChild, "Transformed node", nullptr))
        return false;
    sampleData(system);
    return true;
}
void BfTransformNode::visitChildren(System* system, NodeState* state,
                                  const Rect* bounds, NodeVisitor* visitor)
{
    const Rect transformed = {bounds->x + m_transformedPosition.x,
        bounds->y + m_transformedPosition.y, m_transformedSize.x,
        m_transformedSize.y, bounds->z};
    visitor->traverse0x08(system, m_transformedChild, &transformed, &m_transformedState);
    visitor->visit(system, m_next, bounds, state);
}
void BfTransformNode::paint(System* system, const NodeState* state, const Rect* bounds,
                          const Rect* clip, RenderContext* context)
{
    const Rect transformed = {bounds->x + m_transformedPosition.x,
        bounds->y + m_transformedPosition.y, m_transformedSize.x,
        m_transformedSize.y, bounds->z};
    paintSelf(system, state, &transformed, clip, context);
    if (m_transformedChild) {
        if (context->effect) {
            RenderContext childContext = {context, nullptr, context->field08,
                context->field0c, context->field10,
                decodedState(m_transformedState.current), 1.0f};
            childContext.field10 = stateRate(m_transformedState);
            m_transformedChild->paint(system, &m_transformedState, &transformed, clip, &childContext);
        } else {
            m_transformedChild->paint(system, &m_transformedState, &transformed, clip, context);
        }
    }
    if (m_next) m_next->paint(system, state, bounds, clip, context);
}
bool BfTransformNode::update(System* system, const NodeState* state,
                           const Rect* bounds, float delta)
{
    const Rect transformed = {bounds->x + m_transformedPosition.x,
        bounds->y + m_transformedPosition.y, m_transformedSize.x,
        m_transformedSize.y, bounds->z};
    bool inside = false;
    if (state->target < 0.0f) {
        const float* pointer = system->getPointerPosition();
        inside = !(pointer[0] < transformed.x) && !(pointer[1] < transformed.y)
            && sumNotLess(transformed.width, transformed.x, pointer[0])
            && sumNotLess(transformed.height, transformed.y, pointer[1]);
    }
    const bool enabled = stateEnabled(state->target);
    updateNodeState(&m_transformedState, system, m_transformedChild, this,
                    &transformed, delta, inside, enabled);
    if (m_transformedChild
        && !m_transformedChild->update(system, &m_transformedState, &transformed, delta))
        return false;
    return Node::update(system, state, bounds, delta);
}
bool BfTransformNode::updateSelf(System* system, const NodeState*, const Rect*, float)
{
    sampleData(system);
    return true;
}
#if defined(_M_IX86)
static_assert(sizeof(BfTransformNode) == 80, "client BfTransformNode layout");
#endif
} }
#endif
