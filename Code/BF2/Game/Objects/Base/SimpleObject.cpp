// Path:     attested by __FILE__ in both PE and ELF server binaries.
// Contents: recovered SimpleObject construction, teardown, and bounded overrides.

#include "SimpleObject.h"
#include "SimpleObjectTemplate.h"

#include "../../../AI/AIInterface/AIInstance.h"
#include "../../../AI/AIInterface/BFKnowledge/AITemplate.h"           // ...and its hit arm's createObject
#include "../../../AI/AIInterface/BFKnowledge/IAIObject.h"            // m_aiObject's type: init(), release()
#include "../../../AI/AIInterface/BFKnowledge/AITemplateManager.h"    // aiObjectCreateWithInstance's lookup
#include "../../../AI/AIInterface/BFKnowledge/KitTemplateManager.h"   // and its discarded miss-arm lookup
#include "../../../Core/ClassManager.h"
#include "../../../Core/Debug.h"
#include "../../../Core/Ids.h"
#include "../../../Core/StringUtil.h"
#include "../../../Game/IGameLogic.h"
#include "../../../Geom/DecalSystem/DecalObjectAccess.h"  // ~SimpleObject's DecalObject slot 10 (release)
#include "../../../Geom/DecalSystem/IDecalManager.h"      // the ctor's object decal (createClient +0x14)
#include "../../../Geom/IGeometryTemplate.h"
#if !DEDICATED && defined(_MSC_VER)
#include "../../../Geom/GeometryTemplateManager.h"
#endif
#include "../../../Physics/CollisionManager.h"
#include "../../../Physics/ICollisionMesh.h"
#include "../../../Physics/IPhysicsNode.h"
#include "../../../Physics/IResponsePhysics.h"
#include "../../../Physics/Material.h"
#include "../../../Physics/MaterialManager.h"
#include "../GameLogicComp/IArmorGLComp.h"
#include "../../../Scene/Object/ObjectUtil.h"
#include "../../../Scene/Object/PlayerManager.h"
#include "../../../Scene/Object/WorldObjTemplPortal/PortalManager.h"
#include "../../Main/Game.h"                        // isDedicated() - the ctor's decal arm (BF2.exe 0x440510)

#include <cfloat>
#include <cmath>

