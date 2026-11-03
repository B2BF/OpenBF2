// Cover urgency from directional threat strengths and reachable cover geometry.

// static init: ELF 0x2204C0 (__tcf_0 0x220520, _GLOBAL__I 0x220550): <iostream>'s Init object only.
#include <iostream>

#include "BBTakeCover.h"

#include "CustomCurve.h"
#include "../AIBot/BotMain.h"
#include "../AIBot/BotMemory.h"
#include "../AIBotBehaviourPattern/BBPTakeCover.h"
#include "../AIKnowledge/BFInformationEnvironment.h"
#include "../AIKnowledge/DangerObject.h"
#include "../AIKnowledge/IPICover.h"
#include "../AIKnowledge/IPIMobile.h"
#include "../AIKnowledge/IPIPhysical.h"
#include "../AIKnowledge/IPIUnit.h"
#include "../AIPlanInterpreter/VehicleControlInline.h"
#include "../AIUtils/VehicleOccupancy.h"
#include "../AIUtils/checkPointer.h"
#include "../Main/AISettingsInterfaces.h"
#include "../Main/CommonSingletonServer.h"
#include "../Main/IAIPathfinding.h"
#include "../../../../Math/LocalBoundingBox.h"

#include <algorithm>
#include <new>

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x21D7F0 (C2), 0x21DC00 (C1)
BBTakeCover::BBTakeCover(unsigned char index, unsigned char queue, PlanGenerator* pattern,
                         UrgeCurve* curve, float* modifiers)
    : BotBehaviour(modifiers, curve,
                   static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getMaxNBots())
{
    m_count = static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getMaxNBots();
    m_dangers = checkPointer(new DangerObject*[m_count]);
    m_index = index;
    m_queue = queue;
    m_pattern = pattern;
    m_targets = checkPointer(new unsigned int[m_count]);
    // shipped defect: the first danger-pointer array is overwritten and leaked.
    m_dangers = checkPointer(new DangerObject*[m_count]);
    for (int i = 0; i < m_count; ++i)
    {
        m_targets[i] = 0xFFFFFFFFu;
        m_urgency[i] = 0.0f;
        m_dangers[i] = 0;
    }
}

// ELF 0x21E010 (D2), 0x21E090 (D1), 0x21E110 (D0)
BBTakeCover::~BBTakeCover()
{
    // DangerObject has no destructor slot; the shipped bodies deallocate these trivial wrappers.
    for (int i = 0; i < m_count; ++i)
        ::operator delete(m_dangers[i]);
    if (m_targets)
        delete[] m_targets;
    if (m_dangers)
        delete[] m_dangers;
}

// ELF 0x220440
void BBTakeCover::reset(Bot* bot)
{
    BotBehaviour::reset(bot);
    if (m_dangers[bot->getBotId()])
        ::operator delete(m_dangers[bot->getBotId()]);
    m_dangers[bot->getBotId()] = 0;
}

