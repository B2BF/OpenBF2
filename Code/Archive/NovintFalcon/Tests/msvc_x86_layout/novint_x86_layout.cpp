// [TEMP] Compiles NovintHFX.h for the MSVC x86 ABI from the Mac (bf2_novint_msvc_x86_layout): the
// header's own static_asserts check ControlBoxEffectParams (0x120 bytes, BF2.exe 0x6A587F) and its
// inline-written fields, and IHapticEffect's +0x0C flags. The real check is the Windows x86 build.
#include "../../BF2/External/NovintHFX/NovintHFX.h"
static_assert(sizeof(IHapticEffectParam) == 4, "interface: one vfptr");
static_assert(sizeof(NovintHFX::BaseEffectParams) == 0x18, "base: vfptr + 20 bytes of SDK state");
static_assert(sizeof(NovintHFX::Vect3) == 24, "three doubles");
static_assert(sizeof(NovintHFX::ConstantEffectParams) == 0x20, "constant effect: x86 SDK ABI");
static_assert(offsetof(NovintHFX::ConstantEffectParams, m_unresolved18) == 0x18, "x86 SDK ABI");
static_assert(offsetof(NovintHFX::ConstantEffectParams, m_unresolved1C) == 0x1C, "x86 SDK ABI");
static_assert(sizeof(NovintHFX::EnvelopeEffectParams) == 0x40, "envelope effect: x86 SDK ABI");
static_assert(offsetof(NovintHFX::EnvelopeEffectParams, m_unresolved18) == 0x18, "x86 SDK ABI");
static_assert(offsetof(NovintHFX::EnvelopeEffectParams, m_unresolved1C) == 0x1C, "x86 SDK ABI");
static_assert(offsetof(NovintHFX::EnvelopeEffectParams, m_force) == 0x28, "x86 SDK ABI");
