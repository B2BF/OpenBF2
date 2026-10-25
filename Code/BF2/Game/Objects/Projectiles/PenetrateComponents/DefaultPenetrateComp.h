// Tests material/geometry penetration and updates damage, exit position and lifetime through output arguments.
// A successful penetration also moves the projectile to the exit point.
// Water may bypass geometry and damage cost. damageLoss -1 refuses penetration.
// Damage can be written before a later refusal; only the return value establishes penetration.
// Deviation uses a stream separate from ricochet (RW-68). Source placement is [INFERRED].

#ifndef GAME_OBJECTS_PROJECTILES_PENETRATECOMPONENTS_DEFAULTPENETRATECOMP_H
#define GAME_OBJECTS_PROJECTILES_PENETRATECOMPONENTS_DEFAULTPENETRATECOMP_H

#include "../GenericProjectile.h"      // IProjectileComp
#include "../../WeaponComp.h"          // ProjectileComp

namespace dice {
namespace hfe {
namespace world {

class DefaultPenetrateCompTemplate;
class IObject;

// The complete secondary face is recovered from ELF 0x88BF9DC / PE 0x7DCC9C. Both tables contain
// the IBase quartet, hasUpdate/updateComponent/activate, the ABI-specific destructor slot(s), then
// penetrate and nothing after it. GenericProjectile therefore dispatches through the typed face.
class DefaultPenetrateComp : public ProjectileComp, public IProjectilePenetrateComp
{
public:
    explicit DefaultPenetrateComp(DefaultPenetrateCompTemplate* objectTemplate);
    virtual ~DefaultPenetrateComp();

    virtual void  addRef();
    virtual int   getRef();
    virtual int   release();
    virtual void* queryInterface(unsigned int iid) const;
    virtual unsigned int getInterfaceID() const;

    virtual bool hasUpdate() const;
    virtual void updateComponent(float seconds, unsigned int flags);
    virtual void activate();

    // . Signature attested by the mangling; the Itanium substitution chain
    // resolves the tail as float&, Vec3&, float& (S8_ is `float&`, recorded from the earlier `Rf`).
    // _ZN...20DefaultPenetrateComp9penetrateEPNS1_7IObjectERKNS0_4Vec3EiifffRfRS5_S8_
    virtual bool penetrate(IObject* target, const Vec3& direction,
                           int a, int collisionType,
                           float damageScale, float minDamage, float timeScale,
                           float& outTime, Vec3& outPosition, float& outDamage);

private:
    /* +0x14 */ int m_refCount;
    /* +0x18 */ DefaultPenetrateCompTemplate* m_template;
};

} // namespace world
} // namespace hfe
} // namespace dice

#endif // GAME_OBJECTS_PROJECTILES_PENETRATECOMPONENTS_DEFAULTPENETRATECOMP_H