// ELF 0x21E190
float BBTakeCover::calculateUrgency(Bot* bot, bool evaluate, float scale)
{
    if (!evaluate)
    {
        m_targetChanged[bot->getBotId()] = false;
        if (m_pattern->isInitiated(bot))
            return m_urgency[bot->getBotId()];
        m_urgency[bot->getBotId()] = 0.0f;
        return 0.0f;
    }

    // [INFERRED] local packaging of the repeated inlined pattern-clear block.
    const auto clearPattern = [&]()
    {
        if (m_pattern->isInitiated(bot))
        {
            m_targetChanged[bot->getBotId()] = true;
            m_pattern->unInit(bot);
        }
        // shipped defect: this overwrites the changed-target flag just set above.
        m_targetChanged[bot->getBotId()] = false;
        m_urgency[bot->getBotId()] = 0.0f;
    };

    const int side = bot->getSide();
    Information* self = informationManager->getInformation(bot->getCurrentEquipment());
    IPIPhysical* physical = self->getPhysical();
    IPIMobile* mobile = self->getMobile();
    const int equipmentType = self->getUnit()->getEquipmentType();
    const auto& sensed = bot->getSensedObjects();
    const auto* incoming = bot->getIncommingFire();
    if (sensed.empty() && incoming->empty())
    {
        clearPattern();
        return 0.0f;
    }

    float total = 0.0f;
    float sums[8] = {};
    float maxima[8] = {};
    DangerObject* dangers[8] = {};
    const Vec3 axisX = {1.0f, 0.0f, 0.0f};
    const Vec3 axisZ = {0.0f, 0.0f, 1.0f};
    const Vec3 diagonalA = {0.70710677f, 0.0f, -0.70710677f};
    const Vec3 diagonalB = {0.70710677f, 0.0f, 0.70710677f};
    const Vec3 position = self->getPosition();
    const int strengthType = physical->getStrengthType();
    const auto dot = [](const Vec3& a, const Vec3& b)
    {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    };
    const auto quadrants = [&](const Vec3& delta, int& first, int& second)
    {
        const float x = dot(axisX, delta);
        const float z = dot(axisZ, delta);
        first = !(z < 0.0f) ? int(!(x < 0.0f)) : 2 + int(x < 0.0f);
        const float a = dot(diagonalA, delta);
        const float b = dot(diagonalB, delta);
        second = !(b < 0.0f) ? 4 + int(!(a < 0.0f)) : 6 + int(a < 0.0f);
    };
    for (const auto& entry : *incoming)
    {
        FireObject* fire = entry.first;
        const Vec3& origin = fire->getOrigin();
        const Vec3 delta = {origin.x - position.x, origin.y - position.y, origin.z - position.z};
        int first, second;
        quadrants(delta, first, second);
        float strength = fire->getBattleStrength(strengthType);
        if (!fire->isDirect())
            strength *= 0.8f;
        sums[first] += strength;
        sums[second] += strength;
        total += strength;
        if (strength > maxima[first])
        {
            maxima[first] = strength;
            if (!dangers[first])
                dangers[first] = checkPointer(new FireDanger(fire));
            else
                static_cast<FireDanger*>(dangers[first])->m_fire = fire;
        }
        if (strength > maxima[second])
        {
            // shipped defect: the second quadrant's maximum is never updated for fire.
            if (!dangers[second])
                dangers[second] = checkPointer(new FireDanger(fire));
            else
                static_cast<FireDanger*>(dangers[second])->m_fire = fire;
        }
    }

    Information* ownSector = informationManager->getInformation(mobile->getSector());
    unsigned int matchedOrders = 0;
    const auto weightOrder = [&](unsigned int handle, float& rating)
    {
        if (bot->isOrderedBehaviour(m_index) && bot->isTakingOrders() &&
            matchedOrders < bot->getOrderList(m_index)->size())
        {
            const auto end = bot->getOrderList(m_index)->end();
            const auto begin = bot->getOrderList(m_index)->begin();
            const auto last = bot->getOrderList(m_index)->end();
            if (end != std::find(begin, last, handle))
            {
                ++matchedOrders;
                rating *= bot->getMemory()->m_float8;
            }
        }
    };
    for (const auto& entry : sensed)
    {
        Information* target = informationManager->getInformation(entry.first);
        if (!target || target->getSide() != 0 || !vehicleOccupied(entry.first, side) || !target->getUnit())
            continue;
        const Vec3& at = target->getPosition();
        const Vec3 delta = {at.x - position.x, at.y - position.y, at.z - position.z};
        int first, second;
        quadrants(delta, first, second);
        float distance = vehiclecontrol::distanceVec3(position, target->getPosition());
        distance = distance > 5.0f ? distance : 5.0f;
        IPIMobile* targetMobile = target->getMobile();
        if (targetMobile && ownSector)
        {
            Information* targetSector = informationManager->getInformation(targetMobile->getSector());
            if (targetSector && ownSector->getAIObject() == targetSector->getAIObject())
                distance *= 0.5f;
        }
        int iterator = target->getFirstSecondaryObjectIterator();
        unsigned int next = target->getSecondaryObject(iterator, side);
        Information* seat = target;
        float rating;
        do
        {
            // shipped defects: overwrite rather than accumulate, skip the first secondary seat,
            // and match the next seat's handle against orders while rating the current seat.
            rating = seat->getUnit()->getBattleStrengths()[strengthType] / distance;
            weightOrder(next, rating);
            next = target->getSecondaryObject(iterator, side);
            seat = informationManager->getInformation(next);
        } while (seat);
        sums[first] += rating;
        sums[second] += rating;
        total += rating;
        const auto setInformationDanger = [&](int quadrant)
        {
            if (rating > maxima[quadrant])
            {
                maxima[quadrant] = rating;
                if (dangers[quadrant] && dangers[quadrant]->isInformation())
                    static_cast<InfoDanger*>(dangers[quadrant])->m_information = entry.first;
                else
                {
                    ::operator delete(dangers[quadrant]);
                    dangers[quadrant] = checkPointer(new InfoDanger(entry.first));
                }
            }
        };
        setInformationDanger(first);
        setInformationDanger(second);
    }

    Vec3 coverPosition = {};
    DangerObject* previous = 0;
    if (total > 0.0f)
    {
        IAIPathfinding* pathfinding = IAIPathfinding::instance;
        previous = m_dangers[bot->getBotId()];
        m_dangers[bot->getBotId()] = 0;
        float bestQuadrant = 0.0f;
        for (int i = 0; i < 8; ++i)
        {
            if (sums[i] > bestQuadrant)
            {
                bestQuadrant = sums[i];
                ::operator delete(m_dangers[bot->getBotId()]);
                m_dangers[bot->getBotId()] = dangers[i];
            }
            else
                ::operator delete(dangers[i]);
        }
        if (!m_dangers[bot->getBotId()])
        {
            clearPattern();
            if (previous)
                ::operator delete(previous);
            m_urgency[bot->getBotId()] = 0.0f;
            return 0.0f;
        }
        Information* threatSector = 0;
        DangerObject* danger = m_dangers[bot->getBotId()];
        if (danger->isInformation())
        {
            Information* threat = informationManager->getInformation(static_cast<InfoDanger*>(danger)->m_information);
            if (threat && threat->getMobile())
                threatSector = informationManager->getInformation(threat->getMobile()->getSector());
        }
        Vec3 threatPosition = {};
        if (!m_dangers[bot->getBotId()]->getPosition(threatPosition))
        {
            clearPattern();
            if (previous)
                ::operator delete(previous);
            m_urgency[bot->getBotId()] = 0.0f;
            return 0.0f;
        }

        float multiplier = 1.0f;
        float bestCover = 0.0f;
        LocalBoundingBox* bounds = const_cast<LocalBoundingBox*>(physical->getLocalBoundingBox());
        const float sizeX = bounds->getSize().x;
        const float sizeZ = bounds->getSize().z;
        const float width = sizeZ > sizeX ? sizeZ : sizeX;
        const float height = bounds->getSize().y;
        std::vector<unsigned int> covers;
        static_cast<IAIInformationEnvironment*>(AISingletonServer::m_iaiInformationEnvironment)->getCover(*bot, covers);
        for (unsigned int handle : covers)
        {
            Information* cover = informationManager->getInformation(handle);
            if (!cover)
                continue;
            IPIPhysical* coverPhysical = cover->getPhysical();
            IPICover* plugin = cover->getCover();
            if (!coverPhysical || !plugin ||
                (threatSector && threatSector->getAIObject() == cover->getAIObject()))
                continue;
            const Vec3 at = cover->getPosition();
            Vec3 direction = {at.x - threatPosition.x, at.y - threatPosition.y, at.z - threatPosition.z};
            vehiclecontrol::normalizeVec3(direction);
            const Vec3 start = vehiclecontrol::alongFrom(at, direction, coverPhysical->getRadius() * 0.75f);
            const Vec3 end = vehiclecontrol::alongFrom(at, direction, coverPhysical->getRadius() * 3.0f);
            Vec3 valid = {};
            unsigned int polygon, island;
            if (!pathfinding->findClosestValidPolygon(polygon, island, valid, mobile->getVehicleNumber(),
                                                       start, end, 10, equipmentType))
                continue;
            LocalBoundingBox* coverBounds = const_cast<LocalBoundingBox*>(coverPhysical->getLocalBoundingBox());
            const float coverX = coverBounds->getSize().x;
            const float coverZ = coverBounds->getSize().z;
            const float coverWidth = coverZ > coverX ? coverZ : coverX;
            const float coverHeight = coverBounds->getSize().y;
            float widthFactor = (coverWidth / width) * 0.2f;
            float heightFactor = (coverHeight / height) * 0.5f;
            if (widthFactor < 0.0f) widthFactor = 0.0f;
            if (heightFactor < 0.0f) heightFactor = 0.0f;
            if (widthFactor > 1.0f) widthFactor = 1.0f;
            if (heightFactor > 1.0f) heightFactor = 1.0f;
            const float factor = widthFactor * 0.25f + heightFactor * 0.75f;
            float denominator = vehiclecontrol::distanceVec3(cover->getPosition(), position) * factor;
            if (plugin->getPortalCount())
                denominator *= 0.1f;
            float rating = plugin->getCoverValue() / denominator;
            weightOrder(handle, rating);
            if (rating > bestCover)
            {
                bestCover = rating;
                m_targets[bot->getBotId()] = handle;
                coverPosition = valid;
                multiplier *= 1.3f;
            }
            else if (rating > 0.0f)
                multiplier *= (rating / bestCover) * 0.3f + 1.0f;
        }
        total = bestCover > 0.0f ? total * multiplier : 0.0f;
    }

    if (bot->isOrderedBehaviour(m_index) && bot->isTakingOrders() && bot->getOrderList(m_index)->empty())
        total *= bot->getMemory()->m_float8;
    // The final x87 comparison takes the positive arm for unordered values as well.
    if (!(total <= 0.0f))
    {
        const unsigned int coverHandle = m_targets[bot->getBotId()];
        if (m_dangers[bot->getBotId()])
        {
            const unsigned int changedIndex = bot->getBotId();
            m_targetChanged[changedIndex] = previous != m_dangers[bot->getBotId()];
            m_urgency[bot->getBotId()] = 0.0f;
            if (previous)
                ::operator delete(previous);
            BBPTakeCover* pattern = static_cast<BBPTakeCover*>(m_pattern);
            DangerObject* danger = m_dangers[bot->getBotId()];
            Vec3 threatPosition = {};
            danger->getPosition(threatPosition);
            if (danger->isInformation())
                pattern->init(bot, static_cast<InfoDanger*>(danger)->m_information, coverPosition, coverHandle);
            else
                pattern->init(bot, threatPosition, coverPosition, coverHandle);
            float distance = vehiclecontrol::distanceVec3(threatPosition, self->getPosition());
            distance = (distance > 10.0f ? distance : 10.0f) * 0.1f;
            const unsigned int urgencyIndex = bot->getBotId();
            float* urgency = m_urgency;
            const float strengthCurve = DecleiningSlopeCurve::calculate(total);
            const float distanceCurve = SCurve::calculate(distance);
            const float result = (distanceCurve * strengthCurve - 0.19f) * 1.0f * scale;
            urgency[urgencyIndex] = result;
            return result;
        }
        bot->getBotId();
        m_targetChanged[bot->getBotId()] = false;
        m_pattern->unInit(bot);
        m_urgency[bot->getBotId()] = 0.0f;
        return 0.0f;
    }
    clearPattern();
    if (previous)
        ::operator delete(previous);
    m_urgency[bot->getBotId()] = 0.0f;
    return 0.0f;
}

} // namespace ai
} // namespace hfe
} // namespace dice
