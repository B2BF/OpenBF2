// Soldier-pose chooser shared by pose and move actions.

#ifndef AI_AIDLL_AI_AIBOTACTIONPLAN_BAPACOMPONENTSOLDIERPOSE_H
#define AI_AIDLL_AI_AIBOTACTIONPLAN_BAPACOMPONENTSOLDIERPOSE_H

#include "../../../../Math/Vec3.h"
#include "../../../AIInterface/BFKnowledge/SoldierPose.h"

namespace dice {
namespace hfe {

namespace ai {

class Bot;

// ELF: vtable 0x4B7AE0 (16 words), package C-actions.
// Soldier-pose chooser mixed into BAPAMoveTo and BAPASoldierPose (secondary base, size 0x2C).
// Setters return nothing (no eax write); the float getters return through x87.
class BAPAComponentSoldierPose
{
public:
    BAPAComponentSoldierPose(SoldierPose normal, SoldierPose incommingFire, float fireObjectsThreshold,
                             SoldierPose danger, float controlThreshold, unsigned int object,
                             float timeDuration);   // 0x1D1620
    virtual ~BAPAComponentSoldierPose();   // 0x1D1760

    virtual void setNormalPose(SoldierPose);   // 0x1D17C0, slot +0x08
    virtual void setIncommingFirePose(SoldierPose);   // 0x1D17D0, slot +0x0C
    virtual void setFireObjectsThreshold(float);   // 0x1D17E0, slot +0x10
    virtual void setDangerPose(SoldierPose);   // 0x1D17F0, slot +0x14
    virtual void setControlThreshold(float);   // 0x1D1800, slot +0x18
    virtual void setTimeDuration(float);   // 0x1D1810, slot +0x1C
    virtual SoldierPose getNormalPose();   // 0x1D1820, slot +0x20
    virtual SoldierPose getIncommingFirePose() const;   // 0x1D18E0, slot +0x24
    virtual float getFireObjectsThreshold() const;   // 0x1D18F0, slot +0x28
    virtual SoldierPose getDangerPose() const;   // 0x1D1900, slot +0x2C
    virtual float getControlThreshold() const;   // 0x1D1910, slot +0x30
    virtual float getTimeDuration() const;   // 0x1D1920, slot +0x34
    virtual SoldierPose getCurrentPose() const;   // 0x1D1930, slot +0x38
    virtual SoldierPose computeCurrentPose(float incommingFire, float control, double time, Bot*,
                                           const Vec3& position);   // 0x1D1940, slot +0x3C

private:
    // True when m_object is a soldier-type information (flags bit 21) whose player has a soldier
    // that is not standing: the inline test getNormalPose and computeCurrentPose both spell out.
    bool objectForcesCrouch() const;

    /* +0x04 */ SoldierPose m_normalPose;
    /* +0x08 */ SoldierPose m_incommingFirePose;
    /* +0x0C */ float m_fireObjectsThreshold;   // randomised: t + rnd * t * 5
    /* +0x10 */ SoldierPose m_dangerPose;
    /* +0x14 */ SoldierPose m_currentPose;      // = normal
    /* +0x18 */ float m_controlThreshold;       // clamped to [-1, 1]
    /* +0x1C */ float m_timeDuration;
    /* +0x20 */ double m_lastChangeTime;        // = -FLT_MAX
    /* +0x28 */ unsigned int m_object;          // information handle, -1 = none

};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTACTIONPLAN_BAPACOMPONENTSOLDIERPOSE_H
