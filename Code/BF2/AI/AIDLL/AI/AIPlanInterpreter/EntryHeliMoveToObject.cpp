// EntryHeliMoveToObject (split out of BAPEntries.cpp by S0, bodies unchanged).

// static init: ELF 0x2B7580 (__tcf_0 0x2B75E0, _GLOBAL__I 0x2B7610): <iostream>'s Init object only.
#include <iostream>

#include "EntryHeliMoveToObject.h"

#include "HeliControl.h"
#include "../AIBot/BotMain.h"
#include "../AIBotActionPlan/BAPAMoveTo3dObject.h"
#include "../AIKnowledge/Information.h"
#include "../Main/InformationManager.h"
#include "../AIKnowledge/IPIMobile.h"
#include "../Main/CommonSingletonServer.h"
#include "../../../AIInterface/BFKnowledge/BFTimer.h"
#include "../../../../Math/Mat4.h"

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x2B7340 (C1; C2 0x2B7320).
EntryHeliMoveToObject::EntryHeliMoveToObject()
{
}

// ELF 0x2B7380 (D1; D2 0x2B7360, D0 0x2B73A0).
EntryHeliMoveToObject::~EntryHeliMoveToObject()
{
}

// ELF 0x2B7630.
int EntryHeliMoveToObject::getTargetId()
{
    return 9;
}

// ELF 0x2B73E0.
bool EntryHeliMoveToObject::execute(BotScript* script, Bot* bot, bool& ok, bool& done)
{
    BAPAMoveTo3dObject* const moveTo = static_cast<BAPAMoveTo3dObject*>(script);
    Mat4 unused = {{1.0f, 0.0f, 0.0f, 0.0f,
                    0.0f, 1.0f, 0.0f, 0.0f,
                    0.0f, 0.0f, 1.0f, 0.0f,
                    0.0f, 0.0f, 0.0f, 1.0f}};
    (void)unused;
    Information* const info = informationManager->getInformation(bot->getCurrentEquipment());
    if (!info)
    {
        done = true;
        ok = false;
        return true;
    }
    if (moveTo->isFinished(bot))
    {
        done = true;
        ok = true;
        // shipped defect: the completed statement still drives the helicopter below.
    }
    else
        done = false;
    const Vec3& speed = info->getMobile()->getSpeed();
    const Vec3 change = {speed.x - moveTo->m_vec34.x,
                         speed.y - moveTo->m_vec34.y,
                         speed.z - moveTo->m_vec34.z};
    const float dt = static_cast<float>(static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getTimeIncrease());
    const float inverse = 1.0f / dt;
    const Vec3 acceleration = {change.x * inverse, change.y * inverse, change.z * inverse};
    moveTo->m_vec34 = speed;
    const float distance = moveTo->m_distance;
    const Vec3& point = *moveTo->getFirstPoint();
    ok = HeliControl::moveToPoint(bot, point, distance, acceleration) != 0;
    return true;
}

} // namespace ai
} // namespace hfe
} // namespace dice
