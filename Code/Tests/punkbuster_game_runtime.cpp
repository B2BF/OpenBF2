// [TEMP] [CHOSEN, not recovered] The BF2-side PunkBuster functions (Game/PunkBuster/PbGame.cpp, BF2.exe
// 0x4D94D0..0x4DA49B, ELF 0x84F7340..0x84F8A90) against fake games, console, players, glue entry points
// and helpers, in both images (CTests bf2_punkbuster_game_check, client, and
// bf2_punkbuster_game_dedicated_check). The fakes of the large interfaces and classes are complete
// (generated from their headers) and fail loudly for every call the functions under test must not make.
// FileManager, PlayerManager and SettingsRepostitory get every virtual defined here, so the fixture can
// construct them without the file system, scene or settings TUs.
#include "../BF2/Game/PunkBuster/PbGlue.h"
#include "../BF2/Game/IGame.h"
#include "../BF2/Game/IGameClient.h"
#include "../BF2/Game/IGameServer.h"
#include "../BF2/Game/MapInfo.h"
#include "../BF2/Game/Objects/IPlayer.h"
#include "../BF2/Game/GameServer/ServerSettings.h"
#include "../BF2/Game/Utility/BuildNrUtil.h"
#include "../BF2/Core/Ids.h"
#include "../BF2/Core/SettingsRepostitory.h"
#include "../BF2/IO/Console/Console.h"
#include "../BF2/IO/File/FileManager.h"
#include "../BF2/IO/Network/NetworkLayer/Socket/UDPSocket.h"
#include "../BF2/Scene/Object/PlayerManager.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <unistd.h>

using namespace dice::hfe;

static unsigned checks, failures;
static void check(bool value, const std::string& label)
{
    ++checks;
    if (!value) { ++failures; std::printf("FAIL %s\n", label.c_str()); }
}
[[noreturn]] static void unexpected(const char* name)
{
    std::fprintf(stderr, "[TEMP] punkbuster_game check: unexpected call %s\n", name);
    std::fflush(stderr);
    std::abort();
}
static std::vector<std::string> calls;
static std::string joined()
{
    std::string r;
    for (const std::string& c : calls) r += c + ";";
    return r;
}
static std::string bytes(const void* data, int length) { return std::string(static_cast<const char*>(data), length); }

// ---- globals PbGame.cpp reads ------------------------------------------------------------------
namespace dice { namespace hfe {
IGame* game = nullptr;
SettingsRepostitory* settings = nullptr;
ServerSettings* serverSettings = nullptr;
namespace io { FileManager* fileManager = nullptr; OldConsole* mainConsole = nullptr; }
namespace world { PlayerManager* playerManager = nullptr; }
namespace gamespy { int g_gameID = 1059; }
// Test definitions of the two ServerSettings members PB_handlePacket / PB_examineSetting call.
static int s_maxPlayers = 4;
static bool s_svPunkBuster = true;
int ServerSettings::getMaxPlayers() const { calls.push_back("maxPlayers"); return s_maxPlayers; }
bool ServerSettings::getSvPunkBuster() const { return s_svPunkBuster; }
namespace BuildNrUtil { const char* getBuildNrString() { calls.push_back("buildNr"); return "1.5.3153-802.0"; } }
namespace io {
static bool s_resolves = true;
int UDPSocket::nameToAddress(const std::string& name, Address* address, std::list<Address>* all)
{
    calls.push_back("resolve " + name + (all ? " list" : "") + (address->length == 0x10 ? "" : " badlen"));
    return s_resolves ? 0 : -1;
}
bool UDPSocket::UDPaddressToString(const Address&, std::string* out) { *out = "10.1.2.3:0"; return true; }
}
} }

extern "C" char* gcd_getkeyhash(int gameid, int localid)
{
    static std::string hash;
    hash = "hash" + std::to_string(gameid) + "-" + std::to_string(localid);
    return const_cast<char*>(hash.c_str());
}

// ---- PlayerManager and SettingsRepostitory: every virtual defined here (generated from the headers),
// failing loudly except the lookups under test ----------------------------------------------------
namespace dice { namespace hfe {
namespace world {
PlayerManager::PlayerManager() {}
static IPlayer* s_players[4];
static IPlayer* s_localPlayer;
IPlayer* PlayerManager::getPlayerFromId(int id)
{
    calls.push_back("playerFromId " + std::to_string(id));
    return id >= 0 && id < 4 ? s_players[id] : nullptr;
}
IPlayer* PlayerManager::getLocalHumanPlayer() { return s_localPlayer; }
PlayerManager::~PlayerManager() { unexpected("PlayerManager::~PlayerManager"); }
void PlayerManager::addRef() { unexpected("PlayerManager::addRef"); }
int PlayerManager::getRef() { unexpected("PlayerManager::getRef"); }
int PlayerManager::release() { unexpected("PlayerManager::release"); }
void* PlayerManager::queryInterface(unsigned int iid) const { unexpected("PlayerManager::queryInterface"); }
void PlayerManager::addPlayer(IPlayer* player) { unexpected("PlayerManager::addPlayer"); }
void PlayerManager::removePlayer(IPlayer* player) { unexpected("PlayerManager::removePlayer"); }
void PlayerManager::removePlayerId(int id) { unexpected("PlayerManager::removePlayerId"); }
IPlayer* PlayerManager::getPlayerFromName(const std::string& name) { unexpected("PlayerManager::getPlayerFromName"); }
IPlayer* PlayerManager::getNextPlayerFromId(int id) { unexpected("PlayerManager::getNextPlayerFromId"); }
IPlayer* PlayerManager::getPlayerFromProfileId(int profileId) { unexpected("PlayerManager::getPlayerFromProfileId"); }
IPlayer* PlayerManager::getPlayerFromIdString(const std::string& id) { unexpected("PlayerManager::getPlayerFromIdString"); }
void PlayerManager::setLocalHumanPlayer(IPlayer* player) { unexpected("PlayerManager::setLocalHumanPlayer"); }
void PlayerManager::setLocalHumanPlayer(int id) { unexpected("PlayerManager::setLocalHumanPlayer"); }
const std::list<IPlayer*>& PlayerManager::getPlayers() { unexpected("PlayerManager::getPlayers"); }
const std::list<IPlayer*>& PlayerManager::getPlayersSortedByScore() { unexpected("PlayerManager::getPlayersSortedByScore"); }
const std::list<IPlayer*>& PlayerManager::getPlayersSortedByRank() { unexpected("PlayerManager::getPlayersSortedByRank"); }
const std::list<IPlayer*>& PlayerManager::getPlayers(int team) { unexpected("PlayerManager::getPlayers"); }
const std::list<IPlayer*>& PlayerManager::getPlayersSortedByScore(int team) { unexpected("PlayerManager::getPlayersSortedByScore"); }
const std::list<IPlayer*>& PlayerManager::getPlayersSortedByRank(int team) { unexpected("PlayerManager::getPlayersSortedByRank"); }
const std::list<IPlayer*>& PlayerManager::getPlayersInSquad(int team, int squad) { unexpected("PlayerManager::getPlayersInSquad"); }
const std::list<IPlayer*>& PlayerManager::getPlayersInSquadSortedByScore(int team, int squad) { unexpected("PlayerManager::getPlayersInSquadSortedByScore"); }
const std::list<IPlayer*>& PlayerManager::getPlayersInSquadSortedByRank(int team, int squad) { unexpected("PlayerManager::getPlayersInSquadSortedByRank"); }
const std::list<IPlayer*>& PlayerManager::getTeamSquadLeaders(int team) { unexpected("PlayerManager::getTeamSquadLeaders"); }
void PlayerManager::updatePlayers(float deltaTime) { unexpected("PlayerManager::updatePlayers"); }
void PlayerManager::registerPlayerClass(unsigned int classId, const std::string& className) { unexpected("PlayerManager::registerPlayerClass"); }
void PlayerManager::getPlayerClasses(std::list<std::pair<unsigned int, std::string> >& out) const { unexpected("PlayerManager::getPlayerClasses"); }
const std::string& PlayerManager::getClassNameFromPlayerClassID(unsigned int classId) const { unexpected("PlayerManager::getClassNameFromPlayerClassID"); }
IPlayer* PlayerManager::createPlayer(unsigned int classId, const std::string& name, int id) { unexpected("PlayerManager::createPlayer"); }
IPlayer* PlayerManager::createPlayer(const std::string& className, const std::string& name, int id) { unexpected("PlayerManager::createPlayer"); }
bool PlayerManager::saveAllPlayers(const std::string& fileName) { unexpected("PlayerManager::saveAllPlayers"); }
IPlayer* PlayerManager::getActivePlayer() const { unexpected("PlayerManager::getActivePlayer"); }
void PlayerManager::setActivePlayer(IPlayer* player) { unexpected("PlayerManager::setActivePlayer"); }
int PlayerManager::getNumberOfPlayers() const { unexpected("PlayerManager::getNumberOfPlayers"); }
int PlayerManager::getNumberOfHumanPlayers() const { unexpected("PlayerManager::getNumberOfHumanPlayers"); }
int PlayerManager::getNumberOfAliveHumanPlayers() const { unexpected("PlayerManager::getNumberOfAliveHumanPlayers"); }
int PlayerManager::getNrOfPlayerInTeam(int team) { unexpected("PlayerManager::getNrOfPlayerInTeam"); }
int PlayerManager::getNrOfPlayerInSquads(int team) { unexpected("PlayerManager::getNrOfPlayerInSquads"); }
int PlayerManager::getNrOfPlayersInSquad(int team, int squad) { unexpected("PlayerManager::getNrOfPlayersInSquad"); }
const std::list<IPlayer*>& PlayerManager::getInvitedPlayers(int team, int squad) { unexpected("PlayerManager::getInvitedPlayers"); }
int PlayerManager::getNrOfActiveSquads(int team) { unexpected("PlayerManager::getNrOfActiveSquads"); }
int PlayerManager::getTeamToAddPlayer(float team1Weight, float team2Weight) { unexpected("PlayerManager::getTeamToAddPlayer"); }
bool PlayerManager::deactivatePlayer(int id) { unexpected("PlayerManager::deactivatePlayer"); }
bool PlayerManager::reactivatePlayer(int id) { unexpected("PlayerManager::reactivatePlayer"); }
bool PlayerManager::renamePlayer(IPlayer* player, std::string name) { unexpected("PlayerManager::renamePlayer"); }
}
SettingsRepostitory::SettingsRepostitory() {}
SettingsRepostitory::~SettingsRepostitory() { unexpected("SettingsRepostitory::~SettingsRepostitory"); }
static std::string s_pbPathSetting;
const std::string& SettingsRepostitory::stringGet(const std::string& key, const std::string& fallback)
{
    calls.push_back("stringGet " + key + " [" + fallback + "]");
    return s_pbPathSetting.empty() ? fallback : s_pbPathSetting;
}
void SettingsRepostitory::addRef() { unexpected("SettingsRepostitory::addRef"); }
int SettingsRepostitory::getRef() { unexpected("SettingsRepostitory::getRef"); }
int SettingsRepostitory::release() { unexpected("SettingsRepostitory::release"); }
void* SettingsRepostitory::queryInterface(unsigned int) const { unexpected("SettingsRepostitory::queryInterface"); }
const int& SettingsRepostitory::intGet(const std::string&, const int&) { unexpected("SettingsRepostitory::intGet"); }
void SettingsRepostitory::intSet(const std::string&, const int&) { unexpected("SettingsRepostitory::intSet"); }
bool SettingsRepostitory::intExists(const std::string&) { unexpected("SettingsRepostitory::intExists"); }
void SettingsRepostitory::intErase(const std::string&) { unexpected("SettingsRepostitory::intErase"); }
const float& SettingsRepostitory::floatGet(const std::string&, const float&) { unexpected("SettingsRepostitory::floatGet"); }
void SettingsRepostitory::floatSet(const std::string&, const float&) { unexpected("SettingsRepostitory::floatSet"); }
bool SettingsRepostitory::floatExists(const std::string&) { unexpected("SettingsRepostitory::floatExists"); }
void SettingsRepostitory::floatErase(const std::string&) { unexpected("SettingsRepostitory::floatErase"); }
const unsigned int& SettingsRepostitory::U32Get(const std::string&, const unsigned int&) { unexpected("SettingsRepostitory::U32Get"); }
void SettingsRepostitory::U32Set(const std::string&, const unsigned int&) { unexpected("SettingsRepostitory::U32Set"); }
bool SettingsRepostitory::U32Exists(const std::string&) { unexpected("SettingsRepostitory::U32Exists"); }
void SettingsRepostitory::U32Erase(const std::string&) { unexpected("SettingsRepostitory::U32Erase"); }
void SettingsRepostitory::stringSet(const std::string&, const std::string&) { unexpected("SettingsRepostitory::stringSet"); }
bool SettingsRepostitory::stringExists(const std::string&) { unexpected("SettingsRepostitory::stringExists"); }
void SettingsRepostitory::stringErase(const std::string&) { unexpected("SettingsRepostitory::stringErase"); }
const bool& SettingsRepostitory::boolGet(const std::string&, const bool&) { unexpected("SettingsRepostitory::boolGet"); }
void SettingsRepostitory::boolSet(const std::string& key, const bool& value)
{
    calls.push_back("boolSet " + key + " " + (value ? "1" : "0"));
}
bool SettingsRepostitory::boolExists(const std::string&) { unexpected("SettingsRepostitory::boolExists"); }
void SettingsRepostitory::boolErase(const std::string&) { unexpected("SettingsRepostitory::boolErase"); }
const Vec3& SettingsRepostitory::Vec3Get(const std::string&, const Vec3&) { unexpected("SettingsRepostitory::Vec3Get"); }
void SettingsRepostitory::Vec3Set(const std::string&, const Vec3&) { unexpected("SettingsRepostitory::Vec3Set"); }
bool SettingsRepostitory::Vec3Exists(const std::string&) { unexpected("SettingsRepostitory::Vec3Exists"); }
void SettingsRepostitory::Vec3Erase(const std::string&) { unexpected("SettingsRepostitory::Vec3Erase"); }
const Vec4& SettingsRepostitory::Vec4Get(const std::string&, const Vec4&) { unexpected("SettingsRepostitory::Vec4Get"); }
void SettingsRepostitory::Vec4Set(const std::string&, const Vec4&) { unexpected("SettingsRepostitory::Vec4Set"); }
bool SettingsRepostitory::Vec4Exists(const std::string&) { unexpected("SettingsRepostitory::Vec4Exists"); }
void SettingsRepostitory::Vec4Erase(const std::string&) { unexpected("SettingsRepostitory::Vec4Erase"); }
} }

