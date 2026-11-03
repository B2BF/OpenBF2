// Path:     [INFERRED] - beside its header. Contents: recovered from bf2.i64.

#include "ParticleTemplate.h"
#include "../../../IO/Stream/IStream.h"
#include "../../../Core/OverTimeDistribution.h"
#include "../../../Core/StringUtil.h"
#include "../../../Physics/PhysicsType.h"
#include <sstream>
#include <cstring>
#include "Particle.h"                                   // createObject's product
#include "../../Main/Game.h"                            // isDedicated() - BF2.exe 0x440510
#include "../../../Scene/Object/ObjectTypes/Base/ObjectTemplateManager.h"   // getCollisionEffectTemplate

#include <cmath>
#include "../../../Core/Ids.h"
#include "../../../Physics/CollisionManager.h"    // setCollisionMesh registers through the manager
// The two physics binders - the same set SimpleObjectTemplate.cpp pulls for its sibling pair.
#include "../../../Physics/StaticPhysicsNode.h"
#include "../../../Physics/PointPhysicsNode.h"
#include "../../../Physics/RotationalPointPhysicsNode.h"
#include "../../../Physics/PhysicsNode.h"
#include "../../../Physics/StaticResponsePhysics.h"
#include "../../../Physics/PointResponsePhysics.h"
#include "../../../Physics/RotationalPointResponsePhysics.h"
#include "../../../Physics/ResponsePhysics.h"
#include "../../../Physics/PhysicsNodeManager.h"        // physicsNodeManager, addPhysicsNode
#include "../../../Physics/ResponsePhysicsManager.h"    // responsePhysicsManager, addObject
#include "../../../Core/Debug.h"                        // the no-manager Debug site

