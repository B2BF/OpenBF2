// Menu blend-function effect. [INFERRED] Source placement.

#include "Bf2BlendFuncEffect.h"
#if !DEDICATED
#include "../External/BFMeme/TypeDescriptor.h"
namespace dice { namespace meme {

Bf2BlendFuncEffect::Bf2BlendFuncEffect(int source, int destination)
    : BlendFuncEffect(source, destination) {}
Bf2BlendFuncEffect::~Bf2BlendFuncEffect() {}               // 835660, nothing owned

TypeDescriptor* Bf2BlendFuncEffect::getClassType()
{
    // Complete eight-slot metadata937B8C; independent counter A22B48.
    static ClassTypeDescriptor<Bf2BlendFuncEffect, BlendFuncEffect>
        type("dice::meme::Bf2BlendFuncEffect");
    return &type;
}
TypeDescriptor* Bf2BlendFuncEffect::getType() { return getClassType(); }
namespace {
TypeDescriptor* const registeredBf2BlendFuncEffectType = Bf2BlendFuncEffect::getClassType();
}

// Neither setter validates its argument or reports anything; both are the
// folded single-store bodies this image shares with other one-slot setters.
void Bf2BlendFuncEffect::setSource(int source) { m_source = source; }
void Bf2BlendFuncEffect::setDestination(int destination)
{ m_destination = destination; }

#if defined(_M_IX86)
static_assert(sizeof(Bf2BlendFuncEffect) == 12, "native Bf2BlendFuncEffect storage");
static_assert(sizeof(ClassTypeDescriptor<Bf2BlendFuncEffect, BlendFuncEffect>) == 16,
    "Bf2BlendFuncEffect metadata");
#endif
} }
#endif
