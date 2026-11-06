
#include "WeaponInfo.h"

// <iostream>'s ios_base::Init object is this TU's static initialisation and teardown (ELF
// 0x2DD370, __tcf_0 0x2DD3D0, _GLOBAL__I 0x2DD400).
#include <iostream>

namespace dice { namespace hfe { namespace ai {

WeaponInfo::WeaponInfo(IWeapon* weapon)
    : m_weapon(weapon), m_rearmAmount(0.0f)
{
}

WeaponInfo::~WeaponInfo()
{
}

bool WeaponInfo::update()
{
    return true;
}

int WeaponInfo::getFireRate() const { return m_weapon->getFireRate(); }
float WeaponInfo::getReloadTime() const { return m_weapon->getReloadTime(); }
const Mat4& WeaponInfo::getAbsoluteTransformation() const { return m_weapon->getAbsoluteTransformation(); }
const Vec3& WeaponInfo::getPosition() const { return m_weapon->getPosition(); }
const Mat4& WeaponInfo::getProjectileOrigin() { return m_weapon->getProjectileOrigin(); }
bool WeaponInfo::hasAutoReload() const { return m_weapon->hasAutoReload(); }
bool WeaponInfo::hasHeat() const { return m_weapon->hasHeat(); }
bool WeaponInfo::usesHeat() const { return m_weapon->usesHeat(); }
int WeaponInfo::getWeaponType() const { return m_weapon->getWeaponType(); }
int WeaponInfo::getWeaponGuidanceType() const { return m_weapon->getWeaponGuidingSystem(); }
bool WeaponInfo::getBurst() const { return m_weapon->getBurst(); }
bool WeaponInfo::getIndirect() const { return m_weapon->getIndirect(); }
int WeaponInfo::getProjectileMaterial() const { return m_weapon->getProjectileMaterial(); }
int WeaponInfo::getMaxAmmoInMag() const { return m_weapon->getMaxAmmoInMag(); }
float WeaponInfo::getDrag() const { return m_weapon->getDrag(); }
float WeaponInfo::getGravityModifier() const { return m_weapon->getGravityModifier(); }
float WeaponInfo::getExitVelocity() const { return m_weapon->getExitVelocity(); }
float WeaponInfo::getMinRange() const { return m_weapon->getMinRange(); }
float WeaponInfo::getMaxRange() const { return m_weapon->getMaxRange(); }
float WeaponInfo::getOptimalRangePercentage() const { return m_weapon->getOptimalRangePercentage(); }
bool WeaponInfo::getFiresThroughTransparent() const { return m_weapon->getFiresThroughTransparent(); }
SoldierPose WeaponInfo::getFiringPose() const { return m_weapon->getFiringPose(); }
float WeaponInfo::getAllowedDeviation() const { return m_weapon->getAllowedDeviation(); }
float WeaponInfo::getExplosionRadius() { return m_weapon->getExplosionRadius(); }
const int* WeaponInfo::getStrengths() const { return m_weapon->getStrengths(); }
int WeaponInfo::getStrength(int index) const { return m_weapon->getStrength(index); }
int WeaponInfo::getWeaponActivate() const { return m_weapon->getWeaponActivate(); }
int WeaponInfo::getWeaponFire() const { return m_weapon->getWeaponFire(); }
int WeaponInfo::getWeaponZoom() const { return m_weapon->getWeaponZoom(); }
int WeaponInfo::getWeaponAltFire() const { return m_weapon->getWeaponAltFire(); }
bool WeaponInfo::isZoomed() const { return m_weapon->isZoomed(); }
int WeaponInfo::getWeaponState() const { return m_weapon->getWeaponState(); }
int WeaponInfo::getMaxAmmo() const { return m_weapon->getMaxAmmo(); }
bool WeaponInfo::getIsThrown() const { return m_weapon->getIsThrown(); }
bool WeaponInfo::useAimerOnly() const { return m_weapon->useAimerOnly(); }
float WeaponInfo::getSoundSphereRadius() const { return m_weapon->getSoundSphereRadius(); }
bool WeaponInfo::setFakeFire(bool fake) const { return m_weapon->setFakeFire(fake); }
float WeaponInfo::getReplenishRadius() const { return m_weapon->getReplenishRadius(); }
void WeaponInfo::setBotSkill(float skill, double since, float scale) { m_weapon->setBotSkill(skill, since, scale); }
bool WeaponInfo::setTarget(unsigned int targetId) { return m_weapon->setTarget(targetId); }
bool WeaponInfo::hasTargetLocked() const { return m_weapon->hasTargetLocked(); }
IAIObject* WeaponInfo::getLockedTarget() const { return m_weapon->getLockedTarget(); }
const std::string& WeaponInfo::getName() const { return m_weapon->getName(); }

} } }
