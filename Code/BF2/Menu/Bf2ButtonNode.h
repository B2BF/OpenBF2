// Button node with separate press and release actions.
#ifndef MENU_BF2BUTTONNODE_H
#define MENU_BF2BUTTONNODE_H
#include "Bf2PictureNode.h"
#if !DEDICATED
#include <cstdint>
#include "../Math/Vec3.h"             // hfe::Vec2, returned by value from +10C/+110
#include "../External/BFMeme/Quad.h"   // Color, the +104 slot's by-value argument
namespace dice { namespace meme {
class Action;

class Bf2ButtonNode : public Bf2PictureNode {
public:
    // x and y are accepted and ignored - only width and height are fild'd,
    // into the mouse area at +98/+9C. Defaults [INFERRED] from factory7D0CB0,
    // which supplies a null name and 20 for all four integers.
    Bf2ButtonNode(const char* name = nullptr, int x = 20, int y = 20,
                  int width = 20, int height = 20);
    ~Bf2ButtonNode() override;
    TypeDescriptor* getType() override;
    unsigned int getObjectSize() override;
    bool updateSelf(System*, const NodeState*, const Rect*, float) override; // 7D06C0 [FRONTIER]
    void paintSelf(System*, const NodeState*, const Rect*, const Rect*, RenderContext*) override; // 7D0D20 [FRONTIER]
    bool onEvent1(System*, const Rect*, const NodeState*, int first, int index, int second) override;
    bool onEvent2(System*, const Rect*, const NodeState*, int first, int index) override;
    bool onEvent3(System*, const Rect*, const NodeState*, int first, int index) override;
    static TypeDescriptor* getClassType();               // 7D0CE0, metadata938C64

    // [CHOSEN, not recovered] Nonvirtual accessors used by native event routing.
    // The routing checks exact node types; review dispatchNodeEvent20 before deriving another button type.
    // [RETIRE-WHEN: bf2-picture-native-accessors] Replace these accessors after the corresponding Node virtual methods are typed and validated.
    bool releasePointerForHud();                          // 7D0A10, +C0
    // Index 0 and any index above 3 are refused, and so is a zero texture; the
    // shipped body also returns the index's low byte rather than a literal 1 on
    // the index-1 arm, which happens to be the same value.
    bool setTextureForHud(int index, TextureHandle texture); // 7D0650, +EC
    bool setActionForHud(Action* action, int index);      // 7D0B50, +F0
    bool setAltActionForHud(Action* action, int index);   // 7D0B70, +F4
    Action* getActionForHud(int index);                   // 7D0B90, +F8
    Action* getAltActionForHud(int index);                // 7D0BA0, +FC
    // crossed stores: width/height land at +98/+9C before x/y land at +90/+94.
    bool setMouseAreaForHud(int x, int y, int width, int height);
    bool setMouseOverColorForHud(Color color);
    bool setDebugForHud(bool enabled);
    hfe::Vec2 getMouseAreaPositionForHud();               // 7D0C30, +10C
    hfe::Vec2 getMouseAreaSizeForHud();

protected:
    friend struct Bf2ButtonNodeFixture;
    // The three texture handles the console row selects by index, and the one
    // the paint path actually uses. Index 1 also writes PictureNode::m_texture.
    TextureHandle m_normalTexture;                       // +60, index 1
    TextureHandle m_pressedTexture;                      // +64, index 2 [INFERRED] role
    TextureHandle m_highlightTexture;                    // +68, index 3 [INFERRED] role
    TextureHandle m_currentTexture;                      // +6C
    // Four action slots and four alternate action slots. The console rows accept
    // an index below 4, which is exactly these four entries.
    Action* m_actions[4];                                // +70..7C
    Action* m_altActions[4];                             // +80..8C
    hfe::Vec2 m_mouseAreaPosition;                       // +90/+94, zeroed twice
    hfe::Vec2 m_mouseAreaSize;                           // +98/+9C, the ctor's w/h
    Color m_mouseOverColor;                              // +A0..AC
    bool m_mouseOverColorSet;                            // +B0, set by +104
    bool m_debug;                                        // +B1, set by +108
    bool m_pointerHeld;                                  // +B2 [INFERRED] role, cleared by +C0
    bool m_pointerReleased;                              // +B3 [INFERRED] role, ctor true
    std::uint32_t m_fieldb4, m_fieldb8;                  // [UNRESOLVED], never written
    bool m_fieldbc;                                      // +BC [UNRESOLVED] role, ctor false
};
#if defined(_M_IX86)
static_assert(sizeof(Bf2ButtonNode) == 192, "native Bf2ButtonNode storage");
#endif
} }
#endif
#endif
