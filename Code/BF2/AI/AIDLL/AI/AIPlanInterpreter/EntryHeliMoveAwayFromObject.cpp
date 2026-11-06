// EntryHeliMoveAwayFromObject (split out of BAPEntries.cpp by S0, bodies unchanged).

// static init: ELF 0x2B6830 (__tcf_0 0x2B6890, _GLOBAL__I 0x2B68C0): <iostream>'s Init object only.
#include <iostream>

#include "EntryHeliMoveAwayFromObject.h"

#include "HeliControl.h"
#include "../AIBot/BotMain.h"
#include "../AIBotActionPlan/BAPAMoveAwayFrom3dObject.h"
#include "../AIKnowledge/Information.h"
#include "../Main/InformationManager.h"
#include "../AIKnowledge/IPIMobile.h"
#include "../Main/CommonSingletonServer.h"
#include "../../../AIInterface/BFKnowledge/BFTimer.h"
#include "../../../../Math/Mat4.h"

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x2B64E0 (C1; C2 0x2B64C0).
EntryHeliMoveAwayFromObject::EntryHeliMoveAwayFromObject()
{
}

// ELF 0x2B6520 (D1; D2 0x2B6500, D0 0x2B6540).
EntryHeliMoveAwayFromObject::~EntryHeliMoveAwayFromObject()
{
}

// ELF 0x2B68E0.
int EntryHeliMoveAwayFromObject::getTargetId()
{
    return 10;
}

// ELF 0x2B6580.
bool EntryHeliMoveAwayFromObject::execute(BotScript* script, Bot* bot, bool& ok, bool& done)
{
    BAPAMoveAwayFrom3dObject* const moveAway = static_cast<BAPAMoveAwayFrom3dObject*>(script);
    Information* const info = informationManager->getInformation(bot->getCurrentEquipment());
    Information* const object = informationManager->getInformation(moveAway->m_object);
    if (!info || !object)
    {
        done = true;
        ok = false;
        return true;
    }
    if (moveAway->isFinished(bot))
    {
        done = true;
        ok = true;
        // shipped defect: the completed statement continues and overwrites both flags.
    }
    else
        done = false;
    IPIMobile* const mobile = info->getMobile();
    const Mat4& transform = info->getTransformation();
    const Vec3& speed = mobile->getSpeed();
    const Vec3 change = {speed.x - moveAway->m_vec30.x,
                         speed.y - moveAway->m_vec30.y,
                         speed.z - moveAway->m_vec30.z};
    const float dt = static_cast<float>(static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getTimeIncrease());
    const float inverse = 1.0f / dt;
    const Vec3 acceleration = {change.x * inverse, change.y * inverse, change.z * inverse};
    moveAway->m_vec30 = speed;
    const Vec3 point = {transform.m[8] * 100.0f + transform.m[12],
                        0.0f * 100.0f + transform.m[13],
                        transform.m[10] * 100.0f + transform.m[14]};
    ok = HeliControl::moveToPoint(bot, point, 25.0f, acceleration) != 0;
    const float distance = moveAway->m_distance;
    const Vec3 target = *moveAway->getFirstPoint();
    const float x = target.x - transform.m[12];
    const float z = target.z - transform.m[14];
    done = x * x + 0.0f + z * z > distance * distance;
    return true;
}

} // namespace ai
} // namespace hfe
} // namespace dice
