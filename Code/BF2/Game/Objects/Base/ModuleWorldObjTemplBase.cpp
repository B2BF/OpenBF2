// ObjectTemplate console bindings for base template families. Module registration lives in Core/ModuleRegistry.cpp.
#include "../../../Core/ConsoleObjectMacro.h"
#include "../../../Core/Types.h"                         // U32 console metadata
#include "../../../Scene/Object/ObjectTemplateUtil.h"     // world::getActiveTemplate
#include "SimpleObjectTemplate.h"
#include "BundleTemplate.h"                               // addTemplate
#include "../RotationalBundleTemplate.h"                  // the RB console block
#include "../CameraTemplate.h"                            // worldFOV / insideFOV
#include "../OverheadCameraTemplate.h"                    // commander-camera template fields
#include "../WindAffectorTemplate.h"                      // undotted wind* component block
#include "../CompTemplateInterfaces.h"                    // IIkTemplateComponent
#include "../TemplateLeaves.h"                            // EntryPointTemplate - entryRadius
#include "../Visual/LightSourceTemplate.h"                // the LightSource block
#include "../../../IO/Input/Input.h"                      // io::PlayerInputMap + its stream pair
#include "../../../Physics/PhysicsType.h"                 // symbolic Point/Mesh vocabulary
#include "../../../Core/Ids.h"                            // CID_SimpleObjectTemplate
#include <string>

namespace dice {
namespace hfe {
namespace world {

// [CHOSEN, not recovered] Console metadata aliases. The generated executors here move three dwords
// for these colours, so the three-float alias reproduces the reported metadata shape; the Math type
// of the same name holds four floats (Math/Vec3.h). The material flags are the metadata's "uint".
typedef Math::Vec3 Vec3ColorFloatIntensity;
typedef unsigned int uint;

// [CHOSEN, not recovered] Retail's console metadata spelling. The generated executor stores the
// parsed value as one unsigned dword and all 201 stock uses are numeric; keeping the alias local
// preserves that exact content path without inventing a new public engine type or an unread
// symbolic-token vocabulary.
typedef unsigned int PhysicsGripFlags;

// Zero-layout friend bridge for direct-field/template-cursor executors. Keeping this access here
// avoids inventing public gameplay setters/getters or exposing BundleTemplate's shipped static
// member cursor.
struct BaseTemplateConsoleAccess
{
    static SimpleObjectTemplate* simpleObject()
    {
        return static_cast<SimpleObjectTemplate*>(getActiveTemplate(CID_SimpleObjectTemplate));
    }
    static float getDrag() { return simpleObject()->m_drag; }
    static void setDrag(float value) { simpleObject()->m_drag = value; }
    static float getMass() { return simpleObject()->m_mass; }
    static void setMass(float value) { simpleObject()->m_mass = value; }
    static float getGravityModifier() { return simpleObject()->m_gravityModifier; }
    static void setGravityModifier(float value) { simpleObject()->m_gravityModifier = value; }
    // The four Vec3 members of the same physics block - see the bindings below.
    static const Math::Vec3& getDragOffset() { return simpleObject()->m_dragOffset; }
    static void setDragOffset(const Math::Vec3& value) { simpleObject()->m_dragOffset = value; }
    static const Math::Vec3& getDragModifier() { return simpleObject()->m_dragModifier; }
    static void setDragModifier(const Math::Vec3& value) { simpleObject()->m_dragModifier = value; }
    static const Math::Vec3& getCenterOfMassOffset() { return simpleObject()->m_centerOfMassOffset; }
    static void setCenterOfMassOffset(const Math::Vec3& value)
    {
        simpleObject()->m_centerOfMassOffset = value;
    }
    static const Math::Vec3& getInertiaModifier() { return simpleObject()->m_inertiaModifier; }
    static void setInertiaModifier(const Math::Vec3& value) { simpleObject()->m_inertiaModifier = value; }
    static PhysicsGripFlags getGrip() { return simpleObject()->m_grip; }
    static void setGrip(PhysicsGripFlags value) { simpleObject()->m_grip = value; }
    static U32 getCollisionGroups() { return simpleObject()->m_collisionGroups; }
    static void setCollisionGroups(U32 value) { simpleObject()->m_collisionGroups = value; }

    // Typed access to console-bound collision and physics fields.
    static const Math::Vec3& getCenterOfCollisionOffset()          // +108
        { return simpleObject()->m_centerOfCollisionOffset; }
    static void setCenterOfCollisionOffset(const Math::Vec3& value)
        { simpleObject()->m_centerOfCollisionOffset = value; }
    static float getFloaterMod() { return simpleObject()->m_floaterMod; }                 // +136
    static void setFloaterMod(float value) { simpleObject()->m_floaterMod = value; }
    static int getOtherCollisionLod() { return simpleObject()->m_otherCollisionLod; }     // +168
    static void setOtherCollisionLod(int value) { simpleObject()->m_otherCollisionLod = value; }
    static int getPhysicsFrequency() { return simpleObject()->m_physicsFrequency; }       // +188
    static void setPhysicsFrequency(int value) { simpleObject()->m_physicsFrequency = value; }
    static float getHandleCollisionSpeed() { return simpleObject()->m_handleCollisionSpeed; }  // +320
    static void setHandleCollisionSpeed(float value) { simpleObject()->m_handleCollisionSpeed = value; }
    static bool getIsPortalPassing() { return simpleObject()->m_isPortalPassing != 0; }   // +200
    static void setIsPortalPassing(bool value)
        { simpleObject()->m_isPortalPassing = static_cast<unsigned char>(value ? 1 : 0); }
    static const Math::Vec3& getPortalPassingPosition()                                   // +204
        { return simpleObject()->m_portalPassingPosition; }
    static void setPortalPassingPosition(const Math::Vec3& value)
        { simpleObject()->m_portalPassingPosition = value; }
    // ConsoleClass991 `aiTemplate`: the executor copy-constructs its argument and
    // `assign`s it straight into +296 (ELF +0x128 / PE +0x18C); the get arm copies the field back.
    static const std::string& getAiTemplate() { return simpleObject()->m_aiTemplate; }   // +296
    static void setAiTemplate(const std::string& value) { simpleObject()->m_aiTemplate = value; }

    // Console setters update individual physics bits. Physics-type changes recompute the selected type; sleeping does not.
    static bool getIsSleeping() { return (simpleObject()->m_physicsFlags & 0x01) != 0; }
    static void setIsSleeping(bool value)
    {
        SimpleObjectTemplate* t = simpleObject();
        t->m_physicsFlags = static_cast<unsigned char>((t->m_physicsFlags & 0xFE) | (value ? 0x01 : 0));
    }
    static void setPhysicsBit(unsigned char mask, bool value)
    {
        SimpleObjectTemplate* t = simpleObject();
        t->m_physicsFlags = static_cast<unsigned char>((t->m_physicsFlags & ~mask) | (value ? mask : 0));
        t->checkPhysicsTypes();
    }

    static CameraTemplate* camera()
    {
        return static_cast<CameraTemplate*>(getActiveTemplate(CID_CameraTemplate));
    }
    static int getCameraId() { return camera()->m_cameraId; }
    static void setCameraId(int value) { camera()->m_cameraId = value; }

