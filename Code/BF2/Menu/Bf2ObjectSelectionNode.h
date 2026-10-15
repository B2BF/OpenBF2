// Target-selection overlay for guided weapons.
#ifndef MENU_BF2OBJECTSELECTIONNODE_H
#define MENU_BF2OBJECTSELECTIONNODE_H
#include "Bf2TransformNode.h"
#if !DEDICATED
#include <cstdint>
namespace dice { namespace meme {
class Bf2ObjectSelectionNode : public Bf2TransformNode {
public:
    // The shipped constructor takes only a name: it builds its base with five
    // zeros and then routes the name through Bf2TransformNode::setName.
    // The default is [INFERRED] from factory7CBF30.
    explicit Bf2ObjectSelectionNode(const char* name = nullptr);
    ~Bf2ObjectSelectionNode() override;
    TypeDescriptor* getType() override;
    unsigned int getObjectSize() override;
    // +64 7CBC40: keeps the HUD pointer inside the box in HUD modes 0 and 10.
    bool updateSelf(System*, const NodeState*, const Rect*, float) override;
    // +74 7CBE30: a press in mode 0 aims the active weapon at the world point under
    // the pointer (IWeaponObject +0x12C setTargetDirection). Always answers false.
    bool onEvent1(System*, const Rect*, const NodeState*, int, int, int) override;
    static TypeDescriptor* getClassType();               // 7CBF80, metadata937FB4

    // [CHOSEN, not recovered] Sets pointer dimensions from floats through native dispatch.
    // [RETIRE-WHEN: bf2-picture-native-accessors] Replace after the corresponding Node virtual slots are typed and validated.
    bool setPointerSizeForHud(float width, float height); // 7CBDA0, +D8

protected:
    friend struct Bf2ObjectSelectionNodeFixture;
    // Neither behaviour body reads the pointer size: updateSelf insets by HudAmmo's
    // crosshair size (+0xA0) instead. No reader of +3C/+40 has been found.
    float m_pointerWidth;                // +3C, starts 32
    float m_pointerHeight;               // +40, starts 32
    std::uint32_t m_field044;            // +44 [UNRESOLVED], zeroed
};
#if defined(_M_IX86)
static_assert(sizeof(Bf2ObjectSelectionNode) == 72, "native Bf2ObjectSelectionNode storage");
#endif
} }
#endif
#endif
