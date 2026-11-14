// Camera configuration with independent view-mode flags and separate interior lighting and field of view.
// Color fields contain RGB only. Console bindings parse Vec3, so their metadata type name differs from retail.
// [INFERRED] Source placement; native STL layout differs from the original ELF layout.

#ifndef GAME_OBJECTS_CAMERATEMPLATE_H     // [INFERRED] guard style
#define GAME_OBJECTS_CAMERATEMPLATE_H

#include "RotationalBundleTemplate.h"
#include "../../Math/Vec3.h"

namespace dice {
namespace hfe {
namespace world {

class CameraTemplate : public RotationalBundleTemplate
{
    friend struct BaseTemplateConsoleAccess; // retail console's direct cameraId field access
public:
    // Initializes the represented camera defaults. Additional unnamed fields remain outside this model.
    CameraTemplate();

    // Creates the camera object and exposes its template identity.
    virtual unsigned int getClassID() const;              // +0x10
    virtual IObject* createObject() const;                // +0x54
    virtual void* queryImplementation(unsigned int cid);  // +0x50

    // Typed access to fields consumed directly by Camera.
    int  getDefaultPcoId() const     { return m_pcoId; }        // +532
    int  getDefaultCameraId() const  { return m_cameraId; }     // +540
    void setHasTarget(bool value) { m_hasTarget = value; }
    bool getHasTarget() const { return m_hasTarget; }

    bool getToggleMouseLook() const  { return m_toggleMouseLook; }   // +564
    // The console's `toggleMouseLook` (ConsoleClass277, base 0x8AF6500) stores +564 raw; this is
    // the same protected-boundary bridge as the getter beside it, with the binding.
    void setToggleMouseLook(bool value) { m_toggleMouseLook = value; }

    // Interior lighting gates used by Camera.
    bool getAllowInsideDynamicLights() const { return m_allowInsideDynamicLights; }   // +740
    bool getAllowInsideStaticSun() const     { return m_allowInsideStaticSun; }       // +741

    // Interior lighting values consumed directly by Camera.
    const Math::Vec3& getInsideStaticSunDirection() const    { return m_insideStaticSunDirection; }     // +744
    const Math::Vec3& getInsideStaticSunColor() const        { return m_insideStaticSunColor; }         // +756
    const Math::Vec3& getInsideStaticSunDamageColor() const  { return m_insideStaticSunDamageColor; }   // +768
    float             getInsideDamageFrequency() const       { return m_insideDamageFrequency; }        // +780
    float             getInsideDamageThreshold() const       { return m_insideDamageThreshold; }        // +784
    const Math::Vec3& getInsideStaticSunAmbientColor() const { return m_insideStaticSunAmbientColor; }  // +788

    // also NOT functions IN THE ELF - Camera::setViewMode tests these seven flags directly
    // (the raw reads `cmp byte ptr [edx+221h], 0` for CVMChase, and so on across 544..550).
    bool getCVMInside() const        { return m_CVMInside; }        // +544
    bool getCVMChase() const         { return m_CVMChase; }         // +545
    bool getCVMFlyBy() const         { return m_CVMFlyBy; }         // +546
    bool getCVMTrace() const         { return m_CVMTrace; }         // +547
    bool getCVMExternTrace() const   { return m_CVMExternTrace; }   // +548
    bool getCVMFrontChase() const    { return m_CVMFrontChase; }    // +549
    bool getCVMNose() const          { return m_CVMNose; }          // +550

    // Camera's recovered nose/chase transformation arms read these fields directly in both
    // shipped images.  Accessors are this tree's typed bridge across the protected boundary;
    // they are not additional shipped virtuals.
    const Math::Vec3& getNosePos() const      { return m_nosePos; }         // +552
    float getFollowStiffness() const          { return m_followStiffness; } // +568
    void setFollowStiffness(float value)      { m_followStiffness = value; }
    float getChaseDistance() const            { return m_chaseDistance; }  // +572
    float getChaseAngle() const               { return m_chaseAngle; }     // +576
    const Math::Vec3& getChaseOffset() const   { return m_chaseOffset; }    // +580

    // Also read directly by Camera::changeSubGeometry. -1 in either means "defer to the
    // PCO's own value"; anything else overrides it.
    int  getCockpitLod() const       { return m_cockpitLod; }       // +800
    int  getCockpitSubGeom() const   { return m_cockpitSubGeom; }   // +804

    // Read directly by Camera::getActiveFOV, which picks between them on the view mode.
    float getWorldFOV() const        { return m_worldFOV; }         // +732
    float getInsideFOV() const       { return m_insideFOV; }        // +736

