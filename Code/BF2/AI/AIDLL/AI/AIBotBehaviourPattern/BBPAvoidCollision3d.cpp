// Collision-avoidance pattern of aircraft (AI/AIDLL/AI/AIBotBehaviourPattern/BBPAvoidCollision3d.cpp
// in the image). Recovered from libaidll.so (Linux 1.5.3153, ia-32, symbols); every body carries its
// ELF address. createPlan was read from the Intel-syntax listing (fsubrp operand order) and checked
// against the Ghidra pseudocode, which misnames the altitude calls (+0x34) as getMaxSpeed.

// static init: ELF 0x22C390 (__tcf_0 0x22C3F0, _GLOBAL__I 0x22C420): <iostream>'s Init object only.
#include <iostream>

#include "BBPAvoidCollision3d.h"

#include "../../../../Math/Vec3.h"
#include "../AIBot/BotMain.h"
#include "../AIBot/SquadMember.h"
#include "../AIBotActionPlan/BAPActionInline.h"
#include "../AIBotActionPlan/BAPAMoveTo3d.h"
#include "../AIBotActionPlan/BAPConOr.h"
#include "../AIBotActionPlan/BAPConPosition.h"
#include "../AIBotActionPlan/BAPConTime.h"
#include "../AIKnowledge/IPIControlInfo.h"
#include "../AIKnowledge/IPIMobile.h"
#include "../AIKnowledge/Information.h"
#include "../AIKnowledge/InformationManager.h"
#include "../AIPlanInterpreter/BAPContext.h"
#include "../AIPlanInterpreter/BotScriptDebugger.h"
#include "../AIUtils/checkPointer.h"
#include "../Main/AISettingsInterfaces.h"
#include "../Main/CommonSingletonServer.h"

#include <cmath>
#include <list>

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x22B220: BBPAvoidCollision(), the vtable store, the argument at +0x08, then
// `new Vec3[getMaxNBots()]` (IAISettings slot 0; every component stored from the 0.0f constant)
// through the inlined checkPointer into +0x1C.
BBPAvoidCollision3d::BBPAvoidCollision3d(int id)
    : BBPAvoidCollision()
{
    m_id = id;
    m_collisionPoint =
        checkPointer(new Vec3[static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getMaxNBots()]());
}

// ELF 0x22B410 (D2 0x22B3C0, D0 0x22B460): delete[] +0x1C when set, then ~BBPAvoidCollision.
BBPAvoidCollision3d::~BBPAvoidCollision3d()
{
    if (m_collisionPoint)
        delete[] m_collisionPoint;
}

// ELF 0x22B4B0. Stores the arguments per bot, in this order.
void BBPAvoidCollision3d::init(Bot* bot, Vec3& collisionPoint, unsigned int obstacle, Vec3& relativeSpeed,
                               float value)
{
    m_initiated[bot->getBotId()] = true;
    m_collisionPoint[bot->getBotId()] = collisionPoint;
    m_obstacle[bot->getBotId()] = obstacle;
    m_relativeSpeed[bot->getBotId()] = relativeSpeed;
    m_value[bot->getBotId()] = value;
}

