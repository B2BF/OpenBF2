// [TEMP] Actual Member/Coordinator bodies with native chosen lifetimes. Formation
// helpers are typed observations, not recovered implementations in this executable.
// REAL_ALLOWANCE tests the actual refusal/owner stop; other modes choose its result.
// Member::update is a chosen second-loop observer; member_update_runtime tests its body.
#include "member_formation_runtime.h"
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
static_assert(std::is_same<decltype(&SquadMember::updateFormationStatus), void(SquadMember::*)()>::value, "member void contract");
static unsigned checks;
[[noreturn]] static void unexpected(const char* reason)
{ std::fprintf(stderr, "FAIL %s\n", reason); std::fflush(stderr); std::exit(2); }
static void require(bool condition, const char* reason) { ++checks; if (!condition) unexpected(reason); }
static std::string mode, calls, expectedStopTrace;
static bool checkpointPrinted;
static void trace(const std::string& value)
{ if (checkpointPrinted) unexpected("callback after completed expected trace"); if (!calls.empty()) calls += ','; calls += value; }
static void checkpoint()
{
    if (!expectedStopTrace.empty() && calls == expectedStopTrace)
    { checkpointPrinted = true; std::printf("VERIFIED %s\n", mode.c_str()); std::fflush(stdout); }
}
static std::function<bool(SquadMember*)> allowance;
static std::function<bool(SquadFormation*, int)> membership;
static std::function<void(SquadFormation*, int, bool)> change;
static std::function<void(SquadFormation*)> closest;
static std::function<bool(int, int)> playerCallback;
static SquadCoordinator* observedCoordinator;
static SquadFormation* finalFormation;
static bool observePlayer(int identity, int operation)
{
    trace("p" + std::to_string(identity) + ":" + std::to_string(operation));
    const bool result = playerCallback(identity, operation);
    if (mode == "owner_stop" && operation == 2) checkpoint();
    return result;
}
#include "mocks.inl"

namespace dice { namespace hfe { namespace ai {
// [CHOSEN, not recovered] Scoped lifetime definitions do not publish singletons or
// claim retail construction. Every touched scalar and all native containers live.
SquadCoordinator::SquadCoordinator(int side, int index)
    : m_squadIndex(index), m_side(side), m_strategicAreaAssignment(nullptr), m_i40(4),
      m_i44(0), m_formation(nullptr), m_missionStatus(0), m_d124(-999.0) {}
SquadMember::SquadMember(SquadCoordinator* squad, IAIPlayer* player, int squadIndex, int side, int index)
    : m_squadIndex(squadIndex), m_memberIndex(index), m_player(player), m_bot(nullptr),
      m_squad(squad), m_side(side), m_havePath(true), m_request(nullptr), m_position{},
      m_positionValid(false), m_flag49(true), m_flag4A(false), m_flag4B(true) {}
SquadMember::~SquadMember() {}
SquadFormation::SquadFormation(SquadCoordinator* coordinator)
    : m_coordinator(coordinator), m_numberOfSlots(0) {}
IAIPlayer::~IAIPlayer() {}
IAIMain* IAIMain::m_instance = nullptr;
bool SAI::isPlayerCommander() { unexpected("unselected mission commander"); }
bool SAI::ownAllTakeableCps(int) { unexpected("unselected mission predicate"); }
bool SAI::haveAnyOwnedAndTakeableCps(int) { unexpected("unselected mission predicate"); }
bool AIStrategicArea::isAreaTakeable() { unexpected("unselected mission area"); }
#if !defined(REAL_ALLOWANCE)
bool SquadMember::isAllowedInFormation() { trace("allow"); return allowance(this); }
#endif
bool SquadFormation::isInFormation(int index) { return membership(this, index); }
void SquadFormation::joinFormation(int index) { change(this, index, true); }
void SquadFormation::leaveFormation(int index) { change(this, index, false); }
void SquadFormation::updateClosestSlots() { closest(this); }
void SquadMember::update(double time)
{
    require(time==10.000000000000002,"coordinator forwards original double to second member loop");
    trace("tick"+std::to_string(m_memberIndex));checkpoint();
}
// [CHOSEN] These two receivers observe the enclosing actual coordinator only.
void SquadCoordinator::updateSquadManeuver()
{
    trace("maneuver");
    if(mode=="coord_one"){m_formation=finalFormation;m_d124=-111.0;}
}
void SquadFormation::update(double time)
{
    require(time==10.000000000000002,"original double reaches formation after maneuver mutation");
    require(this==observedCoordinator->m_formation,"fresh formation receiver after maneuver");
    if(mode=="coord_one")require(this==finalFormation&&observedCoordinator->m_d124==-111.0,"mutated formation and stored time distinguished");
    trace("formation");
}
// [CHOSEN]
// Coordinator observers check call order and argument forwarding only.
void SquadCoordinator::updateSquadPosition()
{ require(this==observedCoordinator,"tail receiver"); trace("position"); }
void SquadCoordinator::updateShotsFired()
{ require(this==observedCoordinator,"tail receiver"); trace("shots"); }
void SquadCoordinator::updateRepairRequest(double time)
{ require(time==10.000000000000002,"original double reaches the repair expiry"); trace("repair"); }
void SquadCoordinator::updateNeedOrdersMessage(double time)
{ require(time==10.000000000000002,"original double reaches the need-orders timer"); trace("orders"); }
void SquadCoordinator::updateIncomingMessages()
{ require(this==observedCoordinator,"tail receiver"); trace("incoming"); }
} } }

