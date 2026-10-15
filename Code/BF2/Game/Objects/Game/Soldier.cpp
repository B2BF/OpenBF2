// Path: attested by __FILE__; recovered from the symbolized ELF.
// Infantry lifecycle, input, equipment, movement and animation. Read Soldier.h for layout.
// Server animation feeds collision skeletons. Item index 0 is deliberately not returned.
// Keep camera, kit and animation order; remaining [FRONTIER] bodies stay explicit.


#include "Soldier.h"

#include <cmath>
#include <algorithm>
#include <cstdio>
#include <cstdlib>

#include "SoldierTemplate.h"
#include "Kit.h"
#include "KitTemplate.h"                   // addWeapon's m_defaultWeaponIndex match
#include "../Item.h"                       // IItemObject faces - addItem / enableItem / setActiveWeapon
#include "../GenericFireArmTemplate.h"     // isSelectable / the gas-mask + NVG device flags
#include "../Weapons/GenericFireArm.h"     // primary-vtable firearm calls
#include "../IItemTemplate.h"              // addWeapon's default-weapon match
#include "../../../Core/Debug.h"           // the four attested Soldier.cpp Debug lines
#include "../../../Scene/Object/ObjectUtil.h"   // world::updateFlagsRecursive
#include "../../../Physics/PhysicsUtil.h"  // isPhysicsDisabled / disablePhysics
#include "../../Python/PythonHost.h"       // buildArgs + handleEvent(PlayerChangeWeapon)
#include "../../Python/PyRef.h"
#include "../../Utility/Vars.h"            // the tuning farm - the _0ii_172 var registrations
#include "../../../Sound/SoldierSound.h"   // the ctor makes one
#include "../../../Sound/NullSound.h"
#include "../../../Animation/BoneAnimation/Skeleton.h"   // initAnimation makes two
#include "../../../Animation/BoneAnimation/RagDoll.h"
#include "../../../Animation/BoneAnimation/AnimationSystem.h"   // the manager at ds:0x8AB0F88
#include "../../../Scene/Object/ObjectTypes/Base/ObjectTemplateManager.h"   // "fallingWeapon"
#include "../ICameraObject.h"
#include "../IObject.h"
#include "../IAnimatedBundle.h"         // getCollisionSkeleton restores the active item's rig
#include "../IWeaponObject.h"
#include "../IObjectTemplate.h"
#include "../IPlayerControlObjectTemplate.h"
#include "../IPlayer.h"
#include "../Base/SimpleObjectTemplate.h"
#include "../ObjectLayoutAccess.h"
#include <string>                                             // playDamageAnimations' trigger names
#include "../GameLogicComp/IArmorGLComp.h"                     // getLastHitDirection / getLastHitBoneIndex
#include "../../IGameLogic.h"                    // gameLogic - addPco (+0x194)
#include "../../../Core/EventManager.h"          // eventManager - enter()'s AI event
#include "../GameLogicComp/ArmorGLComp.h"        // handleMessage reads the armor status
#include "../../../AI/AIInterface/AIInstance.h"  // the AI singleton, ds:0x8AC37F8
#include "../../../AI/AIInterface/BFKnowledge/IAIObject.h"   // init()'s lock bracket, the revive's init()
#include "../../GameServer/ServerSettings.h"     // serverSettings - the man-down time
#include "../../GameLogic/ServerGameLogic.h"     // the kit handout on revive
#include "../../../Scene/Object/PlayerManager.h" // playerManager - the medic call
#include "../../../Scene/Object/ObjectTypes/Base/ObjectTemplateManager.h"
#include "../../../Core/Ids.h"
#include "../../../Physics/PointPhysicsNode.h"
#include "../../../Physics/SoldierResponse.h"
#include "../../../Physics/HeightmapCluster.h"   // getWaterLevelInWorldCoords
#include "../../../Physics/MaterialManager.h"    // handleCollision's barbwire lookup
#include "../../../Physics/Material.h"           // Material::getIsBarbwire (IMaterial slot 27)
#include "../../IGame.h"                         // game->queryInterface(IID_IGameServer)
#include "../ISpawnPoint.h"     // setProneOnSpawn - updateSoldierSpawnPoint
#include "../../../Core/System.h"                // WorldPref::getTickCount
#include "../../../Math/Mat4.h"
#include "../../../Scene/Object/ObjectManager.h"     // intersectLine / postMessage - dropKit, exitPlayer
#include "../../../Scene/Object/ObjectTypes/Game/IPlayerControlObject.h" // handleFrameUpdate's active-weapon reconciliation
#include "../../../Scene/Object/ObjectPredicator.h"  // ProjectileVsTargetPredicator - dropKit's ray
#include "../../../Sound/BfSound.h"                  // g_bfSound - stopTinnitus
#include "../../SingletonServer.h"                   // m_iNametagManager - stopManDownEffect's client arm
#include "../../Main/Game.h"                         // isDedicated() - the same arm (inline true on the server)
#include "../../../Scene/Object/WorldObjTemplPortal/PortalManager.h"   // purgePortalPassingObject (SimpleObject)
#include "../../../Scene/Object/ObjectUtil.h"    // setNetworkableComp

