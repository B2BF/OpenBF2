// Control information plug-in: the AI-side view of an object's control template (input maps,
// regulator constants, camera limits). Every accessor reads through to the engine object.
// ELF: vtable 0x4B7450; the 51 non-virtual forwarders 0x2ED380..0x2EDD50 sit between
// IPIArmamentDummy.cpp and IPICover.cpp (no FILE symbol: no static initialiser), hence
// IPIControlInfo.cpp.
#ifndef AI_AIDLL_AI_AIKNOWLEDGE_IPICONTROLINFO_H
#define AI_AIDLL_AI_AIKNOWLEDGE_IPICONTROLINFO_H

#include "InformationPlugIn.h"
#include "Information.h"                                     // callers rely on it (E, mac-metal bd97c30)
#include "../../../AIInterface/BFKnowledge/IAIObjectPlugIn.h"   // io::PlayerInputMap, Vec3, Mat4

namespace dice { namespace hfe { namespace ai {

class IPIControlInfo : public InformationPlugIn
{
public:
    explicit IPIControlInfo(Information* information) { m_information = information; }

    // only the base's vptr store - nothing of its own.
    virtual ~IPIControlInfo() {}
    // 5 - Information::removePlugIn's case for the control-info plug-in.
    virtual int getType() { return 5; }
    // empty - the control values are read
    // through to the engine object on every call, so there is nothing to refresh.
    virtual void setTime() {}
    virtual void setTime(double /*time*/) {}

    // Non-virtual forwarders, out of line in the image. Each body is
    // `m_information(+4)->m_object(+0x20)->getControlInfo()(IAIObject +0x88)->slot()`, the
    // arguments passed through unchanged (IAIObjectControlInfo slot named on the right).
    // Constness follows the mangled names: only the camera block is const.
    unsigned long long getDriveControls();                              // ELF 0x2ED380 -> slot +0x18
    unsigned long long getAimControls();                                // ELF 0x2ED3B0 -> slot +0x24
    unsigned long long getLookControls();                               // ELF 0x2ED3E0 -> slot +0x30
    float getThrottleSensitivity();                                     // ELF 0x2ED410 -> slot +0x3C
    float getPitchSensitivity();                                        // ELF 0x2ED440 -> slot +0x40
    float getLookHorizontalSensitivity();                               // ELF 0x2ED470 -> slot +0x7C
    float getLookVerticalSensitivity();                                 // ELF 0x2ED4A0 -> slot +0x78
    float getRollSensitivity();                                         // ELF 0x2ED4D0 -> slot +0x44
    float getYawSensitivity();                                          // ELF 0x2ED500 -> slot +0x48
    float getProportionalConstant();                                    // ELF 0x2ED530 -> slot +0x4C
    float getDerivativeConstant();                                      // ELF 0x2ED560 -> slot +0x50
    float getAngleSpeedConstant();                                      // ELF 0x2ED590 -> slot +0x54
    float getThrottleLookAhead();                                       // ELF 0x2ED5C0 -> slot +0x58
    float getPitchLookAhead();                                          // ELF 0x2ED5F0 -> slot +0x5C
    float getLookHorizontalLookAhead();                                 // ELF 0x2ED620 -> slot +0x84
    float getLookVerticalLookAhead();                                   // ELF 0x2ED650 -> slot +0x80
    float getRollLookAhead();                                           // ELF 0x2ED680 -> slot +0x60
    float getYawLookAhead();                                            // ELF 0x2ED6B0 -> slot +0x64
    float getThrottleScale();                                           // ELF 0x2ED6E0 -> slot +0x68
    float getPitchScale();                                              // ELF 0x2ED710 -> slot +0x6C
    float getLookHorizontalScale();                                     // ELF 0x2ED740 -> slot +0x8C
    float getLookVerticalScale();                                       // ELF 0x2ED770 -> slot +0x88
    float getRollScale();                                               // ELF 0x2ED7A0 -> slot +0x70
    float getYawScale();                                                // ELF 0x2ED7D0 -> slot +0x74
    io::PlayerInputMap getDriveThrottleControl();                       // ELF 0x2ED800 -> slot +0x1C
    io::PlayerInputMap getDriveTurnControl();                           // ELF 0x2ED830 -> slot +0x20
    io::PlayerInputMap getAimVerticalControl();                         // ELF 0x2ED860 -> slot +0x28
    io::PlayerInputMap getAimHorizontalControl();                       // ELF 0x2ED890 -> slot +0x2C
    io::PlayerInputMap getLookVerticalControl();                        // ELF 0x2ED8C0 -> slot +0x34
    io::PlayerInputMap getLookHorizontalControl();                      // ELF 0x2ED8F0 -> slot +0x38
    float getMaxSpeedBumpinessFallOff();                                // ELF 0x2ED920 -> slot +0x90
    float getCameraRelativeDofRotationOffset() const;                   // ELF 0x2ED950 -> slot +0x94
    const Vec3& getCameraRelativeMinRotation() const;                   // ELF 0x2ED980 -> slot +0x98
    const Vec3& getCameraRelativeMaxRotation() const;                   // ELF 0x2ED9B0 -> slot +0x9C
    void getCameraRelativeRotation(Vec3& out) const;                    // ELF 0x2ED9E0 -> slot +0xA0
    const Mat4& getCameraTransformation() const;                        // ELF 0x2EDA10 -> slot +0xA4
    void getCameraBaseTransformation(Mat4& out) const;                  // ELF 0x2EDA40 -> slot +0xA8
    void getCameraBaseRight(Vec3& out) const;                           // ELF 0x2EDA70 -> slot +0xAC
    void getCameraBaseUp(Vec3& out) const;                              // ELF 0x2EDAA0 -> slot +0xB0
    void getCameraBaseDof(Vec3& out) const;                             // ELF 0x2EDAD0 -> slot +0xB4
    void getCameraBasePos(Vec3& out) const;                             // ELF 0x2EDB00 -> slot +0xB8
    bool validateCameraDirection(const Vec3& direction) const;          // ELF 0x2EDB30 -> slot +0xBC
    bool validateCameraDirectionYaw(const Vec3& direction) const;       // ELF 0x2EDB70 -> slot +0xC0
    bool validateCameraDirectionPitch(const Vec3& direction) const;     // ELF 0x2EDBB0 -> slot +0xC4
    bool validateCameraDirectionByPos(const Vec3& position) const;      // ELF 0x2EDBF0 -> slot +0xC8
    float getMaxClimbAngle();                                           // ELF 0x2EDC30 -> slot +0xCC
    float getMaxRollAngle();                                            // ELF 0x2EDC60 -> slot +0xD0
    io::PlayerInputMap getDriveRollControl();                           // ELF 0x2EDC90 -> slot +0xD4
    io::PlayerInputMap getDrivePitchControl();                          // ELF 0x2EDCC0 -> slot +0xD8
    io::PlayerInputMap getAimThrottleControl();                         // ELF 0x2EDCF0 -> slot +0xDC
    io::PlayerInputMap getAimRollControl();                             // ELF 0x2EDD20 -> slot +0xE0
};

} } }
#endif
