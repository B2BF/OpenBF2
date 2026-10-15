// HUD movement effect with typed property setters. [INFERRED] Header placement.
#ifndef MENU_BF2MOVEEFFECT_H
#define MENU_BF2MOVEEFFECT_H
#include "../External/BFMeme/MoveEffect.h"
#if !DEDICATED
namespace dice { namespace meme {
class Bf2MoveEffect : public MoveEffect {
public:
    // The shipped constructor takes direction first and stores it at +8, then
    // length at +4 - the reverse of the member order. Defaults [INFERRED] from
    // factory7CB6C0, which supplies two nulls.
    Bf2MoveEffect(Data* direction = nullptr, Data* length = nullptr);
    ~Bf2MoveEffect() override;                           // 835660, owns nothing
    TypeDescriptor* getType() override;
    // Names [INFERRED]; the two slots this class adds above MoveEffect's. Both
    // shipped bodies are one-store setters that answer a hard true.
    virtual bool setDirection(Data* direction);
    virtual bool setLength(Data* length);
    static TypeDescriptor* getClassType();               // 7CB620, metadata937A04
};
#if defined(_M_IX86)
static_assert(sizeof(Bf2MoveEffect) == 12, "native Bf2MoveEffect storage");
#endif
} }
#endif
#endif
