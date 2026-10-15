// Descriptor-attested dice::meme::ColorEffect (835700), table94DD50.
// Path and field names [INFERRED]; complete inherited21-slot surface.
#ifndef BFMEME_COLOREFFECT_H
#define BFMEME_COLOREFFECT_H
#include "Effect.h"
#include "Quad.h"
#if !DEDICATED
namespace dice { namespace meme {
class ColorEffect : public Effect {
public:
    ColorEffect(float red = 1, float green = 1, float blue = 1, float alpha = 1);
    ~ColorEffect() override; // 835660, no owned resources
    TypeDescriptor* getType() override;
    unsigned int getObjectSize() override;
    bool streamProperties(System*, Stream*) override;
    Color* getColor() override;
    bool drawCharacter(System*, int character, FontHandle font, int elementIndex,
                       Quad*, RenderContext*) override;
    bool drawTexture(System*, TextureHandle texture, Quad*, RenderContext*) override;
    static TypeDescriptor* getClassType();
private:
    Color m_color;
};
} }
#endif
#endif

