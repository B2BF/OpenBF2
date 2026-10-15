// HUD blend-function effect with typed setters. [INFERRED] Header placement.
#ifndef MENU_BF2BLENDFUNCEFFECT_H
#define MENU_BF2BLENDFUNCEFFECT_H
#include "../External/BFMeme/BlendFuncEffect.h"
#if !DEDICATED
namespace dice { namespace meme {
class Bf2BlendFuncEffect : public BlendFuncEffect {
public:
    // Defaults [INFERRED] from factory7CB7A0, which supplies the same7 and8.
    Bf2BlendFuncEffect(int source = 7, int destination = 8);
    ~Bf2BlendFuncEffect() override;                      // 835660, owns nothing
    TypeDescriptor* getType() override;
    // Names [INFERRED]; the two slots this class adds above BlendFuncEffect's.
    // Both shipped bodies are the folded one-store setters and answer nothing.
    virtual void setSource(int source);
    virtual void setDestination(int destination);
    static TypeDescriptor* getClassType();               // 7CB740, metadata937B8C
};
#if defined(_M_IX86)
static_assert(sizeof(Bf2BlendFuncEffect) == 12, "native Bf2BlendFuncEffect storage");
#endif
} }
#endif
#endif
