// [INFERRED] path. Complete primary94A1D0 (50 slots), metadata94A2C8,
// constructor82C220/factory82CB10 and every differing prefix body read.
// Only the existing Node33 prefix is emitted.
#include "EffectNode.h"
#if !DEDICATED
#include "Effect.h"
#include "Stream.h"
#include "TypeDescriptor.h"
#include <new>
namespace dice { namespace meme {
namespace {
// [INFERRED] native descriptor name; independent counterA39784 is separate
// from descriptorA397B4. Object construction/destruction does not change it.
int effectNodeInstances;
class EffectNodeTypeDescriptor final : public TypeDescriptor
{
public:
    EffectNodeTypeDescriptor() : TypeDescriptor("dice::meme::EffectNode") {}
    TypeDescriptor* getBaseType() override { return Node::getClassType(); }
    RuntimeObject* create() override
    {
        // [CHOSEN, not recovered] [RETIRE-WHEN: node-native-prefix-publication]
        // Publication policy only; ordinary allocation/constructor are recovered.
        void* allocation = ::operator new(sizeof(EffectNode));
        return allocation ? new(allocation) EffectNode : nullptr;
    }
    // Inherited destructor/description are the recovered no-unlink/null bodies.
    void addInstance() override { ++effectNodeInstances; }
    void removeInstance() override { --effectNodeInstances; } // 82CB50, no zero guard
    int getInstanceCount() override { return effectNodeInstances; }
    unsigned int getInstanceSize() override { return sizeof(EffectNode); }
};
#if defined(_M_IX86)
static_assert(sizeof(EffectNodeTypeDescriptor) == 16, "EffectNode metadata storage");
#endif
}

EffectNode::EffectNode(Effect* effect) : Node(), m_effect(effect) {}
EffectNode::~EffectNode() {}                              // 835660, borrowed links
unsigned int EffectNode::getObjectSize() { return sizeof(EffectNode); }
TypeDescriptor* EffectNode::getClassType()
{
    static EffectNodeTypeDescriptor type;
    return &type;
}
TypeDescriptor* EffectNode::getType() { return getClassType(); }
namespace {
// CRT96E09C ->86FF30 unconditionally sets the guard before constructing the
// same descriptor and storesA398C0. Native static lifetime owns its teardown.
TypeDescriptor* const registeredEffectNodeType = EffectNode::getClassType();
}

bool EffectNode::streamProperties(System* system, Stream* stream)
{
    if (!Node::streamProperties(system, stream)) return false;
    return stream->streamEffect(system, m_effect, "Effect", nullptr) != false;
}

void EffectNode::paint(System* system, const NodeState* state, const Rect* bounds,
                       const Rect* clip, RenderContext* context)
{
    Effect* const effect = m_effect;
    if (!effect) {
        Node::paint(system, state, bounds, clip, context);
        return;
    }
    // Retail dereferences the supplied context whenever an effect is present,
    // even with no successor. Node::paint retains virtual paintSelf first.
    if (context->effect) {
        RenderContext nested = {context, effect, context->field08, context->field0c,
                                context->field10, context->field14, 1.0f};
        Node::paint(system, state, bounds, clip, &nested);
    } else {
        context->effect = effect;
        context->field18 = 1.0f;
        Node::paint(system, state, bounds, clip, context);
        // Do not restore field18 or other context changes made by callbacks.
        context->effect = nullptr;
    }
}

bool EffectNode::updateSelf(System* system, const NodeState*, const Rect*, float delta)
{
    return updateEffect(system, m_effect, delta);
}

#if defined(_M_IX86)
static_assert(sizeof(EffectNode) == 12, "client BFMeme EffectNode");
#endif
} }
#endif