    // Camera console bindings access these fields directly.
    static bool getCamCVMInside() { return camera()->m_CVMInside; }                // +544
    static void setCamCVMInside(bool value) { camera()->m_CVMInside = value; }
    static bool getCamCVMChase() { return camera()->m_CVMChase; }                  // +545
    static void setCamCVMChase(bool value) { camera()->m_CVMChase = value; }
    static bool getCamCVMFlyBy() { return camera()->m_CVMFlyBy; }                  // +546
    static void setCamCVMFlyBy(bool value) { camera()->m_CVMFlyBy = value; }
    static bool getCamCVMTrace() { return camera()->m_CVMTrace; }                  // +547
    static void setCamCVMTrace(bool value) { camera()->m_CVMTrace = value; }
    static bool getCamCVMExternTrace() { return camera()->m_CVMExternTrace; }      // +548
    static void setCamCVMExternTrace(bool value) { camera()->m_CVMExternTrace = value; }
    static bool getCamCVMFrontChase() { return camera()->m_CVMFrontChase; }        // +549
    static void setCamCVMFrontChase(bool value) { camera()->m_CVMFrontChase = value; }
    static bool getCamCVMNose() { return camera()->m_CVMNose; }                    // +550
    static void setCamCVMNose(bool value) { camera()->m_CVMNose = value; }
    static const Math::Vec3& getCamNosePos() { return camera()->m_nosePos; }       // +552
    static void setCamNosePos(const Math::Vec3& value) { camera()->m_nosePos = value; }
    static float getCamChaseDistance() { return camera()->m_chaseDistance; }       // +572
    static void setCamChaseDistance(float value) { camera()->m_chaseDistance = value; }
    static float getCamChaseAngle() { return camera()->m_chaseAngle; }             // +576
    static void setCamChaseAngle(float value) { camera()->m_chaseAngle = value; }
    static const Math::Vec3& getCamChaseOffset() { return camera()->m_chaseOffset; } // +580
    static void setCamChaseOffset(const Math::Vec3& value) { camera()->m_chaseOffset = value; }
    static float getCamCameraSwayRodLength() { return camera()->m_cameraSwayRodLength; } // +592
    static void setCamCameraSwayRodLength(float value) { camera()->m_cameraSwayRodLength = value; }
    static float getCamCameraSwaySensitivity() { return camera()->m_cameraSwaySensitivity; } // +596
    static void setCamCameraSwaySensitivity(float value) { camera()->m_cameraSwaySensitivity = value; }
    static float getCamCameraSwayDofMax() { return camera()->m_cameraSwayDofMax; } // +600
    static void setCamCameraSwayDofMax(float value) { camera()->m_cameraSwayDofMax = value; }
    static float getCamCameraSwayDofMin() { return camera()->m_cameraSwayDofMin; } // +604
    static void setCamCameraSwayDofMin(float value) { camera()->m_cameraSwayDofMin = value; }
    static float getCamCameraSwayRightMax() { return camera()->m_cameraSwayRightMax; } // +608
    static void setCamCameraSwayRightMax(float value) { camera()->m_cameraSwayRightMax = value; }
    static float getCamCameraSwayRightMin() { return camera()->m_cameraSwayRightMin; } // +612
    static void setCamCameraSwayRightMin(float value) { camera()->m_cameraSwayRightMin = value; }
    static float getCamCameraSwaySpringStrength() { return camera()->m_cameraSwaySpringStrength; } // +616
    static void setCamCameraSwaySpringStrength(float value) { camera()->m_cameraSwaySpringStrength = value; }
    static float getCamCameraSwaySpringDamping() { return camera()->m_cameraSwaySpringDamping; } // +620
    static void setCamCameraSwaySpringDamping(float value) { camera()->m_cameraSwaySpringDamping = value; }
    static float getCamCameraShakeSensitivity() { return camera()->m_cameraShakeSensitivity; } // +704
    static void setCamCameraShakeSensitivity(float value) { camera()->m_cameraShakeSensitivity = value; }
    static float getCamCameraShakeStartSpeed() { return camera()->m_cameraShakeStartSpeed; } // +708
    static void setCamCameraShakeStartSpeed(float value) { camera()->m_cameraShakeStartSpeed = value; }
    static float getCamCameraShakeMaxSpeed() { return camera()->m_cameraShakeMaxSpeed; } // +712
    static void setCamCameraShakeMaxSpeed(float value) { camera()->m_cameraShakeMaxSpeed = value; }
    static float getCamBounceLength() { return camera()->m_bounceLength; }         // +716
    static void setCamBounceLength(float value) { camera()->m_bounceLength = value; }
    static float getCamBounceSensitivity() { return camera()->m_bounceSensitivity; } // +720
    static void setCamBounceSensitivity(float value) { camera()->m_bounceSensitivity = value; }
    static float getCamBounceStrength() { return camera()->m_bounceStrength; }     // +724
    static void setCamBounceStrength(float value) { camera()->m_bounceStrength = value; }
    static float getCamBounceDamping() { return camera()->m_bounceDamping; }       // +728
    static void setCamBounceDamping(float value) { camera()->m_bounceDamping = value; }
    static bool getCamAllowInsideDynamicLights() { return camera()->m_allowInsideDynamicLights; } // +740
    static void setCamAllowInsideDynamicLights(bool value) { camera()->m_allowInsideDynamicLights = value; }
    static bool getCamAllowInsideStaticSun() { return camera()->m_allowInsideStaticSun; } // +741
    static void setCamAllowInsideStaticSun(bool value) { camera()->m_allowInsideStaticSun = value; }
    static const Math::Vec3& getCamInsideStaticSunDirection() { return camera()->m_insideStaticSunDirection; } // +744
    static void setCamInsideStaticSunDirection(const Math::Vec3& value) { camera()->m_insideStaticSunDirection = value; }
    static float getCamInsideDamageFrequency() { return camera()->m_insideDamageFrequency; } // +780
    static void setCamInsideDamageFrequency(float value) { camera()->m_insideDamageFrequency = value; }
    static float getCamInsideDamageThreshold() { return camera()->m_insideDamageThreshold; } // +784
    static void setCamInsideDamageThreshold(float value) { camera()->m_insideDamageThreshold = value; }
    // The three inside-cockpit colours. Bound as Vec3 on the evidence recorded in CameraTemplate.h:
    // their executors move three dwords and all 131 shipped statements carry three tokens, so
    // retail's `Vec3ColorFloatIntensity` metadata string is the mis-attribution that header argues.
    static const Math::Vec3& getCamInsideStaticSunColor()                         // +756
        { return camera()->m_insideStaticSunColor; }
    static void setCamInsideStaticSunColor(const Math::Vec3& value)
        { camera()->m_insideStaticSunColor = value; }
    static const Math::Vec3& getCamInsideStaticSunDamageColor()                   // +768
        { return camera()->m_insideStaticSunDamageColor; }
    static void setCamInsideStaticSunDamageColor(const Math::Vec3& value)
        { camera()->m_insideStaticSunDamageColor = value; }
    static const Math::Vec3& getCamInsideStaticSunAmbientColor()                  // +788
        { return camera()->m_insideStaticSunAmbientColor; }
    static void setCamInsideStaticSunAmbientColor(const Math::Vec3& value)
        { camera()->m_insideStaticSunAmbientColor = value; }
    // ConsoleClass315 / 316 (see the binding block): raw int stores, -1 = defer to the PCO.
    static int getCamCockpitLod() { return camera()->m_cockpitLod; }               // +800
    static void setCamCockpitLod(int value) { camera()->m_cockpitLod = value; }
    static int getCamCockpitSubGeom() { return camera()->m_cockpitSubGeom; }       // +804
    static void setCamCockpitSubGeom(int value) { camera()->m_cockpitSubGeom = value; }
    // setHasTarget - CameraTemplate::setHasTarget (0x81DF4C0) is a plain store to +536; the
    // accessor is declared on the class because retail calls it by name.
    static void setCamHasTarget(bool value) { camera()->setHasTarget(value); }


    static OverheadCameraTemplate* overheadCamera()
    {
        return static_cast<OverheadCameraTemplate*>(
            getActiveTemplate(CID_OverheadCameraTemplate));
    }
    static float getFOV() { return overheadCamera()->m_activeFOV; }
    static void setFOV(float value) { overheadCamera()->m_activeFOV = value; }
    static float getSpeed() { return overheadCamera()->m_speed; }
    static void setSpeed(float value) { overheadCamera()->m_speed = value; }
    static float getHeight() { return overheadCamera()->m_height; }
    static void setHeight(float value) { overheadCamera()->m_height = value; }

    static BundleTemplate* bundle()
    {
        return static_cast<BundleTemplate*>(getActiveTemplate(CID_BundleTemplate));
    }
    static void setPosition(const Math::Vec3& value, int index = BundleTemplate::mActiveMemberId)
    {
        bundle()->setPosition(value, index);
    }
    // Console access to protected destruction and missile flags.
    static void setExplodePartAtDestroy(bool value) { bundle()->m_explodePartAtDestroy = value; }
    static bool getKillHeatSeekingMissile() { return bundle()->m_killHeatSeekingMissile; }
    static void setKillHeatSeekingMissile(bool value) { bundle()->m_killHeatSeekingMissile = value; }

