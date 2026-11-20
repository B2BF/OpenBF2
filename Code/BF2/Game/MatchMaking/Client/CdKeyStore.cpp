// Path:     [INFERRED] - see CdKeyStore.h for the PE bracketing that placed this file and for the
//           note that retail splits it across two translation units.
// Contents: BF2.exe (1.50 retail client), Every body below is transcribed from the
//           address named above it; nothing here is a stub and nothing returns a fabricated value.

#include "CdKeyStore.h"
#if !DEDICATED

#if defined(_WIN32)
#include <windows.h>
#include <wincrypt.h>
#else
// [CHOSEN, not recovered] macOS: the DPAPI counterpart (docs/macos-port.md, "Protected profile
// data (DPAPI)"). The flow below is the shipped one; only the API calls differ.
#include "../../../Platform/macOS/ProtectedData.h"
#endif

#include <cstdlib>                                   // malloc / free - the 402CF0 / 402D10 pair
#include <cstring>                                   // memcmp / strlen
#include <iomanip>                                   // setw / setfill
#include <ios>                                       // hex
#include <ostream>
#include <sstream>

#include "../../../Core/Registry.h"                  // registry->getString (PE 450FE0)
#include "../../../Core/StringUtil.h"                // compareIgnoreCase (PE 431950)

