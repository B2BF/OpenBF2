// Path attested: _GLOBAL__N_Core_Registry.cppvohp2b9rinstance in the symbolized ELF.
#include "Registry.h"

#if !DEDICATED
// [CHOSEN, not recovered] Owner decisions: the client's registry file (RegistryFile.h).
#include "RegistryFile.h"
#include <cstdio>
#include <map>
#include <sstream>
#endif
#ifdef _WIN32
#include <windows.h>
#if !DEDICATED
#include <wincrypt.h>
#include <shlobj.h>
#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")
#endif
#elif !DEDICATED
#include "PosixRegistry.h"
#include <fstream>
#if defined(__APPLE__)
#include "../Platform/macOS/ClientPaths.h"
#else
#include "../IO/File/FileManager.h"
#endif
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

#if !DEDICATED
// [CHOSEN, not recovered] File syntax and owner decisions: docs/registry-file.md.
namespace registryFile {
namespace {
std::string trim(const std::string& text)
{
    const auto first = text.find_first_not_of(" \t\r");
    return first == std::string::npos ? std::string() :
        text.substr(first, text.find_last_not_of(" \t\r") - first + 1);
}

std::string fold(std::string text)
{
    for (char& c : text)
        if (c >= 'A' && c <= 'Z')
            c += 'a' - 'A';
    return text;
}

std::string keyPath(RegKeyCategory category, const std::string& path)
{
    return fold(std::string(category == RegKeyLocalMachine ?
        "HKEY_LOCAL_MACHINE\\" : "HKEY_CURRENT_USER\\") + path);
}
} // namespace

Values parse(const std::string& text, const char* sourceName)
{
    Values values;
    // A leading UTF-8 byte-order mark is not part of the first line.
    std::istringstream input(text.compare(0, 3, "\xEF\xBB\xBF") == 0 ? text.substr(3) : text);
    std::string line;
    unsigned int number = 0;
    while (std::getline(input, line)) {
        ++number;
        line = trim(line);
        if (line.empty() || line[0] == '#' || line[0] == ';')
            continue;
        const auto separator = line.find('=');
        const std::string key = fold(trim(line.substr(0, separator)));
        const auto last = key.find_last_of('\\');
        if (separator == std::string::npos || line.find('\0') != std::string::npos ||
            (key.compare(0, 19, "hkey_local_machine\\") != 0 &&
             key.compare(0, 18, "hkey_current_user\\") != 0) ||
            last == std::string::npos || last + 1 == key.size() ||
            key.find("\\\\") != std::string::npos) {
            // Never include values: the file can contain a CD key.
            std::fprintf(stderr, "%s: ignoring malformed line %u\n", sourceName, number);
            std::fflush(stderr);
            continue;
        }
        values[key] = trim(line.substr(separator + 1));
    }
    return values;
}

bool readValue(const Values& values, RegKeyCategory category, const std::string& path,
               const std::string& name, std::string& output)
{
    const auto found = values.find(keyPath(category, path) + "\\" +
                                   (name.empty() ? "@" : fold(name)));
    if (found == values.end())
        return false;
    output = found->second;
    return true;
}

bool keyExists(const Values& values, RegKeyCategory category, const std::string& path)
{
    const std::string prefix = keyPath(category, path) + "\\";
    const auto found = values.lower_bound(prefix);
    return found != values.end() && found->first.compare(0, prefix.size(), prefix) == 0;
}

bool readGameValue(const Values& values, RegKeyCategory category, const std::string& name,
                   const std::string& modName, std::string& output)
{
    const std::string path = std::string("SOFTWARE\\Electronic Arts\\EA Games\\") +
        (modName.empty() ? "Battlefield 2" : modName);
    return readValue(values, category, path, name, output) ||
           readValue(values, category, path + "\\" + name, "", output);
}

#ifdef _WIN32
bool unprotect(const std::string& blob, std::string& text)
{
    DATA_BLOB input;
    input.pbData = reinterpret_cast<BYTE*>(const_cast<char*>(blob.data()));
    input.cbData = static_cast<DWORD>(blob.size());
    DATA_BLOB output = { 0, 0 };
    // CurrentUser scope (the blob was protected without CRYPTPROTECT_LOCAL_MACHINE), no entropy,
    // and never a prompt.
    if (blob.empty() || !CryptUnprotectData(&input, 0, 0, 0, 0, CRYPTPROTECT_UI_FORBIDDEN, &output))
        return false;
    std::string plain(reinterpret_cast<const char*>(output.pbData), output.cbData);
    SecureZeroMemory(output.pbData, output.cbData);
    LocalFree(output.pbData);
    // A writer that protects a C string with its terminator leaves a NUL behind the text.
    while (!plain.empty() && plain[plain.size() - 1] == '\0')
        plain.erase(plain.size() - 1);
    text.swap(plain);
    return true;
}

bool readProtectedFile(const std::wstring& path, Values& values)
{
    if (path.empty())
        return false;
    HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING,
                              FILE_ATTRIBUTE_NORMAL, 0);
    if (file == INVALID_HANDLE_VALUE)
        return false;
    std::string blob;
    LARGE_INTEGER size;
    bool read = GetFileSizeEx(file, &size) && size.QuadPart > 0 && size.QuadPart <= 64 * 1024;
    if (read) {
        blob.resize(static_cast<size_t>(size.QuadPart));
        DWORD bytes = 0;
        read = ReadFile(file, &blob[0], static_cast<DWORD>(blob.size()), &bytes, 0) &&
               bytes == blob.size();
    }
    CloseHandle(file);
    std::string text;
    if (!read || !unprotect(blob, text)) {
        std::fprintf(stderr, "Registry.dat: unreadable, using the registry\n");
        std::fflush(stderr);
        return false;
    }
    values = parse(text, "Registry.dat");
    SecureZeroMemory(&text[0], text.size());
    return true;
}

