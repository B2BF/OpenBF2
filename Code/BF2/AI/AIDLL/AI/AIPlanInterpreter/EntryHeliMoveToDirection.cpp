// EntryHeliMoveToDirection (split out of BAPEntries.cpp by S0, bodies unchanged).

// static init: ELF 0x2B7260 (__tcf_0 0x2B72C0, _GLOBAL__I 0x2B72F0): <iostream>'s Init object only.
#include <iostream>

#include "EntryHeliMoveToDirection.h"

#include "HeliControl.h"
#include "../AIBot/BotMain.h"
#include "../AIBotActionPlan/BAPAMoveTo3dDirection.h"
#include "../AIKnowledge/Information.h"
#include "../Main/InformationManager.h"
#include "../AIKnowledge/IPIMobile.h"
#include "../Main/CommonSingletonServer.h"
#include "../../../AIInterface/BFKnowledge/BFTimer.h"
#include "../../../../Math/Mat4.h"

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x2B6C90 (C1; C2 0x2B6C70).
EntryHeliMoveToDirection::EntryHeliMoveToDirection()
{
}

// ELF 0x2B6CD0 (D1; D2 0x2B6CB0, D0 0x2B6CF0).
EntryHeliMoveToDirection::~EntryHeliMoveToDirection()
{
}

// ELF 0x2B7310.
int EntryHeliMoveToDirection::getTargetId()
{
    return 8;
}

// ELF 0x2B6D30.
bool EntryHeliMoveToDirection::execute(BotScript* script, Bot* bot, bool& ok, bool& done)
{
    BAPAMoveTo3dDirection* const moveTo = static_cast<BAPAMoveTo3dDirection*>(script);
    Mat4 transform = {{1.0f, 0.0f, 0.0f, 0.0f,
                       0.0f, 1.0f, 0.0f, 0.0f,
                       0.0f, 0.0f, 1.0f, 0.0f,
                       0.0f, 0.0f, 0.0f, 1.0f}};
    Mat4 transpose = transform;
    Information* const info = informationManager->getInformation(bot->getCurrentEquipment());
    if (!info)
    {
        done = true;
        ok = false;
        return true;
    }
    info->getMobile()->getTurnRadius();
    if (moveTo->isFinished(bot))
    {
        done = true;
        ok = true;
        return true;
    }
    done = false;
    transform = info->getTransformation();
    transpose.setTranspose(transform);
    const Vec3 position = info->getPosition();
    (void)position;
    info->getMobile()->getSpeedMagnitude();
    Vec3 point = moveTo->m_target;
    const Vec3& speed = info->getMobile()->getSpeed();
    const Vec3 change = {speed.x - moveTo->m_vector24.x,
                         speed.y - moveTo->m_vector24.y,
                         speed.z - moveTo->m_vector24.z};
    const float dt = static_cast<float>(static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getTimeIncrease());
    const float inverse = 1.0f / dt;
    const Vec3 acceleration = {change.x * inverse, change.y * inverse, change.z * inverse};
    moveTo->m_vector24 = speed;
    point.y = -99999.0f;
    ok = HeliControl::moveToPoint(bot, point, moveTo->m_altitude, acceleration) != 0;
    return true;
}

} // namespace ai
} // namespace hfe
} // namespace dice