namespace dice {
namespace hfe {

namespace {

// ds:9EBBCC. A file-scope std::string with a dynamic initialiser, not a literal: the TU's static
// constructor sub_853440 is `std::string::string(&unk_9EBBCC, "x9392"); atexit(sub_878830);`.
// Reproduced as one, because its size() is what translateCdKey compares and strips.
std::string g_cdKeyMarker("x9392");

// Converts lowercase hex pairs without validation; out must hold hexText.size()/2 bytes.
// The unused stringstream's construction and destruction are preserved.
void hexToBytes(const std::string& hexText, unsigned char* out)
{
    const unsigned int count = static_cast<unsigned int>(hexText.size()) >> 1;

    std::stringstream unused;                        // built, never written to, destroyed
    std::istringstream in(hexText);

    for (unsigned int i = 0; i < count; ++i)
    {
        char high = 0;
        char low  = 0;
        in >> high >> low;

        const char highValue = (high <= '9') ? static_cast<char>(high - '0')
                                             : static_cast<char>(high - 87);
        const char lowValue  = (low  <= '9') ? static_cast<char>(low  - '0')
                                             : static_cast<char>(low  - 87);
        out[i] = static_cast<unsigned char>(lowValue + 16 * highValue);
    }
}

} // namespace

// ProtectedData

// `mov eax,ecx / mov dword ptr [eax],0 / retn`.
ProtectedData::ProtectedData()
    : m_blob(0)
{
}

ProtectedData::~ProtectedData()
{
    release();
}

// Inlined at both of its retail sites (the destructor 5B0190 and protect's re-arm). The blob struct
// comes from the engine heap (operator new / the 402D10 free thunk); its pbData comes from dpapi and
// must go back through LocalFree.
void ProtectedData::release()
{
    if (m_blob)
    {
#if defined(_WIN32)
        ::LocalFree(m_blob->pbData);
#else
        macos::freeProtectedData(m_blob->pbData);
#endif
        delete m_blob;
        m_blob = 0;
    }
}

// Encrypts the terminating NUL with the text. Empty input returns false without allocating a blob.
// Calling writeHex after that failure dereferences null, as in the original.
bool ProtectedData::protect(const std::string& plainText)
{
    DATA_BLOB in;
    const char* const text = plainText.c_str();
#if defined(_WIN32)
    in.pbData = reinterpret_cast<BYTE*>(const_cast<char*>(text));
    in.cbData = static_cast<DWORD>(std::strlen(text) + 1);
#else
    in.pbData = reinterpret_cast<unsigned char*>(const_cast<char*>(text));
    in.cbData = static_cast<std::uint32_t>(std::strlen(text) + 1);
#endif

    if (plainText.empty())
        return false;

    release();

    m_blob = new DATA_BLOB;                          // : operator new(8)
#if defined(_WIN32)
    return ::CryptProtectData(&in, L"This is the description string.",
                              0, 0, 0, CRYPTPROTECT_UI_FORBIDDEN, m_blob) != FALSE;
#else
    // A failed protect leaves the blob empty, as a failed CryptProtectData does.
    m_blob->cbData = 0;
    m_blob->pbData = 0;
    return macos::protectData(in.pbData, in.cbData, m_blob);
#endif
}

// Decodes a dpapi-protected hexadecimal value in place. Assigns plaintext only on
// success; failure preserves the original text and leaks the temporary decode buffer.
bool ProtectedData::unprotect(std::string& hexText)
{
#if defined(_WIN32)
    const DWORD count = static_cast<DWORD>(hexText.size() >> 1);
    LPWSTR description = 0;
#else
    const std::uint32_t count = static_cast<std::uint32_t>(hexText.size() >> 1);
#endif

    if (!count)
        return false;

    unsigned char* const bytes = static_cast<unsigned char*>(std::malloc(count));   // : the engine malloc
    hexToBytes(hexText, bytes);

    DATA_BLOB in;
    in.pbData = bytes;
    in.cbData = count;

    DATA_BLOB out;
#if defined(_WIN32)
    if (::CryptUnprotectData(&in, &description, 0, 0, 0, CRYPTPROTECT_UI_FORBIDDEN, &out))
    {
        hexText = std::string(reinterpret_cast<const char*>(out.pbData));
        ::LocalFree(description);
        std::free(bytes);
        ::LocalFree(out.pbData);
        return true;
    }
#else
    out.cbData = 0;
    out.pbData = 0;
    // The plain text carries its terminator (protect includes it), so it is read as a C string.
    if (macos::unprotectData(in.pbData, in.cbData, &out) && out.cbData && out.pbData[out.cbData - 1] == 0)
    {
        hexText = std::string(reinterpret_cast<const char*>(out.pbData));
        std::free(bytes);
        macos::freeProtectedData(out.pbData);
        return true;
    }
    if (out.pbData)
        macos::freeProtectedData(out.pbData);
    // As shipped, `bytes` is not freed on failure; there is no system message to format.
    return false;
#endif
#if defined(_WIN32)

    char* message = 0;
    if (::FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                         FORMAT_MESSAGE_IGNORE_INSERTS,
                         0, ::GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                         reinterpret_cast<LPSTR>(&message), 0, 0))
        ::LocalFree(message);
    return false;
#endif
}

// Writes the encrypted blob as hex. Requires a non-null blob from successful protection.
void ProtectedData::writeHex(std::ostream& stream)
{
    for (unsigned int i = 0; i < m_blob->cbData; ++i)
        stream << std::hex << std::setw(2) << std::setfill('0')
               << static_cast<int>(m_blob->pbData[i]);
}

// Reads ergc from the product's registry folder, returning empty when absent.
// An empty product name selects Battlefield 2.
std::string getRegistryCdKey(std::string productName)
{
    std::string key("");
    key = registry->getString(RegKeyLocalMachine, std::string("ergc"), std::string(""), productName);
    return key;
}

// Decrypts marked keys and encrypts unmarked keys. Both dpapi results are ignored;
// failed decryption leaves the payload unchanged.
std::string translateCdKey(const std::string& storedKey)
{
    std::string result("");

    // [CHOSEN, not recovered] Retail protects an empty key too: protect refuses it, and
    // writeHex then dereferences the null blob, so joining a server without a stored key
    // crashes at the first challenge. Answer empty instead; GameClient::handleChallenge
    // already disconnects with reason 7 when the key is empty.
    if (storedKey.empty())
        return result;

    bool marked;
    if (storedKey.size() > g_cdKeyMarker.size())
        marked = std::memcmp(g_cdKeyMarker.c_str(), storedKey.c_str(), g_cdKeyMarker.size()) == 0;
    else
        marked = false;

    std::string body;
    if (marked)
        body = storedKey.substr(g_cdKeyMarker.size(), storedKey.size() - g_cdKeyMarker.size());
    else
        body = storedKey;

    ProtectedData blob;                              // : before the branch, as shipped

    if (marked)
    {
        blob.unprotect(body);                        // answer dropped ON purpose - see above
        result = body;                               // decrypted, or the payload verbatim
    }
    else
    {
        std::stringstream hex;
        blob.protect(body);                          // answer dropped
        blob.writeHex(hex);                          // faults when protect refused an empty key
        result = g_cdKeyMarker + hex.str();
    }

    return result;
}

// getExpansionProductName - 59F580, 92 B. The only mod the client treats as an expansion is
// "xpack"; every other mod name - including every community mod - answers with the empty string,
// which is what makes handleChallenge send one event instead of two.
std::string getExpansionProductName(std::string modName)
{
    const bool isExpansion = compareIgnoreCase(modName, std::string("xpack")) == 0;

    if (isExpansion)
        return std::string("Battlefield 2 Special Forces");
    return std::string("");
}

// getExpansionGameId - 59F640, 91 B. gamespy::g_XPackGameID. The client body spells it as an
// immediate (`mov eax, 4E8h`), not as a load of the global the server side uses; kept as read.
int getExpansionGameId(std::string modName)
{
    const bool isExpansion = compareIgnoreCase(modName, std::string("xpack")) == 0;

    if (isExpansion)
        return 1256;
    return -1;
}

} // namespace hfe
} // namespace dice

#endif // !dedicated
