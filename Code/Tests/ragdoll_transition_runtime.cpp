// [TEMP] [CHOSEN, not recovered] A static authored pose feeds the real Soldier
// transition, Skeleton evaluation, RagDollTemplate creation and RagDoll reset.
// Animation playback is disabled through its cvars; there is no map, collision
// simulation, damage event, rendering or joined-client acceptance in this fixture.
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include "../BF2/Animation/BoneAnimation/AnimationSystem.h"
#include "../BF2/Animation/BoneAnimation/RagDoll.h"
#include "../BF2/Animation/BoneAnimation/RagDollTemplate.h"
#include "../BF2/Animation/BoneAnimation/Skeleton.h"
#include "../BF2/Core/ClassManager.h"
#include "../BF2/Core/EventManager.h"
#include "../BF2/Core/Ids.h"
#include "../BF2/Core/Modules.h"
#include "../BF2/Core/System.h"
#include "../BF2/Game/GameLogic/GameLogic.h"
#include "../BF2/Game/Objects/Base/Camera.h"
#include "../BF2/Game/Objects/Game/Soldier.h"
#include "../BF2/Game/Objects/Game/SoldierTemplate.h"
#include "../BF2/Game/Objects/ObjectLayoutAccess.h"
#include "../BF2/Game/Objects/TemplateLeaves.h"
#include "../BF2/Game/Utility/Vars.h"
#include "../BF2/IO/Input/PlayerInput.h"
#include "../BF2/Physics/PhysicsNodeManager.h"
#include "../BF2/Physics/ResponsePhysicsManager.h"
#include "../BF2/Physics/SoldierPhysicsNode.h"
#include "../BF2/Physics/SoldierResponse.h"
#include "../BF2/Sound/BfSound.h"
#include "../BF2/Scene/Object/BinTreeCuller.h"
#include "../BF2/Scene/Object/QuadTreeCuller.h"
#include "../BF2/Scene/Object/ObjectManager.h"
#include "../BF2/Scene/Object/ObjectTypes/Base/ObjectTemplateManager.h"
namespace dice { namespace hfe { extern ClassManager* classManager;
namespace world { extern ResponsePhysicsManager* responsePhysicsManager;
extern PhysicsNodeManager* physicsNodeManager; } } }
using namespace dice::hfe;
using namespace dice::hfe::world;
namespace anim = dice::anim;
namespace {
int checks = 0, failures = 0;
void check(bool ok, const char* label) {
    ++checks; if (!ok) ++failures;
    std::printf("%s %s\n", ok ? "PASS" : "FAIL", label); std::fflush(stdout);
}
bool near(const Vec3& a, const Vec3& b) {
    return std::fabs(a.x-b.x) < 0.0001f && std::fabs(a.y-b.y) < 0.0001f
        && std::fabs(a.z-b.z) < 0.0001f;
}
Vec3 position(const Mat4& m) { return Vec3{m.m[12],m.m[13],m.m[14]}; }
[[noreturn]] void unexpected() { std::abort(); }
class Classes : public ClassManager {
    std::vector<ClassRegInfo> entries;
public:
    Classes() {
        GameLogicObjTemplBFModule().getClassList(entries);
        GameLogicObjTemplBaseModule().getClassList(entries);
        ObjTemplBaseModule().getClassList(entries);
        BfSoundModule().getClassList(entries);
    }
    IBase* createInstance(unsigned int cid, unsigned int iid, IBase* outer) override {
        if (cid == CID_BinTreeCuller) return BinTreeCuller::create(iid, outer);
        if (cid == CID_QuadTreeCuller) return QuadTreeCuller::create(iid, outer);
        for (const auto& entry : entries) if (entry.classId == cid) return entry.create(iid, outer);
        unexpected();
    }
};
// Own the real GameLogic registry required by Soldier construction. Unrelated
// abstract operations abort instead of supplying fabricated engine behavior.
class Logic : public GameLogic {
public:
    void addRef() override { unexpected(); }
    int getRef() override { unexpected(); }
    int release() override { unexpected(); }
    void* queryInterface(unsigned int) const override { unexpected(); }
    void runNextLevel() override { unexpected(); }
    void setNextLevel(int) override { unexpected(); }
    bool selectKitAndUnlockLevel(world::IPlayer*,int,int) override { unexpected(); }
    void setTicketState(unsigned int,int) override { unexpected(); }
    int getTicketState(unsigned int) override { unexpected(); }
    void setDefaultNumberOfTickets(unsigned int,unsigned int) override { unexpected(); }
    unsigned int getDefaultNumberOfTickets(unsigned int) override { unexpected(); }
    void setDefaultNumberOfTicketsEx(unsigned int,unsigned int,unsigned int) override { unexpected(); }
    unsigned int getDefaultNumberOfTicketsEx(unsigned int,unsigned int) override { unexpected(); }
    void setDefaultTimeToNextAIWave(float) override { unexpected(); }
    float getDefaultTimeToNextAIWave() override { unexpected(); }
    void setTicketLossAtEndPerMin(float) override { unexpected(); }
    float getTicketLossAtEndPerMin() override { unexpected(); }
    void setTicketLossPerMin(unsigned int,float) override { unexpected(); }
    float getTicketLossPerMin(unsigned int) override { unexpected(); }
    void playEffect(Vec3,const IObjectTemplate*,Vec3) override { unexpected(); }
    bool handleCollisionForProjectile(IObject*,IObject*,const Vec3&,const Vec3&,const Vec3&,int,int,CollisionFaceInfo*,CollisionFaceInfo*) override { unexpected(); }
    bool handleCollisionForReplenisher(bool,IObject*,IObject*,const Vec3&,const Vec3&,const Vec3&,int,int) override { unexpected(); }
    bool handleCollisionForResurrecter(IObject*,IObject*,float,float,const Vec3&,const Vec3&,const Vec3&,int,int) override { unexpected(); }
    void handleCollision(IObject*,IObject*,const Vec3&,const Vec3&,const Vec3&,int,int,CollisionFaceInfo*,CollisionFaceInfo*) override { unexpected(); }
    void spawnMultipleObjects(const IObjectTemplate*,int) override { unexpected(); }
    IObject* spawnObject(const std::string&,Vec3,Vec3,int) override { unexpected(); }
    IObject* spawnObject(const IObjectTemplate*,Vec3,Vec3,int) override { unexpected(); }
    void checkPlayerTriggers(world::IPlayer*,IObject*,io::PlayerInput,bool) override { unexpected(); }
    void toggleLocalFreeCamera() override { unexpected(); }
    TicketHistory& getTicketHistory() override { unexpected(); }
    void setTicketHistory(const TicketHistory&) override { unexpected(); }
};
class EmptyAnimation : public anim::AnimationSystemTemplate {
    anim::AnimationSystem* makeInstance() override { return 0; }
};
const int bones[] = {1,2,4,6,7,9,15,16,19,31,32,34,47};
const Vec3 poses[] = {{-.15f,1,0},{-.15f,.55f,.03f},{-.15f,.1f,.05f},
    {0,1,0},{.15f,.55f,.03f},{.15f,.1f,.05f},{-.25f,1.5f,0},
    {-.5f,1.35f,.1f},{-.7f,1.1f,.15f},{.25f,1.5f,0},
    {.5f,1.35f,.1f},{.7f,1.1f,.15f},{0,1.8f,0}};
anim::Skeleton* makeSkeleton() {
    auto* skeleton = new anim::Skeleton();
    skeleton->m_boneCount = 75; // Includes the actual third-person spine/head override indices.
    skeleton->m_bones = new anim::Skeleton::Bone[75];
    skeleton->m_boneTemplates = new anim::Skeleton::BoneTemplate[75];
    skeleton->m_transforms = new Mat4[75];
    for (int i=0;i<75;++i) {
        skeleton->m_boneTemplates[i].m_index=i;
        skeleton->m_boneTemplates[i].m_parentIndex=i ? 0 : -1;
        skeleton->m_boneTemplates[i].m_depth=i ? 1 : 0;
        skeleton->m_transforms[i]=Mat4::getIdentity();
    }
    for (unsigned i=0;i<sizeof(bones)/sizeof(bones[0]);++i) {
        Mat4& bone=skeleton->m_bones[bones[i]].m_transform;
        bone.m[12]=poses[i].x; bone.m[13]=poses[i].y; bone.m[14]=poses[i].z;
    }
    skeleton->transform(&Mat4::getIdentity(),-1);
    return skeleton;
}
class SoldierSettings : public SoldierTemplate {
public:
    SoldierSettings() {
        m_skeleton1P=makeSkeleton(); m_skeleton3P=makeSkeleton();
        m_animationSystem1P=new EmptyAnimation(); m_animationSystem3P=new EmptyAnimation();
    }
};
void transitionCase(float yaw, const Vec3& translation, const Vec3& velocity) {
    auto* settings=new SoldierSettings();
    auto* soldier=new Soldier(settings);
    Mat4 objectTransform=Mat4::getIdentity();
    setRotateYDeg(objectTransform,yaw);
    objectTransform.m[12]=translation.x; objectTransform.m[13]=translation.y;
    objectTransform.m[14]=translation.z;
    soldier->setAbsoluteTransformation(objectTransform);
    auto* camera=new Camera(new CameraTemplate());
    camera->setAbsoluteTransformation(objectTransform);
    (*soldier->getOwnCameras())[0]=camera;
    soldier->setCharacterMode(1);
    check(soldier->getActiveItemIndex()==-1,"fixture soldier has no active held item");
    auto* node=new SoldierPhysicsNode(); node->setObject(soldier);
    object_detail::LayoutAccess::physicsNode(soldier)=node;
    auto* response=new SoldierResponsePhysics(); response->setObject(soldier);
    object_detail::LayoutAccess::responsePhysics(soldier)=response;
    node->setPositionalSpeed(velocity);
    auto* definition=new anim::RagDollTemplate();
    anim::g_ragDollTemplate=definition;
    definition->m_skeleton=settings->getSkeleton3P();
    for (int bone : bones) definition->addParticle(bone,2.0f);
    definition->addDistanceConstraint(1,6);
    // Establish the actual living third-person pose before the transition.
    soldier->applyAnimationsOnSkeleton(0.0f,true);
    std::vector<Vec3> before;
    for (int bone : bones) {
        Mat4 world=Mat4::getIdentity();
        world.mult(soldier->getCollisionSkeleton(0)->m_transforms[bone],objectTransform);
        before.push_back(position(world));
    }
    check(near(before[3],translation),"living hip starts at the soldier position");
    soldier->enableRagDoll();
    anim::RagDoll* const ragdoll=definition->m_lastInstance;
    check(ragdoll!=0,"real transition registers its new ragdoll");
    if (!ragdoll) return;
    check(ragdoll->m_particles.size()==before.size(),"all thirteen authored particles are created");
    check(!node->getEnabled() && !response->getEnabled(),"transition disables both original physics interfaces");
    for (unsigned i=0;i<before.size() && i<ragdoll->m_particles.size();++i) {
        const auto& particle=ragdoll->m_particles[i];
        check(near(particle.m_position,before[i]),"world particle matches the living skeleton without a one-metre jump");
        const Vec3 displacement={particle.m_position.x-particle.m_oldPosition.x,
            particle.m_position.y-particle.m_oldPosition.y,particle.m_position.z-particle.m_oldPosition.z};
        const Vec3 inherited={velocity.x*.015f,velocity.y*.015f,velocity.z*.015f};
        check(near(displacement,inherited),"particle history preserves inherited velocity");
    }
    check(near(soldier->getAbsolutePosition(),translation),"initial hip centre keeps the soldier world position");
    check(near(ragdoll->m_position,translation),"real negative-time centre update observes the rebased hip");
    const std::vector<anim::RagDoll::Particle> first=ragdoll->m_particles;
    const Vec3 centre=ragdoll->m_position;
    soldier->enableRagDoll();
    bool unchanged=definition->m_lastInstance==ragdoll && near(ragdoll->m_position,centre);
    for (unsigned i=0;i<first.size();++i)
        unchanged &= near(first[i].m_position,ragdoll->m_particles[i].m_position)
            && near(first[i].m_oldPosition,ragdoll->m_particles[i].m_oldPosition);
    check(unchanged,"repeated enable preserves the instance, pose and motion history");
    std::printf("transition yaw=%g hip=(%g,%g,%g) expected=(%g,%g,%g)\n",yaw,
        centre.x,centre.y,centre.z,translation.x,translation.y,translation.z);
}
}
int main() {
    g_system=new System(); eventManager=new EventManager(); classManager=new Classes();
    responsePhysicsManager=new ResponsePhysicsManager(); g_bfSound=new BfSoundNull();
    objectTemplateManager=new ObjectTemplateManager(); objectManager=new ObjectManager();
    physicsNodeManager=new PhysicsNodeManager(); gameLogic=new Logic();
    *Vars::getInt("anim_playSoldierAnimations",1)=0;
    *Vars::getInt("anim_playSoldierPostAnimations",1)=0;
    transitionCase(0,Vec3{7,10,-11},Vec3{0,0,0});
    transitionCase(90,Vec3{-17,4,9},Vec3{3,-2,4});
    std::printf("Ragdoll transition: %d checks, %d failures\n",checks,failures);
    // Process-owned fixture objects avoid claiming engine/map teardown coverage.
    return failures ? 1 : 0;
}
