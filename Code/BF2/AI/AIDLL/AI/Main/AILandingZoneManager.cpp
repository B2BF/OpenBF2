// Source placement: AIDLL_r.dll __FILE__ "Code\BF2\AI\AIDLL\AI\Main\AILandingZoneManager.cpp" (0x10081FA0).

// static init: ELF 0x39AA80, __tcf_0 0x39AB10, _GLOBAL__I 0x39AB40: stream Init precedes the manager.
#include <iostream>

#include "AILandingZoneManager.h"

#include <sstream>

#include "../AIStrategicEntity/AILandingZone.h"
#include "../AIUtils/checkPointer.h"
#include "../AIUtils/DebugRendering/AIRenderUtil.h"   // updateNumberOfLandingZones

namespace dice {
namespace hfe {
namespace ai {

AILandingZoneManager* AILandingZoneManager::m_instance = 0;

// ELF 0x399980 (C1; C2 0x399B50). Initializes an inactive landing-zone manager and explicitly
// resizes its new vector to zero.
AILandingZoneManager::AILandingZoneManager()
    : m_activeZone(0)
    , m_unresolved20(0)
    , m_activeZoneName("No valid zone is active")
{
    m_zones.resize(0);
}

// static init: ELF 0x39AA80 (with __tcf_0 0x39AB10 and _GLOBAL__I 0x39AB40):
// constructs the landing-zone manager at DLL load. BF2AI::clear owns its later destruction.
namespace {

struct AILandingZoneManagerInstaller
{
    AILandingZoneManagerInstaller()
    {
        AILandingZoneManager::m_instance = new AILandingZoneManager();
    }
};

AILandingZoneManagerInstaller g_aiLandingZoneManagerInstaller;

} // namespace

// ELF 0x399C50 (D1; D0 0x399D60): deletes every non-null zone (AILandingZone D1 + operator delete),
// then the name, the vector and the map go as members. m_instance is not cleared.
AILandingZoneManager::~AILandingZoneManager()
{
    for (std::vector<AILandingZone*>::iterator zone = m_zones.begin(); zone != m_zones.end(); ++zone)
    {
        if (*zone)
            delete *zone;
    }
}

// ELF 0x39A350: deletes the zones (as above), zeroes +0x20 and +0x1C, renames the active zone
// "No valid zone is active" and empties the zone vector (resize(0)); the name map is kept.
bool AILandingZoneManager::reset()
{
    for (std::vector<AILandingZone*>::iterator zone = m_zones.begin(); zone != m_zones.end(); ++zone)
    {
        if (*zone)
            delete *zone;
    }
    m_unresolved20 = 0;
    m_activeZone = 0;
    m_activeZoneName = "No valid zone is active";
    m_zones.resize(0, 0);
    return true;
}

// ELF 0x399E70.
const std::string& AILandingZoneManager::activeZone()
{
    return m_activeZone ? m_activeZone->m_name : m_activeZoneName;
}

// ELF 0x399E90.
const std::string& AILandingZoneManager::activeZone(int index)
{
    if (index < static_cast<int>(m_zones.size()) && index >= 0)
    {
        m_activeZone = m_zones[index];
        return m_activeZone->m_name;
    }
    m_activeZone = 0;
    return m_activeZoneName;
}

// ELF 0x399ED0.
const std::string& AILandingZoneManager::activeZone(const std::string& name)
{
    std::map<std::string, AILandingZone*>::iterator it = m_zonesByName.find(name);
    if (it == m_zonesByName.end())
    {
        m_activeZone = 0;
        return m_activeZoneName;
    }
    m_activeZone = it->second;
    return m_activeZone->m_name;
}

// ELF 0x399F40.
AILandingZone* AILandingZoneManager::getActiveZone()
{
    return m_activeZone;
}

// ELF 0x399F50.
AILandingZone* AILandingZoneManager::getAILandingZone(int index)
{
    if (index < static_cast<int>(m_zones.size()) && index >= 0)
        return m_zones[index];
    return 0;
}

// ELF 0x399F80.
AILandingZone* AILandingZoneManager::getAILandingZone(const std::string& name)
{
    std::map<std::string, AILandingZone*>::iterator it = m_zonesByName.find(name);
    if (it == m_zonesByName.end())
        return 0;
    return it->second;
}

// ELF 0x399FE0.
int AILandingZoneManager::getNumberOfLandingZones() const
{
    return static_cast<int>(m_zones.size());
}

// ELF 0x39A000. AIRenderUtil::m_instance is not null-checked.
AILandingZone* AILandingZoneManager::createLandingZone(const std::string& name, const Vec2& minCorner,
                                                       const Vec2& maxCorner, float height,
                                                       LandingZoneDirection beachDirection)
{
    const int id = static_cast<int>(m_zones.size());
    AILandingZone* const zone =
        checkPointer(new AILandingZone(name, id, minCorner, maxCorner, height, beachDirection));
    m_zones.resize(m_zones.size() + 1);
    m_zones[id] = zone;
    m_activeZone = zone;
    m_zonesByName.insert(std::make_pair(name, zone));
    AIRenderUtil::m_instance->updateNumberOfLandingZones();
    return zone;
}

// ELF 0x39A460.
bool AILandingZoneManager::isInsideLandingZone(const Vec3& position) const
{
    AILandingZone* zone;
    return isInsideLandingZone(position, zone);
}

// ELF 0x39A4A0: `zone` receives the zone, 0 when none contains the position.
bool AILandingZoneManager::isInsideLandingZone(const Vec3& position, AILandingZone*& zone) const
{
    for (std::vector<AILandingZone*>::const_iterator it = m_zones.begin(); it != m_zones.end(); ++it)
    {
        if ((*it)->isInZone(position))
        {
            zone = *it;
            return true;
        }
    }
    zone = 0;
    return false;
}

// ELF 0x39A510. Null slots are skipped; the corners use the image's inlined x/y Vec2 output and
// the direction prints as an int.
std::string AILandingZoneManager::listZones()
{
    std::stringstream stream;
    for (std::vector<AILandingZone*>::iterator it = m_zones.begin(); it != m_zones.end(); ++it)
    {
        AILandingZone* const zone = *it;
        if (!zone)
            continue;
        const Vec2& minCorner = zone->getMinCorner();
        const Vec2& maxCorner = zone->getMaxCorner();
        stream << zone->m_name << " " << minCorner.x << '/' << minCorner.y << " " << maxCorner.x
               << '/' << maxCorner.y << " " << static_cast<int>(zone->getBeachDirection())
               << std::endl;
    }
    return stream.str();
}

} // namespace ai
} // namespace hfe
} // namespace dice
