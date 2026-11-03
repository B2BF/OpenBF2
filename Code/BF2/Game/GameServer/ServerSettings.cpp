// Server configuration and validation of console settings.

#include "ServerSettings.h"
#include "ServerInfo.h"                     // the real ServerInfo - the mirrored flags below

#include "../IGame.h"                        // real IGame - `game` carries its real type now
#include "../IGameServer.h"                  // real IGameServer - retires the callIsRunning shim
#include "../IGameLogic.h"                   // gameLogic + GameStatus/BeginRoundState - setTimeLimit
#include "../GameEvents/BeginRoundEvent.h"   // setTimeLimit re-announces the round it just moved
#include "../../Core/ClassManager.h"         // ClassManager::getSingleton  - setCoopBotCount
#include "../../Core/SmartItf.h"             // the `classManager` global itself
#include "../../Core/SettingsRepostitory.h"  // real repository - ditto `settings`
#include "../../AI/AIInterface/AIInstance.h" // g_aiInstance - ranked servers cannot use AI
#include "../../IO/Console/Console.h"        // mainConsole - load() runs the settings file
#include "../../IO/File/FileManager.h"       // fileManager - load()'s existence gate
#if !DEDICATED
#include "../GeneralSettings.h"               // setCoopBotCount's two client-only pushes
#endif

#include <algorithm>
#include <ostream>                          // save(std::ostream&) - the settings script writer
#include <sstream>                          // save() builds the whole script in memory first
#include <string>

