// [TEMP] Exact extracted selector and squad getters; chosen native lifetimes and
// receiving callbacks. Actual predicate rejects a native null-unit record. No claim of
// real squad/SAI construction, spatial filtering, pathfinding or random refill.
#include "bot_selector_runtime.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <cstddef>
#include <windows.h>
#include <crtdbg.h>
using namespace dice::hfe;
using namespace dice::hfe::ai;
static_assert(sizeof(void*)==4 && sizeof(SpawnChoice)==28,"native x86 record");
static_assert(offsetof(SpawnChoice,group)==0 && offsetof(SpawnChoice,strategicValue)==4 &&
    offsetof(SpawnChoice,informationCount)==8 && offsetof(SpawnChoice,freeStrength)==12 &&
    offsetof(SpawnChoice,object)==16 && offsetof(SpawnChoice,distance)==20 &&
    offsetof(SpawnChoice,weight)==24,"paired primitive SpawnChoice fields");
[[noreturn]] static void unexpected(const char* message)
{ std::fprintf(stderr,"UNEXPECTED %s\n",message); std::fflush(stderr); std::abort(); }
static void require(bool value,const char* message) { if(!value) unexpected(message); }
static std::string mode,calls;
static std::string n(int value) { return std::to_string(value); }
static void trace(const std::string& event) { if(!calls.empty()) calls+=','; calls+=event; }
static float bits(unsigned raw) { float result; std::memcpy(&result,&raw,4); return result; }
static unsigned undoRight(unsigned value,unsigned shift)
{ unsigned result=value; for(int i=0;i<8;++i) result=value^(result>>shift); return result; }
static unsigned undoLeft(unsigned value,unsigned shift,unsigned mask)
{ unsigned result=value; for(int i=0;i<8;++i) result=value^((result<<shift)&mask); return result; }
static unsigned stateFor(unsigned value)
{ return undoRight(undoLeft(undoLeft(undoRight(value,18),15,0xDB8B0000u),7,0x2B5B2500u),11); }
static unsigned firstRaw=0xffffffffu,secondRaw=0;
static int randomConstruction,*randomIndices[2],refills;
static const AIRandom* randoms[2];
static AIRandom* alternateRandom;
static IAIGame* alternateGame;
static IAIMain* alternateMain;
static IAIInformationEnvironment* alternateEnvironment;
static IAIPathfinding* alternatePath;
static AIStrategicObjectManager* managers[2];
static SAI* sai;
static SquadMember *member,*leaders[2];
static SquadCoordinator *squads[2];
static IAIPlayer* players[3];
static AIStrategicObject* objects[4];
static AIStrategicObject** outputSlot;
static Information* rejectedInformation;
static std::vector<unsigned>* queryVector;
static std::list<int> groups;
static bool playerAI[3],playerAlive[3]={true,true,true},playerDown[3];
static int currentGroup,added=-1,queries,validCalls;
struct Point { bool eligible=true,valid=true; int control=0,object=0,reads=0; Vec3 first={},second={}; };
static Point points[32];
static bool nearArm;

