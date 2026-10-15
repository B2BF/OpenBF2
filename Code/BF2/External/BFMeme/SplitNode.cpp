// Split node implementation with diagnostic stops at unsupported Node suffix calls.
#include "SplitNode.h"
#if !DEDICATED
#include "Stream.h"
#include "TypeDescriptor.h"
namespace dice { namespace meme {
namespace {
// [INFERRED] descriptor class name. Table94DF38 has eight slots and a
// separate zero-initialized counterA3A044; construction does not increment it.
int splitNodeInstances;
class SplitNodeTypeDescriptor final : public TypeDescriptor
{
public:
    SplitNodeTypeDescriptor() : TypeDescriptor("dice::meme::SplitNode") {}
    TypeDescriptor* getBaseType() override { return Node::getClassType(); }
    RuntimeObject* create() override
    {
        // [RETIRE-WHEN: node-native-prefix-publication]
        void* allocation = ::operator new(sizeof(SplitNode));
        return allocation ? new(allocation) SplitNode : nullptr;
    }
    // The inherited destructor/description are the actual no-unlink/null bodies.
    void addInstance() override { ++splitNodeInstances; }
    void removeInstance() override { --splitNodeInstances; }
    int getInstanceCount() override { return splitNodeInstances; }
    unsigned int getInstanceSize() override { return sizeof(SplitNode); }
};
}
SplitNode::SplitNode() : m_child(nullptr) {}
SplitNode::~SplitNode() {}
unsigned int SplitNode::getObjectSize() { return sizeof(SplitNode); }
int SplitNode::getChildCount() { return 2; }
Node*& SplitNode::getChild(int index)
{ return index == 1 ? m_child : m_next; }
void SplitNode::setChild(int index, Node* child)
{
    if (index == 0) m_next = child;
    else if (index == 1) m_child = child;
}
bool SplitNode::streamProperties(System* system, Stream* stream)
{
    if (!Node::streamProperties(system, stream)) return false;
    return stream->streamNode0x84(system, m_child, "Split node", nullptr) != false;
}
void SplitNode::visitChildren(System* system, NodeState* state,
                              const Rect* rect, NodeVisitor* visitor)
{
    visitor->traverse0x08(system, m_child, rect, state);
    visitor->visit(system, m_next, rect, state);
}
void SplitNode::paint(System* system, const NodeState* state, const Rect* bounds,
                      const Rect* clip, RenderContext* context)
{
    if (m_child) m_child->paint(system, state, bounds, clip, context);
    Node::paint(system, state, bounds, clip, context);
}
bool SplitNode::update(System* system, const NodeState* state,
                       const Rect* bounds, float delta)
{
    if (m_child && !m_child->update(system, state, bounds, delta)) return false;
    return Node::update(system, state, bounds, delta);
}
void SplitNode::reset()
{
    if (m_child) m_child->reset();
    Node::reset(); // Actual Node+C4 dispatch remains the existing named frontier.
}

TypeDescriptor* SplitNode::getClassType()
{
    static SplitNodeTypeDescriptor type;
    return &type;
}
TypeDescriptor* SplitNode::getType() { return getClassType(); }
namespace {
// Eager initializer871650 shares the lazy descriptor, registering before lookup.
TypeDescriptor* const registeredSplitNodeType = SplitNode::getClassType();
}
#if defined(_M_IX86)
static_assert(sizeof(SplitNode) == 12, "client BFMeme SplitNode");
static_assert(sizeof(SplitNodeTypeDescriptor) == 16, "client SplitNode metadata");
#endif
} }
#endif
