// [CHOSEN, not recovered] The online services' domain and the host names formed from it; see
// OnlineProvider.h. This file has no link dependency on the GameSpy SDK: the persistence classes
// that ask it for a URL are also built on their own by test fixtures. The SDK's host-name
// variables are written in OnlineProviderGameSpy.cpp.
#include "OnlineProvider.h"

#include "../../External/GameSpy/common/gsCommon.h"   // GSI_DOMAIN_NAME, the SDK's compiled domain

#include <cctype>
#include <cstring>

namespace dice {
namespace hfe {
namespace onlineProvider {

namespace {

// The GameSpy game name the services are registered under (MatchMakingServer.cpp g_gameName,
// ProfileManager.cpp and ServerBrowser.cpp pass the same literal).
const char kGameName[] = "battlefield2";

// The SDK keeps host names in char[64] variables.
const std::string::size_type kMaxHostName = 63;

std::string& domainStorage()
{
    static std::string value(GSI_DOMAIN_NAME);
    return value;
}

bool& overriddenStorage()
{
    static bool value = false;
    return value;
}

// Index of the master server that lists a game: sb_serverlist.c forms "<game>.ms<index>.<domain>"
// from StringHash(game, NUM_MASTER_SERVERS). The SDK variable that replaces the list server
// (SBOverrideMasterServer) takes the whole name, so the index is computed here the same way.
int masterServerIndex(const char* gameName)
{
    unsigned int hash = 0;
    for (const char* s = gameName; *s; ++s)
        hash = hash * static_cast<unsigned int>(-1664117991)
             + static_cast<unsigned int>(std::tolower(static_cast<unsigned char>(*s)));
    return static_cast<int>(hash % 20);
}

std::string decimal(int value)
{
    std::string text;
    do
    {
        text.insert(text.begin(), static_cast<char>('0' + value % 10));
        value /= 10;
    } while (value);
    return text;
}

// Labels of letters, digits and '-' separated by single dots, as in a DNS name.
bool isDomainName(const std::string& text)
{
    bool labelStart = true;
    for (std::string::size_type i = 0; i < text.size(); ++i)
    {
        const unsigned char c = static_cast<unsigned char>(text[i]);
        if (c == '.')
        {
            if (labelStart)
                return false;
            labelStart = true;
        }
        else if (std::isalnum(c) || c == '-')
            labelStart = false;
        else
            return false;
    }
    return !labelStart;
}

bool isNumericAddress(const std::string& text)
{
    for (std::string::size_type i = 0; i < text.size(); ++i)
        if (text[i] != '.' && !std::isdigit(static_cast<unsigned char>(text[i])))
            return false;
    return true;
}

std::string gameHost(const char* service)
{
    return std::string(kGameName) + "." + service + "." + domain();
}

} // namespace

bool setDomain(const std::string& value, std::string& error)
{
    if (value.empty())
    {
        error = "a domain name is required";
        return false;
    }
    if (!isDomainName(value))
    {
        error = "\"" + value + "\" is not a domain name";
        return false;
    }
    if (isNumericAddress(value))
    {
        // "<game>.available.<address>" is not a host: an address cannot stand in for the domain.
        error = "\"" + value + "\" is an address, not a domain name";
        return false;
    }
    // The longest name formed is "<game>.available.<domain>" (as long as the statistics one).
    if (std::strlen(kGameName) + std::strlen(".available.") + value.size() > kMaxHostName)
    {
        error = "\"" + value + "\" is too long";
        return false;
    }
    domainStorage() = value;
    overriddenStorage() = value != GSI_DOMAIN_NAME;
    return true;
}

const std::string& domain()
{
    return domainStorage();
}

bool isOverridden()
{
    return overriddenStorage();
}

std::string availableHost()         { return gameHost("available"); }
std::string masterHost()            { return gameHost("master"); }
std::string connectionManagerHost() { return "gpcm." + domain(); }
std::string searchManagerHost()     { return "gpsp." + domain(); }
std::string cdKeyHost()             { return "key." + domain(); }
std::string gameStatsServerName()   { return "gamestats." + domain(); }
std::string gameStatsHost()         { return gameHost("gamestats"); }
std::string statsWebHost()          { return "BF2Web." + domain(); }

std::string serverListHost()
{
    return gameHost(("ms" + decimal(masterServerIndex(kGameName))).c_str());
}

std::string natNegHost(int index)
{
    return gameHost(("natneg" + decimal(index)).c_str());
}

std::string statsWebUrl()
{
    return "http://" + statsWebHost() + "/ASP/";
}

std::string playerInfoUrl()
{
    return "http://stage-net." + domain() + "/bf2/getplayerinfo.aspx?pid=";
}

std::string describe(bool clientServices)
{
    std::string line("online services: domain ");
    line += domain();
    line += isOverridden() ? " (+gamespy)" : " (default)";
    line += "; available=" + availableHost();
    line += " master=" + masterHost();
    line += " cdkey=" + masterHost();
    line += " natneg=" + natNegHost(1) + "," + natNegHost(2) + "," + natNegHost(3);
    line += " gamestats=" + gameStatsHost();
    if (clientServices)
    {
        line += " serverlist=" + serverListHost();
        line += " gpcm=" + connectionManagerHost();
        line += " gpsp=" + searchManagerHost();
    }
    line += " statsweb=" + statsWebUrl();
    line += " playerinfo=" + playerInfoUrl();
    return line;
}

std::string commandLineArguments()
{
    return isOverridden() ? " +gamespy " + domain() : std::string();
}

} // namespace onlineProvider
} // namespace hfe
} // namespace dice
