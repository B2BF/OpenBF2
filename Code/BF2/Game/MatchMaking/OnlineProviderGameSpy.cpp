// [CHOSEN, not recovered] Prepares the linked GameSpy SDK for the process: the redirects of the
// +onlineProviderConfig provider file (OnlineProvider.h) go into the SDK's resolver seam.
#include "OnlineProvider.h"

#include <cstdio>
#include <mutex>
#include <set>
#include <string>
#if !defined(_WIN32)
#include <csignal>
#endif

extern "C" {
#include "../../External/GameSpy/common/gsCommon.h"   // gsiSetHostRedirect (gsPlatformSocket.h)
}

namespace dice {
namespace hfe {
namespace onlineProvider {

namespace {

// The redirect's state. Lookups run on the game thread and on the SDK's resolver thread.
std::mutex& redirectLock()
{
    static std::mutex lock;
    return lock;
}

// Every name handed to the SDK to look up instead. An entry is added once and never changed or
// removed, so the pointer handed to the SDK stays valid for the process.
std::set<std::string>& lookupNames()
{
    static std::set<std::string> names;
    return names;
}

// Names already logged, so that each name is reported once.
std::set<std::string>& loggedNames()
{
    static std::set<std::string> names;
    return names;
}

void logOnce(const std::string& key, const std::string& line)
{
    if (!loggedNames().insert(key).second)
        return;
    std::fprintf(stderr, "online services: %s\n", line.c_str());
    std::fflush(stderr);
}

// The SDK's gsiHostRedirectFunc: the name redirectLookup chooses, which the SDK then resolves
// itself at this and every later lookup (an address without DNS, a host name through the system
// resolver), or hostname itself for a name no entry of the file covers.
const char* redirectHostname(const char* hostname)
{
    std::string source;
    const std::string lookup = redirectLookup(hostname, &source);
    std::lock_guard<std::mutex> guard(redirectLock());
    if (lookup.empty())
    {
        logOnce(hostname, std::string("forwarding ") + hostname);
        return hostname;
    }
    logOnce(hostname, std::string("redirecting ") + hostname + " to " + lookup + " (" + source + ")");
    return lookupNames().insert(lookup).first->c_str();
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

    if (hasRedirect())
    {
        redirectLock();             // constructed here, before the SDK can start its resolver thread
        gsiSetHostRedirect(&redirectHostname);
    }
}

} // namespace onlineProvider
} // namespace hfe
} // namespace dice
