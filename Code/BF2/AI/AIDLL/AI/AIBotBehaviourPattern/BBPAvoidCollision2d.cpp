// Vehicle collision-avoidance plans, recovered from libaidll.so.

// static init: ELF 0x22AFD0 (__tcf_0 0x22B030, _GLOBAL__I 0x22B060): <iostream>'s Init object only.
#include <iostream>

#include "BBPAvoidCollision2d.h"

#include "../AIUtils/checkPointer.h"
#include "../AIBot/BotMain.h"
#include "../AIBot/BotNavigation.h"
#include "../AIBot/SquadMember.h"
#include "../AIBotActionPlan/BAPAHonkHorn.h"
#include "../AIBotActionPlan/BAPAMoveToDirect.h"
#include "../AIBotActionPlan/BAPCombinerSerial.h"
#include "../AIBotActionPlan/BAPConOr.h"
#include "../AIBotActionPlan/BAPConPosition.h"
#include "../AIBotActionPlan/BAPConTime.h"
#include "../AIKnowledge/Information.h"
#include "../AIKnowledge/InformationManager.h"
#include "../AIKnowledge/IPIControlInfo.h"
#include "../AIKnowledge/IPIMobile.h"
#include "../AIKnowledge/IPIPhysical.h"
#include "../AIKnowledge/IPIUnit.h"
#include "../AIPlanInterpreter/BAPContext.h"
#include "../AIPlanInterpreter/BotScriptDebugger.h"
#include "../AIPlanInterpreter/VehicleControlInline.h"
#include "../Main/IAIPathfinding.h"
#include "../Main/AISettingsInterfaces.h"
#include "../Main/CommonSingletonServer.h"
#include "../../../../Math/Vec3.h"
#include "../../../../Math/Mat4.h"

#include <string>
#include <cmath>

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x229AD0: BBPAvoidCollision(), the vtable store (GOT -0xF48 + 8), the argument at +0x08,
// then `new Vec2[getMaxNBots()]` (IAISettings slot 0; 8 bytes each, both words zeroed from the
// 0.0f constant) through the inlined checkPointer (Debug type 6, line 46) into +0x1C. The same
// body as BBPAvoidCollisionInfantery's.
BBPAvoidCollision2d::BBPAvoidCollision2d(int id)
    : BBPAvoidCollision()
{
    m_id = id;
    m_pointXZ = checkPointer(new Vec2[static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getMaxNBots()]());
}

// ELF 0x229C70 (D2) / 0x229CC0 (D1) / 0x229D10 (D0): delete[] +0x1C when set, then
// ~BBPAvoidCollision.
BBPAvoidCollision2d::~BBPAvoidCollision2d()
{
    if (m_pointXZ)
        delete[] m_pointXZ;
}

