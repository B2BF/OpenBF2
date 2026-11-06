// Handles collisions in the AI module.
// Preserve virtual declaration order. Parameter types are attested by the libaidll.so mangled
// names. [INFERRED] the int return types: the shipped bodies of handleIndirectProjectileHit,
// handleProjectileMiss, shooterUpdate and handleProjectileTimeout set no return value and
// every engine caller discards it; victimUpdate answers 0/1 and BFCollisionHandler tests it.
// [INFERRED] Source placement.

#ifndef AI_AIDLL_AI_MAIN_AICOLLISIONHANDLER_H
#define AI_AIDLL_AI_MAIN_AICOLLISIONHANDLER_H

#include <string>
#include <vector>
#include <list>
#include <set>
#include <map>

#include "AIDllTypes.h"

namespace dice {
namespace hfe {
namespace ai {

// Collision-handler interface. Registration guards the roster static, which may remain
// stale until updateStaticMethods refreshes it.
class IAICollisionHandler
{
public:
    IAICollisionHandler();
    virtual ~IAICollisionHandler();
    virtual void handleProjectileFire(const Vec3&, world::IObject *, const Mat4&, int, IAIObject *, int, int) = 0;
    virtual int handleIndirectProjectileHit(int, IAIObject *, int, const Vec3&, int) = 0;
    virtual int handleIndirectProjectileHit(int, IAIObject *, const Vec3&, int) = 0;
    virtual int handleProjectileDamage(IAIObject *, IAIObject *, const int&, int) = 0;
    virtual int handleProjectileMiss(IAIObject *, int, int, const Vec3&) = 0;
    virtual int victimUpdate(int, int, const Vec3&, IAIObject *, IAIObject *) = 0;
    virtual int shooterUpdate(int, int, world::IObject *, IAIObject *, const Vec3&) = 0;
    virtual int handleProjectileTimeout(int, int, const Vec3&, bool) = 0;
};

// AICollisionHandler - sizeof == 4, from BF2AI::init step 10.
class AICollisionHandler : public IAICollisionHandler
{
public:
    AICollisionHandler();
    virtual ~AICollisionHandler();

    // IAICollisionHandler
    virtual void handleProjectileFire(const Vec3&, world::IObject *, const Mat4&, int, IAIObject *, int, int);
    virtual int handleIndirectProjectileHit(int, IAIObject *, int, const Vec3&, int);
    virtual int handleIndirectProjectileHit(int, IAIObject *, const Vec3&, int);
    virtual int handleProjectileDamage(IAIObject *, IAIObject *, const int&, int);
    virtual int handleProjectileMiss(IAIObject *, int, int, const Vec3&);
    virtual int victimUpdate(int, int, const Vec3&, IAIObject *, IAIObject *);
    virtual int shooterUpdate(int, int, world::IObject *, IAIObject *, const Vec3&);
    virtual int handleProjectileTimeout(int, int, const Vec3&, bool);

    // Non-virtual helpers (mangled names in libaidll.so; [INFERRED] access and return spelling:
    // checkIfGrenade and updateBotsOnProjectile set no return value, isC4 materialises a bool).
    void checkIfGrenade(Bot* bot, IAIObject* projectile, const Vec3& position);   // 0x382D00
    bool isC4(IAIObject* object);                                                 // 0x382FE0
    void updateBotsOnProjectile(IAIObject* projectile, const Vec3& position);     // 0x3830B0
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_MAIN_AICOLLISIONHANDLER_H
