// [TEMP] Main: actual chooser/dispatcher/member leaves, real pool, typed receivers.
// DISPATCH_OBSERVERS: dispatcher only, chosen chooser/isBot/getBot for mutation oracles.
#include "squad_maneuver_runtime.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <csignal>
#include <functional>
#include <limits>
#include <string>
#include <type_traits>
#include <windows.h>
#include <crtdbg.h>
using namespace dice::hfe;
using namespace dice::hfe::ai;
static_assert(sizeof(void*)==4,"MSVC x86 required");
static_assert(std::is_same<decltype(&SquadCoordinator::chooseSquadManeuver),int(SquadCoordinator::*)()>::value,"full integer chooser");
static_assert(std::is_same<decltype(&SquadCoordinator::updateSquadManeuver),void(SquadCoordinator::*)()>::value,"void dispatcher");
static unsigned checks;
[[noreturn]] static void unexpected(const char* reason)
{std::fprintf(stderr,"FAIL %s\n",reason);std::fflush(stderr);std::exit(2);}
static void require(bool value,const char* reason){++checks;if(!value)unexpected(reason);}
static std::string mode,calls,expectedStopTrace;
static void trace(const std::string& value){if(!calls.empty())calls+=',';calls+=value;}
static SquadCoordinator* stoppingSquad;
static int expectedStopStored=77;
static void observeAbort(int)
{
    if(!stoppingSquad||calls!=expectedStopTrace||stoppingSquad->m_i148!=expectedStopStored||
       !stoppingSquad->m_b156||!stoppingSquad->m_b157)unexpected("frontier state/trace mismatch");
    std::printf("VERIFIED %s\n",mode.c_str());std::fflush(stdout);
}
static std::function<bool(int)> aliveCallback;
static std::function<unsigned(int,int)> botCallback;
static std::function<void(int,bool)> allowCallback;
static std::function<int(SquadCoordinator*)> chooserCallback;
static std::function<bool(SquadMember*)> isBotCallback;
static std::function<Bot*(SquadMember*)> getBotCallback;
static bool observeAlive(int id){trace("p"+std::to_string(id));return aliveCallback(id);}
static unsigned observeBot(int id,int op){trace(std::string(op?"f":"e")+std::to_string(id));return botCallback(id,op);}
static void observeAllow(int id,bool value){trace("a"+std::to_string(id));allowCallback(id,value);}
static unsigned* flags[2];static int constructingInfo;
static bool* firingField;
#include "mocks.inl"
namespace dice {namespace hfe {namespace ai {
// [CHOSEN, not recovered] Native scoped lifetimes; no retail publication/deep construction.
SquadCoordinator::SquadCoordinator(int side,int index):m_squadIndex(index),m_side(side),m_missionStatus(0),m_i148(77),
    m_b156(true),m_b157(true),m_v28{},m_v108{} {}
SquadMember::SquadMember(SquadCoordinator* squad,IAIPlayer* player,int squadIndex,int side,int index)
    :m_squadIndex(squadIndex),m_memberIndex(index),m_player(player),m_bot(nullptr),m_squad(squad),m_side(side),
    m_request(nullptr),m_position{},m_positionValid(true) {}
SquadMember::~SquadMember() {}
Bot::~Bot() {}
BotMain::BotMain(IAIPlayer*,unsigned int,float*,float,int,BotOrder*,BotMemory*,PlanInterpreter*,int,UrgencyGenerator*,UrgencyGenerator**)
    :m_isFiring(true),m_fireAllowed(false){firingField=&m_isFiring;}
BotMain::~BotMain() {}
IAIPlayer::~IAIPlayer() {}
Information::Information(IAIObject* object,double time):m_objectTypeFlags(0),m_time(time),m_object(object)
{flags[constructingInfo]=&m_objectTypeFlags;}
Information::~Information() {}
int Information::getInformationSort(){unexpected("Information::getInformationSort");}
#if defined(DISPATCH_OBSERVERS)
int SquadCoordinator::chooseSquadManeuver(){return chooserCallback(this);}
bool SquadMember::isBot(){return isBotCallback(this);}
Bot* SquadMember::getBot(){return getBotCallback(this);}
#endif
} } }

