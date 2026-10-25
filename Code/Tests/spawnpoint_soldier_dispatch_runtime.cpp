// [TEMP] [CHOSEN, not recovered] Test lifetimes and typed collaborators only.
// The generated TU contains unchanged SpawnPoint::getActive and Soldier::getOccupingPlayer.
// No map, player session, engine initialization or real Soldier::enter runs here.
#include "spawnpoint_soldier_dispatch_runtime.h"

using namespace dice::hfe;
using namespace dice::hfe::world;

[[noreturn]] void spawnUnexpected(const char* method)
{
    std::fprintf(stderr, "UNEXPECTED collaborator: %s\n", method);
    std::fflush(stderr);
    std::abort();
}

static int checks = 0;
static std::string trace;
static void check(bool value, const char* description)
{
    ++checks;
    if (!value)
    {
        std::fprintf(stderr, "FAIL %s [trace=%s]\n", description, trace.c_str());
        std::exit(1);
    }
}

#include "spawnpoint_collaborators.inl"

// These lifetimes construct real C++ subobjects and containers without registering game objects.
// Only fields read by the two tested production bodies are initialized or exposed by observers.
namespace dice { namespace hfe {
IGameLogic* gameLogic = 0;
SprintState::SprintState() {}
SpawnPointTemplate::SpawnPointTemplate()
    : m_active(true), m_enterOnSpawn(false), m_aiEnterOnSpawn(false),
      m_minSpawnHeight(-1.0f), m_scatterSpawnPositions(false), m_onlyForHuman(false),
      m_onlyForAI(false), m_allowSpawnCloseToVehicle(false) {}
SpawnPointTemplate::~SpawnPointTemplate() {}
SpawnPoint::SpawnPoint(const SpawnPointTemplate* objectTemplate)
    : Object(objectTemplate), m_spawnTemplate(objectTemplate), m_active(true), m_bool156(false) {}
SpawnPoint::~SpawnPoint() {}
namespace world {
ObjectManager* objectManager = 0;
IBase* heightmapCluster = 0;
IBase* materialManager = 0; // Only the flag predicator is exercised; material predicates stay unused.
IObject::IObject() : m_template(0), m_parent(0), m_armorComponent(0), m_flags(0) {}
IObject::~IObject() {}
Object::Object(const IObjectTemplate* objectTemplate) { m_template = objectTemplate; }
Object::~Object() {}
SimpleObject::SimpleObject(const SimpleObjectTemplate*) : Object(0) {}
SimpleObject::~SimpleObject() {}
Bundle::Bundle(const BundleTemplate*) : SimpleObject(0) {}
Bundle::~Bundle() {}
Soldier::Soldier(const SoldierTemplate*) : Bundle(0), m_occupyingPlayerWeak(0) {}
Soldier::~Soldier() {}
IObjectTemplate::IObjectTemplate() : m_weakRef(0) {}
IObjectTemplate::~IObjectTemplate() {}
ObjectTemplate::ObjectTemplate() {}
ObjectTemplate::~ObjectTemplate() {}
ObjectManager::ObjectManager() {}
ObjectManager::~ObjectManager() {}
IPlayer::IPlayer() : m_weakControl(0) {}
IPlayer::~IPlayer() {}

// Root membership and entry lookup are collaborator boundaries, not implementations under test.
static std::map<const IObject*, IObject*> roots;
IObject* getRootParent(const IObject* object)
{
    std::map<const IObject*, IObject*>::const_iterator it = roots.find(object);
    check(it != roots.end(), "root query names a configured object");
    return it->second;
}
IPlayerControlObject* BFfindEntryPoint(const Vec3&, float, int, IPlayer*, bool, bool, bool)
{ spawnUnexpected("BFfindEntryPoint"); }
} } }

struct Occupant : ObservedPlayer
{
    bool alive = true;
    mutable int reads = 0;
    bool getIsAlive() const override { ++reads; trace += 'A'; return alive; }
};

struct SoldierTemplateTag : ObservedTemplate
{
    mutable int reads = 0;
    unsigned int getClassID() const override { ++reads; trace += 'C'; return CID_SoldierTemplate; }
};

