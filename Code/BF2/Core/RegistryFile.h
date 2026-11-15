// [CHOSEN, not recovered] The client's registry file, shared by every client platform: the plain
// Registry.con on POSIX (PosixRegistry.h) and, on Windows, Registry.dat, the same text protected with
// DPAPI. Owner decisions; the file contract is docs/registry-file.md. Bodies in Core/Registry.cpp.
#ifndef CORE_REGISTRYFILE_H
#define CORE_REGISTRYFILE_H

#if !DEDICATED
#include "Registry.h"
#include <map>
#include <string>

namespace dice { namespace hfe { namespace registryFile {

// Folded "hive\path\name" (a final "@" for a key's default value) -> value text.
typedef std::map<std::string, std::string> Values;

// Parses the file text: one `HIVE\path\name = value` line per value. sourceName only names the file
// in the line-number-only diagnostics; values never appear in them (the file holds CD keys).
Values parse(const std::string& text, const char* sourceName);

// A named value of `HIVE\path`; an empty name denotes the key's default value.
bool readValue(const Values& values, RegKeyCategory category, const std::string& path,
               const std::string& name, std::string& output);

// True when the text has a value in `HIVE\path` or one of its descendants.
bool keyExists(const Values& values, RegKeyCategory category, const std::string& path);

// Registry::getString's lookup: the value `name` of SOFTWARE\Electronic Arts\EA Games\<mod or
// "Battlefield 2">, else the default value of its subkey `name`, as the registry reader falls back.
// Leaves output untouched when neither is present.
bool readGameValue(const Values& values, RegKeyCategory category, const std::string& name,
                   const std::string& modName, std::string& output);

#ifdef _WIN32
// CryptUnprotectData(CurrentUser blob, no entropy, CRYPTPROTECT_UI_FORBIDDEN); trailing NUL bytes of
// the plaintext are dropped. False when the blob does not decrypt for this user.
bool unprotect(const std::string& blob, std::string& text);

// Reads, decrypts and parses the file at path. False, leaving values untouched, when it is absent,
// unreadable, larger than 64 KiB or does not decrypt.
bool readProtectedFile(const std::wstring& path, Values& values);

// %LOCALAPPDATA%\OpenBF2\Registry.dat (FOLDERID_LocalAppData); empty when the folder is unknown.
std::wstring protectedFilePath();
#endif

} } }
#endif
#endif
