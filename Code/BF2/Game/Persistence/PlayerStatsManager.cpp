// Path: [INFERRED]. Constructor ELF 0x82DEAA0 / PE 0x5C8490; operations

// Preserve duplicated keys and the original spelling/type quirks.
#include "PlayerStatsManager.h"
#include "../../Core/Ids.h"
#include "../../Core/StringUtil.h"
#include "../../External/GameSpy/ghttp/ghttp.h"
#include <cstdlib>
namespace dice { namespace hfe {
namespace {
// [CHOSEN, not recovered] Cross-platform OpenSpy policy, see #21. The reference images
// (ELF static init 0x82F53D0 / PE 0x76F7B0) used stage-net.game.bf2.us (ELF) and
// stage-net.gamespy.com (PE), not openspy.net.
const std::string playerInfoUrl = "http://stage-net.bf2hub.p.bf2.org/bf2/getplayerinfo.aspx?pid=";
}
GHTTPBool cbCompleted(GHTTPRequest, GHTTPResult result, char* body, GHTTPByteCount, void* data)
{
    IPlayerStatsManager* manager = static_cast<IPlayerStatsManager*>(data);
    if (result == GHTTPSuccess) manager->downloadCallback(body);
    else manager->downloadFailed();
    return GHTTPTrue;
}
void PlayerStatKey::set(const std::string& value)
{
    if (type == 0) numberValue = std::atoi(value.c_str());
    else if (type == 1) textValue = value;
}
PlayerStatsManager::PlayerStatsManager() : m_refCount(1), m_connected(false), m_updating(false)
{
    m_keys.push_back(PlayerStatKey("pID", "player id"));
    m_keys.push_back(PlayerStatKey("pID", "player id"));
    m_keys.push_back(PlayerStatKey("cID", "clanID"));
    m_keys.push_back(PlayerStatKey("t", "team"));
    m_keys.push_back(PlayerStatKey("m", "map"));
    m_keys.push_back(PlayerStatKey("gm", "game mode"));
    m_keys.push_back(PlayerStatKey("a", "round end army"));
    m_keys.push_back(PlayerStatKey("ctime", "time"));
    m_keys.push_back(PlayerStatKey("c", "complete"));
    m_keys.push_back(PlayerStatKey("h", "hostname", "undefined"));
    m_keys.push_back(PlayerStatKey("ip", "ip address"));
    m_keys.push_back(PlayerStatKey("v", "version"));
    m_keys.push_back(PlayerStatKey("kck", "times kicked"));
    m_keys.push_back(PlayerStatKey("ban", "times banned"));
    m_keys.push_back(PlayerStatKey("rs", "round end score"));
    m_keys.push_back(PlayerStatKey("cs", "commander score"));
    m_keys.push_back(PlayerStatKey("ts", "team score"));
    m_keys.push_back(PlayerStatKey("ss", "skill scoare"));
    m_keys.push_back(PlayerStatKey("ab", "badges"));
    m_keys.push_back(PlayerStatKey("am", "medals"));
    m_keys.push_back(PlayerStatKey("as", "end of round start"));
    m_keys.push_back(PlayerStatKey("ap", "purple hear"));
    m_keys.push_back(PlayerStatKey("tco", "time played as commander"));
    m_keys.push_back(PlayerStatKey("tsl", "time played as squad leader"));
    m_keys.push_back(PlayerStatKey("tsm", "time played as squad member"));
    m_keys.push_back(PlayerStatKey("tlw", "time played as lone wolf"));
    m_keys.push_back(PlayerStatKey("tk0", "time as anti-tank"));
    m_keys.push_back(PlayerStatKey("tk1", "time as assault"));
    m_keys.push_back(PlayerStatKey("tk2", "time as engineer"));
    m_keys.push_back(PlayerStatKey("tk3", "time as medic"));
    m_keys.push_back(PlayerStatKey("tk4", "time as spec op"));
    m_keys.push_back(PlayerStatKey("tk5", "time as support"));
    m_keys.push_back(PlayerStatKey("tk6", "time as sniper"));
    m_keys.push_back(PlayerStatKey("kk0", "kills as anti-tank"));
    m_keys.push_back(PlayerStatKey("kk1", "kills as assault"));
    m_keys.push_back(PlayerStatKey("kk2", "kills as engineer"));
    m_keys.push_back(PlayerStatKey("kk3", "kills as medic"));
    m_keys.push_back(PlayerStatKey("kk4", "kills as spec ops"));
    m_keys.push_back(PlayerStatKey("kk5", "kills as support"));
    m_keys.push_back(PlayerStatKey("kk6", "kills as sniper"));
    m_keys.push_back(PlayerStatKey("dk0", "deaths as anti-tank"));
    m_keys.push_back(PlayerStatKey("dk1", "deaths as assault"));
    m_keys.push_back(PlayerStatKey("dk2", "deaths as engineer"));
    m_keys.push_back(PlayerStatKey("dk3", "deaths as medic"));
    m_keys.push_back(PlayerStatKey("dk4", "deaths as spec ops"));
    m_keys.push_back(PlayerStatKey("dk5", "deaths as support"));
    m_keys.push_back(PlayerStatKey("dk6", "deaths as sniper"));
    m_keys.push_back(PlayerStatKey("tv0", "time in armor"));
    m_keys.push_back(PlayerStatKey("tv1", "time in aviator"));
    m_keys.push_back(PlayerStatKey("tv2", "time in air-defense"));
    m_keys.push_back(PlayerStatKey("tv3", "time in helicopter"));
    m_keys.push_back(PlayerStatKey("tv4", "time in transport"));
    m_keys.push_back(PlayerStatKey("tv5", "time in artillery"));
    m_keys.push_back(PlayerStatKey("tv6", "time in ground-def"));
    m_keys.push_back(PlayerStatKey("tvp", "time in parachute"));
    m_keys.push_back(PlayerStatKey("tnv", "time in night vision"));
    m_keys.push_back(PlayerStatKey("tgm", "time in gas mask"));
    m_keys.push_back(PlayerStatKey("kv0", "kills in armor"));
    m_keys.push_back(PlayerStatKey("kv1", "kills in aviator"));
    m_keys.push_back(PlayerStatKey("kv2", "kills in air-defense"));
    m_keys.push_back(PlayerStatKey("kv3", "kills in helicopter"));
    m_keys.push_back(PlayerStatKey("kv4", "kills in transport"));
    m_keys.push_back(PlayerStatKey("kv5", "kills in artillery"));
    m_keys.push_back(PlayerStatKey("kv6", "kills in ground-def"));
    m_keys.push_back(PlayerStatKey("kvr", "road kill"));
    m_keys.push_back(PlayerStatKey("bv0", "deaths by armor"));
    m_keys.push_back(PlayerStatKey("bv1", "deaths by aviator"));
    m_keys.push_back(PlayerStatKey("bv2", "deaths by air-def"));
    m_keys.push_back(PlayerStatKey("bv3", "deaths by helicopter"));
    m_keys.push_back(PlayerStatKey("bv4", "deaths by transport"));
    m_keys.push_back(PlayerStatKey("bv5", "deaths by artillery"));
    m_keys.push_back(PlayerStatKey("bv6", "deaths by ground-def"));
    m_keys.push_back(PlayerStatKey("ta0", "time as USA"));
    m_keys.push_back(PlayerStatKey("ta1", "time as MEC"));
    m_keys.push_back(PlayerStatKey("ta2", "time as Chinese"));
    m_keys.push_back(PlayerStatKey("ta3", "time as SEALS"));
    m_keys.push_back(PlayerStatKey("ta4", "time as SAS"));
    m_keys.push_back(PlayerStatKey("ta5", "time as Spetsnaz"));
    m_keys.push_back(PlayerStatKey("ta6", "time as MEC Special Forces"));
    m_keys.push_back(PlayerStatKey("ta7", "time as Rebels"));
    m_keys.push_back(PlayerStatKey("ta8", "time as Insurgent Forces"));
    m_keys.push_back(PlayerStatKey("ta9", "time as European Union"));
    m_keys.push_back(PlayerStatKey("tw0", "time with assault rifle"));
    m_keys.push_back(PlayerStatKey("tw1", "time with assault rifle w/gl"));
    m_keys.push_back(PlayerStatKey("tw2", "time with carbine"));
    m_keys.push_back(PlayerStatKey("tw3", "time with light mg"));
    m_keys.push_back(PlayerStatKey("tw4", "time with sniper rifle"));
    m_keys.push_back(PlayerStatKey("tw5", "time with pistol"));
    m_keys.push_back(PlayerStatKey("tw6", "time with at/aa"));
    m_keys.push_back(PlayerStatKey("tw7", "time with smg"));
    m_keys.push_back(PlayerStatKey("tw8", "time with shotgun"));
    m_keys.push_back(PlayerStatKey("kw0", "kills with assault rifle"));
    m_keys.push_back(PlayerStatKey("kw1", "kills with assault rifle w/gl"));
    m_keys.push_back(PlayerStatKey("kw2", "kills with carbine"));
    m_keys.push_back(PlayerStatKey("kw3", "kills with light mg"));
    m_keys.push_back(PlayerStatKey("kw4", "kills with sniper rifle"));
    m_keys.push_back(PlayerStatKey("kw5", "kills with pistol"));
    m_keys.push_back(PlayerStatKey("kw6", "kills with at/aa"));
    m_keys.push_back(PlayerStatKey("kw7", "kills with smg"));
    m_keys.push_back(PlayerStatKey("kw8", "kills with shotgun"));
    m_keys.push_back(PlayerStatKey("bw0", "deaths by assault rifle"));
    m_keys.push_back(PlayerStatKey("bw1", "deaths by assault rifle w/gl"));
    m_keys.push_back(PlayerStatKey("bw2", "deaths by carbine"));
    m_keys.push_back(PlayerStatKey("bw3", "deaths by light mg"));
    m_keys.push_back(PlayerStatKey("bw4", "deaths by sniper rifle"));
    m_keys.push_back(PlayerStatKey("bw5", "deaths by pistol"));
    m_keys.push_back(PlayerStatKey("bw6", "deaths by at/aa"));
    m_keys.push_back(PlayerStatKey("bw7", "deaths by smg"));
    m_keys.push_back(PlayerStatKey("bw8", "deaths by shotgun"));
    m_keys.push_back(PlayerStatKey("sw0", "shots with assault rifle"));
    m_keys.push_back(PlayerStatKey("sw1", "shots with assault rifle w/gl"));
    m_keys.push_back(PlayerStatKey("sw2", "shots with carbine"));
    m_keys.push_back(PlayerStatKey("sw3", "shots with light mg"));
    m_keys.push_back(PlayerStatKey("sw4", "shots with sniper rifle"));
    m_keys.push_back(PlayerStatKey("sw5", "shots with pistol"));
    m_keys.push_back(PlayerStatKey("sw6", "shots with at/aa"));
    m_keys.push_back(PlayerStatKey("sw7", "shots with smg"));
    m_keys.push_back(PlayerStatKey("sw8", "shots with shotgun"));
    m_keys.push_back(PlayerStatKey("hw0", "hits with assault rifle"));
    m_keys.push_back(PlayerStatKey("hw1", "hits with assault rifle w/gl"));
    m_keys.push_back(PlayerStatKey("hw2", "hits with carbine"));
    m_keys.push_back(PlayerStatKey("hw3", "hits with light mg"));
    m_keys.push_back(PlayerStatKey("hw4", "hits with sniper rifle"));
    m_keys.push_back(PlayerStatKey("hw5", "hits with pistol"));
    m_keys.push_back(PlayerStatKey("hw6", "hits with at/aa"));
    m_keys.push_back(PlayerStatKey("hw7", "hits with smg"));
    m_keys.push_back(PlayerStatKey("hw8", "hits with shotgun"));
    m_keys.push_back(PlayerStatKey("te0", "time with knife"));
    m_keys.push_back(PlayerStatKey("te1", "time with C4"));
    m_keys.push_back(PlayerStatKey("te2", "time with claymore"));
    m_keys.push_back(PlayerStatKey("te3", "time with handgrenade"));
    m_keys.push_back(PlayerStatKey("te4", "time with shockpaddles"));
    m_keys.push_back(PlayerStatKey("te5", "time with AT mine"));
    m_keys.push_back(PlayerStatKey("te6", "time with tactical"));
    m_keys.push_back(PlayerStatKey("te7", "time with grappling hook"));
    m_keys.push_back(PlayerStatKey("te8", "time with zip line"));
    m_keys.push_back(PlayerStatKey("ke0", "kills with knife"));
    m_keys.push_back(PlayerStatKey("ke1", "kills with C4"));
    m_keys.push_back(PlayerStatKey("ke2", "kills with claymore"));
    m_keys.push_back(PlayerStatKey("ke3", "kills with handgrenade"));
    m_keys.push_back(PlayerStatKey("ke4", "kills with shockpaddles"));
    m_keys.push_back(PlayerStatKey("ke5", "kills with AT mine"));
    m_keys.push_back(PlayerStatKey("ke6", "kills with tactical"));
    m_keys.push_back(PlayerStatKey("ke7", "kills with grappling hook"));
    m_keys.push_back(PlayerStatKey("ke8", "kills with zip line"));
    m_keys.push_back(PlayerStatKey("be0", "killed by knife"));
    m_keys.push_back(PlayerStatKey("be1", "killed by C4"));
    m_keys.push_back(PlayerStatKey("be2", "killed by claymore"));
    m_keys.push_back(PlayerStatKey("be3", "killed by handgrenade"));
    m_keys.push_back(PlayerStatKey("be4", "killed by shockpaddles"));
    m_keys.push_back(PlayerStatKey("be5", "killed by AT mine"));
    m_keys.push_back(PlayerStatKey("be8", "killed by zip line"));
    m_keys.push_back(PlayerStatKey("de6", "deployed tactical"));
    m_keys.push_back(PlayerStatKey("de7", "deployed grappling hook"));
    m_keys.push_back(PlayerStatKey("de8", "deployed zip line"));
    m_keys.push_back(PlayerStatKey("cpc", "cp capture"));
    m_keys.push_back(PlayerStatKey("cpa", "cp assist"));
    m_keys.push_back(PlayerStatKey("cpd", "cp defened"));
    m_keys.push_back(PlayerStatKey("ka", "kill assist"));
    m_keys.push_back(PlayerStatKey("he", "heal"));
    m_keys.push_back(PlayerStatKey("riv", "revive"));
    m_keys.push_back(PlayerStatKey("rsp", "resupply"));
    m_keys.push_back(PlayerStatKey("tre", "target enemy"));
    m_keys.push_back(PlayerStatKey("drs", "driver special"));
    m_keys.push_back(PlayerStatKey("drka", "driver kill assist"));
    m_keys.push_back(PlayerStatKey("tmkl", "team kill"));
    m_keys.push_back(PlayerStatKey("tmdg", "team damage"));
    m_keys.push_back(PlayerStatKey("tmvd", "team vehicle damage"));
    m_keys.push_back(PlayerStatKey("su", "suicide"));
    m_keys.push_back(PlayerStatKey("ks", "kill streak"));
    m_keys.push_back(PlayerStatKey("ds", "death streak"));
    m_keys.push_back(PlayerStatKey("mvns", "my victim name"));
    m_keys.push_back(PlayerStatKey("mvks", "my victim kills"));
    m_keys.push_back(PlayerStatKey("vmns", "victimized me name"));
    m_keys.push_back(PlayerStatKey("vmks", "victimized me kills"));
    m_query = "&getarg=";
    for (unsigned int i = 0; i < m_keys.size() - 1; ++i) {
        m_query += m_keys[i].wireCode;
        m_query += ",";
    }
    m_query += m_keys[m_keys.size() - 1].wireCode;
}
PlayerStatsManager::~PlayerStatsManager() { disconnect(); }
IBase* PlayerStatsManager::create(unsigned int iid, IBase* outer)
{
    if (outer) return 0;
    PlayerStatsManager* object = new PlayerStatsManager();
    IBase* result = static_cast<IBase*>(object->queryInterface(iid));
    if (!result) object->release();
    return result;
}
void PlayerStatsManager::addRef() { ++m_refCount; }
int PlayerStatsManager::getRef() { return m_refCount; }
int PlayerStatsManager::release()
{
    const int remaining = --m_refCount;
    if (!remaining) delete this;
    return remaining;
}
void* PlayerStatsManager::queryInterface(unsigned int iid) const
{
    return iid == IID_IBase || iid == IID_IPlayerStatsManager ? const_cast<PlayerStatsManager*>(this) : 0;
}
bool PlayerStatsManager::connect()
{
    ghttpStartup();
    m_connected = true;
    return true;
}
void PlayerStatsManager::disconnect()
{
    if (m_connected) {
        ghttpCleanup();
        m_connected = false;
    }
    m_updating = false;
}
bool PlayerStatsManager::isConnected() { return m_connected; }
bool PlayerStatsManager::isUpdating() { return m_updating; }
bool PlayerStatsManager::update()
{
    if (!isConnected() || isUpdating()) return false;
    const std::string url = playerInfoUrl + m_profile + m_query;
    if (ghttpGetA(url.c_str(), GHTTPFalse, cbCompleted, this) < 0) return false;
    m_updating = true;
    return true;
}
void PlayerStatsManager::downloadCallback(const char* body)
{
    m_updating = false;
    const std::string text(body);
    std::string unused;
    std::string::size_type end = text.find(',', 0);
    if (end == std::string::npos) return;
    std::string::size_type start = 0;
    for (unsigned int i = 0; i < m_keys.size(); ++i) {
        m_keys[i].set(text.substr(start, end - start));
        start = end + 1;
        end = text.find(',', end + 1);
    }
}
void PlayerStatsManager::downloadFailed() { m_updating = false; }
PlayerStatKey* PlayerStatsManager::findKey(const std::string& name)
{
    for (unsigned int i = 0; i < m_keys.size(); ++i)
        // Both ports return the first NON-matching wire code. Preserve the defect.
        if (compareIgnoreCase(m_keys[i].wireCode, name)) return &m_keys[i];
    return 0;
}
std::vector<PlayerStatKey>& PlayerStatsManager::getKeys() { return m_keys; }
} }
