// Medium-LOD turning entry for BAPATurn. Attested TU: EntryTurnToMedium.cpp.

// static init: ELF 0x2C89C0 (__tcf_0 0x2C8A20, _GLOBAL__I 0x2C8A50): <iostream>'s Init object only.
#include <iostream>

#include "EntryTurnToMedium.h"

#include "InfanteryControl.h"
#include "VehicleControlInline.h"
#include "../AIBot/BotMain.h"
#include "../AIBotActionPlan/BAPATurn.h"
#include "../AIBotActionPlan/BAPCondition.h"
#include "../AIKnowledge/Information.h"
#include "../Main/InformationManager.h"
#include "../AIKnowledge/IPIMobile.h"
#include "../Main/CommonSingletonServer.h"
#include "../../../AIInterface/BFKnowledge/BFObjectEnvironment.h"
#include "../../../AIInterface/BFKnowledge/IAIObject.h"
#include "../../../AIInterface/BFKnowledge/IAIObjectPlugIn.h"
#include "../../../../Math/Mat4.h"
#include "../../../../Math/Vec3.h"

#include <cmath>

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x2C7DB0 (C1; C2 0x2C7D90).
EntryTurnToMedium::EntryTurnToMedium()
{
}

// ELF 0x2C7DF0 (D1; D2 0x2C7DD0, D0 0x2C7E10).
EntryTurnToMedium::~EntryTurnToMedium()
{
}

// ELF 0x2C7E50. Turns the object without physics using a fixed angular step per tick, then
// aligns its axes to the terrain normal without moving its position. Only the condition ends it.
bool EntryTurnToMedium::execute(BotScript* script, Bot* bot, bool& ok, bool& done)
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
    IAIObjectPhysical* const physical = info->getAIObject()->getPhysical();
    if (physical->hasPhysicsEnabled())
        physical->disablePhysics(info->getSide());
    IPIMobile* const mobile = info->getMobile();
    IPIControlInfo* const controlInfo = info->getControlInfo();
    IPIPhysical* const physicalInfo = info->getPhysical();
    if (!mobile || !controlInfo || !physicalInfo)
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
    Vec2 forward2;
    vec3ToVec2XZ(forward3, forward2);
    vehiclecontrol::normalizeVec2(forward2);
    Vec2 right2;
    vec3ToVec2XZ(vehiclecontrol::rightAxis(info->getTransformation()), right2);
    vehiclecontrol::normalizeVec2(right2);
    float dot = right2.x * direction.x + right2.y * direction.y;
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
    if (forward2.x * direction.x + forward2.y * direction.y < 0.0f)
    {
        const float sign = angle > 0.0f ? 1.0f : (0.0f > angle ? -1.0f : 0.0f);
        angle = angle + sign * halfPi;
    }
    const float magnitude = std::fabs(angle);
    if (magnitude > turn->m_turnSpeed)
    {
        IAIObject* const object = info->getAIObject();
        const Mat4& current = object->getTransformation();
        (void)info->getPosition();
        Vec3 forward;
        const float step = 0.003926991f;
        if (magnitude > step)
        {
            const float sign = angle > 0.0f ? 1.0f : (0.0f > angle ? -1.0f : 0.0f);
            const Vec3& currentForward = *reinterpret_cast<const Vec3*>(&current.m[8]);
            rotateXZ(currentForward, forward, sign * step);
        }
        else
        {
            forward.x = direction.x;
            forward.y = 0.0f;
            forward.z = direction.y;
        }
        const Vec3& objectPosition = object->getPosition();
        float height;
        Vec3 up;
        up.x = up.y = up.z = 0.0f;
        float slope;
        static_cast<IAIObjectEnvironment*>(CommonSingletonServer::m_iaiObjectEnvironment)
            ->getHeightAndNormalForObject(object, objectPosition, height, up, slope);
        Vec3 right = infantry_detail::crossInline(up, forward);
        vehiclecontrol::normalizeVec3(right);
        forward = infantry_detail::crossInline(right, up);
        vehiclecontrol::normalizeVec3(forward);
        Mat4 transformation = current;
        transformation.m[0] = right.x;
        transformation.m[1] = right.y;
        transformation.m[2] = right.z;
        transformation.m[4] = up.x;
        transformation.m[5] = up.y;
        transformation.m[6] = up.z;
        transformation.m[8] = forward.x;
        transformation.m[9] = forward.y;
        transformation.m[10] = forward.z;
        object->setTransformation(transformation, bot->getSide());
    }
    const Mat4& transformation = info->getTransformation();
    turn->m_vec28.x = transformation.m[8];
    turn->m_vec28.y = transformation.m[9];
    turn->m_vec28.z = transformation.m[10];
    turn->m_unwritten24 = mobile->getSpeedMagnitude();
    return true;
}

// ELF 0x2C8A70
int EntryTurnToMedium::getTargetId()
{
    return 24;
}

} // namespace ai
} // namespace hfe
} // namespace dice
