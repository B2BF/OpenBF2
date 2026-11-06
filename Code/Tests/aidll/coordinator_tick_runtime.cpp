// [TEMP] Chosen native lifetimes and typed observers isolate exact production definitions.
// PATH_OBSERVERS replaces only setHavePath, in a separately linked executable, to test
// callback mutation of the real path loop. Ordinary modes use the actual two-flag setter.
// Both Member calls/closest-slot calls are chosen typed observers in this suite;
// member_formation_runtime and member_update_runtime test the actual member bodies.
#include "coordinator_tick_runtime.h"
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
static_assert(sizeof(void*) == 4, "strict MSVC x86 fixture");
static_assert(std::is_same<decltype(&SquadCoordinator::update), void(SquadCoordinator::*)(double)>::value, "tick contract");
static_assert(std::is_same<decltype(&SquadCoordinator::updateSquadPaths), void(SquadCoordinator::*)()>::value, "path contract");
static unsigned checks;
[[noreturn]] static void unexpected(const char* reason)
{ std::fprintf(stderr, "FAIL %s\n", reason); std::fflush(stderr); std::abort(); }
static void require(bool value, const char* reason) { ++checks; if (!value) unexpected(reason); }
static std::string mode, calls, expectedMemberTrace;
static void trace(const std::string& text) { if (!calls.empty()) calls += ','; calls += text; }
static void printTrace() { std::printf("TRACE %s\n", calls.c_str()); std::fflush(stdout); }
static std::function<SAI*(int, int)> mainCallback;
static std::function<bool(int, int, int)> saiCallback;
static std::function<bool()> areaCallback;
static std::function<void(SquadMember*, bool)> pathCallback;
static std::function<void(SquadMember*)> memberCallback;
static std::function<void(SquadMember*)> tickCallback;
static SquadCoordinator* observedSquad;
static SquadFormation* observedFormation;
static SquadFormation* finalFormation;
static SAI* observeMain(int identity, int side)
{ trace("m" + std::to_string(identity) + ":" + std::to_string(side)); return mainCallback(identity, side); }
#include "mocks.inl"

namespace dice { namespace hfe { namespace ai {
// [CHOSEN, not recovered] Native field/STL lifetimes without publication or deep construction.
SquadCoordinator::SquadCoordinator(int side, int index)
    : m_squadIndex(index), m_side(side), m_strategicAreaAssignment(nullptr), m_i40(4),
      m_i44(789), m_formation(nullptr), m_missionStatus(0), m_d124(-999.0) {}
SquadMember::SquadMember(SquadCoordinator* squad, IAIPlayer* player, int squadIndex, int side, int index)
    : m_squadIndex(squadIndex), m_memberIndex(index), m_player(player), m_bot(nullptr),
      m_squad(squad), m_side(side), m_havePath(true), m_request(nullptr), m_flag4A(false) {}
SquadMember::~SquadMember() {}
SquadFormation::SquadFormation(SquadCoordinator* squad) : m_coordinator(squad) {}
void SquadMember::updateFormationStatus()
{
    require(observedSquad->m_d124 == 123.00000000000003, "timestamp before member callback");
    trace("u" + std::to_string(m_memberIndex));
    if (memberCallback) memberCallback(this);
}
void SquadFormation::updateClosestSlots()
{
    require(this == observedFormation, "fresh formation after member callbacks");
    trace("f");
    observedSquad->m_d124=-4000.0; // Observer mutation must not replace the caller's original argument.
}
void SquadMember::update(double time)
{
    require(time==123.00000000000003,"original unrounded double survives stored-time mutation");
    trace("t"+std::to_string(m_memberIndex));
    if(tickCallback)tickCallback(this);
}
void SquadCoordinator::updateSquadManeuver()
{
    require(calls==expectedMemberTrace,"maneuver follows the exact two live loops");
    trace("maneuver");
    if(mode=="nonempty_tail"){m_formation=finalFormation;m_d124=-5000.0;}
}
// [CHOSEN] Actual Formation tick is tested by formation_tick_runtime separately.
void SquadFormation::update(double time)
{
    require(time==123.00000000000003,"formation receives original double");
    require(this==observedSquad->m_formation,"formation receiver reloaded after maneuver");
    if(mode=="nonempty_tail")require(this==finalFormation&&observedSquad->m_d124==-5000.0,"fresh receiver despite stored-time mutation");
    trace("formation");
}
// [CHOSEN]
// Observers check order, receiver, and double-precision forwarding; coordinator_tail_runtime tests the bodies.
void SquadCoordinator::updateSquadPosition()
{ require(this==observedSquad,"tail receiver"); trace("position"); }
void SquadCoordinator::updateShotsFired()
{ require(this==observedSquad,"tail receiver"); trace("shots"); }
void SquadCoordinator::updateRepairRequest(double time)
{ require(time==123.00000000000003,"repair expiry receives the original double, not m_d124"); trace("repair"); }
void SquadCoordinator::updateNeedOrdersMessage(double time)
{ require(time==123.00000000000003,"need-orders timer receives the original double"); trace("orders"); }
void SquadCoordinator::updateIncomingMessages()
{ require(this==observedSquad,"tail receiver"); trace("incoming"); }
IAIMain* IAIMain::m_instance = nullptr;
IAIMain::IAIMain() {}
IAIMain::~IAIMain() {}
ISAI::~ISAI() {}
SAI::SAI(int side, int, int, Vec2, Vec2) : m_side(side), m_flag1D1(false) {}
SAI::~SAI() {}
AIStrategicObject::AIStrategicObject(float, float)
{ for (int i = 0; i < 4; ++i) m_status[i] = StatusMap::Unknown; }
AIStrategicObject::~AIStrategicObject() {}
AIStrategicArea::AIStrategicArea(const std::string& name, unsigned int, float radius)
    : AIStrategicObject(radius, 0.0f), m_name(name) {}
AIStrategicArea::~AIStrategicArea() {}
#if defined(PATH_OBSERVERS)
void SquadMember::setHavePath(bool value) { pathCallback(this, value); }
#endif
} } }

