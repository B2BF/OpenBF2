// Ground roaming pattern, recovered from libaidll.so (Linux 1.5, ia-32).

#ifndef AI_AIDLL_AI_AIBOTBEHAVIOURPATTERN_BBPROAM2D_H
#define AI_AIDLL_AI_AIBOTBEHAVIOURPATTERN_BBPROAM2D_H

#include "../../../../Math/Vec3.h"
#include "BBPRoam.h"

namespace dice {
namespace hfe {

namespace ai {

class Bot;
class BotScript;

// ELF: vtable 0x4BC5E0 (7 words), package H-deferred.
class BBPRoam2d : public BBPRoam
{
public:
    explicit BBPRoam2d(int);   // 0x29B5A0
    virtual ~BBPRoam2d();   // 0x29B620

    virtual BotScript* createPlan(Bot*);   // 0x29B700, slot +0x08; from the base declaration
    virtual int getRoamType();   // 0x29BA50, slot +0x18; from the base declaration

    void init(Bot*, const Vec3&);   // 0x29B6A0; [INFERRED] ELF void
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTBEHAVIOURPATTERN_BBPROAM2D_H
