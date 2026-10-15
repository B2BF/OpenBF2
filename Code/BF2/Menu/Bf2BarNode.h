// HUD bar node. [INFERRED] Header placement. Higher operations use native accessors until the Node interface is complete.
#ifndef MENU_BF2BARNODE_H
#define MENU_BF2BARNODE_H
#include "Bf2PictureNode.h"
#if !DEDICATED
namespace dice { namespace meme {
class Bf2BarNode : public Bf2PictureNode {
public:
    // Defaults [INFERRED] from factory7D13C0, which supplies (null, 3).
    explicit Bf2BarNode(const char* name = nullptr, int direction = 3);
    ~Bf2BarNode() override;
    TypeDescriptor* getType() override;
    unsigned int getObjectSize() override;
    bool updateSelf(System*, const NodeState*, const Rect*, float) override;
    void paintSelf(System*, const NodeState*, const Rect*, const Rect*, RenderContext*) override;
    static TypeDescriptor* getClassType();               // 7D1800, metadata939078

    // [CHOSEN, not recovered] Native access to indexed textures.
    // [RETIRE-WHEN: bf2-picture-native-accessors] Replace after the corresponding Node virtual slots are typed and validated.
    bool setIndexedTextureNameForHud(int index, const char* name);        // 7D10F0, +EC
    bool setIndexedTextureBindingForHud(int index, const char** binding);  // 7D1140, +F0
    bool setValueBindingForHud(float* value);                            // 7D1380, +F4
    void setDirectionForHud(int direction);                              // 4B3A40, +F8
    bool setFillBorderForHud(int top, int bottom, int left, int right);   // 7D0070, +FC
    bool setSnapForHud(int snap);
    bool setSnapDirectionForHud(bool towardsEnd);
protected:
    // Member names [INFERRED] from the update/paint bodies.
    TextureHandle m_fillTexture;    // +60, loaded from the second name pair
    int m_fillTop, m_fillBottom;    // +64/+68, inset applied to the fill rect
    int m_fillLeft, m_fillRight;    // +6C/+70
    int m_snap;                     // +74, quantiser for the fill length
    bool m_snapTowardsEnd;          // +78, ceil instead of floor in direction 0
    unsigned char m_padding79[3];   // untouched
    // The "not bound" marker for both texture pairs is the picture's own +28.
    const char** m_fillTextureBinding;   // +7C
    char* m_fillRequestedTextureName;    // +80, owned or the shared unset marker
    char* m_fillPreviousTextureName;     // +84, owned or the shared unset marker
    float* m_value;                 // +88, defaults to a shared zero float
    int m_direction;                // +8C, 0 up, 1 down, 2 left, 3 right
};
#if defined(_M_IX86)
static_assert(sizeof(Bf2BarNode) == 144, "native Bf2BarNode storage");
#endif
} }
#endif
#endif
