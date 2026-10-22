// Path:     [INFERRED] - see GrapplingHookRopeContainer.h.
// Contents: the container's lifecycle, camera, animation and attach/detach spine recovered from
// bf2.i64 and bf2_w32ded.exe.

#include "GrapplingHookRopeContainer.h"

#include "GrapplingHookRope.h"
#include "RopeLink.h"
#include "RotationalBundle.h"
#include "Soldier.h"
#include "../GrapplingHookRopeTemplate.h"
#include "../TemplateLeaves.h"
#include "../ICameraObject.h"
#include "../IPlayer.h"
#include "../ObjectLayoutAccess.h"
#include "../../../Animation/BoneAnimation/AnimationSystem.h"
#include "../../../Core/Ids.h"
#include "../../../Game/IGame.h"
#include "../../../Game/IGameLogic.h"
#include "../../../IO/Input/PlayerInput.h"
#include "../../../Physics/HeightmapCluster.h"
#include "../../../Physics/IPhysicsNode.h"
#include "../../../Physics/IResponsePhysics.h"
#include "../../../Scene/Object/PlayerManager.h"
#include "../../GameEvents/GrapplingHookContainerDetachEvent.h"
#include "../../GameEvents/GrapplingHookContainerUpdateEvent.h"

#include <cmath>

