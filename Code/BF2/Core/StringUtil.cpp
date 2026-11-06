// Shared string comparisons, hashing, formatting and path helpers.
// Source path is attested by anonymous-namespace symbols.

#include "StringUtil.h"

#include <cstdarg>      // formatString is variadic
#include <cstdlib>      // mbstowcs
#include <cstdio>       // vsnprintf / sprintf
#include <cwchar>
#include <vector>

#ifdef _MSC_VER
#  include <string.h>
#  define strcasecmp  _stricmp
#  define strncasecmp _strnicmp
#else
#  include <strings.h>
#endif

namespace {

// ELF 0x48BE30 (string), 0x48C420 (wstring); engine ELF 0x83D1510/0x83D1A40.
template <typename String>
String extractFileExt_internal(const String& path)
{
    typedef typename String::value_type Char;
    const int dot = static_cast<int>(path.rfind(static_cast<Char>('.')));
    if (dot == -1 || (dot > 1 && path[dot - 1] == static_cast<Char>('.')))
        return String();
    return path.substr(dot + 1);
}

// ELF 0x48BEF0 (string), 0x48C4E0 (wstring); engine ELF 0x83D15B0/0x83D1AE0.
template <typename String>
String extractFileName_internal(const String& path)
{
    typedef typename String::value_type Char;
    int last = static_cast<int>(path.rfind(static_cast<Char>('\\')));
    const int slash = static_cast<int>(path.rfind(static_cast<Char>('/')));
    if (slash > last) last = slash;
    return last == -1 ? path : path.substr(last + 1);
}

// ELF 0x48BFB0 (string), 0x48C5A0 (wstring); engine ELF 0x83D1660/0x83D1B90.
template <typename String, typename Char>
String extractFilePath_internal(const String& path, Char delimiter)
{
    int last = static_cast<int>(path.rfind(static_cast<Char>('\\')));
    const int slash = static_cast<int>(path.rfind(static_cast<Char>('/')));
    if (slash > last) last = slash;
    if (last == -1) return String();
    return path.substr(0, last) + delimiter;
}

// ELF 0x48C0A0 (string), 0x48C690 (wstring); engine ELF 0x83D1740/0x83D1C70.
template <typename String>
String extractFileNameWithoutExt_internal(const String& path)
{
    const String name = dice::hfe::extractFileName(path);
    const int dot = static_cast<int>(name.rfind(static_cast<typename String::value_type>('.')));
    return dot == -1 ? name : name.substr(0, dot);
}

// ELF 0x48C160 (string), 0x48C750 (wstring); engine ELF 0x83D17F0/0x83D1D20.
template <typename String>
String extractEverythingExceptExt_internal(const String& path)
{
    typedef typename String::value_type Char;
    const String normalized = dice::hfe::getFixPathDelimiters(path, static_cast<Char>('/'));
    const int dot = static_cast<int>(normalized.rfind(static_cast<Char>('.')));
    return dot == -1 ? normalized : normalized.substr(0, dot);
}

// ELF 0x48C230 (string), 0x48C820 (wstring); engine ELF 0x83D18A0/0x83D1DD0.
template <typename String, typename Char>
void fixPath_internal(String& path, Char delimiter)
{
    if (!path.empty())
    {
        dice::hfe::fixPathDelimiters(path, delimiter);
        if (path[path.length() - 1] != delimiter)
            path.append(1, delimiter);
    }
}

// ELF 0x48C2D0 (string), 0x48C8A0 (wstring); engine ELF 0x83D1920/0x83D1E50.
template <typename String, typename Char>
void fixPathDelimiters_internal(String& path, Char delimiter)
{
    const typename String::size_type length = path.length();
    for (typename String::size_type i = 0; i < length; ++i)
        if (path[i] == static_cast<Char>('/') || path[i] == static_cast<Char>('\\'))
            path[i] = delimiter;
}

// ELF 0x48C380 (string), 0x48C950 (wstring); engine ELF 0x83D19C0/0x83D1EF0.
template <typename String, typename Char>
String getFixPath_internal(const String& path, Char delimiter)
{
    String result(path);
    dice::hfe::fixPath(result, delimiter);
    return result;
}

// ELF 0x48C3D0 (string), 0x48C9A0 (wstring); engine ELF 0x83D1A00/0x83D1F30.
template <typename String, typename Char>
String getFixPathDelimiters_internal(const String& path, Char delimiter)
{
    String result(path);
    dice::hfe::fixPathDelimiters(result, delimiter);
    return result;
}

} // anonymous namespace