static const std::list<int>& observeList(int identity,int side)
{
    trace("list"+n(identity)); require(identity==0 && side==2,"entry game and side");
    if(mode=="cached_context") CommonSingletonServer::m_iaiGame=alternateGame;
    return groups;
}
static SAI* observeSAI(int identity,int side)
{
    trace("main"+n(identity)); require(identity==0 && side==2,"fresh main after group query");
    if(mode=="cached_context") { IAIMain::m_instance=alternateMain; AIStrategicObjectManager::m_instance=managers[1]; }
    return sai;
}
static SquadMember* observeMember() { trace("member"); return member; }
static bool observePlayer(int identity,int operation)
{
    trace(std::string(operation==0?"ai":operation==1?"alive":"down")+n(identity));
    if(mode=="leader_reload") {
        if(operation==0) member->m_squad=squads[1];
        if(operation==1) leaders[1]->m_player=players[2];
    }
    return operation==0 ? playerAI[identity] : operation==1 ? playerAlive[identity] : playerDown[identity];
}
static bool observeEligible(int identity,int side,int group)
{
    trace("eligible"+n(identity)+":"+n(group));
    require(identity==0 && side==2 && group>=0 && group<32,"captured game and side at eligibility");
    if(mode=="live_list" && group==10) groups.push_back(11);
    return points[group].eligible;
}
static const Vec3& observeSpawnPosition(int identity,int group)
{
    trace("pos"+n(identity)+":"+n(group)); require(identity==0,"captured game for position");
    currentGroup=group; Point& p=points[group]; ++p.reads;
    return nearArm && p.reads%2==0 ? p.second : p.first;
}
static int observeControl(int identity,int group)
{
    trace("control"+n(identity)+":"+n(group)); require(identity==0,"captured game for control");
    if(mode=="weighted_mutation") points[group].first.x=42;
    return points[group].control;
}
static int observeTraceMap(int identity)
{
    trace("map"+n(identity)); require(identity==0,"cached pathfinder map query");
    IAIPathfinding::instance=alternatePath;
    return -17;
}
static bool observeValid(int identity,int map,int type,const Vec3& position,bool flag)
{
    trace("valid"+n(identity)+":"+n(currentGroup)); ++validCalls;
    require(identity==0 && map==-17 && type==3 && flag,"typed path query, cached receiver, returned map");
    require(std::memcmp(&position,&points[currentGroup].second,sizeof(Vec3))==0,"second position copy for validation");
    return points[currentGroup].valid;
}
static void observeQuery(int identity,std::vector<unsigned>& out,const Vec3& position,
                         float radius,int side,bool flag,IAIInformationGridPredicate* predicate)
{
    trace("query"+n(identity)+":"+n(currentGroup)); ++queries;
    require(identity==0 && radius==50.0f && side==2 && flag && predicate,"radius query arguments and cached environment");
    require(out.empty(),"actual local vector cleared before each query");
    require(&position==&points[currentGroup].first,"borrowed spawn reference retained through control and closest");
    if(mode=="predicate_refusal")
        require(!predicate->includeInformation(rejectedInformation),"actual predicate rejects a native null-unit record");
    out.push_back(91); out.push_back(92); queryVector=&out;
    if(mode=="weighted_mutation") {
        require(position.x==42,"control callback mutates borrowed position");
        objects[0]->m_f164[1]=99; *outputSlot=objects[1];
    }
    if(mode=="cached_context") {
        AISingletonServer::m_iaiInformationEnvironment=alternateEnvironment;
        AIRandom::m_instance=alternateRandom;
    }
}
static const Vec2& observeObjectPosition(int identity,int side)
{
    trace("object"+n(identity)); require(side==2,"object side");
    if(mode=="weighted_mutation") {
        require(identity==1,"output object reloaded after query");
        queryVector->push_back(93); *outputSlot=objects[2]; groups.front()=12;
    }
    return objects[identity]->m_position[1];
}
static void observeAdd(int group)
{
    trace("add:"+n(group)); require(added==-1,"single selected counter callback"); added=group;
    if(mode=="leader_reload") groups.front()=77;
    if(mode=="selected_reload") {
        require(BotSpawner::m_pointWeightVector.size()==1,"one selected record");
        BotSpawner::m_pointWeightVector[0].group=77;
        BotSpawner::m_pointWeightVector[0].object=objects[3];
    }
}

#include "bot_selector_mocks.inl"

