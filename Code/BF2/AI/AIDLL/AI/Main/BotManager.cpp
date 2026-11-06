// Owns the bot pool and schedules sensing, decisions and plan execution.
// Work is distributed across ticks according to per-bot update deadlines and detail level.

// static init: ELF 0x3E1470 (__tcf_0 0x3E14D0, _GLOBAL__I 0x3E1500): <iostream>'s Init object only.
#include <iostream>

#include <cstdlib>
#include "BotManager.h"


#include <sstream>
#include "../AIBot/WPEmpty.h"
#include "../AIBotActionPlan/BAPStatement.h"
#include "../../../AIInterface/BFKnowledge/BFGame.h"
#include "../../../../IO/File/FileManager.h"
#include "../../../../Game/Objects/IObject.h"
#include "../../../../Game/Objects/IObjectTemplate.h"
#include "AIMain.h"                  // IAIMain::m_instance - cached at +60
#include "AISettingsInterfaces.h"    // IAISettings::getMaxNBots
#include "../AIBot/BotMain.h"          // the preallocated pool
#include "../AIBot/BotMemory.h"
#include "../AIBot/BotOrder.h"
#include "../AIBot/UrgencyMerger.h"
#include "BotBehaviourManager.h"
#include "../AIUtils/checkPointer.h"
#include "CommonSingletonServer.h"
#include "../AIStrategic/SAI.h"                 // ISAI::addBot - reactivateBot hands it over
#include "../../../AIInterface/BFKnowledge/AIPlayer.h"   // IAIPlayer::setPlayer(int)
#include "../../../AIInterface/BFKnowledge/AIPlayerFactory.h"
#include "../../../AIInterface/BFKnowledge/BFTimer.h"    // IBFTimer::getUpdateTime - action's clock
#include "../../../AIInterface/BFKnowledge/BFGameEffector.h"   // IAIGameEffector::giveDamage
#include "../AIBot/BotNavigation.h"          // actionComAgent / actionUpdateNavPoints
#include "../AIBot/CommunicationAgent.h"     // actionComAgent
#include "../AIKnowledge/Information.h"      // updateDamages
#include "InformationManager.h"
#include "../AIKnowledge/IPIMobile.h"        // actionExecutePlan's stall test, actionPathfinding
#include "IAIPathfinding.h"                  // actionPathfinding hands the searcher its share
#include "../../../AIInterface/BFKnowledge/BFObjectEnvironment.h"   // actionHearing's object list
#include "../AIKnowledge/IPIPhysical.h"
#include "../../../../IO/Input/PlayerInput.h" // actionExecutePlan presses a climber's inputs
#include "../../../AIInterface/BFKnowledge/IAIObject.h"          // the object map, vehicle changes
#include "../../../AIInterface/BFKnowledge/IAIObjectPlugIn.h"    // IAIObjectPhysical::enablePhysics
#include "../AIBot/BotCallBack.h"            // death and spawn reports
#include "../AIBot/BotSquad.h"               // the order forwarders
#include "../../../AIInterface/BFKnowledge/IWeapon.h"   // actionHearing

namespace dice {
namespace hfe {
namespace ai {

int BotManager::s_tabLevel = 0;

// Runs the scheduled bot work within the supplied time budget.
// ELF 0x3DD580
void BotManager::action(const double& budget)
{
    IBFTimer* const timer = static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer);
    const double deadline = timer->getUpdateTime() + budget;

    updateDamages();
    actionComAgent();

    // Measured after the two unbudgeted steps, so they shrink every share downstream.
    const double share = deadline - timer->getUpdateTime();
    const double planSlice = share * 0.6;
    actionExecutePlan(planSlice);
    actionUpdateNavPoints();

    double remaining = deadline - timer->getUpdateTime();
    std::list<BMUpdate>::iterator update = m_updates.begin();
    for (; update != m_updates.end() && remaining > 0.0; ++update)
    {
        switch (update->type)
        {
        case BMUpdate::Idle:
            break;
        case BMUpdate::DecisionMaking:
        {
            double slice = share * 0.2;
            if (slice > remaining)
                slice = remaining;
            actionDecisionMaking(slice);
            break;
        }
        case BMUpdate::Pathfinding:
        {
            double slice = share * 0.3;
            if (slice > remaining)
                slice = remaining;
            actionPathfinding(slice);
            break;
        }
        case BMUpdate::Hearing:
        {
            double slice = share * 0.025;
            if (slice > remaining)
                slice = remaining;
            actionHearing(slice);
            break;
        }
        case BMUpdate::Sensing:
        {
            double slice = share * 0.15;
            if (slice > remaining)
                slice = remaining;
            actionSensing(slice);
            break;
        }
        default:
            Debug(DebugTypeError, "AI/AIDLL/AI/Main/BotManager.cpp", 1179, "")
                << "Bad update type.";
        }
        remaining = deadline - timer->getUpdateTime();
    }

    // Rotate the work this tick did not reach toward the front: at most the ring's five entries.
    int moves = 5;
    std::list<BMUpdate>::iterator front = m_updates.begin();
    while (front != m_updates.end() && front->type != update->type && moves > 0)
    {
        m_updates.push_back(*front);
        m_updates.erase(front++);
        --moves;
    }
}

// Applies accumulated bot damage once per tick.
// ELF 0x3E0AF0
void BotManager::updateDamages()
{
    IAIGameEffector* const effector =
        static_cast<IAIGameEffector*>(CommonSingletonServer::m_iaiGameEffector);
    for (std::vector<BotInfo*>::iterator it = m_bots.begin(); it != m_bots.end(); ++it)
    {
        if (!(*it)->m_bot->isDead() && (*it)->m_damage > 0.0f)
        {
            world::IPlayer* const from =
                m_bots[(*it)->m_attackerIndex]->m_bot->getPlayer()->getPlayer();
#if defined(_WIN32)
            Bot* const attacker = m_bots[(*it)->m_attackerIndex]->m_bot;
            const unsigned int handle = (*it)->m_bot->getCurrentEquipment();
            Information* const information =
                (handle & 0xffffu) ? informationManager->getInformation(handle) : nullptr;
            IAIObject* const target = information->getAIObject();
            const float damage = (*it)->m_damage;
            effector->giveDamage(target, from, attacker->getSide(), damage);
#else
            BotInfo* const info = *it;
            const int side = m_bots[info->m_attackerIndex]->m_bot->getSide();
            InformationManager* const manager = informationManager;
            const unsigned int handle = (*it)->m_bot->getCurrentEquipment();
            Information* const information = manager->getInformation(handle);
            effector->giveDamage(information->getAIObject(), from, side, info->m_damage);
#endif
        }
        (*it)->m_damage = 0.0f;
    }
}

// [INFERRED] Services one eligible bot's radio per tick, resuming from the saved cursor.
// Skips dead, man-down, or navigation-blocked bots and makes at most one roster circuit.
// ELF 0x3DE360
void BotManager::actionComAgent()
{
    const unsigned int start = m_comAgentCursor;
    if (m_bots.empty())
        return;
    Bot* bot = m_bots[m_comAgentCursor]->m_bot;
    for (;;)
    {
        if (!bot->isDead() && !bot->getManDown() && !bot->getBotNavigation()->isPathSearchPending())
        {
            bot->getCommunicationAgent()->update();
            if (++m_comAgentCursor >= m_bots.size())
                m_comAgentCursor = 0;
            return;
        }
        if (++m_comAgentCursor >= m_bots.size())
            m_comAgentCursor = 0;
        if (start == m_comAgentCursor)
            return;
        bot = m_bots[m_comAgentCursor]->m_bot;
    }
}

// The squared speed the stall test compares, grouped (x*x + y*y) + z*z as the PE's x87 sequence
// is and summed in double, which is exactly the PE's 53-bit precision (float products are exact
// at 53 bits). [PLATFORM] the ELF keeps the sum at 64-bit precision. A NaN sum is never "at most"
// which is both images' unordered branch. BotMain::updatePlayerAction keeps its own copy.
static bool speedSquaredAtMost(const Vec3& speed, float limit)
{
    const double x = speed.x, y = speed.y, z = speed.z;
    return (x * x + y * y) + z * z <= limit;
}

// Executes plans for the scheduled portion of the roster and refills work queues within the budget.
// ELF 0x3DDA90
void BotManager::actionExecutePlan(const double& slice)
{
    IBFTimer* const timer = static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer);
    const double deadline = timer->getUpdateTime() + slice;

