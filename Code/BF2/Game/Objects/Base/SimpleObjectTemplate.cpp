// Simple-object geometry, materials, collision, and physics configuration.

#include "../../../Core/Ids.h"
#include "../../../Core/Debug.h"
#include "../../../Core/SmartItf.h"                  // classManager - createComponent
#include "../IObjectTemplateComponent.h"             // getClassID slot - createComponent
#include "../../../Physics/CollisionManager.h"
#include "../../../Physics/StaticPhysicsNode.h"          // the static physics pair - built by
#include "../../../Physics/StaticResponsePhysics.h"      // the two build helpers below
#include "../../../Physics/PointPhysicsNode.h"           // the mobile family - the two mobile arms
#include "../../../Physics/RotationalPointPhysicsNode.h"
#include "../../../Physics/PhysicsNode.h"
#include "../../../Physics/PhysicsType.h"
#include "../../../Physics/SoldierPhysicsNode.h"
#include "../../../Physics/PointResponsePhysics.h"
#include "../../../Physics/RotationalPointResponsePhysics.h"
#include "../../../Physics/ResponsePhysics.h"
#include "../../../Physics/SoldierResponse.h"
#include "../../../Physics/PhysicsNodeManager.h"         // physicsNodeManager, addPhysicsNode
#include "../../../Physics/ResponsePhysicsManager.h"     // responsePhysicsManager, addObject
#include "../../IGameLogic.h"                            // gameLogic->isServer() - the wake/sleep choice
#include "../../../Physics/Material.h"                   // CheckForSpecialMaterials' two traits
#include "../../../Physics/MaterialManager.h"            // and the id lookup
#include "SimpleObjectTemplate.h"
#include "../../../Core/StringUtil.h"
#include "../../../IO/Stream/IStream.h"
#include "../../../Scene/Object/ObjectTypes/Base/ObjectTemplateManager.h"  // addSoundTemplate creates through the manager
#include "../../../Geom/IGeometryTemplate.h"
#if !DEDICATED
#include "../../Main/Game.h"
#include "../../../Sound/BfSound.h"
#include "../../../Sound/SoundObjectTemplate.h"
#endif
#if !DEDICATED && defined(_MSC_VER)
#include "../../../Geom/GeometryTemplateManager.h"
#endif

#include <cfloat>
#include <cmath>
#include <sstream>
#include "SimpleObject.h"

