// Refreshes browser query fields from live server, mod, and player state.

#include <cstring>
#include <list>

#include "ServerQueryCache.h"

#include "../../IGame.h"                      // game, getCurrentMapName / GameMode / Size
#include "../../IGameLogic.h"                 // gameLogic
#include "../../IGameServer.h"                // IGameServer client counts + useContentChecking
#include "../../GameServer/ServerSettings.h"  // serverSettings
#include "../../Utility/BuildNrUtil.h"        // getModName / getBuildNrString
#include "../../Objects/IPlayer.h"            // IPlayer accessors
#include "../../../Scene/Object/PlayerManager.h"  // playerManager, getPlayersSortedByScore
#include "../../../Core/StringUtil.h"         // formatString
#include "../../../Core/Ids.h"                // IID_IGameServer
#include "../../../Core/ClassManager.h"
#include "../../../Scene/Object/PlayerScoreData.h"
#include "../../IModInfo.h"
#include "../../MapList.h"
#include "../../Main/BF2Engine.h"

namespace dice {
namespace hfe {

ServerQueryCache* serverQueryCache = 0;
extern ClassManager* classManager;
extern BF2Engine* bf2Engine;
namespace ai { extern IBase* g_aiInstance; }

// the 45 server strings and the 64x8 player strings default-construct empty; index 1 is
// the build number and index 2 the OS, seeded once here. numPlayers 0, no mod.
ServerQueryCache::ServerQueryCache()
    : m_numPlayers(0)
    , m_modInfo(0)
{
    m_serverKeys[SQK_buildNr] = BuildNrUtil::getBuildNrString();
    m_serverKeys[SQK_os]      = getOS();
}

ServerQueryCache::~ServerQueryCache()
{
}

const char* ServerQueryCache::getServerKey(ServerQueryKey serverQueryKey) const
{
    if (static_cast<unsigned int>(serverQueryKey) <= 44)
        return m_serverKeys[serverQueryKey].c_str();
    return 0;
}

int ServerQueryCache::getNumPlayers() const
{
    return m_numPlayers;
}

const char* ServerQueryCache::getPlayerKey(int player, PlayerQueryKey playerQueryKey) const
{
    if (static_cast<unsigned int>(playerQueryKey) <= 7 && static_cast<unsigned int>(player) <= 63)
        return m_playerInfo[player].keys[playerQueryKey].c_str();
    return 0;
}

// update - ELF 0x80C0090, 5,188 B. Rebuild the whole browser snapshot from live game state.
void ServerQueryCache::update()
{
    if (!m_modInfo)
        m_modInfo = static_cast<IModInfo*>(classManager->getSingleton(std::string("ModInfo")));
    if (!m_modInfo) return;
    std::string mapName;
    bool usesPlasma = false;
    for (int i = 0; i < mapList->getMapCount(); ++i) {
        const std::string name = mapList->getEntry(i)->name;
        const IModInfo::Map* map = m_modInfo->findMapFromCurrentMod(name);
        if (map && map->plasma) {
            usesPlasma = true;
            break;
        }
    }
    const IModInfo::Map* currentMap = m_modInfo->findMapFromCurrentMod(game->getCurrentMapName());
    if (currentMap) mapName = currentMap->name;
    ServerSettings* const sv = serverSettings;

    m_serverKeys[SQK_serverName]        = getServerName();
    m_serverKeys[SQK_currentMapId]      = mapName;
    m_serverKeys[SQK_currentMapGameMode]= game->getCurrentMapGameMode();
    m_serverKeys[SQK_modName]           = getModName();
    m_serverKeys[SQK_serverPort]        = formatString("%d", sv->m_serverPort);
    m_serverKeys[SQK_usesPlasma]        = formatString("%d", usesPlasma);

    int allClients = 0;
    IGameServer* const gs = game ? static_cast<IGameServer*>(game->queryInterface(IID_IGameServer)) : 0;
    if (gs)
        allClients = gs->getNumberOfConnectedClients() + gs->getNumberOfConnectingClients();
    world::playerManager->getLocalHumanPlayer(); // return ignored, both ports
    m_serverKeys[SQK_numberOfAllClients]= formatString("%d", allClients);

    m_serverKeys[SQK_maxPlayers]        = formatString("%d", sv->getMaxPlayers());
    m_serverKeys[SQK_scoreLimit]        = formatString("%d", sv->m_scoreLimit);
    m_serverKeys[SQK_roundsPerMap]      = formatString("%d", sv->m_roundsPerMap);
    m_serverKeys[SQK_fixme]             = "fixme";
    m_serverKeys[SQK_timeLimit]         = formatString("%d", sv->m_timeLimit);
    m_serverKeys[SQK_timeUntilRestartMap] = formatString("%f", gameLogic->getTimeUntilRestartMap());
    m_serverKeys[SQK_hasPassword]       = sv->getPassword().length() ? "1" : "0";
    m_serverKeys[SQK_isRanked]          = (sv->getRanked() == true) ? "1" : "0";
    m_serverKeys[SQK_hardcoded1]        = "1";
    m_serverKeys[SQK_hasPunkbuster]     = (sv->getSvPunkBuster() == true) ? "1" : "0";
    m_serverKeys[SQK_autoRecording]     = (sv->m_autoRecord == true) ? "1" : "0";
    m_serverKeys[SQK_demoIndexURL]      = sv->m_demoIndexURL;
    m_serverKeys[SQK_demoDownloadURL]   = sv->m_demoDownloadURL;
    m_serverKeys[SQK_teamName1]         = gameLogic->getTeamName(1);
    m_serverKeys[SQK_teamName2]         = gameLogic->getTeamName(2);
    m_serverKeys[SQK_hasBots]           = ai::g_aiInstance ? "1" : "0";
    // The second query and missing null guard are present in both shipped images.
    IGameServer* const contentServer = game ? static_cast<IGameServer*>(game->queryInterface(IID_IGameServer)) : 0;
    m_serverKeys[SQK_useContentChecking]= contentServer->useContentChecking() == true ? "1" : "0";
    m_serverKeys[SQK_currentMapSize]    = formatString("%d", game->getCurrentMapSize());
    m_serverKeys[SQK_useGlobalUnlocks]  = (sv->getUseGlobalUnlocks() == true) ? "1" : "0";
    m_serverKeys[SQK_avgFps]            = formatString("%f", bf2Engine->getAverageFPS());

    bool voipEnabled = false;
    unsigned int voipAddr = 0; unsigned short voipPort = 0, voipBfPort = 0; int voipQuality = 0;
    gameLogic->getVoipServerInfo(voipEnabled, voipAddr, voipPort, voipBfPort, voipQuality);
    m_serverKeys[SQK_hasVoipServerInfo] = voipEnabled ? "1" : "0";

    m_serverKeys[SQK_autoBalance]       = (sv->m_autoBalanceTeam == true) ? "1" : "0";
    m_serverKeys[SQK_friendlyFireEnabled] = (sv->m_soldierFriendlyFire > 99) ? "1" : "0";
    m_serverKeys[SQK_punish]            = (sv->m_tkPunishEnabled == true) ? "Punish" : "No Punish";
    m_serverKeys[SQK_startDelay]        = formatString("%d", sv->m_startDelay);
    m_serverKeys[SQK_spawnTime]         = formatString("%f", sv->getSpawnTime());
    m_serverKeys[SQK_sponsorText]       = sv->m_sponsorText;
    m_serverKeys[SQK_sponsorLogoURL]    = sv->m_sponsorLogoURL;
    m_serverKeys[SQK_communityLogoURL]  = sv->m_communityLogoURL;
    m_serverKeys[SQK_ticketRatio]       = formatString("%d", sv->getTicketRatio());
    m_serverKeys[SQK_teamRatioPercent]  = formatString("%f", sv->getTeamRatioPercent());
    m_serverKeys[SQK_numReservedSlots]  = formatString("%d", sv->getNumReservedSlots());
    // [CHOSEN, not recovered] Owner-approved R-main-1 improvement: retail leaves
    // these registered bot keys empty.
    m_serverKeys[SQK_coopBotRatio]      = formatString("%f", sv->getCoopBotRatio());
    m_serverKeys[SQK_coopBotCount]      = formatString("%d", sv->getCoopBotCount());
    m_serverKeys[SQK_coopBotDifficulty] = formatString("%f", sv->getCoopBotDifficulty());
    m_serverKeys[SQK_noVehicles]        = (sv->getNoVehicles() == true) ? "1" : "0";

    // the player rows, sorted by score, capped at 64
    int playerCount = 0;
    const std::list<world::IPlayer*> players = world::playerManager->getPlayersSortedByScore();
    for (std::list<world::IPlayer*>::const_iterator it = players.begin();
         it != players.end() && playerCount <= 63; ++it, ++playerCount)
    {
        world::IPlayer* const player = *it;
        const world::PlayerScoreData* const sd = player->getScoreData();
        PlayerInfo& row = m_playerInfo[playerCount];
        row.keys[PQK_playerName] = player->getName();
        row.keys[PQK_ping]       = formatString("%d", player->getPing());
        row.keys[PQK_team]       = formatString("%d", player->getTeam());
        row.keys[PQK_totalScore] = formatString("%d", sd->m_score);
        row.keys[PQK_someScore1] = formatString("%d", sd->m_field14);
        // [PLATFORM] PE 0x615830 reads +0x10; ELF 0x80C0090 reads +0x18.
#ifdef _WIN32
        row.keys[PQK_someScore2] = formatString("%d", sd->m_field10);
#else
        row.keys[PQK_someScore2] = formatString("%d", sd->m_field18);
#endif
        row.keys[PQK_profileId]  = formatString("%d", player->getProfileId());
        row.keys[PQK_isAIPlayer] = formatString("%d", player->getIsAIPlayer());
    }
    m_numPlayers = playerCount;
}

} // namespace hfe
} // namespace dice