    m_planned->clear();
    m_unplanned->clear();
    m_sensingQueue->clear();
    m_planExecutionsThisTick = 0;
    m_tickCount180 = 0;
    m_tickCount184 = 0;

    const unsigned int size = static_cast<unsigned int>(m_bots.size());
    m_planHalfToggle = !m_planHalfToggle;
    unsigned int count = size / 2 + 1;
    if (count > size)
        count = size;
    if (!m_planHalfToggle && (size & 1))
        --count;

    unsigned int processed = 0;
    while (processed < count)
    {
        BotInfo* info = m_bots[m_planCursor];
        Bot* const bot = info->m_bot;
        if (!bot || bot->isDead() || bot->isDeactivated() || !bot->getSquadMember())
        {
            ++processed;
            m_planCursor = (m_planCursor + 1) % size;
            continue;
        }

        bool executed = false;
        // Both images re-read the record's bot for these two questions.
        Information* const information =
            informationManager->getInformation(info->m_bot->getCurrentEquipment());
        const std::string name = info->m_bot->getPlayer()->getName();   // unused in both images
        if (!information)
        {
            ++processed;
            m_planCursor = (m_planCursor + 1) % size;
            continue;
        }

        if (bot->getBotNavigation()->isPathSearchPending())
        {
            if (bot->getPlayer() && bot->getPlayer()->getCurrentVehicle())
            {
                // ELF 0x3DE2A7: IAIObject +0xFC getPrimaryObject; a stuck bot marks its vehicle's
                // primary object as not allowed to bots (+0x9C), after Bot +0x148 setBotStuck.
                IAIObject* const primary = bot->getPlayer()->getCurrentVehicle()->getPrimaryObject();
                if (primary)
                {
                    bot->setBotStuck(true);
                    primary->setIsAllowedByBots(false);
                }
            }
            bool onFoot = false;
            if (bot->getPlayer())
                onFoot = bot->getPlayer()->getDefaultVehicle() == bot->getPlayer()->getCurrentVehicle();
            IPIMobile* const mobile = information->getMobile();
            if (!mobile
                || (speedSquaredAtMost(mobile->getSpeed(), 0.25f) && onFoot)
                || (!information->getPhysical()->isInAir() && information->getPhysical()->isInWater()))
            {
                static_cast<IAIGameEffector*>(CommonSingletonServer::m_iaiGameEffector)
                    ->respawnBot(bot->getPlayer());
                ++processed;
                m_planCursor = (m_planCursor + 1) % size;
                bot->updatePlayerAction(false);
                continue;
            }
        }

        if (information->hasArmament())
            bot->rearm(information);
        info->m_double24 += timer->getTimeIncrease();
        double time = timer->getUpdateTime();
        const bool inBudget = deadline > time;
        if (inBudget)
        {
            const double noSlice = 0.0;
            bot->planExecution(noSlice);
            // The same local becomes the elapsed time (the ELF stores it back into one slot).
            time = timer->getUpdateTime() - time;
            info->m_double48 += time;
            ++info->m_word36;
            executed = true;
            if (bot->getPlayer()->isClimbingLadder())
            {
                // Both are the float overload; the ELF inlines the first, the PE both.
                bot->m_playerInput->setInput(io::PIThrottle, 1.0f);
                bot->m_playerInput->setInput(io::PIMouseLookY, 1.0f);
            }
        }

        info->m_priority += m_lodLevelFullPriority;
        info->m_sensingPriority += m_lodLevelFullPriority;
        ++m_planExecutionsThisTick;
        if (bot->hasPlan() && !bot->hasDefaultPlan())
        {
            if (info->m_double24 > m_plannedDecisionMakingThresholdFull)
                m_planned->push(info);
        }
        else if (info->m_double24 > m_unplannedDecisionMakingThresholdFull)
        {
            m_unplanned->push(info);
        }
        m_sensingQueue->push(info);
        bot->updatePlayerAction(!inBudget);
        if (!executed)
            break;
        ++processed;
        m_planCursor = (m_planCursor + 1) % size;
    }

    for (unsigned int i = 0; i < size - processed; ++i)
        m_bots[m_planCursor]->m_bot->updatePlayerAction(false);
}

// [INFERRED] Visits at most one roster circuit and refreshes at most size/16 + 1 active
// navigations, skipping dead and man-down bots. Requires nonnull bot records: a null bot
// does not advance the cursor and would loop indefinitely.
// ELF 0x3DD980
void BotManager::actionUpdateNavPoints()
{
    if (m_bots.empty())
        return;
    const unsigned int limit = static_cast<unsigned int>(m_bots.size() >> 4) + 1;
    unsigned int updated = 0;
    unsigned int visited = 0;
    while (updated < limit && visited < m_bots.size())
    {
        Bot* const bot = m_bots[m_navPointCursor]->m_bot;
        if (!bot)
            continue;
        m_navPointCursor = (m_navPointCursor + 1) % static_cast<unsigned int>(m_bots.size());
        ++visited;
        if (bot->isDead() || bot->getManDown())
            continue;
        bot->getBotNavigation()->updateObstacles();
        if (bot->getBotNavigation()->isActive())
        {
            bot->getBotNavigation()->updateOccationally();
            ++updated;
        }
    }
}

// Schedules decisions using the interleave limits and each bot's detail level.
// ELF 0x3DE850
void BotManager::actionDecisionMaking(const double& slice)
{
    unsigned int light = 0;
    unsigned int full = 0;
    unsigned int medium = 0;
    const double deadline =
        static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getUpdateTime() + slice;

    int spare = static_cast<int>(m_plannedDecisionMakingInterleave)
        - static_cast<int>(m_planned->size());
    if (spare < 0)
        spare = 0;
    const unsigned int unplannedLimit = m_unplannedDecisionMakingInterleave + spare;
    m_selectedBots.resize(0);

    while (!m_unplanned->empty() && m_selectedBots.size() < unplannedLimit)
    {
        BotInfo* info = m_unplanned->top();
        m_unplanned->pop();
        switch (info->m_bot->getLodLevel())
        {
        case BotLodFull:
            ++full;
            break;
        case BotLodMedium:
            ++medium;
            break;
        case BotLodLight:
            ++light;
            // no break - see the block above
        default:
            Debug(DebugTypeError, "AI/AIDLL/AI/Main/BotManager.cpp", 1666, "") << "Unhandled lod.";
        }
        m_selectedBots.push_back(info);
    }

    unsigned int planned = 0;
    while (!m_planned->empty() && planned++ < m_plannedDecisionMakingInterleave)
    {
        BotInfo* info = m_planned->top();
        m_planned->pop();
        switch (info->m_bot->getLodLevel())
        {
        case BotLodFull:
            ++full;
            break;
        case BotLodMedium:
            ++medium;
            break;
        case BotLodLight:
            ++light;
            // no break - see the block above
        default:
            Debug(DebugTypeError, "AI/AIDLL/AI/Main/BotManager.cpp", 1690, "") << "Unhandled lod.";
        }
        m_selectedBots.push_back(info);
    }

    if (m_selectedBots.empty())
        return;
    const unsigned int weight =
        full * m_lodLevelFullTicks + medium * m_lodLevelMediumTicks + light * m_lodLevelLightTicks;
    if (!weight)
        return;
    const double perTick = slice / static_cast<double>(weight);
    double botSlice;    // one slot for the whole walk, as in both images
    for (std::vector<BotInfo*>::iterator it = m_selectedBots.begin(); it != m_selectedBots.end(); ++it)
    {
        BotInfo* const info = *it;
        if (!info->m_bot || info->m_bot->isDead())
            continue;
        if (!informationManager->getInformation(info->m_bot->getCurrentEquipment()))
            continue;
        switch (info->m_bot->getLodLevel())
        {
        case BotLodFull:
            botSlice = static_cast<double>(m_lodLevelFullTicks) * perTick;
            break;
        case BotLodMedium:
            botSlice = static_cast<double>(m_lodLevelMediumTicks) * perTick;
            break;
        case BotLodLight:
            botSlice = static_cast<double>(m_lodLevelLightTicks) * perTick;
            break;
        default:
            Debug(DebugTypeError, "AI/AIDLL/AI/Main/BotManager.cpp", 1735, "") << "Unhandled lod.";
        }
        if (botSlice > 0.0)
        {
            const double start = static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getUpdateTime();
            info->m_behaviourUpdateCount += info->m_bot->decisionMaking(botSlice);
            const double end = static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getUpdateTime();
            ++info->m_word40;
            info->m_double24 = 0.0;
            info->m_double56 += end - start;
        }
        info->m_priority = 0;
        if (static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getUpdateTime() > deadline)
            return;
    }
}

// Uses one tenth of the slice for bot path requests and the remainder for search processing.
// ELF 0x3DF7F0
void BotManager::actionPathfinding(const double& slice)
{
    IBFTimer* const timer = static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer);
    const double now = timer->getUpdateTime();
    const double deadline = now + slice;
    const double botDeadline = now + slice * 0.1;
    const unsigned int size = static_cast<unsigned int>(m_bots.size());