// ---- FileManager (and IFileSystem's two non-pure slots): every virtual defined here, failing loudly
// except fileExists, which answers from the set of files the test says exist -------------------------
namespace dice { namespace hfe { namespace io {
static std::vector<std::string> s_existingFiles;
FileManager::FileManager() {}
FileManager::~FileManager() { unexpected("FileManager::~FileManager"); }
bool FileManager::fileExists(const std::string& name)
{
    calls.push_back("exists " + name);
    for (const std::string& f : s_existingFiles) if (f == name) return true;
    return false;
}
void IFileSystem::init() { unexpected("IFileSystem::init"); }
bool IFileSystem::isLastFileOpenedFromArchive() const { unexpected("IFileSystem::isLastFileOpenedFromArchive"); }
void FileManager::addRef() { unexpected("FileManager::addRef"); }
int FileManager::getRef() { unexpected("FileManager::getRef"); }
int FileManager::release() { unexpected("FileManager::release"); }
void* FileManager::queryInterface(unsigned int iid) const { unexpected("FileManager::queryInterface"); }
bool FileManager::isLastFileOpenedFromArchive() const { unexpected("FileManager::isLastFileOpenedFromArchive"); }
IStream* FileManager::open(const std::string& name, FileMode, FileAccess, unsigned int) { unexpected("FileManager::open"); }
bool FileManager::deleteFile(const std::string&) { unexpected("FileManager::deleteFile"); }
bool FileManager::moveFile(const std::string& from, const std::string& to) { unexpected("FileManager::moveFile"); }
bool FileManager::copyFile(const std::string& from, const std::string& to, bool overwrite) { unexpected("FileManager::copyFile"); }
bool FileManager::readFile(const std::string& name, void* buffer, unsigned int size, unsigned int& bytesRead) { unexpected("FileManager::readFile"); }
bool FileManager::writeFile(const std::string& name, const void* buffer, unsigned int size, bool append) { unexpected("FileManager::writeFile"); }
int FileManager::findFiles(const std::string& path, bool recursive, std::list<FileInfo>& out) { unexpected("FileManager::findFiles"); }
bool FileManager::getFileInfo(const std::string& name, FileInfo& out) { unexpected("FileManager::getFileInfo"); }
bool FileManager::compareFileTime(const std::string& a, const std::string& b) { unexpected("FileManager::compareFileTime"); }
bool FileManager::getFullFileName(std::string&) { unexpected("FileManager::getFullFileName"); }
void FileManager::update() { unexpected("FileManager::update"); }
const std::string& FileManager::getHomeSubstitution() const { unexpected("FileManager::getHomeSubstitution"); }
const std::string& FileManager::getRootSubstitution() const { unexpected("FileManager::getRootSubstitution"); }
void FileManager::setBaseSystem(IFileSystem*) { unexpected("FileManager::setBaseSystem"); }
IFileSystem* FileManager::getBaseSystem() { unexpected("FileManager::getBaseSystem"); }
IStream* FileManager::openwin32(const std::string& name, FileMode, FileAccess, unsigned int) { unexpected("FileManager::openwin32"); }
void FileManager::addArchiveHandler(IArchiveHandler*) { unexpected("FileManager::addArchiveHandler"); }
void FileManager::removeArchiveHandler(IArchiveHandler*) { unexpected("FileManager::removeArchiveHandler"); }
bool FileManager::addPathOverload(const std::string& from, const std::string& to) { unexpected("FileManager::addPathOverload"); }
bool FileManager::removePathOverload(const std::string& from, const std::string& to) { unexpected("FileManager::removePathOverload"); }
bool FileManager::addMergeDirectory(const std::string&, const std::string&) { unexpected("FileManager::addMergeDirectory"); }
bool FileManager::removeMergeDirectory(const std::string&) { unexpected("FileManager::removeMergeDirectory"); }
IArchive* FileManager::openArchive(const std::string& file, int unused, bool flag) { unexpected("FileManager::openArchive"); }
void FileManager::permitMountArchives(bool) { unexpected("FileManager::permitMountArchives"); }
IArchive* FileManager::mountArchive(const std::string& archiveName, const std::string& mountPath, bool contentCheck) { unexpected("FileManager::mountArchive"); }
bool FileManager::unmountArchive(const std::string&) { unexpected("FileManager::unmountArchive"); }
void FileManager::unmountAllArchives() { unexpected("FileManager::unmountAllArchives"); }
void FileManager::getAllArchives(std::vector<IArchive*>& out) { unexpected("FileManager::getAllArchives"); }
bool FileManager::hasMountedArchives() const { unexpected("FileManager::hasMountedArchives"); }
std::string FileManager::stripModPath(const std::string&) { unexpected("FileManager::stripModPath"); }
void* FileManager::getPathList() const { unexpected("FileManager::getPathList"); }
void FileManager::addPath(const std::string&) { unexpected("FileManager::addPath"); }
void FileManager::removePath(const std::string&) { unexpected("FileManager::removePath"); }
void FileManager::removeAllPaths() { unexpected("FileManager::removeAllPaths"); }
void FileManager::getWritePath(const std::string& name, std::string& out) { unexpected("FileManager::getWritePath"); }
int FileManager::getFileResolveMode() const { unexpected("FileManager::getFileResolveMode"); }
void FileManager::setFileResolveMode(int) { unexpected("FileManager::setFileResolveMode"); }
std::string FileManager::fileExistsEx(const std::string&, bool skipArchives) { unexpected("FileManager::fileExistsEx"); }
std::string FileManager::fileExistsEx2(const std::string&) { unexpected("FileManager::fileExistsEx2"); }
void FileManager::setModPaths(std::vector<std::string>) { unexpected("FileManager::setModPaths"); }
} } }

// Console's out-of-line virtuals (its key function lives in Console.cpp, not linked here).
namespace dice { namespace hfe { namespace io {
std::string Console::ignoredString_;
void Console::setConsoleActive(bool) { unexpected("Console::setConsoleActive"); }
bool Console::getConsoleActive() { unexpected("Console::getConsoleActive"); }
void Console::loadCommandHistory(const std::string&) { unexpected("Console::loadCommandHistory"); }
void Console::saveCommandHistory(const std::string&) { unexpected("Console::saveCommandHistory"); }
} } }

// ---- fake glue entry points -------------------------------------------------------------------
char* PbClAddEvent(int type, int length, char* data)
{
    calls.push_back("cl " + std::to_string(type) + " " + bytes(data, length));
    return nullptr;
}
char* PbSvAddEvent(int type, int index, int length, char* data)
{
    calls.push_back("sv " + std::to_string(type) + " " + std::to_string(index) + " " + bytes(data, length));
    return nullptr;
}

// ---- complete fakes, generated from the interface headers ----------------------------------------
namespace dice { namespace hfe { namespace io {
struct UnusedConsole : Console {
    void setConsoleCompiler(ConsoleCompiler*) override { unexpected("setConsoleCompiler"); }
    void executeLine(const std::string& cmd, std::string& out, bool outputToConsole, bool outputError, bool addToHistory, bool, bool) override { unexpected("executeLine"); }
    void executeLines(const std::list<std::string>&, std::list<std::string>&, bool, bool, bool) override { unexpected("executeLines"); }
    void output(const std::string&) override { unexpected("output"); }
    void updateAsciiKey(char) override { unexpected("updateAsciiKey"); }
    void updateGameInput(const GameInput&) override { unexpected("updateGameInput"); }
    void updateKeyMacros(const GameInput&) override { unexpected("updateKeyMacros"); }
    void getLines(int, std::vector<std::string>&) const override { unexpected("getLines"); }
    bool run(const std::string& file, const std::string& a1, const std::string& a2, const std::string& a3, const std::string& a4, const std::string& a5, const std::string& a6, const std::string& a7, const std::string& a8, std::string& out) override { unexpected("run"); }
    bool runFullAccess(const std::string& file, const std::string& a1, const std::string& a2, const std::string& a3, const std::string& a4, const std::string& a5, const std::string& a6, const std::string& a7, const std::string& a8, std::string& out) override { unexpected("runFullAccess"); }
    bool include(const std::string& file, const std::string& a1, const std::string& a2, const std::string& a3, const std::string& a4, const std::string& a5, const std::string& a6, const std::string& a7, const std::string& a8, std::string& out) override { unexpected("include"); }
    bool bindKeyToConsoleScript(int key, const std::string& script) override { unexpected("bindKeyToConsoleScript"); }
    void setExitCallBack(void (*fn)(void*), void* userData) override { unexpected("setExitCallBack"); }
    void update(char, std::string&, std::vector<std::string>&) override { unexpected("update"); }
    bool addVariable(const std::string& name, const std::string& value) override { unexpected("addVariable"); }
    bool addConstant(const std::string& name, const std::string& value) override { unexpected("addConstant"); }
    std::string listVariables(const std::string& filter) override { unexpected("listVariables"); }
    std::string listConstants(const std::string& filter) override { unexpected("listConstants"); }
    void setEchoErrors(bool) override { unexpected("setEchoErrors"); }
    bool getEchoErrors() const override { unexpected("getEchoErrors"); }
    void setEcho(bool) override { unexpected("setEcho"); }
    bool getEcho() const override { unexpected("getEcho"); }
    void setMaxLineSize(int) override { unexpected("setMaxLineSize"); }
    int getMaxLineSize() const override { unexpected("getMaxLineSize"); }
    void setMaxHistorySize(int) override { unexpected("setMaxHistorySize"); }
    int getMaxHistorySize() const override { unexpected("getMaxHistorySize"); }
    void setMaxCommandHistorySize(int) override { unexpected("setMaxCommandHistorySize"); }
    int getMaxCommandHistorySize() const override { unexpected("getMaxCommandHistorySize"); }
    void setUseRelativePaths(bool) override { unexpected("setUseRelativePaths"); }
    bool getUseRelativePaths() const override { unexpected("getUseRelativePaths"); }
    void setAllowMultipleFileLoad(bool) override { unexpected("setAllowMultipleFileLoad"); }
    bool getAllowMultipleFileLoad() const override { unexpected("getAllowMultipleFileLoad"); }
    const std::string& getWorkingPath() const override { unexpected("getWorkingPath"); }
    void setWorkingPath(const std::string&) override { unexpected("setWorkingPath"); }
    const std::string& getWorkingFile() const override { unexpected("getWorkingFile"); }
    std::string getWorkingFileAndLine() const override { unexpected("getWorkingFileAndLine"); }
    void setAccess(int) override { unexpected("setAccess"); }
    int getAccess() const override { unexpected("getAccess"); }
    void setCurrentLine(std::string&) override { unexpected("setCurrentLine"); }
    void addAutoComplArgument(const std::string&) override { unexpected("addAutoComplArgument"); }
    void removeAutoComplArgument(const std::string&) override { unexpected("removeAutoComplArgument"); }
    void setHandleCommandHook(bool (*hook)(const char*, int)) override { unexpected("setHandleCommandHook"); }
};
} } }
namespace {
using dice::hfe::io::UnusedConsole;
struct UnusedGame : IGame {
    void addRef() override { unexpected("IGame::addRef"); }
    int getRef() override { unexpected("IGame::getRef"); }
    int release() override { unexpected("IGame::release"); }
    void* queryInterface(unsigned int) const override { unexpected("IGame::queryInterface"); }
    bool init() override { unexpected("IGame::init"); }
    void shutdown() override { unexpected("IGame::shutdown"); }
    bool load() override { unexpected("IGame::load"); }
    bool loadPath(const std::string&) override { unexpected("IGame::loadPath"); }
    bool unload() override { unexpected("IGame::unload"); }
    StatsMonitor* getStatsMonitor() const override { unexpected("IGame::getStatsMonitor"); }
    bool update(int steps, float dt) override { unexpected("IGame::update"); }
    bool say(const std::string&) override { unexpected("IGame::say"); }
    TargetChannel getCurrentChannel() override { unexpected("IGame::getCurrentChannel"); }
    void setCurrentChannel(TargetChannel) override { unexpected("IGame::setCurrentChannel"); }
    int getCurrentFlags() override { unexpected("IGame::getCurrentFlags"); }
    void setCurrentFlags(int) override { unexpected("IGame::setCurrentFlags"); }
    ChannelMessageType getCurrentType() override { unexpected("IGame::getCurrentType"); }
    void setCurrentType(ChannelMessageType) override { unexpected("IGame::setCurrentType"); }
    MapInfo getMapInfo() const override { unexpected("IGame::getMapInfo"); }
    bool radioMessage(TargetChannel, const std::string&, world::IPlayer*, unsigned int, bool, bool) override { unexpected("IGame::radioMessage"); }
    bool radioMessageFromPlayer(TargetChannel, const std::string&, unsigned int, unsigned int, bool, bool) override { unexpected("IGame::radioMessageFromPlayer"); }
    bool localRadioMessage(TargetChannel, const std::string&) override { unexpected("IGame::localRadioMessage"); }
    bool gameInfoMessage(TargetChannel, const std::string&, unsigned int, unsigned int, bool) override { unexpected("IGame::gameInfoMessage"); }
    bool localGameInfoMessage(TargetChannel, const std::string&) override { unexpected("IGame::localGameInfoMessage"); }
    void addPlayerInput(const io::PlayerInput&) override { unexpected("IGame::addPlayerInput"); }
    int getCurrentInputIndex() override { unexpected("IGame::getCurrentInputIndex"); }
    void confirmGhost(int, unsigned int) override { unexpected("IGame::confirmGhost"); }
    void confirmReset(int) override { unexpected("IGame::confirmReset"); }
    world::IObject* createObjectOnAllClients(const world::IObjectTemplate*, Math::Vec3, Math::Vec3, bool, int, int) override { unexpected("IGame::createObjectOnAllClients"); }
    world::IObject* getObjectFromNetworkableId(const unsigned short&) const override { unexpected("IGame::getObjectFromNetworkableId"); }
    void createAndDestroyObject(const world::IObjectTemplate*) override { unexpected("IGame::createAndDestroyObject"); }
    void sendGameEventToAllPlaying(GameEvent*) override { unexpected("IGame::sendGameEventToAllPlaying"); }
    void sendGameEventIfPlaying(unsigned int, GameEvent*) override { unexpected("IGame::sendGameEventIfPlaying"); }
    void sendGameEventToAll(GameEvent*) override { unexpected("IGame::sendGameEventToAll"); }
    void sendGameEvent(unsigned int, GameEvent*) override { unexpected("IGame::sendGameEvent"); }
    void sendGameEventToAllConnected(GameEvent*) override { unexpected("IGame::sendGameEventToAllConnected"); }
    void sendGameEventIfConnected(unsigned int, GameEvent*) override { unexpected("IGame::sendGameEventIfConnected"); }
    void sendMissileMessageToOccupyingPlayers(world::IObject*, world::MissileLockMessage) override { unexpected("IGame::sendMissileMessageToOccupyingPlayers"); }
    bool destroyObject(world::IObject*) override { unexpected("IGame::destroyObject"); }
    bool enterVehicle(world::IPlayer*, world::IObject*, bool) override { unexpected("IGame::enterVehicle"); }
    bool exitVehicle(world::IPlayer*, bool, bool) override { unexpected("IGame::exitVehicle"); }
    std::string listPlayers() const override { unexpected("IGame::listPlayers"); }
    void joinSquad(int) override { unexpected("IGame::joinSquad"); }
    void leaveSquad(int) override { unexpected("IGame::leaveSquad"); }
    std::string listSquadMembers(int, int) override { unexpected("IGame::listSquadMembers"); }
    std::string listSquads(int) override { unexpected("IGame::listSquads"); }
    void changeSquadName(const std::string&, bool) override { unexpected("IGame::changeSquadName"); }
    void makeMeCommander() override { unexpected("IGame::makeMeCommander"); }
    void popNextCommanderApplicant(int) override { unexpected("IGame::popNextCommanderApplicant"); }
    std::string getSpottedMessage(unsigned short) override { unexpected("IGame::getSpottedMessage"); }
    void sendSpottedMessage(world::IPlayer*, unsigned short, Vec2, world::VehicleType) override { unexpected("IGame::sendSpottedMessage"); }
    void callForSpecialist(world::IPlayer*, HudNodeTypes) override { unexpected("IGame::callForSpecialist"); }
    void recordDemo(DemoRecorder*) override { unexpected("IGame::recordDemo"); }
    void stopRecording() override { unexpected("IGame::stopRecording"); }
    bool isPlayingDemo() override { unexpected("IGame::isPlayingDemo"); }
    void setSimulationRate(int) override { unexpected("IGame::setSimulationRate"); }
    int getSimulationRate() override { unexpected("IGame::getSimulationRate"); }
    void debugShowActiveGhosts(int) override { unexpected("IGame::debugShowActiveGhosts"); }
    const std::string& getCurrentMapName() const override { unexpected("IGame::getCurrentMapName"); }
    const std::string& getCurrentMapGameMode() const override { unexpected("IGame::getCurrentMapGameMode"); }
    const std::string& getCurrentMapLevelPath() const override { unexpected("IGame::getCurrentMapLevelPath"); }
    int getCurrentMapMaxPlayers() const override { unexpected("IGame::getCurrentMapMaxPlayers"); }
    int getCurrentMapSize() const override { unexpected("IGame::getCurrentMapSize"); }
    void setCurrentMapSize(unsigned int) override { unexpected("IGame::setCurrentMapSize"); }
    const io::BasicNetworkStats* getNetworkStats() const override { unexpected("IGame::getNetworkStats"); }
    void sendUDPPacket(const char*, unsigned short, void*, int) override { unexpected("IGame::sendUDPPacket"); }
    void addTextToChatWindow(const char*) override { unexpected("IGame::addTextToChatWindow"); }
    void failedExitVehicle(int) override { unexpected("IGame::failedExitVehicle"); }
    bool isPunkBusterEnabled() const override { unexpected("IGame::isPunkBusterEnabled"); }
    bool getAbortLoadLevel() const override { unexpected("IGame::getAbortLoadLevel"); }
};
struct UnusedClient : IGameClient {
    bool handleControlObjectState(io::BitStream* stream, unsigned short networkableId, int actionBufferEntries, unsigned int tick, float period) override { unexpected("IGameClient::handleControlObjectState"); }
    bool playDemo(DemoRecorder* recorder) override { unexpected("IGameClient::playDemo"); }
    void handleChallenge(ChallengeEvent* event) override { unexpected("IGameClient::handleChallenge"); }
    int joinServer(const std::string& address, int port, const std::string& password, float timeoutSeconds, bool punkBuster) override { unexpected("IGameClient::joinServer"); }
    IClientConnection* getConnection() override { unexpected("IGameClient::getConnection"); }
    bool createObject(unsigned int templateId, unsigned short networkableId, const Vec3* position, const Vec3* rotation) override { unexpected("IGameClient::createObject"); }
    bool createKit(unsigned int templateId, unsigned short networkableId, unsigned int creationIndex, int unlockLevel, const Vec3* position, const Vec3& rotation) override { unexpected("IGameClient::createKit"); }
    bool destroyPlayer(int playerId) override { unexpected("IGameClient::destroyPlayer"); }
    bool destroyObject(unsigned short networkableId) override { unexpected("IGameClient::destroyObject"); }
    bool enterVehicle(int playerId, unsigned short networkableId, bool setAsDefaultVehicle) override { unexpected("IGameClient::enterVehicle"); }
    bool exitVehicle(int playerId, bool field0D) override { unexpected("IGameClient::exitVehicle"); }
    bool getConnectionStatus(io::ConnectionStatus& out) override { unexpected("IGameClient::getConnectionStatus"); }
    std::string getServerAddress() override { unexpected("IGameClient::getServerAddress"); }
    void slot44(int) override { unexpected("IGameClient::slot44"); }
    void restartDemo() override { unexpected("IGameClient::restartDemo"); }
    void demoSwitchPlayer(bool next) override { unexpected("IGameClient::demoSwitchPlayer"); }
    void demoNextCamera() override { unexpected("IGameClient::demoNextCamera"); }
    int demoGetCurrentPlayerId() override { unexpected("IGameClient::demoGetCurrentPlayerId"); }
    bool demoIsFreeFlying() override { unexpected("IGameClient::demoIsFreeFlying"); }
    void togglePause() override { unexpected("IGameClient::togglePause"); }
    void setPause(bool paused) override { unexpected("IGameClient::setPause"); }
    bool getPause() override { unexpected("IGameClient::getPause"); }
    bool isDemoAtEnd() override { unexpected("IGameClient::isDemoAtEnd"); }
    void addDemoBookmark(const std::string& url, const std::string& mapName) override { unexpected("IGameClient::addDemoBookmark"); }
    bool getDisconnectRequested() override { unexpected("IGameClient::getDisconnectRequested"); }
    bool getIsReady() override { unexpected("IGameClient::getIsReady"); }
    std::string getServerAddressCopy() override { unexpected("IGameClient::getServerAddressCopy"); }
    void sendUDPPacket(const char* address, unsigned short port, void* data, int size) override { unexpected("IGameClient::sendUDPPacket"); }
};
struct UnusedServer : IGameServer {
    void addScopableGhost(io::NetworkableDescriptor* descriptor) override { unexpected("IGameServer::addScopableGhost"); }
    void removeGhost(io::NetworkableDescriptor* descriptor) override { unexpected("IGameServer::removeGhost"); }
    void addStaticGhost(io::NetworkableDescriptor* descriptor) override { unexpected("IGameServer::addStaticGhost"); }
    bool startServer(const std::string& name, int port, int unused) override { unexpected("IGameServer::startServer"); }
    int createPlayer(const std::string& name, bool isAI) override { unexpected("IGameServer::createPlayer"); }
    int createAndSpawnAIPlayer(const std::string& name, int teamId, int kitId) override { unexpected("IGameServer::createAndSpawnAIPlayer"); }
    void destroyPlayer(world::IPlayer* player) override { unexpected("IGameServer::destroyPlayer"); }
    void challengeResponse(int connectionId, GameEvent* event) override { unexpected("IGameServer::challengeResponse"); }
    void setConnectionType(int connectionId, ConnectionType type) override { unexpected("IGameServer::setConnectionType"); }
    void chokeBandwidth() override { unexpected("IGameServer::chokeBandwidth"); }
    bool changePlayerName(int playerId, const std::string& name) override { unexpected("IGameServer::changePlayerName"); }
    IClientConnection* getConnection(int connectionId) const override { unexpected("IGameServer::getConnection"); }
    int getConnectionState(int connectionId) const override { unexpected("IGameServer::getConnectionState"); }
    bool getConnectionStatus(int connectionId, io::ConnectionStatus& out) override { unexpected("IGameServer::getConnectionStatus"); }
    void toggleFreeCamera(world::IPlayer* player) override { unexpected("IGameServer::toggleFreeCamera"); }
    void resetConnections() override { unexpected("IGameServer::resetConnections"); }
    void debugShowActiveGhosts(int connectionId) override { unexpected("IGameServer::debugShowActiveGhosts"); }
    void debugShowStreamManagerSizes() override { unexpected("IGameServer::debugShowStreamManagerSizes"); }
    void debugShowGhostPriorities(int connectionId) override { unexpected("IGameServer::debugShowGhostPriorities"); }
    int getTotalActiveGhosts() override { unexpected("IGameServer::getTotalActiveGhosts"); }
    bool getPlayerConnectionIp(world::IPlayer* player, std::string& out) override { unexpected("IGameServer::getPlayerConnectionIp"); }
    bool getPlayerConnectionIpAndPort(world::IPlayer* player, std::string& out) override { unexpected("IGameServer::getPlayerConnectionIpAndPort"); }
    void updatePlayersVoipStatusOnAllClients(int playerId, bool status) override { unexpected("IGameServer::updatePlayersVoipStatusOnAllClients"); }
    void kickPlayer(int playerId, io::PResult reason, const std::string& text) override { unexpected("IGameServer::kickPlayer"); }
    std::string consoleKickPlayer(const std::string& name) override { unexpected("IGameServer::consoleKickPlayer"); }
    void* getStatsManager() override { unexpected("IGameServer::getStatsManager"); }
    int getNumberOfConnectedClients() const override { unexpected("IGameServer::getNumberOfConnectedClients"); }
    int getNumberOfConnectingClients() const override { unexpected("IGameServer::getNumberOfConnectingClients"); }
    int getNumDroppedFrames() const override { unexpected("IGameServer::getNumDroppedFrames"); }
    int getNumOvershotFrames() const override { unexpected("IGameServer::getNumOvershotFrames"); }
    bool getInitComplete() const override { unexpected("IGameServer::getInitComplete"); }
    bool getIsInternetServer() const override { unexpected("IGameServer::getIsInternetServer"); }
    void sendUDPPacket(const char* address, unsigned short port, void* data, int size) override { unexpected("IGameServer::sendUDPPacket"); }
    bool say(const std::string& text) override { unexpected("IGameServer::say"); }
    bool sayAll(const std::string& text) override { unexpected("IGameServer::sayAll"); }
    bool sayTeam(int team, const std::string& text) override { unexpected("IGameServer::sayTeam"); }
    bool say(const std::string& text, TargetChannel channel, ChannelMessageType type, unsigned int, unsigned int) override { unexpected("IGameServer::say"); }
    void enqueueRadioMessage(EventNode* node) override { unexpected("IGameServer::enqueueRadioMessage"); }
    bool addPersistenceConnection(HTTPRequest* request) override { unexpected("IGameServer::addPersistenceConnection"); }
    bool removePersistenceConnection(HTTPRequest* request) override { unexpected("IGameServer::removePersistenceConnection"); }
    void setCurrentPersistenceObject(IBase* object) override { unexpected("IGameServer::setCurrentPersistenceObject"); }
    IBase* getCurrentPersistenceObject() override { unexpected("IGameServer::getCurrentPersistenceObject"); }
    AutoRecorder* getAutoRecorder() override { unexpected("IGameServer::getAutoRecorder"); }
    io::NetServer* getNetServer() override { unexpected("IGameServer::getNetServer"); }
    void onContentCheckEvent(int, const unsigned char*, const unsigned char*, const unsigned char*) override { unexpected("IGameServer::onContentCheckEvent"); }
    bool useContentChecking() override { unexpected("IGameServer::useContentChecking"); }
    bool isClientReady(int connectionId) override { unexpected("IGameServer::isClientReady"); }
    IPlayerUnlockInfo* getPlayerUnlockInfo() override { unexpected("IGameServer::getPlayerUnlockInfo"); }
    bool isPythonValid() override { unexpected("IGameServer::isPythonValid"); }
    void setPythonValid(int, bool valid) override { unexpected("IGameServer::setPythonValid"); }
    std::string getServerAddress() const override { unexpected("IGameServer::getServerAddress"); }
    std::string getName() const override { unexpected("IGameServer::getName"); }
};

struct FakeConsole final : UnusedConsole {
    void output(const std::string& line) override { calls.push_back("console " + line); }
    void executeLine(const std::string& cmd, std::string& out, bool a, bool b, bool c, bool d, bool e) override
    {
        calls.push_back("execute " + cmd + (&out == &io::Console::ignoredString_ ? "" : " BADOUT")
                        + (a || b || c || d || e ? " BADFLAGS" : ""));
    }
};

} // namespace
namespace dice { namespace hfe { namespace world {
struct FakePlayer final : IPlayer {
    std::string name;
    bool connected = true;
    int team = 2;
    void addRef() override { unexpected("IPlayer::addRef"); }
    int getRef() override { unexpected("IPlayer::getRef"); }
    int release() override { unexpected("IPlayer::release"); }
    void* queryInterface(unsigned int) const override { unexpected("IPlayer::queryInterface"); }
    const std::string& getName() const override { return name; }
    bool getIsConnected() const override { return connected; }
    int getTeam() const override { return team; }
    unsigned int getClassID() const override { unexpected("IPlayer::getClassID"); }
    WeakPtr<IPlayer> getWeakPtr() override { unexpected("IPlayer::getWeakPtr"); }
    void setName(const std::string& name) override { unexpected("IPlayer::setName"); }
    void setFlags(const Flag32Bits<IPlayer>& flags) override { unexpected("IPlayer::setFlags"); }
    bool testFlags(const Flag32Bits<IPlayer>& flags) const override { unexpected("IPlayer::testFlags"); }
    void handleUpdate(float dt, unsigned int) override { unexpected("IPlayer::handleUpdate"); }
    void handleFrameUpdate(float dt) override { unexpected("IPlayer::handleFrameUpdate"); }
    UpdateFreqencyType getUpdateFrequencyType() const override { unexpected("IPlayer::getUpdateFrequencyType"); }
    void setUpdateFrequencyType(UpdateFreqencyType type) override { unexpected("IPlayer::setUpdateFrequencyType"); }
    void handleInput(io::PlayerInput& input, float dt, unsigned int) override { unexpected("IPlayer::handleInput"); }
    void getAutoInput(io::PlayerInput& out, bool) override { unexpected("IPlayer::getAutoInput"); }
    IObject* getVehicle() const override { unexpected("IPlayer::getVehicle"); }
    bool setVehicle(IObject* vehicle, int seat) override { unexpected("IPlayer::setVehicle"); }
    int getInputId() const override { unexpected("IPlayer::getInputId"); }
    int getId() const override { unexpected("IPlayer::getId"); }
    void setIsRemote(bool remote) override { unexpected("IPlayer::setIsRemote"); }
    bool getIsRemote() const override { unexpected("IPlayer::getIsRemote"); }
    void setIsAIPlayer(bool ai) override { unexpected("IPlayer::setIsAIPlayer"); }
    bool getIsAIPlayer() const override { unexpected("IPlayer::getIsAIPlayer"); }
    void setIsAlive(bool alive) override { unexpected("IPlayer::setIsAlive"); }
    bool getIsAlive() const override { unexpected("IPlayer::getIsAlive"); }
    void setSuicide(bool) override { unexpected("IPlayer::setSuicide"); }
    bool getSuicide() const override { unexpected("IPlayer::getSuicide"); }
    void setIsConnected(bool) override { unexpected("IPlayer::setIsConnected"); }
    float getCrossHairFadeValue() const override { unexpected("IPlayer::getCrossHairFadeValue"); }
    void setViewDistance(float) override { unexpected("IPlayer::setViewDistance"); }
    float getViewDistance() const override { unexpected("IPlayer::getViewDistance"); }
    void setFov(float fov, float time) override { unexpected("IPlayer::setFov"); }
    float getFov() const override { unexpected("IPlayer::getFov"); }
    float getEndFov() const override { unexpected("IPlayer::getEndFov"); }
    ActionBuffer* getActionBuffer() override { unexpected("IPlayer::getActionBuffer"); }
    void* getAIPlayerInput() const override { unexpected("IPlayer::getAIPlayerInput"); }
    void setDefaultVehicle(IObject*) override { unexpected("IPlayer::setDefaultVehicle"); }
    IObject* getDefaultVehicle() override { unexpected("IPlayer::getDefaultVehicle"); }
    IObject* getFreeCamera() const override { unexpected("IPlayer::getFreeCamera"); }
    void setFreeCameraEnabled(bool) override { unexpected("IPlayer::setFreeCameraEnabled"); }
    bool getFreeCameraEnabled() const override { unexpected("IPlayer::getFreeCameraEnabled"); }
    void toggleFreeCameraEnabled() override { unexpected("IPlayer::toggleFreeCameraEnabled"); }
    void setOverheadCameraEnabled(bool) override { unexpected("IPlayer::setOverheadCameraEnabled"); }
    bool getOverheadCameraEnabled() const override { unexpected("IPlayer::getOverheadCameraEnabled"); }
    void setOverheadCameraTopDownView(bool) override { unexpected("IPlayer::setOverheadCameraTopDownView"); }
    bool getOverheadCameraTopDownView() const override { unexpected("IPlayer::getOverheadCameraTopDownView"); }
    void setCommanderCamOverhead(bool) override { unexpected("IPlayer::setCommanderCamOverhead"); }
    bool getCommanderCamOverhead() const override { unexpected("IPlayer::getCommanderCamOverhead"); }
    int getTickCount() const override { unexpected("IPlayer::getTickCount"); }
    void addTick() override { unexpected("IPlayer::addTick"); }
    void subTick() override { unexpected("IPlayer::subTick"); }
    void resetTickCounter() override { unexpected("IPlayer::resetTickCounter"); }
    float getTimeOutSideWorld() const override { unexpected("IPlayer::getTimeOutSideWorld"); }
    void setTeam(int team, bool) override { unexpected("IPlayer::setTeam"); }
    unsigned int getSpawnAtTick() const override { unexpected("IPlayer::getSpawnAtTick"); }
    void setSpawnAtTick(unsigned int tick) override { unexpected("IPlayer::setSpawnAtTick"); }
    float getTimeToSpawn() const override { unexpected("IPlayer::getTimeToSpawn"); }
    float getManDownTimeLeft() const override { unexpected("IPlayer::getManDownTimeLeft"); }
    void setIsManDown(bool) override { unexpected("IPlayer::setIsManDown"); }
    bool getIsManDown() override { unexpected("IPlayer::getIsManDown"); }
    unsigned int getManDownAtTick() override { unexpected("IPlayer::getManDownAtTick"); }
    void setSquadId(unsigned int id) override { unexpected("IPlayer::setSquadId"); }
    int getSquadId() const override { unexpected("IPlayer::getSquadId"); }
    void setIsCommander(bool) override { unexpected("IPlayer::setIsCommander"); }
    bool getIsCommander() const override { unexpected("IPlayer::getIsCommander"); }
    void setUAVIsReady(bool) override { unexpected("IPlayer::setUAVIsReady"); }
    bool getUAVIsReady() const override { unexpected("IPlayer::getUAVIsReady"); }
    void setSatelliteIsReady(bool) override { unexpected("IPlayer::setSatelliteIsReady"); }
    bool getSatelliteIsReady() const override { unexpected("IPlayer::getSatelliteIsReady"); }
    void setLastTimeCommander(double t) override { unexpected("IPlayer::setLastTimeCommander"); }
    double getLastTimeCommander() const override { unexpected("IPlayer::getLastTimeCommander"); }
    void setCommanderReapplyWaitTime(double t) override { unexpected("IPlayer::setCommanderReapplyWaitTime"); }
    double getCommanderReapplyWaitTime() const override { unexpected("IPlayer::getCommanderReapplyWaitTime"); }
    void setIsSquadLeader(bool) override { unexpected("IPlayer::setIsSquadLeader"); }
    bool getIsSquadLeader() const override { unexpected("IPlayer::getIsSquadLeader"); }
    bool getIsGassed() override { unexpected("IPlayer::getIsGassed"); }
    void setIsInsideCP(bool) override { unexpected("IPlayer::setIsInsideCP"); }
    bool getIsInsideCP() override { unexpected("IPlayer::getIsInsideCP"); }
    void setHasSimulatedFrame(bool) override { unexpected("IPlayer::setHasSimulatedFrame"); }
    bool getHasSimulatedFrame() const override { unexpected("IPlayer::getHasSimulatedFrame"); }
    void setSpawnGroup(int group) override { unexpected("IPlayer::setSpawnGroup"); }
    int getSpawnGroup() const override { unexpected("IPlayer::getSpawnGroup"); }
    void setInterpolationTime(unsigned int ms) override { unexpected("IPlayer::setInterpolationTime"); }
    unsigned int getInterpolationTime() const override { unexpected("IPlayer::getInterpolationTime"); }
    void setPing(unsigned int ping) override { unexpected("IPlayer::setPing"); }
    unsigned int getPing() const override { unexpected("IPlayer::getPing"); }
    void setKit(int kit) override { unexpected("IPlayer::setKit"); }
    int getKit() const override { unexpected("IPlayer::getKit"); }
    PlayerScoreData* getScoreData() override { unexpected("IPlayer::getScoreData"); }
    void setFlagHolder(bool) override { unexpected("IPlayer::setFlagHolder"); }
    bool getFlagHolder() const override { unexpected("IPlayer::getFlagHolder"); }
    ICameraObject* getCamera() const override { unexpected("IPlayer::getCamera"); }
    void setCamera(ICameraObject* camera) override { unexpected("IPlayer::setCamera"); }
    bool hasFirstPersonView() const override { unexpected("IPlayer::hasFirstPersonView"); }
    double getTimeConnected() const override { unexpected("IPlayer::getTimeConnected"); }
    io::INetworkable* getNetworkable() override { unexpected("IPlayer::getNetworkable"); }
    bool getStateSync(io::BitStream* stream, IObject* object) override { unexpected("IPlayer::getStateSync"); }
    bool setStateSync(io::BitStream* stream, IObject* object, double) override { unexpected("IPlayer::setStateSync"); }
    void reset() override { unexpected("IPlayer::reset"); }
    void setFiringState(bool) override { unexpected("IPlayer::setFiringState"); }
    bool getFiringState() const override { unexpected("IPlayer::getFiringState"); }
    void setAltFiringState(bool) override { unexpected("IPlayer::setAltFiringState"); }
    bool getAltFiringState() const override { unexpected("IPlayer::getAltFiringState"); }
    void setFlareFiringState(bool) override { unexpected("IPlayer::setFlareFiringState"); }
    bool getFlareFiringState() const override { unexpected("IPlayer::getFlareFiringState"); }
    void setSprintState(bool) override { unexpected("IPlayer::setSprintState"); }
    bool getSprintState() const override { unexpected("IPlayer::getSprintState"); }
    void setSprintStateLastTick(bool) override { unexpected("IPlayer::setSprintStateLastTick"); }
    bool getSprintStateLastTick() const override { unexpected("IPlayer::getSprintStateLastTick"); }
    void setReloadState() override { unexpected("IPlayer::setReloadState"); }
    bool getAndResetReloadState() override { unexpected("IPlayer::getAndResetReloadState"); }
    void updateFiringState(unsigned int, IObject*) override { unexpected("IPlayer::updateFiringState"); }
    void updateGhostFiringState() override { unexpected("IPlayer::updateGhostFiringState"); }
    void setHitIndicator(bool) override { unexpected("IPlayer::setHitIndicator"); }
    bool getHitIndicator() const override { unexpected("IPlayer::getHitIndicator"); }
    float updateHitFromDirection(const Vec3& direction) override { unexpected("IPlayer::updateHitFromDirection"); }
    float getHitFromDirection() const override { unexpected("IPlayer::getHitFromDirection"); }
    void updateCameraShake(float amount) override { unexpected("IPlayer::updateCameraShake"); }
    float getCameraShake() const override { unexpected("IPlayer::getCameraShake"); }
    unsigned int getSquadInviteMask() const override { unexpected("IPlayer::getSquadInviteMask"); }
    void setSquadInviteMask(unsigned int mask) override { unexpected("IPlayer::setSquadInviteMask"); }
    bool getIsInvitedToSquad(int squad) const override { unexpected("IPlayer::getIsInvitedToSquad"); }
    void setKitTimeStamp(double t) override { unexpected("IPlayer::setKitTimeStamp"); }
    double getKitTimeStamp() const override { unexpected("IPlayer::getKitTimeStamp"); }
    void setCurrentFiringButton(io::PlayerInputMap button) override { unexpected("IPlayer::setCurrentFiringButton"); }
    io::PlayerInputMap getCurrentFiringButton() const override { unexpected("IPlayer::getCurrentFiringButton"); }
    void setPersonalDecalTemplate(const IObjectTemplate* decal) override { unexpected("IPlayer::setPersonalDecalTemplate"); }
    const IObjectTemplate* getPersonalDecalTemplate() const override { unexpected("IPlayer::getPersonalDecalTemplate"); }
    void enableVOHelp(bool) override { unexpected("IPlayer::enableVOHelp"); }
    void enableVOHelpMessage(int) override { unexpected("IPlayer::enableVOHelpMessage"); }
    void disableVOHelpMessage(int) override { unexpected("IPlayer::disableVOHelpMessage"); }
    bool getVOHelpMessagePlayed(int) override { unexpected("IPlayer::getVOHelpMessagePlayed"); }
    void doAddTouchItem(IPlayer* other) override { unexpected("IPlayer::doAddTouchItem"); }
    void setHealRepairedAtTick(unsigned int tick, bool giving, bool repairing) override { unexpected("IPlayer::setHealRepairedAtTick"); }
    unsigned int getHealRepairedAtTick() const override { unexpected("IPlayer::getHealRepairedAtTick"); }
    void setGaveAmmoAtTick(unsigned int tick, bool giving) override { unexpected("IPlayer::setGaveAmmoAtTick"); }
    unsigned int getGaveAmmoAtTick() const override { unexpected("IPlayer::getGaveAmmoAtTick"); }
    void setFailedExitAtTick(unsigned int tick) override { unexpected("IPlayer::setFailedExitAtTick"); }
    unsigned int getFailedExitAtTick() const override { unexpected("IPlayer::getFailedExitAtTick"); }
    bool getIsGivingAbility() const override { unexpected("IPlayer::getIsGivingAbility"); }
    bool getIsRepairing() const override { unexpected("IPlayer::getIsRepairing"); }
    bool getIsGivingReplenish() const override { unexpected("IPlayer::getIsGivingReplenish"); }
    int getChosenUnlockLevel(int kit, int slot) const override { unexpected("IPlayer::getChosenUnlockLevel"); }
    void setChosenUnlockLevel(int kit, int slot, int level) override { unexpected("IPlayer::setChosenUnlockLevel"); }
    bool isUnlocked(int kit, int slot, int level) override { unexpected("IPlayer::isUnlocked"); }
    int getUnlockLevel(int kit, int slot) override { unexpected("IPlayer::getUnlockLevel"); }
    void setUnlocks(std::map<int, int>* unlocks) override { unexpected("IPlayer::setUnlocks"); }
    std::map<int, int>* getUnlocks() override { unexpected("IPlayer::getUnlocks"); }
    void setPeerAddress(const std::string& address) override { unexpected("IPlayer::setPeerAddress"); }
    const std::string& getPeerAddress() const override { unexpected("IPlayer::getPeerAddress"); }
    bool getVoipEnabled() override { unexpected("IPlayer::getVoipEnabled"); }
    unsigned short getVoipSessionId() override { unexpected("IPlayer::getVoipSessionId"); }
    void setVoipSessionId(unsigned short id) override { unexpected("IPlayer::setVoipSessionId"); }
    bool getIsMuted() override { unexpected("IPlayer::getIsMuted"); }
    bool getIsTalking() override { unexpected("IPlayer::getIsTalking"); }
    double getLastTalkTime() override { unexpected("IPlayer::getLastTalkTime"); }
    double getStopTalkingThreshold() override { unexpected("IPlayer::getStopTalkingThreshold"); }
    void setIsMuted(bool) override { unexpected("IPlayer::setIsMuted"); }
    void setIsTalking(bool) override { unexpected("IPlayer::setIsTalking"); }
    void setVoipEnabled(bool) override { unexpected("IPlayer::setVoipEnabled"); }
    void setVotingStatus(PlayerVotingStatus status) override { unexpected("IPlayer::setVotingStatus"); }
    PlayerVotingStatus getVotingStatus() override { unexpected("IPlayer::getVotingStatus"); }
    void setParticipateInVoting(bool) override { unexpected("IPlayer::setParticipateInVoting"); }
    bool getParticipateInVoting() override { unexpected("IPlayer::getParticipateInVoting"); }
    void setTargetOfVote(bool) override { unexpected("IPlayer::setTargetOfVote"); }
    bool getTargetOfVote() override { unexpected("IPlayer::getTargetOfVote"); }
    int getProfileId() override { unexpected("IPlayer::getProfileId"); }
    void setProfileId(int id) override { unexpected("IPlayer::setProfileId"); }
    void setBlurryVision(float, float, float) override { unexpected("IPlayer::setBlurryVision"); }
    float getBlurValue() const override { unexpected("IPlayer::getBlurValue"); }
    bool getIsNearGasCloud() const override { unexpected("IPlayer::getIsNearGasCloud"); }
    bool getIsGasMaskOn() const override { unexpected("IPlayer::getIsGasMaskOn"); }
    void setIsGasMaskOn(bool, bool) override { unexpected("IPlayer::setIsGasMaskOn"); }
    void setGasMaskHudItem(bool) override { unexpected("IPlayer::setGasMaskHudItem"); }
    float getPlayerGasMaskChangeTimer() const override { unexpected("IPlayer::getPlayerGasMaskChangeTimer"); }
    float getPlayerNightVisionChangeTimer() const override { unexpected("IPlayer::getPlayerNightVisionChangeTimer"); }
    bool getIsUnderwater() const override { unexpected("IPlayer::getIsUnderwater"); }
    float getNormalizedNightVisionPower() override { unexpected("IPlayer::getNormalizedNightVisionPower"); }
    void setNightVision(bool, bool) override { unexpected("IPlayer::setNightVision"); }
    void rechargeNightVision() override { unexpected("IPlayer::rechargeNightVision"); }
    void setVisualNightVision(bool) override { unexpected("IPlayer::setVisualNightVision"); }
    bool getNightVision() const override { unexpected("IPlayer::getNightVision"); }
    void disableFlashBangEffect() override { unexpected("IPlayer::disableFlashBangEffect"); }
    void disableBlurryVision() override { unexpected("IPlayer::disableBlurryVision"); }
    io::PlayerInput getPlayerInput() override { unexpected("IPlayer::getPlayerInput"); }
    void resetOnDeath() override { unexpected("IPlayer::resetOnDeath"); }
    bool isPlayerInNightMap() const override { unexpected("IPlayer::isPlayerInNightMap"); }
    void setTracePlayerId(int id) override { unexpected("IPlayer::setTracePlayerId"); }
    int getTracePlayerId() override { unexpected("IPlayer::getTracePlayerId"); }
    void allocateFreeCamera() override { unexpected("IPlayer::allocateFreeCamera"); }
    bool getIsThisASpam(bool) override { unexpected("IPlayer::getIsThisASpam"); }
    void setRadioSpamFlagCount(int) override { unexpected("IPlayer::setRadioSpamFlagCount"); }
    int getRadioSpamFlagCount() override { unexpected("IPlayer::getRadioSpamFlagCount"); }
    void setRadioSpamBlockedFlagCount(int) override { unexpected("IPlayer::setRadioSpamBlockedFlagCount"); }
    int getRadioSpamBlockedFlagCount() override { unexpected("IPlayer::getRadioSpamBlockedFlagCount"); }
    void setRadioSpamFilterFlagTimer(float) override { unexpected("IPlayer::setRadioSpamFilterFlagTimer"); }
    float getRadioSpamFilterFlagTimer() override { unexpected("IPlayer::getRadioSpamFilterFlagTimer"); }
    void resetSpamFilter() override { unexpected("IPlayer::resetSpamFilter"); }
    void getCloseMines(std::vector<IObject*>& out) const override { unexpected("IPlayer::getCloseMines"); }
    bool getAutoReload() const override { unexpected("IPlayer::getAutoReload"); }
    void setAutoReload(bool) override { unexpected("IPlayer::setAutoReload"); }
    void updateClientSettings() override { unexpected("IPlayer::updateClientSettings"); }
};
} } }
namespace {
using dice::hfe::world::FakePlayer;

std::string s_mapName = "strike_at_karkand";
struct ClientGame final : UnusedGame, UnusedClient {
    std::string serverAddress = "10.0.0.1:16567";
    void addRef() override {}
    int getRef() override { return 1; }
    int release() override { return 1; }
    void* queryInterface(unsigned int iid) const override
    {
        return iid == IID_IGameClient ? const_cast<IGameClient*>(static_cast<const IGameClient*>(this)) : nullptr;
    }
    const std::string& getCurrentMapName() const override { return s_mapName; }
    void addTextToChatWindow(const char* text) override { calls.push_back(std::string("chat ") + text); }
    std::string getServerAddressCopy() override { calls.push_back("serverAddress"); return serverAddress; }
    void sendUDPPacket(const char* address, unsigned short port, void* data, int size) override
    {
        calls.push_back(std::string("client udp ") + address + ":" + std::to_string(port) + " " + bytes(data, size));
    }
};
struct ServerGame final : UnusedGame, UnusedServer {
    std::vector<std::string> playerAddresses;
    unsigned addressCalls = 0;
    bool ready = true;
    bool isClientReady(int connectionId) override { calls.push_back("ready " + std::to_string(connectionId)); return ready; }
    void kickPlayer(int playerId, io::PResult reason, const std::string& text) override
    {
        calls.push_back("kick " + std::to_string(playerId) + " " + std::to_string(static_cast<int>(reason)) + " " + text);
    }
    void addRef() override {}
    int getRef() override { return 1; }
    int release() override { return 1; }
    void* queryInterface(unsigned int iid) const override
    {
        return iid == IID_IGameServer ? const_cast<IGameServer*>(static_cast<const IGameServer*>(this)) : nullptr;
    }
    void addTextToChatWindow(const char*) override { unexpected("server chat window"); }
    bool getPlayerConnectionIpAndPort(world::IPlayer*, std::string& out) override
    {
        if (addressCalls >= playerAddresses.size()) return false;
        out = playerAddresses[addressCalls++];
        return !out.empty();
    }
    void sendUDPPacket(const char* address, unsigned short port, void* data, int size) override
    {
        calls.push_back(std::string("server udp ") + address + ":" + std::to_string(port) + " " + bytes(data, size));
    }
    std::string getServerAddress() const override { return "192.168.0.9:16567"; }
    std::string getName() const override { return "test server"; }
};
struct NeitherGame final : UnusedGame {
    void addRef() override {}
    int getRef() override { return 1; }
    int release() override { return 1; }
    void* queryInterface(unsigned int) const override { return nullptr; }
};
} // namespace

