// Grouped template lifecycle, lookup and factory implementations. [INFERRED] Source placement.
// Unimplemented factories retain named boundaries beside their definitions.

#include "TemplateLeaves.h"
#include "Game/TriggerableManager.h"
#include "Game/Triggerable.h"
#include "../../IO/Stream/IStream.h"
#include <sstream>
#include <cstring>
#if defined(_MSC_VER) && defined(_M_IX86)
#include <string>
#endif
#include "Game/DynamicBundle.h"
#include "Game/AreaObject.h"
#include "Game/Obstacle.h"
#include "../../IO/Stream/IStream.h"

#include "Game/UAVVehicle.h"                     // UAVVehicleTemplate::createObject's product
#include "Game/DropVehicle.h"                    // DropVehicleTemplate::createObject's product
#include "Game/Ladder.h"                         // LadderTemplate::createObject's product
#include "Game/RopeLink.h"                       // HookLink/RopeLink products (same runtime type)
#include "Game/Zipline.h"                        // ZiplineTemplate::createObject's product
#include "Game/ZiplineRope.h"                    // ZiplineRopeTemplate::createObject's product
#include "Game/Soldier.h"                        // Zipline vertical-obstruction probes
#include "Game/EntryPoint.h"                     // EntryPointTemplate::createObject's product
#include "Game/Spring.h"                         // SpringTemplate::createObject's product
#include "Game/Rotor.h"                          // RotorTemplate::createObject's product
#include "Fx/Emitter.h"                          // EmitterTemplate::createObject's product
#include "../Main/Game.h"                        // isDedicated() - EmitterTemplate::createObject (BF2.exe 0x440510)
#include "IPlayer.h"                             // Zipline placement owner/vehicle
#include "IObject.h"                             // placement collision objects
#include "../IGame.h"                            // createObjectOnAllClients
#include "../../Physics/PhysicsSpring.h"          // SpringTemplate::setPhysicsNodeComponent's node
#include "../../Physics/PhysicsNodeManager.h"     // physicsNodeManager->addPhysicsNode
#include "../../Physics/HeightmapCluster.h"       // terrain and water placement checks
#include "../../Scene/Object/ObjectManager.h"     // line tests and LOD override
#include "../../Scene/Object/ObjectPredicator.h"  // ObjectFlagPredicator(0x100)
#include "../../Scene/Object/ObjectUtil.h"        // getRootParent
#include "../../Scene/Object/ObjectTypes/Base/ObjectTemplateManager.h"
#include "../../Sound/WheelSound.h"               // SpringTemplate::templateInitialize
#include "../IGameLogic.h"                        // gameLogic->isServer()
#include "../../Core/ClassManager.h"
#include "../../Core/Ids.h"
#include "../../Sound/SoundObjectTemplate.h"
#include "../../Animation/BoneAnimation/AnimationSystem.h"
#include "../../Core/StringUtil.h"
#include "../../IO/Input/Input.h"

#include <cmath>
#include <cstring>
#include <sstream>

