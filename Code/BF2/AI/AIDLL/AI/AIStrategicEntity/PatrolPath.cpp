// PatrolPath. Evidence: libaidll.so (ia-32) 0x350230..0x350F80.
// [INFERRED] Source placement.

#include "PatrolPath.h"

// <iostream>'s ios_base::Init object is this TU's static initialisation and teardown (ELF
// 0x350ED0, __tcf_0 0x350F30, _GLOBAL__I 0x350F60).
#include <iostream>

#include "PatrolPathBotDCB.h"
#include "PatrolPathBotSCB.h"
#include "../AIBot/BotMain.h"
#include "../AIBot/WPPatrol.h"
#include "../AIStrategicMap/BotSpawner.h"
#include "../AIUtils/checkPointer.h"
#include "../Main/CommonSingletonServer.h"
#include "../../../AIInterface/BFKnowledge/BFTerrain.h"    // IAITerrain::getTerrainAltitude

namespace dice { namespace hfe { namespace ai {

// ELF 0x350230 (C2; C1 0x350360): active, minimum distance 1, patrol speed -1.
PatrolPath::PatrolPath(CombatStrengthType strengthType, int side)
    : m_path()
    , m_strengthType(strengthType)
    , m_active(true)
    , m_side(side)
    , m_currentResources(0)
    , m_requiredResources(0)
    , m_minDistance(1.0f)
    , m_patrolSpeed(-1.0f)
    , m_bots()
{
}

// ELF 0x3503E0 (C2; C1 0x3504C0).
PatrolPath::PatrolPath(const std::vector<Vec3>& path, CombatStrengthType strengthType, int side)
    : m_path(path)
    , m_strengthType(strengthType)
    , m_active(true)
    , m_side(side)
    , m_currentResources(0)
    , m_requiredResources(0)
    , m_minDistance(1.0f)
    , m_patrolSpeed(-1.0f)
    , m_bots()
{
}

// ELF 0x3505A0 (D2; D1 0x350620).
PatrolPath::~PatrolPath()
{
}

// ELF 0x3506A0.
CombatStrengthType PatrolPath::getStrengthType() const
{
    return m_strengthType;
}

// ELF 0x3506B0.
const std::vector<Vec3>& PatrolPath::getPath() const
{
    return m_path;
}

// ELF 0x3506C0.
void PatrolPath::addPathPoint(const Vec3& point)
{
    m_path.push_back(point);
}

// ELF 0x350730.
void PatrolPath::addPathPoint(const Vec2& point)
{
    Vec3 position;
    position.x = point.x;
    position.y = static_cast<const IAITerrain*>(CommonSingletonServer::m_iaiTerrain)
                     ->getTerrainAltitude(point.x, point.y) + 3.0f;
    position.z = point.y;
    m_path.push_back(position);
}

// ELF 0x3507E0.
void PatrolPath::setActive(bool active)
{
    m_active = active;
}

// ELF 0x3507F0.
bool PatrolPath::getActive() const
{
    return m_active;
}

// ELF 0x350800.
bool PatrolPath::active() const
{
    return m_active;
}

// ELF 0x350810.
void PatrolPath::setSide(int side)
{
    m_side = side;
}

// ELF 0x350820.
int PatrolPath::getSide() const
{
    return m_side;
}

// ELF 0x350830.
void PatrolPath::setMinDistance(float distance)
{
    m_minDistance = distance;
}

// ELF 0x350840.
float PatrolPath::getMinDistance() const
{
    return m_minDistance;
}

// ELF 0x350850.
void PatrolPath::setPatrolSpeed(float speed)
{
    m_patrolSpeed = speed;
}

// ELF 0x350860.
float PatrolPath::getPatrolSpeed() const
{
    return m_patrolSpeed;
}

// ELF 0x350870.
void PatrolPath::setRequiredResources(int resources)
{
    m_requiredResources = resources;
}

// ELF 0x350880.
int PatrolPath::getRequiredResources() const
{
    return m_requiredResources;
}

// ELF 0x350890.
int PatrolPath::neededResources() const
{
    return m_requiredResources - m_currentResources;
}

// ELF 0x3508A0.
void PatrolPath::recruit(Bot* bot)
{
    bot->setDeathCallBack(checkPointer(new PatrolPathBotDCB(bot, this)));
    bot->setSpawnCallBack(checkPointer(new PatrolPathBotSCB(bot, this)));
    if (bot->isDead())
        botIsDead(bot);
    else
        botIsAlive(bot);
    ++m_currentResources;
    m_bots.push_back(bot);
}

// ELF 0x350BF0: a patrol over the path with this patrol's minimum distance as its radius and
// flags 5; the patrol speed goes to its +8 before the bot takes it.
void PatrolPath::botIsAlive(Bot* bot)
{
    WPPatrol* const patrol = checkPointer(new WPPatrol(m_minDistance, 5, m_path));
    patrol->m_float8 = m_patrolSpeed;
    bot->setWaypoints(patrol);
}

// ELF 0x350D70: a respawn group near the first path point; the result is unused.
void PatrolPath::botIsDead(Bot* bot)
{
    bot->setIsInTransitionPhase(true);
    BotSpawner::findSuitableSpawnGroup(m_path[0], bot);
}

// ELF 0x350DC0. Shipped defect, preserved: the current resources are incremented, not
// decremented. The first list entry for the bot is erased.
void PatrolPath::lost(Bot* bot)
{
    ++m_currentResources;
    for (std::list<Bot*>::iterator it = m_bots.begin(); it != m_bots.end(); ++it)
    {
        if (*it == bot)
        {
            m_bots.erase(it);
            return;
        }
    }
}

// ELF 0x350E50: every dead bot in the list goes through botIsDead.
void PatrolPath::update()
{
    for (std::list<Bot*>::iterator it = m_bots.begin(); it != m_bots.end(); ++it)
    {
        if ((*it)->isDead())
            botIsDead(*it);
    }
}

} } }
