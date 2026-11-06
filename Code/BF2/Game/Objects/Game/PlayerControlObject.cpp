// Vehicle lifecycle, input, occupancy, and camera behavior.

#include "PlayerControlObject.h"
#include "RemoteControlledObject.h"                 // setTeam per-child call
#include "LadderContainer.h"                        // init's Ladder animation-system arm
#include "GrapplingHookRopeContainer.h"             // init's independent SF rope arm
#include "GrapplingHookRope.h"

#include "../../../Core/Ids.h"
#include "../../IGameLogic.h"                     // gameLogic - addPco (+0x194) / removePco (+0x19C)
#include "../../IGame.h"                          // game->destroyObject (+0xA0) - handleMessage
#include "../ObjectSpawnerTemplate.h"             // getDistance / getDamageWhenLost - the watchdog
#include "../GameLogicComp/ArmorGLCompTemplate.h"    // getExplosionRadius - the watchdog's soldier radius
#include "../../../Scene/Object/PlayerManager.h" // playerManager - the watchdog's soldier scan
#include "../../GameLogic/SpawnSystem/SpawnManager.h"   // spawnManager - the dynamic spawn group
#include "../../GameLogic/SpawnSystem/SpawnGroup.h"     // ISpawnGroup - setActive / setTeam
#include "../IPlayer.h"                             // getTeam's client arm
#include "../IWeaponObject.h"
#include "../IItemObject.h"                        // itemObjectGetItemIndex, likewise
#include "../../../Scene/Object/ObjectTypes/Base/ObjectTemplateManager.h"  // the seatParent lookup
#include "../Base/SimpleObjectTemplate.h"         // createSoundFromTemplate (the 12 B server stub)
#include "../PlayerControlObjectTemplate.h"       // initNetworkableComp's networkableInfo
#include "../TemplateLeaves.h"                    // LadderTemplate's animation-system field
// enableTargetNametags / enablePlayerDecal
#include "../Fx/DecalTemplate.h"                  // the decal template's m_decal (+0x34)
#include "../../../Geom/DecalSystem/DecalObjectAccess.h"
#include <map>                                    // the target-object map enableTargetNametags clones
#include <cstring>                                // the two redundant Mat4 clears, as compiled
#include "../../../Scene/Object/ObjectUtil.h"     // setNetworkableComp
#include "../../../Scene/Object/ObjectManager.h"  // objectManager->postMessage - setTimeToLiveUnused
#include "../../../Scene/Object/ObjectPredicator.h"
#include "Soldier.h"                              // the child-soldier weapon delegation
#include "Kit.h"
#include "GroundContactSound.h"
#include "../GameLogicComp/IArmorGLComp.h"
#include "../WarningHudCompTemplate.h"
#include "../../../Physics/MaterialManager.h"
#include "../../../Sound/SoundObjectTemplate.h"
#include "../../../Core/EventManager.h"           // eventManager - setActiveWeapon's ECHud pickup pair
#include <cstdio>                                  // diagnostics for the remaining named stops
#include <cstdlib>                                 // std::abort - their stop
#include <cfloat>                                  // FLT_EPSILON - SprintState::handleUpdate
#include <cmath>                                   // floor / sqrt - calcAmmoStatus, handleUpdate
#include "../ICameraObject.h"
#include "../../../Core/Debug.h"            // setActiveCamera's "TBM: No camera!" site                      // getActiveCamera()->getViewMode / queryInterface - handleUpdate
#include "../ObjectLayoutAccess.h"                 // +0x44 physicsNode - handleUpdate
#include "../../GameServer/ServerSettings.h"       // serverSettings->getNoVehicles - SprintState::handleUpdate
#include "../../../IO/Input/PlayerInput.h"         // io::PlayerInput - the unmanned-input arm
#include "../../../Physics/IPhysicsNode.h"         // getIsSleeping / getAbsolutePositionalSpeed
#include "../../../Physics/BasicPhysicsSystem.h"   // regulateInput's gravity compensation
#include "../../../Physics/HeightmapCluster.h"     // restricted-exit terrain queries
#include "../../../Physics/ICollisionMesh.h"       // exit-ray start from the root collision bounds
#include "../../../Physics/PhysicsUtil.h"          // getWheelMaterials
#include "../../../Math/LocalBoundingBox.h"        // collision-bounds ray intersection
#include "../../../Math/Ray3.h"                    // getExitPosRayTestStart's local ray
#include "../../../Animation/BoneAnimation/AnimationSystem.h" // typed +0x218 runtime system
#include "../../../Core/SettingsRepostitory.h"     // GSDeveloper - exit camera restoration
#include "../../../Sound/BfSound.h"                // listener environment / wind controls
#include "../../Utility/Vars.h"                    // pco-exitpos-testlod
#include "../../../Core/Random.h"                  // shared retail unmanned-input RNG
#include "../../../Core/System.h"                  // unsigned unmanned-input tick gate
// enter()
#include "../SpawnPoint.h"                          // hfe::getSpawnPoint + ISpawnPoint::setActive
#include "../../../AI/AIInterface/AIInstance.h"     // g_aiInstance - the ECAI notify gate
#include "../../../AI/AIInterface/BFKnowledge/IAIObject.h"   // init()'s lock bracket, the revive's init()
#include "RotationalBundle.h"                       // restore the exiting soldier camera's rotation

