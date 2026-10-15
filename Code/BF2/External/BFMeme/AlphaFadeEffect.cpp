// Path [INFERRED]. All primary/metadata targets, eager/factory and differing
// bodies read from the client PE before implementation. See the status document.
#include "AlphaFadeEffect.h"
#if !DEDICATED
#include "Node.h"
#include "Quad.h"
#include "TypeDescriptor.h"
#if !defined(_M_IX86)
#include <cstdint>
#include <cstring>
#endif

namespace dice { namespace meme {
namespace {
int alphaFadeEffectInstances = 0;                    // A39FC0, loader-zeroed
// Concrete descriptor spelling [INFERRED]; complete table94DCD8.
class AlphaFadeEffectTypeDescriptor : public TypeDescriptor
{
public:
    AlphaFadeEffectTypeDescriptor() : TypeDescriptor("dice::meme::AlphaFadeEffect") {}
    TypeDescriptor* getBaseType() override { return Effect::getClassType(); }
    RuntimeObject* create() override
    {
        void* allocation = ::operator new(sizeof(AlphaFadeEffect));
        return allocation ? new(allocation) AlphaFadeEffect : nullptr;
    }
    void addInstance() override { ++alphaFadeEffectInstances; }
    void removeInstance() override { --alphaFadeEffectInstances; }
    int getInstanceCount() override { return alphaFadeEffectInstances; }
    unsigned int getInstanceSize() override { return sizeof(AlphaFadeEffect); }
};

// [INFERRED] helper spelling for the repeated 834F10/834F70 instructions.
// Keep x87 operand order/control semantics, including two-NaN payload choice;
// native mulss does not reproduce that case. Both operands are loaded afresh.
void multiplyAlpha(float* alpha, const float* factor)
{
#if defined(_M_IX86)
    __asm {
        mov edx, factor
        mov eax, alpha
        fld dword ptr [edx]
        fmul dword ptr [eax]
        fstp dword ptr [eax]
    }
#else
    // [PLATFORM] x64: SSE2 arithmetic. The product of two floats is exact in double, so the
    // single float rounding matches. The two-NaN case is selected explicitly: fld quiets the
    // factor, and a quiet alpha NaN with a larger significand wins over it, as in x87.
    const float first = *factor;
    const float second = *alpha;
    std::uint32_t firstBits, secondBits;
    std::memcpy(&firstBits, &first, sizeof firstBits);
    std::memcpy(&secondBits, &second, sizeof secondBits);
    if ((firstBits & 0x7FFFFFFFu) > 0x7F800000u && (secondBits & 0x7FFFFFFFu) > 0x7F800000u) {
        std::uint32_t result = firstBits | 0x00400000u;
        if ((secondBits & 0x00400000u) && (secondBits & 0x007FFFFFu) > (result & 0x007FFFFFu))
            result = secondBits;
        std::memcpy(alpha, &result, sizeof result);
        return;
    }
    *alpha = static_cast<float>(static_cast<double>(first) * second);
#endif
}
}

TypeDescriptor* AlphaFadeEffect::getClassType()
{
    static AlphaFadeEffectTypeDescriptor type;
    return &type;
}
TypeDescriptor* AlphaFadeEffect::getType() { return getClassType(); }
namespace {
// CRT96E1D0 ->871560 uses the same guarded descriptor as the getter, stores
// A3A03C and registers87E0A0 teardown. Native static lifetime owns that storage.
TypeDescriptor* const registeredAlphaFadeEffectType = AlphaFadeEffect::getClassType();
}

bool AlphaFadeEffect::drawCharacter(System* system, int character, FontHandle font,
                                    int elementIndex, Quad* quad, RenderContext* context)
{
    // each component is read, multiplied and stored as float32 before
    // the next. Do not snapshot the context value or restore colors afterward.
    for (auto& corner : quad->corners)
        multiplyAlpha(&corner.color.a, &context->field18);
    return Effect::drawCharacter(system, character, font, elementIndex, quad, context);
}
bool AlphaFadeEffect::drawTexture(System* system, TextureHandle texture, Quad* quad,
                                  RenderContext* context)
{
    for (auto& corner : quad->corners)
        multiplyAlpha(&corner.color.a, &context->field18);
    return Effect::drawTexture(system, texture, quad, context);
}

#if defined(_M_IX86)
static_assert(sizeof(AlphaFadeEffect) == 4, "Native x86 AlphaFadeEffect allocation");
#endif
} }
#endif
