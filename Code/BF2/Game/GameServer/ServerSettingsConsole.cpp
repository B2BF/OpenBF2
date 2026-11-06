// Server-settings console bindings, registered by ModuleGame. [CHOSEN, not recovered]
// Separate translation unit. Bindings use access level 1, followed by access level 2
// for ranked/PunkBuster settings; sv.save remains deliberately unregistered.
#include "../../Core/ConsoleObjectMacro.h"
#include "ServerSettings.h"
#include "../IGame.h"
#include <algorithm>
#include <string>

namespace dice {
namespace hfe {

extern IGame*             game;              // ds:dice::hfe::game
extern const unsigned int IID_IGameClient;   // Core/Globals.cpp

namespace {

// Stands in for the original's file-scope list (ELF 0x8B1704C). ModuleGame::init registers it.
const char* const kSvConsoleTag = "hfe.sv";

// Resolves server settings before a game exists or for a server game. A game
// exposing IGameClient makes these bindings inactive by returning null.
ServerSettings* svReceiver()
{
    if (game != 0 && game->queryInterface(IID_IGameClient) != 0)
        return 0;                        // client arm - dead on a dedicated server
    return serverSettings;               // ds:0x8AAB7A0
}

// the free-function targets, one per binding whose shape needs an expression

// sv.load's executor (PE 0x4CE9C0, 88 B) accepts argc 0 only, sets m_hasReturnValue and returns
// ServerSettings::load()'s bool. Re-running serversettings.con at run time is retail behaviour.
bool svLoad()
{
    return svReceiver()->load();
}

// sv.save's executor (PE 0x4CEAB0) is the same three-line shape, over ServerSettings::save() -
// which answers whether the whole script reached the file.
bool svSave()
{
    return svReceiver()->save();
}

// The SET target the macro calls with the parsed int. The status-string return travels back as the
// console's reply (empty = success) - see BF2_CONSOLE_GETSET_1's header note.
std::string svMaxPlayersSet(int value)
{
    return svReceiver()->setMaxPlayers(value);
}

// The other three status-string members. Each surfaces its refusal text as the console reply, and
// each GET reads the RAW field rather than the accessor - notably sv.password, whose getter would
// blank the value on a ranked server while the console binding shows what was configured.
std::string svPasswordSet(const std::string& value)
{
    return svReceiver()->setPassword(value);
}

std::string svServerPortSet(int value)
{
    return svReceiver()->setServerPort(value);
}

std::string svNumReservedSlotsSet(int value)
{
    return svReceiver()->setNumReservedSlots(value);
}

// Stores the requested player count unchanged, as retail does; the getter applies the ranked
// override.
void svNumPlayersNeededToStartSet(int value)
{
    svReceiver()->m_playersNeededToStart = value;
}

// sv.internet's generated class has the same status-returning argc-0/1 method shape as
// sv.maxPlayers. SET calls the real setter (and therefore surfaces its refusal string); GET reads
// the raw field rather than routing through another accessor.
std::string svInternetSet(bool value)
{
    return svReceiver()->setInternet(value);
}

// sv.punkBuster is intentionally asymmetric: SET writes the stored setting directly, while GET
// calls getSvPunkBuster(), whose result also incorporates sv.internet and sv.ranked.
void svPunkBusterSet(bool value)
{
    svReceiver()->m_punkBuster = value;
}

// The console exposes noVehicles as float and converts nonzero to true.
// Setter errors are discarded, so a refused change produces no console reply.
void svNoVehiclesSet(float value)
{
    svReceiver()->setNoVehicles(value != 0.0f);   // status string constructed and discarded
}

} // namespace

// These console bindings snapshot the argument, clear the reply flag, then
// resolve the receiver so queryInterface cannot alter the chosen value.
// BF2_SV_FIELD_AS also permits a metadata spelling distinct from the C++ type.
#define BF2_SV_FIELD_AS(fnName, T, spelling, member)                                 \
namespace {                                                                          \
class BF2_CC_NAME : public ConsoleObjectBaseImpl                                     \
{                                                                                    \
    BF2_CC_HEAD(fnName, 1, 0, 1, spelling, true)                                     \
        m_argTypes[0] = spelling;                                                    \
    BF2_CC_TAIL(1, kSvConsoleTag)                                                    \
    virtual bool isObjectActive() const { return svReceiver() != 0; }                \
    virtual void executeObjectMethod()                                               \
    {                                                                                \
        if (m_argCount == 1)                                                         \
        {                                                                            \
            const T value = BF2_CC_ARG(T, 0);                                        \
            m_hasReturnValue = false;                                                \
            svReceiver()->member = value;                                            \
        }                                                                            \
        else if (m_argCount == 0)                                                    \
        {                                                                            \
            m_hasReturnValue = true;                                                 \
            m_returnValue = svReceiver()->member;                                    \
        }                                                                            \
    }                                                                                \
    virtual void setArgFromString(int index, const std::string& value)               \
    { BF2_CC_CONVERT(T, 1) }                                                         \
    BF2_CC_RETURN_AS_STRING                                                          \
    BF2_CC_COMMON_HOOKS                                                              \
private:                                                                             \
    T m_returnValue;                                                                 \
};                                                                                   \
static BF2_CC_NAME BF2_CC_INSTANCE;                                                  \
}

#define BF2_SV_FIELD(fnName, T, member)  BF2_SV_FIELD_AS(fnName, T, #T, member)

// GET reads the field raw; SET goes through a ServerSettings method (which is where the mirrors,
// dirty flags and clamps live). The snapshot is taken before the setter's own side effects.
#define BF2_SV_SETTER(fnName, T, member, setter)                                     \
namespace {                                                                          \
class BF2_CC_NAME : public ConsoleObjectBaseImpl                                     \
{                                                                                    \
    BF2_CC_HEAD(fnName, 1, 0, 1, #T, true)                                           \
        m_argTypes[0] = #T;                                                          \
    BF2_CC_TAIL(1, kSvConsoleTag)                                                    \
    virtual bool isObjectActive() const { return svReceiver() != 0; }                \
    virtual void executeObjectMethod()                                               \
    {                                                                                \
        if (m_argCount == 1)                                                         \
        {                                                                            \
            const T value = BF2_CC_ARG(T, 0);                                        \
            m_hasReturnValue = false;                                                \
            svReceiver()->setter(value);                                             \
        }                                                                            \
        else if (m_argCount == 0)                                                    \
        {                                                                            \
            m_hasReturnValue = true;                                                 \
            m_returnValue = svReceiver()->member;                                    \
        }                                                                            \
    }                                                                                \
    virtual void setArgFromString(int index, const std::string& value)               \
    { BF2_CC_CONVERT(T, 1) }                                                         \
    BF2_CC_RETURN_AS_STRING                                                          \
    BF2_CC_COMMON_HOOKS                                                              \
private:                                                                             \
    T m_returnValue;                                                                 \
};                                                                                   \
static BF2_CC_NAME BF2_CC_INSTANCE;                                                  \
}

// The mirror image: GET goes through an accessor (all of these carry a ranked override, so the
// console reports the effective value), SET writes the field raw and is therefore ignored while
// ranked rather than refused.
#define BF2_SV_GETTER(fnName, T, getter, member)                                     \
namespace {                                                                          \
class BF2_CC_NAME : public ConsoleObjectBaseImpl                                     \
{                                                                                    \
    BF2_CC_HEAD(fnName, 1, 0, 1, #T, true)                                           \
        m_argTypes[0] = #T;                                                          \
    BF2_CC_TAIL(1, kSvConsoleTag)                                                    \
    virtual bool isObjectActive() const { return svReceiver() != 0; }                \
    virtual void executeObjectMethod()                                               \
    {                                                                                \
        if (m_argCount == 1)                                                         \
        {                                                                            \
            const T value = BF2_CC_ARG(T, 0);                                        \
            m_hasReturnValue = false;                                                \
            svReceiver()->member = value;                                            \
        }                                                                            \
        else if (m_argCount == 0)                                                    \
        {                                                                            \
            m_hasReturnValue = true;                                                 \
            m_returnValue = svReceiver()->getter();                                  \
        }                                                                            \
    }                                                                                \
    virtual void setArgFromString(int index, const std::string& value)               \
    { BF2_CC_CONVERT(T, 1) }                                                         \
    BF2_CC_RETURN_AS_STRING                                                          \
    BF2_CC_COMMON_HOOKS                                                              \
private:                                                                             \
    T m_returnValue;                                                                 \
};                                                                                   \
static BF2_CC_NAME BF2_CC_INSTANCE;                                                  \
}

// Integer clamp inside THE binding - there is no setter for these five. The executor's exact shape
// is `if (v < lo) { f = lo; return; } t = hi; if (v <= hi) t = v; f = t;`, and the receiver is
// resolved before the range test, so the branch order below is the shipped one.
#define BF2_SV_CLAMPED(fnName, T, member, lo, hi)                                    \
namespace {                                                                          \
class BF2_CC_NAME : public ConsoleObjectBaseImpl                                     \
{                                                                                    \
    BF2_CC_HEAD(fnName, 1, 0, 1, #T, true)                                           \
        m_argTypes[0] = #T;                                                          \
    BF2_CC_TAIL(1, kSvConsoleTag)                                                    \
    virtual bool isObjectActive() const { return svReceiver() != 0; }                \
    virtual void executeObjectMethod()                                               \
    {                                                                                \
        if (m_argCount == 1)                                                         \
        {                                                                            \
            const T value = BF2_CC_ARG(T, 0);                                        \
            m_hasReturnValue = false;                                                \
            ServerSettings* receiver = svReceiver();                                 \
            if (value < (lo))                                                        \
                receiver->member = (lo);                                             \
            else                                                                     \
                receiver->member = (value <= (hi)) ? value : (hi);                   \
        }                                                                            \
        else if (m_argCount == 0)                                                    \
        {                                                                            \
            m_hasReturnValue = true;                                                 \
            m_returnValue = svReceiver()->member;                                    \
        }                                                                            \
    }                                                                                \
    virtual void setArgFromString(int index, const std::string& value)               \
    { BF2_CC_CONVERT(T, 1) }                                                         \
    BF2_CC_RETURN_AS_STRING                                                          \
    BF2_CC_COMMON_HOOKS                                                              \
private:                                                                             \
    T m_returnValue;                                                                 \
};                                                                                   \
static BF2_CC_NAME BF2_CC_INSTANCE;                                                  \
}

// Clamps coop floats with NaN mapping to the lower bound. Preserve the min/max argument order.
#define BF2_SV_GETTER_CLAMPF(fnName, getter, member, lo, hi)                         \
namespace {                                                                          \
class BF2_CC_NAME : public ConsoleObjectBaseImpl                                     \
{                                                                                    \
    BF2_CC_HEAD(fnName, 1, 0, 1, "float", true)                                      \
        m_argTypes[0] = "float";                                                     \
    BF2_CC_TAIL(1, kSvConsoleTag)                                                    \
    virtual bool isObjectActive() const { return svReceiver() != 0; }                \
    virtual void executeObjectMethod()                                               \
    {                                                                                \
        if (m_argCount == 1)                                                         \
        {                                                                            \
            const float value = BF2_CC_ARG(float, 0);                                \
            m_hasReturnValue = false;                                                \
            ServerSettings* receiver = svReceiver();                                 \
            receiver->member = std::max((lo), std::min(value, (hi)));                \
        }                                                                            \
        else if (m_argCount == 0)                                                    \
        {                                                                            \
            m_hasReturnValue = true;                                                 \
            m_returnValue = svReceiver()->getter();                                  \
        }                                                                            \
    }                                                                                \
    virtual void setArgFromString(int index, const std::string& value)               \
    { BF2_CC_CONVERT(float, 1) }                                                     \
    BF2_CC_RETURN_AS_STRING                                                          \
    BF2_CC_COMMON_HOOKS                                                              \
private:                                                                             \
    float m_returnValue;                                                             \
};                                                                                   \
static BF2_CC_NAME BF2_CC_INSTANCE;                                                  \
}

// access 1. PE init slot 0x82AC70 -> 0x7611C0; ELF 0x87B2299 executes before the registrations
// below. The ELF dispatcher at 0x87ABCF4 proves that order. Registration order below is the PE
// initializer table's (VA, not file offsets), slot by slot from 0x82AC74 to 0x82AD8C.
BF2_CONSOLE_OBJECT("sv", 1, 0)

// > ctor 0x4CE930 / executor 0x4CE9C0. Method, not a property: argc 0 only, bool reply.
BF2_CONSOLE_METHOD_0_RET_ACTIVE(load, svLoad, bool, svReceiver() != 0, kSvConsoleTag)

// Saves server settings and returns whether the write succeeded.
BF2_CONSOLE_METHOD_0_RET_ACTIVE(save, svSave, bool, svReceiver() != 0, kSvConsoleTag)

// > ctor 0x4CEB10 / executor 0x4CEC00. Raw std::string both ways, no side effect: this
// only names the file a later sv.load would run, it does not run it.
BF2_SV_FIELD(configFile, std::string, m_configFile)

// serverName: 0x82AC80 -> ctor 0x4CED40; ELF registration 0x87B202F, executor 0x87BCA40.
BF2_SV_FIELD(serverName, std::string, m_serverName)

// password: 0x82AC84 -> ctor 0x4CEF80 / executor 0x4CF0F0. Status-string family. GET streams the
// RAW field through the executor's own ostringstream (helper PE 0x4CF1F0) - it does NOT call
// getPassword(), so the console shows a ranked server's configured password even though every
// other consumer, including io::NetServer, receives the empty string.
BF2_CONSOLE_GETSET_1(password, std::string, svReceiver() != 0, svReceiver()->m_password,
                     svPasswordSet, kSvConsoleTag)

// internet: 0x82AC88 -> ctor 0x4CF240 / executor 0x4CF390. Type 0, argc 0..1;
// SET returns setInternet's status string, GET reads the raw field.
BF2_CONSOLE_GETSET_1(internet, bool, svReceiver() != 0, svReceiver()->m_internet, svInternetSet, kSvConsoleTag)

// Internet-server connection-type ceiling; the LAN path ignores it.
BF2_SV_FIELD_AS(maxConnectionType, int, "ConnectionType", m_maxConnectionType)

// welcomeMessage: 0x82AC90 -> ctor 0x4CF6F0 / executor 0x4CF7E0. SET calls setWelcomeMessage, which
// publishes into ServerInfo's welcome list (truncating to 256 chars) before storing the untruncated
// text here - so GET can return a longer string than the browser ever sees.
BF2_SV_SETTER(welcomeMessage, std::string, m_welcomeMessage, setWelcomeMessage)

// serverIP: ConsoleClass656. ELF registration 0x87B1A1C / executor 0x87BEDE0;
// PE slot 0x82AC94 -> ctor 0x4CF920 / executor 0x4CFA10. String field +0x1C ELF / +0x7C PE.
// Empty still means INADDR_ANY. No address validation or live-listener rebind is performed.
BF2_SV_FIELD(serverIP, std::string, m_serverIP)

// serverPort: ConsoleClass657, 0x82AC98 -> ctor 0x4CFB60 / executor 0x4CFCB0. Status-string family.
// It configures the stored port only: GameServer reads m_serverPort
// when it builds the listener, so a SET after startServer is refused by the setter's own
// running-server gate rather than rebinding anything, and CLI `+port` still wins where it is used.
BF2_CONSOLE_GETSET_1(serverPort, int, svReceiver() != 0, svReceiver()->m_serverPort,
                     svServerPortSet, kSvConsoleTag)

// SET updates ServerInfo and marks it dirty; GET reads the stored setting directly.
BF2_SV_SETTER(allowFreeCam, bool, m_allowFreeCam, setAllowFreeCam)

// The other three mirror-and-dirty view flags, 0x82ACA0 / 0x82ACA4 / 0x82ACA8 ->
// ctors 0x4CFFE0 / 0x4D01F0 / 0x4D0400, executors 0x4D0130 / 0x4D0340 / 0x4D0550. Same shape as
// allowFreeCam in both ports: setter on SET, stored setting on GET.
BF2_SV_SETTER(allowExternalViews, bool, m_allowExternalViews, setAllowExternalViews)
BF2_SV_SETTER(allowNoseCam,       bool, m_allowNoseCam,       setAllowNoseCam)
BF2_SV_SETTER(hitIndicator,       bool, m_hitIndicator,       setHitIndicator)

// maxPlayers: ConsoleClass665, PE slot 0x82ACAC -> ctor 0x4D0610; ELF executor 0x87C1370.
// Type 0 with argc 0..1 and std::string reply. GET bypasses the effective ranked getter.
BF2_CONSOLE_GETSET_1(maxPlayers, int, svReceiver() != 0, svReceiver()->m_maxPlayers, svMaxPlayersSet, kSvConsoleTag)

// numReservedSlots: ConsoleClass666, 0x82ACB0 -> ctor 0x4D0880 / executor 0x4D09D0 (GET helper
// ). Status-string family. The setter caps the request at getMaxPlayers()/4, and because
// getMaxPlayers has a ranked floor of 16 the effective cap on a ranked server is never below 4.
BF2_CONSOLE_GETSET_1(numReservedSlots, int, svReceiver() != 0, svReceiver()->m_numReservedSlots,
                     svNumReservedSlotsSet, kSvConsoleTag)

// numPlayersNeededToStart: ConsoleClass668, PE slot 0x82ACB4 -> ctor 0x4D0AF0 / exec 0x4D0C40;
// ELF registration 0x87B109A / executor 0x87C22A0. Retail SET is a raw field write; GET uses
// the effective ranked getter. The helper keeps that raw write.
BF2_CONSOLE_PROPERTY_FN(numPlayersNeededToStart, int, svReceiver() != 0,
                        svReceiver()->getNumPlayersNeededToStart(),
                        svNumPlayersNeededToStartSet, kSvConsoleTag)

// > ctor 0x4D0CE0 / executor 0x4D0E30. Raw float both ways, no clamp and no accessor -
// the value is bit-copied out of the argument static exactly as the executor does it.
BF2_SV_FIELD(notEnoughPlayersRestartDelay, float, m_notEnoughPlayersRestartDelay)

// > ctor 0x4D0EE0 / executor 0x4D1030. Raw int both ways.
BF2_SV_FIELD(startDelay, int, m_startDelay)

// > ctor 0x4D10D0 / executor 0x4D1220. SET goes through setEndDelay, whose floor of 10
// is why a config asking for less still ends rounds at ten seconds. GET shows the stored value,
// so a `sv.endDelay 5` followed by a read reports 10 - the clamp is visible from the console.
BF2_SV_SETTER(endDelay, int, m_endDelay, setEndDelay)

//  / 0x82ACC8 -> ctors 0x4D12C0 / 0x4D14D0, executors 0x4D1410 / 0x4D1620. Ranked-override
// getters on GET (15.0f each while ranked), raw field writes on SET.
BF2_SV_GETTER(spawnTime,   float, getSpawnTime,   m_spawnTime)
BF2_SV_GETTER(manDownTime, float, getManDownTime, m_manDownTime)

// > ctor 0x4D16E0 / executor 0x4D1830. setEndOfRoundDelay clamps to [15, 30] - the only
// two-sided clamp among the delay setters.
BF2_SV_SETTER(endOfRoundDelay, int, m_endOfRoundDelay, setEndOfRoundDelay)

// > ctor 0x4D18D0 / executor 0x4D1A20. getTicketRatio returns 100 while ranked.
BF2_SV_GETTER(ticketRatio, int, getTicketRatio, m_ticketRatio)

// > ctor 0x4D1AC0 / executor 0x4D1C10. Raw int both ways.
BF2_SV_FIELD(roundsPerMap, int, m_roundsPerMap)

// > ctor 0x4D1CB0 / executor 0x4D1E00. THE only sv.* property that reaches THE live
// round: setTimeLimit floors at zero, shifts the round's end tick by 30 * delta while playing, and
// broadcasts a BeginRoundEvent so connected clients re-sync their countdown. GET is the raw field.
BF2_SV_SETTER(timeLimit, int, m_timeLimit, setTimeLimit)

// > ctor 0x4D1EA0 / executor 0x4D1FF0. Raw int both ways - no round-side effect, unlike
// its neighbour.
BF2_SV_FIELD(scoreLimit, int, m_scoreLimit)

// The four friendly-fire percentages, 0x82ACE0..0x82ACEC -> ctors 0x4D2090 / 0x4D2290 / 0x4D2490 /
// executors 0x4D21E0 / 0x4D23E0 / 0x4D25E0 / 0x4D27E0. No setters exist; the [0, 100]
// clamp lives in the generated executor itself and all four are byte-identical bar the offset.
BF2_SV_CLAMPED(soldierFriendlyFire,       int, m_soldierFriendlyFire,       0, 100)
BF2_SV_CLAMPED(vehicleFriendlyFire,       int, m_vehicleFriendlyFire,       0, 100)
BF2_SV_CLAMPED(soldierSplashFriendlyFire, int, m_soldierSplashFriendlyFire, 0, 100)
BF2_SV_CLAMPED(vehicleSplashFriendlyFire, int, m_vehicleSplashFriendlyFire, 0, 100)

//  / 0x82ACF4 -> ctors 0x4D2890 / 0x4D2A80, executors 0x4D29E0 / 0x4D2BD0. Raw both ways:
// neither the enable flag nor the kick threshold is mirrored into ServerInfo.
BF2_SV_FIELD(tkPunishEnabled,   bool, m_tkPunishEnabled)
BF2_SV_FIELD(tkNumPunishToKick, int,  m_tkNumPunishToKick)

// > ctor 0x4D2C70 / executor 0x4D2DC0. The console name says ByDefault and the field is
// m_tkPunishDefault; the setter mirrors it into ServerInfo and marks the block dirty.
BF2_SV_SETTER(tkPunishByDefault, bool, m_tkPunishDefault, setTKPunishByDefault)

// > ctor 0x4D2E80 / executor 0x4D2FD0. setVotingEnabled is the one mirror that
// normalises its byte (setnz) before publishing it.
BF2_SV_SETTER(votingEnabled, bool, m_votingEnabled, setVotingEnabled)

//  / 0x82AD04 -> ctors 0x4D3090 / 0x4D3280, executors 0x4D31E0 / 0x4D33D0. Raw both ways.
BF2_SV_FIELD(voteTime,            int, m_voteTime)
BF2_SV_FIELD(minPlayersForVoting, int, m_minPlayersForVoting)

// > ctor 0x4D3470 / executor 0x4D35C0. Mirror-and-dirty.
BF2_SV_SETTER(teamVoteOnly, bool, m_teamVoteOnly, setTeamVoteOnly)

// voipEnabled: PE slot 0x82AD0C -> ctor 0x4D3680 / executor 0x4D37D0; direct bool field. The rest
// of the VoIP block (0x82AD10..0x82AD28) is raw both ways too - not one of them has a setter, so
// none of them is mirrored into ServerInfo or refused while the server runs.
BF2_SV_FIELD(voipEnabled,        bool,        m_voipEnabled)
BF2_SV_FIELD(voipServerRemote,   bool,        m_voipServerRemote)
BF2_SV_FIELD(voipServerRemoteIP, std::string, m_voipServerRemoteIP)
BF2_SV_FIELD(voipServerPort,     int,         m_voipServerPort)
BF2_SV_FIELD(voipBFClientPort,   int,         m_voipBFClientPort)
BF2_SV_FIELD(voipBFServerPort,   int,         m_voipBFServerPort)
BF2_SV_FIELD(voipSharedPassword, std::string, m_voipSharedPassword)
BF2_SV_FIELD(voipQuality,        int,         m_voipQuality)

// gameSpyPort: ConsoleClass705. ELF registration 0x87AED8B / executor 0x87CD540;
// PE slot 0x82AD2C -> ctor 0x4D46D0 / executor 0x4D4820. Signed int +0xA8 ELF / +0x168 PE.
// No range, narrowing, dirty notification or running-state guard belongs to this binding.
BF2_SV_FIELD(gameSpyPort, int, m_gameSpyPort)

// > ctor 0x4D48C0 / executor 0x4D4A10. Raw bool.
BF2_SV_FIELD(allowNATNegotiation, bool, m_allowNATNegotiation)

// interfaceIP: ConsoleClass707. ELF registration 0x87AEB24 / executor 0x87CE1A0;
// PE slot 0x82AD34 -> ctor 0x4D4AB0 / executor 0x4D4BA0. String +0xA4 ELF / +0x14C PE.
// GameSpyBackend copies it when constructed; a later SET does not rebind the existing socket.
BF2_SV_FIELD(interfaceIP, std::string, m_interfaceIP)

//  / 0x82AD3C -> ctors 0x4D4CF0 / 0x4D4EF0, executors 0x4D4E40 / 0x4D5040. Raw both ways.
BF2_SV_FIELD(timeBeforeRestartMap, float, m_timeBeforeRestartMap)
BF2_SV_FIELD(autoBalanceTeam,      bool,  m_autoBalanceTeam)

// > ctor 0x4D50E0 / executor 0x4D5230. getTeamRatioPercent returns 100.0f while ranked.
BF2_SV_GETTER(teamRatioPercent, float, getTeamRatioPercent, m_teamRatioPercent)

// The coop trio, 0x82AD44 / 0x82AD48 / 0x82AD4C -> ctors 0x4D52F0 / 0x4D5500 / 0x4D56F0,
// executors 0x4D5440 / 0x4D5650 / 0x4D5840. All three GET through ranked-overriding accessors
// (50.0f / 16 / 50.0f while ranked). The two floats clamp in the binding; the count has a real
// setter which clamps to [1, 48] and republishes into the "GeneralSettings" singleton.
BF2_SV_GETTER_CLAMPF(coopBotRatio,      getCoopBotRatio,      m_coopBotRatio,       0.0f, 100.0f)
BF2_CONSOLE_PROPERTY(coopBotCount, int, svReceiver(), setCoopBotCount, getCoopBotCount, kSvConsoleTag)
BF2_SV_GETTER_CLAMPF(coopBotDifficulty, getCoopBotDifficulty, m_coopBotDifficulty, 10.0f, 100.0f)

// noVehicles: ConsoleClass718, 0x82AD50 -> ctor 0x4D5900 / executor 0x4D5A50. Declared "float",
// means bool, and swallows its setter's refusal string - see svNoVehiclesSet above.
BF2_CONSOLE_PROPERTY_FN(noVehicles, float, svReceiver() != 0,
                        static_cast<float>(static_cast<unsigned char>(svReceiver()->getNoVehicles())),
                        svNoVehiclesSet, kSvConsoleTag)

// The demo block, 0x82AD54..0x82AD64 -> ctors 0x4D5B10 / 0x4D5D00 / 0x4D5F80 / 0x4D61C0 / 0x4D6400.
// Raw except demoQuality, whose executor clamps to [1, 10] the same way the friendly-fire four do.
BF2_SV_FIELD(autoRecord,      bool,        m_autoRecord)
BF2_SV_FIELD(demoIndexURL,    std::string, m_demoIndexURL)
BF2_SV_FIELD(demoDownloadURL, std::string, m_demoDownloadURL)
BF2_SV_FIELD(autoDemoHook,    std::string, m_autoDemoHook)
BF2_SV_CLAMPED(demoQuality,   int,         m_demoQuality, 1, 10)

// ..0x82AD74 -> ctors 0x4D6610 / 0x4D6850 / 0x4D6A90 / 0x4D6CD0. Raw std::string both ways.
BF2_SV_FIELD(adminScript,      std::string, m_adminScript)
BF2_SV_FIELD(sponsorText,      std::string, m_sponsorText)
BF2_SV_FIELD(sponsorLogoURL,   std::string, m_sponsorLogoURL)
BF2_SV_FIELD(communityLogoURL, std::string, m_communityLogoURL)

// The radio spam limits, 0x82AD78..0x82AD80 -> ctors 0x4D6F10 / 0x4D7100 / 0x4D72F0. All three go
// through mirror-and-dirty setters; all three GET the stored field. The console property is
// spelled radioBlockedDurationTime while the field it reaches is m_radioBlockedDuration.
BF2_SV_SETTER(radioSpamInterval,        int, m_radioSpamInterval,     setRadioSpamInterval)
BF2_SV_SETTER(radioMaxSpamFlagCount,    int, m_radioMaxSpamFlagCount, setRadioMaxSpamFlagCount)
BF2_SV_SETTER(radioBlockedDurationTime, int, m_radioBlockedDuration,  setRadioBlockedDurationTime)

//  / 0x82AD88 -> ctors 0x4D74E0 / 0x4D76D0, executors 0x4D7630 / 0x4D7820. Both GET through
// compound accessors and SET raw: getUseGlobalRank is `ranked && internet && field`, and
// getUseGlobalUnlocks is `ranked || (internet && field)`. So on a LAN server both report false (rank)
// and false (unlocks) whatever the config said, and the console shows that rather than the setting.
BF2_SV_GETTER(useGlobalRank,    bool, getUseGlobalRank,    m_useGlobalRank)
BF2_SV_GETTER(useGlobalUnlocks, bool, getUseGlobalUnlocks, m_useGlobalUnlocks)

// > ctor 0x4D78C0 / executor 0x4D7A10. Its setter mirrors into ServerInfo but is the
// ONE mirror in the class that does NOT mark the block dirty - reproduced, not fixed.
BF2_SV_SETTER(friendlyFireWithMines, bool, m_friendlyFireWithMines, setFriendlyFireWithMines)

#undef BF2_SV_FIELD
#undef BF2_SV_FIELD_AS
#undef BF2_SV_SETTER
#undef BF2_SV_GETTER
#undef BF2_SV_CLAMPED
#undef BF2_SV_GETTER_CLAMPF

// access 2 begins only here: PE slot 0x82AD90 -> 0x761AC0, ELF 0x87ACFCB. Two bindings, then the
// table moves on to `maplist` at 0x82AD9C.
BF2_CONSOLE_OBJECT("sv", 2, 0)

// ranked: PE slot 0x82AD94 -> ctor 0x4D7AD0 / executor 0x4D7C20; ELF executor 0x87D7080.
// The property uses the effective ranked getter and the recovered gated setter.
BF2_CONSOLE_PROPERTY(ranked, bool, svReceiver(), setRanked, getRanked, kSvConsoleTag)

// punkBuster: PE slot 0x82AD98 -> ctor 0x4D7CE0 / executor 0x4D7E30. SET writes the raw field;
// GET also considers internet/ranked through getSvPunkBuster().
BF2_CONSOLE_PROPERTY_FN(punkBuster, bool, svReceiver() != 0,
                        svReceiver()->getSvPunkBuster(), svPunkBusterSet, kSvConsoleTag)

// Handed to the console registry by ModuleGame::init, the way the original registers 0x8B1704C.
ConsoleObjectList& serverSettingsConsoleObjects()
{
    return consoleBindingList(kSvConsoleTag);
}

} // namespace hfe
} // namespace dice
