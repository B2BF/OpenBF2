// Grappling-rope lifecycle, simulation, traversal and shared object registry. [INFERRED] Source placement.

#include "GrapplingHookRope.h"

#include "GrapplingHookRopeContainer.h"
#include "Ladder.h"
#include "RopeLink.h"
#include "Soldier.h"
#include "../ISoldier.h"
#include "../IPlayer.h"
#include "../GrapplingHookRopeTemplate.h"
#include "../../IGame.h"
#include "../../GameLogic/Player.h"
#include "../../../Core/Ids.h"
#include "../../../Core/System.h"
#include "../../../IO/Network/INetworkable.h"
#include "../../../Physics/PhysicsUtil.h"
#include "../../../Scene/Object/ObjectManager.h"
#include "../../../Scene/Object/ObjectPredicator.h"
#include "../../../Scene/Object/ObjectTypes/Base/ObjectTemplateManager.h"
#include "../../GameEvents/GrapplingHookContainerCreateEvent.h"
#include "../../GameEvents/GrapplingHookUpdateEvent.h"
#include "../../IGameLogic.h"
#include "../../Python/PyRef.h"
#include "../../Python/PythonHost.h"
#include "../../../Scene/Object/PlayerManager.h"
#include "../../../Sound/BfSound.h"

#include <cmath>