// Test-only typed member access; preserve the real declaration and native layout.
template<class Tag, typename Tag::type Member>
struct PrivateMember { friend typename Tag::type access(Tag) { return Member; } };
struct OccupantBlockTag
{
    using type = void* Soldier::*;
    friend type access(OccupantBlockTag);
};
template struct PrivateMember<OccupantBlockTag, &Soldier::m_occupyingPlayerWeak>;

struct SoldierObserver : Soldier
{
    mutable int occupantReads = 0;
    explicit SoldierObserver(const IObjectTemplate* objectTemplate) : Soldier(0)
    { IObject::m_template = objectTemplate; }
    void occupant(WeakPtr<world::IPlayer>::Ctrl* block) { this->*access(OccupantBlockTag{}) = block; }
    world::IPlayer* getOccupingPlayer() const override
    { ++occupantReads; trace += 'O'; return Soldier::getOccupingPlayer(); }
    bool enter(world::IPlayer*) override { spawnUnexpected("Soldier::enter reached instead of occupancy getter"); }
};

struct SpawnObserver : SpawnPoint
{
    explicit SpawnObserver(const SpawnPointTemplate* objectTemplate) : SpawnPoint(objectTemplate) {}
    Vec3 position = {3.0f, 4.0f, 5.0f};
    int getSpawnGroupId() override { return 0; }
    bool getEnterOnSpawn() override { return false; }
    const Vec3& getAbsolutePosition() const override { return position; }
};

struct GridObserver : ObjectManager
{
    std::vector<IObject*> hits;
    int tempReads = 0, searches = 0;
    std::vector<IObject*>& getObjectTempVector() override
    { ++tempReads; trace += 'T'; return hits; }
    void getObjectsFromGrid(const Vec3& position, float radius, std::vector<IObject*>& out,
                            ObjectPredicator* predicator) override
    {
        ++searches; trace += 'G';
        check(position.x == 3.0f && position.y == 4.0f && position.z == 5.0f && radius == 1.0f,
              "occupancy grid uses marker position and one-unit radius");
        check(&out == &hits && predicator, "grid receives the fetched vector and filter");
    }
};

int main()
{
    SpawnPointTemplate objectTemplate;
    SpawnObserver spawn(&objectTemplate);
    SoldierTemplateTag soldierTemplate;
    SoldierObserver soldier(&soldierTemplate);
    Occupant player;
    WeakPtr<world::IPlayer>::Ctrl block = {1, &player};
    GridObserver grid;
    objectManager = &grid;
    grid.hits.push_back(&soldier);
    roots[&spawn] = &spawn;
    roots[&soldier] = &soldier;
    soldier.occupant(&block);

    check(!spawn.getActive(false, 1), "live occupied soldier rejects spawn");
    check(trace == "TGCOA", "occupancy calls the Soldier getter, then the alive query");
    check(soldier.occupantReads == 1 && player.reads == 1, "one occupancy and alive read");

    trace.clear(); player.alive = false;
    check(spawn.getActive(false, 1), "dead occupied soldier allows spawn");
    check(trace == "TGCOA", "dead occupant still follows the same query order");

    trace.clear(); soldier.occupant(0);
    check(spawn.getActive(false, 1), "unoccupied soldier allows spawn");
    check(trace == "TGCO" && player.reads == 2, "null weak block skips alive query");

    trace.clear(); block.target = 0; soldier.occupant(&block);
    check(spawn.getActive(false, 1), "expired occupant allows spawn");
    check(trace == "TGCO" && player.reads == 2, "expired weak target skips alive query");

    trace.clear(); roots[&soldier] = &spawn; block.target = &player; player.alive = true;
    check(spawn.getActive(false, 1), "own subtree is skipped before checking occupants");
    check(trace == "TG", "same-subtree bypass reads neither class nor occupant");

    trace.clear(); objectTemplate.m_scatterSpawnPositions = true;
    check(spawn.getActive(false, 1), "scatter mode bypasses occupancy");
    check(trace.empty() && grid.searches == 5 && grid.tempReads == 5,
          "scatter bypass never fetches or searches the grid");
    objectManager = 0;
    std::printf("spawnpoint soldier dispatch: %d checks, 0 failures\n", checks);
    return 0;
}