// ELF 0x22B570. Always rebuilds: a single BAPAMoveTo3d to a point 45 degrees left or right of the
// own XZ speed (whichever side the obstacle is not on, judged by the own speed's normal), at the
// own altitude plus twice the stored value when above the obstacle and minus it otherwise, until
// 1.1 times the value has passed or the point is within 5 m. The speed is the own maximum, reduced
// to 30 % unless the obstacle lies behind both 45-degree directions. Neither information is
// null-checked, nor the obstacle's mobile plug-in, as compiled.
// The first test compares |dot(own speed normal, obstacle heading)| - pi/2 with 20 degrees; a dot
// product of unit vectors can never pass it, so the left turn without the obstacle test is dead
// code in practice, kept as compiled. The statement list is constructed and destroyed unused.
// [PLATFORM] precision: the x87 build keeps the rotated components, the dot products and the
// normalization factor in extended precision; the float expressions below round them.
BotScript* BBPAvoidCollision3d::createPlan(Bot* bot)
{
    std::list<BAPStatement*> statements;
    std::list<BAPCondition*> conditions;
    bool behindBoth = false;

    if (!m_initiated[bot->getBotId()])
    {
        if (bot->getDebugger())
            static_cast<BotScriptDebugger*>(bot->getDebugger())->clear();
        bot->getScriptContext()->clear();
        if (bot->getPlan())
        {
            delete bot->getPlan();
            bot->setPlan(0);
        }
        return 0;
    }

    bot->getSquadMember()->clearRequestAction();
    if (bot->getPlan())
    {
        delete bot->getPlan();
        bot->setPlan(0);
    }
    if (bot->getDebugger())
        static_cast<BotScriptDebugger*>(bot->getDebugger())->clear();
    bot->getScriptContext()->clear();

    Information* const self = informationManager->getInformation(bot->getCurrentEquipment());
    IPIMobile* const mobile = self->getMobile();
    Information* const obstacle = informationManager->getInformation(m_obstacle[bot->getBotId()]);

    Vec2 selfXZ;
    vec3ToVec2XZ(self->getPosition(), selfXZ);
    Vec2 obstacleXZ;
    vec3ToVec2XZ(obstacle->getPosition(), obstacleXZ);
    Vec2 speed;
    vec3ToVec2XZ(mobile->getSpeed(), speed);
    Vec2 obstacleSpeed;
    vec3ToVec2XZ(obstacle->getMobile()->getSpeed(), obstacleSpeed);
    Vec2 obstacleHeading = obstacleSpeed;
    bapinline::normalize(obstacleHeading);   // inlined Vec2 normalization

    Vec2 left;
    left.x = speed.x * 0.7071f - speed.y * 0.7071f;
    left.y = speed.x * 0.7071f + speed.y * 0.7071f;
    Vec2 right;
    right.x = speed.x * 0.7071f + speed.y * 0.7071f;
    right.y = speed.y * 0.7071f - 0.7071f * speed.x;

    Vec2 normal;
    getNormal(speed, normal);
    Vec2 destinationXZ = selfXZ;
    if (std::fabs(normal.x * obstacleHeading.x + normal.y * obstacleHeading.y) - 1.5708f > 0.349066f)
    {
        destinationXZ.x += left.x;
        destinationXZ.y += left.y;
    }
    else
    {
        Vec2 toObstacle;
        toObstacle.x = obstacleXZ.x - selfXZ.x;
        toObstacle.y = obstacleXZ.y - selfXZ.y;
        if (toObstacle.x * left.x + toObstacle.y * left.y <= 0.0f
            && toObstacle.x * right.x + toObstacle.y * right.y <= 0.0f)
            behindBoth = true;
        if (normal.x * toObstacle.x + normal.y * toObstacle.y > 0.0f)
        {
            destinationXZ.x += left.x;
            destinationXZ.y += left.y;
        }
        else
        {
            destinationXZ.x += right.x;
            destinationXZ.y += right.y;
        }
    }

    const unsigned long long controls = self->getControlInfo()->getDriveControls();
    float maxSpeed = mobile->getMaxSpeed();
    Vec3 destination;
    vec2ToVec3XZ(destinationXZ, destination);
    const float altitude = self->getAltitude();
    if (altitude > obstacle->getAltitude())
        destination.y = m_value[bot->getBotId()] + m_value[bot->getBotId()] + altitude;
    else
        destination.y = altitude - (m_value[bot->getBotId()] + m_value[bot->getBotId()]);

    // The product is formed in extended precision and stored as the double argument; a float
    // times float product is exact in double.
    BAPConTime* const timeout = checkPointer(new BAPConTime(
        static_cast<double>(1.1f) * m_value[bot->getBotId()], false, false, false));
    BAPConPosition* const reached = checkPointer(new BAPConPosition(destination, 5.0f));
    conditions.push_back(timeout);
    conditions.push_back(reached);
    BAPConOr* const stop = checkPointer(new BAPConOr(conditions));
    if (!behindBoth)
        maxSpeed *= 0.3f;
    BAPAMoveTo3d* const move =
        checkPointer(new BAPAMoveTo3d(bot, destination, maxSpeed, maxSpeed, stop, controls, 30.0f, m_id));
    move->setParent(this);   // +0x04 (BotScript::m_owner)
    return move;
}

} // namespace ai
} // namespace hfe
} // namespace dice