    static void setRotation(const Math::Vec3& value, int index = BundleTemplate::mActiveMemberId)
    {
        bundle()->setRotation(value, index);
    }
};

namespace {

// Stands in for the original's file-scope list (ELF 0x8AF17A4).
const char* const kObjTemplBaseConsoleTag = "world.objTemplBase";

// Every binding in this TU narrows the manager's cursor the same way (ConsoleClass96's
// isObjectActive, 0x8675BB0): the active template, but only if it IS-A SimpleObjectTemplate -
// which a BundleTemplate answers through queryImplementation's base chain.
SimpleObjectTemplate* activeSimple()
{
    return static_cast<SimpleObjectTemplate*>(getActiveTemplate(CID_SimpleObjectTemplate));
}

CameraTemplate* activeCamera()
{
    return static_cast<CameraTemplate*>(getActiveTemplate(CID_CameraTemplate));
}

WindAffectorTemplate* activeWindAffector()
{
    return static_cast<WindAffectorTemplate*>(
        getActiveTemplateComponent(IID_IWindAffectorTemplate));
}

// Accepts physics types 0..4; unsigned comparison also rejects negative values.
void objTemplPhysicsTypeSet(PhysicsType v)
{
    if (static_cast<unsigned int>(v) <= static_cast<unsigned int>(PhysicsType_Platform))
        activeSimple()->m_physicsType = static_cast<int>(v);
}

// hasCollisionPhysics SET (ConsoleClass119 exec 0x867A330): bit 2 of the physics-flags byte
// (+0xAC), read-modify-write exactly as shipped: (4 * (v & 1)) | (flags & 0xFB).
void objTemplHasCollisionPhysicsSet(bool v)
{
    unsigned char& flags = activeSimple()->m_physicsFlags;
    flags = static_cast<unsigned char>((v ? 4 : 0) | (flags & 0xFB));
}

// collisionMesh SET (ConsoleClass105 exec 0x8677A50): the virtual setCollisionMesh (+0x98).
void objTemplCollisionMeshSet(const std::string& v)
{
    activeSimple()->setCollisionMesh(v);
}

} // namespace

// General ObjectTemplate defaults use category 0. Some later general rows retain category-1 metadata.
BF2_CONSOLE_OBJECT("ObjectTemplate", 2, 0)

// Reads or sets the active template's cull-radius scale.
BF2_CONSOLE_PROPERTY(cullRadiusScale, float, getActiveTemplate(CID_ObjectTemplate), setCullRadiusScale, getCullRadiusScale, kObjTemplBaseConsoleTag)

// Deletes the named component from the active SimpleObjectTemplate.
// The neighboring createComponent binding currently differs from retail in dynamic metadata.
namespace {
void objTemplDeleteComponent(const std::string& name)
{
    static_cast<SimpleObjectTemplate*>(getActiveTemplate(CID_SimpleObjectTemplate))->deleteComponent(name);
}
} // namespace
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(deleteComponent, objTemplDeleteComponent, getActiveTemplate(CID_SimpleObjectTemplate) != 0, std::string, kObjTemplBaseConsoleTag)

// WindAffectorTemplate - ELF defaults object 0x08AF8228 and static-init 0x8668655..0x8667E33;
// PE generated constructors 0x4F0E10..0x4F1AA0 independently give the same order. This is the
// component family's unusual undotted ObjectTemplate block. Both blow-time spellings are shipped
// bindings over the same field: `widnBlowTime` is a compatibility typo, while makeScript emits
// only the corrected spelling.
BF2_CONSOLE_OBJECT("ObjectTemplate", 2, 24) // fullAccess, COCWindAffector
BF2_CONSOLE_PROPERTY(windType,          std::string, activeWindAffector(), setWindType,          getWindType,          kObjTemplBaseConsoleTag)
BF2_CONSOLE_PROPERTY(windIsDynamic,     bool,        activeWindAffector(), setWindIsDynamic,     getWindIsDynamic,     kObjTemplBaseConsoleTag)
BF2_CONSOLE_PROPERTY(windBlowTime,      float,       activeWindAffector(), setWindBlowTime,      getWindBlowTime,      kObjTemplBaseConsoleTag)
BF2_CONSOLE_PROPERTY(widnBlowTime,      float,       activeWindAffector(), setWindBlowTime,      getWindBlowTime,      kObjTemplBaseConsoleTag)
BF2_CONSOLE_PROPERTY(windDirection,     Vec3,        activeWindAffector(), setWindDirection,     getWindDirection,     kObjTemplBaseConsoleTag)
BF2_CONSOLE_PROPERTY(windFalloff,       float,       activeWindAffector(), setWindFalloff,       getWindFalloff,       kObjTemplBaseConsoleTag)
BF2_CONSOLE_PROPERTY(windRadialFalloff, float,       activeWindAffector(), setWindRadialFalloff, getWindRadialFalloff, kObjTemplBaseConsoleTag)
BF2_CONSOLE_PROPERTY(windSpeed,         float,       activeWindAffector(), setWindSpeed,         getWindSpeed,         kObjTemplBaseConsoleTag)

// The camera sub-table - SetConsoleClassDefaults("ObjectTemplate", 2, 8) at ELF 0x866BFDB.
// Both are ordinary read/write float properties whose generated executors access the fields
// directly: ConsoleClass303 worldFOV at 0x8699380 (+732) and ConsoleClass304 insideFOV at
// (+736). PE independently has the same bindings at 0x4EEDC0 (+908) and 0x4EEF80
// (+912); the different offsets are the shipped MSVC/Itanium base-layout difference.
// Stock Soldiers/Common/SoldierCamera.tweak sets both to 1.1. Without these registrations those
// lines soft-skip and Soldier::getMouseSpeedMod cannot apply the intended zoomFOV/cameraFOV ratio.
BF2_CONSOLE_OBJECT("ObjectTemplate", 2, 8)
BF2_CONSOLE_FIELD(worldFOV,  float, activeCamera(), m_worldFOV,  kObjTemplBaseConsoleTag)
BF2_CONSOLE_FIELD(insideFOV, float, activeCamera(), m_insideFOV, kObjTemplBaseConsoleTag)

// Camera properties use typed accessors for their protected fields.
BF2_CONSOLE_PROPERTY(toggleMouseLook, bool,  activeCamera(), setToggleMouseLook, getToggleMouseLook, kObjTemplBaseConsoleTag)
BF2_CONSOLE_PROPERTY(followStiffness, float, activeCamera(), setFollowStiffness, getFollowStiffness, kObjTemplBaseConsoleTag)

// Camera view, sway, shake and interior-lighting properties.
BF2_CONSOLE_PROPERTY_FN(CVMInside, bool, activeCamera() != 0,
                        BaseTemplateConsoleAccess::getCamCVMInside(),
                        BaseTemplateConsoleAccess::setCamCVMInside, kObjTemplBaseConsoleTag)
BF2_CONSOLE_PROPERTY_FN(CVMChase, bool, activeCamera() != 0,
                        BaseTemplateConsoleAccess::getCamCVMChase(),
                        BaseTemplateConsoleAccess::setCamCVMChase, kObjTemplBaseConsoleTag)
BF2_CONSOLE_PROPERTY_FN(CVMFlyBy, bool, activeCamera() != 0,
                        BaseTemplateConsoleAccess::getCamCVMFlyBy(),
                        BaseTemplateConsoleAccess::setCamCVMFlyBy, kObjTemplBaseConsoleTag)
BF2_CONSOLE_PROPERTY_FN(CVMTrace, bool, activeCamera() != 0,
                        BaseTemplateConsoleAccess::getCamCVMTrace(),
                        BaseTemplateConsoleAccess::setCamCVMTrace, kObjTemplBaseConsoleTag)
BF2_CONSOLE_PROPERTY_FN(CVMExternTrace, bool, activeCamera() != 0,
                        BaseTemplateConsoleAccess::getCamCVMExternTrace(),
                        BaseTemplateConsoleAccess::setCamCVMExternTrace, kObjTemplBaseConsoleTag)
BF2_CONSOLE_PROPERTY_FN(CVMFrontChase, bool, activeCamera() != 0,
                        BaseTemplateConsoleAccess::getCamCVMFrontChase(),
                        BaseTemplateConsoleAccess::setCamCVMFrontChase, kObjTemplBaseConsoleTag)
BF2_CONSOLE_PROPERTY_FN(CVMNose, bool, activeCamera() != 0,
                        BaseTemplateConsoleAccess::getCamCVMNose(),
                        BaseTemplateConsoleAccess::setCamCVMNose, kObjTemplBaseConsoleTag)
BF2_CONSOLE_PROPERTY_FN(nosePos, Vec3, activeCamera() != 0,
                        BaseTemplateConsoleAccess::getCamNosePos(),
                        BaseTemplateConsoleAccess::setCamNosePos, kObjTemplBaseConsoleTag)
BF2_CONSOLE_PROPERTY_FN(chaseDistance, float, activeCamera() != 0,
                        BaseTemplateConsoleAccess::getCamChaseDistance(),
                        BaseTemplateConsoleAccess::setCamChaseDistance, kObjTemplBaseConsoleTag)
BF2_CONSOLE_PROPERTY_FN(chaseAngle, float, activeCamera() != 0,
                        BaseTemplateConsoleAccess::getCamChaseAngle(),
                        BaseTemplateConsoleAccess::setCamChaseAngle, kObjTemplBaseConsoleTag)
BF2_CONSOLE_PROPERTY_FN(chaseOffset, Vec3, activeCamera() != 0,
                        BaseTemplateConsoleAccess::getCamChaseOffset(),
                        BaseTemplateConsoleAccess::setCamChaseOffset, kObjTemplBaseConsoleTag)
BF2_CONSOLE_PROPERTY_FN(cameraSwayRodLength, float, activeCamera() != 0,
                        BaseTemplateConsoleAccess::getCamCameraSwayRodLength(),
                        BaseTemplateConsoleAccess::setCamCameraSwayRodLength, kObjTemplBaseConsoleTag)
BF2_CONSOLE_PROPERTY_FN(cameraSwaySensitivity, float, activeCamera() != 0,
                        BaseTemplateConsoleAccess::getCamCameraSwaySensitivity(),
                        BaseTemplateConsoleAccess::setCamCameraSwaySensitivity, kObjTemplBaseConsoleTag)
BF2_CONSOLE_PROPERTY_FN(cameraSwayDofMax, float, activeCamera() != 0,
                        BaseTemplateConsoleAccess::getCamCameraSwayDofMax(),
                        BaseTemplateConsoleAccess::setCamCameraSwayDofMax, kObjTemplBaseConsoleTag)
BF2_CONSOLE_PROPERTY_FN(cameraSwayDofMin, float, activeCamera() != 0,
                        BaseTemplateConsoleAccess::getCamCameraSwayDofMin(),
                        BaseTemplateConsoleAccess::setCamCameraSwayDofMin, kObjTemplBaseConsoleTag)
BF2_CONSOLE_PROPERTY_FN(cameraSwayRightMax, float, activeCamera() != 0,
                        BaseTemplateConsoleAccess::getCamCameraSwayRightMax(),
                        BaseTemplateConsoleAccess::setCamCameraSwayRightMax, kObjTemplBaseConsoleTag)
BF2_CONSOLE_PROPERTY_FN(cameraSwayRightMin, float, activeCamera() != 0,
                        BaseTemplateConsoleAccess::getCamCameraSwayRightMin(),
                        BaseTemplateConsoleAccess::setCamCameraSwayRightMin, kObjTemplBaseConsoleTag)
BF2_CONSOLE_PROPERTY_FN(cameraSwaySpringStrength, float, activeCamera() != 0,
                        BaseTemplateConsoleAccess::getCamCameraSwaySpringStrength(),
                        BaseTemplateConsoleAccess::setCamCameraSwaySpringStrength, kObjTemplBaseConsoleTag)
BF2_CONSOLE_PROPERTY_FN(cameraSwaySpringDamping, float, activeCamera() != 0,
                        BaseTemplateConsoleAccess::getCamCameraSwaySpringDamping(),
                        BaseTemplateConsoleAccess::setCamCameraSwaySpringDamping, kObjTemplBaseConsoleTag)
BF2_CONSOLE_PROPERTY_FN(cameraShakeSensitivity, float, activeCamera() != 0,
                        BaseTemplateConsoleAccess::getCamCameraShakeSensitivity(),
                        BaseTemplateConsoleAccess::setCamCameraShakeSensitivity, kObjTemplBaseConsoleTag)
BF2_CONSOLE_PROPERTY_FN(cameraShakeStartSpeed, float, activeCamera() != 0,
                        BaseTemplateConsoleAccess::getCamCameraShakeStartSpeed(),
                        BaseTemplateConsoleAccess::setCamCameraShakeStartSpeed, kObjTemplBaseConsoleTag)
BF2_CONSOLE_PROPERTY_FN(cameraShakeMaxSpeed, float, activeCamera() != 0,
                        BaseTemplateConsoleAccess::getCamCameraShakeMaxSpeed(),
                        BaseTemplateConsoleAccess::setCamCameraShakeMaxSpeed, kObjTemplBaseConsoleTag)
BF2_CONSOLE_PROPERTY_FN(bounceLength, float, activeCamera() != 0,
                        BaseTemplateConsoleAccess::getCamBounceLength(),
                        BaseTemplateConsoleAccess::setCamBounceLength, kObjTemplBaseConsoleTag)
BF2_CONSOLE_PROPERTY_FN(bounceSensitivity, float, activeCamera() != 0,
                        BaseTemplateConsoleAccess::getCamBounceSensitivity(),
                        BaseTemplateConsoleAccess::setCamBounceSensitivity, kObjTemplBaseConsoleTag)
BF2_CONSOLE_PROPERTY_FN(bounceStrength, float, activeCamera() != 0,
                        BaseTemplateConsoleAccess::getCamBounceStrength(),
                        BaseTemplateConsoleAccess::setCamBounceStrength, kObjTemplBaseConsoleTag)
BF2_CONSOLE_PROPERTY_FN(bounceDamping, float, activeCamera() != 0,
                        BaseTemplateConsoleAccess::getCamBounceDamping(),
                        BaseTemplateConsoleAccess::setCamBounceDamping, kObjTemplBaseConsoleTag)
BF2_CONSOLE_PROPERTY_FN(allowInsideDynamicLights, bool, activeCamera() != 0,
                        BaseTemplateConsoleAccess::getCamAllowInsideDynamicLights(),
                        BaseTemplateConsoleAccess::setCamAllowInsideDynamicLights, kObjTemplBaseConsoleTag)
BF2_CONSOLE_PROPERTY_FN(allowInsideStaticSun, bool, activeCamera() != 0,
                        BaseTemplateConsoleAccess::getCamAllowInsideStaticSun(),
                        BaseTemplateConsoleAccess::setCamAllowInsideStaticSun, kObjTemplBaseConsoleTag)
BF2_CONSOLE_PROPERTY_FN(insideStaticSunDirection, Vec3, activeCamera() != 0,
                        BaseTemplateConsoleAccess::getCamInsideStaticSunDirection(),
                        BaseTemplateConsoleAccess::setCamInsideStaticSunDirection, kObjTemplBaseConsoleTag)
BF2_CONSOLE_PROPERTY_FN(insideDamageFrequency, float, activeCamera() != 0,
                        BaseTemplateConsoleAccess::getCamInsideDamageFrequency(),
                        BaseTemplateConsoleAccess::setCamInsideDamageFrequency, kObjTemplBaseConsoleTag)
BF2_CONSOLE_PROPERTY_FN(insideDamageThreshold, float, activeCamera() != 0,
                        BaseTemplateConsoleAccess::getCamInsideDamageThreshold(),
                        BaseTemplateConsoleAccess::setCamInsideDamageThreshold, kObjTemplBaseConsoleTag)
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setHasTarget, BaseTemplateConsoleAccess::setCamHasTarget,
                                   activeCamera() != 0, bool, kObjTemplBaseConsoleTag)
BF2_CONSOLE_PROPERTY_FN(insideStaticSunColor, Vec3ColorFloatIntensity, activeCamera() != 0,
                        BaseTemplateConsoleAccess::getCamInsideStaticSunColor(),
                        BaseTemplateConsoleAccess::setCamInsideStaticSunColor, kObjTemplBaseConsoleTag)
BF2_CONSOLE_PROPERTY_FN(insideStaticSunDamageColor, Vec3ColorFloatIntensity, activeCamera() != 0,
                        BaseTemplateConsoleAccess::getCamInsideStaticSunDamageColor(),
                        BaseTemplateConsoleAccess::setCamInsideStaticSunDamageColor, kObjTemplBaseConsoleTag)
BF2_CONSOLE_PROPERTY_FN(insideStaticSunAmbientColor, Vec3ColorFloatIntensity, activeCamera() != 0,
                        BaseTemplateConsoleAccess::getCamInsideStaticSunAmbientColor(),
                        BaseTemplateConsoleAccess::setCamInsideStaticSunAmbientColor, kObjTemplBaseConsoleTag)

// Camera cockpit LOD and sub-geometry override vehicle settings; -1 defers to the vehicle.
// [INFERRED] Placement within the camera category.
BF2_CONSOLE_PROPERTY_FN(cockpitLod, int, activeCamera() != 0,
                        BaseTemplateConsoleAccess::getCamCockpitLod(),
                        BaseTemplateConsoleAccess::setCamCockpitLod, kObjTemplBaseConsoleTag)
BF2_CONSOLE_PROPERTY_FN(cockpitSubGeom, int, activeCamera() != 0,
                        BaseTemplateConsoleAccess::getCamCockpitSubGeom(),
                        BaseTemplateConsoleAccess::setCamCockpitSubGeom, kObjTemplBaseConsoleTag)


// ConsoleClass268: ELF 0x8691180 reads/writes CameraTemplate +0x21C; PE 0x4EBA50
// uses +0x2CC. Both expose an unrestricted int, zero/one argument, gated on CID_CameraTemplate
// (ELF 0x8690F00 / PE 0x4EB690). Stock Su-34 missiles switch to camera 11 and forbid firing
// outside it; dropping cameraId leaves that camera unnumbered and makes the switch miss.
BF2_CONSOLE_PROPERTY_FN(cameraId, int, activeCamera() != 0,
                        BaseTemplateConsoleAccess::getCameraId(),
                        BaseTemplateConsoleAccess::setCameraId, kObjTemplBaseConsoleTag)

// OverheadCameraTemplate - PE generated executors 0x4F0840 / 0x4F0A00 / 0x4F0BC0, with active
// gates from the corresponding 0x4F07C0 block. The first spelling is deliberately `fOV`.
BF2_CONSOLE_PROPERTY_FN(fOV, float, getActiveTemplate(CID_OverheadCameraTemplate) != 0,
                        BaseTemplateConsoleAccess::getFOV(),
                        BaseTemplateConsoleAccess::setFOV, kObjTemplBaseConsoleTag)
BF2_CONSOLE_PROPERTY_FN(speed, float, getActiveTemplate(CID_OverheadCameraTemplate) != 0,
                        BaseTemplateConsoleAccess::getSpeed(),
                        BaseTemplateConsoleAccess::setSpeed, kObjTemplBaseConsoleTag)
BF2_CONSOLE_PROPERTY_FN(height, float, getActiveTemplate(CID_OverheadCameraTemplate) != 0,
                        BaseTemplateConsoleAccess::getHeight(),
                        BaseTemplateConsoleAccess::setHeight, kObjTemplBaseConsoleTag)

// The physics sub-table - SetConsoleClassDefaults("ObjectTemplate", 2, 23), the block between
// and 0x8671A9A in the TU's init.
BF2_CONSOLE_OBJECT("ObjectTemplate", 2, 23)

// objectTemplate.hasCollisionPhysics - ConsoleClass119 (ctor site 0x8671983). GET is the mask
// test ((flags & 4) != 0), bool into the +0xA2 return member.
BF2_CONSOLE_PROPERTY_FN(hasCollisionPhysics, bool,
                        getActiveTemplate(CID_SimpleObjectTemplate) != 0,
                        (activeSimple()->m_physicsFlags & 4) != 0,
                        objTemplHasCollisionPhysicsSet, kObjTemplBaseConsoleTag)

// One-argument physics methods set their bit and recompute physics type; they have no getter form.
namespace {
void objTemplHasPointPhysicsSet(bool value)           { BaseTemplateConsoleAccess::setPhysicsBit(0x08, value); }
void objTemplHasRotationalPointPhysicsSet(bool value) { BaseTemplateConsoleAccess::setPhysicsBit(0x10, value); }
void objTemplHasResponsePhysicsSet(bool value)        { BaseTemplateConsoleAccess::setPhysicsBit(0x20, value); }
} // namespace
BF2_CONSOLE_METHOD_1_ACTIVE(hasPointPhysics, objTemplHasPointPhysicsSet,
                            getActiveTemplate(CID_SimpleObjectTemplate) != 0, bool, kObjTemplBaseConsoleTag)
BF2_CONSOLE_METHOD_1_ACTIVE(hasRotationalPointPhysics, objTemplHasRotationalPointPhysicsSet,
                            getActiveTemplate(CID_SimpleObjectTemplate) != 0, bool, kObjTemplBaseConsoleTag)
BF2_CONSOLE_METHOD_1_ACTIVE(hasResponsePhysics, objTemplHasResponsePhysicsSet,
                            getActiveTemplate(CID_SimpleObjectTemplate) != 0, bool, kObjTemplBaseConsoleTag)

// objectTemplate.isSleeping - ConsoleClass139, bit 0 of the same byte, and the one bit whose
// binding is a property (get arm returns `flags & 1`). It does NOT call checkPhysicsTypes.
BF2_CONSOLE_PROPERTY_FN(isSleeping, bool,
                        getActiveTemplate(CID_SimpleObjectTemplate) != 0,
                        BaseTemplateConsoleAccess::getIsSleeping(),
                        BaseTemplateConsoleAccess::setIsSleeping, kObjTemplBaseConsoleTag)

// collisionGroups is the same direct-field family: ConsoleClass122 stores a U32 at ELF +0xC0
// (0x867AE30) / PE +0xF0 (0x4E2EF0). GenericProjectile's constructor supplies bit 3 itself, but
// publishing the authored value is required for the rest of the SimpleObject hierarchy too.
BF2_CONSOLE_PROPERTY_FN(collisionGroups, U32,
                        getActiveTemplate(CID_SimpleObjectTemplate) != 0,
                        BaseTemplateConsoleAccess::getCollisionGroups(),
                        BaseTemplateConsoleAccess::setCollisionGroups, kObjTemplBaseConsoleTag)

// objectTemplate.physicsType - ConsoleClass127 of this TU (ctor site 0x8671293). GET reads the
// field raw through the recovered enum vocabulary; the range guard lives on the SET side only.
BF2_CONSOLE_PROPERTY_FN(physicsType, PhysicsType,
                        getActiveTemplate(CID_SimpleObjectTemplate) != 0,
                        static_cast<PhysicsType>(activeSimple()->m_physicsType),
                        objTemplPhysicsTypeSet, kObjTemplBaseConsoleTag)

// The common mobile-physics scalars are direct-field properties on SimpleObjectTemplate. The
// generated executors agree in both shipped ports: drag writes ELF +0x40 / PE +0x70
// (0x867C5B0 / 0x4E37B0), mass writes ELF +0x5C / PE +0x8C (0x867D4F0 / 0x4E3DB0), and
// gravityModifier writes ELF +0x84 / PE +0xB4 (0x867E9A0 / 0x4E4660). These are consumed by
// buildMobilePhysicsNode; dropping them left every authored projectile at drag 0, mass 1 and full
// gravity. The friend bridge preserves the raw-store behavior without inventing gameplay accessors.
BF2_CONSOLE_PROPERTY_FN(drag, float, getActiveTemplate(CID_SimpleObjectTemplate) != 0,
                        BaseTemplateConsoleAccess::getDrag(),
                        BaseTemplateConsoleAccess::setDrag, kObjTemplBaseConsoleTag)
BF2_CONSOLE_PROPERTY_FN(mass, float, getActiveTemplate(CID_SimpleObjectTemplate) != 0,
                        BaseTemplateConsoleAccess::getMass(),
                        BaseTemplateConsoleAccess::setMass, kObjTemplBaseConsoleTag)
BF2_CONSOLE_PROPERTY_FN(gravityModifier, float,
                        getActiveTemplate(CID_SimpleObjectTemplate) != 0,
                        BaseTemplateConsoleAccess::getGravityModifier(),
                        BaseTemplateConsoleAccess::setGravityModifier, kObjTemplBaseConsoleTag)

// Vector drag, mass-center and inertia properties for SimpleObjectTemplate.
// FloatingBundleTemplate's scalar dragModifier is a separate binding selected by the active class.
BF2_CONSOLE_PROPERTY_FN(dragOffset, Vec3, getActiveTemplate(CID_SimpleObjectTemplate) != 0,
                        BaseTemplateConsoleAccess::getDragOffset(),
                        BaseTemplateConsoleAccess::setDragOffset, kObjTemplBaseConsoleTag)
BF2_CONSOLE_PROPERTY_FN(dragModifier, Vec3, getActiveTemplate(CID_SimpleObjectTemplate) != 0,
                        BaseTemplateConsoleAccess::getDragModifier(),
                        BaseTemplateConsoleAccess::setDragModifier, kObjTemplBaseConsoleTag)
BF2_CONSOLE_PROPERTY_FN(centerOfMassOffset, Vec3,
                        getActiveTemplate(CID_SimpleObjectTemplate) != 0,
                        BaseTemplateConsoleAccess::getCenterOfMassOffset(),
                        BaseTemplateConsoleAccess::setCenterOfMassOffset, kObjTemplBaseConsoleTag)
BF2_CONSOLE_PROPERTY_FN(inertiaModifier, Vec3,
                        getActiveTemplate(CID_SimpleObjectTemplate) != 0,
                        BaseTemplateConsoleAccess::getInertiaModifier(),
                        BaseTemplateConsoleAccess::setInertiaModifier, kObjTemplBaseConsoleTag)

// grip - PE ConsoleClass constructor 0x4E4C00 / executor 0x4E4CF0, metadata type
// PhysicsGripFlags, direct dword at PE +0xE8; the ELF field/writeout pair independently places it
// at +0xB8. This is not cosmetic: ResponsePhysics::addFriction selects the driven-wheel arm when
// bit 8 is present. Stock tanks author grip 8 on their drive wheels and grip 128 on passive road
// wheels; without this property every wheel kept the constructor default 2 and no track force was
// ever produced.
BF2_CONSOLE_PROPERTY_FN(grip, PhysicsGripFlags,
                        getActiveTemplate(CID_SimpleObjectTemplate) != 0,
                        BaseTemplateConsoleAccess::getGrip(),
                        BaseTemplateConsoleAccess::setGrip, kObjTemplBaseConsoleTag)

// Collision, simulation frequency and portal-passing properties.
BF2_CONSOLE_PROPERTY_FN(centerOfCollisionOffset, Vec3,
                        getActiveTemplate(CID_SimpleObjectTemplate) != 0,
                        BaseTemplateConsoleAccess::getCenterOfCollisionOffset(),
                        BaseTemplateConsoleAccess::setCenterOfCollisionOffset, kObjTemplBaseConsoleTag)
BF2_CONSOLE_PROPERTY_FN(floaterMod, float, getActiveTemplate(CID_SimpleObjectTemplate) != 0,
                        BaseTemplateConsoleAccess::getFloaterMod(),
                        BaseTemplateConsoleAccess::setFloaterMod, kObjTemplBaseConsoleTag)
BF2_CONSOLE_PROPERTY_FN(otherCollisionLod, int, getActiveTemplate(CID_SimpleObjectTemplate) != 0,
                        BaseTemplateConsoleAccess::getOtherCollisionLod(),
                        BaseTemplateConsoleAccess::setOtherCollisionLod, kObjTemplBaseConsoleTag)
BF2_CONSOLE_PROPERTY_FN(physicsFrequency, int, getActiveTemplate(CID_SimpleObjectTemplate) != 0,
                        BaseTemplateConsoleAccess::getPhysicsFrequency(),
                        BaseTemplateConsoleAccess::setPhysicsFrequency, kObjTemplBaseConsoleTag)
BF2_CONSOLE_PROPERTY_FN(handleCollisionSpeed, float, getActiveTemplate(CID_SimpleObjectTemplate) != 0,
                        BaseTemplateConsoleAccess::getHandleCollisionSpeed(),
                        BaseTemplateConsoleAccess::setHandleCollisionSpeed, kObjTemplBaseConsoleTag)
BF2_CONSOLE_PROPERTY_FN(isPortalPassing, bool, getActiveTemplate(CID_SimpleObjectTemplate) != 0,
                        BaseTemplateConsoleAccess::getIsPortalPassing(),
                        BaseTemplateConsoleAccess::setIsPortalPassing, kObjTemplBaseConsoleTag)
BF2_CONSOLE_PROPERTY_FN(portalPassingPosition, Vec3, getActiveTemplate(CID_SimpleObjectTemplate) != 0,
                        BaseTemplateConsoleAccess::getPortalPassingPosition(),
                        BaseTemplateConsoleAccess::setPortalPassingPosition, kObjTemplBaseConsoleTag)

// Three that need no bridge - the class already carries the accessors the executors call:
// ConsoleClass112  anchorOffset            Vec3    virtual +0xAC set / +0xA8 get
// ConsoleClass114  relativePositionOffset  Vec3    virtual +0x104 set / +0x108 get (by value)
// ConsoleClass124  treeCollisionDiameter   float   NON-virtual pair, 0x81F2BE0 / 0x81F2C00, and
// the setter clamps negatives to 0
BF2_CONSOLE_PROPERTY(anchorOffset, Vec3, activeSimple(), setAnchorOffset, getAnchorOffset, kObjTemplBaseConsoleTag)
BF2_CONSOLE_PROPERTY(relativePositionOffset, Vec3, activeSimple(), setRelativePositionOffset, getRelativePositionOffset, kObjTemplBaseConsoleTag)
BF2_CONSOLE_PROPERTY(treeCollisionDiameter, float, activeSimple(), setTreeCollisionDiameter, getTreeCollisionDiameter, kObjTemplBaseConsoleTag)

// The general sub-table - SetConsoleClassDefaults("ObjectTemplate", 2, 1) at 0x8671A9A.
BF2_CONSOLE_OBJECT("ObjectTemplate", 2, 1)

// objectTemplate.collisionMesh - ConsoleClass105 (ctor site 0x86723C9). A void one-argument method
// in the shipped PE (metadata argc 1..1, return void); a bare read does nothing and answers nothing.
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(collisionMesh, objTemplCollisionMeshSet, getActiveTemplate(CID_SimpleObjectTemplate) != 0, std::string, kObjTemplBaseConsoleTag)

// Alternate console spelling for collisionMesh (the same void one-argument method shape).
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setCollisionMesh, objTemplCollisionMeshSet, getActiveTemplate(CID_SimpleObjectTemplate) != 0, std::string, kObjTemplBaseConsoleTag)

// Reads or sets the AI-template name used by object and weapon lookup in bot games.
BF2_CONSOLE_PROPERTY_FN(aiTemplate, std::string, getActiveTemplate(CID_SimpleObjectTemplate) != 0,
                        BaseTemplateConsoleAccess::getAiTemplate(),
                        BaseTemplateConsoleAccess::setAiTemplate, kObjTemplBaseConsoleTag)

// Collision and geometry part properties select child mesh parts.
// ParticleTemplate registers separate bindings for the same names in the FX module.
namespace {
void objTemplCollisionPartSet(int v) { activeSimple()->setCollisionPart(v); }
void objTemplGeometryPartSet(int v)  { activeSimple()->setGeometryPart(v); }
} // namespace
BF2_CONSOLE_PROPERTY_FN(collisionPart, int, getActiveTemplate(CID_SimpleObjectTemplate) != 0, activeSimple()->m_collisionPart, objTemplCollisionPartSet, kObjTemplBaseConsoleTag)
BF2_CONSOLE_PROPERTY_FN(geometryPart, int, getActiveTemplate(CID_SimpleObjectTemplate) != 0, activeSimple()->m_geometryPart, objTemplGeometryPartSet, kObjTemplBaseConsoleTag)

// objectTemplate.geometry - ConsoleClass96 (ctor site 0x8672AB5, exec 0x8675E70): the clean
// member-pair property - setGeometry (+0xF0) / getGeometry (+0xF4) on the concrete template.
BF2_CONSOLE_PROPERTY(geometry, std::string, activeSimple(), setGeometry, getGeometry, kObjTemplBaseConsoleTag)

// ConsoleClass166 is an accessor property in both ports, not a raw store: ELF 0x86844C0 dispatches
// setMaterial/getMaterial at primary slots +0xC4/+0xC8; PE 0x4E6A70 uses +0xC0/+0xC4.
BF2_CONSOLE_PROPERTY(material, int, activeSimple(), setMaterial, getMaterial, kObjTemplBaseConsoleTag)

// objectTemplate.anchor - ConsoleClass111 (ctor site 0x8671FA3, exec 0x8678A80): a Vec3 property -
// setAnchor (+0xA4) / getAnchor (+0xA0). The slash-separated text form ("x/y/z") is the console's
// Vec3 convention, recovered from this binding's own generated pair and packaged as the stream
// operators in Math/Vec3.{h,cpp}. The shipped PE registers the metadata type as "Vec3" (the ELF
// uses "Math::Vec3"); the dedicated-server dump follows the PE. The 12-byte return member and the
// '/'-joined GET reply come free from the property family once those operators exist.
BF2_CONSOLE_PROPERTY(anchor, Vec3, activeSimple(), setAnchor, getAnchor, kObjTemplBaseConsoleTag)

// objectTemplate.mapMaterial - ConsoleClass107 (ctor site 0x86721D8, exec 0x86780B0): argc
// exactly 3, target = the template's own mapMaterial(unsigned, const std::string&, unsigned)
// (+0x100). The ELF/PE metadata spells the two numeric argTypes "uint"; the local alias above
// registers exactly that string.
namespace {
void objTemplMapMaterialSet(unsigned int index, const std::string& material, unsigned int flags)
{
    activeSimple()->mapMaterial(index, material, flags);
}
} // namespace
BF2_CONSOLE_METHOD_3(mapMaterial, objTemplMapMaterialSet, getActiveTemplate(CID_SimpleObjectTemplate) != 0, uint, std::string, uint, kObjTemplBaseConsoleTag)

// Network descriptor, mobile-physics and child-template bindings, selected by the active template class.
BF2_CONSOLE_PROPERTY(networkableInfo, std::string, activeSimple(), setNetworkableInfo, getNetworkableInfo, kObjTemplBaseConsoleTag)
BF2_CONSOLE_PROPERTY(hasMobilePhysics, bool, activeSimple(), setHasMobilePhysics, getHasMobilePhysics, kObjTemplBaseConsoleTag)
namespace {
void objTemplBundleAddTemplate(const std::string& templateName)
{
    static_cast<BundleTemplate*>(getActiveTemplate(CID_BundleTemplate))->addTemplate(templateName);
}
} // namespace
BF2_CONSOLE_METHOD_1_ACTIVE(addTemplate, objTemplBundleAddTemplate, getActiveTemplate(CID_BundleTemplate) != 0, std::string, kObjTemplBaseConsoleTag)

// objectTemplate.setPosition / setRotation - ConsoleClass193/194, generated executors
// / 0x8686EA0. Both accept one Math::Vec3 plus an optional child index, and the one-arg
// arm uses BundleTemplate::mActiveMemberId selected by addTemplate. Their generated objects store
// minArgs=1, maxArgs=2, void return and m_isDynamic=false (+0xA1), hence the STATIC_ACTIVE family.
// These calls author each child's relation-to-parent transform. Without them every addTemplate
// child remains at the identity transform: vehicle Springs/wheels are created at the hull origin,
// while turret barrels, cameras and entry points likewise lose their authored placement/rotation.
BF2_CONSOLE_METHOD_1_2_STATIC_ACTIVE(setPosition, BaseTemplateConsoleAccess::setPosition, getActiveTemplate(CID_BundleTemplate) != 0, Math::Vec3, int, kObjTemplBaseConsoleTag)
BF2_CONSOLE_METHOD_1_2_STATIC_ACTIVE(setRotation, BaseTemplateConsoleAccess::setRotation, getActiveTemplate(CID_BundleTemplate) != 0, Math::Vec3, int, kObjTemplBaseConsoleTag)

// Bundle member-list and destruction settings.
// Unimplemented optional-index getters require their original argument and return behavior before binding.
namespace {
void objTemplBundleClearTemplateList()
{
    static_cast<BundleTemplate*>(getActiveTemplate(CID_BundleTemplate))->clearTemplateList();
}
std::string objTemplBundleListTemplates()
{
    return static_cast<BundleTemplate*>(getActiveTemplate(CID_BundleTemplate))->listTemplates();
}
void objTemplBundleRemoveTemplate(int index)
{
    static_cast<BundleTemplate*>(getActiveTemplate(CID_BundleTemplate))->removeTemplate(index);
}
void objTemplBundleSetActiveTemplate(int index)
{
    static_cast<BundleTemplate*>(getActiveTemplate(CID_BundleTemplate))->setActiveTemplate(index);
}
} // namespace
BF2_CONSOLE_METHOD_0_RET_ACTIVE(listTemplates, objTemplBundleListTemplates, std::string,
                                getActiveTemplate(CID_BundleTemplate) != 0, kObjTemplBaseConsoleTag)
BF2_CONSOLE_METHOD_0_STATIC_ACTIVE(clearTemplateList, objTemplBundleClearTemplateList,
                                   getActiveTemplate(CID_BundleTemplate) != 0, kObjTemplBaseConsoleTag)
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(removeTemplate, objTemplBundleRemoveTemplate,
                                   getActiveTemplate(CID_BundleTemplate) != 0, int, kObjTemplBaseConsoleTag)
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setActiveTemplate, objTemplBundleSetActiveTemplate,
                                   getActiveTemplate(CID_BundleTemplate) != 0, int, kObjTemplBaseConsoleTag)
