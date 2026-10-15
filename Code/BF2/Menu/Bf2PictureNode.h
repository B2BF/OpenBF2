// HUD picture node. [INFERRED] Header placement. Higher operations use native accessors until Node slots are typed.
#ifndef MENU_BF2PICTURENODE_H
#define MENU_BF2PICTURENODE_H
#include "../External/BFMeme/PictureNode.h"
#if !DEDICATED
namespace dice { namespace meme {
class Bf2PictureNode : public PictureNode {
public:
    // Default [INFERRED] from factory7AD010, which supplies a null name.
    explicit Bf2PictureNode(const char* name = nullptr);
    ~Bf2PictureNode() override;
    TypeDescriptor* getType() override;
    unsigned int getObjectSize() override;
    const char* getName() override;
    bool setName(const char*) override;
    bool slot_0x24() override;
    bool updateSelf(System*, const NodeState*, const Rect*, float) override;
    void paintSelf(System*, const NodeState*, const Rect*, const Rect*, RenderContext*) override;
    static TypeDescriptor* getClassType();               // 7ACEF0, metadata935108

    // [CHOSEN, not recovered] Native event-routing accessors.
    // [RETIRE-WHEN: bf2-picture-native-accessors] Replace them when the corresponding Node virtual methods are typed and validated.
    bool setTextureNameForHud(const char* name);         // 7AC810, +C8
    bool setTextureForHud(TextureHandle texture);        // 7CDFC0, +CC
    bool setSecondaryTextureForHud(TextureHandle texture);
    bool setTextureNameBindingForHud(const char** binding); // 7AC850, +D4
    bool setRotationBindingForHud(const float* binding);  // 7ACA30, +D8
    bool setAdditionalRotationForHud(float degrees);      // 7ACA40, +DC
    bool setCenterPointForHud(int x, int y);             // 7ACA50, +E0
    bool setBorderForHud(int left, int right, int top, int bottom); // 7AC7E0, +E4
    bool setBorderColorForHud(float r, float g, float b, float a);  // 7ACA80, +E8
protected:
    bool m_centerSpecified;                             // +0C
    float m_centerX, m_centerY;                          // +10/+14
    char* m_name;                                       // +18, owned scalar-allocation clone
    const float* m_rotationBinding;                     // +1C, default &m_rotation
    float m_rotation, m_additionalRotation;              // +20/+24
    const char* m_textureName;                           // +28, borrowed empty byte string
    const char** m_textureNameBinding;                   // +2C, default &m_textureName
    char* m_requestedTextureName;                        // +30, owned or identity sentinel
    char* m_previousTextureName;                         // +34, owned or identity sentinel
    TextureHandle m_secondaryTexture;                   // +38
    bool m_borderEnabled;                               // +3C
    int m_borderLeft, m_borderRight, m_borderTop, m_borderBottom; // +40..4C; fild in7ACAB0
    float m_borderRed, m_borderGreen, m_borderBlue, m_borderAlpha; // +50..5C
};
#if defined(_M_IX86)
static_assert(sizeof(Bf2PictureNode)==96, "native Bf2PictureNode storage");
#endif

// 87FB54, the shared empty literal the constructor stores at +28. Bf2BarNode's
// indexed setter compares its argument against this address, so both classes
// must see the same object; name [INFERRED].
extern const char* const emptyHudTextureName;

// Sets the picture texture through the shared native accessor.
bool dispatchPictureTexture(Bf2PictureNode*, TextureHandle texture);
} }
#endif
#endif