    for (unsigned int visited = 0; visited < size; ++visited)
    {
        if (timer->getUpdateTime() > botDeadline)
            break;
        BotInfo* const info = m_bots[m_pathfindingCursor];
        Bot* const bot = info->m_bot;
        if (bot && !bot->isDead() && !bot->isDeactivated() && bot->getSquadMember()
            && info->m_bot->getBotNavigation()->isSearchActive()
            && info->m_bot->getBotNavigation()->isSearchNew())
        {
            Information* const equipment =
                informationManager->getInformation(info->m_bot->getCurrentEquipment());
            if (equipment)
            {
                IPIMobile* const mobile = equipment->getMobile();
                if (mobile && mobile->getVehicleNumber() >= 0)
                {
                    bool first, second;    // written by the callee
                    info->m_bot->updatePathfinding(first, &second);
                }
            }
        }
        m_pathfindingCursor = (m_pathfindingCursor + 1) % size;
    }

    IAIPathfinding::instance->updatePathfinding((deadline - botDeadline) * 0.9);

    for (unsigned int i = 0; i < size; ++i)
    {
        BotInfo* const info = m_bots[i];
        Bot* const bot = info->m_bot;
        if (bot && !bot->isDead() && !bot->isDeactivated() && bot->getSquadMember()
            && info->m_bot->getBotNavigation()->isSearchActive()
            && info->m_bot->getBotNavigation()->isRunning())
        {
            bool first;            // written by the callee
            info->m_bot->updatePathfinding(first, 0);
        }
    }
}

// ELF 0x3DE440. Scans the occupied world objects round-robin from the last resume id. A sided
// object with an armament or a mobile plug-in gets a sound radius: the mobile's
// getSoundSphereRadius (IAIObjectMobile +0x40), or the last fired weapon's (IWeapon +0x10C)
// when that fired within 3.0f seconds (ELF 0x49CC90) and is larger. Without a mobile plug-in the
// ELF keeps the radius at its 0.0f start, so a weapon alone is never heard. A radius above 0.1f
// (ELF 0x49CC80) is heard by every live, standing enemy bot within it (squared distance against
// the float-rounded squared radius), through Bot +0x178 event_heardObject with the object's
// information for the bot's side (IAIObject +0xB4 forceGetInformation(side, false)). The slice
// deadline is tested only after such a pass.
// [PLATFORM] The ELF keeps the time difference and the distance sum in x87 extended precision;
// both are double here.
void BotManager::actionHearing(const double& slice)
{
    const double deadline =
        static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getUpdateTime() + slice;
    IAIObjectEnvironment* const environment =
        static_cast<IAIObjectEnvironment*>(CommonSingletonServer::m_iaiObjectEnvironment);
    m_occupiedObjects.resize(0);
    environment->getWorldPlayerOccupiedObjectsWithIds(m_occupiedObjects, 0);

    std::vector<std::pair<int, IAIObject*> >::iterator next = m_occupiedObjects.begin();
    while (next != m_occupiedObjects.end() && next->first != m_hearingResumeId)
        ++next;
    if (next == m_occupiedObjects.end())
        next = m_occupiedObjects.begin();
    const int count = static_cast<int>(m_occupiedObjects.size());
    const double gameTime = static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
    for (int visited = 0; visited < count;)
    {
        IAIObject* const object = next->second;
        if (++next == m_occupiedObjects.end())
            next = m_occupiedObjects.begin();
        ++visited;
        if (!object->getSide())
            continue;
        IAIObjectArmament* const armament = object->getArmament();
        IAIObjectMobile* const mobile = object->getMobile();
        if (!armament && !mobile)
            continue;

        float weaponRadius = 0.0f;
        float radius = 0.0f;
        if (armament)
        {
            IWeapon* const weapon = armament->getLastFiredWeapon();
            if (weapon && 3.0f > gameTime - weapon->getTimeOfFiring())
                weaponRadius = weapon->getSoundSphereRadius();
        }
        if (mobile)
        {
            const float mobileRadius = mobile->getSoundSphereRadius();
            radius = weaponRadius > mobileRadius ? weaponRadius : mobileRadius;
        }
        if (radius <= 0.1f)
            continue;

        const float radiusSquared = radius * radius;
        const int side = object->getSide();
        const Vec3& position = object->getPosition();
        for (std::vector<std::pair<int, IAIObject*> >::iterator it = m_occupiedObjects.begin();
             it != m_occupiedObjects.end(); ++it)
        {
            Bot* const bot = getBot(it->second);
            if (!bot || bot->isDead() || bot->getManDown() || bot->getSide() == side)
                continue;
            const Vec3& other = it->second->getPosition();
            const double dx = static_cast<double>(position.x) - other.x;
            const double dy = static_cast<double>(position.y) - other.y;
            const double dz = static_cast<double>(position.z) - other.z;
            if (!((dx * dx + dy * dy) + dz * dz > radiusSquared))
                bot->event_heardObject(object->forceGetInformation(bot->getSide(), false));
        }
        if (static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getUpdateTime() > deadline)
            break;
    }
    if (count > 0)
        m_hearingResumeId = next->first;
}

// Processes at most two sensing updates per tick using the full-detail update rate.
// ELF 0x3DF180
void BotManager::actionSensing(const double& slice)
{
    const double deadline =
        static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getUpdateTime() + slice;
    m_selectedBots.resize(0);
    unsigned int full = 0;
    unsigned int medium = 0;
    unsigned int light = 0;
    unsigned int taken = 0;
    while (!m_sensingQueue->empty() && taken <= 1)
    {
        BotInfo* info = m_sensingQueue->top();
        m_sensingQueue->pop();
        Bot* const bot = info->m_bot;
        if (!informationManager->getInformation(info->m_bot->getCurrentEquipment()))
            continue;
        switch (bot->getLodLevel())
        {
        case BotLodFull:
            ++full;
            break;
        case BotLodMedium:
            ++medium;
            break;
        case BotLodLight:
            ++light;
            break;
        default:
            Debug(DebugTypeError, "AI/AIDLL/AI/Main/BotManager.cpp", 1806, "") << "Unhandled lod.";
        }
        m_selectedBots.push_back(info);
        ++taken;
    }

    const unsigned int weight = m_lodLevelFullTicks * (light + medium + full);
    if (!weight)
        return;
    const double perTick = slice / static_cast<double>(weight);
    for (unsigned int i = 0; i < m_selectedBots.size(); ++i)
    {
        BotInfo* const info = m_selectedBots[i];
        double share;
        switch (info->m_bot->getLodLevel())
        {
        case BotLodFull:
        case BotLodMedium:
        case BotLodLight:
            share = perTick * static_cast<double>(m_lodLevelFullTicks);
            break;
        default:
            share = 0.0;
            Debug(DebugTypeError, "AI/AIDLL/AI/Main/BotManager.cpp", 1840, "") << "Unhandled lod.";
        }
        if (share > 0.0)
        {
            const double start = static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getUpdateTime();
            const double seeSlice = share * m_sensingQuotient;
            info->m_bot->see(seeSlice);
            const double senseSlice = share * m_sensingQuotientInverse;
            info->m_bot->updateSensing(senseSlice);
            const double end = static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getUpdateTime();
            ++info->m_word44;
            info->m_sensingPriority = 0;
            info->m_double64 += end - start;
        }
        if (static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getUpdateTime() > deadline)
            break;
    }
}


