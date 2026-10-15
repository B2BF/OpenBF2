// Path: attested by __FILE__; recovered from symbolized ELF and cross-checked with PE.
// Camera, FreeCamera, OverheadCamera and CameraTemplate behavior.
// View modes have separate transformation paths; view changes can switch visible geometry.
// FreeCamera supplies spectator/death views; OverheadCamera is a replicated commander object.
// Keep request/force position paths and the separate general/near-vehicle shake behavior.

#include "Camera.h"
#include "../../../Core/System.h"
#include "../../../Core/Ids.h"
#include "../../../Core/Debug.h"                 // setViewMode's attested line 1171
#include "../../../Core/EventManager.h"          // the client tail's postEvent
#include "../../../Math/Mat4.h"
#include "../../GameServer/ServerInfo.h"         // allowNoseCam / allowExternalViews
#include "../../Main/Game.h"                    // retail client keeps the isDedicated() gate
#include "../../../Scene/Object/PlayerManager.h" // getLocalHumanPlayer
#include "../../../Physics/HeightmapCluster.h"
#include "../../../Physics/IPhysicsNode.h"
#include "../IPlayer.h"
#include <cmath>
#include <vector>
#include "../ObjectLayoutAccess.h"      // geometry(IObject*) - +0x38
#if !DEDICATED && defined(_MSC_VER)
#include "../../../Geom/GeometryInstanceData.h"  // [TEMP] vehicle-geometry-trace: +0x28/+0x2C
#include "../IObjectTemplate.h"                  // [TEMP] vehicle-geometry-trace: getName
#include <cstdio>
#include <cstdlib>
#endif

