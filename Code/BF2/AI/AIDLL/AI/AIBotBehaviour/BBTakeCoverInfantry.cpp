// Infantry cover selection and per-quadrant threat bookkeeping.

#include "BBTakeCoverInfantry.h"
#include "../AIUtils/CustomCurve.h"
#include "../AIBot/BotMain.h"
#include "../AIBot/BotMemory.h"
#include "../AIBot/SensedObject.h"
#include "../AIBotBehaviourPattern/BBPTakeCoverInfantry.h"
#include "../AIKnowledge/BFInformationEnvironment.h"
#include "../AIKnowledge/DangerObject.h"
#include "../AIKnowledge/IPICover.h"
#include "../AIKnowledge/IPIMobile.h"
#include "../AIKnowledge/IPIPhysical.h"
#include "../AIKnowledge/IPIUnit.h"
#include "../AIKnowledge/IPIArmament.h"
#include "../AIKnowledge/WeaponInfo.h"
#include "../AIPlanInterpreter/VehicleControlInline.h"
#include "../AIUtils/checkPointer.h"
#include "../Main/AISettingsInterfaces.h"
#include "../Main/AIMain.h"
#include "../Main/CommonSingletonServer.h"
#include "../Main/IAIPathfinding.h"
#include "../AIStrategic/SAI.h"
#include "../AIStrategicEntity/AIStrategicArea.h"
#include "../AIStrategicEntity/AIStrategicObjectManager.h"
#include "../AIStrategicEntity/CoverPosition/CoverPositionManager.h"
#include "../../../AIInterface/BFKnowledge/BFTimer.h"
#include "../../../AIInterface/BFKnowledge/BFObjectEnvironment.h"
#include "../../../../Math/BoundingBox.h"
#include "../../../../Math/LocalBoundingBox.h"
#include "../../../../Core/Debug.h"

#include "../../../../Math/Vec3.h"

#include <string>
#include <algorithm>
#include <cmath>
#include <iostream>
// static init: ELF 0x2247B0, 0x2248A0, 0x2248D0: iostream Init and four direction vectors.

