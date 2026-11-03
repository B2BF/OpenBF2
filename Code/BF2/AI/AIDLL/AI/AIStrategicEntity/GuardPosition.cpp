// Guard positions. Evidence: libaidll.so (ia-32) 0x34EDB0..0x34FA8F.
// [INFERRED] Source placement.

#include "GuardPosition.h"

// <iostream>'s ios_base::Init object is this TU's static initialisation and teardown (ELF
// 0x34F9E0, __tcf_0 0x34FA40, _GLOBAL__I 0x34FA70).
#include <iostream>
#include <list>

#include "GuardPositionBotDCB.h"
#include "GuardPositionBotSCB.h"
#include "AIStrategicObject.h"
#include "AIStrategicObjectManager.h"
#include "../AIBot/BotMain.h"
#include "../AIBot/WPMoveTo.h"
#include "../AIUtils/checkPointer.h"
#include "../Main/AIMain.h"
#include "../Main/CommonSingletonServer.h"
#include "../../../AIInterface/BFKnowledge/BFGameEffector.h"
#include "../../../AIInterface/BFKnowledge/BFTerrain.h"

namespace dice { namespace hfe { namespace ai {

// ELF 0x34EDB0 (C2; C1 0x34EE70).
GuardPosition::GuardPosition(const Vec2& position, CombatStrengthType strengthType, int side,
                             int kit)
    : m_strengthType(strengthType)
    , m_active(true)
    , m_side(side)
    , m_kit(kit)
    , m_minDistance(5.0f)
    , m_bot(0)
    , m_recruited(false)
{
    m_position.x = 0.0f;
    m_position.y = 0.0f;
    m_position.z = 0.0f;
    m_rotation.x = 1.0f;
    m_rotation.y = 0.0f;
    m_rotation.z = 0.0f;
    vec2ToVec3XZ(position, m_position);
    m_position.y = static_cast<IAITerrain*>(CommonSingletonServer::m_iaiTerrain)
                       ->getHeight(position.x, position.y) + 1.0f;
}

// ELF 0x34EF30 (D2; D1 0x34EFE0).
GuardPosition::~GuardPosition()
{
}

// ELF 0x34F090.
CombatStrengthType GuardPosition::getStrengthType() const
{
    return m_strengthType;
}

// ELF 0x34F0A0.
const Vec3& GuardPosition::getPosition() const
{
    return m_position;
}

// ELF 0x34F0B0.
void GuardPosition::setRotation(const Vec3& rotation)
{
    m_rotation = rotation;
}

// ELF 0x34F0F0.
const Vec3& GuardPosition::getRotation() const
{
    return m_rotation;
}

// ELF 0x34F100.
void GuardPosition::setActive(bool active)
{
    m_active = active;
}

// ELF 0x34F110.
bool GuardPosition::getActive() const
{
    return m_active;
}

// ELF 0x34F120: the same load as getActive.
bool GuardPosition::active() const
{
    return m_active;
}

// ELF 0x34F130.
void GuardPosition::setSide(int side)
{
    m_side = side;
}

// ELF 0x34F140.
int GuardPosition::getSide() const
{
    return m_side;
}

// ELF 0x34F150.
void GuardPosition::setMinDistance(float distance)
{
    m_minDistance = distance;
}

// ELF 0x34F160.
float GuardPosition::getMinDistance() const
{
    return m_minDistance;
}

// ELF 0x34F170.
bool GuardPosition::needBot() const
{
    return !m_recruited && m_bot == 0;
}

// ELF 0x34F190.
void GuardPosition::recruit(Bot* bot)
{
    bot->setDeathCallBack(checkPointer(new GuardPositionBotDCB(bot, this)));
    bot->setSpawnCallBack(checkPointer(new GuardPositionBotSCB(bot, this)));
    m_bot = bot;
    m_recruited = true;
    if (bot->isDead())
    {
        bot->setIsInTransitionPhase(true);
        spawnBot(bot);
    }
    else
        botIsAlive(bot);
}

// ELF 0x34F4A0. A one-point path to the position: radius m_minDistance, step radius 60, flags 1,
// no last area.
void GuardPosition::botIsAlive(Bot* bot)
{
    std::list<Vec3> path;
    path.push_back(m_position);
    bot->setWaypoints(checkPointer(new WPMoveTo(m_minDistance, 60.0f, 1, path, -1)));
}

// ELF 0x34F760.
void GuardPosition::botIsDead(Bot* bot)
{
    bot->setIsInTransitionPhase(true);
    lostBot();
}

// ELF 0x34F7A0. IAIGameEffector::forceSpawnBot (+0x20) with the position and the facing.
void GuardPosition::spawnBot(Bot* bot)
{
    bot->setSpawnTarget(1, m_kit);
    static_cast<IAIGameEffector*>(CommonSingletonServer::m_iaiGameEffector)
        ->forceSpawnBot(bot->getPlayer(), m_position, m_rotation);
}

// ELF 0x34F820. IAIMain::registerFreeBot (+0x10).
void GuardPosition::lostBot()
{
    IAIMain::m_instance->registerFreeBot(m_bot);
    m_bot = 0;
}

// ELF 0x34F870. The object found by index is not null-tested; one found by name is, and an
// unknown name keeps the condition unresolved for the next update.
void GuardPosition::update()
{
    if (!m_bot)
        return;
    AIStrategicObjectManager* const manager = AIStrategicObjectManager::getInstance();
    for (std::vector<AreaCondition>::iterator it = m_areaConditions.begin();
         it != m_areaConditions.end(); ++it)
    {
        AIStrategicObject* object;
        if (it->areaIndex == -1)
        {
            object = manager->getStrategicObject(it->areaName);
            if (!object)
                continue;
            it->areaIndex = object->getIndex();
        }
        else
            object = manager->getStrategicObject(it->areaIndex);
        if (object->getStatus(m_side) == it->status)
        {
            lostBot();
            return;
        }
    }
}

// ELF 0x34F920.
void GuardPosition::addStrategicAreaAbortCondition(const std::string& areaName,
                                                   StatusMap::Status status)
{
    AreaCondition condition;
    condition.areaName = areaName;
    condition.areaIndex = -1;
    condition.status = status;
    m_areaConditions.push_back(condition);
}

} } }
