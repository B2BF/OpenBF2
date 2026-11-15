// [CHOSEN, not recovered] Run-time domain of the online (GameSpy protocol) services. This is an
// OpenBF2 extension: retail compiled "gamespy.com" into every service name, and those hosts no
// longer exist. The names are still formed exactly as retail forms them; only the domain part can
// be replaced, with the command-line option +gamespy <domain> (BF2Engine::parseParameters).
// Without the option every name is retail's.
//
// Retail 1.50 evidence: the function or the string reference that forms each name (ELF: the ia-32
// Linux server 1.5.3153.0; the amd-64 one has the same strings).
//
//   service                      host name                        retail evidence
//   availability check           <game>.available.<domain>        GSIStartAvailableCheck: BF2.exe
//                                                                 0x5A080C, bf2_w32ded.exe, ELF
//   master (heartbeats, CD keys) <game>.master.<domain>           qr2_init: BF2.exe 0x66AD0C,
//                                                                 bf2_w32ded.exe, ELF
//   server list                  <game>.ms<N>.<domain>            SBServerListConnectAndQuery:
//                                                                 BF2.exe 0x668255, ELF 0x84CF6A0
//   presence connection manager  gpcm.<domain>                    GP data: BF2.exe, ELF
//   presence search manager      gpsp.<domain>                    GP data: BF2.exe, ELF
//   game statistics              <game>.gamestats.<domain>        StatsManager::connect copies
//                                                                 "gamestats.<domain>", the SDK
//                                                                 prefixes <game>: all images
//   NAT negotiation              <game>.natneg1..3.<domain>       ResolveServer "%s.%s": ELF
//                                                                 0x84C5D30 (natneg1, natneg2)
//   stats web requests           http://BF2Web.<domain>/ASP/      HTTPRequest::getGameSpyUrl:
//                                                                 BF2.exe 0x59C8C0, bf2_w32ded.exe
//                                                                 0x561760, ELF 0x82BE800
//   player info web request      http://stage-net.<domain>/bf2/.. PlayerStatsManager static init:
//                                                                 BF2.exe 0x846D00, ELF 0x82F53D0
//
// <game> is the GameSpy game name "battlefield2"; <N> is the SDK's hash of it (14, the same
// tolower/-1664117991/mod-20 hash in retail). CD keys are validated at the master's address on UDP
// 29910 (gcd_init_qr2, ELF GameSpyBackend 0x80BE2A0); the SDK's stand-alone key server name
// key.<domain> is set as well and is not used by the game. The Windows images carry no NAT
// negotiation code; retail's SDK has two NAT negotiation servers, the linked revision adds a third
// with the same name format. The SDK's peerchat.<domain> (chat) and motd.<domain> (patch check)
// are linked in the ELF but no game code calls them, here or in retail.
//
// The domain comes from the command line only. A console or settings-file command could be reached
// by mod or map content and would redirect account logins; and the first service starts before
// any server settings file runs.

#ifndef GAME_MATCHMAKING_ONLINEPROVIDER_H
#define GAME_MATCHMAKING_ONLINEPROVIDER_H

#include <string>

namespace dice {
namespace hfe {
namespace onlineProvider {

// Replaces the domain of every online service name. Returns false, with a message for the user
// in error, when the value is not a usable domain name; the current domain is then unchanged.
bool setDomain(const std::string& domain, std::string& error);

// The domain in use: the SDK's compiled name unless setDomain replaced it.
const std::string& domain();

// True when setDomain replaced the compiled domain.
bool isOverridden();

// Host names looked up for each service.
std::string availableHost();            // availability check
std::string masterHost();               // heartbeats and CD-key validation
std::string serverListHost();           // server browser master list
std::string natNegHost(int index);      // NAT negotiation server 1..3
std::string connectionManagerHost();    // presence login
std::string searchManagerHost();        // presence search and account creation
std::string cdKeyHost();                // the SDK's stand-alone key server name
std::string gameStatsHost();            // name looked up for the statistics connection
std::string statsWebHost();             // host of statsWebUrl()

// The value for the SDK's StatsServerHostname: the SDK prefixes the game name itself.
std::string gameStatsServerName();

// "http://BF2Web.<domain>/ASP/", the base of the persistence requests.
std::string statsWebUrl();

// "http://stage-net.<domain>/bf2/getplayerinfo.aspx?pid=", PlayerStatsManager's request.
std::string playerInfoUrl();

// One line naming the host of every service, for the start-up log. clientServices adds the
// services only client images link (server list, presence).
std::string describe(bool clientServices);

// " +gamespy <domain>" when the domain is replaced, otherwise empty. A restarted client
// must keep the domain it was started with (BF2Engine::buildRestartCommandLine).
std::string commandLineArguments();

// Prepares the linked GameSpy SDK; call once, before the first service starts. Writes the service
// names into the SDK's host-name variables when the domain is replaced (with the compiled domain
// the SDK forms the same names itself and nothing is written), and on POSIX makes a closed
// connection an error return instead of a fatal SIGPIPE. Defined in OnlineProviderGameSpy.cpp.
void prepareGameSpySdk();

} // namespace onlineProvider
} // namespace hfe
} // namespace dice

#endif
