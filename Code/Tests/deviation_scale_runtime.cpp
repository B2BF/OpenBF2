// [TEMP] [CHOSEN, not recovered] Exercises the actual DeviationComp postFire body.
// Only game mode/input index and an optional changing weapon velocity are controlled collaborators.
// Real projectile transforms and PointPhysicsNode velocity storage remain in use.
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include "../BF2/Core/ClassManager.h"
#include "../BF2/Core/EventManager.h"
#include "../BF2/Core/Ids.h"
#include "../BF2/Core/System.h"
#include "../BF2/Game/GameServer/GameServer.h"
#include "../BF2/Game/Objects/GenericFireArmTemplate.h"
#include "../BF2/Game/Objects/Projectiles/GenericProjectile.h"
#include "../BF2/Game/Objects/Projectiles/GenericProjectileTemplate.h"
#include "../BF2/Game/Objects/Weapons/GenericFireArm.h"
#include "../BF2/Game/Objects/Weapons/DeviationComponents/SimpleDeviationComp.h"
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
bool same(const Vec3& a, const Vec3& b)
{
    return std::fabs(a.x-b.x) < 0.0001f && std::fabs(a.y-b.y) < 0.0001f
        && std::fabs(a.z-b.z) < 0.0001f;
}
Vec3 difference(const Vec3& a, const Vec3& b)
{
    return Vec3{a.x-b.x, a.y-b.y, a.z-b.z};
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
class Server : public GameServer
{
    const std::string mode = "gpm_cq";
public:
    int getCurrentInputIndex() override { return 137; }
    const std::string& getCurrentMapGameMode() const override { return mode; }
};
class Gun : public GenericFireArm
{
public:
    mutable int velocityCalls = 0;
    int devModCalls = 0;
    float secondVelocity = -1.0f;
    GenericFireArmTemplate* settings;
    explicit Gun(GenericFireArmTemplate* objectTemplate)
        : GenericFireArm(objectTemplate), settings(objectTemplate) {}
    float getVelocity() const override
    {
        if (velocityCalls >= 2) unexpected("extra getVelocity call");
        ++velocityCalls;
        if (velocityCalls == 2 && secondVelocity >= 0) return secondVelocity;
        return GenericFireArm::getVelocity();
    }
    float getDevMod() override
    {
        ++devModCalls;
        return GenericFireArm::getDevMod();
    }
};
class Component : public SimpleDeviationComp
{
public:
    explicit Component(SimpleDeviationCompTemplate* objectTemplate)
        : SimpleDeviationComp(objectTemplate) {}
    void prepare(float deviation, float cachedX, float cachedY)
    {
        m_deviation = deviation;
        m_cachedDeviationX = cachedX;
        m_cachedDeviationY = cachedY;
        m_fireDeviation = 0.0f;
    }
    Vec3 cache() const { return Vec3{m_cachedDeviationX, m_cachedDeviationY, 0}; }
    float fireDeviation() const { return m_fireDeviation; }
};
struct Fixture
{
    Gun* gun = new Gun(new GenericFireArmTemplate());
    SimpleDeviationCompTemplate* settings = new SimpleDeviationCompTemplate();
    Component* component = new Component(settings);
    GenericProjectile* projectile = new GenericProjectile(new GenericProjectileTemplate());
    PointPhysicsNode* node = new PointPhysicsNode();
    Fixture()
    {
        component->setObject(gun);
        component->init(gun);
        node->setObject(projectile);
        check(projectile->setComponent(IID_IPhysicsNode, node), "attach actual point physics node");
        settings->setFireDev(0.25f, 0.5f, 0.0f);
        settings->setSubProjectileDev(2.0f);
    }
    Vec3 run(float deviation, float x, float y, bool dummy = false)
    {
        component->prepare(deviation, x, y);
        node->setPositionalSpeed(Vec3{0, 0, 25});
        gun->velocityCalls = gun->devModCalls = 0;
        WeaponFireEvent event = {};
        event.projectile = projectile;
        event.projectileIndex = 17;
        event.dummyProjectile = dummy;
        component->postFire(event);
        check(component->fireDeviation() == 0.25f, "fire deviation still accumulates and clamps");
        return node->getPositionalSpeed();
    }
};
}

