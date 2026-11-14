// Path [INFERRED] beside the attested BFMeme sources; class identity is the
// descriptor literal "dice::meme::Node" at client PE82C190.
#ifndef BFMEME_NODE_H
#define BFMEME_NODE_H
#include "Object.h"
#if !DEDICATED
#include <cstdint>
namespace dice { namespace meme {
struct Rect;
class Effect;
class Node;

// [INFERRED] names. This is interaction state, not geometric scale:838FB0
// interpolates the second float toward the first and encodes flags by sign.
// TranslateNode833590 and DisableNode82FB90 initialize both floats to1.
struct NodeState { float target, current; };

// [INFERRED] name/field names; the existing Style RenderContext forward names
// this same seven-word effect chain.82D730/82D790 follow parent and invoke
// Effect+4C/+50. PathNode836710 does float arithmetic with+08/+0C;82D340
// stores a float-returning helper at+10 and decoded NodeState at+14;
// ShowEffectNode82C4E0 copies float+0C into+18. EffectNode82C240 mutates effect.
struct RenderContext
{
    RenderContext* parent;
    Effect* effect;
    float field08, field0c, field10, field14, field18;
};

// [INFERRED] plain input payload; distinct from descriptor-backed meme::Event.
// Node82BDE0 reads type/index and both float values;838DD0 copies all five words.
struct NodeInputEvent
{
    int type, index;
    float first, second;
    std::uint32_t field10; // [UNRESOLVED] fifth payload word
};

// Complete three-slot table94ED3C. It has no virtual destructor. Both wrapper
// slots forward all four arguments; the actual base visit hook is empty.
class NodeVisitor
{
public:
    // State is mutable: input visitors83B460/83B500 write its target directly.
    explicit NodeVisitor(int mode);
    virtual void traverse(System*, Node*&, const Rect*, NodeState*);
    virtual void visit(System*, Node*&, const Rect*, NodeState*);
    virtual void traverse0x08(System*, Node*&, const Rect*, NodeState*);
protected:
    int m_mode;                                           // +04, identity [INFERRED]
};

// Base menu node with the known 33-slot prefix of a 50-slot interface.
// [UNRESOLVED] Argument types for slots 0x84 and 0x88; do not invent declarations for them.
// Method names and reference-versus-pointer choices are [INFERRED]; event 16 remains a frontier.
class Node : public Object
{
public:
    Node();
    ~Node() override;                                     // 835660, does not own next
    TypeDescriptor* getType() override;
    unsigned int getObjectSize() override;
    bool streamProperties(System*, Stream*) override;
    virtual int getChildCount();
    virtual Node*& getChild(int index);
    virtual TypeDescriptor* getChildType(int index);        // +48 82C1D0, ignores index
    virtual void setChild(int index, Node* child);          // +4C 82BD80
    virtual void visitChildren(System*, NodeState*, const Rect*, NodeVisitor*);
    virtual void paint(System*, const NodeState*, const Rect* bounds, const Rect* clip, RenderContext*);
    virtual bool update(System*, const NodeState*, const Rect*, float delta);
    virtual void reset();                                  // +5C 82BD10; named+C4 frontier
    virtual NodeState* getState();
    virtual bool updateSelf(System*, const NodeState*, const Rect*, float delta);
    virtual void paintSelf(System*, const NodeState*, const Rect* bounds, const Rect* clip, RenderContext*);
    virtual bool dispatchEvent(System*, const Rect*, const NodeState*, const NodeInputEvent*); // +6C 82BDE0; suffix frontiers below
    virtual bool onEvent(System*, const NodeInputEvent*);
    virtual bool onEvent1(System*, const Rect*, const NodeState*, int first, int index, int second);
    virtual bool onEvent2(System*, const Rect*, const NodeState*, int first, int index);
    virtual bool onEvent3(System*, const Rect*, const NodeState*, int first, int index); // +7C 82C120
    virtual bool onEvent4(System*, const Rect*, const NodeState*, int first, int index, int second);
    static TypeDescriptor* getClassType();                 // 82C190, native prefix factory82C170
protected:
    Node* m_next;                                          // +04, no ctor/destructor recursion
};

// [FRONTIER] [TEMP] [CHOSEN, not recovered] Native routing while the virtual suffix is incomplete.
// Event 16 aborts; supported empty handlers retain their defaults without bypassing dispatch or traversal.
// Replace these helpers once the complete typed virtual interface is available.
bool dispatchNodeEvent9(Node*, System*, const Rect*, const NodeState*);  // +8C
bool dispatchNodeEvent10(Node*, System*, const Rect*, const NodeState*); // chosen native default for +90; 74/74 PE455410
bool dispatchNodeEvent11(Node*, System*, const Rect*, const NodeState*); // chosen native default for +94; 74/74 PE455410
bool dispatchNodeEvent12(Node*, System*, const Rect*, const NodeState*); // chosen native default for +98; 74/74 PE455410
bool dispatchNodeEvent15(Node*, System*, const Rect*, const NodeState*); // chosen native default for +9C; 74/74 PE455410
bool dispatchNodeEvent16(Node*, System*, const Rect*, const NodeState*); // +A0 - 455410 false default, BfEditNode/Bf2EditNode 7AA9B0
bool dispatchNodeEvent13(Node*, System*, const Rect*, const NodeState*); // +A4
bool dispatchNodeEvent17(Node*, System*, const Rect*, const NodeState*); // chosen native default for +A8; no virtual suffix access
bool dispatchNodeEvent18(Node*, System*, const Rect*, const NodeState*); // chosen native default for +AC; 74/74 PE455410
bool dispatchNodeEvent14(Node*, System*, const Rect*, const NodeState*); // +B0
bool dispatchNodeEvent7(Node*, System*, const Rect*, const NodeState*);  // +B4
bool dispatchNodeEvent8(Node*, System*, const Rect*, const NodeState*);  // +B8
bool dispatchNodeEvent21(Node*, System*, const Rect*, const NodeState*); // +BC
bool dispatchNodeEvent20(Node*, System*, const Rect*, const NodeState*); // +C0
void resetNodeSelf(Node*);                                             // +C4 written; retail 83D270 is a bare ret
} }
#endif
#endif