namespace dice { namespace hfe { namespace ai {
// [TEMP] Explicit native lifetime choices. Containers are genuinely constructed.
Bot::~Bot() {}
int Bot::getBotType() const { unexpected("Bot::getBotType"); }
BAPContext* Bot::getScriptContext() { unexpected("Bot::getScriptContext"); }
IAIPlayer::~IAIPlayer() {}
IAIGame::IAIGame() {}
IAIGame::~IAIGame() {}
IAIMain* IAIMain::m_instance=nullptr;
IAIMain::IAIMain() {}
IAIMain::~IAIMain() {}
ISAI::~ISAI() {}
SAI::SAI(int side,int,int,Vec2,Vec2):m_side(side) {}
SAI::~SAI() {}
IAIInformationEnvironment::IAIInformationEnvironment() {}
IAIInformationEnvironment::~IAIInformationEnvironment() {}
IAIPathfinding* IAIPathfinding::instance=nullptr;
IAIPathfinding::IAIPathfinding() {}
IAIPathfinding::~IAIPathfinding() {}
void* CommonSingletonServer::m_iaiGame=nullptr;
void* CommonSingletonServer::m_iBFTimer=nullptr;
void* CommonSingletonServer::m_iaiSettings=nullptr;
void* AISingletonServer::m_iaiInformationEnvironment=nullptr;
// Tests the predicate's rejected arm with real Information/IPIUnit code; unused globals stay null.
// RejectedInformation initializes the otherwise-unwritten prefix.
InformationManager* informationManager=nullptr;
AIRandom* AIRandom::m_instance=nullptr;
AIRandom::AIRandom():m_seed(0),m_index(0)
{
    int identity=randomConstruction++; randoms[identity]=this; randomIndices[identity]=&m_index;
    for(unsigned& word:m_state) word=0;
    m_state[0]=stateFor(identity==0 ? firstRaw : secondRaw);
    m_state[1]=stateFor(secondRaw);
    m_state[623]=stateFor(firstRaw);
    if(identity==0 && mode=="weighted_refill") m_index=623;
}
AIRandom::~AIRandom() {}
unsigned int AIRandom::generateNumbers() const
{
    require(mode=="weighted_refill" && this==randoms[0],"chosen refill only in declared boundary mode");
    ++refills; trace("refill");
    // Typed mutation of a genuinely non-const object; only the refill boundary is modeled.
    AIRandom* self=const_cast<AIRandom*>(this);
    self->m_index=0; self->m_state[0]=stateFor(secondRaw); return 0;
}
AIStrategicObject::AIStrategicObject(float,float)
{
    m_position[0]=m_position[1]=Vec2{0,0}; m_index=-1;
    for(float& value:m_f164) value=1.0f;
}
AIStrategicObject::~AIStrategicObject() {}
AIStrategicObjectManager* AIStrategicObjectManager::m_instance=nullptr;
AIStrategicObjectManager::AIStrategicObjectManager() {}
AIStrategicObjectManager::~AIStrategicObjectManager() {}
// Chosen receiver for the previously separately tested closest-object method.
AIStrategicObject* AIStrategicObjectManager::getClosestObject(const Vec3& position,int side) const
{
    trace("closest:"+n(currentGroup));
    require(this==managers[0] && side==2 && &position==&points[currentGroup].first,"cached manager and borrowed position");
    if(mode=="cached_context") squads[0]->m_v108=Vec3{99,98,97};
    const int index=points[currentGroup].object;
    return index<0 ? nullptr : objects[index];
}
SquadCoordinator::SquadCoordinator(int side,int index):m_squadIndex(index),m_side(side),m_squadLeaderIndex(0),m_formation(nullptr)
{ m_v108=Vec3{0,0,0}; }
SquadMember::SquadMember(SquadCoordinator* squad,IAIPlayer* player,int,int,int):m_isSquadLeader(false),m_player(player),m_squad(squad) {}
SquadMember::~SquadMember() {}
} } }

static void addPoint(int group,int object,float x)
{
    groups.push_back(group); points[group].object=object;
    points[group].first=Vec3{x,0,0}; points[group].second=Vec3{x+100,200,300};
}
static void expectTrace(const std::string& expected)
{
    if(calls!=expected) { std::fprintf(stderr,"FAIL trace\nexpected: %s\nactual:   %s\n",expected.c_str(),calls.c_str()); std::exit(2); }
}
static std::string weighted(int group,int object,bool query=true)
{
    const std::string id=n(group);
    return ",eligible0:"+id+",pos0:"+id+",control0:"+id+
        (query ? ",closest:"+id+",query0:"+id+",object"+n(object) : "");
}
static std::string nearby(int group,bool validate)
{
    const std::string id=n(group);
    return ",eligible0:"+id+",pos0:"+id+",pos0:"+id+
        (validate ? ",map0,valid0:"+id : "");
}

