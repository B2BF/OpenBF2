// Path: [INFERRED]. See StatsManager.h for the paired evidence.
#include "StatsManager.h"
#include "../../Core/Ids.h"
#include "../../Core/StringUtil.h"
#include "../IGame.h"
#include "../GameServer/ServerSettings.h"
#include "../MatchMaking/Server/MatchMakingServer.h"
#include "../../External/GameSpy/gstats/gstats.h"
#include "../../External/GameSpy/gstats/gpersist.h"
#include <cstring>
namespace dice { namespace hfe {
namespace gamespy { const char* getSecretKey(); }
StatsManager::StatsManager() : m_refCount(1), m_game(0) {}
StatsManager::~StatsManager() { disconnect(); }
IBase* StatsManager::create(unsigned int iid, IBase* outer)
{
    if (outer) return 0;
    StatsManager* object = new StatsManager();
    IBase* result = static_cast<IBase*>(object->queryInterface(iid));
    if (!result) object->release();
    return result;
}
void StatsManager::addRef() { ++m_refCount; }
int StatsManager::getRef() { return m_refCount; }
int StatsManager::release()
{
    const int remaining = --m_refCount;
    if (!remaining) delete this;
    return remaining;
}
void* StatsManager::queryInterface(unsigned int iid) const
{
    return iid == IID_IBase || iid == IID_IStatsManager ? const_cast<StatsManager*>(this) : 0;
}
bool StatsManager::connect()
{
    const int port = serverSettings->m_gameSpyPort;
    std::strcpy(gcd_gamename, gamespy::g_gameName);
    std::strcpy(gcd_secret_key, gamespy::getSecretKey());
    std::strcpy(StatsServerHostname, "gamestats.bf2hub.p.bf2.org"); // gamespy.com|openspy.net
    return InitStatsConnection(port) == 0;
}
void StatsManager::disconnect()
{
    if (m_game && isConnected()) {
        FreeGame(m_game);
        m_game = 0;
    }
    // Retail repeats the virtual connection read and leaves the challenge unchanged.
    if (isConnected()) CloseStatsConnection();
}
bool StatsManager::newGame()
{
    if (!isConnected()) return false;
    m_game = NewGame(0);
    m_challenge = GetChallenge(m_game);
    return true;
}
bool StatsManager::endGame()
{
    if (!isConnected()) return false;
    FreeGame(m_game);
    m_game = 0;
    m_challenge = "";
    return true;
}
bool StatsManager::isConnected() { return IsStatsConnected() != 0; }
const std::string& StatsManager::getChallenge() { return m_challenge; }
statsgame_s* StatsManager::getGame() const { return m_game; }
bool StatsManager::think() { return PersistThink() != 0; }
bool StatsManager::sendSnapshot(const std::string& snapshot)
{
    const std::string mode = game->getCurrentMapGameMode();
    // Observed defect, both ports: ONLY gpm_coop gets past this comparison.
    if (compareIgnoreCase(mode, std::string("gpm_coop"))) return false;
    if (!isConnected() && InitStatsConnection(serverSettings->m_gameSpyPort)) return false;
    return SendGameSnapShotA(getGame(), snapshot.c_str(), SNAP_UPDATE) == 0;
}
bool StatsManager::sendFinal(const std::string& snapshot)
{
    if (!isConnected() && InitStatsConnection(serverSettings->m_gameSpyPort)) return false;
    return SendGameSnapShotA(getGame(), snapshot.c_str(), SNAP_FINAL) == 0;
}
bool StatsManager::authenticatePlayer(world::IPlayer*) { return isConnected() != 0; }
std::string StatsManager::getErrorString(int error) const
{
    switch (error) {
    case 1: return "unable to create data socket";
    case 2: return "unable to resolve DNS";
    case 3: return "unable to connect to stats server";
    case 5: return "unable to recieve challenge from stats server, or bad challenge";
    default: return "unknown error";
    }
}
} }
