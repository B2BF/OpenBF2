// Calls for a nearby medic according to travel time and remaining man-down time.

// static init: ELF 0x1FFEA0 (__tcf_0 0x1FFF00, _GLOBAL__I 0x1FFF30): <iostream>'s Init object only.
#include <iostream>

#include "BBCallForMedic.h"

#include "../AIUtils/CustomCurve.h"
#include "../AIBot/BotMain.h"
#include "../AIBotBehaviourPattern/BBPCallForMedic.h"
#include "../AIKnowledge/AIInformationGrid.h"
#include "../AIKnowledge/BFInformationEnvironment.h"
#include "../AIKnowledge/Information.h"
#include "../Main/InformationManager.h"
#include "../AIKnowledge/IPIArmament.h"
#include "../AIKnowledge/IPIMobile.h"
#include "../AIKnowledge/IPIPhysical.h"
#include "../AIKnowledge/WeaponInfo.h"
#include "../AIUtils/WeaponInfoManager.h"
#include "../Main/AISettingsInterfaces.h"
#include "../Main/CommonSingletonServer.h"
#include "../../../../Math/Vec3.h"

#include <cmath>
#include <vector>

namespace dice {
namespace hfe {
namespace ai {

// Same-side objects carrying a weapon of type 1 or 3. No virtual destructor in the shipped table.
// [INFERRED] Source placement with its only caller; the weak body follows this TU's initializer.
class MedicatePredicate : public IAIInformationGridPredicate
{
public:
    explicit MedicatePredicate(int side) : m_side(side) {}

    // ELF 0x1FFF50
    virtual bool includeInformation(Information* information) const
    {
        IPIArmament* armament = information->getArmament();
        if (!armament)
            return false;
        const std::vector<unsigned int>& weapons = armament->getWeapons();
        bool found = false;
        for (unsigned int i = 0; i < weapons.size(); ++i)
        {
            WeaponInfo* weapon = WeaponInfoManager::m_instance->getInformation(weapons[i]);
            if (weapon)
            {
                const int type = weapon->getWeaponType();
                found = false;
                if (type == 3 || type == 1)
                {
                    found = true;
                    break;
                }
            }
        }
        return information->getSide() == m_side && found;
    }

private:
    /* +0x04 */ int m_side;
};

// ELF 0x1FF810 (C2), 0x1FF8E0 (C1)
BBCallForMedic::BBCallForMedic(unsigned char index, unsigned char queue, BBPCallForMedic* pattern,
                             UrgeCurve* curve, float* modifiers)
    : BotBehaviour(modifiers, curve,
                   static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getMaxNBots())
{
    m_index = index;
    m_queue = queue;
    m_pattern = pattern;
    const int count = static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getMaxNBots();
    m_targets = new unsigned int[count];
    for (int i = 0; i < count; ++i)
    {
        m_urgency[i] = 0.0f;
        m_targetChanged[i] = false;
        m_targets[i] = 0xFFFFFFFFu;
    }
}

// ELF 0x1FF9B0 (D2), 0x1FFA00 (D1), 0x1FFA50 (D0)
BBCallForMedic::~BBCallForMedic()
{
    if (m_targets)
        delete[] m_targets;
}

// ELF 0x1FFAA0
float BBCallForMedic::calculateUrgency(Bot* bot, bool evaluate, float)
{
    if (!evaluate)
    {
        if (m_pattern->isInitiated(bot) &&
            informationManager->getInformation(m_targets[bot->getBotId()]))
            return m_urgency[bot->getBotId()];
        m_pattern->unInit(bot);
        m_urgency[bot->getBotId()] = 0.0f;
        return 0.0f;
    }
    std::vector<unsigned int> medics;
    Information* equipment = informationManager->getInformation(bot->getCurrentEquipment());
    const Vec3& position = equipment->getPosition();
    const int side = bot->getSide();
    MedicatePredicate predicate(side);
    static_cast<IAIInformationEnvironment*>(AISingletonServer::m_iaiInformationEnvironment)
        ->getObjectsWithinRadius(medics, position, 50.0f, side, true, &predicate);
    float nearestDistance = 50.0f;
    m_targets[bot->getBotId()] = 0xFFFFFFFFu;
    for (std::vector<unsigned int>::const_iterator i = medics.begin(); i != medics.end(); ++i)
    {
        Information* medic = informationManager->getInformation(*i);
        const Vec3& medicPosition = medic->getPosition();
        const Vec3 delta = {position.x - medicPosition.x, position.y - medicPosition.y,
                            position.z - medicPosition.z};
        const float distance = std::sqrt(delta.x * delta.x + delta.y * delta.y + delta.z * delta.z);
        if (distance < nearestDistance)
        {
            nearestDistance = distance;
            m_targets[bot->getBotId()] = *i;
        }
    }
    static_cast<BBPCallForMedic*>(m_pattern)->init(bot);
    Information* medic = informationManager->getInformation(m_targets[bot->getBotId()]);
    IPIPhysical* physical = equipment->getPhysical();
    if (medic && physical->getManDownTimeLeft() > 0.0f)
    {
        const float speed = medic->getMobile()->getMaxSpeed();
        const float travelTime = nearestDistance / speed;
        float ratio = travelTime / physical->getManDownTimeLeft();
        if (ratio > 1.0f)
            ratio = 1.0f;
        const unsigned int index = bot->getBotId();
        m_urgency[index] = 1.0f - SCurve::calculate(ratio);
    }
    else
        m_urgency[bot->getBotId()] = 0.0f;
    return m_urgency[bot->getBotId()];
}

} // namespace ai
} // namespace hfe
} // namespace dice