namespace dice {
namespace hfe {

#if !DEDICATED
// BF2.exe ds:0x9A5578. Game/Main/BF2Render.cpp defines it at exactly this scope, published right
// after RendDX9's initDll. Declared here, as SimpleObject.cpp does, because this translation unit
// is shared and Game/Main/BF2ClientSeams.h is a client-side header. getAnchorPos's client arm.
extern IBase* g_geometryTemplateManager;
#endif

namespace world {

extern IBase* materialManager;      // ds:0x8AD2820, defined in Core/Globals.cpp

namespace {

// The two epsilon gates, written as the binary writes them: a component is "unchanged" only when
// every one of the three is within FLT_EPSILON of the reference, and the negation is what emits.
bool differsFromAll(float x, float y, float z, float reference)
{
    return !(std::fabs(x - reference) <= FLT_EPSILON &&
             std::fabs(y - reference) <= FLT_EPSILON &&
             std::fabs(z - reference) <= FLT_EPSILON);
}

} // unnamed namespace

// Writes base and simple-object settings, returning false immediately if the base writer fails.
// Always emits hasMobilePhysics; dragOffset is emitted only with nonzero drag.
// Float omission tests preserve their individual tolerances and NaN behavior.
bool SimpleObjectTemplate::makeScript(io::IStream* stream)
{
    if (!ObjectTemplate::makeScript(stream))
        return false;

    std::ostringstream script;
    const std::string prefix("ObjectTemplate");

    if (!m_geometry.empty())
        script << "ObjectTemplate.geometry " << m_geometry << std::endl;

    if (!m_collisionMesh.empty())
    {
        script << "ObjectTemplate.setCollisionMesh " << extractFileNameWithoutExt(m_collisionMesh)
               << std::endl;

        for (std::size_t i = 0; i != m_materialVector.size(); ++i)
        {
            script << "ObjectTemplate.mapMaterial " << i << " " << m_materialVector[i].first << " "
                   << m_materialVector[i].second << std::endl;
        }
    }

    if (m_drag > 0.0f)
    {
        script << prefix << ".drag " << m_drag << std::endl;
        if (m_dragOffset.x != 0.0f || m_dragOffset.y != 0.0f || m_dragOffset.z != 0.0f)
        {
            script << prefix << ".dragOffset " << m_dragOffset.x << '/' << m_dragOffset.y << '/'
                   << m_dragOffset.z << std::endl;
        }
    }

    if (m_dragModifier.x != 1.0f || m_dragModifier.y != 1.0f || m_dragModifier.z != 1.0f)
    {
        script << "ObjectTemplate.dragModifier " << m_dragModifier.x << '/' << m_dragModifier.y
               << '/' << m_dragModifier.z << std::endl;
    }

    if (1.0f > m_mass || m_mass > 1.0f)
        script << "ObjectTemplate.mass " << m_mass << std::endl;

    if (differsFromAll(m_inertiaModifier.x, m_inertiaModifier.y, m_inertiaModifier.z, 1.0f))
    {
        script << "ObjectTemplate.inertiaModifier " << m_inertiaModifier.x << '/'
               << m_inertiaModifier.y << '/' << m_inertiaModifier.z << std::endl;
    }

    if (m_gravityModifier != 1.0f)
        script << "ObjectTemplate.gravityModifier " << m_gravityModifier << std::endl;

    if (m_floaterMod != 1.0f)
        script << "ObjectTemplate.floaterMod " << m_floaterMod << std::endl;

    if (m_treeCollisionDiameter != 0.0f)
        script << "ObjectTemplate.treeCollisionDiameter " << m_treeCollisionDiameter << std::endl;

    if (m_centerOfCollisionOffset.x * m_centerOfCollisionOffset.x +
            m_centerOfCollisionOffset.y * m_centerOfCollisionOffset.y +
            m_centerOfCollisionOffset.z * m_centerOfCollisionOffset.z != 0.0f)
    {
        script << "ObjectTemplate.centerOfCollisionOffset " << m_centerOfCollisionOffset.x << '/'
               << m_centerOfCollisionOffset.y << '/' << m_centerOfCollisionOffset.z << std::endl;
    }

    script << "ObjectTemplate.hasMobilePhysics " << ((m_physicsFlags & 0x02u) != 0) << std::endl;

    if (m_physicsFlags & 0x04u)
        script << "ObjectTemplate.hasCollisionPhysics " << true << std::endl;

    if (m_physicsType)
        script << "ObjectTemplate.physicsType " << static_cast<PhysicsType>(m_physicsType)
               << std::endl;

    if (m_physicsFlags & 0x01u)
        script << "ObjectTemplate.isSleeping " << (m_physicsFlags & 0x01u) << std::endl;

    if (m_handleCollisionSpeed != 1.0f)
        script << "ObjectTemplate.handleCollisionSpeed " << m_handleCollisionSpeed << std::endl;

    if (differsFromAll(m_relativePositionOffset.x, m_relativePositionOffset.y,
                       m_relativePositionOffset.z, 0.0f))
    {
        script << "ObjectTemplate.relativePositionOffset " << m_relativePositionOffset.x << '/'
               << m_relativePositionOffset.y << '/' << m_relativePositionOffset.z << std::endl;
    }

    if (m_grip != 2)
        script << "ObjectTemplate.grip " << m_grip << std::endl;

    if (m_collisionGroups)
        script << "ObjectTemplate.collisionGroups " << m_collisionGroups << std::endl;

    if (!m_aiTemplate.empty())
        script << "ObjectTemplate.aiTemplate " << m_aiTemplate << std::endl;

    if (m_material != -1)
        script << "ObjectTemplate.material " << m_material << std::endl;

    if (m_animatedUVTranslation)
    {
        script << "ObjectTemplate.animatedUVTranslation " << m_animatedUVTranslation << std::endl;
        script << "ObjectTemplate.animatedUVTranslationSpeed " << m_animatedUVTranslationSpeed.x
               << '/' << m_animatedUVTranslationSpeed.y << std::endl;
        script << "ObjectTemplate.animatedUVTranslationMax " << m_animatedUVTranslationMax.x << '/'
               << m_animatedUVTranslationMax.y << std::endl;
        script << "ObjectTemplate.animatedUVTranslationSize " << m_animatedUVTranslationSize.x
               << '/' << m_animatedUVTranslationSize.y << std::endl;
        script << "ObjectTemplate.animatedUVTranslationReverse " << m_animatedUVTranslationReverse
               << std::endl;
        script << "ObjectTemplate.animatedUVTranslationIndex " << m_animatedUVTranslationIndex
               << std::endl;
    }

    if (m_animatedUVRotation)
    {
        script << "ObjectTemplate.animatedUVRotation " << m_animatedUVRotation << std::endl;
        script << "ObjectTemplate.animatedUVRotationRadius " << m_animatedUVRotationRadius
               << std::endl;
        script << "ObjectTemplate.animatedUVRotationScale " << m_animatedUVRotationScale.x << '/'
               << m_animatedUVRotationScale.y << std::endl;
        script << "ObjectTemplate.animatedUVRotationIndex " << m_animatedUVRotationIndex
               << std::endl;
        script << "ObjectTemplate.animatedUVRotationReverse " << m_animatedUVRotationReverse
               << std::endl;
    }

    if (m_rotateAsAnimatedUV)
    {
        script << "ObjectTemplate.rotateAsAnimatedUV " << m_rotateAsAnimatedUV << std::endl;
        script << "ObjectTemplate.rotateAsAnimatedUVReverse " << m_rotateAsAnimatedUVReverse
               << std::endl;
        script << "ObjectTemplate.rotateAsAnimatedUVObject " << m_rotateAsAnimatedUVObject
               << std::endl;
    }

    if (m_otherCollisionLod != -1)
        script << "ObjectTemplate.otherCollisionLod " << m_otherCollisionLod << std::endl;

    const std::string text = script.str();
    return stream->write(text.c_str(), static_cast<unsigned int>(text.length())) == text.length();
}

IObject* SimpleObjectTemplate::createObject() const
{
    if (!_byte160)
        const_cast<SimpleObjectTemplate*>(this)->CheckForSpecialMaterials();
    return new SimpleObject(this);
}

// Accumulates barbwire and water-physics traits from mapped materials once per template, then sets the lookup latch.
void SimpleObjectTemplate::CheckForSpecialMaterials()
{
    for (std::size_t i = 0; i < m_materialVector.size(); ++i)
    {
        Material* material = static_cast<MaterialManager*>(materialManager)
                                 ->getMaterialById(m_materialVector[i].second);
        if (!material)
            continue;
        if (material->getIsBarbwire())
            m_specialMaterials |= 1u;
        if (material->getHasWaterPhysics())
            m_specialMaterials |= 2u;
    }
    _byte160 = true;
}


// Both ports initialize the same logical fields. The differing physical offsets are compiler/STL
// ABI effects, including ISimpleObjectTemplate at ELF +48 and PE +96.
SimpleObjectTemplate::SimpleObjectTemplate()
    : m_refCount(1)
    , m_geometryPart(-1)
    , m_collisionPart(0)
    , m_drag(0.0f)
    , m_mass(1.0f)
    , m_gravityModifier(1.0f)
    , m_floaterMod(1.0f)
    , m_treeCollisionDiameter(0.0f)
    , m_otherCollisionLod(-1)
    , m_physicsFlags(0)
    , m_physicsType(0)
    , m_grip(2)
    , m_physicsFrequency(0)
    , m_collisionGroups(0)
    , m_material(-1)
    , m_isPortalPassing(0)
    , m_animatedUVTranslation(false)
    , m_animatedUVTranslationIndex(0)
    , m_animatedUVTranslationReverse(false)
    , m_animatedUVRotation(false)
    , m_animatedUVRotationIndex(0)
    , m_animatedUVRotationRadius(0.3f)
    , m_animatedUVRotationReverse(false)
    , m_rotateAsAnimatedUV(false)
    , m_rotateAsAnimatedUVReverse(false)
    , m_handleCollisionSpeed(1.0f)
    , _byte160(false)
{
    // Both binaries emit `and byte, 0x80`: the likely source was initialization of seven named
    // one-bit fields while the eighth allocation-unit bit was not a member. Until those remaining
    // bitfield names are recovered, zero is the defined source-semantic equivalent for every known
    // bit; reading an indeterminate byte merely to mimic that instruction would be undefined C++.
    m_dragOffset.x = m_dragOffset.y = m_dragOffset.z = 0.0f;
    m_dragModifier.x = m_dragModifier.y = m_dragModifier.z = 1.0f;
    m_centerOfMassOffset.x = m_centerOfMassOffset.y = m_centerOfMassOffset.z = 0.0f;
    m_centerOfCollisionOffset.x = m_centerOfCollisionOffset.y =
        m_centerOfCollisionOffset.z = 0.0f;
    m_inertiaModifier.x = m_inertiaModifier.y = m_inertiaModifier.z = 1.0f;
    m_unknown8C.x = m_unknown8C.y = m_unknown8C.z = 0.0f;
    m_unknown98.x = m_unknown98.y = m_unknown98.z = 1.0f;

    m_portalPassingPosition.x = m_portalPassingPosition.y =
        m_portalPassingPosition.z = 0.0f;

    m_animatedUVTranslationSpeed.x = m_animatedUVTranslationSpeed.y = 0.0f;
    m_animatedUVTranslationMax.x = 0.2f;
    m_animatedUVTranslationMax.y = 0.0f;
    m_animatedUVTranslationSize.x = 8.0f;
    m_animatedUVTranslationSize.y = 0.0f;
    m_animatedUVRotationScale.x = m_animatedUVRotationScale.y = 1.0f;

    m_relativePositionOffset.x = m_relativePositionOffset.y =
        m_relativePositionOffset.z = 0.0f;
    m_anchor.x = m_anchor.y = m_anchor.z = 0.0f;
    m_anchorOffset.x = m_anchorOffset.y = m_anchorOffset.z = 0.0f;
}

SimpleObjectTemplate::~SimpleObjectTemplate()
{
    // The observed destructors contain only compiler-generated member/base destruction.
}

void SimpleObjectTemplate::addRef()
{
    ++m_refCount;
}

int SimpleObjectTemplate::getRef()
{
    return m_refCount;
}

int SimpleObjectTemplate::release()
{
    const int refs = --m_refCount;
    if (refs == 0)
        delete this;
    return refs;
}

void* SimpleObjectTemplate::queryInterface(unsigned int iid) const
{
    if (iid == IID_IBase || iid == IID_IObjectTemplate)
        return static_cast<IObjectTemplate*>(const_cast<SimpleObjectTemplate*>(this));
    if (iid == IID_ISimpleObjectTemplate)
        return static_cast<ISimpleObjectTemplate*>(const_cast<SimpleObjectTemplate*>(this));
    if (iid == io::IID_IConsoleSaveable)
        return static_cast<io::IConsoleSaveable*>(const_cast<SimpleObjectTemplate*>(this));
    return 0;
}

unsigned int SimpleObjectTemplate::getClassID() const
{
    return CID_SimpleObjectTemplate;
}

void* SimpleObjectTemplate::queryImplementation(unsigned int classId)
{
    if (classId == CID_SimpleObjectTemplate)
        return this;
    return ObjectTemplate::queryImplementation(classId);
}

bool SimpleObjectTemplate::writeBinary(io::IStream* stream) const
{
    return ObjectTemplate::writeBinary(stream);
}

bool SimpleObjectTemplate::readBinary(io::IStream* stream)
{
    return ObjectTemplate::readBinary(stream);
}

// ; PE 0x5C3D50. This override deliberately rejects the legacy attachment path.
// Both ports construct the same type-6 Debug record at original source line 413 and perform no
// operation on either argument. DebugTypeError is emission-gated but fatal with the shipped debug
// configuration, so replacing this with a no-op would silently accept an invalid component path.
void SimpleObjectTemplate::setPhysicsComponent(IObject*) const
{
    Debug(DEBUGTYPE_6, "Game/Objects/Base/SimpleObjectTemplate.cpp", 413)
        << "Should not set PhysicsComponent but PhysicsNodeComponent!";
}

// ; PE 0x5C3DD0. Symmetric diagnostic for the obsolete removal path; the live physics
// node is removed through ObjectTemplate's separate physics-node component slot instead.
void SimpleObjectTemplate::removePhysicsComponent(IObject*) const
{
    Debug(DEBUGTYPE_6, "Game/Objects/Base/SimpleObjectTemplate.cpp", 489)
        << "Removing physicsComponent, not physicsNodeComponent!";
}

// Physics binding: ELF 0x81F2360 (node) and 0x81F2010 (response).
// Both return unless hasCollisionPhysics (0x04) is set. Type/mobile/soldier gates choose
// the concrete node and response classes; preserve the differing bind/release/setObject order.
// Mobile nodes receive tuning and manager registration. Type 4 applies the platform overrides.
// The response manager is required: its missing-manager diagnostic is process-fatal.
void SimpleObjectTemplate::setPhysicsNodeComponent(IObject* object) const
{
    if ((m_physicsFlags & 0x04) == 0)          // hasCollisionPhysics - recovered, exact
        return;

    buildPhysicsNode(object);
}

void SimpleObjectTemplate::setResponsePhysicsComponent(IObject* object) const
{
    if ((m_physicsFlags & 0x04) == 0)          // the identical gate, read from its own bytes
        return;

    buildResponsePhysics(object);
}

extern ResponsePhysicsManager* responsePhysicsManager;   // ds:0x8B27848, Core/Globals.cpp - attested

// Selects and attaches physics components. Platforms take the mobile-class branch even when the mobile bit is clear.
// Node attachment sets the object before storing the component; response attachment sets it afterward.
void SimpleObjectTemplate::buildPhysicsNode(IObject* object) const
{
    const bool isSoldier =
        const_cast<SimpleObjectTemplate*>(this)->queryImplementation(CID_SoldierTemplate) != 0;

    if (isSoldier || (m_physicsFlags & 0x02) || m_physicsType == 4)
    {
        buildMobilePhysicsNode(object);        // the four mobile node classes - below
        return;
    }
    if (m_physicsType != 3)                    // exact: other static types build nothing
        return;

    IPhysicsNode* node = new StaticPhysicsNode;
    node->setObject(object);
    object->setComponent(IID_IPhysicsNode, node);
    node->release();                           // setComponent holds the reference now

    if (m_treeCollisionDiameter > 0.0f)
        node->setTreeCollisionRadius(m_treeCollisionDiameter * 0.5f);
}

void SimpleObjectTemplate::buildResponsePhysics(IObject* object) const
{
    const bool isSoldier =
        const_cast<SimpleObjectTemplate*>(this)->queryImplementation(CID_SoldierTemplate) != 0;

    if (isSoldier || (m_physicsFlags & 0x02) || m_physicsType == 4)
    {
        buildMobileResponsePhysics(object);    // the four response twins - below
        return;
    }
    if (m_physicsType != 3)
        return;

    IResponsePhysics* rp = new StaticResponsePhysics;
    object->setComponent(IID_IResponsePhysics, rp);
    rp->release();
    rp->setObject(object);
    rp->setPermanentGrip(static_cast<unsigned char>(m_grip));   // shipped no-op on statics
    rp->addToCollisionGroup(m_collisionGroups);

    if (responsePhysicsManager)
        responsePhysicsManager->addObject(object);
    else
        Debug(DEBUGTYPE_6, "Game/Objects/Base/SimpleObjectTemplate.cpp", 482, __FUNCTION__)
            << "No responsePhysicsManager allocated.";          // type 6 = process-fatal
}

extern PhysicsNodeManager* physicsNodeManager;          // ds:0x8B27844, Core/Globals.cpp - attested

// Builds soldier, point, rotational-point, mesh or platform physics according to template flags and type.
// Non-mobile platforms retain default node tuning and skip manager registration.
// [CHOSEN, not recovered] Helpers separate branches that were originally in one function.
void SimpleObjectTemplate::buildMobilePhysicsNode(IObject* object) const
{
    const bool isSoldier =
        const_cast<SimpleObjectTemplate*>(this)->queryImplementation(CID_SoldierTemplate) != 0;

    IPhysicsNode* node;
    if (isSoldier)
        node = new SoldierPhysicsNode;
    else if (m_physicsFlags & 0x02)
    {
        switch (m_physicsType)
        {
        case 1:  node = new PointPhysicsNode;            break;
        case 2:  node = new RotationalPointPhysicsNode;  break;
        case 3:  node = new PhysicsNode;                 break;
        case 4:  node = new PhysicsNode; node->setIsPlatformPhysics(true); break;
        default: return;                                 // exact: no component
        }
    }
    else
    {
        if (m_physicsType != 4)                          // the static arm handled 3; nothing else builds
            return;
        node = new PhysicsNode;
        node->setIsPlatformPhysics(true);
    }

    node->setObject(object);
    object->setComponent(IID_IPhysicsNode, node);
    node->release();
    if (m_treeCollisionDiameter > 0.0f)
        node->setTreeCollisionRadius(m_treeCollisionDiameter * 0.5f);

    if ((m_physicsFlags & 0x02) || isSoldier)
    {
        object->updateFlags(8, 0);
        node->setDrag(m_drag);
        node->setDragOffset(m_dragOffset);
        node->setDragModifier(m_dragModifier);
        node->setMass(m_mass);
        node->setCenterOfMassOffset(m_centerOfMassOffset);
        node->setCenterOfCollisionOffset(m_centerOfCollisionOffset);
        node->setInertiaModifier(m_inertiaModifier);
        node->setGravityModifier(m_gravityModifier);
        if (!isSoldier)
        {
            if ((m_physicsFlags & 0x01) == 0 && gameLogic->isServer())
                node->setIsAwake();
            else
                node->setIsSleeping();
        }
        node->setHandleCollisionSpeed(m_handleCollisionSpeed);
        node->setFloaterMod(m_floaterMod);
        if (m_otherCollisionLod != -1)
            node->setOtherCollisionLod(m_otherCollisionLod);
        if (node->getIsPlatformPhysics())
        {
            node->setMass(2000000.0f);
            node->setGravityModifier(0.0f);
            node->setIsSleeping();
        }
        physicsNodeManager->addPhysicsNode(node);
    }
}

void SimpleObjectTemplate::buildMobileResponsePhysics(IObject* object) const
{
    const bool isSoldier =
        const_cast<SimpleObjectTemplate*>(this)->queryImplementation(CID_SoldierTemplate) != 0;

    IResponsePhysics* rp;
    if (isSoldier)
        rp = new SoldierResponsePhysics;
    else if (m_physicsFlags & 0x02)
    {
        switch (m_physicsType)
        {
        case 1:  rp = new PointResponsePhysics;            break;
        case 2:  rp = new RotationalPointResponsePhysics;  break;
        case 3:
        case 4:  rp = new ResponsePhysics;                 break;
        default: return;
        }
    }
    else
    {
        if (m_physicsType != 4)
            return;
        rp = new ResponsePhysics;
    }

    object->setComponent(IID_IResponsePhysics, rp);
    rp->release();
    rp->setObject(object);
    rp->setPermanentGrip(static_cast<unsigned char>(m_grip));
    rp->addToCollisionGroup(m_collisionGroups);

    if (responsePhysicsManager)
        responsePhysicsManager->addObject(object);
    else
        Debug(DEBUGTYPE_6, "Game/Objects/Base/SimpleObjectTemplate.cpp", 482, __FUNCTION__)
            << "No responsePhysicsManager allocated.";
}

// Creates the named template component, attaches it, and releases the factory reference.
// An unknown class silently leaves the component map unchanged.
void SimpleObjectTemplate::createComponent(const std::string& name)
{
    const std::string fullName = std::string("dice.hfe.world.TemplateComponent.") + name;
    IBase* component = classManager->createInstance(fullName, IID_IObjectTemplateComponent, 0);   // +0x38
    if (component)
    {
        // Components are keyed by interface ID for dotted console-property lookup.
        const unsigned int interfaceId = static_cast<IObjectTemplateComponent*>(component)->getInterfaceID();   // slot +0x14
        setComponent(interfaceId, component);                                      // ObjectTemplate +0x58
        component->release();
    }
}

// Creates a temporary named component to obtain its interface id, removes that map entry, then releases the temporary.
// Unknown classes leave the map unchanged.
void SimpleObjectTemplate::deleteComponent(const std::string& name)
{
    const std::string fullName = std::string("dice.hfe.world.TemplateComponent.") + name;
    IBase* component = classManager->createInstance(fullName, IID_IObjectTemplateComponent, 0);   // +0x38
    if (component)
    {
        const unsigned int interfaceId = static_cast<IObjectTemplateComponent*>(component)->getInterfaceID();   // slot +0x14
        setComponent(interfaceId, 0);                                              // ObjectTemplate +0x58 - null erases
        component->release();                                                      // +0x08
    }
}

void SimpleObjectTemplate::setGeometry(const std::string& geometry)
{
    m_geometry = geometry;
}

const std::string& SimpleObjectTemplate::getGeometry()
{
    return m_geometry;
}

void SimpleObjectTemplate::setGeometryPart(int part)
{
    m_geometryPart = part;
}

void SimpleObjectTemplate::setCollisionPart(int part)
{
    m_collisionPart = part;
}

void SimpleObjectTemplate::mapMaterial(
    unsigned int index,
    const std::string& name,
    unsigned int material)
{
    if (index < m_materialVector.size())
    {
        // The original deliberately preserves the existing name on an in-range remap.
        m_materialVector[index].second = material;
        return;
    }

    m_materialVector.resize(index + 1);
    m_materialVector[index] = std::make_pair(name, material);
}

std::vector<std::pair<std::string, unsigned int> >*
SimpleObjectTemplate::getMaterialVector()
{
    return &m_materialVector;
}

void SimpleObjectTemplate::setMaterial(int material)
{
    m_material = material;
}

int SimpleObjectTemplate::getMaterial() const
{
    return m_material;
}

void SimpleObjectTemplate::setCollisionMesh(const std::string& mesh)
{
    m_collisionMesh = mesh;
    // Both binaries dispatch ICollisionManager::create directly, without a null check or QI.
    collisionManager->create(m_collisionMesh);
}

const std::string& SimpleObjectTemplate::getCollisionMesh() const
{
    return m_collisionMesh;
}

void SimpleObjectTemplate::setRelativePositionOffset(const Vec3& offset)
{
    m_relativePositionOffset = offset;
}

Vec3 SimpleObjectTemplate::getRelativePositionOffset()
{
    return m_relativePositionOffset;
}

const Vec3& SimpleObjectTemplate::getAnchor() const
{
    return m_anchor;
}

void SimpleObjectTemplate::setAnchor(const Vec3& anchor)
{
    m_anchor = anchor;
}

const Vec3& SimpleObjectTemplate::getAnchorOffset() const
{
    return m_anchorOffset;
}

void SimpleObjectTemplate::setAnchorOffset(const Vec3& offset)
{
    m_anchorOffset = offset;
}

#if DEDICATED
// Dedicated images call a null geometry manager without a guard. Preserve that shipped fault.
// [PLATFORM] ELF returns the transformed Vec3 by value; keep this separate from the renderer's output-pointer signature.
Vec3 SimpleObjectTemplate::getAnchorPos() const
{
#if defined(_MSC_VER)
    typedef void* (__thiscall* FindGeometry)(void*, const std::string&);
    typedef Vec3 (__thiscall* TransformAnchor)(void*, const Vec3&);
#else
    typedef void* (*FindGeometry)(void*, const std::string&);
    typedef Vec3 (*TransformAnchor)(void*, const Vec3&);
#endif
    void** managerVtable = *reinterpret_cast<void** volatile*>(0);
    geom::IGeometryTemplate* geometry = static_cast<geom::IGeometryTemplate*>(
        reinterpret_cast<FindGeometry>(managerVtable[0x18 / 4])(0, m_geometry));
    if (geometry && geometry->getCompressVertexData())
    {
#if defined(_MSC_VER)
        // [PLATFORM] MSVC evaluates the offset getter before the transformed-anchor operand.
        const Vec3& offset = getAnchorOffset();
        const Vec3 anchor = reinterpret_cast<TransformAnchor>(
            (*reinterpret_cast<void***>(geometry))[0xA8 / 4])(geometry, getAnchor());
        return Vec3{anchor.x + offset.x, anchor.y + offset.y, anchor.z + offset.z};
#else
        const Vec3 anchor = reinterpret_cast<TransformAnchor>(
            (*reinterpret_cast<void***>(geometry))[0xA8 / 4])(geometry, getAnchor());
        const Vec3& offset = getAnchorOffset();
        return Vec3{anchor.x + offset.x, anchor.y + offset.y, anchor.z + offset.z};
#endif
    }
#if defined(_MSC_VER)
    const Vec3& offset = getAnchorOffset();
    const Vec3& anchor = getAnchor();
    return Vec3{anchor.x + offset.x, anchor.y + offset.y, anchor.z + offset.z};
#else
    const Vec3& anchor = getAnchor();
    const Vec3& offset = getAnchorOffset();
    return Vec3{anchor.x + offset.x, anchor.y + offset.y, anchor.z + offset.z};
#endif
}
#else
// Returns the client anchor after geometry decompression, plus its offset. Requires the geometry-template manager.
// Compressed meshes may quantize the anchor; missing geometry uses the authored anchor directly.
// [PLATFORM] Client calls getAnchorOffset before getAnchor; dedicated code uses the opposite order.
Vec3 SimpleObjectTemplate::getAnchorPos() const
{
#if !DEDICATED && defined(_MSC_VER)
    geom::IGeometryTemplateManager* const manager =
        static_cast<geom::IGeometryTemplateManager*>(::dice::hfe::g_geometryTemplateManager);
    geom::IGeometryTemplate* const geometry = manager->getTemplate(m_geometry);
#else
    // The manager header carries x86 renderer layouts; other builds retain this lookup adapter.
#if defined(_MSC_VER) && defined(_M_IX86)
    typedef IBase* (__thiscall* GetTemplateFn)(void*, const std::string&);
#else
    typedef IBase* (*GetTemplateFn)(void*, const std::string&);
#endif
    void* const manager = ::dice::hfe::g_geometryTemplateManager;
    geom::IGeometryTemplate* const geometry = static_cast<geom::IGeometryTemplate*>(
        reinterpret_cast<GetTemplateFn>((*reinterpret_cast<void***>(manager))[0x18 / 4])(
            manager, m_geometry));
#endif
    if (geometry && geometry->getCompressVertexData())
    {
        const Vec3& offset = getAnchorOffset();                          // +0xA4 first
        const Vec3& anchor = getAnchor();                                // +0x9C
        Vec3 scratch;
        const Vec3* const quantized = geometry->decompressPosition(&scratch, &anchor);
        return Vec3{offset.x + quantized->x, offset.y + quantized->y, offset.z + quantized->z};
    }
    const Vec3& offset = getAnchorOffset();                              // +0xA4 first
    const Vec3& anchor = getAnchor();                                    // +0x9C
    return Vec3{anchor.x + offset.x, anchor.y + offset.y, anchor.z + offset.z};
}
#endif

void SimpleObjectTemplate::setDestroyableWindowsTemplate(const std::string&)
{
    // Intentional five-byte no-op in this base; a destroyable-window subclass overrides it.
}

// Updates the mobile-physics bit while preserving every other physics flag.
void SimpleObjectTemplate::setHasMobilePhysics(bool mobile)
{
    m_physicsFlags = static_cast<unsigned char>(
        (m_physicsFlags & 0xFD) | ((mobile ? 1 : 0) << 1));
}

// 22 bytes: `movzx eax,[eax+0ACh] / shr al,1 / and al,1`.
// The shift amount is what identifies the bit — the same technique works for any bitfield accessor
// in this binary.
bool SimpleObjectTemplate::getHasMobilePhysics() const
{
    return ((m_physicsFlags >> 1) & 1) != 0;
}

// Resolves physics type from flags in priority order.
// If no type bit is set, preserves the current type instead of resetting it.
void SimpleObjectTemplate::checkPhysicsTypes()
{
    const unsigned char flags = m_physicsFlags;

    if ((flags >> 3) & 1) { m_physicsType = 1; return; }
    if ((flags >> 4) & 1) { m_physicsType = 2; return; }
    if ((flags >> 5) & 1) { m_physicsType = 3; return; }
    if ((flags >> 6) & 1) { m_physicsType = 4; return; }

    // No type bit set: return without writing m_physicsType. See warning 2 above.
}

// The bundle-sound pair - for the LadderContainer chain (every PCO-family
// template runs these twice from templateInitialize, at createTemplate time).

// 117 bytes, const. The naming rule for a bundle's sound child: "S_<owner>_<suffix>".
std::string SimpleObjectTemplate::makeSoundTemplateName(const std::string& suffix) const
{
    std::string name = "S_";
    name += getName();
    name += "_";
    name += suffix;
    return name;
}

// 142 bytes. Creates the Sound template (classId 9986001 - the id BfSoundModule::init
// registers as "Sound") named by the rule above, sets flag 0x400 on it, and returns it.
ObjectTemplate* SimpleObjectTemplate::addSoundTemplate(const std::string& name)
{
    ObjectTemplate* soundTemplate = static_cast<ObjectTemplate*>(
        objectTemplateManager->createTemplate(9986001u, makeSoundTemplateName(name)));
    soundTemplate->updateFlags(0x400, 0);
    return soundTemplate;
}

void* SimpleObjectTemplate::createSoundFromTemplate(const std::string& name, IObject* owner) const
{
#if DEDICATED
    return 0;
#else
    if (isDedicated() || !g_bfSound)
        return 0;

    SoundObjectTemplate* const objectTemplate = static_cast<SoundObjectTemplate*>(
        objectTemplateManager->getTemplate(makeSoundTemplateName(name), 9986001u));
    if (!objectTemplate)
        return 0;

    ISoundObjectTemplate* const soundTemplate = objectTemplate;
    if (!soundTemplate->impl()
        || !soundTemplate->impl()->getFilename()
        || !*soundTemplate->impl()->getFilename())
    {
        return 0;
    }
    return g_bfSound->createSound(soundTemplate->impl(), owner);
#endif
}

} // namespace world
} // namespace hfe
} // namespace dice
