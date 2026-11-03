// Engine string/path helpers from the attested Core/StringUtil.cpp translation unit.

#ifndef CORE_STRINGUTIL_H     // [INFERRED] guard style
#define CORE_STRINGUTIL_H

#include <string>

namespace dice {
namespace hfe {

// Formats printf-style arguments in a shared buffer and returns a copy; not thread-safe.
std::string formatString(const char* fmt, ...);
std::wstring formatWstring(const wchar_t* fmt, ...);
#if !DEDICATED
// Client PE 431130: wide overload, including the shared buffer and overflow warning.
// [INFERRED] overload spelling; the dedicated ELF has no HUD caller for it.
std::wstring formatString(const wchar_t* fmt, ...);
#endif

// Converts multibyte text to wide characters, returning an empty string on failure. [INFERRED] Name.
std::wstring toWideString(const std::string& s);

// Case-insensitive comparison
// Four overloads exist. The (const char*, const char*) form is a straight strcasecmp; the
// std::string forms null-terminate their operands first and then delegate to it.
int  compareIgnoreCase(const char* a, const char* b);
int  compareIgnoreCase(const char* a, const char* b, int n);
int  compareIgnoreCase(const std::string& a, const std::string& b);
int  compareIgnoreCase(const std::string& a, const std::string& b, int n);

bool equalsIgnoreCase(const std::string& a, const std::string& b);
bool equalsIgnoreCase(const std::string& a, const std::string& b, int n);
std::string::size_type lowerCaseFind(const std::string& text, const std::string& pattern);
// Removes only the first occurrence, if any.
void eraseSubString(std::string& text, const std::string& pattern);
// Replaces matches in place and returns text; text must be non-null.
// An empty pattern has no terminating case.
std::string* replaceAll(std::string* text, const std::string& pattern, const std::string& replacement);

// Case conversion
// The set* forms are 479 bytes each - they go through the locale, not a naive toupper loop.
// The get* forms are 46-byte wrappers that copy and then call the corresponding set*.
void        setUpperCase(std::string&);
void        setLowerCase(std::string&);
std::string getUpperCase(const std::string&);
std::string getLowerCase(const std::string&);
std::wstring makeWide(const std::string&);
std::string  makeNarrow(const std::wstring&);

// Returns the case-insensitive hash used for engine hash-table bucket selection.
unsigned int calcStringHashValueNoCase(const std::string&);

// Paths
// note the difference between the two "fix" families, which is easy to miss and is load-bearing:
//   fixPathDelimiters  normalises separators only
//   fixPath            normalises separators and guarantees a trailing separator
// getFixPath / getFixPathDelimiters are the copying forms of each.
void        fixPathDelimiters(std::string&, char);
void        fixPath(std::string&, char);
std::string getFixPathDelimiters(const std::string&, char);
std::string getFixPath(const std::string&, char);
void         fixPathDelimiters(std::wstring&, wchar_t);
void         fixPath(std::wstring&, wchar_t);
std::wstring getFixPathDelimiters(const std::wstring&, wchar_t);
std::wstring getFixPath(const std::wstring&, wchar_t);

bool        isPathAbsolute(const std::string&);
// Recursively removes doubled forward slashes; backslashes are unchanged.
void        removeDoubleSlashes(std::string&);
// Collapses /../ segments and repeated forward slashes, preserving an absolute leading slash.
// Traversal beyond the beginning is absorbed; this is not a containment check.
void        goUpInPath(std::string&);

std::string extractFilePath(const std::string&, char);
std::string extractFileName(const std::string&);
std::string extractFileNameWithoutExt(const std::string&);
std::string extractFileExt(const std::string&);
std::string extractEverythingExceptExt(const std::string&);                  // internal 0x83d17f0
std::wstring extractFilePath(const std::wstring&, wchar_t);
std::wstring extractFileName(const std::wstring&);
std::wstring extractFileNameWithoutExt(const std::wstring&);
std::wstring extractFileExt(const std::wstring&);
std::wstring extractEverythingExceptExt(const std::wstring&);

// Misc
// Returns a shared empty std::string. Used as a sentinel return by FileManager::fileExistsEx and
// getFileInfo rather than constructing a fresh empty string each time.
const std::string& getEmptyString();

} // namespace hfe
} // namespace dice

#endif // CORE_STRINGUTIL_H
