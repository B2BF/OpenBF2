// [CHOSEN, not recovered] split of the attested Geom/BundledMesh.cpp path.
// Renderer PE 10104160 (height), 101069D0/10106A20 (filters), and 10105E60..10105F6A
// (additive trace). Full instructions read; the dedicated ELF has no renderer body.
#ifdef BF2_IMAGE_RENDDX9
#include "BundledMesh.h"
#include "BundledMeshTemplate.h"
#include "StaticMesh.h"
#include "XpLightManager.h"
#include "../Game/Objects/IObject.h"
#include "../Game/Objects/IPlayer.h"
#include "../Game/Objects/ObjectLayoutAccess.h"
#include "../Scene/Object/ObjectManager.h"
#include "../Scene/Object/ObjectPredicator.h"
#include "../Scene/Object/PlayerManager.h"

namespace dice { namespace hfe {
extern IBase* objectManager;
extern IBase* playerManager;
namespace geom {
extern LightManager* g_LightManager;
namespace {

// The instance publishes a flat PE table. Keep the repeated +A4 calls: its result
// is refreshed at each shipped site rather than cached across a collision callback.
world::IObject* meshRoot(BundledMesh* mesh)
{
    typedef world::IObject* (__thiscall* Function)(void*);
    return reinterpret_cast<Function>(mesh->m_vptr[0xA4 / 4])(mesh);
}
const Vec3& meshPosition(const BundledMesh* mesh)
{
    return *reinterpret_cast<const Vec3*>(&mesh->m_partTransforms[0]->m[12]);
}

// These four-slot predicates have no virtual destructor on either ABI. The actual
// ObjectManager +68 interface takes ObjectPredicator*, including in the native host.
class BundledRootIgnorePredicator : public world::ObjectPredicator
{
public:
    virtual bool predicate(world::IObject* object)
    { return staticMeshRootParent(object) != root; }
    virtual unsigned int getFlags() const { return 0; }
    virtual bool performCulling(world::IObject*) { return true; }
    virtual bool predicateSurface(int) { return true; }
    void* root;
};

// 1025B224 is the renderer's shared ObjectFlagPredicator(0), initialized at
// Its complete predicate 101C7EB0 still reads the object's flags.
class BundledFlagPredicator : public world::ObjectPredicator
{
public:
    BundledFlagPredicator() : flags(0) {}
    virtual bool predicate(world::IObject* object)
    { return (world::object_detail::LayoutAccess::flags(object) & flags) == flags; }
    virtual unsigned int getFlags() const { return flags; }
    virtual bool performCulling(world::IObject*) { return true; }
    virtual bool predicateSurface(int) { return true; }
    unsigned int flags;
};
BundledFlagPredicator heightFlags;

class BundledAdditivePredicator : public world::ObjectPredicator
{
public:
    // The owner rejection precedes root/player and geometry tests.
    virtual bool predicate(world::IObject* object)
    {
        if (owner == object)
            return false;
        if (object)
        {
            world::IObject* const root =
                static_cast<world::IObject*>(staticMeshRootParent(object));
            if (root)
            {
                world::IPlayer* const player =
                    reinterpret_cast<world::PlayerManager*>(playerManager)->getLocalHumanPlayer();
                if (player && player->hasFirstPersonView() && player->getVehicle() == root)
                    return false;
            }
        }
        // There is no null-object guard here in the shipped callback.
        void* const geometry = world::object_detail::LayoutAccess::geometry(object);
        if (geometry && geometry != mesh)
        {
            typedef MeshTemplate* (__thiscall* GetTemplate)(void*);
            MeshTemplate* const objectTemplate = reinterpret_cast<GetTemplate>(
                (*static_cast<void***>(geometry))[0x38 / 4])(geometry);
            if (objectTemplate->getClassId() == 0x870CA651u &&
                static_cast<BundledMeshTemplate*>(objectTemplate)->m_hasEnvironmentMap)
                return false;
        }
        return true;
    }
    virtual unsigned int getFlags() const { return 0; }
    virtual bool performCulling(world::IObject*) { return true; }
    virtual bool predicateSurface(int) { return true; }
    BundledMesh* mesh;
    void* owner;
};
} // anonymous namespace

void BundledMesh::updateHeightOverTerrain()
{
    if (!m_heightmapCluster || !m_ownerObject)
        return;

    // Retail uses process-wide scratch, resets hitObject only before the first
    // probe, and shares the root filter between the first and third probes.
    static Vec3 down = {0.0f, 0.0f, 0.0f};
    static Vec3 up = {0.0f, 0.0f, 0.0f};
    down.y = meshRoot(this)->getVisualBoundingRadius() * -4.0f;
    up.y = g_LightManager->getShadowRoofRayLength();
    static Vec3 hitPoint = {0.0f, 0.0f, 0.0f};
    static Vec3 hitNormal = {0.0f, 0.0f, 0.0f};
    static world::IObject* hitObject;
    hitObject = 0;
    static int hitPart;
    static BundledRootIgnorePredicator rootFilter;
    rootFilter.root = meshRoot(this);
    if (reinterpret_cast<world::ObjectManager*>(objectManager)->intersectLine(hitObject, hitPoint, hitNormal, hitPart,
            meshPosition(this), down, &rootFilter, true, false, true, true, 0))
        m_heightOverTerrain = meshPosition(this).y - hitPoint.y;
    else
        m_heightOverTerrain = meshRoot(this)->getVisualBoundingRadius() * 4.0f;

    const Vec2& fadeRange = g_LightManager->getShadowSunHeightOverFade();
    down.y = -fadeRange.y;
    if (reinterpret_cast<world::ObjectManager*>(objectManager)->intersectLine(hitObject, hitPoint, hitNormal, hitPart,
            meshPosition(this), down, &heightFlags, false, false, true, true, 0))
        m_shadowSunHeight = meshPosition(this).y - hitPoint.y;
    else
        m_shadowSunHeight = fadeRange.y;

    m_heightOverTerrain -= meshRoot(this)->getVisualBoundingRadius();
    if (m_heightOverTerrain < 0.0f)
        m_heightOverTerrain = 0.0f;
    float radius = meshRoot(this)->getVisualBoundingRadius();
    if (radius < 2.0f)
        radius = 2.0f;
    m_heightOverTerrain /= radius;
    m_underRoof = reinterpret_cast<world::ObjectManager*>(objectManager)->intersectLine(hitObject, hitPoint, hitNormal, hitPart,
        meshPosition(this), up, &rootFilter, false, false, false, false, 0);
}

bool bundledMeshTraceAdditive(BundledMesh* mesh, const Vec3& origin,
                              const Vec3& direction, void*& resultObject)
{
    // The draw's separate scratch at 1025AA20..48 survives across calls.
    static Vec3 hitPoint = {0.0f, 0.0f, 0.0f};
    static Vec3 hitNormal = {0.0f, 0.0f, 0.0f};
    static world::IObject* hitObject;
    hitObject = 0;
    static int hitPart;
    static BundledAdditivePredicator filter;
    filter.mesh = mesh;
    filter.owner = mesh->m_ownerObject;
    const bool hit = reinterpret_cast<world::ObjectManager*>(objectManager)->intersectLine(
        hitObject, hitPoint, hitNormal, hitPart, origin, direction,
        &filter, true, false, true, true, 0);
    resultObject = hitObject;
    return hit;
}
} } }
#endif