// Creates bot storage and scheduling queues.
// [RETIRE-WHEN: aidll-02-skeleton] Complete and validate the corresponding BotManager operation.
BotManager::BotManager()
    // The IBotManager and IBotManagerModifier bases construct their own interface subobjects.
    : m_planned(new BotQueue)
    , m_unplanned(new BotQueue)
    , m_sensingQueue(new SensingQueue)
    , m_comAgentCursor(0)
    // cached, not looked UP later. Every later use of the AI main goes through this copy, so a
    // BotManager built before an AIMain exists caches a null and never notices.
    , m_aiMain(IAIMain::m_instance)
    , m_hearingResumeId(0)
    , m_ptr192(0)
    , m_ptr196(0)
    , m_ptr200(0)
    // m_selectedBots: the three zero stores at +216..+224 are its default construction.
    , m_lastCreatedBot(0)
    , m_float232(0.3f)
    , m_flag236(false)
    // m_occupiedObjects: the three zero stores at +240..+248 are its default construction.
    , m_navPointCursor(0)
    , m_planHalfToggle(true)
    , m_planCursor(0)
    , m_pathfindingCursor(0)
{
    // Seeds one update of each type 0..4; action rotates through this five-entry schedule.
    for (unsigned i = 0; i <= 4; ++i)
        m_updates.insert(m_updates.end(), BMUpdate(static_cast<int>(i)));

    // Applies defaults through setters, preserving threshold absolute values and normalized
    // budget weights. setSystemQuotient stores only its first two shares.
    setLodLevelTicks(6, 6, 6);
    setLodLevelPriority(3, 3, 3);

    // by const reference, so the literals need somewhere to live. Retail spills six doubles to
    // the stack and passes their addresses; this is the same thing said in source.
    const double plannedFull   = 0.5;
    const double plannedMedium = 0.5;
    const double plannedLight  = 0.5;
    setPlannedDecisionMakingThreshold(plannedFull, plannedMedium, plannedLight);

    // not three equal values, unlike the planned set one line above: 0.3 for full detail and 0.4
    // for both reduced tiers. An unplanned decision is made more readily by a distant bot.
    const double unplannedFull   = 0.3;
    const double unplannedMedium = 0.4;
    const double unplannedLight  = 0.4;
    setUnplannedDecisionMakingThreshold(unplannedFull, unplannedMedium, unplannedLight);

    setDecisionMakingInterleave(2, 2);
    setSensingQuotient(1, 1);
    setSystemQuotient(30, 40, 30);

    // Preallocates the bot pool. A maximum of -1 disables preallocation.
    // [RETIRE-WHEN: aidll-02-skeleton] Complete and validate the corresponding BotManager operation.
    IAISettings* const settings = static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings);
    if (settings->getMaxNBots() != -1)
    {
        m_bots.reserve(settings->getMaxNBots());

        // Re-queries the maximum bot count on every iteration, independently of the reserve call.
        for (int i = 0; i < settings->getMaxNBots(); ++i)
        {
            // `getNumberOfBehaviours` is also called twice per bot - once to size the array and
            // once as the merger's count - rather than stored. Two calls, same value, reproduced.
            UrgencyGenerator** const generators =
                new UrgencyGenerator*[settings->getNumberOfBehaviours()];

            // Creates one merger per bot with no weight array. Generator-array and merger allocations
            // are unchecked; the following bot objects use checkPointer.
            UrgencyMerger* const merger =
                new UrgencyMerger(settings->getNumberOfBehaviours(), 0, generators);

            BotMemory* const memory = checkPointer(new BotMemory());

            // Preallocates every bot with no player or order; admission later fills these stable slots.
            // [UNRESOLVED] The remaining literal constructor settings have unknown meanings.
            BotMain* const bot = checkPointer(
                new BotMain(0, static_cast<unsigned int>(i), 0, 1.0f, 1, 0, memory, 0, 0,
                            merger, generators));

            m_preallocatedBots.insert(m_preallocatedBots.end(), bot);
        }
    }

    // Creates 256 player-to-bot entries with -1 meaning no assigned bot.
    m_botByPlayerId.resize(256, -1);
}

// These configuration methods remain virtual through IBotManagerModifier.

// ELF 0x3DAD20
void BotManager::setLodLevelTicks(unsigned full, unsigned medium, unsigned light)
{
    m_lodLevelFullTicks   = full;
    m_lodLevelMediumTicks = medium;
    m_lodLevelLightTicks  = light;
}

// ELF 0x3DAD50
void BotManager::setLodLevelPriority(unsigned full, unsigned medium, unsigned light)
{
    m_lodLevelFullPriority   = full;
    m_lodLevelMediumPriority = medium;
    m_lodLevelLightPriority  = light;
}

// Stores the absolute value of each decision-making threshold.
void BotManager::setPlannedDecisionMakingThreshold(const double& full,
                                                   const double& medium,
                                                   const double& light)
{
    m_plannedDecisionMakingThresholdFull   = full   < 0.0 ? -full   : full;
    m_plannedDecisionMakingThresholdMedium = medium < 0.0 ? -medium : medium;
    m_plannedDecisionMakingThresholdLight  = light  < 0.0 ? -light  : light;
}

// The same fabs treatment, and note the third test is written the other way round in the
// binary (`>= 0.0` selecting the positive rather than `< 0.0` selecting the negation). Identical
// for every value including -0.0; recorded, not reproduced as a separate shape.
void BotManager::setUnplannedDecisionMakingThreshold(const double& full,
                                                     const double& medium,
                                                     const double& light)
{
    m_unplannedDecisionMakingThresholdFull   = full   < 0.0 ? -full   : full;
    m_unplannedDecisionMakingThresholdMedium = medium < 0.0 ? -medium : medium;
    m_unplannedDecisionMakingThresholdLight  = light  < 0.0 ? -light  : light;
}

// ELF 0x3DAE30
void BotManager::setDecisionMakingInterleave(unsigned planned, unsigned unplanned)
{
    m_plannedDecisionMakingInterleave   = planned;
    m_unplannedDecisionMakingInterleave = unplanned;
}

// Normalizes two integer weights into complementary shares. Leaves both unchanged
// when their unsigned sum is zero.
// ELF 0x3DAE50
void BotManager::setSensingQuotient(unsigned a, unsigned b)
{
    if (a + b)
    {
        const double quotient = static_cast<double>(a) / static_cast<double>(a + b);
        m_sensingQuotient        = quotient;
        m_sensingQuotientInverse = 1.0 - quotient;
    }
}

// Normalizes three weights but stores only the first two fractions.
// Leaves existing shares unchanged when the unsigned total is zero.
// ELF 0x3DAEB0
void BotManager::setSystemQuotient(unsigned a, unsigned b, unsigned c)
{
    const unsigned total = a + b + c;
    if (total)
    {
        m_systemDecisionMakingQuotient = static_cast<double>(a) / static_cast<double>(total);
        m_systemQuotient2              = static_cast<double>(b) / static_cast<double>(total);
    }
}

// ELF 0x3DA7A0 (D1; recovered on the client track, docs/macos-c-singleplayer.md): reset, then the
// three scheduling queues are deleted (reset has already replaced A and B with fresh ones). The
// containers and both interface bases follow as members.
BotManager::~BotManager()
{
    reset();
    delete m_planned;
    delete m_unplanned;
    delete m_sensingQueue;
}

// - the vector's byte span shifted right by two, i.e. its element count.
// It counts admitted records, including deactivated bots. The constructor only reserves capacity;
// addUnspawnedBot appends a record after consuming or allocating a BotMain.
// ELF 0x3DB060
int BotManager::getBotCount() const
{
    return static_cast<int>(m_bots.size());
}

// Reports the bot counts from the managed roster.
// ELF 0x3DB080
int BotManager::getNumberOfActiveBots() const
{
    return static_cast<int>(m_bots.size()) - static_cast<int>(m_deactivatedBots.size());
}

// ELF 0x3DB0D0
int BotManager::getNumberOfDeactivedBots() const
{
    return static_cast<int>(m_deactivatedBots.size());
}

// Returns the bot at index. The caller must supply a valid index and a live record.
// ELF 0x3DB100
Bot* BotManager::getBot(unsigned int index) const
{
    return m_bots[index]->m_bot;
}

