// [TEMP] [CHOSEN, not recovered] Native target-component regression.
// Uses actual engine objects and the complete LPTargetComp TU. Spies observe only
// template lookup, network object creation, and queued messages; no server starts.
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include "../BF2/Core/ClassManager.h"
#include "../BF2/Core/EventManager.h"
#include "../BF2/Core/Ids.h"
#include "../BF2/Core/System.h"
#include "../BF2/Game/GameLogic/GameLogic.h"
#include "../BF2/Game/GameLogic/Player.h"
#include "../BF2/Game/GameServer/GameServer.h"
#include "../BF2/Game/Objects/GenericFireArmTemplate.h"
#include "../BF2/Game/Objects/TargetObjectTemplate.h"
#include "../BF2/Game/Objects/Game/TargetObject.h"
#include "../BF2/Game/Objects/Weapons/GenericFireArm.h"
#include "../BF2/Game/Objects/Weapons/TargetComponents/LPTargetComp.h"
#include "../BF2/Physics/ResponsePhysicsManager.h"
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
void check(bool ok, const char* label)
{
    ++checks;
    if (!ok) ++failures;
    std::printf("%s %s\n", ok ? "PASS" : "FAIL", label);
    std::fflush(stdout);
}
[[noreturn]] void unexpected(const char* label)
{
    std::fprintf(stderr, "Unexpected fixture collaborator: %s\n", label);
    std::abort();
}
class Classes : public ClassManager
{
    IBase* createInstance(unsigned int cid, unsigned int iid, IBase* outer) override
    {
        if (cid == CID_BinTreeCuller) return BinTreeCuller::create(iid, outer);
        if (cid == CID_QuadTreeCuller) return QuadTreeCuller::create(iid, outer);
        unexpected("class factory");
    }
};
class Templates : public ObjectTemplateManager
{
public:
    IObjectTemplate* current = 0;
    IObjectTemplate* getTemplate(const std::string& name) const override
    {
        if (name == "lp_fixture_target") return current;
        if (name == "MultiPlayerFreeCamera") return 0; // Explicitly absent in this fixture.
        unexpected("template lookup");
    }
};
class TargetTemplate : public TargetObjectTemplate
{
public:
    mutable int creations = 0;
    TargetTemplate(int type, float lifetime, int uvIndex)
    {
        targetType = type;
        setTimeToLive(lifetime);
        // Unrelated native fields deliberately disagree with the target configuration.
        m_animatedUVTranslationIndex = uvIndex;
        m_animatedUVTranslationReverse = true; // Stale TTL low byte cannot equal 7.25f.
        m_animatedUVRotation = false;
    }
    IObject* createObject() const override
    {
        ++creations;
        return TargetObjectTemplate::createObject();
    }
};
class Objects : public ObjectManager
{
public:
    struct Message { IObject* object; TemplateMessage id; float delay; };
    std::vector<Message> messages;
    void postMessage(IObject* object, TemplateMessage id, float delay) override
    {
        messages.push_back(Message{object, id, delay});
    }
};
class Logic : public GameLogic
{
public:
    Logic() { m_targetObjectIdCounter = 0; }
    bool isServer() const override { return true; }
    // Cold abstract game-policy operations must never be reached by this component fixture.
    void addRef() override { unexpected("Logic::addRef"); }
    int getRef() override { unexpected("Logic::getRef"); }
    int release() override { unexpected("Logic::release"); }
    void* queryInterface(unsigned int) const override { unexpected("Logic::queryInterface"); }
    void runNextLevel() override { unexpected("Logic::runNextLevel"); }
    void setNextLevel(int) override { unexpected("Logic::setNextLevel"); }
    bool selectKitAndUnlockLevel(IPlayer*, int, int) override { unexpected("Logic::selectKitAndUnlockLevel"); }
    void setTicketState(unsigned int, int) override { unexpected("Logic::setTicketState"); }
    int getTicketState(unsigned int) override { unexpected("Logic::getTicketState"); }
    void setDefaultNumberOfTickets(unsigned int, unsigned int) override { unexpected("Logic::setDefaultNumberOfTickets"); }
    unsigned int getDefaultNumberOfTickets(unsigned int) override { unexpected("Logic::getDefaultNumberOfTickets"); }
    void setDefaultNumberOfTicketsEx(unsigned int, unsigned int, unsigned int) override { unexpected("Logic::setDefaultNumberOfTicketsEx"); }
    unsigned int getDefaultNumberOfTicketsEx(unsigned int, unsigned int) override { unexpected("Logic::getDefaultNumberOfTicketsEx"); }
    void setDefaultTimeToNextAIWave(float) override { unexpected("Logic::setDefaultTimeToNextAIWave"); }
    float getDefaultTimeToNextAIWave() override { unexpected("Logic::getDefaultTimeToNextAIWave"); }
    void setTicketLossAtEndPerMin(float) override { unexpected("Logic::setTicketLossAtEndPerMin"); }
    float getTicketLossAtEndPerMin() override { unexpected("Logic::getTicketLossAtEndPerMin"); }
    void setTicketLossPerMin(unsigned int, float) override { unexpected("Logic::setTicketLossPerMin"); }
    float getTicketLossPerMin(unsigned int) override { unexpected("Logic::getTicketLossPerMin"); }
    void playEffect(Vec3, const IObjectTemplate*, Vec3) override { unexpected("Logic::playEffect"); }
    bool handleCollisionForProjectile(IObject*, IObject*, const Vec3&, const Vec3&, const Vec3&, int, int, CollisionFaceInfo*, CollisionFaceInfo*) override { unexpected("Logic::handleCollisionForProjectile"); }
    bool handleCollisionForReplenisher(bool, IObject*, IObject*, const Vec3&, const Vec3&, const Vec3&, int, int) override { unexpected("Logic::handleCollisionForReplenisher"); }
    bool handleCollisionForResurrecter(IObject*, IObject*, float, float, const Vec3&, const Vec3&, const Vec3&, int, int) override { unexpected("Logic::handleCollisionForResurrecter"); }
    void handleCollision(IObject*, IObject*, const Vec3&, const Vec3&, const Vec3&, int, int, CollisionFaceInfo*, CollisionFaceInfo*) override { unexpected("Logic::handleCollision"); }
    void spawnMultipleObjects(const IObjectTemplate*, int) override { unexpected("Logic::spawnMultipleObjects"); }
    IObject* spawnObject(const std::string&, Vec3, Vec3, int) override { unexpected("Logic::spawnObject(name)"); }
    IObject* spawnObject(const IObjectTemplate*, Vec3, Vec3, int) override { unexpected("Logic::spawnObject(template)"); }
    void checkPlayerTriggers(IPlayer*, IObject*, io::PlayerInput, bool) override { unexpected("Logic::checkPlayerTriggers"); }
    void toggleLocalFreeCamera() override { unexpected("Logic::toggleLocalFreeCamera"); }
    TicketHistory& getTicketHistory() override { unexpected("Logic::getTicketHistory"); }
    void setTicketHistory(const TicketHistory&) override { unexpected("Logic::setTicketHistory"); }
};
class Server : public GameServer
{
public:
    int creations = 0;
    IObject* createObjectOnAllClients(const IObjectTemplate* objectTemplate,
                                     Vec3 position, Vec3 rotation,
                                     bool flag, int recipient, int team) override
    {
        ++creations;
        check(position.x == 0 && position.y == 0 && position.z == 0
              && rotation.x == 0 && rotation.y == 0 && rotation.z == 0
              && flag && recipient == 0 && team == 0,
              "network creation preserves the original arguments");
        // This boundary returns a real target; replication/lifecycle are outside this test.
        return objectTemplate->createObject();
    }
};
class Gun : public GenericFireArm
{
public:
    IPlayer* player = 0;
    explicit Gun(const GenericFireArmTemplate* objectTemplate) : GenericFireArm(objectTemplate) {}
    IPlayer* getPlayer() override { return player; }
};
}

