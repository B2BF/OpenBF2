// [CHOSEN, not recovered] Run-time redirects of the online (GameSpy protocol) service names. This
// is an OpenBF2 extension: retail compiled "gamespy.com" into every service name, and those hosts
// no longer exist. Every name, URL and HTTP Host header stays exactly as retail forms it; only the
// name that is looked up for it can be changed, inside the process, with the provider file named
// by the command-line option below (BF2Engine::parseParameters). Without it nothing is installed
// and every lookup is the GameSpy SDK's own.
//
// The names, with retail 1.50 evidence: the function or the string reference that forms each name
// (ELF: the ia-32 Linux server 1.5.3153.0; the amd-64 one has the same strings).
//
//   service                      host name                        retail evidence
//   availability check           <game>.available.gamespy.com     GSIStartAvailableCheck: BF2.exe
//                                                                 0x5A080C, bf2_w32ded.exe, ELF
//   master (heartbeats, CD keys) <game>.master.gamespy.com        qr2_init: BF2.exe 0x66AD0C,
//                                                                 bf2_w32ded.exe, ELF
//   server list                  <game>.ms14.gamespy.com          SBServerListConnectAndQuery:
//                                                                 BF2.exe 0x668255, ELF 0x84CF6A0
//   presence connection manager  gpcm.gamespy.com                 GP data: BF2.exe, ELF
//   presence search manager      gpsp.gamespy.com                 GP data: BF2.exe, ELF
//   game statistics              <game>.gamestats.gamespy.com     StatsManager::connect copies
//                                                                 "gamestats.gamespy.com", the SDK
//                                                                 prefixes <game>: all images
//   NAT negotiation              <game>.natneg1..3.gamespy.com    ResolveServer "%s.%s": ELF
//                                                                 0x84C5D30 (natneg1, natneg2)
//   stats web requests           http://BF2Web.gamespy.com/ASP/   HTTPRequest::getGameSpyUrl:
//                                                                 BF2.exe 0x59C8C0, bf2_w32ded.exe
//                                                                 0x561760, ELF 0x82BE800
//   player info web request      http://stage-net.gamespy.com/..  PlayerStatsManager static init:
//                                                                 BF2.exe 0x846D00, ELF 0x82F53D0
//   menu images                  http://eapusher.dice.se/...      menu content, not code
//
// <game> is the GameSpy game name "battlefield2"; 14 is the SDK's master-server hash of it (the
// same tolower/-1664117991/mod-20 hash in retail). CD keys are validated at the master's address
// on UDP 29910 (gcd_init_qr2, ELF GameSpyBackend 0x80BE2A0); the SDK's stand-alone key server name
// key.gamespy.com is never looked up by the game. The Windows images carry no NAT negotiation
// code; retail's SDK has two NAT negotiation servers, the linked revision adds a third with the
// same name format. The SDK's peerchat (chat) and motd (patch check) names are linked in the ELF
// but no game code calls them, here or in retail.
//
// [CHOSEN, not recovered] The option and its file. docs/building.md ("Online services") states the
// same contract for the setup application, which writes these files.
//
//   +onlineProviderConfig <path>      a UTF-8 JSON file, read once at start-up, before any service
//                                     starts. Client and dedicated server images.
//
// The file holds one JSON object, at most 64 KiB, optionally after a UTF-8 byte order mark:
//
//   "schema"    required; the number 1. Any other value refuses the file.
//   "provider"  optional display name for the log lines: letters, digits, space, '-' and '.', at
//               most 64 characters (empty counts as none).
//   "services"  required object with at least one entry. Each value is a dotted IPv4 address or a
//               host/domain name.
//     "*"         every name that contains "gamespy" or is under dice.se (compared without case)
//                 and has no label entry of its own. An address: the name resolves to it. A
//                 domain D (at most 230 characters): the name is resolved with its retail domain
//                 (gamespy.com or dice.se) replaced by D, gpcm.gamespy.com as gpcm.D and
//                 eapusher.dice.se as eapusher.D (a matching name under neither domain is looked
//                 up unchanged).
//     <label>     the names whose label directly before the retail domain is <label> (the name
//                 compared without case). The key is spelled exactly as kLabels in
//                 OnlineProvider.cpp spells it, compared with case: available, ms14, master,
//                 gpcm, gpsp, gamestats, BF2Web, stage-net, natneg1, natneg2, natneg3 (under
//                 gamespy.com) and eapusher (under dice.se). An address: the name resolves to it.
//                 A host name (at most 253 characters): the name resolves as that host.
//   any other top-level key, for example "valid_to", is ignored.
//
// Refused: a file that cannot be read, is larger than 64 KiB or is not one strict JSON object
// (nothing but whitespace after it); a duplicate key at the top level or in "services"; a value of
// the wrong type; an unknown key in "services" (a label in another case among them); a value the
// rules above do not allow. A refused file ends start-up.
//
// A label entry takes precedence over "*"; a name neither covers is looked up unchanged. Every
// entry applies in every image: a dedicated server never looks up the names of the services only
// client images link (ms14, gpcm, gpsp) or of the menu images (eapusher), so those entries simply
// stay unused there. The redirect only hands the SDK another name to look up
// (OnlineProviderGameSpy.cpp, installed into gsiRedirectHostname, GameSpy/LOCAL_CHANGES.md): the
// SDK's own gethostbyname or getaddrinfo resolves it at every lookup, with nothing resolved or
// cached at start-up, and a lookup that fails is not retried with the retail name. Each looked-up
// name is logged once to stderr with what it resolved as and the file's key that chose it
// ("services.gpcm", "services.*").
//
// The option comes from the command line only. A console or settings-file command could be reached
// by mod or map content and would redirect account logins; and the first service starts before
// any server settings file runs.

