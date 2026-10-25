// Grouped template lifecycle and factory implementations. [INFERRED] Source placement.
// SimpleObjectTemplate leaves test class ids directly; BundleTemplate leaves delegate ancestor lookup.

#include "AnimatedBundleTemplate.h"
#include "Game/AnimatedBundle.h"                 // AnimatedBundleTemplate::createObject's product
#include "AntennaObjectTemplate.h"
#include "Game/AntennaObject.h"                 // AntennaObjectTemplate::createObject's product
#include "ForceObjectTemplate.h"
#include "Game/ForceObject.h"                   // ForceObjectTemplate::createObject product
#include "FreeCameraTemplate.h"
#include "Base/FreeCamera.h"                 // createObject - the object it builds
#include "RemoteControlledObjectTemplate.h"
#include "IPlayerControlObjectTemplate.h"
#include "Game/RemoteControlledObject.h"        // RemoteControlledObjectTemplate::createObject product
#include "Game/TurnableRemoteControlledObject.h"   // TurnableRemoteControlledObjectTemplate::createObject product
#include "SupplyObjectTemplate.h"
#include "Game/SupplyObject.h"                   // SupplyObjectTemplate::createObject's product
#include "TargetObjectTemplate.h"
#include "Game/TargetObject.h"                   // TargetObjectTemplate::createObject's product
#include "Projectiles/ProjectileCompTemplates.h" // TargetType writer used by makeScript
#include "OverheadCameraTemplate.h"
#include "Game/OverheadCamera.h"                 // OverheadCameraTemplate::createObject's product
#include "GrapplingHookRopeTemplate.h"
#include "Game/GrapplingHookRope.h"             // GrapplingHookRopeTemplate::createObject product
#include "IObject.h"
#include "IPlayer.h"

#include "../../Animation/BoneAnimation/AnimationSystem.h"
#include "../../Animation/BoneAnimation/Skeleton.h"
#include "../../Core/Ids.h"
#include "../../Core/StringUtil.h"
#include "../../IO/Stream/IStream.h"
#include "../../IO/Network/INetworkable.h"
#include "../GameEvents/GrapplingHookCreateEvent.h"
#include "../IGame.h"
#include "../IGameLogic.h"

#include <cstdio>
#include <cstdlib>
#include <sstream>

