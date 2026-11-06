// Abstract registered aimer interface. [INFERRED] Source placement beside AimerHelper.

#ifndef AI_AIDLL_AI_AIUTILS_IAIMERHELPER_H
#define AI_AIDLL_AI_AIUTILS_IAIMERHELPER_H

#include "../../../../Math/Vec3.h"

namespace dice {
namespace hfe {
namespace ai {

class IAimerHelper
{
public:
    IAimerHelper();
    virtual ~IAimerHelper();

    // [CHOSEN, not recovered] Name, pointer spellings/constness and byte return under
    // the unimplemented-slot exception. Pure in IAimerHelper/AimerHelper vtables
    // ia-32 ELF 0x4BE8E8/0x4BE8C0, amd-64 0x61D160/0x61D120, slot 2 (+0x08/+0x10).
    // The sole caller is engine BFObjectEnvironment::hasIndirectLineOfFire:
    // ia-32 0x8402408 passes stack this, &exitVelocity, &drag, &scaledGravity,
    // &delta, &weaponPosition, &target, &velocity, &apex, then tests al at 0x840240D.
    // amd-64 0x771752: rdi=this; rsi/rdx/rcx/r8/r9 are the first five addresses,
    // the three output addresses are on the stack; al is tested at 0x771757.
    // No AI-image call or concrete override exists. This is not the by-value-float slot 3.
    virtual unsigned char unimplementedAimerSlot2(const float*, const float*, const float*,
        const Vec3*, const Vec3*, Vec3*, Vec3*, Vec3*) = 0;
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif
