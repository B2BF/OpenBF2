// [TEMP] [CHOSEN, not recovered] Checks only client data paths and advisory lock lifecycle.
// No engine, content, window, renderer or networking is initialized.
#include "../../BF2/Platform/macOS/ClientPaths.h"
#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

namespace {
void require(bool condition, const char* description)
{
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s (errno %d)\n", description, errno);
        std::exit(1);
    }
}

void waitForSuccess(pid_t child)
{
    int status = 0;
    require(waitpid(child, &status, 0) == child, "wait for contender");
    require(WIFEXITED(status) && WEXITSTATUS(status) == 0, "contender refuses held lock");
}
}

int main()
{
    using namespace dice::hfe::macos;
    const char* home = std::getenv("HOME");
    require(home && std::string(home).find("/out/") != std::string::npos,
            "isolated HOME must be under out");
    const std::string expected = std::string(home) + "/Library/Application Support/OpenBF2";
    require(clientDataDirectory() == expected, "Application Support path preserves spelling/spaces");
    struct stat info;
    require(stat(expected.c_str(), &info) == 0 && S_ISDIR(info.st_mode)
            && (info.st_mode & 0777) == 0700, "new client data directory has mode 0700");

    // The lock lives in the working directory (the game folder), not the client data directory.
    const std::string gameFolder = std::string(home) + "/game";
    require(mkdir(gameFolder.c_str(), 0700) == 0 && chdir(gameFolder.c_str()) == 0,
            "enter a fresh game folder");
    int fd = acquireClientInstanceLock();
    require(fd >= 0, "first instance acquires lock without +multi");
    require((fcntl(fd, F_GETFD) & FD_CLOEXEC) != 0, "relaunch does not inherit lock");
    const std::string lock = gameFolder + "/Client.lock";
    require(access((expected + "/Client.lock").c_str(), F_OK) != 0, "no lock in the client data directory");
    require(stat(lock.c_str(), &info) == 0 && S_ISREG(info.st_mode)
            && (info.st_mode & 0777) == 0600, "new lock file has mode 0600");
    const ino_t inode = info.st_ino;

    pid_t child = fork();
    require(child >= 0, "fork contender");
    if (child == 0) {
        close(fd);
        _exit(acquireClientInstanceLock() == -1 ? 0 : 1);
    }
    waitForSuccess(child);
    require(close(fd) == 0, "close releases lock");
    fd = acquireClientInstanceLock();
    require(fd >= 0, "immediate reacquisition after close");
    require(stat(lock.c_str(), &info) == 0 && info.st_ino == inode, "lock inode is retained");
    close(fd);

    int ready[2];
    require(pipe(ready) == 0, "create readiness pipe");
    child = fork();
    require(child >= 0, "fork lock holder");
    if (child == 0) {
        close(ready[0]);
        const int owned = acquireClientInstanceLock();
        if (owned < 0 || write(ready[1], "x", 1) != 1) _exit(1);
        for (;;) pause();
    }
    close(ready[1]);
    char signal;
    require(read(ready[0], &signal, 1) == 1, "child holds lock");
    close(ready[0]);
    require(acquireClientInstanceLock() == -1, "parent refuses child's lock");
    require(kill(child, SIGKILL) == 0, "terminate only the test lock holder");
    int status = 0;
    require(waitpid(child, &status, 0) == child && WIFSIGNALED(status), "reap lock holder");
    fd = acquireClientInstanceLock();
    require(fd >= 0, "process death releases lock despite persistent file");
    close(fd);
    std::puts("PASS: macOS client path, modes, contention, close/reacquire, exec flag and process death");
}
