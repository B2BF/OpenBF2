// Owns a ballistic aimer; the inherited slot remains pure. [INFERRED] Source placement.

#ifndef AI_AIDLL_AI_AIUTILS_AIMERHELPER_H
#define AI_AIDLL_AI_AIUTILS_AIMERHELPER_H

#include "IAimerHelper.h"

namespace dice {
namespace hfe {
namespace ai {

class Aimer;

class AimerHelper : public IAimerHelper
{
public:
    AimerHelper();
    virtual ~AimerHelper();

    // Finds a high arc, returning its predicted target, initial velocity and apex.
    // ELF 0x2EA570: a new slot 3 (+0x0C ia-32, +0x18 amd-64), not a slot-2 override.
    virtual bool hasIndirectLineOfFire(float speed, const float& drag, const float& gravity,
        const Vec3& relativePosition, const Vec3& origin, Vec3& predictedTarget,
        Vec3& velocity, Vec3& apex);

private:
    /* +0x04 (amd-64 +0x08) */ Aimer* m_aimer; // owned
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif
