// Library slider node. HUD sliders use Bf2FixedSlider instead.
// Preserve the inherited 50-slot virtual order; this class adds no new slots. [INFERRED] Source placement.
#ifndef BFMEME_SLIDERNODE_H
#define BFMEME_SLIDERNODE_H
#include "Node.h"
#if !DEDICATED
namespace dice { namespace meme {
class Data;

class SliderNode : public Node
{
public:
    SliderNode();
    // No destructor override: slot +00 is Node's own 835660. The class owns
    // nothing, so nothing is freed.
    TypeDescriptor* getType() override;
    unsigned int getObjectSize() override;
    bool streamProperties(System*, Stream*) override;     // 833660 [FRONTIER]
    int getChildCount() override;                         // 828A70, the shared 2
    Node*& getChild(int index) override;
    void setChild(int index, Node* child) override;
    void visitChildren(System*, NodeState*, const Rect*, NodeVisitor*) override;
    void paint(System*, const NodeState*, const Rect*, const Rect*, RenderContext*) override; // 833B80 [FRONTIER]
    bool update(System*, const NodeState*, const Rect*, float) override; // 833C90 [FRONTIER]
    bool updateSelf(System*, const NodeState*, const Rect*, float) override; // 833810 [FRONTIER]
    bool onEvent1(System*, const Rect*, const NodeState*, int, int, int) override; // 833900 [FRONTIER]
    bool onEvent2(System*, const Rect*, const NodeState*, int, int) override; // 833A30 [FRONTIER]
    bool onEvent3(System*, const Rect*, const NodeState*, int, int) override; // 833A30, same body
    static TypeDescriptor* getClassType();

protected:
    friend struct SliderNodeFixture;
    // Names [INFERRED] from visitChildren 833AF0 and the BfFixedSliderNode
    // clone's drag arithmetic (7D4250); the constructor's values are exact.
    float m_grabOffset;        // +08, ctor 0
    float m_thumbOffset;       // +0C, ctor 0; added to bounds.x for the thumb
    float m_minimum;           // +10, ctor 0
    float m_maximum;           // +14, ctor 1.0
    float m_thumbSpan;         // +18, ctor 1.0; divided by |max - min|
    Data* m_data;              // +1C, ctor null
    Node* m_child1;            // +20, ctor null; the thumb, child index 1
    NodeState m_state;         // +24/+28, ctor {1.0, 1.0}
};
#if defined(_M_IX86)
static_assert(sizeof(SliderNode) == 44, "retail and native SliderNode storage");
#endif
} }
#endif
#endif
