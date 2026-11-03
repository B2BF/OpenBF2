// Collision damage and effects. [INFERRED] source path.

#include "GameLogicCollision.h"
#include "ServerGameLogic.h"                       // resurrect - concrete server-only rule

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <float.h>
#include <string>

#include "../../Core/Ids.h"
#include "../../Core/IBase.h"
#include "../Utility/Vars.h"
#include "../IGameLogic.h"                                // gameLogic->giveDamage (+0xE4)
#include "../Objects/IObject.h"
#include "../Objects/IObjectTemplate.h"
#include "../Objects/IPlayer.h"
#include "../Objects/ObjectLayoutAccess.h"                // +0x3C armor, +0x44 node
#include "../Objects/Base/SimpleObjectTemplate.h"         // m_mass (+92) - getMassMod is a friend
#include "../Objects/GameLogicComp/IArmorGLComp.h"        // getAngleMod (+0x78), getSpeedMod (+0x8C)
#include "../../Physics/IPhysicsNode.h"                   // getPositionalSpeed (+0x60), getIsMobile (+0x134)
#include "../../Physics/MaterialManager.h"                // getMaterialPtr / getDamageMod / getMaterialById
#include "../../Physics/Material.h"                       // getHasWaterPhysics / getIsBarbwire / getName
#include "../../Scene/Object/ObjectTypes/Game/IPlayerControlObject.h"
#include "../GameLogic/Player.h"                          // dice::hfe::getPlayer (the identity)
#include "../../Core/StringUtil.h"                        // formatString
#include <list>                                           // giveDamage's assist-player list
#include "../../Core/WeakPtr.h"
#include "../Objects/ISoldier.h"                          // the man-down retraction's getOccupingPlayer
#include "../Objects/Projectiles/GenericProjectile.h"     // the sole IProjectileObject implementor
#include "../../Core/System.h"                            // WorldPref::getTickCount
#include "../../AI/AIInterface/BFKnowledge/BFCollisionHandler.h"   // the AI collision courier
#include "../../AI/AIDLL/AI/Main/CommonSingletonServer.h"     // m_iBFCollisionHandler
#if !DEDICATED
// The client bodies of doImpact / playCollisionEffect (BF2.exe 0x5CADF0 / 0x5CA9C0 / 0x5CA910).
#include "../Main/Game.h"                                 // isDedicated() - BF2.exe 0x440510
#include "../../Scene/Object/ObjectManager.h"             // objectManager->getCameraInfo (A08F58 +0x10C)
#include "../../Scene/Object/ObjectTypes/Base/ObjectTemplateManager.h"   // getTemplate (A08F5C +0x34)
#include "../../Math/Plane3.h"                            // the EffectBundle collision plane
#include "../Objects/Fx/EffectBundle.h"                   // setCollisionPlane - BF2.exe 0x641BA0
#include "../Objects/Fx/DecalTemplate.h"                  // m_decal - the renderer decal template
#include "../../Geom/DecalSystem/DecalObjectAccess.h"     // decal_object::add - decal object PE +0x18
#include "../../Sound/NullSound.h"                        // ISound - setSoundFlags +0x08 / playAt PE +0x18
#endif

