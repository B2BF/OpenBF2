
#include "IPIArmament.h"

// <iostream>'s ios_base::Init object is this TU's static initialisation and teardown (ELF
// 0x2EC6C0, __tcf_0 0x2EC720, _GLOBAL__I 0x2EC750).
#include <iostream>

#include "Information.h"
#include "InformationFactory.h"
#include "WeaponInfo.h"
#include "WeaponInfoManager.h"
#include "../Main/CommonSingletonServer.h"
#include "../../../AIInterface/BFKnowledge/BFTimer.h"
#include "../../../AIInterface/BFKnowledge/IAIObjectPlugIn.h"
#include "../../../AIInterface/BFKnowledge/IWeapon.h"

namespace dice {
namespace hfe {
namespace ai {

const unsigned int invalidWeaponInfoHandle = static_cast<unsigned int>(-1);

IPIArmament::IPIArmament(Information* information)
    : m_weapons()
{
    m_information = information;
    // m_unresolvedFinalDword is intentionally left untouched, as in both retail images.
}

IPIArmament::~IPIArmament()
{
}

int IPIArmament::getType()
{
    return 4;
}

void IPIArmament::setTime()
{
    setTime(static_cast<const IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime());
}

void IPIArmament::setTime(double /*time*/)
{
}

int IPIArmament::getActiveWeapon() const
{
    return m_information->getAIObject()->getArmament()->getActiveWeapon();
}

void IPIArmament::setActiveWeapon(int weapon, int side)
{
    m_information->getAIObject()->getArmament()->setActiveWeapon(weapon, side);
}

int IPIArmament::getWeaponIdFromActivationId(int activation, int fire) const
{
    return m_information->getAIObject()->getArmament()->getWeaponIdFromActivationId(activation,
                                                                                     fire);
}

WeaponInfo* IPIArmament::getLastFiredWeapon()
{
    IAIObjectArmament* const armament = m_information->getAIObject()->getArmament();
    IWeapon* const weapon = armament->getLastFiredWeapon();
    if (!weapon)
        return 0;

    // Retail reads fire before activation, then passes activation first to the mapping function.
    const int fire = weapon->getWeaponFire();
    const int activation = weapon->getWeaponActivate();
    const int index = armament->getWeaponIdFromActivationId(activation, fire);
    return WeaponInfoManager::m_instance->getInformation(m_weapons[index]);
}

int IPIArmament::isAntiAircraft() const
{
    return m_information->getAIObject()->getArmament()->isAntiAircraft();
}

void IPIArmament::rearm(WeaponInfo* weapon, float amount, int side)
{
    weapon->m_rearmAmount += amount;
    if (m_information->getAIObject()->getArmament()->rearm(weapon->m_weapon,
                                                           weapon->m_rearmAmount, side))
        weapon->m_rearmAmount = 0.0f;
}

bool IPIArmament::getProjectilePosition(Vec3& out) const
{
    return m_information->getAIObject()->getArmament()->getProjectilePosition(out);
}

bool IPIArmament::getProjectileSpeed(Vec3& out) const
{
    return m_information->getAIObject()->getArmament()->getProjectileSpeed(out);
}

void IPIArmament::updateWeaponInternal(int index, bool real)
{
    const std::vector<IWeapon*>& engineWeapons =
        m_information->getAIObject()->getArmament()->getWeapons();
    if (engineWeapons.size() != m_weapons.size())
    {
        updateWeaponsInternal(real);
        return;
    }

    WeaponInfoManager::m_instance->freeInformation(m_weapons[index]);
    m_weapons[index] = invalidWeaponInfoHandle;

    IWeapon* const weapon = engineWeapons[index];
    if (weapon)
    {
        const unsigned int handle =
            static_cast<IInformationFactory*>(CommonSingletonServer::m_iInformationFactory)
                ->requestWeaponInfo(m_weapons[index], weapon, real);
        m_weapons[index] = handle;
    }
}

void IPIArmament::updateWeaponsInternal(bool real)
{
    for (unsigned int i = 0; i < m_weapons.size(); ++i)
    {
        WeaponInfoManager::m_instance->freeInformation(m_weapons[i]);
        m_weapons[i] = invalidWeaponInfoHandle;
    }

    const std::vector<IWeapon*>& engineWeapons =
        m_information->getAIObject()->getArmament()->getWeapons();
    m_weapons.resize(engineWeapons.size(), invalidWeaponInfoHandle);

    for (unsigned int i = 0; i < engineWeapons.size(); ++i)
    {
        IWeapon* const weapon = engineWeapons[i];
        if (weapon)
        {
            const unsigned int handle =
                static_cast<IInformationFactory*>(CommonSingletonServer::m_iInformationFactory)
                    ->requestWeaponInfo(m_weapons[i], weapon, real);
            m_weapons[i] = handle;
        }
    }
}

} // namespace ai
} // namespace hfe
} // namespace dice
