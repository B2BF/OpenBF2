// Path [INFERRED]. Full Node table94A0F0 and every target's instructions read;
// only the evidence-complete callable prefix is emitted. The complete eight-slot
// descriptor and factory are available; missing suffix calls stop at their known
// callers.
#include "Node.h"
#if !DEDICATED
#include "Stream.h"
#include "TypeDescriptor.h"
// Concrete menu node types permit event-20 dispatch while the shared Node suffix remains untyped.
#include "../../Menu/Bf2ButtonNode.h"
#include "../../Menu/Bf2ObjectMarkerNode.h"
#include <cstdio>
#include <cstdlib>
#include <new>
namespace dice { namespace meme {
namespace {
// PE descriptor table94A0CC, static counterA39768, storageA3976C. Metadata
// publication does not add the unknown Node+84/+88 virtual declarations.
class NodeTypeDescriptor : public TypeDescriptor
{
public:
    NodeTypeDescriptor() : TypeDescriptor("dice::meme::Node") {}
    TypeDescriptor* getBaseType() override { return Object::getClassType(); }
    RuntimeObject* create() override
    {
        // [CHOSEN, not recovered] [RETIRE-WHEN: node-native-prefix-publication]
        // Publishes the known native prefix; unsupported suffix calls retain diagnostic stops.
        void* allocation = ::operator new(sizeof(Node));
        return allocation ? new(allocation) Node : nullptr;
    }
    const char* getDescription() override { return nullptr; }
    void addInstance() override { ++instanceCount; }
    void removeInstance() override { --instanceCount; }       // 82C140, no zero guard
    int getInstanceCount() override { return instanceCount; }
    unsigned int getInstanceSize() override { return sizeof(Node); }
private:
    static int instanceCount;
};
int NodeTypeDescriptor::instanceCount = 0;
#if defined(_M_IX86)
static_assert(sizeof(NodeTypeDescriptor) == 16, "Node metadata stores its counter separately");
#endif
}

TypeDescriptor* Node::getClassType() // 82C190, shared by the virtual getter
{
    static NodeTypeDescriptor type;
    return &type;
}
TypeDescriptor* Node::getType() { return getClassType(); }
TypeDescriptor* Node::getChildType(int) { return getClassType(); } // 82C1D0, index ignored
TypeDescriptor* getNodeClassType() { return Node::getClassType(); }
namespace {
// CRT96E098 ->86FEF0 constructs this same descriptor and stores its address at
// Keep eager discovery as well as the native function-local lifetime.
TypeDescriptor* const registeredNodeType = Node::getClassType();
}

NodeVisitor::NodeVisitor(int mode) : m_mode(mode) {}
void NodeVisitor::traverse(System* system, Node*& node, const Rect* rect, NodeState* state)
{
    visit(system, node, rect, state);
}
void NodeVisitor::visit(System*, Node*&, const Rect*, NodeState*) {}
void NodeVisitor::traverse0x08(System* system, Node*& node, const Rect* rect, NodeState* state)
{
    traverse(system, node, rect, state);
}

Node::Node() : m_next(nullptr) {}
Node::~Node() {}                                           // 835660, Object destructor only
unsigned int Node::getObjectSize() { return sizeof(Node); }
bool Node::streamProperties(System* system, Stream* stream)
{
    if (!Object::streamProperties(system, stream)) return false;
    return stream->streamNode0x88(system, m_next, "Next node", nullptr);
}
int Node::getChildCount() { return 1; }
Node*& Node::getChild(int) { return m_next; }                // 82BD70, index deliberately ignored
void Node::setChild(int, Node* child) { m_next = child; }    // 82BD80, no old-child destruction
void Node::visitChildren(System* system, NodeState* state, const Rect* rect, NodeVisitor* visitor)
{
    visitor->visit(system, m_next, rect, state);             // 82BC30, does not guard null next
}
void Node::paint(System* system, const NodeState* state, const Rect* rect,
                 const Rect* clip, RenderContext* context)
{
    paintSelf(system, state, rect, clip, context);
    if (m_next) m_next->paint(system, state, rect, clip, context);
}
bool Node::update(System* system, const NodeState* state, const Rect* rect, float delta)
{
    if (!updateSelf(system, state, rect, delta)) return false;
    if (m_next) return m_next->update(system, state, rect, delta);
    return true;
}
void Node::reset()
{
    resetNodeSelf(this);                                   // virtual+C4, recovered empty 83D270
    if (m_next) m_next->reset();                           // reload after self callback
}
NodeState* Node::getState() { return nullptr; }              // 80D430; derived getter evidence
bool Node::updateSelf(System*, const NodeState*, const Rect*, float) { return true; }
void Node::paintSelf(System*, const NodeState*, const Rect*, const Rect*, RenderContext*) {}
bool Node::dispatchEvent(System* system, const Rect* rect, const NodeState* state,
                         const NodeInputEvent* event)
{
    // The original has no null-event guard and never traverses m_next here.
    switch (event->type)
    {
    case 1:
    {
        const float secondValue = event->second;
        const int index = event->index;
        const float firstValue = event->first;
        const int second = static_cast<int>(secondValue);
        const int first = static_cast<int>(firstValue);
        return onEvent1(system, rect, state, first, index, second);
    }
    case 2:
    {
        const float firstValue = event->first;
        const int index = event->index;
        return onEvent2(system, rect, state, static_cast<int>(firstValue), index);
    }
    case 3:
    {
        const float firstValue = event->first;
        const int index = event->index;
        return onEvent3(system, rect, state, static_cast<int>(firstValue), index);
    }
    case 4:
    {
        const float secondValue = event->second;
        const int index = event->index;
        const float firstValue = event->first;
        const int second = static_cast<int>(secondValue);
        const int first = static_cast<int>(firstValue);
        return onEvent4(system, rect, state, first, index, second);
    }
    // The mapping is deliberately not numeric slot order. These typed calls
    // stop until +84/+88 is recovered, except the authorized native defaults.
    case 9:  return dispatchNodeEvent9(this, system, rect, state);
    case 10: return dispatchNodeEvent10(this, system, rect, state);
    case 11: return dispatchNodeEvent11(this, system, rect, state);
    case 12: return dispatchNodeEvent12(this, system, rect, state);
    case 15: return dispatchNodeEvent15(this, system, rect, state);
    case 16: return dispatchNodeEvent16(this, system, rect, state);
    case 13: return dispatchNodeEvent13(this, system, rect, state);
    case 17: return dispatchNodeEvent17(this, system, rect, state);
    case 18: return dispatchNodeEvent18(this, system, rect, state);
    case 14: return dispatchNodeEvent14(this, system, rect, state);
    case 7:  return dispatchNodeEvent7(this, system, rect, state);
    case 8:  return dispatchNodeEvent8(this, system, rect, state);
    case 21: return dispatchNodeEvent21(this, system, rect, state);
    case 20: return dispatchNodeEvent20(this, system, rect, state);
    default: return onEvent(system, event);                // includes5,6,19
    }
}
bool Node::onEvent(System*, const NodeInputEvent*) { return false; }
bool Node::onEvent1(System*, const Rect*, const NodeState*, int, int, int) { return false; }
bool Node::onEvent2(System*, const Rect*, const NodeState*, int, int) { return false; }
bool Node::onEvent3(System*, const Rect*, const NodeState*, int, int) { return false; }
bool Node::onEvent4(System*, const Rect*, const NodeState*, int, int, int) { return false; }

// [TEMP] [CHOSEN, not recovered] [RETIRE-WHEN: node-suffix-call-frontiers]
// Known caller adapters stop before unknown virtual access, except explicitly tracked native defaults.
// Replace with virtual calls when the complete suffix, including slots 0x84 and 0x88, is typed.
bool dispatchNodeEvent9(Node*, System*, const Rect*, const NodeState*)
{
    // [CHOSEN, not recovered] [RETIRE-WHEN: node-startup-event-native-defaults]
    // Current node classes return false for event 9; recheck before publishing a different handler.
    return false;
}

// Routes node events 10, 11, 12, 15 and 18 to their empty defaults. Event 16 remains a frontier.
// [CHOSEN, not recovered] Uses free functions while the virtual suffix is incomplete.
// [RETIRE-WHEN: node-suffix-call-frontiers] Replace routing after the full typed suffix, including slots 0x84 and 0x88, is known.

bool dispatchNodeEvent10(Node*, System*, const Rect*, const NodeState*)
{
    // +90: the fully read PE455410 false default in 74/74 tables. See the banner above.
    // [CHOSEN, not recovered] [RETIRE-WHEN: node-suffix-call-frontiers]
    return false;
}
bool dispatchNodeEvent11(Node*, System*, const Rect*, const NodeState*)
{
    // +94: the fully read PE455410 false default in 74/74 tables. See the banner above.
    // [CHOSEN, not recovered] [RETIRE-WHEN: node-suffix-call-frontiers]
    return false;
}
bool dispatchNodeEvent12(Node*, System*, const Rect*, const NodeState*)
{
    // +98: the fully read PE455410 false default in 74/74 tables. See the banner above.
    // [CHOSEN, not recovered] [RETIRE-WHEN: node-suffix-call-frontiers]
    return false;
}
bool dispatchNodeEvent15(Node*, System*, const Rect*, const NodeState*)
{
    // +9C: the fully read PE455410 false default in 74/74 tables. See the banner above.
    // [CHOSEN, not recovered] [RETIRE-WHEN: node-suffix-call-frontiers]
    return false;
}
bool dispatchNodeEvent16(Node*, System*, const Rect*, const NodeState*)
{
    // [FRONTIER] [TEMP] [CHOSEN, not recovered] Event 16 stops because edit and path nodes have nonempty handlers.
    // [RETIRE-WHEN: node-suffix-call-frontiers] Implement the typed virtual handlers before replacing this stop.
    std::fprintf(stderr, "[FRONTIER] dice::meme::dispatchNodeEvent16 reached (Node virtual+A0 - NOT a false default: 5 of 74 tables override it, BfEditNode/Bf2EditNode 7AA9B0, BfEditNodeInt 7AACF0, PathNode/StackPathNode 836A80)\n");
    std::fflush(stderr);
    std::abort();
}
bool dispatchNodeEvent13(Node*, System*, const Rect*, const NodeState*)
{
    // Published nodes return false for event 13; list-box factories with nonempty handlers remain withheld.
    // [CHOSEN, not recovered] Uses native default routing.
    // [RETIRE-WHEN: node-startup-event-native-defaults] Complete typed dispatch and slots 0x84 and 0x88 before publishing those factories.
    return false;
}
bool dispatchNodeEvent17(Node*, System*, const Rect*, const NodeState*)
{
    // Event 17 returns false for all currently supported node classes; recheck this when adding subclasses.
    // [CHOSEN, not recovered] Uses native default routing.
    // [RETIRE-WHEN: node-event17-native-default] Replace with typed virtual dispatch when its interface is known.
    return false;
}
bool dispatchNodeEvent18(Node*, System*, const Rect*, const NodeState*)
{
    // +AC: the fully read PE455410 false default in 74/74 tables. See the banner above.
    // [CHOSEN, not recovered] [RETIRE-WHEN: node-suffix-call-frontiers]
    return false;
}
bool dispatchNodeEvent14(Node*, System*, const Rect*, const NodeState*)
{
    // [CHOSEN, not recovered] [RETIRE-WHEN: node-startup-event-native-defaults]
    // Current node classes return false for this event; recheck before publishing a different handler.
    return false;
}
bool dispatchNodeEvent7(Node*, System*, const Rect*, const NodeState*)
{
    // [CHOSEN, not recovered] [RETIRE-WHEN: node-startup-event-native-defaults]
    // Event 7 returns false for current node classes; recheck before publishing a different handler.
    return false;
}
bool dispatchNodeEvent8(Node*, System*, const Rect*, const NodeState*)
{
    // [CHOSEN, not recovered] [RETIRE-WHEN: node-startup-event-native-defaults]
    // Event 8 returns false for current node classes; recheck before publishing a different handler.
    return false;
}
bool dispatchNodeEvent21(Node*, System*, const Rect*, const NodeState*)
{
    // Event 21 returns false for all currently supported node classes; recheck this when adding subclasses.
    // [CHOSEN, not recovered] Uses native default routing.
    // [RETIRE-WHEN: node-startup-event-native-defaults] Replace with the complete typed virtual interface.
    return false;
}

// Dispatches the hide event using exact type identity until the suffix is typed.
// Marker and button nodes handle it and stop child traversal; other types return false.
// Requires a live node. Revisit dispatch before subclassing either handled type.
// [RETIRE-WHEN: node-suffix-call-frontiers] Replace with virtual dispatch once +84/+88
// are typed and the Node suffix can be declared contiguously.

bool dispatchNodeEvent20(Node* node, System*, const Rect*, const NodeState*)
{
    TypeDescriptor* const type = node->getType();
    if (type == Bf2ObjectMarkerNode::getClassType())
        return static_cast<Bf2ObjectMarkerNode*>(node)->clearShowFlagsForHud();
    if (type == Bf2ButtonNode::getClassType())
        return static_cast<Bf2ButtonNode*>(node)->releasePointerForHud();
    return false;                       // the 455410 default the other 72 tables carry
}
// Resets this node using the empty default shared by all currently supported node classes.
// Add typed virtual dispatch before publishing a subclass that overrides reset behavior.
void resetNodeSelf(Node*)
{
}

#if defined(_M_IX86)
static_assert(sizeof(Node) == 8, "client BFMeme Node");
#endif
static_assert(sizeof(NodeState) == 8, "client BFMeme interaction state");
static_assert(sizeof(NodeInputEvent) == 20, "client BFMeme input payload");
#if defined(_M_IX86)
static_assert(sizeof(RenderContext) == 28, "client BFMeme effect chain context");
static_assert(sizeof(NodeVisitor) == 8, "client BFMeme three-slot visitor");
#endif
} }
#endif
