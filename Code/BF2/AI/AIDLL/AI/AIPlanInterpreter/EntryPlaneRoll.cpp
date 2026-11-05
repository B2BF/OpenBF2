// Plane roll entry. Attested TU: EntryPlaneRoll.cpp.

// static init: ELF 0x2C38B0 (__tcf_0 0x2C3910, _GLOBAL__I 0x2C3940): <iostream>'s Init object only.
#include <iostream>

#include "EntryPlaneRoll.h"

#include "PlaneControl.h"
#include "../AIBot/BotMain.h"
#include "../AIBotActionPlan/BAPARoll3d.h"
#include "../AIBotActionPlan/BAPCondition.h"
#include "../AIKnowledge/Information.h"
#include "../Main/InformationManager.h"
#include "../AIKnowledge/IPIMobile.h"
#include "../../../../Math/Mat4.h"

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x2C3410 (C1; C2 0x2C33F0).
EntryPlaneRoll::EntryPlaneRoll()
{
}

// ELF 0x2C3450 (D1; D2 0x2C3430, D0 0x2C3470).
EntryPlaneRoll::~EntryPlaneRoll()
{
}

// ELF 0x2C34B0
bool EntryPlaneRoll::execute(BotScript* script, Bot* bot, bool& ok, bool& done)
{
    BAPARoll3d* const roll = static_cast<BAPARoll3d*>(script);
    Mat4 unused = {{1.0f, 0.0f, 0.0f, 0.0f,
                    0.0f, 1.0f, 0.0f, 0.0f,
                    0.0f, 0.0f, 1.0f, 0.0f,
                    0.0f, 0.0f, 0.0f, 1.0f}};
    (void)unused;
    if (roll->getBreakCondition()->evaluate(bot))
    {
        done = true;
        ok = true;
        // shipped defect: satisfying the break condition does not stop the controls.
    }
    else
        done = false;
    Information* const info = informationManager->getInformation(bot->getCurrentEquipment());
    if (!info)
    {
        done = true;
        ok = false;
        return true;
    }
    const Mat4& transform = info->getTransformation();
    const Vec3 position = info->getPosition();
    const Vec3 speed = info->getMobile()->getSpeed();
    // shipped defect: +0x2C, used as the minimum altitude, is not initialized by the constructor.
    const float minimumAltitude = roll->m_float2C;
    const Vec3 lookAhead = {speed.x * 10.0f, speed.y * 10.0f, speed.z * 10.0f};
    const float heightDifference = minimumAltitude - info->getAltitude(lookAhead);
    IPIControlInfo* const controls = info->getControlInfo();
    bool wrapped = false;
    // The second argument is the local [ebp-0xA4] (zeroed 0x2C354E, read 0x2C37BB, never written);
    // PlaneControl::roll does not read it.
    ok = PlaneControl::roll(roll->m_roll, 0.0f, bot->getInput(), transform, roll->m_previousY,
                            heightDifference > 0.0f, roll->m_flatFacing, roll->m_vector78, roll->m_vector6C,
                            roll->m_forward, roll->m_right, roll->m_up, controls, 1.0f,
                            roll->m_direction == 1, wrapped) != 0;
    if (wrapped)
        roll->m_roll += roll->m_direction == 1 ? -6.2831855f : 6.2831855f;
    roll->m_forward = {transform.m[8], transform.m[9], transform.m[10]};
    roll->m_up = {transform.m[4], transform.m[5], transform.m[6]};
    roll->m_right = {transform.m[0], transform.m[1], transform.m[2]};
    roll->m_previousY = position.y;
    return true;
}

// ELF 0x2C3960
int EntryPlaneRoll::getTargetId()
{
    return 25;
}

} // namespace ai
} // namespace hfe
} // namespace dice
