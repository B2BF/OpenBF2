// [TEMP] Exact tested bodies live in actual.cpp. Native chosen lifetimes only.
// LEAF_OBSERVERS replaces named leaves/reset solely for callback-order tests;
// SCHEDULER_OBSERVERS replaces the scheduler's helpers solely to test the real scheduler.
#include "formation_tick_runtime.h"
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <csignal>
#include <functional>
#include <limits>
#include <string>
#include <windows.h>
#include <crtdbg.h>
using namespace dice::hfe;
using namespace dice::hfe::ai;
static_assert(sizeof(void*)==4,"strict x86");
static unsigned checks;
static std::string mode,calls;
[[noreturn]] static void unexpected(const char* s)
{ std::fprintf(stderr,"FAIL %s\n",s);std::fflush(stderr);std::exit(2); }
static void require(bool b,const char* s){++checks;if(!b)unexpected(s);}
static std::function<void()> abortCheck;
static std::function<void(const std::string&,SquadMember*,SquadCoordinator*)> hook;
static void event(const std::string& s,SquadMember* m=nullptr,SquadCoordinator* c=nullptr)
{if(!calls.empty())calls+=',';calls+=s;if(hook)hook(s,m,c);}
static void onAbort(int)
{require(bool(abortCheck),"unexpected abort");abortCheck();std::printf("VERIFIED %s\n",mode.c_str());std::fflush(stdout);}
static unsigned clockCalls;
static SquadFormation* tested;
static double originalTime=123.00000000000003;
static bool equal(const Vec3& a,const Vec3& b)
{return a.x==b.x&&a.y==b.y&&a.z==b.z;}
static bool sameTarget(const std::pair<bool,Vec3>& a,const std::pair<bool,Vec3>& b)
{return a.first==b.first&&equal(a.second,b.second);}
static float fromBits(std::uint32_t bits){float value;std::memcpy(&value,&bits,4);return value;}
static std::uint32_t toBits(float value){std::uint32_t bits;std::memcpy(&bits,&value,4);return bits;}
static Vec3 vecBits(std::uint32_t x,std::uint32_t y,std::uint32_t z){return Vec3{fromBits(x),fromBits(y),fromBits(z)};}
static void sameSlot(const SquadFormation::Slot& a,const SquadFormation::Slot& b)
{
 require(a.hasSquadMemberAssigned==b.hasSquadMemberAssigned,"occupied");require(a.m_memberIndex==b.m_memberIndex,"member index");
 require(equal(a.m_offset,b.m_offset),"offset unchanged");require(a.m_i20==b.m_i20,"trailing index");
 require(a.m_b24==b.m_b24,"flag24");require(a.m_i28==b.m_i28,"float28");
 require(a.m_referenceSlot==b.m_referenceSlot,"reference slot unchanged");
}
// [TEMP] The forward-vector owner query's only typed predecessor.
static unsigned vehicleCalls;
static IAIPlayer* vehiclePlayer;
static IAIObject* observeVehicle(IAIPlayer* player)
{++vehicleCalls;vehiclePlayer=player;event("vehicle");return reinterpret_cast<IAIObject*>(0x1234);}
class ObservedTimer:public IBFTimer {
public:
 double getGameTime() const override {++clockCalls;require(!tested->m_b56,"flag clears before clock");return 20.125;}
 double getTimeIncrease() const override {unexpected("time increase");}
 double getUpdateTime() const override {unexpected("update clock");}
 double getWallClockTime() const override {unexpected("wall clock");}
 void setPause(bool) override {unexpected("pause");}
 bool getPause() override {unexpected("pause query");}
 void reset() override {unexpected("timer reset");}
};
namespace dice { namespace hfe { namespace ai {
// [CHOSEN, not recovered] Native containers/scalars have proper lifetimes; no publication.
void* CommonSingletonServer::m_iBFTimer=nullptr;
IBFTimer::IBFTimer(){} IBFTimer::~IBFTimer(){}
IAIPlayer::~IAIPlayer(){}
SquadFormation::SquadFormation(SquadCoordinator* c)
 :m_coordinator(c),m_formation(3),m_numberOfSlots(6),m_b36(false),m_formationMaxSpeed(17),
  m_side(0),m_i48(77),m_f52(2),m_b56(true),m_b84(false),m_b85(false),m_i88(88),m_d92(30)
{m_direction={1,2,3};m_v60={4,5,6};m_v72={7,8,9};m_trailingPosition={10,11,12};}
SquadCoordinator::SquadCoordinator(int side,int index)
 :m_squadIndex(index),m_side(side),m_formation(nullptr),m_squadLeaderIndex(4)
{m_v108={};}
SquadMember::SquadMember(SquadCoordinator* c,IAIPlayer* p,int squad,int side,int index)
 :m_squadIndex(squad),m_memberIndex(index),m_player(p),m_bot(nullptr),m_squad(c),m_side(side),
 m_havePath(true),m_request(nullptr),m_positionValid(true),m_flag4A(false)
{m_position={};}
SquadMember::~SquadMember(){}
#if defined(LEAF_OBSERVERS)
SquadMember* SquadCoordinator::getSquadLeader(){event("leader",nullptr,this);return m_squadMembers[m_squadLeaderIndex];}
void SquadCoordinator::getSquadPosition(Vec3& p){event("squadpos",nullptr,this);p=m_v108;}
SquadMember* SquadCoordinator::getSquadMember(int i){event("member"+std::to_string(i),nullptr,this);return m_squadMembers[i];}
bool SquadCoordinator::squadHasPosition(){event("valid",nullptr,this);return false;}
int SquadMember::getMemberIndex(){event("index",this);return m_memberIndex;}
bool SquadMember::isBot(){event("bot",this);return m_bot!=nullptr;}
bool SquadMember::getPosition(Vec3& p){event("position",this);p=m_position;return m_positionValid;}
void SquadMember::setHavePath(bool b){require(!b,"only false path clear");m_havePath=b;m_flag4A=!b;event("path",this);}
void SquadFormation::resetTrailingPositions(){event("reset");}
#endif
#if defined(SCHEDULER_OBSERVERS)
void SquadFormation::updateSlotZeroPos(){require(this==tested,"same scheduler receiver");event("zero");}
void SquadFormation::updateMovementFlag(bool b){require(!b&&this==tested,"false movement argument");event("movement");}
void SquadFormation::updateFormationMaxSpeed(){require(this==tested,"speed receiver");event("speed");}
void SquadFormation::updateFormation(){require(this==tested,"formation receiver");event("formation");}
void SquadFormation::updateWaitForSquadMembers(double t){require(this==tested&&std::memcmp(&t,&originalTime,8)==0,"original exact double");event("wait");}
void SquadFormation::updateFormationForwardVector(){require(this==tested,"forward receiver");event("forward");}
void SquadFormation::updateTrailingPositions(){require(this==tested,"trailing receiver");event("trailpos");}
void SquadFormation::updateTrailingSlots(){require(this==tested,"slots receiver");event("trailslots");}
void SquadFormation::selectFormation(){require(this==tested,"select receiver");event("select:"+std::to_string(m_i88));}
void SquadFormation::setFormation(){require(this==tested,"set receiver");event("set:"+std::to_string(m_i88));}
#endif
} } }
#include "mocks.inl"

