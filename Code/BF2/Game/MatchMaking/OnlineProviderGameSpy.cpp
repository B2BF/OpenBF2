// [CHOSEN, not recovered] Prepares the linked GameSpy SDK for the process: the online services'
// host names (OnlineProvider.h) go into the SDK's own run-time host-name variables.
#include "OnlineProvider.h"

#include <cstring>
#if !defined(_WIN32)
#include <csignal>
#endif

extern "C" {
#include "../../External/GameSpy/common/gsCommon.h"
#include "../../External/GameSpy/common/gsAvailable.h"   // GSIACHostname
#include "../../External/GameSpy/qr2/qr2.h"              // qr2_hostname
#include "../../External/GameSpy/gcdkey/gcdkeys.h"       // gcd_hostname
#include "../../External/GameSpy/gstats/gstats.h"        // StatsServerHostname
#include "../../External/GameSpy/natneg/natneg.h"        // Matchup1Hostname, Matchup2Hostname
extern char* Matchup3Hostname;                           // natneg.c; not declared in natneg.h
#if !DEDICATED
#include "../../External/GameSpy/gp/gp.h"                // GPConnectionManagerHostname and
                                                         // GPSearchManagerHostname
extern char* SBOverrideMasterServer;                     // sb_serverlist.c (sb_internal.h)
#endif
}

namespace dice {
namespace hfe {
namespace onlineProvider {

namespace {

// Every SDK host-name array is char[64]; setDomain admits no domain that forms a longer name.
void copyName(char (&destination)[64], const std::string& name)
{
    std::strncpy(destination, name.c_str(), sizeof(destination) - 1);
    destination[sizeof(destination) - 1] = '\0';
}

} // namespace

void prepareGameSpySdk()
{
#if !defined(_WIN32)
    // The SDK's TCP services (presence, server list, statistics, web requests) expect send() to
    // report a connection the peer has closed or refused. On POSIX the default SIGPIPE action
    // ends the process instead. The embedded Python ignores SIGPIPE when it starts, but that is
    // only when a server is initialised; a client in the menu has not got that far.
    std::signal(SIGPIPE, SIG_IGN);
#endif

    if (!isOverridden())
        return;

    // The pointer variables take storage that outlives every SDK call.
    static char natNeg[3][64];
#if !DEDICATED
    static char serverList[64];
#endif

    copyName(GSIACHostname, availableHost());
    copyName(qr2_hostname, masterHost());
    copyName(gcd_hostname, cdKeyHost());
    copyName(StatsServerHostname, gameStatsServerName());
    copyName(natNeg[0], natNegHost(1));
    copyName(natNeg[1], natNegHost(2));
    copyName(natNeg[2], natNegHost(3));
    Matchup1Hostname = natNeg[0];
    Matchup2Hostname = natNeg[1];
    Matchup3Hostname = natNeg[2];
#if !DEDICATED
    // Server browsing and presence are linked into client images only.
    copyName(GPConnectionManagerHostname, connectionManagerHost());
    copyName(GPSearchManagerHostname, searchManagerHost());
    copyName(serverList, serverListHost());
    SBOverrideMasterServer = serverList;
#endif
}

} // namespace onlineProvider
} // namespace hfe
} // namespace dice
