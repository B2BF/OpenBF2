// Path:     [INFERRED] - beside its header. Contents: recovered from bf2.i64.
// Initial bodies ; makeScript from both retail images.
// createObject and the complete preCache resource/id path are real: world::GenericProjectile's
// construction/cache surface is recovered and the factory product is real.

#include "GenericProjectileTemplate.h"
#include "../../../IO/Stream/IStream.h"
#include <sstream>

#include "GenericProjectile.h"
#include "../../../Core/Ids.h"
#include "../../IGameLogic.h"          // gameLogic->addPreStoredTemplate - the 16-deep pre-store
#include "../../../Geom/IGeometryTemplate.h"
#if !DEDICATED && defined(_MSC_VER)
#include "../../../Geom/GeometryTemplateManager.h"
#endif

namespace dice {
namespace hfe {

#if !DEDICATED
// BF2.exe ds:0x9A5578 - Game/Main/BF2Render.cpp defines it at this scope, published right after
// RendDX9's initDll. Declared here, as SimpleObject.cpp does, because this translation unit is
// shared and Game/Main/BF2ClientSeams.h is a client-side header.
extern IBase* g_geometryTemplateManager;
#endif

namespace world {

namespace {

// Tracer caches hold geometry-template pointers despite their legacy IObjectTemplate spelling.
// A present template must create geometry; the temporary instance is immediately released.
void precacheTracerGeometry(IObjectTemplate* tracerTemplate)
{
    if (!tracerTemplate)
        return;

    geom::IGeometryTemplate* const geometryTemplate =
        reinterpret_cast<geom::IGeometryTemplate*>(tracerTemplate);
    IBase* const geometry = static_cast<IBase*>(geometryTemplate->createInstance());
    geometry->release();
}

#if !DEDICATED
// Returns a borrowed geometry template through the legacy cache type. Requires the client manager.
IObjectTemplate* lookupTracerGeometryTemplate(const std::string& name)
{
#if !DEDICATED && defined(_MSC_VER)
    geom::IGeometryTemplateManager* const manager =
        static_cast<geom::IGeometryTemplateManager*>(::dice::hfe::g_geometryTemplateManager);
    return reinterpret_cast<IObjectTemplate*>(manager->getTemplate(name));
#else
    // The manager header carries x86 renderer layouts; other builds retain this lookup adapter.
#if defined(_MSC_VER) && defined(_M_IX86)
    typedef IBase* (__thiscall *GetTemplateFn)(void*, const std::string&);
#else
    typedef IBase* (*GetTemplateFn)(void*, const std::string&);
#endif
    void* const manager = ::dice::hfe::g_geometryTemplateManager;
    GetTemplateFn const getTemplate = reinterpret_cast<GetTemplateFn>(
        (*reinterpret_cast<void***>(manager))[0x18 / 4]);
    return reinterpret_cast<IObjectTemplate*>(getTemplate(manager, name));
#endif
}
#endif

} // namespace

// ctor - 0x8285A70, 530 bytes, read in full. BundleTemplate base (which brings the five vptr
// stores), then every default below verbatim, in the ELF's own store order where it matters:
// the +0x184/+0x188 pair is written early (0 / 1.0f) and rewritten at the tail (1 / 3.0f) - the
// tail wins and only the tail is observable, so only the tail is transcribed. Ctor tail:
// updateFlags(2, 0), collisionGroups |= 8, setArmingDelay(0.0f).
GenericProjectileTemplate::GenericProjectileTemplate()
{
    m_refCount = 1;                               // +0x180

    m_field198 = 0;
    m_maxTracerScaler = 100.0f;
    m_minTracerScaler = 5.0f;
    m_tracerScaler = 0.0f;
    m_tracerSizeModifier = 1.0f;
    m_timeToRemoveTK = -1.0f;
    m_field1B0 = -1.0f;

    // m_tracerTemplateName / m_waterTracerTemplateName: default-constructed empty.
    m_tracerTemplate = 0;
    m_waterTracerTemplate = 0;
    m_tracerInterval = 1;

    m_tracerConvergeDistance = 100.0f;
    m_field1D4 = 0.0f;
    // m_field1D8: NOT written - the ELF ctor skips this slot entirely. Preserved as shipped
    // (operator new memory, so the ELF value was heap garbage; ours is too).
    m_projectileDamage = 10.0f;
    m_minDamage = -1.0f;                          // the getMinDamage sentinel
    m_distToStartLoseDamage = 0.0f; m_distToMinDamage = 0.0f; m_field1EC = 0.0f; m_field1F0 = 0.0f;
    m_hasOnTimeEffect = 1;                                // the ELF's dword store 1 across the byte quartet
    m_invisible = 0;
    m_addToProjectileList = 0;
    m_useProjectileCamera = 0;
    m_byte1F8 = 0;
    m_damageMandownSoldiers = 0;
    m_byte1FA = 0;
    m_field1FC = -1;
    m_localPredictOnClient = 0;
    m_field204 = 0.5f;
    // m_string208 / m_string20C: default-constructed empty.
    m_field210 = 0;
    m_field214 = 0;
    m_detonateAfterProjectileStopped = 0;
    m_detonateAfterProjectileStoppedMinSpeed = 0.125f;
    m_canPickup = 0;

    updateFlags(2, 0);

    // The tail rewrites of the early pair - the observable defaults:
    m_timeToLive.distribution = CRD_UNIFORM;
    m_timeToLive.value = 3.0f;
    m_timeToLive.variation = 0.0f;
    m_timeToLive.relative = false;

    m_collisionGroups |= 8;                       // SimpleObjectTemplate's +0xC0 field

    setArmingDelay(0.0f);                         // the ctor's last call - seeds both delay fields
}

// dtor - 0x8287A30 (D1), 225 B: resets the five vtables (compiler's job) and destroys the four
// strings in reverse order (+0x20C, +0x208, +0x1BC, +0x1B4 - the compiler's job too), then
// ~BundleTemplate. Nothing manual survives the translation.
GenericProjectileTemplate::~GenericProjectileTemplate()
{
}

// The per-class IBase quartet - refcount +0x180, delete on the 1->0 edge through the D0 slot.
void GenericProjectileTemplate::addRef()  { ++m_refCount; }
int  GenericProjectileTemplate::getRef()  { return m_refCount; }

int GenericProjectileTemplate::release()
{
    const int r = --m_refCount;
    if (r == 0)
        delete this;
    return r;
}

// the five-interface hop, in the ELF's own test order.
void* GenericProjectileTemplate::queryInterface(unsigned int iid) const
{
    GenericProjectileTemplate* self = const_cast<GenericProjectileTemplate*>(this);
    if (iid == IID_IBase || iid == IID_IObjectTemplate)
        return static_cast<IObjectTemplate*>(self);
    if (iid == IID_ISimpleObjectTemplate)                     // the ELF's +0x30 subobject
        return static_cast<ISimpleObjectTemplate*>(self);
    if (iid == IID_IBundleTemplate)                           // the ELF's +0x164 subobject
        return static_cast<IBundleTemplate*>(self);
    if (iid == io::IID_IConsoleSaveable)
        return static_cast<io::IConsoleSaveable*>(self);
    if (iid == IID_IProjectileObjectTemplate)                 // 38082 - the ELF's +0x17C subobject
        return static_cast<IProjectileObjectTemplate*>(self);
    return 0;
}

// `mov eax, ds:CID_GenericProjectileTemplate` - the global, not an immediate.
unsigned int GenericProjectileTemplate::getClassID() const
{
    return CID_GenericProjectileTemplate;
}

// 5 bytes in the ELF - the class emits its OWN empty override. Preserve the no-op.
void GenericProjectileTemplate::templateInitialize()
{
}

// the standard one-CID hop into Bundle's chain (a literal tail-jump in the ELF).
void* GenericProjectileTemplate::queryImplementation(unsigned int classId)
{
    if (classId == CID_GenericProjectileTemplate)
        return this;
    return BundleTemplate::queryImplementation(classId);
}

// Creates a projectile using this template.
IObject* GenericProjectileTemplate::createObject() const
{
    return new GenericProjectile(this);
}

// Registers sixteen pre-stored instances for networkable templates, then initializes and destroys a temporary
// projectile to load collision resources before postLevelLoad pruning. Also creates/releases both tracer geometries.
// The 0x20 latch is set indirectly by init, so incomplete initialization can retry.
void GenericProjectileTemplate::preCache() const
{
    if (getFlags() & 0x20)
        return;
    if (getNetworkableInfo().length())
        gameLogic->addPreStoredTemplate(getId(), 16, 16);      // ds:0x8ACBFA0, the GameLogic global

    GenericProjectile* const projectile =
        static_cast<GenericProjectile*>(createObject());
    projectile->init();
    projectile->turnIntoTracer(false);
    projectile->m_byte1FD = true;
    projectile->forceDestroy();

    precacheTracerGeometry(getTracerTemplate());
    precacheTracerGeometry(getWaterTracerTemplate());
}

// the raw assign override; SimpleObjectTemplate::setGeometry's resolution logic is
// deliberately NOT run for projectiles. As shipped.
void GenericProjectileTemplate::setGeometry(const std::string& geometry)
{
    m_geometry = geometry;
}

// the ten accessors
RandomDistribution& GenericProjectileTemplate::getTimeToLive()
{
    return m_timeToLive;
}

void GenericProjectileTemplate::setDamageMandownSoldiers(bool v) { m_damageMandownSoldiers = v; }
bool GenericProjectileTemplate::getDamageMandownSoldiers() const { return m_damageMandownSoldiers != 0; }
void GenericProjectileTemplate::setLocalPredictOnClient(bool v)  { m_localPredictOnClient = v; }
bool GenericProjectileTemplate::getLocalPredictOnClient() const  { return m_localPredictOnClient != 0; }

// assign the name AND NULL the resolved-template cache - a rename invalidates it.
void GenericProjectileTemplate::setTracerTemplateName(const std::string& name)
{
    m_tracerTemplateName = name;
    m_tracerTemplate = 0;
}

const std::string& GenericProjectileTemplate::getTracerTemplateName() const { return m_tracerTemplateName; }

// Returns the cached tracer geometry template, resolving a nonempty name lazily on the client.
// Dedicated builds only read the cache. The name refers to geometry, not an ObjectTemplate effect.
IObjectTemplate* GenericProjectileTemplate::getTracerTemplate() const
{
#if !DEDICATED
    if (!m_tracerTemplate && g_geometryTemplateManager && !m_tracerTemplateName.empty())
        m_tracerTemplate = lookupTracerGeometryTemplate(m_tracerTemplateName);
#endif
    return m_tracerTemplate;
}

void GenericProjectileTemplate::setTracerInterval(int v) { m_tracerInterval = v; }
int  GenericProjectileTemplate::getTracerInterval() const { return m_tracerInterval; }

// getWaterTracerTemplate - ELF 0x8286B80 (14 B, non-virtual) / BF2.exe inlined into
// GenericProjectile::init at 0x612FB8..0x612FE6. The same client-only lazy lookup as
// getTracerTemplate, in the same test order (cache +0x2A0, manager, name length +0x298, then
// getTemplate on the name at +0x284). No stock template authors a water tracer, so on stock
// content the empty name ends it; written because the shipped client does it.
IObjectTemplate* GenericProjectileTemplate::getWaterTracerTemplate() const
{
#if !DEDICATED
    if (!m_waterTracerTemplate && g_geometryTemplateManager && !m_waterTracerTemplateName.empty())
        m_waterTracerTemplate = lookupTracerGeometryTemplate(m_waterTracerTemplateName);
#endif
    return m_waterTracerTemplate;
}

// the one console setter (armingDelay). Seconds mirrored as ticks at the 30 Hz fixed
// timestep; the truncation (not rounding) is the ELF's cvttss2si.
void GenericProjectileTemplate::setArmingDelay(float seconds)
{
    m_armingDelay = seconds;
    m_armingDelayTicks = int(seconds * 30.0f);
}

// the one console getter (minDamage): -1.0f means "unset, use the +0x1DC value".
float GenericProjectileTemplate::getMinDamage() const
{
    if (m_minDamage == -1.0f)
        return m_projectileDamage;
    return m_minDamage;
}

// . Preserve nested tracer/acceleration guards and authoring
// omissions. Byte fields below are retail bool insertions, never character insertions.
bool GenericProjectileTemplate::makeScript(io::IStream* stream)
{
    if (!BundleTemplate::makeScript(stream)) return false;
    std::ostringstream out;
    out << "ObjectTemplate.timeToLive " << m_timeToLive << std::endl;
    out << "ObjectTemplate.material " << static_cast<int>(m_material) << std::endl;
    if (m_invisible) out << "ObjectTemplate.invisible " << bool(m_invisible) << std::endl;
    if (m_hasOnTimeEffect) out << "ObjectTemplate.hasOnTimeEffect " << bool(m_hasOnTimeEffect) << std::endl;
    if (m_armingDelay != 0.0f) out << "ObjectTemplate.armingDelay " << m_armingDelay << std::endl;
    if (m_tracerScaler != 0.0f)
    {
        out << "ObjectTemplate.tracerScaler " << m_tracerScaler << std::endl;
        out << "ObjectTemplate.maxTracerScaler " << m_maxTracerScaler << std::endl;
        out << "ObjectTemplate.minTracerScaler " << m_minTracerScaler << std::endl;
    }
    if (m_tracerSizeModifier != 1.0f) out << "ObjectTemplate.tracerSizeModifier " << m_tracerSizeModifier << std::endl;
    if (!m_tracerTemplateName.empty())
    {
        out << "ObjectTemplate.tracerTemplate " << m_tracerTemplateName << std::endl;
        if (m_tracerInterval != 1) out << "ObjectTemplate.tracerInterval " << m_tracerInterval << std::endl;
        if (m_tracerConvergeDistance != 100.0f) out << "ObjectTemplate.tracerConvergeDistance " << m_tracerConvergeDistance << std::endl;
    }
    if (!m_waterTracerTemplateName.empty()) out << "ObjectTemplate.waterTracerTemplate " << m_waterTracerTemplateName << std::endl;
    if (m_timeToRemoveTK != -1.0f) out << "ObjectTemplate.timeToRemoveTK " << m_timeToRemoveTK << std::endl;
    if (m_addToProjectileList) out << "ObjectTemplate.addToProjectileList " << bool(m_addToProjectileList) << std::endl;
    if (m_localPredictOnClient) out << "ObjectTemplate.localPredictOnClient " << bool(m_localPredictOnClient) << std::endl;
    if (GenericProjectileTemplate::getMinDamage() != m_projectileDamage) out << "ObjectTemplate.minDamage " << m_minDamage << std::endl;
    if (m_projectileDamage != 10.0f) out << "ObjectTemplate.damage " << m_projectileDamage << std::endl;
    if (m_damageMandownSoldiers) out << "ObjectTemplate.damageMandownSoldiers " << bool(m_damageMandownSoldiers) << std::endl;
    if (m_distToStartLoseDamage != 0.0f) out << "ObjectTemplate.distToStartLoseDamage " << m_distToStartLoseDamage << std::endl;
    if (m_distToMinDamage != 0.0f) out << "ObjectTemplate.distToMinDamage " << m_distToMinDamage << std::endl;
    if (m_byte1F8) out << "ObjectTemplate.damageOwner " << bool(m_byte1F8) << std::endl;
    if (m_field1EC != 0.0f)
    {
        out << "ObjectTemplate.acceleration " << m_field1EC << std::endl;
        if (m_field1F0 != 0.0f) out << "ObjectTemplate.maxSpeed " << m_field1F0 << std::endl;
    }
    if (m_field1D4 != 0.0f) out << "ObjectTemplate.startDelay " << m_field1D4 << std::endl;
    if (m_useProjectileCamera) out << "ObjectTemplate.useProjectileCamera " << bool(m_useProjectileCamera) << std::endl;
    if (m_byte1FA) out << "ObjectTemplate.visibleOn3dMap " << bool(m_byte1FA) << std::endl;
    if (m_field1FC != -1) out << "ObjectTemplate.3dMapIcon " << m_field1FC << std::endl;
    const std::string text = out.str();
    return stream->write(text.c_str(), static_cast<unsigned int>(text.size())) == text.size();
}

} // namespace world
} // namespace hfe
} // namespace dice