std::wstring protectedFilePath()
{
    PWSTR folder = 0;
    std::wstring path;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, 0, &folder)) && folder && *folder)
        path = std::wstring(folder) + L"\\OpenBF2\\Registry.dat";
    CoTaskMemFree(folder);
    return path;
}
#endif
} // namespace registryFile
#endif

#if !defined(_WIN32) && !DEDICATED
namespace posixRegistry {
// Each lookup reopens the file; nothing is cached.
registryFile::Values readFile()
{
#if defined(__APPLE__)
    const std::string home = macos::clientDataDirectory();
#else
    // Use the filesystem's @HOME@ policy when a future POSIX client supplies it.
    if (!io::fileManager)
        return registryFile::Values();
    const std::string home = io::fileManager->getHomeSubstitution();
#endif
    if (home.empty())
        return registryFile::Values();
    std::ifstream input(home + "/Registry.con", std::ios::binary);
    std::ostringstream text;
    text << input.rdbuf();
    if (input.bad())
        return registryFile::Values();
    return registryFile::parse(text.str(), "Registry.con");
}

bool readValue(RegKeyCategory category, const std::string& path,
               const std::string& name, std::string& output)
{
    return registryFile::readValue(readFile(), category, path, name, output);
}

bool keyExists(RegKeyCategory category, const std::string& path)
{
    return registryFile::keyExists(readFile(), category, path);
}
} // namespace posixRegistry
#endif

// Client PE 0x450FE0. [PLATFORM] ELF 0x83CB6F0 constructs an empty string and
// ignores even the supplied default; this is an actual shipped Linux body.
std::string Registry::getString(RegKeyCategory category, const std::string& name,
                                const std::string& defaultValue, const std::string& modName) const
{
#ifdef _WIN32
    std::string result = defaultValue;
#if !DEDICATED
    // [CHOSEN, not recovered] Owner decision: a value in the per-user, DPAPI-protected Registry.dat
    // wins; without the file, or without the value in it, the registry is read as retail does.
    registryFile::Values values;
    if (registryFile::readProtectedFile(registryFile::protectedFilePath(), values) &&
        registryFile::readGameValue(values, category, name, modName, result))
        return result;
#endif
    readRegistryString(category, name, defaultValue, result, modName);
    return result;
#elif !DEDICATED
    // [CHOSEN, not recovered] POSIX clients read Registry.con; missing values retain the
    // Windows absent-key defaults.
    std::string result;
    if (registryFile::readGameValue(posixRegistry::readFile(), category, name, modName, result))
        return result;
    return defaultValue;
#else
    return std::string("");
#endif
}

} }
