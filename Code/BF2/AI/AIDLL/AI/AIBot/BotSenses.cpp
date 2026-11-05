// [FRONTIER] Remaining methods stop by name until implemented.
// [RETIRE-WHEN: aidll-02-skeleton] Complete and validate the remaining BotSenses operations.

#include "BotSenses.h"

#include "../AIUtils/checkPointer.h"
#include "../Main/AIDllFrontier.h"
#include "../Main/CommonSingletonServer.h"
#include "BotMain.h"
#include "../Main/InformationManager.h"
#include "../AIKnowledge/Information.h"
#include "../AIKnowledge/IPIPhysical.h"
#include "../../../AIInterface/BFKnowledge/BFTimer.h"
#include "../../../AIInterface/BFKnowledge/IAIObject.h"
#include "../../../AIInterface/BFKnowledge/IAIObjectPlugIn.h"
#include "../AIKnowledge/FireObject.h"
#include "../AIKnowledge/IPIArmament.h"
#include "../AIKnowledge/IPIMobile.h"
#include "../AIKnowledge/WeaponInfo.h"
#include "../Main/AIRandom.h"
#include "CommunicationAgent.h"
#include "SensedObject.h"
#include "SensingAgent.h"

#include <algorithm>
#include <climits>
#include <cmath>
#include <iostream>
// static init: ELF 0x19E110, 0x19E200, 0x19E230: iostream Init and six cosine thresholds.

#include "../../../AIInterface/BFKnowledge/AIAdapterTypes.h"
#include "../../../AIInterface/BFKnowledge/AIPlayer.h"
#include "../../../AIInterface/BFKnowledge/BFObjectEnvironment.h"
#include "../../../../Core/Debug.h"
#include "../../../../Math/Mat4.h"
#include "../AIKnowledge/IPIUnit.h"
#include "../AIUtils/DebugRendering/AIRenderUtil.h"
#include "../Main/IBotManager.h"

namespace dice {
namespace hfe {
namespace ai {

// Creates the sight frustum and initializes sensing collections for the supplied bot.
BotSenses::BotSenses(BotMain* owner)
    : m_bot(owner)
    // the camera. See the block above.
    , m_frustum(checkPointer(new Frustum()))
    , m_collectTime(-1000000.0)
    , m_frustumStep(0)
    , m_seeState(0)
    , m_seeIndex(0)
    , m_fireTotalStrength(0)
    , m_fireTotalStrengthAgainstSelf(0)
{
    // the constructor'S last statement - see the block above. The binary copies the list's head
    // pointer; in source that is the list's own end. The cursor at +80 is deliberately not here.
    m_cursor60 = m_list56.end();
}

// Clears sensed-object records and resets per-life sensing state.
// [RETIRE-WHEN: aidll-02-skeleton] Remove when the corresponding implementation is complete and validated.
void BotSenses::reset()
{
    // ELF 0x199D40: a record holding a contact (+0x64) releases it on its own information
    // (slot 7, removeContact), then the record is deleted and its node erased.
    for (SensedObjectMap::iterator it = m_sensedObjects.begin(); it != m_sensedObjects.end(); )
    {
        if (it->second->m_hasContact)
        {
            if (Information* const information = informationManager->getInformation(it->second->m_handle))
                information->removeContact();
        }
        if (it->second)
            delete it->second;
        m_sensedObjects.erase(it++);
    }
    m_sensedObjects.clear();

    m_map44.clear();
    m_list56.clear();
    m_cursor60 = m_list56.end();
    m_sortBuffer.clear();

    m_seeIndex = 0;
    m_seeState = 0;
    m_frustumStep = 0;
    m_collectTime = -1000000.0;

    m_hearingMemHelperSet.clear();
    m_hearingMemCheckQueue.clear();
    // The constructor never did this; the cursor at +80 becomes valid here for the first time.
    m_cursor80 = m_hearingMemCheckQueue.end();

    // The fire list owns its objects; the attacker map below only borrows them.
    for (std::list<FireEntry>::iterator it = m_list84.begin(); it != m_list84.end(); ++it)
    {
        if (it->first)
            delete it->first;
    }
    m_list84.clear();

    m_fireTotalStrength = 0;
    m_fireTotalStrengthAgainstSelf = 0;

    m_map100.clear();
}

// Resets senses and deletes the frustum; member containers release their own storage.
BotSenses::~BotSenses()
{
    reset();
    delete m_frustum;
}

// [FRONTIER] Remaining sight, hearing and communication operations require implementation.
// [RETIRE-WHEN: aidll-02-skeleton] Remove when the corresponding implementation is complete and validated.

// ELF 0x19A040 (172 bytes). Hearing gets a tenth and the fire objects three tenths of the slice;
// seeing gets what is left of the deadline (start + slice - now). The ELF reads the update time
// (IBFTimer +0x10) three times; the middle read's result is discarded.
void BotSenses::updateSensing(const double& slice)
{
    const IBFTimer* const timer = static_cast<const IBFTimer*>(CommonSingletonServer::m_iBFTimer);
    const double start = timer->getUpdateTime();
    const double total = slice;
    const double hearing = total * 0.1;
    updateHearing(hearing);
    timer->getUpdateTime();
    const double fire = slice * 0.3;
    updateFireObjects(fire);
    const double seeing = (start + total) - timer->getUpdateTime();
    updateSeeing(seeing);
}

// ELF 0x19D8A0: map find at +112, the mapped SensedObject* or null.
SensedObject* BotSenses::getSensedObjectFromHandle(unsigned int handle) const
{
    const SensedObjectMap::const_iterator it = m_sensedObjects.find(handle);
    return it == m_sensedObjects.end() ? 0 : it->second;
}

namespace {

// ELF data 0x4B6CA0 (BotSenses.cpp's anonymous namespace): two rows (m_viewMode) of three
// {angle, threshold} pairs. see() takes the frustum angle from a row's first pair only, and the
// frustum-step threshold from pair m_frustumStep. The thresholds are initialized with cosf.
struct FrustumUpdateAngle
{
    float angle;
    float threshold;
};
FrustumUpdateAngle tweak_frustumUpdateAngle[2][3] =
{
    { { 1.7453293f, std::cos(0.436332315f) }, { 1.0471976f, std::cos(0.261799395f) },
      { 0.5235988f, std::cos(0.130899698f) } },
    { { 1.3089969f, std::cos(0.327249229f) }, { 0.7853982f, std::cos(0.196349546f) },
      { 0.2617994f, std::cos(0.065449849f) } },
};

// The sort order of the candidate buffer (std::__unguarded_linear_insert ELF 0x19FED0):
// ascending squared distance.
struct InfoDistPred
{
    bool operator()(const std::pair<float, unsigned int>& a, const std::pair<float, unsigned int>& b) const
    {
        return a.first < b.first;
    }
};

// ELF vtable 0x4B6CD0, predicate 0x1A0E30: which player-occupied objects a bot considers.
// Neutral objects within the view radius; never its own side; a dead physical object of type 21
// is skipped; with `vehicleSight` the radius is also capped at 300 m (90000), otherwise a unit
// must also be within its own cull distance.
class SideFilter : public AIObjectPredicate
{
public:
    SideFilter(int side, const Vec3& position, float radiusSq, bool vehicleSight)
        : m_side(side), m_position(position), m_radiusSq(radiusSq), m_vehicleSight(vehicleSight)
    {
    }

