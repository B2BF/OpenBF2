// Source placement: AIDLL_r.dll __FILE__ "Code\BF2\AI\AIDLL\AI\AIStrategicEntity\UserOrders\UserOrderManager.cpp" (asserts in 0x1007C7E0 / 0x1007C920).
// static init: ELF 0x3521D0 (__tcf_0 0x352230, _GLOBAL__I 0x352260): <iostream>'s Init object only.
#include <iostream>

#include "UserOrderManager.h"

#include "../../AIUtils/checkPointer.h"
#include "../GuardPosition.h"
#include "../PatrolPath.h"

namespace dice {
namespace hfe {
namespace ai {

// ds:, plain .bss.
UserOrderManager* UserOrderManager::m_instance = 0;

// ELF 0x351510 / 0x351690. Initializes maps and registers the first order manager.
UserOrderManager::UserOrderManager()
    : m_activePatrolPath(0)
    , m_patrolCursor(m_patrolPaths.end())
    , m_guardCursor(m_guardPositions.end())
    , m_updatePhase(0)
{
    // m_activeGuardPosition is deliberately not initialised here. See above.

    if (!m_instance)
        m_instance = this;
}

// ELF 0x351750 / 0x351900. Deletes patrol paths before guard positions, advancing after deletion
// and re-reading end(). Clears a matching singleton after pointee cleanup; map members
// then release their nodes in reverse declaration order.
UserOrderManager::~UserOrderManager()
{
    for (std::map<std::string, PatrolPath*>::iterator i = m_patrolPaths.begin();
         i != m_patrolPaths.end(); ++i)
        delete i->second;

    for (std::map<std::string, GuardPosition*>::iterator i = m_guardPositions.begin();
         i != m_guardPositions.end(); ++i)
        delete i->second;

    if (m_instance == this)
        m_instance = 0;
}

// the four accessors, 10- each
// Each hands out a reference or a raw pointer straight into a member, with no copy and no guard.

// ELF 0x351D30.
PatrolPath* UserOrderManager::getActivePatrolPath() const
{
    return m_activePatrolPath;
}

// ELF 0x351FE0.
GuardPosition* UserOrderManager::getActiveGuardPosition() const
{
    // Indeterminate until something assigns it - see the constructor.
    return m_activeGuardPosition;
}

// ELF 0x3520F0.
const std::map<std::string, PatrolPath*>& UserOrderManager::getPatrolPaths() const
{
    return m_patrolPaths;
}

// ELF 0x351FD0.
const std::map<std::string, GuardPosition*>& UserOrderManager::getGuardPositions() const
{
    return m_guardPositions;
}

// ELF 0x352100. Each phase resets its exhausted cursor before advancing to the next phase.
void UserOrderManager::update()
{
    switch (m_updatePhase)
    {
    case 0:
        if (m_patrolCursor == m_patrolPaths.end())
        {
            m_patrolCursor = m_patrolPaths.begin();
            m_updatePhase = 1;
        }
        else
        {
            m_patrolCursor->second->update();
            ++m_patrolCursor;
        }
        break;
    case 1:
        if (m_guardCursor == m_guardPositions.end())
        {
            m_updatePhase = 2;
            m_guardCursor = m_guardPositions.begin();
        }
        else
        {
            m_guardCursor->second->update();
            ++m_guardCursor;
        }
        break;
    case 2:
        m_updatePhase = 0;
        break;
    }
}

// ELF 0x351AB0. A duplicate name still replaces the active pointer and leaks the new path.
void UserOrderManager::createPatrolPath(const std::string& name, CombatStrengthType strength, int side)
{
    m_activePatrolPath = checkPointer(new PatrolPath(strength, side));
    m_patrolPaths.insert(std::pair<const std::string, PatrolPath*>(name, m_activePatrolPath));
}

// ELF 0x351D40. Duplicate names likewise leave an unregistered active position.
void UserOrderManager::createGuardPosition(const std::string& name, const Vec2& position,
                                           CombatStrengthType strength, int side, int kit)
{
    m_activeGuardPosition = checkPointer(new GuardPosition(position, strength, side, kit));
    m_guardPositions.insert(std::pair<const std::string, GuardPosition*>(name, m_activeGuardPosition));
}

// ELF 0x351FF0. Guard positions take priority over patrol paths; maps are traversed in key order.
// Preserve short-circuit predicate order and reload end after each rejected entry.
// Only exhausted maps return false. Retail's selected arm calls recruit(bot) and returns true
// independent of that call's result.
bool UserOrderManager::recruitBot(Bot* bot, int side)
{
    for (std::map<std::string, GuardPosition*>::iterator i = m_guardPositions.begin();
         i != m_guardPositions.end(); ++i)
    {
        GuardPosition* const position = i->second;
        if (position->active() && position->getSide() == side && position->needBot())
        {
            position->recruit(bot);
            return true;
        }
    }

    for (std::map<std::string, PatrolPath*>::iterator i = m_patrolPaths.begin();
         i != m_patrolPaths.end(); ++i)
    {
        PatrolPath* const path = i->second;
        if (path->active() && path->getSide() == side && path->neededResources() > 0)
        {
            path->recruit(bot);
            return true;
        }
    }
    return false;
}

} // namespace ai
} // namespace hfe
} // namespace dice