// Bot-ID lookup checks bounds; player lookup requires a valid association.
// ELF 0x3DB1A0
Bot* BotManager::getBotFromId(unsigned int id) const
{
    if (id < m_bots.size())
    {
        BotInfo* const info = m_bots[id];

        if (info)
            return info->m_bot;
    }

    return 0;
}

// no bounds check on `playerId` - see the block above.
// ELF 0x3E0A80
Bot* BotManager::getBotFromPlayerId(int playerId) const
{
    if (m_botByPlayerId[playerId] == -1)
        return 0;

    // the same element, read A second time.
    return getBotFromId(static_cast<unsigned int>(m_botByPlayerId[playerId]));
}

// ELF 0x3DB1D0: the first record whose player's name (IAIPlayer +0x18) equals the argument.
Bot* BotManager::getBotFromName(const std::string& name) const
{
    for (std::vector<BotInfo*>::const_iterator it = m_bots.begin(); it != m_bots.end(); ++it)
    {
        if ((*it)->m_bot->getPlayer()->getName() == name)
            return (*it)->m_bot;
    }
    return 0;
}

// BotManager::BotInfo - inlined in ELF addUnspawnedBot at ;
// PE standalone constructor . Only these fields are initialized. In particular
// m_behaviourUpdateCount and m_attackerIndex retain their allocation contents, as do alignment
// bytes.
BotManager::BotInfo::BotInfo(Bot* bot)
    : m_bot(bot), m_word4(0), m_priority(0), m_word12(0), m_sensingPriority(0),
      m_flag20(false), m_double24(0.0), m_word36(0), m_word40(0), m_word44(0),
      m_double48(0.0), m_double56(0.0), m_double64(0.0), m_flag72(true),
      m_flag73(false), m_double76(0.0), m_damage(0.0f)
{
}

// ELF 0x3DB2E0
bool exitAddBot(int playerId)
{
    static_cast<IAIGameEffector*>(CommonSingletonServer::m_iaiGameEffector)->terminatePlayer(playerId);
    return false;
}

// ELF 0x3DB320
bool BotManager::addBot(int playerId, int side, float* personality, IAIObject* object)
{
    const std::size_t rosterSize = m_bots.size();
    if (rosterSize >= static_cast<unsigned>(
            static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getMaxNBots()))
        return exitAddBot(playerId);
    if (m_botByPlayerId[playerId] != -1)
        return exitAddBot(playerId);

    m_lastCreatedBot = 0;
    IAIPlayer* const player = static_cast<IAIPlayerFactory*>(CommonSingletonServer::m_iaiPlayerFactory)
                                 ->getPlayerFromId(playerId);
    IAIGameEffector* const effector = static_cast<IAIGameEffector*>(CommonSingletonServer::m_iaiGameEffector);
    if (object)
    {
        IAIObject* const currentVehicle = player->getCurrentVehicle();
        if (currentVehicle != player->getDefaultVehicle())
            effector->exitVehicle(player);
        if (!object->isUnit())
            return exitAddBot(playerId);
        effector->enterVehicle(player, object);
    }
    IAIObject* const vehicle = player->getCurrentVehicle();
    if (!vehicle)
        return exitAddBot(playerId);
    vehicle->setSide(side);

    const int count = static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getNumberOfBehaviours();
    BotMain* bot;
    if (m_preallocatedBots.empty())
    {
        UrgencyGenerator** const generators = checkPointer(new UrgencyGenerator*[count]);
        UrgencyMerger* const merger = checkPointer(new UrgencyMerger(count, personality, generators));
        float* const weights = checkPointer(new float[count]);
        for (int i = 0; i < count; ++i)
            weights[i] = 1.0f;
        BotOrder* const order = checkPointer(new BotOrder(weights, static_cast<unsigned char>(
            BotBehaviourManager::m_instance->getBehaviourId("Idle"))));
        player->setViewDistance(static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getViewDistance());
        bot = checkPointer(new BotMain(player, static_cast<unsigned int>(m_bots.size()),
            personality, 1.0f, 1, order, new BotMemory(), 0, side, merger, generators));
    }
    else
    {
        bot = static_cast<BotMain*>(m_preallocatedBots.front());
        bot->setPlayer(player);
        bot->m_basicWeights = personality;
        bot->setSide(side);
        float* const weights = checkPointer(new float[count]);
        for (int i = 0; i < count; ++i)
            weights[i] = 1.0f;
        BotOrder* const order = checkPointer(new BotOrder(weights, static_cast<unsigned char>(
            BotBehaviourManager::m_instance->getBehaviourId("Idle"))));
        bot->recieveOrder(order);
        m_preallocatedBots.erase(m_preallocatedBots.begin());
    }

    bot->updateBotVehicle(0);
    bot->updateBot();
    Information* const information = informationManager->getInformation(bot->getCurrentEquipment());
    if (!information)
        return exitAddBot(playerId);
    information->setStrength(information->getStrength() + 1.0f);
    bot->setWaypoints(checkPointer(new WPEmpty(information->getPosition())));
    m_bots.push_back(checkPointer(new BotInfo(bot)));
    // shipped defect: grows to playerId, then writes element playerId; the first lookup is unchecked too.
    if (static_cast<int>(m_botByPlayerId.size()) < playerId)
        m_botByPlayerId.resize(playerId, -1);
    m_botByPlayerId[playerId] = static_cast<int>(bot->getBotId());
    m_lastCreatedBot = bot;
    return true;
}

// Admits an unspawned player using the first available preallocated bot.
// ELF 0x3DC300
bool BotManager::addUnspawnedBot(int playerId, int side, float* personality)
{
    m_lastCreatedBot = 0;
    const int count = static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)
                          ->getNumberOfBehaviours();
    BotMain* bot;

    if (m_preallocatedBots.empty())
    {
        UrgencyGenerator** const generators = checkPointer(new UrgencyGenerator*[count]);
        UrgencyMerger* const merger = checkPointer(new UrgencyMerger(count, personality, generators));
        float* const weights = checkPointer(new float[count]);
        for (int i = 0; i < count; ++i)
            weights[i] = 1.0f;
        BotOrder* const order = checkPointer(new BotOrder(weights, static_cast<unsigned char>(
            BotBehaviourManager::m_instance->getBehaviourId("Idle"))));
        IAIPlayer* const player = static_cast<IAIPlayerFactory*>(CommonSingletonServer::m_iaiPlayerFactory)
                                     ->getPlayerFromId(playerId);
        // BotMain storage is allocated before this nested BotMemory construction in both images.
        // Unlike the preallocation constructor, this path does not checkPointer the memory.
        bot = checkPointer(new BotMain(player, static_cast<unsigned int>(m_bots.size()),
            personality, 1.0f, 1, order, new BotMemory(), 0, side, merger, generators));
    }
    else
    {
        bot = static_cast<BotMain*>(m_preallocatedBots.front());
        bot->setPlayer(static_cast<IAIPlayerFactory*>(CommonSingletonServer::m_iaiPlayerFactory)
                           ->getPlayerFromId(playerId));
        bot->setSide(side);
        bot->m_basicWeights = personality;
        float* const weights = checkPointer(new float[count]);
        for (int i = 0; i < count; ++i)
            weights[i] = 1.0f;
        BotOrder* const order = checkPointer(new BotOrder(weights, static_cast<unsigned char>(
            BotBehaviourManager::m_instance->getBehaviourId("Idle"))));
        bot->recieveOrder(order);
        // Reload begin after the virtual calls, as retail does.
        m_preallocatedBots.erase(m_preallocatedBots.begin());
    }

    m_bots.push_back(checkPointer(new BotInfo(bot)));
    if (static_cast<int>(m_botByPlayerId.size()) < playerId)
        m_botByPlayerId.resize(playerId, -1);
    m_botByPlayerId[playerId] = static_cast<int>(bot->getBotId());
    m_lastCreatedBot = bot;
    return true;
}

