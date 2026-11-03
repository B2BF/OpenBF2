// Condition: the object's ammunition, as the product of the fill fractions of its weapons, is at
// most a threshold. Recovered from libaidll.so (Linux 1.5.3153, ia-32, symbols); every body
// carries its ELF address.

#include "BAPConTotalAmmo.h"

#include "BAPActionInline.h"
#include "../AIKnowledge/IPIArmament.h"
#include "../AIKnowledge/Information.h"
#include "../AIKnowledge/WeaponInfo.h"
#include "../AIKnowledge/WeaponInfoManager.h"

#include <vector>

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x1F1C30 (C1; C2 0x1F1BF0): +0x04 = 0 by the base, +0x08 = the object, +0x0C = threshold.
BAPConTotalAmmo::BAPConTotalAmmo(unsigned int object, float threshold)
{
    m_object = object;
    m_threshold = threshold;
}

// ELF 0x1F1E50 (D1; D2 0x1F1E30, D0 0x1F1E70): base only.
BAPConTotalAmmo::~BAPConTotalAmmo()
{
}

// ELF 0x1F1C70: false without information. Otherwise the object's weapon list is copied and every
// weapon that exists, is neither of type 1 nor 2, whose WeaponInfo slot +0x10 (getAmmo) is not -1
// and whose maximum ammunition is not 0 multiplies the total (1 at first) by
// getCurrentAmmo / getMaxAmmo. True when the total is at most the threshold.
bool BAPConTotalAmmo::evaluate(Bot*)
{
    float total = 1.0f;
    std::vector<unsigned int> weapons;
    Information* info = bapinline::info(m_object);
    if (!info)
        return false;
    weapons = info->getArmament()->getWeapons();
    for (unsigned int i = 0; i < weapons.size(); ++i)
    {
        WeaponInfo* weapon = WeaponInfoManager::m_instance->getInformation(weapons[i]);
        if (!weapon || weapon->getWeaponType() == 1 || weapon->getWeaponType() == 2 || weapon->getAmmo() == -1
            || weapon->getMaxAmmo() == 0)
            continue;
        const float current = static_cast<float>(weapon->getCurrentAmmo());
        total *= current / static_cast<float>(weapon->getMaxAmmo());
    }
    return m_threshold >= total;
}

// ELF 0x1F1EB0
BAPConditionType BAPConTotalAmmo::getConditionType() const
{
    return BAPCTTotalAmmo;
}

} // namespace ai
} // namespace hfe
} // namespace dice
