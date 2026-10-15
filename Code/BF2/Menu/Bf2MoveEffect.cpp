// Menu movement effect. [INFERRED] Source placement.

#include "Bf2MoveEffect.h"
#if !DEDICATED
#include "../External/BFMeme/TypeDescriptor.h"
namespace dice { namespace meme {

Bf2MoveEffect::Bf2MoveEffect(Data* direction, Data* length)
    : MoveEffect(direction, length) {}
Bf2MoveEffect::~Bf2MoveEffect() {}                           // 835660, nothing owned

TypeDescriptor* Bf2MoveEffect::getClassType()
{
    // Complete eight-slot metadata937A04; independent counter A22B10.
    static ClassTypeDescriptor<Bf2MoveEffect, MoveEffect> type("dice::meme::Bf2MoveEffect");
    return &type;
}
TypeDescriptor* Bf2MoveEffect::getType() { return getClassType(); }
namespace {
TypeDescriptor* const registeredBf2MoveEffectType = Bf2MoveEffect::getClassType();
}

// 7CDFC0 is the image's folded one-store setter, shared with
// Bf2PictureNode's texture slot and Bf2ObjectMarkerNode's object count.
bool Bf2MoveEffect::setDirection(Data* direction)
{ m_direction = direction; return true; }
bool Bf2MoveEffect::setLength(Data* length)
{ m_length = length; return true; }

#if defined(_M_IX86)
static_assert(sizeof(Bf2MoveEffect) == 12, "native Bf2MoveEffect storage");
static_assert(sizeof(ClassTypeDescriptor<Bf2MoveEffect, MoveEffect>) == 16,
    "Bf2MoveEffect metadata");
#endif
} }
#endif