    // NOT A shipped function. Camera::handleExplosion reads template +704 directly in both
    // ports (ELF +0x2C0 / PE +0x370); this accessor is only the typed C++ home for that proven read.
    float getCameraShakeSensitivity() const { return m_cameraShakeSensitivity; }
    // Likewise for Camera::updateCameraShake's direct reads of +708 / +712 (ELF 0x81DC291, 0x81DC4BF).
    float getCameraShakeStartSpeed() const { return m_cameraShakeStartSpeed; }
    float getCameraShakeMaxSpeed() const { return m_cameraShakeMaxSpeed; }

protected:
    // layout, offsets 532..807. RotationalBundleTemplate occupies 0..531.
    /* 532 */ int   m_pcoId;                              // was `_dword214` [UNRESOLVED]
    /* 536 */ bool  m_hasTarget;                          // console: setHasTarget
    /* 537 */ char  _pad219[3];
    /* 540 */ int   m_cameraId;

    // the six supported view modes - independent flags, not an enum
    /* 544 */ bool  m_CVMInside;
    /* 545 */ bool  m_CVMChase;
    /* 546 */ bool  m_CVMFlyBy;
    /* 547 */ bool  m_CVMTrace;
    /* 548 */ bool  m_CVMExternTrace;
    /* 549 */ bool  m_CVMFrontChase;
    /* 550 */ bool  m_CVMNose;                            // was `_byte226` - completes the seven
                                                          // Inside, chase, fly-by and front-chase default on; trace, external trace and nose default off.
    /* 551 */ char  _pad227[1];

    /* 552 */ Math::Vec3 m_nosePos;                       // 552,556,560
    /* 564 */ bool  m_toggleMouseLook;
    /* 565 */ char  _pad235[3];

    // chase camera
    /* 568 */ float m_followStiffness;
    /* 572 */ float m_chaseDistance;
    /* 576 */ float m_chaseAngle;
    /* 580 */ Math::Vec3 m_chaseOffset;                   // 580,584,588

    // Scalar camera-sway settings.
    /* 592 */ float m_cameraSwayRodLength;
    /* 596 */ float m_cameraSwaySensitivity;
    /* 600 */ float m_cameraSwayDofMax;                   // type read (fstp), not inferred
    /* 604 */ float m_cameraSwayDofMin;                   // type read (fstp)
    /* 608 */ float m_cameraSwayRightMax;                 // type read (fstp)
    /* 612 */ float m_cameraSwayRightMin;                 // type read (fstp)
    /* 616 */ float m_cameraSwaySpringStrength;
    /* 620 */ float m_cameraSwaySpringDamping;
    // [INFERRED] float on the four without an fstp - every named neighbour is a float and all eight
    // are one parameter group. [UNRESOLVED] 624..703 - no console property writes that range, so
    // this route cannot see it.
    /* 624 */ char  _pad270[80];                          // [UNRESOLVED] 624..703

    // shake and bounce
    /* 704 */ float m_cameraShakeSensitivity;
    /* 708 */ float m_cameraShakeStartSpeed;
    /* 712 */ float m_cameraShakeMaxSpeed;
    /* 716 */ float m_bounceLength;
    /* 720 */ float m_bounceSensitivity;
    /* 724 */ float m_bounceStrength;
    /* 728 */ float m_bounceDamping;

public:
    // Console properties access these FOV fields directly.
    /* 732 */ float m_worldFOV;
    /* 736 */ float m_insideFOV;

protected:
    // the separate inside lighting environment
    /* 740 */ bool  m_allowInsideDynamicLights;
    /* 741 */ bool  m_allowInsideStaticSun;
    /* 742 */ bool  _byte2E6;                             // [UNRESOLVED]
    /* 743 */ bool  _byte2E7;

    /* 744 */ Math::Vec3 m_insideStaticSunDirection;      // 744,748,752
    /* 756 */ Math::Vec3 m_insideStaticSunColor;          // 756,760,764  (console: +intensity)
    /* 768 */ Math::Vec3 m_insideStaticSunDamageColor;    // 768,772,776  blended in on damage

    /* 780 */ float m_insideDamageFrequency;
    /* 784 */ float m_insideDamageThreshold;

    /* 788 */ Math::Vec3 m_insideStaticSunAmbientColor;   // 788,792,796

    /* 800 */ int   m_cockpitLod;
    /* 804 */ int   m_cockpitSubGeom;
};

} // namespace world
} // namespace hfe
} // namespace dice

#endif // GAME_OBJECTS_CAMERATEMPLATE_H
