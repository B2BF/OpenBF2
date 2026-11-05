// Weapon selection helpers shared by the fire patterns.
// Recovered from libaidll.so (Linux 1.5.3153, ia-32, symbols); every body carries its ELF address.
// [INFERRED] file name (see WeaponChoice.h).

#include "WeaponChoice.h"

#include "../AIKnowledge/WeaponInfo.h"
#include "../AIUtils/WeaponInfoManager.h"

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x2A8030
int choseWeapon(const std::vector<unsigned int>& weapons, int current, float distance, int strengthType,
                bool noIndirect, bool type1, bool type3, bool type2)
{
    int best = -1;
    int bestStrength = 0;
    bool inRange = false;
    for (unsigned int i = 0; i < weapons.size(); ++i)
    {
        WeaponInfo* const weapon = WeaponInfoManager::m_instance->getInformation(weapons[i]);
        if (!weapon)
            continue;
        if ((weapon->getWeaponType() == 3) != type3)
            continue;
        if ((weapon->getWeaponType() == 1) != type1)
            continue;
        if ((weapon->getWeaponType() == 2) != type2)
            continue;
        if (noIndirect && weapon->getIndirect())
            continue;
        if (weapon->getAmmo() == 0 && !type2 && !type1 && !type3)
            continue;
        if (weapon->getStrength(strengthType) <= 0)
            continue;
        if (weapon->getMaxRange() > distance && weapon->getMinRange() < distance)
        {
            if (inRange && weapon->getStrength(strengthType) < bestStrength)
                continue;
            inRange = true;
            best = i;
            bestStrength = weapon->getStrength(strengthType);
            if (best == current)
                bestStrength *= 3;
        }
        else if (!inRange && weapon->getStrength(strengthType) > bestStrength)
        {
            best = i;
            bestStrength = weapon->getStrength(strengthType);
            if (best == current)
                bestStrength *= 3;
        }
    }
    return best;
}

// ELF 0x2A8220
int choseWeaponDistanceIndependent(int current, int strengthType, const std::vector<unsigned int>& weapons,
                                   unsigned int allowed)
{
    int best = -1;
    int bestStrength = 0;
    for (int i = 0; i < static_cast<int>(weapons.size()); ++i)
    {
        WeaponInfo* const weapon = WeaponInfoManager::m_instance->getInformation(weapons[i]);
        if (!weapon)
            continue;
        if (!(allowed & 4) && weapon->getWeaponType() == 1)
            continue;
        if (!(allowed & 2) && weapon->getIndirect())
            continue;
        if (!(allowed & 1) && !weapon->getIndirect())
            continue;
        if (weapon->getAmmo() == 0 || weapon->getStrength(strengthType) <= 0)
            continue;
        int strength = weapon->getStrength(strengthType);
        if (current == i)
            strength *= 3;
        if (strength > bestStrength)
        {
            bestStrength = strength;
            best = i;
        }
    }
    return best;
}

// ELF 0x2A8330
int soldier_choseThrownWeapon(const std::vector<unsigned int>& weapons, int, float distance, int strengthType)
{
    for (unsigned int i = 0; i < weapons.size(); ++i)
    {
        WeaponInfo* const weapon = WeaponInfoManager::m_instance->getInformation(weapons[i]);
        if (weapon && weapon->getIsThrown() && weapon->getAmmo() != 0 && weapon->getStrength(strengthType) > 0
            && !(weapon->getMinRange() > distance))
            return i;
    }
    return -1;
}

} // namespace ai
} // namespace hfe
} // namespace dice
