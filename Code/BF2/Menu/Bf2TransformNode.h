// HUD node applying position and bounds to child presentation. [INFERRED] Source placement.
// Native accessors represent higher methods where the base interface is incomplete.
#ifndef MENU_BF2TRANSFORMNODE_H
#define MENU_BF2TRANSFORMNODE_H
#include "../External/BFMeme/TransformNode.h"
#if !DEDICATED
namespace dice { namespace meme {
class Bf2TransformNode : public TransformNode {
public:
    // TransformNode(), then the owned name clone and the four fild'd
    // signed geometry arguments into the inherited position/size.
    Bf2TransformNode(const char* name, int x, int y, int width, int height);
    ~Bf2TransformNode() override;                         // 7C1BD0, frees the name
    TypeDescriptor* getType() override;
    unsigned int getObjectSize() override;
    const char* getName() override;
    bool setName(const char*) override;
    bool slot_0x24() override;                            // 5A5E20, true
    bool update(System*, const NodeState*, const Rect*, float) override;
    static TypeDescriptor* getClassType();

    // [CHOSEN, not recovered] Native transform property access.
    // [RETIRE-WHEN: bf2-picture-native-accessors] Replace after the corresponding Node virtual slots are typed and validated.
    bool setSizeForHud(float width, float height);        // 7C1C00, +C8
    bool setPositionForHud(float x, float y);             // 7C1C20, +CC
    // Index 0 binds x and every other index binds y; a null pointer is the one
    // refusal. The pointee is an int - 7C1D90 FILDs it into the float position.
    bool setPositionBindingForHud(int index, int* value);
    bool setOffsetForHud(int x, int y);                   // 77A8B0, +D4
protected:
    char* m_name;                                        // +24 owned narrow clone
    std::uint32_t m_field028;                             // initialized0, identity unresolved
    // +2C/+30: two signed integers (7C1D90 FILDs both before adding them to the
    // float position; the map's +D4 slot77A8B0 stores two dwords here).
    int m_offsetX;                                       // +2C
    int m_offsetY;                                       // +30
    // +34/+38 are INT pointers, not float pointers:7C1D90 reads each with
    // `fild dword ptr [eax]` before storing the result into the float position.
    int* m_positionX;                                    // +34 borrowed optional live position
    int* m_positionY;                                    // +38 borrowed optional live position
};
#if defined(BF2_IMAGE_EDITOR) && BF2_IMAGE_EDITOR
// [CHOSEN, not recovered] The editor's HUD editor (Tools/BF2Editor/App/HudEditor): told, on each update, where a
// script-made node stands on the HUD canvas - x, y, width and height, its parents' positions included. Editor
// image only; null when nothing listens.
extern void (*editorHudNodeUpdated)(Bf2TransformNode* node, float x, float y, float width, float height);
#endif
} }
#endif
#endif
