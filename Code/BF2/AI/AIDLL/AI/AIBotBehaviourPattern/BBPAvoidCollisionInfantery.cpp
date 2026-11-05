// Infantry avoid-collision pattern (AI/AIDLL/AI/AIBotBehaviourPattern/BBPAvoidCollisionInfantery.cpp
// in the image): init keeps the collision point (XZ), the obstacle, the relative speed and the time
// or strength per bot; createPlan sprints the bot to a free point away from the obstacle, a tear-gas
// cloud or a grenade. Recovered from libaidll.so (Linux 1.5.3153, ia-32, symbols); every body carries
// its ELF address.
//
// static init: ELF 0x22E420 (__tcf_0 0x22E480, _GLOBAL__I 0x22E4B0): the <iostream> guard object only.

#include "BBPAvoidCollisionInfantery.h"

#include "../AIBot/BotMain.h"
#include "../AIBot/BotNavigation.h"
#include "../AIBot/SquadMember.h"
#include "../AIBotActionPlan/BAPActionInline.h"
#include "../AIBotActionPlan/BAPAMergeSoldierInputs.h"
#include "../AIBotActionPlan/BAPAMoveToDirect.h"
#include "../AIBotActionPlan/BAPAOverrideSensingAgent.h"
#include "../AIBotActionPlan/BAPCombinerParallel.h"
#include "../AIBotActionPlan/BAPCombinerSerial.h"
#include "../AIBotActionPlan/BAPConOr.h"
#include "../AIBotActionPlan/BAPConPosition.h"
#include "../AIBotActionPlan/BAPConTime.h"
#include "../AIBotActionPlan/BAPICTurnLock.h"
#include "../AIKnowledge/Information.h"
#include "../Main/InformationManager.h"
#include "../AIKnowledge/IPIControlInfo.h"
#include "../AIKnowledge/IPIMobile.h"
#include "../AIKnowledge/IPIUnit.h"
#include "../AIPlanInterpreter/BAPContext.h"
#include "../AIPlanInterpreter/BotScriptDebugger.h"
#include "../AIUtils/checkPointer.h"
#include "../Main/AIRandom.h"
#include "../Main/AISettingsInterfaces.h"
#include "../Main/CommonSingletonServer.h"
#include "../Main/IAIPathfinding.h"
#include "../../../AIInterface/BFKnowledge/AIPlayer.h"
#include "../../../../Game/GameLogic/Player.h"
#include "../../../../Game/Objects/IPlayer.h"
#include "../../../../Math/Vec3.h"

#include <cmath>
#include <iostream>

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x22C440 (C2) / 0x22C5E0 (C1): BBPAvoidCollision(), +0x08 = the argument, +0x1C =
// checkPointer(new Vec2[getMaxNBots]) with every Vec2 zeroed (rodata 0x49CB34) before the check.
BBPAvoidCollisionInfantery::BBPAvoidCollisionInfantery(int id)
    : BBPAvoidCollision()
{
    m_id = id;
    m_pointXZ = checkPointer(new Vec2[static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getMaxNBots()]());
}

// ELF 0x22C780 (D2) / 0x22C7D0 (D1) / 0x22C820 (D0): delete[] +0x1C when set, then
// ~BBPAvoidCollision.
BBPAvoidCollisionInfantery::~BBPAvoidCollisionInfantery()
{
    if (m_pointXZ)
        delete[] m_pointXZ;
}