namespace dice {
namespace hfe {

// SprintState - the five members
SprintState::SprintState()
{
    m_constant0 = 0.0f;
    m_constant1 = 0.0f;
    m_staminaDissipationFactor = 1.0f;           // +0x0C
    m_constant2 = 0.0f;
    m_stamina = 1.0f;                            // +0x10
    _pad17 = false;
    reset();                                     // the tail jump
}
void SprintState::reset()
{
    m_sprintRequested = false;                   // +0x14
    _byte15 = false;                             // +0x15
    m_isSprinting = false;                       // +0x16
}
void SprintState::setStamina(float stamina)
{
    m_stamina = stamina;
}
void SprintState::setConstants(float a, float b, float c)
{
    m_constant0 = a; m_constant1 = b; m_constant2 = c;
}
void SprintState::handleTMSprint(bool arg)
{
    m_sprintRequested = true;                    // +0x14
    if (arg && !m_isSprinting)
        m_sprintRequested = false;
}

// hfe-scope helper, attested: getSpawnPoint(world::IObject*) - 30 B. ISpawnPoint is an hfe-scope
// interface (SpawnManager.h's convention); the one-queryInterface body lives in SpawnPoint.cpp.
class ISpawnPoint;
ISpawnPoint* getSpawnPoint(world::IObject* object);

extern IBase* spawnManager;                      // ds:0x8AD3A00 - the SpawnPoint.cpp form

namespace world {
extern IBase* materialManager; // Core/Globals.cpp, ELF 8AD2820 / PE 861820.

extern IBase* heightmapCluster;                  // ds:0x8ABAF24 - Core/Globals.cpp


// Shared initialization helpers.
void postInitPcoId(IObject* root);
void postInitCameras(IObject* root);                                 // (calls findCameras)
void findPlayerControlObjects(IObject* root,
                              std::map<int, IPlayerControlObject*>& out);
void findSpawnPoints(IObject* root, std::vector<IObject*>& out);
void collectWeapons(IPlayerControlObject* pco);                      // (calls internalCollect)
void findPlayerInputObjects(IObject* root, std::list<IObject*>& out, bool stopAtMatchingRoot);   // ^
IObject* findObjectHavingTemplateId(IObject* root, unsigned int templateId);
void setSleepTimeoutRecursive(IObject* root, int timeout);
void disablePhysicsOnSoldier(IObject* object);                       // Soldier.cpp - MSVC found it
                                                                     // through Soldier's friend declaration
void reenablePhysicsOnSoldier(IObject* object);

// The constructor - C2 0x81962E0, 1,207 bytes, read in full. Real STL members collapse the
// inlined SGI allocator machinery (the GLIBCPP_FORCE_NEW pooled-alloc dance around every map
// header) into default construction; what remains is the scalar sheet and two template reads:
// m_pcoId           <- template +0x258 m_pcoId (the .con-authored vehicle-position id)
// m_altExitPosition <- template +0x210 m_altSoldierExitPosition (three floats)
// No server/client fork, no updateFlags call of its own - the vehicle waits for init.
PlayerControlObject::PlayerControlObject(const PlayerControlObjectTemplate* objectTemplate)
    : Bundle(objectTemplate)
{
    m_refCount = 1;                              // +0x144
    m_template = objectTemplate;                 // +0x148
    m_handleUpdateTimeout = -1.0f;
    m_entered = false;
    m_pcoId = objectTemplate->m_pcoId;           // +0x154 <- template +0x258
    m_occupyingPlayer = 0;                       // +0x158 (control block)
    m_occupyingObject = 0;                       // +0x15C (control block)
    m_activeWeapons[0] = m_activeWeapons[1] = m_activeWeapons[2] = 0;   // +0x1C4 (the tail loop)
    m_spawnGroupId = -1;                         // +0x1DC
    _dword1E0 = 0;
    m_team = 0;                                  // +0x1E4
    m_defaultTeam = 0;
    m_timeToLiveUnused = -1.0f;
    m_timeToLiveUnusedReset = -1.0f;
    m_timeSpawned = -1.0f;
    m_objectSpawnerId = -1;
    m_objectSpawnerHolding = false;
    m_altExitPosition[0] = objectTemplate->m_altSoldierExitPosition.x;   // template +0x210
    m_altExitPosition[1] = objectTemplate->m_altSoldierExitPosition.y;
    m_altExitPosition[2] = objectTemplate->m_altSoldierExitPosition.z;
    _byte20C = false;
    m_driverDecalObject = 0;
    m_seatParentObject = 0;
    m_activeAnimationSystem = 0;                 // +0x218
    for (int i = 0; i < 9; ++i) m_vehicleMods[i] = 0;
    _dword240 = 0;
    m_specialTogglePressed = false;
    _dword278 = -1;
    m_ambientSoundFromTemplate2 = 0;
    m_ambientSoundFromTemplate = 0;
    m_ambientDamagedSoundFromTemplate = 0;
    m_listener3DObstruction = 0.0f;                  // +0x288 (re-)
    m_groundContactSound = 0;
    m_inputTime = 0.0f;
    m_ammoStatus = 15; // All four ammo-status bits.
    m_driveWheelFirst.m_ctrl  = 0;               // +0x298 the ctor's raw zero, kept explicit
    m_driveWheelSecond.m_ctrl = 0; // +0x29C
    m_unmannedInputTick = 0;                      // ELF +0x2A0 / PE +0x2E4
    m_isDucking = false;
    for (int i = 0; i < 64; ++i) _inputState[i] = 0;   // the 0x100-byte input block
    m_giveUnmannedInput = false;
    _dword3A8 = 0;                               // +0x3A8
    _dword3AC = _dword3B0 = _dword3B4 = 0;
    _byte3B8 = false;
    _byte3B9 = false;
    m_firstGhost = true;                         // +0x3BA
}

// Releases animation, ambient sounds, held objects, and weak occupancy controls.
PlayerControlObject::~PlayerControlObject()
{
    delete m_activeAnimationSystem;
    if (m_ambientSoundFromTemplate)
        static_cast<ISound*>(m_ambientSoundFromTemplate)->release();
    if (m_ambientDamagedSoundFromTemplate)
        static_cast<ISound*>(m_ambientDamagedSoundFromTemplate)->release();
    if (m_groundContactSound)
    {
        m_groundContactSound->m_sound->stop();
        m_groundContactSound->m_sound->release();
        delete m_groundContactSound;
    }
    if (m_occupyingObject && --*m_occupyingObject == 0)     // +0x15C
        ::operator delete(m_occupyingObject);
    m_occupyingObject = 0;
    if (m_occupyingPlayer && --*m_occupyingPlayer == 0) // +0x158
        ::operator delete(m_occupyingPlayer);
    m_occupyingPlayer = 0;
    // (the +0x290..+0x298 refcounted run and the remaining vector frees are the members' own
    // destructors here; the ELF's inline expansions were verified against the container set)
}

void PlayerControlObject::addRef()  { ++m_refCount; }
int  PlayerControlObject::getRef()  { return m_refCount; }
int  PlayerControlObject::release()
{
    const int r = --m_refCount;
    if (r == 0) delete this;
    return r;
}

// five cases, walked: IBase/IObject -> this; ConsoleSaveable -> +0x80; ipco -> +0x13C;
// IAnimatedBundle -> +0x140.
void* PlayerControlObject::queryInterface(unsigned int iid) const
{
    if (iid == IID_IBase || iid == world::IID_IObject)
        return const_cast<PlayerControlObject*>(this);
    if (iid == io::IID_IConsoleSaveable)
        return static_cast<io::IConsoleSaveable*>(const_cast<PlayerControlObject*>(this));
    if (iid == world::IID_IPlayerControlObject)
        return static_cast<IPlayerControlObject*>(const_cast<PlayerControlObject*>(this));   // the +0x13C sub-object
    if (iid == world::IID_IAnimatedBundle)
        return static_cast<IAnimatedBundle*>(const_cast<PlayerControlObject*>(this));   // the +0x140 sub-object
    return 0;
}

// Initializes vehicle hierarchy, weapons, input targets, animation and sounds.
// Only root player-control objects discover seats and spawn points and register with GameLogic.
void PlayerControlObject::initNetworkableComp()
{
    if (m_parent)                                                              // +0x28
    {
        const PlayerControlObjectTemplate* objectTemplate =
            static_cast<const PlayerControlObjectTemplate*>(m_template);        // +0x14
        if (objectTemplate && objectTemplate->getNetworkableInfo().compare("") != 0)   // +0x94
            setNetworkableComp(this, CID_PlayerControlObjectNetworkable);      // 39007
    }
    else
    {
        SimpleObject::initNetworkableComp();
    }
}

void PlayerControlObject::init()
{
    aiObjectCreate();                                            // bots-gated inside
    // Lock AI initialization across construction, then unlock and initialize afterward.
    hfe::ai::IAIObject* const aiObject =
        static_cast<hfe::ai::IAIObject*>(object_detail::LayoutAccess::aiObject(this));
    if (aiObject)
        aiObject->lockInit();

    Bundle::init();

    if (!getParent())
    {
        postInitPcoId(this);
        postInitCameras(this);
        findPlayerControlObjects(this, m_pcos);
        findSpawnPoints(this, m_spawnPoints);
        if (!m_spawnPoints.empty())
        {
            // A failed dynamic spawn-group creation returns ID -1.
            m_spawnGroupId = static_cast<SpawnManager*>(spawnManager)->createDynamicSpawnGroup();
            if (m_spawnGroupId != -1)
            {
                ISpawnGroup* group =
                    static_cast<SpawnManager*>(spawnManager)->getSpawnGroup(m_spawnGroupId);
                if (group)
                {
                    group->setActive(true);

                    for (unsigned int i = 0; i < m_spawnPoints.size(); ++i)
                        static_cast<SpawnManager*>(spawnManager)->addSpawnPoint(
                            getSpawnPoint(m_spawnPoints[i]), m_spawnGroupId);
                }
            }
            setTeam(m_team);                                     // vcall s131, recovered
        }
    }

    collectWeapons(static_cast<IPlayerControlObject*>(this));
    findPlayerInputObjects(this, m_playerInputObjects, true);
    for (std::list<IObject*>::iterator it = m_playerInputObjects.begin();
         it != m_playerInputObjects.end(); ++it)
    {
        const unsigned int id = (*it)->getObjectId();           // inline field read; ID stays 32-bit
        m_playerInputIds.insert(m_playerInputIds.end(), id);
    }

    if (!m_template->m_seatParent.seatInformation.empty())
    {
        IObjectTemplate* seatTemplate = objectTemplateManager->getTemplate(
            m_template->m_seatParent.seatInformation);
        if (seatTemplate)
            m_seatParentObject = findObjectHavingTemplateId(this, seatTemplate->getId());
    }

    // Step 6 begins with the Ladder-specific arm. This and the following GrapplingHook arm are
    // independent `if`s in retail; only the final SeatParent fallback is gated on the result.
    if (m_template->getClassID() == CID_LadderContainerTemplate)
    {
        LadderContainer* ladderContainer = static_cast<LadderContainer*>(this);
        if (ladderContainer->m_ladder)
        {
            anim::IAnimationSystemTemplate* animationTemplate =
                ladderContainer->m_ladder->m_template->m_seatAnimationSystem;
            if (animationTemplate)
                m_activeAnimationSystem = animationTemplate->makeInstance();
        }
    }

    if (m_template->getClassID() == CID_GrapplingHookRopeContainerTemplate)
    {
        GrapplingHookRopeContainer* ropeContainer =
            static_cast<GrapplingHookRopeContainer*>(this);
        if (ropeContainer->m_rope)
        {
            anim::IAnimationSystemTemplate* animationTemplate =
                ropeContainer->m_rope->m_ropeTemplate->m_seatAnimationSystem;
            if (animationTemplate)
                m_activeAnimationSystem = animationTemplate->makeInstance();
        }
    }

    if (!m_activeAnimationSystem)
    {
        anim::IAnimationSystemTemplate* animationTemplate =
            m_template->m_seatParent.seatAnimationSystem;
        if (animationTemplate)
            m_activeAnimationSystem = animationTemplate->makeInstance();
    }

    if (aiObject)
    {
        aiObject->unlockInit();                                   // ELF +0x44 / PE +0x40
        aiObject->init();                                         // ELF +0x48 / PE +0x44
    }

    m_ambientSoundFromTemplate = const_cast<SimpleObjectTemplate*>(
        static_cast<const SimpleObjectTemplate*>(m_template))
            ->createSoundFromTemplate(std::string("Ambient"), this);
    m_ambientDamagedSoundFromTemplate = const_cast<SimpleObjectTemplate*>(
        static_cast<const SimpleObjectTemplate*>(m_template))
            ->createSoundFromTemplate(std::string("AmbientDamaged"), this);
    m_ambientSoundFromTemplate2 = m_ambientSoundFromTemplate;

    if (!getParent())
        gameLogic->addPco(this);                                 // IGameLogic +0x194, recovered
    setSleepTimeoutRecursive(this, 90);
}

// reset - 0x8197760, 374 bytes, read in full. Deregistration and re-arm:
// * a root deregisters from GameLogic (removePco +0x19C);
// * disableWeapons (recovered);
// * position 0 (the driver seat) with a parent: climb the parent chain to the first PCO and
// poke its interface slot +0x20 - the "driver left" notification;
// * release the animation system's +0x0C, re-arm the scalar sheet (exactly the ctor's values),
// and release the occupancy control block.
void PlayerControlObject::reset()
{
    if (!getParent())
        gameLogic->removePco(this);                              // +0x19C recovered

    disableWeapons();

    if (m_pcoId == 0 && getParent())
    {
        IObject* walk = const_cast<IObject*>(getParent());
        IPlayerControlObject* pcoInterface = 0;
        while (walk)
        {
            pcoInterface = static_cast<IPlayerControlObject*>(walk->queryInterface(world::IID_IPlayerControlObject));
            if (pcoInterface)
                break;
            walk = const_cast<IObject*>(walk->getParent());
        }
        if (pcoInterface)
            pcoInterface->exit();                                    // ipco +0x20
    }

    if (m_activeAnimationSystem)
        m_activeAnimationSystem->reset();

    m_objectSpawnerHolding = false;
    m_timeToLiveUnused = -1.0f;
    m_timeToLiveUnusedReset = -1.0f;
    m_handleUpdateTimeout = -1.0f;
    m_timeSpawned = -1.0f;
    m_inputTime = 0.0f;
    m_team = 0;
    m_unmannedInputTick = 0;                          // ELF +0x2A0
    m_objectSpawnerId = -1;
    _byte3B9 = false;
    m_isDucking = false;
    m_firstGhost = true;
    m_ammoStatus = 15;                               // +0x294 (reset: `mov [esi+294h], edx`, edx = 0xF)
    m_entered = false;
    m_defaultTeam = 0;
    m_giveUnmannedInput = false;

    if (m_occupyingObject && --*m_occupyingObject == 0)
        ::operator delete(m_occupyingObject);
    m_occupyingObject = 0;
}

// Parks an unoccupied vehicle after resetting state and returning its dynamic spawn group.
// Entered state or a retained occupant is a fatal invariant violation.
void PlayerControlObject::disableCachedObject()
{
    if (m_entered)                                               // +0x150
        std::abort();
    if (m_occupyingPlayer && reinterpret_cast<WeakPtr<IPlayer>::Ctrl*>(m_occupyingPlayer)->target)
        std::abort();

    reset();
    if (m_spawnGroupId != -1)                                    // +0x1DC
        static_cast<SpawnManager*>(spawnManager)->removeGroup(m_spawnGroupId);   // +0x24
    SimpleObject::disableCachedObject();
}

// Restores spawn points, registration, AI and wheel references when reactivating a cached vehicle.
// Resets sprint stamina to full.
void PlayerControlObject::enableCachedObject()
{
    if (!m_spawnPoints.empty())                                  // +0x1D0
    {
        m_spawnGroupId =
            static_cast<SpawnManager*>(spawnManager)->createDynamicSpawnGroup();      // +0x38
        if (m_spawnGroupId != -1)
        {
            ISpawnGroup* const group =
                static_cast<SpawnManager*>(spawnManager)->getSpawnGroup(m_spawnGroupId);   // +0x28
            if (group)
            {
                group->setActive(true);                          // ISpawnGroup slot 10 (+0x28)
                for (unsigned int i = 0; i < m_spawnPoints.size(); ++i)
                    static_cast<SpawnManager*>(spawnManager)->addSpawnPoint(
                        getSpawnPoint(m_spawnPoints[i]), m_spawnGroupId);              // +0x18
            }
        }
        setTeam(m_team);                                         // virtual - the saved +0x1E4
    }

    if (!getParent())
        gameLogic->addPco(this);                                 // roots only

    aiObjectCreate();                                            // bots-gated inside
    hfe::ai::IAIObject* const aiObject =
        static_cast<hfe::ai::IAIObject*>(m_aiObject);            // +0x34 read before Bundle's
    Bundle::enableCachedObject();
    findDriveWheels(&m_driveWheelFirst, &m_driveWheelSecond, this);      // +0x298 +0x29C
    if (aiObject)
        aiObject->init();                                        // +0x48 PE +0x44
    setSprintStamina(1.0f);                                      // ipco +0x104
}

// Weapon and item operations use typed interfaces so dispatch follows each platform's virtual layout.
namespace {

inline void weaponObjectSetPlayer(void* w, IPlayer* player) { static_cast<IWeaponObject*>(w)->setPlayer(player); }
inline void weaponObjectEnableWeapon(void* w)               { static_cast<IWeaponObject*>(w)->enableWeapon(); }
inline void weaponObjectDisableWeapon(void* w)              { static_cast<IWeaponObject*>(w)->disableWeapon(); }
inline int  itemObjectGetItemIndex(void* i)                 { return static_cast<IItemObject*>(i)->getItemIndex(); }

} // anonymous namespace

// Adds a weapon, assigns its network-safe index and groups it by fire input.
// Weapons with an unrecognized fire input remain in the main list without a firing slot.
void PlayerControlObject::addWeapon(IObject* weapon)
{
    IWeaponObject* weaponObject = 0;
    if (weapon)
        weaponObject = static_cast<IWeaponObject*>(weapon->queryInterface(IID_IWeaponObject));

    if (weaponObject)
        weaponObject->setNetworkSafeItemIndex(static_cast<int>(m_weapons.size()));   // before the add

    m_weapons.push_back(weapon);

    if (!weaponObject)
        return;

    if (weaponObject->getFireInput() == io::PIFire)                  // 8
        m_slotWeapons[0].push_back(weapon);
    else if (weaponObject->getFireInput() == io::PIAltFire)          // 31 - fetch #2
        m_slotWeapons[1].push_back(weapon);
    else if (weaponObject->getFireInput() == io::PIFlareFire)        // 47 - fetch #3
        m_slotWeapons[2].push_back(weapon);
}

void PlayerControlObject::disableWeapons()
{
    for (std::vector<IObject*>::iterator it = m_weapons.begin(); it != m_weapons.end(); ++it)
    {
        void* weapon = (*it)->queryInterface(world::IID_IWeaponObject);
        if (weapon)
        {
            weaponObjectSetPlayer(weapon, 0);            // ELF IWeaponObject +0x68
            weaponObjectDisableWeapon(weapon);           // ELF IWeaponObject +0x184
        }
    }
    m_activeWeapons[0] = m_activeWeapons[1] = m_activeWeapons[2] = 0;
}

// reset, drop the dynamic spawn group, skip bundle to SimpleObject::destroy (the family
// note, once more).
void PlayerControlObject::destroy(bool force)
{
    reset();
    if (m_spawnGroupId != -1)
        static_cast<SpawnManager*>(spawnManager)->removeGroup(m_spawnGroupId);   // +0x24
    SimpleObject::destroy(force);
}

// the trivially-read accessors of the vehicle surface
int  PlayerControlObject::getPcoId() const           { return m_pcoId; }
// only assigns if the id is still unauthored (-1): a .con-authored pcoId can never be
// overwritten by postInitPcoId's allocator. As shipped.
void PlayerControlObject::setPcoId(int id)           { if (m_pcoId == -1) m_pcoId = id; }
std::map<int, ICameraObject*>* PlayerControlObject::getOwnCameras() { return &m_ownCameras; }

// Returns this seat's cameras, or delegates to the occupying object when the local map is empty.
// Delegation requires a player-control object in the occupying hierarchy.
std::map<int, ICameraObject*>* PlayerControlObject::getCameras()
{
    IObject* occupying = m_occupyingObject
        ? reinterpret_cast<WeakPtr<IObject>::Ctrl*>(m_occupyingObject)->target : 0;   // ctrl +4
    if (!occupying)
        return &m_ownCameras;
    if (!m_ownCameras.empty())                                                  // +0x164 != 0
        return &m_ownCameras;
    IPlayerControlObject* pco = 0;
    for (IObject* o = occupying; o; o = o->getParent())                         // +0x28 walk
    {
        pco = static_cast<IPlayerControlObject*>(o->queryInterface(IID_IPlayerControlObject));
        if (pco)
            break;
    }
    return pco->getCameras();                                                   // ipco +0x38, unchecked
}

// nextCamera - ELF 0x819ADF0, 155 B / PE 0x5940A0, 64 B. An occupied PCO with
// no cameras of its own delegates to the nearest PCO at or above its occupied object. Every other
// case increments the otherwise opaque +0x16C value, wraps it against the signed camera count,
// and dispatches setActiveCamera -- including the empty-map case, whose callee emits the shipped
// "TBM: No camera!" diagnostic. The second occupying-object read and unchecked parent PCO are
// retained as shipped.
void PlayerControlObject::nextCamera()
{
    IObject* const firstOccupying = m_occupyingObject
        ? reinterpret_cast<WeakPtr<IObject>::Ctrl*>(m_occupyingObject)->target : 0;
    if (firstOccupying && m_ownCameras.empty())
    {
        IObject* const occupying = m_occupyingObject
            ? reinterpret_cast<WeakPtr<IObject>::Ctrl*>(m_occupyingObject)->target : 0;
        IPlayerControlObject* pco = 0;
        for (IObject* object = occupying; object; object = object->getParent())
        {
            pco = static_cast<IPlayerControlObject*>(
                object->queryInterface(IID_IPlayerControlObject));
            if (pco)
                break;
        }
        pco->nextCamera();                                                       // unchecked
        return;
    }

    int cameraId = _dword16C + 1;
    if (cameraId >= static_cast<int>(m_ownCameras.size()))
        cameraId = 0;
    setActiveCamera(cameraId);
}

// addExternalCamera / removeExternalCamera - ELF 0x819B210 / 0x819B260 (79 / 155 B), PE
// / 0x595970 (50 / 65 B). Both ask the camera for its id exactly once. Insert is unique,
// so a duplicate id keeps the existing pointer; removal erases by id without comparing the stored
// pointer to the supplied camera.
void PlayerControlObject::addExternalCamera(ICameraObject* camera)
{
    m_ownCameras.insert(std::make_pair(camera->getCameraId(), camera));
}

void PlayerControlObject::removeExternalCamera(ICameraObject* camera)
{
    m_ownCameras.erase(camera->getCameraId());
}

std::map<int, IPlayerControlObject*>* PlayerControlObject::getPcos() { return &m_pcos; }
int  PlayerControlObject::getDefaultTeam()           { return m_defaultTeam; }

// addPlayerInputObject / removePlayerInputObject - ELF 0x819D510 / 0x819D590 (116 / 121 B), PE
// / 0x5961B0 (154 / 72 B). The list is pointer-unique: add appends only when absent,
// while remove erases only the first matching node.
void PlayerControlObject::addPlayerInputObject(IObject* object)
{
    for (std::list<IObject*>::const_iterator it = m_playerInputObjects.begin();
         it != m_playerInputObjects.end(); ++it)
    {
        if (*it == object)
            return;
    }
    m_playerInputObjects.push_back(object);
}

void PlayerControlObject::removePlayerInputObject(IObject* object)
{
    for (std::list<IObject*>::iterator it = m_playerInputObjects.begin();
         it != m_playerInputObjects.end(); ++it)
    {
        if (*it == object)
        {
            m_playerInputObjects.erase(it);
            return;
        }
    }
}

// Returns the active weapon for a firing slot, delegating to the occupying soldier when configured.

// (getWeapons() - the no-arg flat-vector accessor - was already written in the template-field
// batch further down, 0x8180DB0.)

IObject* PlayerControlObject::getActiveWeapon(int index) const
{
    if (!isChildSoldierActive())                         // virtual on self, +0x140
        return m_activeWeapons[index];                   // +0x1C4

    IObject* const occupant = m_occupyingObject
        ? reinterpret_cast<WeakPtr<IObject>::Ctrl*>(m_occupyingObject)->target : 0;
    return static_cast<Soldier*>(occupant)->getActiveWeapon(index);   // Soldier +0x1CC; unchecked
}

// . void* return, [CHOSEN] - the callers cast it to std::vector<IObject*>*.
void* PlayerControlObject::getWeapons(int index)
{
    if (!isChildSoldierActive())
        return &m_slotWeapons[index];                    // +0x1A0 + 12 * index

    IObject* const occupant = m_occupyingObject
        ? reinterpret_cast<WeakPtr<IObject>::Ctrl*>(m_occupyingObject)->target : 0;
    return static_cast<Soldier*>(occupant)->getWeapons(index);        // Soldier +0x1D0; unchecked
}

// Selects a weapon already registered in the requested slot; unknown weapons are ignored.
// Disables the previous weapon, enables the replacement and updates local item-pickup HUD state.
void PlayerControlObject::setActiveWeapon(IObject* newWeapon, int slot)
{
    if (isChildSoldierActive())
    {
        IObject* const occupant = m_occupyingObject
            ? reinterpret_cast<WeakPtr<IObject>::Ctrl*>(m_occupyingObject)->target : 0;
        static_cast<Soldier*>(occupant)->setActiveWeapon(newWeapon, slot);   // Soldier +0x1C8
        return;
    }

    void* const newWeaponObject =
        newWeapon ? newWeapon->queryInterface(world::IID_IWeaponObject) : 0;
    IObject* const current = getActiveWeapon(slot);
    void* const currentWeaponObject =
        current ? current->queryInterface(world::IID_IWeaponObject) : 0;
    if (newWeaponObject == currentWeaponObject)
        return;                                          // same weapon (or both null) - nothing

    std::vector<IObject*>& weapons = m_slotWeapons[slot];
    for (unsigned int i = 0; i < weapons.size(); ++i)
    {
        if (weapons[i] != newWeapon)
            continue;

        if (getActiveWeapon(slot))                       // re-tested, as shipped
        {
            IObject* const old = getActiveWeapon(slot);  // and re-read
            void* const oldWeaponObject =
                old ? old->queryInterface(world::IID_IWeaponObject) : 0;
            weaponObjectDisableWeapon(oldWeaponObject);  // ELF +0x184 - unchecked in the ELF
        }

        m_activeWeapons[slot] = newWeapon;               // +0x1C4 the store

        void* const weaponObject =
            newWeapon ? newWeapon->queryInterface(world::IID_IWeaponObject) : 0;
        weaponObjectEnableWeapon(weaponObject);          // ELF +0x180 - unchecked in the ELF

        // the item-pickup HUD tail: the local human only - dead on a dedicated server
        void* const item = newWeapon->queryInterface(world::IID_IItemObject);
        if (item)
        {
            IPlayer* const player = m_occupyingPlayer
                ? reinterpret_cast<WeakPtr<IPlayer>::Ctrl*>(m_occupyingPlayer)->target : 0;
            if (player && player == playerManager->getLocalHumanPlayer())
            {
                int payload[2];
                payload[0] = itemObjectGetItemIndex(item);          // ELF IItemObject +0x30
                payload[1] = static_cast<int>(i);
                eventManager->postEvent(ECHud, 31, this, 0.0f);
                eventManager->postEvent(ECHud, 32, payload, 0.0f);
            }
        }
        return;
    }
}

// On the first ghost, schedules wheel settling after one third of a second and again after five seconds.
void PlayerControlObject::onFirstGhost()
{
    if (m_firstGhost)
    {
        m_firstGhost = false;
        objectManager->postMessage(this, static_cast<TemplateMessage>(70), 0.33333334f);
        objectManager->postMessage(this, static_cast<TemplateMessage>(70), 5.0f);
    }
}

// Vehicle state and template accessors.
void* PlayerControlObject::getActiveAnimationSystem()  { return m_activeAnimationSystem; }   // +0x218

// / 0x8180CF0, 23 B each: the two WeakPtr control blocks' targets (+0x158 / +0x15C -> +4).
IPlayer* PlayerControlObject::getOccupingPlayer() const
{
    return m_occupyingPlayer ? reinterpret_cast<WeakPtr<IPlayer>::Ctrl*>(m_occupyingPlayer)->target : 0;
}

// Dispatches occupant, weapon, input and watchdog messages, forwarding to the occupant before local handling.
// Messages consumed by a specialized handler do not reach Bundle.
void PlayerControlObject::handleMessage(TemplateMessage message, IPlayer* player, bool remote)
{
    bool forwarded = false;

    // 1. down to the occupant, but only if this PCO owns no cameras.
    IObject* occupant = getOccupingObject();                                     // +0x15C
    if (occupant && m_ownCameras.empty())                                        // +0x164 = the count
    {
        occupant->handleMessage(message, player, remote);                        // slot 65
        forwarded = true;
    }

    // 2. ducking swallows the whole rest of the function.
    if (m_isDucking && m_template->m_disableInputWhileDucking
        && message == static_cast<TemplateMessage>(6))
        return;

    // 3. the switch.
    if (message > static_cast<TemplateMessage>(42))
    {
        if (message == static_cast<TemplateMessage>(70))
            snapWheel(this);
    }
    else if (message >= static_cast<TemplateMessage>(41))
    {
        if (!remote)
            m_sprintState.handleTMSprint(message == static_cast<TemplateMessage>(41));
        forwarded = true;                                                        // Bundle is skipped
    }
    else
    {
        if (message == static_cast<TemplateMessage>(6))
        {
            for (int i = 0; i <= 2; ++i)
                if (getActiveWeapon(i))
                    getActiveWeapon(i)->handleMessage(static_cast<TemplateMessage>(6),
                                                      player, remote);
            return;                                                              // Bundle never sees 6
        }
        if (message == static_cast<TemplateMessage>(29))
        {
            updateTimeToLiveUnused();                                            // recovered, 974 B
            return;                                                              // nor 29
        }
    }

    // 4. someone else's seat: only a "wrecked" (24) gets through, and it evicts them.
    if (player && player->getInputId() != getPcoId())                            // +0x50 vs +0x154
    {
        if (message != static_cast<TemplateMessage>(24))
            return;
        disableWeapons();
        if (getOccupingObject() && gameLogic->isServer())
        {
            IObject* victim = getOccupingObject();
            exit();                                                              // recovered shared exit transaction
            game->destroyObject(victim);                                         // IGame +0xA0
        }
        Bundle::handleMessage(message, player, remote);
        return;
    }

    // 5. the same eviction for the rightful owner, then the base.
    if (message == static_cast<TemplateMessage>(24))
    {
        disableWeapons();
        if (getOccupingObject() && gameLogic->isServer())
        {
            IObject* victim = getOccupingObject();
            exit();
            game->destroyObject(victim);
        }
    }

    if (!forwarded)
        Bundle::handleMessage(message, player, remote);
}


// Handles kill-material collisions by applying fatal server damage and consuming the collision.
// Other materials use Bundle collision handling.
bool PlayerControlObject::handleCollision(IObject* other, const Vec3& position, const Vec3& normal,
                                          const Vec3& velocity, int material, int otherMaterial,
                                          CollisionFaceInfo* faceA, CollisionFaceInfo* faceB)
{
    if (otherMaterial != 666)
        return Bundle::handleCollision(other, position, normal, velocity, material, otherMaterial,
                                       faceA, faceB);
    if (gameLogic->isServer())
    {
        Vec3 zero; zero.x = 0.0f; zero.y = 0.0f; zero.z = 0.0f;
        Vec3 up;   up.x = 0.0f;   up.y = 1.0f;   up.z = 0.0f;
        gameLogic->giveDamage(this, 1.0e10f, 0, 0, 0, -1, 0, 0, zero, up, -1, false, true, false);   // IGameLogic +0xE4
    }
    return true;
}

void PlayerControlObject::updateTimeToLiveUnused()
{
    // Occupied: nothing to do for another five seconds.
    if (vehicleIsUsed())
    {
        objectManager->postMessage(this, static_cast<TemplateMessage>(29), 5.0f);
        return;
    }

    const ObjectSpawnerTemplate* spawnerTemplate = 0;
    IObject* spawner = gameLogic->getObjectSpawner(m_objectSpawnerId);            // IGameLogic +0x1E8
    if (spawner)
    {
        if (spawner->getTemplate()->getClassID() == CID_ObjectSpawnerTemplate)
            spawnerTemplate = static_cast<const ObjectSpawnerTemplate*>(spawner->getTemplate());

        // the distance is computed even when the template is not a spawner's - as compiled.
        const Vec3& home = spawner->getAbsolutePosition();                       // slot 27
        const Vec3& here = getAbsolutePosition();
        const float dx = here.x - home.x, dy = here.y - home.y, dz = here.z - home.z;
        const float distance = sqrtf(dz * dz + dy * dy + dx * dx);

        if (spawnerTemplate
            && const_cast<ObjectSpawnerTemplate*>(spawnerTemplate)->getDistance() > distance)   // +0x150
        {
            objectManager->postMessage(this, static_cast<TemplateMessage>(29), 5.0f);
            m_timeToLiveUnused = m_timeToLiveUnusedReset;                        // +0x1EC <- +0x1F0
            return;
        }
    }

    // Requires the vehicle's armor template.
    const void* armor = m_template->getComponentFromClassID(CID_ArmorGLCompTemplate);
    const float radius = const_cast<ArmorGLCompTemplate*>(
                             static_cast<const ArmorGLCompTemplate*>(armor))->getExplosionRadius(); // +0x60
    const float radius3sq = (radius * 3.0f) * (radius * 3.0f);

    const std::list<IPlayer*>& players = playerManager->getPlayers();
    for (std::list<IPlayer*>::const_iterator it = players.begin(); it != players.end(); ++it)
    {
        IPlayer* player = *it;
        if (!player->getIsAlive())                                               // +0x6C
            continue;
        IObject* vehicle = player->getVehicle();                                 // +0x48
        if (vehicle->getTemplate()->getClassID() != CID_SoldierTemplate)
            continue;

        const Vec3& here = getAbsolutePosition();
        const Vec3& them = vehicle->getAbsolutePosition();
        const float dx = them.x - here.x, dy = them.y - here.y, dz = them.z - here.z;
        if (radius3sq > dz * dz + dy * dy + dx * dx)
        {
            objectManager->postMessage(this, static_cast<TemplateMessage>(29), 5.0f);
            return;                                                              // somebody is near
        }
    }

    // Abandoned and away from home: bleed the clock.
    objectManager->postMessage(this, static_cast<TemplateMessage>(29), 0.5f);
    m_timeToLiveUnused -= 0.5f;
    if (m_timeToLiveUnused <= 0.0f)
    {
        float damage = 0.5f;
        if (spawnerTemplate)
            damage = const_cast<ObjectSpawnerTemplate*>(spawnerTemplate)->getDamageWhenLost() * 0.5f;   // +0x198

        Vec3 position; position.x = 0.0f; position.y = 0.0f; position.z = 0.0f;
        Vec3 direction; direction.x = 0.0f; direction.y = 1.0f; direction.z = 0.0f;
        gameLogic->giveDamage(this, damage, 0, 0, 0, -1, 0, 0,
                              position, direction, -1, false, true, false);      // IGameLogic +0xE4
    }
}

// Returns the occupying object's active camera when this seat has no cameras, otherwise its cached camera.
// The occupying hierarchy must contain a player-control object.
ICameraObject* PlayerControlObject::getNonCommanderCamera()
{
    IObject* const occupant = m_occupyingObject
        ? reinterpret_cast<WeakPtr<IObject>::Ctrl*>(m_occupyingObject)->target : 0;
    if (occupant && m_ownCameras.empty())
    {
        IPlayerControlObject* pco = 0;
        for (IObject* object = occupant; object;
             object = object_detail::LayoutAccess::parent(object))
        {
            pco = static_cast<IPlayerControlObject*>(object->queryInterface(IID_IPlayerControlObject));
            if (pco)
                break;
        }
        ICameraObject* const camera = pco->getActiveCamera();
        if (camera)
            return camera;
    }
    return m_activeCamera;
}

} // namespace world

int SprintState::handleUpdate(bool disable, float sprintFactor, float dt)
{
    int result = 71;
    bool disabled = false;
    if (FLT_EPSILON >= m_constant0 || disable)
        disabled = true;

    if (m_isSprinting)
    {
        float dissipation = m_staminaDissipationFactor;
        if (serverSettings->getNoVehicles())
            dissipation *= 0.5f;
        float stamina = m_stamina - dissipation * dt / m_constant0;
        if (0.0f > stamina)
            stamina = 0.0f;
        m_stamina = stamina;
        if (!m_sprintRequested || !(stamina > 0.0f))
            m_isSprinting = false;
        if (disabled)
            m_isSprinting = false;
        if (!m_isSprinting)
            result = 43;
    }
    else
    {
        if (!(m_constant1 > 0.0f))
        {
            m_stamina = 1.0f;
        }
        else if (!(sprintFactor > 0.0f))
        {
            float stamina = m_stamina + dt / m_constant1;
            if (1.0f < stamina)
                stamina = 1.0f;
            m_stamina = stamina;
        }
        if (m_sprintRequested && !(m_stamina < m_constant2) && !disabled)
        {
            m_isSprinting = true;
            result = 42;
        }
    }
    m_sprintRequested = false;
    return result;
}

namespace world {

// Returns an ammo gauge from 0 to 15, or 15 when no finite-ammo weapon qualifies.
// Successive weapon ratios are averaged pairwise, so weapon order affects the result.
int PlayerControlObject::calcAmmoStatus()
{
    float running = 1.0f;
    bool first = true;
    for (std::vector<IObject*>::iterator it = m_weapons.begin(); it != m_weapons.end(); ++it)
    {
        IWeaponObject* weapon = static_cast<IWeaponObject*>(
            (*it)->queryInterface(IID_IWeaponObject));
        if (weapon->reloadAmmoPercent(100.0f, true) == 100.0f)
            continue;
        if (weapon->getMagSize() == -1)
            continue;
        const float a = static_cast<float>(weapon->getCurrentNrOfMags());
        const float b = static_cast<float>(weapon->getMaxNrOfMags());
        if (a == -1.0f || b == -1.0f)
            continue;
        if (first)
            running = a / b;
        else
            running = (a / b + running) * 0.5f;
        first = false;
    }
    float v = running * 15.0f;
    if (0.0f > v)
        v = 0.0f;
    else if (v > 15.0f)
        v = 15.0f;
    return static_cast<int>(std::floor(v));
}

// . True only when this PCO has NO cameras of its own (m_ownCameras empty - the
// +0x164 node count, see the layout note), an occupying player, and an occupying object whose
// template is a soldier's.
bool PlayerControlObject::isChildSoldierActive() const
{
    if (!m_ownCameras.empty())
        return false;
    IPlayer* player = m_occupyingPlayer
        ? reinterpret_cast<WeakPtr<IPlayer>::Ctrl*>(m_occupyingPlayer)->target : 0;
    if (!player)
        return false;
    IObject* occupying = m_occupyingObject
        ? reinterpret_cast<WeakPtr<IObject>::Ctrl*>(m_occupyingObject)->target : 0;
    if (!occupying)
        return false;
    return occupying->getTemplate()->getClassID() == CID_SoldierTemplate;
}

// removeChildVehicle - ELF 0x819DE00, 117 B; the PE internalExit arm confirms the same operation.
// It does not touch the object tree: it only drops m_occupyingObject when that weak target is the
// exact object supplied by the caller.
void PlayerControlObject::removeChildVehicle(IObject* object)
{
    IObject* const occupant = m_occupyingObject
        ? reinterpret_cast<WeakPtr<IObject>::Ctrl*>(m_occupyingObject)->target : 0;
    if (occupant == object)
    {
        if (m_occupyingObject && --(*m_occupyingObject) == 0)
            ::operator delete(m_occupyingObject);
        m_occupyingObject = 0;
    }
}

// toggleOnWeaponLock - ELF 0x819D9E0, 135 B / PE 0x5956A0, 89 B. Scan the three per-slot vectors
// in slot order. The first pointer-identical match stops the whole search; only when that object is
// not already active does the method dispatch setActiveWeapon for the matching slot.
void PlayerControlObject::toggleOnWeaponLock(IObject* object)
{
    for (int slot = 0; slot != 3; ++slot)
    {
        for (std::vector<IObject*>::iterator it = m_slotWeapons[slot].begin();
             it != m_slotWeapons[slot].end(); ++it)
        {
            if (*it == object)
            {
                if (getActiveWeapon(slot) != object)
                    setActiveWeapon(object, slot);
                return;
            }
        }
    }
}

// Updates unmanned input, vehicle timeout, sprint, sound, ammunition and occupant state.
// Ticks Bundle components before the entered seat's camera.
namespace {
    // TU statics from PlayerControlObject.cpp's ELF static initializer. The Vars values are
    // console-backed; the regulation pair has no writer outside regulateInput in either
    // shipped image (the bool therefore remains false unless changed under a debugger).
    // ..0x81A1081 / PE 0x76C4C0..0x76C515, including defaults.
    // ELF 81A0FE7 / dedicated PE 76C480: live state-transition delay, default twenty ticks.
    int* const pcoMinimumPlayingTicks = Vars::getInt("pco-min-playing-ticks", 20);
    float* const pcoUnmannedInputTime = Vars::getFloat("pco-unmanned-random-input-time", 10.0f);
    float* const pcoUnmannedMaxMinorAxis = Vars::getFloat("pco-unmanned-max-minor-axis", 0.4f);
    float* const pcoUnmannedMinMajorAxis = Vars::getFloat("pco-unmanned-min-major-axis", 0.6f);
    int* const pcoExitPositionTestLod = Vars::getInt("pco-exitpos-testlod", 2); // ds:0x8AAD5E4
    bool pcoRegulationDebug = false;                                             // ds:0x8AAD640
    int  pcoRegulationTick = 0;                                                  // ds:0x8AAD644