namespace dice {
namespace hfe {
namespace ai {
namespace {
const Vec3 north = {0.0f, 0.0f, 1.0f};
const Vec3 east = {1.0f, 0.0f, 0.0f};
const Vec3 northEast = {0.707106769f, 0.0f, 0.707106769f};
const Vec3 southEast = {0.707106769f, 0.0f, -0.707106769f};

// ELF 0x220C70.
void computeQuadrants(int& first, int& second, const Vec3& position, const Vec3& target)
{
    const Vec3 delta = {target.x - position.x, target.y - position.y, target.z - position.z};
    const float ne = delta.x * northEast.x + delta.y * northEast.y + delta.z * northEast.z;
    const float se = delta.x * southEast.x + delta.y * southEast.y + delta.z * southEast.z;
    first = ne < 0.0f ? (se < 0.0f ? 3 : 1) : (!(se < 0.0f) ? 2 : 0);
    second = delta.z < 0.0f ? (!(delta.x < 0.0f) ? 6 : 5) : (delta.x < 0.0f ? 7 : 4);
}

// ELF 0x220D80: the direction is not normalized here.
int closestQuadFromDirection(Vec3 direction)
{
    direction.y = 0.0f;
    const Vec3 candidates[] = {north, {0.0f, 0.0f, -1.0f}, east, {-1.0f, 0.0f, 0.0f},
                               northEast, {-0.707106769f, 0.0f, -0.707106769f},
                               southEast, {-0.707106769f, 0.0f, 0.707106769f}};
    for (int i = 0; i < 8; ++i)
        if (direction.x * candidates[i].x + direction.y * candidates[i].y
            + direction.z * candidates[i].z >= 0.906307817f)
            return i;
    return -1;
}

// ELF 0x220F70.
Vec3 getQuadRecommendedLookDirection(Information* information, int quadrant)
{
    Vec3 direction = {};
    switch (quadrant)
    {
    case 0: direction = {-1.0f, 0.0f, 0.0f}; break;
    case 1: direction = {1.0f, 0.0f, 0.0f}; break;
    case 2: direction = {0.0f, 0.0f, 1.0f}; break;
    case 3: direction = {0.0f, 0.0f, -1.0f}; break;
    case 4: direction = {-0.707106769f, 0.0f, 0.707106769f}; break;
    case 5: direction = {0.707106769f, 0.0f, -0.707106769f}; break;
    case 6: direction = {0.707106769f, 0.0f, 0.707106769f}; break;
    case 7: direction = {-0.707106769f, 0.0f, -0.707106769f}; break;
    default: return direction;
    }
    const Mat4& transform = information->getTransformation();
    const Vec3 up = {transform.m[4], transform.m[5], transform.m[6]};
    const Vec3 result = {up.y * direction.z - up.z * direction.y,
                         up.z * direction.x - up.x * direction.z,
                         up.x * direction.y - up.y * direction.x};
    return result;
}

// ELF 0x221090. A threatening neighbour still returns the current area's position.
bool getDangerPositionFromSAI(Bot* bot, Vec2& position)
{
    ISAI* const sai = IAIMain::m_instance->getSAI(bot->getSide());
    const int current = sai->getBotCurrentPos(static_cast<int>(bot->getBotId()));
    AIStrategicObject* const object = AIStrategicObjectManager::getInstance()->getStrategicObject(current);
    if (!object || object->getType() != 0)
        return false;
    AIStrategicArea* const area = static_cast<AIStrategicArea*>(object);
    const int status = static_cast<int>(area->getStatus(bot->getSide()));
    if (status == 1 || status == 2)
    {
        position = area->getPosition(bot->getSide());
        return true;
    }
    if (status != 0)
        return false;
    for (auto it = area->m_neighbouringObjects.begin(); it != area->m_neighbouringObjects.end(); ++it)
    {
        AIStrategicObject* const neighbour = *it;
        if (neighbour && neighbour->getType() == 0)
        {
            const int neighbourStatus = static_cast<int>(neighbour->getStatus(bot->getSide()));
            if (neighbourStatus == 1 || neighbourStatus == 2)
            {
                // shipped defect: this is the current area, not the threatening neighbour.
                position = area->getPosition(bot->getSide());
                return true;
            }
        }
    }
    return false;
}

// [INFERRED] local packaging of repeated inline clamps; unordered input is retained.
float clampCoverWeight(float value)
{
    if (value < 0.0f) return 0.0f;
    if (value > 1.0f) return 1.0f;
    return value;
}
} // namespace

// ELF 0x221230 (C2), 0x2218F0 (C1).
BBTakeCoverInfantry::BBTakeCoverInfantry(unsigned char index, unsigned char queue, PlanGenerator* pattern,
                                       UrgeCurve* curve, float* modifiers)
    : BotBehaviour(modifiers, curve,
                   static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getMaxNBots())
{
    m_count = static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getMaxNBots();
    m_dangers = checkPointer(new DangerObject*[
        static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getMaxNBots()]);
    m_index = index;
    m_queue = queue;
    m_pattern = pattern;
    m_targets = checkPointer(new unsigned int[m_count]);
    // shipped defect: the first pointer array is overwritten and leaked.
    m_dangers = checkPointer(new DangerObject*[m_count]);
    m_fireDangers = checkPointer(new FireDanger[m_count]);
    m_infoDangers = checkPointer(new InfoDanger[m_count]);
    for (int i = 0; i < m_count; ++i)
    {
        m_targets[i] = 0xFFFFFFFFu;
        m_urgency[i] = 0.0f;
        m_dangers[i] = 0;
    }
}

// ELF 0x221FB0 (D2), 0x2220A0 (D1), 0x222190 (D0).
BBTakeCoverInfantry::~BBTakeCoverInfantry()
{
    // shipped defect: selected pointers can point inside the separately owned wrapper arrays.
    for (int i = 0; i < m_count; ++i)
        ::operator delete(m_dangers[i]);
    if (m_targets) delete[] m_targets;
    if (m_dangers) delete[] m_dangers;
    if (m_fireDangers) delete[] m_fireDangers;
    if (m_infoDangers) delete[] m_infoDangers;
}

// ELF 0x222340
float BBTakeCoverInfantry::calculateUrgency(Bot* bot, bool evaluate, float /*scale*/)
{
    if (!evaluate)
    {
        m_targetChanged[bot->getBotId()] = false;
        if (m_pattern->isInitiated(bot))
            return m_urgency[bot->getBotId()];
        m_urgency[bot->getBotId()] = 0.0f;
        return 0.0f;
    }
    const int side = bot->getSide();
    const unsigned int id = bot->getBotId();
    InformationManager* const manager = informationManager;
    Information* const self = manager->getInformation(bot->getCurrentEquipment());
    IPIPhysical* const physical = self->getPhysical();
    InformationManager* const sectorManager = informationManager;
    Information* const sector = sectorManager->getInformation(self->getMobile()->getSector());
    FireDanger fireDangers[8];
    InfoDanger infoDangers[8];
    DangerObject* dangers[8] = {};
    float sums[8] = {}, maxima[8] = {};
    float total = 0.0f;
    unsigned int matchedOrders = 0;
    const Vec3 selfPosition = self->getPosition();
    const int strengthType = physical->getStrengthType();
    const double now = static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
    const auto* incoming = bot->getIncommingFire();
    m_lastIncomingFireTime = 0.0;
    for (auto it = incoming->begin(); it != incoming->end(); ++it)
    {
        FireObject* const fire = it->first;
        int first, second;
        computeQuadrants(first, second, selfPosition, fire->getOrigin());
        const float distance = vehiclecontrol::distanceVec3(selfPosition, fire->getTarget());
        const float divisor = distance <= 0.0f ? 0.1f : distance;
        const float attenuation = clampCoverWeight(4.0f / divisor);
        float strength = fire->getBattleStrength(strengthType) * attenuation * 15.0f;
        m_lastIncomingFireTime = fire->getTime();
        if (fire->isDirect()) strength += strength;
        sums[first] += strength;
        sums[second] += strength;
        total += strength;
        if (strength > maxima[first])
        {
            maxima[first] = strength;
            if (!dangers[first]) dangers[first] = &fireDangers[first];
            static_cast<FireDanger*>(dangers[first])->m_fire = fire;
        }
        if (strength > maxima[second])
        {
            // shipped defect: the second quadrant's maximum is not updated for fire.
            if (!dangers[second]) dangers[second] = &fireDangers[second];
            static_cast<FireDanger*>(dangers[second])->m_fire = fire;
        }
    }
    // [INFERRED] local packaging of the repeated order-list search.
    const auto weightOrder = [&](unsigned int handle, float& strength)
    {
        if (bot->isOrderedBehaviour(m_index) && bot->isTakingOrders()
            && matchedOrders < bot->getOrderList(m_index)->size())
        {
            const auto comparisonEnd = bot->getOrderList(m_index)->end();
            const auto begin = bot->getOrderList(m_index)->begin();
            const auto searchEnd = bot->getOrderList(m_index)->end();
            if (std::find(begin, searchEnd, handle) != comparisonEnd)
            {
                ++matchedOrders;
                strength *= bot->getMemory()->m_float8;
            }
        }
    };
    const auto& sensed = bot->getSensedObjects();
    for (auto it = sensed.begin(); it != sensed.end(); ++it)
    {
        SensedObject* const sensedObject = it->second;
        const unsigned int handle = sensedObject->m_handle;
        Information* const information = informationManager->getInformation(handle);
        if (!information || information->getSide() != 0 || !information->getUnit()
            || !information->getUnit()->isOccupied())
            continue;
        IPIUnit* const unit = information->getUnit();
        WeaponInfo* weapon = 0;
        if (information->getArmament()) weapon = information->getArmament()->getLastFiredWeapon();
        int first, second;
        computeQuadrants(first, second, selfPosition, information->getPosition());
        const float measured = vehiclecontrol::distanceVec3(selfPosition, information->getPosition());
        const float distance = measured > 2.0f ? measured : 2.0f;
        float strength = 0.0f;
        if (sensedObject->m_flags & 2)
        {
            int iterator = information->getFirstSecondaryObjectIterator();
            unsigned int secondary = information->getSecondaryObject(iterator, side);
            Information* current = information;
            do
            {
                const float battleStrength = current->getUnit()->getBattleStrengths()[strengthType];
                float part = battleStrength * battleStrength / distance;
                // The first contribution is the primary's but uses the first secondary's handle.
                weightOrder(secondary, part);
                strength += part;
                secondary = information->getSecondaryObject(iterator, side);
                current = informationManager->getInformation(secondary);
            } while (current);
            if (sensedObject->m_flags & 8)
            {
                const double age = now - sensedObject->m_firstSeenTime;
                const float fraction = static_cast<float>((age < 30.0 ? age : 30.0) / 30.0);
                strength *= (1.0f - fraction) * 0.5f + 0.5f;
            }
        }
        else if (sensedObject->m_flags & 1)
            strength = information->getSecurity() * unit->getBattleStrengths()[strengthType] / distance;
        if ((sensedObject->m_flags & 3) && weapon)
        {
            if (distance >= weapon->getMaxRange()) continue;
            strength *= 1.0f - SCurve::calculate(distance / weapon->getMaxRange());
        }
        if (bot->isOrderedObject(handle, this))
        {
            ++matchedOrders;
            strength *= bot->getMemory()->m_float8;
        }
        // shipped defect: both additions target the first quadrant.
        sums[first] += strength;
        sums[first] += strength;
        total += strength;
        const int quadrants[] = {first, second};
        for (int q : quadrants)
            if (strength > maxima[q])
            {
                maxima[q] = strength;
                if (!dangers[q] || !dangers[q]->isInformation()) dangers[q] = &infoDangers[q];
                static_cast<InfoDanger*>(dangers[q])->m_information = handle;
            }
    }
    bool changed = true;
    unsigned int cover = 0xFFFFFFFFu;
    Vec3 coverPosition = {};
    float projection = 0.0f, multiplier;
    if (total > 0.0f)
    {
        DangerObject* strongest = 0;
        float maximum = 0.0f;
        for (int q = 0; q < 8; ++q)
            if (sums[q] > maximum) { maximum = sums[q]; strongest = dangers[q]; }
        if (!strongest) { unInit(bot); return 0.0f; }
        if (m_dangers[id]) changed = !(*m_dangers[id] == *strongest);
        if (strongest->isInformation())
        {
            m_dangers[id] = &m_infoDangers[id];
            m_infoDangers[id].m_information = static_cast<InfoDanger*>(strongest)->m_information;
        }
        else
        {
            m_dangers[id] = &m_fireDangers[id];
            m_fireDangers[id].m_fire = static_cast<FireDanger*>(strongest)->m_fire;
        }
        Information* threat = 0;
        DangerObject* const danger = m_dangers[id];
        if (danger->isInformation())
        {
            Information* const information = informationManager->getInformation(
                static_cast<InfoDanger*>(danger)->m_information);
            if (information && information->getMobile())
                threat = informationManager->getInformation(information->getMobile()->getSector());
        }
        Vec3 threatPosition = {};
        if (!m_dangers[id]->getPosition(threatPosition)
            || !getCoverObject(bot, threatPosition, threat, false, cover, coverPosition, projection, multiplier, false))
        { unInit(bot); return 0.0f; }
    }
    if (total <= 0.0f) { unInit(bot); return 0.0f; }
    if (bot->isOrderedBehaviour(m_index) && bot->isTakingOrders() && bot->getOrderList(m_index)->empty())
        total *= bot->getMemory()->m_float8;
    m_targetChanged[id] = changed || cover == 0xFFFFFFFFu || cover != m_targets[id];
    m_targets[id] = cover;
    DangerObject* const danger = m_dangers[id];
    if (danger->isInformation())
    {
        Information* const information = informationManager->getInformation(
            static_cast<InfoDanger*>(danger)->m_information);
        std::vector<IAIObject*> ignore;
        ignore.push_back(self->getAIObject());
        ignore.push_back(information->getAIObject());
        const bool coarse = bot->getLodLevel() != BotLodFull;
        const Vec3& to = information->getPosition();
        const Vec3& from = self->getPosition();
        bool hit;
        if (static_cast<IAIObjectEnvironment*>(CommonSingletonServer::m_iaiObjectEnvironment)
                ->collideLineWithWorld(from, to, ignore, coarse, 1.0f, true, hit))
        { m_urgency[id] = 0.0f; return 0.0f; }
    }
    if (m_targetChanged[id])
    {
        BBPTakeCoverInfantry* const pattern = static_cast<BBPTakeCoverInfantry*>(m_pattern);
        Vec3 unused = {};
        danger->getPosition(unused);
        if (danger->isInformation())
            pattern->init(bot, static_cast<InfoDanger*>(danger)->m_information, coverPosition, m_targets[id]);
        else
        {
            Vec3 threatPosition = {};
            danger->getPosition(threatPosition);
            pattern->init(bot, threatPosition, coverPosition, m_targets[id]);
        }
        m_urgency[id] = total;
        if (sector) m_urgency[id] *= 0.5f;
    }
    else
    {
        const float x = self->getPosition().x;
        const float z = self->getPosition().z;
        const float dx = coverPosition.x - x;
        const float dz = coverPosition.z - z;
        if (dx * dx + dz * dz < 4.0f)
            m_urgency[id] = 0.0f;
        else
        {
            const float age = static_cast<float>(now - m_lastIncomingFireTime);
            m_urgency[id] *= clampCoverWeight(5.0f / age);
        }
    }
    if (m_urgency[id] < 0.2f) m_urgency[id] = 0.0f;
    return m_urgency[id];
}

// ELF 0x222290
void BBTakeCoverInfantry::unInit(Bot* bot)
{
    if (m_pattern->isInitiated(bot))
    {
        m_targetChanged[bot->getBotId()] = true;
        m_pattern->unInit(bot);
    }
    // shipped defect: the changed flag just set above is immediately cleared again.
    m_targetChanged[bot->getBotId()] = false;
    m_urgency[bot->getBotId()] = 0.0f;
    m_dangers[bot->getBotId()] = 0;
}

// ELF 0x223920
bool BBTakeCoverInfantry::getCoverObject(Bot* bot, const Vec3& threatPosition, Information* threat,
                                       bool unrestricted, unsigned int& cover, Vec3& position,
                                       float& projection, float& multiplier, bool restrictToArea)
{
    InformationManager* const manager = informationManager;
    Information* const self = manager->getInformation(bot->getCurrentEquipment());
    IPIMobile* const mobile = self->getMobile();
    LocalBoundingBox* const selfBox = const_cast<LocalBoundingBox*>(self->getPhysical()->getLocalBoundingBox());
    const float selfX = selfBox->getSize().x;
    const float selfZ = selfBox->getSize().z;
    const float selfWidth = selfZ > selfX ? selfZ : selfX;
    const float selfHeight = selfBox->getSize().y;
    ISAI* const sai = IAIMain::m_instance->getSAI(bot->getSide());
    AIStrategicObjectManager* const strategic = AIStrategicObjectManager::getInstance();
    AIStrategicObject* const ordered = strategic->getStrategicObject(
        sai->getBotOrderedPos(static_cast<int>(bot->getBotId())));
    multiplier = 1.0f;
    cover = 0xFFFFFFFFu;
    Vec2 coverPosition = {}, from = {}, to = {};
    vec3ToVec2XZ(self->getPosition(), from);
    vec3ToVec2XZ(threatPosition, to);
    if (CoverPositionManager::m_instance->getClosestAttackCoverPosition(from, to, 30.0f, -1.0f, coverPosition))
    {
        vec2ToVec3XZ(coverPosition, position);
        cover = 0;
        return true;
    }
    std::vector<unsigned int> candidates;
    static_cast<IAIInformationEnvironment*>(AISingletonServer::m_iaiInformationEnvironment)
        ->getCover(*bot, candidates);
    float best = 0.0f;
    for (auto it = candidates.begin(); it != candidates.end(); ++it)
    {
        Information* const candidate = informationManager->getInformation(*it);
        // shipped defect: getName is called before the null check.
        candidate->getName();
        if (!candidate || !candidate->getPhysical() || !candidate->getCover())
            continue;
        IPIPhysical* const physical = candidate->getPhysical();
        IPICover* const coverInfo = candidate->getCover();
        if (threat && threat->getAIObject() == candidate->getAIObject())
            continue;
        const Vec3 candidatePosition = candidate->getPosition();
        const Vec3& selfFirst = self->getPosition();
        const Vec3 towardThreat = {threatPosition.x - selfFirst.x, threatPosition.y - selfFirst.y,
                                  threatPosition.z - selfFirst.z};
        const Vec3& selfSecond = self->getPosition();
        const Vec3 towardCover = {candidatePosition.x - selfSecond.x, candidatePosition.y - selfSecond.y,
                                 candidatePosition.z - selfSecond.z};
        const float threatDistance = std::sqrt(towardThreat.x * towardThreat.x + towardThreat.y * towardThreat.y
                                               + towardThreat.z * towardThreat.z);
        const float coverDistance = std::sqrt(towardCover.x * towardCover.x + towardCover.y * towardCover.y
                                              + towardCover.z * towardCover.z);
        const float dot = towardThreat.x * towardCover.x + towardThreat.y * towardCover.y
                        + towardThreat.z * towardCover.z;
        if (!unrestricted && dot / coverDistance > threatDistance * 0.5f)
            continue;
        Vec3 goal = {};
        if (coverInfo->getPortalCount())
            Debug(DebugTypeError, "AI/AIDLL/AI/AIBotBehaviour/BBTakeCoverInfantry.cpp", 1087, "")
                << "What portals? They have been removed for BF2.";
        coverInfo->getPortalCount();
        Vec3 away = {candidatePosition.x - threatPosition.x, candidatePosition.y - threatPosition.y,
                      candidatePosition.z - threatPosition.z};
        vehiclecontrol::normalizeVec3(away);
        const float nearRadius = physical->getRadius() * 0.75f;
        const Vec3 near = vehiclecontrol::alongFrom(candidatePosition, away, nearRadius);
        const float farRadius = physical->getRadius() * 2.0f;
        const Vec3 far = vehiclecontrol::alongFrom(candidatePosition, away, farRadius);
        const int equipmentType = self->getUnit()->getEquipmentType();
        const int vehicle = mobile->getVehicleNumber();
        unsigned int island, polygon;
        if (!IAIPathfinding::instance->findClosestValidPolygon(island, polygon, goal, vehicle,
                                                               near, far, 10, equipmentType))
            continue;
        LocalBoundingBox* const box = const_cast<LocalBoundingBox*>(physical->getLocalBoundingBox());
        const float boxX = box->getSize().x;
        const float boxZ = box->getSize().z;
        const float width = boxZ > boxX ? boxZ : boxX;
        const float height = box->getSize().y;
        const float horizontal = clampCoverWeight(width / selfWidth * 0.2f);
        const float vertical = clampCoverWeight(height / selfHeight * 0.5f);
        const float sizeRating = horizontal * 0.25f + vertical * 0.75f;
        float overlap = 1.0f;
        if (restrictToArea && ordered)
        {
            AIStrategicArea* const area = static_cast<AIStrategicArea*>(ordered);
            Vec2 unused = {};
            const Vec2 centre = area->getPosition(bot->getSide());
            const Vec2 maximum = getBoxMaxCorner(area->m_centre, centre, unused);
            BoundingBox worldBox = {};
            worldBox.initWithLocalBoundingBox(*box);
            worldBox.transform(candidate->getTransformation());
            const Vec3 minimum3 = worldBox.getMin();
            const Vec3 maximum3 = worldBox.getMax();
            const Vec2 minimum2 = {minimum3.x, minimum3.z};
            const Vec2 maximum2 = {maximum3.x, maximum3.z};
            Vec2 intersectionMin = {}, intersectionMax = {};
            overlap = ::dice::hfe::getBoxIntersections(area->m_centre, maximum, minimum2, maximum2,
                                                       intersectionMin, intersectionMax) ? 1.0f : 0.0f;
        }
        const Vec3& selfThird = self->getPosition();
        const Vec3& candidateAgain = candidate->getPosition();
        const float distance = vehiclecontrol::distanceVec3(candidateAgain, selfThird);
        float rating = coverInfo->getCoverValue() * sizeRating / distance * overlap;
        if (bot->isOrderedObject(*it, this))
            rating *= bot->getMemory()->m_float8;
        if (rating > best)
        {
            best = rating;
            cover = *it;
            position = goal;
            projection = dot;
            multiplier *= 1.3f;
        }
        else if (rating > 0.0f)
            multiplier *= rating / best * 0.3f + 1.0f;
    }
    return cover != 0xFFFFFFFFu;
}

} // namespace ai
} // namespace hfe
} // namespace dice
