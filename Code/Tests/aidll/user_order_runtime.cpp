// [TEMP] [CHOSEN, not recovered] Selector-only fixture. The manager and its six predicates
// and destruction are production code. Test constructors create ordinary objects with native STL
// lifetimes solely for arranging inputs; they do not recover construction or bot callbacks.
#include <map>
#include <list>
#include <string>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <type_traits>
#include <new>
#include <windows.h>
#include <crtdbg.h>
#define private public
#include "../../BF2/AI/AIDLL/AI/AIStrategicEntity/UserOrders/UserOrderManager.h"
#include "../../BF2/AI/AIDLL/AI/AIStrategicEntity/GuardPosition.h"
#include "../../BF2/AI/AIDLL/AI/AIStrategicEntity/PatrolPath.h"
#undef private

namespace dice { namespace hfe { namespace ai {
// Test-only definitions of the production declarations. No production constructor is linked.
GuardPosition::GuardPosition(const Vec2&, CombatStrengthType type, int side, int value)
    : m_position(), m_rotation(), m_strengthType(type), m_active(true), m_side(side),
      m_value36(value), m_minDistance(0), m_bot(0), m_flag48(false) {}
PatrolPath::PatrolPath(CombatStrengthType type, int side)
    : m_strengthType(type), m_active(true), m_side(side), m_currentResources(0),
      m_requiredResources(1), m_minDistance(0), m_patrolSpeed(0) {}
} } }

using namespace dice::hfe;
using namespace dice::hfe::ai;
static_assert(sizeof(void*) == 4, "Run this fixture with MSVC x86");
static_assert(!std::is_polymorphic<GuardPosition>::value, "GuardPosition has no vtable");
static_assert(!std::is_polymorphic<PatrolPath>::value, "PatrolPath has no vtable");

// Test allocation hooks observe production delete expressions without replacing any destructor.
// These malloc/free-backed replaceable operators are linked only into this fixture executable.
static void* watched[4];
static int deletionOrder[4], deletionCount;
static UserOrderManager* expectedOwner;
static bool ownerClearedTooEarly;
void* operator new(std::size_t bytes)
{
    if (void* p = std::malloc(bytes ? bytes : 1)) return p;
    throw std::bad_alloc();
}
void operator delete(void* p) noexcept
{
    for (int i = 0; i != 4; ++i)
        if (p && p == watched[i])
        {
            if (deletionCount < 4) deletionOrder[deletionCount] = i;
            ++deletionCount;
            if (UserOrderManager::m_instance != expectedOwner) ownerClearedTooEarly = true;
            watched[i] = 0;
        }
    std::free(p);
}
void operator delete(void* p, std::size_t) noexcept { ::operator delete(p); }

static int ownershipCase()
{
    Vec2 position = {};
    const CombatStrengthType type = static_cast<CombatStrengthType>(0);
    UserOrderManager* const owner = new UserOrderManager;
    expectedOwner = owner;
    UserOrderManager* const secondary = new UserOrderManager;
    if (UserOrderManager::m_instance != owner) return 80;
    delete secondary;
    if (UserOrderManager::m_instance != owner) return 81;

    PatrolPath* const firstPatrol = new PatrolPath(type, 2);
    PatrolPath* const lastPatrol = new PatrolPath(type, 2);
    GuardPosition* const firstGuard = new GuardPosition(position, type, 2, 0);
    GuardPosition* const lastGuard = new GuardPosition(position, type, 2, 0);
    watched[0] = firstPatrol;
    watched[1] = lastPatrol;
    watched[2] = firstGuard;
    watched[3] = lastGuard;
    // Native member storage is populated so production member-only destructors do real work.
    firstPatrol->m_path.resize(3);
    firstPatrol->m_bots.push_back(0);
    firstGuard->m_areaConditions.resize(1);
    firstGuard->m_areaConditions[0].areaName.assign(100, 'a');
    owner->m_patrolPaths["z"] = lastPatrol;
    owner->m_patrolPaths["a"] = firstPatrol;
    owner->m_patrolPaths["m-null"] = 0;
    owner->m_guardPositions["z"] = lastGuard;
    owner->m_guardPositions["a"] = firstGuard;
    owner->m_guardPositions["m-null"] = 0;
    delete owner;
    if (UserOrderManager::m_instance || ownerClearedTooEarly || deletionCount != 4) return 82;
    for (int i = 0; i != 4; ++i) if (deletionOrder[i] != i) return 83;
    UserOrderManager* const replacement = new UserOrderManager;
    if (UserOrderManager::m_instance != replacement) return 84;
    delete replacement;
    if (UserOrderManager::m_instance) return 85;
    std::puts("PASS ownership: ordered pointee deletes, singleton timing and replacement");
    return 0;
}

