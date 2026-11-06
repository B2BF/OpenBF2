// Tests the line from the bot's camera (or the selected soldier pose) to a fixed point.

#ifndef AI_AIDLL_AI_AIBOTACTIONPLAN_BAPCONLINEOFFIRE_H
#define AI_AIDLL_AI_AIBOTACTIONPLAN_BAPCONLINEOFFIRE_H

#include "../../../../Math/Vec3.h"
#include "../../../AIInterface/BFKnowledge/SoldierPose.h"
#include "BAPCondition.h"
#include "BAPTypes.h"

namespace dice {
namespace hfe {

namespace ai {

class Bot;

// ELF: vtable 0x4B92B8 (5 words), operator new 0x1C, package H-deferred.
class BAPConLineOfFire : public BAPCondition
{
public:
    BAPConLineOfFire(const Vec3&, bool);   // 0x1E79B0
    BAPConLineOfFire(const Vec3&, bool, SoldierPose);   // 0x1E7A90
    virtual ~BAPConLineOfFire();   // 0x1E7B20

    virtual bool evaluate(Bot*);   // 0x1E7B80, slot +0x00; from the base declaration
    virtual BAPConditionType getConditionType() const;   // 0x1E7ED0, slot +0x04; from the base declaration
private:
    /* +0x08 */ Vec3 m_target;
    /* +0x14 */ bool m_useCamera;
    /* +0x15 */ bool m_flag15; // [UNRESOLVED] constructor flag; never read by the shipped evaluator
    /* +0x18 */ SoldierPose m_pose; // initialized only by the pose-taking constructor
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTACTIONPLAN_BAPCONLINEOFFIRE_H