namespace dice {
namespace hfe {

extern ClassManager* classManager;      // ds:0x8AC85A0 - the local declaration this tree uses


namespace world {

extern PhysicsNodeManager* physicsNodeManager;   // ds:0x8B27844 - Core/Globals.cpp (SpringTemplate::setPhysicsNodeComponent)
extern IBase* heightmapCluster;                  // ds:0x8ABAF24 - Core/Globals.cpp

namespace {

// Vec3's retail normalization: already-unit vectors are untouched, degenerate vectors become
// exact zero, and every other vector is scaled by 1/sqrt(lengthSq).
void normalizeZiplinePlacementVector(Vec3& value)
{
    const float lengthSq = value.x * value.x + value.y * value.y + value.z * value.z;
    if (std::fabs(lengthSq - 1.0f) < 1.1920929e-7f)
        return;
    if (std::fabs(lengthSq) < 1.1920929e-7f)
    {
        value.x = value.y = value.z = 0.0f;
        return;
    }
    const float inverseLength = 1.0f / std::sqrt(lengthSq);
    value.x *= inverseLength;
    value.y *= inverseLength;
    value.z *= inverseLength;
}

} // namespace

// The macros below expand the two recovered shapes. They are OUR compression of twenty-three
// identical bodies - the shapes themselves are the recovery, verified per class in the bulk pass.

// The IBase quartet, for the twelve leaves that re-implement it. `off` is the refcount offset,
// which is always the base class's size; release deletes on the 1->0 edge through the D0 slot.
#define BF2_LEAF_IBASE_IMPL(cls, base)                                                     \
    void cls::addRef()  { ++m_refCount; }                                                  \
    int  cls::getRef()  { return m_refCount; }                                             \
    int  cls::release()                                                                    \
    {                                                                                      \
        const int r = --m_refCount;                                                        \
        if (r == 0) delete this;                                                           \
        return r;                                                                          \
    }                                                                                      \
    void* cls::queryInterface(unsigned int iid) const { return base::queryInterface(iid); }

// getClassID + dtor + the flat three-cid queryImplementation (SimpleObjectTemplate leaves).
#define BF2_LEAF_SIMPLE(cls)                                                               \
    unsigned int cls::getClassID() const { return CID_##cls; }                             \
    cls::~cls() {}                                                                         \
    void* cls::queryImplementation(unsigned int classId)                                   \
    {                                                                                      \
        if (classId == CID_##cls ||                                                        \
            classId == CID_SimpleObjectTemplate ||                                         \
            classId == CID_ObjectTemplate)                                                 \
            return this;                                                                   \
        return 0;                                                                          \
    }

// getClassID + dtor + the one-cid hop into Bundle's chain (BundleTemplate leaves).
#define BF2_LEAF_BUNDLE(cls)                                                               \
    unsigned int cls::getClassID() const { return CID_##cls; }                             \
    cls::~cls() {}                                                                         \
    void* cls::queryImplementation(unsigned int classId)                                   \
    {                                                                                      \
        if (classId == CID_##cls)                                                          \
            return this;                                                                   \
        return BundleTemplate::queryImplementation(classId);                               \
    }

// createObject: `new(size) Object(this)` against an unrecovered object class - the allocation
// size is the ELF's. Declared-unwritten [FRONTIER]: the body would need the object class.
#define BF2_LEAF_NO_OBJECT(cls)                                                            \
    IObject* cls::createObject() const { return 0; }

// SimpleObjectTemplate leaves

// . Ctor 80 B: +0x164 refcount 1, +0x168 = 0, +0x16C = 1. createObject: new(0x140)
// DestroyableWindow(this). Constructor, factory and script body live in Game/DestroyableWindow.cpp.
BF2_LEAF_SIMPLE(DestroyableWindowTemplate)
BF2_LEAF_IBASE_IMPL(DestroyableWindowTemplate, SimpleObjectTemplate)

// (BF2.exe 0x5C4). Ctor 826 B - the largest leaf ctor: floats at +0x164/+0x16C (1.0f),
// +0x174 (300.0f), four 0x759DC150 'never' sentinels at +0x188/+0x19C/+0x1B0, a 0.1f at +0x190, a
// 11.0f at +0x1A4 and a sampled-curve block from +0x1B4. preCache (+0x6C, 218 B) warms the main
// template and every alternate slot.
BF2_LEAF_SIMPLE(EmitterTemplate)

// Creates client emitters; dedicated builds return null.
IObject* EmitterTemplate::createObject() const
{
    if (isDedicated())
        return 0;
    return new Emitter(this);
}

// both read in full. RandomDistribution constructors supply
// CRD_NONE, variation=0, relative=false and seed 0x759DC150; the emitter then uses its defaults.
EmitterTemplate::EmitterTemplate()
    : m_looping(false), m_addEmitterSpeed(false), m_emitterSpeedScale(1.0f),
      m_startAtCreation(false), m_startProbability(1.0f), m_intensityAtSpeed(0.0f),
      m_lodDistance(300.0f), m_useCameraOrientation(false), m_zRotationSnap(1.0f),
      m_hasOverDamage(false), m_noPhysics(false), m_addChild(false),
      m_showInFirstPerson(false), m_showInThirdPerson(false), m_moveToWaterSurface(false),
      m_unknown476(false), m_numAlternateTemplates(0), m_randomizeGeomPart(false),
      m_geometryPartId(-1)
{
    m_delay.value = 0.0f;
    m_timeToLive.value = 0.1f;
    m_intensity.value = 11.0f;
    // The distribution's first word is the float default, despite the older int field spelling.
    const float defaultSample = 1.0f;
    std::memcpy(&m_intensityOverTime.m_field00, &defaultSample, sizeof(defaultSample));
    for (int i = 0; i < 4; ++i) m_intensityOverTime.m_authored[i] = 0;
    for (int i = 0; i < 102; ++i) m_intensityOverTime.m_samples[i] = defaultSample;
    for (int i = 0; i < 3; ++i)
    {
        m_relativePosition[i].value = 0.0f;
        m_positionalSpeed[i].value = 0.0f;
        m_rotationalSpeed[i].value = 0.0f;
    }
    m_startRotation.value = 0.0f;
    m_zRotation.value = 0.0f;
    for (int i = 0; i < 5; ++i) m_alternateTemplateCache[i] = 0;
}

// . Ordering and the two distinct null guards are identical.
void EmitterTemplate::preCache() const
{
    if (!m_template.empty())
    {
        IObjectTemplate* objectTemplate = objectTemplateManager->getTemplate(m_template);
        if (objectTemplate)
        {
            objectTemplate->preCache();
            IObject* object = objectTemplate->createObject();
            if (object)
            {
                object->init();
                object->forceDestroy();
            }
        }
    }
    for (int i = 0; i < 5; ++i)
    {
        if (m_alternateTemplates[i].empty()) continue;
        IObjectTemplate* objectTemplate = objectTemplateManager->getTemplate(m_alternateTemplates[i]);
        if (objectTemplate)
        {
            objectTemplate->preCache();
            IObject* object = objectTemplate->createObject();
            if (object)
            {
                object->init();
                object->forceDestroy();
            }
        }
    }
}

// . Out-of-range and NaN inputs leave the old value intact.
void EmitterTemplate::setStartProbability(float value)
{
    if (value >= 0.0f && value <= 1.0f) m_startProbability = value;
}

// . Count every nonempty slot, including gaps.
void EmitterTemplate::updateNumAlternateTemplates()
{
    m_numAlternateTemplates = 0;
    for (int i = 0; i != 5; ++i)
        if (!m_alternateTemplates[i].empty()) ++m_numAlternateTemplates;
}

// all instructions read. The PE body receives the +8
// IConsoleSaveable subobject; its offsets are therefore eight less than the constructor's.
bool EmitterTemplate::makeScript(io::IStream* stream)
{
    std::ostringstream out;
    if (!SimpleObjectTemplate::makeScript(stream)) return false;
    out << "ObjectTemplate.template " << m_template << std::endl;
    if (m_numAlternateTemplates > 0)
        for (int i = 0; i < 5; ++i)
            out << "ObjectTemplate.alternateTemplate" << i << ' ' << m_alternateTemplates[i] << std::endl;
    if (m_randomizeGeomPart) out << "ObjectTemplate.randomizeGeomPart " << m_randomizeGeomPart << std::endl;
    if (m_geometryPartId != -1) out << "ObjectTemplate.useGeomPart " << m_geometryPartId << std::endl;
    if (m_looping) out << "ObjectTemplate.looping " << m_looping << std::endl;
    if (m_startAtCreation) out << "ObjectTemplate.startAtCreation " << m_startAtCreation << std::endl;
    if (m_addEmitterSpeed)
    {
        out << "ObjectTemplate.addEmitterSpeed " << m_addEmitterSpeed << std::endl;
        out << "ObjectTemplate.emitterSpeedScale " << m_emitterSpeedScale << std::endl;
    }
    if (m_startProbability != 1.0f) out << "ObjectTemplate.startProbability " << m_startProbability << std::endl;
    if (m_lodDistance != 300.0f) out << "ObjectTemplate.lodDistance " << m_lodDistance << std::endl;
    // The relative byte does not participate in the retail default-value test.
    if (m_delay.distribution || m_delay.value != 0.0f || m_delay.variation != 0.0f)
        out << "ObjectTemplate.delay " << m_delay << std::endl;
    if (m_timeToLive.distribution || m_timeToLive.value != 0.1f || m_timeToLive.variation != 0.0f)
        out << "ObjectTemplate.timeToLive " << m_timeToLive << std::endl;
    if (m_intensity.distribution || m_intensity.value != 11.0f || m_intensity.variation != 0.0f)
        out << "ObjectTemplate.intensity " << m_intensity << std::endl;
    std::ostringstream curve;
    curve << m_intensityOverTime;
    if (!curve.str().empty()) out << "ObjectTemplate.intensityOverTime " << m_intensityOverTime << std::endl;
    if (m_intensityAtSpeed != 0.0f) out << "ObjectTemplate.IntensityAtSpeed " << m_intensityAtSpeed << std::endl;
    if (m_useCameraOrientation) out << "ObjectTemplate.useCameraOrientation " << m_useCameraOrientation << std::endl;
    const char* directions[3] = {"Dof", "Up", "Right"};
    for (int i = 0; i < 3; ++i)
        if (m_relativePosition[i].distribution || m_relativePosition[i].value != 0.0f || m_relativePosition[i].variation != 0.0f)
            out << "ObjectTemplate.relativePositionIn" << directions[i] << ' ' << m_relativePosition[i] << std::endl;
    for (int i = 0; i < 3; ++i)
        if (m_positionalSpeed[i].distribution || m_positionalSpeed[i].value != 0.0f || m_positionalSpeed[i].variation != 0.0f)
            out << "ObjectTemplate.positionalSpeedIn" << directions[i] << ' ' << m_positionalSpeed[i] << std::endl;
    for (int i = 0; i < 3; ++i)
        if (m_rotationalSpeed[i].distribution || m_rotationalSpeed[i].value != 0.0f || m_rotationalSpeed[i].variation != 0.0f)
            out << "ObjectTemplate.rotationalSpeedIn" << directions[i] << ' ' << m_rotationalSpeed[i] << std::endl;
    out << "ObjectTemplate.startRotation " << m_startRotation << std::endl;
    out << "ObjectTemplate.zRotation " << m_zRotation << std::endl;
    out << "ObjectTemplate.zRotationSnap " << m_zRotationSnap << std::endl;
    if (m_hasOverDamage) out << "ObjectTemplate.hasOverDamage " << m_hasOverDamage << std::endl;
    if (m_noPhysics) out << "ObjectTemplate.noPhysics " << m_noPhysics << std::endl;
    if (m_addChild) out << "ObjectTemplate.addChild " << m_addChild << std::endl;
    if (m_showInFirstPerson) out << "ObjectTemplate.showInFirstPerson " << m_showInFirstPerson << std::endl;
    if (m_showInThirdPerson) out << "ObjectTemplate.showInThirdPerson " << m_showInThirdPerson << std::endl;
    if (m_moveToWaterSurface) out << "ObjectTemplate.moveToWaterSurface " << m_moveToWaterSurface << std::endl;
    const std::string text = out.str();
    const bool result = stream->write(text.c_str(), static_cast<unsigned int>(text.size())) == text.size();
    if (!m_template.empty())
    {
        IObjectTemplate* objectTemplate = objectTemplateManager->getTemplate(m_template);
        if (objectTemplate && !(objectTemplate->getFlags() & 1))
        {
            stream->write("\n", 1);
            io::IConsoleSaveable* saveable = static_cast<io::IConsoleSaveable*>(
                objectTemplate->queryInterface(io::IID_IConsoleSaveable));
            saveable->makeScript(stream); // no null guard; nested result is deliberately ignored
        }
    }
    return result;
}

// Entry points default to a three-metre entry radius.
BF2_LEAF_SIMPLE(EntryPointTemplate)
EntryPointTemplate::EntryPointTemplate() : m_entryRadius(3.0f) {}
IObject* EntryPointTemplate::createObject() const { return new EntryPoint(this); }

// . Ctor 66 B: +0x164 refcount 1, +0x168 = 0.
EnvMapTemplate::EnvMapTemplate() : m_envMap(0) { m_refCount=1; }
// ELF 8202210 / dedicated PE 606B00; getter slots 4/2 in both ports (ELF8202690/8202620).
bool EnvMapTemplate::makeScript(io::IStream* stream)
{
    if (!ObjectTemplate::makeScript(stream)) return false;
    std::ostringstream out;
#if defined(_MSC_VER)
    typedef bool (__thiscall* StaticFn)(void*);
    typedef float (__thiscall* SizeFn)(void*);
#else
    typedef bool (*StaticFn)(void*);
    typedef float (*SizeFn)(void*);
#endif
    const bool isStatic = m_envMap ? reinterpret_cast<StaticFn>((*reinterpret_cast<void***>(m_envMap))[4])(m_envMap) : true;
    out << "ObjectTemplate.isStatic " << isStatic << std::endl;
    out << "ObjectTemplate.size ";
    const float size = m_envMap ? reinterpret_cast<SizeFn>((*reinterpret_cast<void***>(m_envMap))[2])(m_envMap) : 0.0f;
    out << size << std::endl;
    const std::string text = out.str();
    return stream->write(text.c_str(), static_cast<unsigned int>(text.size())) == text.size();
}

BF2_LEAF_SIMPLE(EnvMapTemplate)
BF2_LEAF_IBASE_IMPL(EnvMapTemplate, SimpleObjectTemplate)

// `xor eax` with a dead 0x148-byte frame: the compiler left the object-construction frame
// behind after the body was reduced to `return 0`. Reproduced as the null return it is.
IObject* EnvMapTemplate::createObject() const { return 0; }

// 7-byte binary stubs, as shipped.
bool EnvMapTemplate::writeBinary(io::IStream*) const { return false; }
bool EnvMapTemplate::readBinary(io::IStream*)        { return false; }

// ELF8201CC0 / clientPE64E4D0, complete reads. Renderer +1C is getTemplate(name); the renderer
// takes the key as the shared std::string (issue #7: one STL across the image boundary).
void EnvMapTemplate::templateInitialize()
{
    updateFlags(1, 0);
    IBase* manager = classManager->getSingleton(std::string("EnvMapManager"));
    if (manager)
    {
        const std::string& name=getName();
#if defined(_MSC_VER)
        typedef void* (__thiscall* GetTemplate)(void*,const std::string&);
#else
        typedef void* (*GetTemplate)(void*,const std::string&);
#endif
        // ELF8201D3D / clientPE64E51C: +0x1C is logical slot 7 in both 32-bit
        // tables. Preserve the ordinal when native pointer width changes.
        m_envMap=reinterpret_cast<GetTemplate>((*reinterpret_cast<void***>(manager))[7])(
            manager,name);
    }
}

// . Ctor 58 B: one float at +0x164 = 3.0f. createObject returns NULL (7 B).
// templateInitialize is a 5-byte empty - THE body the deep drive faulted on.
BF2_LEAF_SIMPLE(GroundEffectBundleTemplate)
// ELF 8202E10: the only derived field is the trigger-distance float, with no flag update.
GroundEffectBundleTemplate::GroundEffectBundleTemplate() : m_maxTriggerDistance(3.0f) {}

// ELF 8202E70 / dedicated PE 5FE6E0.
bool GroundEffectBundleTemplate::makeScript(io::IStream* stream)
{
    if (!SimpleObjectTemplate::makeScript(stream)) return false;
    std::ostringstream out;
    out << "ObjectTemplate.maxTriggerDistance " << m_maxTriggerDistance << std::endl;
    const std::string text = out.str();
    return stream->write(text.c_str(), static_cast<unsigned int>(text.size())) == text.size();
 }
IObject* GroundEffectBundleTemplate::createObject() const { return 0; }
void GroundEffectBundleTemplate::templateInitialize() {}

// Hook links create RopeLink objects using this template's compatible layout.
HookLinkTemplate::HookLinkTemplate()
{
    m_refCount = 1;
    updateFlags(0x40, 0);
}
BF2_LEAF_SIMPLE(HookLinkTemplate)
BF2_LEAF_IBASE_IMPL(HookLinkTemplate, SimpleObjectTemplate)
IObject* HookLinkTemplate::createObject() const
{
    // Shipped quirk: HookLinkTemplate constructs RopeLink with the sibling template type.
    return new RopeLink(reinterpret_cast<const RopeLinkTemplate*>(this));
}

// The template owns and releases its seat-animation template.
unsigned int LadderTemplate::getClassID() const { return CID_LadderTemplate; }
LadderTemplate::~LadderTemplate()
{
    if (m_seatAnimationSystem)
        anim::animationSystemTemplateManager->release(m_seatAnimationSystem);
}
void* LadderTemplate::queryImplementation(unsigned int classId)
{
    if (classId == CID_LadderTemplate ||
        classId == CID_SimpleObjectTemplate ||
        classId == CID_ObjectTemplate)
        return this;
    return 0;
}
BF2_LEAF_IBASE_IMPL(LadderTemplate, SimpleObjectTemplate)
IObject* LadderTemplate::createObject() const
{
    return new Ladder(this);
}

// set/getSeatAnimationSystem - ELF 0x818A990 / 0x818AA40; PE 0x59C8B0 / 0x59C4C0.
// Same-name assignment is a no-op. Preserve retail's empty-name defect: it releases the old
// manager entry but does not null the field when the replacement name is empty.
void LadderTemplate::setSeatAnimationSystem(const std::string& name)
{
    if (m_seatAnimationSystem)
    {
        if (compareIgnoreCase(getSeatAnimationSystem().c_str(), name.c_str()) == 0)
            return;
        anim::animationSystemTemplateManager->release(m_seatAnimationSystem);
    }
    if (!name.empty())
        m_seatAnimationSystem = anim::animationSystemTemplateManager->load(name);
}

const std::string& LadderTemplate::getSeatAnimationSystem()
{
    return anim::animationSystemTemplateManager->getPath(m_seatAnimationSystem);
}

// . Ctor 81 B: refcount 1, updateFlags(0x40, 0). createObject: new(0x150) RopeLink(this).
RopeLinkTemplate::RopeLinkTemplate()
{
    m_refCount = 1;
    updateFlags(0x40, 0);
}
BF2_LEAF_SIMPLE(RopeLinkTemplate)
BF2_LEAF_IBASE_IMPL(RopeLinkTemplate, SimpleObjectTemplate)
IObject* RopeLinkTemplate::createObject() const
{
    return new RopeLink(this);
}

// SupplyDepotTemplate's actual vector storage, authoring and product live in Game/SupplyDepot.cpp.

// . Ctor 115 B: refcount 1, an empty string at +0x168, 1.0f at +0x16C, updateFlags(0x40, 0).
ZiplineRopeTemplate::ZiplineRopeTemplate()
    : m_ropeName()
    , m_lengthScale(1.0f)
{
    m_refCount = 1;
    updateFlags(0x40, 0);
}
BF2_LEAF_SIMPLE(ZiplineRopeTemplate)
BF2_LEAF_IBASE_IMPL(ZiplineRopeTemplate, SimpleObjectTemplate)
IObject* ZiplineRopeTemplate::createObject() const
{
    return new ZiplineRope(this);
}

// . Ctor 184 B: refcount 1, empty string at +0x168, then a run of tuned floats - +0x16C
// 50.0f, +0x170 3.0f, +0x174 2.0f, +0x178 1.49f, +0x17C 1000.0f, +0x180 0.5f, +0x184 3.0f,
// +0x188 0 - and updateFlags(0x40, 0). createObject: new(0x190) Zipline(this).
ZiplineTemplate::ZiplineTemplate()
    : _string168()
    , m_searchRadius(50.0f)
    , m_entryRadius(3.0f)
    , m_collisionStartRadius(2.0f)
    , m_ziplineSoldierHeight(1.49f)
    , _float17C(1000.0f)
    , m_creationHeightOffset(0.5f)
    , m_minZiplineLength(3.0f)
    , m_player(0)
{
    m_refCount = 1;
    updateFlags(0x40, 0);
}
BF2_LEAF_SIMPLE(ZiplineTemplate)
BF2_LEAF_IBASE_IMPL(ZiplineTemplate, SimpleObjectTemplate)
IObject* ZiplineTemplate::createObject() const
{
    return new Zipline(this);
}

// (25 B); PE counterpart is the same single member store.
void ZiplineTemplate::setZiplineSoldierHeight(float height)
{
    m_ziplineSoldierHeight = height;
}

// . Only a server-side player currently driving a Soldier can deploy
// the object. The collision helper's result affects the authored endpoint, but not this function's
// final true return: retail still asks GameServer to create the Zipline with a zero endpoint when
// the placement ray fails. The separately looked-up "Zipline" template supplies both the object
// passed to the server and the +0x180 creation-height adjustment; `this` remains the helper owner.
bool ZiplineTemplate::createZiplineObject(IPlayer* player, Mat4& transformation, IObject* firearm)
{
    if (!player || !gameLogic->isServer())
        return false;

    m_player = player;
    IObject* vehicle = player->getVehicle();
    if (!vehicle || vehicle->getTemplate()->getClassID() != CID_SoldierTemplate)
        return false;

    Vec3 endpoint = { 0.0f, 0.0f, 0.0f };
    const Vec3 soldierPosition = player->getVehicle()->getAbsolutePosition();
    const bool validEndpoint = checkForRopeCreation(
        player, transformation, soldierPosition, endpoint, false, firearm);

    transformation.m[4] = 0.0f;
    transformation.m[5] = 1.0f;
    transformation.m[6] = 0.0f;
    reinterpret_cast<Vec3*>(&transformation.m[0])->makeOrthonormalBasis(
        *reinterpret_cast<Vec3*>(&transformation.m[4]),
        *reinterpret_cast<Vec3*>(&transformation.m[8]));
    transformation.m[12] = soldierPosition.x;
    transformation.m[13] = soldierPosition.y;
    transformation.m[14] = soldierPosition.z;

    if (!validEndpoint)
        endpoint.x = endpoint.y = endpoint.z = 0.0f;

    IObjectTemplate* objectTemplate =
        objectTemplateManager->getTemplate(std::string("Zipline"));
    const ZiplineTemplate* ziplineTemplate = static_cast<const ZiplineTemplate*>(objectTemplate);
    transformation.m[13] += ziplineTemplate->m_creationHeightOffset;

    const Vec3 position = {
        transformation.m[12], transformation.m[13], transformation.m[14]
    };
    game->createObjectOnAllClients(objectTemplate, position, endpoint, true, 0, 0);
    return true;
}

// . The first object/terrain impact along the authored forward axis is
// only a coarse target. Retail raises the soldier origin by the +0x180 creation offset plus two
// metres, then asks the stricter helper to find a legal endpoint from there.
bool ZiplineTemplate::checkForRopeCreation(IPlayer* player,
                                           const Mat4& transformation,
                                           const Vec3& soldierPosition,
                                           Vec3& endpoint,
                                           bool fromFireComp,
                                           IObject* firearm) const
{
    const float searchRadius = m_searchRadius;
    ObjectFlagPredicator predicator(0x100u);
    IObject* hitObject = 0;
    Vec3 hitPoint = { 0.0f, 0.0f, 0.0f };
    Vec3 hitNormal = { 0.0f, 0.0f, 0.0f };
    int hitPart = 0;

    const Vec3 from = {
        transformation.m[12], transformation.m[13], transformation.m[14]
    };
    const Vec3 direction = {
        transformation.m[8] * searchRadius,
        transformation.m[9] * searchRadius,
        transformation.m[10] * searchRadius
    };

    bool hit = objectManager->intersectLine(
        hitObject, hitPoint, hitNormal, hitPart, from, direction, &predicator,
        true, false, false, true, 0);
    if (!hit)
    {
        const Vec3 to = {
            from.x + direction.x, from.y + direction.y, from.z + direction.z
        };
        float hitFraction = 0.0f;
        unsigned char material = 0;
        hit = static_cast<IHeightmapCluster*>(heightmapCluster)->intersectRayInWorldCoords(
            from, to, hitPoint, hitFraction, hitNormal, material, false);
    }
    if (!hit)
        return false;

    Vec3 raisedStart = soldierPosition;
    raisedStart.y += m_creationHeightOffset + 2.0f;
    return findRopeCollisionPoint(
        player, raisedStart, hitPoint, endpoint, fromFireComp, firearm);
}

// . A part id of 666 at forced collision LOD 2 is a shipped early-out
// which deliberately leaves the manager's LOD override at 2. The ordinary pass excludes Soldiers,
// rejects water-covered hits and the exact five direct/three-root class ids below, then falls back
// to terrain only when no object intersection was found.
bool ZiplineTemplate::findRopeCollisionPoint(IPlayer* /*player*/,
                                             const Vec3& start,
                                             const Vec3& coarsePoint,
                                             Vec3& endpoint,
                                             bool /*fromFireComp*/,
                                             IObject* /*firearm*/) const
{
    bool terrainHit = false;
    ObjectFlagPredicator collisionPredicator(0x100u);

    Vec3 direction = {
        coarsePoint.x - start.x,
        coarsePoint.y - start.y,
        coarsePoint.z - start.z
    };
    normalizeZiplinePlacementVector(direction);
    direction.x *= m_searchRadius;
    direction.y *= m_searchRadius;
    direction.z *= m_searchRadius;

    ObjectFlagPredicator obstructionPredicator(0x100u);
    IObject* obstructionObject = 0;
    Vec3 obstructionPoint = { 0.0f, 0.0f, 0.0f };
    Vec3 obstructionNormal = { 0.0f, 0.0f, 0.0f };
    int obstructionPart = 0;
    objectManager->setOverrideLineTestLod(2);
    if (objectManager->intersectLine(
            obstructionObject, obstructionPoint, obstructionNormal, obstructionPart,
            start, direction, &obstructionPredicator, true, false, false, true, 0) &&
        obstructionPart == 666)
    {
        return false; // shipped quirk: do not restore the LOD override on this arm
    }
    objectManager->setOverrideLineTestLod(-1);

    IObject* hitObject = 0;
    Vec3 hitPoint = { 0.0f, 0.0f, 0.0f };
    Vec3 hitNormal = { 0.0f, 0.0f, 0.0f };
    int hitPart = 0;
    const bool objectHit = objectManager->intersectLine(
        hitObject, hitPoint, hitNormal, hitPart, start, direction, &collisionPredicator,
        true, false, false, true, CID_SoldierTemplate);

    IHeightmapCluster* cluster = static_cast<IHeightmapCluster*>(heightmapCluster);
    if (!objectHit)
    {
        const Vec3 to = {
            start.x + direction.x, start.y + direction.y, start.z + direction.z
        };
        float hitFraction = 0.0f;
        unsigned char material = 0;
        terrainHit = cluster->intersectRayInWorldCoords(
            start, to, hitPoint, hitFraction, hitNormal, material, false);
        if (terrainHit && cluster->getWaterLevelInWorldCoords(hitPoint) > hitPoint.y)
            terrainHit = false;
        if (!terrainHit)
            return false;
    }
    else if (hitObject)
    {
        if (cluster->getWaterLevelInWorldCoords(hitPoint) > hitPoint.y)
            return false;

        IObject* root = getRootParent(hitObject);
        if (hitObject->getTemplate()->getClassID() == CID_PlayerControlObjectTemplate ||
            hitObject->getTemplate()->getClassID() == CID_ZiplineTemplate ||
            hitObject->getTemplate()->getClassID() == CID_SupplyObjectTemplate ||
            (root &&
             (root->getTemplate()->getClassID() == CID_PlayerControlObjectTemplate ||
              root->getTemplate()->getClassID() == CID_ZiplineTemplate ||
              root->getTemplate()->getClassID() == CID_SupplyObjectTemplate)))
        {
            return false;
        }
        if (hitObject->getTemplate()->getClassID() == CID_DestroyableObjectTemplate)
            return false;
        if (hitObject->getTemplate()->getClassID() == CID_TriggerableTargetTemplate)
            return false;
    }

    endpoint = hitPoint;
    return objectHit || terrainHit;
}

// . Both two-metre vertical rays always execute: the first is offset half a metre
// along the Soldier's forward axis, while the second independently re-reads the Soldier position.
bool ZiplineTemplate::checkForVerticalObstruction(Soldier* soldier) const
{
    static float forwardOffset = 0.5f;

    const Vec3& position = soldier->getAbsolutePosition();
    const Mat4& transformation = soldier->getAbsoluteTransformation();
    const Vec3 centre = {
        position.x + transformation.m[8] * forwardOffset,
        position.y + transformation.m[9] * forwardOffset,
        position.z + transformation.m[10] * forwardOffset
    };
    const Vec3 firstFrom = { centre.x, centre.y - 1.0f, centre.z };
    const Vec3 firstTo = { centre.x, centre.y + 1.0f, centre.z };
    const Vec3 firstDirection = {
        firstTo.x - firstFrom.x, firstTo.y - firstFrom.y, firstTo.z - firstFrom.z
    };

    ObjectFlagPredicator predicator(0x100u);
    IObject* hitObject = 0;
    Vec3 hitPoint = { 0.0f, 0.0f, 0.0f };
    Vec3 hitNormal = { 0.0f, 0.0f, 0.0f };
    int hitPart = 0;
    const bool firstHit = objectManager->intersectLine(
        hitObject, hitPoint, hitNormal, hitPart, firstFrom, firstDirection, &predicator,
        true, false, false, true, CID_SoldierTemplate);

    const Vec3& secondPosition = soldier->getAbsolutePosition();
    const Vec3 secondFrom = {
        secondPosition.x, secondPosition.y - 1.0f, secondPosition.z
    };
    const Vec3 secondTo = {
        secondPosition.x, secondPosition.y + 1.0f, secondPosition.z
    };
    const Vec3 secondDirection = {
        secondTo.x - secondFrom.x,
        secondTo.y - secondFrom.y,
        secondTo.z - secondFrom.z
    };
    const bool secondHit = objectManager->intersectLine(
        hitObject, hitPoint, hitNormal, hitPart, secondFrom, secondDirection, &predicator,
        true, false, false, true, CID_SoldierTemplate);
    return firstHit || secondHit;
}

// The derived ctor is inlined into ClassFactory in both ports: one added int, initialized to 30.
// createObject allocates the recovered runtime (ELF 0x168 / PE 0x180).
DropVehicleTemplate::DropVehicleTemplate()
    : m_destroyVehicleTime(30)
{
}

BF2_LEAF_SIMPLE(DropVehicleTemplate)
IObject* DropVehicleTemplate::createObject() const { return new DropVehicle(this); }

// BundleTemplate leaves


// . Ctor 170 B: five zeroed 12-byte blocks from +0x17C, a true bool at +0x1AC, two more
// blocks at +0x1B0/+0x1BC. NO refcount - this leaf inherits the quartet.
// createObject: new(0x180) AreaObject(this), recovered in both dedicated images.
BF2_LEAF_BUNDLE(AreaObjectTemplate)

// radius is deliberately not initialized.
AreaObjectTemplate::AreaObjectTemplate() {}
IObject* AreaObjectTemplate::createObject() const { return new AreaObject(this); }

void AreaObjectTemplate::addLinePoint(const Vec2& point)
{
    // . Expand away from zero by twice the trigger radius.
    m_linePoints.push_back(point);
    Vec3 expanded = {point.x, 0, point.y};
    expanded.x += point.x < 0.0f ? -2.0f * m_triggerRadius : 2.0f * m_triggerRadius;
    expanded.z += point.y < 0.0f ? -2.0f * m_triggerRadius : 2.0f * m_triggerRadius;
    m_areaBounds.expand(expanded);
}
void AreaObjectTemplate::setTriggerRadius(float radius)
{
    // expand the existing box; do not rebuild it.
    m_triggerRadius = std::fabs(radius);
    const float twice = m_triggerRadius + m_triggerRadius;
    m_areaBounds.expand(twice, twice, twice);
    m_areaBounds.expand(-twice, -twice, -twice);
}
float AreaObjectTemplate::getTriggerRadius() { return m_triggerRadius; }

bool AreaObjectTemplate::makeScript(io::IStream* stream)
{
    // (PE body receives the secondary +8 this pointer).
    if (!BundleTemplate::makeScript(stream)) return false;
    std::ostringstream out;
    out << "ObjectTemplate.triggerRadius " << m_triggerRadius << std::endl;
    for (unsigned int i = 0; i < m_linePoints.size(); ++i)
        out << "ObjectTemplate.addLinePoint " << m_linePoints[i].x << '/' << m_linePoints[i].y << std::endl;
    const std::string text = out.str();
    stream->write(text.c_str(), static_cast<unsigned int>(text.size()));
    return true; // shipped writer ignores this suffix's write result
}

// Initializes destroyable-window dimensions, effect caches and the DestroyableObjectInfo network descriptor.
namespace {
const Math::Vec3 kDefaultWindowSizeVector = { 0.2f, 1.0f, 1.8f };   // ds:0x8AACF58 (.bss, static init)
} // namespace
DestroyableWindowsBundleTemplate::DestroyableWindowsBundleTemplate()
{
    m_refCount = 1;                                  // +0x17C
    respawnDelay = -1.0f;                            // +0x180
    numWindows = 0;                                  // +0x188
    windowSizeVector.x = 0.0f;                       // +0x18C..+0x194 zeroed first...
    windowSizeVector.y = 0.0f;
    windowSizeVector.z = 0.0f;
    clientHitDetection = true;                       // +0x184
    // effectTemplateBaseName (+0x198): constructed from the literal
    effectTemplateBaseName = "e_destroyableWindow_";
    updateFlags(1, 0);
    m_networkableInfo.assign("DestroyableObjectInfo");   // the raw member assign, not the setter
    windowSizeVector = kDefaultWindowSizeVector;     // ...then the TU default (0.2, 1.0, 1.8)
    for (int i = 0; i < 9; ++i)
        m_effectTemplates[i] = 0;                    // +0x19C..+0x1BF, non-owning 3x3 cache
}
BF2_LEAF_BUNDLE(DestroyableWindowsBundleTemplate)
BF2_LEAF_IBASE_IMPL(DestroyableWindowsBundleTemplate, BundleTemplate)

// . Ctor 76 B, and its one observable act is NOT updateFlags: `or byte [this+0xAC], 6` -
// it sets bits 1 and 2 of SimpleObjectTemplate's physics-flags byte directly, which is the
// hasCollisionPhysics bit (4) plus bit 1. Then -1.0f at +0x17C.
// templateInitialize is a 5-byte empty. createObject: new(0x13C) DynamicBundle(this).
DynamicBundleTemplate::DynamicBundleTemplate()
{
    m_physicsFlags = static_cast<unsigned char>(m_physicsFlags | 6);
    respawnDelay = -1.0f; // +0x17C
}
BF2_LEAF_BUNDLE(DynamicBundleTemplate)
IObject* DynamicBundleTemplate::createObject() const { return new DynamicBundle(this); }
bool DynamicBundleTemplate::makeScript(io::IStream* stream)
{
    // ; PE receives IConsoleSaveable at primary+8.
    if (!BundleTemplate::makeScript(stream)) return false;
    std::ostringstream out;
    if (respawnDelay != -1.0f) out << "ObjectTemplate.respawnDelay " << respawnDelay << std::endl;
    const std::string text = out.str();
    return stream->write(text.c_str(), static_cast<unsigned int>(text.size())) == text.size();
}
void DynamicBundleTemplate::templateInitialize() {}

// the skeleton gates the whole block, but its saved name
// is the shipped, never-assigned static empty string. speedFactor is not serialized.
bool AntennaObjectTemplate::makeScript(io::IStream* stream)
{
    if (!SimpleObjectTemplate::makeScript(stream)) return false;
    std::ostringstream out;
    if (m_skeleton)
    {
        static std::string skeletonName;
        out << "ObjectTemplate.antenna.skeleton " << skeletonName << std::endl;
        out << "ObjectTemplate.antenna.maxSwayAngle " << maxSwayAngle[0] << '/' << maxSwayAngle[1] << std::endl;
        out << "ObjectTemplate.antenna.stiffness " << stiffness << std::endl;
        out << "ObjectTemplate.antenna.damping " << damping << std::endl;
        out << "ObjectTemplate.antenna.accelerationFactor " << accelerationFactor << std::endl;
    }
    const std::string text = out.str();
    return stream->write(text.c_str(), static_cast<unsigned int>(text.size())) == text.size();
}

// Vec3 equality helper PE 0x539360.
bool ForceObjectTemplate::makeScript(io::IStream* stream)
{
    if (!SimpleObjectTemplate::makeScript(stream)) return false;
    std::ostringstream out;
    if (input != 64) out << "ObjectTemplate.input " << static_cast<io::PlayerInputMap>(input) << std::endl;
    if (force != 0.0f) out << "ObjectTemplate.force " << force << std::endl;
    const float eps = 1.19209290e-7f;
    if (!(std::fabs(rotationalForce[0]) <= eps && std::fabs(rotationalForce[1]) <= eps && std::fabs(rotationalForce[2]) <= eps))
        out << "ObjectTemplate.rotationalForce " << rotationalForce[0] << '/' << rotationalForce[1] << '/' << rotationalForce[2] << std::endl;
    if (constantForce != 0.0f) out << "ObjectTemplate.constantForce " << constantForce << std::endl;
    if (isCounterForce) out << "ObjectTemplate.isCounterForce " << isCounterForce << std::endl;
    if (useAbsoluteDirection) out << "ObjectTemplate.useAbsoluteDirection " << useAbsoluteDirection << std::endl;
    if (startDelay != 0.0f) out << "ObjectTemplate.startDelay " << startDelay << std::endl;
    if (onlyWhenOccupied) out << "ObjectTemplate.onlyWhenOccupied " << onlyWhenOccupied << std::endl;
    const std::string text = out.str();
    return stream->write(text.c_str(), static_cast<unsigned int>(text.size())) == text.size();
}

// construct the stream before the base call, then emit unconditionally.
bool EntryPointTemplate::makeScript(io::IStream* stream)
{
    std::ostringstream out;
    if (!SimpleObjectTemplate::makeScript(stream)) return false;
    out << "ObjectTemplate.setEntryRadius " << m_entryRadius << std::endl;
    const std::string text = out.str();
    return stream->write(text.c_str(), static_cast<unsigned int>(text.size())) == text.size();
}

// . Ctor 69 B: a single 5.0f at +0x17C. createObject: new(0x13C) Obstacle(this).
BF2_LEAF_BUNDLE(ObstacleTemplate)
ObstacleTemplate::ObstacleTemplate() : m_unknown17C(5.0f) {}
IObject* ObstacleTemplate::createObject() const { return new Obstacle(this); }

// Attaches and registers spring physics. Server nodes start awake unless the template requests sleep.
BF2_LEAF_BUNDLE(SpringTemplate)

SpringTemplate::SpringTemplate()
{
    m_positionalFixation.x = m_positionalFixation.y = m_positionalFixation.z = 0.0f;
    m_axisFixation.x = 0.0f; m_axisFixation.y = 1.0f; m_axisFixation.z = 0.0f;
    m_damping = 0.5f;
    m_strength = 1.0f;
    m_rotateUV = false;
    m_fixAxisSpring = false;
}

// ELF 82AE110 / dedicated PE 60AE90. Retail tests x twice and never tests z;
// a change only in z is therefore omitted. Preserve that defect.
bool SpringTemplate::makeScript(io::IStream* stream)
{
    if (!BundleTemplate::makeScript(stream)) return false;
    std::ostringstream out;
    if (m_positionalFixation.x != 0.0f || m_positionalFixation.y != 0.0f || m_positionalFixation.x != 0.0f)
        out << "ObjectTemplate.setPositionalFixation " << m_positionalFixation.x << '/' << m_positionalFixation.y << '/' << m_positionalFixation.z << std::endl;
    if (m_axisFixation.x != 0.0f || m_axisFixation.y != 1.0f || m_axisFixation.x != 0.0f)
        out << "ObjectTemplate.setAxisFixation " << m_axisFixation.x << '/' << m_axisFixation.y << '/' << m_axisFixation.z << std::endl;
    if (m_strength != 1.0f) out << "ObjectTemplate.setStrength " << m_strength << std::endl;
    if (m_damping != 0.5f) out << "ObjectTemplate.setDamping " << m_damping << std::endl;
    if (m_rotateUV == true) out << "ObjectTemplate.rotateUV " << m_rotateUV << std::endl;
    if (m_fixAxisSpring == true) out << "ObjectTemplate.fixAxisSpring " << m_fixAxisSpring << std::endl;
    const std::string text = out.str();
    return stream->write(text.c_str(), static_cast<unsigned int>(text.size())) == text.size();
 }

IObject* SpringTemplate::createObject() const { return new Spring(this); }

// a tail-call to WheelSound::initTemplates (the "Rotation" sound template).
void SpringTemplate::templateInitialize()
{
    hfe::WheelSound::initTemplates(this, 0);
}

void SpringTemplate::setPhysicsNodeComponent(IObject* object) const
{
    PhysicsSpring* node = new PhysicsSpring(this, object);
    node->setObject(object);                                                    // +0x28
    object->setComponent(IID_IPhysicsNode, node);                               // slot 22
    node->release();                                                            // +0x08
    physicsNodeManager->addPhysicsNode(node);                                   // +0x1C
    if ((m_physicsFlags & 0x01) == 0 && gameLogic->isServer())                  // +0xAC bit 0, gameLogic +0x13C
        node->setIsAwake();                                                     // +0x130
    else
        node->setIsSleeping();                                                  // +0x120
}

// Triggerable-target defaults and runtime-object construction.
BF2_LEAF_BUNDLE(TriggerableTargetTemplate)
BF2_LEAF_IBASE_IMPL(TriggerableTargetTemplate, BundleTemplate)
TriggerableTargetTemplate::TriggerableTargetTemplate()
    : m_unresolved180(-1), m_targetType(0), m_movementSpeed(1), m_movementType(0),
      m_clockwise(false), m_delayBeforeStart(0), m_delayBeforeCycle(0),
      m_delayBeforeEnd(0), m_cycles(1), m_unresolved1A4("")
{
    m_refCount=1;
    m_networkableInfo="TriggerableTargetInfo";
    updateFlags(0x50,0);
    if (triggerableManager) triggerableManager->addTriggerableTargetTemplate(this);
}
IObject* TriggerableTargetTemplate::createObject() const { return new TriggerableTarget(this); }
// ELF81C84B0 / PE544140 (the PE implementation receives the script interface at +8).
bool TriggerableTargetTemplate::makeScript(io::IStream* stream)
{
    if (!BundleTemplate::makeScript(stream)) return false;
    std::ostringstream output;
    if (m_targetType) output << "ObjectTemplate.targetType " << m_targetType << std::endl;
    const std::string text=output.str();
    return stream->write(text.c_str(),static_cast<unsigned int>(text.size()))==text.size();
}

// . Ctor 264 B: refcount 1, then an SGI list head allocated at +0x180 (a 12-byte
// self-linked node), 3 at +0x184, 5.0f at +0x188, -1 at +0x18C, 100.0f at +0x190, 10.0f at
// +0x194 1.0f at +0x198, three false bools, and a "TriggerableInf..." literal into +0x10.
// createObject: new(0x170) Triggerable(this).
BF2_LEAF_BUNDLE(TriggerableTemplate)
BF2_LEAF_IBASE_IMPL(TriggerableTemplate, BundleTemplate)
IObject* TriggerableTemplate::createObject() const { return new Triggerable(this); }
// ELF81C6050: preserve the setter's inclusive clamp and NaN pass-through.
void TriggerableTemplate::setChance(float chance)
{
    if (chance<0) m_float190=0;
    else if (chance>1) m_float190=1;
    else m_float190=chance;
}
void TriggerableTemplate::addTriggerableTarget(std::string name) { m_targetNames.push_back(name); }
// ELF81C6090 / PE543ED0: only radius (when nonzero) and unconditional triggerId.
bool TriggerableTemplate::makeScript(io::IStream* stream)
{
    if (!BundleTemplate::makeScript(stream)) return false;
    std::ostringstream output;
    if (m_radius!=0) output << "ObjectTemplate.radius " << m_radius << std::endl;
    output << "ObjectTemplate.triggerId " << m_triggerId << std::endl;
    const std::string text=output.str();
    return stream->write(text.c_str(),static_cast<unsigned int>(text.size()))==text.size();
}

// ELF81C5CE0 / dedicated PE5464D0. List, defaults, networkable name,
// flags, then guarded registration.
TriggerableTemplate::TriggerableTemplate()
{
    m_refCount = 1;
    m_types = 3;
    m_radius = 5.0f;
    m_triggerId = -1;
    m_float190 = 100.0f;
    m_float194 = 10.0f;
    m_float198 = 1.0f;
    m_bool19C = false;
    m_bool19D = false;
    m_bool19E = false;
    m_networkableInfo = "TriggerableInfo";
    updateFlags(0x50, 0);
    if (triggerableManager)
        triggerableManager->addTriggerableTemplate(this);
}

// . Ctor 81 B: updateFlags(0x40, 0) and nothing else of its own.
UAVVehicleTemplate::UAVVehicleTemplate()
{
    updateFlags(0x40, 0);
}
BF2_LEAF_BUNDLE(UAVVehicleTemplate)

// createObject - 47 B: new(0x148). the cascade's third family
// (Game/Objects/Game/UAVVehicle.{h,cpp}).
IObject* UAVVehicleTemplate::createObject() const
{
    return new UAVVehicle(this);
}

// Rotor geometry, LOD, and rotation defaults.
BF2_LEAF_BUNDLE(RotorTemplate)
RotorTemplate::RotorTemplate()
    : m_changeLodAt(0.2f), m_tolerance(0.025f), m_rotationSpeedMod(1.0f),
      m_rotationSpeedModBlur(0.1f), m_rotationAxle(2)
{
}

// ELF 82ACEA0 / dedicated PE 60BBA0: stream construction precedes the base.
bool RotorTemplate::makeScript(io::IStream* stream)
{
    std::ostringstream out;
    if (!BundleTemplate::makeScript(stream)) return false;
    out << "ObjectTemplate.changeLodAt " << m_changeLodAt << std::endl;
    out << "ObjectTemplate.tolerance " << m_tolerance << std::endl;
    out << "ObjectTemplate.rotationSpeedMod " << m_rotationSpeedMod << std::endl;
    out << "ObjectTemplate.rotationSpeedModBlur " << m_rotationSpeedModBlur << std::endl;
    out << "ObjectTemplate.rotationAxle " << m_rotationAxle << std::endl;
    out << "ObjectTemplate.engineName " << m_engineName << std::endl;
    const std::string text = out.str();
    return stream->write(text.c_str(), static_cast<unsigned int>(text.size())) == text.size();
 }
IObject* RotorTemplate::createObject() const { return new Rotor(this); }

// `cmp edx, 2 / ja` then the store to +0x190 - an unsigned
// bound, so 0..2 are accepted and everything else, negatives included, is ignored.
void RotorTemplate::setRotationAxle(int axis)
{
    if (static_cast<unsigned int>(axis) <= 2u)
        m_rotationAxle = axis;
}

#undef BF2_LEAF_IBASE_IMPL
#undef BF2_LEAF_SIMPLE
#undef BF2_LEAF_BUNDLE
#undef BF2_LEAF_NO_OBJECT

} // namespace world
} // namespace hfe
} // namespace dice