int main(int argc, char** argv)
{
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    _CrtSetReportMode(_CRT_ASSERT, 0);
    if (argc != 2) return 2;
    const char* const mode = argv[1];
    if (std::strcmp(mode, "ownership") == 0) return ownershipCase();
    UserOrderManager manager;
    Vec2 position = {};
    const CombatStrengthType type = static_cast<CombatStrengthType>(0);
    GuardPosition guard(position, type, 2, 0), rejectedGuard(position, type, 2, 0);
    PatrolPath patrol(type, 2), rejectedPatrol(type, 2);
    bool expectsStop = false;

    if (std::strcmp(mode, "empty") == 0) {}
    else if (std::strncmp(mode, "guard_", 6) == 0)
    {
        manager.m_guardPositions["selected"] = &guard;
        if (std::strcmp(mode, "guard_inactive") == 0) guard.m_active = false;
        else if (std::strcmp(mode, "guard_wrong_side") == 0) guard.m_side = 1;
        else if (std::strcmp(mode, "guard_waiting") == 0) guard.m_flag48 = true;
        else if (std::strcmp(mode, "guard_occupied") == 0)
            guard.m_bot = reinterpret_cast<Bot*>(0x12345678); // Compared only; never dereferenced.
        else if (std::strcmp(mode, "guard_selected") == 0) expectsStop = true;
        else if (std::strcmp(mode, "guard_priority") == 0)
        {
            manager.m_patrolPaths["selected"] = &patrol;
            expectsStop = true;
        }
        else if (std::strcmp(mode, "guard_after_rejected") == 0)
        {
            rejectedGuard.m_active = false;
            manager.m_guardPositions["first"] = &rejectedGuard;
            expectsStop = true;
        }
        else return 2;
    }
    else if (std::strncmp(mode, "patrol_", 7) == 0)
    {
        manager.m_patrolPaths["selected"] = &patrol;
        if (std::strcmp(mode, "patrol_inactive") == 0) patrol.m_active = false;
        else if (std::strcmp(mode, "patrol_wrong_side") == 0) patrol.m_side = 1;
        else if (std::strcmp(mode, "patrol_full") == 0) patrol.m_currentResources = 1;
        else if (std::strcmp(mode, "patrol_overfull") == 0) patrol.m_currentResources = 2;
        else if (std::strcmp(mode, "patrol_selected") == 0) expectsStop = true;
        else if (std::strcmp(mode, "patrol_after_rejected") == 0)
        {
            rejectedGuard.m_flag48 = true;
            rejectedPatrol.m_side = 1;
            manager.m_guardPositions["guard"] = &rejectedGuard;
            manager.m_patrolPaths["first"] = &rejectedPatrol;
            expectsStop = true;
        }
        else return 2;
    }
    else if (std::strcmp(mode, "exhausted") == 0)
    {
        guard.m_active = false;
        rejectedGuard.m_side = 1;
        patrol.m_currentResources = 2;
        rejectedPatrol.m_active = false;
        manager.m_guardPositions["first"] = &guard;
        manager.m_guardPositions["second"] = &rejectedGuard;
        manager.m_patrolPaths["first"] = &patrol;
        manager.m_patrolPaths["second"] = &rejectedPatrol;
    }
    else return 2;

    const bool result = manager.recruitBot(0, 2); // Retail exhausted paths never dereference bot.
    // Selector inputs above use stack objects; detach fixture references before owning teardown.
    manager.m_guardPositions.clear();
    manager.m_patrolPaths.clear();
    if (expectsStop)
    {
        std::fputs("UNEXPECTED_RETURN_AFTER_SELECTED_RECRUIT\n", stderr);
        return 77;
    }
    if (result) return 78;
    std::printf("PASS %s: exhausted selector returned false\n", mode);
    return 0;
}
