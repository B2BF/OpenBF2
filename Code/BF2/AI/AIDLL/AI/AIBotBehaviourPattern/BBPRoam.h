// Shared roaming pattern state, recovered from libaidll.so (Linux 1.5, ia-32).

#ifndef AI_AIDLL_AI_AIBOTBEHAVIOURPATTERN_BBPROAM_H
#define AI_AIDLL_AI_AIBOTBEHAVIOURPATTERN_BBPROAM_H

#include "BBPPattern.h"
#include "../../../../Math/Vec3.h"

namespace dice {
namespace hfe {

namespace ai {


// ELF: vtable 0x4BC560 (7 words), package H-deferred.
class BBPRoam : public BBPPattern
{
public:
    BBPRoam();   // 0x29B420
    virtual ~BBPRoam();   // 0x29B300

    virtual int getRoamType();   // 0x29B550, slot +0x18; [INFERRED] ELF int

protected:
    // [INFERRED] member names from the constructors and concrete init methods.
    /* +0x08 */ int m_patternId;   // set by concrete pattern constructors
    /* +0x0C */ Vec3* m_positions; // per-bot positions, initially zero
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTBEHAVIOURPATTERN_BBPROAM_H