namespace dice {
namespace hfe {
namespace world {

// ds:0x8AAD024 - constructed empty by static init, exactly like Zipline.cpp's ziplineMap and
// Ladder.cpp's ladderMap. The element type is attested by the insert_unique instantiation
// GrapplingHookRope::initialize compiles at 0x8177CBF; see the header.
std::map<int, GrapplingHookRope*> grapplingHookRopeMap;
int GrapplingHookRope::globalGrapplingHookRopeId = 0; // ds:0x8AAD020
float GrapplingHookRope::m_frameRate = 0.033333335f; // ds:0x8987A14, raw 0x3D088889
float GrapplingHookRope::m_kineticFriction = 0.1f; // ds:0x8987A04; PE 0x8301EC
float GrapplingHookRope::m_staticFriction = 0.0001f; // ds:0x8987A08; PE 0x8301F0

// ELF C2 0x8177350 / C1 0x81775D0; PE constructor independently confirms the scalar sheet and
// updateFlags(0x3000, 0). Members omitted from this list are deliberately uninitialized in retail.
GrapplingHookRope::GrapplingHookRope(const GrapplingHookRopeTemplate* objectTemplate)
    : SimpleObject(objectTemplate)
    , m_refCount(1)
    , m_soldiers()
    , m_soldierIterator()
    , m_ropeTemplate(objectTemplate)
    , m_networkableId(0)
    , m_particles()
    , m_ropeLinks()
    , m_unusedLifetime(0.0f)
    , m_riderCount(0)
    , m_globalForce()
    , m_deltaTime(0.0f)
    , m_frameAccumulator(0.0f)
    , m_constraintIterations(6)
    , _float19C(0.2f)
    , m_ownerPlayerId(-1)
    , m_creationPosition()
    , m_creationDirection()
    , _float1C0(0.5f)
    , _byte1C8(false)
    , _float1CC(0.5f)
    , m_hookParticle(0)
    , m_hookPosition()
    , _float1E4(0.3f)
    , _float1E8(1.0f)
    , m_timeAwake(0.0f)
    , m_awake(true)
    , m_hookedAwakeTime(0.0f)
    , _float1F8(6.0f)
    , m_updateEventAccumulator(0.0f)
    , m_updateEventInterval(4.0f)
    , _byte204(false)
    , m_initialized(false)
    , _float208(0.5f)
    , _byte20C(true)
    , _byte20D(true)
{
    m_creationPosition.x = m_creationPosition.y = m_creationPosition.z = 0.0f;
    m_creationDirection.x = m_creationDirection.y = m_creationDirection.z = 0.0f;
    m_globalForce.x = m_globalForce.y = m_globalForce.z = 0.0f;
    m_hookPosition.x = m_hookPosition.y = m_hookPosition.z = 0.0f;
    updateFlags(0x3000u, 0u);
}

// /0x8177880/0x81778C0: STL members own all dynamic tail storage.
GrapplingHookRope::~GrapplingHookRope()
{
}

void GrapplingHookRope::addRef() { ++m_refCount; }
int GrapplingHookRope::getRef() { return m_refCount; }
int GrapplingHookRope::release()
{
    const int left = --m_refCount;
    if (left == 0)
        delete this;
    return left;
}

// only IBase and IObject are exposed.
void* GrapplingHookRope::queryInterface(unsigned int iid) const
{
    if (iid == IID_IBase || iid == IID_IObject)
        return const_cast<GrapplingHookRope*>(this);
    return 0;
}

// / 0x8178690: exact base forwards.
void GrapplingHookRope::init()
{
    SimpleObject::init();
}

void GrapplingHookRope::handleVisualUpdate(float deltaTime, float interpolation)
{
    SimpleObject::handleVisualUpdate(deltaTime, interpolation);
}

// / PE counterpart. The fixed-step branch subtracts at most one frame even when the
// accumulator is far behind, and the strict `>` comparison is retained.
void GrapplingHookRope::handleFrameUpdate(float deltaTime)
{
    if (!m_initialized)
        return;

    handleTimers(deltaTime);
    if (m_awake && m_frameAccumulator > m_frameRate)
    {
        m_frameAccumulator -= m_frameRate;
        applyForces();
        doPhysics();
        for (int pass = 0; pass < m_constraintIterations; ++pass)
            constrainParticles();
        handleHookCollision(0);
        for (std::vector<RopeParticle>::iterator it = m_particles.begin() + 1;
             it != m_particles.end(); ++it)
            handleParticleCollision(it, 0);
        handleLinkCollision();
    }
    setRopeLinksTransformations();
    setClimbablePartsOfRope();
}

// the public helper is only the ObjectManager forward.
void GrapplingHookRope::destroyGrapplingHook()
{
    objectManager->destroyObject(this);
}

// . The incoming force argument is ignored and the base always receives true. The
// unusual restart-at-begin loop and original rider-count cap prevent an exit callback that failed
// to remove its soldier from spinning forever.
void GrapplingHookRope::destroy(bool)
{
    grapplingHookRopeMap.erase(m_networkableId);
    for (std::vector<RopeLink*>::iterator it = m_ropeLinks.begin();
         it != m_ropeLinks.end(); ++it)
        objectManager->destroyObject(*it);

    const int originalRiderCount = static_cast<int>(m_soldiers.size());
    int processed = 0;
    m_soldierIterator = m_soldiers.begin();
    while (m_soldierIterator != m_soldiers.end())
    {
        ISoldier* soldier = *m_soldierIterator;
        IObject* soldierObject = static_cast<IObject*>(soldier->queryInterface(IID_IObject));
        IObject* parent = soldierObject->getParent();
        if (parent)
        {
            GrapplingHookRopeContainer* container =
                static_cast<GrapplingHookRopeContainer*>(parent);
            container->m_detachPosition = getDismountPosition(
                parent->getAbsolutePosition(),
                container->m_initialSoldierPosition,
                *container->m_segment);
            parent->setAbsolutePosition(container->m_detachPosition);
            container->exit();
        }
        else
        {
            detachSoldier(soldier);
        }

        m_soldierIterator = m_soldiers.begin();
        if (++processed > originalRiderCount)
            break;
    }

    SimpleObject::destroy(true);
}

// / PE counterpart: dwords +0x00..+0x24 and bytes +0x28/+0x29 are zeroed. The two
// padding bytes are not written by retail and are deliberately omitted here.
RopeParticle::RopeParticle()
    : m_position()
    , m_previousPosition()
    , m_force()
    , _state24(0)
    , _byte28(false)
    , m_eventFlag(false)
{
    m_position.x = m_position.y = m_position.z = 0.0f;
    m_previousPosition.x = m_previousPosition.y = m_previousPosition.z = 0.0f;
    m_force.x = m_force.y = m_force.z = 0.0f;
}

namespace {
// The first nine directions serve attachment probes; dismount tests all 17 in first-match order.
const Vec3 kGrapplingHookDirections[17] = {
    {  0.0f,  0.0f,  0.0f },
    {  1.0f,  0.0f,  0.0f }, { -1.0f,  0.0f,  0.0f },
    {  0.0f,  0.0f,  1.0f }, {  0.0f,  0.0f, -1.0f },
    {  1.0f,  0.0f,  1.0f }, {  1.0f,  0.0f, -1.0f },
    { -1.0f,  0.0f,  1.0f }, { -1.0f,  0.0f, -1.0f },
    {  1.0f, -1.0f,  0.0f }, { -1.0f, -1.0f,  0.0f },
    {  0.0f, -1.0f,  1.0f }, {  0.0f, -1.0f, -1.0f },
    {  1.0f, -1.0f,  1.0f }, {  1.0f, -1.0f, -1.0f },
    { -1.0f, -1.0f,  1.0f }, { -1.0f, -1.0f, -1.0f }
};

void normalizeRopeVector(Vec3& value)
{
    const float epsilon = 1.1920929e-7f;
    const float lengthSquared =
        value.x * value.x + value.y * value.y + value.z * value.z;
    if (std::fabs(lengthSquared - 1.0f) >= epsilon)
    {
        if (std::fabs(lengthSquared) >= epsilon)
        {
            const float inverseLength = 1.0f / std::sqrt(lengthSquared);
            value.x *= inverseLength;
            value.y *= inverseLength;
            value.z *= inverseLength;
        }
        else
        {
            value.x = value.y = value.z = 0.0f;
        }
    }
}
} // namespace

// . This is the complete construction pass: establish the rope id/map
// entry, size the source particles, instantiate the endpoint/interior link objects, seed their
// endpoints, and hand the fully initialized particle set to the throw/drop state machine. Both
// shipped ports leave every template/object/factory lookup unchecked.
void GrapplingHookRope::initialize()
{
    m_initialized = true;
    io::INetworkable* networkable = getNetworkable();
    if (networkable)
        m_networkableId = networkable->m_networkableId;
    else
        m_networkableId = globalGrapplingHookRopeId++;

    grapplingHookRopeMap.insert(std::make_pair(m_networkableId, this));

    _float164 = m_ropeTemplate->_float174;
    _dword168 = m_ropeTemplate->_dword178;
    _dword170 = static_cast<int>(static_cast<float>(_dword168) * 0.8f);
    m_ropeLinks.clear();
    _dword16C = _dword168 + 1;
    m_particles.resize(static_cast<std::size_t>(_dword16C));

    for (int i = 0; i < _dword168 + 2; ++i)
    {
        const char* templateName = i == 1 ? "HookLink" : "RopeLink";
        IObjectTemplate* linkTemplate =
            objectTemplateManager->getTemplate(std::string(templateName));
        RopeLink* link = static_cast<RopeLink*>(linkTemplate->createObject());
        link->init();
        objectManager->setActiveObject(link);

        if (gameLogic->isServer())
        {
            IPlayer* player = playerManager->getPlayerFromId(m_ownerPlayerId);
            IObject* defaultVehicle = player->getDefaultVehicle();
            ISoldier* soldier = static_cast<ISoldier*>(
                defaultVehicle->queryInterface(IID_ISoldier));
            link->_dword14C = soldier->getKitType();
        }

        link->m_rope = this;
        if (i == 0)
        {
            link->m_startParticle = &m_particles[0];
            link->m_endParticle = &m_particles[0];
        }
        else if (i == _dword168 + 1)
        {
            link->m_startParticle = &m_particles[static_cast<std::size_t>(_dword168)];
            link->m_endParticle = &m_particles[static_cast<std::size_t>(_dword168)];
        }
        else
        {
            link->m_startParticle = &m_particles[static_cast<std::size_t>(i - 1)];
            link->m_endParticle = &m_particles[static_cast<std::size_t>(i)];
        }
        m_ropeLinks.push_back(link);
    }

    _float174 = _float164 / static_cast<float>(_dword168);
    _float178 = _float174 * _float174;
    _float19C = m_ropeTemplate->m_elasticity;
    _float1F8 = m_ropeTemplate->m_awakeTime;

    for (std::vector<RopeParticle>::iterator it = m_particles.begin();
         it != m_particles.end(); ++it)
    {
        it->m_previousPosition = m_creationPosition;
        it->m_position = it->m_previousPosition;
    }

    _particle1C4 = &m_particles[m_particles.size() - 3];
    if (m_creationScalar >= 0.8f)
        throwRope();
    else
        dropRope();
}

// . The missing-template and failed-create cases are intentionally
// unchecked in retail. The server broadcasts ownership before applying the same local state via
// initContainer; clients replay that state through GrapplingHookContainerCreateEvent.
void GrapplingHookRope::attachSoldier(Soldier* soldier, int segmentIndex, float entryAngle)
{
    if (!soldier->getOccupingPlayer())
        return;

    IObjectTemplate* objectTemplate =
        objectTemplateManager->getTemplate(std::string("GrapplingHookRopeContainer"));
    GrapplingHookRopeContainer* container = static_cast<GrapplingHookRopeContainer*>(
        objectTemplate->createObject());
    container->init();

    game->sendGameEventToAllPlaying(new GrapplingHookContainerCreateEvent(
        soldier->getOccupingPlayer()->getId(),
        m_networkableId,
        segmentIndex,
        entryAngle));
    initContainer(container, soldier, segmentIndex, entryAngle);
}

// . Preserve every occupying-player/vehicle virtual call: the shipped
// source re-queries those weak targets at each test/use, including the unguarded client-side remote
// test after container initialization. setRotateYDeg overwrites all sixteen matrix elements, so
// the soldier position is restored only after that call.
void GrapplingHookRope::initContainer(GrapplingHookRopeContainer* container,
                                      Soldier* soldier,
                                      int segmentIndex,
                                      float entryAngle)
{
    if (soldier->getOccupingPlayer())
    {
        if (soldier->getOccupingPlayer()->getVehicle())
        {
            if (soldier->getOccupingPlayer()->getVehicle()->getTemplate()->getClassID() ==
                    CID_GrapplingHookRopeContainerTemplate ||
                soldier->getOccupingPlayer()->getVehicle()->getTemplate()->getClassID() ==
                    CID_LadderContainerTemplate)
            {
                static_cast<PlayerControlObject*>(
                    soldier->getOccupingPlayer()->getVehicle())->exit();
            }
        }
    }

    Mat4 transformation = soldier->getAbsoluteTransformation();
    setRotateYDeg(transformation, entryAngle);
    const Vec3& soldierPosition = soldier->getAbsolutePosition();
    transformation.m[12] = soldierPosition.x;
    transformation.m[13] = soldierPosition.y;
    transformation.m[14] = soldierPosition.z;
    soldier->setAbsoluteTransformation(transformation);

    container->initialize(soldier, this, static_cast<unsigned int>(segmentIndex));
    container->m_entryAngle = entryAngle;

    if (!game->queryInterface(IID_IGameClient) ||
        !soldier->getOccupingPlayer()->getIsRemote())
    {
        disablePhysics(container, false);
    }

    container->updateFlags(0x100u, 0u);
    container->_enter(soldier->getOccupingPlayer());
    soldier->setGrapplingHookId(static_cast<unsigned int>(m_networkableId));
    soldier->setLookUpDownAngle(container->_float3FC);
    m_soldiers.insert(m_soldiers.begin(), static_cast<ISoldier*>(soldier));
    container->setState(GrapplingHookRopeContainer::AnimationState_4);
    ++m_riderCount;
}

void GrapplingHookRope::setAwake()
{
    // / PE counterpart. No null/container work: it resets four rope-wide values and
    // clears the event byte on every particle.
    m_hookedAwakeTime = 0.0f;
    m_timeAwake = 0.0f;
    m_awake = true;
    _byte1C8 = false;
    for (std::vector<RopeParticle>::iterator it = m_particles.begin();
         it != m_particles.end(); ++it)
        it->m_eventFlag = false;
}

// . Reached once per live rope from toggleEntryPoint. Search proceeds
// from the last link toward (but not including) link zero, preserves the two-way height search and
// its asymmetric run-length requirements, and scores candidates by distance plus yaw difference.
// The bool return is confirmed by PE's explicit AL materialization; the current caller discards it.
bool GrapplingHookRope::checkForEntry(IPlayer* player)
{
    if (!player)
        return false;

    Soldier* soldier = static_cast<Soldier*>(player->getDefaultVehicle());
    const Vec3 soldierPosition = soldier->getAbsolutePosition();
    float soldierFacing = soldier->m_facingDegrees;
    if (soldierFacing < 0.0f)
        soldierFacing += 360.0f;

    bool found = false;
    float attachAngle = 0.0f;
    unsigned int attachIndex = static_cast<unsigned int>(-1);
    float bestScore = 100.0f;

    if (soldier->getPose() == SoldierPose_Prone ||
        !m_networkableId ||
        _float1C0 > 0.0f ||
        soldier->getGrapplingHookId() ||
        soldier->m_readyToClimbTime > 0.0f ||
        !_byte1C8)
    {
        return false;
    }

    int currentIndex = _dword16C;
    std::vector<RopeLink*>::iterator link = m_ropeLinks.end() - 1;
    while (link != m_ropeLinks.begin())
    {
        int direction = (*link)->m_climbDirection;
        if (direction)
        {
            const Vec3 midpoint = (*link)->getMidpoint();
            Vec3 delta = {
                midpoint.x - soldierPosition.x,
                midpoint.y - soldierPosition.y,
                midpoint.z - soldierPosition.z
            };
            const float distanceSquared =
                delta.x * delta.x + delta.y * delta.y + delta.z * delta.z;
            const float distance = std::sqrt(distanceSquared);

            static const float maximumDistance = m_ropeTemplate->m_attachClimberRadius + 1.0f;
            if (distanceSquared < maximumDistance * maximumDistance)
            {
                int movementMultiplier = 1;
                ObjectFlagPredicator predicator(0x100u);
                IObject* hitObject = 0;
                Vec3 hitPoint = { 0.0f, 0.0f, 0.0f };
                Vec3 hitNormal = { 0.0f, 0.0f, 0.0f };
                int hitPart = 0;

                int rangeIndex = findLinkIndexWithinHeightRange(
                    link, direction, soldierPosition.y, _float1E4, _float1E4 + 1.5f);
                int runLength = findUnclimbableIndexFromLink(link, direction);
                bool usable = rangeIndex >= 0 && runLength - rangeIndex > 0 &&
                    !objectManager->intersectLine(
                        hitObject, hitPoint, hitNormal, hitPart,
                        soldierPosition, delta, &predicator,
                        true, false, false, false, CID_SoldierTemplate);

                if (!usable)
                {
                    direction = -direction;
                    movementMultiplier = -1;
                    rangeIndex = findLinkIndexWithinHeightRange(
                        link, direction, soldierPosition.y,
                        -0.5f - _float1E8 - _float174, -_float1E8);
                    runLength = findUnclimbableIndexFromLink(link, direction);
                    usable = rangeIndex >= 0 && runLength - rangeIndex > 2;
                }

                if (usable)
                {
                    // Retail asks for the current vehicle twice and the default vehicle once.
                    if (player->getVehicle() != player->getDefaultVehicle())
                    {
                        IObject* currentVehicle = player->getVehicle();
                        if (currentVehicle->getTemplate()->getClassID() ==
                                CID_LadderContainerTemplate)
                        {
                            const unsigned int ladderId = soldier->getLadderId();
                            if (ladderId)
                            {
                                Ladder* ladder = ladderMap[static_cast<int>(ladderId)];
                                if (ladder)
                                    ladder->detachSoldier(static_cast<ISoldier*>(soldier));
                            }
                        }
                    }

                    Vec3 horizontal = { delta.x, 0.0f, delta.z };
                    normalizeRopeVector(horizontal);
                    float horizontalX = horizontal.x;
                    if (horizontalX < -1.0f)
                        horizontalX = -1.0f;
                    else if (horizontalX > 1.0f)
                        horizontalX = 1.0f;

                    float candidateFacing = std::asin(horizontalX) * 57.295776f;
                    if (horizontal.z < 0.0f)
                        candidateFacing = 180.0f - candidateFacing;
                    if (candidateFacing < 0.0f)
                        candidateFacing += 360.0f;

                    const float score =
                        std::fabs(candidateFacing - soldierFacing) / 100.0f + distance;
                    if (bestScore > score)
                    {
                        bestScore = score;
                        runLength = findUnclimbableIndexFromLink(
                            link, movementMultiplier * direction);
                        found = true;
                        attachAngle = findAttachAngle(
                            currentIndex + movementMultiplier * direction * (runLength - 1));
                        attachIndex = static_cast<unsigned int>(
                            currentIndex + direction * rangeIndex);
                    }
                }
            }
        }

        --currentIndex;
        --link;
    }

    if (!found)
        return false;
    attachSoldier(soldier, static_cast<int>(attachIndex), attachAngle);
    return true;
}

// . The list search is deliberately unchecked: retail erases end() if
// its id/list invariants disagree. The two occupying-player calls are distinct virtual calls.
void GrapplingHookRope::detachSoldier(ISoldier* soldier)
{
    if (!soldier || soldier->getGrapplingHookId() != static_cast<unsigned int>(m_networkableId))
        return;

    std::list<ISoldier*>::iterator found = m_soldiers.begin();
    while (found != m_soldiers.end() && *found != soldier)
        ++found;
    m_soldierIterator = found;
    m_soldiers.erase(found);                         // no end() guard in either shipped port

    IObject* soldierObject = static_cast<IObject*>(soldier->queryInterface(IID_IObject));
    IObject* parent = soldierObject->getParent();
    soldier->setGrapplingHookId(0);
    soldier->setReadyToClimbTime(0.4f);

    IPlayer* occupyingPlayer = soldier->getOccupingPlayer();
    if (occupyingPlayer)
    {
        static_cast<::dice::hfe::Player*>(occupyingPlayer)->m_worldTime =
            static_cast<float>(WorldPref::getWorldTime()) - 0.3f;
    }

    m_riderCount = m_riderCount < 1 ? 0 : m_riderCount - 1;
    if (parent)
    {
        static_cast<GrapplingHookRopeContainer*>(parent)->_exit();
        parent->destroy(true);
        if (!game->queryInterface(IID_IGameClient) ||
            !soldier->getOccupingPlayer()->getIsRemote())
        {
            disablePhysics(soldierObject, false);
        }
    }
}

// . Links zero and size-1 are fixed endpoints; only the interior links rebuild their
// transformations. Retail assumes the initialize-created size >= 2 and performs no guard.
void GrapplingHookRope::setRopeLinksTransformations()
{
    for (std::vector<RopeLink*>::iterator it = m_ropeLinks.begin() + 1;
         it != m_ropeLinks.end() - 1; ++it)
        (*it)->setTransformation();
}

// . The rope uses a fixed per-step acceleration rather than consulting the world's
// configurable physics gravity. Pinned/event particles are skipped.
void GrapplingHookRope::applyForces()
{
    static const Vec3 ropeGravity = { 0.0f, -0.03f, 0.0f }; // ds:0x8AAD030..38
    for (std::vector<RopeParticle>::iterator it = m_particles.begin();
         it != m_particles.end(); ++it)
    {
        if (!it->m_eventFlag)
        {
            it->m_force.x += ropeGravity.x;
            it->m_force.y += ropeGravity.y;
            it->m_force.z += ropeGravity.z;
        }
    }
}

// . Fixed-step Verlet integration: hooked ropes use template damping +0x1C0;
// unhooked ropes use 1.0. Every particle's collision byte and accumulated force are then cleared.
void GrapplingHookRope::doPhysics()
{
    const float damping = _byte1C8 ? m_ropeTemplate->m_airFriction : 1.0f;
    for (std::vector<RopeParticle>::iterator it = m_particles.begin();
         it != m_particles.end(); ++it)
    {
        it->_byte28 = false;
        if (!it->m_eventFlag)
        {
            const Vec3 oldPosition = it->m_position;
            it->m_position.x += (it->m_position.x - it->m_previousPosition.x) * damping +
                              it->m_force.x + m_globalForce.x;
            it->m_position.y += (it->m_position.y - it->m_previousPosition.y) * damping +
                              it->m_force.y + m_globalForce.y;
            it->m_position.z += (it->m_position.z - it->m_previousPosition.z) * damping +
                              it->m_force.z + m_globalForce.z;
            it->m_previousPosition = oldPosition;
        }
        else
        {
            it->m_position = it->m_previousPosition;
        }
        it->m_force.x = it->m_force.y = it->m_force.z = 0.0f;
    }
    m_globalForce.x = m_globalForce.y = m_globalForce.z = 0.0f;
}

// . Link index one is the hook segment and uses the shipped asymmetric 0.7/1.3
// correction weights. Later interior links divide correction evenly. No size guards exist.
void GrapplingHookRope::constrainParticles()
{
    static const float hookWeight = 0.7f; // ds:0x8987A18

    RopeLink* hookLink = m_ropeLinks[1];
    RopeParticle* start = hookLink->m_startParticle;
    RopeParticle* end = hookLink->m_endParticle;
    Vec3 delta = {
        end->m_position.x - start->m_position.x,
        end->m_position.y - start->m_position.y,
        end->m_position.z - start->m_position.z
    };
    const float factor = 0.5f - _float178 /
        (delta.x * delta.x + delta.y * delta.y + delta.z * delta.z + _float178);
    Vec3 correction = { delta.x * factor, delta.y * factor, delta.z * factor };
    if (!start->m_eventFlag)
    {
        start->m_position.x += correction.x * hookWeight;
        start->m_position.y += correction.y * hookWeight;
        start->m_position.z += correction.z * hookWeight;
    }
    if (!end->m_eventFlag)
    {
        const float endWeight = 2.0f - hookWeight;
        end->m_position.x -= correction.x * endWeight;
        end->m_position.y -= correction.y * endWeight;
        end->m_position.z -= correction.z * endWeight;
    }

    for (std::vector<RopeLink*>::iterator it = m_ropeLinks.begin() + 2;
         it != m_ropeLinks.end() - 1; ++it)
    {
        start = (*it)->m_startParticle;
        end = (*it)->m_endParticle;
        delta.x = end->m_position.x - start->m_position.x;
        delta.y = end->m_position.y - start->m_position.y;
        delta.z = end->m_position.z - start->m_position.z;
        const float linkFactor = 0.5f - _float178 /
            (delta.x * delta.x + delta.y * delta.y + delta.z * delta.z + _float178);
        correction.x = delta.x * linkFactor;
        correction.y = delta.y * linkFactor;
        correction.z = delta.z * linkFactor;
        if (!start->m_eventFlag)
        {
            start->m_position.x += correction.x;
            start->m_position.y += correction.y;
            start->m_position.z += correction.z;
        }
        if (!end->m_eventFlag)
        {
            end->m_position.x -= correction.x;
            end->m_position.y -= correction.y;
            end->m_position.z -= correction.z;
        }
    }
}
// . The hook probes nine 5 cm offsets in shared-table order, accepts
// only a sufficiently upward-facing non-PCO/non-trigger root, and otherwise applies the same
// projection/damped reflection used for ordinary rope particles. Retail mutates `depth` only when
// a root exists (`handleHookCollision(++depth)`), so the depth-one friction gate keeps that quirk.
void GrapplingHookRope::handleHookCollision(int depth)
{
    if (depth > 3)
        return;

    RopeParticle& particle = m_particles.front();
    if (particle.m_eventFlag)
        return;

    Vec3 velocity = {
        particle.m_position.x - particle.m_previousPosition.x,
        particle.m_position.y - particle.m_previousPosition.y,
        particle.m_position.z - particle.m_previousPosition.z
    };
    IObject* hitObject = 0;
    Vec3 hitPoint = { 0.0f, 0.0f, 0.0f };
    Vec3 hitNormal = { 0.0f, 0.0f, 0.0f };
    int hitPart; // intersectLine output; neither shipped port initializes it before the call.
    ObjectFlagPredicator predicator(0x100u);

    bool hit = false;
    for (int direction = 0; direction <= 8; ++direction)
    {
        const Vec3& basis = kGrapplingHookDirections[direction];
        Vec3 from = {
            particle.m_previousPosition.x + basis.x * 0.05f,
            particle.m_previousPosition.y + basis.y * 0.05f,
            particle.m_previousPosition.z + basis.z * 0.05f
        };
        if (objectManager->intersectLine(
                hitObject, hitPoint, hitNormal, hitPart,
                from, velocity, &predicator,
                true, false, true, false, CID_SoldierTemplate))
        {
            hit = true;
            break;
        }
    }
    if (!hit)
        return;

    IObject* root = getRootParent(hitObject);
    if (root &&
        root->getTemplate()->getClassID() != CID_PlayerControlObjectTemplate &&
        root->getTemplate()->getClassID() != CID_TriggerableTargetTemplate &&
        hitNormal.y > _float1CC)
    {
        particle.m_eventFlag = true;
        particle.m_previousPosition = hitPoint;
        particle.m_position = particle.m_previousPosition;
        hookRopeTo(&particle);
        pullBackRope();
        return;
    }

    const float surfaceOffset = hitPart == 1 ? 0.01f : 0.00001f;
    hitPoint.x += hitNormal.x * surfaceOffset;
    hitPoint.y += hitNormal.y * surfaceOffset;
    hitPoint.z += hitNormal.z * surfaceOffset;

    const Vec3 savedPrevious = particle.m_previousPosition;
    particle.m_previousPosition.x = hitPoint.x - velocity.x;
    particle.m_previousPosition.y = hitPoint.y - velocity.y;
    particle.m_previousPosition.z = hitPoint.z - velocity.z;

    const float normalMotion =
        velocity.x * hitNormal.x +
        velocity.y * hitNormal.y +
        velocity.z * hitNormal.z;
    const Vec3 projectedPoint = {
        hitPoint.x - hitNormal.x * normalMotion,
        hitPoint.y - hitNormal.y * normalMotion,
        hitPoint.z - hitNormal.z * normalMotion
    };
    particle.m_position.x =
        projectedPoint.x + (projectedPoint.x - particle.m_previousPosition.x);
    particle.m_position.y =
        projectedPoint.y + (projectedPoint.y - particle.m_previousPosition.y);
    particle.m_position.z =
        projectedPoint.z + (projectedPoint.z - particle.m_previousPosition.z);
    particle.m_previousPosition = savedPrevious;

    velocity.x = particle.m_position.x - particle.m_previousPosition.x;
    velocity.y = particle.m_position.y - particle.m_previousPosition.y;
    velocity.z = particle.m_position.z - particle.m_previousPosition.z;
    particle.m_position.x = particle.m_previousPosition.x + velocity.x * _float19C;
    particle.m_position.y = particle.m_previousPosition.y + velocity.y * _float19C;
    particle.m_position.z = particle.m_previousPosition.z + velocity.z * _float19C;

    if (root)
        handleHookCollision(++depth);
    if (depth == 1)
    {
        velocity.x = particle.m_position.x - particle.m_previousPosition.x;
        velocity.y = particle.m_position.y - particle.m_previousPosition.y;
        velocity.z = particle.m_position.z - particle.m_previousPosition.z;
        const Vec3 friction = {
            velocity.x * m_kineticFriction,
            velocity.y * m_kineticFriction,
            velocity.z * m_kineticFriction
        };
        particle.m_position.x -= friction.x * hitNormal.y;
        particle.m_position.y -= friction.y * hitNormal.y;
        particle.m_position.z -= friction.z * hitNormal.y;

        velocity.x = particle.m_position.x - particle.m_previousPosition.x;
        velocity.y = particle.m_position.y - particle.m_previousPosition.y;
        velocity.z = particle.m_position.z - particle.m_previousPosition.z;
        if (velocity.x * velocity.x + velocity.y * velocity.y + velocity.z * velocity.z <
            m_staticFriction)
        {
            particle.m_position = particle.m_previousPosition;
        }
    }
}

// . Zero-motion particles do not raycast. A hit is projected onto the
// collision plane, reflected through that projection, damped, then recursively retested through
// depth four. Kinetic/static friction is applied only on return from the outermost call.
void GrapplingHookRope::handleParticleCollision(
    std::vector<RopeParticle>::iterator particle,
    int depth)
{
    if (particle->m_eventFlag || depth > 3)
        return;

    Vec3 velocity = {
        particle->m_position.x - particle->m_previousPosition.x,
        particle->m_position.y - particle->m_previousPosition.y,
        particle->m_position.z - particle->m_previousPosition.z
    };
    IObject* hitObject = 0;
    Vec3 hitPoint = { 0.0f, 0.0f, 0.0f };
    Vec3 hitNormal = { 0.0f, 0.0f, 0.0f };
    int hitPart; // intersectLine output; neither shipped port initializes it before the call.
    ObjectFlagPredicator predicator(0x100u);

    if (velocity.x * velocity.x + velocity.y * velocity.y + velocity.z * velocity.z == 0.0f ||
        !objectManager->intersectLine(
            hitObject, hitPoint, hitNormal, hitPart,
            particle->m_previousPosition, velocity, &predicator,
            true, false, true, false, CID_SoldierTemplate))
    {
        return;
    }

    const float surfaceOffset = hitPart == 1 ? 0.01f : 0.00001f;
    hitPoint.x += hitNormal.x * surfaceOffset;
    hitPoint.y += hitNormal.y * surfaceOffset;
    hitPoint.z += hitNormal.z * surfaceOffset;

    const Vec3 savedPrevious = particle->m_previousPosition;
    particle->m_previousPosition.x = hitPoint.x - velocity.x;
    particle->m_previousPosition.y = hitPoint.y - velocity.y;
    particle->m_previousPosition.z = hitPoint.z - velocity.z;

    const float normalMotion =
        velocity.x * hitNormal.x +
        velocity.y * hitNormal.y +
        velocity.z * hitNormal.z;
    const Vec3 projectedPoint = {
        hitPoint.x - hitNormal.x * normalMotion,
        hitPoint.y - hitNormal.y * normalMotion,
        hitPoint.z - hitNormal.z * normalMotion
    };
    particle->m_position.x =
        projectedPoint.x + (projectedPoint.x - particle->m_previousPosition.x);
    particle->m_position.y =
        projectedPoint.y + (projectedPoint.y - particle->m_previousPosition.y);
    particle->m_position.z =
        projectedPoint.z + (projectedPoint.z - particle->m_previousPosition.z);
    particle->m_previousPosition = savedPrevious;

    velocity.x = particle->m_position.x - particle->m_previousPosition.x;
    velocity.y = particle->m_position.y - particle->m_previousPosition.y;
    velocity.z = particle->m_position.z - particle->m_previousPosition.z;
    particle->m_position.x = particle->m_previousPosition.x + velocity.x * _float19C;
    particle->m_position.y = particle->m_previousPosition.y + velocity.y * _float19C;
    particle->m_position.z = particle->m_previousPosition.z + velocity.z * _float19C;

    const int nextDepth = depth + 1;
    handleParticleCollision(particle, nextDepth);
    if (nextDepth == 1)
    {
        velocity.x = particle->m_position.x - particle->m_previousPosition.x;
        velocity.y = particle->m_position.y - particle->m_previousPosition.y;
        velocity.z = particle->m_position.z - particle->m_previousPosition.z;
        const Vec3 friction = {
            velocity.x * m_kineticFriction,
            velocity.y * m_kineticFriction,
            velocity.z * m_kineticFriction
        };
        particle->m_position.x -= friction.x * hitNormal.y;
        particle->m_position.y -= friction.y * hitNormal.y;
        particle->m_position.z -= friction.z * hitNormal.y;

        velocity.x = particle->m_position.x - particle->m_previousPosition.x;
        velocity.y = particle->m_position.y - particle->m_previousPosition.y;
        velocity.z = particle->m_position.z - particle->m_previousPosition.z;
        if (velocity.x * velocity.x + velocity.y * velocity.y + velocity.z * velocity.z <
            m_staticFriction)
        {
            particle->m_position = particle->m_previousPosition;
        }
    }
}

// . Interior links sweep both directions at t=1.0..0.0 in fifths,
// rewind free endpoints to the first safe sample, and accumulate low-motion contact time. The
// first template to enter this routine supplies both cached thresholds, as in retail.
void GrapplingHookRope::handleLinkCollision()
{
    static const float lowMotionThreshold = m_ropeTemplate->m_linkAttachMovement;
    static const float hookDelay = m_ropeTemplate->m_linkAttachTime;

    for (std::vector<RopeLink*>::iterator linkIt = m_ropeLinks.begin() + 1;
         linkIt != m_ropeLinks.end() - 1; ++linkIt)
    {
        RopeLink* link = *linkIt;
        RopeParticle* start = link->m_startParticle;
        RopeParticle* end = link->m_endParticle;
        bool startSlow = false;
        bool endSlow = false;

        if (!start->m_eventFlag || !end->m_eventFlag)
        {
            IObject* forwardHit = 0;
            IObject* backwardHit = 0;
            IObject* selectedHit = 0;
            int sampleIndex = 5;
            ObjectFlagPredicator predicator(0x100u);
            Vec3 hitPoint = { 0.0f, 0.0f, 0.0f };
            Vec3 forwardNormal = { 0.0f, 0.0f, 0.0f };
            Vec3 backwardNormal = { 0.0f, 0.0f, 0.0f };
            Vec3 combinedNormal = { 0.0f, 0.0f, 0.0f };
            int hitPart; // shared output in both directional probes; uninitialized in retail.

            const Vec3 startVelocity = {
                start->m_position.x - start->m_previousPosition.x,
                start->m_position.y - start->m_previousPosition.y,
                start->m_position.z - start->m_previousPosition.z
            };
            const Vec3 endVelocity = {
                end->m_position.x - end->m_previousPosition.x,
                end->m_position.y - end->m_previousPosition.y,
                end->m_position.z - end->m_previousPosition.z
            };
            Vec3 startSample = { 0.0f, 0.0f, 0.0f };
            Vec3 endSample = { 0.0f, 0.0f, 0.0f };

            for (;;)
            {
                const float time = static_cast<float>(sampleIndex) / 5.0f;
                startSample.x = start->m_previousPosition.x + startVelocity.x * time;
                startSample.y = start->m_previousPosition.y + startVelocity.y * time;
                startSample.z = start->m_previousPosition.z + startVelocity.z * time;
                endSample.x = end->m_previousPosition.x + endVelocity.x * time;
                endSample.y = end->m_previousPosition.y + endVelocity.y * time;
                endSample.z = end->m_previousPosition.z + endVelocity.z * time;

                const Vec3 forwardDirection = {
                    endSample.x - startSample.x,
                    endSample.y - startSample.y,
                    endSample.z - startSample.z
                };
                const Vec3 backwardDirection = {
                    startSample.x - endSample.x,
                    startSample.y - endSample.y,
                    startSample.z - endSample.z
                };
                const bool forwardCollision = objectManager->intersectLine(
                    forwardHit, hitPoint, forwardNormal, hitPart,
                    startSample, forwardDirection, &predicator,
                    true, false, false, false, CID_SoldierTemplate);
                const bool backwardCollision = objectManager->intersectLine(
                    backwardHit, hitPoint, backwardNormal, hitPart,
                    endSample, backwardDirection, &predicator,
                    true, false, false, false, CID_SoldierTemplate);
                if (!forwardCollision && !backwardCollision)
                    break;
                if (sampleIndex == 0)
                    break;

                combinedNormal.x = forwardNormal.x + backwardNormal.x;
                combinedNormal.y = forwardNormal.y + backwardNormal.y;
                combinedNormal.z = forwardNormal.z + backwardNormal.z;
                normalizeRopeVector(combinedNormal);
                selectedHit = forwardCollision ? forwardHit : backwardHit;
                --sampleIndex;
            }

            if (sampleIndex != 5)
            {
                if (!start->m_eventFlag)
                {
                    start->m_position.x = startSample.x + combinedNormal.x * 0.0001f;
                    start->m_position.y = startSample.y + combinedNormal.y * 0.0001f;
                    start->m_position.z = startSample.z + combinedNormal.z * 0.0001f;
                }
                if (!end->m_eventFlag)
                {
                    end->m_position.x = endSample.x + combinedNormal.x * 0.0001f;
                    end->m_position.y = endSample.y + combinedNormal.y * 0.0001f;
                    end->m_position.z = endSample.z + combinedNormal.z * 0.0001f;
                }

                if (!_byte1C8)
                {
                    IObject* root = getRootParent(selectedHit);
                    if (root->getTemplate()->getClassID() != CID_PlayerControlObjectTemplate &&
                        root->getTemplate()->getClassID() != CID_TriggerableTargetTemplate)
                    {
                        const Vec3 currentStartVelocity = {
                            start->m_position.x - start->m_previousPosition.x,
                            start->m_position.y - start->m_previousPosition.y,
                            start->m_position.z - start->m_previousPosition.z
                        };
                        const Vec3 currentEndVelocity = {
                            end->m_position.x - end->m_previousPosition.x,
                            end->m_position.y - end->m_previousPosition.y,
                            end->m_position.z - end->m_previousPosition.z
                        };
                        if (currentStartVelocity.x * currentStartVelocity.x +
                            currentStartVelocity.y * currentStartVelocity.y +
                            currentStartVelocity.z * currentStartVelocity.z < lowMotionThreshold)
                        {
                            start->_state24 += m_deltaTime;
                            startSlow = true;
                        }
                        if (currentEndVelocity.x * currentEndVelocity.x +
                            currentEndVelocity.y * currentEndVelocity.y +
                            currentEndVelocity.z * currentEndVelocity.z < lowMotionThreshold)
                        {
                            end->_state24 += m_deltaTime;
                            endSlow = true;
                        }
                        if (start->_state24 > hookDelay || end->_state24 > hookDelay)
                        {
                            start->m_eventFlag = true;
                            end->m_eventFlag = true;
                            hookRopeTo(start);
                            m_hookPosition = link->getMidpoint();
                            pullBackRope();
                        }
                    }
                }
            }

            if (!startSlow)
            {
                if (start->_byte28)
                    start->_state24 = 0.0f;
                else
                    start->_byte28 = true;
            }
            if (!endSlow)
            {
                if (end->_byte28)
                    end->_state24 = 0.0f;
                else
                    end->_byte28 = true;
            }
        }
    }
}
// . This one routine owns rope expiry, sleep detection, the fixed-step accumulator,
// local-owner update broadcasts, and the short post-create cooldown. All first-template statics,
// strict comparisons, and the one-frame-at-a-time event cadence are retained.
void GrapplingHookRope::handleTimers(float deltaTime)
{
    m_deltaTime = deltaTime;
    m_timeAwake += deltaTime;
    m_frameAccumulator += deltaTime;

    if (gameLogic->isServer())
    {
        static const float maximumUnusedLifetime = m_ropeTemplate->m_ropeUnusedLifetime;
        if (m_unusedLifetime < maximumUnusedLifetime)
        {
            if (m_riderCount)
                m_unusedLifetime = 0.0f;
            else
                m_unusedLifetime += m_deltaTime;
        }
        else if (m_riderCount <= 0)
        {
            objectManager->destroyObject(this);
            m_unusedLifetime = 0.0f;
        }
    }

    if (m_awake && m_timeAwake > 1.5f)
    {
        bool particlesAtRest = true;
        for (std::vector<RopeParticle>::iterator it = m_particles.begin() + 1;
             it != m_particles.end(); ++it)
        {
            const float dx = it->m_position.x - it->m_previousPosition.x;
            const float dy = it->m_position.y - it->m_previousPosition.y;
            const float dz = it->m_position.z - it->m_previousPosition.z;
            if (dx * dx + dy * dy + dz * dz > 0.000002f)
            {
                particlesAtRest = false;
                break;
            }
        }
        if (particlesAtRest)
        {
            m_awake = false;
            _byte204 = true;
            m_hookedAwakeTime = _float1F8;
        }
    }

    if (_byte1C8 && m_awake)
    {
        m_hookedAwakeTime += m_deltaTime;
        if (m_hookedAwakeTime > _float1F8)
        {
            m_awake = false;
            _byte204 = true;
        }
    }

    IPlayer* owner = playerManager->getPlayerFromId(m_ownerPlayerId);
    if (owner && owner == playerManager->getLocalHumanPlayer())
    {
        if (m_awake)
            m_updateEventAccumulator += m_deltaTime;

        if (m_updateEventAccumulator >= m_updateEventInterval || _byte204)
        {
            _byte204 = false;
            m_updateEventAccumulator -= m_updateEventInterval;
            const unsigned short ropeId = static_cast<unsigned short>(m_networkableId);
            unsigned int index = 0;
            for (std::vector<RopeParticle>::iterator it = m_particles.begin();
                 it != m_particles.end(); ++it, ++index)
            {
                GrapplingHookUpdateEvent* event = new GrapplingHookUpdateEvent;
                event->m_ropeNetId = ropeId;
                event->m_segmentIndex = static_cast<unsigned char>(index);
                event->m_start = it->m_position;
                event->m_end = it->m_previousPosition;
                event->m_flag28 = it->m_eventFlag;
                game->sendGameEventToAllPlaying(event);
            }
        }
    }

    if (_float1C0 > 0.0f)
        _float1C0 -= m_deltaTime;
}

// . The first collision pins the hook, advances the next update event
// by 0.15 seconds, and performs the one-shot sound/server-script notification. The shipped ports
// differ in the raw ISound playAt slot (ELF +0x18, PE +0x1C); the typed interface preserves that
// source call without baking either ABI into this object.
void GrapplingHookRope::hookRopeTo(RopeParticle* particle)
{
    if (_byte1C8)
        return;

    _byte1C8 = true;
    m_hookParticle = particle;
    m_hookPosition = particle->m_position;
    m_updateEventAccumulator = m_updateEventInterval - 0.15f;
    m_timeAwake = 0.0f;

    if (_byte20D)
    {
        _byte20D = false;
        _float1C0 = 0.8f;
        if (_byte20C)
            _float1C0 = 0.25f;

        ISound* sound = static_cast<ISound*>(
            g_bfSound->getSound(std::string("S_GrapplingHookAttached")));
        if (sound)
            sound->playAt(m_hookPosition);

        if (gameLogic->isServer())
        {
            IPlayer* player = playerManager->getPlayerFromId(m_ownerPlayerId);
            python::PyRef args = python::buildArgs(player);
            python::pythonHost()->handleEvent(static_cast<ScriptEvent>(8), args.get());
        }
    }
}

// . Hex-Rays invents a float* return from a scratch register, while the callers use
// no result, so the source return remains [INFERRED] void. Retail walks from the hooked particle
// toward the final particle, stopping at the private marker, and applies a 0.08 pull impulse.
void GrapplingHookRope::pullBackRope()
{
    if (!m_hookParticle || !_byte20C)
        return;

    Vec3 pull = {
        m_creationPosition.x - m_hookPosition.x,
        m_creationPosition.y - m_hookPosition.y,
        m_creationPosition.z - m_hookPosition.z
    };
    normalizeRopeVector(pull);
    pull.y += 1.0f;
    normalizeRopeVector(pull);

    RopeParticle* particle = m_hookParticle;
    while (particle != &m_particles.back() && particle != _particle1C4)
    {
        RopeParticle* next = particle + 1;
        next->m_position = next->m_previousPosition;

        Vec3 segment = {
            next->m_previousPosition.x - particle->m_position.x,
            next->m_previousPosition.y - particle->m_position.y,
            next->m_previousPosition.z - particle->m_position.z
        };
        normalizeRopeVector(segment);

        particle = next;
        particle->m_force.x += (segment.x + pull.x) * 0.08f;
        particle->m_force.y += (segment.y + pull.y) * 0.08f;
        particle->m_force.z += (segment.z + pull.z) * 0.08f;
    }

    _byte20C = false;
}

// . The first rope template to reach this method supplies the process-wide slope
// threshold, matching retail's function-static initialization quirk.
void GrapplingHookRope::setClimbablePartsOfRope()
{
    static const float maxVerticalComponent = m_ropeTemplate->m_climbableAngle;
    const float epsilon = 1.1920929e-7f;
    for (std::vector<RopeLink*>::iterator it = m_ropeLinks.begin();
         it != m_ropeLinks.end(); ++it)
    {
        RopeLink* link = *it;
        const Vec3& start = link->m_startParticle->m_position;
        const Vec3& end = link->m_endParticle->m_position;
        const float dx = end.x - start.x;
        float normalizedY = end.y - start.y;
        const float dz = end.z - start.z;
        const float lengthSquared = dx * dx + normalizedY * normalizedY + dz * dz;
        if (std::fabs(lengthSquared - 1.0f) >= epsilon)
        {
            if (std::fabs(lengthSquared) >= epsilon)
                normalizedY *= 1.0f / std::sqrt(lengthSquared);
            else
                normalizedY = 0.0f;
        }

        if (normalizedY > maxVerticalComponent)
            link->m_climbDirection = 1;
        else if (normalizedY < -maxVerticalComponent)
            link->m_climbDirection = -1;
        else
            link->m_climbDirection = 0;
    }
}

// . The endpoint links terminate every same-direction run, so retail performs no
// explicit bounds check while stepping by -1 or +1.
int GrapplingHookRope::findUnclimbableIndexFromLink(
    std::vector<RopeLink*>::iterator link,
    int direction)
{
    int index = 0;
    std::vector<RopeLink*>::iterator walk = link;
    if (direction)
    {
        do
        {
            ++index;
            walk += direction;
        }
        while ((*walk)->m_climbDirection == (*link)->m_climbDirection);
    }
    return index;
}

// . Search the current same-direction run for a midpoint whose relative height is
// inside the inclusive range. A zero direction or crossing the run boundary yields -1.
int GrapplingHookRope::findLinkIndexWithinHeightRange(
    std::vector<RopeLink*>::iterator link,
    int direction,
    float height,
    float minimum,
    float maximum)
{
    int index = 0;
    std::vector<RopeLink*>::iterator walk = link;
    const int climbDirection = (*link)->m_climbDirection;
    if (!direction)
        return -1;

    for (;;)
    {
        const Vec3 midpoint = (*walk)->getMidpoint();
        const float relativeHeight = midpoint.y - height;
        if (relativeHeight >= minimum && relativeHeight <= maximum)
            return index;

        ++index;
        walk += direction;
        if ((*walk)->m_climbDirection != climbDirection)
            return -1;
    }
}

// . Samples are taken from t=0 while t<=1 in 0.05 increments. At each,
// retail probes the zero vector followed by the eight horizontal compass directions from the
// shared wire direction table (PE 0x830240) and accumulates the negated first hit normal. If none
// hit, the launch direction supplies the fallback yaw.
float GrapplingHookRope::findAttachAngle(int segmentIndex)
{
    RopeLink* link = m_ropeLinks[segmentIndex];
    const Vec3& start = link->m_startParticle->m_position;
    const Vec3& end = link->m_endParticle->m_position;
    const Vec3 segment = {
        end.x - start.x,
        end.y - start.y,
        end.z - start.z
    };

    ObjectFlagPredicator predicator(0x100u);
    IObject* hitObject = 0;
    Vec3 hitPoint = { 0.0f, 0.0f, 0.0f };
    Vec3 hitNormal = { 0.0f, 0.0f, 0.0f };
    int hitPart = 0;
    Vec3 accumulatedNormal = { 0.0f, 0.0f, 0.0f };

    for (float t = 0.0f; t <= 1.0f; t += 0.05f)
    {
        const Vec3 sample = {
            start.x + segment.x * t,
            start.y + segment.y * t,
            start.z + segment.z * t
        };
        for (int direction = 0; direction < 9; ++direction)
        {
            if (objectManager->intersectLine(
                    hitObject, hitPoint, hitNormal, hitPart,
                    sample, kGrapplingHookDirections[direction], &predicator,
                    false, false, false, true, 0))
            {
                accumulatedNormal.x -= hitNormal.x;
                accumulatedNormal.y -= hitNormal.y;
                accumulatedNormal.z -= hitNormal.z;
                break;
            }
        }
    }

    if (accumulatedNormal.x * accumulatedNormal.x +
        accumulatedNormal.y * accumulatedNormal.y +
        accumulatedNormal.z * accumulatedNormal.z == 0.0f)
    {
        accumulatedNormal.x = m_creationDirection.x;
        accumulatedNormal.y = m_creationDirection.y;
        accumulatedNormal.z = m_creationDirection.z;
    }
    accumulatedNormal.y = 0.0f;
    normalizeRopeVector(accumulatedNormal);

    float horizontalX = accumulatedNormal.x;
    if (horizontalX < -1.0f)
        horizontalX = -1.0f;
    else if (horizontalX > 1.0f)
        horizontalX = 1.0f;

    const float angle = std::asin(horizontalX) * 57.295776f;
    if (accumulatedNormal.z < 0.0f)
        return 180.0f - angle;
    return angle;
}

// . Each tuning value is a function-local static initialized from the
// first rope template that reaches the function. That surprising cross-template cache is shipped
// behavior. Both impulse passes affect particles [0, m_dword170) and attenuate before applying.
void GrapplingHookRope::throwRope()
{
    static float firstForce = m_ropeTemplate->m_throwStrength;
    static float firstVerticalBias = m_ropeTemplate->m_upThrowBoost;
    static float firstAttenuation = m_ropeTemplate->m_degradeThrowStrength;

    Vec3 direction = m_creationDirection;
    direction.y += std::fabs(direction.y) * firstVerticalBias;
    float impulse = firstForce * 550.0f * m_creationScalar;
    for (std::vector<RopeParticle>::iterator it = m_particles.begin();
         it != m_particles.begin() + _dword170; ++it)
    {
        impulse *= firstAttenuation;
        it->m_force.x += direction.x * impulse;
        it->m_force.y += direction.y * impulse;
        it->m_force.z += direction.z * impulse;
    }

    static float secondForce = m_ropeTemplate->m_throwStrength2;
    static float secondVerticalBias = m_ropeTemplate->m_upThrowBoost2;
    static float secondAttenuation = m_ropeTemplate->m_degradeThrowStrength2;

    direction = m_creationDirection;
    direction.y += std::fabs(direction.y) * secondVerticalBias;
    impulse = 550.0f * secondForce * m_creationScalar;
    for (std::vector<RopeParticle>::iterator it = m_particles.begin();
         it != m_particles.begin() + _dword170; ++it)
    {
        impulse *= secondAttenuation;
        it->m_force.x += direction.x * impulse;
        it->m_force.y += direction.y * impulse;
        it->m_force.z += direction.z * impulse;
    }
}

// . The last particle down through index four receives an attenuated
// impulse; index three is the exclusive stop. The first particle is then pinned with -0.4 on Y.
void GrapplingHookRope::dropRope()
{
    float impulse = m_ropeTemplate->m_dropStrength;
    static float verticalBias = m_ropeTemplate->m_upDropBoost;
    static float attenuation = m_ropeTemplate->m_degradeDropStrength;

    m_creationDirection.y = m_creationDirection.y * 0.1f + verticalBias;
    for (std::vector<RopeParticle>::iterator it = m_particles.end() - 1;
         it != m_particles.begin() + 3; --it)
    {
        impulse *= attenuation;
        it->m_force.x += m_creationDirection.x * impulse;
        it->m_force.y += m_creationDirection.y * impulse;
        it->m_force.z += m_creationDirection.z * impulse;
    }

    m_particles.front().m_force.x = 0.0f;
    m_particles.front().m_force.y = -0.4f;
    m_particles.front().m_force.z = 0.0f;
    _byte20C = false;
}
// . This is a sparse wire-frame volume test: nine vertical rays from
// radius-offset origins, then nine radial rays at each of five height slices. The shared direction
// entries are used verbatim rather than normalized.
bool GrapplingHookRope::wireBoxCheckForCollision(
    const Vec3& position,
    const float& radius,
    const float& up)
{
    IObject* hitObject = 0;
    Vec3 hitPoint = { 0.0f, 0.0f, 0.0f };
    Vec3 hitNormal = { 0.0f, 0.0f, 0.0f };
    int hitPart; // intersectLine output; neither shipped port initializes it before the call.
    ObjectFlagPredicator predicator(0x100u);

    const Vec3 vertical = { 0.0f, up, 0.0f };
    for (int directionIndex = 0; directionIndex <= 8; ++directionIndex)
    {
        const Vec3& basis = kGrapplingHookDirections[directionIndex];
        const Vec3 from = {
            position.x + basis.x * radius,
            position.y + basis.y * radius,
            position.z + basis.z * radius
        };
        if (objectManager->intersectLine(
                hitObject, hitPoint, hitNormal, hitPart,
                from, vertical, &predicator,
                false, false, true, true, 0))
        {
            return true;
        }
    }

    for (int heightIndex = 0; heightIndex <= 4; ++heightIndex)
    {
        const Vec3 from = {
            position.x,
            position.y + static_cast<float>(heightIndex) * 0.25f * up,
            position.z
        };
        for (int directionIndex = 0; directionIndex <= 8; ++directionIndex)
        {
            const Vec3& basis = kGrapplingHookDirections[directionIndex];
            const Vec3 direction = {
                basis.x * radius,
                basis.y * radius,
                basis.z * radius
            };
            if (objectManager->intersectLine(
                    hitObject, hitPoint, hitNormal, hitPart,
                    from, direction, &predicator,
                    false, false, true, true, 0))
            {
                return true;
            }
        }
    }
    return false;
}

// . Retail accepts the first clear candidate. The first pass searches
// two upward shelves and requires an upward-facing floor; the fallback pass searches three shelves
// without that floor requirement. Both passes preserve the zero direction at table index zero,
// require unobstructed sight from both link endpoints, reject the sparse wire box, then exclude a
// Ladder within 0.6 metres in X/Z (the shipped Y subtraction is candidate.y-candidate.y).
Vec3 GrapplingHookRope::getDismountPosition(
    const Vec3& current,
    const Vec3& fallback,
    const RopeLink& link)
{
    static const float radius = m_ropeTemplate->m_dismountCheckOffset;
    static const float up = m_ropeTemplate->m_minYNormal;

    Vec3 result = fallback;
    IObject* hitObject = 0;
    Vec3 hitPoint = { 0.0f, 0.0f, 0.0f };
    Vec3 hitNormal = { 0.0f, 0.0f, 0.0f };
    int hitPart; // intersectLine output; neither shipped port initializes it before the call.
    ObjectFlagPredicator predicator(0x100u);
    Vec3 shelf = current;

    for (int heightIndex = 0; heightIndex <= 1; ++heightIndex)
    {
        const Vec3 shelfStep = {
            0.0f, static_cast<float>(heightIndex) * 0.5f, 0.0f
        };
        if (objectManager->intersectLine(
                hitObject, hitPoint, hitNormal, hitPart,
                shelf, shelfStep, &predicator,
                false, false, true, true, 0))
        {
            break;
        }
        shelf.x += shelfStep.x;
        shelf.y += shelfStep.y;
        shelf.z += shelfStep.z;

        const Vec3 floorProbe = {
            0.0f, -static_cast<float>(heightIndex) - 1.0f, 0.0f
        };
        for (int radiusIndex = 0; radiusIndex <= 5; ++radiusIndex)
        {
            const float scale = radius * static_cast<float>(radiusIndex + 1);
            for (int directionIndex = 0; directionIndex <= 16; ++directionIndex)
            {
                const Vec3& basis = kGrapplingHookDirections[directionIndex];
                const Vec3 direction = {
                    basis.x * scale,
                    basis.y * scale,
                    basis.z * scale
                };
                if (objectManager->intersectLine(
                        hitObject, hitPoint, hitNormal, hitPart,
                        shelf, direction, &predicator,
                        false, false, true, true, 0))
                {
                    continue;
                }

                Vec3 candidate = {
                    shelf.x + direction.x,
                    shelf.y + direction.y,
                    shelf.z + direction.z
                };
                if (!objectManager->intersectLine(
                        hitObject, hitPoint, hitNormal, hitPart,
                        candidate, floorProbe, &predicator,
                        false, false, true, true, 0) ||
                    hitNormal.y <= up)
                {
                    continue;
                }

                const Vec3 fromStart = {
                    candidate.x - link.m_startParticle->m_position.x,
                    candidate.y - link.m_startParticle->m_position.y,
                    candidate.z - link.m_startParticle->m_position.z
                };
                const Vec3 fromEnd = {
                    candidate.x - link.m_endParticle->m_position.x,
                    candidate.y - link.m_endParticle->m_position.y,
                    candidate.z - link.m_endParticle->m_position.z
                };
                if (objectManager->intersectLine(
                        hitObject, hitPoint, hitNormal, hitPart,
                        link.m_startParticle->m_position, fromStart, &predicator,
                        false, false, true, true, 0) ||
                    objectManager->intersectLine(
                        hitObject, hitPoint, hitNormal, hitPart,
                        link.m_endParticle->m_position, fromEnd, &predicator,
                        false, false, true, true, 0) ||
                    wireBoxCheckForCollision(candidate, radius, up))
                {
                    continue;
                }

                std::vector<IObject*> objects;
                objectManager->getNoCollisionObjectsFromGrid(
                    candidate, 0.6f, objects, &predicator);
                bool besideLadder = false;
                for (std::vector<IObject*>::iterator objectIt = objects.begin();
                     objectIt != objects.end(); ++objectIt)
                {
                    if ((*objectIt)->getTemplate()->getClassID() != CID_LadderTemplate)
                        continue;
                    const Vec3& ladderPosition = (*objectIt)->getAbsolutePosition();
                    const float dx = candidate.x - ladderPosition.x;
                    const float dy = candidate.y - candidate.y;
                    const float dz = candidate.z - ladderPosition.z;
                    if (std::sqrt(dx * dx + dy * dy + dz * dz) < 0.6f)
                    {
                        besideLadder = true;
                        break;
                    }
                }
                if (!besideLadder)
                {
                    result = candidate;
                    result.y += _float208;
                    return result;
                }
            }
        }
    }

    shelf = current;
    for (int heightIndex = 0; heightIndex <= 2; ++heightIndex)
    {
        const Vec3 shelfStep = {
            0.0f, static_cast<float>(heightIndex) * 0.5f, 0.0f
        };
        if (objectManager->intersectLine(
                hitObject, hitPoint, hitNormal, hitPart,
                shelf, shelfStep, &predicator,
                false, false, true, true, 0))
        {
            break;
        }
        shelf.x += shelfStep.x;
        shelf.y += shelfStep.y;
        shelf.z += shelfStep.z;

        for (int radiusIndex = 0; radiusIndex <= 5; ++radiusIndex)
        {
            const float scale = radius * static_cast<float>(radiusIndex + 1);
            for (int directionIndex = 0; directionIndex <= 16; ++directionIndex)
            {
                const Vec3& basis = kGrapplingHookDirections[directionIndex];
                const Vec3 direction = {
                    basis.x * scale,
                    basis.y * scale,
                    basis.z * scale
                };
                if (objectManager->intersectLine(
                        hitObject, hitPoint, hitNormal, hitPart,
                        shelf, direction, &predicator,
                        false, false, true, true, 0))
                {
                    continue;
                }

                Vec3 candidate = {
                    shelf.x + direction.x,
                    shelf.y + direction.y,
                    shelf.z + direction.z
                };
                const Vec3 fromStart = {
                    candidate.x - link.m_startParticle->m_position.x,
                    candidate.y - link.m_startParticle->m_position.y,
                    candidate.z - link.m_startParticle->m_position.z
                };
                const Vec3 fromEnd = {
                    candidate.x - link.m_endParticle->m_position.x,
                    candidate.y - link.m_endParticle->m_position.y,
                    candidate.z - link.m_endParticle->m_position.z
                };
                if (objectManager->intersectLine(
                        hitObject, hitPoint, hitNormal, hitPart,
                        link.m_startParticle->m_position, fromStart, &predicator,
                        false, false, true, true, 0) ||
                    objectManager->intersectLine(
                        hitObject, hitPoint, hitNormal, hitPart,
                        link.m_endParticle->m_position, fromEnd, &predicator,
                        false, false, true, true, 0) ||
                    wireBoxCheckForCollision(candidate, radius, up))
                {
                    continue;
                }

                std::vector<IObject*> objects;
                objectManager->getNoCollisionObjectsFromGrid(
                    candidate, 0.6f, objects, &predicator);
                bool besideLadder = false;
                for (std::vector<IObject*>::iterator objectIt = objects.begin();
                     objectIt != objects.end(); ++objectIt)
                {
                    if ((*objectIt)->getTemplate()->getClassID() != CID_LadderTemplate)
                        continue;
                    const Vec3& ladderPosition = (*objectIt)->getAbsolutePosition();
                    const float dx = candidate.x - ladderPosition.x;
                    const float dy = candidate.y - candidate.y;
                    const float dz = candidate.z - ladderPosition.z;
                    if (std::sqrt(dx * dx + dy * dy + dz * dz) < 0.6f)
                    {
                        besideLadder = true;
                        break;
                    }
                }
                if (!besideLadder)
                {
                    result = candidate;
                    result.y += _float208;
                    return result;
                }
            }
        }
    }
    return result;
}

} // namespace world
} // namespace hfe
} // namespace dice