struct State {
 SquadCoordinator squad,other;
 SquadFormation f;
 SquadMember members[6];
 SquadFormation::Slot slots[6],expected[6],spare;
 bool path[6];bool reset=false;bool expected85=false;
 int expectedCycle=88,expectedFormation=3;
 State():squad(0,0),other(0,1),f(&squad),members{
  {&squad,nullptr,0,0,0},{&squad,nullptr,0,0,1},{&squad,nullptr,0,0,2},
  {&squad,nullptr,0,0,3},{&squad,nullptr,0,0,4},{&squad,nullptr,0,0,5}}
 {
  // Opaque identity used only by actual isBot's null comparison; never dereferenced.
  static int identity;
  for(int i=0;i<6;++i){
   members[i].m_bot=reinterpret_cast<Bot*>(&identity);squad.m_squadMembers.push_back(&members[i]);
   other.m_squadMembers.push_back(&members[i]);slots[i].m_memberIndex=i;slots[i].m_i20=100+i;
   slots[i].m_b24=true;slots[i].m_i28=20.25f+i;slots[i].m_referenceSlot=200+i;
   slots[i].m_offset={30.0f+i,40.0f+i,50.0f+i};
   f.m_slots.push_back(&slots[i]);f.m_trailingPositions.push_back({true,{60.0f+i,70.0f+i,80.0f+i}});path[i]=true;
  }
  squad.m_formation=&f;tested=&f;
 }
 void snapshot(){for(int i=0;i<6;++i)expected[i]=slots[i];expected85=f.m_b85;}
 void payload(int i,int member){expected[i].m_memberIndex=member;expected[i].m_b24=false;expected[i].m_i28=0;}
 void expectReset(){reset=true;for(auto& s:expected)s.m_i20=-1;}
 void verify(){
  for(int i=0;i<6;++i){sameSlot(slots[i],expected[i]);require(members[i].m_havePath==path[i],"path state");
   require(members[i].m_flag4A==!path[i],"inverse path state");
   require(f.m_trailingPositions[i].first==!reset,"target valid state");
   require(equal(f.m_trailingPositions[i].second,{60.0f+i,70.0f+i,80.0f+i}),"target coordinates retained");}
  require(equal(f.m_trailingPosition,reset?f.m_v72:Vec3{10,11,12}),"trailing position copy");
  require(f.m_b85==expected85,"only promotion clears flag85");
  require(f.m_i88==expectedCycle&&f.m_formation==expectedFormation,"scheduler suffix state");
 }
 // Expected m_i20 per slot after a trailing-slot re-deal; everything else must be untouched.
 void verifyTrailIndices(const int (&indices)[6]){
  for(int i=0;i<6;++i){SquadFormation::Slot want=expected[i];want.m_i20=indices[i];sameSlot(slots[i],want);}
  for(int i=0;i<6;++i)require(f.m_trailingPositions[i].first&&equal(f.m_trailingPositions[i].second,{60.0f+i,70.0f+i,80.0f+i}),"trail slots leave crumbs alone");
  require(equal(f.m_trailingPosition,{10,11,12}),"trail slots leave the newest crumb alone");
 }
};

