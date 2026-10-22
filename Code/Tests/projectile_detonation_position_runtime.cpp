// [TEMP] [CHOSEN, not recovered] Tests the actual GenericProjectile detonation TU
// and Object transforms. The detonation component observes the by-value arguments;
// the object manager observes destruction messages. No map, socket or game starts.
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include "../BF2/Core/ClassManager.h"
#include "../BF2/Core/EventManager.h"
#include "../BF2/Core/Ids.h"
#include "../BF2/Core/System.h"
#include "../BF2/Game/GameLogic/GameLogic.h"
#include "../BF2/IO/Input/PlayerInput.h"
#include "../BF2/Game/Objects/Projectiles/GenericProjectile.h"
#include "../BF2/Game/Objects/Projectiles/GenericProjectileTemplate.h"
#include "../BF2/Game/Objects/Projectiles/ProjectileCompTemplates.h"
#include "../BF2/Game/Objects/Projectiles/DetonationComponents/DefaultDetonationComp.h"
#include "../BF2/Physics/MaterialManager.h"
#include "../BF2/Physics/ResponsePhysicsManager.h"
#include "../BF2/Scene/Object/BinTreeCuller.h"
#include "../BF2/Scene/Object/QuadTreeCuller.h"
#include "../BF2/Scene/Object/ObjectManager.h"
#include "../BF2/Scene/Object/ObjectTypes/Base/ObjectTemplateManager.h"
namespace dice { namespace hfe { extern ClassManager* classManager;
namespace world { extern ResponsePhysicsManager* responsePhysicsManager;
extern IBase* materialManager; } } }
using namespace dice::hfe;
using namespace dice::hfe::world;
namespace {
int checks = 0, failures = 0;
void check(bool ok, const char* label) {
    ++checks; if (!ok) ++failures;
    std::printf("%s %s\n", ok ? "PASS" : "FAIL", label); std::fflush(stdout);
}
bool same(const Vec3& a, const Vec3& b) {
    return std::fabs(a.x-b.x)<0.0001f && std::fabs(a.y-b.y)<0.0001f && std::fabs(a.z-b.z)<0.0001f;
}
[[noreturn]] void unexpected(const char* label) {
    std::fprintf(stderr,"Unexpected fixture collaborator: %s\n",label); std::abort();
}
class Classes : public ClassManager {
    IBase* createInstance(unsigned int cid, unsigned int iid, IBase* outer) override {
        if (cid == CID_BinTreeCuller) return BinTreeCuller::create(iid,outer);
        if (cid == CID_QuadTreeCuller) return QuadTreeCuller::create(iid,outer);
        unexpected("class factory");
    }
};
class Objects : public ObjectManager {
public:
    int messages = 0;
    void postMessage(IObject*, TemplateMessage id, float delay) override {
        check(id == static_cast<TemplateMessage>(32) && delay == 0.0f,
              "non-networked detonation queues immediate destruction");
        ++messages;
    }
};
class Logic : public GameLogic {
public:
    bool isServer() const override { return true; }
    void addRef() override { unexpected("addRef"); }
    int getRef() override { unexpected("getRef"); }
    int release() override { unexpected("release"); }
    void* queryInterface(unsigned int) const override { unexpected("queryInterface"); }
    void runNextLevel() override { unexpected("runNextLevel"); }
    void setNextLevel(int) override { unexpected("setNextLevel"); }
    bool selectKitAndUnlockLevel(IPlayer*,int,int) override { unexpected("selectKitAndUnlockLevel"); }
    void setTicketState(unsigned int,int) override { unexpected("setTicketState"); }
    int getTicketState(unsigned int) override { unexpected("getTicketState"); }
    void setDefaultNumberOfTickets(unsigned int,unsigned int) override { unexpected("setDefaultNumberOfTickets"); }
    unsigned int getDefaultNumberOfTickets(unsigned int) override { unexpected("getDefaultNumberOfTickets"); }
    void setDefaultNumberOfTicketsEx(unsigned int,unsigned int,unsigned int) override { unexpected("setDefaultNumberOfTicketsEx"); }
    unsigned int getDefaultNumberOfTicketsEx(unsigned int,unsigned int) override { unexpected("getDefaultNumberOfTicketsEx"); }
    void setDefaultTimeToNextAIWave(float) override { unexpected("setDefaultTimeToNextAIWave"); }
    float getDefaultTimeToNextAIWave() override { unexpected("getDefaultTimeToNextAIWave"); }
    void setTicketLossAtEndPerMin(float) override { unexpected("setTicketLossAtEndPerMin"); }
    float getTicketLossAtEndPerMin() override { unexpected("getTicketLossAtEndPerMin"); }
    void setTicketLossPerMin(unsigned int,float) override { unexpected("setTicketLossPerMin"); }
    float getTicketLossPerMin(unsigned int) override { unexpected("getTicketLossPerMin"); }
    void playEffect(Vec3,const IObjectTemplate*,Vec3) override { unexpected("playEffect"); }
    bool handleCollisionForProjectile(IObject*,IObject*,const Vec3&,const Vec3&,const Vec3&,int,int,CollisionFaceInfo*,CollisionFaceInfo*) override { unexpected("handleCollisionForProjectile"); }
    bool handleCollisionForReplenisher(bool,IObject*,IObject*,const Vec3&,const Vec3&,const Vec3&,int,int) override { unexpected("handleCollisionForReplenisher"); }
    bool handleCollisionForResurrecter(IObject*,IObject*,float,float,const Vec3&,const Vec3&,const Vec3&,int,int) override { unexpected("handleCollisionForResurrecter"); }
    void handleCollision(IObject*,IObject*,const Vec3&,const Vec3&,const Vec3&,int,int,CollisionFaceInfo*,CollisionFaceInfo*) override { unexpected("handleCollision"); }
    void spawnMultipleObjects(const IObjectTemplate*,int) override { unexpected("spawnMultipleObjects"); }
    IObject* spawnObject(const std::string&,Vec3,Vec3,int) override { unexpected("spawnObject(name)"); }
    IObject* spawnObject(const IObjectTemplate*,Vec3,Vec3,int) override { unexpected("spawnObject(template)"); }
    void checkPlayerTriggers(IPlayer*,IObject*,io::PlayerInput,bool) override { unexpected("checkPlayerTriggers"); }
    void toggleLocalFreeCamera() override { unexpected("toggleLocalFreeCamera"); }
    TicketHistory& getTicketHistory() override { unexpected("getTicketHistory"); }
    void setTicketHistory(const TicketHistory&) override { unexpected("setTicketHistory"); }
};
class Detonation : public DefaultDetonationComp {
public:
    int calls = 0;
    Vec3 position = {0,0,0};
    Vec3 normal = {0,0,0};
    int material = -1;
    unsigned int tick = 0;
    Detonation() : DefaultDetonationComp(new DefaultDetonationCompTemplate()) {}
    void detonate(IObject* source, int collisionMaterial, Vec3 at, unsigned int when,
                  Vec3 surfaceNormal, CollisionFaceInfo* face) override {
        ++calls; position=at; normal=surfaceNormal; material=collisionMaterial; tick=when;
        check(!source && !face,"terrain detonation keeps null target and face");
    }
};
void exercise(Objects& objects, const Vec3& start, const Vec3& contact, bool collided) {
    auto* projectile = new GenericProjectile(new GenericProjectileTemplate());
    auto* observer = new Detonation();
    check(projectile->setComponent(IID_IProjectileDetonationComp,
          static_cast<IObjectComponent*>(observer)),"attach typed detonation observer");
    projectile->initComponents();
    projectile->setAbsolutePosition(start);
    const Vec3& originalReference = projectile->getAbsolutePosition();
    const Vec3 normal = {0,1,0}, velocity = {0,-500,0};
    if (collided) {
        const Vec3 offset = {contact.x-start.x,contact.y-start.y,contact.z-start.z};
        check(!projectile->handleCollision(0,velocity,normal,offset,0,40,0,0),
              "actual collision records contact without a collision component");
        check(same(projectile->getAbsolutePosition(),start),"collision recording precedes position relocation");
    }
    const int messages = objects.messages;
    projectile->detonate(false);
    const Vec3 expected = collided ? contact : start;
    check(observer->calls == 1,"actual detonate invokes component once");
    check(same(observer->position,expected),"component receives the post-relocation blast center");
    check(same(projectile->getAbsolutePosition(),expected),"actual object relocates only for a recorded collision");
    check(same(originalReference,expected),"held Object position reference observes root relocation");
    check(objects.messages == messages+1,"detonation preserves destruction message count");
    if (collided) {
        check(observer->material == 40 && same(observer->normal,normal)
              && observer->tick == WorldPref::getTickCount(),"collision metadata survives detonation");
        const Vec3 nearby = {contact.x+0.5f,contact.y,contact.z};
        const float dx=observer->position.x-nearby.x,dy=observer->position.y-nearby.y,dz=observer->position.z-nearby.z;
        check(dx*dx+dy*dy+dz*dz < 1.5f*1.5f,"M134-sized radius includes a point next to the actual impact");
    }
    std::printf("center start=%.2f/%.2f/%.2f contact=%.2f/%.2f/%.2f observed=%.2f/%.2f/%.2f\n",
        start.x,start.y,start.z,contact.x,contact.y,contact.z,observer->position.x,observer->position.y,observer->position.z);
}
}
int main() {
    g_system=new System(); eventManager=new EventManager(); classManager=new Classes();
    responsePhysicsManager=new ResponsePhysicsManager();
    auto* materials=new MaterialManager(); materialManager=materials;
    materials->setAttMaterial(0); materials->setAttMaterial(40);
    objectTemplateManager=new ObjectTemplateManager();
    auto* objects=new Objects(); objectManager=objects; gameLogic=new Logic();
    exercise(*objects,Vec3{10,20,30},Vec3{10,0,30},true);
    exercise(*objects,Vec3{-12,3,17},Vec3{4,-5,-6},true);
    exercise(*objects,Vec3{7,8,9},Vec3{7,8,9},true);
    exercise(*objects,Vec3{1,2,3},Vec3{90,80,70},false);
    std::printf("Projectile detonation position: %d checks, %d failures\n",checks,failures);
    // Test-owned objects survive to process exit; no full lifecycle/gameplay claim.
    return failures ? 1 : 0;
}
