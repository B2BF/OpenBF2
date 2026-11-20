// [CHOSEN, not recovered] Run-time redirects of the online (GameSpy protocol) service names. This
// is an OpenBF2 extension: retail compiled "gamespy.com" into every service name, and those hosts
// no longer exist. Every name, URL and HTTP Host header stays exactly as retail forms it; only the
// address a name resolves to can be changed, inside the process, with the command-line options
// below (BF2Engine::parseParameters). Without any of them nothing is installed and every lookup is
// the GameSpy SDK's own.
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
// [CHOSEN, not recovered] The options. A value is a dotted IPv4 address or a host/domain name.
//
//   +gamespyRedirect <value>          every name that contains "gamespy" or is under dice.se
//                                     (compared without case) and has no label option of its own.
//                                     An address: the name resolves to it. A domain D: the name is
//                                     resolved with its retail domain (gamespy.com or dice.se)
//                                     replaced by D, gpcm.gamespy.com as gpcm.D and
//                                     eapusher.dice.se as eapusher.D (a matching name under
//                                     neither domain is looked up unchanged).
//   +gamespyRedirect<label> <value>   the names whose label directly before the retail domain is
//                                     <label> (compared without case). An address: the name
//                                     resolves to it. A host name: the name resolves as that host.
//                                     Client images only. The labels, from kLabels in
//                                     OnlineProvider.cpp: available, ms14, master, gpcm, gpsp,
//                                     gamestats, BF2Web, stage-net, natneg1, natneg2, natneg3
//                                     (under gamespy.com) and eapusher (under dice.se).
//
// A label option takes precedence over +gamespyRedirect; a name neither covers is looked up
// unchanged. The redirect only hands the SDK another name to look up (OnlineProviderGameSpy.cpp,
// installed into gsiRedirectHostname, GameSpy/LOCAL_CHANGES.md): the SDK's own gethostbyname or
// getaddrinfo resolves it at every lookup, with nothing resolved or cached at start-up, and a
// lookup that fails is not retried with the retail name. Each looked-up name is logged once to
// stderr with what it resolved as and the option that chose it.
//
// The options come from the command line only. A console or settings-file command could be reached
// by mod or map content and would redirect account logins; and the first service starts before
// any server settings file runs.

#ifndef GAME_MATCHMAKING_ONLINEPROVIDER_H
#define GAME_MATCHMAKING_ONLINEPROVIDER_H

#include <string>

namespace dice {
namespace hfe {
namespace onlineProvider {

// +gamespyRedirect: the fallback for every name containing "gamespy" or under dice.se. Returns
// false, with a message in error, when value is neither a dotted IPv4 address nor a domain name.
bool setRedirect(const std::string& value, std::string& error);

// The label options, in the order of their option ids (BF2Engine::parseParameters registers
// index 0 as 79, then one id each).
int labelCount();
const char* label(int index);                   // "gpcm", spelled as in the retail name
std::string labelOption(int index);             // "gamespyRedirectgpcm"
const char* labelHelp(int index);               // the parser's help text

// +gamespyRedirect<label>: the target of one label's names. Returns false, with a message in
// error, when value is neither a dotted IPv4 address nor a host name.
bool setLabelRedirect(int index, const std::string& value, std::string& error);

// The label option index of name (its label directly before its retail domain); -1 for none.
int labelOf(const std::string& name);

// True when +gamespyRedirect or any label option is in effect.
bool hasRedirect();

// The name to look up instead of name: the label option's value, else +gamespyRedirect's address
// or the name with its retail domain replaced, for a name containing "gamespy" or under dice.se;
// empty when the name is looked up unchanged. option, when given, receives the option that chose
// it ("gamespyRedirectgpcm", "gamespyRedirect").
std::string redirectLookup(const std::string& name, std::string* option = 0);

// One line for the start-up log: the options in effect, or that the names resolve unchanged.
std::string describe();

// " +gamespyRedirect <value>" and each " +gamespyRedirect<label> <value>" that is set; empty with
// none. A restarted client must keep the redirects it was started with
// (BF2Engine::buildRestartCommandLine).
std::string commandLineArguments();

// Prepares the linked GameSpy SDK; call once, before the first service starts. Installs the
// redirect into the SDK's resolver seam when hasRedirect(), and on POSIX makes a closed connection
// an error return instead of a fatal SIGPIPE. Defined in OnlineProviderGameSpy.cpp.
void prepareGameSpySdk();

} // namespace onlineProvider
} // namespace hfe
} // namespace dice

#endif
