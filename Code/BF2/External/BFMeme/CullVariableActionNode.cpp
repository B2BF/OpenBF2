// [INFERRED] path. Complete client primary94C2D8, descriptor94BF54,
// inlined constructor/factory831190 and differing prefix bodies read.
// No unknown Node+84/+88 slot is added.
#include "CullVariableActionNode.h"
#if !DEDICATED
#include "Action.h"
#include "Data.h"
#include "Stream.h"
#include "TypeDescriptor.h"
#include <new>
namespace dice { namespace meme {
namespace {
// [INFERRED] descriptor class name. CounterA39B88 is separate from
// descriptorA39BF8 and is not adjusted by construction/destruction.
int cullVariableActionNodeInstances;
class CullVariableActionNodeTypeDescriptor final : public TypeDescriptor
{
public:
    CullVariableActionNodeTypeDescriptor() : TypeDescriptor("dice::meme::CullVariableActionNode") {}
    TypeDescriptor* getBaseType() override { return ActionNode::getClassType(); }
    RuntimeObject* create() override
    {
        // [CHOSEN, not recovered] [RETIRE-WHEN: node-native-prefix-publication]
        // Publication policy only; ordinary allocation and construction are real.
        void* allocation = ::operator new(sizeof(CullVariableActionNode));
        return allocation ? new(allocation) CullVariableActionNode : nullptr;
    }
    // Inherited destructor/description preserve the recovered no-unlink/null bodies.
    void addInstance() override { ++cullVariableActionNodeInstances; }
    void removeInstance() override { --cullVariableActionNodeInstances; }
    int getInstanceCount() override { return cullVariableActionNodeInstances; }
    unsigned int getInstanceSize() override { return sizeof(CullVariableActionNode); }
};
#if defined(_M_IX86)
static_assert(sizeof(CullVariableActionNodeTypeDescriptor) == 16, "CullVariableActionNode metadata storage");
#endif
}

CullVariableActionNode::CullVariableActionNode() : m_variable(nullptr) {}
CullVariableActionNode::~CullVariableActionNode() {}
unsigned int CullVariableActionNode::getObjectSize() { return sizeof(CullVariableActionNode); }
TypeDescriptor* CullVariableActionNode::getClassType()
{
    static CullVariableActionNodeTypeDescriptor type;
    return &type;
}
TypeDescriptor* CullVariableActionNode::getType() { return getClassType(); }
namespace {
// CRT96E140 ->870B00 registers the same descriptor and storesA39C44.
// Native C++ owns static initialization and teardown; spelling [INFERRED].
TypeDescriptor* const registeredCullVariableActionNodeType = CullVariableActionNode::getClassType();
}

bool CullVariableActionNode::streamProperties(System* system, Stream* stream)
{
    if (!ActionNode::streamProperties(system, stream)) return false;
    return stream->streamData(system, m_variable, "Variable", nullptr) != false;
}

bool CullVariableActionNode::dispatchEvent(System* system, const Rect* bounds,
                                           const NodeState* state, const NodeInputEvent* event)
{
    if (!m_action) return false;
    if (m_variable && m_variable->getFloat(system) == 0.0f) return false;
    // Reload the action after the variable callback with no second null check.
    // Nonzero and unordered variable samples both allow execution.
    return m_action->execute(system, bounds, state, event);
}

#if defined(_M_IX86)
static_assert(sizeof(CullVariableActionNode) == 16, "client BFMeme CullVariableActionNode");
#endif
} }
#endif
