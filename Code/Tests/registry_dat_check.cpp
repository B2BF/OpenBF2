// [TEMP] [CHOSEN, not recovered] Windows-only check of the client's Registry.dat reader
// (docs/registry-file.md): synthetic CD keys protected with CryptProtectData (CurrentUser, no entropy)
// go through the production reader in Core/Registry.cpp. Fake keys only; never prints a value.
//
//   registry_dat_check                  self-contained round trips in %TEMP%
//   registry_dat_check <file> <key>     reads a Registry.dat written by tools/windows/new-registry-dat.ps1
//                                       and expects <key> as the Battlefield 2 ergc value
#include "../BF2/Core/Registry.h"
#include "../BF2/Core/RegistryFile.h"
#include <windows.h>
#include <wincrypt.h>
#include <cstdio>
#include <string>
#pragma comment(lib, "crypt32.lib")

using namespace dice::hfe;

static int failures;
#define CHECK(condition) do { if (!(condition)) { \
    std::fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #condition); ++failures; } } while (0)

namespace {
const char kFakeKey[] = "FAKE-0000-1111-2222-3333";
const char kFakeXpackKey[] = "FAKE-XPCK-4444-5555-6666";

std::string protect(const std::string& text, const std::string& entropy = std::string())
{
    DATA_BLOB input = { static_cast<DWORD>(text.size()),
                        reinterpret_cast<BYTE*>(const_cast<char*>(text.data())) };
    DATA_BLOB salt = { static_cast<DWORD>(entropy.size()),
                       reinterpret_cast<BYTE*>(const_cast<char*>(entropy.data())) };
    DATA_BLOB output = { 0, 0 };
    if (!CryptProtectData(&input, L"OpenBF2 Registry", entropy.empty() ? 0 : &salt, 0, 0,
                          CRYPTPROTECT_UI_FORBIDDEN, &output))
        return std::string();
    std::string blob(reinterpret_cast<const char*>(output.pbData), output.cbData);
    LocalFree(output.pbData);
    return blob;
}

bool writeFile(const std::wstring& path, const std::string& bytes)
{
    HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
    if (file == INVALID_HANDLE_VALUE)
        return false;
    DWORD written = 0;
    const bool ok = bytes.empty() ||
        (WriteFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &written, 0) && written == bytes.size());
    CloseHandle(file);
    return ok;
}

std::wstring widen(const char* text)
{
    const int size = MultiByteToWideChar(CP_UTF8, 0, text, -1, 0, 0);
    std::wstring wide(size > 0 ? size - 1 : 0, L'\0');
    if (size > 1)
        MultiByteToWideChar(CP_UTF8, 0, text, -1, &wide[0], size);
    return wide;
}

bool readErgc(const registryFile::Values& values, const std::string& modName, std::string& key)
{
    return registryFile::readGameValue(values, RegKeyLocalMachine, "ergc", modName, key);
}

int checkScriptFile(const char* path, const char* expected)
{
    registryFile::Values values;
    CHECK(registryFile::readProtectedFile(widen(path), values));
    std::string key;
    CHECK(readErgc(values, "", key) && key == expected);
    if (!failures)
        std::printf("registry_dat_check: the script's Registry.dat decrypts and parses\n");
    return failures ? 1 : 0;
}
} // namespace

int main(int argc, char** argv)
{
    if (argc == 3)
        return checkScriptFile(argv[1], argv[2]);

    wchar_t tempDirectory[MAX_PATH];
    const DWORD length = GetTempPathW(MAX_PATH, tempDirectory);
    if (!length || length >= MAX_PATH)
        return 2;
    const std::wstring path = std::wstring(tempDirectory) + L"openbf2-registry-dat-check-" +
        std::to_wstring(GetCurrentProcessId()) + L".dat";

    const std::string base = "HKEY_LOCAL_MACHINE\\SOFTWARE\\Electronic Arts\\EA Games\\";
    const std::string text =
        "# fake keys\r\n"
        + base + "Battlefield 2\\ergc = " + kFakeKey + "\r\n"
        + base + "Battlefield 2 Special Forces\\ERGC\\@ = " + kFakeXpackKey + "\r\n"
        "malformed line without a separator\r\n";

    // 1. The contract: UTF-8 text, CryptProtectData without entropy, the raw blob as the file.
    {
        CHECK(writeFile(path, protect(text)));
        registryFile::Values values;
        CHECK(registryFile::readProtectedFile(path, values));
        std::string key;
        CHECK(readErgc(values, "", key) && key == kFakeKey);
        key.clear();
        CHECK(readErgc(values, "Battlefield 2 Special Forces", key) && key == kFakeXpackKey);
        key = "unchanged";
        CHECK(!registryFile::readGameValue(values, RegKeyLocalMachine, "Language", "", key) &&
              key == "unchanged");
        CHECK(!registryFile::readGameValue(values, RegKeyCurrentUser, "ergc", "", key));
        CHECK(registryFile::keyExists(values, RegKeyLocalMachine,
                                      "software\\electronic arts\\ea games\\battlefield 2"));
    }
    // 2. A UTF-8 byte-order mark, LF endings and a protected terminating NUL are tolerated.
    {
        const std::string variant = std::string("\xEF\xBB\xBF") + base + "Battlefield 2\\ergc = " + kFakeKey;
        CHECK(writeFile(path, protect(variant + std::string(1, '\0'))));
        registryFile::Values values;
        std::string key;
        CHECK(registryFile::readProtectedFile(path, values) && readErgc(values, "", key) && key == kFakeKey);
    }
    // 3. Not a DPAPI blob for this user, protected with entropy, empty, or absent: the reader
    //    reports failure and leaves the values alone, so getString falls back to the registry.
    {
        registryFile::Values values;
        values["sentinel"] = "kept";
        CHECK(writeFile(path, text));
        CHECK(!registryFile::readProtectedFile(path, values));
        CHECK(writeFile(path, protect(text, "entropy")));
        CHECK(!registryFile::readProtectedFile(path, values));
        CHECK(writeFile(path, std::string()));
        CHECK(!registryFile::readProtectedFile(path, values));
        DeleteFileW(path.c_str());
        CHECK(!registryFile::readProtectedFile(path, values));
        CHECK(!registryFile::readProtectedFile(std::wstring(), values));
        CHECK(values.size() == 1 && values["sentinel"] == "kept");
    }
    // 4. The production location.
    {
        const std::wstring production = registryFile::protectedFilePath();
        const std::wstring suffix = L"\\OpenBF2\\Registry.dat";
        CHECK(production.size() > suffix.size() &&
              production.compare(production.size() - suffix.size(), suffix.size(), suffix) == 0);
    }
    DeleteFileW(path.c_str());
    if (!failures)
        std::printf("registry_dat_check: all checks passed\n");
    return failures ? 1 : 0;
}
