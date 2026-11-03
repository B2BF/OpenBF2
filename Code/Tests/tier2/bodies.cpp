// [CHOSEN, not recovered] bounded observers for the newly recovered vehicle bodies.
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <vector>
#include "../../BF2/Core/Ids.h"
#include "../../BF2/Core/Random.h"
#include "../../BF2/Core/System.h"
#include "../../BF2/Game/Objects/ForceObjectTemplate.h"
#include "../../BF2/Game/Objects/Game/ForceObject.h"
#include "../../BF2/Game/Objects/Game/PlayerControlObject.h"
#include "../../BF2/Game/Objects/Game/RotationalBundle.h"
#include "../../BF2/Game/Objects/RotationalBundleTemplate.h"
#include "../../BF2/Game/Objects/GameLogicComp/ArmorGLComp.h"
#include "../../BF2/Game/Objects/GameLogicComp/ArmorGLCompTemplate.h"
#include "../../BF2/Game/Objects/SpawnPoint.h"
#include "../../BF2/Game/Objects/SpawnPointTemplate.h"
#include "../../BF2/Game/Objects/ObjectLayoutAccess.h"
#include "../../BF2/Game/Objects/IObjectTemplate.h"
#include "../../BF2/IO/Input/PlayerInput.h"
#include "../../BF2/IO/Console/Console.h"
#include "../../BF2/Physics/PhysicsNode.h"
#include "../../BF2/Scene/Object/ObjectManager.h"
using namespace dice::hfe;
using namespace dice::hfe::world;
namespace {
int checks=0, failures=0;
void check(bool pass,const char* name) {
    ++checks; if(!pass)++failures;
    std::printf("[vehicle-bodies] %s %s\n",pass?"PASS":"FAIL",name);
}
bool near(float a,float b) {return std::fabs(a-b)<0.000002f;}
struct RecordingNode : PhysicsNode {
    std::vector<Vec3> positions,forces;
    void addAccelerationAtAbsolutePosition(const Vec3& p,const Vec3& f) override {
        positions.push_back(p);forces.push_back(f);
    }
};
struct ForceFixture : ForceObject {
    ForceFixture(const ForceObjectTemplate* t):ForceObject(t) {}
    float getPhysicBoundingRadius() const override {return 2.0f;}
};
// Expose member pointers for observation; no base object is cast to a fake derived instance.
struct PcoAccess : PlayerControlObject {
    static auto timer() { return &PcoAccess::m_unmannedInputTick; }
};
struct RegulateAccess : RotationalBundleTemplate {
    static auto yaw() { return &RegulateAccess::m_regulateYaw; }
    static auto pitch() { return &RegulateAccess::m_regulatePitch; }
    static auto roll() { return &RegulateAccess::m_regulateRoll; }
};
std::string console(const std::string& command) {
    std::string result;
    io::mainConsole->executeLine(command,result,false,false,false,false,false);
    return result;
}
void properties() {
    console("ObjectTemplate.create LandingGear tier2_gear");
    auto old=console("ObjectTemplate.debugForceGearUp");
    console("ObjectTemplate.debugForceGearUp 1");
    check(console("ObjectTemplate.debugForceGearUp")=="1","static force-gear binding");
    console("ObjectTemplate.debugForceGearUp "+old);
    console("ObjectTemplate.create FloatingBundle tier2_floater");
    console("ObjectTemplate.dragModifier 0.375");
    console("ObjectTemplate.sinkingSpeedMod 0.625");
    check(near(std::strtof(console("ObjectTemplate.dragModifier").c_str(),0),0.375f),"floater drag binding");
    check(near(std::strtof(console("ObjectTemplate.sinkingSpeedMod").c_str(),0),0.625f),"floater sinking binding");
}
}
bool vehicleBodies() {
    {
        ForceObjectTemplate t; RecordingNode node; ForceFixture force(&t);
        object_detail::LayoutAccess::physicsNode(&force)=&node;
        force.addRotationalAcceleration(Vec3{0,0,0});
        check(node.forces.empty(),"zero torque emits no force");
        force.addRotationalAcceleration(Vec3{3,5,7});
        const Vec3 p[]={{2,0,0},{-2,0,0},{0,2,0},{0,-2,0},{2,0,0},{-2,0,0}};
        const Vec3 f[]={{0,0,-3},{0,0,3},{0,0,-5},{0,0,5},{0,-7,0},{0,7,0}};
        bool paired=node.forces.size()==6;
        for(unsigned i=0;paired&&i<6;++i)
            paired=std::memcmp(&node.positions[i],&p[i],sizeof(Vec3))==0
                && std::memcmp(&node.forces[i],&f[i],sizeof(Vec3))==0;
        check(paired,"retail absolute lever pairs and ordering");
        object_detail::LayoutAccess::physicsNode(&force)=0;
        force.destroy(false); // Stack lifetime: mark destroyed without queuing a second deletion.
    }
    {
        ArmorGLCompTemplate t; ArmorGLComp armor(&t);
        check(armor.isInsideRepairRadius(Vec3{100,200,300}),"empty repair list permits all");
        t.addRepairPoint(Vec3{10,20,30},5);
        check(armor.isInsideRepairRadius(Vec3{13,24,30}),"inclusive repair sphere boundary");
        check(!armor.isInsideRepairRadius(Vec3{13,24,31}),"outside repair sphere");
        t.addRepairPoint(Vec3{-10,0,0},2);
        check(armor.isInsideRepairRadius(Vec3{-12,0,0}),"later repair sphere accepts");
        check(!armor.isInsideRepairRadius(Vec3{std::numeric_limits<float>::quiet_NaN(),0,0}),"unordered repair point rejects");
    }
    {
        SpawnPointTemplate t; SpawnPoint spawn(&t);
        check(spawn.getObjectToEnter(false,1)==0,"disabled human enter-on-spawn");
        t.m_aiEnterOnSpawn=true;
        check(spawn.getObjectToEnter(false,1)==0,"AI-only enter is disabled for humans");
        spawn.destroy(false);
    }
    PlayerControlObject* pco=0;
    for(const auto& entry:objectManager->getAllRegisteredObjects()) {
        IObject* o=entry.second;
        if(o&&!o->getParent()&&o->getTemplate()->getClassID()==CID_PlayerControlObjectTemplate) {
            pco=static_cast<PlayerControlObject*>(o);break;
        }
    }
    check(pco!=0,"stock PCO available");
    if(pco) {
        auto& nextInput=pco->*PcoAccess::timer();
        const auto timer=nextInput, seed=Rand.m_seed;
        io::PlayerInput input; std::memset(&input,0,sizeof(input));
        input.inputs[50]=0.75f;input.mappingFlags=1ULL<<50;
        const auto original=input;
        nextInput=0xffffffffu;Rand.m_seed=0x12345678u;
        pco->getUnmannedInput(input);
        check(std::memcmp(&input,&original,sizeof(input))==0 && Rand.m_seed==0x12345678u,"unsigned timer leaves input and RNG untouched");
        nextInput=0;
        const auto tick=static_cast<unsigned>(WorldPref::getTickCount());
        pco->getUnmannedInput(input);
        check(input.mappingFlags==((1ULL<<50)|15) && input.inputs[50]==0.75f,"unmanned mapping preserves unrelated controls");
        check(input.inputs[0]==-1 && near(input.inputs[1],-0.6735224f)
            && near(input.inputs[2],0.0627387f) && input.inputs[3]==1
            && Rand.m_seed==1242154650u,"seeded five-draw retail sequence");
        check(nextInput==tick+300,"default unsigned next-input tick");
        nextInput=timer;Rand.m_seed=seed;
    }
    int parented=0;
    for(const auto& entry:objectManager->getAllRegisteredObjects()) {
        IObject* o=entry.second;
        if(!o||o->getTemplate()->getClassID()!=CID_RotationalBundleTemplate)continue;
        IObject* control=o;
        while(control&&!control->queryInterface(IID_IPlayerControlObject))control=control->getParent();
        if(!control||!control->getParent())continue;
        auto bundle=static_cast<RotationalBundle*>(o);
        auto t=const_cast<RotationalBundleTemplate*>(static_cast<const RotationalBundleTemplate*>(o->getTemplate()));
        auto root=getRootParent(o);
        auto node=static_cast<IPhysicsNode*>(object_detail::LayoutAccess::physicsNode(root));
        if(!node)continue;
        auto& ry=t->*RegulateAccess::yaw();
        auto& rp=t->*RegulateAccess::pitch();
        auto& rr=t->*RegulateAccess::roll();
        auto yaw=ry,pitch=rp,roll=rr;
        auto speed=node->getRotationalSpeed();
        ry.x=rp.x=rr.x=1;
        ry.y=rp.y=rr.y=0;
        node->setRotationalSpeed(Vec3{0.25f,0.5f,0.75f});
        Vec3 regulate;bundle->updateRegulate(regulate,1);
        const io::PlayerInputMap axes[]={t->getInputToYaw(),t->getInputToPitch(),t->getInputToRoll()};
        for(int axis=0;axis<3;++axis) {
            if(axes[axis]==io::PINone||std::fabs((&regulate.x)[axis])<0.001f)continue;
            const float value=(&regulate.x)[axis];
            for(int mode=0;mode<3;++mode) {
                io::PlayerInput input={}; input.mappingFlags=1ULL<<axes[axis];
                input.setInput(axes[axis],mode==0?0:mode==1?value*0.5f:-value*0.5f);
                bundle->preHandlePlayerInput(input,1);
                check(near(input.inputs[axes[axis]],mode<2?value:-value*0.5f),"parented pre-input helps same direction only");
            }
            ++parented;break;
        }
        node->setRotationalSpeed(speed);
        ry=yaw;rp=pitch;rr=roll;
        if(parented)break;
    }
    check(parented!=0,"parented stock rotational pre-input exercised");
    properties();
    std::printf("[vehicle-bodies] checks=%d failures=%d\n",checks,failures);
    return checks>0 && failures==0;
}