int main()
{
    g_system = new System();
    eventManager = new EventManager();
    classManager = new Classes();
    responsePhysicsManager = new ResponsePhysicsManager();
    auto* materials = new MaterialManager(); materialManager = materials;
    materials->setAttMaterial(0);
    objectTemplateManager = new ObjectTemplateManager();
    objectManager = new ObjectManager();
    physicsNodeManager = new PhysicsNodeManager();
    game = new Server();
    Fixture fixture;

    Mat4 transforms[2] = {Mat4::getIdentity(), Mat4::getIdentity()};
    // An orthonormal, elevated non-axis-aligned basis tests actual axis selection.
    transforms[1].m[0]=0.6f; transforms[1].m[1]=0; transforms[1].m[2]=-0.8f;
    transforms[1].m[4]=-0.48f; transforms[1].m[5]=0.8f; transforms[1].m[6]=-0.36f;
    transforms[1].m[8]=0.64f; transforms[1].m[9]=0.6f; transforms[1].m[10]=0.48f;
    for (const Mat4& transform : transforms)
    {
        fixture.projectile->setAbsoluteTransformation(transform);
        const float velocities[] = {25, 100, 1000};
        for (float velocity : velocities)
        {
            fixture.gun->settings->m_velocity = velocity;
            const Vec3 baseline = fixture.run(2.0f, 0, 0);
            check(fixture.gun->velocityCalls == 2 && fixture.gun->devModCalls == 0,
                  "each axis queries actual weapon velocity without querying getDevMod");
            const Vec3 x = difference(fixture.run(2.0f, 100, 0), baseline);
            const Vec3 y = difference(fixture.run(2.0f, 0, 100), baseline);
            check(same(x, Vec3{transform.m[4]*velocity,
                              transform.m[5]*velocity, transform.m[6]*velocity}),
                  "100 cached X units contribute the weapon velocity along the up axis");
            check(same(y, Vec3{transform.m[0]*velocity,
                              transform.m[1]*velocity, transform.m[2]*velocity}),
                  "100 cached Y units contribute the weapon velocity along the right axis");
        }
    }
    fixture.projectile->setAbsoluteTransformation(Mat4::getIdentity());
    fixture.gun->settings->m_velocity = 25.0f;
    fixture.gun->secondVelocity = 50.0f;
    const Vec3 changing = fixture.run(2.0f, 0, 0);
    check(same(difference(fixture.run(2.0f, 100, 100), changing), Vec3{50,25,0})
          && fixture.gun->velocityCalls == 2 && fixture.gun->devModCalls == 0,
          "a changing virtual velocity is queried separately for up then right");
    fixture.gun->secondVelocity = -1.0f;
    check(same(fixture.run(0.01f, 100, 100), Vec3{0,0,25})
          && fixture.gun->velocityCalls == 0 && fixture.gun->devModCalls == 0,
          "exact deviation threshold skips cached offsets and weapon queries");
    const Vec3 above = fixture.run(0.011f, 0, 0);
    check(same(difference(fixture.run(0.011f, 100, 100), above), Vec3{25,25,0}),
          "both scaled axes activate immediately above the threshold");

    fixture.run(999.0f, 100, 100, true);
    const Vec3 firstCache = fixture.component->cache();
    fixture.run(0.0f, -100, -100, true);
    const Vec3 secondCache = fixture.component->cache();
    check(same(firstCache, secondCache), "dummy path replaces cache using subprojectile deviation");
    const Vec3 baseline = fixture.run(2.0f, 0, 0);
    const Vec3 cached = fixture.run(2.0f, secondCache.x, secondCache.y);
    check(same(difference(cached, baseline), Vec3{secondCache.y*0.25f, secondCache.x*0.25f, 0}),
          "subsequent real projectile scales the dummy cache exactly once");

    WeaponFireEvent absent = {};
    fixture.gun->velocityCalls = fixture.gun->devModCalls = 0;
    const Vec3 before = fixture.node->getPositionalSpeed();
    fixture.component->postFire(absent);
    check(fixture.gun->velocityCalls == 0 && fixture.gun->devModCalls == 0
          && same(before, fixture.node->getPositionalSpeed()),
          "absent projectile changes no velocity and makes no weapon queries");
    std::printf("Deviation scale: %d checks, %d failures\n", checks, failures);
    // Test-owned objects live to process exit; no map, network or gameplay lifecycle is claimed.
    return failures ? 1 : 0;
}