namespace dice {
namespace hfe {
namespace world {

// Object-hierarchy discovery helpers shared with controlled vehicles.
void findCameras(IObject* root, int pcoId, std::map<int, ICameraObject*>& out);
// written in PlayerControlObjectHelpers.cpp (0x81200E0, 485 B); declared at file scope in
// world the way that file's neighbours are, because the tree has no header for them.
void findSpawnPoints(IObject* root, std::vector<IObject*>& out);

} // namespace world


namespace world {
ICameraObject* getCommanderCameraForPlayer(const IPlayer* player);   // written in Player.cpp
void postInitCameras(IObject* root);
void findPlayerInputObjects(IObject* root, std::list<IObject*>& out, bool stopAtMatchingRoot);

// Shared services used by soldier movement, collision and presentation.
extern IBase* heightmapCluster;                  // ds:0x8ABAF24 - Core/Globals.cpp
extern IBase* materialManager;                   // ds:0x8AD2820 - Core/Globals.cpp

// Console-backed soldier tuning. Pose and speed tables capture values at static initialization.
// Swim pose rows reuse sprint variable names, so their values alias the sprint row.
namespace {

float* const g_soldierTimeToEnableWeaponVar =
    Vars::getFloat("soldier-timeToEnableWeapon", 1.25f);
float* const g_lookFactorXVar          = Vars::getFloat("phy-soldier-look-factor-x", 5.0f);
float* const g_lookFactorYVar          = Vars::getFloat("phy-soldier-look-factor-y", 5.0f);
float* const g_speedToVelocityVar     = Vars::getFloat("phy-soldier-speed-factor", 1.0f);
float* const g_jumpFactorVar          = Vars::getFloat("phy-soldier-jump-factor", 1.0f);
float* const g_jumpLengthFactorVar    = Vars::getFloat("phy-soldier-jump-length-factor", 0.98f); // raw 0x3F7AE148
float* const g_inputPressRateVar      = Vars::getFloat("phy-soldier-acceleration", 0.2f);
float* const g_inputReleaseRateVar    = Vars::getFloat("phy-soldier-deceleration", 0.2f);
// Prone crushing uses the drowning damage rate and damage type.
float* const g_skydiveAtSpeedVar      = Vars::getFloat("phy-soldier-skydiveat-speed", -10.0f);
float* const g_skydiveSpeedVar        = Vars::getFloat("phy-soldier-skydive-speed", 15.0f);
float* const g_soldierDrownDamageVar  = Vars::getFloat("soldier-drown-damage", 8.0f);
float* const g_sprintSpeedVar         = Vars::getFloat("phy-soldier-sprint-speed", 7.0f);
float* const g_runSpeedVar            = Vars::getFloat("phy-soldier-run-speed", 3.9f);
float* const g_walkSpeedVar           = Vars::getFloat("phy-soldier-walk-speed", 1.5f);
float* const g_crouchSpeedVar         = Vars::getFloat("phy-soldier-crouch-speed", 2.0f);
float* const g_crawlSpeedVar          = Vars::getFloat("phy-soldier-crawl-speed", 0.8f);
float* const g_swimSpeedVar           = Vars::getFloat("phy-soldier-swim-speed", 2.0999999f);
float* const g_swimCrawlSpeedVar      = Vars::getFloat("phy-soldier-swimcrawl-speed", 3.6450002f);  // raw 0x406947AE
float* const g_jumpSpeedFactorVar     = Vars::getFloat("phy-soldier-inair-speed", 2.0f);
float* const g_maxTurnRatePerTickVar  = Vars::getFloat("soldier-lookSideRestore", 4.0f);
float* const g_proneWaterClearanceVar = Vars::getFloat("soldier-prone-inwater-limit", 1.1f);
float* const g_fireDelayAfterJumpVar  = Vars::getFloat("fire-delay-after-jump", 1.0f);
float* const g_proneDelayAfterJumpVar = Vars::getFloat("prone-delay-after-jump", 1.0f);
float* const g_poseUpTimerAVar        = Vars::getFloat("jump-delay-after-prone", 1.0f);
float* const g_poseDownTimerVar       = Vars::getFloat("stand-delay-from-prone", 1.0f);
float* const g_poseUpTimerBVar        = Vars::getFloat("prone-delay-from-stand", 1.0f);
float* const g_sprintRechargeDelayAfterJumpVar =
    Vars::getFloat("sprint-recharge-delay-after-jump", 1.0f);
float* const g_poseUpTimerCVar        = Vars::getFloat("fire-delay-from-prone", 0.6f);
float* const g_cameraFollowProneVar   = Vars::getFloat("camera-movement-speed-on-prone", 0.5f);
float* const g_cameraFollowCrouchVar  = Vars::getFloat("camera-movement-speed-on-crouch", 0.5f);
float* const g_cameraFollowDefaultVar = Vars::getFloat("camera-movement-speed-on-stand", 0.5f);

Vec3 lookMaxAngleDefault()
{
    Vec3 v; v.x = 40.0f; v.y = 85.0f; v.z = 0.0f;    // raw 0x42200000 / 0x42AA0000 / 0
    return v;
}
Vec3* const g_lookAngleLimitsVar = Vars::getVec3("soldier-lookMaxAngle", lookMaxAngleDefault());
float* const g_soldierStandingHeightVar =
    Vars::getFloat("coll-soldier-stand-height", 1.7f);                        // raw 0x3FD9999A
float* const g_soldierRadiusVar =
    Vars::getFloat("coll-soldier-radius", 0.25f);
// handleFrameUpdate's man-down smoothing pair. Both ports register these consecutively, speed
// first: ELF pointer slots 0x8AAD8B8 / 0x8AAD8BC, PE 0x885924 / 0x88592C.
float* const g_soldierManDownSpeedVar =
    Vars::getFloat("soldier-mandown-speed", 3.0f);
float* const g_soldierManDownRotSpeedVar =
    Vars::getFloat("soldier-mandown-rotspeed", 1.4f);                       // raw 0x3FB33333

// The two value tables (.bss 0x8AAD920 / 0x8AAD9E4) and the static-init fill that assembles them.
float g_poseTransitionTableStorage[49];              // diagonal stays zero, as the .bss original
float g_soldierSpeedsStorage[7];

struct SoldierTuningTableFill
{
    SoldierTuningTableFill()
    {
        // the 42 pose vars, in the ELF's registration order (row-major, diagonal skipped) -
        // including the shipped pose-sprint-* rows 5 and 6. {name, default} per slot.
        static const struct { const char* name; float fallback; } kPose[42] = {
            {"pose-crawl-crouch",0.5f},{"pose-crawl-walk",0.5f},{"pose-crawl-run",0.5f},{"pose-crawl-sprint",0.5f},{"pose-crawl-swim",0.5f},{"pose-crawl-swimCrawl",0.5f},
            {"pose-crouch-crawl",0.5f},{"pose-crouch-walk",0.25f},{"pose-crouch-run",0.25f},{"pose-crouch-sprint",0.25f},{"pose-crouch-swim",0.5f},{"pose-crouch-swimCrawl",0.5f},
            {"pose-walk-crawl",0.5f},{"pose-walk-crouch",0.25f},{"pose-walk-run",0.25f},{"pose-walk-sprint",0.25f},{"pose-walk-swim",0.5f},{"pose-walk-swimCrawl",0.5f},
            {"pose-run-crawl",0.5f},{"pose-run-crouch",0.25f},{"pose-run-walk",0.25f},{"pose-run-sprint",0.25f},{"pose-run-swim",0.5f},{"pose-run-swimCrawl",0.5f},
            {"pose-sprint-crawl",0.5f},{"pose-sprint-crouch",0.25f},{"pose-sprint-walk",0.25f},{"pose-sprint-run",0.25f},{"pose-sprint-swim",0.5f},{"pose-sprint-swimCrawl",0.5f},
            // rows 5 and 6 - the copy-pasted sprint names, second/third defaults ignored by Vars
            {"pose-sprint-crawl",0.5f},{"pose-sprint-crouch",0.5f},{"pose-sprint-walk",0.5f},{"pose-sprint-run",0.5f},{"pose-sprint-swim",0.5f},{"pose-sprint-swimCrawl",0.5f},
            {"pose-sprint-crawl",0.5f},{"pose-sprint-crouch",0.5f},{"pose-sprint-walk",0.5f},{"pose-sprint-run",0.5f},{"pose-sprint-swim",0.5f},{"pose-sprint-swimCrawl",0.5f},
        };
        int var = 0;
        for (int row = 0; row < 7; ++row)
            for (int col = 0; col < 7; ++col)
            {
                if (row == col)
                    continue;                        // the diagonal stays zero
                g_poseTransitionTableStorage[7 * row + col] =
                    *Vars::getFloat(kPose[var].name, kPose[var].fallback);
                ++var;
            }

        // the speed table, index order crawl(0)..swimCrawl(6) - the copies at 0x8AAD9E4..
        g_soldierSpeedsStorage[0] = *g_crawlSpeedVar;
        g_soldierSpeedsStorage[1] = *g_crouchSpeedVar;
        g_soldierSpeedsStorage[2] = *g_walkSpeedVar;
        g_soldierSpeedsStorage[3] = *g_runSpeedVar;
        g_soldierSpeedsStorage[4] = *g_sprintSpeedVar;
        g_soldierSpeedsStorage[5] = *g_swimSpeedVar;
        g_soldierSpeedsStorage[6] = *g_swimCrawlSpeedVar;
    }
} g_soldierTuningTableFill;

} // anonymous namespace

// The reader-facing aliases - every movement body below keeps its original spelling, and every
// read goes through the live var, exactly as the ELF's pointer-deref sites do.
const float& g_proneWaterClearance = *g_proneWaterClearanceVar;   // ds:0x8AAD8D0
const float& g_poseUpTimerA        = *g_poseUpTimerAVar;          // ds:0x8AAD8E0
const float& g_poseDownTimer       = *g_poseDownTimerVar;         // ds:0x8AAD8E4
const float& g_poseUpTimerB        = *g_poseUpTimerBVar;          // ds:0x8AAD8E8
const float& g_poseUpTimerC        = *g_poseUpTimerCVar;          // ds:0x8AAD8F0
const float& g_inputPressRate      = *g_inputPressRateVar;        // ds:0x8AAD7C4
const float& g_inputReleaseRate    = *g_inputReleaseRateVar;      // ds:0x8AAD7C8
const float& g_soldierTimeToEnableWeapon =
    *g_soldierTimeToEnableWeaponVar;                               // ds:0x8AAD7AC
const float& g_speedToVelocity     = *g_speedToVelocityVar;       // ds:0x8AAD7B8
const float& g_jumpFactor          = *g_jumpFactorVar;            // ds:0x8AAD7BC
const float& g_jumpLengthFactor    = *g_jumpLengthFactorVar;      // ds:0x8AAD7C0
const float& g_skydiveSpeed        = *g_skydiveSpeedVar;          // ds:0x8AAD7D0
const float& g_lookFactorX         = *g_lookFactorXVar;            // ds:0x8AAD7B0
const float& g_lookFactorY         = *g_lookFactorYVar;            // ds:0x8AAD7B4
const float& g_jumpSpeedFactor     = *g_jumpSpeedFactorVar;       // ds:0x8AAD7F0
const float& g_fireDelayAfterJump  = *g_fireDelayAfterJumpVar;    // ds:0x8AAD8D8
const float& g_proneDelayAfterJump = *g_proneDelayAfterJumpVar;   // ds:0x8AAD8DC
const float& g_sprintRechargeDelayAfterJump =
    *g_sprintRechargeDelayAfterJumpVar;                            // ds:0x8AAD8EC
// A pair, not a scalar: [0] is the yaw limit and [1] the pitch limit (Vec3 x/y; z unused).
const float* const g_lookAngleLimits = &g_lookAngleLimitsVar->x;  // ds:0x8AAD8AC
const float& g_soldierStandingHeight = *g_soldierStandingHeightVar; // ds:0x8AAD8B0
// Keep this console-variable alias local; Object.cpp defines a separate global with the same name.
static const float& g_soldierRadius  = *g_soldierRadiusVar;         // ds:0x8AAD8B4
const float& g_cameraFollowProne   = *g_cameraFollowProneVar;     // ds:0x8AAD8F4
const float& g_cameraFollowCrouch  = *g_cameraFollowCrouchVar;    // ds:0x8AAD8F8
const float& g_cameraFollowDefault = *g_cameraFollowDefaultVar;   // ds:0x8AAD8FC
const float& g_maxTurnRatePerTick  = *g_maxTurnRatePerTickVar;    // ds:0x8AAD8A8
const float* const g_soldierSpeeds        = g_soldierSpeedsStorage;        // ds:0x8AAD9E4
const float* const g_poseTransitionTable  = g_poseTransitionTableStorage;  // ds:0x8AAD920

namespace {

// Typed weapon recoil accessors; the enable predicate gates reads of both values.
inline bool  weaponHasRecoil(void* w)      { return static_cast<IWeaponObject*>(w)->getHasRecoilForce(); }
inline float weaponGetPitchRecoil(void* w) { return static_cast<IWeaponObject*>(w)->getPitchRecoil(); }
inline float weaponGetYawRecoil(void* w)   { return static_cast<IWeaponObject*>(w)->getYawRecoil(); }
inline bool  weaponIsZoomed(void* w)       { return static_cast<IWeaponObject*>(w)->getZoom() != 0; }
inline float weaponSpeedMod(void* w)       { return static_cast<IWeaponObject*>(w)->getMouseSpeedMod(); }

// The two raw hooks that used to live here are retired. IItemObject::enableItem/disableItem and
// IAnimationSystem::clearTriggerMessage are recovered typed interfaces. This matters across ports:
// IAnimationSystem's clear slot is ELF +0x38 but PE +0x34 because Itanium has two destructor slots.

// Camera transformation retains the second yaw-limit query even though its result is unused.

// Retail +0x44/+0x48 are the inherited physics/response fields. Use their recovered names so
// these same reads follow the native pointer layout as well as the shipped x86 layout.
inline PointPhysicsNode* soldierPhysicsNode(IObject* o)
{
    return static_cast<PointPhysicsNode*>(static_cast<IPhysicsNode*>(
        object_detail::LayoutAccess::physicsNode(o)));
}
inline SoldierResponsePhysics* soldierResponsePhysics(IObject* o)
{
    return static_cast<SoldierResponsePhysics*>(static_cast<IResponsePhysics*>(
        object_detail::LayoutAccess::responsePhysics(o)));
}

// Clamps to [-1, 1], applies a sign-preserving square, then a 0.001 output deadzone.
inline float conditionAxis(float v)
{
    if (v < -1.0f)      v = -1.0f;
    else if (v > 1.0f)  v =  1.0f;

    const float sign = (v > 0.0f) ? 1.0f : ((v < 0.0f) ? -1.0f : 0.0f);
    float out = sign * (v * v);
    if (fabsf(out) < 0.001f)
        out = 0.0f;
    return out;
}

// PIAction and PILie take only the final 0.001 deadzone. Retail does not run either through the
// movement axes' clamp/square response curve (ELF 0x81A8C43..0x81A8D04).
inline float deadzoneAction(float v)
{
    return (fabsf(v) < 0.001f) ? 0.0f : v;
}

// An input is only read when the control map says it is bound - the 64-bit mappingFlags word, bit
// per index. An unbound axis reads as a hard zero rather than as whatever is in the array.
inline float mappedInput(const io::PlayerInput& in, int index)
{
    return ((in.mappingFlags >> index) & 1ull) ? in.inputs[index] : 0.0f;
}

// A movement axis is one ROW of the movement matrix flattened into XZ and normalised. The ELF's
// normalise has the same two epsilons everywhere in this class: already-unit is left alone, and a
// degenerate vector is zeroed rather than divided.
inline Vec3 normalisedXZ(float x, float z)
{
    Vec3 v; v.x = x; v.y = 0.0f; v.z = z;
    const float lenSq = x * x + z * z;
    if (fabsf(lenSq - 1.0f) >= 1.1920929e-7f)
    {
        if (fabsf(lenSq) >= 1.1920929e-7f)
        {
            const float inv = 1.0f / sqrtf(lenSq);
            v.x *= inv; v.z *= inv;
        }
        else { v.x = 0.0f; v.z = 0.0f; }
    }
    return v;
}


// SprintState - embedded at +0x300 and unrecovered. It owns the stamina meter and writes +0x30C.
void  sprintStateSetStamina(SprintState* state, float stamina);

// Clears all skeleton inverse-kinematics goals.
void  skeletonClearAllIk(anim::Skeleton* skeleton) { skeleton->clearAllIk(); }


// The five typed IWeaponObject reads used by calcAmmoStatus. Keeping the source-level calls here
// avoids the ELF-vtable-offset/MSVC-vtable-offset mismatch that the former raw hooks introduced.
inline bool weaponObjectCanBeRefilled(IWeaponObject* w)         { return w->canBeRefilled(); }
inline int  weaponObjectGetAmmoInMag(IWeaponObject* w, int mag) { return w->getAmmoInMag(static_cast<unsigned int>(mag)); }
inline int  weaponObjectGetMaxNrOfMags(IWeaponObject* w)        { return w->getMaxNrOfMags(); }
inline int  weaponObjectGetCurrentNrOfMags(IWeaponObject* w)    { return w->getCurrentNrOfMags(); }
inline int  weaponObjectGetMagSize(IWeaponObject* w)            { return w->getMagSize(); }


// ELF 81AB798 / PE 553021: the queried camera's +18 takes (mode, IObject*, bool).
// Keep the native interface receiver and discard the shipped bool result.
void cameraObjectSetViewMode(ICameraObject* camera, CameraViewMode mode, IObject* object, bool reset)
{
    camera->setViewMode(mode, object, reset);
}

// GenericFireArm's getFireInput delegates to Fire and has the shipped PINone null default. The
// typed interface call keeps this portable across the ELF and MSVC vtable layouts.
inline io::PlayerInputMap weaponObjectGetFireInput(IWeaponObject* w) { return w->getFireInput(); }

// Creates an animation-system instance from a registered template.
anim::IAnimationSystem* animationSystemCreateInstance(void* animationSystemTemplate) // slot +0x14
{
    return static_cast<anim::AnimationSystemTemplate*>(animationSystemTemplate)->makeInstance();
}

// Forwards field of view to the renderer-owned geometry.
// Uses the PE calling convention and slot because the full geometry interface is not declared here.
void geometrySlot10(void* geometry, float fov)
{
    typedef void(__thiscall* SetFovFn)(void*, float);
    reinterpret_cast<SetFovFn>(
        (*reinterpret_cast<void***>(geometry))[0x10 / 4])(geometry, fov);
}

void geometrySlotD8(void* geometry, bool flag)
{
    typedef void(__thiscall* Fn)(void*, unsigned int);
    reinterpret_cast<Fn>(
        (*reinterpret_cast<void***>(geometry))[0xD4 / 4])(
            geometry, flag ? 1u : 0u);
}

// Registers animation templates by name; file loading is deferred to the core event.
void* animationSystemManagerGet(const std::string& name)
{
    return anim::animationSystemTemplateManager->load(name);
}
void  animationSystemManagerRelease(void* systemTemplate)
{
    anim::animationSystemTemplateManager->release(
        static_cast<anim::AnimationSystemTemplate*>(systemTemplate));
}

// Supplies a borrowed bone-transform palette to the kit geometry through its renderer interface.
void geometrySetBoneTransforms(void* geometry, const Mat4* transforms, int boneCount)  // PE +0x54
{
    typedef void(__thiscall* SetBonesFn)(void*, const Mat4*, unsigned int);
    reinterpret_cast<SetBonesFn>(
        (*reinterpret_cast<void***>(geometry))[0x54 / 4])(
            geometry, transforms, static_cast<unsigned int>(boneCount));
}

void geometrySlot7C(void* geometry, int a)                                             // PE +0x78
{
    typedef void*(__thiscall* SetLodFn)(void*, int);
    reinterpret_cast<SetLodFn>(
        (*reinterpret_cast<void***>(geometry))[0x78 / 4])(geometry, a);
}

// AI events are available only when a published AI instance reports a bots game.
inline bool aiConnectionIsUp()
{
    return ai::g_aiInstance != 0 && ai::aiInstance()->isBotsGame();
}

// The ECAI 1/2 (entered/exited vehicle) payload this body posts for itself: retail stores five
// dwords - this soldier's AI object (Object +0x34, IObject::m_aiObject), a null second AI object,
// team, id and a zero trailing word. PlayerControlObject's AiEnterNote/AiExitNote give the same
// events this shape; the pointer members keep it whole on native x64. The x86 layout is retail's.
struct SoldierAiSeatPayload
{
    void* aiObject;         // +0x00 this soldier's AI object
    void* otherAiObject;    // +0x04 always null here
    int   team;             // +0x08
    int   id;               // +0x0C
    int   trailing;         // +0x10 the trailing flag byte, stored as a whole dword
};
// The ECAI 6/7 (man down / revived) payload: this soldier's AI object, then the player id.
struct SoldierAiPlayerPayload
{
    void* aiObject;         // +0x00 Object +0x34
    int   playerId;         // +0x04
};
#if defined(_M_IX86)
static_assert(sizeof(SoldierAiSeatPayload) == 20, "retail five-dword ECAI 1/2 payload");
static_assert(sizeof(SoldierAiPlayerPayload) == 8, "retail two-dword ECAI 6/7 payload");
#endif

// Retail Object +0x3C, read directly by handleMessage and enter. LayoutAccess names the same
// cached armor component without depending on the shipped pointer width.
inline ArmorGLComp* objectArmorComp(IObject* object)
{
    return static_cast<ArmorGLComp*>(object_detail::LayoutAccess::armorComponent(object));
}

// ELF 81ACFFE / PE 54F57D: IGame +D4/+D0 is the typed specialist radio call.
void gameCallForSpecialist(IPlayer* player, HudNodeTypes specialist)
{
    game->callForSpecialist(player, specialist);
}

// The idle-time source at ds:0x8AADA2C is outside Soldier.cpp's recovered static-initializer block
// and remains unresolved. The three collision/weapon globals that used to stand beside it as zero
// placeholders are now bound above to their exact Vars names and defaults from both shipped ports.
float g_soldierLookIdleTimeSource  = 0.0f;   // ds:0x8AADA2C

} // anonymous namespace

// Initializes soldier state, item slots and the player-control map, including this soldier at id 0.
Soldier::Soldier(const SoldierTemplate* objectTemplate)
    : Bundle(reinterpret_cast<const BundleTemplate*>(objectTemplate))
    , m_refCount(1)
    , m_soldierTemplate(objectTemplate)
    , m_occupyingPlayerWeak(0)
    , m_weapon1(0)
    , m_weapon2(0)
    , m_isOccupied(false)
    , m_team(0)
    , m_activeItemIndex(0)
    , m_kit(0)
    , m_kitBeingPickedUp(0)
    , m_usedVehicleWeak(0)
    , m_refBlock1D8(0)
    , m_hitByVehicleSpeedTicks(0)
    , m_dword1EC(0)
    , m_facingDegrees(0.0f)
    , m_smoothedStrafe(0.0f)
    , m_smoothedForward(0.0f)
    , m_pose(0)
    , m_requestedPose(0)
    , m_lookUpDownAngle(0.0f)
    , m_cameraPitch(0.0f)
    , m_lookLeftRightAngle(0.0f)
    , m_cameraYaw(0.0f)
    , m_float214(0)
    , m_poseInputLatch(0)
    , m_voiceSound(0)
    , m_ragDoll(0)
    , m_dword224(0)
    , m_timeToEnableWeapon(0.0f)
    , m_helmetTime(0.0f)
    , m_float230(0.0f)
    , m_soldierLookIdleTime(0.0f)
    , m_explosionForceTime(0.0f)
    , m_animSkeleton(0)
    , m_animationSystem1P(0)
    , m_collisionSkeleton(0)
    , m_float274(0.0f)
    , m_float278(0.0f)
    , m_byte27C(false)
    , m_byte27D(false)
    , m_inverseKinematicDisabled(false)
    , m_characterMode(0)
    , m_isSwimming(false)
    , m_soldierSpeedIndex(0)
    , m_poseChangeProgress(0.0f)
    , m_poseChangeDuration(0.0f)
    , m_soldierSpeed(0.0f)
    , m_isManDown(0)
    , m_soldierSound(0)
    , m_isSkyDiving(false)
    , m_lastContactMaterial(0)
    , m_medicCalled(0)
    , m_float2C8(0.0f)
    , m_currentAbility(0.0f)
    , m_dword2D0(0)
    , m_dword2D4(0)
    , m_dword2D8(0)
    , m_dword2DC(0)
    , m_dword2E0(0)
    , m_ladderId(0)
    , m_ziplineId(0)
    , m_grapplingHookId(0)
    , m_readyToClimbTime(0.0f)
    , m_byte315(false)
    , m_float318(0.0f)
    , m_float324(0.0f)
    , m_float330(0.0f)
    , m_byte37C(false)
    , m_deployAnimationActive(false)
    , m_deployAnimationTimer(0.0f)
    , m_byte384(false)
    , m_byte385(false)
    , m_dword388(0)
    , m_weaponStowed(false)
    , m_byte38D(false)
    , m_ammoStatus(0)
    , m_inputSuppressTimer(0.0f)
    , m_byte398(false)
    , m_byte399(false)
    , m_fireDelayAfterJump(0.0f)
    , m_float3B4(0.0f)
{
    m_activeWeapons[0] = 0;
    m_activeWeapons[1] = 0;
    m_activeWeapons[2] = 0;

    m_hitByVehicleSpeed.x = 0.0f;
    m_hitByVehicleSpeed.y = 0.0f;
    m_hitByVehicleSpeed.z = 0.0f;

    m_headPosition.x = 0.0f;
    m_headPosition.y = 0.0f;
    m_headPosition.z = 0.0f;

    m_explosionForce.x = 0.0f;
    m_explosionForce.y = 0.0f;
    m_explosionForce.z = 0.0f;

    m_animationSystem3P = 0;
    m_ikObject = 0;
    m_ikTemplateComponent = 0;
    m_dword26C = 0;
    m_dword270 = 0;
    for (int i = 0; i < 5; ++i)  m_byte27F[i] = false;
    m_cameraSpringDisplacement.x = 0.0f;
    m_cameraSpringDisplacement.y = 0.0f;
    m_cameraSpringDisplacement.z = 0.0f;
    m_yawDeltaThisTick = 0.0f;
    m_pitchDeltaThisTick = 0.0f;
    m_dword29C = 0;
    for (int i = 0; i < 2; ++i)  m_float31C[i] = 0.0f;
    for (int i = 0; i < 2; ++i)  m_float328[i] = 0.0f;
    for (int i = 0; i < 2; ++i)  m_float334[i] = 0.0f;
    for (int i = 0; i < 5; ++i)  m_poseTimers[i] = 0.0f;

    // this is ONE Mat4 set to the identity, not four blocks. The stores
    // land at +0x33C, +0x350, +0x364 and +0x378 - 0, 20, 40 and 60 bytes apart, a matrix diagonal.
    for (int i = 0; i < 16; ++i)
        m_mat33C.m[i] = 0.0f;
    m_mat33C.m[0]  = 1.0f;
    m_mat33C.m[5]  = 1.0f;
    m_mat33C.m[10] = 1.0f;
    m_mat33C.m[15] = 1.0f;


    // A soldier IS IN ITS OWN PCO MAP, at key 0.
    m_playerControlObjects.insert(std::make_pair(
        0, static_cast<IPlayerControlObject*>(
               const_cast<Soldier*>(this)->queryInterface(IID_IPlayerControlObject))));

    Object::updateFlags(0x3800u, 0u);            // three bits, [UNRESOLVED] which

    // The item list, sized from the template and blanked. Slot 0 is never handed out - getItem's
    // guard is `index > 0` - so the first usable index is 1.
    m_itemObjects.assign(static_cast<size_t>(m_soldierTemplate->getInventorySize()),
                         static_cast<IObject*>(0));

    // Dedicated sound creation returns no audible resources.
    m_soldierSound = new SoldierSound(this);      // operator new(0x38) + its ctor

    initAnimation();                             // [FRONTIER]
    resetInstance();                             // [FRONTIER]
}

// . [FRONTIER] - it unwinds the animation systems, the sound voice, the item
// vector and the two maps, and every one of those is unwritten here. Declared so the class is
// destructible in the type system; a real delete faults by name.

// Soldier owns the reference count used by its IBase interface.
void Soldier::addRef()
{
    ++m_refCount;
}

int Soldier::getRef()
{
    return m_refCount;
}

// . It decrements first and tests the PRE-decrement value against 1, so the
// delete happens on the transition to zero. The `&& this` in the ELF is a null check on `this`
// that can never fire - reproduced as the plain form.
int Soldier::release()
{
    const int remaining = m_refCount - 1;
    const bool wasLast = (m_refCount == 1);
    m_refCount = remaining;
    if (wasLast)
        delete this;
    return remaining;
}

// Exposes object, soldier, console, item-container, and controlled-object interfaces.
void* Soldier::queryInterface(unsigned int classId) const
{
    Soldier* self = const_cast<Soldier*>(this);
    if (classId == IID_IBase || classId == IID_IObject)
        return static_cast<IObject*>(self);
    if (classId == IID_ISoldier)
        return static_cast<ISoldier*>(self);
    if (classId == io::IID_IConsoleSaveable)
        return static_cast<io::IConsoleSaveable*>(self);
    if (classId == IID_IItemContainerObject)
        return static_cast<IItemContainerObject*>(self);
    if (classId == IID_IPlayerControlObject)
        return static_cast<IPlayerControlObject*>(self);
    return 0;
}

// Initializes soldier components with AI initialization locked, then unlocks and initializes AI.
// Only a parented soldier runs the hierarchy-wide camera initialization pass.
void Soldier::init()
{
    SimpleObject::aiObjectCreate();

    hfe::ai::IAIObject* const aiObject = static_cast<hfe::ai::IAIObject*>(
        object_detail::LayoutAccess::aiObject(this));                   // Object +0x34
    if (aiObject)
        aiObject->lockInit();

    Bundle::init();

    findCameras(this, getPcoId(), m_ownCameras);
    findPlayerInputObjects(this, m_playerInputs, true);

    if (getParent())
        postInitCameras(this);

    gameLogic->addPco(this);                                            // IGameLogic +0x194
    m_medicCalled = 0;

    if (aiObject)
    {
        aiObject->unlockInit();
        aiObject->init();
    }
}

// . Blank the instance, then let SimpleObject tear the object down.
void Soldier::destroy(bool force)
{
    resetInstance();                             // [FRONTIER]
    SimpleObject::destroy(force);
}

// Parks physics throughout the subtree and clears soldier movement input.
// Adds response bodies to collision group 3 and clears their surface velocity. [INFERRED] Source placement.
void disablePhysicsOnSoldier(IObject* object)
{
    typedef object_detail::LayoutAccess L;

    if (L::objectTemplate(object)->getClassID() == CID_SoldierTemplate)
    {
        Soldier* const soldier = static_cast<Soldier*>(object);
        soldier->m_smoothedForward = 0.0f;           // +0x1F8
        soldier->m_smoothedStrafe  = 0.0f;           // +0x1F4
    }
    IPhysicsNode* const node = static_cast<IPhysicsNode*>(L::physicsNode(object));   // +0x44
    if (node)
        node->setHasSeparatePhysicsUpdate(true);     // +0x100
    IResponsePhysics* const response = static_cast<IResponsePhysics*>(
        object->queryComponent(IID_IResponsePhysics, IID_IResponsePhysics));
    if (response)
    {
        response->addToCollisionGroup(3);            // +0x4C
        Vec3 zero; zero.x = 0.0f; zero.y = 0.0f; zero.z = 0.0f;
        response->setSurfacePositionalSpeed(zero);   // +0x88
    }
    for (IObject* child = const_cast<IObject*>(object->getChild());
         child; child = child->getSibling())
        disablePhysicsOnSoldier(child);
}

void reenablePhysicsOnSoldier(IObject* object)
{
    typedef object_detail::LayoutAccess L;

    IPhysicsNode* const node = static_cast<IPhysicsNode*>(L::physicsNode(object));   // +0x44
    if (node)
        node->setHasSeparatePhysicsUpdate(false);    // +0x100
    IResponsePhysics* const response = static_cast<IResponsePhysics*>(
        object->queryComponent(IID_IResponsePhysics, IID_IResponsePhysics));
    if (response)
    {
        response->removeFromCollisionGroup(3);       // +0x50
        Vec3 zero; zero.x = 0.0f; zero.y = 0.0f; zero.z = 0.0f;
        response->setSurfacePositionalSpeed(zero);   // +0x88
    }
    for (IObject* child = const_cast<IObject*>(object->getChild());
         child; child = child->getSibling())
        reenablePhysicsOnSoldier(child);
}

// Aborts if still in a vehicle, then drops the kit and aborts if it remains before caching.
void Soldier::disableCachedObject()
{
    if (getUsedVehicle())
        abort();
    dropKit();
    if (m_kit)
        abort();

    resetInstance();                             // [FRONTIER]
    reenablePhysicsOnSoldier(this);              // ghost - Physics side, unrecovered
    SimpleObject::disableCachedObject();
}

// Recreates AI state, re-enables the object, registers its PCO, and restores full stamina.
void Soldier::enableCachedObject()
{
    SimpleObject::aiObjectCreate();
    hfe::ai::IAIObject* const aiObject =
        static_cast<hfe::ai::IAIObject*>(object_detail::LayoutAccess::aiObject(this));

    Bundle::enableCachedObject();

    if (aiObject)
        aiObject->init();

    gameLogic->addPco(this);
    setSprintStamina(1.0f);
}

// . A straight forward to Object's, and it exists only so the slot carries Soldier's
// symbol - there is no behaviour of its own.
void Soldier::removeChild(IObject* child)
{
    Object::removeChild(child);
}

// Requires nonempty networkable info; aborts otherwise, then rechecks before attaching replication.
void Soldier::initNetworkableComp()
{
    const IObjectTemplate* t = getTemplate();
    if (!t)
        return;

    if (t->getNetworkableInfo().empty())
        abort();

    if (!t->getNetworkableInfo().empty())
        setNetworkableComp(this, CID_SoldierNetworkable);
}


// Releases the occupant, updates ragdoll and AI state, stops tinnitus and clears portal tracking.
void Soldier::exit()
{
    m_isOccupied = false;                                                    // +0x1B0
    if (m_ragDoll)                                                           // +0x220
    {
        m_ragDoll->setIsClient(false);
        m_ragDoll->m_exited = true;                                        // byte +0x5C = 1
    }
    stopTinnitus();

    if (aiConnectionIsUp() && getOccupingPlayer())
    {
        SoldierAiSeatPayload payload;
        payload.aiObject = object_detail::LayoutAccess::aiObject(this);      // +0x34
        payload.otherAiObject = 0;
        payload.team = getOccupingPlayer()->getTeam();                       // +0xE8
        payload.id = getOccupingPlayer()->getId();                           // +0x54
        payload.trailing = 0;                                                // the trailing byte
        eventManager->postEvent(ECAI, 2, &payload, 0.0f);                   // +0x14
    }

    if (getOccupingPlayer())
        getOccupingPlayer()->setVehicle(0, -1);                              // +0x4C

    setOccupyingPlayerWeak(0);                                               // release + null
    purgePortalPassingObject();
}

// Drops the kit, destroys soldier armor and releases the player from this body.
// Clears man-down state and portal tracking; requires an armor component.
void Soldier::exitPlayer()
{
    typedef object_detail::LayoutAccess L;

    Kit* const kit = m_kit;                                                  // +0x1C0 captured
    const bool dropped = dropKit();
    if (kit && (L::parent(this) || !dropped))                                // +0x28
    {
        if (gameLogic->isServer())                                           // +0x13C
            objectManager->postMessage(kit, static_cast<TemplateMessage>(29), 0.0f);   // +0xD0
        kit->updateFlags(0u, 2u);                                            // +0x60
    }

    m_isOccupied = false;                                                    // +0x1B0
    if (m_ragDoll)                                                           // +0x220
    {
        m_ragDoll->setIsClient(false);
        m_ragDoll->m_exited = true;
    }
    stopTinnitus();

    static_cast<IArmorGLComp*>(getArmorComponent())->fullDestroy();          // +0x3C > +0x100, unchecked
    sendMessageToItem(m_activeItemIndex, static_cast<TemplateMessage>(24), getOccupingPlayer(), false);
    setCharacterMode(1);

    if (getOccupingPlayer())
    {
        getOccupingPlayer()->setVehicle(0, -1);                              // +0x4C
        getOccupingPlayer()->setIsManDown(false);                            // +0xFC
    }

    setOccupyingPlayerWeak(0);                                               // release + null
    purgePortalPassingObject();
}

// Removes vehicle kit bonuses and IK attachments after leaving a vehicle, then refreshes weapon state.
void Soldier::onExit()
{
    typedef object_detail::LayoutAccess L;

    if (m_collisionSkeleton)                                                 // +0x25C
        m_collisionSkeleton->clearAllIk();

    if (m_kit)                                                               // +0x1C0
    {
        if (IObject* const used = getUsedVehicle())                          // +0x1D4 block -> +4
        {
            IObject* const root = getRootParent(used);
            IPlayerControlObject* pco = 0;
            for (IObject* o = root; o; o = L::parent(o))
            {
                pco = static_cast<IPlayerControlObject*>(
                    o->queryInterface(IID_IPlayerControlObject));            // +0x0C
                if (pco)
                    break;
            }
            const std::string& rootName = L::objectTemplate(root)->getName();   // +0x14 > +0x2C, root unchecked
            if (pco)
            {
                std::vector<std::pair<std::string, VehicleMod> >* const mods = m_kit->getKitVehicleMods();
                for (unsigned int i = 0; i < mods->size(); ++i)
                {
                    if ((*mods)[i].first == rootName)
                        pco->removeVehicleMod((*mods)[i].second);            // +0xE0
                }
            }
        }
    }

    if (m_usedVehicleWeak)                                                   // +0x1D4
    {
        int* const count = static_cast<int*>(m_usedVehicleWeak);
        if ((*count)-- == 1)
            operator delete(m_usedVehicleWeak);
    }
    m_usedVehicleWeak = 0;

    m_ikObject = 0;                                                          // +0x264
    m_ikTemplateComponent = 0;                                               // +0x268
    m_inverseKinematicDisabled = false;                                      // +0x27E
    m_lookLeftRightAngle = 0.0f;                                             // +0x20C
    m_byte38D = false;                                                       // +0x38D
    updateWeaponIsActiveState();
}

// Stops tinnitus for the local human occupant and posts the updated tinnitus setup event.
void Soldier::stopTinnitus()
{
    if (getOccupingPlayer() == playerManager->getLocalHumanPlayer())         // +0x180 +0x30
    {
        g_bfSound->stopTinnitus();                                           // +0x7C
        eventManager->postEvent(ECGameLogic, 13, g_bfSound->getTinnitusSetup(), 0.0f);   // +0x74 > +0x14
    }
}

// resetWeaponAnimations - ELF 0x81B6400, 99 B. The active item, if any, gets hackSetFov(-1, once)
// and - when it carries a geometry (+0x38) - that geometry's bone transforms cleared (slot +0x58
// with (0, 0)).
void Soldier::resetWeaponAnimations()
{
    typedef object_detail::LayoutAccess L;

    IObject* const item = getItem(m_activeItemIndex);                        // +0x25C
    if (!item)
        return;
    hackSetFov(item, -1.0f, true);
    if (void* const geometry = L::geometry(item))                            // +0x38
        geometrySetBoneTransforms(geometry, 0, 0);                           // slot +0x58
}

// Sets field of view on geometry throughout the object subtree.
// When once is false, also processes the root object's siblings.
void hackSetFov(IObject* object, float fov, bool once)
{
    typedef object_detail::LayoutAccess L;

    while (object)
    {
        if (void* const geometry = L::geometry(object))                      // +0x38
        {
            geometrySlot10(geometry, fov);                                  // slot +0x10
            geometrySlotD8(L::geometry(object), fov > -1.0f);                // slot +0xD8; reread receiver
        }
        hackSetFov(object->getChild(), fov, false);                          // +0xF0
        if (once)
            break;
        once = false;
        object = L::sibling(object);                                         // +0x2C
    }
}

// THE accessors. Everything below is read end to end from the ELF; the surprises are marked.

// . A weak block, dropped the way Player::getFreeCamera drops its own: `block ? block[+4] : 0`
// with no staleness test.
IPlayer* Soldier::getOccupingPlayer() const
{
    const WeakPtr<IPlayer>::Ctrl* block = static_cast<const WeakPtr<IPlayer>::Ctrl*>(m_occupyingPlayerWeak);
    if (!block)
        return 0;
    return block->target;
}

bool Soldier::isOccupied() const                 { return m_isOccupied; }

// Soldier PCO ID is always zero; attempts to change it are ignored.
int  Soldier::getPcoId() const                   { return 0; }
void Soldier::setPcoId(int)                      { }

// getCameras and getOwnCameras hand OUT THE same MAP. A soldier owns every camera it can see.
std::map<int, ICameraObject*>* Soldier::getCameras()      { return &m_ownCameras; }
std::map<int, ICameraObject*>* Soldier::getOwnCameras()   { return &m_ownCameras; }

void Soldier::nextCamera()                       { }
void Soldier::setActiveCamera(int)               { }

// Returns the first soldier camera. Requires a nonempty camera map populated during initialization.
ICameraObject* Soldier::getSoldierCamera()
{
    return m_ownCameras.begin()->second;
}

// Prefers the local commander's camera over the soldier camera.
ICameraObject* Soldier::getActiveCamera()
{
    ICameraObject* commander = getCommanderCameraForPlayer(getOccupingPlayer());
    if (commander)
        return commander;
    return getSoldierCamera();
}

std::map<int, IPlayerControlObject*>* Soldier::getPcos()  { return &m_playerControlObjects; }

// (118 B) / PE 0x552430 (68 B): one shared function-static vector,
// constructed with count zero and value zero, with a process-exit destructor. An empty
// list permits any seat present in the root PCO map.
const std::vector<int>* Soldier::getPcoPosId()
{
    static std::vector<int> dummy(0, 0);
    return &dummy;
}

void* Soldier::getWeapons()                      { return &m_weaponVec; }
void* Soldier::getWeapons(int index)             { return &m_weapons[index]; }

// . NO bounds check on an array of three.
IObject* Soldier::getActiveWeapon(int index) const
{
    return m_activeWeapons[index];
}

// . The same weak-block drop as getOccupingPlayer.
IObject* Soldier::getUsedVehicle() const
{
    const WeakPtr<IObject>::Ctrl* block = static_cast<const WeakPtr<IObject>::Ctrl*>(m_usedVehicleWeak);
    if (!block)
        return 0;
    return block->target;
}

int  Soldier::getActiveItemIndex()               { return m_activeItemIndex; }
void Soldier::toggleOnWeaponLock(IObject*)       { }

// Infantry has no vehicle sub-position, sonar position, or artillery position.
int  Soldier::getSubPos()                        { return 0; }
int  Soldier::getSonarPos()                      { return 0; }
int  Soldier::getArtPos()                        { return 0; }

void Soldier::setTeam(int team)                  { m_team = team; }
int  Soldier::getTeam()                          { return m_team; }
void Soldier::setDefaultTeam(int)                { }

// (116 B) / PE 0x5520E0 (167 B). Pointer equality suppresses duplicates;
// otherwise append, including null. No callbacks or reference ownership changes occur.
// PE implements this directly on the ipco face (+0x154), whose +0x90 is the native list.
void Soldier::addPlayerInputObject(IObject* object)
{
    for (std::list<IObject*>::const_iterator it = m_playerInputs.begin();
         it != m_playerInputs.end(); ++it)
    {
        if (*it == object)
            return;
    }
    m_playerInputs.insert(m_playerInputs.end(), object);
}

// (121 B) / PE 0x5503D0 (81 B): erase only the first matching node.
void Soldier::removePlayerInputObject(IObject* object)
{
    for (std::list<IObject*>::iterator it = m_playerInputs.begin();
         it != m_playerInputs.end(); ++it)
    {
        if (*it == object)
        {
            m_playerInputs.erase(it);
            return;
        }
    }
}
void Soldier::setTimeToLiveUnused(float)         { }

// Soldier does not support object-spawner association or holding.
void Soldier::setObjectSpawnerId(int)            { }
int  Soldier::getObjectSpawnerId() const         { return -1; }
void Soldier::setObjectSpawnerHolding(bool)      { }
bool Soldier::getObjectSpawnerHolding() const    { return false; }

// The three "how do I ride / where is my name tag" answers all come from the template, so they are
// per-soldier-type content rather than per-instance state.
const Vec3& Soldier::getNameTagOffset()          { return m_soldierTemplate->getNameTagOffset(); }
const Vec3& Soldier::getRidingWithOffset()       { return m_soldierTemplate->getRidingWithOffset(); }
float Soldier::getRidingWithDistMod()            { return m_soldierTemplate->getRidingWithDistMod(); }
const std::string& Soldier::getSoundFilter() const { return m_soldierTemplate->getSoundFilter(); }
int  Soldier::getItemContainerType() const       { return m_soldierTemplate->getItemContainerType(); }

// THE vehicle-MOD trio IS inert TOO - add and remove are empty and get is a constant 0. Kits
// register vehicle mods (KitTemplate::addKitVehicleMod); the soldier itself carries none.
void Soldier::addVehicleMod(VehicleMod)          { }
void Soldier::removeVehicleMod(VehicleMod)       { }
int  Soldier::getVehicleMod(VehicleMod)          { return 0; }

// constants, not state: a soldier's camera never shakes by more than the default and its sprint
// factor is reported as zero however fast it is running. Both are real fields on vehicles.
float Soldier::getShakeFactor() const            { return 1.0f; }
float Soldier::getSprintFactor()                 { return 0.0f; }

// Access the embedded sprint state.
bool  Soldier::getIsSprinting() const            { return m_sprintState.m_isSprinting; }
void  Soldier::setIsSprinting(bool sprinting)    { m_sprintState.m_isSprinting = sprinting; }
float Soldier::getSprintStamina() const          { return m_sprintState.m_stamina; }

void Soldier::setSprintStamina(float stamina)
{
    m_sprintState.setStamina(stamina);
}

void Soldier::setSprintStaminaDissipationFactor(float f)
{ m_sprintState.m_staminaDissipationFactor = f; }

// . SoldierSound is constructed unconditionally with the soldier;
// retail therefore forwards without a null guard, just as the destructor does.
void Soldier::stopDrowningSound()
{
    m_soldierSound->stopDrowningSound();
}

// Adds mouse deltas to camera angles after applying speed and per-axis sensitivity.
void Soldier::increaseSoldierCameraTransformation(float dx, float dy)
{
    const float mod = getMouseSpeedMod();
    m_cameraYaw   += dx * mod * g_lookFactorX;
    m_cameraPitch += mod * dy * g_lookFactorY;
    updateCameraTransformation();
}

int  Soldier::getAmmoStatus()                    { return m_ammoStatus; }
void Soldier::setAmmoStatus(int status)          { m_ammoStatus = status; }

// Returns null for index 0 and out-of-range indices.
IObject* Soldier::getItem(int index)
{
    if (index > 0 && static_cast<size_t>(index) < m_itemObjects.size())
        return m_itemObjects[index];
    return 0;
}

// Removes and returns the item at index, stowing it and restoring physics.
// Requires a valid index: the original bounds check does not reject positive out-of-range values.
// An item parented elsewhere is removed from the slot without changing that parent.
IObject* Soldier::removeItem(int index)
{
    if (index <= 0 && index >= static_cast<int>(m_itemObjects.size()))
    {
        Debug(DEBUGTYPE_6, "Game/Objects/Game/Soldier.cpp", 2513)
            << "TBM: removeItem() wrong index:" << index;
        return 0;                                                    // see the note above
    }

    IObject* const item = m_itemObjects[index];
    if (!item)
        return 0;

    if (item->getParent() == this)                                   // +0x28
        removeChild(item);
    else if (item->getParent())
        Debug(DEBUGTYPE_6, "Game/Objects/Game/Soldier.cpp", 2525) << "TBM: Item have wrong parent";

    m_itemObjects[index] = 0;
    static_cast<IItemObject*>(item->queryInterface(IID_IItemObject))
        ->disableItem();                                             // +0x2C unchecked, as shipped
    reenablePhysics(item, false);
    if (item->queryInterface(IID_IWeaponObject))
        removeWeapon(item);
    return item;
}

// . Scan for the pointer and forward to the index form. IT starts AT 1, matching getItem's
// guard - slot 0 is not even looked at.
IObject* Soldier::removeItem(IObject* item)
{
    for (size_t i = 1; i < m_itemObjects.size(); ++i)
    {
        if (m_itemObjects[i] == item)
            return removeItem(static_cast<int>(i));
    }
    return 0;
}

// / 62 B. Both are kit dispatchers, and they are NOT symmetric: pickup tests the incoming
// object's class id, drop treats a NULL object as "drop the kit" too.
void Soldier::handlePickupOnItem(IPlayer* player, IObject* item)
{
    if (item->getTemplate()->getClassID() == CID_KitTemplate)
        pickupKit(player, item);                             // ; result discarded
}

void Soldier::handleDropOnItem(IPlayer*, IObject* item)
{
    if (!item || item->getTemplate()->getClassID() == CID_KitTemplate)
        dropKit();
}

// Temporarily changes pose for the physics validity test, then restores it.
// Requires a response-physics component.
bool Soldier::testIfValidTransformation(SoldierPose pose, const Mat4& transform, bool flag)
{
    const int saved = m_pose;
    m_pose = static_cast<int>(pose);
    IResponsePhysics* const responsePhysics = static_cast<IResponsePhysics*>(
        object_detail::LayoutAccess::responsePhysics(this));
    const bool ok = responsePhysics->testIfValidTransformation(transform, flag);
    m_pose = saved;
    return ok;
}

SoldierPose Soldier::getPose() const             { return static_cast<SoldierPose>(m_pose); }
SoldierPose Soldier::getRequestedPose() const    { return static_cast<SoldierPose>(m_requestedPose); }
void Soldier::setRequestedPose(SoldierPose pose) { m_requestedPose = static_cast<int>(pose); }
bool Soldier::getIsNewPoseRequested() const      { return m_pose != m_requestedPose; }

// . OUT OF range IS abort(), not a clamp and not a null: four poses, and asking for a fifth
// kills the process. The positions live on the template as three floats each.
const Vec3& Soldier::getPoseCameraPosition(int pose) const
{
    if (pose > 3)
        abort();
    return m_soldierTemplate->getPoseCameraPosition(pose);
}

// Soldiers always permit this weapon-selection gate.
bool Soldier::isNotFireingOrHaveRecoil()         { return true; }

Kit* Soldier::getKit()                           { return m_kit; }

// . thirteen IS THE NO-KIT answer, not zero and not -1.
int Soldier::getKitType() const
{
    if (m_kit)
        return m_kit->getType();                             // Kit's one own virtual, +0x1B8
    return 13;
}

int Soldier::getLastContactMaterial() const      { return m_lastContactMaterial; }

// (49 B) / PE 0x54FD10 (36 B, ISoldier face). Cache the sound receiver,
// then read the projectile material through IObject (ELF +0xD0 / PE +0xCC). The former
// objectSlot164 seam confused Hex-Rays' virtual41 label with a byte offset and remained
// declaration-only, faulting before a near-miss could continue into the hit-capsule test.
// The third argument is unused; neither shipped body adds a null guard.
void Soldier::bulletFlyBy(IObject* projectile, const Vec3& at, const Vec3&)
{
    SoldierSound* const sound = m_soldierSound;
    sound->bulletFlyBy(projectile->getMaterial(), at);
}

// Underwater queries return false and normalized time 1; swimming remains a separate state.
bool  Soldier::getIsUnderwater() const           { return false; }
bool  Soldier::getIsSwimming() const             { return m_isSwimming; }
float Soldier::getNormalizedUnderwaterTime() const { return 1.0f; }

// Soldier dimensions come from live console variables shared by all instances.
float Soldier::getSoldierRadius() const          { return g_soldierRadius; }
float Soldier::getStandingHeight() const         { return g_soldierStandingHeight; }

// . grabbing A ladder stops YOU swimming, and re-evaluates whether your weapon is usable.
void Soldier::setLadderId(unsigned int id)
{
    m_isSwimming = false;
    m_ladderId = id;
    updateWeaponIsActiveState();
}

unsigned int Soldier::getLadderId() const        { return m_ladderId; }
void Soldier::setZiplineId(unsigned int id)      { m_ziplineId = id; }
unsigned int Soldier::getZiplineId() const       { return m_ziplineId; }
void Soldier::setGrapplingHookId(unsigned int id){ m_grapplingHookId = id; }
unsigned int Soldier::getGrapplingHookId() const { return m_grapplingHookId; }
void Soldier::setReadyToClimbTime(float t)       { m_readyToClimbTime = t; }

bool  Soldier::getIsSkyDiving() const            { return m_isSkyDiving; }
void  Soldier::dropWeaponFx(bool, Vec3&, int)    { }
float Soldier::getCurrentAbility() const         { return m_currentAbility; }
void  Soldier::setCurrentAbility(float a)        { m_currentAbility = a; }

// . spend, NOT SET: it refuses and changes nothing when the meter is short, and the caller
// learns that from the bool. The comparison is `>=`, so spending exactly the remainder succeeds.
bool Soldier::taxAbility(float amount)
{
    if (m_currentAbility >= amount)
    {
        m_currentAbility -= amount;
        return true;
    }
    return false;
}

float Soldier::getFireDelayAfterJump() const     { return m_fireDelayAfterJump; }

// . TWO timers, both must BE expired, and the second one (+0x3B4) has no named accessor at
// all - it is only ever read here. Firing is blocked after a jump AND after whatever +0x3B4 counts.
bool Soldier::getCanFireWeapon() const
{
    if (m_fireDelayAfterJump > 0.0f)
        return false;
    return m_float3B4 <= 0.0f;
}

// The non-virtual small members.

// . (1, 0, 0) and the current world time - the head starts looking straight ahead.
void Soldier::resetHeadMovements()
{
    m_headPosition.x = 1.0f;
    m_headPosition.y = 0.0f;
    m_headPosition.z = 0.0f;
    m_soldierLookIdleTime = g_soldierLookIdleTimeSource;
}

// . asymmetric: turning IK OFF also clears whatever the skeleton is already solving; turning
// it back ON only lowers the latch.
void Soldier::disableInverseKinematic(bool disable)
{
    if (disable)
    {
        m_inverseKinematicDisabled = true;
        if (m_collisionSkeleton)
            skeletonClearAllIk(m_collisionSkeleton);         // ghost - dice::anim is unrecovered
    }
    else
    {
        m_inverseKinematicDisabled = false;
    }
}

// Look changes are ignored while ragdolling.
void Soldier::setLookUpDownAngle(float angle)
{
    if (m_ragDoll)
        return;
    m_lookUpDownAngle = angle;
    updateCameraTransformation();                            // / BF2.exe 5A8630
}

void Soldier::setLookLeftRightAngle(float angle)
{
    if (m_ragDoll)
        return;
    m_lookLeftRightAngle = angle;
    updateCameraTransformation();                            // / BF2.exe 5A8630
}

// . NO camera IS abort(). The movement basis is the soldier camera's absolute transformation,
// and a soldier without one kills the process rather than falling back to its own transform.
const Mat4& Soldier::getMovementMatrix()
{
    ICameraObject* camera = getSoldierCamera();
    if (!camera)
        abort();
    IObject* cameraObject = static_cast<IObject*>(camera->queryInterface(IID_IObject));
    return cameraObject->getAbsoluteTransformation();
}

// Plays a third-person flinch selected by hit bone and camera-relative shot direction.
// Requires armor; ragdolls and other character modes do not play a flinch.
void Soldier::playDamageAnimations()
{
    typedef object_detail::LayoutAccess L;

    if (m_ragDoll)                                   // +0x220 a ragdolling soldier does not flinch
        return;
    if (!L::geometry(this))
        return;
    if (!m_characterMode)                            // +0x284
        return;
    if (getParent())                                 // +0x28 a soldier in a vehicle does not flinch
        return;

    IArmorGLComp* const armor = static_cast<IArmorGLComp*>(getArmorComponent());
    if (armor->isDestroyed())                        // +0x54 unguarded armor pointer, as shipped
        return;

    const Vec3& hitDirection = armor->getLastHitDirection();       // +0xC0
    const int   bone         = armor->getLastHitBoneIndex();       // +0xB8
    const Mat4& movement     = getMovementMatrix();

    const float alongForward = movement.m[8]  * hitDirection.x
                             + movement.m[9]  * hitDirection.y
                             + movement.m[10] * hitDirection.z;

    const char* front;
    const char* back;
    switch (bone)
    {
    case 15:
    case 17: front = "hitFrontLeft";  back = "hitBackLeft";  break;
    case 31:
    case 33: front = "hitFrontRight"; back = "hitBackRight"; break;
    case 47: front = "hitFrontHead";  back = "hitBackHead";  break;
    default: front = "hitFrontBody";  back = "hitBackBody";  break;
    }

    // >= 0 is back - see the header block.
    const char* const trigger = (alongForward >= 0.0f) ? back : front;

    if (m_characterMode == 1)
        m_animationSystem3P->playPostTrigger(std::string(trigger), false);   // +0x20
}

// ELF / 50 B PE. "Is weapon slot 0 present and enabled" - bit 0 of the IObject +0x04 flags
// word is its disable flag. Retail deliberately calls getActiveWeapon(0) twice.
bool Soldier::hasHandWeaponActive()
{
    if (!getActiveWeapon(0))
        return false;
    return (getActiveWeapon(0)->getFlags() & 1u) == 0;
}

// . changing SKY-diving state forces A pose change - but only if the soldier is holding
// something (activeItemIndex != -1). The requested pose is hard-set to 0 rather than computed.
void Soldier::setIsSkyDiving(bool skyDiving)
{
    if (m_isSkyDiving == skyDiving)
        return;
    const bool hadNoItem = (m_activeItemIndex == -1);
    m_isSkyDiving = skyDiving;
    if (!hadNoItem)
    {
        m_requestedPose = 0;
        executePoseChange();                                 // [FRONTIER]
    }
}

// Soldier::getWeaponFromIndex - ELF 0x81AE810, 133 B / PE 0x550F70, 111 B.
// This scans only m_weapons[0], the primary-fire bucket, NOT the flat m_weaponVec and not the
// 1-based item array. Null objects and objects without IItemObject are skipped. The matching object
// is deliberately re-read from the vector after getItemIndex(), preserving retail's access order.
IObject* Soldier::getWeaponFromIndex(int index) const
{
    for (unsigned int i = 0; i < m_weapons[0].size(); ++i)
    {
        IObject* const weapon = m_weapons[0][i];
        if (!weapon)
            continue;

        IItemObject* const item = static_cast<IItemObject*>(
            weapon->queryInterface(IID_IItemObject));
        if (item && item->getItemIndex() == index)
            return m_weapons[0][i];
    }
    return 0;
}

// . "Previous" is a two-slot preference, not a history: weapon1 if it is still selectable,
// weapon2 otherwise. Retail dispatches GenericFireArm::isSelectable through the object's primary
// vtable, so keep that direct product-class call rather than reducing it to a template flag.
IObject* Soldier::getPreviousWeapon() const
{
    if (m_weapon1 && static_cast<GenericFireArm*>(m_weapon1)->isSelectable())
        return m_weapon1;
    return m_weapon2;
}

// . Ask the item at `index`, skip it if its IObject disable bit is set, then forward the
// message through the typed IObject virtual.
void Soldier::sendMessageToItem(int index, TemplateMessage message, IPlayer* player, bool flag)
{
    IObject* item = getItem(index);
    if (!item)
        return;
    if (item->getFlags() & 1u)
        return;
    item->handleMessage(message, player, flag);              // ELF +0x104 / typed MSVC slot
}

// . THE HIT lasts sixteen ticks. The speed is stored and a deadline stamped at
// getTickCount() + 16 - roughly half a second at 30 Hz.
void Soldier::setHitByVehicleSpeed(const Vec3& speed)
{
    m_hitByVehicleSpeed.x = speed.x;
    m_hitByVehicleSpeed.y = speed.y;
    m_hitByVehicleSpeed.z = speed.z;
    m_hitByVehicleSpeedTicks = WorldPref::getTickCount() + 16;
}

// . IT does NOT drop A helmet - it zeroes one dword. Whatever consumes +0x22C is what makes
// the helmet appear; this only arms it.
void Soldier::dropHelmetFx()
{
    m_helmetTime = 0.0f;
}

// / 5 B / 5 B. All three are empty in the shipped binary. Footstep effects, the breath-holding
// meter and leaving a ladder are named, dispatched and do nothing - on this build.
void Soldier::triggerFootStepEffect(int, int)    { }
void Soldier::updateHoldBreathUnderwaterTime(float) { }
void Soldier::removeFromLadder()                 { }

// Resets an unoccupied soldier, releasing ragdoll, occupant references and animation state.
// Aborts if this soldier or its parent control object is occupied, then unregisters the reset soldier.
void Soldier::resetInstance()
{
    m_float274 = -1.0f;
    m_float278 = -1.0f;
    m_dword26C = 0;                              // +0x26C
    m_dword270 = 4;                              // +0x270 four, not zero

    // Two refcounted blocks, released identically.
    if (m_refBlock1D8)
    {
        int* count = static_cast<int*>(m_refBlock1D8);
        if ((*count)-- == 1)
            operator delete(m_refBlock1D8);
    }
    m_refBlock1D8 = 0;

    if (m_occupyingPlayerWeak)
    {
        int* count = static_cast<int*>(m_occupyingPlayerWeak);
        if ((*count)-- == 1)
            operator delete(m_occupyingPlayerWeak);
    }
    m_occupyingPlayerWeak = 0;

    m_isOccupied = false;
    m_pose = 0;
    m_activeItemIndex = -1;                      // nothing held
    m_poseInputLatch = 0;
    m_requestedPose = 0;
    m_kit = 0;
    m_kitBeingPickedUp = 0;
    m_facingDegrees = 0.0f;
    m_dword1EC = 0;
    m_smoothedStrafe = 0.0f;
    m_smoothedForward = 0.0f;
    m_isManDown = 0;                              // the ELF writes only its LOW byte
    m_lookUpDownAngle = 0.0f;
    m_lookLeftRightAngle = 0.0f;
    m_cameraPitch = 0.0f;
    m_cameraYaw = 0.0f;
    m_float214 = 0;

    m_cameraSpringDisplacement.x = 0.0f;          // +0x288
    m_cameraSpringDisplacement.y = 0.0f;          // +0x28C
    m_cameraSpringDisplacement.z = 0.0f;          // +0x290
    m_yawDeltaThisTick = 0.0f;                   // +0x294
    m_pitchDeltaThisTick = 0.0f;                 // +0x298
    m_dword29C = 0;                             // +0x29C
    m_ikObject = 0;                              // +0x264
    m_ikTemplateComponent = 0;                   // +0x268

    m_soldierSpeedIndex = 3;                     // three, not zero
    m_lastContactMaterial = -1;
    m_dword2D0 = -1;
    m_isSwimming = false;
    m_poseChangeProgress = 0.0f;
    m_poseChangeDuration = 0.0f;
    m_soldierSpeed = 0.0f;
    m_timeToEnableWeapon = 0.0f;
    m_dword2D4 = -1;
    m_weapon2 = 0;
    m_medicCalled = 0;
    m_weapon1 = 0;
    m_ladderId = 0;
    m_isSkyDiving = false;
    m_thirdSpeedOnce = false;                           // +0x314
    m_helmetTime = 0.0f;

    m_explosionForce.x = 0.0f;
    m_explosionForce.y = 0.0f;
    m_explosionForce.z = 0.0f;
    m_ammoStatus = 15;                           // 0b1111 - full
    m_explosionForceTime = 0.0f;

    m_byte27C = false;
    m_byte27D = false;
    m_float230 = 0.0f;
    m_headPosition.x = 0.0f;
    m_headPosition.y = 0.0f;
    m_headPosition.z = 0.0f;
    m_inverseKinematicDisabled = false;
    m_byte27F[0] = false;                        // +0x27F
    m_byte27F[1] = false;                        // +0x280

    m_byte37C = false;
    m_characterMode = 1;                         // one, not zero
    m_byte38D = false;
    m_inputSuppressTimer = 0.0f;
    m_weaponStowed = false;
    m_deployAnimationActive = false;
    m_deployAnimationTimer = 0.0f;
    m_byte384 = false;
    m_byte385 = false;
    m_dword388 = 0;
    m_ziplineId = 0;
    m_grapplingHookId = 0;
    m_readyToClimbTime = 0.0f;
    m_byte399 = false;

    // The ragdoll is destroyed outright, not just dropped.
    if (m_ragDoll)
    {
        delete m_ragDoll;               // ghost - ~RagDoll + operator delete
        m_ragDoll = 0;
    }

    if (m_collisionSkeleton)
        skeletonClearAllIk(m_collisionSkeleton); // ghost - dice::anim is unrecovered

    // Both animation-system instances, walked as a stride-2 array: +0x258 and +0x260.
    if (m_animationSystem1P)
        m_animationSystem1P->reset();
    if (m_animationSystem3P)
        m_animationSystem3P->reset();

    // THE invariant. Two questions, either one fatal.
    IPlayerControlObject* parentPco = 0;
    for (IObject* node = getParent(); node; node = node->getParent())
    {
        parentPco = static_cast<IPlayerControlObject*>(
            node->queryInterface(IID_IPlayerControlObject));
        if (parentPco)
            break;
    }
    if (parentPco)
    {
        // The ELF walks the chain a second time here rather than reusing what it just found.
        IPlayerControlObject* again = 0;
        for (IObject* node = getParent(); node; node = node->getParent())
        {
            again = static_cast<IPlayerControlObject*>(
                node->queryInterface(IID_IPlayerControlObject));
            if (again)
                break;
        }
        if (again->getOccupingPlayer())
            abort();
    }
    if (getOccupingPlayer())
        abort();

    say(0, 0);                                   // stop any speech - see say()
    gameLogic->removePco(this);                  // IGameLogic +0x19C
}

// Soldier rig setters use the console property names without a set prefix.

// Clears animation pointers and marks an existing fallingWeapon template with flag 0x40.
void SoldierTemplate::initAnimation()
{
    m_skeleton1P = 0;
    m_animationSystem1P = 0;
    m_animationSystem3P = 0;
    m_skeleton3P = 0;

    IObjectTemplate* weapon = objectTemplateManager->getTemplate(std::string("fallingWeapon"));
    m_weapObjTemplate = weapon;
    if (weapon)
        weapon->updateFlags(0x40u, 0u);          // IObjectTemplate +0x4C
}

// Deletes both skeletons and releases both animation templates; manager release accepts null.
void SoldierTemplate::releaseAnimation()
{
    if (m_skeleton1P)
        delete m_skeleton1P;
    if (m_skeleton3P)
        delete m_skeleton3P;

    animationSystemManagerRelease(m_animationSystem1P);      // unguarded, see above
    animationSystemManagerRelease(m_animationSystem3P);
}

// Drops the kit and releases animation, sound and weak occupant references.
Soldier::~Soldier()
{
    dropKit();
    releaseAnimation();
    m_soldierSound->stopDrowningSound();

    if (m_soldierSound)
        delete m_soldierSound;

    if (m_refBlock1D8)
    {
        int* count = static_cast<int*>(m_refBlock1D8);
        if ((*count)-- == 1)
            operator delete(m_refBlock1D8);
    }
    if (m_usedVehicleWeak)
    {
        int* count = static_cast<int*>(m_usedVehicleWeak);
        if ((*count)-- == 1)
            operator delete(m_usedVehicleWeak);
    }
    if (m_occupyingPlayerWeak)
    {
        int* count = static_cast<int*>(m_occupyingPlayerWeak);
        if ((*count)-- == 1)
            operator delete(m_occupyingPlayerWeak);
    }
}

// Deletes the ragdoll and both owned skeleton/animation-system pairs, clearing their pointers.
void Soldier::releaseAnimation()
{
    if (m_ragDoll)
    {
        delete m_ragDoll;                                   // ~RagDoll + operator delete
        m_ragDoll = 0;
    }

    if (m_animSkeleton)                                              // +0x254
    {
        delete static_cast<anim::Skeleton*>(m_animSkeleton);
        m_animSkeleton = 0;
    }
    if (m_animationSystem1P)                                         // +0x258
    {
        delete m_animationSystem1P;
        m_animationSystem1P = 0;
    }
    if (m_collisionSkeleton)                                         // +0x25C
    {
        delete m_collisionSkeleton;
        m_collisionSkeleton = 0;
    }
    if (m_animationSystem3P)                                         // +0x260
    {
        delete m_animationSystem3P;
        m_animationSystem3P = 0;
    }
}

// Applies animation for the visual frame, updating ragdoll bones on clients and in bot games.
// The incoming distance argument is unused.
void Soldier::handleVisualUpdate(float deltaTime, float)
{
    if (m_ragDoll)                                                        // +0x220
    {
        // aiConnectionIsUp() is exactly the shipped first arm: the null test AND slot +0x1C.
        if (aiConnectionIsUp() || (gameLogic && !gameLogic->isServer()))  // +0x13C
            m_ragDoll->applyOnSkeleton();
    }

    applyAnimationsOnSkeleton(deltaTime, true);                           // tail call, 0x81B5D60
}

// Updates ragdoll presentation, animation, device deployment and movement cooldowns.
// Cooldowns may finish negative; device deployment completes on the following frame.
void Soldier::handleFrameUpdate(float deltaTime)
{
    typedef object_detail::LayoutAccess L;

    if (m_ragDoll)
    {
        Mat4 transformation = Mat4::getIdentity();
        transformation.m[12] = m_ragDoll->m_position.x;
        transformation.m[13] = m_ragDoll->m_position.y;
        transformation.m[14] = m_ragDoll->m_position.z;
        setAbsoluteTransformation(transformation);

        // The binary writes null directly; it does not delete the interpolation matrix here.
        L::interpolationTransformation(this) = 0;
        if (m_ragDoll->m_awake)
            updateGeometry();

        // The layout is skeleton / animation system / skeleton / animation system. Character mode
        // selects the skeleton at a two-pointer stride, with no range or null check in retail.
        anim::Skeleton* const skeleton =
            static_cast<anim::Skeleton*>(*reinterpret_cast<void**>(
                reinterpret_cast<unsigned char*>(&m_animSkeleton)
                + 2 * sizeof(void*) * m_characterMode));
        const Vec3 local = {
            skeleton->m_transforms[47].m[12],
            skeleton->m_transforms[47].m[13],
            skeleton->m_transforms[47].m[14]
        };
        const Mat4& absolute = getAbsoluteTransformation();
        Vec3 world;
        world.x = local.z * absolute.m[8]  + local.y * absolute.m[4]
                + local.x * absolute.m[0]  + absolute.m[12];
        world.y = local.z * absolute.m[9]  + local.y * absolute.m[5]
                + local.x * absolute.m[1]  + absolute.m[13];
        world.z = local.z * absolute.m[10] + local.y * absolute.m[6]
                + local.x * absolute.m[2]  + absolute.m[14];

        if (m_ragDoll->m_awake)
        {
            const float factor = deltaTime * *g_soldierManDownSpeedVar;
            m_float318     = (world.x - m_float318)     * factor + m_float318;
            m_float31C[0] = (world.y - m_float31C[0]) * factor + m_float31C[0];
            m_float31C[1] = (world.z - m_float31C[1]) * factor + m_float31C[1];
        }

        const float rotationFactor = deltaTime * *g_soldierManDownRotSpeedVar;
        m_float324     = (m_float330     - m_float324)     * rotationFactor + m_float324;
        m_float328[0] = (m_float334[0] - m_float328[0]) * rotationFactor + m_float328[0];
        m_float328[1] = (m_float334[1] - m_float328[1]) * rotationFactor + m_float328[1];

        if (m_ragDoll->m_settled && m_dword224 == 0)
            m_dword224 = WorldPref::getTickCount();
    }

    updateAnimationSystems(deltaTime);

    if (m_deployAnimationActive)
    {
        if (m_deployAnimationTimer > 0.0f)
        {
            m_deployAnimationTimer -= deltaTime;
        }
        else
        {
            const bool gasMaskOn = getOccupingPlayer()->getIsGasMaskOn();
            getOccupingPlayer()->setGasMaskHudItem(gasMaskOn);
            const bool nightVisionOn = getOccupingPlayer()->getNightVision();
            getOccupingPlayer()->setVisualNightVision(nightVisionOn);
            m_deployAnimationActive = false;
            m_byte384 = false;

            IPlayerControlObject* pco = 0;
            for (IObject* node = this; node; node = node->getParent())
            {
                pco = static_cast<IPlayerControlObject*>(
                    node->queryInterface(IID_IPlayerControlObject));
                if (pco)
                    break;
            }
            if (pco)
            {
                IObject* const weapon = pco->getActiveWeapon(0);
                if (weapon)
                {
                    const IObjectTemplate* const weaponTemplate = L::objectTemplate(weapon);
                    if (weaponTemplate
                        && weaponTemplate->getClassID() == CID_GenericFireArmTemplate)
                    {
                        static_cast<GenericFireArm*>(weapon)->m_pendingActiveWeapon =
                            getPreviousWeapon();
                    }
                }
            }
        }
    }

    if (m_fireDelayAfterJump > 0.0f)
        m_fireDelayAfterJump -= deltaTime;
    if (m_poseTimers[0] > 0.0f)
        m_poseTimers[0] -= deltaTime;
    if (m_poseTimers[1] > 0.0f)
        m_poseTimers[1] -= deltaTime;
    if (m_poseTimers[2] > 0.0f)
        m_poseTimers[2] -= deltaTime;
    if (m_poseTimers[3] > 0.0f)
        m_poseTimers[3] -= deltaTime;
    if (m_poseTimers[4] > 0.0f)
        m_poseTimers[4] -= deltaTime;
    if (m_float3B4 > 0.0f)
        m_float3B4 -= deltaTime;
}

// Updates soldier input, sprint, pose, kit, camera and sound for a simulation tick.
// Flag 4 suppresses simulation side effects while retaining positional updates.
void Soldier::handleUpdate(float deltaTime, unsigned int flags)
{
    // 1. the fire-button debounce pair
    if ((flags & 4u) == 0)
    {
        for (int i = 0; i <= 1; ++i)
            m_fireButtonState[i].handleUpdate();
    }

    // 2. the sprint model
    bool sprintBlocked = false;
    if (isWalking() || getParent() || m_pose == 2 || m_pose == 1
        || (getOccupingPlayer() && getOccupingPlayer()->getBlurValue() > 0.0f))     // IPlayer +0x2B8
        sprintBlocked = true;

    float sprintFactor;
    if (getOccupingPlayer())
    {
        if (getOccupingPlayer()->getDefaultVehicle() == getOccupingPlayer()->getVehicle())
        {
            sprintFactor = m_poseTimers[4];                                          // +0x3B0
        }
        else
        {
            sprintFactor = 0.0f;
            if (m_poseTimers[4] <= 0.0f)
                sprintFactor = m_poseTimers[4];
            else
                m_poseTimers[4] = 0.0f;
        }
    }
    else
    {
        sprintFactor = m_poseTimers[4];
    }
    m_sprintState.handleUpdate(sprintBlocked, sprintFactor, deltaTime);

    // 3. the one-shot die-animation latch
    // THE LOW byte only, as compiled - the field is a dword but only its first byte is touched.
    unsigned char* const dieLatch = reinterpret_cast<unsigned char*>(&m_isManDown);
    if (*dieLatch && (flags & 4u) == 0)
    {
        *dieLatch = 0;
        triggerDieAnimation();
    }

    // 4. the explosion force
    if (m_explosionForceTime > 0.0f && (flags & 4u) == 0)
    {
        m_explosionForceTime -= deltaTime;
        if (m_explosionForceTime <= 0.0f)
        {
            m_explosionForce.x = 0.0f;
            m_explosionForce.y = 0.0f;
            m_explosionForce.z = 0.0f;
            m_explosionForceTime = 0.0f;
        }
    }
    if (m_ragDoll)
    {
        if (m_explosionForce.x * m_explosionForce.x + m_explosionForce.y * m_explosionForce.y
            + m_explosionForce.z * m_explosionForce.z > 0.0f)
        {
            m_ragDoll->addForce(-2, m_explosionForce);      // ghost - see the hooks
            m_explosionForce.x = 0.0f;
            m_explosionForce.y = 0.0f;
            m_explosionForce.z = 0.0f;
        }
    }

    const bool parented = getParent() != 0;
    const bool isLocalPlayersSoldier =
        (playerManager->getLocalHumanPlayer() == getOccupingPlayer());

    // 5. an unparented soldier is levelled, sky-dive-tested and crush-tested
    if (!parented)
    {
        // RE-level about world UP: row 1 is forced to (0,1,0) and the frame rebuilt around it, so
        // a soldier keeps his facing but never tilts. makeOrthonormalBasis writes all three rows.
        Mat4 levelled = getAbsoluteTransformation();
        Vec3* const rows = reinterpret_cast<Vec3*>(&levelled.m[0]);   // rows 0,1,2 at m[0], m[4], m[8]
        Vec3& right   = *reinterpret_cast<Vec3*>(&levelled.m[0]);
        Vec3& up      = *reinterpret_cast<Vec3*>(&levelled.m[4]);
        Vec3& forward = *reinterpret_cast<Vec3*>(&levelled.m[8]);
        (void)rows;
        up.x = 0.0f;
        up.y = 1.0f;
        up.z = 0.0f;
        right.makeOrthonormalBasis(up, forward);
        setAbsoluteTransformation(levelled);

        const float verticalSpeed = soldierPhysicsNode(this)->getPositionalSpeed().y;
        setIsSkyDiving(verticalSpeed < *g_skydiveAtSpeedVar);

        if (m_pose == 2 && !soldierCanGoProne())
        {
            if (m_requestedPose <= 1)
            {
                // crushed: prone somewhere prone is illegal pays drown damage every tick until he
                // moves or dies. only while both pose timers are spent.
                if (gameLogic && m_poseTimers[0] <= 0.0f && m_poseTimers[3] <= 0.0f)
                {
                    const float damage = 100.0f / *g_soldierDrownDamageVar * deltaTime;
                    Vec3 at = getAbsolutePosition();
                    Vec3 zero;
                    zero.x = 0.0f; zero.y = 0.0f; zero.z = 0.0f;
                    gameLogic->giveDamage(this, damage, 0, 0, 0, -1, 0, 0, at, zero, 78,
                                          false, true, false);
                }
            }
            else
            {
                m_requestedPose = 0;
            }
        }
    }

    // 6. the gas-mask / character-mode ladder
    bool poseHeld = false;
    bool holdingDevice = false;
    {
        IPlayerControlObject* pco = 0;
        for (IObject* node = this; node; node = node->getParent())
        {
            pco = static_cast<IPlayerControlObject*>(
                node->queryInterface(IID_IPlayerControlObject));
            if (pco)
                break;
        }
        if (pco)
        {
            if (IObject* const active = pco->getActiveWeapon(0))               // ipco +0x70
            {
                if (const IObjectTemplate* const t = active->getTemplate())
                {
                    if (t->getClassID() == CID_GenericFireArmTemplate
                        && static_cast<const GenericFireArmTemplate*>(t)->m_isGasMask)
                        holdingDevice = true;
                }
            }
        }
    }

    if (isLocalPlayersSoldier && getOccupingPlayer()
        && (holdingDevice || !hasHandWeaponActive() || m_weaponStowed))
    {
        getOccupingPlayer()->getCamera();                                       // result discarded
        if (getOccupingPlayer()->hasFirstPersonView())
        {
            poseHeld = true;
            updateFlags(0, 2);
        }
    }

    if (!getOccupingPlayer()
        || getOccupingPlayer() != playerManager->getLocalHumanPlayer())
    {
        if (getOccupingPlayer() && m_characterMode != 1)
            std::abort();                                                       // as shipped
    }
    else if (objectArmorComp(this)->getStatus() != 1)
    {
        if (getOccupingPlayer()->hasFirstPersonView())
            setCharacterMode(0);
        else
            setCharacterMode(1);
    }
    else if (!getOccupingPlayer()->getIsManDown())
    {
        setCharacterMode(1);
    }
    else
    {
        if (m_characterMode)
        {
            if (ICameraObject* const camera = getOccupingPlayer()->getCamera())
                cameraObjectSetViewMode(camera, static_cast<CameraViewMode>(1), 0, false);
            setCharacterMode(0);
            m_requestedPose = 2;
            executePoseChange();
        }
        updateFlags(0, 2);
        poseHeld = true;
        m_pose = 2;
        m_requestedPose = 2;
        updateSoldierSpawnPoint(static_cast<SoldierPose>(2));
    }

    // 7. the kit
    if (m_kit)
    {
        if (m_characterMode == 1)
            m_kit->updateFlags(0, 1);
        else
            m_kit->updateFlags(1, 0);

        if ((flags & 4u) == 0)
        {
            if (m_currentAbility >= 0.0f)
            {
                float ability = m_kit->getAbilityUpdate() + m_currentAbility;
                if (ability > 1.0f)
                    ability = 1.0f;
                m_currentAbility = ability;
            }
            if (m_helmetTime > 0.0f)
            {
                m_helmetTime -= deltaTime;
                if (m_helmetTime <= 0.0f)
                    dropHelmetFx();
            }
            if (m_timeToEnableWeapon > 0.0f)
            {
                m_timeToEnableWeapon -= deltaTime;
                if (m_timeToEnableWeapon <= 0.0f)
                {
                    m_timeToEnableWeapon = 0.0f;
                    if (m_activeItemIndex != -1)
                    {
                        IObject* const item = getItem(m_activeItemIndex);
                        IItemObject* face = 0;
                        if (item)
                            face = static_cast<IItemObject*>(
                                item->queryInterface(IID_IItemObject));
                        face->enableItem();                    // +0x28 unchecked, as shipped
                    }
                }
            }
        }

        if (gameLogic->isServer() && getOccupingPlayer()
            && getOccupingPlayer()->getIsManDown() && hasRagdollLanded())
        {
            gameLogic->handleDrop(m_kit, getOccupingPlayer(), this, true);
        }
    }

    if (!poseHeld)
        updateFlags(2, 0);

    // 8. the base update, the camera, and the sound
    if ((getFlags() & 1u) != 0)
        return;                                                     // disabled - nothing below runs

    SimpleObject::handleUpdate(deltaTime, flags);

    if (m_isOccupied)
    {
        ICameraObject* const camera = getSoldierCamera();
        if (IObject* const cameraObject =
                static_cast<IObject*>(camera->queryInterface(IID_IObject)))   // camera unchecked
            cameraObject->handleUpdate(deltaTime, flags);                     // slot 44
    }

    if (m_ragDoll)
    {
        if ((flags & 4u) == 0)
        {
            float volume = 0.0f;
            if (const anim::RagDoll::ParticleCollision* const collision = m_ragDoll->getParticleCollision())
            {
                volume = collision->m_strength < 0.0f ? 0.0f
                       : (collision->m_strength > 1.0f ? 1.0f : collision->m_strength);
                m_soldierSound->dieHit(collision->m_material, 0.0f);
            }
            m_soldierSound->update(deltaTime, 0);
        }
        return;
    }

    if (!parented)
    {
        // Re-level after SimpleObject::handleUpdate may have changed the transform.
        Mat4 levelled = getAbsoluteTransformation();
        Vec3& right   = *reinterpret_cast<Vec3*>(&levelled.m[0]);
        Vec3& up      = *reinterpret_cast<Vec3*>(&levelled.m[4]);
        Vec3& forward = *reinterpret_cast<Vec3*>(&levelled.m[8]);
        up.x = 0.0f;
        up.y = 1.0f;
        up.z = 0.0f;
        right.makeOrthonormalBasis(up, forward);
        setAbsoluteTransformation(levelled);
    }

    if ((flags & 4u) == 0)
    {
        int material = -1;
        m_soldierSound->update(deltaTime, &material);
        if (!getLadderId() && !getGrapplingHookId())
            triggerFootStepEffect(material, 5500);
    }

    if (SoldierResponsePhysics* const response = soldierResponsePhysics(this))
        response->reset();

    if (!m_grapplingHookId && m_readyToClimbTime > 0.0f)
        m_readyToClimbTime -= deltaTime;

    if (gameLogic->isServer())
        m_ammoStatus = calcAmmoStatus();
}

// Handles soldier contact and forwards ordinary collisions to Bundle.
// Returns false for ragdolls and consumed barbed-wire hits; null object denotes terrain.
// Barbed wire slows movement and applies server damage.
bool Soldier::handleCollision(IObject* object, const Vec3& speed, const Vec3& normal,
                              const Vec3& relative, int ownMaterial, int otherMaterial,
                              CollisionFaceInfo* faceInfo1, CollisionFaceInfo* faceInfo2)
{
    m_byte398 = false;
    if (m_ragDoll)
        return false;

    bool result = true;
    if (object)
    {
        Material* const material =
            static_cast<MaterialManager*>(materialManager)
                ->getMaterialPtr(static_cast<unsigned int>(otherMaterial));       // +0x24
        if (material && material->getIsBarbwire())                                // slot 27 +0x6C
        {
            m_thirdSpeedOnce = true;
            object->handleMessage(static_cast<TemplateMessage>(0), 0, false);     // slot 65
            if (game->queryInterface(IID_IGameServer))
            {
                Vec3 up;
                up.x = 0.0f;
                up.y = 1.0f;
                up.z = 0.0f;
                Vec3 at = getAbsolutePosition();                                  // slot 27
                gameLogic->giveDamage(this, 0.5f, 0, 0, 0, -1, 0, 0, at, up, -1,
                                      false, true, false);                        // IGameLogic +0xE4
            }
            result = false;
        }

        IObject* const root = getRootParent(object);
        if (object->getTemplate()->getClassID() == CID_PlayerControlObjectTemplate
            || (object->getTemplate()->getClassID() == CID_SoldierTemplate && object != this)
            || root->getTemplate()->getClassID() == CID_PlayerControlObjectTemplate
            || (root->getTemplate()->getClassID() == CID_SoldierTemplate && object != this))
            m_byte398 = true;
    }

    m_lastContactMaterial = otherMaterial;
    Bundle::handleCollision(object, speed, normal, relative, ownMaterial, otherMaterial,
                            faceInfo1, faceInfo2);
    return result;
}

// Returns the 0..15 ammunition gauge from refillable primary weapons' magazine counts.
// Empty loaded magazines reduce the count; gas-mask and night-vision items are excluded.
int Soldier::calcAmmoStatus()
{
    float current = 0.0f;
    float maximum = 0.0f;

    std::vector<IObject*>& primary = m_weapons[0];                  // getWeapons(0), +0x1D0
    for (std::vector<IObject*>::iterator it = primary.begin(); it != primary.end(); ++it)
    {
        IWeaponObject* const weaponObject = static_cast<IWeaponObject*>(
            (*it)->queryInterface(IID_IWeaponObject));
        if (!weaponObject)
            continue;
        if (!weaponObjectCanBeRefilled(weaponObject))               // +0x74
            continue;
        if (weaponObjectGetMagSize(weaponObject) <= 0)              // +0xA8
            continue;

        const GenericFireArmTemplate* const weaponTemplate =
            static_cast<const GenericFireArmTemplate*>((*it)->getTemplate());
        if (weaponTemplate->m_isNightVision || weaponTemplate->m_isGasMask)   // NVG tested first
            continue;

        const float mags    = static_cast<float>(weaponObjectGetCurrentNrOfMags(weaponObject));  // +0xA0
        const float maxMags = static_cast<float>(weaponObjectGetMaxNrOfMags(weaponObject));      // +0x98
        if (!(mags > 0.5f) || !(maxMags > 0.5f))
            continue;

        current += mags;
        maximum += maxMags;
        if (!weaponObjectGetAmmoInMag(weaponObject, 0))             // +0x94 the loaded mag is empty
            current -= 1.0f;
    }

    if (maximum < 1.0f)
        return 0;

    float bar = current / maximum * 15.0f;
    if (bar < 0.0f)
        bar = 0.0f;
    else if (bar > 15.0f)
        bar = 15.0f;
    return static_cast<int>(std::floor(bar));
}

// Returns true without a ragdoll; otherwise requires settlement and the configured tick delay.
bool Soldier::hasRagdollLanded() const
{
    if (!m_ragDoll)
        return true;
    if (!m_ragDoll->m_settled)                              // RagDoll +0x5F
        return false;
    const SoldierTemplate* const objectTemplate =
        static_cast<const SoldierTemplate*>(getTemplate());
    const float landedTick = 30.0f * objectTemplate->m_ragdollLandedDelay   // template +632
                           + static_cast<float>(m_dword224);
    return static_cast<float>(WorldPref::getTickCount()) > landedTick;
}

// Creates and initializes an item from templateName. The index argument is unused.
// Returns -1 on failure, 0 for a kit, or the created item's index.
int Soldier::addItem(const std::string& templateName, int /*index*/)
{
    IObjectTemplate* const objectTemplate = objectTemplateManager->getTemplate(templateName);
    if (!objectTemplate)
        return -1;
    IObject* const item = objectTemplate->createObject();
    if (!item)
        return -1;
    item->init();
    item->updateFlags(0, 0x100u);
    world::updateFlagsRecursive(item, 0, 0x100u);
    if (item->getTemplate()->getClassID() == CID_KitTemplate)
    {
        pickupKit(0, item);
        return 0;
    }
    IItemObject* const itemObject = static_cast<IItemObject*>(item->queryInterface(IID_IItemObject));
    itemObject->handlePickup(getOccupingPlayer(), this);
    return itemObject->getItemIndex();
}

bool Soldier::addItem(IObject* item, int index)
{
    if (!item)
        return false;

    IItemObject* const itemFace =
        static_cast<IItemObject*>(item->queryInterface(IID_IItemObject));
    if (!itemFace)
    {
        Debug(DEBUGTYPE_6, "Game/Objects/Game/Soldier.cpp", 2453)
            << "TBM: addItem(), not an item: " << item->getTemplate()->getName();
        return false;
    }

    int itemIndex = index;
    if (index == -1)
        itemIndex = itemFace->getItemIndex();                       // IItemObject +0x30

    if (itemIndex <= 0 || itemIndex >= static_cast<int>(m_itemObjects.size()))
    {
        Debug(DEBUGTYPE_6, "Game/Objects/Game/Soldier.cpp", 2462)
            << "illegal item index, item->getItemIndex() = " << itemIndex;
        return false;
    }
    if (m_itemObjects[itemIndex])
    {
        Debug(DEBUGTYPE_6, "Game/Objects/Game/Soldier.cpp", 2469)
            << "TBM: force remove item! check that the Kits are correct!!";
        return false;
    }

    m_itemObjects[itemIndex] = item;
    // A soldier whose own physics is off hands that state to everything it picks up, so an item
    // taken by a soldier in a vehicle does not start simulating in the world.
    if (isPhysicsDisabled(this))
        disablePhysics(item, true);
    if (item->queryInterface(IID_IWeaponObject))
        addWeapon(item);                                            // +0x1C0
    item->updateFlags(0, 256);
    ::dice::hfe::world::updateFlagsRecursive(item, 0, 0x100u);      // qualified - see Kit.cpp
    return true;
}

// Draws the item in a positive slot, returning whether it accepted activation.
// A refusal leaves the previous item active; ragdolls always refuse.
bool Soldier::enableItem(char index)
{
    if (m_ragDoll)                                                  // +0x220
        return false;
    if (index == -1)
        return true;

    const int previousActive = m_activeItemIndex;
    if (index == m_activeItemIndex)
        return true;

    if (index <= 0 || index >= static_cast<int>(m_itemObjects.size()))
    {
        Debug(DEBUGTYPE_6, "Game/Objects/Game/Soldier.cpp", 2615)
            << "enableItem(), wrong index:" << static_cast<int>(index);
    }
    else if (IObject* const item = m_itemObjects[index])
    {
        IItemObject* const itemFace =
            static_cast<IItemObject*>(item->queryInterface(IID_IItemObject));
        if (!itemFace->enableItem())                                // +0x28 unchecked query
            return false;
        if (m_activeItemIndex != -1)
            disableItem(static_cast<char>(m_activeItemIndex));      // +0x294
        m_activeItemIndex = index;
    }

    // Whatever ended up active - only when it actually changed - is un-disabled and re-parented
    // onto the soldier if it is not already a child of something.
    if (m_activeItemIndex != -1 && previousActive != m_activeItemIndex)
    {
        IObject* const active = getItem(m_activeItemIndex);         // +0x25C
        if (!active)
            return true;
        if (!active->getParent())                                   // +0x28
        {
            active->updateFlags(0, 1);                              // +0x60 clear the disable bit
            addChild(active);
        }
    }
    return true;
}

// Stows and detaches the item in a positive slot.
// Negative indices do nothing; zero and out-of-range indices report an error.
void Soldier::disableItem(char index)
{
    if (index < 0)
        return;                                                      // silent, unlike index 0

    if (index > 0 && index < static_cast<int>(m_itemObjects.size()))
    {
        if (IObject* const item = m_itemObjects[index])
        {
            if (item->getParent())                                   // +0x28
                removeChild(m_itemObjects[index]);                   // re-read, as compiled
            static_cast<IItemObject*>(item->queryInterface(IID_IItemObject))
                ->disableItem();                                     // +0x2C unchecked, as shipped
        }
        return;
    }

    Debug(DEBUGTYPE_6, "Game/Objects/Game/Soldier.cpp", 2656)
        << "disableItem(), wrong index:" << static_cast<int>(index);
}

// Soldier::removeWeapon(IObject*) - 0x81AE060, 200 B. Remove the first flat-list occurrence,
// clear all three active slots unconditionally, then remove the first occurrence from each
// fire-button bucket. The broad active-slot clear is retail behavior, even when another weapon
// remains in that bucket.
void Soldier::removeWeapon(IObject* weapon)
{
    std::vector<IObject*>::iterator flat =
        std::find(m_weaponVec.begin(), m_weaponVec.end(), weapon);
    if (flat != m_weaponVec.end())
        m_weaponVec.erase(flat);

    for (int slot = 0; slot <= 2; ++slot)
    {
        m_activeWeapons[slot] = 0;
        std::vector<IObject*>::iterator found =
            std::find(m_weapons[slot].begin(), m_weapons[slot].end(), weapon);
        if (found != m_weapons[slot].end())
            m_weapons[slot].erase(found);
    }
}

// Returns the collision skeleton after evaluating a temporary third-person pose.
// Pose values 0..3 override the live pose during evaluation; other values preserve it.
// Restores the live pose, character mode and held item's animation selection before returning.
anim::Skeleton* Soldier::getCollisionSkeleton(int pose)
{
    const int savedPose = m_pose;
    m_byte27F[1] = true;
    if (static_cast<unsigned int>(pose) < 4u)
        m_pose = pose;

    const int savedCharacterMode = m_characterMode;
    if (savedCharacterMode != 1)
    {
        m_characterMode = 1;
        updateAnimationSystems(0.033333335f);
    }

    anim::Skeleton* const skeleton = m_collisionSkeleton;
    if (gameLogic && gameLogic->isServer()
        && (m_dword26C != static_cast<int>(WorldPref::getTickCount())
            || m_dword270 != m_pose))
    {
        applyAnimationsOnSkeleton(0.033333335f, false);
    }

    m_characterMode = savedCharacterMode;
    m_pose = savedPose;

    IObject* const item = getItem(m_activeItemIndex);
    if (item && (item->getFlags() & 1u) == 0)
    {
        IAnimatedBundle* const animated = static_cast<IAnimatedBundle*>(
            item->queryInterface(IID_IAnimatedBundle));
        if (animated)
            animated->setActiveAnimationSystem(m_characterMode);
    }

    m_byte27F[1] = false;
    return skeleton;
}

// Soldier::getSkeletonRayTestPoint - ELF 0x81AF440, 316 B / PE 0x54E890. Both ports return a
// Vec3 by value. The nested enum selects one of three collision-skeleton bones (47, 11, 3); any
// out-of-range value deliberately falls back to bone 11. The bone's absolute skeleton translation
// is transformed through the soldier's absolute matrix. A missing collision skeleton falls back
// to the soldier's absolute position.
Vec3 Soldier::getSkeletonRayTestPoint(SkeletonRayTestPoint point)
{
    anim::Skeleton* const skeleton = getCollisionSkeleton(getPose());
    if (!skeleton)
        return getAbsolutePosition();

    int boneIndex = 11;
    if (point == SkeletonRayTestPointHead)
        boneIndex = 47;
    else if (point == SkeletonRayTestPointFeet)
        boneIndex = 3;

    const Mat4& bone = skeleton->m_transforms[boneIndex];
    const float x = bone.m[12];
    const float y = bone.m[13];
    const float z = bone.m[14];
    const Mat4& absolute = getAbsoluteTransformation();

    Vec3 result;
    result.x = absolute.m[0] * x + absolute.m[4] * y + absolute.m[8]  * z + absolute.m[12];
    result.y = absolute.m[1] * x + absolute.m[5] * y + absolute.m[9]  * z + absolute.m[13];
    result.z = absolute.m[2] * x + absolute.m[6] * y + absolute.m[10] * z + absolute.m[14];
    return result;
}

// Registers a weapon and groups it by primary, alternate or flare input.
// During kit pickup, a matching primary weapon becomes the default fallback weapon.
void Soldier::addWeapon(IObject* weapon)
{
    m_weaponVec.push_back(weapon);                                   // +0x160 and it is NOT gated

    IWeaponObject* const weaponObject = weapon
        ? static_cast<IWeaponObject*>(weapon->queryInterface(IID_IWeaponObject)) : 0;
    if (!weaponObject)
        return;

    // The shipped code deliberately re-fetches this virtual for every comparison, and once more
    // for the fatal diagnostic. Both ELF 0x81AE130 and PE 0x552480 preserve all four calls.
    if (weaponObjectGetFireInput(weaponObject) == io::PIFire)         // +0x38 fetch #1
    {
        m_weapons[0].push_back(weapon);
        if (!m_kitBeingPickedUp)                                     // +0x1C4
            return;
        if (!getOccupingPlayer())
        {
            m_weapon2 = weapon;
            return;
        }
        IPlayer* const player = getOccupingPlayer();
        const int kit  = player->getKit();                           // +0x170
        const int team = getOccupingPlayer()->getTeam();             // +0xE8 re-fetched, as compiled
        IObjectTemplate* const kitTemplate = gameLogic->getKitTemplate(team, kit);   // +0x144
        IItemTemplate* const itemTemplate = static_cast<IItemTemplate*>(
            weapon->getTemplate()->queryInterface(IID_IItemTemplate));
        if (kitTemplate
            && itemTemplate->getItemIndex()                          // +0x14 unchecked, as shipped
                   == static_cast<const KitTemplate*>(kitTemplate)->m_defaultWeaponIndex)
            m_weapon2 = weapon;
        return;
    }
    if (weaponObjectGetFireInput(weaponObject) == io::PIAltFire)     // fetch #2
    {
        m_weapons[1].push_back(weapon);
        return;
    }
    if (weaponObjectGetFireInput(weaponObject) == io::PIFlareFire)   // fetch #3
    {
        m_weapons[2].push_back(weapon);
        return;
    }
    Debug(DEBUGTYPE_6, "Game/Objects/Game/Soldier.cpp", 3323)
        << "Unknown weapon fire button for weapon, button : "
        << weaponObjectGetFireInput(weaponObject);                    // fetch #4
}

// Selects a weapon in a firing slot after reconciling skydiving and item activation.
// Notifies scripts with (player, previous weapon, new weapon); device weapons start their deploy animation.
void Soldier::setActiveWeapon(IObject* weapon, int slot)
{
    if (!weapon)
        return;
    if (m_ragDoll)
        return;

    IItemObject* const weaponFace =
        static_cast<IItemObject*>(weapon->queryInterface(IID_IItemObject));
    if (!weaponFace)
        return;
    const int itemIndex = weaponFace->getItemIndex();               // +0x30

    if (itemIndex > 8 || !getActiveWeapon(0) || hasHandWeaponActive())
    {
        if (!m_byte315)
            setIsSkyDiving(false);
    }
    else if (!m_byte315)
    {
        return;
    }

    for (unsigned int i = 0; i < m_weapons[slot].size(); ++i)
    {
        if (m_weapons[slot][i] != weapon)
            continue;

        // Do not reduce this to m_isSelectable: live Fire/Ammo components add two more retail
        // gates. The direct call is the shipped GenericFireArm primary-vtable dispatch.
        if (!static_cast<GenericFireArm*>(weapon)->isSelectable() && gameLogic->isServer())
            return;

        const GenericFireArmTemplate* const weaponTemplate =
            static_cast<const GenericFireArmTemplate*>(weapon->getTemplate());

        if (!enableItem(static_cast<char>(itemIndex)))              // +0x290
            return;

        if (slot == 0 && !m_deployAnimationActive)
            m_weapon1 = m_activeWeapons[0];                         // the previous-weapon memory

        if (gameLogic->isServer() && getOccupingPlayer())
        {
            IPlayer* const player = getOccupingPlayer();
            python::PyRef args = python::buildArgs(player, m_activeWeapons[slot], weapon);
            python::pythonHost()->handleEvent(static_cast<ScriptEvent>(6), args.get());   // PlayerChangeWeapon
        }

        const bool deviceAllowed = !m_deployAnimationActive;        // read before the store
        m_activeWeapons[slot] = weapon;

        if (deviceAllowed)
        {
            if (weaponTemplate->m_isGasMask && !getOccupingPlayer()->getIsGasMaskOn())   // +0x2C0
            {
                m_deployAnimationActive = true;
                m_deployAnimationTimer = weaponTemplate->m_deployAnimationTime;
                const bool on = !getOccupingPlayer()->getIsGasMaskOn();   // re-queried, as compiled
                getOccupingPlayer()->setIsGasMaskOn(on, false);           // +0x2C4
                getOccupingPlayer()->setNightVision(false, false);        // +0x2DC the other one off
                m_byte384 = false;
            }
            if (weaponTemplate->m_isNightVision && !getOccupingPlayer()->getNightVision())  // +0x2E8
            {
                m_deployAnimationActive = true;
                m_deployAnimationTimer = weaponTemplate->m_deployAnimationTime;
                const bool on = !getOccupingPlayer()->getNightVision();
                getOccupingPlayer()->setNightVision(on, false);
                getOccupingPlayer()->setIsGasMaskOn(false, false);
                m_byte384 = false;
            }
        }

        if (playerManager->getLocalHumanPlayer() == getOccupingPlayer())
        {
            int payload[2];
            payload[0] = itemIndex;
            payload[1] = slot;
            eventManager->postEvent(static_cast<EventCategory>(2), 31, this, 0.0f);
            eventManager->postEvent(static_cast<EventCategory>(2), 32, payload, 0.0f);
        }
        return;
    }
}

// Transfers the kit's items to this soldier and draws the first available slot from 3 upward.
// Falls back to a slot below 3 when necessary. Returns true; invalid kit or active-item state aborts.
bool Soldier::pickupKit(IPlayer* player, IObject* kit)
{
    if (m_kit)
    {
        dropKit();
        if (m_kit)
            std::abort();                                       // as shipped
    }
    if (!kit)
        std::abort();                                           // as shipped

    m_kitBeingPickedUp = kit;                                   // +0x1C4 the re-entrancy latch
    IItemObject* const item =
        static_cast<IItemObject*>(kit->queryInterface(IID_IItemObject));
    item->handlePickup(player, this);                           // +0x10 unchecked, as shipped
    if (m_activeItemIndex != -1)
        std::abort();                                           // as shipped

    bool drawn = false;
    for (int i = 3; i < static_cast<int>(m_itemObjects.size()); ++i)
    {
        if (!m_itemObjects[i])
            continue;
        enableItem(static_cast<char>(i));                       // +0x290
        IObject* const drawnItem = m_itemObjects[i];            // re-read, as compiled
        if (drawnItem && drawnItem->queryInterface(IID_IWeaponObject))
            setActiveWeapon(m_itemObjects[i], 0);               // +0x1C8
        drawn = true;
        break;
    }
    if (!drawn)
    {
        // the limit is min(3, size) - slots 0, 1 and 2 only, never 3.
        const int size = static_cast<int>(m_itemObjects.size());
        const int limit = size < 4 ? size : 3;
        for (int i = 0; i < limit; ++i)
        {
            if (m_itemObjects[i])
            {
                enableItem(static_cast<char>(i));
                break;                                          // no weapon test on this arm
            }
        }
    }

    m_kitBeingPickedUp = 0;
    m_kit = static_cast<Kit*>(kit);
    m_currentAbility = m_kit->getCurrentAbility();              // +0x2CC <- the kit's meter

    // gated on the AI object existing, not on aiConnectionIsUp() as the other AI posts in this
    // file are. Category 3 is the AI category; 5 is "kit picked up".
    if (void* const aiObject = object_detail::LayoutAccess::aiObject(this))   // +0x34
        eventManager->postEvent(static_cast<EventCategory>(3), 5, aiObject, 0.0f);   // +0x14

    // Bind this soldier's collision skeleton to the kit's geometry, then slot +0x7C with 0.
    // unreachable in this build - the kit's Object +0x38 is null everywhere (see the ghost hooks).
    if (void* const geometry = object_detail::LayoutAccess::geometry(m_kit))  // +0x38
    {
        geometrySetBoneTransforms(geometry, m_collisionSkeleton->m_transforms,
                                  m_collisionSkeleton->m_boneCount);          // slot +0x58
        geometrySlot7C(geometry, 0);                                          // slot +0x7C
    }
    m_kit->setGeomIndex(-1);                                    // -1 = the template default
    return true;
}

// Drops the kit and its items, notifying scripts and placing it on nearby geometry or terrain.
// Returns false with no kit or unsuitable placement. Requires item callbacks to clear their parents.
bool Soldier::dropKit()
{
    typedef object_detail::LayoutAccess L;

    if (!m_kit)                                                              // +0x1C0
        return false;
    bool result = true;

    IPlayer* const player = getOccupingPlayer();                             // +0x180
    if (player)
    {
        IPlayer* const local = playerManager->getLocalHumanPlayer();         // +0x30
        if (local == player && !local->getIsAIPlayer())                      // +0x64 on the local pointer
        {
            int playerId = player->getId();                                  // +0x54
            eventManager->postEvent(ECHud, 69, &playerId, 0.0f);             // +0x14
        }
    }

    if (gameLogic->isServer())                                               // +0x13C
    {
        IObject* const kitObject = m_kit;
        python::PyRef args = python::buildArgs(player, kitObject);           // <IPlayer*, IObject*>
        python::pythonHost()->handleEvent(static_cast<ScriptEvent>(5), args.get());
    }

    IItemObject* const kitItem =
        m_kit ? static_cast<IItemObject*>(m_kit->queryInterface(IID_IItemObject)) : 0;
    if (!kitItem)
        std::abort();                                                        // as shipped

    resetWeaponAnimations();
    m_kit->setCurrentAbility(m_currentAbility);                              // +0x2CC
    m_currentAbility = -1.0f;
    if (m_activeItemIndex != -1)                                             // +0x1B8
        disableItem(static_cast<char>(m_activeItemIndex));                   // +0x294 virtual

    for (unsigned int i = 0; i < m_itemObjects.size(); ++i)                  // +0x1C8
    {
        IObject* const item = m_itemObjects[i];
        if (!item)
            continue;
        IItemObject* const itemFace =
            static_cast<IItemObject*>(item->queryInterface(IID_IItemObject));   // unchecked
        itemFace->handleDrop(getOccupingPlayer(), this);                     // +0x14
        itemFace->disableItem();                                             // +0x2C
        if (L::parent(item))                                                 // +0x28
            std::abort();                                                    // as shipped
        m_kit->addChild(item);                                               // +0xF4
    }

    Kit* const kit = m_kit;
    m_kit = 0;
    kitItem->handleDrop(getOccupingPlayer(), this);                          // +0x14

    Mat4 transform = getAbsoluteTransformation();                            // +0x84 copied
    IObject* hitObject = 0;
    Vec3 hitPoint;  hitPoint.x  = hitPoint.y  = hitPoint.z  = 0.0f;
    Vec3 hitNormal; hitNormal.x = hitNormal.y = hitNormal.z = 0.0f;
    int hitPart;
    Vec3 from; from.x = transform.m[12]; from.y = transform.m[13]; from.z = transform.m[14];
    Vec3 to;   to.x = 0.0f; to.y = -1000.0f; to.z = 0.0f;
    ProjectileVsTargetPredicator predicator(0x100u, 8u, this, 0);
    if (objectManager->intersectLine(hitObject, hitPoint, hitNormal, hitPart, from, to,
                                     &predicator, true, false, false, true, 0))   // +0x68
    {
        kit->setAbsolutePosition(hitPoint);                                  // +0x70
        if (L::objectTemplate(hitObject)->getClassID() == CID_LadderTemplate)
            result = false;
    }
    else
    {
        transform.m[13] = reinterpret_cast<IHeightmapCluster*>(heightmapCluster)
                              ->getHeightInWorldCoords(transform.m[12], transform.m[14]);   // +0x6C
        kit->setAbsoluteTransformation(transform);                           // +0x8C
    }

    m_activeItemIndex = -1;                                                  // +0x1B8
    m_weapon2 = 0;                                                           // +0x1A0
    m_weapon1 = 0;                                                           // +0x19C
    if (!m_weaponVec.empty())                                                // +0x160
        std::abort();                                                        // as shipped
    for (int i = 0; i <= 2; ++i)
    {
        if (!m_weapons[i].empty())                                           // +0x16C + 12 i
            std::abort();                                                    // as shipped
        if (m_activeWeapons[i])                                              // +0x190 + 4 i
            std::abort();                                                    // as shipped
    }
    return result;
}

// Stores the manager's template result without a null check.
void SoldierTemplate::animationSystem1P(const std::string& name)
{
    m_animationSystem1P = animationSystemManagerGet(name);
}

void SoldierTemplate::animationSystem3P(const std::string& name)
{
    m_animationSystem3P = animationSystemManagerGet(name);
}

// Loads the first-person skeleton and caches the inverse bind transform of its root bone.
// A failed load still stores the returned skeleton; the parser's loaded flag is unused.
void SoldierTemplate::skeleton1P(const std::string& name)
{
    bool loaded = false;
    anim::Skeleton* skeleton = new anim::Skeleton(name, &loaded, 0);   // `loaded` is never read
    m_skeleton1P = skeleton;

    // cache THE bind-pose inverse: the inverse of the first bone's transform.
    m_bone0InverseTransform.setInverse(skeleton->m_bones[0].m_transform);
}

void SoldierTemplate::skeleton3P(const std::string& name)
{
    bool loaded = false;
    m_skeleton3P = new anim::Skeleton(name, &loaded, 0);               // likewise
}

// Creates owned first- and third-person skeleton copies and animation-system instances.
// Uses the third-person rig for collision. Requires all four rig fields in the soldier template.
void Soldier::initAnimation()
{
    m_ragDoll = 0;
    m_hitByVehicleSpeedTicks = 0;
    m_helmetTime = 0.0f;

    m_explosionForce.x = 0.0f;
    m_explosionForce.y = 0.0f;
    m_explosionForce.z = 0.0f;
    m_explosionForceTime = 0.0f;

    m_byte27C = false;
    m_byte27D = false;
    m_float230 = 0.0f;

    // ALL three zero here, where resetHeadMovements puts 1.0f in x.
    m_headPosition.x = 0.0f;
    m_headPosition.y = 0.0f;
    m_headPosition.z = 0.0f;
    m_soldierLookIdleTime = g_soldierLookIdleTimeSource;

    m_inverseKinematicDisabled = false;
    m_byte27F[0] = false;                        // +0x27F
    m_byte27F[1] = false;                        // +0x280

    // the first-person rig
    anim::Skeleton* skeleton1P = new anim::Skeleton();
    m_animSkeleton = skeleton1P;
    *skeleton1P = *m_soldierTemplate->getSkeleton1P();

    // the third-person rig, which is also the collision skeleton
    anim::Skeleton* skeleton3P = new anim::Skeleton();
    m_collisionSkeleton = skeleton3P;
    *skeleton3P = *m_soldierTemplate->getSkeleton3P();

    m_animationSystem1P =
        animationSystemCreateInstance(m_soldierTemplate->getAnimationSystem1P());    // +0x258
    m_animationSystem3P =
        animationSystemCreateInstance(m_soldierTemplate->getAnimationSystem3P());    // +0x260

    m_characterMode = 1;
}

// getPlayingAnimationInfo - ELF 0x81B6470, 496 B / PE 0x5BDA10, 385 B. Select the soldier's
// first/third-person animation system, allowing an occupied vehicle or active item to replace it.
// A negative animation length falls back to a 0.9-second traversal cycle for ladders, grappling
// hooks and ziplines; otherwise both outputs are reset to zero.
void Soldier::getPlayingAnimationInfo(float& length, float& time)
{
    time = 0.0f;
    length = 0.0f;
    if (m_ragDoll)
        return;

    // The recovered mode is 0 (first person) or 1 (third person); the retail pair stride is
    // two pointers, not eight native bytes.
    anim::IAnimationSystem* system = m_characterMode == 0 ? m_animationSystem1P : m_animationSystem3P;

    if (m_characterMode == 1)
    {
        const WeakPtr<IObject>::Ctrl* const weak = static_cast<const WeakPtr<IObject>::Ctrl*>(m_usedVehicleWeak);
        IObject* const used = weak ? weak->target : 0;
        if (used)
        {
            IObject* const usedAgain = weak ? weak->target : 0;
            IAnimatedBundle* animated = static_cast<IAnimatedBundle*>(
                usedAgain->queryInterface(IID_IAnimatedBundle));
            if (animated)
            {
                animated->setActiveAnimationSystem(m_characterMode);
                system = static_cast<anim::IAnimationSystem*>(
                    animated->getActiveAnimationSystem());
            }
        }
    }

    if (m_characterMode == 0)
    {
        IObject* const item = getItem(m_activeItemIndex);
        if (item && (item->getFlags() & 1u) == 0)
        {
            IAnimatedBundle* animated = static_cast<IAnimatedBundle*>(
                item->queryInterface(IID_IAnimatedBundle));
            if (animated)
            {
                animated->setActiveAnimationSystem(m_characterMode);
                system = static_cast<anim::IAnimationSystem*>(
                    animated->getActiveAnimationSystem());
                if (length < 0.0f)
                {
                    system = m_characterMode == 0 ? m_animationSystem1P : m_animationSystem3P;
                }
            }
        }
    }

    if (system)
        system->getPlayingAnimationLength(length, time, true);

    if (length < 0.0f)
    {
        if (getLadderId() || getGrapplingHookId() || getZiplineId())
        {
            length = 0.9f;
            time = ::fmodf(static_cast<float>(g_system->getExactTime()), length);
        }
        else
        {
            time = 0.0f;
            length = 0.0f;
        }
    }
}

// . A NO-OP when THE mode IS unchanged, and a full skeleton re-apply when it is not.
void Soldier::setCharacterMode(int mode)
{
    if (mode == m_characterMode)
        return;
    m_characterMode = mode;
    applyAnimationsOnSkeleton(0.0f, true);                   // [FRONTIER]
}

// Returns the soldier-template implementation for its class id, otherwise delegates to BundleTemplate.
void* SoldierTemplate::queryImplementation(unsigned int classId)
{
    if (classId == CID_SoldierTemplate)
        return this;
    return BundleTemplate::queryImplementation(classId);
}

// Identifies this template as a soldier for gameplay and serialization dispatch.
unsigned int SoldierTemplate::getClassID() const
{
    return CID_SoldierTemplate;
}

IObject* SoldierTemplate::createObject() const
{
    return new Soldier(this);
}

// Rearms the alternate-fire latch for a man-down player receiving a fire message.
// Primary fire and other messages have no effect.
void Soldier::handleManDownActions(TemplateMessage message, IPlayer* player)
{
    if (static_cast<int>(message) != 6)
        return;

    switch (player->getCurrentFiringButton())     // IPlayer vslot ELF +0x208 / PE +0x204
    {
    case io::PIFire:                              // 8 - explicit and empty; see the note above
        break;

    case io::PIAltFire:                           // 31
        m_fireButtonState[1].translateMessage(message);   // ELF this+0x2F8 / PE this+0x330
        break;
    }
}

// Starts camera, sound, and tinnitus effects only for the local human occupant.
// [UNRESOLVED] camera-mode name; mode 1 is used.
void Soldier::startManDownEffect()
{
    if (getOccupingPlayer() != playerManager->getLocalHumanPlayer())         // +0x180 +0x30
        return;
    ICameraObject* const camera = getOccupingPlayer()->getCamera();           // IPlayer +0x180
    if (camera)
        camera->setViewMode(static_cast<CameraViewMode>(1), 0, false);        // +0x18
    g_bfSound->startManDown();                                                // +0x80
    eventManager->postEvent(ECGameLogic, 13, g_bfSound->getTinnitusSetup(), 0.0f);   // +0x74 > +0x14
}

// Stops the local occupant's man-down effect and refreshes client nametag tracking.
void Soldier::stopManDownEffect()
{
    if (getOccupingPlayer() == playerManager->getLocalHumanPlayer())         // +0x180 +0x30
    {
        g_bfSound->stopManDown();                                             // +0x84
        eventManager->postEvent(ECGameLogic, 13, g_bfSound->getTinnitusSetup(), 0.0f);
    }
    if (!isDedicated())
    {
        INametagManager* const nametags = SingletonServer::m_iNametagManager; // ds:9A5970
        if (nametags)
            nametags->addTouchItem(getOccupingPlayer());                      // PE +0x30
    }
}

// Replaces the soldier's voice channel with the requested speech sample.
// Stops and detaches the previous sound before releasing it; a null sound silences the channel.
void Soldier::say(ISound* sound, int sample)
{
    if (m_voiceSound)
    {
        m_voiceSound->stop();
        m_voiceSound->setParent(0);
        m_voiceSound->release();
        m_voiceSound = 0;
    }

    if (!sound)
        return;

    m_voiceSound = sound;
    sound->addRef();
    m_voiceSound->setParent(m_soldierSound);
    m_voiceSound->playSample(sample, 1);
}


// Retains the new weak block before releasing the old one, allowing self-assignment.
void Soldier::setOccupyingPlayerWeak(void* block)
{
    if (block)
        ++*static_cast<int*>(block);

    if (m_occupyingPlayerWeak)
    {
        int* count = static_cast<int*>(m_occupyingPlayerWeak);
        if ((*count)-- == 1)
            operator delete(m_occupyingPlayerWeak);
    }
    m_occupyingPlayerWeak = block;
}

// Attempts to assign player to this soldier. Returns false for null or rejected players.
// A rejected assignment rolls back occupancy before other state changes.
bool Soldier::enter(IPlayer* player)
{
    if (!player)
        return false;

    // 1. claim the player. The weak block is refcounted by hand - addRef the new one, release the
    // old one, and release the temporary. Three counted operations for one assignment.
    // getWeakPtr hands back a counted handle; the field holds its block. The temporary the
    // ELF builds is released immediately after the store, which the helper's add-then-release
    // order already accounts for.
    setOccupyingPlayerWeak(player->getWeakPtr().m_ctrl);
    m_isOccupied = true;
    m_weapon1 = 0;

    // 2. ask the player to accept us. A soldier is a PCO, so this is the vehicle call.
    if (!player->setVehicle(this, getPcoId()))
    {
        setOccupyingPlayerWeak(0);          // the rollback
        m_isOccupied = false;
        return false;
    }

    // 3..5. flags, team, and hide the kit's geometry.
    updateFlags(2u, 0u);
    m_team = getOccupingPlayer()->getTeam();
    if (m_kit)
        m_kit->setGeomIndex(-1);

    // 6. the facing angle - see the block above.
    const Mat4& t = getAbsoluteTransformation();
    Vec3 forward;
    forward.x = t.m[8];       // row 2, x - the forward axis
    forward.y = 0.0f;
    forward.z = t.m[10];      // row 2, z

    const float lenSq = forward.x * forward.x + forward.z * forward.z;
    if (fabsf(lenSq - 1.0f) >= 1.1920929e-7f)      // already unit? leave it alone
    {
        if (fabsf(lenSq) >= 1.1920929e-7f)
        {
            const float inv = 1.0f / sqrtf(lenSq);
            forward.x *= inv;
            forward.y *= inv;
            forward.z *= inv;
        }
        else
        {
            forward.x = 0.0f;
            forward.z = 0.0f;
        }
    }

    float dot = forward.z;                          // dotted against (0, 0, 1)
    if (dot < -1.0f) dot = -1.0f;
    else if (dot > 1.0f) dot = 1.0f;

    float degrees = acosf(dot) * 57.295776f;        // not the same constant getSpawnRotation uses
    if (forward.x <= 0.0f)
        degrees = -degrees;
    m_facingDegrees = degrees;

    // 7. tell the AI, if there is one listening.
    if (aiConnectionIsUp() && getOccupingPlayer())
    {
        SoldierAiSeatPayload payload;
        payload.aiObject = object_detail::LayoutAccess::aiObject(this);   // +0x34
        payload.otherAiObject = 0;
        payload.team = getOccupingPlayer()->getTeam();
        payload.id = getOccupingPlayer()->getId();
        payload.trailing = 0;                       // a trailing byte in the ELF
        eventManager->postEvent(static_cast<EventCategory>(3), 1, &payload, 0.0f);
    }

    // 8. reset the per-body input latches. Backwards, 1 then 0 - dice's.
    for (int i = 1; i >= 0; --i)
        m_fireButtonState[i].reset();

    // THE argument order IS THE ELF'S: +0x1E4 first, then +0x1E0, then +0x1E8 - not the
    // template's declaration order. Named accessors so the transposition is visible.
    m_sprintState.setConstants(m_soldierTemplate->getSprintDissipationTime(),
                               m_soldierTemplate->getSprintRecoverTime(),
                               m_soldierTemplate->getSprintLimit());
    m_sprintState.reset();
    return true;
}


// Handles death, revive, fire, sprint and landing messages and forwards applicable input to the held item.
// Disabled soldiers accept only death and revive; man-down fire uses its separate handler.
void Soldier::handleMessage(TemplateMessage message, IPlayer* player, bool remote)
{
    const int m = static_cast<int>(message);

    // THE gate - see above.
    if ((getFlags() & 1u) != 0 && m != 25 && m != 24)
        return;

    ArmorGLComp* armor = objectArmorComp(this);
    // status 1 = destroyed. See the corrected note above.
    bool destroyed = false;
    if (armor && armor->getStatus() == 1)
        destroyed = m_isOccupied;

    bool forwardToItem = false;

    switch (m)
    {
    case 6:                                       // fire
    case 10:
        if (destroyed)
        {
            if (!remote)
                handleManDownActions(message, player);
        }
        else if (!getIsSprinting() || m != 6)     // sprint+fire is dropped
        {
            forwardToItem = true;
        }
        break;

    case 24:                                      // die
    {
        if (!armor)
            abort();                              // dice's - the component is not optional here

        if (!remote && gameLogic->getGameStatus() == 1)
            m_soldierSound->die(armor->getLastDamageType() == 1);

        IPlayer* occupant = getOccupingPlayer();
        if (occupant && occupant->getVehicle() == occupant->getDefaultVehicle())
        {
            if (destroyed)
            {
                startManDownEffect();
                occupant->setIsManDown(true);
                // seconds TO ticks: the setting is multiplied by 30 and added to the current tick.
                occupant->setSpawnAtTick(
                    static_cast<int>(serverSettings->getManDownTime() * 30.0f
                                     + WorldPref::getTickCount()));
                if (aiConnectionIsUp())
                {
                    SoldierAiPlayerPayload payload;
                    payload.aiObject = object_detail::LayoutAccess::aiObject(this);   // +0x34
                    payload.playerId = getOccupingPlayer()->getId();
                    eventManager->postEvent(static_cast<EventCategory>(3), 6, &payload, 0.0f);
                }
            }

            // trace THE killer - but never yourself.
            IPlayer* killer = armor->getLastHitPlayer();

            if (killer && killer->getIsAlive() && occupant != killer)
                occupant->setTracePlayerId(armor->getLastHitPlayer()->getId());

            // call FOR A medic, once per death, and only for an AI body on the server or for the
            // local human player.
            if (destroyed && !m_medicCalled
                && ((gameLogic->isServer() && occupant->getIsAIPlayer())
                    || occupant == playerManager->getLocalHumanPlayer()))
            {
                gameCallForSpecialist(occupant, static_cast<HudNodeTypes>(50));
                ++m_medicCalled;
                playerManager->getLocalHumanPlayer();   // discarded - a leftover, reproduced
            }
        }
        m_isManDown = 1;
        break;
    }

    case 25:                                      // revive
        stopManDownEffect();
        if (!remote)
            m_soldierSound->revive();
        m_isManDown = 0;
        reviveAnimations();
        m_medicCalled = 0;
        m_timeToEnableWeapon = g_soldierTimeToEnableWeapon;      // ds:0x8AAD7AC
        if (gameLogic->isServer())
        {
            // TEN metres: a revived soldier grabs the nearest kit, or is issued a fresh one.
            // THE same object AS `gameLogic` - the ELF calls both through ds:0x8ACBFA0.
            ServerGameLogic* sgl = static_cast<ServerGameLogic*>(gameLogic);
            if (!sgl->pickUpClosestKit(getOccupingPlayer(), this, 10.0f))
                sgl->getNewKit(getOccupingPlayer());

            if (getOccupingPlayer() && aiConnectionIsUp())
            {
                SoldierAiPlayerPayload payload;
                payload.aiObject = object_detail::LayoutAccess::aiObject(this);   // +0x34
                payload.playerId = getOccupingPlayer()->getId();
                eventManager->postEvent(static_cast<EventCategory>(3), 7, &payload, 0.0f);
            }
        }
        break;

    case 28:
        forwardToItem = true;
        break;

    case 41:                                      // sprint on
    case 42:                                      // sprint off
        m_sprintState.handleTMSprint(m == 41);
        break;

    case 54:                                      // landed
        if (!m_byte27F[1] && !remote)             // +0x280
            m_soldierSound->trigger3PLanding();
        break;

    default:
        Bundle::handleMessage(message, player, remote);
        break;
    }

    // THE double veto - see above.
    if (m_ragDoll || !hasHandWeaponActive())
        forwardToItem = false;
    if (forwardToItem)
        sendMessageToItem(m_activeItemIndex, message, player, remote);
}


// Returns the active weapon's yaw recoil, or zero with no active item or disabled recoil.
// An active item must implement IWeaponObject.
float Soldier::getYawRecoil()
{
    if (m_activeItemIndex == -1)
        return 0.0f;
    IObject* item = getItem(m_activeItemIndex);
    if (!item)
        return 0.0f;
    void* weapon = item->queryInterface(IID_IWeaponObject);     // NOT null-checked below
    if (!weaponHasRecoil(weapon))                               // +0xDC
        return 0.0f;
    return weaponGetYawRecoil(weapon);                          // +0xFC
}

float Soldier::getPitchRecoil()
{
    if (m_activeItemIndex == -1)
        return 0.0f;
    IObject* item = getItem(m_activeItemIndex);
    if (!item)
        return 0.0f;
    void* weapon = item->queryInterface(IID_IWeaponObject);     // same missing check
    if (!weaponHasRecoil(weapon))                               // +0xDC
        return 0.0f;
    return weaponGetPitchRecoil(weapon);                        // +0xF8
}

// Returns the configured movement speed for speedState; 7 selects the current state.
// Requires a valid table index. An occupied soldier in pose 1 also applies the mouse-speed modifier.
float Soldier::getSoldierSpeed(int speedState) const
{
    const int index = (speedState == 7) ? m_soldierSpeedIndex : speedState;
    float speed = g_soldierSpeeds[index];                       // ds:0x8AAD9E4  unbounded
    if (m_pose == 1 && const_cast<Soldier*>(this)->getOccupingPlayer())
        speed *= const_cast<Soldier*>(this)->getMouseSpeedMod();
    return speed;
}


// Returns whether the active primary weapon is zoomed, selecting walking movement speed.
// An active weapon must implement IWeaponObject.
bool Soldier::isWalking() const
{
    Soldier* self = const_cast<Soldier*>(this);
    if (!self->getActiveWeapon(0))
        return false;

    IObject* weapon = self->getActiveWeapon(0);              // asked a second time
    void* iw = 0;
    if (weapon)
        iw = weapon->queryInterface(IID_IWeaponObject);
    return weaponIsZoomed(iw);                               // iw NOT null-checked - +0x108
}

// Selects one of seven speed states from pose, sprint and weapon zoom.
// Aborts for poses outside 0..3; sprint takes precedence over zoom.
void Soldier::updateSpeedState()
{
    int index;

    switch (m_pose)                                          // +0x1FC
    {
    case 0:
        if (getIsSprinting())                                // sprint is tested before zoom
            index = 4;
        else
            index = isWalking() ? 2 : 3;
        break;

    case 1:
        index = 1;
        break;

    case 2:
        index = 0;
        break;

    case 3:
        index = getIsSprinting() ? 6 : 5;                    // swimming
        break;

    default:
        abort();                                             // dice's - no default, no clamp
    }

    m_soldierSpeedIndex = index;                             // +0x2A4
}


// Sets descendant spawn points prone when pose is Prone.
// Every found object must expose ISpawnPoint.
void Soldier::updateSoldierSpawnPoint(SoldierPose pose)
{
    std::vector<IObject*> spawnPoints;
    findSpawnPoints(this, spawnPoints);

    for (size_t i = 0; i < spawnPoints.size(); ++i)
    {
        ISpawnPoint* point = static_cast<ISpawnPoint*>(
                                 spawnPoints[i]->queryInterface(IID_ISpawnPoint));
        point->setProneOnSpawn(pose == SoldierPose_Prone);       // point NOT null-checked, +0x44
    }
}

// Commits the pending pose and selects its transition duration from the old/new speed states.
// Sends notifications before and after the pose change and retains the outgoing movement speed.
void Soldier::executePoseChange()
{
    // both captured before the pose moves
    const float outgoingSpeed = getSoldierSpeed(7);
    const int   fromIndex     = m_soldierSpeedIndex;            // +0x2A4

    m_poseChangeProgress = 0.0f;                                // +0x2A8
    m_soldierSpeed = outgoingSpeed;                             // +0x2B0

    updateSoldierSpawnPoint(static_cast<SoldierPose>(m_requestedPose));

    handleMessage(static_cast<TemplateMessage>(66), getOccupingPlayer(), false);   // before
    m_pose = m_requestedPose;                                   // +0x1FC <- +0x200
    handleMessage(static_cast<TemplateMessage>(67), getOccupingPlayer(), false);   // after

    updateWeaponIsActiveState();
    updateSpeedState();                                         // recomputes m_soldierSpeedIndex

    // [from][to] in a 7x7 matrix - the stride of seven is what confirms the index range.
    m_poseChangeDuration = g_poseTransitionTable[7 * fromIndex + m_soldierSpeedIndex];
}


// Returns the field-of-view and weapon modifier for movement and mouse input.
// Returns 1 for an unoccupied soldier; an occupant must have a camera.
float Soldier::getMouseSpeedMod()
{
    if (!getOccupingPlayer())
        return 1.0f;

    if (!getOccupingPlayer()->getCamera())                       // IPlayer +0x180
        abort();                                                 // dice's

    const float playerFov = getOccupingPlayer()->getFov();       // IPlayer +0x90
    ICameraObject* camera = getOccupingPlayer()->getCamera();
    const float activeFov = camera->getActiveFOV();              // ICameraObject +0x58

    float mod = 1.0f;
    if (activeFov > playerFov)                                   // only when the camera is wider
        mod = playerFov / activeFov;

    IObject* item = getItem(m_activeItemIndex);
    if (item)
    {
        void* weapon = item->queryInterface(IID_IWeaponObject);
        if (weapon)                                              // checked here - see above
            return weaponSpeedMod(weapon) * mod;                 // +0x10C
    }
    return mod;
}

// Stows or activates the held item when swimming, ladder, skydiving or forced-stow state changes.
// With no active item, updates the HUD without changing the stored stow state.
void Soldier::updateWeaponIsActiveState()
{
    if (m_activeItemIndex == -1)
    {
        int payload[2] = { -1, 0 };
        eventManager->postEvent(static_cast<EventCategory>(2), 31, this, 0.0f);
        eventManager->postEvent(static_cast<EventCategory>(2), 32, payload, 0.0f);
        return;                                                  // the latch is left alone
    }

    // the five reasons, ORed
    const bool stow = (m_byte38D
                       || m_isSkyDiving
                       || m_pose == SoldierPose_Swim
                       || m_ladderId
                       || m_isSwimming);

    if (m_weaponStowed)
    {
        if (!stow)                                               // the edge back to armed
        {
            m_animationSystem3P->clearTriggerMessage(0xFFFFu);
            IObject* item = getItem(m_activeItemIndex);
            IItemObject* io = item
                ? static_cast<IItemObject*>(item->queryInterface(IID_IItemObject))
                : 0;
            io->enableItem();                                    // io NOT checked; bool ignored
        }
    }
    else if (stow)                                               // the edge into stowed
    {
        m_animationSystem3P->clearTriggerMessage(0xFFFFu);
        IObject* item = getItem(m_activeItemIndex);
        IItemObject* io = item
            ? static_cast<IItemObject*>(item->queryInterface(IID_IItemObject))
            : 0;
        io->disableItem();                                       // io NOT checked
    }
    m_weaponStowed = stow;

    // client-side only: tell the local player's HUD which item is active.
    if (playerManager->getLocalHumanPlayer() == getOccupingPlayer())
    {
        int payload[2] = { m_activeItemIndex, 0 };
        eventManager->postEvent(static_cast<EventCategory>(2), 31, this, 0.0f);
        eventManager->postEvent(static_cast<EventCategory>(2), 32, payload, 0.0f);
    }
}


// Requires shallow enough water and expired pose timers 0 and 3 before allowing prone.
bool Soldier::soldierCanGoProne()
{
    const Vec3 here = getAbsolutePosition();
    const float radius = getSoldierRadius();

    IHeightmapCluster* cluster = reinterpret_cast<IHeightmapCluster*>(heightmapCluster);
    const float water = cluster->getWaterLevelInWorldCoords(here);          // slot 39 +0x9C

    if (water <= radius + here.y - g_proneWaterClearance && m_poseTimers[0] <= 0.0f)
        return m_poseTimers[3] <= 0.0f;
    return false;
}

// Updates stance from prone-toggle and crouch-hold input, arming transition timers.
// Ignores pose input while swimming or skydiving and checks whether prone placement is allowed.
void Soldier::updateSoldierPose(bool prone, bool crouch)
{
    if (m_isSwimming || m_isSkyDiving)
        return;                                              // no pose changes in water or air

    if (prone)
    {
        if (m_poseInputLatch & 2)
            return;                                          // already handled this press

        const int pose = m_pose;
        const int fallback = (pose < 2) ? pose : 0;          // prone -> stand, otherwise keep
        int requested = fallback;

        if (pose != 2 || m_poseTimers[2] > 0.0f)
        {
            if (soldierCanGoProne())
                requested = 2;
        }

        m_requestedPose = requested;

        if (pose == 2 && requested == 0)                     // getting UP: three timers
        {
            m_poseTimers[1] = g_poseUpTimerA;
            m_poseTimers[3] = g_poseUpTimerB;
            m_float3B4      = g_poseUpTimerC;
        }
        if (pose == 0 && m_requestedPose == 2)               // going down: one timer
            m_poseTimers[2] = g_poseDownTimer;

        m_poseInputLatch |= 2;                               // latch the press
        return;
    }

    m_poseInputLatch &= ~2;                                  // prone released

    if (crouch)
    {
        if (!(m_poseInputLatch & 1))                         // the crouch press edge
        {
            m_poseInputLatch = (m_poseInputLatch & ~3) | 1;
            m_requestedPose = 1;
        }
        if (m_pose == 2 && m_requestedPose == 1)             // prone -> crouch, same three timers
        {
            m_poseTimers[1] = g_poseUpTimerA;
            m_poseTimers[3] = g_poseUpTimerB;
            m_float3B4      = g_poseUpTimerC;
        }
    }
    else
    {
        const bool wasCrouching = (m_pose == 1);
        m_poseInputLatch &= ~3;                              // both released
        if (wasCrouching)
            m_requestedPose = 0;                             // releasing crouch stands YOU UP
    }
}

// Applies facing relative to the vehicle seat, or in world space for an unparented soldier.
// Ragdolls retain their physics-driven transform.
void Soldier::updateTransformation()
{
    if (m_ragDoll)
        return;                                              // the ragdoll owns the transform

    Mat4 yaw = Mat4::getIdentity();
    setRotateYDeg(yaw, m_facingDegrees);                     // +0x1F0 [FRONTIER]

    IObject* vehicle = getUsedVehicle();
    if (vehicle)
    {
        IPlayerControlObjectTemplate* pcoTemplate =
            static_cast<IPlayerControlObjectTemplate*>(
                vehicle->getTemplate()->queryInterface(IID_IPlayerControlObjectTemplate));
        if (pcoTemplate)
        {
            Mat4 seated = Mat4::getIdentity();
            seated.mult(yaw, pcoTemplate->getSeatTransformation());       // +0xE8
            setRelativeTransformation(seated);
        }
        // no else - a vehicle without a PCO template silently does nothing
    }
    else if (!getParent())
    {
        const Vec3& here = getAbsolutePosition();
        yaw.m[12] = here.x;                                  // keep the position, replace the rotation
        yaw.m[13] = here.y;
        yaw.m[14] = here.z;
        setAbsoluteTransformation(yaw);
    }
    // no else - a parented soldier not in a vehicle silently does nothing
}


// Smooths movement input and blends speed across pose changes.
// Ground movement sets surface velocity; airborne steering cannot increase horizontal speed.
// Requires response physics and suppresses strafing while sprinting or swimming.
void Soldier::updateSoldierSpeed(bool inAir, float forwardInput, const Vec3& forwardAxis,
                                 float strafeInput, const Vec3& strafeAxis, float dt)
{
    if (getIsSprinting() || m_isSwimming)
        strafeInput = 0.0f;                                  // no strafing while sprinting/swimming

    float speed = getSoldierSpeed(7);

    // ease from the outgoing pose's speed to the incoming one
    if (m_poseChangeDuration > m_poseChangeProgress)
    {
        const float from = m_soldierSpeed;
        m_poseChangeProgress += dt;
        speed = (speed - from) * m_poseChangeProgress / m_poseChangeDuration + from;
    }

    float jumpSpeed = 0.0f;
    if (inAir && m_float274 < 0.0f && m_float278 > 0.0f)
    {
        jumpSpeed = m_float278 * 0.5f * g_jumpSpeedFactor;
        speed = jumpSpeed;
    }

    // two rates: one to press, one to release
    const float fRate = (forwardInput == 0.0f) ? g_inputReleaseRate : g_inputPressRate;
    m_smoothedForward += fRate * (forwardInput - m_smoothedForward);
    if (fabsf(m_smoothedForward) < 0.001f)
        m_smoothedForward = 0.0f;

    const float sRate = (strafeInput == 0.0f) ? g_inputReleaseRate : g_inputPressRate;
    m_smoothedStrafe += sRate * (strafeInput - m_smoothedStrafe);
    if (fabsf(m_smoothedStrafe) < 0.001f)
        m_smoothedStrafe = 0.0f;

    // combine, then clamp the combined direction to unit length
    Vec3 dir;
    dir.x = forwardAxis.x * m_smoothedForward + strafeAxis.x * m_smoothedStrafe;
    dir.y = forwardAxis.y * m_smoothedForward + strafeAxis.y * m_smoothedStrafe;
    dir.z = forwardAxis.z * m_smoothedForward + strafeAxis.z * m_smoothedStrafe;

    const float mag = sqrtf(m_smoothedForward * m_smoothedForward
                          + m_smoothedStrafe  * m_smoothedStrafe);
    if (mag > 1.0f)
    {
        const float inv = 1.0f / mag;
        dir.x *= inv; dir.y *= inv; dir.z *= inv;
    }

    SoldierResponsePhysics* response = soldierResponsePhysics(this);      // Soldier +0x48
    if (!response)
        abort();                                             // dice's

    if (inAir)
    {
        if (m_float274 < 0.0f && m_float278 > 0.0f)
        {
            PointPhysicsNode* node = soldierPhysicsNode(this);            // Object +0x44

            const Vec3 before = node->getPositionalSpeed();
            const float beforeHoriz = sqrtf(before.x * before.x + before.z * before.z);

            Vec3 add;
            add.x = dir.x * speed * g_speedToVelocity;
            add.y = 0.0f;                                    // the vertical is left to gravity
            add.z = dir.z * speed * g_speedToVelocity;
            node->addPositionalSpeed(add);

            const Vec3 after = node->getPositionalSpeed();
            const float afterHoriz = sqrtf(after.x * after.x + after.z * after.z);

            // steer, DO NOT accelerate: if the horizontal speed grew, scale it back to what it was.
            if (beforeHoriz > jumpSpeed && afterHoriz > beforeHoriz)
            {
                const float k = beforeHoriz / afterHoriz;
                Vec3 capped;
                capped.x = after.x * k;
                capped.y = before.y;                         // the fall is carried through untouched
                capped.z = after.z * k;
                node->setPositionalSpeed(capped);
            }
        }
    }
    else
    {
        // on the ground the velocity is a surface speed - which is what rides moving platforms.
        Vec3 surface;
        surface.x = dir.x * speed * g_speedToVelocity;
        surface.y = dir.y * speed * g_speedToVelocity;
        surface.z = dir.z * speed * g_speedToVelocity;
        response->setSurfacePositionalSpeed(surface);        // slot +0x88
    }
}

// Blends the camera toward the pose's eye height and applies clamped look angles.
// Parented soldiers use the seated pose; ragdolls are skipped. Requires a camera.
void Soldier::updateCameraTransformation()
{
    if (m_ragDoll)
        return;                                              // the ragdoll owns the view

    // clamp the persistent angles to the global pair
    if (m_lookLeftRightAngle < -g_lookAngleLimits[0])      m_lookLeftRightAngle = -g_lookAngleLimits[0];
    else if (m_lookLeftRightAngle > g_lookAngleLimits[0])  m_lookLeftRightAngle =  g_lookAngleLimits[0];

    if (m_lookUpDownAngle < -g_lookAngleLimits[1])         m_lookUpDownAngle = -g_lookAngleLimits[1];
    else if (m_lookUpDownAngle > g_lookAngleLimits[1])     m_lookUpDownAngle =  g_lookAngleLimits[1];

    // the per-tick deltas ride on top of the stored angles
    float yaw   = m_cameraYaw + m_lookLeftRightAngle;                    // +0x210
    float pitch = m_lookUpDownAngle + m_cameraPitch;                     // +0x208
    if (pitch < -g_lookAngleLimits[1])     pitch = -g_lookAngleLimits[1];
    else if (pitch > g_lookAngleLimits[1]) pitch =  g_lookAngleLimits[1];

    // a ridden vehicle keeps the yaw inside its own limits
    IObject* vehicle = getUsedVehicle();
    if (vehicle)
    {
        IPlayerControlObjectTemplate* pcoTemplate =
            static_cast<IPlayerControlObjectTemplate*>(
                vehicle->getTemplate()->queryInterface(IID_IPlayerControlObjectTemplate));
        if (pcoTemplate)
        {
            const float vehicleYawMin = pcoTemplate->getSeatLeftRotationLimit();   // +0xB8
            const float vehicleYawMax = pcoTemplate->getSeatRightRotationLimit();  // +0xC0

            const float lim = g_lookAngleLimits[0];
            bool withinVehicle = true;
            float over = 0.0f;

            if (yaw > lim)
            {
                over = yaw - lim;
                withinVehicle = (over + m_facingDegrees <= vehicleYawMax);
            }
            else if (yaw < -lim)
            {
                over = lim + yaw;
                withinVehicle = (vehicleYawMin <= over + m_facingDegrees);
            }
            if (!withinVehicle)
                yaw -= over;
        }
    }

    ICameraObject* camera = getSoldierCamera();
    if (!camera)
        abort();                                             // dice's
    IObject* cameraObject = static_cast<IObject*>(camera->queryInterface(IID_IObject));

    // a parented soldier is treated as pose 1 whatever his real pose is
    const int pose = getParent() ? 1 : m_pose;

    const Vec3 here = cameraObject->getRelativePosition();               // slot 56

    Mat4 pitchM = Mat4::getIdentity();  setRotateXDeg(pitchM, pitch);
    Mat4 yawM   = Mat4::getIdentity();  setRotateYDeg(yawM,   yaw);
    Mat4 out    = Mat4::getIdentity();
    out.rotMult(pitchM, yawM);                               // rotation only - see the note above

    const float follow = (pose == 1) ? g_cameraFollowCrouch
                       : (pose == 2) ? g_cameraFollowProne
                                     : g_cameraFollowDefault;

    // THE tree already HAD this accessor, and it carries dice's own guard: getPoseCameraPosition
    // aborts on pose > 3. Using it rather than indexing the template directly keeps that guard.
    const Vec3& target = getPoseCameraPosition(pose);      // SoldierTemplate +424 + 12 * pose

    // ease the camera towards this pose's eye position
    out.m[12] = here.x + (target.x - here.x) * follow;
    out.m[13] = here.y + (target.y - here.y) * follow;
    out.m[14] = here.z + (target.z - here.z) * follow;

    cameraObject->setRelativeTransformation(out);            // slot 59
}


// Processes a copy of input for look, movement, stance and firing without modifying the caller's input.
// Skips destroyed soldiers. Active movement paths require camera, armor and physics components.
void Soldier::handlePlayerInput(IPlayer* player, const io::PlayerInput& input,
                                float dt, unsigned int flags)
{
    // 1. two decay timers, unless the caller asks for them to be held
    if (!(flags & 4u))
    {
        if (m_float274 > 0.0f) { m_float274 -= dt; if (m_float274 <= 0.0f) m_float274 = -1.0f; }
        if (m_float278 > 0.0f) { m_float278 -= dt; if (m_float278 <= 0.0f) m_float278 = -1.0f; }
    }

    // 2. the delay gate: the entire rest of the tick is suppressed while it runs
    if (m_inputSuppressTimer > 0.0f)
    {
        m_inputSuppressTimer -= dt;
        return;
    }

    // 3. the per-tick camera deltas are cleared before anything can add to them
    IObject* parent = getParent();
    const bool parented = (parent != 0);                        // setnz
    m_cameraYaw   = 0.0f;                                    // +0x210
    m_cameraPitch = 0.0f;                                    // +0x208

    // 4. a parented soldier only steers if he actually has a hand weapon out
    if (parented && !hasHandWeaponActive())
        return;

    // 5. no camera aborts - the result is used with no second chance
    ICameraObject* camera = getSoldierCamera();
    if (!camera)
        abort();
    camera->queryInterface(IID_IObject);

    // 6. work on a copy; the caller's input is never touched
    io::PlayerInput filtered = input;

    // 7. the +0x399 latch drops as soon as the use key reads zero
    if (m_byte399 && mappedInput(filtered, io::PIUse) == 0.0f)
        m_byte399 = false;

    // 8. no armour component aborts
    ArmorGLComp* armor = objectArmorComp(this);
    if (!armor)
        abort();

    // status 1 = destroyed. A destroyed body does nothing further.
    if (armor->getStatus() == 1)
        return;

    // Forward input to registered child objects. Null list entries are invalid.
    for (std::list<IObject*>::iterator it = m_playerInputs.begin();
         it != m_playerInputs.end(); ++it)
    {
        IObject* child = *it;
        if (!child)
            abort();                                         // dice's - a NULL entry
        child->handlePlayerInput(player, filtered, dt, flags);
    }

    // 9. condition the action and movement axes
    float jumpAxis   = deadzoneAction(mappedInput(filtered, io::PIAction));    // bit 9
    float proneAxis  = deadzoneAction(mappedInput(filtered, io::PILie));       // bit 37
    float forwardAxis = conditionAxis(mappedInput(filtered, io::PIThrottle));  // bit 3
    float strafeAxis  = conditionAxis(mappedInput(filtered, io::PIYaw));       // bit 0

    // A ONE-shot third-scaling: while +0x314 is set the forward axis is divided by three, and the
    // flag is cleared - so it costs exactly one tick and then stops.
    if (m_thirdSpeedOnce)
    {
        forwardAxis /= 3.0f;
        m_thirdSpeedOnce = false;
    }

    // Retail suppresses the jump intent while the post-prone timer is armed or while the previous
    // tick still says we are swimming. This happens before the pose check: a prone Action press is
    // retained as an intent for the get-up arm below, but its physical jump axis is cleared.
    if (m_poseTimers[1] > 0.0f)
        jumpAxis = 0.0f;
    if (m_isSwimming)
        jumpAxis = 0.0f;

    bool wantsJump  = false;
    bool wantsProne = false;
    if (jumpAxis != 0.0f)
    {
        wantsJump = true;
        if (m_pose)                                          // only a standing soldier jumps
            jumpAxis = 0.0f;
    }
    if (proneAxis != 0.0f)
        wantsProne = true;

    // A parented soldier has no on-foot movement - every axis and both intents are dropped.
    if (parented)
    {
        wantsJump = false;
        wantsProne = false;
        strafeAxis = 0.0f;
        forwardAxis = 0.0f;
        jumpAxis = 0.0f;
    }

    // 10. mouse scaling, then recoil
    // Mouse axes do not use conditionAxis: no clamp and no sign-preserving square. The ELF
    // multiplies each raw mapped axis by its live factor, then applies only this deadzone.
    float lookX = mappedInput(filtered, io::PIMouseLookX) * g_lookFactorX;
    float lookY = mappedInput(filtered, io::PIMouseLookY) * g_lookFactorY;
    if (std::fabs(lookX) < 0.001f) lookX = 0.0f;
    if (std::fabs(lookY) < 0.001f) lookY = 0.0f;

    // An occupied body additionally gets the FOV and active-weapon mouse-speed modifier. Retail
    // tests the occupant here even though getMouseSpeedMod repeats that lookup internally.
    if (getOccupingPlayer())
    {
        const float mouseSpeedMod = getMouseSpeedMod();
        lookX *= mouseSpeedMod;
        lookY *= mouseSpeedMod;
    }

    const float yawDelta   = getYawRecoil()   + lookX;
    const float pitchDelta = getPitchRecoil() + lookY;
    m_pitchDeltaThisTick = pitchDelta;                                // +0x298
    m_yawDeltaThisTick = yawDelta;                                    // +0x294
    m_lookUpDownAngle += pitchDelta;

    // 11. the yaw pipeline
    // the vehicle's own yaw limits, if he is riding one
    float vehicleYawMin = -9999.0f;
    float vehicleYawMax =  9999.0f;
    IObject* vehicle = getUsedVehicle();
    if (vehicle)
    {
        IPlayerControlObjectTemplate* pcoTemplate =
            static_cast<IPlayerControlObjectTemplate*>(
                vehicle->getTemplate()->queryInterface(IID_IPlayerControlObjectTemplate));
        if (pcoTemplate)
        {
            vehicleYawMin = pcoTemplate->getSeatLeftRotationLimit();   // +0xB8
            vehicleYawMax = pcoTemplate->getSeatRightRotationLimit(); // +0xC0
        }
    }

    float appliedYaw = yawDelta;
    const float lookLimit = g_lookAngleLimits[0];

    // Moving drives the body immediately and transfers an existing stationary side-look into the
    // restoration accumulator. Stationary input moves only the camera until the side limit is met.
    if (std::fabs(forwardAxis) > 0.01f)
    {
        if (m_lookLeftRightAngle != 0.0f)
            m_float214 = m_lookLeftRightAngle;
    }
    else
    {
        m_lookLeftRightAngle += yawDelta;
        if (m_lookLeftRightAngle >= lookLimit)
        {
            if (vehicle)
                m_lookLeftRightAngle = lookLimit;
            else
                m_float214 = m_lookLeftRightAngle;
        }
        else if (m_lookLeftRightAngle <= -lookLimit)
        {
            if (vehicle)
                m_lookLeftRightAngle = -lookLimit;
            else
                m_float214 = m_lookLeftRightAngle;
        }
        else
        {
            appliedYaw = 0.0f;
        }
    }

    // PAY OUT THE pending turn AT A capped rate. The strict positive magnitude test also
    // preserves the retail NaN skip.
    if (std::fabs(m_float214) > 0.0f)
    {
        const float sign = (m_float214 > 0.0f) ? 1.0f : -1.0f;
        const float requestedStep = sign * g_maxTurnRatePerTick;
        // never overshoot what is pending. The sign test is deliberately on the requested
        // step, preserving retail's odd zero-rate arm (it pays a positive pending turn in full).
        const float step = requestedStep > 0.0f
                               ? std::min(requestedStep, m_float214)
                               : std::max(requestedStep, m_float214);

        m_float214           -= step;
        m_lookLeftRightAngle -= step;
        appliedYaw           += step;
    }

    // the facing angle Soldier::enter seeds - advanced, wrapped, then clamped to the vehicle
    m_facingDegrees += appliedYaw;                           // +0x1F0
    while (m_facingDegrees >=  360.0f) m_facingDegrees -= 360.0f;
    while (m_facingDegrees <= -360.0f) m_facingDegrees += 360.0f;

    if (m_facingDegrees < vehicleYawMin)      m_facingDegrees = vehicleYawMin;
    else if (m_facingDegrees > vehicleYawMax) m_facingDegrees = vehicleYawMax;

    updateCameraTransformation();

    // 12. movement and pose
    if (parented)
    {
        // A seated soldier does not run on-foot physics, but retail still reconciles a pending pose
        // and then reaches the common transformation and weapon-deviation tail below.
        m_isSwimming = false;
        if (m_pose != m_requestedPose)
            executePoseChange();
    }
    else
    {
        PointPhysicsNode* node = soldierPhysicsNode(this);
        SoldierResponsePhysics* response = soldierResponsePhysics(this);
        if (!node)
            abort();                                         // dice's

        // the swim state is the response physics' answer, not the soldier's own idea
        m_isSwimming = response && response->m_isFloating;    // retail response +0xFB
        if (m_isSwimming && m_pose != SoldierPose_Swim)
        {
            m_requestedPose = SoldierPose_Swim;
            executePoseChange();
        }
        else if (!m_isSwimming &&
                 (m_pose == SoldierPose_Swim || m_requestedPose == SoldierPose_Swim))
        {
            m_requestedPose = SoldierPose_Stand;
            executePoseChange();
        }

        // Retail reads the current velocity before asking for the movement matrix. The jump arm
        // below projects this value independently onto the matrix's forward and strafe axes.
        const Vec3& currentSpeed = node->getPositionalSpeed();

        // the two movement axes come out of the movement matrix, flattened and normalised
        const Mat4& move = getMovementMatrix();
        Vec3 forwardDir = normalisedXZ(move.m[8], move.m[10]);
        Vec3 strafeDir  = normalisedXZ(move.m[0], move.m[2]);

        updateSpeedState();

        const bool hasGroundUnderFoot = response && response->m_byteFA; // retail +0xFA
        if (hasGroundUnderFoot && jumpAxis != 0.0f)
        {
            // THE retail jump impulse. It arms all three post-jump delays first, then preserves
            // only the current horizontal velocity projected onto the soldier's two movement axes.
            m_fireDelayAfterJump = g_fireDelayAfterJump;
            m_poseTimers[0]      = g_proneDelayAfterJump;
            m_poseTimers[4]      = g_sprintRechargeDelayAfterJump;
            m_float278           = 2.0f;
            if (response->m_feetContactNormal.y < 0.80000001f) // retail +0xC8
                m_float274 = 0.5f;

            const float alongForward =
                (currentSpeed.x * forwardDir.x + currentSpeed.y * forwardDir.y
                 + currentSpeed.z * forwardDir.z) * g_jumpLengthFactor;
            const float alongStrafe =
                (currentSpeed.x * strafeDir.x + currentSpeed.y * strafeDir.y
                 + currentSpeed.z * strafeDir.z) * g_jumpLengthFactor;

            Vec3 jumpVelocity;
            jumpVelocity.x = forwardDir.x * alongForward + strafeDir.x * alongStrafe;
            jumpVelocity.y = forwardDir.y * alongForward + strafeDir.y * alongStrafe;
            jumpVelocity.z = forwardDir.z * alongForward + strafeDir.z * alongStrafe;

            const float jumpLength = sqrtf(jumpVelocity.x * jumpVelocity.x
                                         + jumpVelocity.y * jumpVelocity.y
                                         + jumpVelocity.z * jumpVelocity.z);
            const float maxJumpLength = getSoldierSpeed(7) * g_jumpLengthFactor;
            if (jumpLength > maxJumpLength)
            {
                const float scale = maxJumpLength / jumpLength;
                jumpVelocity.x *= scale;
                jumpVelocity.y *= scale;
                jumpVelocity.z *= scale;
            }

            jumpVelocity.y = 6.0f * g_jumpFactor;
            node->setLocalLinearSpeed(Vec3::getZero());
            node->setPositionalAcceleration(jumpVelocity);
            node->setPositionalSpeed(jumpVelocity);

            float stamina = m_sprintState.m_stamina - m_soldierTemplate->m_sprintLossAtJump;
            if (stamina <= 0.0f)
                stamina = 0.0f;
            m_sprintState.setStamina(stamina);
        }

        // Grounded and swimming motion is a response-physics surface speed. Unsupported motion
        // uses the in-air steering arm. Skydiving is a third, distinct acceleration path.
        if (hasGroundUnderFoot || m_isSwimming)
        {
            updateSoldierSpeed(false, forwardAxis, forwardDir,
                               strafeAxis, strafeDir, dt);
        }
        else if (m_isSkyDiving)
        {
            Vec3 skydiveDirection;
            skydiveDirection.x = move.m[8];
            skydiveDirection.y = move.m[9];
            skydiveDirection.z = move.m[10];
            if (skydiveDirection.y > 0.0f)
                skydiveDirection = normalisedXZ(skydiveDirection.x, skydiveDirection.z);

            Vec3 acceleration;
            acceleration.x = skydiveDirection.x * g_skydiveSpeed;
            acceleration.y = skydiveDirection.y * g_skydiveSpeed;
            acceleration.z = skydiveDirection.z * g_skydiveSpeed;
            node->addAccelerationAtRelativePosition(Vec3::getZero(), acceleration);
        }
        else
        {
            updateSoldierSpeed(true, forwardAxis, forwardDir,
                               strafeAxis, strafeDir, dt);
        }

        // the rotational speed is the applied yaw expressed per second
        Vec3 rotation;
        rotation.x = 0.0f;
        rotation.y = appliedYaw / dt;
        rotation.z = 0.0f;
        // SoldierPhysicsNode consumes only Y; typed dispatch preserves each platform's vtable ABI.
        node->setRotationalSpeed(rotation);

        // pose input, but only once the go-prone timer has expired
        bool proneReady = false;
        if (wantsJump && m_pose == SoldierPose_Prone && m_poseTimers[2] <= 0.0f)
        {
            proneReady = true;
            m_poseTimers[1] = g_poseUpTimerA;
            m_float3B4      = g_poseUpTimerC;
        }
        const bool crouchHeld = (mappedInput(filtered, io::PICrouch) != 0.0f) || proneReady;
        updateSoldierPose(wantsProne, crouchHeld);
    }

    // Common to both on-foot and parented paths in ELF 0x81A97AF onward.
    updateTransformation();

    // 13. the weapon, unless the caller asked us not to
    if (!(flags & 4u))
    {
        IObject* weapon = getActiveWeapon(0);
        if (weapon)
        {
            IWeaponObject* iw = static_cast<IWeaponObject*>(
                weapon->queryInterface(IID_IWeaponObject));
            if (iw)
            {
                // The complete filtered copy reaches the deviation component. The second
                // argument is the retail jump/prone disturbance gate, not another input axis.
                iw->updateDeviation(filtered, wantsJump || wantsProne);
            }
        }
    }
}


// Prepares this soldier to ride entering: stows equipment, resets aim and applies kit vehicle bonuses.
// Null does nothing. Existing vehicle use, self-entry and entering another soldier are invalid.
void Soldier::onEnter(IObject* entering)
{
    typedef object_detail::LayoutAccess L;

    if (!entering)
        return;

    // all three are abort(), not Debug - see the header block.
    if (getUsedVehicle())                                              // [S] +0x1D4
        abort();
    if (entering == static_cast<IObject*>(this))
        abort();
    if (L::objectTemplate(entering)->getClassID() == CID_SoldierTemplate)   // raw +0x14
        abort();

    setIsSkyDiving(false);

    // 1. m_usedVehicleWeak = entering->getWeakPtr()
    // The same hand-spelled block accounting Player::setDefaultVehicle uses: retain the incoming
    // block, release the old one, store, then release the returned temporary (its destructor).
    void* incoming = 0;
    {
        WeakPtr<IObject> handle = entering->getWeakPtr();              // IObject slot 7, +0x1C
        incoming = handle.m_ctrl;
        handle.m_ctrl = 0;                                             // the steal
        if (incoming)
            ++(*reinterpret_cast<int*>(incoming));
    }
    if (m_usedVehicleWeak)
    {
        int* const old = reinterpret_cast<int*>(m_usedVehicleWeak);
        if (--(*old) == 0)
            ::operator delete(m_usedVehicleWeak);
    }
    m_usedVehicleWeak = incoming;
    if (incoming)
    {
        int* const temp = reinterpret_cast<int*>(incoming);
        if (--(*temp) == 0)
            ::operator delete(incoming);
    }

    // 2. does the thing being entered have a camera?
    // the walk is UP the parent chain (+0x28), not down the children.
    IPlayerControlObject* seat = 0;
    for (IObject* node = entering; node; node = L::parent(node))
    {
        seat = static_cast<IPlayerControlObject*>(
                   node->queryInterface(IID_IPlayerControlObject));    // +0x0C
        if (seat)
            break;
    }

    if (seat && !seat->getCameras()->empty())                          // +0x38 then the node count
    {
        if (m_activeItemIndex != -1)                                   // +0x1B8
        {
            IObject* const item = getItem(m_activeItemIndex);          // [S] +0x25C
            IItemObject* active = 0;
            if (item)
                active = static_cast<IItemObject*>(
                             item->queryInterface(IID_IItemObject));   // +0x0C
            active->disableItem();                                     // unguarded, +0x2C
        }
        m_byte38D = true;                                              // +0x38D
        if (m_kit)
            m_kit->updateFlags(1, 0);                                  // IObject +0x60
    }
    else
    {
        m_byte38D = false;
    }

    if (m_collisionSkeleton)
        m_collisionSkeleton->removeAllPostAbsoluteBoneTransforms();

    // 3. blank the aim and the pose
    m_pose              = 0;                                            // +0x1FC
    m_lookLeftRightAngle = 0.0f;                                        // +0x20C
    m_lookUpDownAngle    = 0.0f;                                        // +0x204
    m_facingDegrees      = 0.0f;                                        // +0x1F0
    m_requestedPose      = 0;                                           // +0x200
    updateSoldierSpawnPoint(static_cast<SoldierPose>(0));
    // retail also zeroes twelve bytes of dead stack here (the WeakPtr temp's slot, reused as a
    // Vec3 that is never read). Nothing observable; not reproduced.
    m_yawDeltaThisTick   = 0.0f;                                        // +0x294
    m_pitchDeltaThisTick = 0.0f;                                        // +0x298
    m_dword29C           = 0;                                           // +0x29C
    updateCameraTransformation();

    // 4. the kit's vehicle mods
    if (m_kit)
    {
        IObject* root = getRootParent(entering);
        // the template is read raw and dereferenced before root's own null test below, as compiled.
        const std::string& rootName = L::objectTemplate(root)->getName();   // +0x2C

        IPlayerControlObject* rootPco = 0;
        for (IObject* node = root; node; node = L::parent(node))            // +0x28
        {
            rootPco = static_cast<IPlayerControlObject*>(
                          node->queryInterface(IID_IPlayerControlObject));
            if (rootPco)
                break;
        }

        std::vector<std::pair<std::string, VehicleMod> >* const mods =
            m_kit->getKitVehicleMods();
        for (unsigned int i = 0; i < mods->size(); ++i)
        {
            if ((*mods)[i].first.compare(rootName) == 0)
                rootPco->addVehicleMod((*mods)[i].second);                  // unguarded, +0xDC
        }
    }

    // 5.
    m_isSwimming = false;                                               // +0x2A0
    updateWeaponIsActiveState();
    if (!hasHandWeaponActive())
        findInverseKinamaticObject();                                   // [FRONTIER]
}


// Finds the first object with an IK template component in the sibling/child traversal, or null.
// Encountering a player-control object or parachute ends that level's search, including later siblings.
IObject* findIkObject(IObject* object)
{
    typedef object_detail::LayoutAccess L;

    for (; object; object = L::sibling(object))                        // raw +0x2C
    {
        // both OF these abandon THE search - see the header block.
        if (L::objectTemplate(object)->getClassID() == CID_PlayerControlObjectTemplate)
            return 0;
        if (L::objectTemplate(object)->getClassID() == CID_ParachuteTemplate)
            return 0;

        // +0x5C and it is NOT const - the raw template read has to lose its constness here.
        if (const_cast<IObjectTemplate*>(L::objectTemplate(object))
                ->queryComponent(IID_IIkTemplateComponent))
            return object;

        if (IObject* const found = findIkObject(const_cast<IObject*>(object->getChild())))  // +0xF0
            return found;
    }
    return 0;
}

IObjectTemplateComponent* Soldier::findInverseKinamaticObject()
{
    // the weak block is read twice, and its target is dereferenced unguarded both times.
    const WeakPtr<IObject>::Ctrl* const weak = static_cast<const WeakPtr<IObject>::Ctrl*>(m_usedVehicleWeak); // +0x1D4
    IObject* const used = weak ? weak->target : 0;

    m_ikObject = findIkObject(const_cast<IObject*>(used->getChild()));               // +0xF0

    if (!m_ikObject)
    {
        const WeakPtr<IObject>::Ctrl* const weakAgain = static_cast<const WeakPtr<IObject>::Ctrl*>(m_usedVehicleWeak);
        IObject* const usedAgain = weakAgain ? weakAgain->target : 0;
        // the sibling, not the parent - see the header block.
        m_ikObject = findIkObject(object_detail::LayoutAccess::sibling(usedAgain));  // raw +0x2C
    }

    if (!m_ikObject)
        return 0;

    m_ikTemplateComponent = const_cast<IObjectTemplate*>(
        object_detail::LayoutAccess::objectTemplate(m_ikObject))
            ->queryComponent(IID_IIkTemplateComponent);                              // +0x5C
    return m_ikTemplateComponent;
}
} // namespace world
} // namespace hfe
} // namespace dice
