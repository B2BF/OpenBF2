// Force-object lifecycle and input-driven force application.

#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
#include "../../../Platform/X87Compat.h" // [CHOSEN, not recovered] x87 conversion results, including invalid inputs.
#endif
#include "ForceObject.h"

#include <cfloat>                                       // FLT_EPSILON - the engine normalise rule
#include <cmath>

#include "../ForceObjectTemplate.h"
#include "../ObjectLayoutAccess.h"                      // raw +0x28 parent / +0x44 physics node
#include "../../../Core/Ids.h"                          // IID_IPlayerControlObject
#include "../../../Core/System.h"                       // WorldPref::getTickCount
#include "../../../Math/Mat4.h"
#include "../../../Physics/IPhysicsNode.h"              // +0x60 getPositionalSpeed / +0x90 add...
#include "../../../IO/Input/PlayerInput.h"
#include "../../../Scene/Object/ObjectTypes/Game/IPlayerControlObject.h"   // +0x18 isOccupied

namespace dice {
namespace hfe {
namespace world {

// SimpleObject(template); the template; +0x13C = 0; the vptrs; +0x144 = -1;
// flag 0x800 - a force object ticks from birth.
ForceObject::ForceObject(const ForceObjectTemplate* objectTemplate)
    : SimpleObject(objectTemplate)
    , m_template(objectTemplate)                                                     // +0x138
    , m_input(0.0f)                                                                  // +0x13C
    , m_dword140(0)                                                                  // [CHOSEN] - untouched by the ELF ctor
    , m_dword144(-1)                                                                 // +0x144
{
    updateFlags(0x800u, 0u);
}

ForceObject::~ForceObject() {}                                                       // ~SimpleObject


void ForceObject::disableCachedObject()
{
    SimpleObject::disableCachedObject();
    m_dword144 = -1;
}

void ForceObject::enableCachedObject()
{
    SimpleObject::enableCachedObject();
}

// / 0x82A83B0, 5 B: nothing - a force object swallows messages and never networks.
void ForceObject::handleMessage(TemplateMessage, IPlayer*, bool) {}
void ForceObject::initNetworkableComp() {}

// , 86 B: copy the authored mapped axis, otherwise clear it.
// Player, delta time and flags are unused. The retail upper-bound comparison is signed.
void ForceObject::handlePlayerInput(IPlayer*, const io::PlayerInput& input, float, unsigned int)
{
    const int axis = m_template->input;
    m_input = axis < 64 && (input.mappingFlags & (1ULL << axis)) != 0
        ? input.inputs[axis] : 0.0f;
}

// Applies input-driven force and torque after occupancy and start-delay checks. Arguments are unused; force is per tick.
// useAbsoluteDirection selects the relative transform when set. Counter-force mode gates against opposing movement.
// Repeated transform lookups preserve cache updates.
void ForceObject::handleUpdate(float, unsigned int)
{
    if (m_template->onlyWhenOccupied)                                    // +0x184
    {
        IPlayerControlObject* pco = 0;
        for (IObject* walk = this; walk; walk = object_detail::LayoutAccess::parent(walk))   // +0x28
        {
            pco = static_cast<IPlayerControlObject*>(
                      walk->queryInterface(IID_IPlayerControlObject));   // +0x0C
            if (pco)
                break;
        }
        if (!pco || !pco->isOccupied())                                  // +0x18
        {
            m_dword144 = -1;                                             // re-arm the start delay
            return;
        }
    }

    if (m_template->startDelay > 0.0f)                                   // +0x180
    {
        if (m_dword144 == -1)
        {
            // getTickCount() is called before the multiply in the image (0x82A77F3), and the
            // cast is the ordinary truncating float->int (fistp under a round-to-zero control word).
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
            m_dword144 = WorldPref::getTickCount()
                       + bf2x87::toIntTruncate(30.0f * m_template->startDelay);
#else
            m_dword144 = WorldPref::getTickCount()
                       + static_cast<int>(30.0f * m_template->startDelay);
#endif
            return;
        }
        if (m_dword144 > WorldPref::getTickCount())
            return;
    }

    IPhysicsNode* const physicsNode = static_cast<IPhysicsNode*>(
        object_detail::LayoutAccess::physicsNode(getRootParent(this)));  // raw +0x44
    if (!physicsNode)
        return;

    // inverted against the name - see the block above. Non-zero takes the relative transform.
    const Mat4& transform = m_template->useAbsoluteDirection             // +0x17D
                              ? getRelativeTransformation()              // slot 58 +0xE8
                              : getAbsoluteTransformation();             // slot 33 +0x84
    Vec3 direction;
    direction.x = transform.m[8];
    direction.y = transform.m[9];
    direction.z = transform.m[10];

    if (m_input != 0.0f)                                                 // +0x13C
    {
        const float* const rotationalForce = m_template->rotationalForce;            // +0x16C
        if (std::sqrt(rotationalForce[0] * rotationalForce[0]
                    + rotationalForce[1] * rotationalForce[1]
                    + rotationalForce[2] * rotationalForce[2]) > 0.0f)
        {
            Vec3 rotational;
            rotational.x = rotationalForce[0] * m_input;
            rotational.y = rotationalForce[1] * m_input;
            rotational.z = m_input * rotationalForce[2];
            addRotationalAcceleration(rotational);
        }
    }

    if (m_template->isCounterForce)                                      // +0x17C
    {
        IPhysicsNode* const rootNode = static_cast<IPhysicsNode*>(
            object_detail::LayoutAccess::physicsNode(getRootParent(this)));
        // a nodeless root skips the whole speed gate and gets the force anyway, as compiled.
        if (rootNode)
        {
            // computed AND discarded - see the block above. The virtual call is the point.
            const Mat4& rootTransform = getRootParent(this)->getAbsoluteTransformation();
            Vec3 rootForward;
            rootForward.x = rootTransform.m[8];
            rootForward.y = rootTransform.m[9];
            rootForward.z = rootTransform.m[10];
            (void)rootForward;

            Vec3 speed = rootNode->getPositionalSpeed();                 // +0x60
            const float speedLength = std::sqrt(speed.y * speed.y
                                              + speed.x * speed.x
                                              + speed.z * speed.z);

            // the engine normalise - the length is squared a second time here, as compiled.
            const float lengthSquared = speed.y * speed.y
                                      + speed.x * speed.x
                                      + speed.z * speed.z;
            if (!(std::fabs(lengthSquared - 1.0f) < FLT_EPSILON))
            {
                if (std::fabs(lengthSquared) < FLT_EPSILON)
                {
                    speed.x = 0.0f;
                    speed.y = 0.0f;
                    speed.z = 0.0f;
                }
                else
                {
                    const float inverseLength = 1.0f / std::sqrt(lengthSquared);
                    speed.x *= inverseLength;
                    speed.y *= inverseLength;
                    speed.z *= inverseLength;
                }
            }

            Vec3 against;
            against.x = -direction.x;
            against.y = -direction.y;
            against.z = -direction.z;

            if (speedLength * (speed.y * against.y
                             + speed.x * against.x
                             + speed.z * against.z) < 4.0f)
                return;
        }
    }

    const float magnitude = m_input * m_template->force                  // +0x168
                          + m_template->constantForce;                   // +0x178
    if (magnitude == 0.0f)
        return;

    Vec3 force;
    force.x = direction.x * magnitude;
    force.y = direction.y * magnitude;
    force.z = magnitude * direction.z;
    physicsNode->addAccelerationAtAbsolutePosition(getAbsolutePosition(), force);   // +0x90
}

// (560 B), PE 0x60BE60 (451 B). Both dispatch through the absolute
// acceleration slot (ELF +0x90, PE +0x8C), at origin-centred radius offsets. There is
// no root-position addition or relative-space conversion. X and Z deliberately
// share the X lever arm; preserve the shipped pairs and their order.
void ForceObject::addRotationalAcceleration(const Vec3& acceleration)
{
    IObject* const root = getRootParent(this);
    IPhysicsNode* const node = static_cast<IPhysicsNode*>(
        object_detail::LayoutAccess::physicsNode(root));
    const float radius = root->getPhysicBoundingRadius();
    if (acceleration.x != 0.0f)
    {
        node->addAccelerationAtAbsolutePosition(Vec3{radius, 0.0f, 0.0f},
                                                Vec3{0.0f, 0.0f, -acceleration.x});
        node->addAccelerationAtAbsolutePosition(Vec3{-radius, 0.0f, 0.0f},
                                                Vec3{0.0f, 0.0f, acceleration.x});
    }
    if (acceleration.y != 0.0f)
    {
        node->addAccelerationAtAbsolutePosition(Vec3{0.0f, radius, 0.0f},
                                                Vec3{0.0f, 0.0f, -acceleration.y});
        node->addAccelerationAtAbsolutePosition(Vec3{0.0f, -radius, 0.0f},
                                                Vec3{0.0f, 0.0f, acceleration.y});
    }
    if (acceleration.z != 0.0f)
    {
        node->addAccelerationAtAbsolutePosition(Vec3{radius, 0.0f, 0.0f},
                                                Vec3{0.0f, -acceleration.z, 0.0f});
        node->addAccelerationAtAbsolutePosition(Vec3{-radius, 0.0f, 0.0f},
                                                Vec3{0.0f, acceleration.z, 0.0f});
    }
}

} // namespace world
} // namespace hfe
} // namespace dice
