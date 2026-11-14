// Slider with fixed-range values and script-visible properties.
#ifndef MENU_BFFIXEDSLIDERNODE_H
#define MENU_BFFIXEDSLIDERNODE_H
#include "../External/BFMeme/SliderNode.h"
#if !DEDICATED
#include <cstdint>
namespace dice { namespace meme {

class BfFixedSliderNode : public SliderNode
{
public:
    BfFixedSliderNode();
    // No destructor override: slot +00 is still Node's own 835660.
    TypeDescriptor* getType() override;
    unsigned int getObjectSize() override;
    bool streamProperties(System*, Stream*) override;     // 7D3D50
    Node*& getChild(int index) override;
    void setChild(int index, Node* child) override;
    void visitChildren(System*, NodeState*, const Rect*, NodeVisitor*) override;
    void paint(System*, const NodeState*, const Rect*, const Rect*, RenderContext*) override; // 7D4920
    bool update(System*, const NodeState*, const Rect*, float) override; // 7D4A30
    bool updateSelf(System*, const NodeState*, const Rect*, float) override; // 7D4250
    bool onEvent1(System*, const Rect*, const NodeState*, int, int, int) override; // 7D40A0
    bool onEvent2(System*, const Rect*, const NodeState*, int, int) override; // 7D4430
    bool onEvent3(System*, const Rect*, const NodeState*, int, int) override; // 7D4430, same body
    // getChildCount is not overridden here: SliderNode's shared 828A70 stands.
    static TypeDescriptor* getClassType();                // 7D4850, metadata939BE8

    // Public: fixtures reach them, and MSVC mangles member access into the name.
    float thumbExtent(const float* width) const;          // 7D3E00
    void syncFromData(System*, const float* width);        // 7D3E40

protected:

    friend struct BfFixedSliderNodeFixture;
    friend class Bf2FixedSliderNode;
    // The same block as SliderNode's, one level down, plus m_step. Names
    // [INFERRED]; the constructor's values and every offset are exact.
    float m_grabOffset;        // +2C, ctor 0
    float m_thumbOffset;       // +30, ctor 0; added to bounds.x for the thumb
    float m_minimum;           // +34, ctor 0   -> the create row's 1st float
    float m_maximum;           // +38, ctor 1.0 -> the create row's 2nd float
    float m_thumbSpan;         // +3C, ctor 1.0 -> the create row's 3rd float
    float m_step;              // +40, ctor 1.0 -> the create row's 4th float
    Data* m_data;              // +44, ctor null; written through +C8
    Node* m_child1;            // +48, ctor null; the thumb, child index 1
    NodeState m_state;         // +4C/+50, ctor {1.0, 1.0}
    float m_releaseTimer;      // +54, ctor 0; seconds a press has been held, read against 0.25
                               // by updateSelf's missed-release check (7D43BF)
};
#if defined(_M_IX86)
static_assert(sizeof(BfFixedSliderNode) == 88, "retail and native BfFixedSliderNode storage");
#endif
} }
#endif
#endif
