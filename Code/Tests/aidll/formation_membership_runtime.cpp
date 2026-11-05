// [TEMP] Actual method bodies are extracted unchanged in actual.cpp. Only native
// lifetimes and the clock are chosen; all membership, slot moves, member queries,
// path flags and trailing/movement updates in this executable are production bodies.
#include "formation_membership_runtime.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <limits>
#include <string>
#include <type_traits>
#include <windows.h>
#include <crtdbg.h>
using namespace dice::hfe;
using namespace dice::hfe::ai;
static_assert(sizeof(void*) == 4, "strict MSVC x86");
static_assert(sizeof(SquadFormation::Slot) == 36, "scalar-only native slot extent");
static_assert(std::is_same<decltype(SquadFormation::Slot::m_i28), float>::value, "observed float slot payload");
static unsigned checks;
[[noreturn]] static void unexpected(const char* reason)
{ std::fprintf(stderr, "FAIL %s\n", reason); std::fflush(stderr); std::abort(); }
static void require(bool value, const char* reason) { ++checks; if (!value) unexpected(reason); }
static SquadFormation* tested;
static int clockCalls;
static std::function<void()> clockCallback;
static double observeTime()
{
    ++clockCalls;
    require(!tested->m_b56, "movement flag clears before clock");
    if (clockCallback) clockCallback();
    return 10.125;
}
#include "mocks.inl"

namespace dice { namespace hfe { namespace ai {
// [CHOSEN, not recovered] Real native containers, no production publication or deep ownership.
void* CommonSingletonServer::m_iBFTimer = nullptr;
IBFTimer::IBFTimer() {}
IBFTimer::~IBFTimer() {}
SquadFormation::SquadFormation(SquadCoordinator* squad)
    : m_coordinator(squad), m_numberOfSlots(6), m_b56(true), m_d92(-9.0)
{ m_v60 = {-8,-9,-10}; m_v72 = {11,12,13}; m_trailingPosition = {21,22,23}; }
SquadCoordinator::SquadCoordinator(int side, int index)
    : m_squadIndex(index), m_side(side), m_formation(nullptr)
{ m_v28 = {}; m_v108 = {}; }
SquadMember::SquadMember(SquadCoordinator* squad, IAIPlayer* player, int squadIndex, int side, int index)
    : m_squadIndex(squadIndex), m_memberIndex(index), m_isSquadLeader(false), m_player(player),
      m_bot(nullptr), m_squad(squad), m_side(side), m_havePath(true), m_request(nullptr),
      m_positionValid(true), m_flag4A(false)
{ m_position = {static_cast<float>(index), 0, 0}; }
SquadMember::~SquadMember() {}
} } }

static bool equal(Vec3 a, Vec3 b) { return a.x == b.x && a.y == b.y && a.z == b.z; }
static void resetExpected(SquadFormation::Slot& s)
{ s.hasSquadMemberAssigned=false; s.m_memberIndex=-1; s.m_i20=-1; s.m_b24=false; s.m_i28=0; }
static void movedExpected(SquadFormation::Slot& to, const SquadFormation::Slot& from, bool copyPosition)
{
    to.hasSquadMemberAssigned=from.hasSquadMemberAssigned; to.m_memberIndex=from.m_memberIndex;
    to.m_i20=from.m_i20; to.m_b24=from.m_b24; to.m_i28=from.m_i28;
    if (copyPosition) to.m_offset=from.m_offset;
}
static void compareSlot(const SquadFormation::Slot& actual, const SquadFormation::Slot& expected)
{
    require(actual.hasSquadMemberAssigned==expected.hasSquadMemberAssigned, "occupied field");
    require(actual.m_memberIndex==expected.m_memberIndex, "member identity field");
    require(actual.m_i20==expected.m_i20, "slot index field");
    require(actual.m_b24==expected.m_b24, "slot Boolean payload");
    require(actual.m_i28==expected.m_i28, "slot float payload");
    require(equal(actual.m_offset,expected.m_offset), "slot offset copy or preservation");
    require(actual.m_referenceSlot==expected.m_referenceSlot, "reference-slot never moves");
}

