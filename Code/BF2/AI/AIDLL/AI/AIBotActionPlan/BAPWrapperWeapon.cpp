// Weapon-value wrapper recovered from libaidll.so.
#include "BAPWrapperWeapon.h"
#include "../AIKnowledge/WeaponInfo.h"
#include "../../../../Core/Debug.h"

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x1FB050 (C1; C2 0x1FAEF0)
BAPWrapperWeapon::BAPWrapperWeapon(ValueId id, BAPVariable<int>* value,
    BAPVariable<WeaponInfo*>* weapon, bool repeat)
    : BAPStatement(UninitializedPatternId()), m_valueId(id), m_integer(value),
      m_weapon(weapon), m_repeat(repeat)
{
    if (m_valueId != 0)
        Debug(DebugTypeError, "AI/AIDLL/AI/AIBotActionPlan/BAPWrapperWeapon.cpp", 28, "")
            << "Unknown integer valueId";
}

// ELF 0x1FB310 (C1; C2 0x1FB1B0)
BAPWrapperWeapon::BAPWrapperWeapon(ValueId id, BAPVariable<float>* value,
    BAPVariable<WeaponInfo*>* weapon, bool repeat)
    : BAPStatement(UninitializedPatternId()), m_valueId(id), m_float(value),
      m_weapon(weapon), m_repeat(repeat)
{
    if (m_valueId != 1)
        Debug(DebugTypeError, "AI/AIDLL/AI/AIBotActionPlan/BAPWrapperWeapon.cpp", 50, "")
            << "Unknown float valueId";
}

// ELF 0x1FB610 (D1; D0 0x1FB650)
BAPWrapperWeapon::~BAPWrapperWeapon()
{
}

// ELF 0x1FB470
bool BAPWrapperWeapon::execute(Bot*, bool& success, bool& finished)
{
    if (m_valueId == 0)
        m_integer->m_value = m_weapon->m_value->getShotsInMag(0);
    else if (m_valueId == 1)
        m_float->m_value = m_weapon->m_value->getHeat();
    else
        Debug(DebugTypeError, "AI/AIDLL/AI/AIBotActionPlan/BAPWrapperWeapon.cpp", 68, "")
            << "Unknown valueId";
    success = true;
    finished = !m_repeat;
    return true;
}

// ELF 0x1FB5F0
BAPStatementType BAPWrapperWeapon::getId() const
{
    return BAPSTWeaponWrapper;
}

// ELF 0x1FB600
bool BAPWrapperWeapon::hasDefault()
{
    return true;
}

} // namespace ai
} // namespace hfe
} // namespace dice
