// HUD compass node. [INFERRED] Header placement. Higher operations use native accessors until the Node interface is complete.
#ifndef MENU_BF2COMPASSNODE_H
#define MENU_BF2COMPASSNODE_H
#include "Bf2BarNode.h"
#include "../External/BFMeme/Style.h"
#if !DEDICATED
namespace dice { namespace meme {
class Bf2CompassNode : public Bf2BarNode {
public:
    // Defaults [INFERRED] from factory7D01D0, which supplies (null, 3, true, false).
    explicit Bf2CompassNode(const char* name = nullptr, int direction = 3,
                            bool snap = true, bool wrap = false);
    ~Bf2CompassNode() override;
    TypeDescriptor* getType() override;
    unsigned int getObjectSize() override;
    void paintSelf(System*, const NodeState*, const Rect*, const Rect*, RenderContext*) override;
    static TypeDescriptor* getClassType();               // 7D0610, metadata938C28

    // [CHOSEN, not recovered] Native compass property access.
    // [RETIRE-WHEN: bf2-picture-native-accessors] Replace after the corresponding Node virtual slots are typed and validated.
    bool setTextureSizeForHud(float width, float height);
    bool setOffsetForHud(int offset);                     // 7D00D0, +10C
    bool setSnapOffsetForHud(int a, int b, int c, int d);
    bool setSnapTextureForHud(bool primary, TextureHandle texture);
protected:
    // Member names [INFERRED] from paintSelf. The first five words are a Rect
    // whose origin setTextureSize always clears.
    Rect m_textureRect;             // +90..+A0
    TextureHandle m_snapTexturePrimary;   // +A4, drawn when the leading limit is hit
    TextureHandle m_snapTextureSecondary; // +A8, drawn when the trailing limit is hit
    int m_offset;                   // +AC, added to the scrolled position
    int m_snapOffset[4];            // +B0..+BC; [1]/[0] vertical, [2]/[3] horizontal
    bool m_snapEnabled;             // +C0
    bool m_wrapEnabled;             // +C1
    unsigned char m_paddingc2[2];   // untouched
};
#if defined(_M_IX86)
static_assert(sizeof(Bf2CompassNode) == 196, "native Bf2CompassNode storage");
#endif
} }
#endif
#endif
