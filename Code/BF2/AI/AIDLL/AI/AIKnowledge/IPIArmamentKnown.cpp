// IPIArmamentKnown. Recovered from libaidll.so 1.5.3153 (ia-32), 0x2ECD50..0x2ED14E; vtable
// 0x4BEAE0 (17 slots: IPIArmament's 16 plus updateWeaponsInternal()).

#include "IPIArmamentKnown.h"

#include "WeaponInfo.h"
#include "../AIUtils/WeaponInfoManager.h"

namespace dice {
namespace hfe {
namespace ai {

// 0x2ECD50: marks the records stale, then builds them at once (both calls direct).
IPIArmamentKnown::IPIArmamentKnown(Information* information)
    : IPIArmament(information)
{
    m_activeWeapon = 0;                                                     // [CHOSEN]
    IPIArmamentKnown::updateWeapons();
    IPIArmamentKnown::updateWeaponsInternal();
}

// 0x2ECDF0: frees every weapon record (no -1 test, as in IPIArmament::updateWeaponsInternal),
// then the base releases the vector.
IPIArmamentKnown::~IPIArmamentKnown()
{
    for (std::vector<unsigned int>::iterator it = m_weapons.begin(); it != m_weapons.end(); ++it)
        WeaponInfoManager::m_instance->freeInformation(*it);
}

// 0x2ED0C0: rebuilds stale records, then lets every existing record refresh itself.
void IPIArmamentKnown::setTime(double /*time*/)
{
    if (m_weaponsStale)
        updateWeaponsInternal();                                            // +16
    for (unsigned int i = 0; i < m_weapons.size(); ++i)
    {
        if (m_weapons[i] == static_cast<unsigned int>(-1))
            continue;
        WeaponInfo* const weapon = WeaponInfoManager::m_instance->getInformation(m_weapons[i]);
        if (weapon)
            weapon->update();                                               // +2
    }
}

// 0x2ECFE0 (unchecked index).
unsigned int IPIArmamentKnown::getWeapon(int index)
{
    if (m_weaponsStale)
        updateWeaponsInternal();                                            // +16
    return m_weapons[index];
}

// 0x2ED010.
std::vector<unsigned int>& IPIArmamentKnown::getWeapons()
{
    if (m_weaponsStale)
        updateWeaponsInternal();                                            // +16
    return m_weapons;
}

// 0x2ED040.
int IPIArmamentKnown::getActiveWeapon()
{
    if (m_weaponsStale)
        updateWeaponsInternal();                                            // +16
    return m_activeWeapon;
}

// 0x2ED070.
void IPIArmamentKnown::updateWeapons()
{
    m_weaponsStale = true;
}

// 0x2ED080: the base rebuild with known (not real) weapon records.
void IPIArmamentKnown::updateWeaponsInternal()
{
    IPIArmament::updateWeaponsInternal(false);
    m_weaponsStale = false;
}

} // namespace ai
} // namespace hfe
} // namespace dice