BF2_CONSOLE_METHOD_1_STATIC_ACTIVE(setExplodePartAtDestroy, BaseTemplateConsoleAccess::setExplodePartAtDestroy,
                                   getActiveTemplate(CID_BundleTemplate) != 0, bool, kObjTemplBaseConsoleTag)
BF2_CONSOLE_PROPERTY_FN(killHeatSeekingMissile, bool, getActiveTemplate(CID_BundleTemplate) != 0,
                        BaseTemplateConsoleAccess::getKillHeatSeekingMissile(),
                        BaseTemplateConsoleAccess::setKillHeatSeekingMissile, kObjTemplBaseConsoleTag)

// Entry-point reach in ordinary world units. Content may use the console set-prefix spelling.
BF2_CONSOLE_FIELD(entryRadius, float, static_cast<EntryPointTemplate*>(getActiveTemplate(CID_EntryPointTemplate)),
                  m_entryRadius, kObjTemplBaseConsoleTag)

// Creates a named component on the active SimpleObjectTemplate; requires one string argument.
namespace {
void objTemplCreateComponent(const std::string& name)
{
    static_cast<SimpleObjectTemplate*>(getActiveTemplate(CID_SimpleObjectTemplate))->createComponent(name);
}
} // namespace
BF2_CONSOLE_METHOD_1_ACTIVE(createComponent, objTemplCreateComponent, getActiveTemplate(CID_SimpleObjectTemplate) != 0, std::string, kObjTemplBaseConsoleTag)