// ELF 0x3DD240. Picks the first dead bot of the side, else the first bot of the side; with none
// the index stays 0 (the ELF's initial value) and record 0 is deactivated. Its name returns to
// the pool (IAISettingsModifier +0x94 setBotNameUnused), its id is queued first in the
// deactivated list, its player slot cleared, its player detached (IAIPlayer +0x14 setPlayer(0))
// and the bot marked deactivated. Both list inserts are at end() (the list header itself).
int BotManager::deactivateBot(int side)
{
    unsigned int index = 0;
    bool found = false;
    for (unsigned int i = 0; i < m_bots.size(); ++i)
    {
        Bot* const bot = m_bots[i]->m_bot;
        if (bot && bot->getSide() == side && bot->isDead())
        {
            index = i;
            found = true;
            break;
        }
    }
    if (!found)
    {
        for (unsigned int i = 0; i < m_bots.size(); ++i)
        {
            Bot* const bot = m_bots[i]->m_bot;
            if (bot && bot->getSide() == side)
            {
                index = i;
                break;
            }
        }
    }
    static_cast<IAISettingsModifier*>(CommonSingletonServer::m_iaiSettingsModifier)
        ->setBotNameUnused(m_bots[index]->m_bot->getPlayer()->getName());
    m_deactivatedBots.push_back(index);
    m_botByPlayerId[m_bots[index]->m_bot->getPlayerId()] = -1;
    m_bots[index]->m_bot->getPlayer()->setPlayer(static_cast<world::IPlayer*>(0));
    m_bots[index]->m_bot->setAsDeactivated(true);
    return static_cast<int>(index);
}

// ELF 0x3DD450. The name goes back to the pool before the test; only a dead bot is then
// deactivated (a live one answers -1).
int BotManager::deactivateBotFromPlayerId(int playerId)
{
    Bot* const bot = getBotFromPlayerId(playerId);
    if (!bot)
        return -1;
    static_cast<IAISettingsModifier*>(CommonSingletonServer::m_iaiSettingsModifier)
        ->setBotNameUnused(bot->getPlayer()->getName());
    const unsigned int botId = bot->getBotId();
    if (!bot->isDead())
        return -1;
    m_deactivatedBots.push_back(botId);
    bot->setAsDeactivated(true);
    bot->getPlayer()->setPlayer(static_cast<world::IPlayer*>(0));
    m_botByPlayerId[playerId] = -1;
    return static_cast<int>(botId);
}

// Reactivates a retired bot for a player and restores it to the active roster.
// ELF 0x3DD040
bool BotManager::reactivateBot(int playerId, int side, const std::string& name)
{
    if (m_deactivatedBots.empty())
        return false;

    // A second, redundant test. See the block above.
    if (m_deactivatedBots.size() == 0)
        return false;

    const std::list<unsigned int>::iterator oldest = m_deactivatedBots.begin();
    const unsigned int botId = *oldest;

    Bot* const bot = m_bots[botId]->m_bot;

    bot->getPlayer()->setPlayer(playerId);
    bot->setSide(side);
    bot->setName(name);
    bot->setAsDeactivated(false);

    // off by one - see the block above.
    if (static_cast<int>(m_botByPlayerId.size()) < playerId)
        m_botByPlayerId.resize(playerId, -1);

    m_botByPlayerId[playerId] = static_cast<int>(botId);

    IAIMain::m_instance->getSAI(bot->getSide())->addBot(bot);

    m_deactivatedBots.erase(oldest);

    return true;
}

// ELF 0x3DB120: the object map's bot id, then that record's bot (a null record answers null).
Bot* BotManager::getBot(IAIObject* object) const
{
    const std::map<IAIObject*, unsigned int>::const_iterator it = m_vehicleBots.find(object);
    if (it == m_vehicleBots.end())
        return 0;
    BotInfo* const info = m_bots[it->second];
    return info ? info->m_bot : 0;
}

// ELF 0x3E0C90
Bot* BotManager::getLastCreatedBot() const
{
    // return [this+228]. PE uses the corresponding member at +276.
    return m_lastCreatedBot;
}

// ELF 0x3E0100: drops the first map entry that carries this bot's id, then maps the object to
// it (insert_unique: an object already mapped keeps its old bot).
void BotManager::updateBotVehicle(Bot* bot, IAIObject* vehicle)
{
    const unsigned int botId = bot->getBotId();
    for (std::map<IAIObject*, unsigned int>::iterator it = m_vehicleBots.begin(); it != m_vehicleBots.end(); ++it)
    {
        if (it->second == botId)
        {
            m_vehicleBots.erase(it);
            break;
        }
    }
    m_vehicleBots.insert(std::pair<IAIObject* const, unsigned int>(vehicle, botId));
}

// ELF 0x3E0210. A physical vehicle first gets IAIObjectPhysical +0x74 enablePhysics(its own
// side) - the plug-in is fetched before the side. A bot outside its transition phase and alive
// then refreshes its vehicle (+0x2C, null out-flag) and itself (+0x30), drops its plan (+0x5C),
// and, with the SAI enabled, reports the equipment change to its side's SAI (+0x40, bot id
// read before the side).
void BotManager::setBotChangedVehicle(int playerId, IAIObject* vehicle)
{
    Bot* const bot = getBotFromPlayerId(playerId);
    if (!bot)
        return;
    if (vehicle && vehicle->isPhysical())
    {
        IAIObjectPhysical* const physical = vehicle->getPhysical();
        physical->enablePhysics(vehicle->getSide());
    }
    if (bot->isInTransitionPhase())
        return;
    if (bot->isDead())
        return;
    bot->updateBotVehicle(0);
    bot->updateBot();
    bot->invalidatePlan();
    if (m_aiMain->getSAIEnable())
    {
        const unsigned int botId = bot->getBotId();
        m_aiMain->getSAI(bot->getSide())->setBotHasChangedEquipment(botId);
    }
}

// ELF 0x3E0340. The first object-map entry whose bot id equals the player id is dropped (the
// ELF compares the mapped bot id with the player id). The bot leaves man-down, enters its
// transition phase, resets navigation and its assigned areas (BotMain +0x214, past the Bot
// interface), its record's +4 word becomes 1 and the bot resets; equipment is cleared (-1),
// placed C4 removed, and the death callback (+0x11C) reports.
// The side's SAI is fetched (IAIMain +0x3C) and not used, as in the ELF.
void BotManager::setBotIsDead(int playerId)
{
    Bot* const bot = getBotFromPlayerId(playerId);
    if (!bot)
        return;
    for (std::map<IAIObject*, unsigned int>::iterator it = m_vehicleBots.begin(); it != m_vehicleBots.end(); ++it)
    {
        if (it->second == static_cast<unsigned int>(playerId))
        {
            m_vehicleBots.erase(it);
            break;
        }
    }
    bot->setManDown(false);
    bot->setIsInTransitionPhase(true);
    bot->getBotNavigation()->resetAll();
    static_cast<BotMain*>(bot)->resetAssignedAreas();
    BotInfo* const info = m_bots[bot->getBotId()];
    IAIMain::m_instance->getSAI(info->m_bot->getSide());
    info->m_word4 = 1;
    info->m_bot->reset();
    bot->setCurrentEquipment(static_cast<unsigned int>(-1));
    if (bot->isC4Placed())
        bot->removeC4();
    IBotDeathCallBack* const callBack = bot->getDeathCallBack();
    if (callBack)
        callBack->reportDeath();
}

// ELF 0x3E0510. Only a bot in its transition phase: the phase ends, vehicle and bot refresh,
// the last change time becomes -1000000.0 (float constant, ELF 0x49CDA4), the record's
// priorities, +0x18 double and +0x14 flag clear, the spawn coordinate is taken from the
// bot's information (Information +0x30 getPosition) and the spawn callback reports.
void BotManager::setBotIsAlive(int playerId)
{
    Bot* const bot = getBotFromPlayerId(playerId);
    if (!bot || !bot->isInTransitionPhase())
        return;
    bot->setIsInTransitionPhase(false);
    bot->updateBotVehicle(0);
    bot->updateBot();
    bot->setLastChangeTime(-1000000.0f);
    BotInfo* const info = m_bots[bot->getBotId()];
    info->m_priority = 0;
    info->m_double24 = 0.0f;
    info->m_word4 = 0;
    info->m_flag20 = false;
    Information* const information = informationManager->getInformation(bot->getCurrentEquipment());
    if (information)
        static_cast<BotMain*>(bot)->setSpawnCoord(information->getPosition());
    IBotSpawnCallBack* const callBack = bot->getSpawnCallBack();
    if (callBack)
        callBack->reportSpawn();
}

