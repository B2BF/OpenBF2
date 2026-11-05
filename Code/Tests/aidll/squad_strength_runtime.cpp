// [TEMP] Actual selected bodies live in another TU. Native fixture constructors skip
// engine publication and formation work. The observer build tests only the manager body.
#include "squad_strength_runtime.h"
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <limits>
#include <string>
#include <type_traits>
#include <windows.h>
#include <crtdbg.h>

using namespace dice::hfe;
using namespace dice::hfe::ai;
static_assert(sizeof(void*) == 4, "MSVC x86 only");
static_assert(sizeof(StrategicStrengthType) == 4, "observed full-width enum");
static_assert(std::is_same<decltype(&SquadManager::getSquadStrengths),
    int(SquadManager::*)(int, int, StrategicStrengthType)>::value, "manager contract");
static_assert(std::is_same<decltype(&SquadCoordinator::getStrategicStrength),
    int(SquadCoordinator::*)(StrategicStrengthType)>::value, "coordinator contract");

static unsigned checks = 0;
[[noreturn]] static void unexpected(const char* message)
{
    std::fprintf(stderr, "FAIL %s\n", message); std::fflush(stderr); std::abort();
}
static void require(bool value, const char* message) { ++checks; if (!value) unexpected(message); }
static std::vector<SquadCoordinator*>* sides;
static std::vector<int> trace;
static std::function<void(int, int)> callback;
static bool printTrace = false;
static void record(int identity, int call)
{
    trace.push_back(identity * 10 + call);
    if (callback) callback(identity, call);
    if (printTrace && call == 3)
    {
        std::printf("TRACE");
        for (int item : trace) std::printf(" %d", item);
        std::printf("\n"); std::fflush(stdout);
    }
}
#include "squad_strength_mocks.inl"

namespace dice { namespace hfe { namespace ai {
// [CHOSEN, not recovered] Only a genuine nonnull identity for the stop test. This
// fixture-only completion has no interface or layout claim and is never dereferenced.
class IAIObject {};
SquadManager::SquadManager() { sides = m_Squads; }
SquadCoordinator::SquadCoordinator(int side, int squadIndex)
    : m_squadIndex(squadIndex), m_side(side), m_strategicAreaAssignment(nullptr),
      m_squadLeaderIndex(-1), m_formation(nullptr), m_missionStatus(0) {}
SquadMember::SquadMember(SquadCoordinator* squad, IAIPlayer* player, int squadIndex, int side, int memberIndex)
    : m_squadIndex(squadIndex), m_memberIndex(memberIndex), m_isSquadLeader(false),
      m_playerId(-1), m_player(player), m_bot(nullptr), m_squad(squad), m_side(side), m_request(nullptr) {}
SquadMember::~SquadMember() {}
IAIPlayer::~IAIPlayer() {}
AIStrategicObject::AIStrategicObject(float, float) {}
AIStrategicObject::~AIStrategicObject() {}
AIStrategicArea::AIStrategicArea(const std::string& name, unsigned int, float radius)
    : AIStrategicObject(radius, 0.0f), m_name(name) {}
AIStrategicArea::~AIStrategicArea() {}
} } }

#if defined(SQUAD_STRENGTH_OBSERVERS)
static std::function<int(SquadCoordinator*)> mission;
static std::function<bool(SquadCoordinator*, int)> assigned;
static std::function<int(SquadCoordinator*, StrategicStrengthType)> strength;
namespace dice { namespace hfe { namespace ai {
// [CHOSEN] Typed collaborators only in the manager-only executable. None is compiled
// into the all-actual helper executable, and the real manager is never shadowed.
int SquadCoordinator::getMissionStatus()
{ trace.push_back(m_squadIndex * 10 + 1); return mission(this); }
bool SquadCoordinator::isSquadAssignedToArea(int index)
{ trace.push_back(m_squadIndex * 10 + 2); return assigned(this, index); }
int SquadCoordinator::getStrategicStrength(StrategicStrengthType type)
{ trace.push_back(m_squadIndex * 10 + 3); return strength(this, type); }
} } }
#endif

