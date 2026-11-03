// Package F (ai-w1-f-urgency): recovered from libaidll.so 1.5.3153 (ia-32); ELF addresses on every body.

// static init: ELF 0x1ECFC0 (__tcf_0 0x1ED020, _GLOBAL__I 0x1ED050): <iostream>'s Init object only.
#include <iostream>

#include "BAPConObjectBehind.h"

#include "BAPActionInline.h"
#include "../AIBot/BotMain.h"
#include "../AIKnowledge/IPIMobile.h"
#include "../AIKnowledge/Information.h"
#include "../../../../Math/Mat4.h"
#include "../../../../Math/Vec3.h"

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x1ECB20 (C1; C2 0x1ECAE0): +4 = 0, +8 handle, +0xC threshold, +0x10 flag.
BAPConObjectBehind::BAPConObjectBehind(unsigned int object, float threshold, bool useEstimate)
{
    setOwner(0);
    m_object = object;
    m_threshold = threshold;
    m_useEstimate = useEstimate;
}

// ELF 0x1ECB80 (D0 0x1ECBA0)
BAPConObjectBehind::~BAPConObjectBehind()
{
}

// ELF 0x1ECBE0. Object position (XZ): the IPIMobile (+0x38) position estimate (vfn +0x2C) when
// +0x10 is set and the object has one, else getPosition (vfn +0x30). Bot position: the current
// equipment's getPosition (no null test in the image). Forward: transformation (vfn +0x3C) row 2
// (+0x20) in XZ, normalised in place; direction object->bot normalised by the same inline
// (1.1920929e-07 / 1.4210855e-14 thresholds). 0x1ECEA2 `flds 0xc(%eax)`, `fucompi`, `setae`:
// threshold >= dot.
bool BAPConObjectBehind::evaluate(Bot* bot)
{
    Information* object = bapinline::info(m_object);
    if (!object)
        return true;
    Information* equipment = bapinline::info(bot->getCurrentEquipment());

    Vec2 objectPosition;
    objectPosition.x = 0.0f;
    objectPosition.y = 0.0f;
    IPIMobile* mobile = object->getMobile();
    if (!m_useEstimate || !mobile) {
        vec3ToVec2XZ(object->getPosition(), objectPosition);
    } else {
        Vec3 estimate;
        estimate.x = 0.0f;
        estimate.y = 0.0f;
        estimate.z = 0.0f;
        mobile->getPositionEstimate(estimate);
        vec3ToVec2XZ(estimate, objectPosition);
    }

    Vec2 botPosition;
    botPosition.x = 0.0f;
    botPosition.y = 0.0f;
    vec3ToVec2XZ(equipment->getPosition(), botPosition);

    Vec2 forward;
    forward.x = 0.0f;
    forward.y = 0.0f;
    const Mat4& transformation = object->getTransformation();
    Vec3 row2;
    row2.x = transformation.m[8];
    row2.y = transformation.m[9];
    row2.z = transformation.m[10];
    vec3ToVec2XZ(row2, forward);
    bapinline::normalize(forward);

    Vec2 direction;
    direction.x = botPosition.x - objectPosition.x;
    direction.y = botPosition.y - objectPosition.y;
    bapinline::normalize(direction);

    return direction.x * forward.x + direction.y * forward.y <= m_threshold;
}

// ELF 0x1ECFB0
BAPConditionType BAPConObjectBehind::getConditionType() const
{
    return BAPCTObjectBehind;   // 17
}

} // namespace ai
} // namespace hfe
} // namespace dice