int main(int argc,char** argv)
{
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);_set_abort_behavior(0,_WRITE_ABORT_MSG|_CALL_REPORTFAULT);
    if(argc!=2)unexpected("mode required");mode=argv[1];
    SquadCoordinator squad(0,3);
    ObservedPlayer pa,pb,pc;pb.identity=1;pc.identity=2;
    ObservedBot ba,bb,bc;bb.identity=1;bc.identity=2;
    SquadMember a(&squad,&pa,3,0,0),b(&squad,&pb,3,0,1),c(&squad,&pc,3,0,2);
    a.m_bot=&ba;b.m_bot=&bb;c.m_bot=&bc;
    InformationManager manager,replacement;informationManager=&manager;
    constructingInfo=0;ObservedInformation info;
    constructingInfo=1;ObservedInformation otherInfo;
    const unsigned handle=manager.getPool()->acquire(&info);
    const unsigned secondHandle=replacement.getPool()->acquire(&otherInfo);
    require(handle==secondHandle&&handle,"native matching pool handles");
    squad.m_squadMembers={&a,&b};
    bool alive[3]={true,true,true},firing[3]={true,true,true};
    int expected=1;bool dispatch=false;
    std::string expectedTrace="p0,e0,f0,p1,e1,f1";
    aliveCallback=[&](int id){require(id>=0&&id<3,"player identity");return alive[id];};
    botCallback=[&](int id,int op)->unsigned{require(id>=0&&id<3,"bot identity");return op?firing[id]:handle;};
    allowCallback=[&](int,bool value){require(value&&squad.m_i148==0,"selected zero stored before allowFire(true)");};
    if(mode=="fire_leaf")
    {
        BotMain concrete(nullptr,0,nullptr,0,0,nullptr,nullptr,nullptr,0,nullptr,nullptr);
        require(!concrete.isFireAllowed(),"chosen initial false");
        Bot* base=&concrete;base->setAllowFire(true);require(concrete.isFireAllowed()&&*firingField,"real virtual true store preserves firing");
        base->setAllowFire(false);require(!concrete.isFireAllowed()&&*firingField,"real virtual false store preserves firing");
        std::printf("PASS %s %u checks\n",mode.c_str(),checks);return 0;
    }
#if defined(DISPATCH_OBSERVERS)
    dispatch=true;expected=0;squad.m_squadMembers={&a};expectedTrace="choose,i0,g0,a0";
    int choice=0;
    if(mode=="observe_one")choice=1;
    else if(mode=="observe_two")choice=2;
    else if(mode=="observe_negative")choice=-1;
    else if(mode=="observe_large")choice=0x10000;
    else require(mode=="observe_zero"||mode=="observe_retained_member"||mode=="observe_fresh_bot","known observer mode");
    expected=choice;
    chooserCallback=[&](SquadCoordinator* receiver){require(receiver==&squad&&squad.m_i148==77,"choose before storing result");trace("choose");return choice;};
    isBotCallback=[&](SquadMember* receiver){require(receiver==&a,"retained current member");trace("i0");
        if(mode=="observe_retained_member")squad.m_squadMembers[0]=&c;
        if(mode=="observe_fresh_bot")a.m_bot=&bc;return true;};
    getBotCallback=[&](SquadMember* receiver){require(receiver==&a,"getBot keeps original member after isBot callback");trace("g0");return receiver->m_bot;};
    if(choice)expectedTrace="choose";
    if(mode=="observe_fresh_bot")expectedTrace="choose,i0,g0,a2";
    if(choice==1||choice==2){stoppingSquad=&squad;expectedStopTrace=expectedTrace;expectedStopStored=choice;}
