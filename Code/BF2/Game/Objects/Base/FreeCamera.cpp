// Spectator and pre-spawn camera behavior.

#include "FreeCamera.h"

#include <cstdlib>
#include "../../../Core/Debug.h"        // resetCameraView's two fatal sites
#include "../../../Core/Ids.h"
#include "../../IGame.h"                        // ← already present for `game`; see below
#include "../../GameServer/ServerInfo.h"        // sv.allowFreeCam - handlePlayerInput's gate
#include "../../IGameLogic.h"                   // gameLogic->getGameStatus - the movement gate
#include "../../../Math/Mat4.h"                 // the basis the camera flies along
#include "../../../Math/Quat.h"                 // death-camera orientation interpolation
#include "../../../Physics/HeightmapCluster.h"  // the ground clamp
#include "../../../Physics/IPhysicsNode.h"      // followed vehicle's positional speed
#include "../../../Core/SettingsRepostitory.h"  // settings->boolGet("GSDeveloper")
#include "../../Utility/Vars.h"                 // cam-slow / cam-normal / cam-fast
#include <cmath>                                // sqrt / fabs - the re-orthonormalisation
#include "../../../IO/Input/PlayerInput.h"      // handlePlayerInput reads input 9                 // the five IIDs queryInterface answers, CID_FreeCameraNetworkable
#include "../../../Scene/Object/ObjectUtil.h"  // setNetworkableComp
#include "../../../Scene/Object/PlayerManager.h"   // playerManager - handleUpdate's spectate target
#include "../../IGame.h"                       // game - handleUpdate's server gate
#include "../../IGameLogic.h"                  // gameLogic - destroy's removePco
#include "../../GameLogic/Player.h"            // the concrete player handleUpdate follows
#include "../IObject.h"
#include "../ObjectLayoutAccess.h"     // createdOnLoad - init's +0x78 store

