// [CHOSEN, not recovered] Owner-requested macOS client startup policy; docs/macos-port.md.
#include "ClientPaths.h"

#if defined(__APPLE__) && !DEDICATED
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <pwd.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

namespace dice { namespace hfe { namespace macos {
namespace {

[[noreturn]] void pathFailure(const char* operation, const std::string& path, int error)
{
    std::fprintf(stderr, "dice::hfe::macos::ClientPaths: %s '%s': %s\n",
                 operation, path.c_str(), std::strerror(error));
    std::fflush(stderr);
    std::abort();
}

std::string makeClientDataDirectory()
{
    const char* home = std::getenv("HOME");
    if (!home || !*home) {
        const passwd* account = getpwuid(getuid());
        home = account ? account->pw_dir : nullptr;
    }
    if (!home || home[0] != '/')
        pathFailure("resolve absolute home", home ? home : "", EINVAL);

    const std::string path = std::string(home) + "/Library/Application Support/OpenBF2";
    for (std::size_t at = 1; at <= path.size(); ++at) {
        if (at != path.size() && path[at] != '/')
            continue;
        const std::string part = path.substr(0, at);
        if (mkdir(part.c_str(), 0700) != 0 && errno != EEXIST)
            pathFailure("mkdir", part, errno);
        struct stat info;
        if (stat(part.c_str(), &info) != 0)
            pathFailure("stat", part, errno);
        if (!S_ISDIR(info.st_mode))
            pathFailure("require directory", part, ENOTDIR);
    }
    return path;
}

} // namespace

const std::string& clientDataDirectory()
{
    static const std::string path = makeClientDataDirectory();
    return path;
}

int acquireClientInstanceLock()
{
    // [CHOSEN, not recovered] Retail BF2.exe guards with the named mutex
    // "Battlefield2_Instance_Mutex" and writes no lock file. The macOS lock file lives in the game
    // folder (the working directory the setup app starts the client in), where retail writes its
    // other non-profile files; only profiles go to the client data directory (owner, 2026-11-14;
    // docs/macos-write-paths.md).
    const std::string path = "Client.lock";
    const int fd = open(path.c_str(), O_CREAT | O_RDWR | O_CLOEXEC | O_NOFOLLOW, 0600);
    if (fd < 0)
        pathFailure("open instance lock", path, errno);
    struct stat info;
    if (fstat(fd, &info) != 0)
        pathFailure("fstat instance lock", path, errno);
    if (!S_ISREG(info.st_mode))
        pathFailure("require regular instance lock", path, EINVAL);
    if (flock(fd, LOCK_EX | LOCK_NB) != 0) {
        const int error = errno;
        close(fd);
        if (error == EWOULDBLOCK)
            return -1;
        pathFailure("flock instance lock", path, error);
    }
    return fd;
}

} } }
#endif