#else
    if(mode=="empty"){squad.m_squadMembers.clear();expected=0;expectedTrace="";}
    else if(mode=="human"){a.m_bot=b.m_bot=nullptr;expected=0;expectedTrace="";}
    else if(mode=="dead"){alive[0]=alive[1]=false;expected=0;expectedTrace="p0,p1";}
    else if(mode=="nonfiring"){firing[0]=firing[1]=false;expected=0;}
    else if(mode=="flag10"||mode=="flag20"||mode=="flag80"){*flags[0]=mode=="flag10"?0x10:mode=="flag20"?0x20:0x80;expected=0;}
    else if(mode=="other_flag")*flags[0]=0x200000;
    else if(mode=="invalid_position"){a.m_positionValid=false;expected=0;}
    else if(mode=="one"){b.m_bot=nullptr;expected=0;expectedTrace="p0,e0,f0";}
    else if(mode=="two"){}
    else if(mode=="continue_after_two"){squad.m_squadMembers.push_back(&c);expectedTrace+=",p2,e2,f2";}
    else if(mode.rfind("range_",0)==0)
    {
        a.m_position.x=mode=="range_below"?89.99999237060546875f:mode=="range_above"?90.00000762939453125f:90.0f;
        if(mode=="range_nan")a.m_position.x=std::numeric_limits<float>::quiet_NaN();
        expected=mode=="range_below"?1:0;
    }
    else if(mode.rfind("spill_",0)==0)
    {
        float* memberAxis=mode=="spill_x"?&a.m_position.x:mode=="spill_y"?&a.m_position.y:&a.m_position.z;
        float* squadAxis=mode=="spill_x"?&squad.m_v108.x:mode=="spill_y"?&squad.m_v108.y:&squad.m_v108.z;
        *memberAxis=90.0f;*squadAxis=0x1p-20f;expected=0;
    }
    else if(mode=="sum_extended")
    {
        const unsigned raw=0x3d0de3bd;float y;std::memcpy(&y,&raw,4);
        a.m_position={89.99999237060546875f,y,0};
        // Exact wide sum is8099.999826709051; an extra float store rounds8100.
    }
    else if(mode=="cached_count")
    {
        squad.m_squadMembers={&a};expected=0;expectedTrace="p0,e0,f0";
        aliveCallback=[&](int id){require(id==0,"appended member excluded by captured count");squad.m_squadMembers.reserve(128);squad.m_squadMembers.push_back(&b);return true;};
    }
    else if(mode=="relocated_vector"||mode=="retained_member")
    {
        aliveCallback=[&](int id){
            if(id==0){std::vector<SquadMember*> changed=mode=="relocated_vector"?std::vector<SquadMember*>{&a,&c}:std::vector<SquadMember*>{&c,&b};squad.m_squadMembers.swap(changed);}
            return true;
        };
        if(mode=="relocated_vector")expectedTrace="p0,e0,f0,p2,e2,f2";
    }
    else if(mode=="captured_bot")
    {botCallback=[&](int id,int op)->unsigned{if(!op&&id==0)a.m_bot=&bc;require(id!=2,"bot captured before equipment callback");return op?1:handle;};}
    else if(mode=="manager_swap")
    {
        *flags[0]=0x10;
        botCallback=[&](int,int op)->unsigned{if(!op)informationManager=&replacement;return op?1:handle;};
    }
    else if(mode=="lowword_zero"||mode=="stale_nonfiring")
    {
        expected=0;
        botCallback=[&](int,int op)->unsigned{
            if(op)return 0;
            if(mode=="lowword_zero"){informationManager=nullptr;return 0xabcd0000;}
            return handle^0x10000;
        };
    }
    else if(mode=="lookup_before_fire")
    {
        squad.m_squadMembers={&a};expected=0;expectedTrace="p0,e0,f0";
        botCallback=[&](int,int op)->unsigned{if(op){manager.getPool()->release(handle);return 1;}return handle;};
        // Add a second safe qualifying member with another native pool record.
        const unsigned spare=manager.getPool()->acquire(&otherInfo);squad.m_squadMembers.push_back(&b);expected=1;expectedTrace= "p0,e0,f0,p1,e1,f1";
        botCallback=[&,spare](int id,int op)->unsigned{if(op){if(id==0)manager.getPool()->release(handle);return 1;}return id==0?handle:spare;};
    }
    else if(mode=="fresh_position")
    {a.m_position={900,0,0};botCallback=[&](int id,int op)->unsigned{if(op&&id==0){a.m_position={1,0,0};squad.m_v108={2,0,0};}return op?1:handle;};}
    else if(mode=="mission_full_width"){squad.m_squadMembers.clear();squad.m_missionStatus=0x102;expected=0;expectedTrace="";}
    else if(mode=="mission_stop"||mode=="mission_after_scan")
    {
        squad.m_missionStatus=2;expected=0;
        if(mode=="mission_stop"){squad.m_squadMembers.clear();expectedTrace="";}
        else {alive[0]=alive[1]=false;expectedTrace="p0,p1";}
        stoppingSquad=&squad;expectedStopTrace=expectedTrace;
    }
    else if(mode=="alternate_stop"){dispatch=true;stoppingSquad=&squad;expectedStopTrace=expectedTrace;expectedStopStored=1;}
    else if(mode.rfind("dispatch_",0)==0)
    {
        dispatch=true;expected=0;alive[0]=alive[1]=alive[2]=false;
        expectedTrace="p0,p1,a0,a1";
        if(mode=="dispatch_empty"){squad.m_squadMembers.clear();expectedTrace="";}
        else if(mode=="dispatch_humans"){a.m_bot=b.m_bot=nullptr;expectedTrace="";}
        else if(mode=="dispatch_mixed"){b.m_bot=nullptr;expectedTrace="p0,a0";}
        else if(mode=="dispatch_duplicate"){squad.m_squadMembers={&a,&a};expectedTrace="p0,p0,a0,a0";}
        else
        {
            allowCallback=[&](int id,bool value){
                require(value&&squad.m_i148==0,"zero stored before all callbacks");
                if(id!=0)return;
                if(mode=="dispatch_append"){squad.m_squadMembers.reserve(128);squad.m_squadMembers.push_back(&c);}
                else if(mode=="dispatch_clear")squad.m_squadMembers.clear();
                else if(mode=="dispatch_replace"){std::vector<SquadMember*> changed{&a,&c};squad.m_squadMembers.swap(changed);}
                else unexpected("unknown dispatcher mutation");
            };
            expectedTrace=mode=="dispatch_append"?"p0,p1,a0,a1,a2":mode=="dispatch_clear"?"p0,p1,a0":"p0,p1,a0,a2";
        }
    }
    else unexpected("unknown main mode");
#endif
    if(stoppingSquad)std::signal(SIGABRT,observeAbort);
    if(dispatch)squad.updateSquadManeuver();
    else require(squad.chooseSquadManeuver()==expected,"actual chooser result");
    require(!stoppingSquad,"expected actual frontier must stop");
    require(calls==expectedTrace,"complete exact trace");
    require(squad.m_i148==(dispatch?expected:77),"dispatcher store versus pure chooser result");
    require(squad.m_b156&&squad.m_b157,"bounded chooser leaves flank flags untouched");
    std::printf("PASS %s %u checks\n",mode.c_str(),checks);
}