// The client-glue forward PB_handlePacket makes in the client image only.
#if DEDICATED
#define CLIENT_FORWARD(text) std::string()
#else
#define CLIENT_FORWARD(text) std::string(text)
#endif

int main()
{
    FakeConsole console;
    io::mainConsole = reinterpret_cast<io::OldConsole*>(static_cast<io::Console*>(&console));
    alignas(16) static unsigned char settingsStandIn[sizeof(ServerSettings)];
    serverSettings = reinterpret_cast<ServerSettings*>(settingsStandIn);  // members read directly; no ctor runs
    world::playerManager = new world::PlayerManager();
    io::fileManager = new io::FileManager();
    settings = new SettingsRepostitory();
    ClientGame client;
    ServerGame server;
    NeitherGame neither;

    // ---- PB_handlePacket ----
    game = &client;
    char packet[32];
    std::memcpy(packet, "\xff\xff\xff\xffPB_X1234", 12);
    calls.clear();
    PB_handlePacket(packet, 7, "1.2.3.4:5");
    std::memcpy(packet, "\xfe\xff\xff\xffPB_X1234", 12);
    PB_handlePacket(packet, 12, "1.2.3.4:5");
    check(joined().empty(), "packet: shorter than 8 or without the ffffffff prefix is dropped");
    std::memcpy(packet, "\xff\xff\xff\xffPB_X1234", 12);
    calls.clear();
    PB_handlePacket(packet, 12, "1.2.3.4:5");
    check(joined() == CLIENT_FORWARD("cl 13 PB_X1234;") + "maxPlayers;maxPlayers;maxPlayers;maxPlayers;maxPlayers;maxPlayers;sv 13 4 PB_X1234;",
          "type X: client event (client image), then the server event with the player count as index (no server)");
    for (char t : std::string("S2GIYBL")) {
        packet[7] = t;
        calls.clear();
        PB_handlePacket(packet, 12, "x");
        check(joined().find("cl ") == std::string::npos && joined().find("sv 13 4") != std::string::npos,
              std::string("type ") + t + ": server only");
    }
    for (char t : std::string("C1J")) {
        packet[7] = t;
        calls.clear();
        PB_handlePacket(packet, 12, "x");
        check(joined() == CLIENT_FORWARD(std::string("cl 13 PB_") + t + "1234;"), std::string("type ") + t + ": client only");
    }
    packet[7] = 'X';
    s_maxPlayers = -1;
    calls.clear();
    PB_handlePacket(packet, 12, "x");
    check(joined() == CLIENT_FORWARD("cl 13 PB_X1234;") + "maxPlayers;sv 13 -1 PB_X1234;", "no player slots: index -1 without a lookup");
    s_maxPlayers = 4;
    game = &server;
    s_maxPlayers = -1;
    calls.clear();
    PB_handlePacket(packet, 12, "1.2.3.4:5");
    check(joined() == CLIENT_FORWARD("cl 13 PB_X1234;") + "maxPlayers;sv 13 -1 PB_X1234;", "server, no player slots: index -1");
    s_maxPlayers = 4;
    // The server-side match: players from id -1 up, compared by connection address.
    server.playerAddresses = { "9.9.9.9:1", "", "1.2.3.4:5" };
    server.addressCalls = 0;
    calls.clear();
    PB_handlePacket(packet, 12, "1.2.3.4:5");
    check(joined() == CLIENT_FORWARD("cl 13 PB_X1234;")
                          + "maxPlayers;playerFromId -1;maxPlayers;playerFromId 0;maxPlayers;playerFromId 1;sv 13 1 PB_X1234;",
          "server: the index of the player whose address matches the sender");

    // ---- PB_examineSetting ----
    game = nullptr;
    char* answer = PB_examineSetting(const_cast<char*>("mapname"));
    check(answer[0] == 0, "no game: empty answer");
    game = &client;
    check(!std::strcmp(PB_examineSetting(const_cast<char*>("MAPNAME")), "strike_at_karkand"), "mapname (any case)");
    check(!std::strcmp(PB_examineSetting(const_cast<char*>("server")), "10.0.0.1:16567"), "server on a client: its server address");
    check(!std::strcmp(PB_examineSetting(const_cast<char*>("sv_hostname")), "10.0.0.1:16567"), "sv_hostname on a client: the server address");
    check(PB_examineSetting(const_cast<char*>("gamename"))[0] == 0, "gamename without a server: empty");
    s_svPunkBuster = true;
    check(!std::strcmp(PB_examineSetting(const_cast<char*>("sv_punkbuster")), "0"), "sv_punkbuster reads 0 on a client");
    check(PB_examineSetting(const_cast<char*>("unknown"))[0] == 0, "unknown setting: empty");
    FakePlayer host;
    host.name = "host";
    host.team = 1;
    world::s_localPlayer = &host;
    check(!std::strcmp(PB_examineSetting(const_cast<char*>("name")), "host"), "name: the local player's");
#if DEDICATED
    check(PB_examineSetting(const_cast<char*>("team"))[0] == 0, "dedicated: no team setting (empty)");
#else
    check(!std::strcmp(PB_examineSetting(const_cast<char*>("team")), "1"), "client: team is the local player's");
#endif
    world::s_localPlayer = nullptr;
    calls.clear();
    check(!std::strcmp(PB_examineSetting(const_cast<char*>("pb_isresolved master.example")), "10.1.2.3:0")
          && joined() == "resolve master.example;", "pb_isresolved: name after the 14-character prefix, resolved");
    io::s_resolves = false;
    check(PB_examineSetting(const_cast<char*>("pb_isresolved nowhere"))[0] == 0, "pb_isresolved: unresolved is empty");
    game = &server;
    check(!std::strcmp(PB_examineSetting(const_cast<char*>("server")), "192.168.0.9:16567"), "server on a server: its address");
    check(!std::strcmp(PB_examineSetting(const_cast<char*>("gamename")), "test server"), "gamename on a server");
    check(!std::strcmp(PB_examineSetting(const_cast<char*>("sv_punkbuster")), "1"), "sv_punkbuster on a server");

    // ---- sending ----
    game = &client;
    calls.clear();
#if !DEDICATED
    PB_cl_sendToServer(const_cast<char*>("data"), 4);
    check(joined() == "serverAddress;client udp 10.0.0.1:16567 " + bytes("\xff\xff\xff\xff" "data", 8) + ";",
          "to server: host and port split at ':', ffffffff + data");
    client.serverAddress = "lanhost";
    calls.clear();
    PB_cl_sendToServer(const_cast<char*>("x"), 1);
    check(joined() == "serverAddress;client udp lanhost:0 " + bytes("\xff\xff\xff\xff" "x", 5) + ";", "no ':' : port 0");
    calls.clear();
#endif
    PB_sendUDPPacket(const_cast<char*>("5.6.7.8"), 9, const_cast<char*>("ab"), 2);
    game = &server;
    PB_sendUDPPacket(const_cast<char*>("5.6.7.8"), 9, const_cast<char*>("cd"), 2);
    game = &neither;
    PB_sendUDPPacket(const_cast<char*>("5.6.7.8"), 9, const_cast<char*>("ef"), 2);
    check(joined() == "client udp 5.6.7.8:9 ab;server udp 5.6.7.8:9 cd;", "udp through the client, else the server, else nothing");

    // ---- text ----
    game = &client;
    calls.clear();
    PB_outputText(const_cast<char*>("hello"));
    PB_outputText(const_cast<char*>("[SKIPNOTIFY] quiet"));
    check(joined() == "console hello\n;chat hello\n;console [SKIPNOTIFY] quiet\n;",
          "client text: console and chat window, [skipnotify] (any case) console only");
    game = &server;
    calls.clear();
    PB_outputText(const_cast<char*>("srv"));
    check(joined() == "console srv\n;", "server text: console only");

    // ---- version and paths ----
    calls.clear();
    check(!std::strcmp(PB_getVersion(), "1.5.3153-802.0") && !std::strcmp(PB_getVersion(), "1.5.3153-802.0")
          && joined() == "buildNr;", "version: the build stamp, read once");
    char cwd[0x200];
    getcwd(cwd, sizeof(cwd));
#if DEDICATED
    calls.clear();
    check(!std::strcmp(PB_homepath(), cwd) && PB_homepath() == PB_basepath() && !std::strcmp(PB_basepath(), cwd)
          && joined() == "stringGet GSPunkBusterPath [];stringGet GSPunkBusterPath [];",
          "dedicated home path: GSPunkBusterPath re-read each call, the base path while it is empty");
    s_pbPathSetting = "/srv/pb";
    check(!std::strcmp(PB_homepath(), "/srv/pb") && PB_homepath() != PB_basepath(), "dedicated home path: the setting when set");
    s_pbPathSetting.clear();
#else
    check(!std::strcmp(PB_homepath(), cwd) && PB_basepath() == PB_homepath(), "home and base path: the working directory, one buffer");
#endif

    // ---- the library-presence tests ----
#if defined(_WIN32)
    const std::string serverLibrary = "pb/pbsv.dll";
#else
    const std::string serverLibrary = "pb/pbsv.so";
#endif
    serverSettings->m_punkBuster = true;
    calls.clear();
    check(!PB_get_sv_punkbusterInstalled(), "server library missing: not installed");
    PB_set_sv_punkbuster(0);
    PB_set_cl_punkbuster(1);
    check(serverSettings->m_punkBuster && joined() == "exists " + serverLibrary + ";exists " + serverLibrary + ";exists pb/pbcl.dll;",
          "libraries missing: neither flag is written");
    io::s_existingFiles = { serverLibrary, "pb/pbcl.dll" };
    calls.clear();
    check(PB_get_sv_punkbusterInstalled(), "server library present: installed");
    PB_set_sv_punkbuster(0);
    const bool off = !serverSettings->m_punkBuster;
    PB_set_sv_punkbuster(7);
    check(off && serverSettings->m_punkBuster, "sv_punkbuster: written as value != 0 while the library exists");
    PB_set_cl_punkbuster(0);
    PB_set_cl_punkbuster(5);
    check(joined() == "exists " + serverLibrary + ";exists " + serverLibrary + ";exists " + serverLibrary
                          + ";exists pb/pbcl.dll;boolSet GSClPunkBuster 0;exists pb/pbcl.dll;boolSet GSClPunkBuster 1;",
          "cl_punkbuster: GSClPunkBuster written as value != 0 while pb/pbcl.dll exists");
    io::s_existingFiles.clear();

    // ---- the server-side functions ----
    check(PB_maxClients() == 4, "max clients: the server's max players");
    calls.clear();
    PB_issueConsoleCommand(const_cast<char*>("pb_sv_plist"));
    check(joined() == "execute pb_sv_plist\n;", "console command: newline appended, run silently into the ignored string");
    game = &client;
    calls.clear();
    PB_DropClient(3, const_cast<char*>("cheat"));
    game = &server;
    PB_DropClient(3, const_cast<char*>("cheat"));
    check(joined() == "kick 3 34 cheat;", "drop client: PResult 0x22 with the reason, only on a server");
    server.playerAddresses = { "9.8.7.6:40000" };
    server.addressCalls = 0;
    calls.clear();
    PB_sv_sendToClient(2, const_cast<char*>("xy"), 2);
    server.playerAddresses = { "lanhost" };
    server.addressCalls = 0;
    PB_sv_sendToClient(1, const_cast<char*>("z"), 1);
    check(joined() == "playerFromId 2;server udp 9.8.7.6:40000 " + bytes("\xff\xff\xff\xff" "xy", 6)
                          + ";playerFromId 1;server udp lanhost:0 " + bytes("\xff\xff\xff\xff" "z", 5) + ";",
          "to client: that player's host and port, ffffffff + data");

    // PB_getPlayerInfo: name +0x00, key hash +0x21, address +0x42, "team=" +0x68.
    FakePlayer alice;
    alice.name = "alice";
    world::s_players[0] = &host;
    world::s_players[1] = &alice;
    world::s_localPlayer = &host;
    serverSettings->m_internet = true;
    char info[0x100];
    std::memset(info, 'q', sizeof(info));
    server.playerAddresses = { "5.5.5.5:7" };
    server.addressCalls = 0;
    calls.clear();
    check(PB_getPlayerInfo(1, info) == 1 && !std::strcmp(info, "alice") && !std::strcmp(info + 0x21, "hash1059-1")
          && !std::strcmp(info + 0x42, "5.5.5.5:7") && !std::strcmp(info + 0x68, "team=2") && info[0x63] == 'q',
          "player info: name, key hash, address, team");
    check(joined() == "playerFromId 1;ready 1;", "player info: a remote player must be a ready client");
    calls.clear();
    check(PB_getPlayerInfo(0, info) == 1 && !std::strcmp(info, "host") && !std::strcmp(info + 0x42, "localhost")
          && joined() == "playerFromId 0;", "player info: the hosting player is localhost, no readiness test");
    server.ready = false;
    check(PB_getPlayerInfo(1, info) == 0, "player info: not ready -> 0");
    server.ready = true;
    std::memset(info, 'q', sizeof(info));
    check(PB_getPlayerInfo(3, info) == 0 && info[0x42] == 0 && info[0] == 'q', "player info: no player -> 0, address cleared");
    serverSettings->m_internet = false;
    server.playerAddresses = {};
    server.addressCalls = 0;
    check(PB_getPlayerInfo(1, info) == 1 && info[0x21] == 0 && info[0x42] == 0, "player info: LAN -> no key hash; no address -> empty");
    serverSettings->m_internet = true;
    alice.connected = false;
    check(PB_getPlayerInfo(1, info) == 1 && info[0x21] == 0, "player info: not connected -> no key hash");
    game = &client;
    check(PB_getPlayerInfo(1, info) == 0, "player info: no server -> 0");
    world::s_localPlayer = nullptr;
    check(PB_GlQuery(nullptr) == 0, "PB_GlQuery (the client's callback 0x194) answers 0");

    std::printf("punkbuster game runtime %u/%u checks passed\n", checks - failures, checks);
    return failures ? 1 : 0;
}
