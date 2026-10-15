// [INFERRED] path. Complete client primary table938600, metadata94E134,
// factory836060 and every differing existing-prefix body read. The native
// 33-slot prefix uses the owner-approved startup exception; inherited
// event/reset suffix calls still stop at their known Node callers.
#include "CullNode.h"
#if !DEDICATED
#include "Data.h"
#include "Stream.h"
#include "TypeDescriptor.h"
#include <new>
namespace dice { namespace meme {
namespace {
// [INFERRED] descriptor class name. CounterA3A07C is separate from the
//16-byte descriptorA3A080 and is not adjusted by construction/destruction.
int cullNodeInstances;
class CullNodeTypeDescriptor final : public TypeDescriptor
{
public:
    CullNodeTypeDescriptor() : TypeDescriptor("dice::meme::CullNode") {}
    TypeDescriptor* getBaseType() override { return Node::getClassType(); }
    RuntimeObject* create() override
    {
        // [CHOSEN, not recovered] [RETIRE-WHEN: node-native-prefix-publication]
        // Publication policy only: allocation and constructor are recovered.
        void* allocation = ::operator new(sizeof(CullNode));
        return allocation ? new(allocation) CullNode : nullptr;
    }
    // Inherited destructor/description are the recovered no-unlink/null bodies.
    void addInstance() override { ++cullNodeInstances; }
    void removeInstance() override { --cullNodeInstances; } // 836040, no zero guard
    int getInstanceCount() override { return cullNodeInstances; }
    unsigned int getInstanceSize() override { return sizeof(CullNode); }
};
#if defined(_M_IX86)
static_assert(sizeof(CullNodeTypeDescriptor) == 16, "CullNode metadata storage");
#endif

// 835F25..835F45 and835F69..835F82 store the rounded float before comparing
// the still-extended x87 result. Keep that order, including unordered results
// and the sign of values that round to zero. Helper names are [INFERRED].
float decreaseFade(float current, float delta, float duration)
{
#if defined(_M_IX86)
    float result;
    const float zero = 0.0f;
    __asm {
        fld delta
        fdiv duration
        fsubr current
        fst result
        fcomp zero
        fnstsw ax
        test ah, 5
        jp decreased
        mov result, 0
    decreased:
    }
    return result;
#else
    // [PLATFORM] x64: SSE2 arithmetic (x87 keeps 80-bit intermediates).
    // Only an ordered negative unrounded result is replaced by +0.
    const double unrounded = static_cast<double>(current) - static_cast<double>(delta) / duration;
    return unrounded < 0.0 ? 0.0f : static_cast<float>(unrounded);
#endif
}
float increaseFade(float current, float delta, float duration)
{
#if defined(_M_IX86)
    float result;
    const float one = 1.0f;
    __asm {
        fld delta
        fdiv duration
        fadd current
        fst result
        fcomp one
        fnstsw ax
        test ah, 41h
        jnz increased
        mov result, 3F800000h
    increased:
    }
    return result;
#else
    // [PLATFORM] x64: SSE2 arithmetic (x87 keeps 80-bit intermediates).
    // Only an ordered unrounded result above one is clamped; equal and unordered keep it.
    const double unrounded = static_cast<double>(delta) / duration + current;
    return unrounded > 1.0 ? 1.0f : static_cast<float>(unrounded);
#endif
}
}

CullNode::CullNode() : m_data(nullptr), m_inTime(0.0f),
    m_outTime(0.0f), m_fade(0.0f) {}
CullNode::~CullNode() {}
unsigned int CullNode::getObjectSize() { return sizeof(CullNode); }
TypeDescriptor* CullNode::getClassType()
{
    static CullNodeTypeDescriptor type;
    return &type;
}
TypeDescriptor* CullNode::getType() { return getClassType(); }
namespace {
// CRT96E1E4 ->8716D0 constructs the same descriptor and storesA3A094.
// Native C++ owns the static guard and teardown; spelling is [INFERRED].
TypeDescriptor* const registeredCullNodeType = CullNode::getClassType();
}

bool CullNode::streamProperties(System* system, Stream* stream)
{
    if (!Node::streamProperties(system, stream)) return false;
    if (!stream->streamData(system, m_data, "Data", nullptr)) return false;
    if (!stream->streamFloat(&m_inTime, "In time")) return false;
    return stream->streamFloat(&m_outTime, "Out time") != false;
}

void CullNode::visitChildren(System* system, NodeState* state,
                             const Rect* rect, NodeVisitor* visitor)
{
    // NaN differs from zero and still visits; no successor null guard.
    if (m_fade != 0.0f) visitor->visit(system, m_next, rect, state);
}

void CullNode::paint(System* system, const NodeState* state, const Rect* bounds,
                     const Rect* clip, RenderContext* context)
{
    if (!m_next) return;
    if (m_fade >= 1.0f)
    {
        m_next->paint(system, state, bounds, clip, context);
        return;
    }
    if (m_fade > 0.0f)
    {
        RenderContext faded = {context, nullptr, context->field08,
            context->field0c * m_fade, context->field10, context->field14, 1.0f};
        faded.field10 = m_data && m_data->getFloat(system) == 0.0f ? -1.0f : 1.0f;
        // The data callback may replace the successor; reload it without an
        // added null guard, as835EA5 does. No paintSelf/base paint call here.
        m_next->paint(system, state, bounds, clip, &faded);
    }
}

bool CullNode::update(System* system, const NodeState* state,
                      const Rect* bounds, float delta)
{
    if (!m_next) return true;
    if (m_data && m_data->getFloat(system) == 0.0f)
    {
        if (m_fade > 0.0f)
            m_fade = m_outTime == 0.0f ? 0.0f : decreaseFade(m_fade, delta, m_outTime);
    }
    else if (m_fade < 1.0f)
    {
        m_fade = m_inTime == 0.0f ? 1.0f : increaseFade(m_fade, delta, m_inTime);
    }
    if (m_fade != 0.0f) return m_next->update(system, state, bounds, delta);
    return true;
}

#if defined(_M_IX86)
static_assert(sizeof(CullNode) == 24, "client BFMeme CullNode");
#endif
} }
#endif