// ELF 0x3E0640: the object is not read.
void BotManager::setBotIsManDown(int playerId, IAIObject*)
{
    Bot* const bot = getBotFromPlayerId(playerId);
    if (!bot)
        return;
    bot->setManDown(true);
    bot->updateBot();
    bot->updateBotVehicle(0);
    bot->invalidatePlan();
    bot->getBotNavigation()->resetAll();
}

// ELF 0x3E06C0. The revived bot leaves man-down, refreshes, resets navigation (reset, not
// resetAll), drops its plan, queues radio message 8 on channel 6 (args 2, 1.0f - ELF
// 0x49CB2C - and -1), and with the SAI enabled reports an equipment change (bot id first).
void BotManager::setBotIsRevived(int playerId, IAIObject*)
{
    Bot* const bot = getBotFromPlayerId(playerId);
    if (!bot)
        return;
    bot->setManDown(false);
    bot->updateBot();
    bot->getBotNavigation()->reset();
    bot->invalidatePlan();
    bot->getCommunicationAgent()->enqueueRadioMessage(static_cast<AIRadioMessageType>(8),
                                                       static_cast<AIRadioChannelType>(6), 2u, 1.0f, -1);
    if (m_aiMain->getSAIEnable())
    {
        const unsigned int botId = bot->getBotId();
        m_aiMain->getSAI(bot->getSide())->setBotHasChangedEquipment(botId);
    }
}

// ELF 0x3E07C0: the third argument names the receiving player.
void BotManager::setBotReceivedRadioMessage(const AIRadioMessageSettings& settings, int a, int playerId)
{
    Bot* const bot = getBotFromPlayerId(playerId);
    if (!bot)
        return;
    bot->getCommunicationAgent()->receiveRadioMessage(settings, a, false);   // ELF 0x3E080E
}

// ELF 0x3E0820: the bot's squad (+0x34 getBotSquad) receives the order.
void BotManager::setBotRecievedAttackCommanderOrder(int playerId)
{
    Bot* const bot = getBotFromPlayerId(playerId);
    if (bot)
        bot->getBotSquad()->recieveAttackCommanderOrder();
}

// ELF 0x3E0870: the bot's squad (+0x34 getBotSquad) receives the order.
void BotManager::setBotRecievedDefendCommanderOrder(int playerId)
{
    Bot* const bot = getBotFromPlayerId(playerId);
    if (bot)
        bot->getBotSquad()->recieveDefendCommanderOrder();
}

// ELF 0x3E08C0: the bot's squad (+0x34 getBotSquad) receives the order.
void BotManager::setBotRecievedMoveCommanderOrder(int playerId)
{
    Bot* const bot = getBotFromPlayerId(playerId);
    if (bot)
        bot->getBotSquad()->recieveMoveCommanderOrder();
}

// ELF 0x3E0910: the bot's squad (+0x34 getBotSquad) receives the order.
void BotManager::setBotRecievedAttackSquadOrder(int playerId)
{
    Bot* const bot = getBotFromPlayerId(playerId);
    if (bot)
        bot->getBotSquad()->recieveAttackSquadOrder();
}

// ELF 0x3E0960: the bot's squad (+0x34 getBotSquad) receives the order.
void BotManager::setBotRecievedDefendSquadOrder(int playerId)
{
    Bot* const bot = getBotFromPlayerId(playerId);
    if (bot)
        bot->getBotSquad()->recieveDefendSquadOrder();
}

// ELF 0x3E09B0: the bot's squad (+0x34 getBotSquad) receives the order.
void BotManager::setBotRecievedMoveSquadOrder(int playerId)
{
    Bot* const bot = getBotFromPlayerId(playerId);
    if (bot)
        bot->getBotSquad()->recieveMoveSquadOrder();
}

// ELF 0x3E0A00: the second argument is not read.
void BotManager::setBotTeamKilled(int playerId, int)
{
    Bot* const bot = getBotFromPlayerId(playerId);
    if (!bot)
        return;
    bot->getCommunicationAgent()->enqueueRadioMessage(static_cast<AIRadioMessageType>(9),
                                                       static_cast<AIRadioChannelType>(6), 3u, 0.0f, -1);
}

// ELF 0x3E0AC0: unchecked index; updateDamages applies and clears the sum each tick.
void BotManager::registerDamageOnBot(float damage, int botIndex, int attackerIndex)
{
    m_bots[botIndex]->m_damage += damage;
    m_bots[botIndex]->m_attackerIndex = attackerIndex;
}

// ELF 0x3DAD10: `flds 0xE8(this)`.
float BotManager::getVehicleBailValue()
{
    return m_float232;
}

// ELF 0x3DB280: every bot targeting the mine (BotMain +0x1FC) gets target -1 (+0x1F8).
void BotManager::removeMineTarget(unsigned int handle)
{
    for (std::vector<BotInfo*>::iterator it = m_bots.begin(); it != m_bots.end(); ++it)
    {
        BotMain* const bot = static_cast<BotMain*>((*it)->m_bot);
        if (bot->getMineTarget() == handle)
            bot->setMineTarget(static_cast<unsigned int>(-1));
    }
}

// ELF 0x3DFCE0 (1056 B; recovered on the client track, docs/macos-c-singleplayer.md). Clears the
// plan cursor, deletes every bot record (its bot through the virtual deleting destructor, +4,
// then the record) and empties the record vector and the bot list; frees the two priority
// queues and empties the +56 list; re-applies the construction-time scheduling defaults through
// the virtual setters (+0x8C..+0xA4); allocates two empty priority queues; clears +188.
// The sensing queue is left alone. Returns 1.
bool BotManager::reset()
{
    m_planCursor = 0;                                                         // +0x104
    for (std::vector<BotInfo*>::iterator info = m_bots.begin(); info != m_bots.end(); ++info)
    {
        if (*info)
        {
            if ((*info)->m_bot)
                delete (*info)->m_bot;                                        // virtual, slot +4
            delete *info;
        }
    }
    m_bots.resize(0, 0);
    m_preallocatedBots.resize(0, 0);
    delete m_planned;
    delete m_unplanned;
    m_list56.clear();                                                         // +0x38
    setLodLevelTicks(3, 2, 1);                                                // +0x8C
    setLodLevelPriority(3, 2, 1);                                             // +0x90
    setPlannedDecisionMakingThreshold(0.5, 1.0, 3.0);                         // +0x94
    setUnplannedDecisionMakingThreshold(0.3, 0.8, 2.0);                       // +0x98
    setDecisionMakingInterleave(2, 2);                                        // +0x9C
    setSensingQuotient(1, 1);                                                 // +0xA0
    setSystemQuotient(30, 40, 30);                                            // +0xA4
    m_planned = new BotQueue;
    m_unplanned = new BotQueue;
    m_hearingResumeId = 0;                                                    // +0xBC
    return true;
}

// ELF 0x3E2CC0 (weak): `movb %dl, 0xEC(this)`.
void BotManager::setRenderBotSensing(bool render)
{
    m_flag236 = render;
}

// ELF 0x3E2CE0 (weak): `movzbl 0xEC(this)`.
bool BotManager::getRenderBotSensing() const
{
    return m_flag236;
}

// ELF 0x3E0CA0
void BotManager::writeBotPlans()
{
    s_tabLevel = 0;
    const std::string levelPath = static_cast<IAIGame*>(CommonSingletonServer::m_iaiGame)->getLevelPath();
    io::IStream* file = io::fileManager->open(levelPath + "../../../AI/BotPlans.cpp",
        static_cast<io::FileMode>(0), static_cast<io::FileAccess>(2), 0);
    for (std::vector<BotInfo*>::iterator it = m_bots.begin(); it != m_bots.end(); ++it)
    {
        Bot* const bot = (*it)->m_bot;
        if (bot->isDead())
            continue;
        BotScript* const plan = bot->getPlan();
        if (!file)
            continue;
        Information* const information = informationManager->getInformation(bot->getCurrentEquipment());
        std::ostringstream out;
        out << "/*******************************************************/" << std::endl;
        const std::string& name = bot->getPlayer()->getName();
        out << "/* Name: " << name << std::endl;
        // shipped defect: a missing information record is dereferenced, and a null plan is not checked.
        const std::string& equipment = information->getAIObject()->getObject()->getTemplate()->getName();
        out << "/* Equipment: " << equipment << "    Pos: ";
        const Vec3& position = information->getPosition();
        out << position.x << '/';
        out << position.y << '/';
        out << position.z << "  */" << std::endl;
        out << "/*******************************************************/" << std::endl;
        out << *static_cast<BAPStatement*>(plan) << std::endl;
        const std::string lengthSource = out.str();
        const std::string text = out.str();
        file->write(text.c_str(), static_cast<unsigned int>(lengthSource.size()));
    }
    if (file)
        file->release();
}

