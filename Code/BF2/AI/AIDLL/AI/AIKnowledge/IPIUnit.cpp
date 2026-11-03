// [INFERRED] path: paired with IPIUnit.h in the attested AIKnowledge directory.
// Recovered from libaidll.so and AIDLL_w32ded.dll
// Retail deliberately omits null and bounds guards in these paths.
#include "IPIUnit.h"

// <iostream>'s ios_base::Init object is this TU's static initialisation and teardown (ELF
// 0x2F1CC0, __tcf_0 0x2F1D20, _GLOBAL__I 0x2F1D50).
#include <iostream>

#include "Information.h"
#include "IPIArmament.h"
#include "IPIPhysical.h"
#include "WeaponInfo.h"
#include "WeaponInfoManager.h"
#include "../AIBotBehaviour/CustomCurve.h"                          // SCurve::calculate (static)
#include "../AIUtils/checkPointer.h"
#include "../Main/AISettingsInterfaces.h"
#include "../Main/CommonSingletonServer.h"
#include "../../../AIInterface/BFKnowledge/BFTimer.h"
#include "../../../AIInterface/BFKnowledge/IAIObjectPlugIn.h"      // IAIObject / IAIObjectUnit

#include <new>

namespace dice { namespace hfe { namespace ai {

IPIUnit::IPIUnit(Information* information)
{
    m_information = information;

    // Shipped order: this read precedes initialization of both cache fields.
    m_ammo = getAmmo();
    m_strengthMod = 1.0f;
    m_battleStrengths = checkPointer(new float[getNBattleStrengths()]);
    m_strategicStrengths = checkPointer(new float[getNStrategicStrengths()]);
    setTime();
    m_ownsPointer24 = false;
    // m_pointer24 is intentionally untouched.
}

IPIUnit::~IPIUnit()
{
    delete[] m_battleStrengths;
    delete[] m_strategicStrengths;
    if (m_ownsPointer24)
        ::operator delete(m_pointer24);
}

int IPIUnit::getType()
{
    return 3;
}

void IPIUnit::setTime()
{
    setTime(static_cast<const IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime());
}

void IPIUnit::setTime(double time)
{
    IPIPhysical* const physical = m_information->getPhysical();
    getAmmo(time);
    calculateStrMod(physical->getHealth());
    calculateStrengths();
}

void IPIUnit::calculateStrengths()
{
    IAIObjectUnit* const unit = m_information->getAIObject()->getUnit();

    const int battleCount = getNBattleStrengths();
    const int* const battleStrengths = unit->getBattleStrengths();
    if (m_strengthMod < 0.0f)
        m_strengthMod = 0.0f;
    else if (m_strengthMod > 1.0f)
        m_strengthMod = 1.0f;

    for (int i = 0; i < battleCount; ++i)
        m_battleStrengths[i] = static_cast<float>(battleStrengths[i]) * m_strengthMod;

    const int strategicCount = getNStrategicStrengths();
    const int* const strategicStrengths = unit->getStrategicStrengths();
    for (int i = 0; i < strategicCount; ++i)
        m_strategicStrengths[i] = static_cast<float>(strategicStrengths[i]) * m_strengthMod;
}

float IPIUnit::getAmmo()
{
    return getAmmo(static_cast<const IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime());
}

float IPIUnit::getAmmo(double time)
{
    if (m_ammoTime >= time)
        return m_ammo;

    m_ammoTime = time;
    IPIArmament* const armament = m_information->getArmament();
    if (!armament)
        return 1.0f;

    float total = 0.0f;
    WeaponInfoManager* const manager = WeaponInfoManager::m_instance;
    for (std::vector<unsigned int>::iterator it = armament->getWeapons().begin();
         it != armament->getWeapons().end(); ++it)
    {
        if (*it == static_cast<unsigned int>(-1))
            continue;

        WeaponInfo* const weapon = manager->getInformation(*it);
        if (!weapon)
            continue;

        const float ammo = static_cast<float>(weapon->getAmmo());
        if (ammo < 0.0f)
            return 1.0f;
        total += ammo;
    }

    const int maximum = m_information->getAIObject()->getUnit()->getMaxAmmo();
    m_ammo = maximum > 0 ? total / static_cast<float>(maximum) : 1.0f;
    return m_ammo;
}

// The PE compares the unrounded double result before storing the rounded float. The ELF rounds
// first; this reconstruction follows the dedicated PE [PLATFORM].
bool IPIUnit::calculateStrMod(float health)
{
    const float fromAmmo =
        static_cast<float>(static_cast<double>(SCurve::calculate(m_ammo)) * 0.5 + 0.25);
    const double modifier = static_cast<double>(SCurve::calculate(health)) * 0.25 + fromAmmo;
    const bool changed = modifier != static_cast<double>(m_strengthMod);
    m_strengthMod = static_cast<float>(modifier);
    return changed;
}

unsigned int IPIUnit::getOccupier(int side)
{
    IAIObject* const object = m_information->getAIObject()->getUnit()->getOccupier();
    return object ? object->getInformation(side) : static_cast<unsigned int>(-1);
}

unsigned int IPIUnit::getOccupiedObject(int side)
{
    IAIObject* const object = m_information->getAIObject()->getUnit()->getOccupiedObject();
    return object ? object->getInformation(side) : static_cast<unsigned int>(-1);
}

int IPIUnit::getOccupyingPlayerId(int /*side*/)
{
    return m_information->getAIObject()->getUnit()->getOccupyingPlayerId();
}

int IPIUnit::getNBattleStrengths()
{
    return static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getNBattleStrengths();
}

int IPIUnit::getNStrategicStrengths()
{
    return static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getNStrategicStrengths();
}

int IPIUnit::getEquipmentType()
{
    return m_information->getAIObject()->getUnit()->getEquipmentType();
}

bool IPIUnit::isOccupied()
{
    return m_information->getAIObject()->getUnit()->isOccupied();
}

bool IPIUnit::useNoPathfindingToGetToObject() const
{
    return m_information->getAIObject()->getUnit()->useNoPathfindingToGetToObject();
}

bool IPIUnit::hasExposedSoldier() const
{
    return m_information->getAIObject()->getUnit()->hasExposedSoldier();
}

bool IPIUnit::getAlwaysTargetSoldier() const
{
    return m_information->getAIObject()->getUnit()->getAlwaysTargetSoldier();
}

float IPIUnit::getCullDistance() const
{
    return m_information->getAIObject()->getUnit()->getCullDistance();
}

float IPIUnit::getCullDistanceSqr() const
{
    return m_information->getAIObject()->getUnit()->getCullDistanceSqr();
}

float IPIUnit::getEntryRadius()
{
    return m_information->getAIObject()->getUnit()->getEntryRadius();
}

io::PlayerInputMap IPIUnit::getSelectKey()
{
    return m_information->getAIObject()->getUnit()->getSelectKey();
}

double IPIUnit::getLastPlayerPossesionTime() const
{
    return m_information->getAIObject()->getUnit()->getLastPlayerPossesionTime();
}

int IPIUnit::getTeamLock() const
{
    return m_information->getAIObject()->getUnit()->getTeamLock();
}

bool IPIUnit::isTeamLocked(int side) const
{
    const int lock = m_information->getAIObject()->getUnit()->getTeamLock();
    return lock == 0 || lock == side;
}

int IPIUnit::getMissileLockStatus() const
{
    return m_information->getAIObject()->getUnit()->getMissileLockStatus();
}

void IPIUnit::setMissileLockStatus(int status)
{
    m_information->getAIObject()->getUnit()->setMissileLockStatus(status);
}

bool IPIUnit::mustBorrowArmament() const
{
    return m_information->getAIObject()->getUnit()->mustBorrowArmament(
        m_information->getAIObject());
}

const Vec3& IPIUnit::getEntryPositionOfNonMobile() const
{
    return m_information->getAIObject()->getUnit()->getEntryPositionOfNonMobile();
}

} } }
