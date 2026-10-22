// [TEMP] [CHOSEN, not recovered] Real ammo, weapon, soldier and replenishment bodies.
// Empty animation rigs and an unobstructed ray are fixture collaborators. This tests
// bag eligibility and refill behavior, not proximity scanning or a joined session.
#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>
#include <vector>
#include "../BF2/Animation/BoneAnimation/AnimationSystem.h"
#include "../BF2/Animation/BoneAnimation/Skeleton.h"
#include "../BF2/Core/ClassManager.h"
#include "../BF2/Core/EventManager.h"
#include "../BF2/Core/Ids.h"
#include "../BF2/Core/System.h"
#include "../BF2/Game/GameLogic/GameLogic.h"
#include "../BF2/IO/Input/PlayerInput.h"
#include "../BF2/Game/Objects/Game/Soldier.h"
#include "../BF2/Game/Objects/Game/SoldierTemplate.h"
#include "../BF2/Game/Objects/GenericFireArmTemplate.h"
#include "../BF2/Game/Objects/DefaultAmmoCompTemplate.h"
#include "../BF2/Game/Objects/Weapons/AmmoComponents/DefaultAmmoComp.h"
#include "../BF2/Game/Objects/Weapons/AmmoComponents/ReplenishingAmmoComp.h"
// Seed only the weapon's cached component pointer; no production body is replaced.
#define private public
#include "../BF2/Game/Objects/Weapons/GenericFireArm.h"
#undef private
#include "../BF2/Game/Objects/Projectiles/GenericProjectile.h"
#include "../BF2/Game/Objects/Projectiles/GenericProjectileTemplate.h"
#include "../BF2/Game/Objects/Projectiles/ProjectileCompTemplates.h"
#include "../BF2/Game/Objects/Projectiles/DetonationComponents/ReplenishDetonationComp.h"
#include "../BF2/Game/Objects/GameLogicComp/ArmorGLComp.h"
#include "../BF2/Game/Objects/GameLogicComp/ArmorGLCompTemplate.h"
#include "../BF2/Physics/MaterialManager.h"
#include "../BF2/Physics/PhysicsNodeManager.h"
#include "../BF2/Physics/PointPhysicsNode.h"
#include "../BF2/Physics/ResponsePhysicsManager.h"
#include "../BF2/Scene/Object/BinTreeCuller.h"
#include "../BF2/Scene/Object/QuadTreeCuller.h"
#include "../BF2/Scene/Object/ObjectManager.h"
#include "../BF2/Scene/Object/ObjectTypes/Base/ObjectTemplateManager.h"
namespace dice { namespace hfe { extern ClassManager* classManager;
namespace world { extern ResponsePhysicsManager* responsePhysicsManager;
extern PhysicsNodeManager* physicsNodeManager;
extern IBase* materialManager; } } }
using namespace dice::hfe;
using namespace dice::hfe::world;
namespace {
int checks = 0, failures = 0;
void check(bool ok, const char* label) {
    ++checks; if (!ok) ++failures;
    std::printf("%s %s\n", ok ? "PASS" : "FAIL", label); std::fflush(stdout);
}
[[noreturn]] void unexpected(const char* label) {
    std::fprintf(stderr, "Unexpected fixture collaborator: %s\n", label); std::abort();
}
class Classes : public ClassManager {
    IBase* createInstance(unsigned int cid, unsigned int iid, IBase* outer) override {
        if (cid == CID_BinTreeCuller) return BinTreeCuller::create(iid, outer);
        if (cid == CID_QuadTreeCuller) return QuadTreeCuller::create(iid, outer);
        unexpected("class factory");
    }
};
class Objects : public ObjectManager {
public:
    int rays = 0;
    bool intersectLine(IObject*& hit, Vec3&, Vec3&, int&, const Vec3&, const Vec3&,
                       ObjectPredicator*, bool, bool, bool, bool, unsigned int) override {
        ++rays; hit = 0; return false;
    }
};
class Logic : public GameLogic {
public:
    Logic() { m_isServer = true; m_gameStatus = 1; }
    void addRef() override { unexpected("addRef"); }
    int getRef() override { unexpected("getRef"); }
    int release() override { unexpected("release"); }
    void* queryInterface(unsigned int) const override { unexpected("queryInterface"); }
    void runNextLevel() override { unexpected("runNextLevel"); }
    void setNextLevel(int) override { unexpected("setNextLevel"); }
    bool selectKitAndUnlockLevel(world::IPlayer*,int,int) override { unexpected("selectKit"); }
    void setTicketState(unsigned int,int) override { unexpected("setTicketState"); }
    int getTicketState(unsigned int) override { unexpected("getTicketState"); }
    void setDefaultNumberOfTickets(unsigned int,unsigned int) override { unexpected("setTickets"); }
    unsigned int getDefaultNumberOfTickets(unsigned int) override { unexpected("getTickets"); }
    void setDefaultNumberOfTicketsEx(unsigned int,unsigned int,unsigned int) override { unexpected("setTicketsEx"); }
    unsigned int getDefaultNumberOfTicketsEx(unsigned int,unsigned int) override { unexpected("getTicketsEx"); }
    void setDefaultTimeToNextAIWave(float) override { unexpected("setAIWave"); }
    float getDefaultTimeToNextAIWave() override { unexpected("getAIWave"); }
    void setTicketLossAtEndPerMin(float) override { unexpected("setTicketLossAtEnd"); }
    float getTicketLossAtEndPerMin() override { unexpected("getTicketLossAtEnd"); }
    void setTicketLossPerMin(unsigned int,float) override { unexpected("setTicketLoss"); }
    float getTicketLossPerMin(unsigned int) override { unexpected("getTicketLoss"); }
    void playEffect(Vec3,const IObjectTemplate*,Vec3) override { unexpected("playEffect"); }
    bool handleCollisionForProjectile(IObject*,IObject*,const Vec3&,const Vec3&,const Vec3&,int,int,CollisionFaceInfo*,CollisionFaceInfo*) override { unexpected("projectile collision"); }
    bool handleCollisionForReplenisher(bool,IObject*,IObject*,const Vec3&,const Vec3&,const Vec3&,int,int) override { unexpected("replenisher collision"); }
    bool handleCollisionForResurrecter(IObject*,IObject*,float,float,const Vec3&,const Vec3&,const Vec3&,int,int) override { unexpected("resurrecter collision"); }
    void handleCollision(IObject*,IObject*,const Vec3&,const Vec3&,const Vec3&,int,int,CollisionFaceInfo*,CollisionFaceInfo*) override { unexpected("collision"); }
    void spawnMultipleObjects(const IObjectTemplate*,int) override { unexpected("spawnMultipleObjects"); }
    IObject* spawnObject(const std::string&,Vec3,Vec3,int) override { unexpected("spawnObject name"); }
    IObject* spawnObject(const IObjectTemplate*,Vec3,Vec3,int) override { unexpected("spawnObject template"); }
    void checkPlayerTriggers(world::IPlayer*,IObject*,io::PlayerInput,bool) override { unexpected("player triggers"); }
    void toggleLocalFreeCamera() override { unexpected("free camera"); }
    TicketHistory& getTicketHistory() override { unexpected("getTicketHistory"); }
    void setTicketHistory(const TicketHistory&) override { unexpected("setTicketHistory"); }
};
class EmptyAnimation : public dice::anim::AnimationSystemTemplate {
    dice::anim::AnimationSystem* makeInstance() override { return 0; }
};
class SoldierSettings : public SoldierTemplate {
public:
    SoldierSettings() {
        m_skeleton1P = new dice::anim::Skeleton();
        m_skeleton3P = new dice::anim::Skeleton();
        m_animationSystem1P = new EmptyAnimation();
        m_animationSystem3P = new EmptyAnimation();
    }
};
class Bag : public ReplenishDetonationComp {
public:
    explicit Bag(ReplenishDetonationCompTemplate* settings) : ReplenishDetonationComp(settings) {}
    using ReplenishDetonationComp::checkIfObjectCanTrigger;
};
GenericFireArm* gun(AmmoComp* ammo, GenericFireArmTemplate* settings = 0) {
    auto* result = new GenericFireArm(settings ? settings : new GenericFireArmTemplate());
    ammo->setObject(result); ammo->init(result);
    result->m_ammoComp = static_cast<IWeaponAmmoComp*>(ammo);
    return result;
}
DefaultAmmoComp* normalAmmo(int mags, int rounds) {
    auto* settings = new DefaultAmmoCompTemplate();
    settings->setNrOfMags(mags); settings->setMagSize(rounds);
    return new DefaultAmmoComp(settings);
}
}
int main() {
    g_system = new System(); eventManager = new EventManager(); classManager = new Classes();
    responsePhysicsManager = new ResponsePhysicsManager();
    auto* materials = new MaterialManager(); materialManager = materials;
    materials->setAttMaterial(0); materials->setDefaultDamage(1.0f);
    objectTemplateManager = new ObjectTemplateManager();
    auto* objects = new Objects(); objectManager = objects;
    physicsNodeManager = new PhysicsNodeManager();
    auto* logic = new Logic(); gameLogic = logic;
    auto* soldier = new Soldier(new SoldierSettings());
    auto* node = new PointPhysicsNode(); node->setObject(soldier);
    check(soldier->setComponent(IID_IPhysicsNode, node), "attach genuine physics component");
    auto* armor = new ArmorGLComp(new ArmorGLCompTemplate()); armor->setObject(soldier);
    check(soldier->setComponent(IID_IArmorGLComp, armor), "attach genuine armor component");
    auto& weapons = *static_cast<std::vector<IObject*>*>(soldier->getWeapons(0));
    auto& allWeapons = *static_cast<std::vector<IObject*>*>(soldier->getWeapons());
    auto* rifleAmmo = normalAmmo(4, 30);
    auto* rifle = gun(rifleAmmo); weapons.push_back(rifle); allWeapons.push_back(rifle);
    auto* pistol = gun(normalAmmo(3, 15)); weapons.push_back(pistol); allWeapons.push_back(pistol);
    auto* toolSettings = new ReplenishingAmmoCompTemplate();
    toolSettings->setNrOfMags(9); toolSettings->setMagSize(1);
    auto* toolAmmo = new ReplenishingAmmoComp(toolSettings);
    auto* tool = gun(toolAmmo); weapons.push_back(tool); allWeapons.push_back(tool);
    auto* infinite = gun(normalAmmo(1, -1)); weapons.push_back(infinite); allWeapons.push_back(infinite);
    auto* bagSettings = new ReplenishDetonationCompTemplate();
    bagSettings->setReplenishingType(RTAmmo); bagSettings->setReplenishingStrength(100.0f);
    bagSettings->setTriggerType(1); bagSettings->setTriggerVictimMinSpeed(0.0f);
    auto* projectile = new GenericProjectile(new GenericProjectileTemplate());
    auto* bag = new Bag(bagSettings); bag->setObject(projectile); bag->init(projectile);

    check(rifle->canBeRefilled() && pistol->canBeRefilled(), "normal weapons inherit refill eligibility");
    check(!tool->canBeRefilled(), "replenishing support tool retains its refusal override");
    check(infinite->canBeRefilled() && infinite->getMagSize() == -1,
          "infinite-ammo weapon is excluded by magazine size, not interface eligibility");
    auto verify = [&](int expectedStatus, bool expectedPickup, const char* label) {
        const std::vector<int> before = *rifleAmmo->getAmmo();
        const int status = soldier->calcAmmoStatus();
        // Mirrors the final assignment in Soldier::handleUpdate without movement/animation.
        soldier->setAmmoStatus(status);
        check(status == expectedStatus, label);
        check(logic->replenishAmmo(soldier, 100.0f, 0, 0, 0, 0, true) == expectedPickup,
              "actual GameLogic query agrees with loadout capacity");
        check(bag->checkIfObjectCanTrigger(soldier) == expectedPickup,
              "actual replenishment trigger agrees with loadout capacity");
        check(*rifleAmmo->getAmmo() == before, "eligibility queries preserve magazine contents");
    };
    verify(15, false, "full ordinary loadout remains full with support tool and infinite weapon");
    rifleAmmo->getAmmo()->pop_back();
    verify(12, true, "one missing magazine lowers soldier gauge");
    check(logic->replenishAmmo(soldier, 100.0f, 0, 0, 0, 0, false),
          "actual apply path replenishes a missing magazine");
    check(rifleAmmo->getAmmo()->size() == 4, "missing magazine is restored by actual ammo component");
    verify(15, false, "restored loadout refuses another bag");
    (*rifleAmmo->getAmmo())[0] = 0;
    verify(12, true, "empty loaded magazine counts as missing capacity");
    (*rifleAmmo->getAmmo())[0] = 29;
    verify(15, false, "part-used loaded magazine preserves retail magazine-count gauge");
    auto* empty = new GenericFireArm(new GenericFireArmTemplate());
    check(!empty->canBeRefilled(), "weapon without an ammo component stays non-refillable");
    check(objects->rays == 5, "every bag candidate executes the real base trigger before ammo query");
    std::printf("Ammo pickup: %d checks, %d failures\n", checks, failures);
    // Test-owned objects survive until process exit. No map or network lifecycle is claimed.
    return failures ? 1 : 0;
}
