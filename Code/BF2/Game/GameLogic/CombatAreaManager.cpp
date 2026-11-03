// Path:     [INFERRED]. See the header.
#include "CombatAreaManager.h"
#include "CombatArea.h"
#include "../Objects/IPlayer.h"
#include "../Objects/IObject.h"
#include "../Objects/IObjectTemplate.h"                 // queryInterface (+0x0C) / getClassID (+0x10)
#include "../Objects/IPlayerControlObjectTemplate.h"    // getVehicleCategory (+0x24)
#include "../Objects/ObjectLayoutAccess.h"              // the raw IObject +0x14 template read
#include "../../Core/Ids.h"
#include "../../Core/StringUtil.h"          // compareIgnoreCase - the area-name lookup is case-insensitive
#include "../../Physics/HeightmapCluster.h"   // IHeightmapCluster::getPrimaryHeightmap
#include "../../Physics/Heightmap.h"          // IHeightmap::getSize

namespace dice {
namespace hfe {

namespace world { extern IBase* heightmapCluster; }   // ds:0x8ABAF24 - Core/Globals.cpp

CombatAreaManager* CombatAreaManager::m_instance = 0;   // ds:0x8AAC96C

// Create an empty area registry with default timers and damage. Only the first manager
// becomes the singleton; constructing another does not replace or own that instance.
CombatAreaManager::CombatAreaManager()
    : m_activeArea(0)
    , m_areas()
    , m_useCombatAreas(false)          // a byte at +0x10 - see the header's decompiler note
    , m_defaultTimeAllowedOutside(10.0f)
    , m_defaultDamage(20.0f)
    , m_vehicleDamage()
    , m_vehicleTimers()
{
    m_vehicleTimers.insert(std::make_pair(0, 10.0f));
    m_vehicleTimers.insert(std::make_pair(2, 10.0f));

    if (!m_instance)                   // first wins
        m_instance = this;
}

// Delete every owned area and clear the singleton only if it points to this manager.
CombatAreaManager::~CombatAreaManager()
{
    for (unsigned int i = 0; i < m_areas.size(); ++i)
    {
        CombatArea* area = m_areas[i];
        if (area)
            delete area;
    }
    m_areas.erase(m_areas.begin(), m_areas.end());

    if (m_instance == this)                                   // ds:0x8AAC96C
        m_instance = 0;
}

// Delete all areas and restore the default filters, damage, and category timers for a new level.
// The singleton remains installed.
void CombatAreaManager::reset()
{
    for (unsigned int i = 0; i < m_areas.size(); ++i)
    {
        CombatArea* area = m_areas[i];
        if (area)
            delete area;
    }
    m_areas.erase(m_areas.begin(), m_areas.end());

    m_vehicleDamage.clear();                                          // +0x1C
    m_vehicleTimers.clear();                                          // +0x28
    m_vehicleTimers.insert(std::make_pair(0, 10.0f));
    m_vehicleTimers.insert(std::make_pair(2, 10.0f));

    m_activeArea         = 0;
    m_useCombatAreas = false;                                         // +0x10, the byte
    m_defaultTimeAllowedOutside         = 10.0f;
    m_defaultDamage         = 20.0f;
}

// Test the player vehicle against applicable inclusions, then exclusions. Null players and
// players without a controllable vehicle return false. If areas are disabled or no inclusion
// applies, test the symmetric terrain bounds; that fallback requires a valid primary heightmap.
bool CombatAreaManager::isPlayerInside(world::IPlayer* player)
{
    if (!player)
        return false;

    world::IObject* vehicle = player->getVehicle();                      // IPlayer +0x48
    if (!vehicle)
        return false;
    if (!vehicle->queryInterface(world::IID_IPlayerControlObject))       // +0x0C
        return false;

    const Vec3 position = player->getVehicle()->getAbsolutePosition();   // a third getVehicle()

    // the pre-pass: does any inclusion area apply to this player at all?
    bool anyApplies = false;
    for (size_t i = 0; i < m_areas.size(); ++i)
    {
        CombatArea* area = m_areas[i];
        if (!area->m_isExclusion && area->checkIfActiveToPlayer(player))
            anyApplies = true;
    }

    if (m_useCombatAreas && anyApplies)
    {
        bool inside = false;

        // pass one - inclusions set it
        for (size_t i = 0; i < m_areas.size(); ++i)
        {
            CombatArea* area = m_areas[i];
            if (area->m_isExclusion || !area->checkIfActiveToPlayer(player))
                continue;
            const bool hit = area->m_isPolygon ? area->checkPositionPolygon(position)
                                               : area->checkPositionRect(position);
            if (hit)
                inside = true;
        }

        // pass two - exclusions clear it, and always win
        for (size_t i = 0; i < m_areas.size(); ++i)
        {
            CombatArea* area = m_areas[i];
            if (!area->m_isExclusion || !area->checkIfActiveToPlayer(player))
                continue;
            const bool hit = area->m_isPolygon ? area->checkPositionPolygon(position)
                                               : area->checkPositionRect(position);
            if (hit)
                inside = false;
        }
        return inside;
    }

    // the fallback: the heightmap's own bounds
    world::IHeightmapCluster* cluster =
        reinterpret_cast<world::IHeightmapCluster*>(world::heightmapCluster);
    world::IHeightmap* heightmap = cluster->getPrimaryHeightmap();       // cluster unchecked
    int dimX = 0, dimZ = 0;
    heightmap->getSize(dimX, dimZ);                                      // heightmap unchecked too

    const float limX = static_cast<float>(dimX);
    const float limZ = static_cast<float>(dimZ);
    if (-limX > position.x) return false;
    if (-limZ > position.z) return false;
    if (position.x > limX)  return false;
    if (position.z > limZ)  return false;
    return true;
}

// Return the vehicle category grace period, or the default for null objects, ladders, and
// unlisted categories. Non-null objects must have a valid template. The timer lookup
// precedes the ladder check.
float CombatAreaManager::getTimeAllowedOutside(world::IObject* object)
{
    if (!object)
        return m_defaultTimeAllowedOutside;

    const world::IObjectTemplate* objectTemplate =
        world::object_detail::LayoutAccess::objectTemplate(object);          // raw +0x14

    // 4 == VCNoVehicleCategory - the value the ELF loads before the optional override.
    int category = 4;
    const world::IPlayerControlObjectTemplate* pcoTemplate =
        static_cast<const world::IPlayerControlObjectTemplate*>(
            objectTemplate->queryInterface(world::IID_IPlayerControlObjectTemplate));   // +0x0C
    if (pcoTemplate)
        category = pcoTemplate->getVehicleCategory();                        // +0x24

    // the lookup precedes the ladder test and is discarded on that arm - see the block above.
    const std::map<int, float>::const_iterator entry = m_vehicleTimers.find(category);

    if (objectTemplate->getClassID() == world::CID_LadderContainerTemplate)  // +0x10
        return m_defaultTimeAllowedOutside;

    if (entry == m_vehicleTimers.end())
        return m_defaultTimeAllowedOutside;
    return entry->second;
}

// Return damage for the vehicle category, or the default for null objects and unlisted
// categories. Non-null objects must have a valid template; ladders have no special case.
float CombatAreaManager::getDamage(world::IObject* object)
{
    if (!object)
        return m_defaultDamage;

    const world::IObjectTemplate* objectTemplate =
        world::object_detail::LayoutAccess::objectTemplate(object);          // raw +0x14

    // 4 == VCNoVehicleCategory - the value the ELF loads before the optional override.
    int category = 4;
    const world::IPlayerControlObjectTemplate* pcoTemplate =
        static_cast<const world::IPlayerControlObjectTemplate*>(
            objectTemplate->queryInterface(world::IID_IPlayerControlObjectTemplate));   // +0x0C
    if (pcoTemplate)
        category = pcoTemplate->getVehicleCategory();                        // +0x24

    const std::map<int, float>::const_iterator entry = m_vehicleDamage.find(category);
    if (entry == m_vehicleDamage.end())
        return m_defaultDamage;
    return entry->second;
}

// Find or create an area by case-insensitive name and make it active. The manager owns
// the returned area; new areas accept all vehicle categories and use AI layer 1.
CombatArea* CombatAreaManager::createCombatArea(const std::string& name)
{
    for (unsigned int i = 0; i < m_areas.size(); ++i)
    {
        if (compareIgnoreCase(name, m_areas[i]->m_name) == 0)
        {
            m_activeArea = m_areas[i];                 // LABEL_8: the hit is made active too
            return m_activeArea;
        }
    }

    CombatArea* area = new CombatArea();               // operator new(0x34)
    area->m_isPolygon        = 0;
    area->m_minX             = 0.0f;
    area->m_minZ             = 0.0f;
    area->m_maxX             = 0.0f;
    area->m_maxZ             = 0.0f;
    area->m_vehicleCategory  = 4;                      // "all", not 0
    area->m_team             = 0;
    area->m_isExclusion      = false;
    area->m_usedByPathFinding = false;
    area->m_layer            = 1;                      // 1, not 0
    area->m_name             = name;

    m_areas.push_back(area);
    m_activeArea = area;
    return area;
}

// Select an existing area by case-insensitive name. A miss clears the active cursor
// and returns null, so subsequent guarded property writes do nothing.
CombatArea* CombatAreaManager::setActiveCombatArea(const std::string& name)
{
    for (unsigned int i = 0; i < m_areas.size(); ++i)
    {
        if (compareIgnoreCase(name, m_areas[i]->m_name) == 0)
        {
            m_activeArea = m_areas[i];
            return m_activeArea;
        }
    }

    m_activeArea = 0;                                  // the miss path, as shipped
    return 0;
}

// Remove the active area from the registry before deleting it and clearing the cursor.
// Do nothing if it is null or absent from the registry; an absent pointer remains active.
void CombatAreaManager::deleteActiveCombatArea()
{
    if (!m_activeArea)
        return;

    for (unsigned int i = 0; i < m_areas.size(); ++i)
    {
        if (m_areas[i] == m_activeArea)
        {
            m_areas.erase(m_areas.begin() + i);
            delete m_activeArea;                       // the inlined ~CombatArea in the ELF
            m_activeArea = 0;
            return;
        }
    }
    // not found: the shipped body returns without clearing m_activeArea.
}

// Return the active area name, or an empty string when no area is active.
std::string CombatAreaManager::getActiveCombatAreaName()
{
    if (m_activeArea)
        return m_activeArea->m_name;
    return std::string("");
}

// Set the damage override for a vehicle category, replacing any previous value.
void CombatAreaManager::addVehicleDamage(int vehicleCategory, float damage)
{
    std::map<int, float>::iterator entry = m_vehicleDamage.find(vehicleCategory);   // +0x1C
    if (entry == m_vehicleDamage.end())
        m_vehicleDamage.insert(std::make_pair(vehicleCategory, damage));
    else
        entry->second = damage;
}

// Set the grace period in seconds for a vehicle category, replacing any previous value.
void CombatAreaManager::addVehicleTimer(int vehicleCategory, float seconds)
{
    std::map<int, float>::iterator entry = m_vehicleTimers.find(vehicleCategory);   // +0x28
    if (entry == m_vehicleTimers.end())
        m_vehicleTimers.insert(std::make_pair(vehicleCategory, seconds));
    else
        entry->second = seconds;
}

// ELF 0x8125A00. The caller owns clearing the output vector.
bool CombatAreaManager::getAiCombatAreas(std::vector<CombatArea*>& areas) const
{
    for (unsigned int i = 0; i < m_areas.size(); ++i)
        if (m_areas[i]->m_usedByPathFinding)
            areas.push_back(m_areas[i]);
    return areas.size() == 1;
}

// ELF 0x8125890. The manager's player/team and enable filters do not apply here.
bool CombatAreaManager::isAiPositionInside(const Vec3& position,
                                         const std::vector<CombatArea*>& areas) const
{
    bool anyInclusion = false;
    bool inside = false;
    for (unsigned int i = 0; i < areas.size(); ++i)
        if (!areas[i]->m_isExclusion)
            anyInclusion = true;

    if (!anyInclusion)
    {
        world::IHeightmap* heightmap =
            static_cast<world::IHeightmapCluster*>(world::heightmapCluster)->getPrimaryHeightmap();
        int sizeX, sizeZ;
        heightmap->getSize(sizeX, sizeZ);
        // shipped defect: the dimensions are queried but never used to test the position.
        inside = true;
    }

    for (unsigned int i = 0; i < areas.size(); ++i)
    {
        CombatArea* area = areas[i];
        if (!area->m_isExclusion)
        {
            const bool hit = static_cast<unsigned char>(area->m_isPolygon)
                ? area->checkPositionPolygon(position) : area->checkPositionRect(position);
            if (hit)
                inside = true;
        }
    }
    for (unsigned int i = 0; i < areas.size(); ++i)
    {
        CombatArea* area = areas[i];
        if (area->m_isExclusion)
        {
            const bool hit = static_cast<unsigned char>(area->m_isPolygon)
                ? area->checkPositionPolygon(position) : area->checkPositionRect(position);
            if (hit)
                inside = false;
        }
    }
    return inside;
}

} // namespace hfe
} // namespace dice