// Rotational settings also determine whether network properties may be shared between vehicle members.
// PlayerInputMap arguments use the input-map stream parser.
namespace {
RotationalBundleTemplate* activeRotBundle()
{
    return static_cast<RotationalBundleTemplate*>(getActiveTemplate(CID_RotationalBundleTemplate));
}
} // namespace

// ConsoleClass225 - the pivot the whole bundle rotates about, +404, raw store. ;
// the field was already in the public island and carried the console name in its comment.
BF2_CONSOLE_FIELD(PivotPosition,  Math::Vec3,         activeRotBundle(), m_pivotPosition,  kObjTemplBaseConsoleTag)
BF2_CONSOLE_FIELD(inputToYaw,     io::PlayerInputMap, activeRotBundle(), m_inputToYaw,     kObjTemplBaseConsoleTag)
BF2_CONSOLE_FIELD(inputToPitch,   io::PlayerInputMap, activeRotBundle(), m_inputToPitch,   kObjTemplBaseConsoleTag)
BF2_CONSOLE_FIELD(inputToRoll,    io::PlayerInputMap, activeRotBundle(), m_inputToRoll,    kObjTemplBaseConsoleTag)
BF2_CONSOLE_FIELD(minRotation,    Math::Vec3,         activeRotBundle(), m_minRotation,    kObjTemplBaseConsoleTag)
BF2_CONSOLE_FIELD(maxRotation,    Math::Vec3,         activeRotBundle(), m_maxRotation,    kObjTemplBaseConsoleTag)
BF2_CONSOLE_FIELD(maxSpeed,       Math::Vec3,         activeRotBundle(), m_maxSpeed,       kObjTemplBaseConsoleTag)
BF2_CONSOLE_FIELD(acceleration,   Math::Vec3,         activeRotBundle(), m_acceleration,   kObjTemplBaseConsoleTag)
BF2_CONSOLE_FIELD(automaticReset, bool,               activeRotBundle(), m_automaticReset, kObjTemplBaseConsoleTag)

