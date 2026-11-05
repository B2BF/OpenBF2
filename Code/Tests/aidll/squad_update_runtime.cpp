// [TEMP] Real manager definitions execute in actual.cpp. Engine interfaces and
// coordinator receivers are chosen native collaborators. Commander/human helper
// observers are explicit; their actual bodies are tested by the separate helper suite.
#include "squad_update_runtime.h"
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <limits>
#include <new>
#include <string>
#include <windows.h>
#include <crtdbg.h>
using namespace dice::hfe;
using namespace dice::hfe::ai;
static_assert(sizeof(void*)==4,"MSVC x86 fixture");
static_assert(sizeof(SquadManager::SideBlock)==72,"paired PE scalar SideBlock size");
static_assert(offsetof(SquadManager::SideBlock,m_artilleryStartTime)==8 &&
    offsetof(SquadManager::SideBlock,m_artilleryPosition)==16 &&
    offsetof(SquadManager::SideBlock,m_artilleryDelay)==32 &&
    offsetof(SquadManager::SideBlock,m_validFlee0)==40 &&
    offsetof(SquadManager::SideBlock,m_validFlee1)==41 &&
    offsetof(SquadManager::SideBlock,m_fleePos0)==44 &&
    offsetof(SquadManager::SideBlock,m_fleePos1)==56,"paired PE scalar SideBlock offsets");
[[noreturn]] static void unexpected(const char* name) { std::fprintf(stderr,"FAIL %s\n",name); std::fflush(stderr); std::abort(); }
static void require(bool condition,const char* name) { if(!condition) unexpected(name); }