    inline float pcoMappedInput(const io::PlayerInput& input, int inputId)
    {
        // The retail check is one-sided (signed inputId >= 64). Configured ids are 0..64; retain
        // the x86 shift's masked count for the malformed-negative case instead of adding a guard.
        if (inputId >= 64)
            return 0.0f;
        const unsigned int bit = static_cast<unsigned int>(inputId) & 63u;
        return ((input.mappingFlags >> bit) & 1ULL) ? input.inputs[inputId] : 0.0f;
    }

    inline float pcoClampUnit(float value)
    {
        if (value >= 1.0f)
            value = 1.0f;
        if (value < -1.0f)
            value = -1.0f;
        return value;
    }

    inline Vec3 pcoTransformPoint(const Mat4& transform, const Vec3& point)
    {
        Vec3 out;
        out.x = transform.m[0] * point.x + transform.m[4] * point.y +
                transform.m[8] * point.z + transform.m[12];
        out.y = transform.m[1] * point.x + transform.m[5] * point.y +
                transform.m[9] * point.z + transform.m[13];
        out.z = transform.m[2] * point.x + transform.m[6] * point.y +
                transform.m[10] * point.z + transform.m[14];
        return out;
    }

    inline Vec3 pcoRotateIntoLocal(const Mat4& transform, const Vec3& vector)
    {
        Vec3 out;
        out.x = transform.m[0] * vector.x + transform.m[1] * vector.y +
                transform.m[2] * vector.z;
        out.y = transform.m[4] * vector.x + transform.m[5] * vector.y +
                transform.m[6] * vector.z;
        out.z = transform.m[8] * vector.x + transform.m[9] * vector.y +
                transform.m[10] * vector.z;
        return out;
    }

