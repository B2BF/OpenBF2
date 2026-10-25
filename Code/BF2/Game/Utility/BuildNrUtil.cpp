// Build-number formatting helpers.
#include "BuildNrUtil.h"

#include <cstdio>
#include <string>

#include "../../Core/StringUtil.h"       // setLowerCase
#include "../../Core/SettingsRepostitory.h"  // dice::hfe::settings, stringGet
#include "../../Core/SystemInfo.h"
#include "../GameServer/ServerSettings.h"

namespace dice {
namespace hfe {

namespace {
// ds:0x8986E60, _ZN45_GLOBAL__N_Game_Utility_BuildNrUtil.cppJ9BEYb7buildNrE - the anonymous-
// namespace symbol that attests this file's path. A plain char buffer, zero-initialised.
// [UNRESOLVED] its declared size; 32 is [CHOSEN] and comfortably fits "1.5.3153-802.0".
char buildNr[32];
}

// Returns the cached protocol build string. Initialization is lazy and not thread-safe.
// Keep branding out of this value: server queries and browser filters use it for compatibility.
// [UNRESOLVED] The trailing numeric pair's meaning is unknown.
namespace BuildNrUtil {

const char* getBuildNrString()
{
    if (!buildNr[0])
        sprintf(buildNr, "%i.%i.%i-%i.%i", 1, 5, 3153, 802, 0);

    return buildNr;
}

// Protocol compatibility version; changing it causes stock peers to reject the
// connection. [UNRESOLVED] Treat the complete value as opaque.
int getNetVersionNumber()
{
    return 0x150C5100;
}

} // namespace BuildNrUtil

// Returns GSModDirectory with its first path component removed and lowercases the
// remainder. With no slash, preserves case. The default is (unknown); challenge
// messages use this short name, while connection validation uses the full path.
std::string getModName()
{
    std::string modName = settings->stringGet(std::string("GSModDirectory"),
                                              std::string("(unknown)"));

    const std::string::size_type slash = modName.find('/', 0);
    if (slash != std::string::npos)
    {
        std::string::size_type n = slash + 1;
        if (n >= modName.length())                  // the cmovnb - cannot fire; see above
            n = modName.length();

        modName.replace(0, n, modName.c_str(), 0);  // a zero-length replacement = erase
        setLowerCase(modName);                      // inside the if - see above
    }
    return modName;
}

//  / PE 0x54D250: empty configured name falls back to the computer name.
std::string getServerName()
{
    std::string name = serverSettings->m_serverName;
    if (name.empty()) {
        name = "BF2 server on ";
        name += systemInfo()->getComputerName();
    }
    return name;
}

std::string getOS()
{
    // [PLATFORM] ELF 0x8149F00 / PE 0x54D390; no runtime OS probing.
#ifdef _WIN32
    return "win32";
#else
    return "linux-64";
#endif
}

// isExpansionPack - 0x814A0A0, 193 B. Compares getModName() against the
// literal "xpack" (.rodata 0x8887F23) case-insensitively; compareIgnoreCase returns 0 on a match,
// so the shipped `if (cmp) return 0; else return 1;` means "true when the mod IS xpack".
// getModName() strips the "mods/" prefix, so this matches the bare "xpack", not "mods/xpack".
bool isExpansionPack()
{
    return compareIgnoreCase(getModName(), std::string("xpack")) == 0;
}

} // namespace hfe
} // namespace dice
