// Client-instantiated physics debris; original source path is attested and also holds ParticleTemplate.
// [PLATFORM] Follows client relative-sign draw order and unguarded collision-mesh access.
// Geometry setup is client-only. Preserve NaN lifetime handling and fatal checks for invalid lifetime,
// unsupported light-source attachment, rejected mesh attachment and invalid geometry part.

#include "Particle.h"
#include "ParticleTemplate.h"
#include "Emitter.h"                               // fx2_emitter's world::Emitter (playParticleCollisionSound)
#include "../GeometryPartAccess.h"                 // geometry_part::count / setTransform / slot
#include "../../IGameLogic.h"                      // gameLogic->playCollisionEffect (+0x110)
#include "../../../Core/ClassManager.h"
#include "../../../Core/Debug.h"
#include "../../../Core/Ids.h"
#include "../../../Core/Random.h"                  // Rand, RandomDistribution
#include "../../../Geom/GeometryInstanceData.h"    // the two opacity factors + setOpacity
#include "../../../Physics/CollisionManager.h"     // ICollisionManager::getCollisionMeshPart
#include "../../../Physics/ICollisionMesh.h"       // ICollisionMesh::createMaterialMapping
#include "../../../Physics/IPhysicsNode.h"         // setGravityModifier (+0xF8) / setDrag (+0xC0)
#include "../../../Scene/Object/ObjectManager.h"   // objectManager: postMessage, the id registry
#include "../../../Scene/Object/ObjectTypes/Base/ObjectTemplateManager.h"
#include <cmath>
#include <map>
#include <string>

