// EntryMoveToObjectMediumSoldier (attested TU `EntryMoveToObjectMediumSoldier.cpp`, split out of BAPEntries.cpp by S0). Recovered from
// libaidll.so (Linux 1.5.3153, ia-32): the medium-LOD soldier entry for BAPAMoveToObject (statement type 9). Package B; see docs/macos-ai-behaviour-plan.md.
// execute's flags follow the BotScript convention: `ok` (the step ran) and `done` (finished; with
// ok == false the statement failed).

// static init: ELF 0x2BFAF0 (__tcf_0 0x2BFB50, _GLOBAL__I 0x2BFB80): <iostream>'s Init object only.
#include <iostream>

#include "EntryMoveToObjectMediumSoldier.h"

#include "InfanteryControl.h"
#include "../Main/CommonSingletonServer.h"
#include "../AIBot/BotMain.h"
#include "../AIBotActionPlan/BAPAMoveTo.h"
#include "../AIKnowledge/Information.h"
#include "../Main/InformationManager.h"
#include "../../../AIInterface/BFKnowledge/BFObjectEnvironment.h"
#include "../../../AIInterface/BFKnowledge/BFTimer.h"
#include "../../../AIInterface/BFKnowledge/IAIObject.h"
#include "../../../AIInterface/BFKnowledge/IAIObjectPlugIn.h"
#include "../../../../Math/Mat4.h"
#include "../../../../Math/Vec3.h"

namespace dice {
namespace hfe {
namespace ai {

EntryMoveToObjectMediumSoldier::EntryMoveToObjectMediumSoldier()
{
}

EntryMoveToObjectMediumSoldier::~EntryMoveToObjectMediumSoldier()
{
}

int EntryMoveToObjectMediumSoldier::getTargetId()
{
    return 9;
}

using infantry_detail::crossInline;
using infantry_detail::normaliseInline;

// ELF 0x2BEFE0. Medium LOD for BAPAMoveToObject: as EntryMoveToMediumSoldier, but it finishes when
// the target object is gone, reports "not done" (instead of returning the action's done flag) when
// the action has arrived, stands still within 1 cm of the point, never jumps along the facing, and
// normalises the right vector of an upright soldier too.
bool EntryMoveToObjectMediumSoldier::execute(BotScript* script, Bot* bot, bool& ok, bool& done)
{
    BAPAMoveTo* moveTo = static_cast<BAPAMoveTo*>(script);
    Information* info = informationManager->getInformation(bot->getCurrentEquipment());
    if (info == 0)
    {
        done = true;
        ok = false;
        return true;
    }
    IAIObject* object = info->getAIObject();
    IAIObjectMobile* mobile = object->getMobile();
    IAIObjectPhysical* physical = info->getAIObject()->getPhysical();
    if (physical->hasPhysicsEnabled())
        physical->disablePhysics(object->getSide());
    infanteryResetControls(bot, bot->getInput(), true, false, false);
    Information* target = informationManager->getInformation(static_cast<unsigned int>(moveTo->getObject()));
    if (target == 0 || moveTo->isFinished(bot))
    {
        done = true;
        ok = true;
        return true;
    }
    Vec2 goal;
    goal.x = 0.0f;
    done = false;
    goal.y = goal.x;
    bool arrived;
    ok = moveTo->update(goal, done, arrived, 0.0f) != 0;
    if (!ok)
    {
        done = true;
        return true;
    }
    if (done || arrived)
    {
        done = false;
        return true;
    }

    Vec2 own;
    own.x = info->getPosition().x;
    own.y = info->getPosition().z;
    // retail: the target's position is read here and only its z difference is formed, unused
    const Vec3 ownPosition = info->getPosition();
    const Vec3& targetPosition = target->getPosition();
    (void)(targetPosition.z - ownPosition.z);
    Vec2 delta;
    delta.x = goal.x - own.x;
    delta.y = goal.y - own.y;
    const float distance = std::sqrt(delta.y * delta.y + delta.x * delta.x);
    if (distance <= 0.01f)
    {
        done = false;
        return true;
    }

    const Mat4& current = object->getTransformation();
    Mat4 transformation;
    for (int i = 0; i < 16; ++i)
        transformation.m[i] = 0.0f;
    for (int i = 0; i < 8; ++i)
        transformation.m[i] = current.m[i];
    transformation.m[11] = current.m[11];
    transformation.m[15] = current.m[15];

    const float dt = static_cast<float>(static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getTimeIncrease());
    bot->requestSoldierPose(SPStanding, false);
    float speed = mobile->getMaxSpeed();
    const SoldierPose pose = info->getSoldierPose();
    if (pose == SPCrouching)
        speed = speed * 0.5f;
    else if (pose == SPLying)
        speed = speed * 0.25f;
    else if (pose != SPStanding)
        speed = 0.0f;

    const Vec3& objectPosition = object->getPosition();
    Vec3 position;
    position.y = objectPosition.y;
    Vec3 forward;
    forward.z = delta.y / distance;
    forward.x = delta.x / distance;
    forward.y = 0.0f;
    float stepZ = delta.y;
    float stepX = delta.x;
    if (dt * speed <= distance)
    {
        stepZ = dt * forward.z * speed;
        stepX = forward.x * dt * speed;
    }
    position.x = objectPosition.x + stepX;
    position.z = objectPosition.z + stepZ;

    Vec3 up;
    up.x = up.y = up.z = 0.0f;
    const float currentHeight = info->getPosition().y;
    float ground;
    float slope;
    static_cast<IAIObjectEnvironment*>(CommonSingletonServer::m_iaiObjectEnvironment)
        ->getHeightAndNormalForObject(object, position, ground, up, slope);
    if (currentHeight <= ground || currentHeight - ground <= slope + slope)
        position.y = mobile->getLodHeight() + ground;
    else
        position.y = currentHeight - slope;

    Vec3 right;
    if (pose == SPLying)
    {
        position.y = position.y - 0.5f;
        right = crossInline(up, forward);
        normaliseInline(right);
        forward = crossInline(right, up);
        normaliseInline(forward);
    }
    else
    {
        up.x = 0.0f;
        up.y = 1.0f;
        up.z = 0.0f;
        right = crossInline(up, forward);
        normaliseInline(right);
    }
    transformation.m[0] = right.x;
    transformation.m[1] = right.y;
    transformation.m[2] = right.z;
    transformation.m[4] = up.x;
    transformation.m[5] = up.y;
    transformation.m[6] = up.z;
    transformation.m[8] = forward.x;
    transformation.m[9] = forward.y;
    transformation.m[10] = forward.z;
    transformation.m[12] = position.x;
    transformation.m[13] = position.y;
    transformation.m[14] = position.z;
    object->setTransformation(transformation, bot->getSide());
    return true;
}

} // namespace ai
} // namespace hfe
} // namespace dice
