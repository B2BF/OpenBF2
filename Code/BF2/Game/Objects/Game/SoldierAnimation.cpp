// Soldier animation evaluation, pose application and ragdoll transitions.
// The third-person rig also provides server collision poses; geometry updates are client-side.

#include "Soldier.h"
#include "SoldierTemplate.h"

#include <cmath>            // asinf / fabsf / sqrtf
#include <cfloat>           // FLT_EPSILON
#include <cstdio>           // fprintf / fflush - the [FRONTIER] stops
#include <cstdlib>          // rand / abort
#include <vector>           // the used-bone list the geometry fills (RW-199)

#include "../../../Core/Ids.h"
#include "../../../Core/System.h"                              // WorldPref::getTickCount
#include "../../../Core/IBase.h"
#include "../../../Math/Mat4.h"
#include "../../../Math/Vec3.h"
#include "../../../Animation/BoneAnimation/AnimationSystem.h"  // IAnimationSystem
#include "../../../Animation/BoneAnimation/CameraSpring.h"
#include "../../../Animation/BoneAnimation/Skeleton.h"
#include "../../../Physics/PointPhysicsNode.h"
#include "../../../Physics/SoldierPhysicsNode.h"
#include "../../../Physics/SoldierResponse.h"
#include "../../../Physics/HeightmapCluster.h"                 // IHeightmapCluster::intersectRayInWorldCoords
#include "../../../Physics/PhysicsUtil.h"                      // resetPhysics - enableRagDoll
#include "../../../Physics/IResponsePhysics.h"                 // setEnabled(false) on death
#include "../../../Animation/BoneAnimation/RagDoll.h"          // THE death path
#include "../../../Animation/BoneAnimation/RagDollTemplate.h"  // g_ragDollTemplate->makeInstance
#include "../IItemObject.h"                                    // disableItem on death
#include "../GameLogicComp/IArmorGLComp.h"                     // getLastHitDirection / getLastHitBoneIndex
#include "../../IGameLogic.h"                                  // gameLogic->isServer()
#include "../../../Scene/Object/ObjectManager.h"               // objectManager->intersectLine
#include "../../../Scene/Object/ObjectPredicator.h"            // ObjectFlagPredicator - JumpTestPredicator's base
#include "../../../Sound/SoldierSound.h"                       // jump / trigger1PLanding
#include "../../Utility/Vars.h"                                // the cvar handles below
#include "../IAnimatedBundle.h"
#include "../ICameraObject.h"
#include "../IWeaponObject.h"
#include "../IPlayer.h"
#include "../IObjectTemplate.h"
#include "../ObjectLayoutAccess.h"
#include "../GeometryPartAccess.h"
#include "../CompTemplateInterfaces.h"                         // IIkTemplateComponent
#include "../GameLogicComp/ArmorGLComp.h"
#include "../RotationalBundleTemplate.h"
#include "RotationalBundle.h"                                  // getCurrentRotation
#include "Kit.h"                                               // reviveAnimations' geometry gate

