// [TEMP] Exact member update and leaves, real handle pool, chosen native lifetimes.
// Position/mobile/movement/mission/formation dependencies are typed observers here.
// REAL_POSITION keeps the actual position helper, including its real owner stop.
#include "member_update_runtime.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <float.h>
#include <functional>
#include <limits>
#include <string>
#include <type_traits>
#include <windows.h>
#include <crtdbg.h>
using namespace dice::hfe;
using namespace dice::hfe::ai;
static_assert(sizeof(void*)==4,"MSVC x86 required");
static_assert(std::is_same<decltype(&SquadMember::update),void(SquadMember::*)(double)>::value,"void member update contract");
static unsigned checks;
[[noreturn]] static void unexpected(const char* reason)
{ std::fprintf(stderr,"FAIL %s\n",reason);std::fflush(stderr);std::exit(2); }
static void require(bool value,const char* reason) { ++checks;if(!value)unexpected(reason); }
static std::string mode,calls,stopTrace;
static bool checkedStop;
static void trace(const std::string& event)
{ if(checkedStop)unexpected("callback after expected terminal trace");if(!calls.empty())calls+=',';calls+=event; }
static void checkpoint()
{ if(!stopTrace.empty()&&calls==stopTrace){checkedStop=true;std::printf("VERIFIED %s\n",mode.c_str());std::fflush(stdout);} }
static float bits(unsigned value) { float f;std::memcpy(&f,&value,4);return f; }
static bool same(const Vec3& a,const Vec3& b) { return std::memcmp(&a,&b,sizeof(a))==0; }
static std::function<void(SquadMember*)> positionCallback;
static std::function<bool(SquadMember*)> mobileCallback;
static std::function<void(SquadMember*,bool)> movementCallback;
static std::function<bool(SquadFormation*,int)> membershipCallback;
static std::function<int(SquadCoordinator*)> missionCallback;
static std::function<SquadMember*(SquadCoordinator*)> leaderCallback;
static std::function<unsigned(int,int)> playerCallback,botCallback;
static unsigned playerValue(int identity,int operation)
{ trace("p"+std::to_string(identity)+":"+std::to_string(operation));auto v=playerCallback(identity,operation);checkpoint();return v; }
static unsigned botValue(int identity,int operation)
{ trace("b"+std::to_string(identity)+":"+std::to_string(operation));auto v=botCallback(identity,operation);checkpoint();return v; }
static unsigned* informationFlags;
static void** informationPlugin;
#include "mocks.inl"
namespace dice { namespace hfe { namespace ai {
// [CHOSEN, not recovered] Native field/container construction without retail publication.
SquadCoordinator::SquadCoordinator(int side,int index):m_squadIndex(index),m_side(side),m_formation(nullptr) {}
SquadMember::SquadMember(SquadCoordinator* squad,IAIPlayer* player,int squadIndex,int side,int index)
    :m_squadIndex(squadIndex),m_memberIndex(index),m_player(player),m_bot(nullptr),m_squad(squad),m_side(side),
    m_time24(0.0),m_havePath(true),m_request(nullptr),m_int38(777),m_position{1,2,3},m_positionValid(false),
    m_flag49(true),m_flag4A(false),m_flag4B(true),m_moved(true),m_vector54{},m_vector60{} {}
SquadMember::~SquadMember() {}
SquadFormation::SquadFormation(SquadCoordinator* squad):m_coordinator(squad),m_numberOfSlots(0) {}
IAIPlayer::~IAIPlayer() {}
Bot::~Bot() {}
int Bot::getBotType() const { unexpected("Bot::getBotType"); }
BAPContext* Bot::getScriptContext() { unexpected("Bot::getScriptContext"); }
Information::Information(IAIObject* object,double time):m_objectTypeFlags(0),m_time(time),m_object(object),m_armament(nullptr)
{ informationFlags=&m_objectTypeFlags;informationPlugin=reinterpret_cast<void**>(&m_armament); }
Information::~Information() {}
int Information::getInformationSort() { unexpected("Information::getInformationSort"); }
#if !defined(REAL_POSITION)
void SquadMember::updateMemberPos() { trace("pos");positionCallback(this); }
#endif
bool SquadMember::isMobile() { trace("mobile");bool result=mobileCallback(this);checkpoint();return result; }
void SquadMember::moveWithFormation() { trace("move1");movementCallback(this,true); }
void SquadMember::moveWithoutFormation() { trace("move0");movementCallback(this,false); }
bool SquadFormation::isInFormation(int index) { return membershipCallback(this,index); }
int SquadCoordinator::getMissionStatus() { return missionCallback(this); }
SquadMember* SquadCoordinator::getSquadLeader() { return leaderCallback(this); }
} } }