int main(int argc,char** argv)
{
 SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);_set_abort_behavior(0,_WRITE_ABORT_MSG|_CALL_REPORTFAULT);
 std::signal(SIGABRT,onAbort);if(argc!=2)unexpected("mode required");mode=argv[1];
 ObservedTimer timer;CommonSingletonServer::m_iBFTimer=&timer;State s;auto& f=s.f;
#if defined(SCHEDULER_OBSERVERS)
 // The real scheduler: eight ordered helpers, then the counter read AFTER them.
 std::string suffix;
 if(mode=="scheduler_order"){f.m_i88=88;suffix=",set:0";}
 else if(mode=="scheduler_select"){f.m_i88=2;suffix=",select:3";}
 else if(mode=="scheduler_plain"){f.m_i88=0;suffix="";}
 else if(mode=="scheduler_mutation"){
  f.m_i88=88;suffix=",select:3";
  // A helper's store before the counter read is what the counter sees.
  hook=[&](const std::string& name,SquadMember*,SquadCoordinator*){if(name=="trailslots")f.m_i88=2;};
 }else unexpected("scheduler mode");
 f.update(originalTime);
 require(calls=="zero,movement,speed,formation,wait,forward,trailpos,trailslots"+suffix,"eight ordered helpers then the cycle");
 require(f.m_i88==(mode=="scheduler_order"?0:mode=="scheduler_plain"?1:3),"signed cycle counter result");
 require(f.m_formation==3,"observed select/set do not store");
