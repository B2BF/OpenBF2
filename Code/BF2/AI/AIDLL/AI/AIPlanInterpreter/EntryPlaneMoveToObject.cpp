// EntryPlaneMoveToObject (split out of BAPEntries.cpp by S0, bodies unchanged).

// static init: ELF 0x2C2F40 (__tcf_0 0x2C2FA0, _GLOBAL__I 0x2C2FD0): <iostream>'s Init object only.
#include <iostream>

#include "EntryPlaneMoveToObject.h"

#include "PlaneControl.h"
#include "ControlFunctions.h"
#include "../AIBot/BotMain.h"
#include "../AIBotActionPlan/BAPAMoveTo3dObject.h"
#include "../AIKnowledge/Information.h"
#include "../Main/InformationManager.h"
#include "../AIKnowledge/IPIMobile.h"
#include "../../../../IO/Input/PlayerInput.h"
#include "../../../../Math/Mat4.h"

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x2C2A30 (C1; C2 0x2C2A10).
EntryPlaneMoveToObject::EntryPlaneMoveToObject()
{
}

// ELF 0x2C2A70 (D1; D2 0x2C2A50, D0 0x2C2A90).
EntryPlaneMoveToObject::~EntryPlaneMoveToObject()
{
}

// ELF 0x2C2FF0.
int EntryPlaneMoveToObject::getTargetId()
{
    return 9;
}

// ELF 0x2C2AD0.
bool EntryPlaneMoveToObject::execute(BotScript* script, Bot* bot, bool& ok, bool& done)
{
    BAPAMoveTo3dObject* const moveTo = static_cast<BAPAMoveTo3dObject*>(script);
    Mat4 unused = {{1.0f, 0.0f, 0.0f, 0.0f,
                    0.0f, 1.0f, 0.0f, 0.0f,
                    0.0f, 0.0f, 1.0f, 0.0f,
                    0.0f, 0.0f, 0.0f, 1.0f}};
    (void)unused;
    Information* const info = informationManager->getInformation(bot->getCurrentEquipment());
    // shipped defect: current equipment is dereferenced before any validity check.
    const Vec3 position = info->getPosition();
    (void)position;
    Information* const target = informationManager->getInformation(moveTo->m_object);
    if (!target)
    {
        done = true;
        ok = false;
        return true;
    }
    if (moveTo->isFinished(bot))
    {
        done = true;
        ok = true;
        // shipped defect: the completed statement still drives the plane below.
    }
    else
        done = false;
    const Mat4& transform = info->getTransformation();
    const float speed = info->getMobile()->getSpeedMagnitude();
    float throttle;
    if (target->isOfType(ITAir))
    {
        io::PlayerInput* const input = bot->getInput();
        throttle = ((input->mappingFlags >> 3) & 1ULL) ? input->inputs[3] : 0.0f;
        const Vec3& targetPosition = target->getPosition();
        const Vec3& currentPosition = info->getPosition();
        const float x = targetPosition.x - currentPosition.x;
        const float y = targetPosition.y - currentPosition.y;
        const float z = targetPosition.z - currentPosition.z;
        const float distanceSquared = x * x + y * y + z * z;
        IPIMobile* const targetMobile = target->getMobile();
        const float range = moveTo->m_float18;
        float targetSpeed;
        if (targetMobile)
            targetSpeed = targetMobile->getSpeedMagnitude();
        else
            targetSpeed = moveTo->m_maxArriveSpeed;
        if (range * range >= distanceSquared)
            updateLinear(targetSpeed, speed, moveTo->m_int40[0], throttle, -1.0f, 1.0f, 1.0f);
        else if ((range + range) * (range + range) >= distanceSquared)
        {
            // shipped defect: the transition uses squared distance divided by range.
            updateLinear((distanceSquared * targetSpeed) / range, speed, moveTo->m_int40[0], throttle, -1.0f, 1.0f, 1.0f);
        }
        else
            throttle = 1.0f;
        moveTo->m_int40[0] = speed;
    }
    else
        throttle = 1.0f;
    const float distance = moveTo->m_distance;
    const Vec3 point = *moveTo->getFirstPoint();
    ok = PlaneControl::towardsPoint(bot, info, point, throttle, moveTo->m_right,
                                   moveTo->m_forward, moveTo->m_up, distance, bot->getInput());
    moveTo->m_right = {transform.m[8], transform.m[9], transform.m[10]};
    moveTo->m_up = {transform.m[4], transform.m[5], transform.m[6]};
    moveTo->m_forward = {transform.m[0], transform.m[1], transform.m[2]};
    moveTo->m_int84 = transform.m[13];
    return true;
}

} // namespace ai
} // namespace hfe
} // namespace dice