int main(int argc, char** argv)
{
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    if (argc != 2) unexpected("mode required");
    const std::string mode(argv[1]);
    const StrategicStrengthType kind = static_cast<StrategicStrengthType>(1);
    SquadManager manager;
    SquadCoordinator a(0, 1), b(0, 2), c(1, 3), d(0, 4);
    AIStrategicArea area("fixture", 0, 0.0f);
    area.m_index = -123;

#if defined(SQUAD_STRENGTH_OBSERVERS)
    mission = [](SquadCoordinator*) { return 2; };
    assigned = [](SquadCoordinator*, int index) {
        require(index == -123, "full signed area index forwarded"); return true;
    };
    strength = [&](SquadCoordinator* squad, StrategicStrengthType type) {
        require(type == kind, "enum forwarded unchanged"); return squad == &a ? -17 : 45;
    };
    if (mode == "sum_filter")
    {
        sides[0] = {&a, &b, &c, &d}; sides[1] = {&a};
        mission = [&](SquadCoordinator* squad) { return squad == &b ? 258 : 2; };
        assigned = [&](SquadCoordinator* squad, int index) {
            require(index == -123, "signed area index"); return squad != &c;
        };
        require(manager.getSquadStrengths(1, -123, kind) == 28, "signed sum of qualifying values");
        require(trace == std::vector<int>({11,12,13,21,31,32,41,42,43}), "mission and assignment short circuit order");
        trace.clear();
        require(manager.getSquadStrengths(2, -123, kind) == -17, "one-based side two");
        require(trace == std::vector<int>({11,12,13}), "other side queried alone");
    }
    else if (mode == "reload")
    {
        sides[0] = {&a};
        mission = [&](SquadCoordinator* squad) {
            require(squad == &a, "first receiver");
            sides[0].reserve(sides[0].capacity() + 17); sides[0][0] = &b; return 2;
        };
        assigned = [&](SquadCoordinator* squad, int index) {
            require(squad == &b && index == -123, "receiver reloaded after mission");
            sides[0].reserve(sides[0].capacity() + 17); sides[0][0] = &c; return true;
        };
        strength = [&](SquadCoordinator* squad, StrategicStrengthType type) {
            require(squad == &c && type == kind, "receiver reloaded after assignment"); return -3456789;
        };
        require(manager.getSquadStrengths(1, -123, kind) == -3456789, "full signed result");
        require(trace == std::vector<int>({11,22,33}), "fresh indexed receivers");
    }
    else if (mode == "append")
    {
        sides[0] = {&a};
        strength = [&](SquadCoordinator* squad, StrategicStrengthType type) {
            require(type == kind, "type survives callbacks");
            if (squad == &a) { sides[0].push_back(&b); return 7; } return -19;
        };
        require(manager.getSquadStrengths(1, -123, kind) == -12, "live vector growth included");
        require(trace == std::vector<int>({11,12,13,21,22,23}), "append order");
    }
    else if (mode == "shrink")
    {
        sides[0] = {&a, &b};
        strength = [&](SquadCoordinator*, StrategicStrengthType) { sides[0].clear(); return 91; };
        require(manager.getSquadStrengths(1, -123, kind) == 91, "shrink retains current contribution");
        require(trace == std::vector<int>({11,12,13}), "fresh end skips removed squad");
    }
    else unexpected("unknown observer mode");
#else
    ObservedPlayer p1(1), p2(2), p3(3);
    SquadMember m1(&a, &p1, 0, 0, 0), m2(&a, &p2, 0, 0, 1), m3(&a, &p3, 0, 0, 2);
    a.m_squadMembers = {&m1};
    if (mode == "leaves")
    {
        a.m_missionStatus = (std::numeric_limits<int>::min)();
        require(a.getMissionStatus() == (std::numeric_limits<int>::min)(), "full-width status field");
        a.m_missionStatus = 258; require(a.getMissionStatus() == 258, "fresh status read");
        require(!a.isSquadAssignedToArea(-123), "null assignment false");
        a.m_strategicAreaAssignment = &area;
        require(a.isSquadAssignedToArea(-123), "signed area index match");
        require(!a.isSquadAssignedToArea(0), "area mismatch");
        area.m_index = (std::numeric_limits<int>::max)();
        require(a.isSquadAssignedToArea(area.m_index), "fresh full-width area index");
        require(a.m_missionStatus == 258 && trace.empty(), "leaf getters do not call players or mutate status");
    }
    else if (mode == "empty")
    {
        a.m_squadMembers.clear();
        require(a.getStrategicStrength(kind) == 0, "empty member strength");
        require(manager.getSquadStrengths(1,-123,kind) == 0, "empty side one");
        require(manager.getSquadStrengths(2,-123,kind) == 0, "empty side two");
        require(trace.empty(), "no player calls");
    }
    else if (mode == "filter")
    {
        sides[0] = {&a,&b,&c}; sides[1] = {&d};
        a.m_missionStatus = 258; a.m_strategicAreaAssignment = &area;
        b.m_missionStatus = 2; b.m_strategicAreaAssignment = nullptr;
        c.m_missionStatus = 2; c.m_strategicAreaAssignment = &area; c.m_squadMembers = {&m3};
        require(manager.getSquadStrengths(1,123,kind) == 0, "mission, null and nonmatching area filters");
        require(trace.empty(), "filtered squads never query player");
        require(manager.getSquadStrengths(1,-123,kind) == 0, "matching dead squad contributes zero");
        require(trace == std::vector<int>({31}), "only matching mission-2 squad queried");
        trace.clear(); d.m_missionStatus=2; d.m_strategicAreaAssignment=&area; d.m_squadMembers={&m2};
        require(manager.getSquadStrengths(2,-123,kind) == 0, "side two selected");
        require(trace == std::vector<int>({21}), "other side independent");
    }
    else if (mode == "order")
    {
        a.m_squadMembers = {&m1,&m2,&m3}; p2.alive=true; p2.down=true; p3.alive=true;
        require(a.getStrategicStrength(kind) == 0, "dead/down/null-vehicle paths all return rounded zero");
        require(trace == std::vector<int>({11,21,22,31,32,33}), "retail short circuit and callback order");
    }
    else if (mode == "retained_player")
    {
        p1.alive=true; p2.alive=true; p2.down=true;
        callback = [&](int id,int call) { if(id==1&&call==1) m1.m_player=&p2; };
        require(a.getStrategicStrength(kind) == 0, "retained player's null vehicle");
        require(trace == std::vector<int>({11,12,13}), "player captured once despite callback replacement");
        require(m1.m_player == &p2, "callback mutation persists");
    }
    else if (mode == "append")
    {
        callback = [&](int id,int call) { if(id==1&&call==1) a.m_squadMembers.push_back(&m2); };
        require(a.getStrategicStrength(kind) == 0, "fresh member bound includes append");
        require(trace == std::vector<int>({11,21}), "appended member visited");
    }
    else if (mode == "shrink")
    {
        a.m_squadMembers = {&m1,&m2}; p1.alive=true;
        callback = [&](int id,int call) { if(id==1&&call==1) a.m_squadMembers.clear(); };
        require(a.getStrategicStrength(kind) == 0, "fresh member bound observes shrink");
        require(trace == std::vector<int>({11,12,13}), "retained player completes while removed next member skipped");
    }
    else if (mode == "outer_append" || mode == "outer_shrink")
    {
        a.m_missionStatus=b.m_missionStatus=2; a.m_strategicAreaAssignment=b.m_strategicAreaAssignment=&area;
        b.m_squadMembers={&m2}; sides[0]={&a};
        if(mode=="outer_shrink") sides[0].push_back(&b);
        callback = [&](int id,int call) {
            if(id==1&&call==1) { if(mode=="outer_append") sides[0].push_back(&b); else sides[0].clear(); }
        };
        require(manager.getSquadStrengths(1,-123,kind)==0, "actual helper callback mutates manager vector");
        require(trace==(mode=="outer_append"?std::vector<int>({11,21}):std::vector<int>({11})), "manager reads current bound");
    }
    else if (mode == "frontier" || mode == "manager_frontier")
    {
        IAIObject identity; p1.alive=true; p1.vehicle=&identity; printTrace=true;
        if(mode=="manager_frontier")
        {
            a.m_missionStatus=2; a.m_strategicAreaAssignment=&area; sides[0]={&a};
            manager.getSquadStrengths(1,-123,kind);
        }
        else a.getStrategicStrength(kind);
        unexpected("frontier returned");
    }
    else unexpected("unknown actual mode");
#endif
    std::printf("PASS %s %u checks\n",mode.c_str(),checks);
    return 0;
}
