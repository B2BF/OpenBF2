// Camera template defaults and object creation.
#include "CameraTemplate.h"
#include "Base/Camera.h"
#include "../../Core/Ids.h"

namespace dice {
namespace hfe {
namespace world {

// Initializes an uncached parent-control id and an unnumbered camera id.
// Inside, chase, fly-by and front-chase start enabled; trace, external trace and nose views start disabled.
CameraTemplate::CameraTemplate()
    : m_pcoId(-1)                    // +532  the uncached sentinel
    , m_hasTarget(false)             // +536
    , m_cameraId(0)                  // +540  "unnumbered" - findCameras assigns a negative id
    , m_CVMInside(true)              // +544
    , m_CVMChase(true)               // +545
    , m_CVMFlyBy(true)               // +546
    , m_CVMTrace(false)              // +547
    , m_CVMExternTrace(false)        // +548
    , m_CVMFrontChase(true)          // +549
    , m_CVMNose(false)               // +550
    , m_nosePos{0.0f, 0.0f, 0.0f}   // +552
    , m_toggleMouseLook(false)       // +564
    , m_followStiffness(1.0f)        // +568
    , m_chaseDistance(-1.0f)         // +572
    , m_chaseAngle(0.30000001f)      // +576
    , m_chaseOffset{0.0f, 0.0f, 0.0f} // +580
    // ELF C1/C2 0x81DE2F0 / 0x81DE0C0 write 1.2 literally at +0x2DC/+0x2E0; PE ctor
    // BF2.exe 0x5A3210 writes the same values at +0x38C/+0x390 (0x5A33B7/0x5A33C1). These defaults
    // matter even before content overrides them; stock SoldierCamera authors both as 1.1.
    // ELF 0x81DE1CF..0x81DE20D / BF2.exe 0x5A3395..0x5A33B1: shake and bounce defaults. Camera's
    // updateCameraShake reads the two speeds every frame; content sets them per camera (e.g. the
    // FAAV pilot camera's 20 / 30). The bounce strength and damping are left unset, as shipped.
    , m_cameraShakeSensitivity(0.1f)     // +704
    , m_cameraShakeStartSpeed(0.0f)      // +708
    , m_cameraShakeMaxSpeed(0.0f)        // +712
    , m_bounceLength(0.0f)               // +716
    , m_bounceSensitivity(1.0f)          // +720
    , m_worldFOV(1.2f)               // +732
    , m_insideFOV(1.2f)              // +736
    // Cockpits permit dynamic lights by default, but not static sunlight.
    , m_allowInsideDynamicLights(true)   // +740
    , m_allowInsideStaticSun(false)      // +741
    // ELF 0x81DE1EB..0x81DE277 / BF2.exe 0x5A33D7..0x5A343E: the inside lighting defaults.
    , m_insideStaticSunDirection{-0.25f, -0.5f, 0.75f}   // +744
    , m_insideStaticSunColor{1.0f, 1.0f, 1.0f}           // +756
    , m_insideStaticSunDamageColor{1.0f, 0.0f, 0.0f}     // +768
    , m_insideDamageFrequency(3.0f)                      // +780
    , m_insideDamageThreshold(0.15f)                     // +784
    , m_insideStaticSunAmbientColor{0.7f, 0.7f, 0.7f}    // +788
    // ELF C1 0x81DE4B2/0x81DE4BD stores -1 at +0x320/+0x324; BF2.exe ctor 0x5A3210
    // stores -1 at +0x3D0/+0x3D4 (0x5A3444/0x5A344E). Camera::changeSubGeometry uses -1
    // to select the PCO's authored cockpit values. Omitting these defaults can pass uninitialized
    // indices to SkinnedMesh::setGeomIndex, whose shipped implementation does not clamp them.
    , m_cockpitLod(-1)
    , m_cockpitSubGeom(-1)
{
}

// . Vtable +0x10.
unsigned int CameraTemplate::getClassID() const
{
    return CID_CameraTemplate;
}

// . Vtable +0x50. A tail JMP to the base, like ItemTemplate's.
void* CameraTemplate::queryImplementation(unsigned int cid)
{
    if (cid == CID_CameraTemplate)
        return this;
    return RotationalBundleTemplate::queryImplementation(cid);
}

// Creates a camera using this template.
IObject* CameraTemplate::createObject() const
{
    return new Camera(this);
}

} // namespace world
} // namespace hfe
} // namespace dice
