// [TEMP] [CHOSEN, not recovered] Registered camera properties feed real GameServer
// entry, Soldier::onEnter and Camera transform bodies. Seat ownership and empty
// animation rigs are narrow collaborators; no map, deployment, client or socket runs.
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include "../BF2/Animation/BoneAnimation/AnimationSystem.h"
#include "../BF2/Animation/BoneAnimation/Skeleton.h"
#include "../BF2/Core/ClassManager.h"
#include "../BF2/Core/ConsoleObjectMacro.h"
#include "../BF2/Core/ConsoleObjects.h"
#include "../BF2/Core/EventManager.h"
#include "../BF2/Core/Ids.h"
#include "../BF2/Core/Modules.h"
#include "../BF2/Core/SettingsRepostitory.h"
#include "../BF2/Core/System.h"
#include "../BF2/Game/GameServer/GameServer.h"
#include "../BF2/Game/GameLogic/Player.h"
#include "../BF2/Game/GameLogic/GameLogic.h"
#include "../BF2/Game/Objects/Base/Camera.h"
#include "../BF2/Game/Objects/Game/Parachute.h"
#include "../BF2/Game/Objects/Game/Soldier.h"
#include "../BF2/Game/Objects/Game/SoldierTemplate.h"
#include "../BF2/Game/Objects/TemplateLeaves.h"
#include "../BF2/IO/Console/Console.h"
#include "../BF2/Physics/ResponsePhysicsManager.h"
#include "../BF2/Sound/BfSound.h"
#include "../BF2/Scene/Object/BinTreeCuller.h"
#include "../BF2/Scene/Object/QuadTreeCuller.h"
#include "../BF2/Scene/Object/ObjectManager.h"
#include "../BF2/Scene/Object/ObjectTypes/Base/ObjectTemplateManager.h"
namespace dice { namespace hfe { extern ClassManager* classManager;
namespace world { extern ResponsePhysicsManager* responsePhysicsManager; } } }
using namespace dice::hfe;
using namespace dice::hfe::world;
namespace {
int checks = 0, failures = 0;
void check(bool ok, const char* label) {
    ++checks; if (!ok) ++failures;
    std::printf("%s %s\n", ok ? "PASS" : "FAIL", label); std::fflush(stdout);
}
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
        std::fprintf(stderr, "Unexpected fixture factory: %u\n", cid); std::abort();
    }
    bool getClassName(unsigned int cid, std::string& name) override {
        for (const auto& entry : entries) if (entry.classId == cid) { name = entry.name; return true; }
        std::abort();
    }
};
[[noreturn]] void unexpected() { std::abort(); }
// Concrete owner for the real GameLogic object registry used by Soldier construction.
// Unrelated abstract game operations stop if this fixture accidentally reaches them.
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
class Pilot : public Player {
public:
    IObject* current;
    IObject* body;
    explicit Pilot(Soldier* soldier) : Player(7, 0), current(soldier), body(soldier) {}
    IObject* getVehicle() const override { return current; }
    IObject* getDefaultVehicle() override { return body; }
    bool setVehicle(IObject* object, int) override { current = object; return true; }
};
// Model only the seat handoff, bypassing unrelated parachute physics, animation,
// weapon and AI setup. The caller, outgoing Soldier and actual Camera remain real.
class Seat : public Parachute {
    Pilot* occupant = 0;
    Soldier* body;
    Camera* camera;
public:
    int entries = 0;
    Seat(ParachuteTemplate* settings, Soldier* soldier, Camera* view)
        : Parachute(settings), body(soldier), camera(view) {}
    bool enter(world::IPlayer* player) override {
        ++entries; occupant = static_cast<Pilot*>(player);
        return player->setVehicle(this, 0);
    }
    world::IPlayer* getOccupingPlayer() const override { return occupant; }
    IObject* getOccupingObject() const override { return occupant ? body : 0; }
    ICameraObject* getActiveCamera() override { return camera; }
};
ConsoleObject* binding(const char* name) {
    ConsoleObject* found = 0;
    for (ConsoleObject* entry : consoleBindingList("world.objTemplGame"))
        if (std::strcmp(entry->getFunctionName(), name) == 0) {
            check(!found, "camera property has one registration"); found = entry;
        }
    return found;
}
std::string command(const std::string& text) {
    std::string result;
    io::mainConsole->handleCommand(text, result, static_cast<AccessType>(7));
    return result;
}
void entryCase(GameServer& server, ParachuteTemplate* settings, float pitch, bool preserve) {
    check(command(std::string("ObjectTemplate.maintainCameraOnEnter ") + (preserve ? "1" : "0")).empty(),
          "entry flag is authored through real console parsing");
    auto* soldier = new Soldier(new SoldierSettings());
    auto* camera = new Camera(new CameraTemplate());
    (*soldier->getOwnCameras())[0] = camera;
    soldier->addChild(camera);
    soldier->setIsSkyDiving(true);
    soldier->setLookUpDownAngle(pitch);
    const Mat4 before = camera->getRelativeTransformation();
    auto* pilot = new Pilot(soldier);
    auto* seat = new Seat(settings, soldier, camera);
    check(server.enterVehicle(pilot, seat, false), "actual server entry succeeds through the seat collaborator");
    check(seat->entries == 1 && pilot->getVehicle() == seat && soldier->getUsedVehicle() == seat
          && !soldier->getIsSkyDiving(), "actual Soldier onEnter stores vehicle and clears skydiving");
    float z, x, y;
    getRotationZXY(camera->getRelativeTransformation(), z, x, y);
    const float observedPitch = -x;
    check(std::fabs(observedPitch - (preserve ? pitch : 0.0f)) < 0.001f,
          "server entry preserves authored pitch only when the template requests it");
    if (preserve) {
        check(std::fabs(camera->getCurrentRotation().y - pitch) < 0.001f,
              "actual rotational camera state retains the outgoing pitch");
        bool same = true;
        const Mat4& after = camera->getRelativeTransformation();
        for (int i = 0; i < 12; ++i) same &= std::fabs(before.m[i] - after.m[i]) < 0.00001f;
        check(same, "camera orientation is continuous across entry");
    }
    std::printf("entry pitch=%g preserve=%d observed=%g\n", pitch, preserve, observedPitch);
}
}
int main() {
    ConsoleObject* enter = binding("maintainCameraOnEnter");
    ConsoleObject* exit = binding("maintainCameraOnExit");
    check(enter && exit, "actual module registers both camera preservation properties");
    ConsoleObject* neighbour = binding("seatInformation");
    check(neighbour && neighbour->getAccess() == 2 && neighbour->getCategory() == 9,
          "adjacent seat property keeps its existing access and category");
    for (ConsoleObject* property : {enter, exit}) if (property) {
        check(property->getAccess() == 2 && property->getCategory() == 15,
              "camera property preserves retail access and category");
        check(property->getType() == 1 && property->getMinNoArgs() == 0
              && property->getMaxNoArgs() == 1 && property->isDynamic() && !property->hasRange()
              && std::strcmp(property->getReturnType(), "bool") == 0
              && std::strcmp(property->getArgType(0), "bool") == 0,
              "camera property uses dynamic zero-or-one bool arguments without a range");
    }
    g_system = new System(); eventManager = new EventManager(); classManager = new Classes();
    responsePhysicsManager = new ResponsePhysicsManager(); g_bfSound = new BfSoundNull();
    io::mainConsole = new io::OldConsole(); objectTemplateManager = new ObjectTemplateManager();
    objectManager = new ObjectManager();
    gameLogic = new Logic();
    settings = new SettingsRepostitory();
    settings->boolSet("GSDeveloper", false);
    mainConsoleObjects->registerConsoleObjects(consoleBindingList("world.objTemplGame"));
    auto* created = objectTemplateManager->createTemplate(CID_ParachuteTemplate, "aim_parachute_fixture");
    check(created != 0, "registered factory creates a real parachute template");
    if (!created) return 1;
    auto* chute = static_cast<ParachuteTemplate*>(created);
    check(!chute->getMaintainCameraOnEnter() && !chute->getMaintainCameraOnExit(),
          "new template starts with both flags disabled");
    check(command("ObjectTemplate.maintainCameraOnExit 1").empty()
          && chute->getMaintainCameraOnExit() && !chute->getMaintainCameraOnEnter(),
          "exit flag is independently authorable");
    check(command("ObjectTemplate.maintainCameraOnEnter 1").empty()
          && chute->getMaintainCameraOnEnter() && chute->getMaintainCameraOnExit(),
          "stock enter and exit commands both take effect");
    check(command("ObjectTemplate.maintainCameraOnEnter") == "1"
          && command("ObjectTemplate.maintainCameraOnExit") == "1", "bool queries read the actual flags");
    check(command("ObjectTemplate.maintainCameraOnEnter 1 0") == "Properties only takes one argument!",
          "extra property arguments are rejected");
    objectTemplateManager->createTemplate(CID_SimpleObjectTemplate, "aim_wrong_template_fixture");
    check(command("ObjectTemplate.maintainCameraOnEnter 0") == "Unknown object or method!"
          && command("ObjectTemplate.maintainCameraOnExit 0") == "Unknown object or method!",
          "unrelated active templates refuse both camera commands");
    objectTemplateManager->setActiveObjectTemplate(chute);
    check(chute->getMaintainCameraOnEnter() && chute->getMaintainCameraOnExit(),
          "wrong-template commands preserve the parachute flags");
    auto* server = new GameServer(); game = server;
    entryCase(*server, chute, 60.0f, true);
    entryCase(*server, chute, -35.0f, true);
    entryCase(*server, chute, 60.0f, false);
    check(command("ObjectTemplate.maintainCameraOnExit 0").empty()
          && !chute->getMaintainCameraOnExit(), "exit preservation can also be disabled");
    std::printf("Parachute aim: %d checks, %d failures\n", checks, failures);
    // Objects live until process exit; no production lifecycle or joined-client claim.
    return failures ? 1 : 0;
}