namespace dice {
namespace hfe {
namespace world {

// ELF `_ZN4dice3hfe5world20g_explosionShakeTimeE` at 0x8AAE0D0 is a reference whose
// lifetime-extended float at 0x8AAE0CC is initialised to 0.6f. PE dword_88E38C has the same
// reference/value shape. Keeping the reference form preserves the shipped global rather than
// turning the value into an unrelated per-camera literal.
const float& g_explosionShakeTime = 0.6f;

// ds:0x8ABAF24 in the ELF, with the matching singleton slot in both PEs.  Core/Globals.cpp owns
// the storage; every shipped camera terrain/water query dereferences it without a null guard.
extern IBase* heightmapCluster;

// The chase function reads two TU-local reference-backed defaults in both images.  ELF
// > 0x8AAE090 holds 2.0f and 0x8AAE09C -> 0x8AAE098 holds 0.0f; PE
// off_8326D0/off_8326D4 have the same reference/value pairs.
namespace {
const float& s_chaseDistanceScale = 2.0f;
const float& s_chaseAccelerationScale = 0.0f;
// BF2.exe 0x853640 initializes 0x9EBEC4 and binds the reference at 0x9EBEEC.
const float& s_insideSurfaceClearance = 0.09f;

inline float cameraLengthSquared(const Vec3& v)
{
    return v.x * v.x + v.y * v.y + v.z * v.z;
}

// This is the Vec3 inline normalisation emitted in the shipped camera arms.  In particular, an
// already-unit vector is left bit-for-bit alone, while a degenerate vector becomes exact zero.
inline void cameraNormalize(Vec3& v)
{
    const float lengthSquared = cameraLengthSquared(v);
    if (fabsf(lengthSquared - 1.0f) >= 1.1920929e-7f)
    {
        if (fabsf(lengthSquared) >= 1.1920929e-7f)
        {
            const float inverseLength = 1.0f / sqrtf(lengthSquared);
            v.x *= inverseLength;
            v.y *= inverseLength;
            v.z *= inverseLength;
        }
        else
        {
            v.x = 0.0f;
            v.y = 0.0f;
            v.z = 0.0f;
        }
    }
}

inline Vec3 cameraTransformDirection(const Mat4& transform, const Vec3& value)
{
    Vec3 result = {
        transform.m[0] * value.x + transform.m[4] * value.y + transform.m[8]  * value.z,
        transform.m[1] * value.x + transform.m[5] * value.y + transform.m[9]  * value.z,
        transform.m[2] * value.x + transform.m[6] * value.y + transform.m[10] * value.z
    };
    return result;
}

inline Vec3 cameraTransformPoint(const Mat4& transform, const Vec3& value)
{
    Vec3 result = cameraTransformDirection(transform, value);
    result.x += transform.m[12];
    result.y += transform.m[13];
    result.z += transform.m[14];
    return result;
}

inline IHeightmapCluster* cameraHeightmapCluster()
{
    return static_cast<IHeightmapCluster*>(heightmapCluster);
}
} // namespace

// Adapts the geometry slots used for cockpit switching without extending the partial interface.
#if defined(_MSC_VER)
#  define BF2_VCALL __thiscall
#else
#  define BF2_VCALL
#endif

template <class Fn>
inline Fn vslot(void* object, unsigned int byteOffset)
{
    return reinterpret_cast<Fn>((*reinterpret_cast<void***>(object))[byteOffset / 4]);
}

typedef int  (BF2_VCALL* VFn_geomCount)(void*);
typedef void (BF2_VCALL* VFn_geomSet)(void*, int);

// [PLATFORM] Geometry dispatch slots differ between ELF and PE.
// Use the verified mapping for each operation; a uniform offset adjustment is not valid for the entire interface.
#if defined(_MSC_VER)
const unsigned int kGeomSetSubGeometrySlot   = 0x6C;
const unsigned int kGeomSubGeometryCountSlot = 0x74;
const unsigned int kGeomSetLodSlot           = 0x78;
#else
const unsigned int kGeomSetSubGeometrySlot   = 0x70;
const unsigned int kGeomSubGeometryCountSlot = 0x78;
const unsigned int kGeomSetLodSlot           = 0x7C;
#endif

inline int  geomSubGeometryCount(void* g)     { return vslot<VFn_geomCount>(g, kGeomSubGeometryCountSlot)(g); }
inline void geomSetSubGeometry(void* g, int i){ vslot<VFn_geomSet>(g, kGeomSetSubGeometrySlot)(g, i); }
inline void geomSetLod(void* g, int i)        { vslot<VFn_geomSet>(g, kGeomSetLodSlot)(g, i); }

#if !DEDICATED && defined(_MSC_VER)
// [TEMP] [CHOSEN, not recovered] Optional BF2_TRACE_VEHICLE_GEOM diagnostics for geometry switches and local camera roots.
// Reports initial samples and later changes with a bounded output count; disabled by default.
// [RETIRE-WHEN: vehicle-geometry-trace] Remove after vehicle visibility and sub-geometry switching are validated.
namespace vehgeom
{
typedef float (BF2_VCALL* VFn_geomFloat)(void*);

bool enabled()
{
    static const bool on = []() {
        const char* const value = std::getenv("BF2_TRACE_VEHICLE_GEOM");
        return value && value[0] == '1';
    }();
    return on;
}

unsigned int g_lines = 0;
bool budget() { return g_lines++ < 400u; }

int   geomIndex(void* g) { return vslot<VFn_geomCount>(g, 0x70)(g); }
int   lodIndex(void* g)  { return vslot<VFn_geomCount>(g, 0x7C)(g); }
float geomFov(void* g)   { return vslot<VFn_geomFloat>(g, 0x14)(g); }

const char* templateName(IObject* object)
{
    return (object && object->getTemplate()) ? object->getTemplate()->getName().c_str() : "<none>";
}

struct RootState
{
    void* geometry;
    int geom, count, lod;
    float fov, baseOpacity, cullFade;
    unsigned int flags;
    int mode;
};

void sample(IObject* root, int mode, RootState& s)
{
    s.geometry = object_detail::LayoutAccess::geometry(root);
    s.geom = s.count = s.lod = -2;
    s.fov = 0.0f;
    s.baseOpacity = s.cullFade = -1.0f;
    if (s.geometry)
    {
        s.geom = geomIndex(s.geometry);
        s.count = geomSubGeometryCount(s.geometry);
        s.lod = lodIndex(s.geometry);
        s.fov = geomFov(s.geometry);
        const ::dice::hfe::geom::GeometryInstanceData* const data =
            static_cast<const ::dice::hfe::geom::GeometryInstanceData*>(s.geometry);
        s.baseOpacity = data->m_float28;
        s.cullFade = data->m_float2C;
    }
    s.flags = root->getFlags();
    s.mode = mode;
}

bool differs(const RootState& a, const RootState& b)
{
    return a.geometry != b.geometry || a.geom != b.geom || a.count != b.count || a.lod != b.lod
        || a.fov != b.fov || a.baseOpacity != b.baseOpacity || a.cullFade != b.cullFade
        || (a.flags & 3u) != (b.flags & 3u) || a.mode != b.mode;
}

void report(const char* which, IObject* root, const RootState& s)
{
    if (!budget())
        return;
    std::fprintf(stderr,
        "[VEHGEOM-ROOT] %s root=%p template=%s geometry=%p geom=%d/%d lod=%d fov=%.3f "
        "baseOpacity=%.3f cullFade=%.3f disabled=%u visible=%u cameraMode=%d\n",
        which, static_cast<void*>(root), templateName(root), s.geometry, s.geom, s.count, s.lod,
        s.fov, s.baseOpacity, s.cullFade, s.flags & 1u, (s.flags >> 1) & 1u, s.mode);
    std::fflush(stderr);
}

void observeLocalVehicle(Camera* camera)
{
    if (!enabled())
        return;
    IPlayer* const local = playerManager->getLocalHumanPlayer();
    if (!local || local->getCamera() != static_cast<ICameraObject*>(camera))
        return;
    IObject* const root = getRootParent(camera);
    if (!root)
        return;

    static WeakPtr<IObject> current;
    static WeakPtr<IObject> previous;
    static RootState currentState;
    static RootState previousState;
    static unsigned int currentSamples = 0;
    static unsigned int previousSamples = 0;

    if (current.get() != root)
    {
        previous = current;                 // the one the local player just left, if any
        previousState = currentState;
        previousSamples = 0;
        current = root->getWeakPtr();
        currentSamples = 0;
        if (budget())
        {
            std::fprintf(stderr, "[VEHGEOM-ROOT] camera %s now under root=%p template=%s; left=%p %s\n",
                         templateName(static_cast<IObject*>(camera)), static_cast<void*>(root),
                         templateName(root),
                         static_cast<void*>(previous.get()), templateName(previous.get()));
            std::fflush(stderr);
        }
    }

    RootState state;
    sample(root, static_cast<int>(camera->getViewMode()), state);
    if (currentSamples++ < 8u || differs(state, currentState))
        report("current", root, state);
    currentState = state;

    if (IObject* const left = previous.get())
    {
        if (left != root)
        {
            RootState leftState;
            sample(left, -1, leftState);
            if (previousSamples++ < 8u || differs(leftState, previousState))
                report("left", left, leftState);
            previousState = leftState;
        }
    }
}
} // namespace vehgeom
#endif // [TEMP] vehicle-geometry-trace


// Returns a projectile's owning player through its projectile interface.
IPlayer* projectileOwner(void* projectile);                      // defined in GenericProjectile.cpp

// world::toggleViewMode - a free function, in Game/GameLogic/ServerGameLogic.cpp
// (0x8105A40, 134 B). Declared here because the tree has no header for it.
void toggleViewMode(ICameraObject* camera, bool forward);

// Copies camera identity from the template and initializes represented view, matrix and chase state.
// Opaque presentation fields retain uninitialized storage until their constructor behavior is known.
Camera::Camera(const CameraTemplate* templ)
    : RotationalBundle(templ)
    , m_refCount(1)                                  // +0x1B4 ONE, not zero
    , m_viewMode(1)                                  // +0x1B8 CVMInside
    , m_float1BC(8.0f)
    , m_chaseRotation{0.0f, 0.0f, 0.0f, 1.0f}
    , m_chaseDistance(0.0f)
    , m_frameTime(0.0f)
    , m_recordingValid(false)
    , m_isActive(false)
    , m_pcoId(templ->getDefaultPcoId())              // +0x1E4 <- template +532
    , m_traceAhead{0.0f, 0.0f, 0.0f}
    , m_traceOrigin{0.0f, 0.0f, 0.0f}
    , m_dword200(0)
    , m_dword204(0)
    , m_float208(-1.0f)
    , m_externCameraTrans(Mat4::getIdentity())
    , m_previousViewMode(1)
    , m_dword250(8)
    , m_traceObjectId(0)
    , m_dword258(-1)
    , m_float25C(1.0f)
    , m_defaultTransformation(Mat4::getIdentity())
    , m_cameraId(templ->getDefaultCameraId())        // +0x2A0 <- template +540
    , m_interpolatedMode(false)
    , m_explosionShakePeak(0.0f)
    // m_explosionShakeStartTime is deliberately absent: both shipped ctors leave the qword
    // untouched, and handleExplosion writes it before the accepted-shake path can consume it.
    , m_speedShakeRoll(0.0f)
    , m_updateCameraRecoil(false)
    , m_recoilImpulseSpeed(0.0f)
    , m_recoilImpulseSize(0.0f)
    , m_explosionShakeTimeRemaining(0.0f)
    , m_explosionShakeMagnitude(1.0f)
    , m_cameraRecoilX(0.0f)
    , m_recoilState314(0.0f)
    , m_soldierShakeX(0.0f)
    , m_soldierShakeY(0.0f)
    , m_cameraRecoilY(0.0f)
    , m_soldierShakeRoll(0.0f)
    , m_soldierShakePitch(0.0f)
    , m_explosionShakeRoll(0.0f)
{
    // the ELF's setViewMode(1, 0, false) lands here and returns at once - m_viewMode is already 1
    // and force is false. Written out so the equivalence is visible rather than assumed.
}

// , 58 B.  Only the two first-person vehicle-camera
// modes accept the impulse.  The shipped body sets the update latch before replacing the two
// authored impulse values and clearing the accumulated horizontal recoil.
void Camera::addRecoilImpulse(float speed, float size)
{
    if (m_viewMode == 1 || m_viewMode == 2)
    {
        m_updateCameraRecoil = true;
        m_recoilImpulseSpeed = speed;
        m_recoilImpulseSize = size;
        m_cameraRecoilX = 0.0f;
    }
}

// , 101 B. Both ports restart the short presentation
// envelope and replace its magnitude on every call. The peak/time stamp, however, advance only
// when authored shaking is enabled and this impulse is strictly stronger than the recorded peak.
// The System call is intentionally unguarded and the ctor intentionally leaves the qword alone;
// the first accepted value necessarily writes both peak and time together.
void Camera::handleExplosion(float amount)
{
    m_explosionShakeTimeRemaining = g_explosionShakeTime;
    m_explosionShakeMagnitude = amount;

    if (static_cast<const CameraTemplate*>(m_template)->getCameraShakeSensitivity() != 0.0f
        && amount > m_explosionShakePeak)
    {
        m_explosionShakePeak = amount;
        m_explosionShakeStartTime = g_system->getExactTime();
    }
}

// THE ONE that unblocks findCameras. It is also the clearest statement of the class's shape:
// two of the four answers are SUB-object addresses, not this.
void* Camera::queryInterface(unsigned int iid) const
{
    if (iid == IID_IBase || iid == IID_IObject)
        return const_cast<Camera*>(this);
    if (iid == IID_ICameraObject)
        return static_cast<ICameraObject*>(const_cast<Camera*>(this));      // ELF: this + 0x1B0
    // The unmodeled CameraScript interface is not exposed through queryInterface.
    return 0;
}

// / 0x81DFD00 / 0x81DFD30. The count is +0x1B4, NOT Object's.
void Camera::addRef() { ++m_refCount; }        // IBase::addRef returns void
int Camera::getRef()  { return m_refCount; }

// . It deletes through the vtable's second destructor slot on the 1 -> 0
// transition, and the null test after the decrement is dice's, not defensive coding.
int Camera::release()
{
    const int now = m_refCount - 1;
    const bool wasLast = (m_refCount == 1);
    m_refCount = now;
    if (wasLast)
        delete this;
    return now;
}

void Camera::setCameraId(int id)          { m_cameraId = id; }
int  Camera::getCameraId()                { return m_cameraId; }
bool Camera::isActive() const             { return m_isActive; }
void Camera::setIsActive(bool active)     { m_isActive = active; }
void Camera::setInterpolatedMode(bool on) { m_interpolatedMode = on; }
void Camera::setTraceObjectId(unsigned int id) { m_traceObjectId = id; }
unsigned int Camera::getTraceObjectId()   { return m_traceObjectId; }

CameraViewMode Camera::getViewMode() const
{
    return static_cast<CameraViewMode>(m_viewMode);
}

// . It answers the template's flag, not a per-camera one.
bool Camera::getToggleMouseLook()
{
    return static_cast<const CameraTemplate*>(getTemplate())->getToggleMouseLook();   // +564
}

// Returns interior lighting permissions in inside view and true in other views.
// Dynamic sun is enabled when the template's inside static-sun flag is clear. Requires a valid template.
bool Camera::getAllowDynamicLights() const
{
    if (m_viewMode == 1)                                                      // +0x1B8 CVMInside
        return static_cast<const CameraTemplate*>(getTemplate())
                   ->getAllowInsideDynamicLights();                           // +740
    return true;
}

bool Camera::getAllowDynamicSun() const
{
    if (m_viewMode == 1)                                                      // +0x1B8 CVMInside
        return !static_cast<const CameraTemplate*>(getTemplate())
                    ->getAllowInsideStaticSun();                              // +741, XOR 1
    return true;
}

// Returns the template's interior lighting and damage settings in every view mode.
// Vector results alias the template. Requires a valid template.
const Vec3& Camera::getStaticSunDirection() const
{
    return static_cast<const CameraTemplate*>(getTemplate())->getInsideStaticSunDirection();
}

const Vec3& Camera::getStaticSunColor() const
{
    return static_cast<const CameraTemplate*>(getTemplate())->getInsideStaticSunColor();
}

const Vec3& Camera::getStaticSunDamageColor() const
{
    return static_cast<const CameraTemplate*>(getTemplate())->getInsideStaticSunDamageColor();
}

float Camera::getInsideDamageFrequency() const
{
    return static_cast<const CameraTemplate*>(getTemplate())->getInsideDamageFrequency();
}

float Camera::getInsideDamageThreshold() const
{
    return static_cast<const CameraTemplate*>(getTemplate())->getInsideDamageThreshold();
}

const Vec3& Camera::getStaticSunAmbientColor() const
{
    return static_cast<const CameraTemplate*>(getTemplate())->getInsideStaticSunAmbientColor();
}

// getExternCameraTrans - ELF 0x81DFFD0, 13 B. +0x40. Swept with the block above because it is the
// same shape and the same RVA-0 risk: `mov eax, [ebp+this]` / `add eax, 20Ch` / `retn`, the address
// of the camera's OWN Mat4, not the template's.
Mat4* Camera::getExternCameraTrans()
{
    return &m_externCameraTrans;                                              // +0x20C
}

// / PE ICameraObject thunk 0x5F3010: the complete matrix is passed BY value.
void Camera::setExternCameraTrans(Mat4 transform)
{
    m_externCameraTrans = transform;
}

// / PE ICameraObject body 0x5F2F10. Preserve virtual dispatch and ignore its result.
void Camera::prevViewMode()
{
    setViewMode(static_cast<CameraViewMode>(m_previousViewMode), 0, false);
}

// -1 IS AN "UNCACHED" sentinel, NOT A value. On the first call the camera walks its parent
// chain for the nearest IPlayerControlObject and caches that object's pcoId. findCameras keys the
// whole camera map on this, so it is what decides which PCO a camera belongs to.
// A camera with no PCO above it caches zero - not -1 - so the walk happens once and then the
// camera permanently claims pco 0. dice's; reproduced.
int Camera::getPcoId()
{
    if (m_pcoId != -1)
        return m_pcoId;

    IPlayerControlObject* pco = 0;
    for (IObject* node = this; node; node = node->getParent())
    {
        pco = static_cast<IPlayerControlObject*>(node->queryInterface(IID_IPlayerControlObject));
        if (pco)
            break;
    }

    m_pcoId = pco ? pco->getPcoId() : 0;   // zero, not -1 - the walk never repeats
    return m_pcoId;
}

// A parentless camera IS NOT initialised AT ALL - it returns before setting the inited byte, so
// init() on a detached camera is a silent no-op rather than an error.
void Camera::init()
{
    RotationalBundle::init();
    if (!getParent())
        return;
    updateFlags(0u, 2048u);
}

// A request for the current mode is accepted without work unless forced. Otherwise the seven-way
// validation switch checks CameraTemplate support, applies the server's external/nose policy,
// seeds Chase/Trace state as required, changes client geometry, resets FOV, and sends the local
// vehicle notification/event tail. Unknown or disallowed modes return false rather than clamping.
bool Camera::setViewMode(CameraViewMode mode, IObject* object, bool force)
{
    int m = static_cast<int>(mode);

    // THE early-OUT. The constructor's setViewMode(1, 0, false) lands here and returns at once.
    if (m_viewMode == m && !force)
        return true;

    const CameraTemplate* t = static_cast<const CameraTemplate*>(getTemplate());

    switch (m)
    {
    case 0:                                    // mode 0 IS remapped TO 1 - there is no mode 0
        m = 1;
        // fall through
    case 1:                                    // CVMInside
        if (!t->getCVMInside())
            return false;
        break;

    case 2:                                    // CVMNose - TWO gates, template AND server
        if (!t->getCVMNose())
            return false;
        if (!serverInfo->m_block.allowNoseCam)
            return false;
        break;

    case 3:                                    // CVMChase
        if (!t->getCVMChase() || !serverInfo->m_block.allowExternalViews)
            return false;
        resetChaseCamera();
        break;

    case 4:                                    // CVMFrontChase
        if (!t->getCVMFrontChase() || !serverInfo->m_block.allowExternalViews)
            return false;
        resetChaseCamera();
        break;

    case 5:                                    // CVMFlyBy
        if (!t->getCVMFlyBy() || !serverInfo->m_block.allowExternalViews)
            return false;
        m_traceBuffer.clear();
        break;

    case 6:                                    // CVMTrace
    {
        if (!t->getCVMTrace())
            return false;
        // seed THE trace twenty units ahead, along the parent's forward axis.
        const Vec3& here = getAbsolutePosition();
        const Mat4& parentTrans = getParent()->getAbsoluteTransformation();     // slot +0x84
        m_traceAhead.x = here.x + parentTrans.m[8]  * 20.0f;
        m_traceAhead.y = here.y + parentTrans.m[9]  * 20.0f;
        m_traceAhead.z = here.z + parentTrans.m[10] * 20.0f;
        const Vec3& again = getAbsolutePosition();      // asked a second time, as compiled
        m_traceOrigin = again;
        m_traceBuffer.clear();
        break;
    }

    case 7:                                    // CVMExternTrace
    {
        if (!t->getCVMExternTrace())
            return false;
        // Reject an external camera whose translation is within epsilon of zero on every axis.
        const float tx = m_externCameraTrans.m[12];
        const float ty = m_externCameraTrans.m[13];
        const float tz = m_externCameraTrans.m[14];
        if (fabsf(tx) <= 1.1920929e-7f && fabsf(ty) <= 1.1920929e-7f && fabsf(tz) <= 1.1920929e-7f)
            return false;
        // the same twenty-units-ahead seed, but from the extern transform's own rows.
        m_traceAhead.x = tx + m_externCameraTrans.m[8]  * 20.0f;
        m_traceAhead.y = ty + m_externCameraTrans.m[9]  * 20.0f;
        m_traceAhead.z = tz + m_externCameraTrans.m[10] * 20.0f;
        break;
    }

    default:
        return false;                          // an unknown mode is refused, not clamped
    }

    // the common tail
    changeSubGeometry(static_cast<unsigned int>(m), -1);

    const int previous = m_viewMode;
    m_viewMode = m;
    m_previousViewMode = previous;                              // +0x24C

    // whose FOV IS this? Walk parents for a PCO; if there is none, fall back to the root being a
    // projectile and ask it. If neither exists dice logs and carries on - it does not give up.
    IPlayerControlObject* pco = 0;
    for (IObject* node = this; node; node = node->getParent())
    {
        pco = static_cast<IPlayerControlObject*>(node->queryInterface(IID_IPlayerControlObject));
        if (pco)
            break;
    }

    IPlayer* player = 0;
    if (pco)
    {
        player = pco->getOccupingPlayer();
    }
    else
    {
        IObject* root = getRootParent(this);
        void* projectile = root->queryInterface(IID_IProjectileObject);
        if (projectile)
            player = projectileOwner(projectile);               // slot +0x54
        else
            // The site that attests this file's line 1171.
            Debug(DEBUGTYPE_6, "Game/Objects/Base/Camera.cpp", 1171, __FILE__)
                << "tl: Wanted to update fov for camera " << this
                << " but no pco, or root projectile, found";
    }

    // A view-mode change resets THE field OF view TO zero, instantly (time 0).
    if (player)
        player->setFov(0.0f, 0.0f);

    // Notify only when the local human occupies this controlled object.
    IObject* pcoObject = pco ? static_cast<IObject*>(pco->queryInterface(IID_IObject)) : 0;
    IPlayer* local = playerManager->getLocalHumanPlayer();
    if (pcoObject && local
        && (pcoObject == local->getVehicle() || pcoObject == local->getDefaultVehicle()))
    {
        pcoObject->handleMessage(static_cast<TemplateMessage>(28), local, false);   // slot +0x104
        eventManager->postEvent(static_cast<EventCategory>(2), 31, this, 0.0f);
    }
    return true;
}

// Uses interior FOV only for inside view; other modes use world FOV.
// Transformation dispatch copies the selected mode's result into the camera's stored matrix and returns that storage.
const Mat4& Camera::getTransformation(float deltaTime)
{
#if !DEDICATED && defined(_MSC_VER)
    vehgeom::observeLocalVehicle(this);                       // [TEMP] vehicle-geometry-trace
#endif
    const Mat4* transform;
    switch (m_viewMode)                                       // +0x1B8
    {
    case 1:  transform = &getTransformationCVMInside(deltaTime);       break;
    case 2:  transform = &getTransformationCVMNose(deltaTime);         break;
    case 3:  transform = &getTransformationCVMChase(deltaTime, false); break;
    case 4:  transform = &getTransformationCVMChase(deltaTime, true);  break;
    case 5:  transform = &getTransformationCVMFlyBy(deltaTime);        break;
    case 6:  transform = &getTransformationCVMTrace(deltaTime);        break;
    default: transform = &getAbsoluteTransformation();                 break;   // +0x84
    }

    m_defaultTransformation = *transform;
    return m_defaultTransformation;
}

// / BF2.exe 0x5A2920, 306 B. The dedicated image ends after the
// absolute-transform copy; the client retains a runtime isDedicated() gate around animation,
// roll and surface constraints. The immediate parent is queried twice, as in 0x5A2955..0x5A2970.
// Camera constructors ELF 0x81DA960 / client 0x5A4F90 zero all four consumed roll components.
// Their update producers remain separate Camera::handleUpdate recovery work.
const Mat4& Camera::getTransformationCVMInside(float)
{
    m_defaultTransformation = getAbsoluteTransformation();   // virtual +0x84
    if (!isDedicated())
    {
        IObject* const parent = getParent();
        if (parent && parent->queryInterface(IID_IPlayerControlObject))
        {
            IPlayerControlObject* const pco = static_cast<IPlayerControlObject*>(
                parent->queryInterface(IID_IPlayerControlObject));
            Mat4 animated = Mat4::getIdentity();
            if (pco->applyCameraAnimation(m_defaultTransformation, animated))
                m_defaultTransformation = animated;
        }

        // Client helper 0x5A2800 rotates about the camera's current forward axis and updates
        // only its rotation. The temporary copy is required: Mat4::rotMult does not alias-check.
        const float roll = m_explosionShakeRoll + m_soldierShakeRoll
                         + m_cameraRecoilY + m_speedShakeRoll;
        if (fabsf(roll) > 1.1920929e-7f)
        {
            Mat4 rotation = Mat4::getIdentity();
            const Vec3 forward = { m_defaultTransformation.m[8],
                                   m_defaultTransformation.m[9],
                                   m_defaultTransformation.m[10] };
            setRotateAboutLine(rotation, roll, forward);
            const Mat4 previous = m_defaultTransformation;
            m_defaultTransformation.rotMult(previous, rotation);
        }

        // BF2.exe +0x6C and +0x98 are the (float,float) terrain and water overloads.
        // Use typed calls: MSVC reverses overload order within these virtual overload groups.
        const float ground = cameraHeightmapCluster()->getHeightInWorldCoords(
            m_defaultTransformation.m[12], m_defaultTransformation.m[14])
            + s_insideSurfaceClearance;
        if (ground > m_defaultTransformation.m[13])
            m_defaultTransformation.m[13] = ground;
        const float water = cameraHeightmapCluster()->getWaterLevelInWorldCoords(
            m_defaultTransformation.m[12], m_defaultTransformation.m[14]);
        if (fabsf(m_defaultTransformation.m[13] - water) < 0.025f)
            m_defaultTransformation.m[13] = water + s_insideSurfaceClearance;
    }
    return m_defaultTransformation;
}

// , 224 B.  The authored nose offset is rotated by
// the immediate parent's basis (translation deliberately excluded), then added to this camera's
// absolute transformation.  Both ports construct and copy an unused identity local; that shipped
// quirk is retained before the meaningful work.
const Mat4& Camera::getTransformationCVMNose(float)
{
    Mat4 unusedIdentity = Mat4::getIdentity();                 // shipped, result intentionally unused
    (void)unusedIdentity;
    const CameraTemplate* cameraTemplate = static_cast<const CameraTemplate*>(getTemplate());
    Vec3 nosePosition = cameraTemplate->getNosePos();

    IObject* parent = getParent();
    if (parent)
        nosePosition = cameraTransformDirection(parent->getAbsoluteTransformation(), nosePosition);

    m_defaultTransformation = getAbsoluteTransformation();
    m_defaultTransformation.m[12] += nosePosition.x;
    m_defaultTransformation.m[13] += nosePosition.y;
    m_defaultTransformation.m[14] += nosePosition.z;
    return m_defaultTransformation;
}

// , 1,028 B (the PE database's imported symbol is shifted
// and incorrectly labels this body CVMNose).  The persistent four-float block is a Quat, not three
// independent ints plus a spare float: both images pass all four values through Quat::slerp.
const Mat4& Camera::getTransformationCVMChase(float deltaTime, bool frontChase)
{
    const CameraTemplate* cameraTemplate = static_cast<const CameraTemplate*>(getTemplate());
    IObject* root = getRootParent(this);                         // shipped: unguarded
    const Mat4& rootTransformation = root->getAbsoluteTransformation();

    Vec3 target = {
        rootTransformation.m[12], rootTransformation.m[13], rootTransformation.m[14]
    };
    const Vec3 chaseOffset = cameraTemplate->getChaseOffset();
    if (sqrtf(cameraLengthSquared(chaseOffset)) != 0.0f)
        target = cameraTransformPoint(rootTransformation, chaseOffset);

    IObject* parent = getParent();
    bool soldierChild = parent
        && parent->getTemplate()->getClassID() == CID_SoldierTemplate;
    if (root->getTemplate()->getClassID() == CID_SoldierTemplate)
        soldierChild = false;

    float desiredDistance = cameraTemplate->getChaseDistance();
    if (desiredDistance == -1.0f || soldierChild)
    {
        float radius = root->getPhysicBoundingRadius();
        if (radius <= 2.0f)
            radius = 2.0f;
#if defined(_MSC_VER)
        else
            // [PLATFORM] PE 0x5F0A36/0x5F0A56 repeats this virtual call on the >2 or unordered
            // arm. ELF 0x81DCAD0 caches its sole call, so the non-Win32 source path does too.
            // Every Windows image (x86 and the native x64, issue #7) takes the PE arm.
            radius = root->getPhysicBoundingRadius();
#endif
        desiredDistance = radius * s_chaseDistanceScale;
    }

    const float chaseAngle = soldierChild ? 0.30000001f : cameraTemplate->getChaseAngle();
    const float directionSign = frontChase ? -1.0f : 1.0f;
    Vec3 desiredDirection = {
        -rootTransformation.m[8]  * directionSign + rootTransformation.m[4] * chaseAngle,
        -rootTransformation.m[9]  * directionSign + rootTransformation.m[5] * chaseAngle,
        -rootTransformation.m[10] * directionSign + rootTransformation.m[6] * chaseAngle
    };
    cameraNormalize(desiredDirection);

    const Quat desiredRotation = {
        desiredDirection.x, desiredDirection.y, desiredDirection.z, 1.0f
    };
    float blend = deltaTime * cameraTemplate->getFollowStiffness();
#if defined(_MSC_VER)
    // [PLATFORM] PE's clamp helper returns zero for unordered/NaN; the ELF's inlined comparisons
    // leave NaN untouched. Every Windows image (x86 and the native x64, issue #7) takes the PE arm.
    if (!(blend >= 0.0f))
        blend = 0.0f;
#else
    if (blend < 0.0f)
        blend = 0.0f;
#endif
    if (blend > 1.0f)
        blend = 1.0f;

    const Vec3 previousDirection = {
        m_chaseRotation.x, m_chaseRotation.y, m_chaseRotation.z
    };
    if (!(cameraLengthSquared(previousDirection) <= 0.0f)) // positive or unordered, as shipped
    {
        Quat interpolated = {0.0f, 0.0f, 0.0f, 1.0f};
        interpolated.slerp(m_chaseRotation, desiredRotation, blend);
        Vec3 direction = {interpolated.x, interpolated.y, interpolated.z};
        cameraNormalize(direction);
        interpolated.x = direction.x;
        interpolated.y = direction.y;
        interpolated.z = direction.z;
        m_chaseRotation = interpolated;
    }
    else
    {
        m_chaseRotation = desiredRotation;
    }

    if (!(m_chaseDistance <= 0.0f))                 // positive or unordered, as shipped
    {
        IPhysicsNode* physicsNode = static_cast<IPhysicsNode*>(
            object_detail::LayoutAccess::physicsNode(root));
        if (physicsNode && s_chaseAccelerationScale != 0.0f)
        {
            const Vec3& acceleration = physicsNode->getPositionalAcceleration();
            desiredDistance -= (acceleration.x * rootTransformation.m[8]
                              + acceleration.y * rootTransformation.m[9]
                              + acceleration.z * rootTransformation.m[10])
                             * s_chaseAccelerationScale;
        }
        m_chaseDistance = m_chaseDistance * (1.0f - blend) + desiredDistance * blend;
    }
    else
    {
        m_chaseDistance = desiredDistance;
    }

    Vec3 eye = {
        target.x + m_chaseRotation.x * m_chaseDistance,
        target.y + m_chaseRotation.y * m_chaseDistance,
        target.z + m_chaseRotation.z * m_chaseDistance
    };
    IHeightmapCluster* heightmap = cameraHeightmapCluster();  // shipped: unguarded singleton
    float minimumHeight = heightmap->getHeightInWorldCoords(eye.x, eye.z) + 1.0f;
    const float waterHeight = heightmap->getWaterLevelInWorldCoords(eye.x, eye.z) + 1.0f;
    if (waterHeight > minimumHeight)
        minimumHeight = waterHeight;
    if (minimumHeight > eye.y)
        eye.y = minimumHeight;

    const Vec3 up = {0.0f, 1.0f, 0.0f};
    calcLookAtMatrix(m_defaultTransformation, eye, target, up);
    return m_defaultTransformation;
}

// , 693 B.  The absent-occupant dereference is shipped:
// after the optional PCO lookup both images call getTracePlayerId without guarding the player.
const Mat4& Camera::getTransformationCVMTrace(float)
{
    IPlayerControlObject* pco = 0;
    for (IObject* object = this; object; object = object->getParent())
    {
        pco = static_cast<IPlayerControlObject*>(
            object->queryInterface(IID_IPlayerControlObject));
        if (pco)
            break;
    }

    IPlayer* occupant = pco ? pco->getOccupingPlayer() : 0;
    IPlayer* tracePlayer = playerManager->getPlayerFromId(occupant->getTracePlayerId());
    IObject* traceObject = tracePlayer && tracePlayer->getIsAlive()
        ? tracePlayer->getVehicle() : 0;

    const Vec3& cameraPosition = getAbsolutePosition();
    m_traceAhead.x += cameraPosition.x - m_traceOrigin.x;
    m_traceAhead.y += cameraPosition.y - m_traceOrigin.y;
    m_traceAhead.z += cameraPosition.z - m_traceOrigin.z;

    bool followedTraceObject = false;
    if (traceObject)
    {
        const Vec3& tracePosition = traceObject->getAbsolutePosition();
        const Vec3& here = getAbsolutePosition();
        const Vec3 separation = {
            tracePosition.x - here.x,
            tracePosition.y - here.y,
            tracePosition.z - here.z
        };
        if (sqrtf(cameraLengthSquared(separation)) > 2.0f)
        {
            const Vec3& currentTracePosition = traceObject->getAbsolutePosition();
            m_traceAhead.x += (currentTracePosition.x - m_traceAhead.x) * 0.2f;
            m_traceAhead.y += (currentTracePosition.y - m_traceAhead.y) * 0.2f;
            m_traceAhead.z += (currentTracePosition.z - m_traceAhead.z) * 0.2f;
            followedTraceObject = true;
        }
    }

    if (!followedTraceObject)
    {
        const Vec3& here = getAbsolutePosition();
        Vec3 traceDirection = {
            m_traceAhead.x - here.x,
            m_traceAhead.y - here.y,
            m_traceAhead.z - here.z
        };
        cameraNormalize(traceDirection);

        const Mat4& parentTransformation = getParent()->getAbsoluteTransformation();
        const float alignment = traceDirection.x * parentTransformation.m[8]
                              + traceDirection.y * parentTransformation.m[9]
                              + traceDirection.z * parentTransformation.m[10];
        if (fabsf(alignment) <= 0.89999998f)
        {
            const Vec3& currentPosition = getAbsolutePosition();
            const Mat4& currentParentTransformation = getParent()->getAbsoluteTransformation();
            m_traceAhead.x += (currentPosition.x + currentParentTransformation.m[8] * 20.0f
                             - m_traceAhead.x) * 0.1f;
            m_traceAhead.y += (currentPosition.y + currentParentTransformation.m[9] * 20.0f
                             - m_traceAhead.y) * 0.1f;
            m_traceAhead.z += (currentPosition.z + currentParentTransformation.m[10] * 20.0f
                             - m_traceAhead.z) * 0.1f;
        }
        else
        {
            const Vec3& currentPosition = getAbsolutePosition();
            const Mat4& currentParentTransformation = getParent()->getAbsoluteTransformation();
            m_traceAhead.x = currentPosition.x + currentParentTransformation.m[8] * 20.0f;
            m_traceAhead.y = currentPosition.y + currentParentTransformation.m[9] * 20.0f;
            m_traceAhead.z = currentPosition.z + currentParentTransformation.m[10] * 20.0f;
        }
    }

    const Mat4& parentTransformation = getParent()->getAbsoluteTransformation();
    const Vec3 up = {
        parentTransformation.m[4], parentTransformation.m[5], parentTransformation.m[6]
    };
    calcLookAtMatrix(m_defaultTransformation, getAbsolutePosition(), m_traceAhead, up);
    m_traceOrigin.x = m_defaultTransformation.m[12];
    m_traceOrigin.y = m_defaultTransformation.m[13];
    m_traceOrigin.z = m_defaultTransformation.m[14];
    return m_defaultTransformation;
}

// , 453 B.  The x87 raw streams settle an ambiguity in both
// decompilations: water+1 clamps the newly recorded frame before writeFrame, while the independent
// post-view terrain query supplies terrain+0.5 for the returned transformation.
const Mat4& Camera::getTransformationCVMFlyBy(float)
{
    if (!getParent() || !m_recordingValid)
        return m_defaultTransformation;

    IObject* root = getRootParent(this);                         // shipped: unguarded
    const Mat4& rootTransformation = root->getAbsoluteTransformation();
    const float radius = root->getPhysicBoundingRadius();
    Vec3 motion = {
        rootTransformation.m[8]  * (radius * 1.2f),
        rootTransformation.m[9]  * (radius * 1.2f),
        rootTransformation.m[10] * (radius * 1.2f)
    };

    const Vec3& cameraPosition = getAbsolutePosition();
    IPhysicsNode* physicsNode = static_cast<IPhysicsNode*>(
        object_detail::LayoutAccess::physicsNode(root));
    if (physicsNode)
    {
        const Vec3& speed = physicsNode->getPositionalSpeed();
        motion.x += speed.x * 0.30000001f;
        motion.y += speed.y * 0.30000001f;
        motion.z += speed.z * 0.30000001f;
    }

    const float scale = 0.40000001f * m_float1BC;
    Vec3 nextPosition = {
        cameraPosition.x + motion.x * scale,
        cameraPosition.y + motion.y * scale,
        cameraPosition.z + motion.z * scale
    };

    IHeightmapCluster* heightmap = cameraHeightmapCluster();     // shipped: unguarded singleton
    const float waterHeight = heightmap->getWaterLevelInWorldCoords(
        nextPosition.x, nextPosition.z) + 1.0f;
    if (waterHeight > nextPosition.y)
        nextPosition.y = waterHeight;

    writeFrame(nextPosition, 60.0f * m_float1BC);
    viewFrame();

    // Both images reload the singleton after viewFrame rather than retaining the earlier pointer.
    const float terrainHeight = cameraHeightmapCluster()->getHeightInWorldCoords(
        m_defaultTransformation.m[12], m_defaultTransformation.m[14]) + 0.5f;
    if (terrainHeight > m_defaultTransformation.m[13])
    {
        m_defaultTransformation.m[13] = terrainHeight;
        const Vec3 adjustedPosition = {
            m_defaultTransformation.m[12],
            m_defaultTransformation.m[13],
            m_defaultTransformation.m[14]
        };
        const Vec3 up = {0.0f, 1.0f, 0.0f};
        calcLookAtMatrix(m_defaultTransformation, adjustedPosition, cameraPosition, up);
    }
    return m_defaultTransformation;
}

// , 151 B.  The interval gate deliberately compares against
// the oldest/front timestamp even though insertion is at the back.  The negated comparison also
// preserves the shipped unordered case: a NaN operand inserts a frame.
void Camera::writeFrame(const Vec3& position, float interval)
{
    if (m_traceBuffer.empty() || !(m_frameTime < m_traceBuffer.front().time + interval))
    {
        Frame frame = {position, m_frameTime};
        m_traceBuffer.push_back(frame);
    }
}

// , 420 B.  The empty-buffer diagnostic deliberately does not
// return; the shipped body goes on to dereference begin(), so that unsafe failure mode is retained.
Vec3 Camera::readFrame()
{
    if (m_traceBuffer.empty())
        Debug(DEBUGTYPE_6, "Game/Objects/Base/Camera.cpp", 410, "")
            << "trace buffer size 0.";

    std::list<Frame>::iterator frame = m_traceBuffer.begin();
    while (frame != m_traceBuffer.end())
    {
        std::list<Frame>::iterator next = frame;
        ++next;
        if (next == m_traceBuffer.end() || !(next->time < m_frameTime))
            break;
        m_traceBuffer.erase(frame);
        frame = m_traceBuffer.begin();
    }

    const Frame current = *frame;
    std::list<Frame>::iterator next = frame;
    ++next;
    if (next == m_traceBuffer.end())
        return current.position;

    float blend = (m_frameTime - current.time) / (next->time - current.time);
    if (blend < 0.0f)
        blend = 0.0f;                                         // deliberately no upper clamp
    const float inverseBlend = 1.0f - blend;
    Vec3 result = {
        current.position.x * inverseBlend + next->position.x * blend,
        current.position.y * inverseBlend + next->position.y * blend,
        current.position.z * inverseBlend + next->position.z * blend
    };
    return result;
}

// , 457 B.  Modes 3/4 force the recorded direction to the
// current root radius; all modes build the rotation around the origin and restore translation last.
void Camera::viewFrame()
{
    Vec3 framePosition = readFrame();
    const Vec3 currentPosition = getAbsolutePosition();
    Vec3 direction = {
        framePosition.x - currentPosition.x,
        framePosition.y - currentPosition.y,
        framePosition.z - currentPosition.z
    };

    if (m_viewMode == 3 || m_viewMode == 4)
    {
        const float radius = getRootParent(this)->getPhysicBoundingRadius() * 1.2f;
        const float length = sqrtf(cameraLengthSquared(direction));
        // Both ports compare against the qword/double 0.01 constant and route unordered to the
        // fallback arm; the negated greater-than form preserves both details.
        if (!(length > 0.01))
        {
            const Mat4& transformation = getAbsoluteTransformation();
            direction.x = -transformation.m[8]  * radius;
            direction.y = -transformation.m[9]  * radius;
            direction.z = -transformation.m[10] * radius;
        }
        else
        {
            const float scale = radius / length;
            direction.x *= scale;
            direction.y *= scale;
            direction.z *= scale;
        }
        framePosition.x = currentPosition.x + direction.x;
        framePosition.y = currentPosition.y + direction.y;
        framePosition.z = currentPosition.z + direction.z;
    }

    const Vec3 origin = {0.0f, 0.0f, 0.0f};
    const Vec3 target = {-direction.x, -direction.y, -direction.z};
    const Vec3 up = {0.0f, 1.0f, 0.0f};
    calcLookAtMatrix(m_defaultTransformation, origin, target, up);
    m_defaultTransformation.m[12] = framePosition.x;
    m_defaultTransformation.m[13] = framePosition.y;
    m_defaultTransformation.m[14] = framePosition.z;
}

float Camera::getActiveFOV() const
{
    const CameraTemplate* t = static_cast<const CameraTemplate*>(getTemplate());
    if (m_viewMode == 1)                     // +0x1B8 CVMInside
        return t->getInsideFOV();            // +736
    return t->getWorldFOV();                 // +732
}

// Switches vehicle and weapon geometry for the local player's camera. Empty vehicles use exterior mode.
// Camera cockpit settings override vehicle settings unless -1; soldiers skip weapon geometry changes.
void Camera::changeSubGeometry(unsigned int viewMode, int subGeom)
{
    const CameraTemplate* templ = static_cast<const CameraTemplate*>(getTemplate());

    // gate 1 - the nearest PCO above us
    IPlayerControlObject* pco = 0;
    for (IObject* node = this; node; node = node->getParent())
    {
        pco = static_cast<IPlayerControlObject*>(node->queryInterface(IID_IPlayerControlObject));
        if (pco)
            break;
    }
    if (!pco)
        return;

    // gate 2 - the whole function is client-only
    IPlayer* local = playerManager->getLocalHumanPlayer();
    if (!local)
        return;

    // gate 3 - and it must be OUR player in there
    IPlayer* occupant = pco->getOccupingPlayer();
#if !DEDICATED && defined(_MSC_VER)
    // [TEMP] vehicle-geometry-trace
    const bool traceCsg = vehgeom::enabled();
    IObject* const tracePco = static_cast<IObject*>(pco->queryInterface(IID_IObject));
    if (traceCsg && vehgeom::budget())
    {
        std::fprintf(stderr,
            "[VEHGEOM-CSG] call camera=%s mode(arg)=%u subGeom(arg)=%d cameraMode=%d pco=%s "
            "occupant=%p local=%p camTemplate cockpitSubGeom=%d cockpitLod=%d pco cockpitSubGeom=%d "
            "cockpitLod=%d CVMInside=%d\n",
            vehgeom::templateName(static_cast<IObject*>(this)), viewMode, subGeom, m_viewMode,
            vehgeom::templateName(tracePco),
            static_cast<void*>(occupant), static_cast<void*>(local), templ->getCockpitSubGeom(),
            templ->getCockpitLod(), pco->getCockpitSubGeom(), pco->getCockpitLod(),
            templ->getCVMInside() ? 1 : 0);
        std::fflush(stderr);
    }
    if (traceCsg && occupant && occupant != local && vehgeom::budget())
    {
        std::fprintf(stderr, "[VEHGEOM-CSG] skipped: the occupant is not the local player\n");
        std::fflush(stderr);
    }
#endif
    if (occupant)
    {
        if (occupant != local)
            return;
    }
    else
    {
        subGeom = 3;                     // an empty vehicle is forced to the exterior arm
    }

    IObject* pcoObject = static_cast<IObject*>(pco->queryInterface(IID_IObject));

    // the vehicle's own geometry, or the root parent's if it has none
    void* vehicleGeom = object_detail::LayoutAccess::geometry(pcoObject);          // +0x38
    if (!vehicleGeom)
        vehicleGeom = object_detail::LayoutAccess::geometry(getRootParent(this));

#if !DEDICATED && defined(_MSC_VER)
    if (traceCsg && (!vehicleGeom || geomSubGeometryCount(vehicleGeom) <= 1) && vehgeom::budget())
    {
        std::fprintf(stderr, "[VEHGEOM-CSG] skipped: vehicle geometry %p has %d sub-geometries\n",
                     vehicleGeom, vehicleGeom ? geomSubGeometryCount(vehicleGeom) : -1);
        std::fflush(stderr);
    }
#endif
    // gate 4 - nothing to switch between
    if (!vehicleGeom || geomSubGeometryCount(vehicleGeom) <= 1)
        return;

    std::vector<void*> geometries;
    geometries.push_back(vehicleGeom);

    // A soldier'S weapons ARE skipped - only vehicles extend the list.
    if (pcoObject->getTemplate()->getClassID() != CID_SoldierTemplate)
    {
        std::vector<IObject*>* weapons = static_cast<std::vector<IObject*>*>(pco->getWeapons());
        for (std::vector<IObject*>::iterator w = weapons->begin(); w != weapons->end(); ++w)
        {
            void* g = object_detail::LayoutAccess::geometry(*w);
            if (g && geomSubGeometryCount(g) > 1)
                geometries.push_back(g);
        }
    }

    // apply. every arm processes every geometry - see the loop-rotation note above.
    for (std::vector<void*>::iterator it = geometries.begin(); it != geometries.end(); ++it)
    {
        void* g = *it;
#if !DEDICATED && defined(_MSC_VER)
        // [TEMP] vehicle-geometry-trace: the state before this geometry is switched.
        const int traceGeomBefore = traceCsg ? vehgeom::geomIndex(g) : 0;
        const int traceLodBefore  = traceCsg ? vehgeom::lodIndex(g) : 0;
#endif

        if (subGeom == -1)
        {
            // "use the view mode": anything above the inside view gets the plain exterior.
            if (viewMode > 1)
            {
                geomSetSubGeometry(g, 1);
                geomSetLod(g, -1);
            }
            else
            {
                int sub = templ->getCockpitSubGeom();                 // +804, read first
                if (sub == -1)
                    sub = pco->getCockpitSubGeom();
                geomSetSubGeometry(g, sub);

                int lod = templ->getCockpitLod();                     // +800, read second
                if (lod == -1)
                    lod = pco->getCockpitLod();
                geomSetLod(g, lod);
            }
        }
        else if (subGeom == 2)
        {
            // the cockpit arm - but only if this camera template has an inside view at all.
            if (templ->getCVMInside())
            {
                geomSetSubGeometry(g, pco->getCockpitSubGeom());
                geomSetLod(g, pco->getCockpitLod());
            }
            else
            {
                geomSetSubGeometry(g, 1);
                geomSetLod(g, -1);
            }
        }
        else if (subGeom == 3)
        {
            geomSetSubGeometry(g, 1);
            geomSetLod(g, -1);
        }
        // any other value does nothing at all - not even a default arm.
#if !DEDICATED && defined(_MSC_VER)
        if (traceCsg && vehgeom::budget())
        {
            std::fprintf(stderr,
                "[VEHGEOM-CSG] geometry=%p %s count=%d geom %d->%d lod %d->%d (subGeom used=%d)\n",
                g, g == vehicleGeom ? "vehicle" : "weapon", geomSubGeometryCount(g),
                traceGeomBefore, vehgeom::geomIndex(g), traceLodBefore, vehgeom::lodIndex(g), subGeom);
            std::fflush(stderr);
        }
#endif
    }
}

// Clears chase state and seeds the default transform and trace origin from the current camera.
void Camera::resetChaseCamera()
{
    m_chaseRotation.x = 0.0f;
    m_chaseRotation.y = 0.0f;
    m_chaseRotation.z = 0.0f;
    m_chaseDistance = 0.0f;                      // quaternion w is deliberately retained
    m_defaultTransformation = getAbsoluteTransformation();
    m_traceOrigin.x = m_defaultTransformation.m[12];
    m_traceOrigin.y = m_defaultTransformation.m[13];
    m_traceOrigin.z = m_defaultTransformation.m[14];
}

// Requests inside view, cycling to an allowed mode if refused, then resets external and chase transforms.
// Caching a camera forces this reset before parking it through the base.
void Camera::disableCachedObject()
{
    resetCameraView(true);
    RotationalBundle::disableCachedObject();
}

void Camera::resetCameraView(bool force)
{
    m_traceBuffer.clear();

    if (!setViewMode(static_cast<CameraViewMode>(1), 0, force))
        toggleViewMode(this, true);              // the fallback cycle

    m_previousViewMode = 1;                      // +0x24C
    m_dword200 = 0;
    m_float208 = -1.0f;
    m_dword204 = 0;
    m_externCameraTrans = Mat4::getIdentity();

    resetChaseCamera();                          // a tail call in the ELF
}

// , 142 B.  Unlike resetCameraView, this retries the camera's
// current authored mode and does not write m_previousViewMode.  ToggleCamera reaches this virtual
// directly after changing the view-mode selection.
void Camera::updateCameraView(bool force)
{
    m_traceBuffer.clear();

    if (!setViewMode(static_cast<CameraViewMode>(m_viewMode), 0, force))
        toggleViewMode(this, true);

    m_dword200 = 0;
    m_float208 = -1.0f;
    m_dword204 = 0;
    m_externCameraTrans = Mat4::getIdentity();
    resetChaseCamera();
}


// Handles mouse-look when enabled by the template; released input decays rotation toward zero.
// Trace view ignores input. Other cameras delegate directly to RotationalBundle.
void Camera::handlePlayerInput(IPlayer* player, const io::PlayerInput& input,
                               float dt, unsigned int flags)
{
    if (m_viewMode == 6)                                     // CVMTrace takes no input
        return;

    const CameraTemplate* t = static_cast<const CameraTemplate*>(getTemplate());
    if (!t->getToggleMouseLook())                            // +564
    {
        RotationalBundle::handlePlayerInput(player, input, dt, flags);
        return;
    }

    // only bound inputs are read - the same rule the soldier's block follows
    bool lookHeld = false;
    if ((input.mappingFlags >> io::PIMouseLook) & 1ull)
        lookHeld = (input.inputs[io::PIMouseLook] > 0.5f);

    if (lookHeld)
    {
        RotationalBundle::handlePlayerInput(player, input, dt, flags);
        return;
    }

    // released: spring the rotation back
    m_currentRotationalSpeed.x = 0.0f;
    m_currentRotationalSpeed.y = 0.0f;
    m_currentRotationalSpeed.z = 0.0f;
    m_rotationInput.x = 0.0f;                                // +0x158
    m_rotationInput.y = 0.0f;
    m_rotationInput.z = 0.0f;

    const float oldX = m_currentRotation.x;
    const float oldY = m_currentRotation.y;
    const float oldZ = m_currentRotation.z;

    m_currentRotation.x = oldX * 0.75f;                      // 25% off, every tick
    m_currentRotation.y = oldY * 0.75f;
    m_currentRotation.z = oldZ * 0.75f;

    const bool settled = fabsf(m_currentRotation.x - oldX) <= 1.1920929e-7f
                      && fabsf(m_currentRotation.y - oldY) <= 1.1920929e-7f
                      && fabsf(m_currentRotation.z - oldZ) <= 1.1920929e-7f;

    if (!settled)
        setState();                                          // only while it is still moving
}

// Updates trace recording and chase state on entry, and restores exterior geometry on local-player exit.
// Forwards every message to RotationalBundle after handling camera state.
void Camera::handleMessage(TemplateMessage message, IPlayer* player, bool flag)
{
    const int m = static_cast<int>(message);
    if (m == 2)
    {
        m_dword250 = 8;                                          // before the player test, as shipped
        if (player)
        {
            if (!player->getIsAIPlayer())                        // ELF +0x64 / BF2.exe +0x60
            {
                setRecordingValid(true);
                resetChaseCamera();
            }
            if (player == playerManager->getLocalHumanPlayer())  // +0x30
                changeSubGeometry(0, 2);
        }
    }
    else if (m == 3)
    {
        setRecordingValid(false);                                // before the player test, as shipped
        if (player && player == playerManager->getLocalHumanPlayer())
            changeSubGeometry(0, 3);
    }
    RotationalBundle::handleMessage(message, player, flag);
}

// Camera::setRecordingValid - ELF 0x81DC030, 32 B: the store, then - only for true - the frame list
// emptied (a tail jump to std::_List_base<Camera::Frame>::__clear). BF2.exe inlines both uses:
// stores 0 alone; 0x5A4821..0x5A482E stores 1 and calls std::list::clear (0x460320).
void Camera::setRecordingValid(bool valid)
{
    m_recordingValid = valid;                                    // ELF +0x1D8 / BF2.exe +0x1F0
    if (valid)
        m_traceBuffer.clear();                                   // ELF +0x1DC / BF2.exe +0x1F4
}

} // namespace world
} // namespace hfe
} // namespace dice