#elif defined(LEAF_OBSERVERS)
 std::string expectedTrace="leader,index,bot,squadpos";int paths=0;
 if(mode=="entry_capture"){
  f.m_numberOfSlots=0;
  hook=[&](const std::string& n,SquadMember* m,SquadCoordinator* c){
   if(n=="index"){require(m==&s.members[4],"entry leader retained");s.squad.m_squadMembers[4]=&s.members[5];}
   if(n=="bot"){require(m==&s.members[4],"same leader after roster replacement");f.m_coordinator=&s.other;}
   if(n=="squadpos")require(c==&s.other,"fresh coordinator for position");};
 }else if(mode.rfind("near_",0)==0){
  s.slots[0].hasSquadMemberAssigned=true;s.slots[2].hasSquadMemberAssigned=true;s.slots[2].m_memberIndex=4;
  s.members[4].m_position={1,0,0};
  if(mode=="near_flag")s.members[4].m_position={100,0,0};
  expectedTrace+=",position,member4,path,member0,path,reset";
  hook=[&](const std::string& n,SquadMember* m,SquadCoordinator* c){
   if(n=="position"){
    require(m==&s.members[4],"retained position receiver");
    if(mode=="near_index")s.members[4].m_memberIndex=5;
    if(mode=="near_flag")f.m_b85=true;
   }
   if(n=="member4")require(s.slots[0].m_memberIndex==4&&!f.m_b85,"cached index and flag store before lookup");
   if(n=="path"&&++paths==1){
    require(m==&s.members[4]&&s.slots[2].m_memberIndex==4,"first path before old slot write");
    if(mode=="near_reload"){s.spare=s.slots[2];std::vector<SquadFormation::Slot*> v=f.m_slots;v[2]=&s.spare;f.m_slots.swap(v);}
    if(mode=="near_coordinator")f.m_coordinator=&s.other;
   }
   if(n=="member0"&&mode=="near_coordinator")require(c==&s.other,"fresh second coordinator");
   if(n=="reset")require(paths==2,"reset after two path clears");
  };
 }else if(mode=="hidden_pointers"||mode=="hidden_indices"){
  s.slots[0].hasSquadMemberAssigned=s.slots[5].hasSquadMemberAssigned=true;s.slots[0].m_memberIndex=4;f.m_b84=true;
  expectedTrace+=mode=="hidden_indices"?",reset,member3,path,member2,path":",reset,member5,path,member4,path";
  hook=[&](const std::string& n,SquadMember*,SquadCoordinator*){
   if(n=="reset"){
    require(s.members[4].m_havePath&&s.members[5].m_havePath,"reset precedes paths");
    require(s.slots[0].m_memberIndex==5&&s.slots[5].m_memberIndex==4,"swaps precede reset");
    if(mode=="hidden_pointers"){s.spare=s.slots[0];s.spare.m_memberIndex=1;f.m_slots[0]=&s.spare;}
    else{s.slots[0].m_memberIndex=3;s.slots[5].m_memberIndex=2;}
   }
  };
 }else if(mode=="match_live"||mode=="match_retained"||mode=="match_clear_order"){
  s.slots[0].hasSquadMemberAssigned=s.slots[1].hasSquadMemberAssigned=true;s.members[0].m_bot=nullptr;
  if(mode=="match_live"){s.members[1].m_bot=nullptr;f.m_numberOfSlots=2;}
  expectedTrace+=mode=="match_live"?",member0,bot,member1,bot,member2,bot,member2,path,reset":",member0,bot,member1,bot,member1,path,reset";
  hook=[&](const std::string& n,SquadMember* m,SquadCoordinator*){
   if(n=="bot"&&m==&s.members[1]){
    if(mode=="match_live"){f.m_numberOfSlots=3;s.slots[2].hasSquadMemberAssigned=true;f.m_slots.reserve(64);}
    if(mode=="match_retained"){s.spare=s.slots[1];s.spare.m_memberIndex=3;f.m_slots[1]=&s.spare;}
   }
   if(n=="path"){++paths;require(!s.members[0].m_flag4A,"only new zero path clears");}
   if(n=="reset")require(paths==1,"path precedes reset");
  };
 }else unexpected("leaf observer mode");
 f.updateFormation();require(calls==expectedTrace,"exact leaf callback trace");
 if(mode=="near_reload")require(s.spare.m_memberIndex==0&&s.slots[2].m_memberIndex==4,"old slot refetched after path callback");
 if(mode=="hidden_pointers")require(s.spare.m_memberIndex==1&&!s.members[4].m_havePath&&!s.members[5].m_havePath,"retained two slot pointers");
 if(mode=="match_retained")require(s.slots[1].m_memberIndex==0&&s.spare.m_memberIndex==3&&s.slots[0].m_memberIndex==1,"captured candidate survives vector replacement");
 require(f.m_i88==88&&f.m_formation==3,"no scheduling suffix");
