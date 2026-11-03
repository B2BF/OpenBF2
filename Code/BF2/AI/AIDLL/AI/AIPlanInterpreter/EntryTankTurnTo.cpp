// Tank turning entry for BAPATurn. Attested TU: EntryTankTurnTo.cpp.

// static init: ELF 0x2C6780 (__tcf_0 0x2C67E0, _GLOBAL__I 0x2C6810): <iostream>'s Init object only.
#include <iostream>

#include "EntryTankTurnTo.h"

#include "TankControl.h"
#include "VehicleControlInline.h"
#include "../AIBot/BotMain.h"
#include "../AIBotActionPlan/BAPATurn.h"
#include "../AIBotActionPlan/BAPCondition.h"
#include "../AIKnowledge/Information.h"
#include "../AIKnowledge/InformationManager.h"
#include "../AIKnowledge/IPIMobile.h"
#include "../../../AIInterface/BFKnowledge/IAIObject.h"
#include "../../../AIInterface/BFKnowledge/IAIObjectPlugIn.h"
#include "../../../../Math/Mat4.h"
#include "../../../../Math/Vec3.h"

#include <cmath>

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x2C5E50 (C1; C2 0x2C5E30).
EntryTankTurnTo::EntryTankTurnTo()
{
}

// ELF 0x2C5E90 (D1; D2 0x2C5E70, D0 0x2C5EB0).
EntryTankTurnTo::~EntryTankTurnTo()
{
}

// ELF 0x2C6830.
int EntryTankTurnTo::getTargetId()
{
    return 24;
}

// ELF 0x2C5EF0. The end condition finishes the statement; otherwise getDirection supplies ok,
// and the entry turns or brakes while keeping the statement active.
bool EntryTankTurnTo::execute(BotScript* script, Bot* bot, bool& ok, bool& done)
{
    BAPATurn* const turn = static_cast<BAPATurn*>(script);
    Vec2 position;
    position.x = position.y = 0.0f;
    if (turn->m_endCondition->evaluate(bot))
    {
        done = true;
        ok = true;
        return true;
    }
    done = false;
    Information* const info = informationManager->getInformation(bot->getCurrentEquipment());
    if (!info)
    {
        done = true;
        ok = false;
        return true;
    }
    IAIObjectPhysical* const object = info->getAIObject()->getPhysical();
    if (!object->hasPhysicsEnabled())
        object->enablePhysics(info->getSide());
    IPIMobile* const mobile = info->getMobile();
    IPIControlInfo* const controlInfo = info->getControlInfo();
    IPIPhysical* const physical = info->getPhysical();
    if (!mobile || !controlInfo || !physical)
    {
        done = true;
        ok = false;
        return true;
    }
    vec3ToVec2XZ(info->getPosition(), position);
    const Vec3 target = turn->getDirection(ok);
    if (!ok)
    {
        done = true;
        return true;
    }
    Vec2 direction;
    vec3ToVec2XZ(target, direction);
    vehiclecontrol::normalizeVec2(direction);
    const Mat4& forwardTransform = info->getTransformation();
    const Vec3& forward3 = *reinterpret_cast<const Vec3*>(&forwardTransform.m[8]);
    Vec2 forward;
    vec3ToVec2XZ(forward3, forward);
    vehiclecontrol::normalizeVec2(forward);
    Vec2 right;
    vec3ToVec2XZ(vehiclecontrol::rightAxis(info->getTransformation()), right);
    vehiclecontrol::normalizeVec2(right);
    float dot = right.x * direction.x + right.y * direction.y;
    if (dot < -1.0f)
        dot = -1.0f;
    else if (dot > 1.0f)
        dot = 1.0f;
    const float halfPi = 1.570796371f;
    float angle = acosf(dot) - halfPi;
    if (angle < -halfPi)
        angle = -halfPi;
    else if (angle > halfPi)
        angle = halfPi;
    if (forward.x * direction.x + forward.y * direction.y < 0.0f)
    {
        if (angle == 0.0f)
            angle = 3.141592741f;
        else
        {
            const float sign = angle > 0.0f ? 1.0f : (0.0f > angle ? -1.0f : 0.0f);
            angle = angle + sign * halfPi;
        }
    }
    if (std::fabs(angle) > turn->m_turnSpeed)
        TankControl::turnTowardsDirection(angle, turn->m_turnSpeed, true, info, mobile,
                                          controlInfo, bot->getInput(), &ok);
    else
    {
        const Vec3 facing = info->getFaceing();
        const Vec3& speed = mobile->getSpeed();
        TankControl::resetControls(bot, bot->getInput(), true, false, false,
                                   speed.x * facing.x + speed.y * facing.y + speed.z * facing.z);
    }
    const Mat4& transformation = info->getTransformation();
    turn->m_vec28.x = transformation.m[8];
    turn->m_vec28.y = transformation.m[9];
    turn->m_vec28.z = transformation.m[10];
    turn->m_unwritten24 = mobile->getSpeedMagnitude();
    return true;
}

// ELF 0x2C65E0. Returns the midpoint of the two lowest-scored relative box corners.
// Corners outside the facing half-plane receive -1 and remain eligible.
void EntryTankTurnTo::getCentreOfFacingSide(const Vec2& origin, const Vec2& minimum,
                                           const Vec2& maximum, const Vec2& facing, Vec2& centre)
{
    Vec2 corners[4];
    corners[0] = minimum;
    corners[1].x = minimum.x;
    corners[1].y = maximum.y;
    corners[2].x = maximum.x;
    corners[2].y = minimum.y;
    corners[3] = maximum;
    const float dx = facing.x - origin.x;
    const float dy = facing.y - origin.y;
    float scores[4];
    for (int i = 0; i < 4; ++i)
    {
        corners[i].x = corners[i].x - origin.x;
        corners[i].y = corners[i].y - origin.y;
        if (dx * corners[i].x + dy * corners[i].y > 0.0f)
            scores[i] = corners[i].x * corners[i].x + corners[i].y * corners[i].y;
        else
            scores[i] = -1.0f;
    }
    int first = 0;
    int second = 1;
    if (scores[0] > scores[1])
    {
        first = 1;
        second = 0;
    }
    for (int i = 2; i < 4; ++i)
    {
        if (scores[first] > scores[i])
        {
            second = first;
            first = i;
        }
        else if (scores[second] > scores[i])
            second = i;
    }
    centre = corners[first];
    centre.x = centre.x + corners[second].x;
    centre.y = centre.y + corners[second].y;
    centre.x = centre.x * 0.5f;
    centre.y = centre.y * 0.5f;
}

} // namespace ai
} // namespace hfe
} // namespace dice
