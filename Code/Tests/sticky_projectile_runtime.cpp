// [TEMP] [CHOSEN, not recovered] Strict engine-object regression for sticky
// attachment decisions and event transport. No map, socket or player session.
// The collision policy accepts contacts; activation is counted without starting
// effects. All attachment, event, networkable, transform and BitStream bodies
// are production code. Fixture-owned services survive until process exit.
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include "../BF2/Core/ClassManager.h"
#include "../BF2/Core/EventManager.h"
#include "../BF2/Core/Ids.h"
#include "../BF2/Core/System.h"
#include "../BF2/Game/GameLogic/GameLogic.h"
#include "../BF2/Game/GameEvents/StickyProjectileEvent.h"
#include "../BF2/Game/Objects/Networkables/Networkables.h"
#include "../BF2/Game/Objects/Projectiles/GenericProjectile.h"
#include "../BF2/Game/Objects/Projectiles/GenericProjectileTemplate.h"
#include "../BF2/Game/Objects/Projectiles/ProjectileCompTemplates.h"
#include "../BF2/Game/Objects/Projectiles/CollisionComponents/StickyCollisionComp.h"
#include "../BF2/IO/BitStream.h"
#include "../BF2/IO/Input/PlayerInput.h"
#include "../BF2/IO/Network/NetworkManager.h"
#include "../BF2/IO/Network/NetworkableInfo.h"
#include "../BF2/Physics/MaterialManager.h"
#include "../BF2/Physics/Material.h"
#include "../BF2/Physics/ResponsePhysicsManager.h"
#include "../BF2/Scene/Object/BinTreeCuller.h"
#include "../BF2/Scene/Object/QuadTreeCuller.h"
#include "../BF2/Scene/Object/ObjectManager.h"
#include "../BF2/Scene/Object/PlayerManager.h"
#include "../BF2/Scene/Object/ObjectTypes/Base/ObjectTemplateManager.h"
namespace dice { namespace hfe { extern ClassManager* classManager;
namespace io { extern NetworkManager* networkManager; }
namespace world { extern ResponsePhysicsManager* responsePhysicsManager;
extern IBase* materialManager; } } }
using namespace dice::hfe;
using namespace dice::hfe::world;
namespace {
int checks=0, failures=0;
void check(bool condition, const char* label) {
    ++checks; if (!condition) ++failures;
    std::printf("%s %s\n",condition ? "PASS" : "FAIL",label); std::fflush(stdout);
}
bool same(const Mat4& a, const Mat4& b) {
    for (int i=0;i<16;++i) if (std::fabs(a.m[i]-b.m[i])>0.00001f) return false;
    return true;
}
[[noreturn]] void unexpected(const char* label) {
    std::fprintf(stderr,"Unexpected fixture collaborator: %s\n",label); std::abort();
}
class Classes : public ClassManager {
    IBase* createInstance(unsigned int cid,unsigned int iid,IBase* outer) override {
        if (cid==CID_BinTreeCuller) return BinTreeCuller::create(iid,outer);
        if (cid==CID_QuadTreeCuller) return QuadTreeCuller::create(iid,outer);
        unexpected("class factory");
    }
};
class Logic : public GameLogic {
public:
    bool server=true;
    int contacts=0, sourceMaterial=-1, targetMaterial=-1;
    bool isServer() const override { return server; }
    bool handleCollisionForProjectile(IObject*,IObject*,const Vec3&,const Vec3&,const Vec3&,
        int source,int target,CollisionFaceInfo*,CollisionFaceInfo*) override {
        ++contacts; sourceMaterial=source; targetMaterial=target; return true;
    }
    void addRef() override { unexpected("addRef"); }
    int getRef() override { unexpected("getRef"); }
    int release() override { unexpected("release"); }
    void* queryInterface(unsigned int) const override { unexpected("queryInterface"); }
    void runNextLevel() override { unexpected("runNextLevel"); }
    bool selectKitAndUnlockLevel(world::IPlayer*,int,int) override { unexpected("selectKit"); }
    void setNextLevel(int) override { unexpected("setNextLevel"); }
    void setTicketState(unsigned int,int) override { unexpected("setTicketState"); }
    int getTicketState(unsigned int) override { unexpected("getTicketState"); }
    void setDefaultNumberOfTickets(unsigned int,unsigned int) override { unexpected("setTickets"); }
    unsigned int getDefaultNumberOfTickets(unsigned int) override { unexpected("getTickets"); }
    void setDefaultNumberOfTicketsEx(unsigned int,unsigned int,unsigned int) override { unexpected("setTicketsEx"); }
    unsigned int getDefaultNumberOfTicketsEx(unsigned int,unsigned int) override { unexpected("getTicketsEx"); }
    void setDefaultTimeToNextAIWave(float) override { unexpected("setWave"); }
    float getDefaultTimeToNextAIWave() override { unexpected("getWave"); }
    void setTicketLossAtEndPerMin(float) override { unexpected("setTicketLossAtEnd"); }
    float getTicketLossAtEndPerMin() override { unexpected("getTicketLossAtEnd"); }
    void setTicketLossPerMin(unsigned int,float) override { unexpected("setTicketLoss"); }
    float getTicketLossPerMin(unsigned int) override { unexpected("getTicketLoss"); }
    void playEffect(Vec3,const IObjectTemplate*,Vec3) override { unexpected("playEffect"); }
    bool handleCollisionForReplenisher(bool,IObject*,IObject*,const Vec3&,const Vec3&,const Vec3&,int,int) override { unexpected("replenisher"); }
    bool handleCollisionForResurrecter(IObject*,IObject*,float,float,const Vec3&,const Vec3&,const Vec3&,int,int) override { unexpected("resurrecter"); }
    void handleCollision(IObject*,IObject*,const Vec3&,const Vec3&,const Vec3&,int,int,CollisionFaceInfo*,CollisionFaceInfo*) override { unexpected("handleCollision"); }
    void spawnMultipleObjects(const IObjectTemplate*,int) override { unexpected("spawnMultipleObjects"); }
    IObject* spawnObject(const std::string&,Vec3,Vec3,int) override { unexpected("spawnObject(name)"); }
    IObject* spawnObject(const IObjectTemplate*,Vec3,Vec3,int) override { unexpected("spawnObject(template)"); }
    void checkPlayerTriggers(world::IPlayer*,IObject*,io::PlayerInput,bool) override { unexpected("checkTriggers"); }
    void toggleLocalFreeCamera() override { unexpected("toggleCamera"); }
    TicketHistory& getTicketHistory() override { unexpected("getTicketHistory"); }
    void setTicketHistory(const TicketHistory&) override { unexpected("setTicketHistory"); }
};
class Projectile : public GenericProjectile {
public:
    GenericProjectileNetworkable* network;
    StickyCollisionComp* collision;
    int activations=0;
    Projectile() : GenericProjectile(new GenericProjectileTemplate()), network(0) {
        collision=new StickyCollisionComp(new StickyCollisionCompTemplate());
        check(setComponent(IID_IProjectileCollisionComp,static_cast<IObjectComponent*>(collision)),
              "attach actual sticky collision component");
        initComponents(); collision->setObject(this); collision->init(this);
    }
    io::INetworkable* getNetworkable() const override { return network; }
    void activate() override { ++activations; }
};
class Event : public StickyProjectileEvent {
public:
    Event() : StickyProjectileEvent() {}
    Event(GenericProjectile* p,IObject* t,const Mat4& m) : StickyProjectileEvent(p,t,m) {}
    unsigned short projectileId() const { return m_projectileNetId; }
    unsigned short targetId() const { return m_targetNetId; }
    unsigned char playerId() const { return m_playerId; }
    unsigned char targetCount() const { return m_targetNetIdCount; }
    unsigned char childIndex() const { return m_childIndex; }
    const Mat4& transform() const { return m_transform; }
};
void registerProjectile(Projectile* projectile,unsigned short id) {
    projectile->network=new GenericProjectileNetworkable();
    check(projectile->network->init(projectile),"initialize canonical projectile networkable");
    projectile->network->setNetworkableInfo(new io::NetworkableInfo());
    auto* descriptor=new io::NetworkableDescriptor();
    io::UniqueNetProperty property;
    property.networkables.push_back(projectile->network);
    descriptor->properties.push_back(property);
    io::networkManager->addDescriptor(descriptor,id);
    check(projectile->network->m_networkableId==id && io::networkManager->getObject(id)==projectile,
          "real network registry resolves the selected 16-bit ID");
}
void eventCase(unsigned short id,Projectile* target) {
    auto* projectile=new Projectile();
    if (id) registerProjectile(projectile,id);
    else {
        projectile->network=new GenericProjectileNetworkable();
        check(projectile->network->init(projectile),"initialize unregistered canonical networkable");
        check(projectile->network->m_networkableId==0,"unregistered network ID starts at zero");
    }
    Mat4 absolute=Mat4::getIdentity(); absolute.m[12]=12.5f; absolute.m[13]=-3.25f; absolute.m[14]=77.0f;
    projectile->setAbsoluteTransformation(absolute);
    Mat4 relative=Mat4::getIdentity(); relative.m[0]=0; relative.m[2]=-1;
    relative.m[8]=1; relative.m[10]=0; relative.m[12]=1; relative.m[13]=2; relative.m[14]=3;
    Event source(projectile,target,relative);
    const Mat4 expected=target ? relative : absolute;
    check(source.projectileId()==id,"producer captures native-layout projectile network ID");
    check(source.targetId()==(target ? target->network->m_networkableId : 0),"producer captures optional target ID");
    check(source.playerId()==255,"unowned projectile retains player sentinel");
    check(same(source.transform(),expected),"producer selects relative target or absolute terrain transform");
    io::BitStream packet(128);
    check(source.serialize(&packet),"actual event serializer succeeds");
    const unsigned bits=packet.getWriteBitPosition();
    check(bits==gameEventTypeIdBitCount()+16+16+8+384+(target ? 16 : 0),"wire size includes only present optional fields");
    // Check the format independently of StickyProjectileEvent::deSerialize.
    io::BitStream wire(packet.getBuffer(),packet.getSizeBytes()); wire.setBitSize(bits);
    unsigned wireType=0, wireProjectile=0, wireTarget=0, wireOwner=0;
    wire.readBits(&wireType,gameEventTypeIdBitCount());
    wire.readBits(&wireProjectile,16); wire.readBits(&wireTarget,16);
    check(wireType==32 && wireProjectile==id && wireTarget==(target ? target->network->m_networkableId : 0),
          "independent wire reader verifies ID fields and widths");
    if (target) {
        unsigned count=0, child=0; wire.readBits(&count,8); wire.readBits(&child,8);
        check(count==0 && child==0,"independent wire reader verifies optional index bytes");
    }
    wire.readBits(&wireOwner,8); check(wireOwner==255,"independent wire reader verifies owner byte");
    const int rows[4]={8,0,4,12};
    bool correctRows=true;
    for(int row : rows) for(int column=0;column<3;++column) {
        float value=0; wire.readBits(&value,32);
        correctRows=correctRows && value==expected.m[row+column];
    }
    check(correctRows && !wire.hasError(),"independent wire reader verifies row order 2,0,1,3");
    io::BitStream input(packet.getBuffer(),packet.getSizeBytes()); input.setBitSize(bits);
    unsigned type=0; input.readBits(&type,gameEventTypeIdBitCount());
    check(type==32,"wire event type is sticky projectile");
    Event decoded; check(decoded.deSerialize(&input),"actual event decoder succeeds");
    check(decoded.projectileId()==id,"wire roundtrip retains full 16-bit projectile ID");
    check(decoded.targetId()==source.targetId() && decoded.playerId()==255,"wire roundtrip retains target and owner");
    if(target) check(decoded.targetCount()==0 && decoded.childIndex()==0,"root target count and child index roundtrip");
    check(same(decoded.transform(),expected),"wire roundtrip retains transform row order");
    if(id) projectile->setAbsolutePosition(Vec3{-99,-98,-97});
    decoded.executeClient();
    check(projectile->activations==(id ? 1 : 0),"decoded event activates only nonzero registered projectile ID");
    check(projectile->getParent()==target,"decoded event applies host or absolute attachment");
    check(same(target ? projectile->getRelativeTransformation() : projectile->getAbsoluteTransformation(),expected),
          "actual sticky delegate applies decoded transform");
    io::BitStream truncated(packet.getBuffer(),packet.getSizeBytes()); truncated.setBitSize(bits-1);
    type=0; truncated.readBits(&type,gameEventTypeIdBitCount());
    Event incomplete; check(!incomplete.deSerialize(&truncated),"truncated transform packet is rejected");
    check(!truncated.hasError(),"decoder retains shipped error-clearing contract");
}
void collisionCase(Logic& logic,int sourceMaterial,int targetMaterial,bool destroy,bool expectStick,
                   bool parentBeforeUpdate=false) {
    auto* projectile=new Projectile(); projectile->setLocalPredict(true);
    Mat4 start=Mat4::getIdentity(); start.m[12]=3; start.m[13]=4; start.m[14]=5;
    projectile->setAbsoluteTransformation(start);
    const int contacts=logic.contacts;
    const bool handled=projectile->collision->handleCollision(0,Vec3{0,-1,0},Vec3{0,1,0},
        Vec3{0,-2,0},sourceMaterial,targetMaterial,-1,0,0);
    check(logic.contacts==contacts+1 && logic.sourceMaterial==sourceMaterial && logic.targetMaterial==targetMaterial,
          "collision policy observes unchanged source and target material arguments");
    const bool accepted=targetMaterial!=2;
    check(handled==!accepted,"target material alone controls scheduling on dry or water contact");
    check(same(projectile->getAbsoluteTransformation(),start),"accepted contact defers translation until update");
    Projectile* parent=0;
    if(parentBeforeUpdate) {
        parent=new Projectile(); parent->setAbsolutePosition(Vec3{20,30,40});
        parent->addChild(projectile); projectile->setAbsoluteTransformation(start);
        check(projectile->getParent()==parent,"actual object tree acquires a parent after collision");
    }
    projectile->setObjectInDestroyList(destroy);
    projectile->collision->updateComponent(0.02f,0);
    Mat4 expected=start; if(expectStick) expected.m[13]=2.05f;
    check(same(projectile->getAbsoluteTransformation(),expected),"component update applies only live dry attachment");
    projectile->collision->updateComponent(0.02f,0);
    check(same(projectile->getAbsoluteTransformation(),expected),"second component update does not accumulate push-out");
    if(parent) check(projectile->getParent()==parent,"local-predicted absolute stick preserves existing parent");
    if(destroy && accepted) {
        projectile->setObjectInDestroyList(false);
        projectile->collision->updateComponent(0.02f,0);
        expected.m[13]=2.05f;
        check(same(projectile->getAbsoluteTransformation(),expected),"destroy-list suppression retains pending attachment");
    }
}
}
int main() {
    g_system=new System(); eventManager=new EventManager(); classManager=new Classes();
    responsePhysicsManager=new ResponsePhysicsManager();
    auto* materials=new MaterialManager(); materialManager=materials;
    materials->setAttMaterial(0); materials->setAttMaterial(1); materials->setAttMaterial(2);
    materials->getMaterialPtr(2)->setHasWaterPhysics(true);
    objectTemplateManager=new ObjectTemplateManager(); objectManager=new ObjectManager();
    auto* logic=new Logic(); gameLogic=logic;
    io::networkManager=new io::NetworkManager(); playerManager=new PlayerManager();
    gameEventRegistry=new GameEventRegistry();
    gameEventRegistry->registerEventMaker(32,"StickyProjectile",new GameEventMakerMaker<StickyProjectileEvent>());
    auto* target=new Projectile(); registerProjectile(target,0x4567);
    target->setAbsolutePosition(Vec3{20,30,40});
    eventCase(0,0); eventCase(0x1234,0); eventCase(0xFFFF,target);
    collisionCase(*logic,1,1,false,true);
    collisionCase(*logic,2,1,false,true);
    collisionCase(*logic,1,2,false,false);
    collisionCase(*logic,1,1,true,false);
    collisionCase(*logic,1,1,false,true,true);
    std::printf("Sticky projectile: %d checks, %d failures\n",checks,failures);
    return failures ? 1 : 0;
}
