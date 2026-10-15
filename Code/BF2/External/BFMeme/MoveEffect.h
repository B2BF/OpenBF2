// Movement overrides for Effect. Bf2MoveEffect adds two virtual slots.
#ifndef BFMEME_MOVEEFFECT_H
#define BFMEME_MOVEEFFECT_H
#include "Effect.h"
#if !DEDICATED
namespace dice { namespace meme {
class Data;
class Stream;

class MoveEffect : public Effect
{
public:
    // Both default to null: the base factory832740 zeroes +4 then +8.
    MoveEffect(Data* direction = nullptr, Data* length = nullptr);
    ~MoveEffect() override;                               // 835660, borrows both
    TypeDescriptor* getType() override;
    unsigned int getObjectSize() override;
    bool streamProperties(System*, Stream*) override;
    bool drawCharacter(System*, int character, FontHandle font, int elementIndex,
                       Quad*, RenderContext*) override;
    bool drawTexture(System*, TextureHandle texture, Quad*, RenderContext*) override;
    static TypeDescriptor* getClassType();                // 832700, metadata94CB6C
protected:
    // Both are borrowed expression nodes sampled every frame. The stream labels
    // are "Move length" for +4 and "Move direction" for +8, in that visit order.
    Data* m_length;                                      // +04
    Data* m_direction;                                   // +08
};
} }
#endif
#endif