namespace dice {
namespace hfe {

namespace world { extern IBase* materialManager; }       // ds:0x8AD2820 - Core/Globals.cpp

namespace world {

// The TU's statics, in the static initialiser's order (0x80DFBE0). The Mat4 is the one with an
// ELF symbol (`global constructor keyed to GameLogicCollision::m_effectTransformation`); the rest
// are namespace-scope objects the initialiser builds inline, named here [CHOSEN].
Mat4 GameLogicCollision::m_effectTransformation = Mat4::getIdentity();

namespace {
GameLogicCollision::CollisionLog g_collisionLog =
{
    false,        // enabled
    true, true,   // showObjectNames, showMaterialNames
    1.0f,         // minDamage
    true,         // showZeroDamage
    true, true, true, true,   // enableType1..4
    true,         // detailed
    true, true,   // byte0E, byte0F
    false,        // suppressRepeats
    0, 0          // lastOwnMaterial, lastOtherMaterial
};
Vec3 g_vec3C0 = { 0.0f, 0.0f, 0.0f };                                    //  [UNRESOLVED role]

float* const g_soldierTerrainSpeedLimit  = Vars::getFloat("soldier-terrain-speed-limit", 9.1f);
float* const g_damageAngleModChoke       = Vars::getFloat("damage-angle-mod-choke", 0.0f);
float* const g_collisionImpactSpeedLimit = Vars::getFloat("collision-impact-speed-limit", 4.0f);
float* const g_soldierImpactSpeedLimit   = Vars::getFloat("soldier-impact-speed-limit", 10.0f);
float* const g_soldierImpactSpeedLimit2  = Vars::getFloat("soldier-impact-speed-limit-2", 3.0f);
int*   const g_collisionDebugDefaultTerrain = Vars::getInt("collision-debug-default-terrain", 0);
int*   const g_collisionUseModOnSoldier  = Vars::getInt("collision-use-mod-on-soldier", 1);
int*   const g_collisionUseCubicSpeedMod = Vars::getInt("collision-use-cubic-speedmod", 0);

// The two parent walks every handler does: the nearest armor component up the chain (+0x3C, then
// +0x28) and the nearest object answering IID_IPlayerControlObject.
IArmorGLComp* findArmor(IObject* start)
{
    for (IObject* o = start; o; o = o->getParent())
        if (IObjectComponent* c = object_detail::LayoutAccess::armorComponent(o))
            return static_cast<IArmorGLComp*>(c);
    return 0;
}
IPlayerControlObject* findPco(const IObject* start)
{
    for (const IObject* o = start; o; o = o->getParent())
        if (void* p = o->queryInterface(IID_IPlayerControlObject))
            return static_cast<IPlayerControlObject*>(p);
    return 0;
}
IPhysicsNode* nodeOf(const IObject* o)
{
    return static_cast<IPhysicsNode*>(object_detail::LayoutAccess::physicsNode(const_cast<IObject*>(o)));
}

// The engine's "normalise unless already unit" form, inlined at every handler's entry for the
// speed and the normal: |lenSq - 1| < eps leaves the vector alone; a degenerate one becomes zero.
void normaliseIfNeeded(Vec3& v)
{
    const float lenSq = v.y * v.y + v.x * v.x + v.z * v.z;
    if (fabsf(lenSq - 1.0f) >= FLT_EPSILON)
    {
        if (fabsf(lenSq) >= FLT_EPSILON)
        {
            const float inv = 1.0f / sqrtf(lenSq);
            v.x *= inv; v.y *= inv; v.z *= inv;
        }
        else
        {
            v.x = 0.0f; v.y = 0.0f; v.z = 0.0f;
        }
    }
}
inline MaterialManager* materials() { return static_cast<MaterialManager*>(materialManager); }


#if !DEDICATED
// Builds a plane from normal and point, preserving (z + y) + x dot-product order.
// [INFERRED] Equivalent of the original Plane3 constructor.
inline Plane3 collisionPlaneFrom(const Vec3& normal, const Vec3& point)
{
    Plane3 plane;
    plane.normal = normal;
    plane.d = -(point.z * normal.z + point.y * normal.y + normal.x * point.x);
    return plane;
}

// Returns IProjectileObject's constant base decal modifier.
// [CHOSEN, not recovered] [RETIRE-WHEN: iprojectileobject-declared]
// Replace with virtual dispatch once the full interface is declared (RW-254).
inline float decalSizeModifier(const IProjectileObject*)
{
    return 1.0f;
}
#endif
} // namespace

// Builds the effect frame using raw up in both cross products; do not normalize
// it in place. NaN still takes the frame-building branch.
// [PLATFORM] The original client uses x87 extended precision and a different dot-sum order.
const Mat4& GameLogicCollision::getCollisionEffectTransformation(
    const Vec3& position, const Vec3& up, const Vec3& normal)
{
    m_effectTransformation = Mat4::getIdentity();
    const float dot = up.x * normal.x + up.y * normal.y + up.z * normal.z;
    if (!(std::fabs(dot) > 0.99989998f))
    {
        const auto normalize = [](Vec3& v) {
            const float lengthSq = v.x * v.x + v.y * v.y + v.z * v.z;
            if (std::fabs(lengthSq - 1.0f) < FLT_EPSILON) return;
            if (std::fabs(lengthSq) < FLT_EPSILON) v.x = v.y = v.z = 0.0f;
            else
            {
                const float inverse = 1.0f / std::sqrt(lengthSq);
                v.x *= inverse; v.y *= inverse; v.z *= inverse;
            }
        };
        Vec3 y = up;
        normalize(y);
        Vec3 x = {normal.y * up.z - normal.z * up.y,
                  normal.z * up.x - normal.x * up.z,
                  normal.x * up.y - normal.y * up.x};
        normalize(x);
        const Vec3 z = {x.y * up.z - x.z * up.y,
                        x.z * up.x - x.x * up.z,
                        x.x * up.y - x.y * up.x};
        m_effectTransformation.m[0] = x.x; m_effectTransformation.m[1] = x.y; m_effectTransformation.m[2] = x.z;
        m_effectTransformation.m[4] = y.x; m_effectTransformation.m[5] = y.y; m_effectTransformation.m[6] = y.z;
        m_effectTransformation.m[8] = z.x; m_effectTransformation.m[9] = z.y; m_effectTransformation.m[10] = z.z;
    }
    m_effectTransformation.m[12] = position.x;
    m_effectTransformation.m[13] = position.y;
    m_effectTransformation.m[14] = position.z;
    return m_effectTransformation;
}

float getMassMod(IObject* other, IObject* self)
{
    float mod = 1.0f;
    IPhysicsNode* nodeOther = nodeOf(other);
    IPhysicsNode* nodeSelf  = nodeOf(self);
    if (nodeOther && nodeSelf && nodeOther->getIsMobile() && nodeSelf->getIsMobile()
        && other->getTemplate()->queryInterface(IID_ISimpleObjectTemplate)
        && self->getTemplate()->queryInterface(IID_ISimpleObjectTemplate))
    {
        IObjectTemplate* rootOther = const_cast<IObjectTemplate*>(getRootParent(other)->getTemplate());
        IObjectTemplate* rootSelf  = const_cast<IObjectTemplate*>(getRootParent(self)->getTemplate());
        SimpleObjectTemplate* simpleOther =
            static_cast<SimpleObjectTemplate*>(rootOther->queryImplementation(CID_SimpleObjectTemplate));   // +0x50
        SimpleObjectTemplate* simpleSelf =
            static_cast<SimpleObjectTemplate*>(rootSelf->queryImplementation(CID_SimpleObjectTemplate));
        const float massOther = simpleOther->m_mass;                                   // +92
        const float massSelf  = self->getTemplate()->getClassID() == CID_SoldierTemplate
                              ? 200.0f : simpleSelf->m_mass;
        if (massSelf > 0.0f)
        {
            float ratio = massOther / massSelf;
            if (ratio > 3.0f)
                ratio = 3.0f;
            if (ratio <= 0.3f)
                return 0.3f;
            return ratio;
        }
    }
    return mod;
}

// Plays material impact sound, effects and decals in the client image.
// Dedicated mode is inert. Camera distance and underwater conditions can suppress playback.
// Object hits use the object's decal target; ground hits use the game-logic decal target.
#if DEDICATED
void GameLogicCollision::doImpact(int, int, MaterialCollisionType, const Vec3&, const Vec3&,
                                  const Vec3&, IObject*, IObject*, IProjectileObject*,
                                  CollisionFaceInfo*, Vec3, bool)
{
}
#else
void GameLogicCollision::doImpact(int materialA, int materialB, MaterialCollisionType collisionType,
                                  const Vec3& position, const Vec3& normal, const Vec3& direction,
                                  IObject* objectA, IObject* objectB, IProjectileObject* projectile,
                                  CollisionFaceInfo* face, Vec3 effectRotation, bool ignoreUnderWater)
{
    (void)objectA;                         // never read by the client body either

    if (isDedicated())
        return;

    // The flag lives in the TOP byte of the bool argument's own stack slot ([ebp+37h]) - codegen,
    // not source. A NaN depth compares false and suppresses the effect (`test ah, 41h; jp`).
    bool playEffect = true;
    if (!ignoreUnderWater && projectile)
    {
        // IProjectileObject subtable PE +0xF4 = BF2.exe 0x612040 = GenericProjectile::getUnderWater
        // (m_physicsNode ? node->getUnderWater() : 0.0f). Typed downcast: GenericProjectile is the
        // interface's only implementor and IProjectileObject.h forbids dispatching through it.
        playEffect = static_cast<GenericProjectile*>(projectile)->getUnderWater() <= 0.1f;
    }

    // The effect matrix doubles as camera scratch and retains that value when neither effect template exists.
    float cameraFar;
    objectManager->getCameraInfo(&m_effectTransformation, &cameraFar, 0);             //  +0x10C
    const float dx = position.x - m_effectTransformation.m[12];
    const float dy = position.y - m_effectTransformation.m[13];
    const float dz = position.z - m_effectTransformation.m[14];
    // Summed (dz*dz + dy*dy) + dx*dx in x87 extended precision; `test ah, 5; jnp` returns only on
    // a strict far^2 < dist^2, so a NaN on either side keeps the impact.
    if (cameraFar * cameraFar < dz * dz + dy * dy + dx * dx)
        return;

    // 4. the sound
    MaterialManager* const manager = materials();
    ISound* const sound = static_cast<ISound*>(manager->getSound(
        static_cast<unsigned int>(materialA), static_cast<unsigned int>(materialB),
        static_cast<int>(collisionType), true));                               // +0x68
    if (sound && playEffect)
    {
        sound->setSoundFlags(1);                                               // +0x08
        Vec3 at;
        at.x = normal.x * 0.050000001f + position.x;
        at.y = normal.y * 0.050000001f + position.y;
        at.z = normal.z * 0.050000001f + position.z;   // retail rounds only this product to float
        sound->playAt(at, 1);                          // PE +0x18 = playAt(const Vec3&, int), typed
    }

    // 5. the cell's templates and the frame
    // Evaluation order as shipped: the effect template (+0x5C) first, then the decal (+0x60).
    IObjectTemplate* const effectTemplate = static_cast<IObjectTemplate*>(manager->getEffectTemplate(
        static_cast<unsigned int>(materialA), static_cast<unsigned int>(materialB),
        static_cast<int>(collisionType)));                                     // +0x5C
    IObjectTemplate* decalTemplate = static_cast<IObjectTemplate*>(manager->getDecalTemplate(
        static_cast<unsigned int>(materialA), static_cast<unsigned int>(materialB),
        static_cast<int>(collisionType)));                                     // +0x60
    if (decalTemplate || effectTemplate)
        getCollisionEffectTransformation(position, normal, direction);         // the static

    // 6. the authored rotation
    // The length is a real fsqrt compared against 0.0f (93C850) with `test ah, 41h; jne` - only a
    // strictly positive length rotates; NaN does not.
    if (sqrtf(effectRotation.z * effectRotation.z + effectRotation.y * effectRotation.y
              + effectRotation.x * effectRotation.x) > 0.0f)
    {
        Mat4 rotation = Mat4::getIdentity();                                   // Mat4::Mat4()
        // The components go in reversed and negated (-z, -y, -x) - the same shape
        // GameClient's createObject arm hands 0x6DA3D0.
        setRotationZXY(rotation, -effectRotation.z, -effectRotation.y, -effectRotation.x);
        const Mat4 current = m_effectTransformation;                           // mult does
        m_effectTransformation.mult(current, rotation);                        //   not alias-check: 6D9840
    }

    // 7. the effect
    if (playEffect)
        playCollisionEffect(effectTemplate, m_effectTransformation);           // result dropped

    // 8. the decal
    if (!decalTemplate)
    {
        // attested name: the ELF keeps this function-local static's symbol
        // (_ZZN...GameLogicCollision8doImpact...E21mhrDebugForceTemplate) though its body is gone.
        // BF2.exe holds it at ds:0x9EE1B8 (.data, 0) and no instruction writes it - a switch for a
        // debugger. The template lookup below therefore never runs in a shipped session.
        static bool mhrDebugForceTemplate = false;
        if (!mhrDebugForceTemplate)
            return;
        // Runs before the playEffect test, as shipped.
        decalTemplate = objectTemplateManager->getTemplate(std::string("decal_mexp_default"));   // +0x34
        if (!decalTemplate)
            return;
    }
    if (!playEffect)
        return;

    float decalScale = 1.0f;                                                   // [ebp+34h] = 3F800000h
    if (projectile)
        decalScale = decalSizeModifier(projectile);                            // subtable +0xC8 (1.0f)

    void* decalObject;
    void* collisionMesh;
    CollisionFaceInfo* decalFace;
    if (objectB)
    {
        if (!face)
            return;
        if (!objectB->getDecalObject())                                        // +0xD8
            return;
        decalObject   = objectB->getDecalObject();                             // asked twice, as shipped
        collisionMesh = object_detail::LayoutAccess::collisionMesh(objectB);   // +0x40
        decalFace     = face;
    }
    else
    {
        if (!gameLogic->getGroundDecalObject())                                // +0x2A0
            return;
        // asked twice, as shipped. The tree types the answer world::IObject* [INFERRED]; it is the
        // renderer's DecalObject (RendDX9 table 0x10221860), the same interface
        // IObject::getDecalObject answers - hence the plain void* hand-off.
        decalObject   = gameLogic->getGroundDecalObject();
        collisionMesh = 0;
        decalFace     = 0;
    }
    // No type test on the decal template: its +0x64 (DecalTemplate::m_decal) is read blindly.
    // The returned decal id is discarded.
    geom::decal_object::add(decalObject, &m_effectTransformation,
                            static_cast<DecalTemplate*>(decalTemplate)->m_decal, decalScale,
                            collisionMesh, decalFace);                         // decal object PE +0x18
}
#endif

// Creates and initializes a collision effect on clients; dedicated builds return null.
// Effect bundles receive the collision plane. The returned object follows the
// effect's normal lifetime; this function does not retain it.
#if DEDICATED
IObject* GameLogicCollision::playCollisionEffect(Vec3, const IObjectTemplate*, Vec3, Vec3)
{
    return 0;                                                                  // ELF: xor eax, eax
}

IObject* GameLogicCollision::playCollisionEffect(const IObjectTemplate*, const Mat4&)
{
    return 0;                                                                  // ELF: xor eax, eax
}
#else
IObject* GameLogicCollision::playCollisionEffect(Vec3 position, const IObjectTemplate* effectTemplate,
                                                 Vec3 up, Vec3 normal)
{
    if (isDedicated())
        return 0;
    return playCollisionEffect(effectTemplate,
                               getCollisionEffectTransformation(position, up, normal));
}

IObject* GameLogicCollision::playCollisionEffect(const IObjectTemplate* effectTemplate,
                                                 const Mat4& transform)
{
    if (isDedicated())
        return 0;
    if (!effectTemplate)
        return 0;
    IObject* const effect = effectTemplate->createObject();                   // +0x50
    if (!effect)
        return 0;

    // Keep both class-ID queries. The GroundEffectBundle branch retains an invalid
    // EffectBundle cast from the original; current template creation returns null.
    if (effect->getTemplate()->getClassID() == CID_EffectBundleTemplate
        || effect->getTemplate()->getClassID() == CID_GroundEffectBundleTemplate)
    {
        const Vec3 up    = { transform.m[4],  transform.m[5],  transform.m[6] };   // row 1: [edi+10h]
        const Vec3 point = { transform.m[12], transform.m[13], transform.m[14] };  // row 3: [edi+30h]
        const Plane3 plane = collisionPlaneFrom(up, point);
        static_cast<EffectBundle*>(effect)->setCollisionPlane(plane);
    }

    if (effect->getParent())                                                   // +0x28
        effect->setRelativeTransformation(transform);                          // +0xE8
    else
        effect->setAbsoluteTransformation(transform);                          // +0x88
    effect->init();                                                            // +0x1C
    effect->handleMessage(static_cast<TemplateMessage>(0), 0, false);          // +0x100
    return effect;
}
#endif

bool GameLogicCollision::calculateCollisionType(const Vec3& speed, const Vec3& normal, int materialA,
                                                int materialB, float& out)
{
    Material* a = materials()->getMaterialPtr(materialA);                   // +0x24
    Material* b = materials()->getMaterialPtr(materialB);
    if (!a)
        a = materials()->getMaterialPtr(0);
    if (!b)
        b = materials()->getMaterialPtr(0);
    const float hardnessA = a->getProjectileCollisionHardness();              // slot 13
    const float hardness  = b->getProjectileCollisionHardness() * hardnessA;
    if (hardness <= 0.0f)
    {
        out = 0.1f;
        return false;
    }
    const float d = -(normal.y * speed.y + normal.x * speed.x + normal.z * speed.z);
    const float v = sqrtf(1.0f - d * d) * hardness;
    out = v;
    return cosf((1.0f - v) * 1.5707964f) >= d;
}

void GameLogicCollision::getAttackPlayer(const IObject* other, const IObject* self, const Vec3& speed,
                                         IPlayer*& player, int& team)
{
    IPlayerControlObject* pco = findPco(other);
    if (!pco)
        return;
    player = pco->getOccupingPlayer();                                        // +0x10
    if (!player)
        return;
    team = getPlayer(player)->getTeam();                                      // +0xE8
    IPlayerControlObject* selfPco = findPco(self);
    if (!selfPco || selfPco->getTeam() != team)                               // +0x98
        return;
    IPhysicsNode* nodeOther = nodeOf(other);
    if (!nodeOther)
        return;
    IPhysicsNode* nodeSelf = nodeOf(self);
    if (!nodeSelf)
        return;
    const Vec3 vSelf  = nodeSelf->getPositionalSpeed();                       // +0x60
    const Vec3 vOther = nodeOther->getPositionalSpeed();
    const float sx = speed.x, sy = speed.y, sz = speed.z;
    const float lenSq = sy * sy + sx * sx + sz * sz;

    // self's speed projected on the contact speed
    float px = 0.0f, py = 0.0f, pz = 0.0f;
    if (fabsf(lenSq) > FLT_EPSILON)
    {
        const float k = (vSelf.x * sx + vSelf.y * sy + vSelf.z * sz) / lenSq;
        px = sx * k; py = sy * k; pz = k * sz;
    }
    // other's speed projected the same way
    float qx = 0.0f, qy = 0.0f, qz = 0.0f;
    const float lenSq2 = sy * sy + sx * sx + sz * sz;
    if (fabsf(lenSq2) > FLT_EPSILON)
    {
        const float k = (vOther.x * sx + vOther.y * sy + vOther.z * sz) / lenSq2;
        qx = sx * k; qy = sy * k; qz = speed.z * k;
    }
    if (sqrtf(px * px + py * py + pz * pz) > sqrtf(qx * qx + qy * qy + qz * qz))
    {
        player = 0;
        team = -1;
    }
}

// Handles terrain/water collisions. Preserve the discarded water-physics query,
// dedicated impact call, and separate contact-position reads.
void GameLogicCollision::serverHandleCollisionLandOrWater(IObject* other, IObject* self,
                                                          const Vec3& speed, const Vec3& normal,
                                                          const Vec3& relative, int surfaceMaterial,
                                                          int ownMaterial)
{
    IArmorGLComp* armor = findArmor(self);
    if (!armor)
        return;

    Vec3 dir = speed;
    normaliseIfNeeded(dir);
    Vec3 n = normal;
    normaliseIfNeeded(n);
    const float cosA = fabsf(dir.y * n.y + dir.x * n.x + dir.z * n.z);
    const float v = sqrtf(speed.x * speed.x + speed.y * speed.y + speed.z * speed.z);
    IObject* root = getRootParent(self);

    const bool soldier = self->getTemplate()->getClassID() == CID_SoldierTemplate;
    if (soldier && *g_soldierTerrainSpeedLimit > v)
        return;

    int mat = surfaceMaterial;
    if (*g_collisionDebugDefaultTerrain > 0 && surfaceMaterial == 0)
        mat = 3;
    if (Material* m = materials()->getMaterialPtr(mat))                       // +0x24
        m->getHasWaterPhysics();                                              // slot 25 - result unused

    Vec3 hitDir = { 0.0f, 0.0f, 0.0f };
    Vec3 contact;
    {
        const Vec3& pos = self->getAbsolutePosition();                        // +0x6C
        contact.x = relative.x + pos.x;
        contact.y = relative.y + pos.y;
        contact.z = relative.z + pos.z;
    }
    // Pass the surface material before own material, and use the collision normal.
    doImpact(mat, ownMaterial, static_cast<MaterialCollisionType>(0), contact, normal, dir, other,
             self, 0, 0, hitDir, false);

    float angle = armor->getAngleMod();                                       // +0x78
    if (angle <= cosA)
        angle = cosA;
    const float speedTerm = self->getTemplate()->getClassID() == CID_SoldierTemplate
                          ? (v - *g_soldierTerrainSpeedLimit) * v
                          : v * v;
    const float damageMod = materials()->getDamageMod(mat, ownMaterial);      // +0x58
    const float damage = damageMod * (angle * speedTerm);
    g_collisionLog.report(GLCT_LandOrWater, self, other, ownMaterial, mat, damage, 0.0f, angle,
                          speedTerm, damageMod, 0.0f);
    hitDir = dir;
    {
        const Vec3& pos = self->getAbsolutePosition();
        contact.x = relative.x + pos.x;
        contact.y = relative.y + pos.y;
        contact.z = relative.z + pos.z;
    }
    gameLogic->giveDamage(root, damage, 0, 0, 0, -1, 0, 0, contact, hitDir, mat, false, true, false);   // +0xE4
}

// Damage uses the angle once as cosine squared, and speedMod times excess speed
// raised to its exponent.
void GameLogicCollision::serverHandleCollisionObjectVsObject(IObject* other, IObject* self,
                                                             const Vec3& speed, const Vec3& normal,
                                                             const Vec3& relative, int otherMaterial,
                                                             int ownMaterial)
{
    if (other->getTemplate()->getClassID() == CID_SoldierTemplate)
        return;
    if (self->getTemplate()->getClassID() == CID_SoldierTemplate)
    {
        serverHandleCollisionObjectVsSoldier(other, self, speed, normal, relative, otherMaterial,
                                             ownMaterial);
        return;
    }

    Vec3 dir = speed;
    normaliseIfNeeded(dir);
    Vec3 n = normal;
    normaliseIfNeeded(n);
    const float cosA = fabsf(dir.y * n.y + dir.x * n.x + dir.z * n.z);
    float excess = sqrtf(speed.x * speed.x + speed.y * speed.y + speed.z * speed.z)
                 - *g_collisionImpactSpeedLimit;
    if (excess <= 0.0f)
        excess = 0.0f;

    IObject* rootSelf = getRootParent(self);
    if (getRootParent(other) == rootSelf)
        return;

    IArmorGLComp* armor = findArmor(self);
    if (armor)
    {
        IPlayer* player = 0;
        int team = -1;
        getAttackPlayer(other, self, speed, player, team);
        const float notProjectile =
            (other->queryInterface(IID_IProjectileObject) || self->queryInterface(IID_IProjectileObject)
             || other->queryInterface(IID_IParticle)) ? 0.0f : 1.0f;
        const float cos2 = cosA * cosA;
        const float speedMod = armor->getSpeedMod();                          // +0x8C - float in ST(0)
        const float speedTerm = speedMod * (*g_collisionUseCubicSpeedMod ? excess * excess * excess
                                                                      : excess * excess);
        const float massMod = getMassMod(other, self);
        const float damageMod = materials()->getDamageMod(otherMaterial, ownMaterial);   // +0x58
        const float damage = damageMod * (notProjectile * cos2 * speedTerm * massMod);
        g_collisionLog.report(GLCT_ObjectVsObject, self, other, ownMaterial, otherMaterial, damage,
                              notProjectile, cos2, speedTerm, damageMod, massMod);

        Vec3 hitDir = { 0.0f, 0.0f, 0.0f };
        Vec3 contact;
        {
            const Vec3& pos = self->getAbsolutePosition();
            contact.x = relative.x + pos.x;
            contact.y = relative.y + pos.y;
            contact.z = relative.z + pos.z;
        }
        // Pass other material before own material.
        doImpact(otherMaterial, ownMaterial, static_cast<MaterialCollisionType>(0), contact, normal,
                 dir, other, self, 0, 0, hitDir, false);

        hitDir = dir;
        {
            const Vec3& pos = self->getAbsolutePosition();
            contact.x = relative.x + pos.x;
            contact.y = relative.y + pos.y;
            contact.z = relative.z + pos.z;
        }
        IObject* target = armor->getObject();                                 // +0x20
        gameLogic->giveDamage(target, damage, player, 0, other, team, 0, 0, contact, hitDir,
                              ownMaterial, true, true, false);                // +0xE4
    }
    else
    {
        Vec3 hitDir = { 0.0f, 0.0f, 0.0f };
        Vec3 contact;
        const Vec3& pos = self->getAbsolutePosition();
        contact.x = relative.x + pos.x;
        contact.y = relative.y + pos.y;
        contact.z = relative.z + pos.z;
        // Pass other material before own material.
        doImpact(otherMaterial, ownMaterial, static_cast<MaterialCollisionType>(0), contact, normal,
                 dir, other, self, 0, 0, hitDir, false);
    }
}

// Uses a 3 m/s threshold when a mobile object overtakes the soldier, otherwise
// 10 m/s. Deadly/barbwire materials bypass it with speed factor 1. Damage uses
// zero position and normalized velocity direction.
void GameLogicCollision::serverHandleCollisionObjectVsSoldier(IObject* other, IObject* self,
                                                              const Vec3& speed, const Vec3& normal,
                                                              const Vec3& relative, int otherMaterial,
                                                              int ownMaterial)
{
    Vec3 dir = speed;
    normaliseIfNeeded(dir);
    Vec3 n = normal;
    normaliseIfNeeded(n);
    const float v = sqrtf(speed.x * speed.x + speed.y * speed.y + speed.z * speed.z);

    Material* material = materials()->getMaterialPtr(otherMaterial);          // +0x24
    const bool deadly = otherMaterial == 666 || (material && material->getIsBarbwire());   // slot 27

    IPhysicsNode* nodeOther = nodeOf(other);
    IPhysicsNode* nodeSelf  = nodeOf(self);
    float limit;
    if (nodeOther && nodeSelf && nodeOther->getIsMobile())                    // +0x134
    {
        const Vec3& vo = nodeOther->getPositionalSpeed();                     // +0x60
        const float otherSq = vo.x * vo.x + vo.y * vo.y + vo.z * vo.z;
        const Vec3& vs = nodeSelf->getPositionalSpeed();
        limit = otherSq > vs.x * vs.x + vs.y * vs.y + vs.z * vs.z ? *g_soldierImpactSpeedLimit2
                                                                   : *g_soldierImpactSpeedLimit;
    }
    else
        limit = *g_soldierImpactSpeedLimit;

    if (!(limit <= v || deadly))
        return;

    IPlayer* player = 0;
    int team = -1;
    getAttackPlayer(other, self, speed, player, team);
    IArmorGLComp* armor = findArmor(self);

    float speedTerm = 1.0f;
    if (otherMaterial != 666)
        speedTerm = (v - limit) * v;
    const float massMod = getMassMod(other, self);
    float matMod;
    if (*g_collisionUseModOnSoldier || otherMaterial == 666)
        matMod = materials()->getDamageMod(otherMaterial, ownMaterial);       // +0x58
    else
        matMod = 1.0f;
    const float damage = speedTerm * massMod * matMod;
    g_collisionLog.report(GLCT_ObjectVsSoldier, self, other, ownMaterial, otherMaterial, damage, 1.0f,
                          1.0f, speedTerm, matMod, massMod);

    Vec3 hitDir = { 0.0f, 0.0f, 0.0f };
    Vec3 contact;
    {
        const Vec3& pos = self->getAbsolutePosition();
        contact.x = relative.x + pos.x;
        contact.y = relative.y + pos.y;
        contact.z = relative.z + pos.z;
    }
    // Pass other material before own material.
    doImpact(otherMaterial, ownMaterial, static_cast<MaterialCollisionType>(0), contact, normal, dir,
             other, self, 0, 0, hitDir, false);

    if (other->queryInterface(IID_IProjectileObject))
        return;
    if (self->queryInterface(IID_IProjectileObject))
        return;
    Vec3 zero = { 0.0f, 0.0f, 0.0f };
    IObject* target = armor->getObject();                                     // +0x20 - no null
                                                                              //   check on armor, as shipped
    gameLogic->giveDamage(target, damage, player, 0, other, team, 0, 0, zero, dir, ownMaterial,
                          true, true, false);                                 // +0xE4
}

// Forwards projectile impact arguments in doImpact's order, supplying a zero vector
// and false for the final parameters. Dedicated images do not render the effect.
void GameLogicCollision::doProjectileImpact(int materialA, int materialB,
                                            MaterialCollisionType collisionType,
                                            const Vec3& contact, const Vec3& normal,
                                            const Vec3& direction,
                                            IProjectileObject* proj, IObject* projectile,
                                            IObject* hitObject, IArmorGLComp* armor, bool damaged,
                                            CollisionFaceInfo* face)
{
    (void)armor;
    (void)damaged;
    Vec3 unused = { 0.0f, 0.0f, 0.0f };
    doImpact(materialA, materialB, collisionType, contact, normal, direction,
             projectile, hitObject, proj, face, unused, false);
}

// Applies projectile damage and resolves ricochet/penetration using material and armor modifiers.
// Requires a normalized normal; collisionOffset is relative to the projectile root.
// Self-hits return false before effects. Impact effects are throttled per projectile for ten ticks;
// friendly-fire and man-down rules can retract the reported damage result.
bool GameLogicCollision::serverHandleCollisionForProjectile(IObject* projectile, IObject* hitObject,
                                                            const Vec3& velocity, const Vec3& normal,
                                                            const Vec3& collisionOffset,
                                                            int materialA, int materialB,
                                                            CollisionFaceInfo* faceA,
                                                            CollisionFaceInfo* faceB)
{
    (void)faceA;                     // read by neither arm; both impacts forward faceB
    bool damaged = false;

    Vec3 unitVelocity = velocity;
    normaliseIfNeeded(unitVelocity);

    // The impact cosine, off the caller-normalised normal. Positive for a head-on hit.
    const float impactCos = -(normal.x * unitVelocity.x
                            + normal.y * unitVelocity.y
                            + normal.z * unitVelocity.z);

    const float speed = sqrtf(velocity.x * velocity.x
                            + velocity.y * velocity.y
                            + velocity.z * velocity.z);

    // This test uses the RAW velocity, not the normalised one.
    bool playImpact = (velocity.x * normal.x + velocity.y * normal.y + velocity.z * normal.z) < -0.5f
                   || speed > 10.0f;

    IObject* const root = getRootParent(projectile);
    IProjectileObject* proj = 0;
    if (root)
        proj = static_cast<IProjectileObject*>(root->queryInterface(IID_IProjectileObject));
    // GenericProjectile is the interface's only implementor; IProjectileObject.h forbids dispatching
    // through the interface pointer until its full vtable is recovered, so this is the typed route.
    GenericProjectile* const gp = static_cast<GenericProjectile*>(proj);

    // `root` is dereferenced without a null check even though the queryInterface above guards it.
    Vec3 contact;
    {
        const Vec3& rootPos = root->getAbsolutePosition();
        contact.x = collisionOffset.x + rootPos.x;
        contact.y = collisionOffset.y + rootPos.y;
        contact.z = collisionOffset.z + rootPos.z;
    }

    const unsigned int tick = WorldPref::getTickCount();
    if (static_cast<int>(tick - gp->getPlayedImpactEffectAtTick()) <= 10)     // +0x11C
        playImpact = false;

    // the collision type
    // The default of 1 only survives when there is no target AND the round is slower than its own
    // minimum detonation speed - i.e. a slow round in open air keeps type 1 and ricochets.
    float ricochetFraction = 0.0f;
    int collisionType = 1;
    if ((hitObject && hitObject != projectile) || gp->getMinDetonationSpeed() <= speed)   // +0xF0
    {
        collisionType = gp->getDefaultCollisionType(materialB);                          // +0xE8
        if (!collisionType)
        {
            // The normal goes in first here. calculateCollisionType's own parameters are named
            // (speed, normal) in this tree, which is the wrong way round; its dot product is
            // symmetric so the behaviour is identical, but the names do not match the caller.
            collisionType = calculateCollisionType(normal, unitVelocity, materialA, materialB,
                                                   ricochetFraction) ? 1 : 0;
        }
    }

    if (hitObject)
    {
        IArmorGLComp* const armor = findArmor(hitObject);
        if (armor)
        {
            if (root == getRootParent(hitObject))
                return false;                        // the self-hit drop - see the header block

            const float baseDamage = gp->getDamage(contact);                             // +0xBC

            float angle = armor->getAngleMod();                                          // +0x78
            if (angle <= impactCos)
                angle = impactCos;
            float t = angle + 0.44999999f;
            if (t > 1.0f)
                t = 1.0f;
            const float angleTerm = 1.0f - (1.0f - t * t) * (*g_damageAngleModChoke);

            const float damage =
                materials()->getDamageMod(materialA, materialB) * (angleTerm * baseDamage);  // +0x58

            if (damage > 0.0099999998f)
            {
                // Retail evaluates these in the reverse of the parameter order, innermost first.
                const bool canTK          = gp->canTK();                                 // +0x70
                const Vec3 startPos       = gp->getStartPos();                           // +0x6C
                IObject* launcherVehicle  = gp->getLauncherVehicle();                    // +0x4C
                IObject* launcherWeapon   = gp->getLauncherWeapon();                     // +0x34
                const int team            = gp->getTeam();                               // +0x64
                std::list<WeakPtr<IPlayer> >* assists = gp->getAssistPlayers();          // +0x5C
                IPlayer* const owner      = gp->getPlayer();                             // +0x54

                damaged = gameLogic->giveDamage(armor->getObject(), damage, owner, assists,
                                                projectile, team, launcherWeapon, launcherVehicle,
                                                startPos, unitVelocity, materialB,
                                                true, canTK, false);                     // +0xE4
            }
        }

        if (playImpact)
        {
            gp->setPlayedImpactEffectAtTick(tick);                                       // +0x120
            doProjectileImpact(materialA, materialB,
                               static_cast<MaterialCollisionType>(collisionType),
                               contact, normal, unitVelocity,
                               proj, projectile, hitObject, armor, damaged, faceB);
        }

        // ELF 0x80DD04A: the AI collision handler hears the object hit (+0x0C) at the contact.
        if (ai::IBFCollisionHandler* const collision = static_cast<ai::IBFCollisionHandler*>(
                ai::CommonSingletonServer::m_iBFCollisionHandler))
            collision->handleCollision(projectile, hitObject, contact);
    }
    else
    {
        if (playImpact)
        {
            gp->setPlayedImpactEffectAtTick(tick);
            // Every object-side argument is explicitly null in this arm - no target, no armour,
            // and `damaged` false regardless of what happened above (it cannot be true here).
            doProjectileImpact(materialA, materialB,
                               static_cast<MaterialCollisionType>(collisionType),
                               contact, normal, unitVelocity,
                               proj, projectile, 0, 0, false, faceB);
        }

        // ELF 0x80DD468: a terrain hit is a miss (+0x30): the owner's id (-1 without one), the
        // round and its object id, the contact.
        if (ai::IBFCollisionHandler* const collision = static_cast<ai::IBFCollisionHandler*>(
                ai::CommonSingletonServer::m_iBFCollisionHandler))
        {
            IPlayer* const owner = gp->getPlayer();                                     // +0x54
            collision->handleProjectileMiss(owner ? owner->getId() : -1, projectile,
                                            projectile->getObjectId(), contact);
        }
    }

    // ricochet / penetrate
    // Type 1 ricochets, type 0 penetrates, and ANY other non-zero type does neither. The retained
    // fraction is 1 - the value calculateCollisionType wrote, which stays 0 when that call never ran.
    if (collisionType)
    {
        if (collisionType == 1)
            gp->ricochet(collisionOffset, velocity, normal, 1.0f - ricochetFraction);    // +0xEC
    }
    else
    {
        gp->penetrate(hitObject, collisionOffset, materialA, materialB,
                      1.0f - ricochetFraction);                                          // +0xF4
    }

    // the retraction rules
    // Everything below can only turn a reported hit OFF. `damaged` is never raised here.
    // dice::hfe::getPlayer is the shipped identity forwarder (0x80F7E10, `return p;`), kept
    // because the ELF calls it here exactly as getAttackPlayer does.
    IPlayer* const owner = getPlayer(gp->getPlayer());

    IArmorGLComp* const hitArmor = hitObject ? findArmor(hitObject) : 0;
    if (hitArmor && owner && damaged)
    {
        IPlayerControlObject* const pco = findPco(getRootParent(hitArmor->getObject()));  // +0x20
        if (pco)
        {
            // Two calls to the same getter, and the second is what makes an unteamed control
            // object suppress the hit as well as a same-team one.
            if (pco->getTeam() == owner->getTeam() || !pco->getTeam())                    // +0x98
                damaged = false;

            IObject* const hitRoot = getRootParent(hitObject);
            if (hitRoot)
            {
                ISoldier* const soldier =
                    static_cast<ISoldier*>(hitRoot->queryInterface(IID_ISoldier));
                if (soldier)
                {
                    IPlayer* const victim = soldier->getOccupingPlayer();                 // ELF +0x30
                    if (victim)
                    {
                        if (!victim->getIsAlive())                                        // +0x6C
                            return false;
                        if (victim->getIsManDown()                                        // +0x100
                            && victim->getManDownAtTick() != WorldPref::getTickCount())    // +0x104
                        {
                            return false;
                        }
                    }
                }
            }
        }
    }

    return damaged;
}

// Applies heal/ammo service at the world contact. Accepted service penetrates
// and returns true; refusal ricochets and returns that result.
bool GameLogicCollision::serverHandleCollisionForReplenisher(bool isHeal, IObject* projectile,
                                                              IObject* hitObject,
                                                              const Vec3& velocity,
                                                              const Vec3& normal,
                                                              const Vec3& collisionOffset,
                                                              int materialA, int materialB)
{
    Vec3 unitVelocity = velocity;
    normaliseIfNeeded(unitVelocity);

    IObject* const projectileRoot = getRootParent(projectile);
    IProjectileObject* proj = 0;
    if (projectileRoot)
        proj = static_cast<IProjectileObject*>(projectileRoot->queryInterface(IID_IProjectileObject));
    GenericProjectile* const gp = static_cast<GenericProjectile*>(proj);

    Vec3 contact;
    const Vec3& projectilePosition = projectileRoot->getAbsolutePosition();
    contact.x = projectilePosition.x + collisionOffset.x;
    contact.y = projectilePosition.y + collisionOffset.y;
    contact.z = projectilePosition.z + collisionOffset.z;

    bool handled = false;
    IArmorGLComp* const armor = hitObject ? findArmor(hitObject) : 0;
    if (armor)
    {
        if (projectileRoot == getRootParent(hitObject))
            return false;

        const float amount = gp->getHealEffect(contact);
        if (amount > 0.0099999998f)
        {
            if (isHeal)
                handled = gameLogic->heal(armor->getObject(), amount, gp->getPlayer(),
                                          materialA, materialB, 0, false, false);
            else
                handled = gameLogic->replenishAmmo(armor->getObject(), amount, gp->getPlayer(),
                                                   materialA, materialB, 0, false);
        }
    }

    const Vec3 zero = { 0.0f, 0.0f, 0.0f };
    doImpact(materialA, materialB, static_cast<MaterialCollisionType>(handled ? 0 : 1),
             contact, normal, unitVelocity, projectile, hitObject, 0, 0, zero, false);

    if (handled)
    {
        gp->penetrate(hitObject, collisionOffset, materialA, materialB, 1.0f);
        return true;
    }
    return gp->ricochet(collisionOffset, velocity, normal, 0.0f);
}

// serverHandleCollisionForResurrecter - ELF 0x80DDA90, 856 B / PE 0x56AB70, 321 B.
// Unlike replenishment, the scalar passed to resurrect is projectile damage at the contact point.
// The bounce/penetration result is discarded here; only ServerGameLogic::resurrect's bool is
// returned. The two following floats retain their shipped order: resurrectDamage, then restoreHP.
bool GameLogicCollision::serverHandleCollisionForResurrecter(IObject* projectile,
                                                              IObject* hitObject,
                                                              float resurrectDamage,
                                                              float restoreHP,
                                                              const Vec3& velocity,
                                                              const Vec3& normal,
                                                              const Vec3& collisionOffset,
                                                              int materialA, int materialB)
{
    Vec3 unitVelocity = velocity;
    normaliseIfNeeded(unitVelocity);

    IObject* const projectileRoot = getRootParent(projectile);
    IProjectileObject* proj = 0;
    if (projectileRoot)
        proj = static_cast<IProjectileObject*>(projectileRoot->queryInterface(IID_IProjectileObject));
    GenericProjectile* const gp = static_cast<GenericProjectile*>(proj);

    Vec3 contact;
    const Vec3& projectilePosition = projectileRoot->getAbsolutePosition();
    contact.x = projectilePosition.x + collisionOffset.x;
    contact.y = projectilePosition.y + collisionOffset.y;
    contact.z = projectilePosition.z + collisionOffset.z;

    bool handled = false;
    IArmorGLComp* const armor = hitObject ? findArmor(hitObject) : 0;
    if (armor)
    {
        if (projectileRoot == getRootParent(hitObject))
            return false;

        const float amount = gp->getDamage(contact);
        if (amount > 0.0099999998f)
        {
            handled = static_cast<ServerGameLogic*>(gameLogic)->resurrect(
                proj, armor->getObject(), amount, resurrectDamage, restoreHP, gp->getPlayer(),
                materialA, materialB);
        }
    }

    const Vec3 zero = { 0.0f, 0.0f, 0.0f };
    doImpact(materialA, materialB, static_cast<MaterialCollisionType>(handled ? 0 : 1),
             contact, normal, unitVelocity, projectile, hitObject, 0, 0, zero, false);

    if (handled)
        gp->penetrate(hitObject, collisionOffset, materialA, materialB, 1.0f);
    else
        gp->ricochet(collisionOffset, velocity, normal, 0.0f);
    return handled;
}

// CollisionLog::report - 0x80DF730, 1,191 B. Disabled on this image by its initialiser; written
// in full because the enabled path is observable through its virtual calls. The two formatted lines
// are built and then destroyed - the ELF has no output call in this function.
void GameLogicCollision::CollisionLog::report(GLCollisionType type, IObject* self, IObject* other,
                                              int ownMaterial, int otherMaterial, float total,
                                              float constant, float angle, float speed, float material,
                                              float mass)
{
    if (!enabled)
        return;
    if (!enableType1 && type == 1) return;
    if (!enableType2 && type == 2) return;
    if (!enableType3 && type == 3) return;
    if (!enableType4 && type == 4) return;
    if (suppressRepeats && lastOwnMaterial == ownMaterial && lastOtherMaterial == otherMaterial)
        return;
    lastOwnMaterial = ownMaterial;
    lastOtherMaterial = otherMaterial;
    if (!(minDamage <= total))
    {
        if (!(total == -1.0f || (showZeroDamage && total == 0.0f)))
            return;
    }

    std::string otherName, selfName, rootName;
    if (other && showObjectNames)
        otherName = other->getTemplate()->getName();
    if (self && showObjectNames)
        selfName = self->getTemplate()->getName();
    if (self && showObjectNames)
        rootName = getRootParent(self)->getTemplate()->getName();

    std::string ownMaterialName, otherMaterialName;
    if (showMaterialNames && materials()->getMaterialById(ownMaterial))      // +0x88
        ownMaterialName = materials()->getMaterialById(ownMaterial)->getName();   // slot 5
    else
        ownMaterialName = formatString("%d", ownMaterial);
    if (showMaterialNames && materials()->getMaterialById(otherMaterial))
        otherMaterialName = materials()->getMaterialById(otherMaterial)->getName();
    else
        otherMaterialName = formatString("%d", otherMaterial);

    const std::string attacked = formatString("%s[m:%s] attacked %s[m:%s]", otherName.c_str(),
                                              otherMaterialName.c_str(), selfName.c_str(),
                                              ownMaterialName.c_str());
    std::string line;
    if (detailed)
        line = formatString("%s takes damage(total/constant/angle/speed/mat/mass): "
                            "%.2f/%.2f/%.2f/%.2f/%.2f/%.2f",
                            rootName.c_str(), static_cast<double>(total), static_cast<double>(constant),
                            static_cast<double>(angle), static_cast<double>(speed),
                            static_cast<double>(material), static_cast<double>(mass));
    else
        line = formatString("%s takes damage: %f", selfName.c_str(), static_cast<double>(total));
    (void)attacked;
    (void)line;
}

// The ordinary projectile family and both specialised support-projectile handlers are recovered,
// including their doProjectileImpact/doImpact source calls.

} // namespace world
} // namespace hfe
} // namespace dice