namespace dice {
namespace hfe {

extern IGame*               game;                                        // ds:dice::hfe::game
extern const unsigned int   IID_IGameServer;
extern SettingsRepostitory* settings;                                    // ds:0x8ABBCBC
const float RANKED_FIXED_SPAWN_TIME = 15.0f;  // ds:flt_888D944 = 15.0f - (was [UNRESOLVED])
// The other two ranked-override literals the coop / team-ratio getters return ():
const float RANKED_FIXED_COOP_50    = 50.0f;   // ds:flt_888D948 - getCoopBotRatio / getCoopBotDifficulty
const float RANKED_FIXED_TEAM_RATIO = 100.0f;  // ds:flt_88844B8 - getTeamRatioPercent
const std::string& getEmptyString();
int compareIgnoreCase(const std::string&, const std::string&);

// Executes the configured settings script with relative paths temporarily disabled.
// Returns false for an empty/missing path; per-line console errors do not make
// this function fail. Restores the previous path mode afterward.
bool ServerSettings::load()
{
    if (m_configFile.empty()
        || !io::fileManager->fileExists(m_configFile))    // vt +0x1C on the singleton
        return false;

    const bool useRelative = io::mainConsole->getUseRelativePaths();
    io::mainConsole->setUseRelativePaths(false);

    std::string out;
    io::mainConsole->run(m_configFile, "", "", "", "", "", "", "", "", out);  // +0x28

    io::mainConsole->setUseRelativePaths(useRelative);
    return true;
}

// Writes settings as console commands, flushing each line. Only ranked servers emit sv.ranked.
// interfaceIP, voipServerRemoteIP and voipSharedPassword are unquoted, so spaces do not round-trip.
// The explicit integer casts on selected flags preserve their output overloads.
void ServerSettings::save(std::ostream& out)
{
    out << "sv.serverName \"" << m_serverName << '"' << std::endl;
    out << "sv.password \"" << m_password << '"' << std::endl;
    out << "sv.internet " << m_internet << std::endl;
    out << "sv.bandwidthChoke " << m_bandwidthChoke << std::endl;
    out << "sv.serverIP \"" << m_serverIP << "\"" << std::endl;
    out << "sv.serverPort " << m_serverPort << std::endl;
    out << "sv.welcomeMessage \"" << m_welcomeMessage << '"' << std::endl;
    out << "sv.punkBuster " << m_punkBuster << std::endl;
    out << "sv.allowFreeCam " << static_cast<int>(m_allowFreeCam) << std::endl;
    out << "sv.allowExternalViews " << static_cast<int>(m_allowExternalViews) << std::endl;
    out << "sv.allowNoseCam " << static_cast<int>(m_allowNoseCam) << std::endl;
    out << "sv.hitIndicator " << static_cast<int>(m_hitIndicator) << std::endl;
    out << "sv.maxPlayers " << m_maxPlayers << std::endl;
    out << "sv.numPlayersNeededToStart " << m_playersNeededToStart << std::endl;
    out << "sv.notEnoughPlayersRestartDelay " << m_notEnoughPlayersRestartDelay << std::endl;
    out << "sv.startDelay " << m_startDelay << std::endl;
    out << "sv.endDelay " << m_endDelay << std::endl;
    out << "sv.spawnTime " << m_spawnTime << std::endl;
    out << "sv.manDownTime " << m_manDownTime << std::endl;
    out << "sv.endOfRoundDelay " << m_endOfRoundDelay << std::endl;
    out << "sv.ticketRatio " << m_ticketRatio << std::endl;
    out << "sv.roundsPerMap " << m_roundsPerMap << std::endl;
    out << "sv.timeLimit " << m_timeLimit << std::endl;
    out << "sv.scoreLimit " << m_scoreLimit << std::endl;
    out << "sv.soldierFriendlyFire " << m_soldierFriendlyFire << std::endl;
    out << "sv.vehicleFriendlyFire " << m_vehicleFriendlyFire << std::endl;
    out << "sv.soldierSplashFriendlyFire " << m_soldierSplashFriendlyFire << std::endl;
    out << "sv.vehicleSplashFriendlyFire " << m_vehicleSplashFriendlyFire << std::endl;
    out << "sv.tkPunishEnabled " << m_tkPunishEnabled << std::endl;
    out << "sv.tkNumPunishToKick " << m_tkNumPunishToKick << std::endl;
    out << "sv.tkPunishByDefault " << m_tkPunishDefault << std::endl;
    out << "sv.votingEnabled " << m_votingEnabled << std::endl;
    out << "sv.voteTime " << m_voteTime << std::endl;
    out << "sv.minPlayersForVoting " << m_minPlayersForVoting << std::endl;
    out << "sv.teamVoteOnly " << m_teamVoteOnly << std::endl;
    out << "sv.gameSpyPort " << m_gameSpyPort << std::endl;
    out << "sv.allowNATNegotiation " << m_allowNATNegotiation << std::endl;
    out << "sv.interfaceIP " << m_interfaceIP << std::endl;
    out << "sv.autoRecord " << static_cast<int>(m_autoRecord) << std::endl;
    out << "sv.demoIndexURL \"" << m_demoIndexURL << '"' << std::endl;
    out << "sv.demoDownloadURL \"" << m_demoDownloadURL << '"' << std::endl;
    out << "sv.autoDemoHook \"" << m_autoDemoHook << '"' << std::endl;
    out << "sv.demoQuality " << m_demoQuality << std::endl;
    out << "sv.adminScript \"" << m_adminScript << '"' << std::endl;
    out << "sv.timeBeforeRestartMap " << m_timeBeforeRestartMap << std::endl;
    out << "sv.autoBalanceTeam " << m_autoBalanceTeam << std::endl;
    out << "sv.teamRatioPercent " << m_teamRatioPercent << std::endl;
    out << "sv.coopBotRatio " << m_coopBotRatio << std::endl;
    out << "sv.coopBotCount " << m_coopBotCount << std::endl;
    out << "sv.coopBotDifficulty " << m_coopBotDifficulty << std::endl;
    out << "sv.voipEnabled " << m_voipEnabled << std::endl;
    out << "sv.voipQuality " << m_voipQuality << std::endl;
    out << "sv.voipServerRemote " << m_voipServerRemote << std::endl;
    out << "sv.voipServerRemoteIP " << m_voipServerRemoteIP << std::endl;
    out << "sv.voipServerPort " << m_voipServerPort << std::endl;
    out << "sv.voipBFClientPort " << m_voipBFClientPort << std::endl;
    out << "sv.voipBFServerPort " << m_voipBFServerPort << std::endl;
    out << "sv.voipSharedPassword " << m_voipSharedPassword << std::endl;
    out << "sv.useGlobalRank " << m_useGlobalRank << std::endl;
    out << "sv.useGlobalUnlocks " << m_useGlobalUnlocks << std::endl;
    if (m_ranked)
        out << "sv.ranked " << m_ranked << std::endl;
    out << "sv.noVehicles " << m_noVehicles << std::endl;
    out << "sv.sponsorText \"" << m_sponsorText << '"' << std::endl;
    out << "sv.sponsorLogoURL \"" << m_sponsorLogoURL << '"' << std::endl;
    out << "sv.communityLogoURL \"" << m_communityLogoURL << '"' << std::endl;
    out << "sv.radioSpamInterval " << m_radioSpamInterval << std::endl;
    out << "sv.radioMaxSpamFlagCount " << m_radioMaxSpamFlagCount << std::endl;
    out << "sv.radioBlockedDurationTime " << m_radioBlockedDuration << std::endl;
    out << "sv.numReservedSlots " << m_numReservedSlots << std::endl;
    out << "sv.friendlyFireWithMines " << m_friendlyFireWithMines << std::endl;
}

// Serializes settings and writes the script in one operation. Returns false for
// a missing filename, failed open, or incomplete write. Always releases the stream.
bool ServerSettings::save()
{
    if (m_configFile.empty())
        return false;
    io::IStream* stream = io::fileManager->open(m_configFile, io::FileMode(0), io::FileAccess(2), 0);
    if (!stream)
        return false;
    std::ostringstream text;
    save(text);
    const std::string script = text.str();
    const unsigned int written = stream->write(script.data(), static_cast<unsigned int>(script.size()));
    stream->release();
    return written == script.size();
}

// Initializes default server settings, finishing with setMaxPlayers(16) so an
// available command-line override can take precedence. m_gap90 is left untouched.
ServerSettings::ServerSettings()
    : m_voipSharedPassword("")           // the one literal-constructed string - see above
{
    // Stores in the constructor's own order (which is offset order, bar the compiler's string
    // refcount interleavings). Right-hand comments: the ELF store site.
    m_internet                     = false;    // +0x0C
    m_bandwidthChoke               = 0;        // +0x10
    m_maxConnectionType            = 5;        // +0x14    sv.maxConnectionType
    m_serverPort                   = 16567;    // +0x20    the famous default port
    m_votingEnabled                = true;     // +0x24
    m_voteTime                     = 60;       // +0x28
    m_minPlayersForVoting          = 2;        // +0x2C
    m_teamVoteOnly                 = true;     // +0x30
    m_voipEnabled                  = true;     // +0x31
    m_voipQuality                  = 3;        // +0x34
    m_voipServerRemote             = false;    // +0x38
    m_voipServerPort               = 55125;    // +0x40
    m_voipBFClientPort             = 55123;    // +0x44
    m_voipBFServerPort             = 55124;    // +0x48
    m_allowFreeCam                 = true;     // +0x50
    m_notEnoughPlayersRestartDelay = 5.0f;     // +0x5C
    m_spawnTime                    = 5.0f;     // +0x68
    m_manDownTime                  = 10.0f;    // +0x6C
    m_soldierFriendlyFire          = 100;      // +0x80
    m_vehicleFriendlyFire          = 100;      // +0x84
    m_allowExternalViews           = true;     // +0x51
    m_allowNoseCam                 = true;     // +0x52
    m_hitIndicator                 = true;     // +0x53
    m_maxPlayers                   = 0;        // +0x54    overwritten by the call below
    m_playersNeededToStart         = 2;        // +0x58
    m_startDelay                   = 2;        // +0x60
    m_endDelay                     = 10;       // +0x64
    m_ticketRatio                  = 100;      // +0x70
    m_roundsPerMap                 = 3;        // +0x74
    m_timeLimit                    = 0;        // +0x78
    m_scoreLimit                   = 0;        // +0x7C
    m_soldierSplashFriendlyFire    = 100;      // +0x88
    m_vehicleSplashFriendlyFire    = 100;      // +0x8C
    m_tkPunishEnabled              = false;    // +0x98
    m_tkNumPunishToKick            = 3;        // +0x9C
    m_tkPunishDefault              = true;     // +0xA0
    m_allowNATNegotiation          = false;    // +0xAC
    m_gameSpyPort                  = 29900;    // +0xA8
    m_autoRecord                   = false;    // +0xAD
    m_demoQuality                  = 1;        // +0xBC
    m_autoBalanceTeam              = false;    // +0xC8
    m_timeBeforeRestartMap         = 30.0f;    // +0xC4    0x41F00000 - a float
    m_teamRatioPercent             = 100.0f;   // +0xCC
    m_coopBotRatio                 = 50.0f;    // +0xD0
    m_coopBotCount                 = 16;       // +0xD4
    m_coopBotDifficulty            = 50.0f;    // +0xD8
    m_radioSpamInterval            = 6;        // +0xE8
    m_radioMaxSpamFlagCount        = 6;        // +0xEC
    m_radioBlockedDuration         = 30;       // +0xF0
    m_endOfRoundDelay              = 15;       // +0xF8
    m_numReservedSlots             = 0;        // +0xFC    via xor - NOT 15
    m_punkBuster                   = false;    // +0xF4
    m_ranked                       = false;    // +0xF5
    m_useGlobalRank                = true;     // +0xF6
    m_useGlobalUnlocks             = true;     // +0xF7
    m_friendlyFireWithMines        = false;    // +0x100
    m_noVehicles                   = false;    // +0x101

    setMaxPlayers(16);                         // the result string is discarded
}

// instance - ELF 0x80BAC20, 72 bytes.  A Meyers singleton: guarded function-local
// static (object 0x8AAB7C0, guard byte 0x8AAB7A8, dtor registered via __cxa_atexit). NOT a heap
// allocation, and it does NOT write the `serverSettings` global - the TU's dynamic initialiser
// below does that.
ServerSettings* ServerSettings::instance()
{
    static ServerSettings s;
    return &s;
}

// Publish server settings before console registration and main. Constructor reads
// of game/settings tolerate their zero-initialized null pointers.
ServerSettings* serverSettings = ServerSettings::instance();   // ds:0x8AAB7A0

// Returns the configured spawn time, except ranked servers always use 15 seconds.
float ServerSettings::getSpawnTime()
{
    if (m_ranked)                       // +0xF5
        return RANKED_FIXED_SPAWN_TIME; // ds:flt_888D944  [UNRESOLVED] value
    return m_spawnTime;                 // +0x68
}

// Sets internet mode before server initialization. Returns refusal text while the
// server is initialized, otherwise an empty string. LAN mode skips backend registration.
std::string ServerSettings::setInternet(bool value)
{
    IGameServer* server = 0;
    if (game != 0)
        server = (IGameServer*)game->queryInterface(IID_IGameServer);   // vt +0x0C

    if (m_internet != value)            // +0x0C
    {
        // vt +0x88 on the IGameServer = getInitComplete - see the note at the top of this file.
        if (server != 0 && server->getInitComplete())
            return std::string("Cannot change sv.internet while running");
        m_internet = value;
    }
    return getEmptyString();
}

// Sets the player limit, refusing a changed value after server initialization.
// Caps the request at 64 without a lower bound; a nonzero command-line override
// takes precedence. Returns an empty string on success or the refusal text.
std::string ServerSettings::setMaxPlayers(int value)
{
    IGameServer* server = 0;
    if (game != 0)
        server = (IGameServer*)game->queryInterface(IID_IGameServer);   // vt +0x0C

    if (value != m_maxPlayers && server != 0 && server->getInitComplete())   // vt +0x88
        return std::string("Cannot change sv.maxPlayers while running.");

    int clamped = (value < 65) ? value : 64;              // upper clamp only

    if (settings != 0)
    {
        const int commandLine = settings->intGet(std::string("GSCommandLineMaxNrOfPlayers"), 0);
        if (commandLine != 0)
        {
            m_maxPlayers = commandLine;                   // the command line wins outright
            return getEmptyString();
        }
        settings->intSet(std::string("GSMaxNrOfPlayers"), clamped);
    }
    m_maxPlayers = clamped;                               // +0x54
    return getEmptyString();
}

// Ranked servers report at least 16 slots. The console getter reads the raw
// configured field and therefore bypasses this floor.
int ServerSettings::getMaxPlayers() const
{
    if (m_ranked)                       // +0xF5
        return (m_maxPlayers > 16) ? m_maxPlayers : 16;
    return m_maxPlayers;                // +0x54
}

// Returns the no-vehicles setting. Reading it in gpm_coop clears the stored flag,
// so subsequent reads remain false until the setting is changed.
bool ServerSettings::getNoVehicles()
{
    std::string mode = settings->stringGet("GSGameMode", "gpm_cq");   // vt +0x40 (= the shim-era
                                                                      // "getString"; real name)

    if (compareIgnoreCase(mode, std::string("gpm_coop")) == 0)
        m_noVehicles = false;           // +0x101 - cleared in coop, not just ignored

    return m_noVehicles;
}

// getNumReservedSlots - ELF, 14 bytes.  The simplest shape in the class.
//     mov eax,[ebp+this] / mov eax,[eax+0FCh] / retn
int ServerSettings::getNumReservedSlots() const
{
    return m_numReservedSlots;          // +0xFC
}

// Ranked servers report a ticket ratio of 100 regardless of the configured field.
int ServerSettings::getTicketRatio() const
{
    if (m_ranked)                       // +0xF5
        return 100;
    return m_ticketRatio;               // +0x70
}

// Ranked servers use the same 15-second override as getSpawnTime.
float ServerSettings::getManDownTime() const
{
    if (m_ranked)                       // +0xF5
        return RANKED_FIXED_SPAWN_TIME; // ds:flt_888D944 - shared with getSpawnTime
    return m_manDownTime;               // +0x6C
}

// Number of players the round waits for: ranked servers use 6, or 8 from 33 slots up; every
// other server uses the configured sv.numPlayersNeededToStart, so a value of 1 starts the round
// with the first player. The following getters retain fixed ranked overrides for unlocks, coop
// settings, and team ratio.
int ServerSettings::getNumPlayersNeededToStart() const
{
    if (m_ranked)                       // +0xF5
        return (getMaxPlayers() >= 33 ? 1 : 0) * 2 + 6;
    return m_playersNeededToStart;      // +0x58
}

bool ServerSettings::getUseGlobalUnlocks() const
{
    if (m_ranked)                       // +0xF5
        return true;
    return m_internet && m_useGlobalUnlocks;   // +0x0C && +0xF7
}

// Global rank requires both ranked and internet settings before consulting its flag.
bool ServerSettings::getUseGlobalRank() const
{
    if (m_ranked && m_internet)         // +0xF5 && +0x0C
        return m_useGlobalRank;         // +0xF6
    return false;
}

float ServerSettings::getCoopBotRatio() const
{
    if (m_ranked)
        return RANKED_FIXED_COOP_50;    // flt_888D948
    return m_coopBotRatio;              // +0xD0
}

int ServerSettings::getCoopBotCount() const
{
    if (m_ranked)
        return 16;
    return m_coopBotCount;              // +0xD4
}

float ServerSettings::getCoopBotDifficulty() const
{
    if (m_ranked)
        return RANKED_FIXED_COOP_50;    // flt_888D948
    return m_coopBotDifficulty;         // +0xD8
}

float ServerSettings::getTeamRatioPercent() const
{
    if (m_ranked)
        return RANKED_FIXED_TEAM_RATIO; // flt_88844B8
    return m_teamRatioPercent;          // +0xCC
}

// Clamps round-end delay to at least ten seconds.
void ServerSettings::setEndDelay(int value)
{
    m_endDelay = (value > 10) ? value : 10;     // +0x64
}

// Updates the setting and its ServerInfo copy, then marks server information dirty.
void ServerSettings::setAllowFreeCam(bool value)
{
    m_allowFreeCam = value;             // +0x50
    serverInfo->m_block.allowFreeCam = value;   // ServerInfo +0x19 (block +0x15)
    serverInfo->internalSetDirty();
}

void ServerSettings::setAllowExternalViews(bool value)
{
    m_allowExternalViews = value;             // +0x51
    serverInfo->m_block.allowExternalViews = value;   // ServerInfo +0x1A (block +0x16)
    serverInfo->internalSetDirty();
}

// setFriendlyFireWithMines is 26 bytes and mirrors the same WAY but does NOT call
// internalSetDirty - the only setter of the three that skips it. [UNRESOLVED] whether that is
// deliberate or an omission in dice's code; reproduce it as found.
void ServerSettings::setFriendlyFireWithMines(bool value)
{
    m_friendlyFireWithMines = value;             // +0x100
    serverInfo->m_block.friendlyFireWithMines = value;   // ServerInfo +0x3A (block +0x36)
    // no internalSetDirty() here
}


// Rejects ranked mode with AI present or either stats script absent. After those
// checks, GSRanked can force it on; otherwise stores value. Requires initialized
// fileManager and settings, with no null guards.
void ServerSettings::setRanked(bool value)
{
    if ((value && ai::g_aiInstance != 0)
        || !io::fileManager->fileExists(std::string("python/bf2/stats/medals.py"))
        || !io::fileManager->fileExists(std::string("python/bf2/stats/medal_data.py")))
    {
        m_ranked = false;                              // ELF +0xF5 / PE +0x25D
        return;
    }

    if (settings->boolGet(std::string("GSRanked"), false))
        m_ranked = true;
    else
        m_ranked = value;
}


// Returns effective ranked state: AI, LAN, and gpm_coop each force false before
// the stored ranked flag is read.
bool ServerSettings::getRanked() const
{
    if (ai::g_aiInstance != 0 || !m_internet)
        return false;

    std::string mode = settings->stringGet(std::string("GSGameMode"), std::string("gpm_cq"));
    if (compareIgnoreCase(mode, std::string("gpm_coop")) == 0)
        return false;

    return m_ranked;                                    // ELF +0xF5 / PE +0x25D
}

// PunkBuster is disabled on LAN and enabled for ranked Internet servers;
// other Internet servers use the configured flag.
bool ServerSettings::getSvPunkBuster() const
{
    if (!m_internet)                                    // +0x0C
        return false;
    if (m_ranked)                                       // +0xF5
        return true;
    return m_punkBuster;                                // +0xF4
}

// Ranked servers return the shared empty password, including to NetServer setup;
// other servers return the configured password.
const std::string& ServerSettings::getPassword() const
{
    if (m_ranked)                                       // +0xF5
        return getEmptyString();
    return m_password;                                  // +0x08
}

// Mirrors the setting into ServerInfo and marks that block dirty for replication.
void ServerSettings::setAllowNoseCam(bool value)
{
    m_allowNoseCam = value;                             // +0x52
    serverInfo->m_block.allowNoseCam = value;
    serverInfo->internalSetDirty();
}

//  / PE 0x4254A0, 30 B. Block +0x13.
void ServerSettings::setHitIndicator(bool value)
{
    m_hitIndicator = value;                             // +0x53
    serverInfo->m_block.hitIndicator = value;
    serverInfo->internalSetDirty();
}

//  / PE 0x4254C0, 30 B. Block +0x14. The setter is spelled ...ByDefault while the
// field it writes is m_tkPunishDefault; that mismatch is dice's, and the console name follows the
// setter (sv.tkPunishByDefault).
void ServerSettings::setTKPunishByDefault(bool value)
{
    m_tkPunishDefault = value;                          // +0xA0
    serverInfo->m_block.tkPunishDefault = value;
    serverInfo->internalSetDirty();
}

//  / PE 0x4254E0, 35 B - five bytes longer than its neighbours for one reason: the
// mirror is written through `test al,al / setnz al`, so ServerInfo receives a normalised 0/1 while
// the settings field takes the byte as given. Every other mirror in this file stores al directly.
void ServerSettings::setVotingEnabled(bool value)
{
    m_votingEnabled = value;                            // +0x24
    serverInfo->m_block.votingEnabled = (value != 0) ? 1 : 0;
    serverInfo->internalSetDirty();
}

//  / PE 0x425710, 30 B. Block +0x37.
void ServerSettings::setTeamVoteOnly(bool value)
{
    m_teamVoteOnly = value;                             // +0x30
    serverInfo->m_block.teamVoteOnly = value;
    serverInfo->internalSetDirty();
}

// The three radio limits, ELF 0x80BBDF0 / 0x80BBE20 / 0x80BBE50 (PE 0x425510 / 0x425530 /
// ), 30 B each. Blocks +0x24 / +0x28 / +0x2C. No range check on any of them.
void ServerSettings::setRadioSpamInterval(int value)
{
    m_radioSpamInterval = value;                        // +0xE8
    serverInfo->m_block.radioSpamInterval = value;
    serverInfo->internalSetDirty();
}

void ServerSettings::setRadioMaxSpamFlagCount(int value)
{
    m_radioMaxSpamFlagCount = value;                    // +0xEC
    serverInfo->m_block.radioMaxSpamFlagCount = value;
    serverInfo->internalSetDirty();
}

void ServerSettings::setRadioBlockedDurationTime(int value)
{
    m_radioBlockedDuration = value;                     // +0xF0
    serverInfo->m_block.radioBlockedDuration = value;
    serverInfo->internalSetDirty();
}

// Clamps the end-of-round delay to 15–30 seconds.
void ServerSettings::setEndOfRoundDelay(int value)
{
    int clamped = 15;
    if (value > 14)
    {
        clamped = value;
        if (value >= 31)
            clamped = 30;
    }
    m_endOfRoundDelay = clamped;                        // +0xF8
}

// Clamps the limit to zero and, during server play, shifts the round end by 30 times the change.
// Broadcasts the new countdown after updating it. Requires game on that path and reads
// no-vehicles state through the global settings owner, whose getter may change it.
void ServerSettings::setTimeLimit(int value)
{
    const int requested = (value < 0) ? 0 : value;
    const int previous  = m_timeLimit;                  // +0x78
    m_timeLimit = requested;

    if (gameLogic == 0)
        return;
    if (!gameLogic->isServer())                                     // +0x13C
        return;
    if (gameLogic->getGameStatus() != GameStatus_Playing)           // +0x158
        return;

    gameLogic->setNextStateChangeAtTick(                            // +0x168
        gameLogic->getNextStateChangeAtTick() + 30 * (m_timeLimit - previous));   // +0x164

    BeginRoundEvent* event = new BeginRoundEvent(
        gameLogic->getBeginRoundState(),                            // +0x15C
        gameLogic->getNextStateChangeAtTick(),                      // +0x164, read a second time
        serverSettings->getNoVehicles(),                            // on the singleton, not `this`
        false);
    game->sendGameEventToAllConnected(event);                       // +0x94
}

// Clamps the bot count to 1–48 and mirrors it into both GeneralSettings count fields
// when that singleton exists. [INFERRED] Method name.
void ServerSettings::setCoopBotCount(int value)
{
    int clamped = value;
    if (value >= 1)
    {
        if (value > 48)
            clamped = 48;
    }
    else
        clamped = 1;

    m_coopBotCount = clamped;                           // +0xD4

#if !DEDICATED
    // Client-only: the dedicated image never registers this singleton, so the shipped lookup always
    // answered null there and the two pushes never happened.
    GeneralSettings* generalSettings =
        static_cast<GeneralSettings*>(classManager->getSingleton(std::string("GeneralSettings")));   // +0x4C
    if (generalSettings)
    {
        generalSettings->setMaxBots(m_coopBotCount);        // +0xB4
        generalSettings->setCoopBotCount(m_coopBotCount);   // +0xF4
    }
#endif
}

// shape B, continued: the one setter that reaches ServerInfo's welcome list
//  / PE 0x4250C0, 40 B. Publishes first, stores second - ServerInfo::setWelcomeMsg
// (which truncates to 256 characters and marks the object dirty) runs before m_welcomeMessage is
// assigned, so the browser copy and the settings copy can differ in length. Index 0 is a literal.
void ServerSettings::setWelcomeMessage(const std::string& value)
{
    serverInfo->setWelcomeMsg(0, value);
    m_welcomeMessage = value;                           // +0x18
}

// Refuses password changes while running; preserves the original sv.setPassword diagnostic text.
std::string ServerSettings::setPassword(const std::string& value)
{
    IGameServer* server = 0;
    if (game != 0)
        server = (IGameServer*)game->queryInterface(IID_IGameServer);   // vt +0x0C

    if (server != 0 && server->getInitComplete())                       // vt +0x88
        return std::string("Cannot change sv.setPassword while running.");

    m_password = value;                                 // +0x08
    return getEmptyString();
}

// Rejects ports outside 0–65535 before checking whether the server is running.
std::string ServerSettings::setServerPort(int value)
{
    if (static_cast<unsigned int>(value) > 0xFFFFu)
        return std::string("illegal port value");

    IGameServer* server = 0;
    if (game != 0)
        server = (IGameServer*)game->queryInterface(IID_IGameServer);   // vt +0x0C

    if (server != 0 && server->getInitComplete())                       // vt +0x88
        return std::string("Cannot change sv.serverPort while running.");

    m_serverPort = value;                               // +0x20
    return getEmptyString();
}

//  / PE 0x425670, 102 B. Stores the byte as given; the reading side (getNoVehicles)
// is what forces it false outside gpm_coop, not this.
std::string ServerSettings::setNoVehicles(bool value)
{
    IGameServer* server = 0;
    if (game != 0)
        server = (IGameServer*)game->queryInterface(IID_IGameServer);   // vt +0x0C

    if (server != 0 && server->getInitComplete())                       // vt +0x88
        return std::string("Cannot change sv.noVehicles while running.");

    m_noVehicles = value;                               // +0x101
    return getEmptyString();
}

// Clamps reserved slots to zero through one quarter of max players.
// The repeated getMaxPlayers call is observable and must remain.
std::string ServerSettings::setNumReservedSlots(int value)
{
    IGameServer* server = 0;
    if (game != 0)
        server = (IGameServer*)game->queryInterface(IID_IGameServer);   // vt +0x0C

    if (server != 0 && server->getInitComplete())                       // vt +0x88
        return std::string("Cannot change sv.numReservedSlots while running.");

    int wanted = value;
    if (value < 0)
        wanted = 0;

    if (wanted > getMaxPlayers() / 4)
        wanted = getMaxPlayers() / 4;                   // second call, as shipped

    m_numReservedSlots = wanted;                        // +0xFC
    return getEmptyString();
}

} // namespace hfe
} // namespace dice
