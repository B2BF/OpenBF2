// Run-over behaviour (AI/AIDLL/AI/AIBotBehaviour/BBRunOverSoldier.cpp in the image). Recovered from
// libaidll.so (Linux 1.5.3153, ia-32, symbols); every body carries its ELF address.

// static init: ELF 0x21D3B0 (__tcf_0 0x21D410, _GLOBAL__I 0x21D440): <iostream>'s Init object only.
#include <iostream>

#include "BBRunOverSoldier.h"

#include "../Main/AIDllFrontier.h"
#include "../Main/CommonSingletonServer.h"
#include "../Main/AISettingsInterfaces.h"
#include "../AIUtils/checkPointer.h"
#include "../../../../Math/Mat4.h"
#include "../../../../Math/Vec3.h"
#include "../AIBot/BotMain.h"
#include "../AIBot/SensedObject.h"
#include "../AIBotBehaviourPattern/BBPRunOverSoldier.h"
#include "../AIKnowledge/Information.h"
#include "../AIKnowledge/InformationManager.h"
#include "../AIKnowledge/IPIMobile.h"
#include "../AIKnowledge/IPIPhysical.h"
#include "../AIKnowledge/IPIUnit.h"
#include "../Main/IAIPathfinding.h"
#include "../../../AIInterface/BFKnowledge/AIPlayer.h"

#include <cmath>
#include <map>
#include <string>

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x21C950 (C2) / 0x21CB40 (C1); vtable GOT -0xA20. The base gets getMaxNBots; a second
// getMaxNBots sizes the +0x08 target array (not initialised) through the inlined checkPointer
// (Debug type 6, line 46); every bot starts with urgency 0.0f.
BBRunOverSoldier::BBRunOverSoldier(unsigned char index, unsigned char queue, PlanGenerator* pattern,
                                   UrgeCurve* curve, float* modifiers)
    : BotBehaviour(modifiers, curve, static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getMaxNBots())
{
    m_index = index;
    m_queue = queue;
    m_pattern = pattern;
    const int count = static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getMaxNBots();
    m_targets = checkPointer(new unsigned int[count]);
    for (int i = 0; i < count; ++i)
        m_urgency[i] = 0.0f;
}

// ELF 0x21CD30 (D2) / 0x21CD80 (D1) / 0x21CDD0 (D0): delete[] +0x08 when set, then ~BotBehaviour.
BBRunOverSoldier::~BBRunOverSoldier()
{
    if (m_targets)
        delete[] m_targets;
}

// ELF 0x21CE20 (Ghidra; listing for the trace arguments and the constants). A bot whose vehicle
// moves at least minSpeed runs over the first living enemy soldier it sees directly that lies
// ahead (direction against the transformation's third row at least minDot) within maxDistance
// (distance floored at 1) and that the pathfinding trace from the vehicle's last valid
// position reaches: urgency 65, target in +0x08. The pattern is initialised with the stored
// target even when none was found this time.
float BBRunOverSoldier::calculateUrgency(Bot* bot, bool recalculate, float)
{
    static float minSpeed = 10.0f;      // .data
    static float minDot = 0.85f;        // .data
    static float maxDistance = 50.0f;   // .data

    if (!recalculate)
    {
        m_urgency[bot->getBotId()] = 0.0f;
        return m_urgency[bot->getBotId()];
    }

    Information* const equipment = informationManager->getInformation(bot->getCurrentEquipment());
    m_urgency[bot->getBotId()] = 0.0f;
    const int equipmentType = equipment->getUnit()->getEquipmentType();
    IPIMobile* const mobile = equipment->getMobile();
    if (!(minSpeed <= mobile->getSpeedMagnitude()))
    {
        m_urgency[bot->getBotId()] = 0.0f;
        return m_urgency[bot->getBotId()];
    }

    const Vec3& position = equipment->getPosition();
    const std::map<unsigned int, SensedObject*>& sensed = bot->getSensedObjects();
    for (std::map<unsigned int, SensedObject*>::const_iterator it = sensed.begin(); it != sensed.end(); ++it)
    {
        const unsigned int flags = it->second->m_flags;
        if (!(flags & SensedObject::SENSE_SEEN) || !(flags & SensedObject::SENSE_SIDED)
            || (flags & SensedObject::SENSE_INDIRECT))
            continue;
        Information* const info = informationManager->getInformation(it->second->m_handle);
        if (!info || !(info->getObjectTypeFlags() & 0x200000))
            continue;
        if (info->getSide() == bot->getSide() || info->getSide() == 0)
            continue;
        IPIPhysical* const physical = info->getPhysical();
        IPIMobile* const targetMobile = info->getMobile();
        if (!physical || !(physical->getHealth() > 0.0f) || !targetMobile)
            continue;
        const Vec3& targetPosition = info->getPosition();
        Vec3 direction;
        direction.x = targetPosition.x - position.x;
        direction.y = targetPosition.y - position.y;
        direction.z = targetPosition.z - position.z;
        const float length = std::sqrt(direction.x * direction.x + direction.y * direction.y
                                       + direction.z * direction.z);
        if (!(length > 0.0001f))
            continue;
        const float inverse = 1.0f / length;
        direction.x = direction.x * inverse;
        float distance = 1.0f;
        if (1.0f <= length)
            distance = length;
        direction.y = direction.y * inverse;
        direction.z = inverse * direction.z;
        const Mat4& transformation = equipment->getTransformation();
        if (minDot > transformation.m[10] * direction.z + transformation.m[8] * direction.x
                         + transformation.m[9] * direction.y)
            continue;
        if (distance > maxDistance)
            continue;
        const IPIMobile::ValidPos& targetValid = targetMobile->getLastValidPosition(mobile->getVehicleNumber());
        if (targetValid.quadTree == static_cast<unsigned int>(-1) || targetValid.polygon == static_cast<unsigned int>(-1))
            continue;
        const IPIMobile::ValidPos& valid = mobile->getLastValidPosition();
        IAIPathfinding* const pathfinding = IAIPathfinding::instance;
        Vec3 end;
        end.x = 0.0f;
        end.y = 0.0f;
        end.z = 0.0f;
        unsigned int endPolygon;
        if (pathfinding->trace(valid.polygon, valid.quadTree, mobile->getVehicleNumber(), valid.position,
                               targetValid.position, end, endPolygon, equipmentType, -1, 0.0f, 5, 0))
        {
            m_targets[bot->getBotId()] = it->first;
            m_urgency[bot->getBotId()] = 65.0f;
            break;
        }
    }
    static_cast<BBPRunOverSoldier*>(m_pattern)->init(bot, m_targets[bot->getBotId()]);
    return m_urgency[bot->getBotId()];
}

// ELF 0x21D360 (Ghidra). Applicable while the bot's player is alive and not man down.
bool BBRunOverSoldier::isApplicable(Information*, Bot* bot)
{
    bool applicable = false;
    if (bot->getPlayer()->getIsAlive())
        applicable = !bot->getManDown();
    return applicable;
}

} // namespace ai
} // namespace hfe
} // namespace dice