namespace dice {
namespace hfe {

// , read in full. Two Lehmer steps x' = a*x mod (2^31 - 1), a = 24112 then 26143,
// each in the classic overflow-free 16-bit-halves form, on the persistent state.
namespace {
inline unsigned int lehmerStep(unsigned int x, unsigned int factor)
{
    const unsigned int lo = (x & 0xFFFFu) * factor;
    const unsigned int hi = (x >> 16) * factor;
    const int carry = static_cast<int>(lo >> 16) + static_cast<int>(hi);
    unsigned int r = (lo & 0xFFFFu)
                   + ((static_cast<unsigned int>(carry) & 0x7FFFu) << 16)
                   + static_cast<unsigned int>(carry >> 15);
    const int reduced = static_cast<int>(r - 0x7FFFFFFFu);
    return reduced < 0 ? r : static_cast<unsigned int>(reduced);
}
}
unsigned int Random::getRaw()
{
    m_seed = lehmerStep(m_seed, 24112u);
    m_seed = lehmerStep(m_seed, 26143u);
    return m_seed;
}

// .  Marsaglia's polar Box-Muller transform with a function-static second sample:
// that cache is shared by all Random instances, exactly as the two ELF statics at 0x8AACDE8/cdec
// show.  Retail rejects only radii greater than one; it does not add the customary zero guard.
float Random::getNormal()
{
    static bool secondIsValid = false;
    static float second = 0.0f;

    if (secondIsValid)
    {
        secondIsValid = false;
        return second;
    }

    float firstUniform;
    float secondUniform;
    float radiusSquared;
    do
    {
        const unsigned int firstRaw = ((getRaw() >> 7) | 1u) + 1u;
        const float firstUnit = static_cast<float>(static_cast<int>(firstRaw))
                              * 0.000000059604645f;
        firstUniform = firstUnit + firstUnit - 1.0f;

        const unsigned int secondRaw = ((getRaw() >> 7) | 1u) + 1u;
        const float secondUnit = static_cast<float>(static_cast<int>(secondRaw))
                               * 0.000000059604645f;
        secondUniform = secondUnit + secondUnit - 1.0f;

        radiusSquared = firstUniform * firstUniform
                      + secondUniform * secondUniform;
    }
    while (radiusSquared > 1.0f);

    const float scale = std::sqrt(std::log(radiusSquared) * -2.0f / radiusSquared);
    secondIsValid = true;
    second = secondUniform * scale;
    return firstUniform * scale;
}

namespace world {

// Initializes distributions/curves, then overrides lifetime to 10, transparency to 0 and drag to 0.
// The default gravity sample is deterministically 1 and consumes no random draw.
ParticleTemplate::ParticleTemplate()
{
    m_refCount = 1;                               // +0x34
    m_geometryTemplate = 0;
    // m_geometry / m_collisionMesh: default-constructed empty.
    m_hasCollisionPhysics = false;
    m_collisionGroups = 56;
    m_hasPointPhysics = true; m_hasRotationalPointPhysics = false; m_hasResponsePhysics = false;
    m_physicsType = 1;                            // point physics by default
    m_hasMobilePhysics = false;                   // ...but static until a .con enables it
    m_permanentGrip = 2;
    m_geometryPart = -1;
    m_collisionPart = 0;
    m_dragOffset.x = 0.0f; m_dragOffset.y = 0.0f; m_dragOffset.z = 0.0f;
    m_mass = 0.001f;
    m_centerOfMassOffset.x = 0.0f; m_centerOfMassOffset.y = 0.0f; m_centerOfMassOffset.z = 0.0f;
    m_centerOfCollisionOffset.x = 0.0f; m_centerOfCollisionOffset.y = 0.0f;
    m_centerOfCollisionOffset.z = 0.0f;
    m_inertiaModifier.x = 1.0f; m_inertiaModifier.y = 1.0f; m_inertiaModifier.z = 1.0f;
    m_isSleeping = 0;                             // the ELF clears only bit 0 here (`and ..., 0FEh`):
                                                  // a one-bit field, the byte's other bits unwritten
    m_usedOnTransparentSurface = false;
    m_floaterMod = 0.0f;
    m_otherCollisionLod = -1;
    m_sizeModifier.x = 1.0f; m_sizeModifier.y = 1.0f; m_sizeModifier.z = 1.0f;
    m_timeToLive.value = 10.0f;                   // {CRD_NONE, 10, 0, false}
    // m_size: the default {CRD_NONE, 1, 0, false}
    m_transparency.value = 0.0f;                  // {CRD_NONE, 0, 0, false}
    // m_gravityModifier: the default {CRD_NONE, 1, 0, false}
    m_drag.value = 0.0f;                          // {CRD_NONE, 0, 0, false}

    // The four curves: OverTimeDistribution(1.0f) each - the seed replicated across every sample,
    // the authored mask zeroed - by their own construction.

    // m_materialVector: default-constructed empty.
    m_material = -1;
    // m_collisionEffectName: default-constructed empty.
    m_collisionEffectTemplate = 0;

    Vec4 t;
    t.x = 0.0f; t.y = 0.0f; t.z = 0.0f; t.w = 1.0f;
    setTransparencyGraph(t);

    // The sampler over m_gravityModifier. Mode 0 (the ctor's own state) is the deterministic arm;
    // modes 1-3 use the recovered Random distribution paths. `relative` selects the alternate path,
    // which adds a coin-flip sign on the result - transcribed from the read, unreachable at ctor
    // time.
    float sample = m_gravityModifier.value;
    if (!m_gravityModifier.relative)
    {
        switch (m_gravityModifier.distribution)
        {
        case 0:
            break;                                // sample = the value
        case 1:
            sample = m_gravityModifier.value + (m_gravityModifier.variation - m_gravityModifier.value)
                     * (float(((Rand.getRaw() >> 7) | 1) + 1) * 0.000000059604645f);
            break;
        case 2:
            sample = -m_gravityModifier.value
                     * std::log(float(((Rand.getRaw() >> 7) | 1) + 1) * 0.000000059604645f);
            break;
        case 3:
            sample = m_gravityModifier.value + Rand.getNormal() * m_gravityModifier.variation;
            break;
        default:
            sample = 0.0f;
            break;
        }
    }
    else
    {
        switch (m_gravityModifier.distribution)
        {
        case 0:
            break;
        case 1:
            sample = m_gravityModifier.value + (m_gravityModifier.variation - m_gravityModifier.value)
                     * (float(((Rand.getRaw() >> 7) | 1) + 1) * 0.000000059604645f);
            break;
        case 2:
            sample = -m_gravityModifier.value
                     * std::log(float(((Rand.getRaw() >> 7) | 1) + 1) * 0.000000059604645f);
            break;
        case 3:
            sample = m_gravityModifier.value + Rand.getNormal() * m_gravityModifier.variation;
            break;
        default:
            sample = 0.0f;
            break;
        }
        // The coin flip: u <= 0.5 negates.
        if (float(((Rand.getRaw() >> 7) | 1) + 1) * 0.000000059604645f <= 0.5f)
            sample = -sample;
    }
    m_sampledValue = sample;
}

// (D1): strings + the material vector - the compiler's job in our build.
ParticleTemplate::~ParticleTemplate()
{
}

// The per-class quartet - refcount +0x34, delete on the 1->0 edge through the D0 slot.
void ParticleTemplate::addRef()  { ++m_refCount; }
int  ParticleTemplate::getRef()  { return m_refCount; }

int ParticleTemplate::release()
{
    const int r = --m_refCount;
    if (r == 0)
        delete this;
    return r;
}

// IBase/IObjectTemplate -> this, IConsoleSaveable -> +8, the particle interface -> +0x30.
void* ParticleTemplate::queryInterface(unsigned int iid) const
{
    ParticleTemplate* self = const_cast<ParticleTemplate*>(this);
    if (iid == IID_IBase || iid == IID_IObjectTemplate)
        return static_cast<IObjectTemplate*>(self);
    if (iid == io::IID_IConsoleSaveable)
        return static_cast<io::IConsoleSaveable*>(self);
    if (iid == IID_IParticleTemplate)
        return static_cast<IParticleTemplate*>(self);
    return 0;
}

unsigned int ParticleTemplate::getClassID() const { return CID_ParticleTemplate; }

// flat two-CID check, no chain.
void* ParticleTemplate::queryImplementation(unsigned int classId)
{
    if (classId == CID_ParticleTemplate || classId == CID_ObjectTemplate)
        return this;
    return 0;
}

// Creates particles on clients; dedicated builds return null.
IObject* ParticleTemplate::createObject() const
{
    if (isDedicated())
        return 0;
    return new Particle(this);
}

// The two manager singletons, declared per-TU exactly as SimpleObjectTemplate.cpp declares them -
// Core/Globals.cpp defines both, and an extern minted in the wrong scope would be a wrong-scope
// twin rather than the global (the SimpleObject.cpp lesson).
extern PhysicsNodeManager* physicsNodeManager;          // ds:0x8B27844 - attested
extern ResponsePhysicsManager* responsePhysicsManager;  // ds:0x8B27848 - attested

// Attaches physics at object spawn, selecting the full PhysicsNode as the default type.
// Node setup uses sleepiness instead of SimpleObjectTemplate's awake/sleep pair; response setup has no platform/soldier cases.
// Original TU is attested by __FILE__ as Particle.cpp; this split file's placement is [INFERRED].
void ParticleTemplate::setPhysicsNodeComponent(IObject* object) const
{
    IPhysicsNode* node;
    if (m_hasMobilePhysics)
    {
        if (m_physicsType == 2)
            node = new RotationalPointPhysicsNode;               // 0xE0
        else if (m_physicsType == 1)
            node = new PointPhysicsNode;                         // 0x90
        else
            node = new PhysicsNode;                              // 0x22C
    }
    else
    {
        node = new StaticPhysicsNode;                            // 0x1C
    }

    // The mass is floored, not clamped: a zero or negative template value becomes 0.001f.
    const float mass = (m_mass > 0.0f) ? m_mass : 0.001f;

    node->setObject(object);                                     // +0x28
    object->setComponent(IID_IPhysicsNode, node);                // +0x58
    node->release();                                             // the object took the reference
    object->updateFlags(8, 0);
    node->setDragOffset(m_dragOffset);                           // +0xC8
    node->setMass(mass);                                         // +0xD8
    node->setCenterOfMassOffset(m_centerOfMassOffset);           // +0xE0
    node->setCenterOfCollisionOffset(m_centerOfCollisionOffset); // +0xE8
    node->setInertiaModifier(m_inertiaModifier);                 // +0xF0
    node->setGravityModifier(m_sampledValue);                    // +0xF8
    node->setFloaterMod(m_floaterMod);                           // +0x164
    node->setSleepiness(m_isSleeping ? 0 : 100);                 // +0x124
    if (m_otherCollisionLod != -1)
        node->setOtherCollisionLod(m_otherCollisionLod);          // +0x198
    physicsNodeManager->addPhysicsNode(node);
}

// note THE order: setComponent and release come before setObject here, where the node arm above
// does setObject first. Both are shipped; neither was normalised to the other.
void ParticleTemplate::setResponsePhysicsComponent(IObject* object) const
{
    IResponsePhysics* rp;
    if (m_hasMobilePhysics)
    {
        if (m_physicsType == 2)
            rp = new RotationalPointResponsePhysics;             // 0x94
        else if (m_physicsType == 1)
            rp = new PointResponsePhysics;                       // 0x90
        else
            rp = new ResponsePhysics;                            // 0x19C
    }
    else
    {
        rp = new StaticResponsePhysics;                          // 0x28
    }

    object->setComponent(IID_IResponsePhysics, rp);              // +0x58
    rp->release();
    rp->setObject(object);                                       // +0x3C
    rp->setPermanentGrip(static_cast<unsigned char>(m_permanentGrip));   // +0x84 read as a byte
    rp->addToCollisionGroup(m_collisionGroups);                  // +0x4C

    if (responsePhysicsManager)
        responsePhysicsManager->addObject(object);
    else
        Debug(DEBUGTYPE_6, "Game/Objects/Fx/Particle.cpp", 740, __FUNCTION__)
            << "No responsePhysicsManager allocated.";
}

// assign, then register with the collision manager: the same create() route
// SimpleObjectTemplate::setCollisionMesh takes, against ParticleTemplate's own field.
void ParticleTemplate::setCollisionMesh(const std::string& mesh)
{
    m_collisionMesh = mesh;
    collisionManager->create(m_collisionMesh);
}

const std::string& ParticleTemplate::getCollisionMesh() const { return m_collisionMesh; }

std::vector<std::pair<std::string, unsigned int> >* ParticleTemplate::getMaterialVector()
{
    return &m_materialVector;
}

void ParticleTemplate::setMaterial(int material) { m_material = material; }

// the lazy resolve: -1 means "scan the vector for the first entry with a non-empty name
// and cache its value (or 0 when none)". As shipped, including the 0 fallback overwriting the -1.
int ParticleTemplate::getMaterial() const
{
    if (m_material != -1)
        return m_material;
    ParticleTemplate* self = const_cast<ParticleTemplate*>(this);
    self->m_material = 0;
    for (std::vector<std::pair<std::string, unsigned int> >::const_iterator it =
             m_materialVector.begin(); it != m_materialVector.end(); ++it)
    {
        if (it->first.length())
        {
            self->m_material = int(it->second);
            break;
        }
    }
    return m_material;
}

void ParticleTemplate::setGeometry(const std::string& geometry) { m_geometry = geometry; }
const std::string& ParticleTemplate::getGeometry()              { return m_geometry; }
void ParticleTemplate::setGeometryPart(int part)                { m_geometryPart = part; }
void ParticleTemplate::setCollisionPart(int part)               { m_collisionPart = part; }

// grow the pair-vector to cover the index (empty-name fill), then overwrite both the
// name and the value at the slot. different from SimpleObjectTemplate::mapMaterial, which
// leaves an in-range entry's name alone - the extender's "separate implementations" warning,
// confirmed.
void ParticleTemplate::mapMaterial(unsigned int index, const std::string& name,
                                   unsigned int material)
{
    unsigned int want = unsigned(m_materialVector.size());
    if (want < index + 1)
        want = index + 1;
    m_materialVector.resize(want, std::pair<std::string, unsigned int>(std::string(), 0));
    m_materialVector[index].first = name;
    m_materialVector[index].second = material;
}

// The non-virtual +0x7C8 Vec4 pair (0x820B9B0 / 0x820BA00).
void ParticleTemplate::setTransparencyGraph(const Vec4& v) { m_transparencyGraph = v; }
const Vec4& ParticleTemplate::getTransparencyGraph() const { return m_transparencyGraph; }

// Selects the first enabled physics type, retaining the current type when none is enabled.
void ParticleTemplate::checkPhysicsTypes()
{
    if (m_hasPointPhysics)
        m_physicsType = 1;
    else if (m_hasRotationalPointPhysics)
        m_physicsType = 2;
    else if (m_hasResponsePhysics)
        m_physicsType = 3;
}

// (BF2.exe inlines it at 0x64B5C0): resolve collisionEffectName through the
// template manager once and keep the answer; a null answer is simply retried on the next call.
const IObjectTemplate* ParticleTemplate::getCollisionEffectTemplate() const
{
    if (!m_collisionEffectTemplate && !m_collisionEffectName.empty())
        m_collisionEffectTemplate = objectTemplateManager->getTemplate(m_collisionEffectName);   // +0x24 PE +0x34
    return m_collisionEffectTemplate;
}

// Serializes random distributions and sampled curves through their typed interfaces.
bool ParticleTemplate::makeScript(io::IStream* stream)
{
    std::ostringstream out;
    if (!ObjectTemplate::makeScript(stream)) return false;
    if (!m_geometry.empty()) out << "ObjectTemplate.geometry " << m_geometry << std::endl;
    out << "ObjectTemplate.usedOnTransparentSurface " << (m_usedOnTransparentSurface != 0) << std::endl;
    if (!m_collisionMesh.empty())
    {
        out << "ObjectTemplate.collisionMesh " << extractFileNameWithoutExt(m_collisionMesh) << std::endl;
        for (unsigned int i = 0; i < m_materialVector.size(); ++i)
            out << "ObjectTemplate.mapMaterial " << i << ' ' << m_materialVector[i].first << ' ' << m_materialVector[i].second << std::endl;
    }
    if (m_collisionGroups != 16u) out << "ObjectTemplate.collisionGroups " << m_collisionGroups << std::endl;
    if (!m_collisionEffectName.empty()) out << "ObjectTemplate.collisionEffectName " << m_collisionEffectName << std::endl;
    if (m_dragOffset.x != 0.0f || m_dragOffset.y != 0.0f || m_dragOffset.z != 0.0f)
        out << "ObjectTemplate.dragOffset " << m_dragOffset.x << '/' << m_dragOffset.y << '/' << m_dragOffset.z << std::endl;
    if (m_mass != 0.001f) out << "ObjectTemplate.mass " << m_mass << std::endl;
    // Shipped AND: changing only one inertia component does not emit the vector.
    if (m_inertiaModifier.x != 1.0f && m_inertiaModifier.y != 1.0f && m_inertiaModifier.z != 1.0f)
        out << "ObjectTemplate.inertiaModifier " << m_inertiaModifier.x << '/' << m_inertiaModifier.y << '/' << m_inertiaModifier.z << std::endl;
    const double collisionLengthSquared = static_cast<double>(m_centerOfCollisionOffset.x) * m_centerOfCollisionOffset.x
        + static_cast<double>(m_centerOfCollisionOffset.y) * m_centerOfCollisionOffset.y
        + static_cast<double>(m_centerOfCollisionOffset.z) * m_centerOfCollisionOffset.z;
    if (collisionLengthSquared != 0.0)
        out << "ObjectTemplate.centerOfCollisionOffset " << m_centerOfCollisionOffset.x << '/' << m_centerOfCollisionOffset.y << '/' << m_centerOfCollisionOffset.z << std::endl;
    if (m_hasMobilePhysics) out << "ObjectTemplate.hasMobilePhysics " << (m_hasMobilePhysics != 0) << std::endl;
    if (m_physicsType != 1) out << "ObjectTemplate.physicsType " << static_cast<PhysicsType>(m_physicsType) << std::endl;
    if (m_isSleeping) out << "ObjectTemplate.isSleeping " << (m_isSleeping & 1) << std::endl;
    if (m_permanentGrip != 2) out << "ObjectTemplate.grip " << m_permanentGrip << std::endl;
    if (m_hasCollisionPhysics) out << "ObjectTemplate.hasCollisionPhysics " << (m_hasCollisionPhysics != 0) << std::endl;
    RandomDistribution& timeToLive = m_timeToLive;
    RandomDistribution& size = m_size;
    RandomDistribution& gravityModifier = m_gravityModifier;
    RandomDistribution& drag = m_drag;
    RandomDistribution& transparency = m_transparency;
    if (timeToLive.distribution != 0 || timeToLive.value != 10.0f)
        out << "ObjectTemplate.timeToLive " << static_cast<const RandomDistribution&>(timeToLive) << std::endl;
    if (size.distribution != 0 || size.value != 1.0f)
        out << "ObjectTemplate.size " << static_cast<const RandomDistribution&>(size) << std::endl;
    if (gravityModifier.distribution != 0 || gravityModifier.value != 1.0f)
        out << "ObjectTemplate.gravityModifier " << gravityModifier << std::endl;
    if (m_floaterMod != 0.0f) out << "ObjectTemplate.floaterMod " << m_floaterMod << std::endl;
    if (drag.distribution != 0 || drag.value != 0.0f)
        out << "ObjectTemplate.drag " << drag << std::endl;
    if (m_sizeModifier.x != 0.0f || m_sizeModifier.y != 0.0f || m_sizeModifier.z != 0.0f)
        out << "ObjectTemplate.sizeModifier " << m_sizeModifier.x << '/' << m_sizeModifier.y << '/' << m_sizeModifier.z << std::endl;
    // Curve order is alpha, size, gravity, drag.
    const OverTimeDistribution* const graphs[4] = { &m_alphaOverTime, &m_sizeOverTime,
                                                    &m_gravityModifierOverTime, &m_dragOverTime };
    // Retail probes each graph using a separate stream, then serializes it again if nonempty.
    std::ostringstream sizeOverTime;
    sizeOverTime << *graphs[1];
    if (!sizeOverTime.str().empty()) out << "ObjectTemplate.sizeOverTime " << *graphs[1] << std::endl;
    std::ostringstream gravityOverTime;
    gravityOverTime << *graphs[2];
    if (!gravityOverTime.str().empty()) out << "ObjectTemplate.gravityModifierOverTime " << *graphs[2] << std::endl;
    std::ostringstream dragOverTime;
    dragOverTime << *graphs[3];
    if (!dragOverTime.str().empty()) out << "ObjectTemplate.dragOverTime " << *graphs[3] << std::endl;
    std::ostringstream alphaOverTime;
    alphaOverTime << *graphs[0];
    if (!alphaOverTime.str().empty()) out << "ObjectTemplate.alphaOverTime " << *graphs[0] << std::endl;
    out << "ObjectTemplate.transparencyGraph " << m_transparencyGraph.x << '/' << m_transparencyGraph.y << '/' << m_transparencyGraph.z << '/' << m_transparencyGraph.w << std::endl;
    out << "ObjectTemplate.transparency " << static_cast<const RandomDistribution&>(transparency) << std::endl;
    const std::string text = out.str();
    return stream->write(text.c_str(), static_cast<unsigned int>(text.size())) == text.size();
}

} // namespace world
} // namespace hfe
} // namespace dice
