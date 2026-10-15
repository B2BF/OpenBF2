// Effect interface with blend-function overrides. Bf2BlendFuncEffect adds two virtual slots.
#ifndef BFMEME_BLENDFUNCEFFECT_H
#define BFMEME_BLENDFUNCEFFECT_H
#include "Effect.h"
#if !DEDICATED
namespace dice { namespace meme {
class Stream;

// The enum both properties stream through. Names are the shipped strings; the
// spellings blendFuncName/blendFuncNext are [INFERRED]. 800AA0 sits in the same
// text neighbourhood as rotateQuad (800BE0), so the shipped definition probably
// lives in the shared drawing translation unit; it is kept beside its only
// reader here. Value0 is the out-of-range name the table itself publishes.
const char* __fastcall blendFuncName(int value);
int __fastcall blendFuncNext(int value);

class BlendFuncEffect : public Effect
{
public:
    // Defaults [INFERRED] from factory839720, which supplies7 and8 - source
    // alpha and one minus source alpha, the ordinary alpha blend.
    BlendFuncEffect(int source = 7, int destination = 8);
    ~BlendFuncEffect() override;                          // 835660, owns nothing
    TypeDescriptor* getType() override;
    unsigned int getObjectSize() override;
    bool streamProperties(System*, Stream*) override;
    bool drawCharacter(System*, int character, FontHandle font, int elementIndex,
                       Quad*, RenderContext*) override;
    bool drawTexture(System*, TextureHandle texture, Quad*, RenderContext*) override;
    static TypeDescriptor* getClassType();                // 8396E0, metadata94EB38
protected:
    // Both are written straight into the quad's two state words; the class
    // never validates them against the twelve named values.
    int m_source;                                        // +04
    int m_destination;                                   // +08
};
} }
#endif
#endif