namespace dice {
namespace hfe {

#if !DEDICATED
// Game/Main/BF2Render.cpp defines this at exactly this scope, published right after RendDX9's
// initDll (BF2.exe ds:0x9A5578). Declared here rather than pulled through
// Game/Main/BF2ClientSeams.h, which is a client-side header while this translation unit is shared.
extern IBase* g_geometryTemplateManager;
#endif
extern ClassManager* classManager;
namespace world {

extern IBase* materialManager;      // ds:0x8AD2820, defined in Core/Globals.cpp

namespace {

#if defined(_MSC_VER)
#  define BF2_SIMPLE_VCALL __thiscall
#else
#  define BF2_SIMPLE_VCALL
#endif

template <class Fn>
inline Fn simpleVslot(void* object, unsigned int byteOffset)
{
    return reinterpret_cast<Fn>((*reinterpret_cast<void***>(object))[byteOffset / 4]);
}

// The two renderer-geometry operations the animated-UV update drives. [INFERRED] names - the server
// tree has no IGeometry declaration and neither slot is otherwise reachable, so only the physical
// offsets and the argument shapes are recovered.
void setGeometryAnimatedUVTranslation(void* geometry, int index, float first, float second)
{
#if defined(_MSC_VER)
    const unsigned int slot = 0x90;
#else
    const unsigned int slot = 0x94;
#endif
    typedef void (BF2_SIMPLE_VCALL* Fn)(void*, int, float, float);
    simpleVslot<Fn>(geometry, slot)(geometry, index, first, second);
}

void setGeometryAnimatedUVRotation(void* geometry, int index, const Mat4& transform)
{
#if defined(_MSC_VER)
    const unsigned int slot = 0x98;
#else
    const unsigned int slot = 0x9C;
#endif
    typedef void (BF2_SIMPLE_VCALL* Fn)(void*, int, const Mat4&);
    simpleVslot<Fn>(geometry, slot)(geometry, index, transform);
}

// Leaves nearly unit vectors unchanged and collapses nearly zero vectors to zero.
// [UNRESOLVED] original helper placement.
void normalizeInPlace(Vec3& value)
{
    const float squaredLength = value.x * value.x + value.y * value.y + value.z * value.z;
    if (std::fabs(squaredLength - 1.0f) < FLT_EPSILON)
        return;

    if (std::fabs(squaredLength) >= FLT_EPSILON)
    {
        const float inverse = 1.0f / std::sqrt(squaredLength);
        value.x *= inverse;
        value.y *= inverse;
        value.z *= inverse;
    }
    else
    {
        value.x = 0.0f;
        value.y = 0.0f;
        value.z = 0.0f;
    }
}

// Wraps using max's sign except at exactly 0.001f, which leaves value unchanged.
// A zero max with a nonzero value never terminates.
void wrapAnimatedUVValue(float& value, float max)
{
    if (max > 0.001f)
    {
        while (value > max) value -= max;
        while (value < 0.0f) value += max;
    }
    else if (max < 0.001f)
    {
        while (max > value) value += std::fabs(max);
        while (value > 0.0f) value -= std::fabs(max);
    }
}

#undef BF2_SIMPLE_VCALL

} // unnamed namespace

// Creates and attaches geometry on the client, releasing the temporary reference after attachment.
// Returns false on unavailable services or lookup failure. Dedicated builds return false without attaching geometry.
bool setGeometry(IObject* object, const std::string& geometry)
{
#if DEDICATED
    // 12 bytes.  The shipped dedicated-server body, preserved exactly.
    (void)object;
    (void)geometry;
    return false;
#else
    if (!::dice::hfe::g_geometryTemplateManager)
        return false;

#if !DEDICATED && defined(_MSC_VER)
    geom::IGeometryTemplate* const geometryTemplate =
        static_cast<geom::IGeometryTemplateManager*>(
            ::dice::hfe::g_geometryTemplateManager)->getTemplate(geometry);
#else
    // Other configurations retain the engine string carrier without importing x86 renderer layouts.
    typedef IBase* (__thiscall* GetTemplateFn)(void*, const std::string&);
    void* const manager = ::dice::hfe::g_geometryTemplateManager;
    geom::IGeometryTemplate* const geometryTemplate =
        static_cast<geom::IGeometryTemplate*>(reinterpret_cast<GetTemplateFn>(
            (*reinterpret_cast<void***>(manager))[0x18 / 4])(manager, geometry));
#endif
    if (!geometryTemplate)
        return false;

    IBase* const instance = static_cast<IBase*>(geometryTemplate->createInstance());
    if (!instance)
        return false;

    // Attach through IObject's typed component interface.
    if (!object->setComponent(geom::IID_IGeometry, instance))
    {
        instance->release();
        return false;
    }

    // The geometry instance's +0x9C, handed the object it now belongs to.  The geometry instance
    // interface is not modelled anywhere in this tree, so this stays a documented raw slot - the
    // same policy as ObjectDrawer's +0x18 and +0xCC.
    typedef void (__thiscall* SetGeometryOwnerFn)(void*, IObject*);
    reinterpret_cast<SetGeometryOwnerFn>(
        (*reinterpret_cast<void***>(instance))[0x9C / 4])(instance, object);

    instance->release();
    return true;
#endif
}

// Seven zeroed words then a true byte, in that order. The true byte is what makes the first visual
// update discard its travel delta.
SimpleObject::AnimatedUV::AnimatedUV()
    : m_rotation(0.0f),
      m_rotateSource(0),
      m_firstSample(true)
{
    m_translation[0] = 0.0f;
    m_translation[1] = 0.0f;
    m_lastPosition.x = 0.0f;
    m_lastPosition.y = 0.0f;
    m_lastPosition.z = 0.0f;
}

SimpleObject::SimpleObject(const SimpleObjectTemplate* objectTemplate)
    : Object(objectTemplate),
      m_animatedUV(0),
      m_decalObject(0),
      m_replicationState(0)
{
    objectTemplate->setPhysicsNodeComponent(this);
    m_updateFrequency = objectTemplate->m_physicsFrequency;
    if (objectTemplate->m_physicsFlags & 0x04u)
        Object::updateFlags(0x100u, 0);
    objectTemplate->setResponsePhysicsComponent(this);

    if (objectTemplate->m_animatedUVTranslation ||
        objectTemplate->m_animatedUVRotation ||
        objectTemplate->m_rotateAsAnimatedUV)
        m_animatedUV = new AnimatedUV;

    if (!objectTemplate->m_geometry.empty())
        setGeometry(this, objectTemplate->m_geometry);

    if (!objectTemplate->m_collisionMesh.empty())
    {
        ICollisionManager* manager = reinterpret_cast<ICollisionManager*>(
            classManager->getSingleton(std::string("CollisionManager")));
        ICollisionMesh* mesh = manager->getCollisionMeshPart(
            objectTemplate->getCollisionMesh(), objectTemplate->m_collisionPart);
        if (mesh)
        {
            mesh->createMaterialMapping(objectTemplate);
            if (!Object::setComponent(IID_ICollisionMesh,
                                      reinterpret_cast<IBase*>(mesh)))
            {
                Debug(DEBUGTYPE_6, "Game/Objects/Base/SimpleObject.cpp", 160)
                    << "Couldn't create collision mesh \""
                    << objectTemplate->m_collisionMesh
                    << "\" when creating object of template \""
                    << objectTemplate->getName();
            }
            reinterpret_cast<IBase*>(mesh)->release();
        }
    }

    // Client collision objects without point physics receive a decal owner.
    // Requires DecalManager; the destructor releases the owner.
    const unsigned char physicsFlags = objectTemplate->m_physicsFlags;
    if (!(physicsFlags & 0x18u) && (physicsFlags & 0x04u) && !isDedicated())
    {
        geom::IDecalManager* const decalManager = static_cast<geom::IDecalManager*>(
            classManager->getSingleton(std::string("DecalManager")));
        m_decalObject = decalManager->createClient(this);
    }

    if (objectTemplate->m_animatedUVTranslation ||
        objectTemplate->m_animatedUVRotation ||
        objectTemplate->m_rotateAsAnimatedUV)
        Object::updateFlags(0x1000u, 0);
}

SimpleObject::~SimpleObject()
{
    const SimpleObjectTemplate* objectTemplate =
        static_cast<const SimpleObjectTemplate*>(m_template);
    ::operator delete(m_animatedUV);
    if (m_decalObject)
    {
        // DecalObject slot 10 (client PE 0x627D72 +0x28 / ELF 0x81EE9E8 +0x2C) - re-
        geom::decal_object::release(m_decalObject);
        m_decalObject = 0;
    }

    if (m_aiObject)
    {
        if (Object::queryComponent(IID_IPortalPassing, IID_IPortalPassing))
            reinterpret_cast<IPortalManager*>(portalManager)
                ->unRegisterPortalPassingObject(this);
        if (Object::queryComponent(IID_ISector, IID_ISector))
            reinterpret_cast<IPortalManager*>(portalManager)
                ->unRegisterSectorObject(this);
    }

    objectTemplate->removeResponsePhysicsComponent(this);
    objectTemplate->removePhysicsNodeComponent(this);
}

void SimpleObject::destroy(bool force) { Object::destroy(force); }

void SimpleObject::disableCachedObject()
{
    if (m_aiObject)
    {
        setComponent(ai::IID_IAIObject, 0);
        if (queryComponent(IID_IPortalPassing, IID_IPortalPassing))
            reinterpret_cast<IPortalManager*>(portalManager)
                ->unRegisterPortalPassingObject(this);
        if (queryComponent(IID_ISector, IID_ISector))
            reinterpret_cast<IPortalManager*>(portalManager)
                ->unRegisterSectorObject(this);
    }
    Object::disableCachedObject();
}

// Revives cached objects and restores bot-game AI and root portal registration when applicable.
void SimpleObject::enableCachedObject()
{
    if (!m_aiObject && !queryComponent(ai::IID_IWeapon, ai::IID_IWeapon))
    {
        aiObjectCreate();                        // bots-gated inside; a no-op without AI
        if (hfe::ai::g_aiInstance &&
            hfe::ai::aiInstance()->isBotsGame())
        {
            enableAiObjectAndPortal();           // the interior above
        }
    }
    Object::enableCachedObject();
}

// Updates animated rotation and UVs from movement and configured speed; time arguments are unused.
// The first movement sample contributes zero. Rotation replaces the prior relative rotation while preserving position.
void SimpleObject::handleVisualUpdate(float, float)
{
    const SimpleObjectTemplate* objectTemplate =
        static_cast<const SimpleObjectTemplate*>(m_template);

    if (objectTemplate->m_rotateAsAnimatedUV)
    {
        AnimatedUV* animated = m_animatedUV;
        if (!animated->m_rotateSource)
            animated->m_rotateSource = findObjectWithName(
                getRootParent(this), objectTemplate->m_rotateAsAnimatedUVObject);

        if (m_animatedUV->m_rotateSource)
        {
            Mat4 rotation = Mat4::getIdentity();
            const Vec3& position = getRelativePosition();

            float phase = 0.0f;
            const AnimatedUV* source = static_cast<SimpleObject*>(
                m_animatedUV->m_rotateSource)->m_animatedUV;
            if (source)
                phase = source->m_rotation;

            float degrees = phase / 6.2831855f * 360.0f;
            if (objectTemplate->m_rotateAsAnimatedUVReverse)
                degrees = -degrees;
            setRotateXDeg(rotation, degrees);

            rotation.m[12] = position.x;
            rotation.m[13] = position.y;
            rotation.m[14] = position.z;
            setRelativeTransformation(rotation);
        }
    }

    if (!objectTemplate->m_animatedUVTranslation && !objectTemplate->m_animatedUVRotation)
        return;

    IObject* root = getRootParent(this);
    if (!root)
        return;

    void* geometry = root->m_geometry;
    if (!geometry)
        return;

    const Vec3& position = getAbsolutePosition();
    AnimatedUV* animated = m_animatedUV;

    Vec3 delta = {position.x - animated->m_lastPosition.x,
                  position.y - animated->m_lastPosition.y,
                  position.z - animated->m_lastPosition.z};
    const float negatedDistance =
        -std::sqrt(delta.x * delta.x + delta.y * delta.y + delta.z * delta.z);
    normalizeInPlace(delta);

    const Mat4& transform = getAbsoluteTransformation();
    float travel = negatedDistance *
                   (transform.m[8] * delta.x + transform.m[9] * delta.y +
                    transform.m[10] * delta.z);

    if (animated->m_firstSample)
    {
        animated->m_firstSample = false;
        travel = 0.0f;
    }

    if (objectTemplate->m_animatedUVTranslation)
    {
        // Second channel first, exactly as shipped.
        if (objectTemplate->m_animatedUVTranslationSize.y != 0.0f)
        {
            const float step = travel / objectTemplate->m_animatedUVTranslationSize.y;
            animated->m_translation[1] = objectTemplate->m_animatedUVTranslationReverse
                                             ? animated->m_translation[1] - step
                                             : animated->m_translation[1] + step;
            animated->m_translation[1] += objectTemplate->m_animatedUVTranslationSpeed.y;
            wrapAnimatedUVValue(animated->m_translation[1],
                                objectTemplate->m_animatedUVTranslationMax.y);
        }

        if (objectTemplate->m_animatedUVTranslationSize.x != 0.0f)
        {
            const float step = travel / objectTemplate->m_animatedUVTranslationSize.x;
            animated->m_translation[0] = objectTemplate->m_animatedUVTranslationReverse
                                             ? animated->m_translation[0] - step
                                             : animated->m_translation[0] + step;
            animated->m_translation[0] += objectTemplate->m_animatedUVTranslationSpeed.x;
            wrapAnimatedUVValue(animated->m_translation[0],
                                objectTemplate->m_animatedUVTranslationMax.x);
        }

        setGeometryAnimatedUVTranslation(geometry,
                                         objectTemplate->m_animatedUVTranslationIndex,
                                         animated->m_translation[0],
                                         animated->m_translation[1]);
    }

    if (objectTemplate->m_animatedUVRotation)
    {
        const float step = travel /
                           (objectTemplate->m_animatedUVRotationRadius * 6.2831855f) * 6.2831855f;
        animated->m_rotation = objectTemplate->m_animatedUVRotationReverse
                                   ? animated->m_rotation - step
                                   : animated->m_rotation + step;

        while (animated->m_rotation > 6.2831855f) animated->m_rotation -= 6.2831855f;
        while (animated->m_rotation < 0.0f) animated->m_rotation += 6.2831855f;

        const float phase = animated->m_rotation;
        Mat4 rotation = Mat4::getIdentity();
        rotation.m[0] = std::cos(phase) * objectTemplate->m_animatedUVRotationScale.x;
        rotation.m[1] = -std::sin(phase) * objectTemplate->m_animatedUVRotationScale.y;
        rotation.m[4] = std::sin(phase) * objectTemplate->m_animatedUVRotationScale.x;
        rotation.m[5] = std::cos(phase) * objectTemplate->m_animatedUVRotationScale.y;

        setGeometryAnimatedUVRotation(geometry, objectTemplate->m_animatedUVRotationIndex,
                                      rotation);
    }

    const Vec3& updated = getAbsolutePosition();
    animated->m_lastPosition = updated;
}

void SimpleObject::handleUpdate(float deltaTime, unsigned int)
{
    const unsigned int flags = m_flags;
    if ((flags & 0x00400000u) != 0)
    {
        for (ComponentMap::iterator it = m_components.begin(); it != m_components.end(); ++it)
        {
            IObjectComponent* component = it->second.get();
            if (component)
                component->handleUpdate(deltaTime);
        }
    }
    if ((flags & 0x01000000u) != 0 && m_responsePhysics)
        static_cast<IResponsePhysics*>(m_responsePhysics)->updateEffect(deltaTime);
}

void SimpleObject::handleMessage(TemplateMessage message, IPlayer* player, bool flag)
{
    if (m_radioGLComponent)
        playerManager->getLocalHumanPlayer();

    if (message == static_cast<TemplateMessage>(39) ||
        message == static_cast<TemplateMessage>(40))
    {
        for (ComponentMap::iterator it = m_components.begin();
             it != m_components.end(); ++it)
        {
            IObjectComponent* component = it->second.get();
            if (component)
                component->handleMessage(message, player, flag);
        }
    }
}

// Processes barbwire contact, ancestor armor and game-logic collision callbacks.
// The second integer is the material id. Barbwire returns false but does not skip the later callbacks.
// Non-projectile dispatch reverses the collision perspective.
bool SimpleObject::handleCollision(IObject* other, const Vec3& first, const Vec3& second,
                                   const Vec3& third, int firstInt, int materialId,
                                   CollisionFaceInfo* firstFace, CollisionFaceInfo* secondFace)
{
    getRootParent(this);

    bool result = true;
    if (other)
    {
        Material* material = reinterpret_cast<MaterialManager*>(materialManager)
                                 ->getMaterialPtr(static_cast<unsigned int>(materialId));
        if (material && material->getIsBarbwire())
        {
            other->handleMessage(static_cast<TemplateMessage>(0), 0, false);
            result = false;
        }
    }

    IObjectComponent* armor = 0;
    for (IObject* object = this; object; object = object->m_parent)
    {
        armor = object->m_armorComponent;
        if (armor)
            break;
    }

    if (armor)
    {
        if (static_cast<IArmorGLComp*>(armor)->collision(firstInt, materialId, other))
        {
            if (queryInterface(IID_IProjectileObject))
            {
                gameLogic->handleCollisionForProjectile(this, other, first, second, third,
                                                        firstInt, materialId, firstFace, secondFace);
            }
            else
            {
                const Vec3 negated = {-first.x, -first.y, -first.z};
                gameLogic->handleCollision(other, this, negated, second, third,
                                           materialId, firstInt, secondFace, firstFace);
            }
        }

        const Vec3& position = getAbsolutePosition();
        static_cast<IArmorGLComp*>(armor)->setLastCollisionHeight(position.y);
    }
    else if (queryInterface(IID_IProjectileObject))
    {
        gameLogic->handleCollisionForProjectile(this, other, first, second, third,
                                                firstInt, materialId, firstFace, secondFace);
    }

    return result;
}

std::string SimpleObject::getArmorInfo()
{
    IObjectComponent* armor = 0;
    for (IObject* object = this; object; object = object->m_parent)
    {
        armor = object->m_armorComponent;
        if (armor)
            break;
    }
    if (!armor)
        return std::string();

    const char* suffix = "";
    if (m_physicsNode && static_cast<IPhysicsNode*>(m_physicsNode)->getIsSleeping())
        suffix = "*";
    return formatString("%.2f%s", static_cast<IArmorGLComp*>(armor)->getHitPoints(), suffix);
}

int SimpleObject::getGeometryPart() const
{
    const SimpleObjectTemplate* objectTemplate =
        static_cast<const SimpleObjectTemplate*>(m_template);
    return objectTemplate->m_geometryPart;
}

int SimpleObject::getMaterial() const
{
    const SimpleObjectTemplate* objectTemplate =
        static_cast<const SimpleObjectTemplate*>(m_template);
    return objectTemplate->getMaterial();
}

void* SimpleObject::getDecalObject() const { return m_decalObject; }

// Attaches SimpleObject replication when a template and nonempty network descriptor are present.
// The shared helper repeats those checks; this caller ignores its result.
void SimpleObject::aiObjectCreate()
{
    if (hfe::ai::g_aiInstance &&
        hfe::ai::aiInstance()->isBotsGame())
    {
        aiObjectCreateWithInstance();            // the AI arm, below
    }
}

void SimpleObject::init()
{
    // hfe::ai, NOT world::ai - an extern declared inside this namespace would mint a wrong-scope
    // twin of the global. Declared below at hfe scope; qualified here.
    Object::init();

    if (hfe::ai::g_aiInstance &&
        hfe::ai::aiInstance()->isBotsGame())
    {
        initAiObjectAndPortal();                 // see the block above
    }
}

// [CHOSEN, not recovered] Helpers split the original AI operations at their bot-game gates.

// Creates the named AI component, attaches it and releases the temporary reference; a miss queries the kit-template manager.
// Portal-passing registration is independent of AI-template presence. Requires initialized managers.

void SimpleObject::aiObjectCreateWithInstance()
{
    const SimpleObjectTemplate* objectTemplate =
        static_cast<const SimpleObjectTemplate*>(m_template);

    // The binary tests the string's length word directly (`*(rep - 12)`; PE `size() != 0`).
    if (!objectTemplate->m_aiTemplate.empty())
    {
        // THE copy IS retail'S: getTemplate takes the name BY value (ELF 0x81EEDBB copy-constructs
        // a temporary; the PE pushes a 28-byte copy that the callee destroys).
        hfe::ai::AITemplate* const aiTemplate =
            hfe::ai::AITemplateManager::m_instance->getTemplate(objectTemplate->m_aiTemplate);
        if (aiTemplate)
        {
            hfe::ai::IAIObject* const aiObject = aiTemplate->createObject(0, this);
            setComponent(hfe::ai::IID_IAIObject, aiObject);                           // +0x58
            aiObject->release();                                                      // +0x08
        }
        else
        {
            // result discarded, as shipped (ELF 0x81EEE33 / PE 0x61526D): a kit-template lookup
            // by the AI template's name, made for nothing.
            hfe::ai::KitTemplateManager::m_instance->getTemplate(objectTemplate->m_aiTemplate);
        }
    }

    if (objectTemplate->m_isPortalPassing)
    {
        reinterpret_cast<IPortalManager*>(portalManager)
            ->registerPortalPassingObject(this, objectTemplate->m_portalPassingPosition);   // +0x18
    }
}

// Initializes a newly created AI object and registers root objects with portal geometry as sectors.

void SimpleObject::initAiObjectAndPortal()
{
    if (!m_aiObject)
    {
        aiObjectCreate();

        if (m_aiObject)
            static_cast<hfe::ai::IAIObject*>(m_aiObject)->init();                    // +0x48
    }

    // Root objects with portals register as sectors; teardown unregisters them.
    if (!m_parent)
    {
        IBase* const portalMesh = getPortalMesh(this, 0);
        if (portalMesh && static_cast<geom::IPortalMesh*>(portalMesh)->getPortalCount())
        {
            reinterpret_cast<IPortalManager*>(portalManager)->registerSectorObject(this);  // +0x20
        }
    }
}

// Reinitializes an existing AI object and restores root portal-sector registration after cache revival.
void SimpleObject::enableAiObjectAndPortal()
{
    if (m_aiObject)
        static_cast<hfe::ai::IAIObject*>(m_aiObject)->init();                        // +0x48

    if (!m_parent)
    {
        IBase* const portalMesh = getPortalMesh(this, 0);
        if (portalMesh && static_cast<geom::IPortalMesh*>(portalMesh)->getPortalCount())
        {
            reinterpret_cast<IPortalManager*>(portalManager)->registerSectorObject(this);  // +0x20
        }
    }
}
void SimpleObject::initNetworkableComp()
{
    const SimpleObjectTemplate* objectTemplate =
        static_cast<const SimpleObjectTemplate*>(m_template);
    if (!objectTemplate)
        return;

    if (objectTemplate->getNetworkableInfo().compare("") != 0)
        setNetworkableComp(this, CID_SimpleObjectNetworkable);
}

// Unregisters this portal-passing object; requires the portal manager.
void SimpleObject::purgePortalPassingObject()
{
    reinterpret_cast<IPortalManager*>(portalManager)->unRegisterPortalPassingObject(this);   // +0x1C
}

} // namespace world
} // namespace hfe
} // namespace dice
