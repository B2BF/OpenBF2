// EntryHeliMoveTo (split out of BAPEntries.cpp by S0, bodies unchanged).

// static init: ELF 0x2B6BB0 (__tcf_0 0x2B6C10, _GLOBAL__I 0x2B6C40): <iostream>'s Init object only.
#include <iostream>

#include "EntryHeliMoveTo.h"

#include "HeliControl.h"
#include "../AIBot/BotMain.h"
#include "../AIBotActionPlan/BAPAMoveTo3d.h"
#include "../AIKnowledge/Information.h"
#include "../Main/InformationManager.h"
#include "../AIKnowledge/IPIMobile.h"
#include "../Main/CommonSingletonServer.h"
#include "../../../AIInterface/BFKnowledge/BFTimer.h"
#include "../../../../Math/Mat4.h"

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x2B6910 (C1; C2 0x2B68F0).
EntryHeliMoveTo::EntryHeliMoveTo()
{
}

// ELF 0x2B6950 (D1; D2 0x2B6930, D0 0x2B6970).
EntryHeliMoveTo::~EntryHeliMoveTo()
{
}

// ELF 0x2B6C60.
int EntryHeliMoveTo::getTargetId()
{
    return 7;
}

// ELF 0x2B69B0.
bool EntryHeliMoveTo::execute(BotScript* script, Bot* bot, bool& ok, bool& done)
{
    BAPAMoveTo3d* const moveTo = static_cast<BAPAMoveTo3d*>(script);
    Mat4 unused = {{1.0f, 0.0f, 0.0f, 0.0f,
                    0.0f, 1.0f, 0.0f, 0.0f,
                    0.0f, 0.0f, 1.0f, 0.0f,
                    0.0f, 0.0f, 0.0f, 1.0f}};
    (void)unused;
    Vec3 point = {0.0f, 0.0f, 0.0f};
    Information* const info = informationManager->getInformation(bot->getCurrentEquipment());
    if (!info)
    {
        done = true;
        ok = false;
        // shipped defect: missing equipment does not stop execution.
    }
    if (moveTo->isFinished(bot))
    {
        done = true;
        ok = true;
        return true;
    }
    done = false;
    point = *moveTo->getFirstPoint();
    const Vec3& speed = info->getMobile()->getSpeed();
    const Vec3 change = {speed.x - moveTo->m_vec3C.x,
                         speed.y - moveTo->m_vec3C.y,
                         speed.z - moveTo->m_vec3C.z};
    const float dt = static_cast<float>(static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getTimeIncrease());
    const float inverse = 1.0f / dt;
    const Vec3 acceleration = {change.x * inverse, change.y * inverse, change.z * inverse};
    moveTo->m_vec3C = speed;
    ok = HeliControl::moveToPoint(bot, point, moveTo->m_distance, acceleration) != 0;
    return true;
}

} // namespace ai
} // namespace hfe
} // namespace dice
