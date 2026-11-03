// [INFERRED] Source placement.

// static init: ELF 0x39C180 (__tcf_0 0x39C1E0, _GLOBAL__I 0x39C210): <iostream>'s Init object only.
#include <iostream>

#include "AILODManager.h"

#include <cmath>
#include "CommonSingletonServer.h"
#include "IBotManager.h"
#include "../AIBot/BotMain.h"
#include "../AIKnowledge/Information.h"
#include "../AIKnowledge/InformationManager.h"
#include "../AIKnowledge/IPIMobile.h"
#include "../../../AIInterface/BFKnowledge/AIPlayer.h"
#include "../../../AIInterface/BFKnowledge/AIPlayerFactory.h"
#include "../../../AIInterface/BFKnowledge/BFObjectEnvironment.h"
#include "../../../AIInterface/BFKnowledge/IAIObject.h"
#include "../../../AIInterface/BFKnowledge/IAIObjectPlugIn.h"
#include "../../../../Math/Mat4.h"

namespace dice {
namespace hfe {
namespace ai {

AILODManager* AILODManager::m_instance = 0;

// ELF 0x39B0D0 / 0x39B100.
PlayerLODStats::PlayerLODStats() : m_position(), m_player(0), m_lodDistance(0.0f) {}

// ELF 0x39B130 / 0x39B160.
PlayerLODStats::PlayerLODStats(IAIPlayer* player)
    : m_position(), m_player(player), m_lodDistance(0.0f) {}

// ELF 0x39B190 / 0x39B1D0.
PlayerLODStats::PlayerLODStats(const PlayerLODStats& other)
    : m_position(other.m_position), m_player(other.m_player), m_lodDistance(other.m_lodDistance) {}

// ELF 0x39B210 / 0x39B220. Does not own the player.
PlayerLODStats::~PlayerLODStats() {}

// ELF 0x39B230. Dead players, or those without a vehicle, use the camera position.
void PlayerLODStats::update(float basicDistance, float speedScale)
{
    float speed = 0.0f;
    IAIObject* vehicle = 0;
    if (m_player->getIsAlive())
        vehicle = m_player->getCurrentVehicle();
    if (vehicle)
    {
        m_position = vehicle->getPosition();
        if (vehicle->isMobile())
            speed = vehicle->getMobile()->getSpeedMagnitude();
    }
    else
    {
        const Mat4* const camera = static_cast<const Mat4*>(m_player->getCameraTransformation());
        m_position.x = camera->m[12];
        m_position.y = camera->m[13];
        m_position.z = camera->m[14];
    }
    speedScale *= speed;
    m_lodDistance = basicDistance + speedScale;
    m_lodDistance *= m_player->getFieldOfViewModifier();
}

// ELF 0x39B320 / 0x39B4C0. Seeds both distances equally and updates players before
// publishing the singleton. Player initialization must work before publication.
AILODManager::AILODManager(float basicLODDistance, float f36)
    : m_playerStats()
    , m_updatedPlayerIds()
    , m_basicLODDistance(basicLODDistance)
    , m_maxLODDistance(basicLODDistance)
    , m_f36(f36)
    , m_lodEnable(true)
    , m_playerCursor(-1)
    , m_botCursor(0)
{
    updateAllPlayers();

    if (!m_instance)
        m_instance = this;
}

// ELF 0x39B690 (D1, 240 B; recovered on the client track, docs/macos-c-singleplayer.md): the
// singleton is zeroed when it is this one; the set and the stats map go as members.
AILODManager::~AILODManager()
{
    if (m_instance == this)
        m_instance = 0;
}

// ELF 0x39C090. Updates players, clears their statistics, and enables LOD.
bool AILODManager::reset()
{
    updatePlayers();

    m_playerStats.clear();
    m_lodEnable = true;
    return true;
}

// ELF 0x39C170. `AISettings::setViewDistance` is its only caller in the image.
void AILODManager::setBasicLODDistance(float distance)
{
    m_basicLODDistance = distance;
}

// Setter and getter intentionally share the lodEnable name.
// ELF 0x39C060: stores the byte and returns it (movzbl of the argument).
bool AILODManager::lodEnable(bool enable)
{
    m_lodEnable = enable;
    return enable;
}

// ELF 0x39C080. Nonconst by the mangled symbol.
bool AILODManager::lodEnable()
{
    return m_lodEnable;
}

// ELF 0x39B780.
void AILODManager::updateAllPlayers()
{
    if (!m_lodEnable)
        return;
    const std::vector<int>& players = static_cast<IAIObjectEnvironment*>(
        CommonSingletonServer::m_iaiObjectEnvironment)->getActivePlayerIds();
    m_updatedPlayerIds.clear();
    for (std::vector<int>::const_iterator it = players.begin(); it != players.end(); ++it)
        updatePlayer(*it);
    updatePlayerMap();
    m_updatedPlayerIds.clear();
}

// ELF 0x39B880. A completed sweep prunes stale entries before clearing its ID set.
void AILODManager::updatePlayers()
{
    IAIObjectEnvironment* const environment = static_cast<IAIObjectEnvironment*>(
        CommonSingletonServer::m_iaiObjectEnvironment);
    const std::vector<int>& players = environment->getActivePlayerIds();
    if (environment->isValidPlayerId(m_playerCursor))
    {
        updatePlayer(m_playerCursor);
        m_playerCursor = environment->getNextPlayerId(m_playerCursor);
    }
    else
    {
        if (!players.empty())
        {
            m_playerCursor = players.front();
            updatePlayerMap();
        }
        m_updatedPlayerIds.clear();
    }
}

// ELF 0x39B970. Disabled LOD still answers true without touching player state.
bool AILODManager::updatePlayer(int playerId)
{
    if (m_lodEnable)
    {
        std::map<int, PlayerLODStats>::iterator it = m_playerStats.find(playerId);
        if (it == m_playerStats.end())
        {
            IAIPlayer* const player = static_cast<IAIPlayerFactory*>(
                CommonSingletonServer::m_iaiPlayerFactory)->getPlayerFromId(playerId);
            it = m_playerStats.insert(std::pair<const int, PlayerLODStats>(
                playerId, PlayerLODStats(player))).first;
        }
        it->second.update(m_basicLODDistance, m_f36);
        m_updatedPlayerIds.insert(it->first);
    }
    return true;
}

// ELF 0x39BB80. Equal container sizes skip the scan, even if their keys differ.
void AILODManager::updatePlayerMap()
{
    if (m_updatedPlayerIds.size() == m_playerStats.size())
        return;
    std::map<int, PlayerLODStats>::iterator it = m_playerStats.begin();
    while (it != m_playerStats.end())
    {
        if (m_updatedPlayerIds.find(it->first) == m_updatedPlayerIds.end())
            m_playerStats.erase(it++);
        else
            ++it;
    }
}

// ELF 0x39BC90. Limits each enabled pass to four successful updates, retaining the cursor.
void AILODManager::updateBots()
{
    IBotManager* const manager = IBotManager::instance;
    if (!manager || manager->getBotCount() == 0)
        return;
    int updated = 0;
    Bot* const first = manager->getBot(static_cast<unsigned int>(m_botCursor));
    ++m_botCursor;
    if (!m_lodEnable)
        first->setLodLevel(BotLodFull);
    else
    {
        if (updateBot(first))
            updated = 1;
        while (updated < 4 && m_botCursor < manager->getBotCount())
        {
            Bot* const bot = manager->getBot(static_cast<unsigned int>(m_botCursor));
            const bool success = updateBot(bot);
            ++m_botCursor;
            if (success)
                ++updated;
        }
    }
    if (m_botCursor >= manager->getBotCount())
        m_botCursor = 0;
}

// ELF 0x39BD70. Hysteresis depends on the old level; each pass begins at light detail.
bool AILODManager::updateBot(Bot* bot)
{
    const BotLodLevel oldLevel = bot->getLodLevel();
    bot->setLodLevel(BotLodLight);
    if (bot->isDead())
        return false;
    Information* const information = informationManager->getInformation(bot->getCurrentEquipment());
    if (!information)
        return false;
    IAIObject* const object = information->getAIObject();
    const Vec3& position = information->getPosition();
    float fullThreshold = 1.0f;
    float mediumThreshold = 1.425f;
    if (oldLevel == BotLodFull)
    {
        fullThreshold = 1.05f;
        mediumThreshold = 1.5f;
    }
    else if (oldLevel == BotLodMedium)
    {
        fullThreshold = 0.95f;
        mediumThreshold = 1.575f;
    }
    float speed = 0.0f;
    IPIMobile* const mobile = information->getMobile();
    if (mobile)
        speed = mobile->getSpeedMagnitude();
    for (std::map<int, PlayerLODStats>::iterator it = m_playerStats.begin();
         it != m_playerStats.end(); ++it)
    {
        const float x = it->second.m_position.x - position.x;
        const float y = it->second.m_position.y - position.y;
        const float z = it->second.m_position.z - position.z;
        float distance = std::sqrt(x * x + y * y + z * z) - speed * m_f36;
        if (it->second.m_lodDistance > 1.0e-7f)
            distance /= it->second.m_lodDistance;
        if (distance < fullThreshold)
        {
            bot->setLodLevel(BotLodFull);
            if (oldLevel != BotLodFull && object->isMobile() && object->isPhysical())
            {
                IAIObjectPhysical* const physical = object->getPhysical();
                if (!physical->hasPhysicsEnabled())
                    physical->enablePhysics(bot->getSide());
            }
            break;
        }
        if (bot->getLodLevel() > BotLodMedium && distance < mediumThreshold)
            bot->setLodLevel(BotLodMedium);
    }
    return true;
}

// ELF 0x39C110.
void AILODManager::removePlayer(int playerId)
{
    if (m_playerCursor == playerId)
        m_playerCursor = -1;
    m_playerStats.erase(playerId);
    m_updatedPlayerIds.erase(playerId);
}

} // namespace ai
} // namespace hfe
} // namespace dice