namespace dice {
namespace hfe {
namespace world {

// Initializes camera state in order around updateFlags.
// Repeated writes are intentional because that virtual call can observe the intermediate values.
FreeCamera::FreeCamera(const FreeCameraTemplate* objectTemplate)
    : Bundle(objectTemplate)
    , m_refCount(1)                      // +0x148
    , m_weapons()                        // +0x14C
    , m_primWeaponIcons()                // +0x158
    , m_secWeaponIcons()                 // +0x15C
    , m_vehicleIcon()                    // +0x160 ""
    , m_occupingPlayer()                 // +0x164
    , m_isActive(false)                  // +0x168
    , m_ownCameras()                     // +0x16C
    , m_playerControlObjs()              // +0x178
    , m_pcoPosIds()                      // +0x184
    , m_viewMode(1)                      // +0x190 see the note below
    , _float194(-1.0f)                   // +0x194 sentinel
    , m_viewModeToggleArmed(true)                     // +0x198 true
    , m_followedPlayerId(-1)             // +0x1A8 sentinel
    , m_traceObjectId(0xFFFFFFFFu)       // +0x1EC sentinel
    , _bool1F0(true)                     // +0x1F0 true
    , m_cameraSpeedThreshold(1.0f)                    // +0x200 NOT 0
    , _bool204(false)                    // +0x204
    , m_cameraSpeed(1.0f)                    // +0x208 NOT 0
    , m_soundFilter()                    // +0x20C ""
    , _dword250(0)
    , _dword254(0)
    , _float258(0.0f)                    // overwritten with 1.0f below
    , _dword25C(0)
    , _float26C(1.0f)                    // +0x26C NOT 0
    , _float27C(2.0f)                    // +0x27C NOT 0
    , m_traceObject()                    // +0x28C
    , m_clampToGround(true)                     // +0x290 true
{
    // m_viewMode's initial value IS 1, and exit() sets it to 0. getActiveFOV branches on exactly
    // that value, so a camera nobody has entered reports the template's active FOV, not its default.

    for (int i = 0; i < 3; ++i)
    {
        m_nameTagOffset[i] = 0.0f;       // +0x19C
        _vec1F4[i]         = 0.0f;       // +0x1F4
        _vec260[i]         = 0.0f;       // +0x260
        _vec270[i]         = 0.0f;       // +0x270
        _vec280[i]         = 0.0f;       // +0x280
    }

    // +0x1AC and +0x210: two identity matrices, both filled by the ELF's 4x4 zero loop with 1.0f
    // stored at elements 0, 5, 10 and 15 afterwards.
    for (int i = 0; i < 16; ++i)
    {
        _mat1AC[i]          = 0.0f;
        m_transformation[i] = 0.0f;
    }
    _mat1AC[0] = _mat1AC[5] = _mat1AC[10] = _mat1AC[15] = 1.0f;
    m_transformation[0] = m_transformation[5] = 1.0f;
    m_transformation[10] = m_transformation[15] = 1.0f;

    // . Set bit 3, clear bit 11 - bit 0x800 is the one enter()/exit() toggle below.
    updateFlags(8u, 0x800u);

    // the second pass - see the header block. Same three zeroes, then 1.0f where 0.0f was.
    _dword250 = 0;
    _dword254 = 0;
    _float258 = 1.0f;
    _vec260[0] = 0.0f;
    _vec260[1] = 0.0f;
    _vec260[2] = 0.0f;
}

// ~FreeCamera - D1 0x81E5950, 610 B. The 610 bytes are the four vptr restores and the member
// teardown GCC inlines (two lists, two maps, two vectors, two strings, the weak block); there is no
// statement of its own. Real containers reproduce all of it.
FreeCamera::~FreeCamera()
{
}

// FreeCamera owns its reference count; release returns the decremented count after virtual deletion.
void FreeCamera::addRef()
{
    ++m_refCount;
}

int FreeCamera::getRef()
{
    return m_refCount;
}

int FreeCamera::release()
{
    const int left = --m_refCount;
    if (left == 0 && this != 0)
        delete this;                                 // `call [vptr+0x18]` - the deleting destructor
    return left;
}

// Returns the requested object, camera, entry-point, player-control or console interface, or null.
void* FreeCamera::queryInterface(unsigned int iid) const
{
    FreeCamera* self = const_cast<FreeCamera*>(this);

    if (iid == IID_IBase || iid == IID_IObject)
        return static_cast<IObject*>(self);
    if (iid == IID_ICameraObject)
        return static_cast<ICameraObject*>(self);
    if (iid == IID_IEntryPointObject)
        return static_cast<IEntryPointObject*>(self);
    if (iid == IID_IPlayerControlObject)
        return static_cast<IPlayerControlObject*>(self);
    if (iid == io::IID_IConsoleSaveable)
        return static_cast<io::IConsoleSaveable*>(self);
    return 0;
}

// Registers itself as camera and controlled object under key 0.
// Marks the camera as created on load so it survives round resets.
void FreeCamera::init()
{
    Bundle::init();

    object_detail::LayoutAccess::createdOnLoad(this) = true;   // Object +0x78

    ICameraObject* camera =
        static_cast<ICameraObject*>(queryInterface(IID_ICameraObject));
    m_ownCameras.insert(std::make_pair(0, camera));

    IPlayerControlObject* pco =
        static_cast<IPlayerControlObject*>(queryInterface(IID_IPlayerControlObject));
    m_playerControlObjs.insert(std::make_pair(0, pco));
}

// Removes the PCO registration before base destruction; repeated destruction is ignored.
void FreeCamera::destroy(bool force)
{
    if (isObjectDestroyed())
        return;

    gameLogic->removePco(this);
    SimpleObject::destroy(force);
}

// In server trace mode, ages the active timer and follows the target vehicle's position without changing rotation.
// A timer value of -1 disables aging.
void FreeCamera::handleUpdate(float deltaTime, unsigned int /*flags*/)
{
    if (!game->queryInterface(IID_IGameServer))
        return;
    if (m_viewMode != 6)
        return;

    if (_float194 != -1.0f)
        _float194 = _float194 + deltaTime;

    IPlayer* player = 0;
    if (m_followedPlayerId != -1)
        player = playerManager->getPlayerFromId(m_followedPlayerId);
    if (!player)
        return;

    if (!player->getVehicle())                       // first of two - see the note
        return;

    IObject* vehicle = player->getVehicle();
    setAbsolutePosition(vehicle->getAbsolutePosition());
}

// Creates FreeCameraNetworkable when the template supplies any nonempty networkable-info string.
// Prediction remains a downstream frontier in that component.
void FreeCamera::initNetworkableComp()
{
    const IObjectTemplate* t = getTemplate();
    if (!t)
        return;
    if (t->getNetworkableInfo() != "")
        setNetworkableComp(this, CID_FreeCameraNetworkable);
}

// enter(IPlayer*, int&) - 0x81E2170, 92 B. IEntryPointObject's. It ignores the seat index entirely -
// a camera has one seat - and forwards to the IPlayerControlObject enter below.
bool FreeCamera::enter(IPlayer* player, int& /*seat*/)
{
    return enter(player);
}

// exit(IPlayer*) - 0x81E21D0, 50 B. IEntryPointObject's. Refuses unless somebody is actually in the
// camera, and ignores which player is asking - the occupant leaves whoever knocks.
bool FreeCamera::exit(IPlayer* /*player*/)
{
    if (!m_occupingPlayer.get())
        return false;
    exit();
    return true;
}

// Retains the player weakly and sets its vehicle to this camera, seat 0.
// Sets the occupied flag both before and after player notification; rejects null players.
bool FreeCamera::enter(IPlayer* player)
{
    if (!player)
        return false;

    updateFlags(0x800u, 0u);

    m_occupingPlayer = player->getWeakPtr();

    player->setVehicle(this, getPcoId());            // IPlayer +0x4C, and the ICameraObject getPcoId

    updateFlags(0x800u, 0u);                         // again, exactly as compiled
    return true;
}

// Clears the player's vehicle, resets view mode, and clears the occupied flag.
// The stored weak occupant is intentionally retained.
void FreeCamera::exit()
{
    IPlayer* player = m_occupingPlayer.get();
    if (!player)
        return;

    player->setVehicle(0, -1);
    m_viewMode = 0;
    updateFlags(0u, 0x800u);
}

// Releases the held object reference before exiting and clearing occupant, view mode and active flags.
void FreeCamera::exitPlayer()
{
    m_traceObject = WeakPtr<IObject>();

    exit();                                                 // vtable +0x1CC
}

// THE constant surface. Sixty slots that answer without looking at anything. They are transcribed
// one for one; the marks are the ones whose constant is not the obvious one.

void FreeCamera::setInterpolatedMode(bool) {}                         // empty
void FreeCamera::setCameraId(int) {}                                  // empty
int  FreeCamera::getCameraId() { return 0; }
void FreeCamera::prevViewMode() {}                                    // empty
void FreeCamera::setExternCameraTrans(Mat4) {}                        // empty
Mat4* FreeCamera::getExternCameraTrans() { return 0; }
bool FreeCamera::getToggleMouseLook() { return false; }
void FreeCamera::handleExplosion(float) {}                            // empty
void FreeCamera::onEnter(IObject*) {}                                 // empty
void FreeCamera::onExit() {}                                          // empty
void FreeCamera::setPcoId(int) {}                                     // empty
void FreeCamera::nextCamera() {}                                      // empty
void FreeCamera::setActiveCamera(int) {}                              // empty
void FreeCamera::addWeapon(IObject*) {}                               // empty
IObject* FreeCamera::getActiveWeapon(int) const { return 0; }
void FreeCamera::setActiveWeapon(IObject*, int) {}                    // empty
void FreeCamera::toggleOnWeaponLock(IObject*) {}                      // empty
void FreeCamera::addPlayerInputObject(IObject*) {}                    // empty
void FreeCamera::removePlayerInputObject(IObject*) {}                 // empty
int  FreeCamera::getSubPos() { return 0; }
int  FreeCamera::getSonarPos() { return 0; }
int  FreeCamera::getArtPos() { return 0; }
void FreeCamera::setTeam(int) {}                                      // empty
int  FreeCamera::getTeam() { return 0; }
void FreeCamera::setDefaultTeam(int) {}                               // empty
void FreeCamera::setTimeToLiveUnused(float) {}                        // empty
void FreeCamera::setObjectSpawnerId(int) {}                           // empty
void FreeCamera::setObjectSpawnerHolding(bool) {}                     // empty
bool FreeCamera::getObjectSpawnerHolding() const { return false; }
void FreeCamera::setMenuVisibleForTeam(unsigned char) {}              // empty
unsigned char FreeCamera::getMenuVisibleForTeam() { return 0; }
int  FreeCamera::getPcoId() { return 0; }                             // ICameraObject's
int  FreeCamera::getPcoId() const { return 0; }                       // IPlayerControlObject's
void FreeCamera::addVehicleMod(VehicleMod) {}                         // empty
void FreeCamera::removeVehicleMod(VehicleMod) {}                      // empty
int  FreeCamera::getVehicleMod(VehicleMod) { return 0; }
float FreeCamera::getShakeFactor() const { return 0.0f; }
bool FreeCamera::getIsSprinting() const { return false; }
bool FreeCamera::getIsSprintingPossible() const { return false; }
float FreeCamera::getSprintStamina() const { return 0.0f; }
void FreeCamera::setSprintStamina(float) {}                           // empty
void FreeCamera::setSprintStaminaDissipationFactor(float) {}          // empty
float FreeCamera::getSprintFactor() { return 0.0f; }
bool FreeCamera::getIsUnderwater() const { return false; }
void FreeCamera::setListenerObstruction(float) {}                     // empty
float FreeCamera::getListenerObstruction() const { return 0.0f; }

// -1, NOT 0. An object spawner asking "whose are you" gets the sentinel, so a free camera is never
// mistaken for a spawner's charge.
int FreeCamera::getObjectSpawnerId() const { return -1; }

// Returns completed underwater time even though this camera never reports being underwater.
float FreeCamera::getNormalizedUnderwaterTime() const { return 1.0f; }

// false, where THE interface's default IS true (ICameraObject +0x7C, 0x81DFB20). The free camera
// is the one camera the mode cycle must not reach.
bool FreeCamera::allowCameraModeCycle() { return false; }

// Always reports occupied, independently of the stored player.
bool FreeCamera::isOccupied() const { return true; }

// the four static-sun answers: all Vec3::getIdentity
const Vec3& FreeCamera::getStaticSunDirection() const    { return Vec3::getIdentity(); }
const Vec3& FreeCamera::getStaticSunColor() const        { return Vec3::getIdentity(); }
const Vec3& FreeCamera::getStaticSunDamageColor() const  { return Vec3::getIdentity(); }
const Vec3& FreeCamera::getStaticSunAmbientColor() const { return Vec3::getIdentity(); }

// the plain field views

CameraViewMode FreeCamera::getViewMode() const
{
    return static_cast<CameraViewMode>(m_viewMode);   // +0x190
}

bool FreeCamera::isActive() const                     { return m_isActive; }            // +0x168
void FreeCamera::setIsActive(bool active)             { m_isActive = active; }
void FreeCamera::setTraceObjectId(unsigned int id)    { m_traceObjectId = id; }         // +0x1EC
unsigned int FreeCamera::getTraceObjectId()           { return m_traceObjectId; }

std::map<int, ICameraObject*>* FreeCamera::getCameras()    { return &m_ownCameras; }    // +0x16C
std::map<int, ICameraObject*>* FreeCamera::getOwnCameras() { return &m_ownCameras; }    // the same
std::map<int, IPlayerControlObject*>* FreeCamera::getPcos() { return &m_playerControlObjs; }
const std::vector<int>* FreeCamera::getPcoPosId()     { return &m_pcoPosIds; }          // object-owned vector
void* FreeCamera::getWeapons()                        { return &m_weapons; }            // +0x14C
void* FreeCamera::getWeapons(int)                     { return &m_weapons; }            // the same
std::list<std::string>* FreeCamera::getPrimWeaponIcons() { return &m_primWeaponIcons; } // +0x158
std::list<std::string>* FreeCamera::getSecWeaponIcons()  { return &m_secWeaponIcons; }  // +0x15C
const std::string& FreeCamera::getSoundFilter() const { return m_soundFilter; }         // +0x20C

// returns A copy, not a reference - the ELF copy-constructs a std::string from +0x160 into the
// caller's sret slot (0x81E6180, `std::string::string(const std::string&)`).
std::string FreeCamera::getVehicleIcon()              { return m_vehicleIcon; }

// THE camera IS ITS OWN active camera and its own linked control object - the +0x13C and +0x144
// sub-objects, which is what init() put in the two maps.
ICameraObject* FreeCamera::getActiveCamera()
{
    return static_cast<ICameraObject*>(this);         // this + 0x13C
}

IPlayerControlObject* FreeCamera::getLinkedPlayerControlObject()
{
    return static_cast<IPlayerControlObject*>(this);  // this + 0x144
}

// Both offsets share one Vec3; its X component also supplies the distance modifier.
const Vec3& FreeCamera::getNameTagOffset()
{
    return *reinterpret_cast<const Vec3*>(m_nameTagOffset);
}

const Vec3& FreeCamera::getRidingWithOffset()
{
    return *reinterpret_cast<const Vec3*>(m_nameTagOffset);
}

float FreeCamera::getRidingWithDistMod()
{
    return m_nameTagOffset[0];
}

// Both interfaces expose the same weak occupant.
IPlayer* FreeCamera::getPlayer() const
{
    return m_occupingPlayer.get();
}

IPlayer* FreeCamera::getOccupingPlayer() const
{
    return m_occupingPlayer.get();
}

// resetTraceCamera - ELF 0x81E47D0 (72 B). The eight stores are in shipped order. The PE inlines
// the same stores into setViewMode's mode-6 arm.
void FreeCamera::resetTraceCamera()
{
    m_followedPlayerId = -1;
    m_traceObjectId = 0xFFFFFFFFu;
    _bool1F0 = true;
    _vec260[0] = 0.0f;
    _vec260[1] = 0.0f;
    _vec260[2] = 0.0f;
    _dword25C = 0;
    _float194 = 0.0f;
}

// Selects trace mode 6 or coerces other requests to mode 1.
// Trace mode snapshots the supplied or held object, otherwise selects an eligible teammate and falls back to mode 1.
bool FreeCamera::setViewMode(CameraViewMode mode, IObject* object, bool /*force*/)
{
    if (static_cast<int>(mode) != 6)
    {
        m_viewMode = 1;
        resetCameraView(true);
        return true;
    }

    m_viewMode = 6;
    resetTraceCamera();

    IObject* traceObject = object ? object : m_traceObject.get();
    if (traceObject)
    {
        *reinterpret_cast<Mat4*>(_mat1AC) = traceObject->getAbsoluteTransformation();

        m_traceObject = traceObject->getWeakPtr();
        m_traceObject = traceObject->getWeakPtr();                         // repeated as shipped

        setAbsoluteTransformation(traceObject->getAbsoluteTransformation());

        const float radius = getRootParent(traceObject)->getVisualBoundingRadius() * 3.0f;
        _float27C = radius > 3.0f ? radius : 3.0f;

        const Vec3& position = getAbsolutePosition();
        _vec270[0] = position.x;
        _vec270[1] = position.y;
        _vec270[2] = position.z;

        const Mat4& transform = getAbsoluteTransformation();
        _dword250 = transform.m[8];
        _dword254 = transform.m[9];
        _float258 = transform.m[10];

        *reinterpret_cast<Mat4*>(m_transformation) = getTransformationDeath(0.0f);
    }
    else
    {
        followNextPlayer(false);
        if (m_followedPlayerId == -1)
            m_viewMode = 1;
    }

    resetCameraView(true);
    return true;
}

// Mode 1 uses active FOV; other modes use default FOV. Requires the template.
float FreeCamera::getActiveFOV() const
{
    const FreeCameraTemplate* t = static_cast<const FreeCameraTemplate*>(m_template);
    return (m_viewMode == 1) ? t->m_activeFoV : t->m_defaultFoV;
}

// When force is true, reapplies the occupying player's FOV from this camera. False does nothing.
// Requires an occupied player-control object; missing occupancy triggers a fatal diagnostic.
void FreeCamera::resetCameraView(bool update)
{
    if (!update)
        return;

    IPlayerControlObject* pco = 0;
    for (IObject* node = static_cast<IObject*>(this);
         node;
         node = object_detail::LayoutAccess::parent(node))          // +0x28
    {
        pco = static_cast<IPlayerControlObject*>(
                  node->queryInterface(IID_IPlayerControlObject));
        if (pco)
            break;
    }

    if (pco)
    {
        IPlayer* player = pco->getOccupingPlayer();                 // PCO +0x10
        if (player)
        {
            player->setFov(0.0f, 0.0f);                             // IPlayer +0x8C - 0 = "yours"
            return;
        }

        // The site that attests Game/Objects/Base/FreeCamera.cpp line 760. fatal.
        Debug(DEBUGTYPE_6, "Game/Objects/Base/FreeCamera.cpp", 760, __FILE__)
            << "tl: Wanted to update fov for camera " << this
            << " but no occupying player found";
    }
    else
    {
        // The site that attests Game/Objects/Base/FreeCamera.cpp line 765. fatal.
        Debug(DEBUGTYPE_6, "Game/Objects/Base/FreeCamera.cpp", 765, __FILE__)
            << "tl: Wanted to update fov for camera " << this
            << " but no pco found.";
    }
}

// updateCameraView - 0x81E5EE0, 32 B. A virtual forward to resetCameraView, not a direct call: the
// ELF dispatches through the primary table's slot 111. Kept virtual here for the same reason.
void FreeCamera::updateCameraView(bool flag)
{
    this->resetCameraView(flag);
}


// Ignores input while parented; otherwise toggles allowed free-camera modes or dispatches mode-specific input.
// Mode-6 trace input runs only on a server.
void FreeCamera::handlePlayerInput(IPlayer* player, const io::PlayerInput& input,
                                   float deltaTime, unsigned int flags)
{
    if (object_detail::LayoutAccess::parent(this))                     // +0x28
        return;

    // BF2.exe 0x63EC73..0x63ED26 (ledger N2): the local player's free camera zeroes the Falcon's force
    // effects; none exists without the archived device (Code/Archive/NovintFalcon).

    IBase* gameServer = static_cast<IBase*>(game->queryInterface(IID_IGameServer));

    bool toggleRequested = false;
    if (gameServer && serverInfo->m_block.allowFreeCam && m_viewModeToggleArmed)   // +0x19 +0x198
    {
        if ((input.mappingFlags & (1ULL << io::PIAction)) &&
            input.inputs[io::PIAction] > 0.5f)
            toggleRequested = true;
    }

    if (toggleRequested)
    {
        int mode = m_viewMode;                                          // +0x190

        if (mode == 1)
        {
            // Following nobody? Pick someone first - and if there is nobody at all, do not toggle.
            world::IObject* followed = m_traceObject.get();            // +0x28C > control +4
            if (!followed)
            {
                followNextPlayer(false);
                if (m_followedPlayerId == -1)                           // +0x1A8
                    return;
                mode = m_viewMode;                                      // re-read
            }
        }

        if (mode == 6)
        {
            // Leaving the trace: pin the camera where the trace left it.
            setAbsoluteTransformation(*reinterpret_cast<const Mat4*>(m_transformation));  // slot 35
            mode = m_viewMode;                                          // re-read
        }

        m_viewModeToggleArmed = false;
        setViewMode(static_cast<CameraViewMode>(5 * (mode != 6) + 1), 0, false);   // +0x18
        return;
    }

    const bool inTrace = (m_viewMode == 6);
    m_viewModeToggleArmed = true;

    if (!inTrace)
    {
        handlePlayerInputDefault(player, input, deltaTime, flags);
        return;
    }
    if (gameServer)                                                     // tested a second time
        handlePlayerInputCVMTrace(player, input, deltaTime, flags);
}


// ds:0x8ABAF24 - Core/Globals.cpp. Declared per-TU as an IBase* and cast at the point of use,
// exactly as GameLogic.cpp and EngineBase.cpp already do.
extern IBase* heightmapCluster;

namespace {

// Cache registry pointers so camera tuning changes are observed immediately.
float* const g_camSlow   = Vars::getFloat("cam-slow",   0.1f);
float* const g_camNormal = Vars::getFloat("cam-normal", 1.0f);
float* const g_camFast   = Vars::getFloat("cam-fast",  10.0f);
float* const g_camStiffness = Vars::getFloat("cam-stiffness", 1.5f);
float* const g_camInterpolateAttachObject =
    Vars::getFloat("cam-interpolate-attachobject", 1.2f);
float* const g_deathCamTerrainOffset = Vars::getFloat("deathcam-terrainoffset", 0.8f);

inline bool inputPressed(const io::PlayerInput& input, io::PlayerInputMap index)
{
    return (input.mappingFlags & (1ULL << index)) != 0 && input.inputs[index] > 0.5f;
}

// AN axis IS read AS A value, with NO threshold - and an unmapped one reads as 0.0f, not as
// "absent". That is the difference between this and inputPressed.
inline float inputAxis(const io::PlayerInput& input, io::PlayerInputMap index)
{
    return (input.mappingFlags & (1ULL << index)) ? input.inputs[index] : 0.0f;
}

// Re-fetch after each transformation write because setters may recompute the basis.
inline const Mat4& liveTransform(IObject* object)
{
    return object->getParent() ? object->getRelativeTransformation()     // slot 58
                               : object->getAbsoluteTransformation();    // slot 33
}

inline void storeTransform(IObject* object, const Mat4& m)
{
    if (object->getParent())
        object->setRelativeTransformation(m);                            // slot 59
    else
        object->setAbsoluteTransformation(m);                            // slot 35
}

// Leaves nearly unit vectors unchanged; nearly zero vectors collapse to zero.
void normaliseIfNeeded(float& x, float& y, float& z)
{
    const float len2 = x * x + y * y + z * z;
    if (fabsf(len2 - 1.0f) < 1.192092896e-07f)
        return;                                   // already unit - leave it exactly alone
    if (fabsf(len2) < 1.192092896e-07f)
    {
        x = 0.0f; y = 0.0f; z = 0.0f;
        return;
    }
    const float inv = 1.0f / sqrtf(len2);
    x *= inv; y *= inv; z *= inv;
}

} // anonymous namespace

// Advances to the next player while the camera-toggle input is held and presets the trace age to ten seconds on success.
// The caller rearms the latch each tick, so holding the input repeats the hop. Other arguments are unused.
void FreeCamera::handlePlayerInputCVMTrace(IPlayer* /*player*/, const io::PlayerInput& input,
                                           float /*deltaTime*/, unsigned int /*flags*/)
{
    if (inputPressed(input, io::PIToggleCameraMode))            // input 35
    {
        if (m_viewModeToggleArmed)                              // +0x198 PE +0x1E0
        {
            followNextPlayer(false);

            if (m_followedPlayerId != -1)                       // +0x1A8 PE +0x1F0
                _float194 = 10.0f;                              // +0x194 PE +0x1DC

            m_viewModeToggleArmed = false;
        }
    }
    else
    {
        m_viewModeToggleArmed = true;
    }
}


// Moves and rotates the free camera when server settings or demo playback permit it.
// PISprint selects slow speed; PIAltSprint selects fast speed. Keeps the horizon level and clamps above terrain outside developer mode.
// player and flags are unused.
void FreeCamera::handlePlayerInputDefault(IPlayer* /*player*/, const io::PlayerInput& input,
                                          float deltaTime, unsigned int /*flags*/)
{
    IBase* gameServer = static_cast<IBase*>(game->queryInterface(IID_IGameServer));

    if ((!serverInfo->m_block.allowFreeCam && !game->isPlayingDemo()) ||
        (gameLogic->getGameStatus() != 1 &&
         (!gameServer || gameLogic->getGameStatus() != 4)))
        return;

    const float lookY   = inputAxis(input, io::PIMouseLookY);
    const float lookX   = inputAxis(input, io::PIMouseLookX);
    const float strafe  = inputAxis(input, io::PIYaw);
    const float forward = inputAxis(input, io::PIThrottle);

    IObject* root = getRootParent(this);

    // 1. this frame's speed
    if (inputPressed(input, io::PISprint))
        m_cameraSpeed = *g_camSlow;                                  // slow - see the note
    else if (inputPressed(input, io::PIAltSprint))
        m_cameraSpeed = *g_camFast;
    else if (m_cameraSpeed > m_cameraSpeedThreshold && forward > 0.0f)
        m_cameraSpeed = *g_camFast;                                  // still boosting - hold it
    else
        m_cameraSpeed = *g_camNormal;

    const float step = m_cameraSpeed * 20.0f * deltaTime;

    // 2. strafe along the right row
    {
        Vec3 pos = root->getAbsolutePosition();                      // slot 27
        const Mat4& t = liveTransform(root);
        const float d = strafe * step;
        pos.x += t.m[0] * d;  pos.y += t.m[1] * d;  pos.z += t.m[2] * d;
        root->setAbsolutePosition(pos);                              // slot 28
    }

    // 3. fly along the forward row
    {
        Vec3 pos = root->getAbsolutePosition();
        const Mat4& t = liveTransform(root);
        const float d = forward * step;
        pos.x += t.m[8] * d;  pos.y += t.m[9] * d;  pos.z += t.m[10] * d;
        root->setAbsolutePosition(pos);
    }

    // Skip near-zero yaw and pitch rotations.
    {
        const float degrees = lookX * deltaTime * 100.0f;
        Mat4 t = liveTransform(root);
        if (fabsf(degrees) > 1.192092896e-07f)
        {
            Mat4 rot;
            for (int i = 0; i < 16; ++i) rot.m[i] = 0.0f;
            rot.m[0] = 1.0f; rot.m[5] = 1.0f; rot.m[10] = 1.0f; rot.m[15] = 1.0f;
            setRotateAboutLine(rot, degrees, *reinterpret_cast<const Vec3*>(&t.m[4]));   // UP
            const Mat4 was = t;
            t.rotMult(was, rot);
        }
        storeTransform(root, t);
    }
    {
        const float degrees = lookY * deltaTime * 100.0f;
        Mat4 t = liveTransform(root);
        if (fabsf(degrees) > 1.192092896e-07f)
        {
            Mat4 rot;
            for (int i = 0; i < 16; ++i) rot.m[i] = 0.0f;
            rot.m[0] = 1.0f; rot.m[5] = 1.0f; rot.m[10] = 1.0f; rot.m[15] = 1.0f;
            setRotateAboutLine(rot, degrees, *reinterpret_cast<const Vec3*>(&t.m[0]));   // right
            const Mat4 was = t;
            t.rotMult(was, rot);
        }
        storeTransform(root, t);
    }

    // 5. PILie / PICrouch move straight up and down, in world space
    {
        Vec3 pos = root->getAbsolutePosition();
        if (inputPressed(input, io::PILie))
            pos.y += step;
        else if (inputPressed(input, io::PICrouch))
            pos.y -= step;
        root->setAbsolutePosition(pos);
    }

    // 6. re-orthonormalise, right row first and levelled
    {
        Mat4 t = liveTransform(root);
        float rx = t.m[0], ry = 0.0f, rz = t.m[2];      // Y forced TO zero - no roll, ever
        normaliseIfNeeded(rx, ry, rz);
        t.m[0] = rx;  t.m[1] = ry;  t.m[2] = rz;
        storeTransform(root, t);
    }
    {
        Mat4 t = liveTransform(root);
        // up = forward x right, in the element order the ELF multiplies them
        float ux = t.m[2] * t.m[9]  - t.m[1] * t.m[10];
        float uy = t.m[0] * t.m[10] - t.m[2] * t.m[8];
        float uz = t.m[1] * t.m[8]  - t.m[0] * t.m[9];
        normaliseIfNeeded(ux, uy, uz);
        t.m[4] = ux;  t.m[5] = uy;  t.m[6] = uz;
        storeTransform(root, t);
    }
    {
        Mat4 t = liveTransform(root);
        // forward = up x right, same shape
        float fx = t.m[6] * t.m[1] - t.m[5] * t.m[2];
        float fy = t.m[4] * t.m[2] - t.m[6] * t.m[0];
        float fz = t.m[5] * t.m[0] - t.m[4] * t.m[1];
        normaliseIfNeeded(fx, fy, fz);
        t.m[8] = fx;  t.m[9] = fy;  t.m[10] = fz;
        storeTransform(root, t);
    }

    // 7. the ground clamp, unless this is a developer build
    const bool developer = settings->boolGet(std::string("GSDeveloper"), false);
    if (!developer && m_clampToGround && heightmapCluster)
    {
        Vec3 pos = root->getAbsolutePosition();
        const float ground =
            reinterpret_cast<IHeightmapCluster*>(heightmapCluster)
                ->getHeightInWorldCoords(pos.x, pos.z) + 0.2f;      // slot 27 (+0x6C)
        if (ground > pos.y)
        {
            pos.y = ground;
            root->setAbsolutePosition(pos);
        }
    }
}

// Mode 6 selects death/follow-player view; other modes use the object's absolute transform.
// Copies the chosen transform into the member and returns that reference.
const Mat4& FreeCamera::getTransformation(float deltaTime)
{
    const Mat4* transform;
    if (m_viewMode == 6)                                     // +0x190
    {
        if (m_followedPlayerId == -1)                        // +0x1A8
            transform = &getTransformationDeath(deltaTime);
        else
            transform = &getTransformationFollowPlayer(deltaTime);
    }
    else
    {
        transform = &getAbsoluteTransformation();            // virtual +0x84
    }

    *reinterpret_cast<Mat4*>(m_transformation) = *transform;         // +0x210 the float[16]
    return *reinterpret_cast<const Mat4*>(m_transformation);
}

// getTraceObject - ELF 0x81E4060 (141 B) / PE 0x5F4080 (106 B), read in full. Walk this camera's
// object-parent chain for its IPlayerControlObject face, ask the occupying player which player is
// being traced, and return that live player's vehicle. The occupant dereference is deliberately
// unchecked in both ports: a FreeCamera in this mode is required to be occupied.
IObject* FreeCamera::getTraceObject()
{
    IPlayerControlObject* pco = 0;
    for (IObject* object = this; object; object = object->getParent())
    {
        pco = static_cast<IPlayerControlObject*>(
            object->queryInterface(IID_IPlayerControlObject));
        if (pco)
            break;
    }

    IPlayer* occupant = 0;
    if (pco)
        occupant = pco->getOccupingPlayer();

    const int tracePlayerId = occupant->getTracePlayerId();             // unchecked as shipped
    IPlayer* tracePlayer = playerManager->getPlayerFromId(tracePlayerId);
    if (tracePlayer && tracePlayer->getIsAlive())
        return tracePlayer->getVehicle();
    return 0;
}

// getTransformationDeath - ELF 0x81E40F0 (1,760 B) / PE 0x5F4ED0 (876 B), read in full.
// The killer/death camera eases a quaternion-like direction and radius around its weak target,
// optionally steering toward the traced player's vehicle once it is far enough away. Three retail
// details are intentional: q.w is ignored by the first-frame test; the wanted quaternion is built
// before getTraceObject may refresh the stored direction; and attachment interpolation has only an
// upper clamp, so a negative delta time extrapolates.
const Mat4& FreeCamera::getTransformationDeath(float deltaTime)
{
    const bool first =
        _vec260[0] * _vec260[0] + _vec260[1] * _vec260[1] + _vec260[2] * _vec260[2]
        <= 0.0f;

    // The weak target is re-read for the test, flag load, and position dispatch in both ports.
    if (m_traceObject.get() && !(m_traceObject.get()->getFlags() & 1u))
    {
        const Vec3& position = m_traceObject.get()->getAbsolutePosition();
        _vec270[0] = position.x;
        _vec270[1] = position.y;
        _vec270[2] = position.z;
    }

    Quat wanted;
    wanted.x = -_dword250;
    wanted.y = -_dword254 + 0.3f;
    wanted.z = -_float258;
    wanted.w = 1.0f;
    normaliseIfNeeded(wanted.x, wanted.y, wanted.z);                      // xyz only

    IObject* traceObject = getTraceObject();
    if (traceObject && _dword25C > _float27C * 0.8f)
    {
        const Vec3& position = traceObject->getAbsolutePosition();
        _dword250 = position.x - _vec270[0];
        _dword254 = position.y - _vec270[1];
        _dword254 = 0.0f;
        _float258 = position.z - _vec270[2];
        normaliseIfNeeded(_dword250, _dword254, _float258);
    }

    float turn = deltaTime * *g_camStiffness;
    if (turn > 1.0f)
        turn = 1.0f;
    if (turn < 0.0f)
        turn = 0.0f;

    if (first)
    {
        _vec260[0] = -_dword250 * 0.1f;
        _vec260[1] = -_dword254 * 0.1f;
        _vec260[2] = -_float258 * 0.1f;
        // _float26C (q.w) is not written on this arm.
        _dword25C = 0.5f;
        _vec280[0] = _vec270[0];
        _vec280[1] = _vec270[1];
        _vec280[2] = _vec270[2];
    }
    else
    {
        Quat current;
        current.x = _vec260[0];
        current.y = _vec260[1];
        current.z = _vec260[2];
        current.w = _float26C;

        Quat blended;
        blended.slerp(current, wanted, turn);
        normaliseIfNeeded(blended.x, blended.y, blended.z);               // xyz only
        _vec260[0] = blended.x;
        _vec260[1] = blended.y;
        _vec260[2] = blended.z;
        _float26C = blended.w;
    }

    float attach = deltaTime * *g_camInterpolateAttachObject;
    if (attach > 1.0f)
        attach = 1.0f;                                                    // no lower clamp
    _vec280[0] += (_vec270[0] - _vec280[0]) * attach;
    _vec280[1] += (_vec270[1] - _vec280[1]) * attach;
    _vec280[2] += (_vec270[2] - _vec280[2]) * attach;

    _dword25C = _float27C * turn + _dword25C * (1.0f - turn);

    Vec3 eye;
    eye.x = _vec280[0] + _vec260[0] * _dword25C;
    eye.y = _vec280[1] + _vec260[1] * _dword25C;
    eye.z = _vec280[2] + _vec260[2] * _dword25C;

    const float ground =
        reinterpret_cast<IHeightmapCluster*>(heightmapCluster)
            ->getHeightInWorldCoords(eye.x, eye.z) + *g_deathCamTerrainOffset;
    if (ground > eye.y)
        eye.y = ground;

    Vec3 target;
    target.x = _vec270[0];
    target.y = _vec270[1];
    target.z = _vec270[2];
    Vec3 up;
    up.x = 0.0f;
    up.y = 1.0f;
    up.z = 0.0f;

    Mat4& output = *reinterpret_cast<Mat4*>(m_transformation);
    calcLookAtMatrix(output, eye, target, up);
    return output;
}

// getTransformationFollowPlayer - ELF 0x81E3A70 (1,505 B) / PE 0x5F43A0 (897 B), read in full.
// The followed player's active camera supplies the look target and basis. The vehicle is fetched
// three separate times as shipped: once for the saved root, once for its radius, and once to begin
// the IPlayerControlObject parent walk. The float parameter is deliberately unused; smoothing uses
// a fixed 1-exp(-1/15) factor on every call.
const Mat4& FreeCamera::getTransformationFollowPlayer(float /*deltaTime*/)
{
    IPlayer* followedPlayer = playerManager->getPlayerFromId(m_followedPlayerId);
    if (!followedPlayer || !followedPlayer->getIsAlive())
        return getAbsoluteTransformation();

    ICameraObject* playerCamera = followedPlayer->getCamera();
    IObject* cameraObject = static_cast<IObject*>(playerCamera->queryInterface(IID_IObject));
    const Mat4 playerTransform = cameraObject->getAbsoluteTransformation();

    Vec3 target;
    target.x = playerTransform.m[12];
    target.y = playerTransform.m[13];
    target.z = playerTransform.m[14];

    IObject* root = getRootParent(followedPlayer->getVehicle());          // first fetch
    float distance =
        getRootParent(followedPlayer->getVehicle())->getPhysicBoundingRadius(); // second fetch

    IPlayerControlObject* pco = 0;
    for (IObject* object = followedPlayer->getVehicle(); object; object = object->getParent())
    {
        pco = static_cast<IPlayerControlObject*>(
            object->queryInterface(IID_IPlayerControlObject));            // third fetch starts walk
        if (pco)
            break;
    }

    if (pco)
    {
        const Vec3 offset = pco->getRidingWithOffset();
        distance *= pco->getRidingWithDistMod();

        // The negated <= chain preserves retail's NaN behavior: a NaN offset enters this arm.
        if (!(fabsf(offset.x) <= 1.192092896e-07f
              && fabsf(offset.y) <= 1.192092896e-07f
              && fabsf(offset.z) <= 1.192092896e-07f))
        {
            target.x = ((target.x - playerTransform.m[8] * offset.z)
                                  - playerTransform.m[0] * offset.x)
                                  + playerTransform.m[4] * offset.y;
            target.y = ((target.y - playerTransform.m[9] * offset.z)
                                  - playerTransform.m[1] * offset.x)
                                  + playerTransform.m[5] * offset.y;
            target.z = ((target.z - playerTransform.m[10] * offset.z)
                                  - playerTransform.m[2] * offset.x)
                                  + playerTransform.m[6] * offset.y;
        }
    }

    Vec3 desired;
    desired.x = -playerTransform.m[8]  * distance
              +  playerTransform.m[4]  * distance * 0.3f;
    desired.y = -playerTransform.m[9]  * distance
              +  playerTransform.m[5]  * distance * 0.3f;
    desired.z = -playerTransform.m[10] * distance
              +  playerTransform.m[6]  * distance * 0.3f;

    Vec3 correction;
    correction.x = desired.x - _vec1F4[0];
    correction.y = desired.y - _vec1F4[1];
    correction.z = desired.z - _vec1F4[2];

    IPhysicsNode* physics =
        static_cast<IPhysicsNode*>(object_detail::LayoutAccess::physicsNode(root));
    if (physics)
    {
        const Vec3& speed = physics->getPositionalSpeed();
        correction.x -= speed.x * 0.6f;
        correction.y -= speed.y * 0.6f;
        correction.z -= speed.z * 0.6f;
    }

    const float smoothing = 1.0f - expf(-0.06666667f);
    Vec3 eye;
    eye.x = correction.x * smoothing + _vec1F4[0] + target.x;
    eye.y = correction.y * smoothing + _vec1F4[1] + target.y;
    eye.z = correction.z * smoothing + _vec1F4[2] + target.z;

    const float ground =
        reinterpret_cast<IHeightmapCluster*>(heightmapCluster)
            ->getHeightInWorldCoords(eye.x, eye.z) + 1.0f;
    if (ground > eye.y)
        eye.y = ground;

    Vec3 up;
    up.x = 0.0f;
    up.y = 1.0f;
    up.z = 0.0f;
    Mat4& output = *reinterpret_cast<Mat4*>(m_transformation);
    calcLookAtMatrix(output, eye, target, up);

    _vec1F4[0] = eye.x - target.x;
    _vec1F4[1] = eye.y - target.y;
    _vec1F4[2] = eye.z - target.z;
    return output;
}

// Follows the next living human teammate, wrapping around; retains the ID if none is available.
// Requires an occupant. Ignores the argument and leaves trace timing to the caller.
void FreeCamera::followNextPlayer(bool /*unused*/)
{
    const std::list<IPlayer*>& players = playerManager->getPlayers();
    int firstOtherId = -1;
    bool passedCurrent = false;
    for (std::list<IPlayer*>::const_iterator it = players.begin(); it != players.end(); ++it)
    {
        IPlayer* player = *it;
        // [PLATFORM] PE reads the occupant's team first; ELF reads the candidate's first.
#if defined(_WIN32)
        const int occupantTeam = m_occupingPlayer.get()->getTeam();
        const int candidateTeam = player->getTeam();
#else
        const int candidateTeam = player->getTeam();
        const int occupantTeam = m_occupingPlayer.get()->getTeam();
#endif
        if (candidateTeam != occupantTeam || !player->getIsAlive() || player->getIsAIPlayer())
            continue;

        const int candidateId = player->getId();
        if (candidateId == m_occupingPlayer.get()->getId())
            continue;
        if (player->getId() == m_followedPlayerId)
        {
            passedCurrent = true;
        }
        else
        {
            if (firstOtherId == -1)
                firstOtherId = player->getId();
            if (passedCurrent)
            {
                m_followedPlayerId = player->getId();
                return;
            }
        }
    }
    if (firstOtherId != -1)
        m_followedPlayerId = firstOtherId;
}

} // namespace world
} // namespace hfe
} // namespace dice
