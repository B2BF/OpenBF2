// [CHOSEN, not recovered] Owner-requested POSIX Registry.con backend.
#ifndef CORE_POSIXREGISTRY_H
#define CORE_POSIXREGISTRY_H

#if !defined(_WIN32) && !DEDICATED
#include "RegistryFile.h"

namespace dice { namespace hfe { namespace posixRegistry {

// Reads and parses @HOME@/Registry.con (RegistryFile.h); empty for an absent/unreadable file.
registryFile::Values readFile();

// Reads a named value from @HOME@/Registry.con. An empty name denotes the default value.
// Returns false for an absent/unreadable file or value, leaving output untouched.
bool readValue(RegKeyCategory category, const std::string& path,
               const std::string& name, std::string& output);

// Tests whether the file contains a value in this key or one of its descendants.
bool keyExists(RegKeyCategory category, const std::string& path);

} } }
#endif
#endif