// ELF 0x3E2CF0 / 0x3E2D10 (weak): `incl` / `decl` of the class static.
void BotManager::increaseTabLevel()
{
    ++s_tabLevel;
}

// ELF 0x3E2D10
void BotManager::decreaseTabLevel()
{
    --s_tabLevel;
}

// ELF 0x3E1410
void BotManager::tabulate(std::ostream& out)
{
    for (int i = 0; i < s_tabLevel; ++i)
        out << '\t';
}

// ELF 0x3DAF10: `movl 0x84(this)`.
unsigned BotManager::getLodLevelFullTicks() const
{
    return m_lodLevelFullTicks;
}

// ELF 0x3DAF20: `movl 0x88(this)`.
unsigned BotManager::getLodLevelMediumTicks() const
{
    return m_lodLevelMediumTicks;
}

// ELF 0x3DAF30: `movl 0x8C(this)`.
unsigned BotManager::getLodLevelLightTicks() const
{
    return m_lodLevelLightTicks;
}

// ELF 0x3DAF40: `movl 0x40(this)`.
unsigned BotManager::getLodLevelFullPriority() const
{
    return m_lodLevelFullPriority;
}

// ELF 0x3DAF50: `movl 0x44(this)`.
unsigned BotManager::getLodLevelMediumPriority() const
{
    return m_lodLevelMediumPriority;
}

// ELF 0x3DAF60: `movl 0x48(this)`.
unsigned BotManager::getLodLevelLightPriority() const
{
    return m_lodLevelLightPriority;
}

// ELF 0x3DAF70: `fldl 0x54(this)`.
double BotManager::getPlannedDecisionMakingThresholdFull() const
{
    return m_plannedDecisionMakingThresholdFull;
}

// ELF 0x3DAF80: `fldl 0x5C(this)`.
double BotManager::getPlannedDecisionMakingThresholdMedium() const
{
    return m_plannedDecisionMakingThresholdMedium;
}

// ELF 0x3DAF90: `fldl 0x64(this)`.
double BotManager::getPlannedDecisionMakingThresholdLight() const
{
    return m_plannedDecisionMakingThresholdLight;
}

// ELF 0x3DAFA0: `fldl 0x6C(this)`.
double BotManager::getUnplannedDecisionMakingThresholdFull() const
{
    return m_unplannedDecisionMakingThresholdFull;
}

// ELF 0x3DAFB0: `fldl 0x74(this)`.
double BotManager::getUnplannedDecisionMakingThresholdMedium() const
{
    return m_unplannedDecisionMakingThresholdMedium;
}

// ELF 0x3DAFC0: `fldl 0x7C(this)`.
double BotManager::getUnplannedDecisionMakingThresholdLight() const
{
    return m_unplannedDecisionMakingThresholdLight;
}

// ELF 0x3DAFD0: `movl 0x4C(this)`.
unsigned BotManager::getPlannedDecisionMakingInterleave() const
{
    return m_plannedDecisionMakingInterleave;
}

// ELF 0x3DAFE0: `movl 0x50(this)`.
unsigned BotManager::getUnplannedDecisionMakingInterleave() const
{
    return m_unplannedDecisionMakingInterleave;
}

// ELF 0x3DAFF0: `fldl 0xA0(this)`.
double BotManager::getSensingQuotient() const
{
    return m_sensingQuotient;
}

// ELF 0x3DB000: `fldl 0xA8(this) - the sensing split's second share`.
double BotManager::getMemoryUpdateQuotient() const
{
    return m_sensingQuotientInverse;
}

// ELF 0x3DB010: `fldl 0x90(this)`.
double BotManager::getSystemDecisionMakingQuotient() const
{
    return m_systemDecisionMakingQuotient;
}

// ELF 0x3DB020: `fldl 0x98(this) - setSystemQuotient's second share`.
double BotManager::getSystemPathfindingQuotient() const
{
    return m_systemQuotient2;
}

// ELF 0x3DB030: the third share setSystemQuotient does not store, `1.0f - [0x98] - [0x90]`
// (1.0f at ELF 0x49CB2C, evaluated left to right in x87).
double BotManager::getSystemSensingQuotient() const
{
    return (1.0f - m_systemQuotient2) - m_systemDecisionMakingQuotient;
}

// ELF 0x3DFA80: every record's four counters (+0x20..+0x2C) and three run times (+0x30..+0x40).
void BotManager::resetStatistics()
{
    for (std::vector<BotInfo*>::iterator it = m_bots.begin(); it != m_bots.end(); ++it)
    {
        BotInfo* const info = *it;
        info->m_behaviourUpdateCount = 0;
        info->m_word36 = 0;
        info->m_word40 = 0;
        info->m_word44 = 0;
        info->m_double48 = 0.0f;
        info->m_double56 = 0.0f;
        info->m_double64 = 0.0f;
    }
}

// ELF 0x3DFAF0: the same for one unchecked index (the records are reached through pointers).
void BotManager::resetStatistics(unsigned int botIndex) const
{
    BotInfo* const info = m_bots[botIndex];
    info->m_behaviourUpdateCount = 0;
    info->m_word36 = 0;
    info->m_word40 = 0;
    info->m_word44 = 0;
    info->m_double48 = 0.0f;
    info->m_double56 = 0.0f;
    info->m_double64 = 0.0f;
}

// ELF 0x3DFB50 (+0x24), unchecked index.
unsigned BotManager::getPlanExecutionUpdateCount(unsigned int botIndex) const
{
    return m_bots[botIndex]->m_word36;
}

// ELF 0x3DFB70 (+0x28), unchecked index.
unsigned BotManager::getDecisionMakingUpdateCount(unsigned int botIndex) const
{
    return m_bots[botIndex]->m_word40;
}

// ELF 0x3DFB90 (+0x2C), unchecked index.
unsigned BotManager::getSensingUpdateCount(unsigned int botIndex) const
{
    return m_bots[botIndex]->m_word44;
}

// ELF 0x3DFCC0 (+0x20), unchecked index.
unsigned BotManager::getDecisionMakingBehaviourUpdateCount(unsigned int botIndex) const
{
    return m_bots[botIndex]->m_behaviourUpdateCount;
}

// ELF 0x3DFBB0 (+0x30), unchecked index.
double BotManager::getPlanExecutionRunTime(unsigned int botIndex) const
{
    return m_bots[botIndex]->m_double48;
}

// ELF 0x3DFBD0 (+0x38), unchecked index.
double BotManager::getDecisionMakingRunTime(unsigned int botIndex) const
{
    return m_bots[botIndex]->m_double56;
}

// ELF 0x3DFBF0 (+0x40), unchecked index.
double BotManager::getSensingRunTime(unsigned int botIndex) const
{
    return m_bots[botIndex]->m_double64;
}

// ELF 0x3DFC10 (+0x14), unchecked index.
bool BotManager::isPathfinding(unsigned int botIndex) const
{
    return m_bots[botIndex]->m_flag20;
}

// ELF 0x3DFC40 (+0x48), unchecked index.
bool BotManager::getLastPathfindingSucceeded(unsigned int botIndex) const
{
    return m_bots[botIndex]->m_flag72;
}

// ELF 0x3DFC70 (+0x49), unchecked index.
bool BotManager::getPathfindingStall(unsigned int botIndex) const
{
    return m_bots[botIndex]->m_flag73;
}

// ELF 0x3DFCA0 (+0x4C), unchecked index.
double BotManager::getLastPathfindingRunTime(unsigned int botIndex) const
{
    return m_bots[botIndex]->m_double76;
}

// ELF 0x3E0C40
void BotManager::enableBotScriptDebugger(bool enable)
{
    for (std::vector<BotInfo*>::iterator it = m_bots.begin(); it != m_bots.end(); ++it)
        (*it)->m_bot->enableBotScriptDebugger(enable);
}

// ELF 0x3E0AB0
void BotManager::buildObjectMap()
{
}

} // namespace ai
} // namespace hfe
} // namespace dice
