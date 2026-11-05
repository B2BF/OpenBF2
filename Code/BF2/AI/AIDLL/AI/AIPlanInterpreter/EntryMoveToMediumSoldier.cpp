// EntryMoveToMediumSoldier (attested TU `EntryMoveToMediumSoldier.cpp`, split out of BAPEntries.cpp by S0). Recovered from
// libaidll.so (Linux 1.5.3153, ia-32): the medium-LOD soldier entry for BAPAMoveTo (statement type 7). Package B; see docs/macos-ai-behaviour-plan.md.
// execute's flags follow the BotScript convention: `ok` (the step ran) and `done` (finished; with
// ok == false the statement failed).

// static init: ELF 0x2BCFF0 (__tcf_0 0x2BD050, _GLOBAL__I 0x2BD080): <iostream>'s Init object only.
#include <iostream>

#include "EntryMoveToMediumSoldier.h"

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

EntryMoveToMediumSoldier::EntryMoveToMediumSoldier()
{
}

EntryMoveToMediumSoldier::~EntryMoveToMediumSoldier()
{
}

int EntryMoveToMediumSoldier::getTargetId()
{
    return 7;
}

using infantry_detail::crossInline;
using infantry_detail::normaliseInline;

// ELF 0x2BC400. Medium LOD: no inputs are simulated. Physics is switched off and the soldier's
// transformation is moved directly towards the BAPAMoveTo point at its top speed (halved
// crouching, quartered lying, 0 for any other pose), snapped to the ground (or let down by at most
// the slope step when it is above it) and oriented along the move; a lying soldier is aligned to
// the ground normal and lowered by 0.5. Close to the point (within one step) it jumps there, and
// within 0.5 m it moves half a metre along its current facing.
bool EntryMoveToMediumSoldier::execute(BotScript* script, Bot* bot, bool& ok, bool& done)
{
    BAPAMoveTo* moveTo = static_cast<BAPAMoveTo*>(script);
    Mat4 transformation;
    for (int i = 0; i < 16; ++i)
        transformation.m[i] = 0.0f;
    Vec2 dir;
    dir.x = dir.y = 0.0f;
    Vec3 right;
    right.x = right.y = right.z = 0.0f;
    Vec3 forward;
    forward.x = forward.y = forward.z = 0.0f;
    Vec3 up;
    up.x = up.y = up.z = 0.0f;
    Vec2 goal;
    goal.x = goal.y = 0.0f;
    Vec3 position;
    position.x = position.y = position.z = 0.0f;
    Vec2 position2;
    position2.x = position2.y = 0.0f;

    Information* info = informationManager->getInformation(bot->getCurrentEquipment());
    if (info == 0)
    {
        done = true;
        ok = false;
        return true;
    }
    IAIObject* object = info->getAIObject();
    IAIObjectPhysical* physical = object->getPhysical();
    if (physical->hasPhysicsEnabled())
        physical->disablePhysics(object->getSide());
    if (moveTo->isFinished(bot))
    {
        done = true;
        ok = true;
        return true;
    }
    done = false;
    infanteryResetControls(bot, bot->getInput(), true, false, false);
    position = object->getPosition();
    vec3ToVec2XZ(position, position2);
    bool arrived;
    ok = moveTo->update(goal, done, arrived, 0.0f) != 0;
    if (!ok)
    {
        done = true;
        return true;
    }
    if (done || arrived)
        return true;

    Vec2 delta;
    delta.x = goal.x - position2.x;
    delta.y = goal.y - position2.y;
    const float distance = std::sqrt(delta.y * delta.y + delta.x * delta.x);
    dir = delta;
    IAIObjectMobile* mobile = object->getMobile();
    const float dt = static_cast<float>(static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getTimeIncrease());
    float speed = mobile->getMaxSpeed();
    bot->requestSoldierPose(SPStanding, false);
    const SoldierPose pose = info->getSoldierPose();
    if (pose == SPCrouching)
        speed = speed * 0.5f;
    else if (pose == SPLying)
        speed = speed * 0.25f;
    else if (pose != SPStanding)
        speed = 0.0f;
    transformation = object->getTransformation();

    if (dt * speed <= distance)
    {
        if (1e-05f < distance)
        {
            dir.x = dir.x * (1.0f / distance);
            dir.y = (1.0f / distance) * dir.y;
        }
        const float stepZ = dt * dir.y * speed;
        const float stepX = dir.x * dt * speed;
        position.x = stepX + position.x;
        position.z = stepZ + position.z;
        vec2ToVec3XZ(dir, forward);
    }
    else if (distance <= 0.5f)
    {
        position.x = transformation.m[8] * 0.5f + position.x;
        position.z = transformation.m[10] * 0.5f + position.z;
        forward.x = transformation.m[8];
        forward.y = transformation.m[9];
        forward.z = transformation.m[10];
    }
    else
    {
        position.x = dir.x + position.x;
        position.z = dir.y + position.z;
        vec2ToVec3XZ(dir, forward);
    }
    normaliseInline(forward);

    const float currentHeight = info->getPosition().y;
    float ground;
    float slope;
    static_cast<IAIObjectEnvironment*>(CommonSingletonServer::m_iaiObjectEnvironment)
        ->getHeightAndNormalForObject(object, position, ground, up, slope);
    if (currentHeight <= ground || currentHeight - ground <= slope + slope)
        position.y = mobile->getLodHeight() + ground;
    else
        position.y = currentHeight - slope;

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
        // upright: up is the world y axis; the right vector stays unnormalised here (retail)
        up.x = 0.0f;
        up.y = 1.0f;
        up.z = 0.0f;
        right = crossInline(up, forward);
    }
    transformation.m[12] = position.x;
    transformation.m[13] = position.y;
    transformation.m[14] = position.z;
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
    return true;
}

} // namespace ai
} // namespace hfe
} // namespace dice
