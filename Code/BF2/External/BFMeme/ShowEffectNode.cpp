// [INFERRED] path. Complete primary94A448, metadata94A308, construction,
// eager initialization and every differing prefix body read before recovery.
// No suffix differs from EffectNode/Node; existing caller stops remain.
#include "ShowEffectNode.h"
#if !DEDICATED
#include "TypeDescriptor.h"
#include <cstring>
#include <new>
namespace dice { namespace meme {
namespace {
int showEffectNodeInstances;                              // A3978C, independent counter
class ShowEffectNodeTypeDescriptor final : public TypeDescriptor
{
public:
    ShowEffectNodeTypeDescriptor() : TypeDescriptor("dice::meme::ShowEffectNode") {}
    TypeDescriptor* getBaseType() override { return EffectNode::getClassType(); }
    RuntimeObject* create() override
    {
        // [CHOSEN, not recovered] [RETIRE-WHEN: node-native-prefix-publication]
        // Publication policy only; allocation/null branch/construction are real.
        void* allocation = ::operator new(sizeof(ShowEffectNode));
        return allocation ? new(allocation) ShowEffectNode : nullptr;
    }
    void addInstance() override { ++showEffectNodeInstances; }
    void removeInstance() override { --showEffectNodeInstances; }
    int getInstanceCount() override { return showEffectNodeInstances; }
    unsigned int getInstanceSize() override { return sizeof(ShowEffectNode); }
};
}
ShowEffectNode::ShowEffectNode(Effect* effect) : EffectNode(effect) {}
TypeDescriptor* ShowEffectNode::getClassType()
{
    static ShowEffectNodeTypeDescriptor type;
    return &type;
}
TypeDescriptor* ShowEffectNode::getType() { return getClassType(); }
namespace {
// CRT96E0A4 ->86FFC0 guards the shared descriptorA397DC and storesA398C4.
TypeDescriptor* const registeredShowEffectNodeType = ShowEffectNode::getClassType();
}
bool ShowEffectNode::onCreate(System* system)
{
    return EffectNode::onCreate(system) != false;
}
bool ShowEffectNode::visit0x34(System* system, Stream* stream)
{
    return EffectNode::visit0x34(system, stream) != false;
}
void ShowEffectNode::paint(System* system, const NodeState* state,
                           const Rect* bounds, const Rect* clip,
                           RenderContext* context)
{
    if (m_effect)
    {
        // Retail compares the raw word, not an FP value. There is no context
        // null guard on this arm and no check of the incoming effect field.
        std::uint32_t bits;
        std::memcpy(&bits, &context->field0c, sizeof(bits));
        if (bits != 0x3F800000u)
        {
            RenderContext shown;
            shown.parent = context;
            shown.effect = m_effect;
            std::memcpy(&shown.field08, &context->field08, 4 * sizeof(float));
            std::memcpy(&shown.field18, &context->field0c, sizeof(float));
            Node::paint(system, state, bounds, clip, &shown);
            return;
        }
    }
    Node::paint(system, state, bounds, clip, context);
}
#if defined(_M_IX86)
static_assert(sizeof(ShowEffectNode) == 12, "client ShowEffectNode prefix storage");
static_assert(sizeof(ShowEffectNodeTypeDescriptor) == 16, "ShowEffectNode metadata");
#endif
} }
#endif