class ObservedSAI : public SAI
{
public:
    int identity;
    explicit ObservedSAI(int id) : SAI(1, 0, 0, Vec2(), Vec2()), identity(id) {}
    bool isPlayerCommander() override
    { trace("c" + std::to_string(identity)); return saiCallback(identity, 0, 0); }
    bool ownAllTakeableCps(int side) override
    {
        trace("o" + std::to_string(identity) + ":" + std::to_string(side));
        if (mode == "own_all_stop") { printTrace(); return SAI::ownAllTakeableCps(side); }
        return saiCallback(identity, 1, side);
    }
    bool haveAnyOwnedAndTakeableCps(int side) override
    {
        trace("h" + std::to_string(identity) + ":" + std::to_string(side));
        if (mode == "have_any_stop") { printTrace(); return SAI::haveAnyOwnedAndTakeableCps(side); }
        return saiCallback(identity, 2, side);
    }
};
class ObservedArea : public AIStrategicArea
{
public:
    ObservedArea() : AIStrategicArea("fixture", 0, 0.0f) {}
    bool isAreaTakeable() override
    {
        trace("a");
        if (mode == "takeable_stop") { printTrace(); return AIStrategicArea::isAreaTakeable(); }
        return areaCallback();
    }
};

int main(int argc, char** argv)
{
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    if (argc != 2) unexpected("mode required");
    mode = argv[1];
    SquadCoordinator squad(0, 5);
    SquadFormation formation(&squad), replacementFormation(&squad);
    squad.m_formation = &formation; observedSquad = &squad; observedFormation = &formation;
    finalFormation=&replacementFormation;
    SquadMember a(&squad, nullptr, 5, 0, 0), b(&squad, nullptr, 5, 0, 1), c(&squad, nullptr, 5, 0, 2);
    ObservedMain main, replacementMain;
    replacementMain.identity = 1;
    ObservedSAI sai(0), replacementSai(1);
    ObservedArea area, replacementArea;
    IAIMain::m_instance = &main;
    const double time = 123.00000000000003;
    bool expectedUnassigned = false;
    std::string expectedCalls;
    mainCallback = [&](int identity, int side) -> SAI* {
        require(identity == 0 && side == squad.m_side + 1, "main identity/current one-based side");
        require(squad.m_d124 == time, "timestamp before first callback"); return &sai;
    };
    saiCallback = [&](int identity, int operation, int side) {
        require(identity == 0, "retained SAI identity");
        if (operation == 0) return sai.SAI::isPlayerCommander();
        require(side == (operation == 1 ? squad.m_side + 1 : (squad.m_side == 0) + 1), "predicate side");
        return operation == 1;
    };
    areaCallback = [] { return true; };

#if defined(PATH_OBSERVERS)
    squad.m_squadMembers = {&a, nullptr, &b};
    pathCallback = [&](SquadMember* member, bool value) {
        require(!value, "always invalidates"); trace(std::to_string(member->m_memberIndex));
        if (member == &a)
        {
            if (mode == "paths_append")
            { squad.m_squadMembers.reserve(128); squad.m_squadMembers.push_back(&c); }
            else if (mode == "paths_shrink") squad.m_squadMembers.clear();
            else if (mode == "paths_replace")
            { std::vector<SquadMember*> replacement{&a, &c}; squad.m_squadMembers.swap(replacement); }
            else unexpected("unknown observer mode");
        }
    };
    squad.updateSquadPaths();
    expectedCalls = mode == "paths_append" ? "0,1,2" : mode == "paths_shrink" ? "0" : "0,2";
    require(calls == expectedCalls, "live path vector reload after callback");
#else
    if (mode == "paths")
    {
        squad.updateSquadPaths();
        require(a.m_havePath && !a.m_flag4A && calls.empty(), "empty has no effects");
        squad.m_squadMembers = {nullptr, &a, &b, nullptr, &a};
        squad.updateSquadPaths();
        require(!a.m_havePath && a.m_flag4A && !b.m_havePath && b.m_flag4A, "two real setter flags including duplicate");
        require(c.m_havePath && !c.m_flag4A, "unlisted member unchanged");
        squad.m_squadMembers = {&c}; a.m_havePath = true; a.m_flag4A = false;
        squad.updateSquadPaths();
        require(!c.m_havePath && c.m_flag4A && a.m_havePath && !a.m_flag4A, "changed roster on later invocation");
        require(calls.empty() && squad.m_d124 == -999.0, "path invalidation has no tick callbacks/timestamp");
    }
    else
    {
        if (mode == "empty") squad.m_missionStatus = 258;
        else if (mode == "timestamp_bits")
        {
            const unsigned long long bits = 0x7ff8000000001234ULL;
            double value; std::memcpy(&value, &bits, sizeof(value)); squad.update(value);
            unsigned long long actual; std::memcpy(&actual, &squad.m_d124, sizeof(actual));
            require(actual == bits && calls.empty(), "double NaN payload preserved on empty tick");
        }
        else if (mode == "nonempty_tail" || mode == "duplicate_members_tail" || mode.rfind("members_", 0) == 0 || mode.rfind("ticks_",0)==0)
        {
            squad.m_squadMembers.push_back(&a);
            expectedMemberTrace="u0,f,t0";
            if (mode == "duplicate_members_tail") squad.m_squadMembers.push_back(&a);
            if (mode == "duplicate_members_tail")expectedMemberTrace="u0,u0,f,t0,t0";
            if (mode.rfind("members_", 0) == 0)
            {
                squad.m_squadMembers.push_back(&b);
                memberCallback = [&](SquadMember* member) {
                    if (member != &a) return;
                    if (mode == "members_append_tail")
                    { squad.m_squadMembers.reserve(128); squad.m_squadMembers.push_back(&c); }
                    else if (mode == "members_shrink_tail") squad.m_squadMembers.clear();
                    else if (mode == "members_replace_tail")
                    { std::vector<SquadMember*> replacement{&a, &c}; squad.m_squadMembers.swap(replacement); }
                    else unexpected("unknown member mutation");
                    squad.m_formation = &replacementFormation; observedFormation = &replacementFormation;
                };
                expectedMemberTrace=mode=="members_append_tail"?"u0,u1,u2,f,t0,t1,t2":
                    mode=="members_shrink_tail"?"u0,f":"u0,u2,f,t0,t2";
            }
            if(mode.rfind("ticks_",0)==0)
            {
                squad.m_squadMembers.push_back(&b);
                tickCallback=[&](SquadMember* member){
                    if(member!=&a)return;
                    if(mode=="ticks_append_tail") {squad.m_squadMembers.reserve(128);squad.m_squadMembers.push_back(&c);}
                    else if(mode=="ticks_shrink_tail")squad.m_squadMembers.clear();
                    else if(mode=="ticks_replace_tail") {std::vector<SquadMember*> changed{&a,&c};squad.m_squadMembers.swap(changed);}
                    else unexpected("unknown tick mutation");
                };
                expectedMemberTrace=mode=="ticks_append_tail"?"u0,u1,f,t0,t1,t2":
                    mode=="ticks_shrink_tail"?"u0,u1,f,t0":"u0,u1,f,t0,t2";
            }
            expectedCalls = expectedMemberTrace + ",maneuver,formation,position,shots,repair,orders,incoming";
        }
        else
        {
            squad.m_missionStatus = 2; squad.m_i40 = 0; squad.m_strategicAreaAssignment = &area;
            area.m_status[0] = area.m_status[1] = StatusMap::Hostile;
            expectedCalls = "m0:1,c0";
            if (mode == "commander")
                saiCallback = [](int, int operation, int) { require(operation == 0, "commander skips predicates"); return true; };
            else if (mode == "null_assignment") squad.m_strategicAreaAssignment = nullptr;
            else if (mode == "other_kind") squad.m_i40 = 258;
            else if (mode == "owned_attack") { area.m_status[0] = StatusMap::Owned; expectedUnassigned = true; }
            else if (mode == "takeable" || mode == "takeable_stop") expectedCalls += ",a";
            else if (mode == "own_all" || mode == "not_own_all" || mode == "own_all_stop")
            {
                areaCallback = [] { return false; }; expectedCalls += ",a,m0:1,o0:1";
                if (mode == "not_own_all")
                { expectedUnassigned = true; saiCallback = [](int, int, int) { return false; }; }
            }
            else if (mode == "defend_side_one" || mode == "defend_side_two" || mode == "defend_not_owned" ||
                     mode == "defend_refusal" || mode == "have_any_stop")
            {
                squad.m_i40 = 1;
                if (mode == "defend_side_two") squad.m_side = 1;
                if (mode != "defend_not_owned")
                {
                    area.m_status[squad.m_side] = StatusMap::Owned;
                    expectedUnassigned = mode != "defend_refusal";
                    saiCallback = [&](int, int op, int side) {
                        if (!op) return false;
                        require(op == 2 && side == (squad.m_side == 0) + 1, "captured opposite one-based side");
                        return mode != "defend_refusal";
                    };
                    expectedCalls = squad.m_side == 0 ? "m0:1,c0,m0:1,h0:2" : "m0:2,c0,m0:2,h0:1";
                }
            }
            else if (mode == "commander_mutation")
            {
                saiCallback = [&](int, int op, int) {
                    require(op == 0, "mutation occurs in commander query");
                    squad.m_side = 1; squad.m_i40 = 0; squad.m_strategicAreaAssignment = &replacementArea;
                    replacementArea.m_status[1] = StatusMap::Owned; return false;
                };
                expectedUnassigned = true;
            }
            else if (mode == "fresh_main_side")
            {
                areaCallback = [&] { squad.m_side = 1; IAIMain::m_instance = &replacementMain; return false; };
                mainCallback = [&](int id, int side) -> SAI* {
                    require(squad.m_d124 == time, "stored time precedes callbacks");
                    if (id == 0) { require(side == 1, "first main side"); return &sai; }
                    require(side == 2, "fresh side and main after area callback");
                    squad.m_side = 0; IAIMain::m_instance = &main; return &replacementSai;
                };
                saiCallback = [](int id, int op, int side) {
                    if (!op) { require(id == 0, "initial commander SAI"); return false; }
                    require(id == 1 && op == 1 && side == 1, "returned SAI retained and side reread after getSAI"); return true;
                };
                expectedCalls += ",a,m1:2,o1:1";
            }
            else if (mode == "captured_enemy")
            {
                squad.m_i40 = 1; area.m_status[0] = StatusMap::Owned; int lookups = 0;
                mainCallback = [&, lookups](int id, int side) mutable -> SAI* {
                    require(id == 0 && side == 1, "both lookup arguments before side mutation");
                    if (++lookups == 2) squad.m_side = 1; return &sai;
                };
                saiCallback = [](int, int op, int side) {
                    if (!op) return false;
                    require(op == 2 && side == 2, "opposite side captured before getSAI mutation"); return true;
                };
                expectedCalls += ",m0:1,h0:2"; expectedUnassigned = true;
            }
            else if (mode == "cleared_members")
            {
                squad.m_squadMembers.push_back(&a);
                saiCallback = [&](int, int op, int) {
                    require(op == 0, "commander clears members"); squad.m_squadMembers.clear(); return true;
                };
            }
            else unexpected("unknown mode");
        }
        if (mode != "timestamp_bits")
        {
            squad.update(time);
            const bool tail = mode.size() > 5 && mode.compare(mode.size() - 5, 5, "_tail") == 0;
            // Tail cases keep whatever the observers stored: closest-slots writes -4000, the
            // nonempty maneuver -5000. The tail itself still received the original argument.
            require(squad.m_d124 == (!tail ? time : mode == "nonempty_tail" ? -5000.0 : -4000.0),
                    "double timestamp retained exactly");
            require(calls == expectedCalls, "exact query order and arguments");
            if (tail) require(squad.m_i44 == 789, "no unassignment on a tail case");
            else if (expectedUnassigned)
                require(!squad.m_strategicAreaAssignment && squad.m_i40 == 4 && squad.m_i44 == 0 && squad.m_missionStatus == 0,
                        "all four real unassignment stores");
            else require(squad.m_i44 == 789, "no unassignment when refused");
        }
    }
#endif
    std::printf("PASS %s %u checks\n", mode.c_str(), checks);
    return 0;
}