int main(int argc,char** argv)
{
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
    _set_abort_behavior(0,_WRITE_ABORT_MSG|_CALL_REPORTFAULT);
    if(argc!=2)unexpected("mode required");mode=argv[1];
    SquadCoordinator squad(0,3),otherSquad(1,4);
    SquadFormation formation(&squad),otherFormation(&otherSquad);
    squad.m_formation=&formation;otherSquad.m_formation=&otherFormation;
    ObservedPlayer player,replacementPlayer,leaderPlayer;replacementPlayer.identity=1;leaderPlayer.identity=2;
    ObservedBot bot,replacementBot;replacementBot.identity=1;
    SquadMember member(&squad,&player,3,0,7),leader(&squad,&leaderPlayer,3,0,8);
    InformationManager manager,otherManager;informationManager=&manager;
    ObservedInformation information;
    const unsigned handle=manager.getPool()->acquire(&information);
    const unsigned otherHandle=otherManager.getPool()->acquire(&information);
    require(handle==otherHandle&&handle!=0,"native pools begin with matching valid handles");
    double time=1.0;bool invalidated=false,expectedFlag4B=true,expectedFormation=false;
    double expectedTime=0.0;
    std::string expected="pos,m0,q7,move0";
    SquadFormation* capturedFormation=&formation;
    positionCallback=[](SquadMember*){};
    mobileCallback=[](SquadMember*){return false;};
    playerCallback=[](int,int operation)->unsigned{require(operation<2,"unexpected vehicle query");return 0;};
    botCallback=[](int,int operation)->unsigned{require(operation==0,"firing guard suppresses equipment");return 1;};
    missionCallback=[&](SquadCoordinator* receiver){trace(receiver==&squad?"m0":"m1");return 0;};
    leaderCallback=[&](SquadCoordinator* receiver)->SquadMember*{trace(receiver==&squad?"l0":"l1");return nullptr;};
    membershipCallback=[&](SquadFormation* receiver,int index){
        require(receiver==capturedFormation,"formation retained from after position callback");
        require(index==member.m_memberIndex,"fresh member index after earlier callbacks");
        require(member.m_int38==777,"clear follows membership callback");
        trace("q"+std::to_string(index));member.m_int38=999;return expectedFormation;
    };
    movementCallback=[&](SquadMember* receiver,bool inFormation){
        require(receiver==&member&&inFormation==expectedFormation,"selected movement receiver and branch");
        require(member.m_int38==0,"int38 cleared after query and before movement");
    };
    const float nan=std::numeric_limits<float>::quiet_NaN();
    unsigned originalControl=0;bool changedControl=false;
#if defined(REAL_POSITION)
    expected="p0:2,m0,q7,move0";
    member.m_positionValid=true;member.m_moved=true;
    const Vec3 oldPosition=member.m_position;
    playerCallback=[&](int identity,int operation)->unsigned{
        require(identity==0&&operation==2,"only entry current vehicle query");
        require(member.m_positionValid&&member.m_moved,"position flags clear after getter");
        if(mode=="real_position_callback") { member.m_squad=&otherSquad;member.m_memberIndex=99;capturedFormation=&otherFormation; }
        if(mode=="real_position_stop")return 1;
        return 0;
    };
    if(mode=="real_position_callback")expected="p0:2,m1,q99,move0";
    else if(mode=="real_position_stop")stopTrace="p0:2";
    else require(mode=="real_position_null","known actual position mode");
#else
    if(mode=="no_bot"){}
    else if(mode=="timer_equal"||mode=="timer_below"||mode=="timer_nan"||mode=="previous_nan")
    {
        member.m_time24=4.0;time=mode=="timer_below"?12.999999999999998:13.0;expectedTime=4.0;
        if(mode=="timer_nan")time=std::numeric_limits<double>::quiet_NaN();
        if(mode=="previous_nan")member.m_time24=expectedTime=std::numeric_limits<double>::quiet_NaN();
        member.m_vector54={10,20,30};
    }
    else if(mode=="timer_extended")
    {
        require(_controlfp_s(&originalControl,0,0)==0,"read x87 control");
        unsigned ignored=0;require(_controlfp_s(&ignored,_PC_64,_MCW_PC)==0,"select extended x87 precision");changedControl=true;
        member.m_time24=9007199254740994.0;time=9007199254741004.0;
        expectedTime=time;member.m_vector54={1,0,0};invalidated=true;
    }
    else if(mode=="displacement_clear"||mode=="displacement_nan"||mode=="displacement_dx_spill")
    {
        time=10.0;expectedTime=time;
        if(mode=="displacement_nan")member.m_vector54={nan,0,0};
        else if(mode=="displacement_clear") {member.m_vector54={4,5,6};member.m_vector60={3,1,-8};invalidated=true;}
        else {member.m_vector54={bits(0x3e13a35f),bits(0x3d444098),0};member.m_vector60={bits(0x3d670775),0,0};invalidated=true;}
    }
    else if(mode=="invalid_position"||mode=="null_player"||mode=="dead_player"||mode=="mobile_false"||mode=="artillery_stop")
    {
        member.m_bot=&bot;expected="pos,p0:0,b0:0,m0,q7,move0";
        if(mode=="null_player") {member.m_player=nullptr;expected="pos,b0:0,m0,q7,move0";}
        else if(mode!="dead_player") playerCallback=[](int,int operation)->unsigned{require(operation==0,"alive gate only");return 1;};
        if(mode=="invalid_position") {time=10;expectedTime=time;}
        if(mode=="mobile_false"||mode=="artillery_stop")
        {
            member.m_positionValid=true;expected="pos,p0:0,mobile,b0:0,m0,q7,move0";expectedFlag4B=false;
            if(mode=="artillery_stop") {mobileCallback=[&](SquadMember* r){require(r==&member,"mobile receiver");return true;};stopTrace="pos,p0:0,mobile";}
        }
    }
    else if(mode=="capture_formation")
    {
        positionCallback=[&](SquadMember*){member.m_squad=&otherSquad;capturedFormation=&otherFormation;};
        missionCallback=[&](SquadCoordinator* r){require(r==&otherSquad,"fresh squad mission query");trace("m1");member.m_squad=&squad;otherSquad.m_formation=&formation;return 0;};
        expected="pos,m1,q7,move0";
    }
    else if(mode=="fresh_index")
    {missionCallback=[&](SquadCoordinator*){trace("m0");member.m_memberIndex=99;return 0;};expected="pos,m0,q99,move0";}
    else if(mode=="membership_clear_timing"){}
    else if(mode=="within_formation") {expectedFormation=true;expected="pos,m0,q7,move1";}
    else if(mode=="waypoint_stop")
    {
        member.m_bot=&bot;member.m_positionValid=true;time=10;
        botCallback=[&](int id,int op)->unsigned{require(id==0&&op==2,"waypoints selected before artillery/firing");require(member.m_time24==0,"timestamp remains old before owner seam");require(member.m_havePath&&!member.m_flag4A,"path remains unchanged before waypoint owner seam");return 0;};
        stopTrace="pos,b0:2";
    }
    else if(mode.rfind("equipment_",0)==0)
    {
        member.m_bot=&bot;expected="pos,p0:0,b0:0,b0:1,m0,q7,move0";
        *informationFlags=0x200000;*informationPlugin=nullptr;
        botCallback=[&](int id,int op)->unsigned{
            if(op==0){require(id==0,"initial firing receiver");if(mode=="equipment_fresh_bot")member.m_bot=&replacementBot;return mode=="equipment_firing";}
            require(op==1&&id==(mode=="equipment_fresh_bot"?1:0),"fresh bot equipment receiver");
            if(mode=="equipment_manager_swap")informationManager=&otherManager;
            return mode=="equipment_zero"?0:mode=="equipment_stale"?(handle^0x10000):handle;
        };
        if(mode=="equipment_firing")expected="pos,p0:0,b0:0,m0,q7,move0";
        if(mode=="equipment_fresh_bot")expected="pos,p0:0,b0:0,b1:1,m0,q7,move0";
        if(mode=="equipment_wrong_flags") {*informationFlags=0x100000;*informationPlugin=&member;}
        if(mode=="equipment_manager_swap")
        {
            manager.getPool()->release(handle); // PE must choose the fresh manager after callback.
            *informationPlugin=&member;stopTrace="pos,p0:0,b0:0,b0:1";
        }
        if(mode=="equipment_stop") {*informationPlugin=&member;stopTrace="pos,p0:0,b0:0,b0:1";}
    }
    else if(mode.rfind("mission_",0)==0)
    {
        missionCallback=[&](SquadCoordinator* r){require(r==&squad,"first mission receiver");trace("m0");if(mode=="mission_fresh_squad")member.m_squad=&otherSquad;return 1;};
        leaderCallback=[&](SquadCoordinator* r)->SquadMember*{
            require(r==(mode=="mission_fresh_squad"?&otherSquad:&squad),"fresh squad leader receiver");trace(r==&squad?"l0":"l1");
            return mode.find("leader")!=std::string::npos?&leader:nullptr;
        };
        expected="pos,m0,l0,p0:0,q7,move0";
        if(mode=="mission_dead"){}
        else if(mode=="mission_fresh_squad")expected="pos,m0,l1,p0:0,q7,move0";
        else if(mode=="mission_down") {playerCallback=[](int,int)->unsigned{return 1;};expected="pos,m0,l0,p0:0,p0:1,q7,move0";}
        else if(mode=="mission_leader_dead")expected="pos,m0,l0,p2:0,p0:0,q7,move0";
        else if(mode=="mission_leader_down")
        {playerCallback=[](int id,int)->unsigned{return id==2;};expected="pos,m0,l0,p2:0,p2:1,p0:0,q7,move0";}
        else if(mode=="mission_fresh_player")
        {playerCallback=[&](int id,int op)->unsigned{if(!op){require(id==0,"initial self alive");member.m_player=&replacementPlayer;return 1;}require(id==1,"fresh player for down query");return 1;};expected="pos,m0,l0,p0:0,p1:1,q7,move0";}
        else if(mode=="mission_self_stop"||mode=="mission_leader_stop")
        {playerCallback=[](int,int op)->unsigned{return op==0;};stopTrace=mode=="mission_self_stop"?"pos,m0,l0,p0:0,p0:1":"pos,m0,l0,p2:0,p2:1";}
        else unexpected("unknown mission case");
    }
    else unexpected("unknown mode");
#endif
    const Vec3 initialHistory=member.m_vector54;
    member.update(time);
    if(changedControl){unsigned ignored=0;require(_controlfp_s(&ignored,originalControl,_MCW_PC)==0,"restore x87 precision");}
    require(stopTrace.empty(),"expected production boundary must stop");
    require(calls==expected,"exact callback trace");
    require(member.m_havePath==!invalidated&&member.m_flag4A==invalidated,"path and inverse flag");
    require(member.m_flag4B==expectedFlag4B&&member.m_flag49,"artillery and adjacent flags");
    require((std::isnan(expectedTime)&&std::isnan(member.m_time24))||member.m_time24==expectedTime,"timer stores only completed reconsideration");
    require(same(member.m_vector54,invalidated?member.m_vector60:initialHistory),"history copy only on displacement invalidation");
#if defined(REAL_POSITION)
    require(!member.m_positionValid&&!member.m_moved&&same(oldPosition,member.m_position),"real null-position preserves coordinates and clears flags");
#endif
    std::printf("PASS %s %u checks\n",mode.c_str(),checks);
}