// ELF 0x22C930 (listing; Ghidra loses the stack after the sret calls). Builds
//   serial { turn lock on,
//            parallel { override sensing agent,
//                       sprint (BAPAMoveToDirect) to the flee point until BAPConOr(time, within 2 m),
//                       merge soldier inputs },
//            turn lock off }
// The danger is the obstacle's position (XZ) or, without obstacle information, the tear-gas
// position of the bot's engine Player plus (1, 1, 1) with a 5 s flee time, else the grenade warning
// (none: 0). Without an obstacle handle, or with a zero collision point, the flee point is a trace
// 15 m (with an obstacle handle 10 m) straight away from the danger, then to a random side, then the
// other side (the last result is used whatever the trace answers); otherwise one trace sideways of the
// obstacle's velocity (its facing when it stands still), on the side away from it. The time is the
// flee time without an obstacle handle (rounded to float), else init's value. As compiled: the
// current plan is always dropped, the bot's equipment information is not null-checked, the obstacle's
// mobile plug-in is not null-checked, and a pattern that is not initiated returns 0 untouched.
BotScript* BBPAvoidCollisionInfantery::createPlan(Bot* bot)
{
    Vec2 ownXZ;
    ownXZ.x = ownXZ.y = 0.0f;
    Vec2 dangerXZ;
    dangerXZ.x = dangerXZ.y = 0.0f;
    Vec2 destinationXZ;
    destinationXZ.x = destinationXZ.y = 0.0f;

    world::IPlayer* const player = bot->getPlayer()->getPlayer();   // 0x22C97C, before the test
    if (!m_initiated[bot->getBotId()])
        return 0;   // 0x22E412

    bot->getSquadMember()->clearRequestAction();
    if (bot->getPlan())
    {
        // 0x22E3E1
        delete bot->getPlan();
        bot->setPlan(0);
    }
    if (bot->getDebugger())
        static_cast<BotScriptDebugger*>(bot->getDebugger())->clear();   // 0x22E3C6
    bot->getScriptContext()->clear();

    const bool noObstacle = m_obstacle[bot->getBotId()] == 0xFFFFFFFFu;
    Information* const obstacle = informationManager->getInformation(m_obstacle[bot->getBotId()]);
    Information* const info = informationManager->getInformation(bot->getCurrentEquipment());
    IPIMobile* const mobile = info->getMobile();   // 0x22CAA0: unchecked
    const Vec3& position = info->getPosition();
    vec3ToVec2XZ(position, ownXZ);

    double fleeTime = 0.0;
    Vec3 destination;   // -0x1A8: the danger position first, then the trace's flee point
    if (!obstacle)
    {
        // 0x22E2A3
        destination.x = destination.y = destination.z = 0.0f;
        if (player->getIsGassed())
        {
            fleeTime = 5.0;
            Vec3 one;
            one.x = one.y = one.z = 1.0f;
            const Vec3 gas = static_cast<Player*>(player)->getTearGasPosition();   // raw +0x2E8 in the image
            destination = bapinline::add(gas, one);
        }
        else if (!bot->getBotNavigation()->getGrenadeWarning(destination, fleeTime))
        {
            return 0;   // 0x22E3AA
        }
        vec3ToVec2XZ(destination, dangerXZ);
    }
    else
    {
        vec3ToVec2XZ(obstacle->getPosition(), dangerXZ);   // 0x22CAE6
    }

    // 0x22CB0F
    const float distance = noObstacle ? 15.0f : 10.0f;
    info->getMobile()->getVehicleNumber();   // result discarded
    const unsigned long long driveControls = info->getControlInfo()->getDriveControls();
    destination.x = destination.y = destination.z = 0.0f;
    IAIPathfinding* const pathfinding = IAIPathfinding::instance;
    const float maxSpeed = mobile->getMaxSpeed();
    const float side = AIRandom::m_instance->next() * 2.3283064e-10f > 0.5f ? 1.0f : -1.0f;
    const int equipmentType = info->getUnit()->getEquipmentType();

    bool atZeroPoint = false;
    if (!noObstacle)
    {
        // 0x22E200: the collision point compared with zero.
        Vec2 zero;
        zero.x = zero.y = 0.0f;
        const Vec2& point = m_pointXZ[bot->getBotId()];
        if (std::fabs(point.x - zero.x) + std::fabs(point.y - zero.y) <= 1.1920929e-07f)
            atZeroPoint = true;
    }

    Vec3 target;
    target.x = target.y = target.z = 0.0f;
    unsigned int polygon;
    if (atZeroPoint || noObstacle)
    {
        // 0x22CC76: straight away from the danger, else to one side, else the other.
        Vec2 toDanger;
        toDanger.x = dangerXZ.x - ownXZ.x;
        toDanger.y = dangerXZ.y - ownXZ.y;
        bapinline::normalize(toDanger);
        Vec2 sideways;
        sideways.x = distance * toDanger.y;
        sideways.y = distance * -toDanger.x;
        Vec2 offset;
        offset.x = distance * -toDanger.x;
        offset.y = -toDanger.y * distance;

        target = position;
        addVec2ToVec3XZ(offset, target);
        if (!pathfinding->trace(bot->getBotNavigation(), false, target, destination, polygon, equipmentType, -1, 0.0f, 0, 0))
        {
            // 0x22DC20
            offset.x = side * sideways.x;
            offset.y = side * sideways.y;
            target = position;
            addVec2ToVec3XZ(offset, target);
            if (!pathfinding->trace(bot->getBotNavigation(), false, target, destination, polygon, equipmentType, -1, 0.0f, 0, 0))
            {
                // 0x22DCEC
                offset.x = sideways.x * -side;
                offset.y = -side * sideways.y;
                target = position;
                addVec2ToVec3XZ(offset, target);
                pathfinding->trace(bot->getBotNavigation(), false, target, destination, polygon, equipmentType, -1, 0.0f, 0, 0);
            }
        }
    }
    else
    {
        // 0x22DE00: sideways of the obstacle's motion, on the side away from the obstacle.
        Vec3 velocity = obstacle->getMobile()->getSpeed();
        const Vec3& zero = Vec3::getZero();
        if (std::fabs(velocity.x - zero.x) <= 1.1920929e-07f && std::fabs(velocity.y - zero.y) <= 1.1920929e-07f
            && std::fabs(velocity.z - zero.z) <= 1.1920929e-07f)
            velocity = obstacle->getFaceing();   // 0x22E1B4
        const Vec3& obstaclePosition = obstacle->getPosition();
        const Vec3 own = position;
        Vec3 away;
        away.x = own.x - obstaclePosition.x;
        away.y = 0.0f;
        away.z = own.z - obstaclePosition.z;
        Vec3 offset;
        offset.x = velocity.z;
        offset.y = 0.0f;
        offset.z = -velocity.x;
        bapinline::normalize(offset);
        offset.x = distance * offset.x;
        offset.y = distance * offset.y;
        offset.z = distance * offset.z;
        if (bapinline::dot(offset, away) > 0.0f)
            target = bapinline::add(position, offset);
        else
            target = bapinline::sub(position, offset);   // 0x22E107
        pathfinding->trace(bot->getBotNavigation(), false, target, destination, polygon, equipmentType, -1, 0.0f, 0, 0);
    }

    // 0x22CE58
    BAPConTime* time;
    if (noObstacle)
        time = checkPointer(new BAPConTime(static_cast<float>(fleeTime), false, false, false));
    else
        time = checkPointer(new BAPConTime(m_value[bot->getBotId()], false, false, false));   // 0x22DAD0
    vec3ToVec2XZ(destination, destinationXZ);
    BAPConPosition* const reached = checkPointer(new BAPConPosition(destinationXZ, 2.0f));
    BAPConOr* const done = checkPointer(new BAPConOr());
    done->addCondition(time);
    done->addCondition(reached);
    BAPAMoveToDirect* const move =
        checkPointer(new BAPAMoveToDirect(bot, destination, maxSpeed, maxSpeed, done, driveControls, m_id));
    move->setIsSprinting(bot, true, destination, 0xFFFFFFFFu);

    BAPCombinerSerial* const serial = checkPointer(new BAPCombinerSerial(m_id, -1));
    serial->addStatement(checkPointer(new BAPICTurnLock(true)));
    BAPCombinerParallel* const parallel = checkPointer(new BAPCombinerParallel(m_id, true, -1, false));
    parallel->addStatement(checkPointer(new BAPAOverrideSensingAgent()));
    parallel->addStatement(move);
    parallel->addStatement(checkPointer(new BAPAMergeSoldierInputs(m_id)));
    serial->addStatement(parallel);
    serial->addStatement(checkPointer(new BAPICTurnLock(false)));
    serial->setParent(this);   // +0x04 (BotScript::mpOwner)
    return serial;
}

// ELF 0x22C870 (64 insns; reached from BBAvoid::calculateUrgency through slot +0x18):
// m_initiated[id] = true, vec3ToVec2XZ(point, +0x1C[id]), +0x0C[id] = handle, +0x10[id] = speed
// (three-word copy), +0x14[id] = value.
void BBPAvoidCollisionInfantery::init(Bot* bot, Vec3& point, unsigned int obstacle, Vec3& relativeSpeed, float value)
{
    m_initiated[bot->getBotId()] = true;
    vec3ToVec2XZ(point, m_pointXZ[bot->getBotId()]);
    m_obstacle[bot->getBotId()] = obstacle;
    m_relativeSpeed[bot->getBotId()] = relativeSpeed;
    m_value[bot->getBotId()] = value;
}

} // namespace ai
} // namespace hfe
} // namespace dice