namespace dice {
namespace hfe {
namespace world {

// AntennaObjectTemplate - CID 38045, the frontier that surfaced the whole group.
// SimpleObjectTemplate base. Ctor 117 B: refcount 1, then a run of tuned floats - two 45.0f
// (0x42340000), a 1.0f, a 0.1f, and two more 1.0f. createObject: new(0x174) AntennaObject.
AntennaObjectTemplate::AntennaObjectTemplate()
{
    m_refCount = 1;                              // +0x164
    m_skeleton = 0;                              // +0x168 the antenna.skeleton pointer
    maxSwayAngle[0] = 45.0f;                     // +0x16C the two-component value the header
    maxSwayAngle[1] = 45.0f;                     // corrected from a scalar; both are 45 degrees
    stiffness = 1.0f;
    damping = 0.1f;
    accelerationFactor = 1.0f;
    speedFactor = 1.0f;
}

AntennaObjectTemplate::~AntennaObjectTemplate() {}

unsigned int AntennaObjectTemplate::getClassID() const { return CID_AntennaObjectTemplate; }

void* AntennaObjectTemplate::queryImplementation(unsigned int classId)
{
    if (classId == CID_AntennaObjectTemplate ||
        classId == CID_SimpleObjectTemplate ||
        classId == CID_ObjectTemplate)
        return this;
    return 0;
}

void AntennaObjectTemplate::addRef()  { ++m_refCount; }
int  AntennaObjectTemplate::getRef()  { return m_refCount; }
int  AntennaObjectTemplate::release()
{
    const int r = --m_refCount;
    if (r == 0) delete this;
    return r;
}
void* AntennaObjectTemplate::queryInterface(unsigned int iid) const
{
    return SimpleObjectTemplate::queryInterface(iid);
}
// createObject 0x816A990 (47 B): new(0x174) AntennaObject(this) -
IObject* AntennaObjectTemplate::createObject() const { return new AntennaObject(this); }

// AnimatedBundleTemplate - BundleTemplate base. Ctor 93 B: refcount 1 at +0x17C, then +0x180,
// +0x184 and +0x188 zeroed. createObject: new(0x154) AnimatedBundle.
AnimatedBundleTemplate::AnimatedBundleTemplate()
{
    m_refCount3 = 1;                             // +0x17C (380) - the header's own name
    m_skeleton = 0;                              // +0x180 three zeroed pointers (the
    m_animationSystem1P = 0;                     // +0x184 old note read them as _S_empty strings; the raw
    m_animationSystem3P = 0;                     // +0x188 is three zeroed registers - see the header)
}

// The dtor (ELF 0x8167A50, PE 0x592160): delete the skeleton first, then hand both animation-system
// template pointers to the manager's release method without null guards. release(0) is explicitly
// null-safe; preserving the calls matters because a non-null release also clears the manager's
// active template. The base destructor follows automatically.
AnimatedBundleTemplate::~AnimatedBundleTemplate()
{
    delete m_skeleton;
    anim::animationSystemTemplateManager->release(m_animationSystem1P);
    anim::animationSystemTemplateManager->release(m_animationSystem3P);
}

// skeleton - ELF 0x8168110, 118 B; PE 0x592470. The newly allocated pointer is stored before the
// loaded flag is tested. A failed load deletes that same object and clears the member. Retail does
// not release an older member value before overwriting it.
void AnimatedBundleTemplate::skeleton(const std::string& name)
{
    bool loaded = false;
    anim::Skeleton* const value = new anim::Skeleton(name, &loaded, 0);
    m_skeleton = value;
    if (!loaded)
    {
        delete value;
        m_skeleton = 0;
    }
}

// animationSystem1P / animationSystem3P - ELF 0x8168190 / 0x81681C0 (40 B each), PE 0x5921D0 /
// (32 B each). One manager load and one store; no manager/result/old-value guard.
void AnimatedBundleTemplate::animationSystem1P(const std::string& name)
{
    m_animationSystem1P = anim::animationSystemTemplateManager->load(name);
}

void AnimatedBundleTemplate::animationSystem3P(const std::string& name)
{
    m_animationSystem3P = anim::animationSystemTemplateManager->load(name);
}

unsigned int AnimatedBundleTemplate::getClassID() const { return CID_AnimatedBundleTemplate; }

void* AnimatedBundleTemplate::queryImplementation(unsigned int classId)
{
    if (classId == CID_AnimatedBundleTemplate)
        return this;
    return BundleTemplate::queryImplementation(classId);
}

void AnimatedBundleTemplate::addRef()  { ++m_refCount3; }
int  AnimatedBundleTemplate::getRef()  { return m_refCount3; }
int  AnimatedBundleTemplate::release()
{
    const int r = --m_refCount3;
    if (r == 0) delete this;
    return r;
}
void* AnimatedBundleTemplate::queryInterface(unsigned int iid) const
{
    return BundleTemplate::queryInterface(iid);
}
// Creates an animated bundle using this template.
IObject* AnimatedBundleTemplate::createObject() const { return new AnimatedBundle(this); }

// ForceObjectTemplate - SimpleObjectTemplate base. Ctor 120 B, and its +0x164 is NOT a refcount:
// the class has no quartet at all, and the ctor seeds that slot with 64 - the same 0x40
// unmapped-input code PlayerControlObjectTemplate and RotationalBundleTemplate use. Everything
// after it is zeroed: a dword, a Vec3 at +0x16C, a dword at +0x178, two bools, a dword and a
// final bool. createObject: new(0x148) ForceObject.
ForceObjectTemplate::ForceObjectTemplate()
{
    input = 64;                                  // 0x40 - the header's [INFERRED] PlayerInputMap
                                                 // reading is confirmed: same code PCO uses
    force = 0.0f;
    rotationalForce[0] = 0.0f; rotationalForce[1] = 0.0f; rotationalForce[2] = 0.0f;
    constantForce = 0.0f;
    isCounterForce = false;
    useAbsoluteDirection = false;
    startDelay = 0.0f;
    onlyWhenOccupied = false;
}

ForceObjectTemplate::~ForceObjectTemplate() {}

unsigned int ForceObjectTemplate::getClassID() const { return CID_ForceObjectTemplate; }

void* ForceObjectTemplate::queryImplementation(unsigned int classId)
{
    if (classId == CID_ForceObjectTemplate ||
        classId == CID_SimpleObjectTemplate ||
        classId == CID_ObjectTemplate)
        return this;
    return 0;
}
// createObject 0x82A8670 (47 B): new(0x148) ForceObject(this) -
IObject* ForceObjectTemplate::createObject() const { return new ForceObject(this); }

// FreeCameraTemplate - BundleTemplate base. Ctor 109 B: refcount 1 at +0x17C, then TWO 1.2f
// values (0x3F99999A) at +0x180/+0x184, and updateFlags(0x42, 0) - bits 1 and 6 together, a
// combination no other recovered template uses. createObject: new(0x294) FreeCamera.
FreeCameraTemplate::FreeCameraTemplate()
{
    m_refCount = 1;                              // +0x17C (380)
    m_defaultFoV = 1.2f;                         // radians (~69 degrees), not degrees
    m_activeFoV = 1.2f;

    updateFlags(0x42, 0);
}

FreeCameraTemplate::~FreeCameraTemplate() {}

unsigned int FreeCameraTemplate::getClassID() const { return CID_FreeCameraTemplate; }

void* FreeCameraTemplate::queryImplementation(unsigned int classId)
{
    if (classId == CID_FreeCameraTemplate)
        return this;
    return BundleTemplate::queryImplementation(classId);
}

void FreeCameraTemplate::addRef()  { ++m_refCount; }
int  FreeCameraTemplate::getRef()  { return m_refCount; }
int  FreeCameraTemplate::release()
{
    const int r = --m_refCount;
    if (r == 0) delete this;
    return r;
}
void* FreeCameraTemplate::queryInterface(unsigned int iid) const
{
    return BundleTemplate::queryInterface(iid);
}
// Creates the free camera, including during template precaching.
IObject* FreeCameraTemplate::createObject() const { return new FreeCamera(this); }

// RemoteControlledObjectTemplate - CID 40002 ("ArtilleryControlObjectMEC"), the frontier that
// followed. BundleTemplate base. Ctor 148 B: refcount 1 at +0x17C, rcType 0, the UAV block zeroed,
// and uavVehicleTemplate constructed AS A string - which is what settled its declared type.
// The ctor stops at +0x198: turnVelocity (+0x1A0) and everything past it are NOT written by it.
// createObject: new(0x150) RemoteControlledObject.
RemoteControlledObjectTemplate::RemoteControlledObjectTemplate()
{
    m_refCount = 1;                              // +0x17C
    rcType = 0;                                  // +0x180
    minTimeBetweenActivation = 0.0f;             // +0x184
    // uavVehicleTemplate (+0x188): default-constructed empty, as the ELF's _S_empty construction.
    uavVehicleSpeed = 0.0f;                      // +0x18C
    uavVehicleRadius = 0.0f;                     // +0x190
    uavVehicleFlightHeight = 0.0f;               // +0x194
    uavVehicleSampleResolution = 0.0f;            // +0x198
    // +0x19C onward - turnVelocity and its neighbours - are NOT touched by the ctor.
}

RemoteControlledObjectTemplate::~RemoteControlledObjectTemplate() {}

unsigned int RemoteControlledObjectTemplate::getClassID() const
{
    return CID_RemoteControlledObjectTemplate;
}

void* RemoteControlledObjectTemplate::queryImplementation(unsigned int classId)
{
    if (classId == CID_RemoteControlledObjectTemplate)
        return this;
    return BundleTemplate::queryImplementation(classId);
}

void RemoteControlledObjectTemplate::addRef()  { ++m_refCount; }
int  RemoteControlledObjectTemplate::getRef()  { return m_refCount; }
int  RemoteControlledObjectTemplate::release()
{
    const int r = --m_refCount;
    if (r == 0) delete this;
    return r;
}
void* RemoteControlledObjectTemplate::queryInterface(unsigned int iid) const
{
    return BundleTemplate::queryInterface(iid);
}
// createObject 0x81A4390 (47 B): new(0x150) RemoteControlledObject(this) -
IObject* RemoteControlledObjectTemplate::createObject() const { return new RemoteControlledObject(this); }

// read end to end. The UAV configuration block is deliberately NOT emitted here;
// retail only round-trips the non-zero type and activation cooldown from this class. The authored
// UAV fields still load through their recovered console bindings.
bool RemoteControlledObjectTemplate::makeScript(io::IStream* stream)
{
    if (!BundleTemplate::makeScript(stream))
        return false;

    std::ostringstream script;
    if (rcType != RCNone)
        script << "ObjectTemplate.rcType "
               << static_cast<RemoteControlledObjectType>(rcType) << std::endl;
    if (minTimeBetweenActivation != 0.0f)
    {
        script << "ObjectTemplate.minTimeBetweenActivation "
               << minTimeBetweenActivation << std::endl;
    }

    const std::string text = script.str();
    return stream->write(text.c_str(), static_cast<unsigned int>(text.length())) == text.length();
}

// TurnableRemoteControlledObjectTemplate - CID 40003 ("HorizontalTurnObject"). RTTI si confirms it
// derives RemoteControlledObjectTemplate, exactly as that header's commented guess said. Its own
// refcount at +0x19C pins the base's size, and the base's ctor stopping at +0x198 agrees.
// Ctor 80 B: refcount 1, turnVelocity 0. createObject: new(0x198) TurnableRemoteControlledObject.
TurnableRemoteControlledObjectTemplate::TurnableRemoteControlledObjectTemplate()
{
    m_refCount = 1;                              // +0x19C
    turnVelocity = 0.0f;                         // +0x1A0
}

TurnableRemoteControlledObjectTemplate::~TurnableRemoteControlledObjectTemplate() {}

unsigned int TurnableRemoteControlledObjectTemplate::getClassID() const
{
    return CID_TurnableRemoteControlledObjectTemplate;
}

void* TurnableRemoteControlledObjectTemplate::queryImplementation(unsigned int classId)
{
    if (classId == CID_TurnableRemoteControlledObjectTemplate)
        return this;
    return RemoteControlledObjectTemplate::queryImplementation(classId);
}

void TurnableRemoteControlledObjectTemplate::addRef()  { ++m_refCount; }
int  TurnableRemoteControlledObjectTemplate::getRef()  { return m_refCount; }
int  TurnableRemoteControlledObjectTemplate::release()
{
    const int r = --m_refCount;
    if (r == 0) delete this;
    return r;
}
void* TurnableRemoteControlledObjectTemplate::queryInterface(unsigned int iid) const
{
    return RemoteControlledObjectTemplate::queryInterface(iid);
}
// createObject 0x81CBE40 (47 B): new(0x198) TurnableRemoteControlledObject(this) -
IObject* TurnableRemoteControlledObjectTemplate::createObject() const { return new TurnableRemoteControlledObject(this); }

// . Base failure is a hard gate; the leaf emits only a non-default turn velocity.
bool TurnableRemoteControlledObjectTemplate::makeScript(io::IStream* stream)
{
    if (!RemoteControlledObjectTemplate::makeScript(stream))
        return false;

    std::ostringstream script;
    if (turnVelocity != 0.0f)
        script << "ObjectTemplate.turnVelocity " << turnVelocity << std::endl;

    const std::string text = script.str();
    return stream->write(text.c_str(), static_cast<unsigned int>(text.length())) == text.length();
}


// Supply-object template lifecycle.
SupplyObjectTemplate::SupplyObjectTemplate()
{
    radius = 2.0f;                               // +0x17C
    team = 0;                                    // +0x180

    workOnVehicles = false;                      // +0x184
    workOnSoldiers = true;                       // +0x185 the one non-zero bool

    damageSpeed = 0.0f;                          // +0x188
    healSpeed = 0.0f;                            // +0x18C
    refillAmmoSpeed = 0.0f;                      // +0x190
    restoreSpeed = 0.0f;                         // +0x194

    healStorageSize = -1.0f;                     // +0x198 0xBF800000, the unlimited sentinel
    ammoStorageSize = -1.0f;                     // +0x19C likewise
    sharedStorageSize = 0.0f;                    // +0x1A0

    visibleOn3dMap = false;                      // +0x1A4
    mapIcon3d = -1;                              // +0x1A8 .con `3dMapIcon`; -1 = none
    destroyWhenEmptyStorage = false;             // +0x1AC

    // vehicleTypes (+0x1B0..+0x1BB): the ctor nulls all three words, i.e. an empty vector - which
    // testVehicleType reads as "no restriction, accept every vehicle". Default-constructed here.

    teamFromClosestCP = false;                   // +0x1BC

    updateFlags(0x40, 0);
}

SupplyObjectTemplate::~SupplyObjectTemplate() {}

unsigned int SupplyObjectTemplate::getClassID() const { return CID_SupplyObjectTemplate; }

void* SupplyObjectTemplate::queryImplementation(unsigned int classId)
{
    if (classId == CID_SupplyObjectTemplate)
        return this;
    return BundleTemplate::queryImplementation(classId);
}
// Creates a supply object using this template.
IObject* SupplyObjectTemplate::createObject() const
{
    return new SupplyObject(this);
}

// makeScript - PE 0x59B0A0 (1,111 B), read end to end. The omission tests are against the exact
// constructor defaults. The vehicle-category loop re-reads size and emits one line per element.
bool SupplyObjectTemplate::makeScript(io::IStream* stream)
{
    if (!BundleTemplate::makeScript(stream))
        return false;

    std::ostringstream script;
    if (team != 0)
        script << "ObjectTemplate.team " << team << std::endl;
    if (radius != 2.0f)
        script << "ObjectTemplate.radius " << radius << std::endl;
    if (workOnSoldiers != true)
        script << "ObjectTemplate.workOnSoldiers " << workOnSoldiers << std::endl;
    if (workOnVehicles)
        script << "ObjectTemplate.workOnVehicles " << workOnVehicles << std::endl;
    if (healSpeed != 0.0f)
        script << "ObjectTemplate.healSpeed " << healSpeed << std::endl;
    if (damageSpeed != 0.0f)
        script << "ObjectTemplate.damageSpeed " << damageSpeed << std::endl;
    if (refillAmmoSpeed != 0.0f)
        script << "ObjectTemplate.refillAmmoSpeed " << refillAmmoSpeed << std::endl;
    if (restoreSpeed != 0.0f)
        script << "ObjectTemplate.restoreSpeed " << restoreSpeed << std::endl;
    if (healStorageSize != -1.0f)
        script << "ObjectTemplate.healStorageSize " << healStorageSize << std::endl;
    if (ammoStorageSize != -1.0f)
        script << "ObjectTemplate.ammoStorageSize " << ammoStorageSize << std::endl;
    if (sharedStorageSize != 0.0f)
        script << "ObjectTemplate.sharedStorageSize " << sharedStorageSize << std::endl;
    if (destroyWhenEmptyStorage)
    {
        script << "ObjectTemplate.destroyWhenEmptyStorage "
               << destroyWhenEmptyStorage << std::endl;
    }
    if (teamFromClosestCP)
        script << "ObjectTemplate.teamFromClosestCP " << teamFromClosestCP << std::endl;
    if (visibleOn3dMap)
        script << "ObjectTemplate.visibleOn3dMap " << visibleOn3dMap << std::endl;
    if (mapIcon3d != -1)
        script << "ObjectTemplate.3dMapIcon " << mapIcon3d << std::endl;
    for (unsigned int i = 0; i < vehicleTypes.size(); ++i)
    {
        script << "ObjectTemplate.addVehicleType "
               << static_cast<VehicleCategory>(vehicleTypes[i]) << std::endl;
    }

    const std::string text = script.str();
    return stream->write(text.c_str(), static_cast<unsigned int>(text.length())) == text.length();
}
// testVehicleType - ELF 0x81BF240 / PE 0x59B000. Both query the object's template for the complete
// IPlayerControlObjectTemplate secondary face and dispatch getVehicleCategory at +0x24. A failed
// query rejects; an empty vector accepts every PCO; size and category are re-read on every pass.
bool SupplyObjectTemplate::testVehicleType(IObject* object) const
{
    IPlayerControlObjectTemplate* pcoTemplate =
        static_cast<IPlayerControlObjectTemplate*>(
            object->getTemplate()->queryInterface(IID_IPlayerControlObjectTemplate));
    if (!pcoTemplate)
        return false;

    if (vehicleTypes.empty())
        return true;

    for (unsigned int i = 0; i < vehicleTypes.size(); ++i)
    {
        if (pcoTemplate->getVehicleCategory() == vehicleTypes[i])
            return true;
    }
    return false;
}

// TargetObjectTemplate - CID 38078. BundleTemplate base. Ctor 140 B.
// The ctor's shape is worth stating because it is not a flat store list: it writes targetType, the
// two bools and the delay directly, but reaches timeToLive through setTimeToLive(5.0f) - which is
// what exposed the second field hiding behind it.
TargetObjectTemplate::TargetObjectTemplate()
{
    targetType = 2;                              // +0x17C
    disableWhenWreck = false;                    // +0x188
    disableWhenEmptyVehicle = false;             // +0x189
    disableWhenEmptyVehicleDelay = 0.0f;         // +0x18C

    // Initialize lifetime and its derived simulation-tick count.
    setTimeToLive(5.0f);                         // => timeToLive 5.0f, timeToLiveTicks 151

    updateFlags(0x40, 0);
}

// The 30 Hz constant is flt_888B174 = 30.0f, read from the binary rather than assumed - the same
// fixed timestep GameLogic and GenericProjectileTemplate both bake in.
void TargetObjectTemplate::setTimeToLive(float seconds)
{
    timeToLive = seconds;
    timeToLiveTicks = static_cast<int>(seconds * 30.0f) + 1;   // truncating conversion, then +1
}

TargetObjectTemplate::~TargetObjectTemplate() {}

unsigned int TargetObjectTemplate::getClassID() const { return CID_TargetObjectTemplate; }

void* TargetObjectTemplate::queryImplementation(unsigned int classId)
{
    if (classId == CID_TargetObjectTemplate)
        return this;
    return BundleTemplate::queryImplementation(classId);
}
// createObject - 47 B. the cascade's second family ("UniqueTarget" is the
// next 0x40-flagged template after supply_crate). Game/Objects/Game/TargetObject.{h,cpp}.
IObject* TargetObjectTemplate::createObject() const
{
    return new TargetObject(this);
}

// makeScript - PE 0x58AAE0 (420 B), read end to end. timeToLive and targetType are always emitted.
// The empty-vehicle delay is nested under its enabling flag; a non-zero delay with the flag off is
// therefore intentionally not round-tripped.
bool TargetObjectTemplate::makeScript(io::IStream* stream)
{
    if (!BundleTemplate::makeScript(stream))
        return false;

    std::ostringstream script;
    script << "ObjectTemplate.timeToLive " << timeToLive << std::endl;
    script << "ObjectTemplate.targetType "
           << static_cast<TargetType>(targetType) << std::endl;
    if (disableWhenEmptyVehicle)
    {
        script << "ObjectTemplate.disableWhenEmptyVehicle "
               << disableWhenEmptyVehicle << std::endl;
        if (disableWhenEmptyVehicleDelay != 0.0f)
        {
            script << "ObjectTemplate.disableWhenEmptyVehicleDelay "
                   << disableWhenEmptyVehicleDelay << std::endl;
        }
    }
    if (disableWhenWreck)
        script << "ObjectTemplate.disableWhenWreck " << disableWhenWreck << std::endl;

    const std::string text = script.str();
    return stream->write(text.c_str(), static_cast<unsigned int>(text.length())) == text.length();
}

// OverheadCameraTemplate - CID 38028, the commander's view. BundleTemplate base. Ctor 125 B, and
// the cleanest of the four: three tuned floats and a refcount.
OverheadCameraTemplate::OverheadCameraTemplate()
{
    m_refCount3 = 1;                             // +0x17C
    m_activeFOV = 2.3f;                          // +0x180
    m_speed = 10.0f;                             // +0x184
    m_height = 160.0f;                           // +0x188 (0x43200000) - metres above the map

    updateFlags(0x40, 0);
}

OverheadCameraTemplate::~OverheadCameraTemplate() {}

unsigned int OverheadCameraTemplate::getClassID() const { return CID_OverheadCameraTemplate; }

void* OverheadCameraTemplate::queryImplementation(unsigned int classId)
{
    if (classId == CID_OverheadCameraTemplate)
        return this;
    return BundleTemplate::queryImplementation(classId);
}

void OverheadCameraTemplate::addRef()  { ++m_refCount3; }
int  OverheadCameraTemplate::getRef()  { return m_refCount3; }
int  OverheadCameraTemplate::release()
{
    const int r = --m_refCount3;
    if (r == 0) delete this;
    return r;
}
void* OverheadCameraTemplate::queryInterface(unsigned int iid) const
{
    // The ELF emits its own 84-byte body here, byte-identical to BundleTemplate's 84-byte one -
    // the five-case table quoted in this class's header. Forwarding reproduces it exactly.
    return BundleTemplate::queryInterface(iid);
}
// createObject - 47 B. the cascade's fourth family
// (Game/Objects/Game/OverheadCamera.{h,cpp}).
IObject* OverheadCameraTemplate::createObject() const
{
    return new OverheadCamera(this);
}

// makeScript - ELF 0x81E8D40 (829 B), PE 0x5F6180 (124 B). Both shipped ports deliberately skip
// BundleTemplate::makeScript, ignore this template's three fields, and submit an empty buffer to
// IStream::write. The size difference is old libstdc++ ostringstream machinery, not behavior.
bool OverheadCameraTemplate::makeScript(io::IStream* stream)
{
    std::ostringstream script;
    const std::string text = script.str();
    return stream->write(text.c_str(), static_cast<unsigned int>(text.length())) == text.length();
}

// GrapplingHookRopeTemplate - CID 40013. SimpleObjectTemplate base (so the flat three-cid
// queryImplementation and the refcount at +0x164, not +0x17C). Ctor 350 B - the largest of the
// four. The ELF/PE factories allocate one final float at +0x1D0/+0x294 which the constructors do
// not write; live-rope initialize reads it, so that shipped uninitialized tail remains untouched.
GrapplingHookRopeTemplate::GrapplingHookRopeTemplate()
{
    m_refCount2 = 1;                             // +0x164
    m_seatAnimationSystem = 0;                   // +0x168 no animation system claimed yet
    // m_animationName (+0x16C): default-constructed empty, which is what the ELF's
    // _S_empty_rep_storage construction does. This is the slot the compact pass called "1".
    m_player = 0;                                // +0x170

    _float174 = 14.0f;                           // +0x174
    _dword178 = 28;                              // +0x178 (0x1C)

    m_ropeUnusedLifetime = 40.0f;                // +0x17C (no console property)
    m_throwStrength = 0.00125f;                  // +0x180
    m_upThrowBoost = 0.5f;                       // +0x184
    m_degradeThrowStrength = 0.8f;               // +0x188
    m_throwStrength2 = 0.0012f;                  // +0x18C
    m_upThrowBoost2 = 0.39f;                     // +0x190
    m_degradeThrowStrength2 = 0.98f;             // +0x194
    m_dropStrength = 0.35f;                      // +0x198
    m_upDropBoost = 0.9f;                        // +0x19C
    m_degradeDropStrength = 0.96f;               // +0x1A0
    m_linkAttachMovement = 0.05f;                // +0x1A4
    m_linkAttachTime = 0.5f;                     // +0x1A8
    m_dismountCheckOffset = 0.25f;               // +0x1AC
    m_minYNormal = 0.5f;                         // +0x1B0
    m_attachClimberRadius = 3.7f;                // +0x1B4
    m_climbingSpeed = 2.5f;                      // +0x1B8
    m_elasticity = 1.1f;                         // +0x1BC
    m_airFriction = 0.98f;                       // +0x1C0
    m_climbableAngle = 0.5f;                     // +0x1C4
    m_dismountAngle = 0.75f;                     // +0x1C8
    m_climbersAttachForce = 0.14f;               // +0x1CC

    updateFlags(0x40, 0);
}

// opens by releasing the animation-system template when present. Everything else
// the ELF dtor does is implicit here: m_animationName's release (the `lock xadd`
// on [this+0x16C]) and SimpleObjectTemplate's own destruction, which GCC inlined into this body -
// the five string releases at +0x130/+0x12C/+0x128/+0x124/+0x11C and the container at +0xD8 all
// belong to the base, not to this class.
GrapplingHookRopeTemplate::~GrapplingHookRopeTemplate()
{
    if (m_seatAnimationSystem)
        anim::animationSystemTemplateManager->release(m_seatAnimationSystem);
}

// (168 B). Preserve the shipped empty-name replacement quirk: after releasing a
// previous value, an empty name performs no store, leaving the pointer stale.
void GrapplingHookRopeTemplate::setSeatAnimationSystem(const std::string& name)
{
    if (m_seatAnimationSystem)
    {
        if (compareIgnoreCase(getSeatAnimationSystem(), name) == 0)
            return;
        anim::animationSystemTemplateManager->release(m_seatAnimationSystem);
    }

    if (!name.empty())
        m_seatAnimationSystem = anim::animationSystemTemplateManager->load(name);
}

// (37 B), a direct manager +0x1C tail call in retail.
const std::string& GrapplingHookRopeTemplate::getSeatAnimationSystem()
{
    return anim::animationSystemTemplateManager->getPath(m_seatAnimationSystem);
}

unsigned int GrapplingHookRopeTemplate::getClassID() const
{
    return CID_GrapplingHookRopeTemplate;
}

void* GrapplingHookRopeTemplate::queryImplementation(unsigned int classId)
{
    if (classId == CID_GrapplingHookRopeTemplate ||
        classId == CID_SimpleObjectTemplate ||
        classId == CID_ObjectTemplate)
        return this;
    return 0;
}

void GrapplingHookRopeTemplate::addRef()  { ++m_refCount2; }
int  GrapplingHookRopeTemplate::getRef()  { return m_refCount2; }
int  GrapplingHookRopeTemplate::release()
{
    const int r = --m_refCount2;
    if (r == 0) delete this;
    return r;
}
void* GrapplingHookRopeTemplate::queryInterface(unsigned int iid) const
{
    // in the ELF, byte-identical to SimpleObjectTemplate's 56 B. Note the absence of an
    // IID_IBundleTemplate case, which is correct: this class is Simple-derived and has no
    // sub-object at +0x164 - that slot is its refcount instead.
    return SimpleObjectTemplate::queryInterface(iid);
}
// / PE counterpart: the ordinary leaf factory, now that the rope object surface is
// concrete. The specialized weapon builder below remains the deeper RW-47 boundary.
IObject* GrapplingHookRopeTemplate::createObject() const
{
    return new GrapplingHookRope(this);
}

// . This specialized firearm builder is server-only and accepts only a
// soldier vehicle. The supplied transform contributes translation as the spawn/event position and
// its third basis row as the rotation/event direction. createObjectOnAllClients is deliberately
// dereferenced for getNetworkable before the later null guard, exactly as retail.
GrapplingHookRope* GrapplingHookRopeTemplate::createGrapplingHookObject(IPlayer* player,
                                                                        Mat4& transform,
                                                                        float scalar)
{
    if (!player)
        return 0;
    if (!gameLogic->isServer())
        return 0;

    m_player = player;
    IObject* vehicle = player->getVehicle();
    if (!vehicle || vehicle->getTemplate()->getClassID() != CID_SoldierTemplate)
        return 0;

    Vec3 position;
    position.x = transform.m[12];
    position.y = transform.m[13];
    position.z = transform.m[14];
    Vec3 direction;
    direction.x = transform.m[8];
    direction.y = transform.m[9];
    direction.z = transform.m[10];

    GrapplingHookRope* rope = static_cast<GrapplingHookRope*>(
        game->createObjectOnAllClients(this, position, direction, true, 0, 0));
    io::INetworkable* networkable = rope->getNetworkable(); // no rope null guard in either port
    if (networkable)
    {
        game->sendGameEventToAllPlaying(new GrapplingHookCreateEvent(
            networkable->m_networkableId,
            player->getId(),
            scalar,
            position,
            direction));
    }

    if (rope)
    {
        rope->m_ownerPlayerId = player->getId();
        rope->m_creationScalar = scalar;
        rope->m_creationPosition = position;
        rope->m_creationDirection = direction;
        rope->initialize();
    }
    return rope;
}
// makeScript: NOT overridden - inherits SimpleObjectTemplate's emitter.

} // namespace world
} // namespace hfe
} // namespace dice
