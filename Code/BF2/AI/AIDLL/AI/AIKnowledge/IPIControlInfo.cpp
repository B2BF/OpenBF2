// IPIControlInfo forwarders. Recovered from libaidll.so 1.5.3153 0x2ED380..0x2EDD50: every
// function is the same 0x30-byte shape,
//   mov 0x8(%ebp),%eax; mov 0x4(%eax),%eax; mov 0x20(%eax),%eax; call *0x88(vtbl);
//   [push the one reference argument]; call *slot(vtbl); [movzbl %al for the bool ones]
// so the return value (eax, edx:eax for the 64-bit masks, st0 for floats) passes through.

#include "IPIControlInfo.h"

#include "Information.h"

namespace dice {
namespace hfe {
namespace ai {

namespace {

// Information +0x20 (the owning AI object), then IAIObject +0x88.
inline IAIObjectControlInfo* controlInfoOf(Information* information)
{
    return information->getAIObject()->getControlInfo();
}

} // anonymous namespace

// ELF 0x2ED380: plug-in slot +0x18.
unsigned long long IPIControlInfo::getDriveControls()
{
    return controlInfoOf(m_information)->getDriveControls();
}

// ELF 0x2ED3B0: plug-in slot +0x24.
unsigned long long IPIControlInfo::getAimControls()
{
    return controlInfoOf(m_information)->getAimControls();
}

// ELF 0x2ED3E0: plug-in slot +0x30.
unsigned long long IPIControlInfo::getLookControls()
{
    return controlInfoOf(m_information)->getLookControls();
}

// ELF 0x2ED410: plug-in slot +0x3C.
float IPIControlInfo::getThrottleSensitivity()
{
    return controlInfoOf(m_information)->getThrottleSensitivity();
}

// ELF 0x2ED440: plug-in slot +0x40.
float IPIControlInfo::getPitchSensitivity()
{
    return controlInfoOf(m_information)->getPitchSensitivity();
}

// ELF 0x2ED470: plug-in slot +0x7C.
float IPIControlInfo::getLookHorizontalSensitivity()
{
    return controlInfoOf(m_information)->getLookHorizontalSensitivity();
}

// ELF 0x2ED4A0: plug-in slot +0x78.
float IPIControlInfo::getLookVerticalSensitivity()
{
    return controlInfoOf(m_information)->getLookVerticalSensitivity();
}

// ELF 0x2ED4D0: plug-in slot +0x44.
float IPIControlInfo::getRollSensitivity()
{
    return controlInfoOf(m_information)->getRollSensitivity();
}

// ELF 0x2ED500: plug-in slot +0x48.
float IPIControlInfo::getYawSensitivity()
{
    return controlInfoOf(m_information)->getYawSensitivity();
}

// ELF 0x2ED530: plug-in slot +0x4C.
float IPIControlInfo::getProportionalConstant()
{
    return controlInfoOf(m_information)->getProportionalConstant();
}

// ELF 0x2ED560: plug-in slot +0x50.
float IPIControlInfo::getDerivativeConstant()
{
    return controlInfoOf(m_information)->getDerivativeConstant();
}

// ELF 0x2ED590: plug-in slot +0x54.
float IPIControlInfo::getAngleSpeedConstant()
{
    return controlInfoOf(m_information)->getAngleSpeedConstant();
}

// ELF 0x2ED5C0: plug-in slot +0x58.
float IPIControlInfo::getThrottleLookAhead()
{
    return controlInfoOf(m_information)->getThrottleLookAhead();
}

// ELF 0x2ED5F0: plug-in slot +0x5C.
float IPIControlInfo::getPitchLookAhead()
{
    return controlInfoOf(m_information)->getPitchLookAhead();
}

// ELF 0x2ED620: plug-in slot +0x84.
float IPIControlInfo::getLookHorizontalLookAhead()
{
    return controlInfoOf(m_information)->getLookHorizontalLookAhead();
}

// ELF 0x2ED650: plug-in slot +0x80.
float IPIControlInfo::getLookVerticalLookAhead()
{
    return controlInfoOf(m_information)->getLookVerticalLookAhead();
}

// ELF 0x2ED680: plug-in slot +0x60.
float IPIControlInfo::getRollLookAhead()
{
    return controlInfoOf(m_information)->getRollLookAhead();
}

// ELF 0x2ED6B0: plug-in slot +0x64.
float IPIControlInfo::getYawLookAhead()
{
    return controlInfoOf(m_information)->getYawLookAhead();
}

// ELF 0x2ED6E0: plug-in slot +0x68.
float IPIControlInfo::getThrottleScale()
{
    return controlInfoOf(m_information)->getThrottleScale();
}

// ELF 0x2ED710: plug-in slot +0x6C.
float IPIControlInfo::getPitchScale()
{
    return controlInfoOf(m_information)->getPitchScale();
}

// ELF 0x2ED740: plug-in slot +0x8C.
float IPIControlInfo::getLookHorizontalScale()
{
    return controlInfoOf(m_information)->getLookHorizontalScale();
}

// ELF 0x2ED770: plug-in slot +0x88.
float IPIControlInfo::getLookVerticalScale()
{
    return controlInfoOf(m_information)->getLookVerticalScale();
}

// ELF 0x2ED7A0: plug-in slot +0x70.
float IPIControlInfo::getRollScale()
{
    return controlInfoOf(m_information)->getRollScale();
}

// ELF 0x2ED7D0: plug-in slot +0x74.
float IPIControlInfo::getYawScale()
{
    return controlInfoOf(m_information)->getYawScale();
}

// ELF 0x2ED800: plug-in slot +0x1C.
io::PlayerInputMap IPIControlInfo::getDriveThrottleControl()
{
    return controlInfoOf(m_information)->getDriveThrottleControl();
}

// ELF 0x2ED830: plug-in slot +0x20.
io::PlayerInputMap IPIControlInfo::getDriveTurnControl()
{
    return controlInfoOf(m_information)->getDriveTurnControl();
}

// ELF 0x2ED860: plug-in slot +0x28.
io::PlayerInputMap IPIControlInfo::getAimVerticalControl()
{
    return controlInfoOf(m_information)->getAimVerticalControl();
}

// ELF 0x2ED890: plug-in slot +0x2C.
io::PlayerInputMap IPIControlInfo::getAimHorizontalControl()
{
    return controlInfoOf(m_information)->getAimHorizontalControl();
}

// ELF 0x2ED8C0: plug-in slot +0x34.
io::PlayerInputMap IPIControlInfo::getLookVerticalControl()
{
    return controlInfoOf(m_information)->getLookVerticalControl();
}

// ELF 0x2ED8F0: plug-in slot +0x38.
io::PlayerInputMap IPIControlInfo::getLookHorizontalControl()
{
    return controlInfoOf(m_information)->getLookHorizontalControl();
}

// ELF 0x2ED920: plug-in slot +0x90.
float IPIControlInfo::getMaxSpeedBumpinessFallOff()
{
    return controlInfoOf(m_information)->getMaxSpeedBumpinessFallOff();
}

// ELF 0x2ED950: plug-in slot +0x94.
float IPIControlInfo::getCameraRelativeDofRotationOffset() const
{
    return controlInfoOf(m_information)->getCameraRelativeDofRotationOffset();
}

// ELF 0x2ED980: plug-in slot +0x98.
const Vec3& IPIControlInfo::getCameraRelativeMinRotation() const
{
    return controlInfoOf(m_information)->getCameraRelativeMinRotation();
}

// ELF 0x2ED9B0: plug-in slot +0x9C.
const Vec3& IPIControlInfo::getCameraRelativeMaxRotation() const
{
    return controlInfoOf(m_information)->getCameraRelativeMaxRotation();
}

// ELF 0x2ED9E0: plug-in slot +0xA0.
void IPIControlInfo::getCameraRelativeRotation(Vec3& out) const
{
    controlInfoOf(m_information)->getCameraRelativeRotation(out);
}

// ELF 0x2EDA10: plug-in slot +0xA4.
const Mat4& IPIControlInfo::getCameraTransformation() const
{
    return controlInfoOf(m_information)->getCameraTransformation();
}

// ELF 0x2EDA40: plug-in slot +0xA8.
void IPIControlInfo::getCameraBaseTransformation(Mat4& out) const
{
    controlInfoOf(m_information)->getCameraBaseTransformation(out);
}

// ELF 0x2EDA70: plug-in slot +0xAC.
void IPIControlInfo::getCameraBaseRight(Vec3& out) const
{
    controlInfoOf(m_information)->getCameraBaseRight(out);
}

// ELF 0x2EDAA0: plug-in slot +0xB0.
void IPIControlInfo::getCameraBaseUp(Vec3& out) const
{
    controlInfoOf(m_information)->getCameraBaseUp(out);
}

// ELF 0x2EDAD0: plug-in slot +0xB4.
void IPIControlInfo::getCameraBaseDof(Vec3& out) const
{
    controlInfoOf(m_information)->getCameraBaseDof(out);
}

// ELF 0x2EDB00: plug-in slot +0xB8.
void IPIControlInfo::getCameraBasePos(Vec3& out) const
{
    controlInfoOf(m_information)->getCameraBasePos(out);
}

// ELF 0x2EDB30: plug-in slot +0xBC.
bool IPIControlInfo::validateCameraDirection(const Vec3& direction) const
{
    return controlInfoOf(m_information)->validateCameraDirection(direction);
}

// ELF 0x2EDB70: plug-in slot +0xC0.
bool IPIControlInfo::validateCameraDirectionYaw(const Vec3& direction) const
{
    return controlInfoOf(m_information)->validateCameraDirectionYaw(direction);
}

// ELF 0x2EDBB0: plug-in slot +0xC4.
bool IPIControlInfo::validateCameraDirectionPitch(const Vec3& direction) const
{
    return controlInfoOf(m_information)->validateCameraDirectionPitch(direction);
}

// ELF 0x2EDBF0: plug-in slot +0xC8.
bool IPIControlInfo::validateCameraDirectionByPos(const Vec3& position) const
{
    return controlInfoOf(m_information)->validateCameraDirectionByPos(position);
}

// ELF 0x2EDC30: plug-in slot +0xCC.
float IPIControlInfo::getMaxClimbAngle()
{
    return controlInfoOf(m_information)->getMaxClimbAngle();
}

// ELF 0x2EDC60: plug-in slot +0xD0.
float IPIControlInfo::getMaxRollAngle()
{
    return controlInfoOf(m_information)->getMaxRollAngle();
}

// ELF 0x2EDC90: plug-in slot +0xD4.
io::PlayerInputMap IPIControlInfo::getDriveRollControl()
{
    return controlInfoOf(m_information)->getDriveRollControl();
}

// ELF 0x2EDCC0: plug-in slot +0xD8.
io::PlayerInputMap IPIControlInfo::getDrivePitchControl()
{
    return controlInfoOf(m_information)->getDrivePitchControl();
}

// ELF 0x2EDCF0: plug-in slot +0xDC.
io::PlayerInputMap IPIControlInfo::getAimThrottleControl()
{
    return controlInfoOf(m_information)->getAimThrottleControl();
}

// ELF 0x2EDD20: plug-in slot +0xE0.
io::PlayerInputMap IPIControlInfo::getAimRollControl()
{
    return controlInfoOf(m_information)->getAimRollControl();
}

} // namespace ai
} // namespace hfe
} // namespace dice