// Rotation speed, stabilization and reset properties. Preserve the console spelling continousRotationSpeed.
// lesserYawAtSpeed ignores values that are not positive.
BF2_CONSOLE_FIELD(maxResetSpeed,               Math::Vec3, activeRotBundle(), m_maxResetSpeed,               kObjTemplBaseConsoleTag)
BF2_CONSOLE_FIELD(deAcceleration,              Math::Vec3, activeRotBundle(), m_deAcceleration,              kObjTemplBaseConsoleTag)
BF2_CONSOLE_FIELD(continousRotationSpeed,      Math::Vec3, activeRotBundle(), m_continousRotationSpeed,      kObjTemplBaseConsoleTag)
BF2_CONSOLE_FIELD(snapToZeroOnNoInput,         bool,       activeRotBundle(), m_snapToZeroOnNoInput,         kObjTemplBaseConsoleTag)
BF2_CONSOLE_FIELD(useDeAcceleration,           bool,       activeRotBundle(), m_useDeAcceleration,           kObjTemplBaseConsoleTag)
BF2_CONSOLE_PROPERTY(rememberExcessInput,      bool,       activeRotBundle(), setRememberExcessInput, getRememberExcessInput, kObjTemplBaseConsoleTag)
BF2_CONSOLE_PROPERTY(lesserYawAtSpeed,         float,      activeRotBundle(), setLesserYawAtSpeed,    getLesserYawAtSpeed,    kObjTemplBaseConsoleTag)
BF2_CONSOLE_FIELD(automaticYawStabilization,   bool,       activeRotBundle(), m_automaticYawStabilization,   kObjTemplBaseConsoleTag)
BF2_CONSOLE_FIELD(automaticPitchStabilization, bool,       activeRotBundle(), m_automaticPitchStabilization, kObjTemplBaseConsoleTag)
BF2_CONSOLE_FIELD(startAutoRotateOnEnter,      bool,       activeRotBundle(), m_startAutoRotateOnEnter,      kObjTemplBaseConsoleTag)
BF2_CONSOLE_FIELD(debugAutoRotate,             bool,       activeRotBundle(), m_debugAutoRotate,             kObjTemplBaseConsoleTag)
BF2_CONSOLE_FIELD(restoreRotationOnExit,       bool,       activeRotBundle(), m_restoreRotationOnExit,       kObjTemplBaseConsoleTag)
BF2_CONSOLE_FIELD(capMaxSpeed,                 bool,       activeRotBundle(), m_capMaxSpeed,                 kObjTemplBaseConsoleTag)