int main()
{
    g_system = new System();
    eventManager = new EventManager();
    classManager = new Classes();
    responsePhysicsManager = new ResponsePhysicsManager();
    auto* templates = new Templates();
    objectTemplateManager = templates;
    auto* objects = new Objects();
    objectManager = objects;
    gameLogic = new Logic();
    auto* server = new Server();
    game = server;
    auto* gun = new Gun(new GenericFireArmTemplate());
    gun->player = new Player(37, 0);

    const int types[] = {3, 2};
    for (int type : types)
    {
        auto* targetTemplate = new TargetTemplate(type, 7.25f, type == 3 ? 0 : 3);
        templates->current = targetTemplate;
        auto* componentTemplate = new LPTargetCompTemplate();
        componentTemplate->setTargetObjectTemplate("lp_fixture_target");
        componentTemplate->setTargetSystem(TSTVGuided);
        componentTemplate->setSendVehicleLockWarning(false);
        auto* component = new LPTargetComp(componentTemplate);
        component->setObject(gun);
        component->init(gun);
        const int beforeCreations = server->creations;
        objects->messages.clear();
        WeaponInitEvent event = {0, 0};
        component->initComponent(event);
        component->enable();
        auto* target = static_cast<TargetObject*>(component->getTargetObject());
        check(target != 0 && targetTemplate->creations == 1,
              "each path constructs exactly one actual target");
        if (!target) return 1;
        check(server->creations - beforeCreations == (type == 3 ? 0 : 1),
              "target type alone selects private versus network creation");
        check(type == 3 ? objects->messages.size() == 1
                            && objects->messages[0].object == target
                            && objects->messages[0].id == static_cast<TemplateMessage>(29)
                            && objects->messages[0].delay == 0.0f
                       : objects->messages.empty(),
              "private creation executes actual TargetObject::init");

        // enable() already has a correct typed attribution write. Reset it so that
        // the separate enableTargetObject path cannot pass by retaining that value.
        target->setTargetPlayerId(-77);
        const Mat4 beforeTransform = target->getAbsoluteTransformation();
        component->setTargetObject(target, true, 0);
        check(target->getTargetActive(), "lock activates the actual target");
        check(target->getTargetPlayerId() == 37, "lock stamps the actual native player-id field");
        bool sameTransform = true;
        for (int i = 0; i != 16; ++i)
            sameTransform = sameTransform && beforeTransform.m[i] == target->getAbsoluteTransformation().m[i];
        check(sameTransform, "attribution preserves the object transformation");

        objects->messages.clear();
        componentTemplate->setTargetSystem(TSLaserGuided);
        component->fireStop(false);
        check(objects->messages.size() == 1 && objects->messages[0].object == target
              && objects->messages[0].id == static_cast<TemplateMessage>(30)
              && objects->messages[0].delay == 7.25f,
              "laser stop posts message 30 with the actual template lifetime");
        objects->messages.clear();
        componentTemplate->setTargetSystem(TSTVGuided);
        component->fireStop(false);
        componentTemplate->setTargetSystem(TSWireGuided);
        component->fireStop(true);
        check(objects->messages.empty(), "TV and wire-guided stop bypass lifetime messages");
        component->setTargetObject(0, false, 0);
        componentTemplate->setTargetSystem(TSLaserGuided);
        component->fireStop(false);
        check(objects->messages.empty(), "absent laser target posts no message");
    }
    std::printf("LP target native fixture: %d checks, %d failures\n", checks, failures);
    // Test-owned objects survive to process exit; no game lifecycle claim.
    return failures ? 1 : 0;
}
