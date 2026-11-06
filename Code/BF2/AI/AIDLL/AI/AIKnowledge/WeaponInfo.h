// Weapon information record borrowing an engine IWeapon. Tracks a rearm accumulator.
// [INFERRED] Source placement.
#ifndef AI_AIDLL_AI_AIKNOWLEDGE_WEAPONINFO_H
#define AI_AIDLL_AI_AIKNOWLEDGE_WEAPONINFO_H

#include "../../../AIInterface/BFKnowledge/IWeapon.h"

namespace dice { namespace hfe { namespace ai {

class IPIArmament;

class WeaponInfo
{
public:
    explicit WeaponInfo(IWeapon* weapon);     // ELF / ;
    virtual ~WeaponInfo();                    // ELF ;

    virtual bool   update();                              // +2, true
    virtual int    getShotsInMag(int magazine) = 0;       // +3
    virtual int    getAmmo() const = 0;                   // +4
    virtual int    getCurrentAmmo() const = 0;            // +5
    virtual int    getNumberOfMags() const = 0;           // +6
    virtual bool   isReadyToFire() const = 0;             // +7
    virtual double getTimeOfFiring() const = 0;           // +8
    virtual float  getHeat() const = 0;                   // +9
    virtual bool   usesHeat() const = 0;                  // +10; base has a legal definition
    virtual float  getSecurity() const = 0;               // +11
    virtual float  getSecurity(double time) const = 0;    // +12

    int getFireRate() const;
    float getReloadTime() const;
    const Mat4& getAbsoluteTransformation() const;
    const Vec3& getPosition() const;
    const Mat4& getProjectileOrigin();
    bool hasAutoReload() const;
    bool hasHeat() const;
    int getWeaponType() const;
    int getWeaponGuidanceType() const;
    bool getBurst() const;
    bool getIndirect() const;
    int getProjectileMaterial() const;
    int getMaxAmmoInMag() const;
    float getDrag() const;
    float getGravityModifier() const;
    float getExitVelocity() const;
    float getMinRange() const;
    float getMaxRange() const;
    float getOptimalRangePercentage() const;
    bool getFiresThroughTransparent() const;
    SoldierPose getFiringPose() const;
    float getAllowedDeviation() const;
    float getExplosionRadius();
    const int* getStrengths() const;
    int getStrength(int index) const;
    int getWeaponActivate() const;
    int getWeaponFire() const;
    int getWeaponZoom() const;
    int getWeaponAltFire() const;
    bool isZoomed() const;
    int getWeaponState() const;
    int getMaxAmmo() const;
    bool getIsThrown() const;
    bool useAimerOnly() const;
    float getSoundSphereRadius() const;
    bool setFakeFire(bool fake) const;
    float getReplenishRadius() const;
    void setBotSkill(float skill, double since, float scale);
    bool setTarget(unsigned int targetId);
    bool hasTargetLocked() const;
    IAIObject* getLockedTarget() const;
    const std::string& getName() const;

public:
    // [CHOSEN, not recovered] added by ai-w1-e-fire (coord/ai-requests.md): the image reads +0x04
    // inline (BBPFireInfantery::createPlan passes it to IAIPlayer::getNumShotsFired and
    // IAIObjectEnvironment::hasIndirectLineOfFire).
    IWeapon* getWeapon() const { return m_weapon; }

protected:
    friend class IPIArmament;                 // retail IPIArmament::rearm reads both fields directly
    /* +0x04 */ IWeapon* m_weapon;
    /* +0x08 */ float    m_rearmAmount;
};

} } }
#endif