// Rotational-bundle regulation ranges. Player-control templates have separate bindings with the same names.
BF2_CONSOLE_FIELD(regulateYaw,                 Vec2,      activeRotBundle(), m_regulateYaw,                 kObjTemplBaseConsoleTag)
BF2_CONSOLE_FIELD(regulatePitch,               Vec2,      activeRotBundle(), m_regulatePitch,               kObjTemplBaseConsoleTag)
BF2_CONSOLE_FIELD(regulateRoll,                Vec2,      activeRotBundle(), m_regulateRoll,                kObjTemplBaseConsoleTag)

// LightSource console bindings: category COCLight (18), 35 rows in shipped order.
// Class-gated executors re-read the active template; geometry accesses the raw string field.
// Preserve scale's reversed get/set slots and getConeAngle1 returning coneAngle2.
// Range/angle setters report fatal errors above their caps (100 / 180); empty arguments are gets.
// RW-261: the earlier objectManager castsDynamicShadow row remains withheld, so this
// LightSource-specific row currently receives those calls. Registration order matters.

namespace {
LightSourceTemplate* activeLightSource()
{
    return static_cast<LightSourceTemplate*>(getActiveTemplate(CID_LightSourceTemplate));
}
} // namespace

BF2_CONSOLE_OBJECT("ObjectTemplate", 2, 18)
BF2_CONSOLE_FIELD(geometry,                    std::string,             activeLightSource(), m_geometry, kObjTemplBaseConsoleTag)
BF2_CONSOLE_PROPERTY(lightType,                std::string,             activeLightSource(), setLightType,                getLightType,                kObjTemplBaseConsoleTag)
BF2_CONSOLE_PROPERTY(affectingType,            int,                     activeLightSource(), setAffectingType,            getAffectingType,            kObjTemplBaseConsoleTag)
BF2_CONSOLE_PROPERTY(isDynamic,                bool,                    activeLightSource(), setIsDynamic,                getIsDynamic,                kObjTemplBaseConsoleTag)
BF2_CONSOLE_PROPERTY(castsStaticShadow,        bool,                    activeLightSource(), setCastsStaticShadow,        getCastsStaticShadow,        kObjTemplBaseConsoleTag)
BF2_CONSOLE_PROPERTY(castsDynamicShadow,       bool,                    activeLightSource(), setCastsDynamicShadow,       getCastsDynamicShadow,       kObjTemplBaseConsoleTag)
BF2_CONSOLE_PROPERTY(selfLights,               bool,                    activeLightSource(), setSelfLights,               getSelfLights,               kObjTemplBaseConsoleTag)
BF2_CONSOLE_PROPERTY(selfShadows,              bool,                    activeLightSource(), setSelfShadows,              getSelfShadows,              kObjTemplBaseConsoleTag)
BF2_CONSOLE_PROPERTY(supportsPerVertex,        bool,                    activeLightSource(), setSupportsPerVertex,        getSupportsPerVertex,        kObjTemplBaseConsoleTag)
BF2_CONSOLE_PROPERTY(supportsPerPixel,         bool,                    activeLightSource(), setSupportsPerPixel,         getSupportsPerPixel,         kObjTemplBaseConsoleTag)
BF2_CONSOLE_PROPERTY(supportsGlow,             bool,                    activeLightSource(), setSupportsGlow,             getSupportsGlow,             kObjTemplBaseConsoleTag)
BF2_CONSOLE_PROPERTY(supportsEmitter,          bool,                    activeLightSource(), setSupportsEmitter,          getSupportsEmitter,          kObjTemplBaseConsoleTag)
BF2_CONSOLE_PROPERTY(affectLightmappedObjects, bool,                    activeLightSource(), setAffectLightmappedObjects, getAffectLightmappedObjects, kObjTemplBaseConsoleTag)
BF2_CONSOLE_PROPERTY(attenuationRange1,        float,                   activeLightSource(), setAttenuationRange1,        getAttenuationRange1,        kObjTemplBaseConsoleTag)
BF2_CONSOLE_PROPERTY(attenuationRange2,        float,                   activeLightSource(), setAttenuationRange2,        getAttenuationRange2,        kObjTemplBaseConsoleTag)
BF2_CONSOLE_PROPERTY(color,                    Vec3ColorFloatIntensity, activeLightSource(), setColor,                    getColor,                    kObjTemplBaseConsoleTag)
BF2_CONSOLE_PROPERTY(speccolor,                Vec3ColorFloatIntensity, activeLightSource(), setSpecColor,                getSpecColor,                kObjTemplBaseConsoleTag)
BF2_CONSOLE_PROPERTY(HDRIntensity,             float,                   activeLightSource(), setHDRIntensity,             getHDRIntensity,             kObjTemplBaseConsoleTag)
BF2_CONSOLE_PROPERTY(coneAngle1,               float,                   activeLightSource(), setConeAngle1,               getConeAngle1,               kObjTemplBaseConsoleTag)
BF2_CONSOLE_PROPERTY(coneAngle2,               float,                   activeLightSource(), setConeAngle2,               getConeAngle2,               kObjTemplBaseConsoleTag)
BF2_CONSOLE_PROPERTY(direction,                Vec3,                    activeLightSource(), setDirection,                getDirection,                kObjTemplBaseConsoleTag)
BF2_CONSOLE_PROPERTY(projectorTexture,         std::string,             activeLightSource(), setProjectorTexture,         getProjectorTexture,         kObjTemplBaseConsoleTag)
BF2_CONSOLE_PROPERTY(projectorFov,             float,                   activeLightSource(), setProjectorFov,             getProjectorFov,             kObjTemplBaseConsoleTag)
BF2_CONSOLE_PROPERTY(projectorAspect,          float,                   activeLightSource(), setProjectorAspect,          getProjectorAspect,          kObjTemplBaseConsoleTag)
BF2_CONSOLE_PROPERTY(projectorNear,            float,                   activeLightSource(), setProjectorNear,            getProjectorNear,            kObjTemplBaseConsoleTag)
BF2_CONSOLE_PROPERTY(projectorFar,             float,                   activeLightSource(), setProjectorFar,             getProjectorFar,             kObjTemplBaseConsoleTag)
BF2_CONSOLE_PROPERTY(projectorApplyMask,       bool,                    activeLightSource(), setProjectorApplyMask,       getProjectorApplyMask,       kObjTemplBaseConsoleTag)
BF2_CONSOLE_PROPERTY(enabled,                  bool,                    activeLightSource(), setEnabled,                  getEnabled,                  kObjTemplBaseConsoleTag)
BF2_CONSOLE_PROPERTY(fov,                      float,                   activeLightSource(), setFov,                      getFov,                      kObjTemplBaseConsoleTag)
BF2_CONSOLE_PROPERTY(intensity,                float,                   activeLightSource(), setIntensity,                getIntensity,                kObjTemplBaseConsoleTag)
BF2_CONSOLE_PROPERTY(shadowIntensity,          float,                   activeLightSource(), setShadowIntensity,          getShadowIntensity,          kObjTemplBaseConsoleTag)
BF2_CONSOLE_PROPERTY(softness,                 float,                   activeLightSource(), setSoftness,                 getSoftness,                 kObjTemplBaseConsoleTag)
BF2_CONSOLE_PROPERTY(objectShadows,            bool,                    activeLightSource(), setObjectShadows,            getObjectShadows,            kObjTemplBaseConsoleTag)
BF2_CONSOLE_PROPERTY(flicker,                  float,                   activeLightSource(), setFlicker,                  getFlicker,                  kObjTemplBaseConsoleTag)
BF2_CONSOLE_PROPERTY(scale,                    Vec3,                    activeLightSource(), setScale,                    getScale,                    kObjTemplBaseConsoleTag)