    virtual bool predicate(IAIObject* object)
    {
        const int side = object->getSide();
        if (side == 0)
            return m_radiusSq > distanceSq(object);
        if (side == m_side)
            return false;
        if (object->isOfType(static_cast<InformationType>(0x15)))
        {
            if (!object->isPhysical())
                return false;
            if (0.0f >= object->getPhysical()->getHealth())
                return false;
        }
        const float distSq = distanceSq(object);
        if (object->isUnit() && !m_vehicleSight)
        {
            if (!(m_radiusSq > distSq))
                return false;
            return object->getUnit()->getCullDistanceSqr() > distSq;
        }
        if (!m_vehicleSight)
            return m_radiusSq > distSq;
        if (!(m_radiusSq > distSq))
            return false;
        return 90000.0f > distSq;
    }

private:
    float distanceSq(IAIObject* object) const
    {
        const Vec3& position = object->getPosition();
        const float dx = position.x - m_position.x;
        const float dy = position.y - m_position.y;
        const float dz = position.z - m_position.z;
        return dx * dx + dy * dy + dz * dz;
    }

    /* +0x04 */ int   m_side;
    /* +0x08 */ Vec3  m_position;
    /* +0x14 */ float m_radiusSq;
    /* +0x18 */ bool  m_vehicleSight;
};

float distanceSqTo(const Vec3& own, IAIObject* object)
{
    const Vec3& position = object->getPosition();
    const float dx = own.x - position.x;
    const float dy = own.y - position.y;
    const float dz = own.z - position.z;
    return dx * dx + dy * dy + dz * dz;
}

} // namespace

// ELF 0x198420 (6421 bytes). A time-sliced state machine over m_seeState:
//   (3 -> 0) mark the camera's quadrants seen and step the frustum (m_frustumStep cycles 0, 1, 2
//            and drops to 0 once the camera has turned away from the last frustum direction);
//   always   rebuild the frustum from the camera (tweak angle, aspect 1, near 0.01, the player's
//            view distance);
//   0        gather the player-occupied candidates (SideFilter), keep those in the frustum, close
//            and in front (vehicle sight), or within three own radii, with their exposed
//            occupiers, as (squared distance, handle);
//   1        sort them by distance;
//   2        for each not yet in the seen list, sample up to 1..10 sense points (bounding radius *
//            30 / distance) and cast a ray from the eye (camera position, y - 0.1); the first clear
//            ray reports the object as seen to the bot and to every bot riding with it.
// Between the states the slice is checked against the update time.
void BotSenses::see(const double& slice)
{
    const IBFTimer* const timer = static_cast<const IBFTimer*>(CommonSingletonServer::m_iBFTimer);
    const double start = timer->getUpdateTime();
    // As in the image, the equipment information is used without a null check.
    Information* const equipment = informationManager->getInformation(m_bot->getCurrentEquipment());
    m_bot->getName();
    const Mat4& camera = *static_cast<const Mat4*>(m_bot->getPlayer()->getCameraTransformation());
    m_viewMode = equipment->isOfType(ITObstructedView);
    if (m_seeState == 3)
    {
        m_bot->setQuadSeen();
        m_seeState = 0;
        const Mat4& previous = m_frustum->m_transformation;
        const float turn = camera.m[8] * previous.m[8] + camera.m[9] * previous.m[9]
                         + camera.m[10] * previous.m[10];
        if (m_frustumStep == 1)
            m_frustumStep = 2;
        else if (m_frustumStep == 0)
            m_frustumStep = 1;
        else
            m_frustumStep = 0;
        if (turn <= tweak_frustumUpdateAngle[m_viewMode][m_frustumStep].threshold)
            m_frustumStep = 0;
    }
    IAIPlayer* const player = m_bot->getPlayer();
    player->getFieldOfView();
    const float viewDistance = player->getViewDistance();
    m_frustum->setPerspective(tweak_frustumUpdateAngle[m_viewMode][0].angle, 1.0f, 0.01f, viewDistance, camera);

    IAIObjectEnvironment* const environment =
        static_cast<IAIObjectEnvironment*>(CommonSingletonServer::m_iaiObjectEnvironment);
    switch (m_seeState)
    {
    case 0:
    {
        m_candidates.resize(0);
        const int side = m_bot->getSide();
        bool vehicleSight = false;
        if (equipment->isOfType(ITAir) || equipment->isOfType(ITHelicopter))
            vehicleSight = true;
        SideFilter filter(side, equipment->getPosition(), viewDistance * viewDistance, vehicleSight);
        const float ownRadius = equipment->getPhysical()->getRadius();
        environment->getWorldPlayerOccupiedObjects(m_candidates, &filter);
        m_sortBuffer.resize(0);
        const Vec3& own = equipment->getPosition();
        const float closeSq = static_cast<float>(static_cast<double>(ownRadius) * 3.0 * ownRadius * 3.0);
        for (std::vector<IAIObject*>::iterator it = m_candidates.begin(); it != m_candidates.end(); ++it)
        {
            IAIObject* const object = *it;
            Information* const information = informationManager->getInformation(object->getInformation(m_bot->getSide()));
            const float radius = object->getBoundingRadius();
            bool take = m_frustum->inside(object->getPosition(), radius);
            if (!take)
            {
                if (!equipment->isOfType(ITSoldier)
                    && (!information || information->isOfType(ITSoldier)))
                {
                    take = vehicleSight && isCloseAndInFront(equipment, object);
                }
                else
                {
                    const Vec3& position = object->getPosition();
                    const float dx = position.x - own.x;
                    const float dy = position.y - own.y;
                    const float dz = position.z - own.z;
                    take = dx * dx + dy * dy + dz * dz <= closeSq;
                }
            }
            if (!take)
                continue;
            {
                const float distSq = distanceSqTo(own, object);
                m_sortBuffer.push_back(std::make_pair(distSq, object->forceGetInformation(side, false)));
            }
            if (!object->isUnit())
            {
                Debug(DebugTypeError, "AI/AIDLL/AI/AIBot/BotSenses.cpp", 413, "")
                    << object->getName() << " has no unit.";
            }
            else
            {
                IAIObjectUnit* const unit = object->getUnit();
                if (unit->hasExposedSoldier() && unit->isOccupied())
                {
                    IAIObject* const occupier = unit->getOccupier();
                    const float distSq = distanceSqTo(own, occupier);
                    m_sortBuffer.push_back(std::make_pair(distSq, occupier->forceGetInformation(side, false)));
                }
            }
            int iterator;
            for (IAIObject* secondary = object->getFirstSecondaryObject(iterator); secondary;
                 secondary = object->getSecondaryObject(iterator))
            {
                IAIObjectUnit* const unit = secondary->getUnit();
                if (!unit || !unit->hasExposedSoldier() || !unit->isOccupied())
                    continue;
                IAIObject* const occupier = unit->getOccupier();
                if (!occupier)
                    continue;
                const float distSq = distanceSqTo(own, occupier);
                m_sortBuffer.push_back(std::make_pair(distSq, occupier->forceGetInformation(side, false)));
            }
        }
        m_seeIndex = 0;
        m_collectTime = timer->getGameTime();
        m_seeState = 1;
        if (slice < timer->getUpdateTime() - start)
            return;
    }
    // fall through
    case 1:
        equipment->getPosition();
        if (!m_sortBuffer.empty())
            std::sort(m_sortBuffer.begin(), m_sortBuffer.end(), InfoDistPred());
        m_seeState = 2;
        if (slice < timer->getUpdateTime() - start)
            return;
        // fall through
    case 2:
        break;
    case 3:
        timer->getUpdateTime();
        return;
    default:
        return;
    }

    const Vec3& own = equipment->getPosition();
    std::vector<IAIObject*> ignore;
    ignore.push_back(equipment->getAIObject()->getPrimaryObject());
    ignore.push_back(0);
    ignore.push_back(0);
    const bool render = IBotManager::instance->getRenderBotSensing();
    AIRenderUtil* const renderUtil = render ? AIRenderUtil::m_instance : 0;
    const int side = m_bot->getSide();
    double elapsed = timer->getUpdateTime() - start;
    equipment->getPhysical()->getRadius();
    bool finished = true;
    while (m_seeIndex < static_cast<int>(m_sortBuffer.size()))
    {
        if (slice <= elapsed)
        {
            finished = false;
            break;
        }
        const unsigned int handle = m_sortBuffer[m_seeIndex].second;
        Information* const target = m_map44.find(handle) == m_map44.end()
                                  ? informationManager->getInformation(handle) : 0;
        if (target && target->isRootObject())
        {
            IAIObject* const targetObject = target->getAIObject();
            Bot* const targetBot = IBotManager::instance->getBot(targetObject);
            bool activeBot = false;
            if (targetBot)
                activeBot = 0 < static_cast<int>(targetBot->getLodLevel());
            targetObject->getBoundingRadius();
            ignore[1] = targetObject;
            // [CHOSEN, not recovered] The image stores the third ignored object even after an
            // earlier target shrank the list to two (inside the capacity, past the size, so the
            // ray test never sees it); the size test keeps that store out of a native vector.
            IAIObject* rider = 0;
            bool keepRider = false;
            if (IPIUnit* const unit = target->getUnit())
            {
                if (!target->isOfType(ITSoldier))
                {
                    if (unit->hasExposedSoldier())
                    {
                        if (Information* const occupier =
                                informationManager->getInformation(unit->getOccupier(m_bot->getSide())))
                        {
                            rider = occupier->getAIObject();
                            keepRider = true;
                        }
                    }
                }
                else if (Information* const occupied =
                             informationManager->getInformation(unit->getOccupiedObject(m_bot->getSide())))
                {
                    if (occupied->getUnit() && occupied->getUnit()->hasExposedSoldier())
                    {
                        rider = occupied->getAIObject();
                        keepRider = true;
                    }
                }
            }
            if (keepRider)
            {
                if (ignore.size() > 2)
                    ignore[2] = rider;
            }
            else
            {
                ignore.resize(2);
            }

            Vec3 eye;
            eye.x = camera.m[12];
            eye.y = camera.m[13] - 0.1f;
            eye.z = camera.m[14];
            const Vec3& targetPosition = targetObject->getPosition();
            const float dx = own.x - targetPosition.x;
            const float dy = own.y - targetPosition.y;
            const float dz = own.z - targetPosition.z;
            const float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
            // The image converts with fistp: 1e14 (and any value past INT_MAX) becomes INT_MIN,
            // i.e. one sample after the clamp. [CHOSEN] spelled out for a native conversion.
            const double ratio = 0.0f < distance
                ? static_cast<double>(targetObject->getBoundingRadius()) * 30.0 / distance : 1e14;
            int samples = ratio >= 2147483648.0 || ratio <= -2147483649.0 ? INT_MIN : static_cast<int>(ratio);
            if (samples < 1)
                samples = 1;
            else if (10 < samples)
                samples = 10;
            unsigned int part = static_cast<unsigned int>(-1);
            const bool soldier = target->isOfType(ITSoldier);
            for (int k = 0; k < samples; ++k)
            {
                Vec3 point;
                point.x = 0.0f;
                point.y = 0.0f;
                point.z = 0.0f;
                Vec3 normal = point;
                if (!soldier && k == 0 && samples != 1)
                    point = targetObject->getPosition();
                else if (!soldier)
                    environment->pickRandomSensePosition(point, normal, eye, targetObject);
                else
                    environment->pickSoldierRandomSensePosition(point, normal, targetObject, part);
                bool hitFlag;
                const bool blocked =
                    environment->collideLineWithWorld(eye, point, ignore, activeBot, 0.8f, true, hitFlag);
                if (render)
                    renderUtil->addSensingLine(eye, point, side, !blocked);
                if (blocked)
                    continue;
                Information* self = equipment;
                if (!equipment->isPrimary())
                    self = informationManager->getInformation(equipment->getPrimaryObject(m_bot->getSide()));
                if (Bot* const viewer = IBotManager::instance->getBot(self->getAIObject()))
                    viewer->getSensingAgent()->getSenses()->event_seenObject(
                        handle, normal, part, target, viewer == m_bot, hitFlag);
                int iterator = -1;
                for (Information* secondary = informationManager->getInformation(
                         self->getFirstSecondaryObject(iterator, m_bot->getSide()));
                     secondary;
                     secondary = informationManager->getInformation(self->getSecondaryObject(iterator, m_bot->getSide())))
                {
                    if (Bot* const viewer = IBotManager::instance->getBot(secondary->getAIObject()))
                        viewer->getSensingAgent()->getSenses()->event_seenObject(
                            handle, normal, part, target, viewer == m_bot, hitFlag);
                }
                break;
            }
        }
        ++m_seeIndex;
        elapsed = timer->getUpdateTime() - start;
    }
    if (finished)
    {
        m_seeState = 3;
        m_seeIndex = 0;
    }
    if (slice < timer->getUpdateTime() - start)
        return;
    timer->getUpdateTime();
}

// ELF 0x19BE60 (1528 bytes). Nothing is heard within 1.5 s of the bot's own last shot. With
// equipment, the heard list (+76) is worked through once, front to back, within the slice: each
// record is popped; one whose information is gone or has neither mobile nor armament leaves the
// heard set (+64) and loses SENSE_HEARD (and is deleted unless it is also seen); a heard one
// refreshes its primary object and the primary's secondary objects (setTime(now, max(s, 0.1)));
// one unheard for more than 30 s is dropped the same way; the rest are requeued at the back.
void BotSenses::updateHearing(const double& slice)
{
    const IBFTimer* const timer = static_cast<const IBFTimer*>(CommonSingletonServer::m_iBFTimer);
    const double now = timer->getGameTime();
    if (!(now - m_bot->getTimeOfLastFire() >= 1.5))
        return;
    const double start = timer->getUpdateTime();
    const double budget = slice;
    if (!informationManager->getInformation(m_bot->getCurrentEquipment()))
        return;
    const unsigned int count = static_cast<unsigned int>(m_hearingMemCheckQueue.size());
    const int side = m_bot->getSide();
    for (unsigned int i = 0; i < count && timer->getUpdateTime() < static_cast<double>(start + budget); ++i)
    {
        SensedObject* const object = m_hearingMemCheckQueue.front().second;
        m_hearingMemCheckQueue.erase(m_hearingMemCheckQueue.begin());
        const unsigned int handle = object->m_handle;
        Information* information = informationManager->getInformation(handle);
        if (!information || (!information->getMobile() && !information->getArmament()))
        {
            m_hearingMemHelperSet.erase(handle);
            const unsigned int flags = object->m_flags;
            object->m_flags = flags & ~SensedObject::SENSE_HEARD;
            if (!(flags & SensedObject::SENSE_SEEN))
            {
                m_sensedObjects.erase(object->m_handle);
                delete object;
            }
            continue;
        }
        const float strength = hear(handle, information);
        if (0.0f < strength)
        {
            object->m_flags |= SensedObject::SENSE_HEARD_NOW;
            object->m_lastSense = SensedObject::SENSE_HEARD_NOW;
            object->m_lastSensedTime = now;
            information = informationManager->getInformation(information->getPrimaryObject(side));
            float weight = 0.1f;
            if (0.1f <= strength)
                weight = strength;
            if (information->setTime(timer->getGameTime(), weight))
                information->getAIObject()->addInformationToGrid(side);
            int iterator = -1;
            Information* secondary =
                informationManager->getInformation(information->getFirstSecondaryObject(iterator, side));
            while (secondary)
            {
                float secondaryWeight = strength;
                if (strength < 0.1f)
                    secondaryWeight = 0.1f;
                secondary->setTime(timer->getGameTime(), secondaryWeight);
                secondary = informationManager->getInformation(information->getSecondaryObject(iterator, side));
            }
        }
        else if (!(object->m_flags & SensedObject::SENSE_HEARD_NOW))
        {
            if (30.0 < now - object->m_lastSensedTime)
            {
                m_hearingMemHelperSet.erase(handle);
                const unsigned int flags = object->m_flags;
                object->m_flags = flags & ~SensedObject::SENSE_HEARD;
                if (!(flags & SensedObject::SENSE_SEEN))
                {
                    m_sensedObjects.erase(object->m_handle);
                    delete object;
                }
                continue;
            }
        }
        else
        {
            object->m_flags &= ~SensedObject::SENSE_HEARD_NOW;
            object->m_lastSensedTime = now;
        }
        m_hearingMemCheckQueue.push_back(TimedObject(now, object));
    }
}

// ELF 0x19C460 (1232 bytes). The incoming-fire strengths (+88 over the six strength types, +92
// against the equipment's own strength type), each shot weighted x10 when direct, over the fire
// list (+84). Shots younger than min(60 / max(n, 1), 5) s are recorded per shooter in the attacker
// map (+100) when newer than the one there; an older duplicate, or an old shot the map does not
// hold, is deleted. Then the attacker strength (+96) over the map, whose entries older than
// max(30 / max(size, 1), 15) s are erased (not deleted: the list owns them). The slice is unused.
void BotSenses::updateFireObjects(const double&)
{
    m_fireTotalStrength = 0.0f;
    m_fireTotalStrengthAgainstSelf = 0.0f;
    Information* const equipment = informationManager->getInformation(m_bot->getCurrentEquipment());
    IPIPhysical* physical = 0;
    if (equipment)
        physical = equipment->getPhysical();
    int ownStrengthType = 0;
    if (physical)
        ownStrengthType = physical->getStrengthType();
    const IBFTimer* const timer = static_cast<const IBFTimer*>(CommonSingletonServer::m_iBFTimer);

    float divisor = static_cast<float>(static_cast<unsigned int>(m_list84.size()));
    if (divisor < 1.0f)
        divisor = 1.0f;
    float window = 60.0f / divisor;
    if (5.0f < 60.0f / divisor)
        window = 5.0f;
    for (std::list<FireEntry>::iterator it = m_list84.begin(); it != m_list84.end(); )
    {
        FireObject* const fire = it->first;
        float weight = 10.0f;
        if (!fire->isDirect())
            weight = 1.0f;
        for (int type = 0; type < 6; ++type)
            m_fireTotalStrength = fire->getBattleStrength(type) * weight + m_fireTotalStrength;
        if (physical)
            m_fireTotalStrengthAgainstSelf =
                fire->getBattleStrength(ownStrengthType) * weight + m_fireTotalStrengthAgainstSelf;
        if (timer->getGameTime() - fire->getTime() <= static_cast<double>(window))
        {
            std::pair<std::map<unsigned int, FireObject*>::iterator, bool> inserted =
                m_map100.insert(std::make_pair(it->second, fire));
            if (inserted.second || inserted.first->second->getTime() < fire->getTime())
            {
                inserted.first->second = fire;
            }
            else if (inserted.first->second != fire)
            {
                delete fire;
                it = m_list84.erase(it);
                continue;
            }
            ++it;
        }
        else
        {
            const std::map<unsigned int, FireObject*>::iterator held = m_map100.find(it->second);
            if (held != m_map100.end() && held->second == fire)
            {
                ++it;
                continue;
            }
            delete fire;
            it = m_list84.erase(it);
        }
    }

    m_attackerStrength = 0.0f;
    float attackers = 1.0f;
    if (1.0f <= static_cast<float>(static_cast<unsigned int>(m_map100.size())))
        attackers = static_cast<float>(static_cast<unsigned int>(m_map100.size()));
    float memory = 30.0f / attackers;
    if (30.0f / attackers <= 15.0f)
        memory = 15.0f;
    const double now = timer->getGameTime();
    for (std::map<unsigned int, FireObject*>::iterator it = m_map100.begin(); it != m_map100.end(); )
    {
        if (static_cast<double>(memory) <= now - it->second->getTime())
        {
            m_map100.erase(it++);
        }
        else
        {
            m_attackerStrength = it->second->getBattleStrength(ownStrengthType) + m_attackerStrength;
            ++it;
        }
    }
}

// ELF 0x19C930: `fldl 0x20(fire)` when found, else -1000.0f widened.
bool BotSenses::isAttacking(unsigned int handle, double& time) const
{
    const std::map<unsigned int, FireObject*>::const_iterator it = m_map100.find(handle);
    const bool found = it != m_map100.end();
    if (found)
        time = it->second->getTime();
    else
        time = -1000.0f;
    return found;
}

// ELF 0x19C9A0. The shot goes to the back of the fire list. When the shooter's information is of
// another side, its primary object is reported as seen (zero position, part -1, not direct,
// flag40 off) and the shooter's seen record - looked up by the shooter's handle, not the primary's
// - gets the attack time (+0x3C).
void BotSenses::event_projectile(FireObject* fire, unsigned int handle)
{
    m_list84.push_back(FireEntry(fire, handle));
    Information* information = informationManager->getInformation(handle);
    if (!information)
        return;
    if (information->getSide() == m_bot->getSide())
        return;
    const unsigned int primaryHandle = information->getPrimaryObject(m_bot->getSide());
    information = informationManager->getInformation(primaryHandle);
    if (!information)
        return;
    Vec3 zero;
    zero.x = 0.0f;
    zero.y = 0.0f;
    zero.z = 0.0f;
    event_seenObject(primaryHandle, zero, static_cast<unsigned int>(-1), information, false, true);
    const std::map<unsigned int, TimedList::iterator>::iterator seen = m_map44.find(handle);
    if (seen != m_map44.end())
        seen->second->second->m_lastAttackTime =
            static_cast<const IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
}

// ELF 0x19CB60 (1803 bytes). The information's time is refreshed (setTime(now); a true result puts
// its object into the side's grid). A handle not in the seen list gets a record (a new one is
// SENSE_INDIRECT with +0x14 = now) that becomes SENSE_SEEN with both positions refreshed; a direct
// sighting clears INDIRECT and, once per record, books a contact (reporting a new enemy first when
// the information had none); an indirect one tells the sensing agent. It then joins the back of
// the seen list and the handle -> list-position map. A handle already in the list is only moved to
// the back when it was INDIRECT and this sighting is direct.
void BotSenses::event_seenObject(unsigned int handle, const Vec3& position, unsigned int part,
                                 Information* information, bool direct, bool notFlag40)
{
    const int side = m_bot->getSide();
    const double now = static_cast<const IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
    if (information->setTime(now))
        information->getAIObject()->addInformationToGrid(side);

    const std::map<unsigned int, TimedList::iterator>::iterator listed = m_map44.find(handle);
    if (listed == m_map44.end())
    {
        SensedObject* object;
        const SensedObjectMap::iterator known = m_sensedObjects.find(handle);
        if (known == m_sensedObjects.end())
        {
            object = checkPointer(new SensedObject(handle, true, information->getSide() != 0));
            object->m_flags |= SensedObject::SENSE_INDIRECT;
            object->m_lastSense = SensedObject::SENSE_INDIRECT;
            object->m_firstSeenTime = now;
            m_sensedObjects.insert(std::make_pair(handle, object));
        }
        else
        {
            object = known->second;
        }
        object->m_flags |= SensedObject::SENSE_SEEN;
        object->m_lastSense = SensedObject::SENSE_SEEN;
        object->m_lastPosition = information->getPosition();
        object->m_lastPositionTime = now;
        object->m_lastSeenPosition = information->getPosition();
        object->m_lastSeenTime = now;
        if (direct)
        {
            object->m_flags &= ~SensedObject::SENSE_INDIRECT;
            if (!object->m_hasContact)
            {
                if (!information->hasContact())
                    m_bot->getCommunicationAgent()->setSeenNewEnemy(information);
                information->addContact();
                object->m_hasContact = true;
            }
        }
        else
        {
            m_bot->getSensingAgent()->setSensedNewObject();
        }
        if (!notFlag40)
        {
            object->m_flags |= SensedObject::SENSE_FLAG40;
            object->m_lastSense = SensedObject::SENSE_FLAG40;
        }
        object->m_lastSensedTime = now;
        object->m_seenPosition = position;
        object->m_seenPart = part;
        m_list56.push_back(TimedObject(now, object));
        m_map44[handle] = --m_list56.end();
        return;
    }

    const TimedList::iterator entry = listed->second;
    SensedObject* const object = entry->second;
    if ((object->m_flags & SensedObject::SENSE_INDIRECT) && direct)
    {
        if (m_cursor60 == entry)
            ++m_cursor60;
        m_list56.erase(entry);
        if (!object->m_hasContact)
        {
            information->addContact();
            object->m_hasContact = true;
        }
        object->m_seenPosition = position;
        object->m_seenPart = part;
        object->m_lastPosition = information->getPosition();
        object->m_lastPositionTime = now;
        object->m_lastSeenPosition = information->getPosition();
        object->m_flags &= ~SensedObject::SENSE_INDIRECT;
        object->m_lastSeenTime = now;
        m_list56.push_back(TimedObject(now, object));
        listed->second = --m_list56.end();
    }
}

// ELF 0x19D270 (1632 bytes). Needs the bot's equipment information and 1.5 s since its own last
// shot; the bot does not hear its own equipment, its own side, or - from a non-soldier equipment -
// a soldier (type flag 0x200000). When hear() > 0: a handle already in the heard set (+64) only
// refreshes its record and the primary object's time; a new one books the information into the
// side's grid, gets (or reuses) a record, refreshes the primary (grid on a true setTime) and
// joins the back of the heard list. A brand-new record tells the sensing agent.
void BotSenses::event_heardObject(unsigned int handle)
{
    Information* const equipment = informationManager->getInformation(m_bot->getCurrentEquipment());
    if (!equipment)
        return;
    const double now = static_cast<const IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
    if (!(1.5 <= now - m_bot->getTimeOfLastFire()))
        return;
    if (handle == m_bot->getCurrentEquipment())
        return;
    Information* const information = informationManager->getInformation(handle);
    if (!information)
        return;
    if (information->getSide() == m_bot->getSide())
        return;
    if (!equipment->isOfType(ITSoldier) && information->isOfType(ITSoldier))
        return;
    const float strength = hear(handle, information);
    if (!(0.0f < strength))
        return;
    const int side = m_bot->getSide();
    const std::pair<std::set<unsigned int>::iterator, bool> inserted = m_hearingMemHelperSet.insert(handle);
    if (!inserted.second)
    {
        // The image does not check the find result.
        SensedObject* const object = m_sensedObjects.find(handle)->second;
        object->m_flags |= SensedObject::SENSE_HEARD;
        object->m_lastSense = SensedObject::SENSE_HEARD;
        object->m_lastPosition = information->getPosition();
        object->m_lastPositionTime = now;
        object->m_lastSensedTime = now;
        Information* const primary = informationManager->getInformation(information->getPrimaryObject(side));
        float weight = 0.1f;
        if (0.1f <= strength)
            weight = strength;
        primary->setTime(now, weight);
        return;
    }

    const SensedObjectMap::iterator known = m_sensedObjects.find(handle);
    information->getAIObject()->addInformationToGrid(information->getSide());
    SensedObject* object;
    if (known == m_sensedObjects.end())
    {
        object = checkPointer(new SensedObject(handle, false, information->getSide() != 0));
        m_sensedObjects.insert(std::make_pair(handle, object));
        m_bot->getSensingAgent()->setSensedNewObject();
    }
    else
    {
        object = known->second;
        object->m_flags |= SensedObject::SENSE_HEARD;
        object->m_lastSense = SensedObject::SENSE_HEARD;
    }
    object->m_handle = handle;
    object->m_flags |= SensedObject::SENSE_HEARD;
    object->m_lastSense = SensedObject::SENSE_HEARD;
    object->m_lastSensedTime = now;
    object->m_lastPosition = information->getPosition();
    object->m_lastPositionTime = now;
    Information* const primary = informationManager->getInformation(information->getPrimaryObject(side));
    float weight = 0.1f;
    if (0.1f <= strength)
        weight = strength;
    if (primary->setTime(now, weight))
        primary->getAIObject()->addInformationToGrid(side);
    m_hearingMemCheckQueue.push_back(TimedObject(now, object));
}

// ELF 0x19D8F0 (1152 bytes). Zero unless the bot has equipment, 1.5 s have passed since its own
// last shot, the object is alive (its physical plug-in's and its engine object's health > 0) and it
// has a mobile or an armament plug-in. The audible radius is the larger of the object's mobile
// sound radius (zeroed for a soldier in pose 2 or 3) and, within 5 s of a shot, its weapon's sound
// radius. Within that radius the hearing probability P interpolates the listener's mobile
// min..max by distance / the object's maximum sound radius (1 without a listener mobile); the
// object is heard (P returned) when a uniform draw exceeds (1 - max(speed / maxSpeed, 1)) * P.
// As in the image, the object's mobile is used for the maximum radius without a null check.
float BotSenses::hear(unsigned int, Information* information)
{
    Information* const equipment = informationManager->getInformation(m_bot->getCurrentEquipment());
    if (!equipment)
        return 0.0f;
    const IBFTimer* const timer = static_cast<const IBFTimer*>(CommonSingletonServer::m_iBFTimer);
    const double now = timer->getGameTime();
    if (1.5 > now - m_bot->getTimeOfLastFire())
        return 0.0f;
    if (IPIPhysical* const physical = information->getPhysical())
    {
        if (0.0f >= physical->getHealth())
            return 0.0f;
    }
    IPIMobile* const mobile = information->getMobile();
    IPIArmament* const armament = information->getArmament();
    if (!armament && !mobile)
        return 0.0f;
    if (IAIObjectPhysical* const objectPhysical = information->getAIObject()->getPhysical())
    {
        if (0.0f >= objectPhysical->getHealth())
            return 0.0f;
    }
    IPIMobile* const listener = equipment->getMobile();
    float mobileRadius;
    if (mobile)
        mobileRadius = mobile->getSoundSphereRadius();
    else
        mobileRadius = 0.0f;
    float weaponRadius = 0.0f;
    if (armament)
    {
        if (WeaponInfo* const weapon = armament->getLastFiredWeapon())
        {
            if (5.0 > now - weapon->getTimeOfFiring())
                weaponRadius = weapon->getSoundSphereRadius();
        }
    }
    if (information->isOfType(ITSoldier))
    {
        const int pose = information->getSoldierPose();
        if (pose != 1 && !(pose < 1))
        {
            if (pose <= 3)
                mobileRadius = 0.0f;
        }
    }
    float radius = weaponRadius;
    if (weaponRadius <= mobileRadius)
        radius = mobileRadius;
    float moving;
    if (listener)
    {
        moving = listener->getSpeedMagnitude() / listener->getMaxSpeed();
        if (moving <= 1.0f)
            moving = 1.0f;
    }
    else
    {
        moving = 0.0f;
    }
    const Vec3& target = information->getAIObject()->getPosition();
    const Vec3& own = equipment->getPosition();
    const float dx = own.x - target.x;
    const float dy = own.y - target.y;
    const float dz = own.z - target.z;
    const float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (distance > radius)
        return 0.0f;
    const float fraction = distance / mobile->getSoundSphereRadiusMax();
    float probability;
    if (listener)
        probability = (1.0f - fraction) * listener->getHearingProbabilityMin()
                      + listener->getHearingProbabilityMax() * fraction;
    else
        probability = 1.0f;
    const float threshold = (1.0f - moving) * probability;
    const float draw = AIRandom::m_instance->nextFloat();
    return draw > threshold ? probability : 0.0f;
}

// ELF 0x19DD70 (480 bytes). The information's primary object is looked up first (its contact is
// what a record releases). A listed handle leaves the seen list (the cursor at +60 resets to the
// end when it points at this record) and the position map, drops its contact and SENSE_SEEN, and
// is deleted unless it is also heard.
void BotSenses::forgetObject(unsigned int handle)
{
    Information* const information = informationManager->getInformation(handle);
    if (!information)
        return;
    Information* const primary =
        informationManager->getInformation(information->getPrimaryObject(m_bot->getSide()));
    const std::map<unsigned int, TimedList::iterator>::iterator listed = m_map44.find(handle);
    if (listed == m_map44.end())
        return;
    SensedObject* const object = listed->second->second;
    // [CHOSEN, not recovered] The image reads the cursor's element even at end() (its sentinel is a
    // full node in that library); the end test keeps a native list's sentinel unread.
    if (m_cursor60 != m_list56.end() && m_cursor60->second == object)
        m_cursor60 = m_list56.end();
    m_list56.erase(listed->second);
    m_map44.erase(listed);
    if (object->m_hasContact)
    {
        primary->removeContact();
        object->m_hasContact = false;
    }
    const unsigned int flags = object->m_flags;
    object->m_flags = flags & ~SensedObject::SENSE_SEEN;
    if (!(flags & SensedObject::SENSE_HEARD))
    {
        m_sensedObjects.erase(handle);
        delete object;
    }
}

// ELF 0x19DF50 (352 bytes). `information`'s primary object's transformation (row 2 forward,
// row 3 position) is read before `object` is tested; `object`'s information for that side must
// be a primary one, within 200 m (squared < 40000) and in front (positive dot with forward).
bool BotSenses::isCloseAndInFront(Information* information, IAIObject* object) const
{
    Information* const primary =
        informationManager->getInformation(information->getPrimaryObject(m_bot->getSide()));
    if (!primary)
        return false;
    const Mat4& transformation = primary->getTransformation();
    if (!object)
        return false;
    Information* const other = informationManager->getInformation(object->getInformation(information->getSide()));
    if (!other || !other->isPrimary())
        return false;
    const Vec3& position = object->getPosition();
    const float dx = position.x - transformation.m[12];
    const float dy = position.y - transformation.m[13];
    const float dz = position.z - transformation.m[14];
    return dx * dx + dy * dy + dz * dz < 40000.0f
        && 0.0f < dx * transformation.m[8] + dy * transformation.m[9] + dz * transformation.m[10];
}

namespace {

// ELF 0x197650 (448 bytes). The centre of a soldier for the sight ray: its position plus a pose
// offset (lying: y - 0.7; standing and crouching: none). Any other pose logs "Unknown pose"
// (DebugTypeError, BotSenses.cpp line 207) and leaves `offset` as the caller set it.
void soldierHack_getSoldierCentre(Vec3& centre, Vec3& offset, Information* information)
{
    if (information->getSoldierPose() == SPLying)
    {
        offset.x = 0.0f;
        offset.y = -0.7f;
        offset.z = 0.0f;
    }
    else if (information->getSoldierPose() == SPCrouching || information->getSoldierPose() == SPStanding)
    {
        offset.x = 0.0f;
        offset.y = 0.0f;
        offset.z = 0.0f;
    }
    else
    {
        Debug(DebugTypeError, "AI/AIDLL/AI/AIBot/BotSenses.cpp", 207, "") << "Unknown pose";
    }
    const Vec3& position = information->getPosition();
    centre.x = offset.x + position.x;
    centre.y = offset.y + position.y;
    centre.z = offset.z + position.z;
}

// The pose probe of updateSeeing (four inlined copies in the image): while the bot is changing
// pose (requested pose 2 or 3 differs from the equipment's current one), a second ray from where
// the eye will be tells the bot whether its firing target stays visible after the change.
// Crouching (2): y - 0.53 from standing, otherwise y + 0.38; lying (3): y - 0.91 from standing,
// otherwise y - 0.38. Reported through Bot +0x19C setPoseStatus(pose, handle, visible, hit).
void probePose(BotMain* bot, IAIObjectEnvironment* environment, const Vec3& eye, const Vec3& point,
               const std::vector<IAIObject*>& ignore, bool activeBot, float width, SoldierPose pose,
               int requested, unsigned int handle)
{
    Vec3 probe = eye;
    if (requested == 2)
        probe.y = pose == SPStanding ? eye.y - 0.53f : eye.y + 0.38f;
    else if (requested == 3)
        probe.y = eye.y - (pose == SPStanding ? 0.91f : 0.38f);
    else
        return;
    bool hit;
    const bool blocked = environment->collideLineWithWorld(probe, point, ignore, activeBot, width, true, hit);
    bot->setPoseStatus(requested, handle, !blocked, hit);
}

} // namespace

// ELF 0x19A0F0 (7533 bytes). Reads the render flag (IBotManager +0x78), the update time and the
// game time, then works through the seen list (+56) from the cursor (+60, wrapping to the front)
// until the cursor is back where it started, the slice is spent or the list is empty. Per record:
//   * dropped (seen bit cleared, deleted unless heard) when its information is gone, has no
//     object, is not a root object, is on the bot's side or side 0, or is physically dead;
//   * "in view" for a primary object or a soldier when it attacked the bot within 5 s, or the bot
//     is a soldier and it is within 2r + 3 * own radius, or it is inside the frustum and within its
//     unit's cull distance; or, with vehicle sight (equipment flag 0x10 / 0x20), close and in front;
//   * in view, a ray (0.75) from the eye (camera row 3, y - 0.1) ignoring {own primary, target,
//     rider}: an active bot target is sensed at its centre (soldier) or position; otherwise the
//     stored sense point (+4) through the object matrix (non-soldier) or the stored body part
//     (soldier, IAIObjectEnvironment +0x58); failing that, a fresh pick (+0x54 / +0x50);
//   * visible: setTime, a contact booked when it was indirect, positions and times refreshed,
//     flag 0x40 from the ray's hit output, and every other bot riding in the bot's primary object
//     and its secondaries told (event_seenObject, indirect);
//   * not visible: INDIRECT set (+0x14 = now) once; later the contact is released after 2 s, and
//     the record forgotten after 60 s, or when the target is not primary, or after 5 s when the
//     bot's equipment has no mobile, can engage it (anti-aircraft against flag 0x10), or the bot
//     is going for a flag (Bot +0x27C).
// Kept records move to the back of the list (time = now) and re-register in the map (+44).
void BotSenses::updateSeeing(const double& slice)
{
    IBotManager* const botManager = IBotManager::instance;
    IAIObjectEnvironment* const environment =
        static_cast<IAIObjectEnvironment*>(CommonSingletonServer::m_iaiObjectEnvironment);
    const bool render = botManager->getRenderBotSensing();
    AIRenderUtil* const renderUtil = render ? AIRenderUtil::m_instance : 0;
    const IBFTimer* const timer = static_cast<const IBFTimer*>(CommonSingletonServer::m_iBFTimer);
    const double start = timer->getUpdateTime();
    const double now = timer->getGameTime();
    if (m_list56.empty())
        return;

    // As in the image, the equipment information is used without a null check.
    Information* const equipment = informationManager->getInformation(m_bot->getCurrentEquipment());
    const bool ownSoldier = equipment->isOfType(ITSoldier);
    const float ownRadius3 = equipment->getPhysical()->getRadius() * 3.0f;
    std::vector<IAIObject*> ignore;
    ignore.push_back(equipment->getAIObject()->getPrimaryObject());
    ignore.push_back(0);
    ignore.push_back(0);
    const Mat4& camera = *static_cast<const Mat4*>(m_bot->getPlayer()->getCameraTransformation());
    Vec3 eye;
    eye.x = camera.m[12];
    eye.y = camera.m[13] - 0.1f;
    eye.z = camera.m[14];

    // [CHOSEN, not recovered] The image stops when the cursor reaches the node it started on. A
    // kept record is erased and re-inserted at the back, and the pool allocator hands the freed
    // node straight back, so the start node survives that move; splice keeps the node the same
    // way. When the start record is forgotten its node is gone, and the image's test then only
    // matches if the pool reuses the node later; here the start is dropped and the slice or an
    // empty list ends the pass.
    const TimedList::iterator stop = m_cursor60;
    bool stopValid = true;
    double elapsed;
    do
    {
        if (m_cursor60 == m_list56.end())
            m_cursor60 = m_list56.begin();
        const TimedList::iterator current = m_cursor60++;
        SensedObject* const object = current->second;
        const unsigned int handle = object->m_handle;
        Information* const information = informationManager->getInformation(handle);

        bool drop = !information || !information->getAIObject() || !information->isRootObject()
                 || information->getSide() == m_bot->getSide() || information->getSide() == 0
                 || (information->getPhysical() && information->getPhysical()->getHealth() <= 0.0f);
        bool forget = false;
        if (!drop)
        {
            IAIObject* const target = information->getAIObject();
            const bool primary = information->isPrimary();
            const bool soldier = information->isOfType(ITSoldier);
            const float radius = target->getBoundingRadius();
            const Vec3& position = target->getPosition();
            const float dx = position.x - eye.x;
            const float dy = position.y - eye.y;
            const float dz = position.z - eye.z;
            const float distSq = dz * dz + dy * dy + dx * dx;
            bool inView = false;
            if (primary || soldier)
            {
                inView = true;
                if (5.0 < now - object->m_lastAttackTime)
                {
                    const float close = radius + radius + ownRadius3;
                    if (!ownSoldier || close * close < distSq)
                    {
                        if (!m_frustum->inside(target->getPosition(), radius)
                            || (information->getUnit() && information->getUnit()->getCullDistanceSqr() < distSq))
                            inView = false;
                    }
                }
            }
            const bool vehicleSight = equipment->isOfType(ITAir) || equipment->isOfType(ITHelicopter);
            const bool consider = inView || (vehicleSight && isCloseAndInFront(equipment, target));
            bool visible = consider;
            Vec3 point;
            point.x = 0.0f;
            point.y = 0.0f;
            point.z = 0.0f;
            Vec3 sense = point;
            Bot* const targetBot = IBotManager::instance->getBot(target);
            bool activeBot = false;
            if (targetBot)
                activeBot = 0 < static_cast<int>(targetBot->getLodLevel());
            unsigned int part = static_cast<unsigned int>(-1);
            bool hit = false;

            if (consider)
            {
                ignore[1] = target;
                // The rider slot keeps the previous record's object when none is found here.
                if (IPIUnit* const unit = information->getUnit())
                {
                    if (!soldier)
                    {
                        if (unit->hasExposedSoldier())
                        {
                            if (Information* const occupier =
                                    informationManager->getInformation(unit->getOccupier(m_bot->getSide())))
                                ignore[2] = occupier->getAIObject();
                        }
                    }
                    else if (Information* const occupied =
                                 informationManager->getInformation(unit->getOccupiedObject(m_bot->getSide())))
                    {
                        if (occupied->getUnit() && occupied->getUnit()->hasExposedSoldier())
                            ignore[2] = occupied->getAIObject();
                    }
                }

                // First ray: an active bot target at its centre (soldier) or any target at its
                // position (non-soldier); a soldier that is not an active bot gets no first ray.
                if (soldier)
                {
                    if (!activeBot)
                    {
                        visible = false;
                    }
                    else
                    {
                        soldierHack_getSoldierCentre(point, sense, information);
                        visible = !environment->collideLineWithWorld(eye, point, ignore, activeBot, 0.75f, true, hit);
                        const SoldierPose pose = equipment->getSoldierPose();
                        const int requested = m_bot->getRequestedSolderPose();
                        if (visible && pose != requested && pose != SPSwimming
                            && m_bot->getFiringTarget() == handle)
                            probePose(m_bot, environment, eye, point, ignore, activeBot, 0.75f, pose, requested, handle);
                    }
                }
                else
                {
                    sense.x = 0.0f;
                    sense.y = 0.0f;
                    sense.z = 0.0f;
                    point = target->getPosition();
                    visible = !environment->collideLineWithWorld(eye, point, ignore, activeBot, 0.75f, true, hit);
                }

                // Second ray (not for an active bot): the stored sense point.
                if (!visible && !activeBot)
                {
                    if (!soldier)
                    {
                        sense = object->m_seenPosition;
                        const Mat4& matrix = target->getTransformation();
                        point.x = matrix.m[8] * sense.z + matrix.m[0] * sense.x + matrix.m[4] * sense.y + matrix.m[12];
                        point.y = matrix.m[1] * sense.x + matrix.m[5] * sense.y + matrix.m[9] * sense.z + matrix.m[13];
                        point.z = sense.y * matrix.m[6] + matrix.m[2] * sense.x + sense.z * matrix.m[10] + matrix.m[14];
                        visible = !environment->collideLineWithWorld(eye, point, ignore, false, 0.75f, true, hit);
                    }
                    else
                    {
                        const bool sensed = environment->getSoldierSensePosition(point, sense, target, object->m_seenPart);
                        part = object->m_seenPart;
                        if (sensed)
                        {
                            visible = !environment->collideLineWithWorld(eye, point, ignore, false, 0.75f, true, hit);
                            const SoldierPose pose = equipment->getSoldierPose();
                            const int requested = m_bot->getRequestedSolderPose();
                            if (visible && pose != requested && m_bot->getFiringTarget() == handle)
                                probePose(m_bot, environment, eye, point, ignore, false, 0.8f, pose, requested, handle);
                        }
                    }
                }

                // Third ray (not for an active bot): a fresh sense point.
                if (!visible && !activeBot)
                {
                    if (soldier)
                    {
                        environment->pickSoldierRandomSensePosition(point, sense, target, part);
                        object->m_seenPart = part;
                    }
                    else
                    {
                        environment->pickRandomSensePosition(point, sense, eye, target);
                    }
                    visible = !environment->collideLineWithWorld(eye, point, ignore, false, 0.75f, true, hit);
                    const SoldierPose pose = equipment->getSoldierPose();
                    const int requested = m_bot->getRequestedSolderPose();
                    if (visible && requested != pose && m_bot->getFiringTarget() == handle)
                        probePose(m_bot, environment, eye, point, ignore, false, 0.8f, pose, requested, handle);
                }
            }

            if (visible)
            {
                information->setTime(now);
                if (object->m_flags & SensedObject::SENSE_INDIRECT)
                {
                    if (!object->m_hasContact)
                    {
                        information->addContact();
                        object->m_hasContact = true;
                    }
                    object->m_flags &= ~SensedObject::SENSE_INDIRECT;
                }
                object->m_lastPosition = point;
                object->m_lastPositionTime = now;
                object->m_lastSeenPosition = point;
                object->m_lastSeenTime = now;
                object->m_seenPosition = sense;
                if (!hit)
                {
                    object->m_flags |= SensedObject::SENSE_FLAG40;
                    object->m_lastSense = SensedObject::SENSE_FLAG40;
                }
                else
                {
                    object->m_flags &= ~SensedObject::SENSE_FLAG40;
                }

                // Every other bot riding in this bot's primary object or its secondaries.
                Information* self = equipment;
                if (!equipment->isPrimary())
                    self = informationManager->getInformation(equipment->getPrimaryObject(m_bot->getSide()));
                const auto tellRider = [&](Bot* rider)
                {
                    if (!rider || rider == m_bot)
                        return;
                    rider->getSensingAgent()->getSenses()->event_seenObject(
                        handle, object->m_seenPosition, part, information, false, hit);
                    if (botManager->getRenderBotSensing() && AIRenderUtil::m_instance)
                        AIRenderUtil::m_instance->addSensingLine(eye, sense, m_bot->getPlayer()->getSide(), visible);
                };
                tellRider(IBotManager::instance->getBot(self->getAIObject()));
                int iterator = -1;
                for (Information* secondary = informationManager->getInformation(
                         self->getFirstSecondaryObject(iterator, m_bot->getSide()));
                     secondary;
                     secondary = informationManager->getInformation(self->getSecondaryObject(iterator, m_bot->getSide())))
                    tellRider(IBotManager::instance->getBot(secondary->getAIObject()));
            }
            else if (!(object->m_flags & SensedObject::SENSE_INDIRECT))
            {
                object->m_lastSense = SensedObject::SENSE_INDIRECT;
                object->m_flags |= SensedObject::SENSE_INDIRECT;
                object->m_firstSeenTime = now;   // here: when it went out of sight
            }
            else
            {
                const double unseen = now - object->m_firstSeenTime;
                Information* const lost = informationManager->getInformation(handle);
                if (2.0 < unseen && object->m_hasContact)
                {
                    object->m_hasContact = false;
                    lost->removeContact();
                    int iterator = -1;
                    for (Information* secondary = informationManager->getInformation(
                             lost->getFirstSecondaryObject(iterator, m_bot->getSide()));
                         secondary;
                         secondary = informationManager->getInformation(lost->getSecondaryObject(iterator, m_bot->getSide())))
                        secondary->removeContact();
                }
                // As in the image, the equipment information is used without a null check.
                Information* const own = informationManager->getInformation(m_bot->getCurrentEquipment());
                IPIArmament* const armament = own->getArmament();
                const bool canEngage = armament && lost->isOfType(ITAir) && 0 < armament->isAntiAircraft();
                if (60.0 < unseen || !primary || (!own->getMobile() && 5.0 < unseen)
                    || (canEngage && 5.0 < unseen) || (m_bot->getIsGoingForFlag() && 5.0 < unseen))
                    forget = true;
            }
            if (render)
                renderUtil->addSensingLine(eye, point, m_bot->getSide(), visible);
        }

        if (drop || forget)
        {
            if (current == stop)
                stopValid = false;
            m_list56.erase(current);
            m_map44.erase(handle);
            const unsigned int flags = object->m_flags;
            object->m_flags = flags & ~SensedObject::SENSE_SEEN;
            if (!(flags & SensedObject::SENSE_HEARD))
            {
                m_sensedObjects.erase(handle);
                delete object;
            }
        }
        else
        {
            current->first = now;
            m_list56.splice(m_list56.end(), m_list56, current);
            m_map44[handle] = --m_list56.end();
        }
        elapsed = timer->getUpdateTime() - start;
    } while ((!stopValid || m_cursor60 != stop) && elapsed < slice && !m_list56.empty());
}

} // namespace ai
} // namespace hfe
} // namespace dice