namespace dice {
namespace hfe {

extern ClassManager* classManager;                 // declared per TU, as SimpleObject.cpp does
#if !DEDICATED
// Game/Main/BF2Render.cpp defines it at exactly this scope (BF2.exe ds:0x9A5578), published after
// RendDX9's initDll. Declared here, as SimpleObject.cpp declares it, rather than through the
// client-side BF2ClientSeams.h, because this translation unit is shared by both images.
extern IBase* g_geometryTemplateManager;
#endif

namespace world {

int Particle::mNoParticlesDebug = 0;

namespace {

// geom::IScaleable - the IBase quartet plus setScale at +0x10, exactly the view GenericProjectile.cpp
// declares for the same IID. ELF 0x82090A4 / PE 0x64A1B1 call +0x10 with the address of a Vec3.
class IScaleable : public IBase
{
public:
    virtual void setScale(const Vec3& scale) = 0;
};

// the RandomDistribution sampler, client order
// BF2.exe 0x5E74B0 (121 B) on the process-global Random (ds:977F88; the ELF's Rand 0x8ABBCB0, this
// tree's Rand): when `relative` is set it draws the sign first - u <= 0.5 (compared as a double
// 0.5) gives -1 - and only then 0x5E7450 draws the value; the product is the result. [PLATFORM] The
// ELF constructor inlines the same distribution five times with the draws the other way round
// (value first, sign second: e.g. 0x82060BC after the arm at 0x8207000). Both use the global
// Random, never the one embedded in the distribution.
float unitRandom()
{
    // / 0x5E7360 / 0x5E74B0: `(raw | 0x80) sar 7, + 1` - equal to `((raw >> 7) | 1) + 1`
    // for every raw the Lehmer state can produce (< 2^31) - then * 2^-24.
    return static_cast<float>(static_cast<int>(((Rand.getRaw() >> 7) | 1u) + 1u))
         * 0.000000059604645f;
}

float sampleDistribution(const RandomDistribution& distribution)
{
    const float sign = (distribution.relative && unitRandom() <= 0.5f) ? -1.0f : 1.0f;
    float value;
    switch (distribution.distribution)                 // `cmp eax, 3; ja` -> 0.0
    {
    case CRD_NONE:
        value = distribution.value;
        break;
    case CRD_UNIFORM:                                  // (variation - value) * u + value
        value = (distribution.variation - distribution.value) * unitRandom() + distribution.value;
        break;
    case CRD_EXPONENTIAL:                              // -(ln u * value), fyl2x in x87
        value = static_cast<float>(-(std::log(static_cast<double>(unitRandom()))
                                     * distribution.value));
        break;
    case CRD_NORMAL:                                   // (getNormal) * variation + value
        value = Rand.getNormal() * distribution.variation + distribution.value;
        break;
    default:
        value = 0.0f;
        break;
    }
    return value * sign;
}

// the transparency graph
// BF2.exe 0x649C40 (59 B): a thiscall method on the template's Vec4 at PE +0x828 - so in dice's
// source the graph is a small class of its own whose name is [UNRESOLVED]; this free helper is
// [CHOSEN]. It evaluates the cubic x*t^3 + y*t^2 + z*t + w and clamps it through the shared
// (upper bound first, then the lower - so a NaN comes out as the lower bound 0).
// [PLATFORM] The ELF inlines it (0x82090B2..0x8209122) summing ((x t^3 + y t^2) + z t) + w and
// clamping lower first then upper with fcmov, which lets a NaN through unchanged.
float evaluateTransparencyGraph(const Vec4& graph, float t)
{
    const float t2 = t * t;
    const float t3 = t * t2;
    float value = t3 * graph.x + (t2 * graph.y + t * graph.z) + graph.w;
    if (value >= 1.0f)
        value = 1.0f;
    if (!(value >= 0.0f))                              // fcmovb: taken for NaN as well
        value = 0.0f;
    return value;
}

// the renderer geometry instance seams (the interface is not modelled on the host)
// Both slots are the same in the two images for the whole-geometry transform (+0x50) and one apart
// for the chain link (PE +0xCC / ELF +0xD0) - the RendDX9 BundledMesh table 0x1021B350 names them
// setGeometryTransformation (0x10103DB0, keeps the pointer) and getNextGeometry (0x101071C0).
#if defined(_MSC_VER) && defined(_M_IX86)
#define BF2_PARTICLE_GEOMETRY_CALL __thiscall
#else
#define BF2_PARTICLE_GEOMETRY_CALL
#endif

void setGeometryTransformation(void* geometry, const Mat4* transform)
{
    typedef void (BF2_PARTICLE_GEOMETRY_CALL* Function)(void*, const Mat4*);
    geometry_part::slot<Function>(geometry, 0x50, 0x50)(geometry, transform);
}

void* getNextGeometry(void* geometry)
{
    typedef void* (BF2_PARTICLE_GEOMETRY_CALL* Function)(void*);
    return geometry_part::slot<Function>(geometry, 0xCC, 0xD0)(geometry);
}

#if !DEDICATED
// the renderer geometry template seams, client only (PE offsets; no ELF counterpart)
// GeometryTemplateManager +0x18 getTemplate(name) and IGeometryTemplate +0x24 createInstance are
// the two SimpleObject.cpp's setGeometry already reaches this way. +0x48/+0x4C are the template's
// one-flag setter/getter pair (MeshTemplate base: nullsub 0x100E13B0 / `return 0` 0x10083AF0;
// BundledMeshTemplate 0x100E76E0 / 0x100E76F0 store/test its byte +0x125). [INFERRED] from the
// only caller, this one, which feeds them the particle template's usedOnTransparentSurface.
IBase* geometryTemplateManagerGetTemplate(IBase* manager, const std::string& name)
{
    typedef IBase* (BF2_PARTICLE_GEOMETRY_CALL* Function)(void*, const std::string&);
    return geometry_part::slot<Function>(manager, 0x18, 0x18)(manager, name);
}

IBase* geometryTemplateCreateInstance(IBase* geometryTemplate)
{
    typedef IBase* (BF2_PARTICLE_GEOMETRY_CALL* Function)(void*);
    return geometry_part::slot<Function>(geometryTemplate, 0x24, 0x24)(geometryTemplate);
}

void geometryTemplateSetUsedOnTransparentSurface(IBase* geometryTemplate, bool value)
{
    // BF2.exe pushes the whole dword it reloaded from a one-byte store (0x64A37F `mov edx,
    // [ebp-8]`), so the upper three bytes are stack garbage; every receiver reads the low byte.
    typedef void (BF2_PARTICLE_GEOMETRY_CALL* Function)(void*, int);
    geometry_part::slot<Function>(geometryTemplate, 0x48, 0x48)(geometryTemplate, value ? 1 : 0);
}

bool geometryTemplateGetUsedOnTransparentSurface(IBase* geometryTemplate)
{
    typedef bool (BF2_PARTICLE_GEOMETRY_CALL* Function)(void*);
    return geometry_part::slot<Function>(geometryTemplate, 0x4C, 0x4C)(geometryTemplate);
}
#endif

#undef BF2_PARTICLE_GEOMETRY_CALL

const char* const kParticleFile = "Game/Objects/Fx/Particle.cpp";   // BF2.exe passes dice's full
                                                                    // build path (0x9014A0)
} // anonymous namespace

// Particle::Particle - ELF C2 0x8205FC0 (C1 0x82071F0 is an identical clone), 4,643 B; BF2.exe
// . The size gap is entirely the five RandomDistribution samplers the ELF inlines
// (each ~700 B with its relative and plain arms) where BF2.exe calls 0x5E74B0; the statement
// sequence is the same in both, read in full:
// members -> ++mNoParticlesDebug -> an unused local string -> the five samples (with the
// negative-lifetime report) -> flags 0x2808 -> the physics-node binder -> OFHasCollision on or
// off -> the response binder -> the "_lightSource" probe -> the timed self-destroy message.
Particle::Particle(const ParticleTemplate* objectTemplate)
    : Object(objectTemplate),
      m_refCount(1),                               // +0x130 +0x148
      m_scaleable(0),                              // +0x134 +0x14C
      m_age(0.0f),                                 // +0x138 +0x150
      m_frameUpdated(0),                           // +0x150 +0x168
      m_lightSource(0),                            // +0x154 +0x16C
      m_emitterId(0xFFFFFFFFu),                    // +0x158 +0x170
      m_geometryPart(-1),                          // +0x15C +0x174
      m_assignedGeometryPart(-1),                  // +0x160 +0x178
      m_randomizeGeomPart(false)                   // +0x164 +0x17C
{
    ++mNoParticlesDebug;

    // [UNRESOLVED] A default-constructed local string lives for the whole body in both images
    // (ELF [ebp-28h], built at 0x8206062 and released at 0x820667E; PE [ebp-70h], 0x649D24 /
    // ) and nothing ever writes or reads it. Kept: the shipped body has it.
    std::string unused;

    m_timeToLive = sampleDistribution(objectTemplate->m_timeToLive);     // template +0xB4 / PE +0x114
    if (m_timeToLive < 0.0f)                       // ELF `fucompi; ja` / PE `fcomp; test ah,5; jp`
    {
        // Debug type 6 - aborts in this tree. See the file header: unreachable with stock content.
        Debug(DebugTypeError, kParticleFile, 79, "")
            << "mTimeToLive should be positive for particles";
        m_timeToLive = 0.5f;
    }
    m_size            = sampleDistribution(objectTemplate->m_size);             // +0xC8 +0x128
    m_transparency    = sampleDistribution(objectTemplate->m_transparency);     // +0xDC +0x13C
    m_gravityModifier = sampleDistribution(objectTemplate->m_gravityModifier);  // +0xF0 +0x150
    m_drag            = sampleDistribution(objectTemplate->m_drag);             // +0x104 +0x164

    // OFHasMobilePhysics | OFHasUpdates | OFHasFrameUpdates. Both images call Object::updateFlags
    // directly (ELF 0x8206468, PE 0x649E19 -> 0x71D680), not through the table.
    Object::updateFlags(0x2808u, 0u);
    objectTemplate->setPhysicsNodeComponent(this);                      // ELF +0x80 / PE +0x7C
    if (objectTemplate->m_hasCollisionPhysics)                          // +0x44 PE +0xA4
        Object::updateFlags(0x100u, 0u);                                // OFHasCollision on
    else
        Object::updateFlags(0u, 0x100u);                                // ... or off
    objectTemplate->setResponsePhysicsComponent(this);                  // ELF +0x88 / PE +0x84

    // The name is a temporary of the call: both images destroy it straight after the lookup (ELF
    // PE 0x649E9F), before the branch below. ELF +0x28 / PE +0x30 is this overload.
    IObjectTemplate* const lightTemplate = objectTemplateManager->getTemplate(
        m_template->getName() + "_lightSource", CID_LightSourceTemplate);
    if (lightTemplate)
    {
        // Debug type 6 - aborts in this tree, so the light below is never built here. The message
        // is literally "mats" in both images (ELF 0x88AC1E4, PE 0x901458). Unreachable with stock
        // content (see the file header).
        Debug(DebugTypeError, kParticleFile, 103, "") << "mats";
        m_lightSource = lightTemplate->createObject();                  // ELF +0x54 / PE +0x50
        // Direct, non-virtual call in both images (ELF 0x8206611 -> 0x830B5D0, PE 0x649F22 ->
        // ); the light's own slots are virtual.
        m_lightSource->setAbsoluteTransformation(Object::getVisualAbsoluteTransformation());
        m_lightSource->init();
    }

    // The self-destruct: message 29 after the lifetime plus one 30 Hz tick (0.033333335f, ELF
    // ds:0x88A2CCC / PE ds:0x89D7C8). handleMessage turns it into destroy(true).
    objectManager->postMessage(this, static_cast<TemplateMessage>(29),
                               m_timeToLive + 0.033333335f);           // +0xD0 in both images
}

// ~Particle - ELF D2 0x8208C90 (D1 0x8208D20 / D0 0x8208DB0 are clones + delete); BF2.exe deleting
// destructor 0x649F80 -> body 0x649B20. The template's two component removers run in the reverse
// of the constructor's binder order - response first, then the node - as SimpleObject's do.
Particle::~Particle()
{
    if (m_lightSource)
        m_lightSource->destroy(true);                                   // ELF +0x34 / PE +0x30
    --mNoParticlesDebug;
    const IObjectTemplate* const objectTemplate = m_template;
    objectTemplate->removeResponsePhysicsComponent(this);               // ELF +0x8C / PE +0x88
    objectTemplate->removePhysicsNodeComponent(this);                   // ELF +0x84 / PE +0x80
}

// the IBase quartet: the refcount at +0x130 (PE +0x148), NOT Object's +0x84
void Particle::addRef() { ++m_refCount; }         // (shared body)
int  Particle::getRef() { return m_refCount; }    // (shared body)

int Particle::release()
{
    const int count = --m_refCount;
    if (count == 0)
        delete this;                               // ELF D0 via +0x18 / PE scalar-deleting +0x14
    return count;
}

// (39 B) / PE 0x649B80 (43 B): two answers only.
// NOT IID_IObject, NOT IID_IConsoleSaveable - Object's own answers are not chained, so a particle
// asked for either returns null. Shipped; kept.
void* Particle::queryInterface(unsigned int iid) const
{
    if (iid == IID_IBase)
        return const_cast<Particle*>(this);
    if (iid == IID_IParticle)
        return static_cast<IParticle*>(const_cast<Particle*>(this));   // this + 0x12C / PE + 0x144
    return 0;
}

// Resolves the default geometry part, creates client geometry, randomizes the part, attaches collision and calls Object::init.
// An unresolved client geometry name returns before collision setup and base initialization.
void Particle::init()
{
    const ParticleTemplate* const objectTemplate = static_cast<const ParticleTemplate*>(m_template);
    if (m_geometryPart == -1)
        m_geometryPart = objectTemplate->m_geometryPart;               // +0x5C PE +0xBC

#if !DEDICATED
    // [PLATFORM] BF2.exe 0x64A336..0x64A3B6; absent from the ELF, which has no renderer.
    if (g_geometryTemplateManager && !objectTemplate->m_geometry.empty())
    {
        // The shipped body writes the lazy cache through the object's const template pointer.
        ParticleTemplate* const cache = const_cast<ParticleTemplate*>(objectTemplate);
        if (!cache->m_geometryTemplate)                                 // PE +0x84 (ELF +0x3C)
        {
            cache->m_geometryTemplate =
                geometryTemplateManagerGetTemplate(g_geometryTemplateManager, cache->m_geometry);
            if (!cache->m_geometryTemplate)
                return;                            // no collision mesh, no Object::init
            if (cache->m_usedOnTransparentSurface)                      // PE +0xFD (ELF +0x9D)
            {
                const bool flag = cache->m_usedOnTransparentSurface;
                if (geometryTemplateGetUsedOnTransparentSurface(cache->m_geometryTemplate) != flag)
                    geometryTemplateSetUsedOnTransparentSurface(cache->m_geometryTemplate, flag);
            }
        }
        IBase* const geometry = geometryTemplateCreateInstance(cache->m_geometryTemplate);
        if (geometry)
        {
            // The result is ignored here (PE 0x64A3AF) and the instance is never handed its owner
            // (SimpleObject's setGeometry makes a +0x9C call this path does not).
            setComponent(geom::IID_IGeometry, geometry);                // PE +0x54
            geometry->release();
        }
    }
#endif

    if (m_geometry && m_randomizeGeomPart)                              // IObject +0x38; +0x164
    {
        // A local uniform [0, 1) distribution, built field by field in both images (ELF
        // .., PE 0x64A3CB..): CRD_UNIFORM, value 0, variation 1, relative false, the
        // default Random seed (0x759DC150) - which the sampler never reads.
        RandomDistribution partDistribution;
        partDistribution.distribution = CRD_UNIFORM;
        partDistribution.value = 0.0f;
        partDistribution.variation = 1.0f;
        partDistribution.relative = false;
        const int partCount = geometry_part::count(m_geometry);        // ELF +0x60 / PE +0x5C
        if (partCount != -1)
        {
            // [PLATFORM] BF2.exe multiplies the x87 sample by the count (fimul), floors it as a
            // double (msvcr71 floor) and truncates; the ELF uses floorf on the float product.
            const float sample = sampleDistribution(partDistribution);
            const int part = static_cast<int>(std::floor(static_cast<double>(sample)
                                                         * static_cast<double>(partCount)));
            m_geometryPart = part;
            m_assignedGeometryPart = part;
        }
    }

    if (!objectTemplate->m_collisionMesh.empty())                       // +0x40 PE +0x88
    {
        ICollisionManager* const manager = static_cast<ICollisionManager*>(
            classManager->getSingleton(std::string("CollisionManager")));   // ELF +0x4C / PE +0x48
        ICollisionMesh* const mesh = manager->getCollisionMeshPart(
            objectTemplate->getCollisionMesh(), objectTemplate->m_collisionPart);   // +0x2C +0x28
        // NO NULL guard before this call in either image. [PLATFORM] The ELF then tests the mesh
        // for null (skipping setComponent and the release) - dead, since a null mesh has already
        // crashed here; BF2.exe has no test at all and is what is written.
        mesh->createMaterialMapping(objectTemplate);                    // ELF +0x28 / PE +0x24
        if (!setComponent(IID_ICollisionMesh, mesh))
        {
            // Debug type 6 - aborts in this tree. The missing closing quote is shipped.
            Debug(DebugTypeError, kParticleFile, 184, "")
                << "Couldn't create collision mesh \"" << objectTemplate->m_collisionMesh
                << "\" when creating object of template \"" << objectTemplate->getName();
        }
        mesh->release();
    }

    Object::init();
}

// message 29 - the constructor's timed post - destroys the particle.
void Particle::handleMessage(TemplateMessage message, IPlayer*, bool)
{
    if (message == static_cast<TemplateMessage>(29))
        destroy(true);                             // ELF +0x34 / PE +0x30
}

// updateGeometry - ELF 0x8208A20 (621 B) / BF2.exe 0x64A560 (399 B).
// part -1: the whole geometry, and every geometry chained behind it, follows the visual transform;
// part  n: part n follows it and every other part is collapsed to the zero matrix (hidden).
// The comparison with the part count is signed in both images (ELF `jge`, PE `jl`) while the zero
// loop's is unsigned (`jae` / `jb`), and the count is re-read on every iteration.
void Particle::updateGeometry()
{
    if (!m_geometry)
        return;
    int geometryPart = m_geometryPart;             // a local copy: the member is never rewritten
    const IObjectTemplate* const objectTemplate = m_template;
    if (geometryPart == -1)
    {
        setGeometryTransformation(m_geometry, &getVisualAbsoluteTransformation());
        for (void* next = getNextGeometry(m_geometry); next; next = getNextGeometry(next))
            setGeometryTransformation(next, &getVisualAbsoluteTransformation());
        return;
    }

    if (geometryPart >= geometry_part::count(m_geometry))
    {
        // Debug type 6 - aborts in this tree. The count is re-queried for the message and printed
        // unsigned in both images (ELF _ZNSolsEm, PE `??6...@I@Z`); the part as an int.
        Debug(DebugTypeError, kParticleFile, 223, "")
            << objectTemplate->getName() << "::geomPart " << geometryPart << " > partCount "
            << static_cast<unsigned int>(geometry_part::count(m_geometry));
        geometryPart = 0;
    }
    // Both calls hand the renderer a pointer it keeps (BundledMesh 0x10104940): the object's own
    // visual transform and the process-wide zero matrix are both stable storage.
    geometry_part::setTransform(m_geometry, &getVisualAbsoluteTransformation(), geometryPart);
    for (unsigned int part = 0;
         part < static_cast<unsigned int>(geometry_part::count(m_geometry)); ++part)
    {
        if (part != static_cast<unsigned int>(geometryPart))
            geometry_part::setTransform(m_geometry, &Mat4::getZero(), static_cast<int>(part));
    }
}

// the latch handleUpdate's ageing waits for. Never cleared.
void Particle::handleFrameUpdate(float)
{
    m_frameUpdated = 1;
}

// Updates gravity/drag and geometry scale from clamped lifetime curves, opacity from the unclamped fraction.
// Lifetime -1 uses fraction zero. Flag 0x02000000 skips scale/opacity; its name remains [UNRESOLVED].
// Age starts after the first frame update and stops on destruction.
void Particle::handleUpdate(float deltaTime, unsigned int)
{
    const ParticleTemplate* const objectTemplate = static_cast<const ParticleTemplate*>(m_template);

    float lifeFraction = 0.0f;                     // unclamped - it feeds the transparency graph
    int sample;
    float blend;
    if (m_timeToLive == -1.0f)                     // PE compares the bit pattern 0xBF800000
    {
        sample = 0;
        blend = 0.0f;
    }
    else
    {
        lifeFraction = m_age / m_timeToLive;
        float clamped = lifeFraction;
        if (clamped > 1.0f)                        // PE `test ah, 41h; jne` / ELF fcmovnbe
            clamped = 1.0f;
        const float scaled = clamped * 100.0f;     // 101 curve positions, 0..100
        sample = static_cast<int>(scaled);         // truncation (PE fistp under a chop control word)
        blend = scaled - static_cast<float>(sample);
    }

    IPhysicsNode* const node = static_cast<IPhysicsNode*>(m_physicsNode);   // IObject +0x44
    if (node)
    {
        const float inverse = 1.0f - blend;
        node->setGravityModifier(                                       // ELF +0xF8 / PE +0xF4
            (blend * objectTemplate->m_gravityModifierOverTime.m_samples[sample + 1]
             + inverse * objectTemplate->m_gravityModifierOverTime.m_samples[sample])
            * m_gravityModifier);
        node->setDrag(                                                  // ELF +0xC0 / PE +0xBC
            (blend * objectTemplate->m_dragOverTime.m_samples[sample + 1]
             + inverse * objectTemplate->m_dragOverTime.m_samples[sample])
            * m_drag);
    }

    if (!(m_flags & 0x02000000u))
    {
        const Vec3& sizeModifier = objectTemplate->m_sizeModifier;       // +0xA8 PE +0x108
        if (sizeModifier.x != 0.0f || sizeModifier.y != 0.0f || sizeModifier.z != 0.0f)
        {
            const float size = ((1.0f - blend) * objectTemplate->m_sizeOverTime.m_samples[sample]
                                + blend * objectTemplate->m_sizeOverTime.m_samples[sample + 1])
                               * m_size;
            if (!m_scaleable && m_geometry)
                m_scaleable = static_cast<IBase*>(m_geometry)->queryInterface(geom::IID_IScaleable);
            if (m_scaleable)
            {
                Vec3 scale;
                scale.x = 1.0f;
                scale.y = 1.0f;
                scale.z = 1.0f;
                if (sizeModifier.x > 0.0f)
                    scale.x = size * sizeModifier.x;
                if (sizeModifier.y > 0.0f)
                    scale.y = size * sizeModifier.y;
                if (sizeModifier.z > 0.0f)
                    scale.z = size * sizeModifier.z;
                static_cast<IScaleable*>(m_scaleable)->setScale(scale);
            }
        }

        if (m_geometry)
        {
            const float alpha = evaluateTransparencyGraph(objectTemplate->m_transparencyGraph,
                                                          lifeFraction)
                              * (1.0f - m_transparency);
            // A direct field write into the renderer's instance, then the opacity slot - the same
            // two-factor form ArmorGLComp's destruction fade uses (PE +0x28 / +0x2C, slot +0xF4).
            geom::GeometryInstanceData* const instance =
                static_cast<geom::GeometryInstanceData*>(m_geometry);
            instance->m_float28 = alpha;
            instance->setOpacity(alpha * instance->m_float2C);
        }
    }

    if (m_lightSource)                             // virtual getVisualAbsoluteTransformation here
        m_lightSource->setAbsoluteTransformation(getVisualAbsoluteTransformation());

    if (m_frameUpdated)
    {
        m_age = deltaTime + m_age;
        // BF2.exe 0x64A218..0x64A232: fld dt / fadd age / fst age / fcomp ttl / `test ah, 1; jne`. C0
        // is set for "less" AND for unordered, so the destroy runs only for an ordered sum >= ttl: a
        // NaN age or ttl never destroys. The ELF's fucomi + `jae` (0x820914F..0x820916A) is the same.
        // Both compare the unrounded x87 sum - a last-bit difference from this float compare.
        if (m_age >= m_timeToLive)
            destroy(true);
    }
}

// handleCollision - ELF 0x8209210 (485 B) / BF2.exe 0x64B490 (423 B). Both arms return true.
// 1. the emitter's collision sound, once per particle: the emitter is looked up by its object id
// in the object manager's registry (it may be gone), a hit on another Particle is silent, and
// the id is cleared after the first sound;
// 2. the template's collisionEffectName: the effect plays at the contact point and the particle
// destroys itself (the 16 stock "e_shardshatter" window shards take this path).
bool Particle::handleCollision(IObject* other, const Vec3&, const Vec3& normal, const Vec3& contact,
                               int firstMaterial, int secondMaterial, CollisionFaceInfo*,
                               CollisionFaceInfo*)
{
    if (m_emitterId != 0xFFFFFFFFu)
    {
        const std::map<unsigned int, IObject*>& objects =
            objectManager->getAllRegisteredObjects();                   // +0xB4 in both images
        const std::map<unsigned int, IObject*>::const_iterator found = objects.find(m_emitterId);
        if (found != objects.end())
        {
            IObject* const emitter = found->second;
            if (!other || other->getTemplate()->getClassID() != CID_ParticleTemplate)
            {
                // The sound sits 5 cm along the collision normal from the contact point.
                const Vec3& position = getAbsolutePosition();           // ELF +0x6C / PE +0x68
                Vec3 soundPosition;
                soundPosition.x = (contact.x + position.x) + normal.x * 0.05f;
                soundPosition.y = (contact.y + position.y) + normal.y * 0.05f;
                soundPosition.z = (contact.z + position.z) + normal.z * 0.05f;
                // The registry's IObject* is passed through unadjusted as the Emitter.
                static_cast<Emitter*>(emitter)->playParticleCollisionSound(
                    soundPosition, firstMaterial, secondMaterial);
                m_emitterId = 0xFFFFFFFFu;
            }
        }
    }

    const ParticleTemplate* const objectTemplate = static_cast<const ParticleTemplate*>(m_template);
    if (!objectTemplate->m_collisionEffectName.empty())                  // +0x7E8 PE +0x84C
    {
        // The arguments are built in the shipped order (both images): the transform's third row,
        // the normal copy, the effect template, then the contact position.
        const Mat4& transform = getVisualAbsoluteTransformation();
        Vec3 direction;
        direction.x = transform.m[8];
        direction.y = transform.m[9];
        direction.z = transform.m[10];
        const Vec3 effectNormal = normal;
        const IObjectTemplate* const effect = objectTemplate->getCollisionEffectTemplate();
        const Vec3& position = getAbsolutePosition();
        Vec3 effectPosition;
        effectPosition.x = contact.x + position.x;
        effectPosition.y = contact.y + position.y;
        effectPosition.z = contact.z + position.z;
        gameLogic->playCollisionEffect(effectPosition, effect, effectNormal, direction);   // +0x110
        destroy(true);
    }
    return true;
}

// the emitter's object ID (IObject +0x10), not the pointer.
void Particle::setEmitter(Emitter* emitter)
{
    m_emitterId = emitter->getObjectId();
}

// (a body BF2.exe shares): the template's material.
int Particle::getMaterial() const
{
    return m_template->getMaterial();              // ELF +0xC8 / PE +0xC4
}

} // namespace world
} // namespace hfe
} // namespace dice