int main(int argc, char** argv)
{
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
    _set_abort_behavior(0,_WRITE_ABORT_MSG|_CALL_REPORTFAULT);
    if (argc != 2) unexpected("mode required");
    const std::string mode=argv[1];
    ObservedTimer timer;
    CommonSingletonServer::m_iBFTimer=&timer;
    SquadCoordinator squad(0,0);
    SquadFormation formation(&squad); tested=&formation;
    SquadMember members[6]={{&squad,nullptr,0,0,0},{&squad,nullptr,0,0,1},{&squad,nullptr,0,0,2},
                          {&squad,nullptr,0,0,3},{&squad,nullptr,0,0,4},{&squad,nullptr,0,0,5}};
    SquadFormation::Slot slots[6], expected[6];
    // Opaque nonnull identity only, read by actual isBot; no Bot access or ABI is claimed.
    static int botIdentity;
    for (int i=0;i<6;++i)
    {
        members[i].m_bot=reinterpret_cast<Bot*>(&botIdentity);
        squad.m_squadMembers.push_back(&members[i]);
        slots[i].hasSquadMemberAssigned=true; slots[i].m_memberIndex=i;
        slots[i].m_i20=100+i; slots[i].m_b24=(i&1)!=0; slots[i].m_i28=20.25f+i;
        slots[i].m_offset={30.0f+i,40.0f+i,50.0f+i}; slots[i].m_referenceSlot=200+i;
        formation.m_slots.push_back(&slots[i]);
        formation.m_trailingPositions.push_back({true,{300.0f+i,400.0f+i,500.0f+i}});
    }
    if (mode=="membership")
    {
        formation.m_numberOfSlots=0;
        require(!formation.isInFormation(0), "zero count returns false without slots");
        formation.m_numberOfSlots=3;
        slots[1].hasSquadMemberAssigned=false; slots[1].m_memberIndex=-1;
        require(formation.isInFormation(-1), "unoccupied minus-one slot matches");
        require(formation.isInFormation(2) && !formation.isInFormation(5), "captured count bounds scan");
        slots[2].m_memberIndex=(std::numeric_limits<int>::min)();
        require(formation.isInFormation((std::numeric_limits<int>::min)()) && !formation.isInFormation(0x10000), "full signed identifier");
        require(clockCalls==0, "membership has no timer side effects");
    }
    else if (mode=="reset")
    {
        for(int i=0;i<6;++i) expected[i]=slots[i];
        resetExpected(expected[3]); formation.resetSlot(3);
        for(int i=0;i<6;++i) compareSlot(slots[i],expected[i]);
        require(clockCalls==0 && formation.m_trailingPositions[3].first, "reset leaves timer and target flag");
    }
    else
    {
        int removed=0, selected=-1, replacement=-1;
        bool resetTrailing=false, movePosition=false;
        if(mode=="not_found") removed=99;
        else if(mode=="last") removed=5;
        else if(mode=="middle" || mode=="duplicate" || mode=="timer_mutation")
        { removed=2; selected=5; if(mode=="duplicate") slots[4].m_memberIndex=2; }
        else if(mode=="next_empty") { removed=2; slots[3].hasSquadMemberAssigned=false; }
        else if(mode=="only_zero" || mode=="zero_two" || mode=="trailing_stop")
        {
            for(int i=(mode=="only_zero"?1:2);i<6;++i) slots[i].hasSquadMemberAssigned=false;
            resetTrailing=true;
            if(mode!="only_zero") selected=1;
            if(mode=="trailing_stop") squad.m_v108={1,0,0};
        }
        else
        {
            movePosition=true; resetTrailing=true; selected=5;
            if(mode=="nearest_filters")
            { members[5].m_isSquadLeader=true; members[4].m_bot=nullptr; members[3].m_positionValid=false; selected=1; replacement=5; }
            else if(mode=="nearest_default")
            { for(auto& m:members) m.m_isSquadLeader=true; }
            else if(mode=="nearest_tie")
            { for(auto& m:members) m.m_position={1,0,0}; }
            else if(mode=="nearest_nan" || mode=="nearest_infinity")
            { for(auto& m:members) m.m_position={mode=="nearest_nan"?std::numeric_limits<float>::quiet_NaN():std::numeric_limits<float>::infinity(),0,0}; }
            else
            {
                for(int i=1;i<4;++i) members[i].m_positionValid=false;
                if(mode=="dx_spill")
                {
                    // PE rounds 1+2^-24 to float1 before squaring; candidate4's
                    // exact 1+2^-48 is greater. Widening dx wrongly chooses4.
                    squad.m_v28={-std::ldexp(1.0f,-24),0,0};
                    members[5].m_position={1,0,0}; members[4].m_position={0,1,0};
                }
                else if(mode=="dy_wide")
                {
                    // dy retains 1+2^-24; its squared minimum stores1+2^-23.
                    // Candidate4 is below that. Premature float dy wrongly keeps5.
                    squad.m_v28={0,-std::ldexp(1.0f,-24),0};
                    members[5].m_position={0,1,0}; members[4].m_position={1,0,0}; selected=4; replacement=5;
                }
                else if(mode=="before_float_store")
                {
                    // Candidate4 distance is <1 but rounds to float1. Comparing
                    // after float narrowing wrongly retains5 instead of choosing4.
                    squad.m_v28={0,std::ldexp(1.0f,-26),0};
                    members[5].m_position={0,squad.m_v28.y,1}; members[4].m_position={0,1,0}; selected=4; replacement=5;
                }
                else if(mode=="minimum_underflow")
                {
                    // First positive distance narrows to zero. The later zero is
                    // equal to that saved minimum, so the strict comparison retains5.
                    members[5].m_position={0,1.0e-30f,0}; members[4].m_position={0,0,0};
                }
                else unexpected("unknown mode");
            }
        }
        for(int i=0;i<6;++i) expected[i]=slots[i];
        if(removed<6) resetExpected(expected[removed]);
        if(selected>=0)
        {
            movedExpected(expected[removed],expected[selected],movePosition);
            resetExpected(expected[selected]);
            if(replacement>=0)
            { movedExpected(expected[selected],expected[replacement],true); resetExpected(expected[replacement]); }
        }
        if(resetTrailing) for(auto& s:expected) s.m_i20=-1;
        if(mode=="timer_mutation")
            clockCallback=[&] {
                require(slots[2].m_memberIndex==5 && !slots[5].hasSquadMemberAssigned && !members[5].m_havePath, "all moves and path invalidation before clock");
                formation.m_v72={77,78,79};
            };
        formation.leaveFormation(removed);
        if(mode=="trailing_stop") unexpected("frontier returned");
        for(int i=0;i<6;++i)
        {
            compareSlot(slots[i],expected[i]);
            require(formation.m_slots[i]==&slots[i], "slot pointers not moved or replaced");
            require(formation.m_trailingPositions[i].first==!resetTrailing, "only trailing reset clears target flags");
            require(equal(formation.m_trailingPositions[i].second,{300.0f+i,400.0f+i,500.0f+i}), "target positions untouched");
            const bool moved=(selected==i || replacement==i);
            require(members[i].m_havePath==!moved && members[i].m_flag4A==moved, "only moved members get both real path flags");
        }
        require(equal(formation.m_trailingPosition,resetTrailing?Vec3{11,12,13}:Vec3{21,22,23}), "trailing position reset policy");
        require(clockCalls==(slots[0].hasSquadMemberAssigned?1:0), "movement timer only for occupied slot0");
        if(slots[0].hasSquadMemberAssigned)
            require(formation.m_b56 && formation.m_d92==10.125+0.2 && equal(formation.m_v60,formation.m_v72), "forced movement timestamp and fresh position");
        else require(formation.m_b56 && formation.m_d92==-9.0 && equal(formation.m_v60,{-8,-9,-10}), "empty slot0 preserves previous movement state");
    }
    std::printf("PASS %s %u checks\n",mode.c_str(),checks);
    return 0;
}