namespace dice {
namespace hfe {
namespace world {

extern IBase* heightmapCluster;                  // ds:0x8ABAF24, Core/Globals.cpp

namespace {

template <class T>
T* grappleWeakTarget(int* control)
{
    return control ? reinterpret_cast<typename WeakPtr<T>::Ctrl*>(control)->target : 0;
}

Vec3 getObjectRotation(IObject* object)
{
    const bool hasParent = object_detail::LayoutAccess::parent(object) != 0;
    const Mat4& transform = hasParent
        ? object->getRelativeTransformation()
        : object->getAbsoluteTransformation();
    float z, x, y;
    getRotationZXY(transform, z, x, y);
    Vec3 out = { -y, -x, -z };
    return out;
}

void setObjectRotation(IObject* object, const Vec3& rotation)
{
    const bool hasParent = object_detail::LayoutAccess::parent(object) != 0;
    Mat4 transform = hasParent
        ? object->getRelativeTransformation()
        : object->getAbsoluteTransformation();
    setRotationZXY(transform, -rotation.z, -rotation.y, -rotation.x);
    if (hasParent)
        object->setRelativeTransformation(transform);
    else
        object->setAbsoluteTransformation(transform);
}

} // namespace

// ELF C2 0x817D6B0 / C1 0x817D7D0; PE constructor confirms the shifted scalar sheet.
GrapplingHookRopeContainer::GrapplingHookRopeContainer(
    const GrapplingHookRopeContainerTemplate* objectTemplate)
    : PlayerControlObject(objectTemplate)
    , m_refCount(1)
    , _float3C0(0.03f)
    , m_containerTemplate(objectTemplate)
    , m_rope(0)
    , m_soldier(0)
    , m_segment(0)
    , m_segmentIndex(0)
    , m_segmentProgress(0.5f)
    , m_initialSoldierPosition()
    , m_detachPosition()
    , _float400(-1.0f)
    , _float404(0.0f)
    , m_eventField18(0.0f)
    , _float40C(0.0f)
    , m_animationTime(0.0f)
    , _float414(0.4f)
    , m_animationState(AnimationState_4)
    , m_previousAnimationState(AnimationState_4)
{
    m_initialSoldierPosition.x = m_initialSoldierPosition.y = m_initialSoldierPosition.z = 0.0f;
    m_detachPosition.x = m_detachPosition.y = m_detachPosition.z = 0.0f;
    Object::updateFlags(0x2000u, 0u);
}

// root containers remove themselves from GameLogic before the base teardown.
GrapplingHookRopeContainer::~GrapplingHookRopeContainer()
{
    if (!getParent())
        gameLogic->removePco(this);
}

void GrapplingHookRopeContainer::addRef() { ++m_refCount; }
int GrapplingHookRopeContainer::getRef() { return m_refCount; }
int GrapplingHookRopeContainer::release()
{
    const int left = --m_refCount;
    if (left == 0)
        delete this;
    return left;
}

void* GrapplingHookRopeContainer::queryInterface(unsigned int iid) const
{
    return PlayerControlObject::queryInterface(iid);
}

// . The cached 2.5f is initialized from the first rope template seen,
// exactly like retail's function-local static. The link index is deliberately unchecked.
void GrapplingHookRopeContainer::initialize(Soldier* soldier,
                                            GrapplingHookRope* rope,
                                            unsigned int segmentIndex)
{
    setAbsoluteTransformation(soldier->getAbsoluteTransformation());
    m_soldier = soldier;
    m_rope = rope;
    m_segment = rope->m_ropeLinks[segmentIndex];
    m_segmentIndex = segmentIndex;
    m_initialSoldierPosition = soldier->getAbsolutePosition();
    setAbsolutePosition(m_segment->getMidpoint());

    static const float firstTemplateDistance = rope->m_ropeTemplate->m_climbingSpeed;
    _float3DC = firstTemplateDistance / (100.0f * rope->_float174);
    if (_float3DC > 1.0f)
        _float3DC = 1.0f;

    m_segmentProgress = 0.5f;
    _float3FC = -10.0f;
    _float414 = 0.4f;
}

// ignores the incoming force flag and always asks the base to force destruction.
void GrapplingHookRopeContainer::destroy(bool)
{
    if (m_rope)
    {
        ISoldier* soldier = m_soldier ? static_cast<ISoldier*>(m_soldier) : 0;
        m_rope->detachSoldier(soldier);
    }
    PlayerControlObject::destroy(true);
}

// . Preserve the repeated weak-target load around the null test.
io::INetworkable* GrapplingHookRopeContainer::getNetworkable() const
{
    if (!grappleWeakTarget<IObject>(m_occupyingObject))
        return 0;
    return grappleWeakTarget<IObject>(m_occupyingObject)->getNetworkable();
}

// ordinary vehicle entry is refused; GrapplingHookRope uses _enter.
bool GrapplingHookRopeContainer::enter(IPlayer*)
{
    return false;
}

// . The camera query result is intentionally discarded.
void GrapplingHookRopeContainer::handleFrameUpdate(float deltaTime)
{
    if (!m_soldier->getOccupingPlayer())
        exit();

    _float3C0 = deltaTime;
    if (m_rope)
    {
        getActiveCamera()->queryInterface(IID_IObject);
        updateClimbAnimation();
        handleClimber();
        if (_float414 > 0.0f)
            _float414 -= deltaTime;
    }
}

// . Animation state values are the trigger masks themselves.
void GrapplingHookRopeContainer::setState(AnimationState state)
{
    m_previousAnimationState = m_animationState;
    m_animationState = state;
    m_activeAnimationSystem->clearTriggerMessage(
        static_cast<unsigned int>(m_previousAnimationState));
    m_activeAnimationSystem->setTriggerOnceMessage(static_cast<unsigned int>(m_animationState));
    m_animationTime = 0.0f;

    if (state == AnimationState_1)
        _float40C = 1.3f;
    else if (state == AnimationState_2 || state == AnimationState_4)
        _float40C = 0.0f;

    float length;
    float time;
    m_activeAnimationSystem->getPlayingAnimationLength(length, time, false);
}

// walk the raw sibling chain and return the first camera-bearing child object.
IObject* GrapplingHookRopeContainer::getCamera()
{
    IObject* child = getChild();
    if (!child)
        return 0;
    while (!child->queryInterface(IID_ICameraObject))
    {
        child = child->getSibling();
        if (!child)
            return 0;
    }
    return child;
}

// . Remote/no-player state is applied; the local human owns its interpolation.
void GrapplingHookRopeContainer::setCurrentState(unsigned int segmentIndex, float t)
{
    if (!m_soldier ||
        !m_soldier->getOccupingPlayer() ||
        m_soldier->getOccupingPlayer() != playerManager->getLocalHumanPlayer())
    {
        m_segmentIndex = segmentIndex;
        m_segment = m_rope->m_ropeLinks[segmentIndex];
        m_segmentProgress = t;

        const Vec3& start = m_segment->m_startParticle->m_position;
        const Vec3& end = m_segment->m_endParticle->m_position;
        Vec3 position;
        position.x = start.x + (end.x - start.x) * t;
        position.y = start.y + (end.y - start.y) * t;
        position.z = start.z + (end.z - start.z) * t;
        setAbsolutePosition(position);
    }
}

bool GrapplingHookRopeContainer::isAllowedToExit() { return false; }
bool GrapplingHookRopeContainer::allowEnterOtherVehicle() { return true; }

// . The all-zero detach location is replaced with the container's current position;
// the three epsilon tests and the two getOccupingPlayer calls are both observable retail details.
void GrapplingHookRopeContainer::exit()
{
    if (!m_soldier)
        return;

    if (m_soldier->getOccupingPlayer() && gameLogic->isServer())
    {
        const float epsilon = 1.1920929e-7f;
        if (std::fabs(m_detachPosition.x) <= epsilon &&
            std::fabs(m_detachPosition.y) <= epsilon &&
            std::fabs(m_detachPosition.z) <= epsilon)
        {
            m_detachPosition = getAbsolutePosition();
        }

        game->sendGameEventToAllPlaying(new GrapplingHookContainerDetachEvent(
            m_soldier->getOccupingPlayer()->getId(), m_detachPosition));
    }

    if (m_rope)
    {
        setAbsolutePosition(m_detachPosition);
        ISoldier* soldier = m_soldier ? static_cast<ISoldier*>(m_soldier) : 0;
        m_rope->detachSoldier(soldier);
        destroy(true);
    }
}

// . This is the rope-only mount path called by
// GrapplingHookRope::initContainer; ordinary enter() remains a shipped false gate.
bool GrapplingHookRopeContainer::_enter(IPlayer* player)
{
    IObject* vehicle = player->getVehicle();
    IPlayerControlObject* vehiclePco = static_cast<IPlayerControlObject*>(
        vehicle->queryInterface(IID_IPlayerControlObject));

    ICameraObject* activeCamera = vehiclePco->getActiveCamera();
    IObject* activeCameraObject = static_cast<IObject*>(
        activeCamera->queryInterface(IID_IObject));
    Vec3 savedRotation = { 0.0f, 0.0f, 0.0f };
    if (activeCameraObject)
        savedRotation = getObjectRotation(activeCameraObject);
    const CameraViewMode savedViewMode = activeCamera->getViewMode();

    vehiclePco->onEnter(this);
    const bool entered = PlayerControlObject::enter(player);

    IObject* occupant = grappleWeakTarget<IObject>(m_occupyingObject);
    if (occupant)
    {
        occupant->updateFlags(0, 1);
        updateFlags(0x2000u, 1u);

        IPhysicsNode* physicsNode = static_cast<IPhysicsNode*>(
            object_detail::LayoutAccess::physicsNode(occupant));
        physicsNode->reset(true);

        IResponsePhysics* response = static_cast<IResponsePhysics*>(
            object_detail::LayoutAccess::responsePhysics(occupant));
        response->reset();
        const Vec3 stop = { 0.0f, 0.0f, 0.0f };
        response->setSurfacePositionalSpeed(stop);
    }

    // Retail repeats getCamera for each operation, including a discarded IObject query.
    if (getCamera())
    {
        IObject* cameraObject = getCamera();
        ICameraObject* camera = static_cast<ICameraObject*>(
            cameraObject->queryInterface(IID_ICameraObject));
        camera->queryInterface(IID_IObject);
        camera->setViewMode(savedViewMode, 0, false);

        Vec3 rotation = { 0.0f, savedRotation.y, 0.0f };
        setObjectRotation(getCamera(), rotation);
        static_cast<RotationalBundle*>(getCamera())->setCurrentRotation(rotation);
    }

    return entered;
}

// . The transform copy is dead in source-visible behaviour but retained because both
// shipped ports materialize it before the base exit. The method assumes a live occupant/components.
void GrapplingHookRopeContainer::_exit()
{
    IObject* occupant = grappleWeakTarget<IObject>(m_occupyingObject);
    IPlayerControlObject* occupantPco = static_cast<IPlayerControlObject*>(
        occupant->queryInterface(IID_IPlayerControlObject));
    Mat4 unusedTransform = getAbsoluteTransformation();
    (void)unusedTransform;

    PlayerControlObject::exit();
    occupantPco->onExit();

    IPhysicsNode* physicsNode = static_cast<IPhysicsNode*>(
        object_detail::LayoutAccess::physicsNode(occupant));
    physicsNode->reset(true);
    IResponsePhysics* response = static_cast<IResponsePhysics*>(
        object_detail::LayoutAccess::responsePhysics(occupant));
    response->reset();
}

// . Above 40 metres retail copies the input and applies a third-row
// offset scaled by 1-cos(0), which is deliberately zero; below/equal to 40 it returns false.
bool GrapplingHookRopeContainer::applyCameraAnimation(const Mat4& input, Mat4& output)
{
    const Mat4& transform = getAbsoluteTransformation();
    if (transform.m[13] <= 40.0f)
        return false;

    output = input;
    const float factor = 1.0f - std::cos(0.0f);
    output.m[12] += transform.m[8] * factor;
    output.m[13] += transform.m[9] * factor;
    output.m[14] += transform.m[10] * factor;
    return true;
}

// . Free-camera players are ignored. The occupied object receives a
// flags-zero input first, followed by every registered input object with the caller's flags. The
// server owns use/action dismounts; the clamped throttle sample is updated on both sides.
void GrapplingHookRopeContainer::handlePlayerInput(IPlayer* player,
                                                   const io::PlayerInput& input,
                                                   float deltaTime,
                                                   unsigned int flags)
{
    if (!player || player->getFreeCameraEnabled())
        return;

    int* const occupyingControl = m_occupyingObject;
    if (grappleWeakTarget<IObject>(occupyingControl))
    {
        grappleWeakTarget<IObject>(occupyingControl)->handlePlayerInput(
            player, input, deltaTime, 0);
    }

    for (std::list<IObject*>::iterator it = m_playerInputObjects.begin();
         it != m_playerInputObjects.end(); ++it)
    {
        (*it)->handlePlayerInput(player, input, deltaTime, flags);
    }

    if (gameLogic->isServer())
    {
        if (_float400 == -1.0f)
        {
            float use = 0.0f;
            if ((input.mappingFlags & (1ULL << io::PIUse)) != 0)
                use = input.inputs[io::PIUse];
            if (use == 0.0f)
                _float400 = 0.0f;
        }
        else if (_float414 <= 0.0f)
        {
            float use = 0.0f;
            if ((input.mappingFlags & (1ULL << io::PIUse)) != 0)
                use = input.inputs[io::PIUse];
            _float400 = use;
        }

        if (_float400 > 0.0f)
        {
            for (std::vector<RopeLink*>::iterator it = m_rope->m_ropeLinks.begin() + 1;
                 it != m_rope->m_ropeLinks.end() - 1; ++it)
            {
                if (m_segment == *it)
                {
                    const int direction = m_segment->m_climbDirection;
                    const int runLength = m_rope->findUnclimbableIndexFromLink(it, direction);
                    RopeLink* candidate = m_rope->m_ropeLinks[
                        m_segmentIndex + direction * runLength];
                    if (candidate == m_rope->m_ropeLinks[0])
                        candidate = m_rope->m_ropeLinks[1];
                    else if (candidate == m_rope->m_ropeLinks[m_rope->_dword168 + 1])
                        candidate = m_rope->m_ropeLinks[m_rope->_dword168];

                    const Vec3 midpoint = candidate->getMidpoint();
                    const Vec3& soldierPosition =
                        player->getDefaultVehicle()->getAbsolutePosition();
                    const float dx = midpoint.x - soldierPosition.x;
                    const float dy = midpoint.y - soldierPosition.y;
                    const float dz = midpoint.z - soldierPosition.z;
                    const float radius = m_containerTemplate->m_float30C;
                    if (radius * radius > dz * dz + dy * dy + dx * dx)
                    {
                        Vec3 endpoint = candidate->m_startParticle->m_position;
                        if (candidate->m_endParticle->m_position.y
                            > candidate->m_startParticle->m_position.y)
                        {
                            endpoint = candidate->m_endParticle->m_position;
                        }
                        m_detachPosition = m_rope->getDismountPosition(
                            endpoint, m_initialSoldierPosition, *candidate);
                        setAbsolutePosition(m_detachPosition);
                        exit();
                    }
                }
            }
        }

        _float404 = 0.0f;
        if ((input.mappingFlags & (1ULL << io::PIAction)) != 0)
            _float404 = input.inputs[io::PIAction];
        if (_float404 != 0.0f)
        {
            const Vec3 position = getAbsolutePosition();
            m_detachPosition = m_rope->getDismountPosition(
                position, m_initialSoldierPosition, *m_segment);
            setAbsolutePosition(m_detachPosition);
            exit();
        }
    }

    float throttle = 0.0f;
    if ((input.mappingFlags & (1ULL << io::PIThrottle)) != 0)
        throttle = input.inputs[io::PIThrottle];
    if (throttle < -1.0f)
        throttle = -1.0f;
    else if (throttle > 1.0f)
        throttle = 1.0f;
    m_eventField18 = throttle;
    if (m_soldier->m_lookUpDownAngle > 0.0f)
        m_eventField18 = -throttle;
}
// . State 1 follows the currently playing clip's time while its
// countdown expires; states 2/4 select the direction transition from m_eventField18.
void GrapplingHookRopeContainer::updateClimbAnimation()
{
    if (!m_activeAnimationSystem)
        return;

    if (m_animationState == AnimationState_1)
    {
        float length;
        float time;
        m_activeAnimationSystem->getPlayingAnimationLength(length, time, false);
        if (time > m_animationTime)
            m_animationTime = time;
        else if (m_eventField18 <= 0.0f)
            setState(AnimationState_4);
        else
            setState(AnimationState_1);

        _float40C -= _float3C0;
        if (_float40C < 0.0f)
            _float40C = 0.0f;
        return;
    }

    if (m_animationState == AnimationState_2)
    {
        _float40C += _float3C0 * 3.0f;
        if (_float40C > 8.0f)
            _float40C = 8.0f;

        if (m_eventField18 > 0.0f)
            setState(AnimationState_1);
        else if (m_eventField18 == 0.0f)
            setState(AnimationState_4);
        return;
    }

    if (m_animationState == AnimationState_4)
    {
        if (m_eventField18 > 0.0f)
            setState(AnimationState_1);
        else if (m_eventField18 < 0.0f)
            setState(AnimationState_2);
    }
}
// . Movement follows a run of links having the same climb direction;
// the original iterator is deliberately passed to handleDismount even after changing segments.
void GrapplingHookRopeContainer::handleClimber()
{
    for (std::vector<RopeLink*>::iterator it = m_rope->m_ropeLinks.begin() + 1;
         it != m_rope->m_ropeLinks.end() - 1; ++it)
    {
        if (m_segment != *it)
            continue;

        const int direction = m_segment->m_climbDirection;
        if (m_animationState == AnimationState_1)
        {
            m_segmentProgress += _float3C0 * 2.5f * 30.0f
                * (_float40C * _float3DC) * static_cast<float>(direction);
        }
        if (m_animationState == AnimationState_2)
        {
            m_segmentProgress -= _float40C * _float3DC
                * static_cast<float>(direction);
        }

        if (m_segmentProgress < 0.0f)
        {
            if ((*(it - 1))->m_climbDirection == direction)
            {
                --m_segmentIndex;
                m_segmentProgress += 1.0f;
                m_segment = m_rope->m_ropeLinks[m_segmentIndex];
            }
            else
            {
                m_segmentProgress = 0.0f;
            }
        }
        else if (m_segmentProgress >= 1.0f)
        {
            if ((*(it + 1))->m_climbDirection == direction)
            {
                ++m_segmentIndex;
                m_segmentProgress -= 1.0f;
                m_segment = m_rope->m_ropeLinks[m_segmentIndex];
            }
            else
            {
                m_segmentProgress = 0.99f;
            }
        }

        handleDismount(it, direction);
    }

    if (m_soldier && m_soldier->getOccupingPlayer())
    {
        if (m_soldier->getOccupingPlayer() == playerManager->getLocalHumanPlayer())
        {
            const Vec3& start = m_segment->m_startParticle->m_position;
            const Vec3& end = m_segment->m_endParticle->m_position;
            Vec3 direction = {
                end.x - start.x,
                end.y - start.y,
                end.z - start.z
            };
            Vec3 scaled = {
                direction.x * m_segmentProgress,
                direction.y * m_segmentProgress,
                direction.z * m_segmentProgress
            };
            const Vec3& base = m_segment->m_startParticle->m_position;
            Vec3 position = {
                scaled.x + base.x,
                scaled.y + base.y,
                scaled.z + base.z
            };
            setAbsolutePosition(position);
            game->sendGameEventToAllPlaying(new GrapplingHookContainerUpdateEvent(
                m_soldier->getOccupingPlayer()->getId(),
                static_cast<int>(m_segmentIndex), m_segmentProgress, m_eventField18));
        }
    }
}

// . The raised-position test dismounts at the next run boundary while
// state 1 climbs upward. Otherwise a 1.4-metre terrain ray permits the downward/end-of-run exit.
void GrapplingHookRopeContainer::handleDismount(
    std::vector<RopeLink*>::iterator link, int direction)
{
    if (!m_soldier->getOccupingPlayer() || !gameLogic->isServer())
        return;

    Vec3 raisedPosition = getAbsolutePosition();
    const float linkHeight = m_rope->_float174 * m_rope->m_ropeTemplate->m_climbableAngle;
    raisedPosition.y += linkHeight + linkHeight;

    const int runLength = m_rope->findUnclimbableIndexFromLink(link, direction);
    RopeLink* candidate = *(link + direction * runLength);
    if (candidate == m_rope->m_ropeLinks[0])
        candidate = m_rope->m_ropeLinks[1];
    else if (candidate == m_rope->m_ropeLinks[m_rope->_dword168 + 1])
        candidate = m_rope->m_ropeLinks[m_rope->_dword168];

    Vec3 endpoint = candidate->m_startParticle->m_position;
    if (candidate->m_endParticle->m_position.y > candidate->m_startParticle->m_position.y)
        endpoint = candidate->m_endParticle->m_position;

    if (raisedPosition.y > endpoint.y && m_animationState == AnimationState_1)
    {
        m_detachPosition = m_rope->getDismountPosition(
            endpoint, m_initialSoldierPosition, *candidate);
        setAbsolutePosition(m_detachPosition);
        exit();
        return;
    }

    unsigned char material = 0;
    const Vec3 from = getAbsolutePosition();
    const Vec3 downward = { 0.0f, -1.4f, 0.0f };
    const Vec3 to = {
        from.x + downward.x,
        from.y + downward.y,
        from.z + downward.z
    };
    Vec3 hitPoint = { 0.0f, 0.0f, 0.0f };
    float hitFraction;
    Vec3 hitNormal = { 0.0f, 0.0f, 0.0f };
    if (static_cast<IHeightmapCluster*>(heightmapCluster)->intersectRayInWorldCoords(
            from, to, hitPoint, hitFraction, hitNormal, material, true))
    {
        if ((m_eventField18 < 0.0f
             && hitNormal.y > m_rope->m_ropeTemplate->m_dismountAngle)
            || direction == 0)
        {
            Vec3 position = from;
            m_detachPosition = m_rope->getDismountPosition(
                position, m_initialSoldierPosition, *m_segment);
            setAbsolutePosition(m_detachPosition);
            exit();
        }
    }
}

} // namespace world
} // namespace hfe
} // namespace dice