namespace dice {
namespace hfe {

// ELF 0x48A920.
// static init: ELF 0x48BDE0 (getEmptyString's local static teardown).
const std::string& getEmptyString()
{
    static const std::string empty;
    return empty;
}

// ELF 0x48BA40. Converts wide text using the active C locale. The existing error check tests count + 1,
// so callers must not rely on it to reject every invalid conversion.
std::string makeNarrow(const std::wstring& source)
{
    const std::size_t capacity = std::wcstombs(0, source.c_str(), 0) + 1;
    if (capacity == static_cast<std::size_t>(-1))
        return "no_conversion_exists";
    std::vector<char> buffer(capacity, '\0');
    const std::size_t converted = std::wcstombs(buffer.data(), source.c_str(), capacity);
    if (converted == static_cast<std::size_t>(-1))
        return "no_conversion_exists";
    if (converted == 0)
        return "";
    return std::string(buffer.data(), converted);
}

// ELF 0x48BCA0. Converts multibyte text to wide characters using the active C locale.
std::wstring makeWide(const std::string& source)
{
    const std::size_t capacity = std::mbstowcs(0, source.c_str(), 0) + 1;
    wchar_t* buffer = new wchar_t[capacity];
    const std::size_t converted = std::mbstowcs(buffer, source.c_str(), capacity);

    std::wstring result;
    if (converted != 0)
        result.assign(buffer, converted);

    delete[] buffer;
    return result;
}

// ELF 0x48A990. Formats into a shared 4-KB buffer and returns a string copy; not reentrant or thread-safe.
// A negative vsnprintf result replaces the message with a warning. Successful formatting writes
// the terminator at the reported length. Preserve the original runtime-dependent overflow behavior.
std::string formatString(const char* fmt, ...)
{
    // ds:0x8ABBD20. 4096 bytes - vsnprintf is given 4095 and byte [n] is written explicitly.
    static char s_buffer[4096];             // [INFERRED] name; the ELF slot is unnamed

    va_list args;
    va_start(args, fmt);
    const int n = vsnprintf(s_buffer, 0xFFF, fmt, args);   // 0x83D03DA maxlen, 0x83D03EA call
    va_end(args);                                          // no-op on this ABI; not in the binary

    if (n == -1)
        sprintf(s_buffer, "%s",
                "WARNING: The original message was too long for formatString(). \n");
    else
        s_buffer[n] = 0;

    return std::string(s_buffer);
}

// ELF 0x48AA30; engine ELF 0x83D0440.
std::wstring formatWstring(const wchar_t* fmt, ...)
{
    static wchar_t buffer[4096];
    va_list args;
    va_start(args, fmt);
    const int count = std::vswprintf(buffer, 0xFFF, fmt, args);
    va_end(args);
    if (count == -1)
    {
        // shipped defect: POSIX %s reads the wide warning as narrow bytes, producing only "W".
        std::swprintf(buffer, 0x1000, L"%s",
            L"WARNING: The original message was too long for formatString(). \n");
    }
    else
        buffer[count] = 0;
    return std::wstring(buffer);
}

#if !DEDICATED
// Client PE 431130, all 29 instructions read. Buffer 99C158 is shared, and
// _vsnwprintf's -1 result replaces the whole message with the shipped warning.
std::wstring formatString(const wchar_t* fmt, ...)
{
    static wchar_t buffer[4096];
    va_list args;
    va_start(args, fmt);
    const int count = _vsnwprintf(buffer, 0xFFF, fmt, args);
    va_end(args);
    if (count == -1)
        _swprintf(buffer, L"%s",
            L"WARNING: The original message was too long for formatString(). \n");
    else
        buffer[count] = 0;
    return std::wstring(buffer);
}
#endif

// ELF 0x48AB30. Compares null-terminated strings without case. String overloads delegate through c_str().
int compareIgnoreCase(const char* a, const char* b)
{
    return strcasecmp(a, b);
}

// ELF 0x48ABD0.
int compareIgnoreCase(const char* a, const char* b, int n)
{
    if (n == 0)
        return 0;
    return strncasecmp(a, b, n);
}

// ELF 0x48AAD0.
int compareIgnoreCase(const std::string& a, const std::string& b)
{
    return compareIgnoreCase(a.c_str(), b.c_str());
}

// ELF 0x48AB60.
int compareIgnoreCase(const std::string& a, const std::string& b, int n)
{
    return compareIgnoreCase(a.c_str(), b.c_str(), n);
}

// ELF 0x48AC10.
bool equalsIgnoreCase(const std::string& a, const std::string& b)
{
    return compareIgnoreCase(a, b) == 0;
}

// ELF 0x48AC50.
bool equalsIgnoreCase(const std::string& a, const std::string& b, int n)
{
    return compareIgnoreCase(a, b, n) == 0;
}

// ELF 0x48AC90; engine ELF 0x83D0600. amd-64 preserves the full size_type return.
std::string::size_type lowerCaseFind(const std::string& text, const std::string& pattern)
{
    std::string loweredText(text);
    std::string loweredPattern(pattern);
    setLowerCase(loweredText);
    setLowerCase(loweredPattern);
    return loweredText.find(loweredPattern);
}

// ELF 0x48AD60; engine ELF 0x83D06B0.
void eraseSubString(std::string& text, const std::string& pattern)
{
    const std::string::size_type at = text.find(pattern);
    if (at != std::string::npos)
        text.erase(at, pattern.length());
}

// ELF 0x48B9A0; engine ELF 0x83D10F0. Both images explicitly return the input pointer.
std::string* replaceAll(std::string* text, const std::string& pattern, const std::string& replacement)
{
    std::string::size_type at = 0;
    while ((at = text->find(pattern, at)) != std::string::npos)
    {
        text->replace(at, pattern.length(), replacement);
        // Read the replacement length after the mutation too, preserving argument aliasing.
        at += replacement.length();
    }
    return text;
}

// ELF 0x487AF0. Returns the engine's case-insensitive hash using ASCII-only uppercase folding.
// Preserve its constants and shifts because bucket order affects duplicate selection.
unsigned int calcStringHashValueNoCase(const std::string& s)
{
    unsigned int a    = 39842;
    unsigned int b    = 230498;
    unsigned int hash = 0;

    for (unsigned int i = 0; i < s.size(); )
    {
        unsigned char c = static_cast<unsigned char>(s[i]);
        if (static_cast<unsigned char>(c - 'a') <= 25)
            c -= 32;                                   // ASCII-only uppercase

        const int shiftA = (b & 7) - (a & 7);
        const unsigned int bPrev = b;
        b -= 1994177228u;

        const unsigned int term = static_cast<unsigned int>(c) << (shiftA + 16);
        const int shiftB = (a & 3) - (bPrev & 3);
        a += 1465131657u;

        hash += (i++) | (((hash << (shiftB + 15)) + term) >> 14);
    }

    return (hash >> 16) + hash;                        // hiword(hash) + hash
}

// ELF 0x48B910. Reports whether the virtual-filesystem path is absolute under the selected platform rules.
// [PLATFORM] POSIX requires a leading slash; Windows searches for :\ or :/ anywhere in the string.
// Leading slash alone is relative on Windows, allowing engine search-path resolution.
bool isPathAbsolute(const std::string& s)
{
#ifdef _WIN32
    return s.find(":\\") != std::string::npos || s.find(":/") != std::string::npos;
#else
    return !s.empty() && s[0] == '/';
#endif
}

// Normalizes both slash forms to c. fixPath additionally ensures a trailing separator;
// both preserve an empty string as empty.
// ELF 0x48B3E0.
void fixPathDelimiters(std::string& s, char c)
{
    fixPathDelimiters_internal(s, c);
}

// ELF 0x48B3B0.
void fixPath(std::string& s, char c)
{
    fixPath_internal(s, c);
}

// ELF 0x48B460.
std::string getFixPathDelimiters(const std::string& s, char c)
{
    return getFixPathDelimiters_internal(s, c);
}

// ELF 0x48B410.
std::string getFixPath(const std::string& s, char c)
{
    return getFixPath_internal(s, c);
}

// ELF 0x48B600; engine ELF 0x83D0DF0.
void fixPath(std::wstring& s, wchar_t c) { fixPath_internal(s, c); }
// ELF 0x48B630; engine ELF 0x83D0E00.
void fixPathDelimiters(std::wstring& s, wchar_t c) { fixPathDelimiters_internal(s, c); }
// ELF 0x48B660; engine ELF 0x83D0E10.
std::wstring getFixPath(const std::wstring& s, wchar_t c) { return getFixPath_internal(s, c); }
// ELF 0x48B6B0; engine ELF 0x83D0E40.
std::wstring getFixPathDelimiters(const std::wstring& s, wchar_t c) { return getFixPathDelimiters_internal(s, c); }

// Changes case using explicit Latin-1 pairs for å/Å, ä/Ä, ö/Ö, ü/Ü and é/É before CRT fallback.
// Keep these mappings independent of the active locale; archive and content lookup depend on them.

namespace {

// The shared table, written once rather than twice. The binary has it open-coded in both
// directions; the two functions are byte-for-byte mirrors of each other.
struct CasePair { unsigned char lower; unsigned char upper; };

const CasePair kLatin1CasePairs[] =
{
    { 0xE5, 0xC5 },   // å Å
    { 0xE4, 0xC4 },   // ä Ä
    { 0xF6, 0xD6 },   // ö Ö
    { 0xFC, 0xDC },   // ü Ü
    { 0xE9, 0xC9 }    // é É
};

const unsigned int kLatin1CasePairCount = sizeof(kLatin1CasePairs) / sizeof(kLatin1CasePairs[0]);

} // anonymous namespace

// ELF 0x48AE00. Uppercases text in place, handling the explicit extended-character pairs before toupper.
void setUpperCase(std::string& s)
{
    for (std::string::size_type i = 0; i < s.size(); ++i)
    {
        const unsigned char c = static_cast<unsigned char>(s[i]);

        unsigned int p = 0;
        for (; p < kLatin1CasePairCount; ++p)
        {
            if (c == kLatin1CasePairs[p].lower)
            {
                s[i] = static_cast<char>(kLatin1CasePairs[p].upper);
                break;
            }
        }

        if (p == kLatin1CasePairCount)
            s[i] = static_cast<char>(toupper(c));
    }
}

// ELF 0x48AFF0. The exact mirror - same five pairs, tested upper-first.
void setLowerCase(std::string& s)
{
    for (std::string::size_type i = 0; i < s.size(); ++i)
    {
        const unsigned char c = static_cast<unsigned char>(s[i]);

        unsigned int p = 0;
        for (; p < kLatin1CasePairCount; ++p)
        {
            if (c == kLatin1CasePairs[p].upper)
            {
                s[i] = static_cast<char>(kLatin1CasePairs[p].lower);
                break;
            }
        }

        if (p == kLatin1CasePairCount)
            s[i] = static_cast<char>(tolower(c));
    }
}

// each: copy-construct, convert in place, return.

// these genuinely return by value, unlike IArchiveHandler::getExtensions - and the same ABI test
// settles it in the opposite direction. Here the body constructs into the hidden sret pointer
// (`std::string::string(this, a2)` where `this` is the return slot) and hands that back. A
// reference-returning function has nothing to construct.
// ELF 0x48B1E0.
std::string getUpperCase(const std::string& s)
{
    std::string result(s);
    setUpperCase(result);
    return result;
}

// ELF 0x48B220.
std::string getLowerCase(const std::string& s)
{
    std::string result(s);
    setLowerCase(result);
    return result;
}

// ELF 0x48B2E0. Returns the directory through the last slash of either kind, using delimiter for the trailing separator.
// Returns an empty string when no separator exists.
std::string extractFilePath(const std::string& path, char delimiter)
{
    return extractFilePath_internal(path, delimiter);
}

// ELF 0x48B260. Returns text after the last dot, or empty for no dot/trailing dot.
// A preceding dot suppresses extensions only when the last dot is at index 2 or later:
// ..x yields x, while a..x yields empty.
std::string extractFileExt(const std::string& path)
{
    return extractFileExt_internal(path);
}

// ELF 0x48B870; engine ELF 0x83D0FD0.
void removeDoubleSlashes(std::string& path)
{
    const std::string::size_type at = path.find("//");
    if (at != std::string::npos)
    {
        path.erase(at, 1);
        removeDoubleSlashes(path);
    }
}

namespace {
// ELF 0x48B700; engine ELF 0x83D0E70.
void goUpInPath_worker(std::string& path)
{
    removeDoubleSlashes(path);

    if (!path.empty() && path[0] == '/')
        path.erase(0, 1);

    const std::string::size_type found = path.find("/../");
    if (found == std::string::npos)
        return;

    std::string::size_type start = path.rfind("/", found - 1);
    if (start == std::string::npos)
        start = 0;

    std::string::size_type count = (found + 3) - start;
    if (count > path.size() - start)
        count = path.size() - start;

    path.erase(start, count);

    goUpInPath_worker(path);
}
} // anonymous namespace

// ELF 0x48B930; engine ELF 0x83D1090.
void goUpInPath(std::string& path)
{
    // The worker strips a leading '/'; the ELF restores it for an absolute path, and on ELF an
    // absolute path is exactly one with a leading '/'.
    // [PLATFORM] isPathAbsolute also accepts drive paths (C:/...) on Windows, which never had a
    // leading '/'; restoring one there yields "/C:/..." and breaks every file lookup. Windows
    // restores the slash only when the path started with one. [INFERRED] PE body not compared.
#if defined(_WIN32)
    const bool absolute = !path.empty() && path[0] == '/';
#else
    const bool absolute = isPathAbsolute(path);
#endif
    goUpInPath_worker(path);
    if (absolute)
        path.insert(0, "/");
}

// ELF 0x48B330. Returns the final path component without its last extension. Names without a dot remain unchanged.
std::string extractFileNameWithoutExt(const std::string& path)
{
    return extractFileNameWithoutExt_internal(path);
}
// ELF 0x48B2A0. Returns the component after the last slash of either kind. A trailing slash produces an empty name.
std::string extractFileName(const std::string& path)
{
    return extractFileName_internal(path);
}

// ELF 0x48B370; engine ELF 0x83D0C30.
std::string extractEverythingExceptExt(const std::string& path) { return extractEverythingExceptExt_internal(path); }
// ELF 0x48B4B0; engine ELF 0x83D0D00.
std::wstring extractFileExt(const std::wstring& path) { return extractFileExt_internal(path); }
// ELF 0x48B4F0; engine ELF 0x83D0D30.
std::wstring extractFileName(const std::wstring& path) { return extractFileName_internal(path); }
// ELF 0x48B530; engine ELF 0x83D0D60.
std::wstring extractFilePath(const std::wstring& path, wchar_t c) { return extractFilePath_internal(path, c); }
// ELF 0x48B580; engine ELF 0x83D0D90.
std::wstring extractFileNameWithoutExt(const std::wstring& path) { return extractFileNameWithoutExt_internal(path); }
// ELF 0x48B5C0; engine ELF 0x83D0DC0.
std::wstring extractEverythingExceptExt(const std::wstring& path) { return extractEverythingExceptExt_internal(path); }
// Converts multibyte text to wide characters; failed conversion returns an empty string.
std::wstring toWideString(const std::string& s)
{
    const size_t count = mbstowcs(0, s.c_str(), 0) + 1;         // + terminator; 0 on an invalid sequence
    wchar_t* buffer = new wchar_t[count];
    std::wstring result;
    if (mbstowcs(buffer, s.c_str(), count))
        result = buffer;
    else
        result = L"";
    delete[] buffer;
    return result;
}

} // namespace hfe
} // namespace dice
