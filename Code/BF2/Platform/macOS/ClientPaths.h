// [CHOSEN, not recovered] macOS client profile and instance-lock policy; docs/macos-port.md.
#ifndef PLATFORM_MACOS_CLIENTPATHS_H
#define PLATFORM_MACOS_CLIENTPATHS_H

#if defined(__APPLE__) && !DEDICATED
#include <string>

namespace dice { namespace hfe { namespace macos {

// Returns ~/Library/Application Support/OpenBF2, creating missing directories with mode 0700.
// HOME takes precedence over the account home so isolated runs can redirect all client data.
// An unavailable home or directory creation failure reports to stderr and aborts.
const std::string& clientDataDirectory();

// Opens and exclusively locks Client.lock in the working directory (the game folder), without waiting.
// Returns an owned close-on-exec descriptor, or -1 when another instance holds the lock.
// Other failures report to stderr and abort. Close the descriptor to release; never unlink it.
int acquireClientInstanceLock();

} } }
#endif
#endif
