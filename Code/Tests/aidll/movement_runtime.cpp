// [TEMP] Exact production movement prefixes. Chosen native lifetimes establish real
// scalar/container storage. MOVEMENT_OBSERVERS replaces only three nonvirtual getters
// to observe prefix ordering; it does not claim those getters have retail callbacks.
#include "movement_runtime.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <string>
#include <type_traits>
#include <windows.h>
#include <crtdbg.h>
using namespace dice::hfe;
using namespace dice::hfe::ai;
static_assert(sizeof(void*) == 4, "MSVC x86 required");
static_assert(std::is_same<decltype(&SquadMember::moveWithFormation), void(SquadMember::*)()>::value, "movement contract");
static_assert(std::is_same<decltype(&SquadMember::moveWithoutFormation), void(SquadMember::*)()>::value, "movement contract");
static unsigned checks;
[[noreturn]] static void unexpected(const char* reason)
{ std::fprintf(stderr, "FAIL %s\n", reason); std::fflush(stderr); std::exit(2); }
static void require(bool condition, const char* reason)
{ ++checks; if (!condition) unexpected(reason); }
static std::string mode, calls;
static void trace(const std::string& value)
{ if (!calls.empty()) calls += ','; calls += value; }
static void verified(const char* expected)
{
    require(calls == expected, "exact prefix callback order");
    std::printf("VERIFIED %s\n", mode.c_str()); std::fflush(stdout);
}
static std::function<bool(int)> alive;
static std::function<bool(int)> down;
static std::function<IAIObject*(int)> vehicle;
static bool observeAlive(int id) { trace("alive" + std::to_string(id)); return alive(id); }
static bool observeDown(int id) { trace("down" + std::to_string(id)); return down(id); }
static IAIObject* observeVehicle(int id) { trace("vehicle" + std::to_string(id)); return vehicle(id); }
#include "mocks.inl"

static SquadMember* observedMember;
static SquadCoordinator* expectedSquad;
static SquadCoordinator* laterSquad;
static IAIPlayer* middlePlayer;
static IAIPlayer* laterPlayer;
namespace dice { namespace hfe { namespace ai {
// [CHOSEN, not recovered] Native lifetime only, no singleton publication or ownership claim.
SquadMember::SquadMember(SquadCoordinator* squad, IAIPlayer* player, int squadIndex, int side, int index)
    : m_squadIndex(squadIndex), m_memberIndex(index), m_isSquadLeader(true), m_playerId(-70),
      m_player(player), m_bot(nullptr), m_squad(squad), m_side(side), m_estimatedArrival(1.75f),
      m_time24(12.25), m_maxSpeed(4.25f), m_flag30(true), m_havePath(true), m_request(nullptr),
      m_int38(-7), m_position{2.0f, 3.0f, 4.0f}, m_positionValid(false), m_flag49(true),
      m_flag4A(false), m_flag4B(true), m_formationDistanceSq(800.0f), m_moved(true),
      m_vector54{5.0f, 6.0f, 7.0f}, m_vector60{8.0f, 9.0f, 10.0f} {}
SquadMember::~SquadMember() {}
SquadCoordinator::SquadCoordinator(int side, int index)
    : m_squadIndex(index), m_side(side), m_v28{21.0f, 22.0f, 23.0f}, m_formation(nullptr) {}
SquadFormation::SquadFormation(SquadCoordinator* squad)
    : m_coordinator(squad), m_numberOfSlots(0) {}
IAIPlayer::~IAIPlayer() {}

#if defined(MOVEMENT_OBSERVERS)
// [CHOSEN, not recovered] Separate executable observers, never mixed with the actual getters.
SquadFormation* SquadCoordinator::getSquadFormation()
{
    trace("formation");
    require(this == expectedSquad, "formation getter receiver");
    return m_formation;
}
bool SquadMember::getPosition(Vec3& position)
{
    trace("position");
    require(this == observedMember, "position receiver");
    require(position.x == 0 && position.y == 0 && position.z == 0, "caller zeroes position output");
    position = m_position;
    if (mode == "independent_mutation")
    { m_squad = laterSquad; m_player = middlePlayer; m_memberIndex = -999; }
    if (mode == "formation_order" || mode == "formation_invalid")
    {
        require(calls == "formation,position", "formation obtained before copied position");
        require(m_positionValid == (mode == "formation_order"), "both position validity states selected");
        verified("formation,position");
    }
    return m_positionValid;
}
const Vec3& SquadCoordinator::getSquadGotoPos()
{
    trace("goal");
    require(this == (mode == "independent_mutation" ? laterSquad : expectedSquad), "fresh squad after position callback");
    if (mode == "independent_mutation") observedMember->m_player = laterPlayer;
    return m_v28;
}
#endif
} } }

static void unchanged(const SquadMember& m)
{
    require(m.m_squadIndex == 4 && m.m_memberIndex == 7, "indices retained");
    require(m.m_isSquadLeader && m.m_playerId == -70, "leader/id retained");
    require(m.m_side == 1, "side retained");
    require(m.m_estimatedArrival == 1.75f && m.m_time24 == 12.25 && m.m_maxSpeed == 4.25f, "movement scalar values retained");
    require(m.m_flag30 && m.m_havePath && !m.m_request && m.m_int38 == -7, "request/path state retained");
    require(!m.m_positionValid && m.m_flag49 && !m.m_flag4A && m.m_flag4B, "position/path flags retained");
    require(m.m_formationDistanceSq == 800.0f && m.m_moved, "movement state retained");
    require(m.m_vector54.x == 5 && m.m_vector54.y == 6 && m.m_vector54.z == 7, "first previous vector retained");
    require(m.m_vector60.x == 8 && m.m_vector60.y == 9 && m.m_vector60.z == 10, "second previous vector retained");
}

