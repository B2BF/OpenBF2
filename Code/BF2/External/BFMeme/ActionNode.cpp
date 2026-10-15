// [INFERRED] path. Complete client primary94BFB8, descriptor94BEB4,
// constructor831010/factory831050 and differing prefix instructions read.
// The owner native-prefix publication policy retains Node's unknown-slot and
// actual-use caller boundaries.
#include "ActionNode.h"
#if !DEDICATED
#include "Action.h"
#include "Stream.h"
#include "TypeDescriptor.h"
#include <new>
namespace dice { namespace meme {
namespace {
// [INFERRED] descriptor class name. Separate counterA39B74 and16-byte
// descriptorA39B94; construction/destruction does not adjust the counter.
int actionNodeInstances;
class ActionNodeTypeDescriptor final : public TypeDescriptor
{
public:
    ActionNodeTypeDescriptor() : TypeDescriptor("dice::meme::ActionNode") {}
    TypeDescriptor* getBaseType() override { return Node::getClassType(); }
    RuntimeObject* create() override
    {
        // [CHOSEN, not recovered] [RETIRE-WHEN: node-native-prefix-publication]
        // Publication policy only; the ordinary allocation/constructor are real.
        void* allocation = ::operator new(sizeof(ActionNode));
        return allocation ? new(allocation) ActionNode : nullptr;
    }
    // Inherited destructor/description preserve the recovered no-unlink/null bodies.
    void addInstance() override { ++actionNodeInstances; }
    void removeInstance() override { --actionNodeInstances; } // 830EA0, no zero guard
    int getInstanceCount() override { return actionNodeInstances; }
    unsigned int getInstanceSize() override { return sizeof(ActionNode); }
};
#if defined(_M_IX86)
static_assert(sizeof(ActionNodeTypeDescriptor) == 16, "ActionNode metadata storage");
#endif
}

ActionNode::ActionNode() : m_action(nullptr) {}
ActionNode::~ActionNode() {}
unsigned int ActionNode::getObjectSize() { return sizeof(ActionNode); }
TypeDescriptor* ActionNode::getClassType()
{
    static ActionNodeTypeDescriptor type;
    return &type;
}
TypeDescriptor* ActionNode::getType() { return getClassType(); }
namespace {
// CRT96E12C ->870980 registers the same descriptor and storesA39C48.
// Native C++ owns static initialization and teardown; spelling [INFERRED].
TypeDescriptor* const registeredActionNodeType = ActionNode::getClassType();
}

bool ActionNode::streamProperties(System* system, Stream* stream)
{
    if (!Node::streamProperties(system, stream)) return false;
    return stream->streamAction(system, m_action, "Action", nullptr) != false;
}

bool ActionNode::dispatchEvent(System* system, const Rect* bounds,
                               const NodeState* state, const NodeInputEvent* event)
{
    if (!m_action) return false;
    return m_action->execute(system, bounds, state, event);
}

bool ActionNode::updateSelf(System* system, const NodeState* state,
                            const Rect* bounds, float delta)
{
    if (!Node::updateSelf(system, state, bounds, delta)) return false;
    NodeInputEvent event = {22, 0, delta, 0.0f, 0};
    dispatchEvent(system, bounds, state, &event); // Virtual call; retail ignores its result.
    return true;
}

#if defined(_M_IX86)
static_assert(sizeof(ActionNode) == 12, "client BFMeme ActionNode");
#endif
} }
#endif