int main(int argc, char** argv)
{
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    if (argc != 2) unexpected("mode required");
    mode = argv[1];
    SquadCoordinator squad(0, 3), replacementSquad(1, 4);
    SquadFormation formation(&squad), replacement(&replacementSquad);
    squad.m_formation = &formation; replacementSquad.m_formation = &replacement;
    observedCoordinator=&squad;finalFormation=&replacement;
    ObservedPlayer player, replacementPlayer;
    replacementPlayer.identity = 1;
    SquadMember member(&squad, &player, 3, 0, 7);
    SquadMember second(&squad, &replacementPlayer, 3, 0, 8);
    ObservedPlayer thirdPlayer; thirdPlayer.identity = 2;
    SquadMember third(&squad, &thirdPlayer, 3, 0, 9);
    int queries = 0, actions = 0, allowanceCalls = 0;
    bool allowed = false, first = false, last = false;
    std::string expected;
    allowance = [&](SquadMember* target) { ++allowanceCalls; require(target == &member, "allowance receiver"); return allowed; };
    membership = [&](SquadFormation* target, int index) {
        require(target == &formation, "formation captured once before allowance");
        require(index == member.m_memberIndex, "fresh member index");
        trace("q" + std::to_string(index));
        return ++queries == 1 ? first : last;
    };
    change = [&](SquadFormation* target, int index, bool joining) {
        require(target == &formation && index == member.m_memberIndex, "retained formation and fresh action index");
        require(member.m_havePath, "path clear follows action callback");
        trace(std::string(joining ? "join" : "leave") + std::to_string(index)); ++actions;
    };
    closest = [&](SquadFormation* target) {
        require(target == squad.m_formation, "coordinator rereads formation after member callbacks");
        trace(target == &replacement ? "closest1" : "closest0");
        require(member.m_havePath && second.m_havePath && third.m_havePath, "refused nonmembers retain path flags");
        require(!member.m_flag4A && !second.m_flag4A, "refused nonmembers retain inverse flags");
        if (mode == "coord_closest_clear") squad.m_squadMembers.clear();
        if (mode == "coord_closest_restore") squad.m_squadMembers.push_back(&second);
        checkpoint();
    };
    playerCallback = [](int, int operation) { require(operation == 0, "dead refusal skips later player calls"); return false; };

#if defined(REAL_ALLOWANCE)
    if (mode.rfind("coord_", 0) == 0)
    {
        const double time = 10.000000000000002;
        membership = [&](SquadFormation* target, int index) {
            require(target == ((mode == "coord_replace" && index == 9) ? &replacement : &formation),
                    "each member captures its formation before its own allowance callbacks");
            require(squad.m_d124 == time, "timestamp before every member callback");
            trace("q" + std::to_string(index)); ++queries; return false;
        };
        if (mode == "coord_empty")
        {
            squad.update(time);
            require(calls.empty() && squad.m_d124 == time, "empty coordinator returns before formation work");
        }
        else
        {
            squad.m_squadMembers = {&member};
            if (mode == "coord_two" || mode == "coord_replace") squad.m_squadMembers.push_back(&second);
            playerCallback = [&](int identity, int operation) {
                require(operation == 0 && squad.m_d124 == time, "real dead refusal and timestamp");
                if (identity == 0)
                {
                    if (mode == "coord_append")
                    { squad.m_squadMembers.reserve(128); squad.m_squadMembers.push_back(&third); }
                    if (mode == "coord_shrink" || mode == "coord_closest_restore") squad.m_squadMembers.clear();
                    if (mode == "coord_replace")
                    { std::vector<SquadMember*> changed{&member, &third}; squad.m_squadMembers.swap(changed); squad.m_formation = &replacement; }
                }
                return false;
            };
            expectedStopTrace = "p0:0,q7,q7";
            if (mode == "coord_two") expectedStopTrace += ",p1:0,q8,q8";
            if (mode == "coord_append" || mode == "coord_replace") expectedStopTrace += ",p2:0,q9,q9";
            expectedStopTrace += mode == "coord_replace" ? ",closest1" : ",closest0";
            if(mode=="coord_one")expectedStopTrace+=",tick7";
            if(mode=="coord_two")expectedStopTrace+=",tick7,tick8";
            if(mode=="coord_append"||mode=="coord_replace")expectedStopTrace+=",tick7,tick9";
            if(mode=="coord_closest_restore")expectedStopTrace+=",tick8";
            expectedStopTrace+=",maneuver,formation,position,shots,repair,orders,incoming";
            squad.update(time);
            require(calls==expectedStopTrace,"complete coordinator order through the written tail");
        }
    }
    else
    {
        expected = "p0:0,q7,q7";
        if (mode == "dead") {}
        else if (mode == "man_down")
        { playerCallback = [](int id, int op) { require(id == 0 && op < 2, "alive then down"); return true; }; expected = "p0:0,p0:1,q7,q7"; }
        else if (mode == "no_vehicle" || mode == "owner_stop")
        {
            playerCallback = [&](int id, int op) { require(id == 0 && op <= 2, "fresh player sequence"); return op == 0 || (op == 2 && mode == "owner_stop"); };
            expected = "p0:0,p0:1,p0:2,q7,q7";
            if (mode == "owner_stop") expectedStopTrace = "p0:0,p0:1,p0:2";
        }
        else if (mode == "fresh_player")
        {
            playerCallback = [&](int id, int op) {
                if (op == 0) { require(id == 0, "first player alive"); member.m_player = &replacementPlayer; return true; }
                require(id == 1 && op == 1, "player reloaded after alive callback"); return true;
            }; expected = "p0:0,p1:1,q7,q7";
        }
        else if (mode == "real_capture")
        {
            playerCallback = [&](int, int op) { require(op == 0, "dead only"); squad.m_formation = &replacement;
                member.m_squad = &replacementSquad; member.m_memberIndex = 77; return false; };
            expected = "p0:0,q77,q77";
        }
        else unexpected("unknown real mode");
        member.updateFormationStatus();
        require(calls == expected && queries == 2 && actions == 0, "actual refusal still performs both membership queries");
        require(member.m_havePath && !member.m_flag4A, "refusal without membership does not clear either flag");
    }
#else
    if (mode == "position_copy")
    {
        member.m_position = {1.25f, -2.5f, 8.0f}; Vec3 destination{};
        require(!member.getPosition(destination), "false validity returned");
        require(destination.x == 1.25f && destination.y == -2.5f && destination.z == 8.0f, "position copied even when invalid");
        member.m_positionValid = true;
        require(member.getPosition(member.m_position), "alias output and true validity");
        require(member.m_position.x == 1.25f && member.m_position.z == 8.0f, "alias preserves coordinates");
    }
    else if (mode == "goto_borrowed")
    {
        const Vec3& position = squad.getSquadGotoPos();
        require(&position == &squad.m_v28, "goto position is borrowed native field");
        squad.m_v28 = {7.0f, -2.0f, 12.0f};
        require(position.x == 7.0f && position.y == -2.0f && position.z == 12.0f, "borrowed position remains live");
    }
    else if (mode == "getter")
    {
        require(squad.getSquadFormation() == &formation, "borrowed formation getter");
        squad.m_formation = &replacement;
        require(squad.getSquadFormation() == &replacement, "fresh getter member read");
        squad.m_formation = nullptr;
        require(squad.getSquadFormation() == nullptr, "getter preserves null");
    }
    else
    {
        if (mode.rfind("table_", 0) == 0)
        {
            const int value = std::atoi(mode.c_str() + 6);
            allowed = (value & 4) != 0; first = (value & 2) != 0; last = (value & 1) != 0;
            expected = "allow,q7";
            if (first && !allowed) expected += ",leave7";
            expected += ",q7";
            if (!last && allowed) expected += ",join7";
        }
        else if (mode == "capture_formation")
        {
            allowance = [&](SquadMember* target) { ++allowanceCalls; require(target == &member, "allowance receiver");
                squad.m_formation = &replacement; member.m_squad = &replacementSquad; member.m_memberIndex = 77; return true; };
            allowed = true; first = last = true; expected = "allow,q77,q77";
        }
        else if (mode == "fresh_leave_index" || mode == "fresh_second_index" || mode == "fresh_join_index")
        {
            allowed = mode != "fresh_leave_index"; first = mode != "fresh_join_index"; last = mode == "fresh_second_index";
            membership = [&](SquadFormation* target, int index) {
                require(target == &formation && index == member.m_memberIndex, "captured formation/current index before callback");
                trace("q" + std::to_string(index)); ++queries;
                if (queries == (mode == "fresh_join_index" ? 2 : 1)) member.m_memberIndex = 99;
                return queries == 1 ? first : last;
            };
            expected = mode == "fresh_leave_index" ? "allow,q7,leave99,q99" :
                       mode == "fresh_second_index" ? "allow,q7,q99" : "allow,q7,q7,join99";
        }
        else if (mode == "retain_allowance_true" || mode == "retain_allowance_false")
        {
            allowed = mode == "retain_allowance_true";
            membership = [&](SquadFormation* target, int index) {
                require(target == &formation && index == 7, "formation and index"); trace("q7"); ++queries;
                allowed = !allowed; return false;
            };
            expected = "allow,q7,q7" + std::string(mode == "retain_allowance_true" ? ",join7" : "");
        }
        else if (mode == "leave_clear_timing" || mode == "join_clear_timing")
        {
            allowed = mode == "join_clear_timing"; first = !allowed; last = false;
            change = [&](SquadFormation* target, int index, bool joining) {
                require(target == &formation && index == 7 && joining == allowed, "selected formation action");
                require(member.m_havePath && !member.m_flag4A, "neither flag changed before callback");
                trace(joining ? "join7" : "leave7"); ++actions;
                member.m_flag4A = true; member.m_havePath = true;
                if (!joining) member.m_memberIndex = 99;
            };
            membership = [&](SquadFormation* target, int index) {
                require(target == &formation && index == member.m_memberIndex, "fresh formation-query index after leave");
                trace("q" + std::to_string(index)); ++queries;
                if (queries == 2 && !allowed) require(!member.m_havePath && member.m_flag4A, "leave cleared only path before second query");
                return queries == 1 ? first : last;
            };
            expected = allowed ? "allow,q7,q7,join7" : "allow,q7,leave7,q99";
        }
        else if (mode == "no_action_flags")
        {
            allowed = true;
            membership = [&](SquadFormation* target, int index) {
                require(target == &formation && index == 7, "formation query"); trace("q7"); ++queries;
                member.m_havePath = false; member.m_flag4A = true; return true;
            }; expected = "allow,q7,q7";
        }
        else unexpected("unknown chosen mode");
        member.updateFormationStatus();
        require(calls == expected && queries == 2 && allowanceCalls == 1, "exact calls, two membership queries and one allowance");
        const bool changedFlags = mode == "leave_clear_timing" || mode == "join_clear_timing" || mode == "no_action_flags";
        require(member.m_flag4A == changedFlags && member.m_flag49 && member.m_flag4B, "inverse and adjacent flags preserved");
        require(member.m_havePath == (actions == 0 && mode != "no_action_flags"), "only action arms clear path");
    }
#endif
    std::printf("PASS %s %u checks\n", mode.c_str(), checks);
    return 0;
}