int main(int argc,char** argv)
{
    require(argc==2,"mode"); mode=argv[1];
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
    _set_abort_behavior(0,_WRITE_ABORT_MSG|_CALL_REPORTFAULT);
    nearArm=mode=="leader_nearest" || mode=="leader_tie" || mode=="leader_invalid" ||
        mode=="leader_nan" || mode=="leader_rounding" || mode=="leader_reload" || mode=="random_above" || mode=="leader_fallback";
    if(mode=="leader_reload" || mode=="leader_ai" || mode=="leader_dead" || mode=="leader_down") firstRaw=0;
    if(mode=="random_equal") firstRaw=0x4ccccd00u;
    if(mode=="random_above") firstRaw=0x4ccccd01u;
    if(mode=="weighted_last") secondRaw=0xffffffffu;
    if(mode=="weighted_boundary" || mode=="weighted_wide" || mode=="weighted_refill") secondRaw=0x80000000u;
    AIRandom random,otherRandom; AIRandom::m_instance=&random; alternateRandom=&otherRandom;
    ObservedBot bot;
    RejectedInformation rejection; rejectedInformation=&rejection;
    ObservedGame game,otherGame; otherGame.identity=1; alternateGame=&otherGame; CommonSingletonServer::m_iaiGame=&game;
    ObservedMain main,otherMain; otherMain.identity=1; alternateMain=&otherMain; IAIMain::m_instance=&main;
    ObservedEnvironment environment,otherEnvironment; otherEnvironment.identity=1; alternateEnvironment=&otherEnvironment;
    AISingletonServer::m_iaiInformationEnvironment=&environment;
    ObservedPath path,otherPath; otherPath.identity=1; alternatePath=&otherPath; IAIPathfinding::instance=&path;
    ObservedSAI observedSAI; sai=&observedSAI;
    AIStrategicObjectManager manager,otherManager; managers[0]=&manager; managers[1]=&otherManager; AIStrategicObjectManager::m_instance=&manager;
    ObservedObject strategic[4]; for(int i=0;i<4;++i) { strategic[i].identity=i; objects[i]=&strategic[i]; }
    ObservedPlayer playerObjects[3]; for(int i=0;i<3;++i) { playerObjects[i].identity=i; players[i]=&playerObjects[i]; }
    SquadCoordinator squad(2,0),otherSquad(2,1); squads[0]=&squad; squads[1]=&otherSquad;
    SquadMember current(&squad,players[0],0,0,0),leader(&squad,players[0],0,0,0),otherLeader(&otherSquad,players[1],0,0,0);
    member=&current; leaders[0]=&leader; leaders[1]=&otherLeader;
    squad.m_squadMembers.push_back(&leader); otherSquad.m_squadMembers.push_back(&otherLeader);
    current.m_isSquadLeader=!nearArm;
    if(mode=="leader_dead" || mode=="leader_down" || mode=="leader_ai" || mode=="random_equal") current.m_isSquadLeader=false;
    if(mode=="leader_ai" || mode=="random_equal" || mode=="random_above") playerAI[0]=true;
    if(mode=="leader_dead") playerAlive[0]=false;
    if(mode=="leader_down") playerDown[0]=true;
    AIStrategicObject* output=objects[3]; outputSlot=&output;
    BotSpawner::m_pointWeightVector.reserve(16);
    BotSpawner::m_pointWeightVector.push_back(SpawnChoice{99,9,9,9,objects[3],9,9});
    std::string expected="list0,main0,member";
    int wanted=10; AIStrategicObject* wantedObject=objects[0];
    if(mode=="empty") {
        require(BotSpawner::findSuitableSpawnGroup(2,output,&bot)==0,"empty result zero");
        require(output==objects[3] && BotSpawner::m_pointWeightVector.size()==1 && BotSpawner::m_pointWeightVector[0].group==99,"empty leaves output/static vector");
        require(*randomIndices[0]==0,"empty no random draw"); expectTrace("list0");
        std::printf("PASS %s\n",mode.c_str()); return 0;
    }
    addPoint(10,0,nearArm?2.0f:0.0f);
    if(mode=="none") { points[10].eligible=false; expected+=",eligible0:10"; wanted=-1; wantedObject=objects[3]; }
    else if(mode=="control_minus_one") { points[10].control=-1; expected+=weighted(10,0,false); wanted=-1; wantedObject=objects[3]; }
    else if(mode=="closest_null") { points[10].object=-1; expected+=weighted(10,0,false)+",closest:10"; wanted=-1; wantedObject=nullptr; }
    else if(nearArm) {
        expected+= mode=="leader_reload" ? ",ai0,alive1,down2" : ",alive0,down0";
        if(mode=="leader_nearest") {
            addPoint(11,1,1); addPoint(12,2,1); wanted=11;
            expected+=nearby(10,true)+nearby(11,true)+nearby(12,false);
        } else if(mode=="leader_tie") {
            addPoint(11,1,2); expected+=nearby(10,true)+nearby(11,false);
        } else if(mode=="leader_invalid") {
            points[10].valid=false; addPoint(11,1,3); wanted=11;
            expected+=nearby(10,true)+nearby(11,true);
        } else if(mode=="leader_nan") {
            points[10].first.x=std::numeric_limits<float>::quiet_NaN(); addPoint(11,1,3); wanted=11;
            expected+=nearby(10,false)+nearby(11,true);
        } else if(mode=="leader_rounding") {
            squad.m_v108.x=1; points[10].first.x=0; addPoint(11,1,0.0000000298023223876953125f);
            expected+=nearby(10,true)+nearby(11,false);
        } else { expected+=nearby(10,true); if(mode=="leader_reload") wanted=77; }
        if(mode=="leader_fallback") {
            points[10].valid=false; expected+=weighted(10,0);
        } else wantedObject=nullptr;
        expected+=",add:"+n(mode=="leader_reload"?10:wanted);
    } else {
        if(mode=="leader_ai" || mode=="random_equal") expected+=",ai0";
        if(mode=="leader_dead") expected+=",ai0,alive0";
        if(mode=="leader_down") expected+=",ai0,alive0,down0";
        const bool pair=mode=="weighted_first" || mode=="weighted_last" || mode=="weighted_clamp" ||
            mode=="weighted_nan" || mode=="weighted_infinity" || mode=="weighted_boundary" ||
            mode=="weighted_wide" || mode=="weighted_refill" || mode=="cached_context" || mode=="live_list";
        if(pair && mode!="live_list") addPoint(11,1,0);
        if(mode=="live_list") { points[11].object=1; points[11].first=Vec3{0,0,0}; }
        if(mode=="weighted_last" || mode=="weighted_nan" || mode=="weighted_infinity" || mode=="weighted_wide") { wanted=11; wantedObject=objects[1]; }
        if(mode=="weighted_clamp") { objects[0]->m_f164[1]=-5; objects[1]->m_f164[1]=0; }
        if(mode=="weighted_nan") objects[0]->m_f164[1]=std::numeric_limits<float>::quiet_NaN();
        if(mode=="weighted_infinity") objects[0]->m_f164[1]=std::numeric_limits<float>::infinity();
        if(mode=="weighted_wide" || mode=="weighted_refill") objects[1]->m_f164[1]=bits(0x3f800001);
        if(mode=="weighted_mutation") {
            objects[0]->m_f164[1]=7; objects[1]->m_position[1]=Vec2{3,4};
            wanted=12; wantedObject=objects[2]; expected+=weighted(10,1);
        } else expected+=weighted(10,0);
        if(pair) expected+=weighted(11,1);
        if(mode=="weighted_refill") expected+=",refill";
        expected+=",add:"+n(wanted);
        if(mode=="selected_reload") { wanted=77; wantedObject=objects[3]; }
    }
    const int result=BotSpawner::findSuitableSpawnGroup(2,output,&bot);
    require(result==wanted && output==wantedObject,"return and output object");
    expectTrace(expected);
    const auto& choices=BotSpawner::m_pointWeightVector;
    const bool nearSuccess=nearArm && mode!="leader_fallback";
    if(nearSuccess || wanted<0) require(choices.empty(),"static candidates cleared on all nonempty invocations");
    else {
        require(!choices.empty() && choices.size()==static_cast<size_t>(queries),"one native record per query");
        for(const auto& c:choices) require(c.informationCount==2 && c.freeStrength==0,"actual count and uncalled predicate accumulator");
        if(mode=="weighted_mutation") {
            require(choices[0].group==12 && choices[0].strategicValue==7 && choices[0].object==objects[2] && choices[0].distance==5,"callback capture/reload record");
            require(choices[0].weight==6.5f,"wide distance penalty then record float");
        }
        if(mode=="weighted_clamp") require(choices[0].weight==1 && choices[1].weight==1,"strict minimum clamp");
        if(mode=="weighted_nan") require(std::isnan(choices[0].weight),"NaN retained by clamp");
        if(mode=="weighted_infinity") require(std::isinf(choices[0].weight),"infinite weight retained, zero draw NaN falls through");
        if(mode=="cached_context") require(choices[0].distance==0 && choices[1].distance==0,"squad XZ copied before callback mutation");
    }
    if(mode=="cached_context") require(*randomIndices[0]==1 && *randomIndices[1]==1,"fresh random singleton at roulette draw");
    else if(mode=="weighted_refill") require(refills==1 && *randomIndices[0]==1,"exact second draw refill boundary");
    else require(*randomIndices[0]==(nearSuccess || wanted<0 ? 1:2),"actual random draw count");
    std::printf("PASS %s\n",mode.c_str()); return 0;
}