namespace dice {
namespace hfe {
namespace world {

extern IBase* heightmapCluster;                  // ds:0x8ABAF24 - Core/Globals.cpp

namespace {

typedef object_detail::LayoutAccess L;     // the raw Object offsets, as Soldier.cpp reads them

// reviveAnimations tells the carried kit's geometry that the soldier has returned to the prone
// pose. geom::IGeometry is still unrecovered, but this one seam is bounded in both shipped x86
// images: handleMessage is PE/MSVC +0x60 and ELF/Itanium +0x64 (the destructor-pair shift).
// The slot follows the compiler's destructor ABI, so native MSVC x64 takes the PE arm too.
void geometryHandleMessage(void* geometry, TemplateMessage message, void* payload)
{
#if defined(_MSC_VER)
    typedef void (__thiscall *Method)(void*, TemplateMessage, void*);
    const unsigned int slot = 0x60 / 4;
#else
    typedef void (*Method)(void*, TemplateMessage, void*);
    const unsigned int slot = 0x64 / 4;
#endif
    void** const vtable = *reinterpret_cast<void***>(geometry);
    reinterpret_cast<Method>(vtable[slot])(geometry, message, payload);
}

// Renderer dispatch for field of view, geometry selection and bone palettes.
// Explicit PE/ELF slot constants account for their differing destructor layouts; MSVC x64 keeps
// the MSVC (PE) table order.
#if defined(_MSC_VER)
#define BF2_SOLDIER_GEOM_CALL __thiscall
const unsigned int kGeometrySetFovSlot            = 0x10;
const unsigned int kGeometrySetGeomIndexSlot      = 0x6C;
const unsigned int kGeometrySetBoneTransformsSlot = 0x54;
const unsigned int kGeometryGatherUsedBonesSlot   = 0x58;
#else
#define BF2_SOLDIER_GEOM_CALL
const unsigned int kGeometrySetFovSlot            = 0x10;
const unsigned int kGeometrySetGeomIndexSlot      = 0x70;
const unsigned int kGeometrySetBoneTransformsSlot = 0x58;
const unsigned int kGeometryGatherUsedBonesSlot   = 0x5C;
#endif

template<class F> F geometrySlot(void* geometry, unsigned int offset)
{
    return reinterpret_cast<F>((*static_cast<void***>(geometry))[offset / 4]);
}

void geometrySetFov(void* geometry, float fov)
{
    typedef void (BF2_SOLDIER_GEOM_CALL* F)(void*, float);
    geometrySlot<F>(geometry, kGeometrySetFovSlot)(geometry, fov);
}

void geometrySetGeomIndex(void* geometry, int geom)
{
    typedef void (BF2_SOLDIER_GEOM_CALL* F)(void*, int);
    geometrySlot<F>(geometry, kGeometrySetGeomIndexSlot)(geometry, geom);
}

void geometrySetBoneTransforms(void* geometry, const Mat4* transforms, unsigned int boneCount)
{
    typedef void (BF2_SOLDIER_GEOM_CALL* F)(void*, const Mat4*, unsigned int);
    geometrySlot<F>(geometry, kGeometrySetBoneTransformsSlot)(geometry, transforms, boneCount);
}

// Appends geometry-used bone ids to a caller-owned vector.
// The rebuilt client and renderer share native STL and heap conventions at this boundary.
void geometryGatherUsedBones(void* geometry, std::vector<int>* out)
{
    typedef void (BF2_SOLDIER_GEOM_CALL* F)(void*, std::vector<int>*);
    geometrySlot<F>(geometry, kGeometryGatherUsedBonesSlot)(geometry, out);
}
#undef BF2_SOLDIER_GEOM_CALL

// Soldier animation and physics console tuning values.
float* const rd_expForceMod                 = Vars::getFloat("rd_expForceMod", 1.0f);                  // applyExplosionForce
float* const rd_dieInVehicleForce           = Vars::getFloat("rd_dieInVehicleForce", 100.0f);
float* const rd_hitForce                    = Vars::getFloat("rd_hitForce", 9000.0f);
float* const rd_hitUp                       = Vars::getFloat("rd_hitUp", 0.0f);
float* const soldier_lookIdleAngle          = Vars::getFloat("soldier_lookIdleAngle", 15.0f);
float* const soldier_lookIdleTime           = Vars::getFloat("soldier_lookIdleTime", 2.5f);
float* const soldier_lookSmooth             = Vars::getFloat("soldier_lookSmooth", 10.0f);
float* const anim_soldier_spring_clamp       = Vars::getFloat("anim-soldier-spring-clamp", 0.001f);
float* const anim_soldier_spring_zoom_factor = Vars::getFloat("anim-soldier-spring-zoom-factor", 6.0f);
float* const soldierAnim1pSpeedCrouch       = Vars::getFloat("anim-soldier-1p-speed-crouch", 1.7f);
float* const soldierAnim1pSpeedProne        = Vars::getFloat("anim-soldier-1p-speed-prone", 3.0f);
int*   const anim_playCameraAnimations     = Vars::getInt("anim_playCameraAnimations", 1);
int*   const anim_playSoldierAnimations     = Vars::getInt("anim_playSoldierAnimations", 1);
int*   const anim_playSoldierPostAnimations = Vars::getInt("anim_playSoldierPostAnimations", 1);
int*   const anim_playWeaponAnimations      = Vars::getInt("anim_playWeaponAnimations", 1);
int*   const anim_playVehicleAnimations     = Vars::getInt("anim_playVehicleAnimations", 1);
float* const anim_startLandAtHeight         = Vars::getFloat("anim_startLandAtHeight", 1.0f);
float* const anim_startLandAtSpeed          = Vars::getFloat("anim_startLandAtSpeed", -0.5f);
float* const anim_startJumpAtSpeed          = Vars::getFloat("anim_startJumpAtSpeed", 2.5f);
float* const fov1p                          = Vars::getFloat("soldier-fov1p", 0.82f);
float* const soldier_lookUp                 = Vars::getFloat("soldier-lookUp", -70.0f);
float* const soldier_lookDown               = Vars::getFloat("soldier-lookDown", 70.0f);
float* const soldier_proneLookUp            = Vars::getFloat("soldier-proneLookUp", -30.0f);
float* const soldier_proneLookDown          = Vars::getFloat("soldier-proneLookDown", 5.0f);

// JumpTestPredicator - attested (its vtable is
// _ZTVN4dice3hfe5world55_GLOBAL__N_Game_Objects_Game_SoldierAnimation.cppJW4FSa18JumpTestPredicatorE
// at 0x88A0718): an ObjectFlagPredicator for flag 0x100 that additionally rejects soldiers and
// anything whose physics bounding radius is under 0.5 - the landing ray must not land on a
// person or a pebble. predicate (0x81B8450, 84 B) is its only override.
class JumpTestPredicator : public ObjectFlagPredicator
{
public:
    JumpTestPredicator() : ObjectFlagPredicator(0x100u) {}
    virtual bool predicate(IObject* object)
    {
        return object->getTemplate()->getClassID() != CID_SoldierTemplate
            && object->getPhysicBoundingRadius() >= 0.5f                     // IObject slot 38
            && ObjectFlagPredicator::predicate(object);
    }
};

// Vec3 has no operator[] in this tree; the two rotational-bundle readers index by axis.
float& component(Vec3& v, int i)
{
    return i == 0 ? v.x : (i == 1 ? v.y : v.z);
}
float component(const Vec3& v, int i)
{
    return i == 0 ? v.x : (i == 1 ? v.y : v.z);
}

// Retail Object +0x3C, read directly as in Soldier.cpp's objectArmorComp().
ArmorGLComp* armorOf(IObject* soldier)
{
    return static_cast<ArmorGLComp*>(L::armorComponent(soldier));
}

// Character mode 0 selects the first-person rig; mode 1 selects third-person.
anim::Skeleton* characterSkeleton(Soldier* soldier, int mode, void* skeleton1P, anim::Skeleton* skeleton3P)
{
    (void)soldier;
    return mode == 0 ? static_cast<anim::Skeleton*>(skeleton1P) : skeleton3P;
}

// The +0x40 override matrix of one bone, allocated on first use - the ELF's inline
// `operator new(0x40)` plus an identity fill, then the copy.
void setBoneOverride(anim::Skeleton* skeleton, int boneIndex, const Mat4& m)
{
    anim::Skeleton::Bone& bone = skeleton->m_bones[boneIndex];               // +0x04 each
    if (!bone.m_override)
    {
        bone.m_override = new Mat4;
        *bone.m_override = Mat4::getIdentity();
    }
    *bone.m_override = m;
}

} // namespace

// forward - defined below getRotationalBundleRotation, as in the ELF's layout order
void accumulateRotationalBundleRotations(IObject* object, Vec3& out);

// Accumulates normalized rotation from the containing PCO and its first child.
void getRotationalBundleRotation(IObject* object, Vec3& out)
{
    IObject* current = object;
    if (!current)
        return;
    while (current->getTemplate()->getClassID() != CID_PlayerControlObjectTemplate)
    {
        current = L::parent(current);                                        // +0x28
        if (!current)
            return;
    }
    if (current)
    {
        accumulateRotationalBundleRotations(current, out);
        accumulateRotationalBundleRotations(current->getChild(), out);      // +0xF0
    }
}

// Accumulates normalized input-driven bundle rotations into out while traversing the vehicle tree.
// Maps bundle axes as X to Y, Y to X and Z to Z; player-control objects bound recursion.
void accumulateRotationalBundleRotations(IObject* object, Vec3& out)
{
    if (!object)
        return;

    if (object->getTemplate()->getClassID() == CID_RotationalBundleTemplate)
    {
        const RotationalBundleTemplate* const templ =
            static_cast<const RotationalBundleTemplate*>(object->getTemplate());
        if (static_cast<int>(templ->getInputToPitch()) != 64
            || static_cast<int>(templ->getInputToYaw()) != 64
            || static_cast<int>(templ->getInputToRoll()) != 64)
        {
            const Vec3 maxSpeed    = templ->m_maxSpeed;                      // +0x1A0
            const Vec3 maxRotation = templ->m_maxRotation;                   // +0x188
            const Vec3 minRotation = templ->m_minRotation;                   // +0x17C
            const Vec3& current = static_cast<RotationalBundle*>(object)->getCurrentRotation();   // +0x184
            static const int outIndex[3] = { 1, 0, 2 };
            for (int i = 0; i < 3; ++i)
            {
                if (fabsf(component(maxSpeed, i)) > 0.001f)
                {
                    float value;
                    if (fabsf(component(minRotation, i)) > 0.001f
                        || fabsf(component(maxRotation, i)) > 0.001f)
                    {
                        const float low = component(minRotation, i);
                        const float mid = (component(maxRotation, i) - low) * 0.5f + low;
                        const float offset = component(current, i) - mid;
                        float span;
                        if (0.0f <= offset)
                            span = component(maxRotation, i) - mid;
                        else
                            span = mid - low;
                        value = offset / fabsf(span);
                    }
                    else
                    {
                        value = component(current, i) / 180.0f;
                    }
                    component(out, outIndex[i]) = value + component(out, outIndex[i]);
                }
            }
        }
    }

    if (object->getTemplate()->getClassID() != CID_PlayerControlObjectTemplate)
    {
        accumulateRotationalBundleRotations(object->getChild(), out);       // +0xF0
        accumulateRotationalBundleRotations(L::sibling(object), out);       // +0x2C
    }
}

// Updates jump/landing state and sound from world velocity, returning the animation's vertical input.
// Landing detection queries nearby geometry and terrain and records the contact material.
float Soldier::soldierInAirAction(const Vec3& speed)
{
    if (m_byte27C)
        return speed.y;

    bool jumping;
    if (*anim_startLandAtSpeed > speed.y)
    {
        int material = 0;
        if (*anim_startLandAtHeight > 0.0f)
        {
            const Vec3& position = getAbsolutePosition();
            Vec3 from;
            from.x = position.x;
            from.y = position.y + -1.0f;
            from.z = position.z;

            const float height = *anim_startLandAtHeight;
            Vec3 ray;
            ray.x = speed.x * height;
            ray.y = speed.y * height;
            ray.z = height * speed.z;
            const float lengthSq = ray.y * ray.y + ray.x * ray.x + ray.z * ray.z;
            const float heightSq = height * height;
            if (!(lengthSq <= heightSq && !(heightSq > lengthSq)))       // not exactly `height` long
            {
                float n;
                if (lengthSq <= heightSq)
                {
                    ray.y = -1.0f;                                            // point it straight down
                    n = ray.x * ray.x + 1.0f + ray.z * ray.z;
                }
                else
                    n = lengthSq;
                if (fabsf(n - 1.0f) >= FLT_EPSILON)
                {
                    if (fabsf(n) >= FLT_EPSILON)
                    {
                        const float inv = 1.0f / sqrtf(n);
                        ray.x = ray.x * inv;
                        ray.y = ray.y * inv;
                        ray.z = inv * ray.z;
                    }
                    else
                    {
                        ray.x = 0.0f; ray.y = 0.0f; ray.z = 0.0f;
                    }
                }
                ray.x = ray.x * height;
                ray.y = ray.y * height;
                ray.z = height * ray.z;
            }

            IObject* hitObject = 0;
            Vec3 hitPoint;
            hitPoint.x = 0.0f; hitPoint.y = -10000.0f; hitPoint.z = 0.0f;
            Vec3 hitNormal;
            hitNormal.x = 0.0f; hitNormal.y = 0.0f; hitNormal.z = 0.0f;
            JumpTestPredicator predicator;
            bool hit = objectManager->intersectLine(hitObject, hitPoint, hitNormal, material,
                                                    from, ray, &predicator,
                                                    true, false, false, true, 0);
            if (!hit)
            {
                Vec3 groundPoint;
                groundPoint.x = 0.0f; groundPoint.y = -10000.0f; groundPoint.z = 0.0f;
                // the ELF asks the ClassManager for "HeightmapCluster" by name here; the tree
                // reads the same registered singleton through its global, as dropKit does.
                IHeightmapCluster* const cluster = reinterpret_cast<IHeightmapCluster*>(heightmapCluster);
                Vec3 to;
                to.x = ray.x + from.x;
                to.y = ray.y + from.y;
                to.z = ray.z + from.z;
                float fraction = 0.0f;
                unsigned char groundMaterial = 0;
                hit = cluster->intersectRayInWorldCoords(from, to, groundPoint, fraction, hitNormal,
                                                         groundMaterial, true);
                material = groundMaterial;                                   // before the test
            }
            if (hit)
            {
                m_byte27C = true;                                            // landed
                m_lastContactMaterial = material;                            // +0x2C0
                if (!m_byte27F[1])                                           // +0x280
                    m_soldierSound->trigger1PLanding();
            }
        }
        jumping = m_byte27D;
    }
    else
    {
        jumping = m_byte27D;
        if (!jumping && speed.y >= *anim_startJumpAtSpeed)
        {
            m_byte27D = true;                                                // jumping
            m_byte27C = false;
            if (!m_byte27F[1])                                               // +0x280
                m_soldierSound->jump();
            jumping = true;
        }
    }

    const bool landed = m_byte27C;
    if (!jumping)
    {
        if (landed)
            return 0.0f;
        if (speed.y > 0.0f)
            return 0.0f;
        return speed.y;
    }
    if (landed)
        return 0.0f;
    return 1.0f;
}

// Feeds movement, pose and weapon state into soldier, item and vehicle animation systems.
// Ragdolls are skipped; vehicle rotation replaces walking movement when seated.
void Soldier::updateAnimationSystems(float dt)
{
    if (m_ragDoll)
        return;

    Vec3 rotation;
    rotation.x = 0.0f; rotation.y = 0.0f; rotation.z = 0.0f;
    Vec3 movement;
    movement.x = 0.0f; movement.y = 0.0f; movement.z = 0.0f;
    SoldierPhysicsNode* const node = static_cast<SoldierPhysicsNode*>(L::physicsNode(this));   // +0x44
    float movementDt = dt;
    bool syncItemMovement = true;

    Vec3 speed = node->getAbsolutePositionalSpeed();
    Vec3 tangent;
    tangent.x = 0.0f; tangent.y = 0.0f; tangent.z = 0.0f;
    node->getCollidingMeshTangentSpeed(tangent);

    const bool tangentIsZero = fabsf(tangent.x - 0.0f) <= FLT_EPSILON
                            && fabsf(tangent.y - 0.0f) <= FLT_EPSILON
                            && fabsf(tangent.z - 0.0f) <= FLT_EPSILON;
    if (!tangentIsZero)
    {
        speed.x = speed.x - tangent.x;
        speed.y = speed.y - tangent.y;
        speed.z = speed.z - tangent.z;
        if (speed.y * speed.y + speed.x * speed.x + speed.z * speed.z < 0.49f)
        {
            speed.x = 0.0f; speed.y = 0.0f; speed.z = 0.0f;
        }
    }

    float horizontalSpeed = sqrtf(speed.z * speed.z + 0.0f + speed.x * speed.x);
    if (horizontalSpeed > 0.001f)
    {
        const Mat4& frame = getMovementMatrix();
        movement.z = frame.m[8] * speed.x + frame.m[10] * speed.z;
        if (getIsSprinting())
        {
            movement.x = 0.0f;
        }
        else
        {
            const Mat4& frame2 = getMovementMatrix();
            movement.x = frame2.m[0] * speed.x + frame2.m[2] * speed.z;
        }
        const float n = movement.x * movement.x + movement.y * movement.y + movement.z * movement.z;
        if (fabsf(n - 1.0f) >= FLT_EPSILON)
        {
            if (fabsf(n) >= FLT_EPSILON)
            {
                const float inv = 1.0f / sqrtf(n);
                movement.x = movement.x * inv;
                movement.y = movement.y * inv;
                movement.z = inv * movement.z;
            }
            else
            {
                movement.x = 0.0f; movement.y = 0.0f; movement.z = 0.0f;
            }
        }
    }
    else
    {
        movement.z = 0.0f;
        movement.x = 0.0f;
    }

    const float verticalSpeed = speed.y;
    movement.y = speed.y;

    if (IObject* const parent = L::parent(this))                              // +0x28 in a vehicle
    {
        getRotationalBundleRotation(parent, rotation);
        rotation.y = m_facingDegrees / 360.0f + rotation.y;                  // +0x1F0
        if (rotation.x < -1.0f) rotation.x = -1.0f; else if (rotation.x > 1.0f) rotation.x = 1.0f;
        if (rotation.y < -1.0f) rotation.y = -1.0f; else if (rotation.y > 1.0f) rotation.y = 1.0f;
        if (rotation.z < -1.0f) rotation.z = -1.0f; else if (rotation.z > 1.0f) rotation.z = 1.0f;
        horizontalSpeed = 0.0f;
    }
    else
    {
        // Retail reads the ground-contact byte directly and does not null-check the response.
        if (static_cast<SoldierResponsePhysics*>(static_cast<IResponsePhysics*>(
                L::responsePhysics(this)))->m_byteFA)
        {
            m_byte27D = false;
            m_byte27C = false;
            movement.y = 0.0f;
        }
        else if (m_isSkyDiving)
        {
            m_byte27D = false;
            m_byte27C = false;
            movement.y = verticalSpeed;
        }
        else
        {
            movement.y = soldierInAirAction(speed);
        }

        if (m_byte27D && getCanFireWeapon())
        {
            // a jumping soldier who can fire: hold the item still and snap the direction
            syncItemMovement = false;
            movementDt = 0.0f;
            if (fabsf(movement.z) <= 0.5f)
            {
                float side;
                if (movement.x <= 0.0f)
                {
                    side = -1.0f;
                    if (0.0f == movement.x)
                        side = 0.0f;
                }
                else
                    side = 1.0f;
                movement.x = side;
                movement.z = 0.0f;
            }
            else
            {
                float forward;
                if (movement.z <= 0.0f)
                {
                    forward = 0.0f;
                    if (0.0f > movement.z)
                        forward = -1.0f;
                }
                else
                    forward = 1.0f;
                movement.z = forward;
                movement.x = 0.0f;
            }
        }
        else
        {
            const Vec3& rotationalSpeed = node->getRotationalSpeed();
            rotation.z = 0.01f * rotationalSpeed.z;
            rotation.x = rotationalSpeed.x * 0.01f;
            rotation.y = rotationalSpeed.y * 0.01f;
        }
    }

    if (m_characterMode == 0)                                               // +0x284
    {
        const int pose = m_pose;                                             // +0x1FC
        if (pose == 1 || pose == 2)
        {
            const float scaled = horizontalSpeed * (pose == 1 ? *soldierAnim1pSpeedCrouch
                                                              : *soldierAnim1pSpeedProne);
            const float limit = getSoldierSpeed(3);
            float clamped = scaled;
            if (scaled < 0.0f)
                clamped = 0.0f;
            else if (clamped > limit)
                clamped = limit;
            horizontalSpeed = clamped;
        }
        if (!getCanFireWeapon())
        {
            horizontalSpeed = getSoldierSpeed(4);
            movement.x = 0.0f;
            movement.y = 0.0f;
            movement.z = 1.0f;
        }
    }

    // Selection is repeated at each shipped call site: callbacks may change character mode.
    const auto activeSystem = [this]() -> anim::IAnimationSystem*
    {
        return m_characterMode == 0 ? m_animationSystem1P : m_animationSystem3P;
    };
    activeSystem()->setTriggerMovement(movementDt, movement, horizontalSpeed, rotation);   // +0x3C
    activeSystem()->setTriggerPose(static_cast<unsigned int>(m_pose));                     // +0x2C

    IObject* const item = getItem(m_activeItemIndex);                        // +0x25C +0x1B8
    if (item && !(L::flags(item) & 1u))                                      // Object +0x04 bit 0
    {
        IAnimatedBundle* const bundle =
            static_cast<IAnimatedBundle*>(item->queryInterface(IID_IAnimatedBundle));
        if (bundle)
        {
            bundle->setActiveAnimationSystem(m_characterMode);               // +0x18
            anim::IAnimationSystem* const itemSystem =
                static_cast<anim::IAnimationSystem*>(bundle->getActiveAnimationSystem());   // +0x1C
            if (itemSystem)
            {
                activeSystem()->copyStateToSystem(itemSystem, true);         // +0x28
                if (syncItemMovement)
                    itemSystem->syncMovementAnimations(activeSystem());      // +0x44
                else
                    itemSystem->setTriggerMovement(movementDt, Vec3::getZero(), 0.0f, Vec3::getZero());
                itemSystem->update(item, dt);                                // +0x10
            }
        }
    }

    if (m_characterMode == 1 || m_grapplingHookId)                           // +0x2EC
    {
        IObject* vehicle = 0;
        if (m_usedVehicleWeak)                                               // +0x1D4
            vehicle = static_cast<WeakPtr<IObject>::Ctrl*>(m_usedVehicleWeak)->target;
        if (!vehicle && m_refBlock1D8)                                       // +0x1D8
            vehicle = static_cast<WeakPtr<IObject>::Ctrl*>(m_refBlock1D8)->target;
        if (vehicle)
        {
            IAnimatedBundle* const bundle =
                static_cast<IAnimatedBundle*>(vehicle->queryInterface(IID_IAnimatedBundle));
            if (bundle)
            {
                bundle->setActiveAnimationSystem(m_characterMode);
                anim::IAnimationSystem* const vehicleSystem =
                    static_cast<anim::IAnimationSystem*>(bundle->getActiveAnimationSystem());
                if (vehicleSystem)
                {
                    activeSystem()->copyStateToSystem(vehicleSystem, false);
                    vehicleSystem->update(vehicle, dt);
                }
            }
        }

        if (m_ikObject && m_ikTemplateComponent)                             // +0x264 +0x268
        {
            IWeaponObject* const weapon =
                static_cast<IWeaponObject*>(m_ikObject->queryInterface(IID_IWeaponObject));
            if (weapon)
            {
                if (weapon->getZoom())                                       // +0x108
                    activeSystem()->setTriggerMessage(4);                    // +0x30
                else
                    activeSystem()->clearTriggerMessage(4);                  // +0x38
                if (weapon->isFiring())                                      // +0x1C
                    activeSystem()->setTriggerMessage(1);
                else
                    activeSystem()->clearTriggerMessage(1);
            }
            resetHeadMovements();
        }
    }

    activeSystem()->update(this, dt);                                        // +0x10
}

// Evaluates soldier, item and vehicle animation layers onto the active skeleton over dt.
// When updateGeometry is true, also publishes character mode, field of view and bone transforms.
void Soldier::applyAnimationsOnSkeleton(float dt, bool updateGeometry)
{
    int mode = m_characterMode;                                              // +0x284
    if (mode == 1)
    {
        m_dword26C = WorldPref::getTickCount();                              // +0x26C
        mode = m_characterMode;
        m_dword270 = m_pose;                                                 // +0x270
    }
    anim::Skeleton* const skeleton = characterSkeleton(this, mode, m_animSkeleton, m_collisionSkeleton);
    void* const geometry = L::geometry(this);                                // +0x38
    // Publish the current character geometry and bone palette; -1 disables the first-person FOV override.
    if (updateGeometry && geometry)
    {
        geometrySetGeomIndex(geometry, mode);                                // +0x70 PE +0x6C
        geometrySetFov(geometry, m_characterMode ? -1.0f : *fov1p);           // +0x10 both ports
        geometrySetBoneTransforms(geometry,                                  // +0x58 PE +0x54
                                  skeleton->m_transforms,                    // Skeleton +0x10
                                  static_cast<unsigned int>(skeleton->m_boneCount));   // +0x14
    }

    if (m_ragDoll)                                                           // +0x220
        return;

    if (*anim_playSoldierAnimations)
        (m_characterMode == 0 ? m_animationSystem1P : m_animationSystem3P)
            ->applyOnSkeleton(skeleton, false);                              // +0x14

    IObject* const item = getItem(m_activeItemIndex);
    if (*anim_playWeaponAnimations && item && !(L::flags(item) & 1u))
    {
        IAnimatedBundle* const bundle =
            static_cast<IAnimatedBundle*>(item->queryInterface(IID_IAnimatedBundle));
        if (bundle)
        {
            bundle->setActiveAnimationSystem(m_characterMode);
            anim::IAnimationSystem* const itemSystem =
                static_cast<anim::IAnimationSystem*>(bundle->getActiveAnimationSystem());
            if (itemSystem)
                itemSystem->applyOnSkeleton(skeleton, false);
        }
    }

    IObject* vehicle = 0;
    if (m_usedVehicleWeak)
        vehicle = static_cast<WeakPtr<IObject>::Ctrl*>(m_usedVehicleWeak)->target;
    if (!vehicle && m_refBlock1D8)
        vehicle = static_cast<WeakPtr<IObject>::Ctrl*>(m_refBlock1D8)->target;

    bool thirdPerson;
    if (*anim_playVehicleAnimations && vehicle)
    {
        if (m_characterMode != 1)
        {
            thirdPerson = false;
        }
        else
        {
            IAnimatedBundle* const bundle =
                static_cast<IAnimatedBundle*>(vehicle->queryInterface(IID_IAnimatedBundle));
            if (bundle)
            {
                bundle->setActiveAnimationSystem(m_characterMode);
                anim::IAnimationSystem* const vehicleSystem =
                    static_cast<anim::IAnimationSystem*>(bundle->getActiveAnimationSystem());
                if (vehicleSystem)
                    vehicleSystem->applyOnSkeleton(skeleton, false);
            }
            thirdPerson = (m_characterMode == 1);
        }
    }
    else
    {
        thirdPerson = (m_characterMode == 1);
    }

    if (thirdPerson)
    {
        if (*anim_playSoldierPostAnimations)
            m_animationSystem3P->applyOnSkeleton(skeleton, true);            // +0x260 the post layer
        if (m_characterMode == 1)
            updateThirdPersonAnimations(dt, updateGeometry);
        else
            updateFirstPersonAnimations(dt);
    }
    else
    {
        updateFirstPersonAnimations(dt);
    }

    // Geometry alone gates this block; updateGeometry controls only the inner update.
    if (geometry)
    {
        const Mat4* const soldierTransform = &getAbsoluteTransformation();    // slot 33 +0x84
        if (updateGeometry && item && !(L::flags(item) & 1u))
        {
            // The two authored tables, indexed by character mode (0 = first person, 1 = third).
            // 54/64 is the bone the active item hangs off; 16/8 caps how many weapon bones are
            // copied out below.
            const int boneBase[2]  = { 54, 64 };
            const int boneLimit[2] = { 16,  8 };

            // A negative base falls back TO THE identity. It cannot happen with the table above,
            // and the shipped code tests for it anyway; the test is kept.
            const int base = boneBase[m_characterMode];
            const Mat4* const bone = (base < 0) ? &Mat4::getIdentity()
                                                : &skeleton->m_transforms[base];
            item->setRelativeTransformation(*bone);                           // slot 59 +0xEC
            item->getAbsoluteTransformation();                                // slot 33 forces the
                                                                              // recompute; discarded
            hackSetFov(item, m_characterMode ? -1.0f : *fov1p, true);

            if (getActiveWeapon(0))                                           // +0x1CC
            {
                // asked twice, as shipped - the test above and this fetch are two separate
                // virtual calls, not a cached pointer.
                IObject* const weaponObject = getActiveWeapon(0);
                IWeaponObject* const weapon = weaponObject
                    ? static_cast<IWeaponObject*>(
                          weaponObject->queryInterface(IID_IWeaponObject))
                    : 0;
                // Requires the active weapon to expose IWeaponObject.
                void* const weaponGeometry = weapon->getActiveWeaponGeometry();   // +0x160
                if (weaponGeometry)
                {
                    // Weapon parts: ELF 0x81B5D60 / client PE 0x61F8DE..0x61F9CB.
                    // getTemplate is deliberately fetched twice in the empty-RPG branch.
                    geometrySetGeomIndex(weaponGeometry, m_characterMode);
                    if (weapon->getTotalAmmo() <= 0
                        && geometry_part::getLod(weaponGeometry) != 1
                        && geometry_part::getLodCount(weaponGeometry) > 2)
                    {
                        if (geometry_part::getTemplate(weaponGeometry))
                        {
                            void* const objectTemplate = geometry_part::getTemplate(weaponGeometry);
                            if (geometry_part::templateNameEquals(objectTemplate, "insgr_rpg"))
                                geometry_part::setLod(weaponGeometry, 2);
                        }
                    }
                    const unsigned int parts = static_cast<unsigned int>(
                        geometry_part::count(weaponGeometry));
                    const unsigned int cap = static_cast<unsigned int>(boneLimit[m_characterMode]);
                    const unsigned int limit = parts < cap ? parts : cap;
                    for (unsigned int part = 1; part < limit; ++part)
                    {
                        const int sourceIndex = boneBase[m_characterMode] + static_cast<int>(part);
                        const Mat4* const source = sourceIndex < 0 ? &Mat4::getIdentity()
                            : &skeleton->m_transforms[sourceIndex];
                        Mat4 worldTransform = Mat4::getIdentity();
                        worldTransform.fullMult(*source, *soldierTransform);
                        skeleton->m_transforms[boneBase[m_characterMode] + part] = worldTransform;
                        // The geometry keeps this exact skeleton pointer. Never publish the
                        // temporary above: the following draw reads the now-world-space palette.
                        geometry_part::setTransform(weaponGeometry, source, static_cast<int>(part));
                    }
                }
            }
        }
    }
}

// Applies third-person head tracking, spine look and vehicle hand IK, then transforms the skeleton.
// When updateGeometry is true, geometry may restrict the transformed bone set.
void Soldier::updateThirdPersonAnimations(float dt, bool updateGeometry)
{
    anim::Skeleton* const skeleton = characterSkeleton(this, m_characterMode, m_animSkeleton, m_collisionSkeleton);
    ICameraObject* const camera = getSoldierCamera();
    IObject* const cameraObject = static_cast<IObject*>(camera->queryInterface(IID_IObject));
    const Mat4& cam = cameraObject->getAbsoluteTransformation();             // slot 33
    const Mat4& mine = getAbsoluteTransformation();

    float pitchSin = mine.m[5] * cam.m[9] + mine.m[4] * cam.m[8] + mine.m[6] * cam.m[10];
    if (pitchSin < -1.0f) pitchSin = -1.0f; else if (pitchSin > 1.0f) pitchSin = 1.0f;
    float pitchDeg = asinf(pitchSin) * -57.295776f;

    float yawSin = mine.m[1] * cam.m[9] + mine.m[0] * cam.m[8] + mine.m[2] * cam.m[10];
    if (yawSin < -1.0f) yawSin = -1.0f; else if (yawSin > 1.0f) yawSin = 1.0f;
    const float yawDeg = -asinf(yawSin) * -57.295776f;

    int pose = m_pose;                                                       // +0x1FC
    if (pose == 3)
        pitchDeg = 0.0f;

    float headYaw = 0.0f;
    ArmorGLComp* const armor = armorOf(this);                                // +0x3C
    if (armor)
    {
        if (armor->getStatus() != 1)                                         // alive
        {
            float settle = m_headPosition.x - dt;                            // +0x234
            if (0.0f > settle)
                settle = 0.0f;
            else if (settle > m_headPosition.x)
                settle = m_headPosition.x;
            m_headPosition.x = settle;
            if (settle == 0.0f)
                m_headPosition.y = yawDeg + m_headPosition.z;                // +0x238 <- +0x23C

            if (getActiveWeapon(0))
            {
                IWeaponObject* weapon = 0;
                IObject* const active = getActiveWeapon(0);
                if (active)
                    weapon = static_cast<IWeaponObject*>(active->queryInterface(IID_IWeaponObject));
                if (weapon->getZoom() || weapon->isFiring() || weapon->isReloading())   // +0x108 +0x1C, +0xB4
                    resetHeadMovements();
            }

            float target = m_headPosition.y;
            const float rate = fabsf(m_float230 - target) * *soldier_lookSmooth;   // +0x230
            if (m_float230 <= target)
            {
                const float goal = target;
                target = m_float230;
                if (goal > target)
                {
                    const float step = target + rate * dt;
                    float next;
                    if (step < -80.0f)
                        next = -80.0f;
                    else
                    {
                        next = step;
                        if (step > goal)
                            next = goal;
                    }
                    target = next;
                    m_float230 = next;
                }
            }
            else
            {
                const float step = m_float230 - rate * dt;
                if (target <= step)
                {
                    float next = step;
                    if (step > 80.0f)
                        next = 80.0f;
                    target = next;
                }
                m_float230 = target;
            }
            if (fabsf(target) < 0.001f)
            {
                target = 0.0f;
                m_float230 = 0.0f;
            }
            headYaw = target;

            if (m_soldierLookIdleTime > 0.0f)                                // +0x240
            {
                pose = m_pose;
                m_soldierLookIdleTime = m_soldierLookIdleTime - dt;
            }
            else
            {
                const float roll = static_cast<float>(rand() % 16) * 0.0625f;
                m_soldierLookIdleTime = roll * *soldier_lookIdleTime;
                if (roll >= 0.5f)
                {
                    m_headPosition.z = *soldier_lookIdleAngle;
                    if ((rand() & 1) != 0)
                        m_headPosition.z = -m_headPosition.z;
                }
                else
                {
                    m_headPosition.z = 0.0f;
                }
                pose = m_pose;
            }
        }
        else
        {
            pose = m_pose;
        }
    }

    float lookUp, lookDown;
    if (pose == 2)
    {
        lookUp = *soldier_proneLookUp;
        lookDown = *soldier_proneLookDown;
    }
    else
    {
        lookUp = *soldier_lookUp;
        lookDown = *soldier_lookDown;
    }
    float pitch = pitchDeg;
    if (lookUp > pitch)
        pitch = lookUp;
    else if (pitch > lookDown)
        pitch = lookDown;

    const float pitchQuarter = pitch * 0.25f;
    const float yawThird = yawDeg * 0.33333334f;

    Mat4 rotateX = Mat4::getIdentity();
    Mat4 rotateY = Mat4::getIdentity();
    Mat4 combined = Mat4::getIdentity();

    setRotateXDeg(rotateX, pitchQuarter);
    setRotateYDeg(rotateY, yawThird);
    combined.rotMult(rotateX, rotateY);
    setBoneOverride(skeleton, 11, combined);

    setRotateXDeg(rotateX, pitchQuarter);
    setRotateYDeg(rotateY, yawThird);
    combined.rotMult(rotateX, rotateY);
    setBoneOverride(skeleton, 12, combined);

    setRotateXDeg(rotateX, pitchQuarter);
    setRotateYDeg(rotateY, yawThird);
    combined.rotMult(rotateX, rotateY);
    setBoneOverride(skeleton, 13, combined);

    setRotateXDeg(rotateX, pitchQuarter);
    setRotateYDeg(rotateY, headYaw);
    combined.rotMult(rotateX, rotateY);
    setBoneOverride(skeleton, 47, combined);

    setRotateXDeg(combined, -pitchQuarter);
    setBoneOverride(skeleton, 74, combined);

    if (!m_inverseKinematicDisabled && m_ikObject && m_ikTemplateComponent)   // +0x27E +0x264, +0x268
    {
        const IIkTemplateComponent* const ik =
            static_cast<const IIkTemplateComponent*>(m_ikTemplateComponent);
        const Mat4& inverse = getInvAbsoluteTransformation();                // slot 34
        const Mat4& objectTransform = m_ikObject->getAbsoluteTransformation();
        Mat4 local = Mat4::getIdentity();
        local.mult(objectTransform, inverse);

        Mat4 handle = Mat4::getIdentity();
        handle.rotMult(ik->getRotationLeftMat(), local);                     // +0x3C
        const Vec3& positionLeft = ik->getPositionLeft();                    // +0x30
        handle.m[12] = local.m[4] * positionLeft.y + local.m[0] * positionLeft.x + local.m[8] * positionLeft.z + local.m[12];
        handle.m[13] = local.m[9] * positionLeft.z + local.m[5] * positionLeft.y + local.m[1] * positionLeft.x + local.m[13];
        handle.m[14] = positionLeft.z * local.m[10] + positionLeft.y * local.m[6] + positionLeft.x * local.m[2] + local.m[14];
        const Vec3& poleLeft = ik->getPoleVectorLeft();                      // +0x44
        Vec3 target;
        target.x = local.m[8] * poleLeft.z + local.m[4] * poleLeft.y + local.m[0] * poleLeft.x;
        target.y = local.m[9] * poleLeft.z + local.m[5] * poleLeft.y + local.m[1] * poleLeft.x;
        target.z = poleLeft.z * local.m[10] + poleLeft.y * local.m[6] + poleLeft.x * local.m[2];
        m_collisionSkeleton->applyIk(18, 19, target, handle);

        handle = Mat4::getIdentity();
        handle.rotMult(ik->getRotationRightMat(), local);                    // +0x58
        const Vec3& positionRight = ik->getPositionRight();                  // +0x4C
        handle.m[12] = local.m[4] * positionRight.y + local.m[0] * positionRight.x + local.m[8] * positionRight.z + local.m[12];
        handle.m[13] = local.m[9] * positionRight.z + local.m[5] * positionRight.y + local.m[1] * positionRight.x + local.m[13];
        handle.m[14] = positionRight.z * local.m[10] + positionRight.y * local.m[6] + positionRight.x * local.m[2] + local.m[14];
        const Vec3& poleRight = ik->getPoleVectorRight();                    // +0x60
        Vec3 targetRight;
        targetRight.x = local.m[8] * poleRight.z + local.m[4] * poleRight.y + local.m[0] * poleRight.x;
        targetRight.y = local.m[9] * poleRight.z + local.m[5] * poleRight.y + local.m[1] * poleRight.x;
        targetRight.z = poleRight.z * local.m[10] + poleRight.y * local.m[6] + poleRight.x * local.m[2];
        m_collisionSkeleton->applyIk(34, 34, targetRight, handle);
    }

    Mat4 root = Mat4::getIdentity();
    root.m[12] = 0.0f;
    root.m[13] = -1.0f;
    root.m[14] = 0.0f;
    if (updateGeometry)
    {
        if (void* const geometry = L::geometry(this))
        {
            // Transform only bones used by the soldier, kit, and active weapon.
            std::vector<int> usedBones;
            usedBones.reserve(static_cast<std::size_t>(skeleton->m_boneCount));   // Skeleton +0x14

            geometryGatherUsedBones(geometry, &usedBones);                        // +0x5C PE +0x58

            // THE KIT IS reached through ITS OWN geometry, and both tests are separate - a
            // soldier with a kit whose geometry is null simply contributes nothing.
            if (m_kit)                                                            // +0x1C0
            {
                if (void* const kitGeometry = L::geometry(m_kit))                 // +0x38
                    geometryGatherUsedBones(kitGeometry, &usedBones);
            }

            // Weapon bones form a contiguous range beginning at 64, capped at eight.
            if (IObject* const weaponObject = getActiveWeapon(0))                 // +0x1CC
            {
                IWeaponObject* const weapon = static_cast<IWeaponObject*>(
                    weaponObject->queryInterface(IID_IWeaponObject));
                if (weapon)
                {
                    // NULL-tested here, unlike arm 2's call to the same accessor. The shipped
                    // code guards this one and not that one; both are reproduced as written.
                    if (void* const weaponGeometry = weapon->getActiveWeaponGeometry()) // +0x160
                    {
                        // / client PE 0x61EBC0: unsigned count, capped at eight.
                        const unsigned int parts = static_cast<unsigned int>(
                            geometry_part::count(weaponGeometry));
                        const unsigned int limit = parts < 8u ? parts : 8u;
                        for (unsigned int part = 0; part < limit; ++part)
                            usedBones.push_back(static_cast<int>(64u + part));
                    }
                }
            }

            skeleton->transformUsedBones(&root, usedBones);
        }
        else
        {
            skeleton->transform(&root, -1);
        }
    }
    else
    {
        skeleton->transform(&root, -1);
    }
}

// Leaves output untouched when disabled or when a living soldier uses third-person mode.
// First-person animation composes bind-pose correction and input camera; ragdolls rotate existing output.
bool Soldier::applyCameraAnimation(const Mat4& input, Mat4& output)
{
    if (!*anim_playCameraAnimations || (m_characterMode == 1 && !m_ragDoll))
        return false;

    if (m_ragDoll)
    {
        Mat4 rotation = Mat4::getIdentity();
        setRotationZXY(rotation, -m_float328[1], -m_float328[0], -m_float324);
        const Mat4 previous = output;
        output.rotMult(previous, rotation);
        output.m[12] = m_float318;
        output.m[13] = m_float31C[0];
        output.m[14] = m_float31C[1];
    }
    else
    {
        anim::Skeleton* const skeleton = *reinterpret_cast<anim::Skeleton**>(
            reinterpret_cast<unsigned char*>(&m_animSkeleton)
            + 2 * sizeof(void*) * m_characterMode);
        const Mat4 root = skeleton->m_bones[0].m_transform;
        Mat4 animatedRoot = Mat4::getIdentity();
        animatedRoot.mult(root, m_soldierTemplate->m_bone0InverseTransform);
        output.mult(animatedRoot, input);
        m_mat33C = input;
    }
    return true;
}

// applySpringFunction - ELF 0x81B3CD0 (102 B), client PE 0x61CA50 (126 B).
// The minimum restoring step is independent of dt; it still decays a spring at dt == 0.
// The ELF mangling proves (float, float&, float); PE passes the reference in ECX.
void applySpringFunction(float stiffness, float& displacement, float dt)
{
    float step = displacement * displacement * stiffness * dt;
    if (step < *anim_soldier_spring_clamp)
        step = *anim_soldier_spring_clamp;
    if (displacement > 0.0f)
    {
        displacement -= step;
        if (displacement <= 0.0001f)
            displacement = 0.0f;
    }
    else if (displacement < 0.0f)
    {
        displacement += step;
        if (displacement >= -0.0001f)
            displacement = 0.0f;
    }
}

// Soldier::updateFirstPersonAnimations - ELF 0x81B3D40 (2,560 B), client PE
// ..0x61DEC5 (1,573 B), read in full. Camera and animated-item interface results are
// deliberately unguarded where retail dereferences them. The camera matrix is relative
// (IObject ELF +0xE8 / PE +0xE4), not its world transform. The final skeleton therefore remains
// in soldier space; geometry supplies the soldier's world transform separately.
void Soldier::updateFirstPersonAnimations(float dt)
{
    if (m_ragDoll)
        return;

    IObject* const weaponObject = getActiveWeapon(0);
    IWeaponObject* const weapon = weaponObject
        ? static_cast<IWeaponObject*>(weaponObject->queryInterface(IID_IWeaponObject)) : 0;
    ICameraObject* const camera = getSoldierCamera();
    IObject* const cameraObject = static_cast<IObject*>(camera->queryInterface(IID_IObject));
    const Mat4& cameraTransformation = cameraObject->getRelativeTransformation();
    IObject* const item = getItem(m_activeItemIndex);

    anim::CameraSpring spring;
    anim::IAnimationSystem* system = m_characterMode == 0 ? m_animationSystem1P : m_animationSystem3P;
    if (system->getTemplate()->getCameraSpring())
    {
        // Both binaries repeat the system/template lookup after testing the pointer.
        system = m_characterMode == 0 ? m_animationSystem1P : m_animationSystem3P;
        spring = *system->getTemplate()->getCameraSpring();
    }
    if (item && !(L::flags(item) & 1u))
    {
        IAnimatedBundle* const bundle =
            static_cast<IAnimatedBundle*>(item->queryInterface(IID_IAnimatedBundle));
        anim::IAnimationSystem* const itemSystem =
            static_cast<anim::IAnimationSystem*>(bundle->getActiveAnimationSystem());
        if (itemSystem && itemSystem->getTemplate()->getCameraSpring())
            spring.merge(itemSystem->getTemplate()->getCameraSpring());
    }

    const Mat4& movement = getMovementMatrix();
    Vec3& displacement = m_cameraSpringDisplacement;
    applySpringFunction(spring.stiffness.x, displacement.x, dt);
    applySpringFunction(spring.stiffness.y, displacement.y, dt);
    applySpringFunction(spring.stiffness.z, displacement.z, dt);
    displacement.x -= spring.look.x * m_yawDeltaThisTick * dt;
    displacement.y += spring.look.y * m_pitchDeltaThisTick * dt;

    SoldierPhysicsNode* const node = static_cast<SoldierPhysicsNode*>(L::physicsNode(this));
    Vec3 speed = node->getAbsolutePositionalSpeed();
    Vec3 tangent = { 0.0f, 0.0f, 0.0f };
    node->getCollidingMeshTangentSpeed(tangent);
    if (!(std::fabs(tangent.x) <= FLT_EPSILON
          && std::fabs(tangent.y) <= FLT_EPSILON
          && std::fabs(tangent.z) <= FLT_EPSILON))
    {
        speed.x -= tangent.x;
        speed.y -= tangent.y;
        speed.z -= tangent.z;
        if (speed.x * speed.x + speed.y * speed.y + speed.z * speed.z < 0.48999998f)
            speed = Vec3::getZero();
    }
    const float side = (speed.z * movement.m[2] + speed.y * movement.m[1]
                        + speed.x * movement.m[0]) * spring.move.x * dt;
    displacement.x += side;
    displacement.y += std::fabs(side) * -0.30000001f
        - (speed.z * movement.m[10] + speed.y * movement.m[9]
           + speed.x * movement.m[8]) * spring.move.z * dt;
    displacement.y += (speed.z * movement.m[6] + speed.y * movement.m[5]
                       + speed.x * movement.m[4]) * spring.move.y * dt;

    const float recoil = getPitchRecoil() * dt;
    displacement.x += spring.fire.x * recoil;
    displacement.y += spring.fire.y * recoil;
    displacement.z += spring.fire.z * recoil;
    for (int axis = 0; axis != 3; ++axis)
    {
        float& value = component(displacement, axis);
        if (value >= 1.0f) value = 1.0f;
        if (value < -1.0f) value = -1.0f;
    }

    Mat4 root = Mat4::getIdentity();
    root.mult(m_soldierTemplate->m_bone0InverseTransform, cameraTransformation);
    Vec3 amplitude = spring.amplitude;
    IPlayer* const player = getOccupingPlayer();
    if (player && weapon)
    {
        const float fov = player->getFov();
        const float cameraFov = player->getCamera()->getActiveFOV();
        const float zoomFov = weapon->getZoomFactor(1);
        if (fov < cameraFov && zoomFov > 0.0f)
        {
            player->getEndFov();                       // shipped call, result discarded
            const float zoom = 1.0f - (fov - zoomFov) / (cameraFov - zoomFov);
            Vec3 damping = spring.zoomDamping;
            damping.x *= *anim_soldier_spring_zoom_factor;
            damping.y *= *anim_soldier_spring_zoom_factor;
            damping.z *= *anim_soldier_spring_zoom_factor;
            damping.x = damping.x <= 0.0f ? 0.0f : 1.0f / damping.x;
            damping.y = damping.y <= 0.0f ? 0.0f : 1.0f / damping.y;
            damping.z = damping.z <= 0.0f ? 0.0f : 1.0f / damping.z;
            const float unzoomed = 1.0f - zoom;
            amplitude.x = damping.x * amplitude.x * zoom + unzoomed * amplitude.x;
            amplitude.y = damping.y * amplitude.y * zoom + unzoomed * amplitude.y;
            amplitude.z = damping.z * amplitude.z * zoom + unzoomed * amplitude.z;
        }
    }
    if (std::fabs(amplitude.x) < 0.001f) amplitude.x = 0.0f;
    if (std::fabs(amplitude.y) < 0.001f) amplitude.y = 0.0f;
    if (std::fabs(amplitude.z) < 0.001f) amplitude.z = 0.0f;
    const Vec3 offset = { amplitude.x * displacement.x, amplitude.y * displacement.y,
                          amplitude.z * displacement.z };
    root.m[12] += offset.z * cameraTransformation.m[8] + offset.y * cameraTransformation.m[4]
                + offset.x * cameraTransformation.m[0];
    root.m[13] += offset.x * cameraTransformation.m[1] + offset.z * cameraTransformation.m[9]
                + offset.y * cameraTransformation.m[5];
    root.m[14] += offset.x * cameraTransformation.m[2] + offset.z * cameraTransformation.m[10]
                + offset.y * cameraTransformation.m[6];
    characterSkeleton(this, m_characterMode, m_animSkeleton, m_collisionSkeleton)->transform(&root, -1);
}

// ds:0x8AADA00 - a byte this TU owns and Soldier::enableRagDoll zeroes (0x81B66F4). Its only other
// users are the get/set arms of a bool console property in the world-templates module (that TU's
// ConsoleClass257, 0x85B9630: `get` returns the byte, `set` stores it). The property's name is in
// an unnamed constructor and is [UNRESOLVED]; so is the role. Namespace-scope, as the cross-TU
// reference requires. [INFERRED name]
unsigned char g_soldierByte8AADA00 = 0;

namespace {

// The inlined dice::hfe::Vec3::normalize (unit vectors untouched, degenerate ones zeroed), as
// triggerDieAnimation carries it twice.
inline void normalizeHitDirection(Vec3& v)
{
    const float n = v.y * v.y + v.x * v.x + v.z * v.z;
    if (fabsf(n - 1.0f) < FLT_EPSILON)
        return;
    if (fabsf(n) >= FLT_EPSILON)
    {
        const float inv = 1.0f / sqrtf(n);
        v.x *= inv;
        v.y *= inv;
        v.z *= inv;
    }
    else
    {
        v.x = 0.0f;
        v.y = 0.0f;
        v.z = 0.0f;
    }
}

} // namespace

// Death transitions transfer stored impulses to a ragdoll owned by the soldier.

// applyExplosionForce - 0x81B6E80, 105 B. Stored, not applied: enableRagDoll hands m_explosionForce
// to the new ragdoll as the front-hit force (-2), and triggerDieAnimation lets it override the
// last-hit force. Ignored while an existing ragdoll is asleep (+0x19). Return type [CHOSEN] void.
void Soldier::applyExplosionForce(const Vec3& force)
{
    if (m_ragDoll && !m_ragDoll->m_awake)
        return;
    const float k = *rd_expForceMod;
    m_explosionForce.x = force.x * k;
    m_explosionForce.y = force.y * k;
    m_explosionForce.z = k * force.z;
    m_explosionForceTime = 0.5f;                                             // +0x250
}

void Soldier::wakeUpRagdoll()
{
    if (m_ragDoll)
        m_ragDoll->wakeUp();
}

// Creates a ragdoll once, inheriting velocity and pending impulses from the soldier.
// Disables soldier physics and held items, then tracks the ragdoll's position.
void Soldier::enableRagDoll()
{
    if (m_ragDoll)
        return;

    g_soldierByte8AADA00 = 0;
    updateFlags(0x100u, 0u);

    SoldierPhysicsNode* const node = static_cast<SoldierPhysicsNode*>(L::physicsNode(this));   // +0x44
    Vec3 velocity = node->getAbsolutePositionalSpeed();
    {
        const Vec3& local = node->getPositionalSpeed();
        if (sqrtf(local.x * local.x + local.y * local.y + local.z * local.z)
            > sqrtf(velocity.x * velocity.x + velocity.y * velocity.y + velocity.z * velocity.z) * 1.5f)
            velocity = local;
    }
    if (WorldPref::getTickCount() < m_hitByVehicleSpeedTicks)
    {
        velocity = m_hitByVehicleSpeed;
        m_hitByVehicleSpeedTicks = 0;
    }

    node->setEnabled(false);
    static_cast<IResponsePhysics*>(L::responsePhysics(this))->setEnabled(false);            // +0x48
    resetPhysics(this, false);

    anim::Skeleton* const skeleton = m_collisionSkeleton;
    applyAnimationsOnSkeleton(0.0f, true);
    Mat4 skeletonOffset = Mat4::getIdentity();
    skeletonOffset.m[13] = -1.0f;
    skeleton->transform(&skeletonOffset, -1);

    m_ragDoll = anim::g_ragDollTemplate->makeInstance(getAbsoluteTransformation(), velocity,
                                                      *m_collisionSkeleton);

    IPlayer* const player = getOccupingPlayer();
    if (!gameLogic->isServer() && player)
        m_ragDoll->setIsClient(true);

    if (m_explosionForce.x * m_explosionForce.x + m_explosionForce.y * m_explosionForce.y
        + m_explosionForce.z * m_explosionForce.z > 0.0f)
    {
        m_ragDoll->addForce(-2, m_explosionForce);
        m_explosionForce.x = 0.0f;
        m_explosionForce.y = 0.0f;
        m_explosionForce.z = 0.0f;
    }

    // 5. the look-state rebase off m_mat33C
    m_float330 = 0.0f;
    m_float334[0] = -90.0f;
    m_float334[1] = 90.0f;
    {
        float rz = 0.0f, rx = 0.0f, ry = 0.0f;
        getRotationZXY(m_mat33C, rz, rx, ry);
        Mat4 yaw = Mat4::getIdentity();
        setRotationZXY(yaw, 0.0f, 0.0f, ry);
        const float x = m_float330;
        const float y = m_float334[0];
        const float z = m_float334[1];
        const float nx = yaw.m[4] * y + yaw.m[0] * x + yaw.m[8] * z;
        const float ny = yaw.m[5] * y + yaw.m[1] * x + yaw.m[9] * z;
        const float nz = y * yaw.m[6] + x * yaw.m[2] + z * yaw.m[10];
        m_float330 = nx;
        m_float334[0] = ny;
        m_float334[1] = nz;
    }
    m_float318 = m_mat33C.m[12];
    m_float31C[0] = m_mat33C.m[13];
    m_float31C[1] = m_mat33C.m[14];
    {
        float rz = 0.0f, rx = 0.0f, ry = 0.0f;
        getRotationZXY(m_mat33C, rz, rx, ry);
        m_float324 = -ry;
        m_float328[0] = -rx;
        m_float328[1] = -rz;
    }

    if (m_activeItemIndex != -1)
    {
        IObject* const item = getItem(m_activeItemIndex);
        IItemObject* itemObject = 0;
        if (item)
            itemObject = static_cast<IItemObject*>(item->queryInterface(IID_IItemObject));
        itemObject->disableItem();                                           // +0x2C unchecked
    }
    m_timeToEnableWeapon = 0.0f;

    Mat4 transform = Mat4::getIdentity();
    const Vec3& centre = m_ragDoll->update(-1.0f);
    transform.m[12] = centre.x;
    transform.m[13] = centre.y;
    transform.m[14] = centre.z;
    setAbsoluteTransformation(transform);

    m_dword224 = 0;
    if (m_ragDoll->m_settled)
        m_dword224 = static_cast<int>(WorldPref::getTickCount());
    L::interpolationTransformation(this) = 0;                                // Object +0x08
}

// Starts the death animation and, for an unparented soldier, applies the killing force to a ragdoll.
void Soldier::triggerDieAnimation()
{
    setCharacterMode(1);
    if (m_ragDoll)
        return;

    m_float2C8 = -1.0e9f;
    resetHeadMovements();
    {
        // (&m_animationSystem1P)[2 * m_characterMode]: +0x258 for mode 0, +0x260 for mode 1
        anim::IAnimationSystem* const system = m_characterMode ? m_animationSystem3P : m_animationSystem1P;
        system->playTrigger(std::string("face_dead"), true);
    }
    if (L::parent(this))                                                     // +0x28
        return;

    const Mat4& abs = getAbsoluteTransformation();
    Vec3 forward;
    forward.x = abs.m[8];
    forward.y = abs.m[9];
    forward.z = abs.m[10];

    enableRagDoll();
    anim::RagDoll* const ragDoll = m_ragDoll;
    if (!ragDoll)
        return;

    if (m_byte27F[0])
    {
        const float k = *rd_dieInVehicleForce;
        Vec3 force;
        force.y = forward.y * k;
        force.z = forward.z * k;
        force.x = k * forward.x;
        ragDoll->addForce(-1, force);
        return;
    }

    if (m_explosionForce.x * m_explosionForce.x + m_explosionForce.y * m_explosionForce.y
        + m_explosionForce.z * m_explosionForce.z > 0.0f)
        return;

    IArmorGLComp* const armor = static_cast<IArmorGLComp*>(getArmorComponent());     // +0x3C
    armor->getLastHitDirection();
    Vec3 direction = armor->getLastHitDirection();

    int bone = 47;
    const Vec3* force = &forward;
    Vec3 scaled;
    if (*rd_hitForce == 0.0f
        || direction.z * direction.z + direction.y * direction.y + direction.x * direction.x <= 0.0f)
    {
        // the facing, on the head
    }
    else
    {
        int hitBone = -1;
        const int lastHit = armor->getLastHitBoneIndex();
        if (lastHit != 255)
            hitBone = lastHit;
        if (direction.y < 0.0f)
            direction.y = 0.0f;
        normalizeHitDirection(direction);
        direction.y *= *rd_hitUp;
        normalizeHitDirection(direction);
        const float h = *rd_hitForce;
        scaled.x = direction.x * h;
        scaled.y = direction.y * h;
        scaled.z = direction.z * h;
        switch (hitBone)
        {
        case 1:                 bone = 1;  break;
        case 2:  case 3:        bone = 2;  break;
        case 6:                 bone = 6;  break;
        case 7:  case 8:        bone = 7;  break;
        case 11: case 12: case 47: bone = 47; break;
        case 15:                bone = 15; break;
        case 17:                bone = 16; break;
        case 31:                bone = 31; break;
        case 33:                bone = 32; break;
        default:                           break;
        }
        force = &scaled;
    }
    m_ragDoll->addForce(bone, *force);
}

// Restores an animated soldier from the ragdoll pose and reenables its physics.
// Requires both animation systems and physics components; destroys parked objects before clearing their ids.
void Soldier::reviveAnimations()
{
    if (m_ragDoll)
    {
        const bool forwardYIsNegative = m_ragDoll->getForward().y < 0.0f;
        anim::Skeleton* const collisionSkeleton = m_collisionSkeleton;

        const Mat4 oldTransformation = getAbsoluteTransformation();
        Mat4 revivedTransformation = oldTransformation;

        Vec3 up = m_ragDoll->getUp();
        if (!forwardYIsNegative)
        {
            up.x = -up.x;
            up.y = -up.y;
            up.z = -up.z;
        }

        Vec3& right = *reinterpret_cast<Vec3*>(&revivedTransformation.m[0]);
        Vec3& forward = *reinterpret_cast<Vec3*>(&revivedTransformation.m[4]);
        Vec3& frameUp = *reinterpret_cast<Vec3*>(&revivedTransformation.m[8]);
        frameUp = up;
        forward.x = 0.0f;
        forward.y = 1.0f;
        forward.z = 0.0f;
        right.makeOrthonormalBasis(forward, frameUp);          // return deliberately ignored

        revivedTransformation.m[13] -= -1.0f;                 // shipped spelling: move up 1 m
        setAbsoluteTransformation(revivedTransformation);

        Mat4 inverseTransformation;
        inverseTransformation.setInverse(revivedTransformation);
        Mat4 relativeTransformation;
        relativeTransformation.mult(oldTransformation, inverseTransformation);
        relativeTransformation.m[12] = 0.0f;
        relativeTransformation.m[13] = 0.0f;
        relativeTransformation.m[14] = 0.0f;
        collisionSkeleton->calculateRelativeTransforms(relativeTransformation);

        Mat4 skeletonOffset = Mat4::getIdentity();
        skeletonOffset.m[13] = -1.0f;
        collisionSkeleton->transform(&skeletonOffset, -1);

        delete m_ragDoll;
        m_ragDoll = 0;
    }

    m_helmetTime = 0.0f;
    m_pose = SoldierPose_Prone;
    m_requestedPose = SoldierPose_Prone;
    updateSoldierSpawnPoint(SoldierPose_Prone);
    m_byte37C = true;

    if (m_kit)
    {
        void* const geometry = L::geometry(m_kit);
        if (geometry)
            geometryHandleMessage(geometry, static_cast<TemplateMessage>(47), 0);
    }

    m_animationSystem1P->reset();
    m_animationSystem3P->reset();
    static_cast<SoldierPhysicsNode*>(L::physicsNode(this))->setEnabled(true);
    static_cast<IResponsePhysics*>(L::responsePhysics(this))->setEnabled(true);
    resetPhysics(this, false);

    if (m_dword2D0 != -1)
    {
        IObject* const object = objectManager->getObjectFromGrid(
            static_cast<unsigned int>(m_dword2D0));
        if (object)
            object->destroy(true);
        m_dword2D0 = -1;
    }

    if (m_dword2D4 != -1)
    {
        IObject* const object = objectManager->getObjectFromGrid(
            static_cast<unsigned int>(m_dword2D4));
        if (object)
            object->destroy(true);
        m_dword2D4 = -1;
    }
}

// Returns the ragdoll's lowest particle, or the absolute position one metre down.
Vec3 Soldier::getLowestVisiblePosition() const
{
    Vec3 result;
    result.x = 0.0f;
    result.y = 0.0f;
    result.z = 0.0f;
    if (m_ragDoll)
    {
        result = m_ragDoll->getLowestVisiblePosition();
    }
    else
    {
        const Vec3& position = getAbsolutePosition();
        result.x = position.x;
        result.z = position.z;
        result.y = position.y + -1.0f;
    }
    return result;
}

} // namespace world
} // namespace hfe
} // namespace dice