// IK: PE 4F5CF0..4F6740 append to this module's list (863F18), with the six
// executors 4F5E70/4F6080/4F6290/4F64A0/4F66B0/4F68E0. The component cursor
// narrows to IID 50381 (4F6840). Soldier animation consumes this template directly.
namespace {
IIkTemplateComponent* activeIk()
{
    return static_cast<IIkTemplateComponent*>(getActiveTemplateComponent(IID_IIkTemplateComponent));
}
}
BF2_CONSOLE_OBJECT("ObjectTemplate", 2, 0)
BF2_CONSOLE_PROPERTY(ik.positionLeft, Vec3, activeIk(), setPositionLeft, getPositionLeft, kObjTemplBaseConsoleTag)
BF2_CONSOLE_PROPERTY(ik.rotationLeft, Vec3, activeIk(), setRotationLeft, getRotationLeft, kObjTemplBaseConsoleTag)
BF2_CONSOLE_PROPERTY(ik.poleVectorLeft, Vec3, activeIk(), setPoleVectorLeft, getPoleVectorLeft, kObjTemplBaseConsoleTag)
BF2_CONSOLE_PROPERTY(ik.positionRight, Vec3, activeIk(), setPositionRight, getPositionRight, kObjTemplBaseConsoleTag)
BF2_CONSOLE_PROPERTY(ik.rotationRight, Vec3, activeIk(), setRotationRight, getRotationRight, kObjTemplBaseConsoleTag)
BF2_CONSOLE_PROPERTY(ik.poleVectorRight, Vec3, activeIk(), setPoleVectorRight, getPoleVectorRight, kObjTemplBaseConsoleTag)

// Handed to the console registry by GameLogicObjTemplBaseModule::init, the way the original
// registers 0x8AF17A4.
ConsoleObjectList& objTemplBaseConsoleObjects()
{
    return consoleBindingList(kObjTemplBaseConsoleTag);
}

} // namespace world
} // namespace hfe
} // namespace dice