int main(int argc, char** argv)
{
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    _CrtSetReportMode(_CRT_ASSERT, 0);
    if (argc != 2) unexpected("mode required");
    mode = argv[1];
    SquadCoordinator squad(1, 4), replacementSquad(0, 5);
    SquadFormation formation(&squad);
    squad.m_formation = &formation;
    ObservedPlayer player, second, third;
    second.identity = 1; third.identity = 2;
    SquadMember member(&squad, &player, 4, 1, 7);
    // Opaque identity tokens are only compared with null before the real frontier.
    // They are never dereferenced and make no Bot/IAIObject lifetime or ABI claim.
    static int botToken, vehicleToken;
    member.m_bot = reinterpret_cast<Bot*>(&botToken);
    observedMember = &member; expectedSquad = &squad; laterSquad = &replacementSquad;
    middlePlayer = &second; laterPlayer = &third;
    alive = [](int id) { require(id == 0, "initial player identity"); return false; };
    down = [](int) -> bool { unexpected("unselected man-down query"); };
    vehicle = [](int) -> IAIObject* { unexpected("unselected vehicle query"); };

    if (mode.rfind("mobile_", 0) == 0)
    {
        member.m_bot = nullptr;
        std::string expected = "vehicle0";
        vehicle = [&](int id) -> IAIObject* {
            require(id == 0, "mobile starts with vehicle, independent of bot");
            if (mode == "mobile_null_mutation") member.m_player = nullptr;
            if (mode == "mobile_no_vehicle" || mode == "mobile_null_mutation") return nullptr;
            if (mode == "mobile_fresh_players") member.m_player = &second;
            return reinterpret_cast<IAIObject*>(&vehicleToken);
        };
        if (mode != "mobile_no_vehicle" && mode != "mobile_null_mutation")
        {
            expected += mode == "mobile_fresh_players" ? ",alive1" : ",alive0";
            alive = [&](int id) {
                require(id == (mode == "mobile_fresh_players" ? 1 : 0), "fresh mobile alive player");
                if (mode == "mobile_fresh_players") member.m_player = &third;
                return mode != "mobile_dead";
            };
            if (mode != "mobile_dead")
            {
                expected += mode == "mobile_fresh_players" ? ",down2" : ",down0";
                down = [&](int id) {
                    require(id == (mode == "mobile_fresh_players" ? 2 : 0), "fresh mobile man-down player");
                    if (mode == "mobile_owner") { unchanged(member); verified("vehicle0,alive0,down0"); }
                    return mode != "mobile_owner";
                };
            }
        }
        const bool result = member.isMobile();
        require(!result, "actual mobile refusal is false");
        require(calls == expected, "mobile callback short-circuit order");
        unchanged(member);
    }
    else if (mode == "human")
    {
        member.m_bot = nullptr; member.m_player = nullptr; member.m_squad = nullptr;
        member.moveWithoutFormation();
        require(calls.empty(), "null bot returns before every collaborator");
        require(!member.m_bot && !member.m_player && !member.m_squad, "null pointers retained");
        unchanged(member);
    }
    else if (mode == "dead" || mode == "dead_mutation")
    {
        if (mode == "dead_mutation") alive = [&](int id) {
            require(id == 0, "dead callback receiver");
            member.m_bot = nullptr; member.m_player = nullptr; member.m_squad = nullptr;
            member.m_position = {41, 42, 43}; return false;
        };
        member.moveWithoutFormation();
        require(calls == "alive0", "dead player returns before position/squad/vehicle work");
        require(member.m_position.x == (mode == "dead_mutation" ? 41.0f : 2.0f), "callback position preserved");
        if (mode == "dead_mutation") require(!member.m_player && !member.m_squad && !member.m_bot, "no stale pointer work after refusal");
        unchanged(member);
    }
    else if (mode.rfind("formation_", 0) == 0)
    {
        member.m_player = nullptr; member.m_bot = nullptr;
        member.m_positionValid = mode == "formation_order";
        if (mode == "formation_actual") verified("");
        member.moveWithFormation();
        unexpected("world-position frontier returned");
    }
    else
    {
        alive = [&](int id) {
            require(id == 0, "alive receiver captured at call");
            if (mode == "fresh_player") member.m_player = &second;
            return true;
        };
        vehicle = [&](int id) -> IAIObject* {
            const int expectedId = mode == "fresh_player" ? 1 : mode == "independent_mutation" ? 2 : 0;
            require(id == expectedId, "fresh player after prior callbacks");
            require(!member.m_positionValid, "invalid position does not refuse alive movement");
            if (mode == "independent_mutation") require(member.m_memberIndex == -999, "position callback mutation retained");
            else unchanged(member);
            const std::string expected = std::string("alive0,") +
                (mode.rfind("independent_", 0) == 0 ? "position,goal," : "") +
                "vehicle" + std::to_string(expectedId);
            verified(expected.c_str());
            return mode == "alive_vehicle" ? reinterpret_cast<IAIObject*>(&vehicleToken) : nullptr;
        };
        member.moveWithoutFormation();
        unexpected("vehicle owner frontier returned");
    }
    std::printf("PASS %s %u checks\n", mode.c_str(), checks);
    return 0;
}
