// Script-defined AI weapon properties. init completes them from the first live firearm.
// The constructor leaves activation and fire inputs, zoom, weapon type and replenish radius unset.
// init replaces drag and weaponFire, and fills exitVelocity only if it is zero.
// [INFERRED] Source placement.

#ifndef AI_AIINTERFACE_BFKNOWLEDGE_WEAPONTEMPLATE_H
#define AI_AIINTERFACE_BFKNOWLEDGE_WEAPONTEMPLATE_H

#include "SoldierPose.h"

#include "../../../IO/Input/Input.h"            // io::PlayerInputMap

namespace dice {
namespace hfe {
namespace world { class IObject; }
namespace ai {

class IWeapon;

// ai::WeaponType - the name is the mangled parameter type of WeaponTemplate::init; the enumerator
// names 0..3 are [CHOSEN], from how WeaponObject::init derives each value: a replenishing ammo
// component gives 1 (ReplenishingType RTHeal) or 2 (RTAmmo), a projectile with a resurrect collision
// component gives 3, with a sticky collision component 4 (checked in that order, overriding the ammo
// answer), anything else 0. 4 is DICE's `WTC4`: AIDLL_r.dll (BoosterPack2 debug build) BBC4.cpp asserts
// "wInfo->getWeaponType() == WTC4" ("Expected C4 weapon type"), string at 0x1029E0AC.
// AIObjectArmament counts only type-0 weapons into a unit's battle strength, and
// WeaponObject::getReplenishRadius answers only for 1 and 2.
enum WeaponType
{
    WTDefault     = 0,
    WTHeal        = 1,
    WTAmmo        = 2,
    WTResurrect   = 3,
    WTC4          = 4
};

// ai::WeaponGuidingSystem - mangled name attested; enumerators [CHOSEN] after world::TargetSystem
// which WeaponObject::targetSystemToWeaponGuidingSystem maps one for one for 1..4 (TSParabellic, 5
// and anything else become 0).
enum WeaponGuidingSystem
{
    WGSNone        = 0,
    WGSHeatSeeking = 1,
    WGSLaserGuided = 2,
    WGSWireGuided  = 3,
    WGSTVGuided    = 4
};

class WeaponTemplate
{
public:
    // `strengthCount` is the manager's m_kind (6, the combat
    // strength types) - the size of the strengths array, allocated through checkPointer and zeroed.
    explicit WeaponTemplate(int strengthCount);
    ~WeaponTemplate();

    // a new ai::WeaponObject bound to this template. The object
    // argument is not used in either image. [INFERRED] the interface return type.
    IWeapon* createWeapon(world::IObject* object);

    bool getIsThrown() const;                           // (`_ZNK`)
    void setIsThrown(bool thrown);

    // Once, and never while precaching: the live firearm's numbers
    // (WeaponObject::init gathers them). [INFERRED bool] - false on the early-out, true after.
    bool init(float drag, float gravityModifier, float exitVelocity, int projectileMaterial,
              int maxAmmoInMag, int maxAmmo, io::PlayerInputMap weaponFire,
              io::PlayerInputMap weaponZoom, io::PlayerInputMap altFire, int itemIndex,
              WeaponType weaponType, float replenishRadius, WeaponGuidingSystem guidingSystem);
    // item index 1..9 -> PIWeaponSelect1..9, anything else PINone.
    void initActivate(int itemIndex);

    /* +0x00 */ bool  m_indirect;
    /* +0x01 */ bool  m_initialized;                    // [INFERRED name] init's once-flag
    /* +0x02 */ bool  m_isThrown;
    /* +0x03 */ bool  m_useAimerOnly;
    /* +0x04 */ int   m_projectileMaterial;             // init; ctor -1
    /* +0x08 */ int   m_maxAmmo;                        // init (the firearm's total ammunition)
    /* +0x0C */ int   m_maxAmmoInMag;                   // init (magazine 0's ammunition)
    /* +0x10 */ int   m_fireRate;
    /* +0x14 */ float m_drag;
    /* +0x18 */ float m_minRange;
    /* +0x1C */ float m_maxRange;
    /* +0x20 */ float m_explosionRadius;
    /* +0x24 */ float m_soundSphereRadius;
    /* +0x28 */ float m_exitVelocity;
    /* +0x2C */ float m_gravityModifier;                // init
    /* +0x30 */ float m_deviation;
    /* +0x34 */ float m_deviationCorrectionTime;
    /* +0x38 */ float m_deviationCorrectionRate;        // [INFERRED name] 1 / time, or 0 for 0
    /* +0x3C */ int*  m_strengths;                      // indexed by CombatStrengthType
    /* +0x40 */ SoldierPose m_firingPose;
    /* +0x44 */ float m_allowedDeviation;
    /* +0x48 */ float m_optimalRangePercentage;
    /* +0x4C */ bool  m_firesThroughTransparent;
    /* +0x50 */ io::PlayerInputMap m_weaponActivate;    // not initialised
    /* +0x54 */ io::PlayerInputMap m_weaponFire;        // not initialised
    /* +0x58 */ io::PlayerInputMap m_weaponZoom;        // init; not initialised
    /* +0x5C */ io::PlayerInputMap m_altFire;           // init; not initialised
    /* +0x60 */ WeaponType m_weaponType;                // init; not initialised
    /* +0x64 */ float m_replenishRadius;                // init; not initialised
    /* +0x68 */ WeaponGuidingSystem m_weaponGuidingSystem;  // init; ctor 0
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIINTERFACE_BFKNOWLEDGE_WEAPONTEMPLATE_H
