// [CHOSEN, not recovered] Check of the frontier trap's default (Code/RendDX9Abi/Frontier.cpp).
// Built twice: once as a dev build (BF2_FRONTIER_DEFAULT_CONTINUE unset, expected default abort) and
// once as a release build (BF2_FRONTIER_DEFAULT_CONTINUE=1, expected default continue).
// BF2_FRONTIER_CHECK_EXPECT_CONTINUE says which one this binary is. Every case runs in a forked
// child (the trap state is cached per process) inside its own temporary directory, so the log
// banner can be read back: unset / unexpected values give the build's default, an explicit
// "continue" or "abort" wins in both builds, and an armed frontier() really aborts (caught by a
// SIGABRT handler that exits with kAborted, so no crash report is written).

#include "../RendDX9Abi/Frontier.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#ifndef BF2_FRONTIER_CHECK_EXPECT_CONTINUE
#error "BF2_FRONTIER_CHECK_EXPECT_CONTINUE must be 0 or 1"
#endif

namespace {

const char* const kEnv = "BF2_RENDDX9_FRONTIER";
const char* const kLog = "RendDX9Rebuilt.log";
const int         kAborted = 42;

void onAbort(int) { ::_exit(kAborted); }

// Child body: returns 0 when the banner and frontierTrapEnabled() agree with expectArmed; when
// armed, a frontier() call must not return (it aborts, and onAbort exits with kAborted).
int child(const char* dir, const char* value, bool expectArmed)
{
    if (::chdir(dir) != 0)
        return 10;
    ::signal(SIGABRT, onAbort);
    if (value)
        ::setenv(kEnv, value, 1);
    else
        ::unsetenv(kEnv);

    renddx9abi::logOpen();
    FILE* f = ::fopen(kLog, "r");
    char line[512] = {0};
    if (!f || !::fgets(line, sizeof(line), f))
        return 11;
    ::fclose(f);
    const char* want = expectArmed ? "Frontier trap: ARMED (abort)." : "Frontier trap: DISARMED (continue).";
    if (!::strstr(line, want))
    {
        ::fprintf(stderr, "  banner mismatch: %s", line);
        return 12;
    }
    if (renddx9abi::frontierTrapEnabled() != expectArmed)
        return 13;

    renddx9abi::frontier("frontier_default_check::probe", 0, 0);
    // Reached only when disarmed.
    return expectArmed ? 14 : 0;
}

bool runCase(const char* label, const char* value, bool expectArmed)
{
    char dir[] = "/tmp/bf2_frontier_check.XXXXXX";
    if (!::mkdtemp(dir))
        return false;
    ::fflush(0);
    const pid_t pid = ::fork();
    if (pid < 0)
        return false;
    if (pid == 0)
        ::_exit(child(dir, value, expectArmed));
    int status = 0;
    ::waitpid(pid, &status, 0);
    const bool ok = WIFEXITED(status) && WEXITSTATUS(status) == (expectArmed ? kAborted : 0);
    char log[sizeof(dir) + 32];
    ::snprintf(log, sizeof(log), "%s/%s", dir, kLog);
    ::unlink(log);
    ::rmdir(dir);
    ::printf("%s %-28s expect %s\n", ok ? "PASS" : "FAIL", label,
             expectArmed ? "ARMED (abort)" : "DISARMED (continue)");
    if (!ok && WIFEXITED(status))
        ::printf("     child exit %d\n", WEXITSTATUS(status));
    return ok;
}

} // namespace

int main()
{
    const bool defaultArmed = BF2_FRONTIER_CHECK_EXPECT_CONTINUE == 0;
    ::printf("frontier default check: %s build\n", defaultArmed ? "dev (default abort)" : "release (default continue)");
    int failures = 0;
    failures += !runCase("unset", 0, defaultArmed);
    failures += !runCase("empty", "", defaultArmed);
    failures += !runCase("typo 'contine'", "contine", defaultArmed);
    failures += !runCase("too long for the buffer", "continue-continue-continue-continue", defaultArmed);
    failures += !runCase("'continue'", "continue", false);
    failures += !runCase("'CONTINUE'", "CONTINUE", false);
    failures += !runCase("'abort'", "abort", true);
    failures += !runCase("'Abort'", "Abort", true);
    ::printf("%d failure(s)\n", failures);
    return failures == 0 ? 0 : 1;
}
