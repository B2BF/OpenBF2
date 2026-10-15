// Path attested: _GLOBAL__N_Core_Registry.cppvohp2b9rinstance in the symbolized ELF.
#include "Registry.h"

#ifdef _WIN32
#include <windows.h>
#endif

namespace dice { namespace hfe {

// ELF C1/C2 0x83CB6C0/0x83CB6B0 and D1/D2 0x83CB6E0/0x83CB6D0: empty bodies.
Registry::Registry() {}
Registry::~Registry() {}

namespace {
Registry rinstance;

#ifdef _WIN32
// Reads a registry string using the default-valued subkey fallback. [INFERRED] Helper name.
// String length excludes the final byte; zero-length REG_SZ data is not guarded.
bool readRegistryString(RegKeyCategory category, const std::string& name,
                        const std::string& defaultValue, std::string& output,
                        const std::string& modName)
{
    const HKEY root = category == RegKeyLocalMachine ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
    HKEY key = 0;
    std::string path = "SOFTWARE\\Electronic Arts\\EA Games\\Battlefield 2";
    if (!modName.empty())
        path = std::string("SOFTWARE\\Electronic Arts\\EA Games\\") + modName;

    bool found = false;
#if defined(_M_IX86)
    if (RegOpenKeyA(root, path.c_str(), &key) == ERROR_SUCCESS)
#else
    // [CHOSEN, not recovered] x64 (https://github.com/B2BF/OpenBF2/issues/7): BF2's 32-bit installer
    // writes these keys into the 32-bit registry view (WOW6432Node), which a 32-bit process reads
    // implicitly; a 64-bit process must ask for that view or it finds no keys (an empty CD key then
    // takes translateCdKey's shipped null-blob fault at the first server challenge). Subkeys opened
    // relative to this handle stay in the same view.
    if (RegOpenKeyExA(root, path.c_str(), 0, KEY_READ | KEY_WOW64_32KEY, &key) == ERROR_SUCCESS)
#endif
    {
        BYTE data[1024];
        DWORD type;
        DWORD bytes = sizeof(data);
        LSTATUS status = RegQueryValueExA(key, name.c_str(), 0, &type, data, &bytes);
        if (status != ERROR_SUCCESS)
        {
            HKEY subkey = 0;
            if (RegOpenKeyA(key, name.c_str(), &subkey) == ERROR_SUCCESS)
            {
                // Retail reuses bytes from the failed query, without resetting it.
                status = RegQueryValueExA(subkey, 0, 0, &type, data, &bytes);
                RegCloseKey(subkey);
            }
            else
            {
                char message[256];
                FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, 0, GetLastError(), 0,
                               message, sizeof(message), 0);
            }
        }
        if (status == ERROR_SUCCESS && type == REG_SZ)
        {
            output = std::string(reinterpret_cast<const char*>(data), bytes - 1);
            found = true;
        }
    }
    if (key)
        RegCloseKey(key);
    if (found)
        return true;
    if (!defaultValue.empty())
    {
        output = defaultValue;
        return true;
    }
    return false;
}
#endif
}

Registry* registry = &rinstance;

// Client PE 0x450FE0. [PLATFORM] ELF 0x83CB6F0 constructs an empty string and
// ignores even the supplied default; this is an actual shipped Linux body.
std::string Registry::getString(RegKeyCategory category, const std::string& name,
                                const std::string& defaultValue, const std::string& modName) const
{
#ifdef _WIN32
    std::string result = defaultValue;
    readRegistryString(category, name, defaultValue, result, modName);
    return result;
#else
    return std::string("");
#endif
}

} }