// [TEMP] Explicit template-instantiation access exception yields typed native
// member pointers. It changes neither the real header nor the object layout.
template<class Tag,typename Tag::type Member> struct NativeAccess {
    friend typename Tag::type nativeMember(Tag) { return Member; }
};
using Rosters=std::vector<SquadCoordinator*>[2];
using PositionMap=std::map<int,double>;
using Messages=std::list<SquadManager::CommanderMessage*>;
#define MEMBER_ACCESS(Tag,Type,Member) struct Tag { using type=Type SquadManager::*; friend type nativeMember(Tag); }; template struct NativeAccess<Tag,&SquadManager::Member>;
MEMBER_ACCESS(RostersTag,Rosters,m_Squads)
MEMBER_ACCESS(SquadCursorTag,int,m_squadCursor)
MEMBER_ACCESS(SideCursorTag,int,m_sideCursor)
using Blocks=SquadManager::SideBlock[2];
MEMBER_ACCESS(BlocksTag,Blocks,m_artillery)
MEMBER_ACCESS(DurationTag,double,m_double156)
MEMBER_ACCESS(FloatTag,float,m_float152)
MEMBER_ACCESS(PositionsTag,PositionMap,m_playerValidPositions)
MEMBER_ACCESS(MessagesTag,Messages,m_commanderMessages)
#undef MEMBER_ACCESS
static SquadManager* tested;
static Rosters& sides() { return tested->*nativeMember(RostersTag{}); }
static int& squadCursor() { return tested->*nativeMember(SquadCursorTag{}); }
static int& sideCursor() { return tested->*nativeMember(SideCursorTag{}); }
static SquadManager::SideBlock& block(int side) { return (tested->*nativeMember(BlocksTag{}))[side?1:0]; }
static double& duration() { return tested->*nativeMember(DurationTag{}); }
static std::string mode,calls;
static int maximumSquads=1,maxQueries,pathCalls,humanCalls;
static double expectedTime=50.000000000000007;
static std::function<void(SquadCoordinator*)> onUpdate,onPath;
static std::function<void()> onMessages,onHumans;
static void trace(const std::string& text) { if(!calls.empty()) calls+=','; calls+=text; }
static void expect(const std::string& wanted) { if(calls!=wanted) { std::fprintf(stderr,"TRACE got=%s expected=%s\n",calls.c_str(),wanted.c_str()); unexpected("exact callback order"); } calls.clear(); }
static int gameSquads() { ++maxQueries; return maximumSquads; }
#include "mocks.inl"
namespace dice { namespace hfe { namespace ai {
void* CommonSingletonServer::m_iaiGame=nullptr;
SquadManager* SquadManager::m_instance=nullptr;
IAIGame::IAIGame() {} IAIGame::~IAIGame() {}
// [CHOSEN] Scoped collaborator bodies, not among the five extracted tested definitions.
void SquadManager::updateCommanderMessages() { trace("messages"); if(onMessages) onMessages(); }
void SquadManager::updateHumanPlayers(double time) {
    trace("humans"); ++humanCalls; require(squadCursor()==0,"human update sees reset cursor");
    require(std::memcmp(&time,&expectedTime,sizeof(time))==0,"unchanged double human time");
    if(onHumans) onHumans();
}
// Genuine native container lifetimes; production coordinator construction is not tested.
SquadCoordinator::SquadCoordinator(int side,int index):m_squadIndex(index),m_side(side),m_formation(nullptr) {}
void SquadCoordinator::update(double time) {
    require(std::memcmp(&time,&expectedTime,sizeof(time))==0,"unchanged double coordinator time");
    trace("u"+std::to_string(m_side)+":"+std::to_string(m_squadIndex)); if(onUpdate) onUpdate(this);
}
void SquadCoordinator::updateSquadPaths() {
    ++pathCalls; trace("p"+std::to_string(m_side)+":"+std::to_string(m_squadIndex)); if(onPath) onPath(this);
}
} } }
static void byteValue(const void* bytes,std::size_t size,unsigned char value,const char* why) {
    const unsigned char* p=static_cast<const unsigned char*>(bytes);
    for(std::size_t i=0;i<size;++i) require(p[i]==value,why);
}
int main(int argc,char** argv) {
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX); _set_abort_behavior(0,_WRITE_ABORT_MSG|_CALL_REPORTFAULT);
    _CrtSetReportMode(_CRT_ASSERT,_CRTDBG_MODE_FILE); _CrtSetReportFile(_CRT_ASSERT,_CRTDBG_FILE_STDERR);
    require(argc==2,"mode required"); mode=argv[1];
    ObservedGame game; CommonSingletonServer::m_iaiGame=&game;
    if(mode=="ctor_poison") {
        alignas(SquadManager) unsigned char storage[sizeof(SquadManager)]; std::memset(storage,0xa5,sizeof(storage));
        SquadManager* manager=new(storage) SquadManager; tested=manager;
        require(maxQueries==1 && SquadManager::m_instance==manager,"real constructor game query and publication");
        require(squadCursor()==0 && sideCursor()==1 && duration()==25.0 && manager->*nativeMember(FloatTag{})==40.0f,"real constructor scalar stores");
        require(sides()[0].empty() && sides()[1].empty() && (manager->*nativeMember(PositionsTag{})).empty() && (manager->*nativeMember(MessagesTag{})).empty(),"real native containers empty");
        for(int side=0;side<2;++side) {
            require(!block(side).incomingArtillery,"side active flag initialized");
            const Vec3 positions[]={block(side).m_artilleryPosition,block(side).m_fleePos0,block(side).m_fleePos1};
            for(const Vec3& p:positions) require(p.x==0 && p.y==0 && p.z==0,"three real Vec3 values zero initialized");
            byteValue(&block(side).m_artilleryStartTime,sizeof(double),0xa5,"timestamp representation remains unwritten");
            byteValue(&block(side).m_artilleryDelay,sizeof(double),0xa5,"delay representation remains unwritten");
            byteValue(&block(side).m_validFlee0,sizeof(bool),0xa5,"first valid flag representation remains unwritten");
            byteValue(&block(side).m_validFlee1,sizeof(bool),0xa5,"second valid flag representation remains unwritten");
        }
        manager->~SquadManager(); SquadManager::m_instance=nullptr;
        std::printf("PASS %s\n",mode.c_str()); return 0;
    }
    SquadManager manager; tested=&manager;
    if(mode=="ctor_firstwins") {
        SquadManager other; require(maxQueries==2 && SquadManager::m_instance==&manager,"first-wins publication");
        SquadManager::m_instance=nullptr; std::printf("PASS %s\n",mode.c_str()); return 0;
    }
    SquadCoordinator a(0,0),b(0,1),c(1,0),d(1,1),e(0,2);
    sides()[0]={&a,&b}; sides()[1]={&c,&d};
    block(0).m_artilleryStartTime=block(1).m_artilleryStartTime=10.0;
    const std::string pathPass="p0:0,p0:1,p1:0,p1:1";
    if(mode.compare(0,4,"art_")==0) {
        block(0).incomingArtillery=true;
        std::string wanted=pathPass; bool active0=false,active1=false;
        if(mode=="art_inactive") { block(0).incomingArtillery=false; wanted=""; }
        else if(mode=="art_equal" || mode=="art_future") { expectedTime=mode=="art_equal"?35.0:34.0; wanted=""; active0=true; }
        else if(mode=="art_first") {}
        else if(mode=="art_just_after") expectedTime=std::nextafter(35.0,36.0);
        else if(mode=="art_second") { block(0).incomingArtillery=false; block(1).incomingArtillery=true; }
        else if(mode=="art_both") { block(1).incomingArtillery=true; wanted+=","+pathPass; }
        else if(mode=="art_time_nan" || mode=="art_duration_nan" || mode=="art_start_nan") {
            if(mode=="art_time_nan") expectedTime=std::numeric_limits<double>::quiet_NaN();
            else if(mode=="art_duration_nan") duration()=std::numeric_limits<double>::quiet_NaN();
            else block(0).m_artilleryStartTime=std::numeric_limits<double>::quiet_NaN();
            wanted=""; active0=true;
        }
        else if(mode=="art_enable_second") {
            onPath=[&](SquadCoordinator*) { if(pathCalls==1) block(1).incomingArtillery=true; }; wanted+=","+pathPass;
        }
        else if(mode=="art_disable_second") {
            block(1).incomingArtillery=true; onPath=[&](SquadCoordinator*) { if(pathCalls==1) block(1).incomingArtillery=false; };
        }
        else if(mode=="art_duration_change") {
            block(1).incomingArtillery=true; active1=true; onPath=[&](SquadCoordinator*) { if(pathCalls==1) duration()=100; };
        }
        else if(mode=="art_grow") {
            onPath=[&](SquadCoordinator*) { if(pathCalls==1) { sides()[0].reserve(sides()[0].capacity()+7); sides()[0].push_back(&e); } };
            wanted="p0:0,p0:1,p0:2,p1:0,p1:1";
        }
        else if(mode=="art_shrink") { onPath=[&](SquadCoordinator*) { if(pathCalls==1) sides()[0].clear(); }; wanted="p0:0,p1:0,p1:1"; }
        else if(mode=="art_replace") {
            onPath=[&](SquadCoordinator*) { if(pathCalls==1) { sides()[0].reserve(sides()[0].capacity()+7); sides()[0][1]=&e; } };
            wanted="p0:0,p0:2,p1:0,p1:1";
        }
        else if(mode=="art_rearm_first") { onPath=[&](SquadCoordinator*) { if(pathCalls==1) block(0).incomingArtillery=true; }; active0=true; }
        else unexpected("unknown artillery mode");
        const auto mutation=onPath;
        onPath=[&](SquadCoordinator* coordinator) {
            if(mode=="art_both") require(block(0).incomingArtillery==false && block(1).incomingArtillery==(pathCalls<=4),"side clear precedes each full traversal");
            else if(pathCalls==1) require(mode=="art_second"?!block(1).incomingArtillery:!block(0).incomingArtillery,"expired flag cleared before callback");
            if(mutation) mutation(coordinator);
        };
        manager.updateArtilleryStatus(expectedTime); expect(wanted);
        require(block(0).incomingArtillery==active0 && block(1).incomingArtillery==active1,"final artillery flags");
    } else {
        std::string wanted="messages,u0:0"; int wantedSide=0,wantedCursor=0;
        if(mode=="tick_cycle") {
            manager.update(expectedTime); expect("messages,u0:0"); require(sideCursor()==0 && squadCursor()==0,"first side");
            manager.update(expectedTime); expect("messages,u1:0"); require(sideCursor()==1 && squadCursor()==1,"advance after second side");
            manager.update(expectedTime); expect("messages,u0:1"); require(sideCursor()==0 && squadCursor()==1,"next index first side");
            wanted="messages,u1:1,humans"; wantedSide=1;
        }
        else if(mode=="tick_side_nonzero") sideCursor()=-27;
        else if(mode=="tick_callback_side0") { sideCursor()=0; wanted="messages,u1:0"; onUpdate=[](SquadCoordinator*) { sideCursor()=0; }; }
        else if(mode=="tick_callback_side1") { wantedSide=1; wantedCursor=1; onUpdate=[](SquadCoordinator*) { sideCursor()=1; }; }
        else if(mode=="tick_callback_side2") { wantedSide=2; onUpdate=[](SquadCoordinator*) { sideCursor()=2; }; }
        else if(mode=="tick_callback_cursor" || mode=="tick_high_cursor") {
            sideCursor()=0; wantedSide=1; wanted="messages,u1:0,humans";
            onUpdate=[](SquadCoordinator*) { squadCursor()=mode=="tick_high_cursor"?-1:1; };
        }
        else if(mode=="tick_size_grow") {
            sideCursor()=0; squadCursor()=1; wanted="messages,u1:1"; wantedSide=1; wantedCursor=2;
            onUpdate=[&](SquadCoordinator*) { sides()[0].push_back(&e); };
        }
        else if(mode=="tick_size_shrink") {
            sideCursor()=0; wanted="messages,u1:0,humans"; wantedSide=1;
            onUpdate=[](SquadCoordinator*) { sides()[0].resize(1); };
        }
        else if(mode=="tick_zero_size") {
            sideCursor()=0; wanted="messages,u1:0"; wantedSide=1; wantedCursor=4;
            onUpdate=[](SquadCoordinator*) { sides()[0].clear(); squadCursor()=3; };
        }
        else if(mode=="tick_artillery_order") { block(0).incomingArtillery=true; wanted=pathPass+",messages,u0:0"; }
        else if(mode=="tick_message_mutation") {
            wanted="messages,u0:2,humans"; wantedSide=1;
            onMessages=[&]() { sideCursor()=0; squadCursor()=1; sides()[1].reserve(sides()[1].capacity()+7); sides()[1][1]=&e; };
        }
        else if(mode=="tick_human_mutation") {
            sideCursor()=0; squadCursor()=1; wanted="messages,u1:1,humans"; wantedSide=2; wantedCursor=7;
            onHumans=[]() { sideCursor()=2; squadCursor()=7; };
        }
        else unexpected("unknown tick mode");
        manager.update(expectedTime); expect(wanted);
        require(sideCursor()==wantedSide && squadCursor()==wantedCursor,"post-callback cursor state");
    }
    SquadManager::m_instance=nullptr;
    std::printf("PASS %s\n",mode.c_str());
}