#ifndef GAME_MATCHMAKING_ONLINEPROVIDER_H
#define GAME_MATCHMAKING_ONLINEPROVIDER_H

#include <string>

namespace dice {
namespace hfe {
namespace onlineProvider {

// +onlineProviderConfig: reads the provider file at path and installs its entries in place of any
// installed before. Returns false, with the reason in error, when the file cannot be read or does
// not follow the rules above; nothing changes then.
bool loadConfigFile(const std::string& path, std::string& error);

// The same for the file's text, already read; path is the file describe() and
// commandLineArguments() name.
bool loadConfigText(const std::string& text, const std::string& path, std::string& error);

// The labels a "services" key can name, spelled as in the retail names, in kLabels order.
int labelCount();
const char* label(int index);                   // "gpcm"

// The label index of name (its label directly before its retail domain); -1 for none.
int labelOf(const std::string& name);

// True when a provider file is in effect.
bool hasRedirect();

// The name to look up instead of name: the label entry's value, else the "*" entry's address or
// the name with its retail domain replaced, for a name containing "gamespy" or under dice.se;
// empty when the name is looked up unchanged. source, when given, receives the file's key that
// chose it ("services.gpcm", "services.*").
std::string redirectLookup(const std::string& name, std::string* source = 0);

// One line for the start-up log: the provider, the file and its entries, or that the names
// resolve unchanged.
std::string describe();

// " +onlineProviderConfig <path>" (the path in double quotes when it holds white space or starts
// with a double quote), empty without a file. A restarted client must keep the provider it was started
// with (BF2Engine::buildRestartCommandLine).
std::string commandLineArguments();

// Prepares the linked GameSpy SDK; call once, before the first service starts. Installs the
// redirect into the SDK's resolver seam when hasRedirect(), and on POSIX makes a closed connection
// an error return instead of a fatal SIGPIPE. Defined in OnlineProviderGameSpy.cpp.
void prepareGameSpySdk();

} // namespace onlineProvider
} // namespace hfe
} // namespace dice

#endif
