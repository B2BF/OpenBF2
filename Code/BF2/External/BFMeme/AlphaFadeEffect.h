// Path and declarations [INFERRED] beside the attested BFMeme sources.
// Registered name94DE1C, complete21-slot Effect table933400, factory835810.
#ifndef BFMEME_ALPHA_FADE_EFFECT_H
#define BFMEME_ALPHA_FADE_EFFECT_H
#include "Effect.h"
#if !DEDICATED
namespace dice { namespace meme {
class AlphaFadeEffect : public Effect
{
public:
    // Inlined83581E/79DD40 construction installs only the primary; no fields.
    AlphaFadeEffect() = default;
    TypeDescriptor* getType() override;
    bool drawCharacter(System*, int character, FontHandle font, int elementIndex,
                       Quad*, RenderContext*) override; // +4C 834F10
    bool drawTexture(System*, TextureHandle texture, Quad*, RenderContext*) override;
    static TypeDescriptor* getClassType();
};
} }
#endif
#endif