#else
 if(mode.rfind("range_",0)==0){
  Vec3 a={},b={};bool expected=true;
  if(mode=="range_equal"){a.x=50;expected=false;}
  else if(mode=="range_nan"){a.x=std::numeric_limits<float>::quiet_NaN();expected=false;}
  else if(mode=="range_sum"){a={49.999996185302734375f,0.017f,0};}
  else if(mode=="range_stack"){
   for(int i=0;i<2048;++i){
    require(fixtureFormationRange({1,2,3},{}),"repeated true comparison");
    require(!fixtureFormationRange({50,0,0},{}),"repeated equal comparison");
    require(!fixtureFormationRange({std::numeric_limits<float>::quiet_NaN(),0,0},{}),"repeated unordered comparison");
   }
  }
  else {const int axis=mode=="range_x"?0:mode=="range_y"?1:2;
   if(axis==0){a.x=50;b.x=std::ldexp(1.0f,-22);}if(axis==1){a.y=50;b.y=std::ldexp(1.0f,-22);}if(axis==2){a.z=50;b.z=std::ldexp(1.0f,-22);}}
  require(fixtureFormationRange(a,b)==expected,"wide delta and ordered strict range");
 }else if(mode.rfind("wait_",0)==0){
  if(mode=="wait_zero")f.m_numberOfSlots=0;
  if(mode=="wait_skip_zero")s.slots[0].hasSquadMemberAssigned=true;
  if(mode=="wait_occupied_stop")s.slots[3].hasSquadMemberAssigned=true;
  s.snapshot();abortCheck=[&]{require(f.m_b36,"wait flag written before occupied frontier");s.verify();require(clockCalls==0,"no timer before frontier");};
  f.updateWaitForSquadMembers(originalTime);require(f.m_b36,"wait flag true");s.verify();require(clockCalls==0,"empty wait skips timing");
 }else if(mode=="scheduler_slot_stop"||mode=="scheduler_speed_stop"){
  if(mode=="scheduler_slot_stop"){s.slots[0].hasSquadMemberAssigned=true;s.squad.m_v108={1,0,0};}
  if(mode=="scheduler_speed_stop")s.slots[0].hasSquadMemberAssigned=true;
  s.snapshot();abortCheck=[&]{
   require(f.m_i88==88&&f.m_formation==3,"suffix counter/selection untouched");
   if(mode=="scheduler_slot_stop")require(!f.m_b36&&f.m_b56&&f.m_formationMaxSpeed==17&&clockCalls==0,"first nested stop prevents following helpers");
   if(mode=="scheduler_speed_stop")require(!f.m_b36&&!f.m_b56&&f.m_formationMaxSpeed==(std::numeric_limits<float>::max)()&&clockCalls==1,"movement then speed seed before stop");};
  f.update(originalTime);unexpected("scheduler should stop");
 }else if(mode=="scheduler_unoccupied"){
  // The whole tick on the path the live coop probe takes: every helper returns.
  f.m_i88=0;s.snapshot();s.expectedCycle=1;
  f.update(originalTime);
  require(f.m_b36&&f.m_formationMaxSpeed==5&&f.m_b56,"actual unoccupied prefix");
  require(clockCalls==0&&vehicleCalls==0,"no timer or owner query on an unoccupied tick");
  require(f.m_i48==77&&f.m_f52==2,"forward vector untouched");require(equal(f.m_direction,{1,2,3}),"no re-lay");
  s.verify();
 }else if(mode.rfind("cycle_",0)==0){
  s.snapshot();
  if(mode=="cycle_plain"){f.m_i88=4;s.expectedCycle=5;}
  else if(mode=="cycle_select"){f.m_i88=2;s.expectedCycle=3;s.expectedFormation=0;}
  else if(mode=="cycle_negative"){f.m_i88=-123;s.expectedCycle=-122;}
  else if(mode=="cycle_wrap_line"){f.m_i88=5;f.m_formation=0;s.expectedCycle=0;s.expectedFormation=0;
   const float offsets[6]={0,1,-1,1,-1,1};const int references[6]={200,0,0,1,2,3};
   for(int i=0;i<6;++i){s.expected[i].m_offset={offsets[i],0,0};s.expected[i].m_referenceSlot=references[i];}}
  else if(mode=="cycle_wedge_stop"||mode=="cycle_echelon_stop"){
   f.m_i88=5;f.m_formation=mode=="cycle_wedge_stop"?1:3;
   abortCheck=[&]{
    require(f.m_i88==0,"counter wraps to zero before the re-lay");
    require(f.m_formation==(mode=="cycle_wedge_stop"?1:3),"shape selection untouched by the wrap");
    if(mode=="cycle_wedge_stop")require(equal(f.m_direction,{0.80000001f,0,-0.60000002f}),"wedge direction before its stop");
    else require(equal(f.m_direction,{0.60000002f,0,0.80000001f}),"echelon B direction before its stop");
    for(int i=0;i<6;++i)sameSlot(s.slots[i],s.expected[i]);};
   f.update(originalTime);unexpected("wedge/echelon re-lay should stop");
  }else unexpected("cycle mode");
  f.update(originalTime);
  if(mode=="cycle_wrap_line")require(equal(f.m_direction,{1,0,0}),"line re-lay writes its direction");
  else require(equal(f.m_direction,{1,2,3}),"no re-lay off the wrap");
  s.verify();
 }else if(mode.rfind("forward_",0)==0){
  ObservedPlayer player;
  if(mode=="forward_unoccupied"){
   s.members[0].m_player=&player;f.updateFormationForwardVector();
   require(vehicleCalls==0&&calls.empty(),"empty anchor returns before any member or owner call");
   require(f.m_i48==77&&f.m_f52==2,"forward vector untouched");
  }else{
   s.slots[0].hasSquadMemberAssigned=true;s.slots[0].m_memberIndex=2;s.members[2].m_player=&player;
   abortCheck=[&]{require(vehicleCalls==1&&vehiclePlayer==&player,"anchor member's player gives its current vehicle once");
    require(f.m_i48==77&&f.m_f52==2,"no forward write before the owner query");};
   f.updateFormationForwardVector();unexpected("occupied forward vector should stop");
  }
 }else if(mode.rfind("trail_",0)==0){
  bool shifted=false;bool seeded=false;
  if(mode=="trail_unset_anchor")f.m_v72={0,0,0};
  else if(mode=="trail_nan_anchor")f.m_v72={std::numeric_limits<float>::quiet_NaN(),0,0};
  else if(mode=="trail_seed"){f.m_trailingPosition={0,0,0};seeded=true;}
  else if(mode=="trail_unoccupied"){}
  else if(mode=="trail_equal"){s.slots[0].hasSquadMemberAssigned=true;f.m_trailingPosition={13,8,9};}
  else if(mode=="trail_far"||mode=="trail_one_slot"){
   s.slots[0].hasSquadMemberAssigned=true;f.m_trailingPosition={13.5f,8,9};shifted=true;
   f.m_trailingPositions[2].first=false;f.m_trailingPositions[4].first=false;
   if(mode=="trail_one_slot")f.m_numberOfSlots=1;
  }else unexpected("trail mode");
  s.snapshot();const auto oldTargets=f.m_trailingPositions;const Vec3 before=f.m_trailingPosition;
  f.updateTrailingPositions();
  for(int i=0;i<6;++i)sameSlot(s.slots[i],s.expected[i]);
  require(clockCalls==0,"no clock in the breadcrumb pass");
  if(seeded){require(equal(f.m_trailingPosition,f.m_v72),"unset crumb seeded from the anchor");
   for(int i=0;i<6;++i)require(sameTarget(f.m_trailingPositions[i],oldTargets[i]),"seed leaves the history");}
  else if(shifted){
   const int moved=mode=="trail_one_slot"?0:5;
   for(int i=1;i<6;++i)require(sameTarget(f.m_trailingPositions[i],i<=moved?oldTargets[i-1]:oldTargets[i]),"history shifts back one place");
   require(f.m_trailingPositions[0].first&&equal(f.m_trailingPositions[0].second,before),"old crumb pushed at the front");
   require(equal(f.m_trailingPosition,f.m_v72),"anchor becomes the newest crumb");
  }else{
   require(equal(f.m_trailingPosition,before),"crumb unchanged");
   for(int i=0;i<6;++i)require(sameTarget(f.m_trailingPositions[i],oldTargets[i]),"history unchanged");
  }
 }else if(mode.rfind("slots_",0)==0||mode=="witness_member_wide"){
  f.m_v72={0,0,0};
  auto occupy=[&](int slot,float x,float y){s.slots[slot].hasSquadMemberAssigned=true;s.members[s.slots[slot].m_memberIndex].m_position={x,y,0};};
  int want[6]={100,101,102,103,104,105};
  if(mode=="slots_empty"){}
  else if(mode=="slots_unclaimed"){s.slots[1].hasSquadMemberAssigned=true;s.slots[1].m_i20=-1;s.slots[2].m_i20=5;want[1]=-1;want[2]=5;}
  else{
   for(int& w:want)w=-1;
   if(mode=="slots_sorted"){occupy(1,3,0);occupy(2,1,0);occupy(3,2,0);want[2]=0;want[3]=1;want[1]=2;}
   else if(mode=="slots_tie"){occupy(1,2,0);occupy(2,1,0);occupy(3,0,2);want[2]=0;want[1]=1;want[3]=2;}
   else if(mode=="slots_zero_member"){occupy(0,1,0);occupy(2,2,0);want[5]=0;want[2]=1;}
   else if(mode=="slots_single"){f.m_numberOfSlots=1;occupy(0,1,0);for(int i=1;i<6;++i)want[i]=100+i;want[0]=0;}
   else if(mode=="slots_invalid_position"){occupy(1,5,0);s.members[1].m_positionValid=false;occupy(2,1,0);want[2]=0;want[1]=1;}
   else if(mode=="witness_member_wide"){
    // PE keys 0x3F7FFFB1 (p) > 0x3F7FFFB0 (q): q first, although wide deltas would order them the other way.
    f.m_v72=vecBits(855638016u,0,0);
    s.slots[1].hasSquadMemberAssigned=s.slots[2].hasSquadMemberAssigned=true;
    s.members[1].m_position=vecBits(1065353176u,964689920u,0);s.members[2].m_position=vecBits(1065353177u,0,0);
    want[2]=0;want[1]=1;
   }else unexpected("slots mode");
  }
  s.snapshot();
  f.updateTrailingSlots();
  s.verifyTrailIndices(want);
  require(clockCalls==0,"no clock in the trail re-deal");
 }else if(mode=="accessors"){
  s.slots[4].m_memberIndex=-1;
  require(f.getSquadMemberIndexAtSlot(3)==3&&f.getSquadMemberIndexAtSlot(4)==-1,"slot index read with no occupied test");
  f.m_b84=true;require(f.shouldLeaveSquadLeader(),"leave flag read");f.m_b84=false;require(!f.shouldLeaveSquadLeader(),"leave flag read false");
  f.selectFormation();require(f.m_formation==0,"select stores line");
 }else if(mode=="join_leader"){
  f.m_b84=true;f.m_b85=false;f.joinSquadLeader();
  require(f.m_b85&&!f.m_b84&&f.m_b56&&clockCalls==0,"unoccupied anchor: flags only, movement flag untouched");
  s.slots[0].hasSquadMemberAssigned=true;f.m_b84=true;f.m_b85=false;f.joinSquadLeader();
  require(f.m_b85&&!f.m_b84&&f.m_b56&&clockCalls==1,"forced movement stamp");
  require(f.m_d92==20.125+0.2&&equal(f.m_v60,f.m_v72),"stamp time and anchor copy");
 }else if(mode=="witness_trail_set"){
  // PE sums the squares wide: set. Float-rounded squares would round the total down to 0.001f.
  const Vec3 v=vecBits(1023510049u,962592768u,0);
  volatile float x=v.x,y=v.y,z=v.z;volatile float naive=(z*z+y*y)+x*x;
  require(fixtureTrailPointIsSet(v),"PE wide sum is above 0.001f");require(!(naive>0.001f),"witness separates from float sums");
  require(!fixtureTrailPointIsSet({0,0,0})&&!fixtureTrailPointIsSet({std::numeric_limits<float>::quiet_NaN(),0,0}),"zero and NaN are unset");
 }else if(mode=="witness_crumb"){
  // PE rounds the difference to float first: exactly 36.0f, so no crumb is dropped. Wide: above.
  const Vec3 a=vecBits(1086324733u,0,0),c=vecBits(3051356160u,0,0);
  require(toBits(fixtureTrailCrumbDistanceSq(a,c))==1108344832u,"PE key is exactly 36.0f");
  const double wide=static_cast<double>(a.x)-c.x;require(static_cast<float>(wide*wide)>36.0f,"witness separates from wide deltas");
  require(fixtureTrailCrumbDistanceSq({7,8,9},{13.5f,8,9})==42.25f,"plain squared distance");
 }else if(mode=="witness_member_allspill"){
  const Vec3 p=vecBits(0,1065353176u,964689920u),q=vecBits(0,1065353177u,0),a=vecBits(0,855638016u,0);
  require(toBits(fixtureTrailMemberDistanceSq(p,a))==1065353136u&&toBits(fixtureTrailMemberDistanceSq(q,a))==1065353137u,
          "PE spills only dx: these keys order p before q");
 }else{
  bool promotion=false,hidden=false,matching=false,change=false,nested=false;
  if(mode=="unoccupied"){}
  else if(mode=="zero_count_storage")f.m_numberOfSlots=0;
  else if(mode=="leader_zero"){s.slots[0].hasSquadMemberAssigned=true;s.slots[0].m_memberIndex=4;}
  else if(mode.rfind("hidden_",0)==0){
   hidden=true;f.m_b84=true;f.m_b85=true;s.slots[0].hasSquadMemberAssigned=true;s.slots[0].m_memberIndex=4;
   if(mode=="hidden_nonzero"){s.slots[0].m_memberIndex=0;s.slots[2].hasSquadMemberAssigned=true;s.slots[2].m_memberIndex=4;}
   else if(mode!="hidden_only"){
    s.slots[1].hasSquadMemberAssigned=true;s.slots[5].hasSquadMemberAssigned=true;change=true;
    if(mode=="hidden_holes"){s.slots[5].hasSquadMemberAssigned=false;s.slots[3].hasSquadMemberAssigned=true;}
    if(mode=="hidden_duplicate")s.slots[5].m_memberIndex=4;
    if(mode=="hidden_reset_stop"){s.squad.m_v108={1,0,0};nested=true;}
   }
  }else if(mode.rfind("match_",0)==0){
   matching=true;s.slots[0].hasSquadMemberAssigned=true;s.members[0].m_bot=nullptr;s.slots[1].hasSquadMemberAssigned=true;change=true;f.m_b85=true;
   if(mode=="match_human"){s.members[4].m_bot=nullptr;s.members[0].m_bot=s.members[2].m_bot;s.members[1].m_bot=nullptr;}
   if(mode=="match_same"){s.members[0].m_bot=s.members[1].m_bot;change=false;}
   if(mode=="match_none"){s.members[1].m_bot=nullptr;change=false;}
   if(mode=="match_skip"){s.slots[1].hasSquadMemberAssigned=false;s.slots[3].hasSquadMemberAssigned=true;}
   if(mode=="match_first")s.slots[3].hasSquadMemberAssigned=true;
   if(mode=="match_reset_stop"){s.squad.m_v108={1,0,0};nested=true;}
  }else{
   promotion=true;s.slots[0].hasSquadMemberAssigned=true;s.slots[2].hasSquadMemberAssigned=true;s.slots[2].m_memberIndex=4;
   s.members[4].m_position={1,0,0};change=true;
   if(mode=="equal"||mode=="force_equal"){s.members[4].m_position.x=50;change=false;}
   if(mode=="far"){s.members[4].m_position.x=51;change=false;}
   if(mode=="nan"||mode=="force_nan"){s.members[4].m_position.x=std::numeric_limits<float>::quiet_NaN();change=false;}
   if(mode=="force_equal"||mode=="force_nan"){f.m_b85=true;change=true;}
   if(mode=="invalid_position")s.members[4].m_positionValid=false;
   if(mode=="empty_anchor")s.slots[0].hasSquadMemberAssigned=false;
   if(mode=="duplicate_leader"){s.slots[1].hasSquadMemberAssigned=true;s.slots[1].m_memberIndex=4;}
   if(mode=="near_reset_stop"){s.squad.m_v108={1,0,0};nested=true;}
  }
  s.snapshot();
  if(change){
   if(hidden){const int other=mode=="hidden_holes"?3:5;const int id=s.slots[other].m_memberIndex;s.payload(0,id);s.payload(other,4);
    if(!nested){s.path[id]=false;s.path[4]=false;s.expectReset();}}
   if(matching){const int other=mode=="match_skip"?3:1;s.payload(0,other);s.payload(other,0);s.path[other]=false;if(!nested)s.expectReset();}
   if(promotion){const int oldSlot=mode=="duplicate_leader"?1:2;const bool occupied=s.slots[0].hasSquadMemberAssigned;
    s.payload(0,4);s.expected[0].hasSquadMemberAssigned=true;s.payload(oldSlot,0);s.expected[oldSlot].hasSquadMemberAssigned=occupied;
    s.path[4]=s.path[0]=false;s.expected85=false;if(!nested)s.expectReset();}
  }
  if(nested)abortCheck=[&]{s.verify();require(clockCalls==0,"nested world-position stop precedes timer");};
  f.updateFormation();require(!nested,"expected nested stop");s.verify();require(clockCalls==0,"formation reassignment has no clock");
 }
#endif
 std::printf("PASS %s %u checks\n",mode.c_str(),checks);
}