    // PE emits this parent walk as sub_433340; the ELF inlines the identical loop at each use.
    inline IPlayerControlObject* pcoFindParentControlObject(IObject* object)
    {
        while (object)
        {
            IPlayerControlObject* const pco = static_cast<IPlayerControlObject*>(
                object->queryInterface(IID_IPlayerControlObject));
            if (pco)
                return pco;
            object = object->getParent();
        }
        return 0;
    }

    // The exit transaction's shipped bodies read the two WeakPtr control blocks directly rather
    // than dispatching the public occupying-player/object virtuals. Keep each call site separate:
    // several of the PE and ELF paths deliberately reload the target after observable work.
    template <class T>
    inline T* pcoWeakTarget(int* control)
    {
        return control ? reinterpret_cast<typename WeakPtr<T>::Ctrl*>(control)->target : 0;
    }
}

// GroundContactSound::updateState - ELF 81A2430; PE 592D30 with change helper 592C80.
// The client ctor (5F4CB0) supplies the embedded fixed node and controller layout, but no
// allocation branch is added to this dedicated owner. State 0 delays; unknown states do nothing.
void GroundContactSound::updateState()
{
    if (m_state == 0)
    {
        m_ticks = static_cast<int>(static_cast<unsigned int>(m_ticks) + 1u);
        if (m_ticks >= *pcoMinimumPlayingTicks)
            m_state = 1;
        return;
    }
    if (m_state != 1)
        return;

    if (m_controller.m_value > FLT_EPSILON && m_materialChanged)
    {
        m_materialChanged = false;
        m_state = 0;
        m_ticks = 0;
        SoundObjectTemplate* const soundTemplate = static_cast<SoundObjectTemplate*>(
            static_cast<MaterialManager*>(materialManager)->getSoundTemplate(static_cast<unsigned int>(m_otherMaterial),
                                             static_cast<unsigned int>(m_material), 0));
        bool replace = !soundTemplate;
        if (soundTemplate)
        {
            // [PLATFORM] PE asks the object-template implementation first; ELF asks the
            // current sound first. Both retain a fresh impl() call when replacement occurs.
#if defined(_WIN32)
            ISoundTemplate* const wanted = static_cast<ISoundObjectTemplate*>(soundTemplate)->impl();
            replace = m_sound->getTemplate() != wanted;
#else
            ISoundTemplate* const current = m_sound->getTemplate();
            replace = current != soundTemplate->impl();
#endif
        }
        if (replace)
        {
            m_sound->stop();
            m_sound->release();
            if (soundTemplate)
            {
                IObject* const owner = m_node.m_object;
                ISoundTemplate* const impl = static_cast<ISoundObjectTemplate*>(soundTemplate)->impl();
                m_sound = g_bfSound->createSound(impl, owner);
                m_sound->setParent(&m_node);
                m_sound->setVolumeEvaluator(0, &m_controller.m_value);
                m_sound->setPitchEvaluator(0, &m_controller.m_value);
            }
            else
            {
                m_sound = g_nullSound;
                m_sound->addRef();
            }
        }
        m_sound->play(0);
    }
    else if (m_controller.m_value <= FLT_EPSILON)
        m_sound->stop();
    else if (!m_sound->isPlaying())
        m_sound->play(); // Unordered value comparisons take this arm in both retail images.
}

// setListenerEnvironment - ELF 0x819BD40, 321 B / PE 0x594220, 274 B. The bool return is explicit
// in the PE epilogues. Only the local human player's PCO changes the listener. First-person entry
// selects the template obstruction and ambient sound, otherwise the live obstruction is cleared;
// the wind direction is intentionally inverted (zero obstruction starts wind, non-zero stops it).
// Both engine() calls are retained: retail re-asks the facade before each opaque engine vcall.
bool PlayerControlObject::setListenerEnvironment(bool enabled)
{
    if (!pcoWeakTarget<IPlayer>(m_occupyingPlayer))
        return false;
    if (pcoWeakTarget<IPlayer>(m_occupyingPlayer) != playerManager->getLocalHumanPlayer())
        return false;

    void* ambientSound = 0;
    float obstruction = 0.0f;
    if (enabled && pcoWeakTarget<IPlayer>(m_occupyingPlayer)->hasFirstPersonView())
    {
        obstruction = m_template->getListenerObstruction();
        ambientSound = m_ambientSoundFromTemplate2;
        if (std::fabs(obstruction) <= FLT_EPSILON)
            g_bfSound->startWind();
        else
            g_bfSound->stopWind();
    }
    else
    {
        m_listener3DObstruction = 0.0f;
        g_bfSound->stopWind();
    }

    g_bfSound->engine()->setProperty("listener-3d-obstruction", obstruction);
    g_bfSound->engine()->setTrigger(static_cast<ISound*>(ambientSound));
    return true;
}

// ELF 819AD00 / PE 594AE0, full paired reads. Only the local occupant with a damaged
// ambient sound can switch. The selected sound is cached before reapplying the listener.
void PlayerControlObject::updateAmbientSound()
{
    if (!pcoWeakTarget<IPlayer>(m_occupyingPlayer))
        return;
    IPlayer* const occupant = pcoWeakTarget<IPlayer>(m_occupyingPlayer);
    if (occupant != playerManager->getLocalHumanPlayer() || !m_ambientDamagedSoundFromTemplate)
        return;

    IArmorGLComp* const armor = static_cast<IArmorGLComp*>(
        object_detail::LayoutAccess::armorComponent(getRootParent(this)));
    void* selected = m_ambientSoundFromTemplate;
    if (armor)
    {
        // [PLATFORM] PE loads the template limit before getHitPoints; ELF loads it afterward.
#if defined(_WIN32)
        const float limit = m_template->m_damagedAmbientSoundLimit;
        const float hitPoints = armor->getHitPoints();
#else
        const float hitPoints = armor->getHitPoints();
        const float limit = m_template->m_damagedAmbientSoundLimit;
#endif
        if (armor->getMaxHitPoints() * limit > hitPoints)
            selected = m_ambientDamagedSoundFromTemplate;
    }
    if (selected != m_ambientSoundFromTemplate2)
    {
        m_ambientSoundFromTemplate2 = selected;
        setListenerEnvironment(true);
    }
}

// ELF 819E6C0 / PE 594820. The ISoldier getOccupingPlayer slot differs (+30/+34),
// while getKit is +54 on both. Repeated player queries and unguarded later answers remain.
void PlayerControlObject::setProperKitGeometry()
{
    if (!pcoWeakTarget<IObject>(m_occupyingObject))
        return;
    ISoldier* const soldier = static_cast<ISoldier*>(
        pcoWeakTarget<IObject>(m_occupyingObject)->queryInterface(IID_ISoldier));
    if (!soldier)
        return;

    bool useDefault = false;
    if (soldier->getOccupingPlayer())
    {
        if (soldier->getOccupingPlayer()->getIsGasMaskOn() ||
            soldier->getOccupingPlayer()->getNightVision())
            useDefault = true;
    }
    Kit* const kit = soldier->getKit();
    if (kit)
    {
        if (useDefault)
        {
            if (kit->m_geomIndex == m_template->m_crewKitIndex)
                kit->setGeomIndex(-1);
        }
        else if (kit->m_geomIndex != m_template->m_crewKitIndex)
            kit->setGeomIndex(m_template->m_crewKitIndex);
    }
}

void PlayerControlObject::handleUpdate(float deltaTime, unsigned int flags)
{
    // 1.
    if (m_giveUnmannedInput)
    {
        if (gameLogic->isServer() && !isOccupied())
        {
            IPhysicsNode* node = static_cast<IPhysicsNode*>(
                object_detail::LayoutAccess::physicsNode(getRootParent(this)));
            if (node && !node->getIsSleeping())
            {
                io::PlayerInput& input = reinterpret_cast<io::PlayerInput&>(_inputState[0]);   // +0x2A4 as the ELF's `lea`
                getUnmannedInput(input);
                handlePlayerInput(0, input, deltaTime, 0);
            }
        }
    }

    // 2.
    if ((flags & 4u) == 0)
    {
        // a. the update timeout
        if (m_handleUpdateTimeout > 0.0f)
        {
            IPhysicsNode* node = static_cast<IPhysicsNode*>(object_detail::LayoutAccess::physicsNode(this));
            if (node)
            {
                if (node->getIsSleeping())
                {
                    m_handleUpdateTimeout = -1.0f;
                    updateFlags(0, 0x800u);
                    return;
                }
            }
            else
            {
                m_handleUpdateTimeout -= deltaTime;
                if (!(m_handleUpdateTimeout > 0.0f))     // `fucomip; jb` on the OLD value vs dt: ran out
                {
                    m_handleUpdateTimeout = -1.0f;
                    updateFlags(0, 0x800u);
                }
            }
        }

        // b. sprint
        if (getSprintFactor() > 0.0f)
        {
            const int message = m_sprintState.handleUpdate(false, 0.0f, deltaTime);   // 0.0: see the note
            if (static_cast<unsigned int>(message - 42) <= 1u)
                Bundle::handleMessage(static_cast<TemplateMessage>(message), 0, false);
        }

        // c. the sound block - dead on a dedicated server, written behind its gates
        if (m_ambientSoundFromTemplate || m_ambientDamagedSoundFromTemplate || m_groundContactSound)
        {
            IPhysicsNode* node = static_cast<IPhysicsNode*>(
                object_detail::LayoutAccess::physicsNode(getRootParent(this)));
            if (node)
            {
                const Vec3& v = node->getAbsolutePositionalSpeed();               // slot +0x14C
                float speed = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);

                float obstruction = std::fabs(speed) - 0.1f;
                if (obstruction < 0.0f) obstruction = 0.0f;
#if defined(_WIN32)
                obstruction *= 0.033333335f;
#else
                obstruction /= 30.0f;
#endif
                if (obstruction > 1.0f) obstruction = 1.0f;
                m_listener3DObstruction = obstruction;
            }
        }
        IPlayer* occupant = pcoWeakTarget<IPlayer>(m_occupyingPlayer);
        if (occupant && occupant == playerManager->getLocalHumanPlayer())
        {
            ICameraObject* camera = getActiveCamera();
            if (camera)
            {
                const int environment = static_cast<int>(camera->getViewMode());   // +0x1C
                if (environment != _dword278 && setListenerEnvironment(true))
                    _dword278 = environment;
            }
            if (m_ambientSoundFromTemplate || m_ambientDamagedSoundFromTemplate)
                updateAmbientSound();
        }
        if (m_groundContactSound)
        {
            int second = -1;
            int first = getWheelMaterials(m_driveWheelFirst, &second);    // +0x298/+0x29C - typed
            int material = getWheelMaterials(m_driveWheelSecond, 0);
            if (first > 0) material = first;
            GroundContactSound* const sound = m_groundContactSound;
            sound->m_hasContact = (material >= 0);
            if (material >= 0 && (sound->m_otherMaterial != second || sound->m_material != material))
            {
                sound->m_material = material;
                sound->m_otherMaterial = second;
                sound->m_materialChanged = true;
            }
            sound->m_input = sound->m_hasContact ? m_listener3DObstruction : 0.0f;
            sound->m_controller.update(1);
            sound->updateState();
        }
    }

    // 3.
    if (gameLogic->isServer())
        m_ammoStatus = calcAmmoStatus();

    // 4.
    if (isChildSoldierActive())
    {
        IObject* occupying = pcoWeakTarget<IObject>(m_occupyingObject);
        occupying->handleUpdate(deltaTime, flags);                                 // slot 44
    }

    // 5.
    SimpleObject::handleUpdate(deltaTime, flags);

    // 6.
    if (m_entered)
    {
        ICameraObject* camera = getActiveCamera();
        IObject* cameraObject = static_cast<IObject*>(camera->queryInterface(IID_IObject));   // no null test on the camera - as shipped
        if (cameraObject)
            cameraObject->handleUpdate(deltaTime, flags);
    }

