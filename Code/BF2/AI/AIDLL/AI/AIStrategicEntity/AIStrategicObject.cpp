// [INFERRED] Source placement.

#include "AIStrategicObject.h"

// <iostream>'s ios_base::Init object is this TU's static initialisation and teardown (ELF
// 0x342FE0, __tcf_0 0x343040, _GLOBAL__I 0x343070).
#include <iostream>

#include <cmath>

#include "../AIStrategic/SAI.h"          // EnemyStatistics - the four objects the constructor makes
#include "../Main/AIDllFrontier.h"
#include "../Main/AIMain.h"
#include "../Main/AIRandom.h"
#include "../Main/AISettingsInterfaces.h"            // IAISettings::getVehicleGroup
#include "../Main/CommonSingletonServer.h"
#include "../Main/IBotManager.h"
#include "../AIBot/BotMain.h"                       // Bot::getCurrentEquipment
#include "../AIKnowledge/Information.h"
#include "../Main/InformationManager.h"

namespace dice {
namespace hfe {
namespace ai {

// Initializes strategic-object state and its owned enemy-statistics records.

// ELF 0x340C60 (C2; C1 0x341190).
AIStrategicObject::AIStrategicObject(float radius, float f148)
    : m_position()      // the two-iteration zeroing loop at +132, in member-declaration position
{
    m_statistics[0] = new EnemyStatistics(6);
    m_statistics[1] = new EnemyStatistics(6);
    m_statistics[2] = new EnemyStatistics(2);
    m_statistics[3] = new EnemyStatistics(2);

    const std::set<int> empty;
    m_availableAttackers[0] = empty;
    m_availableAttackers[1] = empty;
    m_availableDefenders[0] = empty;
    m_availableDefenders[1] = empty;
    m_set68[0] = empty;
    m_set68[1] = empty;
    m_presentUnits[0] = empty;
    m_presentUnits[1] = empty;

    m_status[0] = StatusMap::Neutral;
    m_status[1] = StatusMap::Neutral;
    m_status[2] = StatusMap::Neutral;
    m_status[3] = StatusMap::Neutral;

    m_f148 = f148;

    m_size[0] = radius;
    m_size[1] = 0.0f;
    m_size[2] = 0.0f;

    m_f164[0] = radius;
    m_f164[1] = radius;
    m_f164[2] = radius;
    m_f164[3] = radius;

    for (int i = 0; i < 2; ++i)
    {
        m_needCurveInput[i] = 0.0f;
        m_friendlyRatio[i] = 0.0f;
        m_friendlyBattleStrength[i] = 0.0f;
        m_enemyBattleStrength[i] = 0.0f;
        m_unresolved212[i] = 0;
        m_resourceNeed[i] = 0;
        m_assignedSquadStrength[i] = 0;
    }

    m_attack[0] = true;
    m_attack[1] = true;

    // `m_index` (+240) is deliberately absent. The constructor does not touch it; see the header.
    m_i244    = -1;
    m_i248    = 0;
    m_category = -1;
}

// ELF 0x341600 (D2; D1 0x3418F0, D0 0x341BE0): deletes the four EnemyStatistics (+4..+0x10, each
// tested for null); the sets (+0x100 down to +0x14) go as members.
AIStrategicObject::~AIStrategicObject()
{
    for (int i = 0; i < 4; ++i)
    {
        if (m_statistics[i])
            delete m_statistics[i];
    }
}

// native set size, side is one-based.
// ELF getAssignedUnits(int) at returns the matching set; the two old STL layouts
// place their counts four bytes apart. SAI::updateStates tests the signed result against zero.
// ELF 0x3423A0.
int AIStrategicObject::getNumberOfAssignedUnits(int side) const
{
    return static_cast<int>(m_set68[side - 1].size());
}

// byte at this + 0xEB + side, no bounds guard.
// ELF 0x341FD0.
bool AIStrategicObject::isAttack(int side) const
{
    return m_attack[side - 1];
}

// load this + 4*side. The first
// statistics pointer is at +4, so the side is one-based. The caller passes this
// borrowed address to EnemyStatistics::add(EnemyStatistics&); pointer versus
// reference return spelling is [INFERRED]. No null or side guard is present.
// ELF 0x3425E0.
EnemyStatistics& AIStrategicObject::getBattleStatistics(int side) const
{
    return *m_statistics[side - 1];
}

// Returns the signed count of released resources, or zero when a commander refuses.
// Re-queries the SAI after the commander callback and reads the object index afterward.
// The downstream SAI release operation remains a named frontier.
// ELF 0x342090.
int AIStrategicObject::releaseAssignedResources(int side)
{
    if (IAIMain::m_instance->getSAI(side)->isPlayerCommander())
        return 0;

    ISAI* const sai = IAIMain::m_instance->getSAI(side);
    return sai->releaseSquadsFromObject(m_index, side);
}

// ELF 0x3422A0.
std::set<int>& AIStrategicObject::getAvailableBots(bool attackers, int side)
{
    if (attackers)
        return m_availableAttackers[side - 1];
    return m_availableDefenders[side - 1];
}

// ELF 0x3422D0.
std::set<int>& AIStrategicObject::getAvailableAttackers(int side)
{
    return m_availableAttackers[side - 1];
}

// ELF 0x3422F0.
std::set<int>& AIStrategicObject::getAvailableDefenders(int side)
{
    return m_availableDefenders[side - 1];
}

// ELF 0x3426E0.
bool AIStrategicObject::unregisterAvailableAttacker(std::set<int>::iterator& it, int side)
{
    std::set<int>& units = m_availableAttackers[side - 1];
    if (it == units.end())
        return false;
    units.erase(it++);
    return true;
}

// ELF 0x342780.
bool AIStrategicObject::unregisterAvailableDefender(std::set<int>::iterator& it, int side)
{
    std::set<int>& units = m_availableDefenders[side - 1];
    if (it == units.end())
        return false;
    units.erase(it++);
    return true;
}

// ELF 0x341FF0.
bool AIStrategicObject::registerAvailableAttacker(int unit, int side)
{
    m_availableAttackers[side - 1].insert(unit);
    return true;
}

// ELF 0x342040.
bool AIStrategicObject::registerAvailableDefender(int unit, int side)
{
    m_availableDefenders[side - 1].insert(unit);
    return true;
}

// ELF 0x342600.
bool AIStrategicObject::unregisterAvailableAttacker(int unit, int side)
{
    std::set<int>::iterator it = m_availableAttackers[side - 1].find(unit);
    return unregisterAvailableAttacker(it, side);
}

// ELF 0x342670.
bool AIStrategicObject::unregisterAvailableDefender(int unit, int side)
{
    std::set<int>::iterator it = m_availableDefenders[side - 1].find(unit);
    return unregisterAvailableDefender(it, side);
}

// ELF 0x342310.
std::set<int>& AIStrategicObject::getAssignedUnits(int side)
{
    return m_set68[side - 1];
}

// ELF 0x3423E0.
std::set<int>& AIStrategicObject::getPresentUnits(int side)
{
    return m_presentUnits[side - 1];
}

// ELF 0x342400.
bool AIStrategicObject::isPresentUnit(int unit, int side) const
{
    return m_presentUnits[side - 1].find(unit) != m_presentUnits[side - 1].end();
}

// ELF 0x342460.
std::pair<std::set<int>::iterator, bool> AIStrategicObject::setAsPresentUnit(int unit, int side)
{
    return m_presentUnits[side - 1].insert(unit);
}

// ELF 0x342550.
void AIStrategicObject::removeAsPresentUnit(std::set<int>::iterator& it, int side)
{
    m_presentUnits[side - 1].erase(it++);
}

// ELF 0x3424C0.
std::set<int>::iterator AIStrategicObject::removeAsPresentUnit(int unit, int side)
{
    std::set<int>::iterator it = m_presentUnits[side - 1].find(unit);
    if (it != m_presentUnits[side - 1].end())
        removeAsPresentUnit(it, side);
    return it;
}

// ELF 0x342E80.
bool AIStrategicObject::isAllowedVehicleGroup(int group) const
{
    return (m_i244 >> group) & 1;
}

// ELF 0x342EA0.
bool AIStrategicObject::allowsSomeVehicleGroups(int groupMask) const
{
    return (m_i244 & groupMask) != 0;
}

// Two draws (AIRandom, the inline tempering), x then z: offset = draw * extent * scale -
// 0.5 * extent, each extent queried twice (`call *0x74/*0x78` per use).
// ELF 0x342B80.
void AIStrategicObject::randomizePos(int side, float scale, Vec3& position)
{
    AIRandom* const random = AIRandom::m_instance;
    const float drawX = random->nextFloat();
    const float scaledX = getXExtent(side) * drawX * scale;
    const float dx = scaledX - getXExtent(side) * 0.5f;
    const float drawZ = random->nextFloat();
    const float scaledZ = drawZ * getZExtent(side) * scale;
    const float dz = scaledZ - getZExtent(side) * 0.5f;
    position.x = dx + m_position[side - 1].x;
    position.y = m_f148;
    position.z = dz + m_position[side - 1].y;
}

// Kept in x87 extended precision in the image: the angle (draw * 2 * pi, rodata 3.14159274f) and
// the distance chain; sinf/cosf take the float angle. The direction is the image's
// (1,0,0) * sin + (0,0,1) * cos sum, zero products included.
// ELF 0x342990.
void AIStrategicObject::randomizeControlpointPos(float scale, float spread, Vec3& position)
{
    AIRandom* const random = AIRandom::m_instance;
    const double turn = static_cast<double>(random->next()) * static_cast<double>(2.32830644e-10f);
    const double angle = (turn + turn) * static_cast<double>(3.14159274f);
    const double draw = static_cast<double>(random->next()) * static_cast<double>(2.32830644e-10f);
    const float distance = static_cast<float>(
        (0.5 + draw * (static_cast<double>(spread) - 0.5)) * scale);
    const double s = std::sin(static_cast<float>(angle));
    const double c = std::cos(static_cast<float>(angle));
    const double dirX = s * 1.0 + c * 0.0;
    const double dirY = c * 0.0 + s * 0.0;
    const double dirZ = c * 1.0 + s * 0.0;
    position.x = static_cast<float>(dirX * distance + position.x);
    position.y = static_cast<float>(dirY * distance + position.y);
    position.z = static_cast<float>(dirZ * distance + position.z);
}

// ELF 0x342EF0.
bool AIStrategicObject::isExpelledUnit(int equipment) const
{
    return (m_i248 >> equipment) & 1;
}

// ELF 0x341FB0.
void AIStrategicObject::setAttack(bool attack, int side)
{
    m_attack[side - 1] = attack;
}

// ELF 0x342940.
float AIStrategicObject::getAssignedPartOfWantedStrength(int side) const
{
    const int need = m_resourceNeed[side - 1];
    if (!need)
        return 1.0f;
    return static_cast<float>(m_assignedSquadStrength[side - 1] / need);
}

// The ten accessors. Every one of them is between seven and thirty-three bytes in the image.

// Returns type code 3; AIStrategicArea returns 0.
// [UNRESOLVED] These codes are not established as StrategicObjectType values.
// ELF 0x343280.
int AIStrategicObject::getType() const
{
    return 3;
}

// - `return (char *)this + 8 * side + 124;`, i.e. the 1-based fold. See the header.
// ELF 0x340B20.
const Vec2& AIStrategicObject::getPosition(int side) const
{
    return m_position[side - 1];
}

// - two dword stores, a memberwise Vec2 copy.
// ELF 0x340B40.
void AIStrategicObject::setPosition(int side, const Vec2& position)
{
    m_position[side - 1] = position;
}

// Returns bit 27. [UNRESOLVED] No named StrategicObjectType enumerator covers that bit.
// ELF 0x343290.
int AIStrategicObject::getObjectTypeFlags() const
{
    return 0x8000000;
}

// ELF 0x3432A0.
bool AIStrategicObject::hasObjectTypeFlag(StrategicObjectType) const
{
    return false;
}

// ELF 0x3432B0.
bool AIStrategicObject::hasObjectTypeFlags(int) const
{
    return false;
}

// ELF 0x3432C0.
float AIStrategicObject::gravity(const Vec3&, int)
{
    return 0.0f;
}

// ELF 0x3432E0.
float AIStrategicObject::gravity(const Vec2&, int)
{
    return 0.0f;
}

// Returns twice the side-specific size for both axes; base strategic objects are square.
// ELF 0x343300.
float AIStrategicObject::getXExtent(int side) const
{
    return m_size[side] + m_size[side];
}

// ELF 0x343320.
float AIStrategicObject::getZExtent(int side) const
{
    return m_size[side] + m_size[side];
}

// ELF 0x342FC0: the current statuses (+0x74/+0x78) take the initial ones (+0x7C/+0x80).
void AIStrategicObject::resetStatus()
{
    m_status[0] = m_status[2];
    m_status[1] = m_status[3];
}

// ELF 0x341ED0.
void AIStrategicObject::setSide(int side)
{
    if (side == 1)
    {
        m_status[0] = StatusMap::Owned;
        m_status[2] = StatusMap::Owned;
        m_status[1] = StatusMap::Hostile;
        m_status[3] = StatusMap::Hostile;
    }
    else if (side == 2)
    {
        m_status[0] = StatusMap::Hostile;
        m_status[2] = StatusMap::Hostile;
        m_status[1] = StatusMap::Owned;
        m_status[3] = StatusMap::Owned;
    }
    else
    {
        m_status[2] = StatusMap::Neutral;
        m_status[3] = StatusMap::Neutral;
    }
}

// ELF 0x341F40.
int AIStrategicObject::getSide()
{
    if (m_status[2] == StatusMap::Owned)
        return 1;
    return m_status[3] == StatusMap::Owned ? 2 : 0;
}

// ELF 0x341F70.
void AIStrategicObject::setPriority(int priority, int side)
{
    m_unresolved212[side - 1] = priority;
}

// ELF 0x341F90.
int AIStrategicObject::getPriority(int side) const
{
    return m_unresolved212[side - 1];
}

// ELF 0x342110.
bool AIStrategicObject::releaseAssignedUnit(int unit, int side)
{
    std::set<int>::iterator it = m_set68[side - 1].find(unit);
    if (it == m_set68[side - 1].end())
        return false;
    return IAIMain::m_instance->getSAI(side)->releaseUnitFromObject(m_index, it, side);
}

// ELF 0x3421C0. The find in the chosen available set is the image's (its result is unused).
bool AIStrategicObject::bookBot(bool attacker, int side, std::set<int>::iterator& it)
{
    const int unit = *it;
    if (attacker)
        m_availableAttackers[side - 1].find(unit);
    else
        m_availableDefenders[side - 1].find(unit);
    Bot* const bot = IBotManager::instance->getBotFromId(unit);
    InformationManager* const manager = informationManager;
    Information* const information = manager->getInformation(bot->getCurrentEquipment());
    return information && information->getUnit();
}

// ELF 0x342330.
int AIStrategicObject::getNumberOfAvailableBots(bool attackers, int side) const
{
    if (attackers)
        return static_cast<int>(m_availableAttackers[side - 1].size());
    return static_cast<int>(m_availableDefenders[side - 1].size());
}

// ELF 0x342360.
int AIStrategicObject::getNumberOfAvailableAttackers(int side) const
{
    return static_cast<int>(m_availableAttackers[side - 1].size());
}

// ELF 0x342380.
int AIStrategicObject::getNumberOfAvailableDefenders(int side) const
{
    return static_cast<int>(m_availableDefenders[side - 1].size());
}

// ELF 0x3423C0.
int AIStrategicObject::getNumberOfPresentUnits(int side) const
{
    return static_cast<int>(m_presentUnits[side - 1].size());
}

// ELF 0x3425F0.
EnemyStatistics& AIStrategicObject::getStrategicStatistics(int side) const
{
    return *m_statistics[side + 1];
}

// ELF 0x342820.
int AIStrategicObject::unregisterAllAvailableAttackers(int side)
{
    const int count = static_cast<int>(m_availableAttackers[side - 1].size());
    m_availableAttackers[side - 1].clear();
    return count;
}

// ELF 0x3428B0.
int AIStrategicObject::unregisterAllAvailableDefenders(int side)
{
    const int count = static_cast<int>(m_availableDefenders[side - 1].size());
    m_availableDefenders[side - 1].clear();
    return count;
}

// ELF 0x342D40.
void AIStrategicObject::addAllowedUnit(int equipment)
{
    addAllowedVehicleGroup(
        static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getVehicleGroup(equipment));
}

// ELF 0x342D90.
void AIStrategicObject::addAllowedVehicleGroup(int group)
{
    if (m_i244 == -1)
        m_i244 = 0;
    m_i244 |= 1 << group;
}

// ELF 0x342DD0.
void AIStrategicObject::addAllowedVehicleGroup(const std::string& group)
{
    addAllowedVehicleGroup(
        static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getVehicleGroup(group));
}

// ELF 0x342E20.
int AIStrategicObject::getAllowedVehicleGroups() const
{
    return m_i244;
}

// ELF 0x342E30.
bool AIStrategicObject::isAllowedUnit(int equipment) const
{
    return isAllowedVehicleGroup(
        static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getVehicleGroup(equipment));
}

// ELF 0x342EC0.
void AIStrategicObject::addExpelledUnit(int equipment)
{
    m_i248 |= 1 << equipment;
}

// ELF 0x342EE0.
int AIStrategicObject::getExpelledVehicleTypes() const
{
    return m_i248;
}

// ELF 0x342F10.
bool AIStrategicObject::removeGrouping(int group)
{
    std::set<int>::iterator it = m_set256.find(group);
    if (it == m_set256.end())
        return false;
    m_set256.erase(it);
    return true;
}

} // namespace ai
} // namespace hfe
} // namespace dice