// ELF 0x229E20
BotScript* BBPAvoidCollision2d::createPlan(Bot* bot)
{
    if (!m_initiated[bot->getBotId()])
    {
        if (bot->getDebugger())
            static_cast<BotScriptDebugger*>(bot->getDebugger())->clear();
        bot->getScriptContext()->clear();
        if (bot->getPlan())
        {
            BotScript* previous = bot->getPlan();
            if (previous)
                delete previous;
            bot->setPlan(0);
        }
        return 0;
    }
    if (bot->getPlan())
    {
        BotScript* previous = bot->getPlan();
        if (previous)
            delete previous;
        bot->setPlan(0);
    }
    if (bot->getDebugger())
        static_cast<BotScriptDebugger*>(bot->getDebugger())->clear();
    bot->getScriptContext()->clear();
    const unsigned int obstacleHandle = m_obstacle[bot->getBotId()];
    Information* obstacle = informationManager->getInformation(obstacleHandle);
    if (!obstacle)
        return 0;
    IPIMobile* obstacleMobile = obstacle->getMobile();
    if (!obstacleMobile)
        return 0;
    const float time = m_value[bot->getBotId()];
    const Vec2 collisionPoint = m_pointXZ[bot->getBotId()];
    const Vec3 relativeSpeed = m_relativeSpeed[bot->getBotId()];
    (void)collisionPoint;
    (void)relativeSpeed;
    Information* self = informationManager->getInformation(bot->getCurrentEquipment());
    IPIMobile* mobile = self->getMobile();
    IPIControlInfo* controls = self->getControlInfo();
    const Vec3& position = self->getPosition();
    const Vec2 ownXZ = {position.x, position.z};
    const Vec3& obstaclePosition = obstacle->getPosition();
    const Vec2 obstacleXZ = {obstaclePosition.x, obstaclePosition.z};
    Vec2 velocity;
    velocity.x = mobile->getSpeed().x;
    velocity.y = mobile->getSpeed().z;
    if (velocity.x * velocity.x + velocity.y * velocity.y == 0.0f)
    {
        velocity.x = self->getTransformation().m[8];
        velocity.y = self->getTransformation().m[10];
    }
    Vec2 obstacleVelocity;
    obstacleVelocity.x = obstacleMobile->getSpeed().x;
    obstacleVelocity.y = obstacleMobile->getSpeed().z;
    const float obstacleSpeed = std::sqrt(obstacleVelocity.x * obstacleVelocity.x
                                        + obstacleVelocity.y * obstacleVelocity.y);
    Vec2 unusedDirection = obstacleVelocity;
    if (obstacleSpeed > 0.0001f)
    {
        const float inverse = 1.0f / obstacleSpeed;
        unusedDirection.x *= inverse;
        unusedDirection.y *= inverse;
    }
    (void)unusedDirection;
    Vec2 direction = velocity;
    vehiclecontrol::normalizeVec2(direction);
    const float dx = obstacleXZ.x - ownXZ.x;
    const float dy = obstacleXZ.y - ownXZ.y;
    const float ahead = velocity.x * dx + velocity.y * dy;
    const float across = direction.y * dx - direction.x * dy;
    const float side = obstacleSpeed > 5.0f ? (ahead > 0.0f ? 1.0f : -1.0f)
                                          : (across < 0.0f ? 1.0f : -1.0f);
    const float ownRadius = self->getPhysical()->getRadius();
    const float obstacleRadius = obstacle->getPhysical()->getRadius();
    const Mat4& transformation = self->getTransformation();
    const Vec3 offset = {side * transformation.m[0], side * transformation.m[1], side * transformation.m[2]};
    const float radius = ownRadius + obstacleRadius;
    Vec3 target = {radius * offset.x + obstaclePosition.x,
                   radius * offset.y + obstaclePosition.y,
                   radius * offset.z + obstaclePosition.z};
    BotNavigation* navigation = bot->getBotNavigation();
    Vec3 hit = {0.0f, 0.0f, 0.0f};
    if (navigation->m_infantryPolygon == -1)
        return 0;
    unsigned int polygon;
    const int equipmentType = self->getUnit()->getEquipmentType();
    if (IAIPathfinding::instance->trace(navigation, true, target, hit, polygon,
                                       equipmentType, -1, 0.0f, 0, 0))
        target = hit;
    else
    {
        const Mat4& fallback = self->getTransformation();
        target = Vec3{0.1f + fallback.m[12], fallback.m[13] + 0.0f, fallback.m[14] + 0.0f};
    }
    bot->getSquadMember()->clearRequestAction();
    BAPConOr* stop = checkPointer(new BAPConOr());
    stop->addCondition(checkPointer(new BAPConTime(static_cast<double>(time * 1.5f), false, false, false)));
    stop->addCondition(checkPointer(new BAPConPosition(target, 1.0f)));
    const unsigned long long driveControls = controls->getDriveControls();
    const float arriveSpeed = mobile->getMaxSpeed();
    const float speed = mobile->getMaxSpeed();
    BAPAMoveToDirect* move = checkPointer(new BAPAMoveToDirect(bot, target, speed, arriveSpeed, stop, driveControls, m_id));
    BAPAHonkHorn* horn = 0;
    if (self->getObjectTypeFlags() & 0x04000000u)
    {
        const int obstacleSide = obstacle->getSide();
        const int ownSide = self->getSide();
        if (obstacleSide == ownSide && obstacle->getUnit()->isOccupied())
            horn = checkPointer(new BAPAHonkHorn(obstacleHandle, m_id));
    }
    BAPCombinerSerial* serial = checkPointer(new BAPCombinerSerial(m_id, -1));
    if (horn)
        serial->addStatement(horn);
    serial->addStatement(move);
    serial->m_owner = this;
    return serial;
}

// ELF 0x229D60
void BBPAvoidCollision2d::init(Bot* bot, Vec3& point, unsigned int obstacle, Vec3& relativeSpeed, float time)
{
    m_initiated[bot->getBotId()] = true;
    vec3ToVec2XZ(point, m_pointXZ[bot->getBotId()]);
    m_obstacle[bot->getBotId()] = obstacle;
    m_relativeSpeed[bot->getBotId()] = relativeSpeed;
    m_value[bot->getBotId()] = time;
}

} // namespace ai
} // namespace hfe
} // namespace dice