    // 7.
    if (static_cast<const PlayerControlObjectTemplate*>(m_template)->m_crewKitIndex != -1)
        setProperKitGeometry();
}

// Regulates mapped pitch/yaw/roll/vertical inputs, applying only corrections toward zero.
// Vertical correction includes gravity and attitude fade; ducking is cleared even without physics.
void PlayerControlObject::regulateInput(io::PlayerInput& playerInput, float deltaTime) const
{
    IPhysicsNode* const physicsNode = static_cast<IPhysicsNode*>(
        object_detail::LayoutAccess::physicsNode(getRootParent(this)));
    if (physicsNode)
    {
        const PlayerControlObjectTemplate* const objectTemplate = m_template;
        const int inputIds[4] = {
            objectTemplate->m_regulatePitchInput,
            objectTemplate->m_regulateYawInput,
            objectTemplate->m_regulateRollInput,
            objectTemplate->m_regulateVerticalPosInput
        };

        if (inputIds[0] != io::PINone || inputIds[1] != io::PINone ||
            inputIds[2] != io::PINone || inputIds[3] != io::PINone)
        {
            float current[4] = {
                pcoMappedInput(playerInput, inputIds[0]),
                pcoMappedInput(playerInput, inputIds[1]),
                pcoMappedInput(playerInput, inputIds[2]),
                pcoMappedInput(playerInput, inputIds[3])
            };
            const float scale[4] = {
                objectTemplate->m_regulatePitch.x,
                objectTemplate->m_regulateYaw.x,
                objectTemplate->m_regulateRoll.x,
                objectTemplate->m_regulateVerticalPos.x
            };
            const float accelerationScale[4] = {
                objectTemplate->m_regulatePitch.y,
                objectTemplate->m_regulateYaw.y,
                objectTemplate->m_regulateRoll.y,
                objectTemplate->m_regulateVerticalPos.y
            };

            const Vec3& rotationalSpeedRef = physicsNode->getRotationalSpeed();
            const Vec3 rotationalSpeed = rotationalSpeedRef;
            const float inverseDeltaTime = 1.0f / deltaTime;
            const Vec3 rotationalSpeedPerDelta = {
                rotationalSpeed.x * inverseDeltaTime,
                rotationalSpeed.y * inverseDeltaTime,
                rotationalSpeed.z * inverseDeltaTime
            };

            // Retail copies the matrix before the later physics-node calls.
            const Mat4 transformation = getRootParent(this)->getAbsoluteTransformation();
            const Vec3 localRotationalSpeed =
                pcoRotateIntoLocal(transformation, rotationalSpeed);
            const float positionalSpeed = physicsNode->getPositionalSpeed().y;
            const Vec3 localRotationalSpeedPerDelta =
                pcoRotateIntoLocal(transformation, rotationalSpeedPerDelta);
            const float positionalAcceleration = physicsNode->getPositionalAcceleration().y;

            const float localSpeed[4] = {
                localRotationalSpeed.x,
                localRotationalSpeed.y,
                localRotationalSpeed.z,
                positionalSpeed
            };
            const float localAcceleration[4] = {
                localRotationalSpeedPerDelta.x,
                localRotationalSpeedPerDelta.y,
                localRotationalSpeedPerDelta.z,
                positionalAcceleration
            };
            float regulated[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

            for (int axis = 0; axis != 4; ++axis)
            {
                if (inputIds[axis] == io::PINone)
                    continue;

                if (axis == 3)
                {
                    const float maxAngleCos = cosf(
                        objectTemplate->m_maxVertRegAngle * 6.2831855f / 360.0f);
                    const float noAngleCos = cosf(
                        objectTemplate->m_noVertRegAngle * 6.2831855f / 360.0f);
                    const float up = transformation.m[5];
                    if (up > noAngleCos)
                    {
                        // The global system is fetched before the node's modifier in both ports.
                        const float gravity = static_cast<IPhysicsSystem*>(basicPhysicsSystem)
                                                  ->getGravity();
                        const float gravityModifier = physicsNode->getGravityModifier();
                        regulated[axis] = pcoClampUnit(
                            ((localAcceleration[axis] - gravityModifier * gravity) *
                                 accelerationScale[axis] +
                             localSpeed[axis]) * scale[axis]);
                        if (up < maxAngleCos)
                        {
                            regulated[axis] = (up - noAngleCos) /
                                (maxAngleCos - noAngleCos) * regulated[axis];
                        }
                    }
                }
                else
                {
                    regulated[axis] = pcoClampUnit(
                        (localAcceleration[axis] * accelerationScale[axis] +
                         localSpeed[axis]) * scale[axis]);
                }

                if ((current[axis] > regulated[axis] && current[axis] <= 0.0f) ||
                    (regulated[axis] > current[axis] && current[axis] >= 0.0f))
                {
                    current[axis] = regulated[axis];
                }
                playerInput.setInput(static_cast<io::PlayerInputMap>(inputIds[axis]), current[axis]);
            }

            ++pcoRegulationTick;
            if (pcoRegulationDebug && pcoRegulationTick == 6)
                pcoRegulationTick = 0;
        }
    }

    if (m_isDucking && m_template->m_disableInputWhileDucking)
        std::memset(playerInput.inputs, 0, sizeof(playerInput.inputs));
}

// handlePlayerInput - ELF 0x819BE90, 700 B / PE 0x595DC0, 355 B. The occupied child receives the
// original input first, weapon deviation sees that same original by value, regulation changes a
// private copy, ducking still reads the original, and only then do collected input objects receive
// the regulated copy. The weak target and virtual predicates are deliberately re-read as shipped.
void PlayerControlObject::handlePlayerInput(IPlayer* player, const io::PlayerInput& input,
                                            float deltaTime, unsigned int flags)
{
    if (pcoWeakTarget<IObject>(m_occupyingObject))
        pcoWeakTarget<IObject>(m_occupyingObject)->handlePlayerInput(player, input, deltaTime, 0);

    if ((flags & 4u) == 0)
    {
        for (std::vector<IObject*>::iterator it = m_weapons.begin(); it != m_weapons.end(); ++it)
        {
            IWeaponObject* weapon = 0;
            if (*it)
                weapon = static_cast<IWeaponObject*>((*it)->queryInterface(IID_IWeaponObject));
            if (weapon)
                weapon->updateDeviation(input, false);
        }
    }

    io::PlayerInput regulatedInput = input;
    if (isOccupied())
        regulateInput(regulatedInput, deltaTime);

    if (m_activeAnimationSystem && m_template->m_allowDucking)
    {
        bool ducking = false;
        if ((input.mappingFlags & (1ULL << io::PICrouch)) != 0)
            ducking = input.inputs[io::PICrouch] > 0.5f;
        setIsDucking(ducking);
    }

    for (std::list<IObject*>::iterator it = m_playerInputObjects.begin();
         it != m_playerInputObjects.end(); ++it)
    {
        (*it)->handlePlayerInput(player, regulatedInput, deltaTime, flags);
    }
}

// preHandlePlayerInput - ELF 0x819E4A0, 115 B / PE 0x5947C0, 83 B. Only collected rotational
// bundles participate; the class-id query and virtual call are repeated once per list entry.
void PlayerControlObject::preHandlePlayerInput(io::PlayerInput& input, float deltaTime) const
{
    for (std::list<IObject*>::const_iterator it = m_playerInputObjects.begin();
         it != m_playerInputObjects.end(); ++it)
    {
        if ((*it)->getTemplate()->getClassID() == CID_RotationalBundleTemplate)
            static_cast<RotationalBundle*>(*it)->preHandlePlayerInput(input, deltaTime);
    }
}

IObject* PlayerControlObject::getOccupingObject() const
{
    return m_occupyingObject ? reinterpret_cast<WeakPtr<IObject>::Ctrl*>(m_occupyingObject)->target : 0;
}

// isOccupied - 0x8198050, 140 B: m_entered answers; when it is clear and this PCO occupies an object,
// the nearest ipco at or above that object is asked getOccupingPlayer() (result discarded - the
// ELF's dead call, kept) and m_entered is re-read. So the answer is m_entered either way.
bool PlayerControlObject::isOccupied() const
{
    if (m_entered)
        return true;
    IObject* occupying = getOccupingObject();
    if (occupying)
    {
        IPlayerControlObject* pco = 0;
        for (IObject* o = occupying; o; o = o->getParent())
        {
            pco = static_cast<IPlayerControlObject*>(o->queryInterface(IID_IPlayerControlObject));
            if (pco)
                break;
        }
        if (pco)
            pco->getOccupingPlayer();                                          // +0x10 as shipped, unused
        return m_entered;
    }
    return false;
}

// vehicleIsUsed - 0x819C6B0, 93 B: any pco in m_pcos answering isOccupied() (ipco +0x18).
bool PlayerControlObject::vehicleIsUsed() const
{
    for (std::map<int, IPlayerControlObject*>::const_iterator it = m_pcos.begin(); it != m_pcos.end(); ++it)
        if (it->second->isOccupied())
            return true;
    return false;
}

// enableHandleUpdate - 0x8196BC0, 72 B: on -> the +0x14C timer re-armed (-1.0) and the update flag
// 0x800 raised; off -> unless flags bit 22 (0x400000) is set, the timer is seated at 3.0 (a
// three-second run-out before the update stops - see handleUpdate).
void PlayerControlObject::enableHandleUpdate(bool enable)
{
    if (enable)
    {
        m_handleUpdateTimeout = -1.0f;
        updateFlags(0x800u, 0);
    }
    else if ((m_flags & 0x400000u) == 0)
        m_handleUpdateTimeout = 3.0f;
}

// Sets both unused-lifetime values; the first server-side assignment on a root schedules the watchdog.
void PlayerControlObject::setTimeToLiveUnused(float timeToLive)
{
    if (m_timeToLiveUnusedReset == -1.0f && !m_parent && gameLogic->isServer())
        objectManager->postMessage(this, static_cast<TemplateMessage>(29), 0.5f);   // +0xD0
    m_timeToLiveUnused = timeToLive;                                           // +0x1EC
    m_timeToLiveUnusedReset = timeToLive;                                                    // +0x1F0
}

// updateTeam - 0x819D390, 378 B. Server only, and only when this object's root is a
// PlayerControlObject-templated object: walk the root's pco map (ipco +0x60 getPcos, on the root
// pco - the +0x1CC vcall is the root's primary-table getPcos); every occupied seat's player must
// agree on ONE team (a second, different one is a dice abort()); count the occupants; then, when the
// template's +0x20C flag is clear or someone is aboard, every pco in the map gets setTeam(team)
// (ipco +0x8C) - team = the occupants' team, else this pco's default team.
void PlayerControlObject::updateTeam()
{
    if (!gameLogic->isServer())
        return;
    IObject* root = getRootParent(this);
    if (!root || root->getTemplate()->getClassID() != CID_PlayerControlObjectTemplate)
        return;
    IPlayerControlObject* rootPco = static_cast<IPlayerControlObject*>(root->queryInterface(IID_IPlayerControlObject));
    // (the ELF dispatches the root's primary slot +0x1CC = PlayerControlObject::getPcos on the root
    // object; the interface pointer reaches the same body)
    int team = m_defaultTeam;
    int occupants = 0;
    std::map<int, IPlayerControlObject*>* pcos = rootPco->getPcos();
    for (std::map<int, IPlayerControlObject*>::iterator it = pcos->begin(); it != pcos->end(); ++it)
    {
        if (IPlayer* player = it->second->getOccupingPlayer())                 // +0x10
        {
            if (team > 0 && team != player->getTeam())                         // IPlayer +0xE8
                abort();
            ++occupants;
            team = player->getTeam();
        }
    }
    const PlayerControlObjectTemplate* t = static_cast<const PlayerControlObjectTemplate*>(m_template);
    if (!t->m_dontClearTeamOnExit || occupants > 0)                            // template +0x20C
    {
        for (std::map<int, IPlayerControlObject*>::iterator it = pcos->begin(); it != pcos->end(); ++it)
            it->second->setTeam(team);                                         // ipco +0x8C
    }
}

// The template-field readers, 14..30 B each:
void*  PlayerControlObject::getWeapons()                { return &m_weapons; }                                   // &+0x194
bool   PlayerControlObject::getExitPosition(Vec3& out)  { return _getExitPosition(out, static_cast<ExitPositionType>(0)); }   // primary +0x200 with type 0

// Selects and transforms the authored exit point, mirroring X for types 1/3 and adding 0.5 to Y.
// A zero local point falls back to a parent PCO; returns validity of the local point.
bool PlayerControlObject::_getExitPosition(Vec3& out, ExitPositionType type)
{
    const unsigned int typeValue = static_cast<unsigned int>(type);
    const Vec3* local = ((typeValue - 2u) > 1u)
        ? &m_template->m_soldierExitPosition
        : &m_template->m_altSoldierExitPosition;
    out = *local;

    PlayerControlObject* fallback = 0;
    float x = out.x;
    float y = out.y;
    float z = out.z;
    if (x * x + y * y + z * z <= 0.0f)
    {
        IPlayerControlObject* const parentInterface =
            pcoFindParentControlObject(getRootParent(this));
        PlayerControlObject* const parent = parentInterface
            ? static_cast<PlayerControlObject*>(parentInterface) : 0;
        const PlayerControlObjectTemplate* const parentTemplate =
            parent ? parent->m_template : 0;
        if (parentTemplate)
        {
            fallback = parent;
            local = (typeValue == 2u)
                ? &parentTemplate->m_altSoldierExitPosition
                : &parentTemplate->m_soldierExitPosition;
            out = *local;
            x = out.x;
            y = out.y;
            z = out.z;
        }
        else
        {
            x = out.x;
            y = out.y;
            z = out.z;
        }
    }

    if (typeValue == 1u || typeValue == 3u)
    {
        out.x = -x;
        x = out.x;
    }

    const bool hasPosition = 0.0f < (x * x + y * y + z * z);
    const Mat4& transform = fallback
        ? getRootParent(this)->getAbsoluteTransformation()
        : getAbsoluteTransformation();

    const float localX = out.x;
    const float localY = out.y;
    const float localZ = out.z;
    out.x = transform.m[0] * localX + transform.m[4] * localY +
            transform.m[8] * localZ + transform.m[12];
    out.y = transform.m[1] * localX + transform.m[5] * localY +
            transform.m[9] * localZ + transform.m[13] + 0.5f;
    out.z = transform.m[2] * localX + transform.m[6] * localY +
            transform.m[10] * localZ + transform.m[14];
    return hasPosition;
}

// getExitPosRayTestStart - ELF 0x8198FA0, 564 B / PE 0x593430, 425 B. For a root with response
// physics and collision LOD 2, cast the requested exit point into root-local space and shoot a ray
// from its negation through twice that vector. A bounds hit is transformed back to world space;
// every other path returns the root's absolute position. There are intentionally no null guards on
// the root, collision mesh, or bounding box.
Vec3 PlayerControlObject::getExitPosRayTestStart(const Vec3& exitPosition)
{
    IObject* const root = getRootParent(this);
    if ((object_detail::LayoutAccess::flags(root) & 8u) != 0 && root->hasCollisionLod(2))
    {
        ICollisionMesh* const collisionMesh = static_cast<ICollisionMesh*>(
            object_detail::LayoutAccess::collisionMesh(root));
        const Vec3 localExit = pcoTransformPoint(
            root->getInvAbsoluteTransformation(), exitPosition);

        Ray3 ray;
        ray.origin.x = -localExit.x;
        ray.origin.y = -localExit.y;
        ray.origin.z = -localExit.z;
        ray.direction.x = localExit.x + localExit.x;
        ray.direction.y = localExit.y + localExit.y;
        ray.direction.z = localExit.z + localExit.z;

        Vec3 localHit = { 0.0f, 0.0f, 0.0f };
        if (collisionMesh->getBoundingBox(2)->intersectRay(ray, localHit, 0))
            return pcoTransformPoint(root->getAbsoluteTransformation(), localHit);
    }
    return root->getAbsolutePosition();
}

// Tests restricted exits in authored/mirrored order for obstruction, clearance, and support.
// Keeps the last unsupported but otherwise valid fallback; categories 2/3 skip support checks.
// Restores the LOD override after candidate testing.
bool PlayerControlObject::isAllowedToExit()
{
    if (m_template->m_dontAllowExit)
        return false;

    if (!m_template->m_hasRestrictedExit)
    {
        m_altExitPosition[0] = m_template->m_soldierExitPosition.x;
        m_altExitPosition[1] = m_template->m_soldierExitPosition.y;
        m_altExitPosition[2] = m_template->m_soldierExitPosition.z;

        const Mat4& transformation = getAbsoluteTransformation();
        const float localX = m_altExitPosition[0];
        const float localY = m_altExitPosition[1];
        const float localZ = m_altExitPosition[2];
        m_altExitPosition[0] = transformation.m[0] * localX +
            transformation.m[4] * localY + transformation.m[8] * localZ + transformation.m[12];
        m_altExitPosition[1] = transformation.m[1] * localX +
            transformation.m[5] * localY + transformation.m[9] * localZ + transformation.m[13];
        m_altExitPosition[2] = transformation.m[2] * localX +
            transformation.m[6] * localY + transformation.m[10] * localZ + transformation.m[14];
        return true;
    }

    bool foundValidSoldierPosition = false;
    Vec3 exitPosition = { 0.0f, 0.0f, 0.0f };
    Vec3 fallbackPosition = { 0.0f, 0.0f, 0.0f };
    bool downwardObjectHit = false;

    if (!pcoWeakTarget<IObject>(m_occupyingObject))
        return false;
    ISoldier* const soldier = static_cast<ISoldier*>(
        pcoWeakTarget<IObject>(m_occupyingObject)->queryInterface(IID_ISoldier));
    if (!soldier)
        return false;

    for (int exitType = 0; exitType != 4; ++exitType)
    {
        if (!_getExitPosition(exitPosition, static_cast<ExitPositionType>(exitType)))
            continue;

        const Vec3& absolutePosition = getAbsolutePosition();
        Vec3 outward = {
            exitPosition.x - absolutePosition.x,
            exitPosition.y - absolutePosition.y,
            exitPosition.z - absolutePosition.z
        };
        const float lengthSquared = outward.x * outward.x + outward.y * outward.y +
                                    outward.z * outward.z;
        if (std::fabs(lengthSquared - 1.0f) >= FLT_EPSILON)
        {
            if (std::fabs(lengthSquared) >= FLT_EPSILON)
            {
                const float inverseLength = 1.0f / std::sqrt(lengthSquared);
                outward.x *= inverseLength;
                outward.y *= inverseLength;
                outward.z *= inverseLength;
            }
            else
            {
                outward.x = 0.0f;
                outward.y = 0.0f;
                outward.z = 0.0f;
            }
        }

        const Vec3 rayStart = getExitPosRayTestStart(exitPosition);
        objectManager->setOverrideLineTestLod(*pcoExitPositionTestLod);
        if (!checkIfRayHitAnyObject(rayStart, exitPosition, getRootParent(this), true))
        {
            const float clearance = exitPosition.y - 0.5f;
            if (clearance > static_cast<IHeightmapCluster*>(heightmapCluster)
                                ->getHeightInWorldCoords(exitPosition.x, exitPosition.z))
            {
                // Retail uses one function-local static identity and rewrites its translation for
                // every candidate rather than constructing a fresh matrix.
                static Mat4 soldierTransformation = Mat4::getIdentity();
                soldierTransformation.m[12] = exitPosition.x;
                soldierTransformation.m[13] = exitPosition.y;
                soldierTransformation.m[14] = exitPosition.z;

                if (soldier->testIfValidTransformation(
                        SoldierPose_Stand, soldierTransformation, false))
                {
                    bool acceptPosition = false;
                    if (m_template->m_vehicleCategory == 2 || m_template->m_vehicleCategory == 3)
                    {
                        acceptPosition = true;
                    }
                    else
                    {
                        foundValidSoldierPosition = true;
                        fallbackPosition = exitPosition;

                        Vec3 downwardRayStart = {
                            exitPosition.x + outward.x * 0.25f,
                            exitPosition.y + outward.y * 0.25f,
                            exitPosition.z + outward.z * 0.25f
                        };
                        Vec3 downwardRayEnd = downwardRayStart;
                        downwardRayEnd.y -= 5.0f;
                        Vec3 terrainHit = { 0.0f, 0.0f, 0.0f };
                        Vec3 terrainNormal = { 0.0f, 0.0f, 0.0f };
                        float terrainFraction;
                        unsigned char terrainMaterial;

                        if (static_cast<IHeightmapCluster*>(heightmapCluster)
                                ->intersectRayInWorldCoords(
                                    downwardRayStart, downwardRayEnd, terrainHit,
                                    terrainFraction, terrainNormal, terrainMaterial, true))
                        {
                            acceptPosition = true;
                        }
                        else
                        {
                            downwardObjectHit = checkIfRayHitAnyObject(
                                downwardRayStart, downwardRayEnd, getRootParent(this), true) != 0;
                            if (downwardObjectHit)
                                acceptPosition = true;
                        }
                    }

                    if (acceptPosition)
                    {
                        m_altExitPosition[0] = exitPosition.x;
                        m_altExitPosition[1] = exitPosition.y;
                        m_altExitPosition[2] = exitPosition.z;
                        _byte20C = true;
                        objectManager->setOverrideLineTestLod(-1);
                        return true;
                    }
                }
            }
        }
    }

    objectManager->setOverrideLineTestLod(-1);
    if (!downwardObjectHit && foundValidSoldierPosition)
    {
        m_altExitPosition[0] = fallbackPosition.x;
        m_altExitPosition[1] = fallbackPosition.y;
        m_altExitPosition[2] = fallbackPosition.z;
        _byte20C = true;
        return true;
    }
    return false;
}

bool   PlayerControlObject::getAltExitPosition(Vec3& out)                                                       // +0x200..+0x208, true
{
    out.x = m_altExitPosition[0]; out.y = m_altExitPosition[1]; out.z = m_altExitPosition[2];
    return true;
}
bool   PlayerControlObject::getIsSprinting() const      { return m_sprintState.m_isSprinting; }                  // +0x260+0x16
void   PlayerControlObject::setIsSprinting(bool s)      { m_sprintState.m_isSprinting = s; }
float  PlayerControlObject::getSprintStamina() const    { return m_sprintState.m_stamina; }                      // +0x260+0x10
void   PlayerControlObject::setSprintStamina(float f)   { m_sprintState.setStamina(f); }                         // the tail jump
void   PlayerControlObject::setSprintStaminaDissipationFactor(float f) { m_sprintState.m_staminaDissipationFactor = f; }   // +0x260+0x0C
// addDriverDecalTransformation - ELF 0x81810F0, 247 B / PE 0x57A860, 118 B. Both ports are the
// native std::vector<Mat4>::push_back shape; retain that source-level operation rather than the
// decompiler's allocator and element-construction internals.
void PlayerControlObject::addDriverDecalTransformation(const Mat4& transformation)
{
    m_driverDecalTransformations.push_back(transformation);
}

std::vector<Mat4>* PlayerControlObject::getDriverDecalTransformation() const
{
    return const_cast<std::vector<Mat4>*>(&m_driverDecalTransformations);      // &+0x244
}
Vec3*  PlayerControlObject::getExitRotation()           { return const_cast<Vec3*>(&static_cast<const PlayerControlObjectTemplate*>(m_template)->m_soldierExitRotation); }   // &template+0x1E0
int    PlayerControlObject::getArtPos()                 { return static_cast<const PlayerControlObjectTemplate*>(m_template)->m_artPos; }   // template(+0x14) +0x280 byte, movzx
bool   PlayerControlObject::getNeverDrawAs1p() const    { return static_cast<const PlayerControlObjectTemplate*>(m_template)->getNeverDrawAs1p(); }   // template(+0x14) vslot +0x244
int    PlayerControlObject::getVehicleCameraShake()     { return static_cast<const PlayerControlObjectTemplate*>(m_template)->m_vehicleCameraShake; }   // template(+0x14) +0x2B0
float  PlayerControlObject::getVehicleFov()             { return static_cast<const PlayerControlObjectTemplate*>(m_template)->m_vehicleFoV; }   // +0x2B4
const std::vector<int>* PlayerControlObject::getPcoPosId() { return &static_cast<const PlayerControlObjectTemplate*>(m_template)->m_pcoPosIds; } // template-owned vector
const std::string& PlayerControlObject::getSoundFilter() const { return static_cast<const PlayerControlObjectTemplate*>(m_template)->getSoundFilter(); }   // template vslot +0x220 (a string ref)
const Vec3& PlayerControlObject::getNameTagOffset()     { return static_cast<const PlayerControlObjectTemplate*>(m_template)->m_nameTagOffset; }   // &template+0x1EC
const Vec3& PlayerControlObject::getRidingWithOffset()  { return static_cast<const PlayerControlObjectTemplate*>(m_template)->m_ridingWithOffset; }   // &template+0x1F8
float  PlayerControlObject::getRidingWithDistMod()      { return static_cast<const PlayerControlObjectTemplate*>(m_template)->m_ridingWithDistMod; }   // template+0x204
int    PlayerControlObject::getCockpitLod()             { return static_cast<const PlayerControlObjectTemplate*>(m_template)->m_cockpitLod; }   // +0x2BC
int    PlayerControlObject::getCockpitSubGeom()         { return static_cast<const PlayerControlObjectTemplate*>(m_template)->m_cockpitSubGeom; }   // +0x2C0
float  PlayerControlObject::getShakeFactor() const      { return static_cast<const PlayerControlObjectTemplate*>(m_template)->m_shakeFactor; }   // +0x2D4
float  PlayerControlObject::getSprintFactor()           { return static_cast<const PlayerControlObjectTemplate*>(m_template)->m_sprintFactor; }   // +0x2E4
int    PlayerControlObject::getSpecialToggleWeaponInput() const { return static_cast<const PlayerControlObjectTemplate*>(m_template)->m_specialToggleWeaponInput; }   // +0x2EC
bool   PlayerControlObject::getAutoUseAbility() const   { return static_cast<const PlayerControlObjectTemplate*>(m_template)->m_autoUseAbility; }   // +0x2EA byte
// Returns the occupied-radius modifier without testing occupancy.
float PlayerControlObject::getBoundingRadiusModifier() const
{
    return static_cast<const PlayerControlObjectTemplate*>(m_template)->m_boundingRadiusModifierWhenOccupied;
}
// Stores the default team, then dispatches the virtual team setter.
void PlayerControlObject::setDefaultTeam(int team)   { m_defaultTeam = team; setTeam(team); }

// Changes the vehicle and descendant remote-controlled objects' team and updates its spawn group.
namespace {
} // namespace

void PlayerControlObject::setTeam(int team)
{
    if (team != m_team)                                                        // +0x1E4
    {
        m_team = team;
        std::vector<IObject*> objects;
        getChilds(this, objects, 0, 0);
        for (std::vector<IObject*>::iterator it = objects.begin(); it != objects.end(); ++it)
        {
            if ((*it)->getTemplate()->getClassID() == CID_RemoteControlledObjectTemplate)   // template +0x10
                static_cast<RemoteControlledObject*>(*it)->setTeam(team);       // addToGameLogic
        }
    }
    if (m_spawnGroupId != -1)                                                  // +0x1DC
    {
        ISpawnGroup* group = static_cast<SpawnManager*>(spawnManager)->getSpawnGroup(m_spawnGroupId);   // +0x28
        if (group)
            group->setTeam(team);                                              // ISpawnGroup slot 6
    }
}

int PlayerControlObject::getTeam()
{
    if (gameLogic->isServer())                                                 // +0x13C
        return m_team;                                                         // +0x1E4
    const int defaultTeam = m_defaultTeam;                                     // +0x1E8
    IPlayerControlObject* rootPco = 0;
    if (IObject* root = getRootParent(this))
        rootPco = static_cast<IPlayerControlObject*>(root->queryInterface(IID_IPlayerControlObject));
    if (rootPco)
    {
        std::map<int, IPlayerControlObject*>* pcos = rootPco->getPcos();      // ipco +0x60
        for (std::map<int, IPlayerControlObject*>::iterator it = pcos->begin(); it != pcos->end(); ++it)
        {
            if (IPlayer* occupant = it->second->getOccupingPlayer())           // ipco +0x10
                return occupant->getTeam();                                    // IPlayer +0xE8
        }
    }
    return defaultTeam;
}
float PlayerControlObject::getTimeToLiveUnused()     { return m_timeToLiveUnused; }
void PlayerControlObject::setObjectSpawnerId(int id) { m_objectSpawnerId = id; }
int  PlayerControlObject::getObjectSpawnerId() const { return m_objectSpawnerId; }
void PlayerControlObject::setTimeSpawned(float t)    { m_timeSpawned = t; }
float PlayerControlObject::getTimeSpawned() const    { return m_timeSpawned; }
void PlayerControlObject::setObjectSpawnerHolding(bool h) { m_objectSpawnerHolding = h; }
bool PlayerControlObject::getObjectSpawnerHolding() const { return m_objectSpawnerHolding; }
void PlayerControlObject::setDriverDecalObject(IObject* o) { m_driverDecalObject = o; }
IObject* PlayerControlObject::getDriverDecalObject() const { return m_driverDecalObject; }
bool PlayerControlObject::getIsDucking()             { return m_isDucking; }
int  PlayerControlObject::getAmmoStatus()            { return m_ammoStatus; }
void PlayerControlObject::setAmmoStatus(int s)       { m_ammoStatus = s; }

// On a ducking change, refreshes HUD, camera, animation pose, and occupant inverse kinematics.
void PlayerControlObject::setIsDucking(bool ducking)
{
    if (ducking == m_isDucking)
        return;                                          // edge-triggered, as shipped

    m_isDucking = ducking;
    eventManager->postEvent(ECHud, 31, this, 0.0f);      // the HUD refresh

    if (m_isDucking)
    {
        setActiveCamera(1);                              // virtual; recovered body
        if (m_activeAnimationSystem)
            m_activeAnimationSystem->setTriggerPose(1u);
    }
    else
    {
        setActiveCamera(0);
        if (m_activeAnimationSystem)
            m_activeAnimationSystem->setTriggerPose(0u);
    }

    IObject* const occupant = m_occupyingObject
        ? reinterpret_cast<WeakPtr<IObject>::Ctrl*>(m_occupyingObject)->target : 0;
    if (occupant)
        static_cast<Soldier*>(occupant)->disableInverseKinematic(m_isDucking);
}
void PlayerControlObject::setGiveUnmannedInput(bool g) { m_giveUnmannedInput = g; }
bool PlayerControlObject::getIsUnderwater() const    { return false; }
float PlayerControlObject::getNormalizedUnderwaterTime() const { return 0.0f; }
void PlayerControlObject::setActiveAnimationSystem(int) {}
// Assigns the player to all weapons; enter also repeats this traversal inline.
void PlayerControlObject::setPlayerOnWeapons(IPlayer* player)
{
    for (unsigned int i = 0; i < m_weapons.size(); ++i)                  // +0x194
    {
        IWeaponObject* const weapon = static_cast<IWeaponObject*>(
            m_weapons[i]->queryInterface(IID_IWeaponObject));            // +0x0C
        if (weapon)
            weapon->setPlayer(player);                                   // IWeaponObject +0x68
    }
}

// Toggles owned spawn points only when the template enables disableSpawnPointsOnEnter.
// Requires each stored spawn point to resolve.
void PlayerControlObject::enableSpawnPoints(bool enable)
{
    if (!m_template->m_disableSpawnPointsOnEnter)                        // +0x2AC
        return;
    for (unsigned int i = 0; i < m_spawnPoints.size(); ++i)              // +0x1D0
        getSpawnPoint(m_spawnPoints[i])->setActive(enable);              // ISpawnPoint +0x18
}

// Detects a rising input edge above 0.5 for a bound special action.
// Unbound actions clear their latch; the no-input sentinel leaves it unchanged.
bool PlayerControlObject::checkSpecialToggle(io::PlayerInput input)
{
    bool result = false;

    const int index = m_template->m_specialToggleWeaponInput;            // +0x2EC
    if (index == 64)                                                     // io::PINone
        return false;                                                    // latch NOT touched

    bool down = false;
    if (index <= 63)
    {
        // bound by the control map? (+0x100, the 64-bit mask)
        if (((input.mappingFlags >> index) & 1ULL) != 0)
            down = input.inputs[index] > 0.5f;                           // flt_888B170, strictly >
    }

    if (down)
    {
        result = true;
        if (m_specialTogglePressed)                                      // +0x25C already held
            return false;                                                // no rewrite, as compiled
    }

    m_specialTogglePressed = result;
    return result;
}

bool PlayerControlObject::checkSpecialToggleLastUpdate(io::PlayerInputMap map)
{
    return m_specialTogglePressed &&                                     // +0x25C
           static_cast<int>(map) == m_template->m_specialToggleWeaponInput;   // +0x2EC
}

// Copies and walks the target map when the template has a watch list.
// This image performs no nametag updates; enable is unused.
void PlayerControlObject::enableTargetNametags(bool /*enable*/)     // the argument is never read
{
    typedef object_detail::LayoutAccess L;

    const PlayerControlObjectTemplate* const objectTemplate =
        static_cast<const PlayerControlObjectTemplate*>(L::objectTemplate(this));   // raw +0x14
    if (objectTemplate->m_targetObjectTypesToWatch.empty())                          // +0x2CC count
        return;

    // BY value, ON purpose - see the header block. gameLogic +0x1B0 hands back the live map.
    const std::map<unsigned int, IObject*>* const live =
        static_cast<const std::map<unsigned int, IObject*>*>(gameLogic->getTargetObjectMap());
    std::map<unsigned int, IObject*> targets(*live);

    for (std::map<unsigned int, IObject*>::const_iterator it = targets.begin();
         it != targets.end(); ++it)
    {
        // empty IN THE shipped server image. Not an omission - see the header block.
    }
}

// Creates or updates the player's vehicle decals, using the default texture when no personal decal exists.
// Disabling removes the stored decal instances.
void PlayerControlObject::enablePlayerDecal(IPlayer* player, bool enable)
{
    if (enable && player)
    {
        const IObjectTemplate* decalTemplate =
            objectTemplateManager->getTemplate(std::string("decal_defaultPlayerTag"));   // +0x24
        // asked twice, as compiled - the player's own tag wins when they have one.
        if (player->getPersonalDecalTemplate())                                          // +0x210
            decalTemplate = player->getPersonalDecalTemplate();
        if (!decalTemplate)
            return;

        IObject* const decalOwner = getDriverDecalObject();                              // +0x11C
        if (!decalOwner)
            return;
        if (!decalOwner->getDecalObject())                                               // +0xDC
            return;
        void* const decalObject = decalOwner->getDecalObject();                          // asked again

        // one id per mount point - the inlined std::vector::resize
        m_driverDecalIds.resize(m_driverDecalTransformations.size());

        unsigned int idIndex = 0;
        for (std::vector<Mat4>::const_iterator t = m_driverDecalTransformations.begin();
             t != m_driverDecalTransformations.end(); ++t)
        {
            // zeroed then immediately overwritten, as compiled.
            Mat4 placed;
            std::memset(&placed, 0, sizeof(placed));
            placed = *t;

            const Mat4& ownerTransform = decalOwner->getAbsoluteTransformation();        // +0x84

            Mat4 local;
            std::memset(&local, 0, sizeof(local));
            local = placed;
            placed.mult(local, ownerTransform);

            m_driverDecalIds[idIndex++] = geom::decal_object::add(
                decalObject, &placed,
                static_cast<const DecalTemplate*>(decalTemplate)->m_decal,               // +0x34
                1.0f, 0, 0);                                                             // slot +0x1C
        }
    }
    else
    {
        // the two getters are re-asked rather than cached, as compiled.
        if (!getDriverDecalObject())                                                     // +0x11C
            return;
        if (!getDriverDecalObject()->getDecalObject())                                   // +0xDC
            return;
        void* const decalObject = getDriverDecalObject()->getDecalObject();

        for (unsigned int i = 0; i < m_driverDecalIds.size(); ++i)
            geom::decal_object::remove(decalObject, m_driverDecalIds[i]);

        m_driverDecalIds.resize(0);
    }
}

// Attempts to place player in the seat, evicting a bot occupant when necessary.
// Returns false for an occupied human seat; success updates weapons, physics, spawn points and team.
// Requires the player's current vehicle and the seat's configured interfaces.
bool PlayerControlObject::enter(IPlayer* player)
{
    typedef object_detail::LayoutAccess L;

    if (!player)
        return false;

    _byte3B9 = true;                                                     // +0x3B9 before the gate

    // 1. is the seat already taken?
    if (m_entered)                                                       // +0x150
    {
        if (getOccupingPlayer())                                         // +0x158 weak target
        {
            if (!getOccupingPlayer()->getIsAIPlayer())                   // IPlayer +0x64
                return false;                                            // a human keeps it
            game->exitVehicle(getOccupingPlayer(), true, true);          // IGame +0xA8
        }
    }

    // 2/3. sprint and ticking
    m_sprintState.setConstants(m_template->m_sprintDissipationTime,      // +0x2DC
                               m_template->m_sprintRecoverTime,          // +0x2D8
                               m_template->m_sprintLimit);               // +0x2E0
    m_sprintState.reset();
    enableHandleUpdate(true);

    // Object's +0x14 template, captured here and used only by the AI block - see the header note.
    const IObjectTemplate* const objectTemplate = L::objectTemplate(this);

    _dword278 = -1;                                                      // +0x278
    setIsDucking(false);                                                 // primary s169

    // 4. the two weak captures
    // The hand-spelled block accounting this tree uses everywhere for a raw weak slot.
    {
        WeakPtr<IPlayer> handle = player->getWeakPtr();                  // IPlayer slot 7
        int* incoming = reinterpret_cast<int*>(handle.m_ctrl);
        handle.m_ctrl = 0;                                               // the steal
        if (incoming)
            ++(*incoming);
        if (m_occupyingPlayer && --(*m_occupyingPlayer) == 0)
            ::operator delete(m_occupyingPlayer);
        m_occupyingPlayer = incoming;
        if (incoming && --(*incoming) == 0)
            ::operator delete(incoming);
    }
    m_entered = true;                                                    // +0x150
    {
        // the player's current vehicle - the body arriving, captured before setVehicle rebinds it.
        IObject* const arriving = player->getVehicle();                  // IPlayer +0x48
        WeakPtr<IObject> handle = arriving->getWeakPtr();                // IObject slot 7
        int* incoming = reinterpret_cast<int*>(handle.m_ctrl);
        handle.m_ctrl = 0;
        if (incoming)
            ++(*incoming);
        if (m_occupyingObject && --(*m_occupyingObject) == 0)
            ::operator delete(m_occupyingObject);
        m_occupyingObject = incoming;
        if (incoming && --(*incoming) == 0)
            ::operator delete(incoming);
    }
    _byte3B8 = false;                                                    // +0x3B8 [UNRESOLVED]
                                                                         // NOT m_giveUnmannedInput,
                                                                         // which is +0x3A4

    // 5. the three weapon slots
    // the shipped loop counter is a short (`movsx esi, ax` / `cmp si, 2`).
    for (short slot = 0; slot <= 2; ++slot)
    {
        if (!m_slotWeapons[slot].empty())                                // +0x1A0 + 12 * slot
            setActiveWeapon(m_slotWeapons[slot].front(), slot);          // primary s123
    }

    // 6. attach the body
    if (m_seatParentObject)                                              // +0x214
    {
        m_seatParentObject->addChild(getOccupingObject());               // IObject +0xF4
        getOccupingObject()->setRelativeTransformation(                  // IObject +0xEC
            m_template->m_seatParent.seatTransformation);
    }
    else if (getOccupingObject())
    {
        addChild(getOccupingObject());                                   // IObject +0xF4, on this
        getOccupingObject()->updateFlags(1, 0);                          // IObject +0x60
    }

    if (m_activeAnimationSystem)                                         // +0x218
        m_activeAnimationSystem->reset();

    if (m_template->m_crewKitIndex != -1)                                // +0x2FC
        setProperKitGeometry();

    // 7. the handshake that must not fail
    if (!getOccupingPlayer()->setVehicle(this, getPcoId()))              // IPlayer +0x4C / s106
    {                                                                    // getOccupingPlayer unguarded
        Debug(DEBUGTYPE_7, "Game/Objects/Game/PlayerControlObject.cpp", 735, __FILE__)
            << "setVehicle fails, should never happen";
        return false;
    }

    // 8. hand the player to the weapons
    setPlayerOnWeapons(player);
    if (getOccupingObject())
        disablePhysicsOnSoldier(getOccupingObject());
    for (unsigned int i = 0; i < m_weapons.size(); ++i)                  // +0x194
    {
        IWeaponObject* const weapon = static_cast<IWeaponObject*>(
            m_weapons[i]->queryInterface(IID_IWeaponObject));
        if (weapon)
            weapon->setPlayer(player);                                   // IWeaponObject +0x68
    }

    // 9. the ECAI notify - see the in the header block
    if (ai::g_aiInstance && ai::aiInstance()->isInitialized())           // ds:0x8AC37F8 / +0x1C
    {
        bool specialContainer = false;
        if (objectTemplate->getClassID() == CID_LadderContainerTemplate ||
            objectTemplate->getClassID() == CID_ParachuteTemplate ||
            objectTemplate->getClassID() == CID_ZiplineContainerTemplate ||
            objectTemplate->getClassID() == CID_GrapplingHookRopeContainerTemplate)
        {
            specialContainer = true;
        }

        // 20 bytes on the stack: this seat's AI object, the body's AI object, team, id, the flag.
        struct AiEnterNote
        {
            void* seatAiObject;
            void* bodyAiObject;
            int   team;
            int   id;
            bool  specialContainer;
        } note;
        note.seatAiObject = L::aiObject(this);                           // IObject +0x34
        note.bodyAiObject = getOccupingObject() ? L::aiObject(getOccupingObject()) : 0;
        note.team         = player->getTeam();                           // IPlayer +0xE8
        note.id           = player->getId();                             // IPlayer +0x54
        note.specialContainer = specialContainer;

        if (!specialContainer && !note.seatAiObject)
        {
            Debug(DEBUGTYPE_6, "Game/Objects/Game/PlayerControlObject.cpp", 771, __FILE__)
                << "You have tried to enter a vehicle that is not prepared for use with AI: "
                << m_template->getName();                                // IObjectTemplate +0x2C
            return false;
        }
        eventManager->postEvent(ECAI, 1, &note, 0.0f);
    }

    // 10. the seat's own presentation
    enableSpawnPoints(false);
    enableTargetNametags(true);
    enablePlayerDecal(player, true);

    // 11. the squad-leader spawn-point override
    IObject* const root = getRootParent(this);
    IPlayerControlObject* rootPco = 0;
    if (root)
        rootPco = static_cast<IPlayerControlObject*>(
                      root->queryInterface(IID_IPlayerControlObject));

    if (getOccupingPlayer() &&
        getOccupingPlayer()->getIsSquadLeader() &&                       // IPlayer +0x13C
        rootPco && !rootPco->getPcos()->empty())                         // ipco +0x60, the node count
    {
        std::vector<IObject*> spawnPoints;
        findSpawnPoints(getOccupingPlayer()->getDefaultVehicle(),        // IPlayer +0xA4
                        spawnPoints);
        // a short counter again.
        for (short i = 0; i < static_cast<short>(spawnPoints.size()); ++i)
        {
            ISpawnPoint* const point = static_cast<ISpawnPoint*>(
                spawnPoints[i]->queryInterface(IID_ISpawnPoint));
            point->setOverrideEnterOnSpawn(true);                        // unguarded, +0x3C
        }
    }

    // 12/13.
    updateTeam();
    m_timeToLiveUnused = m_timeToLiveUnusedReset;                        // +0x1EC <- +0x1F0
    if (isChildSoldierActive())                                          // primary s172
    {
        if (L::parent(getOccupingObject()) != static_cast<IObject*>(this))   // unguarded, +0x28
            abort();
    }
    return true;
}

void PlayerControlObject::onEnter(IObject*) {}
void PlayerControlObject::onExit() {}

// Detaches the occupant and resets seat state.
// Explicit exits update exit animation; automatic exits also position and reenter the soldier object.
void PlayerControlObject::internalExit(bool byPlayer)
{
    typedef object_detail::LayoutAccess L;

    Vec3 exitRotation = *getExitRotation();
    CameraViewMode savedViewMode = static_cast<CameraViewMode>(8);
    if (m_template->m_maintainCameraOnExit)
    {
        ICameraObject* const camera = getActiveCamera();
        if (camera)
        {
            IObject* const cameraObject = static_cast<IObject*>(
                camera->queryInterface(IID_IObject));                    // unguarded result
            float z;
            float x;
            float y;
            getRotationZXY(cameraObject->getRelativeTransformation(), z, x, y);
            exitRotation.x = -y;
            exitRotation.y = -x;
            exitRotation.z = -z;
        }
        else
        {
            exitRotation.x = 0.0f;
            exitRotation.y = 0.0f;
            exitRotation.z = 0.0f;
        }

        savedViewMode = static_cast<CameraViewMode>(8);
        if (camera)
            savedViewMode = camera->getViewMode();
    }

    handleMessage(static_cast<TemplateMessage>(5),
                  pcoWeakTarget<IPlayer>(m_occupyingPlayer), false);
    handleMessage(static_cast<TemplateMessage>(3),
                  pcoWeakTarget<IPlayer>(m_occupyingPlayer), false);
    setListenerEnvironment(false);
    enableHandleUpdate(false);
    if (m_warningHudComponent)
        static_cast<IWarningHudComp*>(m_warningHudComponent)
            ->setWarningState(static_cast<WarningState>(0));

    // Raw Object::m_template (+0x14), intentionally distinct from the typed PCO template +0x148.
    const IObjectTemplate* const objectTemplate = L::objectTemplate(this);
    IObject* const occupyingObject = pcoWeakTarget<IObject>(m_occupyingObject);

    if (ai::g_aiInstance && ai::aiInstance()->isInitialized() &&
        pcoWeakTarget<IPlayer>(m_occupyingPlayer) && L::aiObject(this))
    {
        bool specialContainer = false;
        if (objectTemplate->getClassID() == CID_LadderContainerTemplate ||
            objectTemplate->getClassID() == CID_ParachuteTemplate ||
            objectTemplate->getClassID() == CID_ZiplineContainerTemplate ||
            objectTemplate->getClassID() == CID_GrapplingHookRopeContainerTemplate)
        {
            specialContainer = true;
        }

        struct AiExitNote
        {
            void* bodyAiObject;
            void* seatAiObject;
            int   team;
            int   id;
            bool  specialContainer;
        } note;
        note.bodyAiObject = occupyingObject ? L::aiObject(occupyingObject) : 0;
        note.seatAiObject = L::aiObject(this);
        note.team = pcoWeakTarget<IPlayer>(m_occupyingPlayer)->getTeam();
        note.id = pcoWeakTarget<IPlayer>(m_occupyingPlayer)->getId();
        note.specialContainer = specialContainer;
        eventManager->postEvent(ECAI, 2, &note, 0.0f);
    }

    if (byPlayer)
    {
        if (occupyingObject)
        {
            // Retail assumes the body has a PCO in its parent chain and calls through null if not.
            pcoFindParentControlObject(occupyingObject)->exitPlayer();

            if ((L::flags(occupyingObject) & 1u) == 0)
            {
                bool playedExitAnimation = false;
                if (m_activeAnimationSystem)
                    playedExitAnimation =
                        m_activeAnimationSystem->playTrigger(std::string("die"), false);

                if (!playedExitAnimation && m_seatParentObject)
                {
                    m_seatParentObject->removeChild(occupyingObject);
                    removeChildVehicle(occupyingObject);                  // virtual s174
                    reenablePhysicsOnSoldier(occupyingObject);
                    static_cast<Soldier*>(occupyingObject)->m_byte27F[0] = true;
                }
                else
                {
                    WeakPtr<IObject> handle = static_cast<IObject*>(this)->getWeakPtr();
                    int* incoming = reinterpret_cast<int*>(handle.m_ctrl);
                    handle.m_ctrl = 0;
                    if (incoming)
                        ++(*incoming);

                    Soldier* const soldier = static_cast<Soldier*>(occupyingObject);
                    int* const old = reinterpret_cast<int*>(soldier->m_refBlock1D8);
                    if (old && --(*old) == 0)
                        ::operator delete(old);
                    soldier->m_refBlock1D8 = incoming;
                    if (incoming && --(*incoming) == 0)
                        ::operator delete(incoming);
                    soldier->disableInverseKinematic(true);
                }
            }
        }
    }
    else
    {
        if (occupyingObject && pcoWeakTarget<IPlayer>(m_occupyingPlayer))
        {
            // This first parent-PCO result is unguarded in both ports.
            IPlayerControlObject* const bodyPco =
                pcoFindParentControlObject(occupyingObject);
            const int bodyPcoId = bodyPco->getPcoId();
            pcoWeakTarget<IPlayer>(m_occupyingPlayer)->setVehicle(occupyingObject, bodyPcoId);
        }

        if (m_seatParentObject)
        {
            // The cached body is passed even when null; the seat-parent pointer is the only gate.
            m_seatParentObject->removeChild(occupyingObject);
        }
        else if (occupyingObject)
        {
            removeChild(occupyingObject);
            occupyingObject->updateFlags(0, 1u);
        }

        Mat4 exitTransform;
        std::memset(&exitTransform, 0, sizeof(exitTransform));
        exitTransform = Mat4::getIdentity();
        Mat4 exitRotationMatrix;
        std::memset(&exitRotationMatrix, 0, sizeof(exitRotationMatrix));
        exitRotationMatrix = Mat4::getIdentity();
        setRotationZXY(exitRotationMatrix, 0.0f, 0.0f, -exitRotation.x);
        exitTransform.mult(exitRotationMatrix,
                           getRootParent(this)->getAbsoluteTransformation());

        Vec3& exitPosition = *reinterpret_cast<Vec3*>(&exitTransform.m[12]);
        if (_byte20C)
        {
            exitPosition.x = m_altExitPosition[0];
            exitPosition.y = m_altExitPosition[1];
            exitPosition.z = m_altExitPosition[2];
        }
        else
        {
            getExitPosition(exitPosition);
        }
        _byte20C = false;

        if (occupyingObject)
        {
            occupyingObject->setAbsoluteTransformation(exitTransform);
            if (occupyingObject->queryInterface(IID_ISoldier))
            {
                Soldier* const soldier = static_cast<Soldier*>(occupyingObject);
                bool restoreViewMode = false;
                const bool developerDefault = false;
                if (settings->boolGet(std::string("GSDeveloper"), developerDefault) &&
                    savedViewMode != static_cast<CameraViewMode>(8))
                {
                    restoreViewMode = soldier->getActiveCamera() != 0;
                }

                if (restoreViewMode)
                    soldier->getActiveCamera()->setViewMode(savedViewMode, 0, false);

                IObject* const cameraObject = static_cast<IObject*>(
                    soldier->getActiveCamera()->queryInterface(IID_IObject));   // unguarded
                Vec3 cameraRotation;
                cameraRotation.x = 0.0f;
                cameraRotation.y = exitRotation.y;
                cameraRotation.z = 0.0f;

                Mat4 cameraTransform;
                std::memset(&cameraTransform, 0, sizeof(cameraTransform));
                cameraTransform = cameraObject->getParent()
                    ? cameraObject->getRelativeTransformation()
                    : cameraObject->getAbsoluteTransformation();
                setRotationZXY(cameraTransform,
                               -cameraRotation.z,
                               -cameraRotation.y,
                               -cameraRotation.x);
                if (cameraObject->getParent())
                    cameraObject->setRelativeTransformation(cameraTransform);
                else
                    cameraObject->setAbsoluteTransformation(cameraTransform);

                if (const_cast<IObjectTemplate*>(cameraObject->getTemplate())
                        ->queryImplementation(CID_RotationalBundleTemplate))
                {
                    static_cast<RotationalBundle*>(cameraObject)
                        ->setCurrentRotation(cameraRotation);
                }
                soldier->setLookUpDownAngle(exitRotation.y);
            }
            reenablePhysicsOnSoldier(occupyingObject);
        }

        IPlayerControlObject* const bodyPco =
            pcoFindParentControlObject(occupyingObject);
        if (bodyPco)
            bodyPco->enter(pcoWeakTarget<IPlayer>(m_occupyingPlayer));

        if (m_occupyingObject && --(*m_occupyingObject) == 0)
            ::operator delete(m_occupyingObject);
        m_occupyingObject = 0;

        std::vector<IObject*> spawnPoints;
        findSpawnPoints(pcoWeakTarget<IPlayer>(m_occupyingPlayer)->getDefaultVehicle(),
                        spawnPoints); // player unguarded
        for (unsigned int i = 0; i < spawnPoints.size(); ++i)
        {
            ISpawnPoint* const point = static_cast<ISpawnPoint*>(
                spawnPoints[i]->queryInterface(IID_ISpawnPoint));
            point->setOverrideEnterOnSpawn(false);                            // unguarded
        }
    }

    for (std::vector<IObject*>::iterator it = m_weapons.begin();
         it != m_weapons.end(); ++it)
    {
        IWeaponObject* const weapon = static_cast<IWeaponObject*>(
            (*it)->queryInterface(IID_IWeaponObject));
        if (weapon)
            weapon->setPlayer(0);
    }

    if (!m_ownCameras.empty())
        m_activeCamera = m_ownCameras.begin()->second;
    else
        m_activeCamera = 0;
    disableWeapons();
    enablePlayerDecal(pcoWeakTarget<IPlayer>(m_occupyingPlayer), false);
    m_entered = false;

    if (m_occupyingPlayer && --(*m_occupyingPlayer) == 0)
        ::operator delete(m_occupyingPlayer);
    m_occupyingPlayer = 0;

    enableSpawnPoints(true);
    const PlayerControlObjectTemplate* const rawPcoTemplate =
        static_cast<const PlayerControlObjectTemplate*>(objectTemplate);
    if (rawPcoTemplate->m_disableSpawnPointsOnEnter)
    {
        // Deliberately repeated after enableSpawnPoints(true), as shipped.
        for (unsigned int i = 0; i < m_spawnPoints.size(); ++i)
            getSpawnPoint(m_spawnPoints[i])->setActive(true);                  // unguarded
    }
    enableTargetNametags(false);

    if (byPlayer)
    {
        if (occupyingObject)
            occupyingObject->setUpdateFrequencyType(static_cast<UpdateFreqencyType>(0));
    }
    else if (rawPcoTemplate->m_destroyOnExit &&
             game->queryInterface(IID_IGameServer))
    {
        Vec3 zero;
        zero.x = 0.0f;
        zero.y = 0.0f;
        zero.z = 0.0f;
        Vec3 up;
        up.x = 0.0f;
        up.y = 1.0f;
        up.z = 0.0f;
        gameLogic->giveDamage(this, 1.0e10f, 0, 0, 0, -1, 0, 0,
                              zero, up, -1, false, true, false);
    }

    updateTeam();
}

// exit - ELF 0x8199230, 46 B / PE 0x599010, 27 B. Only an occupied PCO enters the shared exit
// transaction; the bool distinguishes an automatic/vehicle exit from exitPlayer's explicit one.
void PlayerControlObject::exit()
{
    if (pcoWeakTarget<IPlayer>(m_occupyingPlayer))
        internalExit(false);
}

// exitPlayer - ELF 0x8199260, 30 B. `internalExit(true)`, nothing else.
void PlayerControlObject::exitPlayer()
{
    internalExit(true);
}

// snapWheel - ELF 0x819E520 / PE 0x593890. Sibling, then child, then this spring. The extent
// is abs(first collision vertex.y), not the mesh bounding radius. Keep the position reference
// live across the ray test and only move when the vertical correction is strictly below 1 metre.
void PlayerControlObject::snapWheel(IObject* object)
{
    if (!object)
        return;
    snapWheel(object_detail::LayoutAccess::sibling(object));
    snapWheel(object->getChild());
    if (object_detail::LayoutAccess::objectTemplate(object)->getClassID() != CID_SpringTemplate)
        return;
    if (!object->hasCollisionLod(1))
        return;

    ICollisionMesh* mesh = static_cast<ICollisionMesh*>(object_detail::LayoutAccess::collisionMesh(object));
    const float extent = std::fabs(static_cast<const Vec3*>(mesh->getVertexVector(1))->y);
    const Vec3& position = object->getAbsolutePosition();
    const Vec3 start = {position.x, position.y + extent, position.z};
    const Vec3 down = {0.0f, extent * -2.0f, 0.0f};
    RootObjectIgnorePredicator pred(0x100u, object);
    IObject* hitObject = 0;
    Vec3 hitPoint = {}, hitNormal = {};
    int hitPart;
    if (objectManager->intersectLine(hitObject, hitPoint, hitNormal, hitPart,
            start, down, &pred, false, false, true, false, 0))
    {
        hitPoint.y += extent;
        if (std::fabs(position.y - hitPoint.y) < 1.0f)
            object->setAbsolutePosition(hitPoint);
    }
}

// getUnmannedInput — ELF 0x819E000, 1,049 B / PE 0x5945F0, 456 B, read in full.
// The old census start 0x819DFD0 precedes this function. The gate is an unsigned tick,
// not the separate float m_inputTime at ELF +0x290. Stock content still does not enable
// m_giveUnmannedInput. Preserve five shared-RNG draws, the major-axis ceil (1 or 2),
// and the final overwrite of axis 0 even though a random value was just generated for it.
void PlayerControlObject::getUnmannedInput(io::PlayerInput& input)
{
    if (m_unmannedInputTick > static_cast<unsigned int>(WorldPref::getTickCount()))
        return;
#if defined(_WIN32)
    // [PLATFORM] PE subtracts trunc(-30 * seconds); ELF adds trunc(30 * seconds).
    m_unmannedInputTick = static_cast<unsigned int>(WorldPref::getTickCount())
        - static_cast<int>(-30.0f * *pcoUnmannedInputTime);
#else
    m_unmannedInputTick = static_cast<unsigned int>(WorldPref::getTickCount())
        + static_cast<int>(30.0f * *pcoUnmannedInputTime);
#endif
    const auto randomInteger = []() -> float {
        return static_cast<float>((static_cast<int>(Rand.getRaw()) >> 7 | 1) + 1);
    };
    const float inverseRange = 0.000000059604645f;
    const int majorAxis = static_cast<int>(std::ceil(randomInteger() * (2.0f * inverseRange)));
    const float direction = randomInteger() * inverseRange > 0.5f ? -1.0f : 1.0f;
    for (int i = 0; i <= 2; ++i)
    {
        const auto axis = static_cast<io::PlayerInputMap>(i);
        input.setInputIsMapped(axis, true);
        float value;
        if (i == majorAxis)
        {
            const float minimum = *pcoUnmannedMinMajorAxis;
            value = (randomInteger() * (1.0f - minimum) * inverseRange + minimum) * direction;
        }
        else
        {
            const float maximum = *pcoUnmannedMaxMinorAxis;
            const float minimum = -maximum;
            value = randomInteger() * (maximum - minimum) * inverseRange + minimum;
        }
        input.setInput(axis, value);
    }
    input.setInput(static_cast<io::PlayerInputMap>(0), direction);
    input.setInputIsMapped(static_cast<io::PlayerInputMap>(3), true);
    input.setInput(static_cast<io::PlayerInputMap>(3), 1.0f);
}


// Declared at file scope in world, the way Soldier.cpp and PlayerControlObjectHelpers.cpp declare
// it - the tree has no header for these free functions. Written in Player.cpp (0x80F7E20).
ICameraObject* getCommanderCameraForPlayer(const IPlayer* player);

// Resolves the seat camera, delegating through the occupying hierarchy when this seat has none.

// Prefers the commander camera, then a parent PCO camera when this object has none, then its cache.
ICameraObject* PlayerControlObject::getActiveCamera()
{
    ICameraObject* camera = getCommanderCameraForPlayer(getOccupingPlayer());
    if (camera)
        return camera;

    IObject* occupant = pcoWeakTarget<IObject>(m_occupyingObject); // native Ctrl target, x86 +4

    if (occupant && m_ownCameras.empty())                        // +0x164 read as a boolean
    {
        IPlayerControlObject* pco = 0;
        for (IObject* node = occupant; node; node = node->getParent())   // Object +0x28
        {
            pco = static_cast<IPlayerControlObject*>(
                      node->queryInterface(IID_IPlayerControlObject));   // +0x0C
            if (pco)
                break;
        }
        // RAW-verified: `call dword ptr [eax+44h]` - IPlayerControlObject::getActiveCamera, the
        // symmetric forward. IDA labels this slot `Object::getObjectInDestroyList`, which is the
        // documented vtable-offset coincidence and is NOT what is being called.
        camera = pco->getActiveCamera();                         // +0x44 unguarded
    }

    if (!camera)
        return m_activeCamera;                                   // +0x170 the cache
    return camera;
}

// Selects a camera by id, falling back to the lowest id in a nonempty local map.
// Delegates to the occupying object when there are no local cameras.
void PlayerControlObject::setActiveCamera(int cameraId)
{
    IObject* occupant = pcoWeakTarget<IObject>(m_occupyingObject); // native Ctrl target, x86 +4

    if (!occupant && m_ownCameras.empty())
    {
        // The site that attests this file's line 1377.
        Debug(DEBUGTYPE_6, "Game/Objects/Game/PlayerControlObject.cpp", 1377, __FILE__)
            << "TBM: No camera!";
        return;
    }

    if (occupant && m_ownCameras.empty())
    {
        IPlayerControlObject* pco = 0;
        for (IObject* node = occupant; node; node = node->getParent())
        {
            pco = static_cast<IPlayerControlObject*>(
                      node->queryInterface(IID_IPlayerControlObject));
            if (pco)
                break;
        }
        pco->setActiveCamera(cameraId);                          // +0x48 unguarded
        return;
    }

    // this PCO owns cameras: pick one
    std::map<int, ICameraObject*>::iterator it = m_ownCameras.find(cameraId);
    m_activeCamera = (it == m_ownCameras.end()) ? m_ownCameras.begin()->second   // the miss falls
                                                : it->second;                    // back to begin()

    if (getOccupingPlayer())
        m_activeCamera->updateCameraView(true);                  // ICameraObject +0x54
}

// Vehicle-mod counters accept indices 1..8; invalid reads return zero and writes are ignored.
// [UNRESOLVED] enumerator names; damage modifiers 4/5 scale damage by 0.9/1.1.
void PlayerControlObject::addVehicleMod(VehicleMod mod)
{
    const int m = static_cast<int>(mod);
    if (m >= 1 && m <= 8)
        ++m_vehicleMods[m];
}

void PlayerControlObject::removeVehicleMod(VehicleMod mod)
{
    const int m = static_cast<int>(mod);
    if (m >= 1 && m <= 8)
        --m_vehicleMods[m];
}

int PlayerControlObject::getVehicleMod(VehicleMod mod)
{
    const int m = static_cast<int>(mod);
    if (m >= 1 && m <= 8)
        return m_vehicleMods[m];
    return 0;
}

} // namespace world
} // namespace hfe
} // namespace dice
