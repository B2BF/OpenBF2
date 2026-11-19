// [CHOSEN, not recovered] The redirects of the online service names, +gamespyRedirect and
// +gamespyRedirect<label>; see OnlineProvider.h. This file has no link dependency on the GameSpy
// SDK; the redirect is installed into the SDK in OnlineProviderGameSpy.cpp.
#include "OnlineProvider.h"

#include <cctype>
#include <cstring>

namespace dice {
namespace hfe {
namespace onlineProvider {

namespace {

// The retail domains of the names the options cover.
const char kGameSpyDomain[] = "gamespy.com";
const char kDiceDomain[] = "dice.se";

// The label options: each label exactly as the retail name spells it, the retail domain it stands
// before, and the help text. The order is the option ids' (79 upwards).
struct LabelEntry
{
    const char* label;
    const char* domain;
    const char* help;
};

const LabelEntry kLabels[] = {
    { "available", kGameSpyDomain, "Resolve battlefield2.available.gamespy.com (availability check) to this IPv4 address or host" },
    { "ms14",      kGameSpyDomain, "Resolve battlefield2.ms14.gamespy.com (server list) to this IPv4 address or host" },
    { "master",    kGameSpyDomain, "Resolve battlefield2.master.gamespy.com (heartbeats, CD keys) to this IPv4 address or host" },
    { "gpcm",      kGameSpyDomain, "Resolve gpcm.gamespy.com (login) to this IPv4 address or host" },
    { "gpsp",      kGameSpyDomain, "Resolve gpsp.gamespy.com (account search) to this IPv4 address or host" },
    { "gamestats", kGameSpyDomain, "Resolve battlefield2.gamestats.gamespy.com (game statistics) to this IPv4 address or host" },
    { "BF2Web",    kGameSpyDomain, "Resolve BF2Web.gamespy.com (stats web requests) to this IPv4 address or host" },
    { "stage-net", kGameSpyDomain, "Resolve stage-net.gamespy.com (player info request) to this IPv4 address or host" },
    { "eapusher",  kDiceDomain,    "Resolve eapusher.dice.se (menu images) to this IPv4 address or host" },
    { "natneg1",   kGameSpyDomain, "Resolve battlefield2.natneg1.gamespy.com (NAT negotiation) to this IPv4 address or host" },
    { "natneg2",   kGameSpyDomain, "Resolve battlefield2.natneg2.gamespy.com (NAT negotiation) to this IPv4 address or host" },
    { "natneg3",   kGameSpyDomain, "Resolve battlefield2.natneg3.gamespy.com (NAT negotiation) to this IPv4 address or host" },
};
const int kLabelCount = sizeof(kLabels) / sizeof(kLabels[0]);

// The longest DNS name.
const std::string::size_type kMaxName = 253;

// The longest prefix +gamespyRedirect <domain> keeps: "battlefield2.available." (as long as the
// statistics one).
const std::string::size_type kLongestPrefix = sizeof("battlefield2.available.") - 1;

// +gamespyRedirect's value; empty when the option is not given.
std::string& redirectStorage()
{
    static std::string value;
    return value;
}

// +gamespyRedirect<label>'s values, by label index; empty when not given.
std::string& labelStorage(int index)
{
    static std::string values[kLabelCount];
    return values[index];
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

// Four decimal numbers from 0 to 255 separated by dots.
bool isIPv4Address(const std::string& text)
{
    int parts = 0;
    std::string::size_type i = 0;
    while (i < text.size())
    {
        int value = 0;
        std::string::size_type digits = 0;
        for (; i < text.size() && std::isdigit(static_cast<unsigned char>(text[i])); ++i, ++digits)
            value = value * 10 + (text[i] - '0');
        if (digits == 0 || digits > 3 || value > 255)
            return false;
        ++parts;
        if (i == text.size())
            break;
        if (text[i] != '.' || ++i == text.size())
            return false;
    }
    return parts == 4;
}

std::string lowerCase(std::string text)
{
    for (std::string::size_type i = 0; i < text.size(); ++i)
        text[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(text[i])));
    return text;
}

// A value: a dotted IPv4 address or a host/domain name no longer than longest.
bool checkValue(const std::string& value, std::string::size_type longest, std::string& error)
{
    if (value.empty())
    {
        error = "an IPv4 address or host name is required";
        return false;
    }
    if (!isIPv4Address(value) && (!isDomainName(value) || isNumericAddress(value)))
    {
        error = "\"" + value + "\" is not an IPv4 address or host name";
        return false;
    }
    if (value.size() > longest)
    {
        error = "\"" + value + "\" is too long";
        return false;
    }
    return true;
}

// The length of name's part before "." + domain (lower case) when name (lower case) is under
// domain; npos otherwise. A name equal to domain has a prefix of length 0.
std::string::size_type prefixLength(const std::string& name, const char* domain)
{
    const std::string::size_type n = std::strlen(domain);
    if (name.size() == n)
        return name == domain ? 0 : std::string::npos;
    if (name.size() <= n + 1 || name[name.size() - n - 1] != '.' || name.compare(name.size() - n, n, domain) != 0)
        return std::string::npos;
    return name.size() - n - 1;
}

// The retail domain name is under, or 0.
const char* retailDomain(const std::string& lower)
{
    if (prefixLength(lower, kGameSpyDomain) != std::string::npos)
        return kGameSpyDomain;
    if (prefixLength(lower, kDiceDomain) != std::string::npos)
        return kDiceDomain;
    return 0;
}

} // namespace

bool setRedirect(const std::string& value, std::string& error)
{
    // A domain must leave room for the longest name formed with it.
    if (!checkValue(value, isIPv4Address(value) ? kMaxName : kMaxName - kLongestPrefix, error))
        return false;
    redirectStorage() = value;
    return true;
}

int labelCount()
{
    return kLabelCount;
}

const char* label(int index)
{
    return index >= 0 && index < kLabelCount ? kLabels[index].label : "";
}

std::string labelOption(int index)
{
    return std::string("gamespyRedirect") + label(index);
}

const char* labelHelp(int index)
{
    return index >= 0 && index < kLabelCount ? kLabels[index].help : "";
}

bool setLabelRedirect(int index, const std::string& value, std::string& error)
{
    if (index < 0 || index >= kLabelCount)
    {
        error = "not a label option";
        return false;
    }
    if (!checkValue(value, kMaxName, error))
        return false;
    labelStorage(index) = value;
    return true;
}

int labelOf(const std::string& name)
{
    const std::string lower = lowerCase(name);
    const char* domain = retailDomain(lower);
    if (!domain)
        return -1;
    const std::string::size_type end = prefixLength(lower, domain);
    if (end == 0 || end == std::string::npos)
        return -1;
    const std::string::size_type dot = lower.rfind('.', end - 1);
    const std::string::size_type start = dot == std::string::npos ? 0 : dot + 1;
    const std::string labelText = lower.substr(start, end - start);
    for (int i = 0; i < kLabelCount; ++i)
        if (kLabels[i].domain == domain && labelText == lowerCase(kLabels[i].label))
            return i;
    return -1;
}

bool hasRedirect()
{
    if (!redirectStorage().empty())
        return true;
    for (int i = 0; i < kLabelCount; ++i)
        if (!labelStorage(i).empty())
            return true;
    return false;
}

std::string redirectLookup(const std::string& name, std::string* option)
{
    if (option)
        option->clear();
    const int index = labelOf(name);
    if (index >= 0 && !labelStorage(index).empty())
    {
        if (option)
            *option = labelOption(index);
        return labelStorage(index);
    }
    const std::string& fallback = redirectStorage();
    const std::string lower = lowerCase(name);
    const char* domain = retailDomain(lower);
    if (fallback.empty() || (lower.find("gamespy") == std::string::npos && domain != kDiceDomain))
        return std::string();
    std::string lookup;
    if (isIPv4Address(fallback))
        lookup = fallback;
    else if (domain)
    {
        // The name as formed, with its retail domain replaced: gpcm.gamespy.com -> gpcm.<D>.
        const std::string::size_type prefix = prefixLength(lower, domain);
        lookup = prefix ? name.substr(0, prefix) + "." + fallback : fallback;
    }
    else
        return std::string();       // contains "gamespy" but has no retail domain to replace
    if (option)
        *option = "gamespyRedirect";
    return lookup;
}

std::string describe()
{
    if (!hasRedirect())
        return "online services: retail names, resolved unchanged";
    std::string line("online services: retail names");
    for (int i = 0; i < kLabelCount; ++i)
        if (!labelStorage(i).empty())
            line += std::string("; ") + kLabels[i].label + " names resolve as "
                  + labelStorage(i) + " (+" + labelOption(i) + ")";
    const std::string& fallback = redirectStorage();
    if (!fallback.empty())
    {
        line += "; other names containing gamespy or under dice.se resolve ";
        line += isIPv4Address(fallback) ? "to " + fallback
                                        : "with their retail domain replaced by " + fallback;
        line += " (+gamespyRedirect)";
    }
    return line;
}

std::string commandLineArguments()
{
    std::string arguments;
    if (!redirectStorage().empty())
        arguments += " +gamespyRedirect " + redirectStorage();
    for (int i = 0; i < kLabelCount; ++i)
        if (!labelStorage(i).empty())
            arguments += " +" + labelOption(i) + " " + labelStorage(i);
    return arguments;
}

} // namespace onlineProvider
} // namespace hfe
} // namespace dice
