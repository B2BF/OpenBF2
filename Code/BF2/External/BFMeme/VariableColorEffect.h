// Path/member spellings [INFERRED]; descriptor937610 names this class.
// Complete Effect21 table937630.
#ifndef BFMEME_VARIABLECOLOREFFECT_H
#define BFMEME_VARIABLECOLOREFFECT_H
#include "Effect.h"
#if !DEDICATED
namespace dice { namespace meme {
class Data;
class VariableColorEffect : public Effect
{
public:
    VariableColorEffect();                                // inlined in factory7CADF0
    ~VariableColorEffect() override;                       // 835660, borrowed Data
    TypeDescriptor* getType() override;
    unsigned int getObjectSize() override;
    bool streamProperties(System*, Stream*) override;
    bool drawCharacter(System*, int character, FontHandle font, int elementIndex,
                       Quad*, RenderContext*) override;
    bool drawTexture(System*, TextureHandle texture, Quad*, RenderContext*) override;
    static TypeDescriptor* getClassType();
protected:
    Data* m_red;                                          // +04, all borrowed
    Data* m_green;                                        // +08
    Data* m_blue;                                         // +0C
    Data* m_alpha;                                        // +10
};
#if defined(_M_IX86)
static_assert(sizeof(VariableColorEffect) == 20, "client VariableColorEffect");
#endif
} }
#endif
#endif
